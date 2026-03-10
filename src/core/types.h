#pragma once
// Burroughs B1700 Emulator — Core Type Definitions
// Based on the B1700 FE Technical Manual (1053360, May 1973)
//
// ENCODING MODEL (corrected from manual):
//   MC[15:12] = instruction opcode (0-15) — ALWAYS the opcode
//   MD[11:8], ME[7:4], MF[3:0] = operand fields
//   Register groups are NEVER in MC; they go in MD/ME/MF sub-fields.

#include <cstdint>
#include <cstddef>

namespace b1700 {

// ── Register-width type aliases ──────────────────────────────────────────
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

// ══════════════════════════════════════════════════════════════════════════
// Microinstruction field helpers
// MC[15:12] is ALWAYS the instruction opcode.
// ══════════════════════════════════════════════════════════════════════════
struct MicroFields {
    uint16_t raw;

    constexpr uint8_t MC() const { return (raw >> 12) & 0xF; }
    constexpr uint8_t MD() const { return (raw >>  8) & 0xF; }
    constexpr uint8_t ME() const { return (raw >>  4) & 0xF; }
    constexpr uint8_t MF() const { return  raw        & 0xF; }

    // ── 1C Register Move: MC=0001 ───────────────────────────────────────
    //   MD[11:8]  = Dst Group (0-15)
    //   ME[7:6]   = Dst Select (0-3)
    //   ME[5:4]   = Src Select (0-3)
    //   MF[3:0]   = Src Group (0-15)
    constexpr uint8_t r1c_dst_group()  const { return MD(); }
    constexpr uint8_t r1c_dst_select() const { return (raw >> 6) & 0x3; }
    constexpr uint8_t r1c_src_select() const { return (raw >> 4) & 0x3; }
    constexpr uint8_t r1c_src_group()  const { return MF(); }

    // ── 2C Scratchpad Move: MC=0010 ─────────────────────────────────────
    //   MD[11:8]  = Pad word address (0-15)
    //   ME[7]     = 0=Left, 1=Right half
    //   ME[6]     = 0=To pad, 1=From pad
    //   ME[5:4]   = Register Select (0-3)
    //   MF[3:0]   = Register Group (0-15)
    constexpr uint8_t r2c_pad_addr()   const { return MD(); }
    constexpr bool    r2c_is_right()   const { return (raw >> 7) & 1; }
    constexpr bool    r2c_from_pad()   const { return (raw >> 6) & 1; }
    constexpr uint8_t r2c_reg_select() const { return (raw >> 4) & 0x3; }
    constexpr uint8_t r2c_reg_group()  const { return MF(); }

    // ── 3C 4-Bit Manipulate: MC=0011 ────────────────────────────────────
    //   MD[11:8]  = Register Group (0-15)
    //   ME[7]     = Register Select (0 or 1)
    //   ME[6:4]   = Variant (0=SET,1=AND,2=OR,3=EOR,4=INC,5=INC-T,6=DEC,7=DEC-T)
    //   MF[3:0]   = Literal (0-15)
    constexpr uint8_t r3c_group()   const { return MD(); }
    constexpr uint8_t r3c_select()  const { return (raw >> 7) & 1; }
    constexpr uint8_t r3c_variant() const { return (raw >> 4) & 0x7; }
    constexpr uint8_t r3c_literal() const { return MF(); }

    // ── 4C/5C Bit Test Branch: MC=0100 (false) or MC=0101 (true) ────────
    //   MD[11:8]  = Register Group (0-15)
    //   ME[7]     = Register Select (0 or 1)
    //   ME[6:5]   = Bit Number (0-3)
    //   ME[4]     = Displacement Sign (0=positive, 1=negative)
    //   MF[3:0]   = Displacement Magnitude (0-15 words)
    constexpr uint8_t r4c_group()    const { return MD(); }
    constexpr uint8_t r4c_select()   const { return (raw >> 7) & 1; }
    constexpr uint8_t r4c_bit()      const { return (raw >> 5) & 0x3; }
    constexpr bool    r4c_neg()      const { return (raw >> 4) & 1; }
    constexpr uint8_t r4c_disp()     const { return MF(); }

