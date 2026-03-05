#pragma once
// Burroughs B1700 Emulator — Bit-Addressable Memory System
// Implements S-Memory (main) with bit-granularity access and parity.

#include "types.h"
#include <vector>
#include <cstring>
#include <stdexcept>

namespace b1700 {

class Memory {
public:
    explicit Memory(size_t size_bytes)
        : data_(size_bytes, 0), size_bytes_(size_bytes) {}

    // ── Microinstruction fetch (word-aligned) ────────────────────────────
    // addr is a BIT address; low 4 bits are ignored (16-bit alignment).
    // Returns a 16-bit micro-operator.
    uint16_t fetch_micro(uint32_t bit_addr) const {
        uint32_t byte_addr = (bit_addr >> 4) * 2;  // word-aligned byte address
        if (byte_addr + 1 >= size_bytes_) return 0;
        return static_cast<uint16_t>(data_[byte_addr] << 8) | data_[byte_addr + 1];
    }

    // ── Bit-field read ───────────────────────────────────────────────────
    // Read `field_len` bits starting at `bit_addr`.
    // If reverse is true, addresses decrease.
    // Returns result right-justified, zero-filled on the left.
    // Max 24 bits (hardware data-path limit).
    uint32_t read_field(uint32_t bit_addr, uint8_t field_len, bool reverse = false) const {
        if (field_len == 0 || field_len > 24) return 0;

        uint32_t result = 0;
        for (int i = 0; i < field_len; ++i) {
            uint32_t addr = reverse ? (bit_addr - i) : (bit_addr + i);
            if (get_bit(addr))
                result |= (1u << (field_len - 1 - i));
        }
        return result;
    }

    // ── Bit-field write ──────────────────────────────────────────────────
    // Write `field_len` bits from `value` (right-justified) to memory
    // starting at `bit_addr`. Performs read-modify-write to preserve
    // surrounding bits within each byte.
    void write_field(uint32_t bit_addr, uint8_t field_len, uint32_t value, bool reverse = false) {
        if (field_len == 0 || field_len > 24) return;

        for (int i = 0; i < field_len; ++i) {
            bool bit_val = (value >> (field_len - 1 - i)) & 1;
            uint32_t addr = reverse ? (bit_addr - i) : (bit_addr + i);
            set_bit(addr, bit_val);
        }
    }

    // ── Bulk byte access (for loading tape images, etc.) ─────────────────
    void load_bytes(uint32_t byte_offset, const uint8_t* src, size_t len) {
        if (byte_offset + len > size_bytes_)
            throw std::out_of_range("Memory::load_bytes out of range");
        std::memcpy(data_.data() + byte_offset, src, len);
    }

    uint8_t read_byte(uint32_t byte_addr) const {
        if (byte_addr >= size_bytes_) return 0;
        return data_[byte_addr];
    }

    void write_byte(uint32_t byte_addr, uint8_t val) {
        if (byte_addr < size_bytes_)
            data_[byte_addr] = val;
    }

    size_t size_bytes() const { return size_bytes_; }
    size_t size_bits()  const { return size_bytes_ * 8; }

    // Direct access for testing
    const uint8_t* raw() const { return data_.data(); }
    uint8_t*       raw()       { return data_.data(); }

private:
    bool get_bit(uint32_t bit_addr) const {
        uint32_t byte_idx = bit_addr / 8;
        uint8_t  bit_pos  = 7 - (bit_addr % 8);  // MSB-first within byte
        if (byte_idx >= size_bytes_) return false;
        return (data_[byte_idx] >> bit_pos) & 1;
    }

    void set_bit(uint32_t bit_addr, bool val) {
        uint32_t byte_idx = bit_addr / 8;
        uint8_t  bit_pos  = 7 - (bit_addr % 8);
        if (byte_idx >= size_bytes_) return;
        if (val)
            data_[byte_idx] |=  (1u << bit_pos);
        else
            data_[byte_idx] &= ~(1u << bit_pos);
    }

    std::vector<uint8_t> data_;
    size_t size_bytes_;
};

} // namespace b1700
