#pragma once
// Burroughs B1700 Emulator — I/O Bus and I/O Control Interface
// Models the bus-based I/O architecture where the processor communicates
// with peripheral devices through dedicated I/O Controls.
//
// The B1700 uses descriptor-based I/O.  DISPATCH WRITE sends a 7-word
// (24-bit each) descriptor to the I/O control:
//   Word 0: Operation word (bit 23=direction: 1=write, 0=read)
//   Word 1: RIA — Result/Interrupt Address (where the RSW is written)
//   Word 2: A-address (buffer start in memory)
//   Word 3: B-address (buffer end in memory)
//   Word 4: Reserved
//   Word 5: Result area (processor writes interrupt-request bit here)
//   Word 6: Link (next descriptor address)
//
// When the I/O control completes, it:
//   1. Writes the Result Status Word (RSW) to the RIA address
//   2. Asserts a service request (becomes CC(1) bus interrupt)
//
// DISPATCH READ AND CLEAR acknowledges the interrupt, sets L= RIA+24,
// and returns port/status in T.

#include "types.h"
#include "memory.h"
#include <array>
#include <memory>
#include <functional>
#include <vector>
#include <cstdint>
#include <cstdio>

namespace b1700 {

// ── I/O Descriptor ───────────────────────────────────────────────────────
struct IODescriptor {
    uint32_t op_word;    // word 0: operation
    uint32_t ria;        // word 1: result/interrupt address
    uint32_t a_addr;     // word 2: buffer A (start)
    uint32_t b_addr;     // word 3: buffer B (end)
    uint32_t reserved0;  // word 4
    uint32_t result;     // word 5: result area
    uint32_t link;       // word 6: link to next descriptor

    bool is_write() const { return (op_word >> 22) & 1; } // bit 22 of op
    bool is_read()  const { return !is_write(); }
};

// Read a 7-word descriptor from memory starting at the given bit address
inline IODescriptor read_descriptor(Memory& mem, uint32_t bit_addr) {
    IODescriptor d;
    d.op_word  = mem.read_field(bit_addr +   0, 24);
    d.ria      = mem.read_field(bit_addr +  24, 24);
    d.a_addr   = mem.read_field(bit_addr +  48, 24);
    d.b_addr   = mem.read_field(bit_addr +  72, 24);
    d.reserved0= mem.read_field(bit_addr +  96, 24);
    d.result   = mem.read_field(bit_addr + 120, 24);
    d.link     = mem.read_field(bit_addr + 144, 24);
    return d;
}

// ── I/O Control Interface ────────────────────────────────────────────────
// Each peripheral device type implements this interface.
// Slots 0-13 on the I/O bus.
class IOControl {
public:
    virtual ~IOControl() = default;

    // Start an I/O operation based on the descriptor.
    // The control receives the full descriptor + a reference to memory
    // for DMA transfers.  It should schedule completion internally.
    virtual void start_io(const IODescriptor& desc, Memory& mem) = 0;

    // Check if this control has a pending service request (SR)
    virtual bool has_service_request() const = 0;

    // Clear the service request (after processor acknowledges)
    virtual void clear_service_request() = 0;

    // Get the RIA from the most recently completed descriptor
    // (used by DISPATCH READ AND CLEAR to set L)
    virtual uint32_t completed_ria() const = 0;

    // Get 4-bit status for BICN register
    virtual uint8_t get_bus_status() const = 0;

    // Advance one clock cycle (for simulating device timing)
    virtual void tick(uint64_t cycle) = 0;

    // Human-readable name for debug output
    virtual const char* name() const = 0;
};

// ── I/O Bus ──────────────────────────────────────────────────────────────
// Routes commands from the processor to the appropriate I/O control.
// Up to 14 controls can be installed (slots 0-13).
class IOBus {
public:
    static constexpr int MAX_CONTROLS = 14;

    // Install an I/O control at a specific slot (port)
    void install(int slot, std::unique_ptr<IOControl> ctrl) {
        if (slot >= 0 && slot < MAX_CONTROLS) {
            controls_[slot] = std::move(ctrl);
        }
    }

    // Remove an I/O control
    void remove(int slot) {
        if (slot >= 0 && slot < MAX_CONTROLS) {
            controls_[slot].reset();
        }
    }

    // ── Bus operations (called by processor via DISPATCH) ────────────────