    // ── 6C Skip When: MC=0110 ───────────────────────────────────────────
    //   MD[11:8]  = Register Group (0-15)
    //   ME[7]     = Register Select (0 or 1)
    //   ME[6:4]   = Variant (0-7)
    //   MF[3:0]   = Mask (0-15)
    constexpr uint8_t r6c_group()   const { return MD(); }
    constexpr uint8_t r6c_select()  const { return (raw >> 7) & 1; }
    constexpr uint8_t r6c_variant() const { return (raw >> 4) & 0x7; }
    constexpr uint8_t r6c_mask()    const { return MF(); }

    // ── 7C Read/Write Memory: MC=0111 (unchanged from before) ───────────
    //   MD[11]         = Direction (0=read, 1=write)
    //   MD[10:9]       = Register (00=X, 01=Y, 10=T, 11=L)
    //   MD[8]          = Reverse
    //   ME[7:4]:MF[3]  = Field Length (5 bits, 0-31; 0=use CPL)
    //   MF[2:0]        = Count Variant (0-7)
    constexpr bool    r7c_is_write()     const { return (raw >> 11) & 1; }
    constexpr uint8_t r7c_register()     const { return (raw >> 9) & 0x3; }
    constexpr bool    r7c_reverse()      const { return (raw >> 8) & 1; }
    constexpr uint8_t r7c_field_length() const { return (raw >> 3) & 0x1F; }
    constexpr uint8_t r7c_count_var()    const { return raw & 0x07; }

    // ── 8C Move 8-bit Literal: MC=1000 ──────────────────────────────────
    //   MD[11:8]    = Destination Group (0-15)
    //   ME:MF[7:0]  = 8-bit literal
    //   Always writes to select 2 of the group.
    constexpr uint8_t r8c_group()   const { return MD(); }
    constexpr uint8_t r8c_literal() const { return raw & 0xFF; }

    // ── 9C Move 24-bit Literal: MC=1001 ─────────────────────────────────
    //   MD[11:8]    = Destination Group (0-15)
    //   ME:MF[7:0]  = Upper 8 bits of literal
    //   Next word    = Lower 16 bits
    //   Always writes to select 2 of the group.
    constexpr uint8_t r9c_group() const { return MD(); }
    constexpr uint8_t r9c_hi8()   const { return raw & 0xFF; }

    // ── 10C Shift/Rotate T: MC=1010 ─────────────────────────────────────
    //   MD[11:8]       = Destination Group (0-15)
    //   ME[7:6]        = Destination Select (0-3)
    //   ME[5]          = 0=Shift, 1=Rotate
    //   ME[4]:MF[3:0]  = Count (5 bits, 0-23; 0=use CPL)
    constexpr uint8_t r10c_dst_group()  const { return MD(); }
    constexpr uint8_t r10c_dst_select() const { return (raw >> 6) & 0x3; }
    constexpr bool    r10c_is_rotate()  const { return (raw >> 5) & 1; }
    constexpr uint8_t r10c_count()      const { return raw & 0x1F; }

    // ── 11C Extract from T: MC=1011 ─────────────────────────────────────
    //   MD[11:8]:ME[7]  = Rotate count (5 bits, 0-24)
    //   ME[6:5]         = Dest code (00=X, 01=Y, 10=T, 11=L)
    //   ME[4]:MF[3:0]   = Width (5 bits, 0-24)
    constexpr uint8_t r11c_rotate() const { return (raw >> 7) & 0x1F; }
    constexpr uint8_t r11c_dst()    const { return (raw >> 5) & 0x3; }
    constexpr uint8_t r11c_width()  const { return raw & 0x1F; }

    // ── 12C/13C Branch / 14C/15C Call ───────────────────────────────────
    //   MC[12]         = Sign (0=positive, 1=negative)
    //   MD:ME:MF[11:0] = 12-bit displacement (in words)
    constexpr bool     branch_sign()  const { return (raw >> 12) & 1; }
    constexpr uint16_t displacement() const { return raw & 0x0FFF; }

    // ── D-class: 4D Shift X/Y (MC=0, MD=4) ─────────────────────────────
    constexpr bool    r4d_is_y()     const { return (raw >> 7) & 1; }
    constexpr bool    r4d_is_right() const { return (raw >> 6) & 1; }
    constexpr bool    r4d_is_rot()   const { return (raw >> 5) & 1; }
    constexpr uint8_t r4d_count()    const { return raw & 0x1F; }

    // ── D-class: 5D Shift X:Y concat (MC=0, MD=5) ──────────────────────
    constexpr bool    r5d_is_right() const { return (raw >> 7) & 1; }
};

} // namespace b1700
