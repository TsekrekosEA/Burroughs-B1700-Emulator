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