    // Start an I/O operation on the given port using the descriptor from memory
    void dispatch_write(int port, const IODescriptor& desc, Memory& mem) {
        if (port >= 0 && port < MAX_CONTROLS && controls_[port]) {
            controls_[port]->start_io(desc, mem);
        }
    }

    // Check if any I/O control has a pending service request
    // This drives CC(1) — the bus interrupt bit
    bool any_service_request() const {
        for (auto& c : controls_) {
            if (c && c->has_service_request()) return true;
        }
        return false;
    }

    // Get the combined BICN status (OR of all controls)
    uint8_t get_bicn() const {
        uint8_t result = 0;
        for (auto& c : controls_) {
            if (c) result |= c->get_bus_status();
        }
        return result & 0xF;
    }

    // Find which port has a service request (for DISPATCH READ AND CLEAR)
    int find_requesting_port() const {
        for (int i = 0; i < MAX_CONTROLS; ++i) {
            if (controls_[i] && controls_[i]->has_service_request())
                return i;
        }
        return -1;
    }

    // Advance all I/O controls by one cycle
    void tick(uint64_t cycle) {
        for (auto& c : controls_) {
            if (c) c->tick(cycle);
        }
    }

    // Get control at slot (for debug)
    IOControl* get(int slot) const {
        if (slot >= 0 && slot < MAX_CONTROLS) return controls_[slot].get();
        return nullptr;
    }

    // Bus lock state (single-processor: trivial)
    bool locked() const { return locked_; }
    void lock()         { locked_ = true; }
    void unlock()       { locked_ = false; }

private:
    std::array<std::unique_ptr<IOControl>, MAX_CONTROLS> controls_{};
    bool locked_ = false;
};

// ── Null I/O Control (for testing) ───────────────────────────────────────
// A minimal device that accepts descriptors, writes a successful RSW to
// the RIA address, and asserts a service request after a configurable delay.
class NullIOControl : public IOControl {
public:
    explicit NullIOControl(int delay_cycles = 10)
        : delay_(delay_cycles) {}

    void start_io(const IODescriptor& desc, Memory& mem) override {
        desc_ = desc;
        mem_ = &mem;
        pending_ = true;
        complete_at_ = current_cycle_ + delay_;
    }

    bool has_service_request() const override { return service_request_; }

    void clear_service_request() override { service_request_ = false; }

    uint32_t completed_ria() const override { return desc_.ria; }

    uint8_t get_bus_status() const override {
        return service_request_ ? 0x1 : 0x0;
    }

    void tick(uint64_t cycle) override {
        current_cycle_ = cycle;
        if (pending_ && cycle >= complete_at_) {
            pending_ = false;
            if (mem_) {
                mem_->write_field(desc_.ria + 23, 24, 0x800000, true);
            }
            service_request_ = true;
        }
    }

    const char* name() const override { return "NullDevice"; }

private:
    int delay_;
    IODescriptor desc_{};
    Memory* mem_ = nullptr;
    uint64_t current_cycle_ = 0;
    uint64_t complete_at_ = 0;
    bool pending_ = false;
    bool service_request_ = false;
};

// ── EMV Host Control ─────────────────────────────────────────────────────
// Simulates the DCP (Data Communications Processor) host that the B1700
// cold start loader communicates with.  Implements the minimal EMV protocol
// to get the CSL to enter M-load mode and load card data.
//
// Protocol sequence:
//   1. CSL sends READ → host responds with START-M-LOAD preamble
//   2. CSL sends WRITE (card reader preamble) → host ACKs
//   3. CSL sends READ → host sends card data (M-header or data card)
//   4. Repeat from 2 until all cards are sent
//   5. Final card is /EN/ (end sentinel)
//
// The "card data" is loaded from an external buffer (set by the caller).
class EMVHostControl : public IOControl {
public:
    explicit EMVHostControl(int delay_cycles = 20)
        : delay_(delay_cycles) {}

    // Load card images.  Each vector<uint8_t> is one 80-column card image
    // in EBCDIC encoding.  The EMV host will serve them in order.
    void load_cards(std::vector<std::vector<uint8_t>> cards) {
        cards_ = std::move(cards);
        card_index_ = 0;
    }

    void start_io(const IODescriptor& desc, Memory& mem) override {
        desc_ = desc;
        mem_ = &mem;
        pending_ = true;
        complete_at_ = current_cycle_ + delay_;
    }

    bool has_service_request() const override { return service_request_; }

