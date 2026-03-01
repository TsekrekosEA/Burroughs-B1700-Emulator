#pragma once
// Burroughs B1700 Emulator — Core Type Definitions
// Based on the B1700 FE Technical Manual (1053360, May 1973)

#include <cstdint>
#include <cstddef>

namespace b1700 {

// ── Register-width type aliases ──────────────────────────────────────────
// Values are stored in the low N bits; upper bits must be masked.
using reg24_t   = uint32_t;   // 24-bit register value
using reg19_t   = uint32_t;   // 19-bit (MAR bit-address)
using reg16_t   = uint16_t;   // 16-bit (M, U registers)
using reg8_t    = uint8_t;    // 8-bit  (CP register)
using reg4_t    = uint8_t;    // 4-bit  (sub-field registers)
using bit_addr_t = uint32_t;  // bit address (up to 24 bits)

// ── Bit masks ────────────────────────────────────────────────────────────
constexpr uint32_t MASK_24 = 0x00FF'FFFF;
constexpr uint32_t MASK_19 = 0x0007'FFFF;
constexpr uint32_t MASK_16 = 0x0000'FFFF;
constexpr uint32_t MASK_8  = 0x0000'00FF;
constexpr uint32_t MASK_4  = 0x0000'000F;

// ── Machine configuration constants ─────────────────────────────────────
constexpr size_t B1710_STACK_DEPTH  = 16;
constexpr size_t B1720_STACK_DEPTH  = 32;
constexpr size_t SCRATCHPAD_WORDS   = 16;   // 16 double-words
constexpr size_t SCRATCHPAD_BITS    = 48;   // each word is 48 bits (left 24 + right 24)

// ── Processor model ─────────────────────────────────────────────────────
enum class ProcessorModel { B1710, B1720 };

// ── Configuration ────────────────────────────────────────────────────────
struct ProcessorConfig {
    ProcessorModel model       = ProcessorModel::B1710;
    size_t  s_memory_bytes     = 65536;     // 64 KB default
    size_t  m_memory_bytes     = 0;         // 0 for B1710
    size_t  stack_depth        = B1710_STACK_DEPTH;
    uint32_t maxs              = 0;         // computed at init (bits)
    uint32_t maxm              = 0;         // computed at init (bits)
};

// ── Microinstruction field helpers ───────────────────────────────────────
struct MicroFields {
    uint16_t raw;

    constexpr uint8_t MC() const { return (raw >> 12) & 0xF; }
    constexpr uint8_t MD() const { return (raw >>  8) & 0xF; }
    constexpr uint8_t ME() const { return (raw >>  4) & 0xF; }
    constexpr uint8_t MF() const { return  raw        & 0xF; }

    // 1C register move fields
    constexpr uint8_t src_group()  const { return MC(); }
    constexpr uint8_t src_select() const { return (MD() >> 2) & 0x3; }
    constexpr uint8_t dst_group()  const { return ME(); }
    constexpr uint8_t dst_select() const { return (MF() >> 2) & 0x3; }
    constexpr uint8_t variant()    const { return MD() & 0x3; }

    // 8C – 8-bit literal: literal is MD:ME (8 bits), dest group = MC, dest select forced to 2 (?)
    // Actually: MC = dest_group, MD:ME = 8-bit literal, MF encodes dest stuff
    constexpr uint8_t literal8()   const { return (MD() << 4) | ME(); }

    // 12C/13C branch & 14C/15C call
    constexpr bool     branch_sign()  const { return (raw >> 12) & 1; }
    constexpr uint16_t displacement() const { return raw & 0x0FFF; }

    // 7C memory access
    constexpr bool    mem_is_write()       const { return (raw >> 11) & 1; }
    constexpr uint8_t mem_register()       const { return (raw >> 9) & 0x3; }  // 0=X,1=Y,2=T,3=L
    constexpr bool    mem_field_reverse()  const { return (raw >> 8) & 1; }
    constexpr uint8_t mem_field_length()   const { return raw & 0xFF; }        // 0 → use CPL
    constexpr uint8_t mem_count_variant()  const { return raw & 0x07; }

    // 10C shift/rotate T
    constexpr bool    shift_is_rotate() const { return (raw >> 9) & 1; }
    constexpr uint8_t shift_count()     const { return ((raw >> 8) & 1) << 4 | ME(); }  // 5-bit

    // 4D shift X/Y
    constexpr bool    shift4d_reg_is_y() const { return (raw >> 11) & 1; }
    constexpr bool    shift4d_is_right() const { return (raw >> 10) & 1; }
    constexpr bool    shift4d_is_rotate() const { return (raw >>  9) & 1; }
    constexpr uint8_t shift4d_count()    const { return ((raw >> 4) & 0x1F); } // 5-bit from MD[8]:ME

    // 5D shift X:Y
    constexpr bool    shift5d_is_right() const { return (raw >> 8) & 1; }
};

} // namespace b1700