    void clear_service_request() override { service_request_ = false; }

    uint32_t completed_ria() const override { return desc_.ria; }

    uint8_t get_bus_status() const override {
        return service_request_ ? 0x1 : 0x0;
    }

    void tick(uint64_t cycle) override {
        current_cycle_ = cycle;
        if (!pending_ || cycle < complete_at_ || !mem_) return;

        pending_ = false;
        bool is_read = desc_.is_read();

        if (is_read) {
            // The host is providing data TO the processor.
            // Write the response into the A-address buffer.
            uint32_t buf = desc_.a_addr;

            if (io_phase_ == 0) {
                // Phase 0: first READ → respond with START-M-LOAD preamble
                // The preamble is 20 bytes (160 bits).
                // Layout (bit offsets within buffer):
                //   +16: 4-bit flags (0 = no NAK)
                //   +32: 8-bit unit type (0 = sys-to-sys)
                //   +32 reverse 12 bits: operation code 0x531 (START M LOAD)
                // All zeros except the 12-bit opcode written in reverse.
                mem_->write_field(buf + 32, 12, 0x531, /*reverse=*/true);
                io_phase_ = 1;
                std::printf("[EMV] Sent START-M-LOAD preamble to buffer %06X\n", buf);
            } else {
                // Phase 1+: subsequent READs → serve card data
                // The response has a 20-byte preamble (160 bits), then data at offset 0xA0.
                uint32_t data_start = buf + 0xA0; // EMV-INPUT-BUFFER-DATA offset

                // Clear the preamble area so stale opcodes don't confuse the CSL
                for (uint32_t a = buf; a < buf + 0xA0; a += 8)
                    mem_->write_field(a, 8, 0);

                // Write unit type = 1 (card reader) at preamble offset 32
                // PREAMBLE-SERVICE-IN reads unit type from this offset
                mem_->write_field(buf + 32, 8, 0x01);

                if (card_index_ < cards_.size()) {
                    const auto& card = cards_[card_index_];
                    // Write card image at data offset, 8 bits at a time
                    uint32_t addr = data_start;
                    for (size_t i = 0; i < card.size() && addr < desc_.b_addr; ++i) {
                        mem_->write_field(addr, 8, card[i]);
                        addr += 8;
                    }
                    std::printf("[EMV] Sent card %zu/%zu (%zu bytes) to buffer %06X\n",
                                card_index_ + 1, cards_.size(), card.size(), data_start);
                    card_index_++;
                } else {
                    // No more cards — send /EN/ (end sentinel)
                    // /EN/ = 0x61C5D5 in EBCDIC: '/' = 0x61, 'E' = 0xC5, 'N' = 0xD5
                    // Write /EN/ at the data area where CSL reads it
                    mem_->write_field(data_start, 8, 0x61);       // '/'
                    mem_->write_field(data_start + 8, 8, 0xC5);   // 'E'
                    mem_->write_field(data_start + 16, 8, 0xD5);  // 'N'
                    // Verify the write
                    uint32_t v = mem_->read_field(data_start, 24);
                    std::printf("[EMV] Sent /EN/ at %06X, readback=%06X (expect 61C5D5)\n",
                                data_start, v);
                }
            }
        } else {
            // WRITE: host receives data from CSL (preamble / card request).
            // For now, just acknowledge it (NOP).
            std::printf("[EMV] Received WRITE from CSL, A=%06X\n", desc_.a_addr);
        }

        // Write RSW to RIA in reverse (TA(0)=1 complete, TA(1)=0 no exceptions)
        // B1700 MSB-first: TA is the upper 4 bits of T (bits 0-3).
        // TA(0) is bit 0 of T, which is value 0x800000.
        mem_->write_field(desc_.ria + 23, 24, 0x800000, /*reverse=*/true);
        service_request_ = true;
    }

    const char* name() const override { return "EMVHost"; }

    size_t cards_remaining() const {
        return card_index_ < cards_.size() ? cards_.size() - card_index_ : 0;
    }

private:
    int delay_;
    IODescriptor desc_{};
    Memory* mem_ = nullptr;
    uint64_t current_cycle_ = 0;
    uint64_t complete_at_ = 0;
    bool pending_ = false;
    bool service_request_ = false;

    int io_phase_ = 0;         // 0 = initial contact, 1+ = card serving
    std::vector<std::vector<uint8_t>> cards_;
    size_t card_index_ = 0;
};

} // namespace b1700
