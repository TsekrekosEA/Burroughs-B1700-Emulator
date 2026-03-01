#pragma once
// Burroughs B1700 Emulator — Register File
// Complete register set including scratchpad, A-stack, and pseudo-registers.

#include "types.h"
#include <array>
#include <cstring>

namespace b1700 {

class RegisterFile {
public:
    // ── Main 24-bit registers ────────────────────────────────────────────
    reg24_t X   = 0;   // Group 4 / Select 0  — left operand
    reg24_t Y   = 0;   // Group 4 / Select 1  — right operand
    reg24_t T   = 0;   // Group 4 / Select 2  — transform register
    reg24_t L   = 0;   // Group 4 / Select 3  — logical register

    reg24_t FA  = 0;   // Group 8 / Select 0  — field address (bit-address)
    reg24_t FB  = 0;   // Group 8 / Select 1  — field descriptor (FU:FT:FL)
    // FL is the low 16 bits of FB.  FU is bits 23–20.  FT is bits 19–16.

    reg24_t BR  = 0;   // Group 5 / Select 2  — base register
    reg24_t LR  = 0;   // Group 5 / Select 3  — limit register

    // ── 19-bit MAR (also known as 'A' register / program counter) ────────
    reg19_t MAR = 0;   // Group 5 / Select 0

    // ── 16-bit registers ─────────────────────────────────────────────────
    reg16_t M   = 0;   // Group 5 / Select 1  — current micro-instruction
    reg16_t U   = 0;   // Group 10/ Select 3  — cassette accumulator (read-only)

    // ── 24-bit Control register (addressed as sub-fields) ────────────────
    // CA(4) CB(4) CC(4) CD(4) CPL(5) CPU(2) CYF(1) = 24 bits
    reg4_t CA  = 0;    // Group 13 / Select 0
    reg4_t CB  = 0;    // Group 13 / Select 1
    reg4_t CC  = 0;    // Group 13 / Select 2  — interrupt status
    reg4_t CD  = 0;    // Group 13 / Select 3  — error/status
    reg8_t CP  = 0;    // Group 11 / Select 2  — CPL(7:3), CPU(2:1), CYF(0)

    // CP field accessors
    uint8_t CPL() const { return (CP >> 3) & 0x1F; }
    uint8_t CPU() const { return (CP >> 1) & 0x03; }
    uint8_t CYF() const { return  CP       & 0x01; }
    void set_CPL(uint8_t v) { CP = (CP & 0x07) | ((v & 0x1F) << 3); }
    void set_CPU(uint8_t v) { CP = (CP & ~0x06) | ((v & 0x03) << 1); }
    void set_CYF(uint8_t v) { CP = (CP & ~0x01) | (v & 0x01); }

    // FL accessor (low 16 bits of FB)
    uint16_t FL() const { return static_cast<uint16_t>(FB & MASK_16); }
    void set_FL(uint16_t v) { FB = (FB & 0x00FF0000) | v; }
    uint8_t  FU() const { return (FB >> 20) & 0xF; }
    uint8_t  FT() const { return (FB >> 16) & 0xF; }
    void set_FU(uint8_t v) { FB = (FB & 0x00FFFFFF & ~(0xF << 20)) | ((v & 0xF) << 20); }
    // Correction: proper mask
    // FB: bits 23-20 = FU, 19-16 = FT, 15-0 = FL
    // set_FU overwrites bits 23:20
    // Let me use cleaner accessors:
    void set_FB_field(uint8_t fu, uint8_t ft, uint16_t fl) {
        FB = ((fu & 0xF) << 20) | ((ft & 0xF) << 16) | fl;
    }

    // ── MAXS / MAXM (hardware-wired constants) ──────────────────────────
    reg24_t MAXS = 0;  // Group 9 / Select 0  — S-memory size in bits
    reg24_t MAXM = 0;  // Group 9 / Select 1  — M-memory size in bits

    // ── A-Stack ──────────────────────────────────────────────────────────
    static constexpr size_t MAX_STACK = B1720_STACK_DEPTH; // allocate max
    std::array<reg24_t, MAX_STACK> a_stack{};
    uint8_t stack_ptr = 0;
    uint8_t stack_depth = B1710_STACK_DEPTH;

    void push(reg24_t value) {
        stack_ptr = (stack_ptr + 1) % stack_depth;
        a_stack[stack_ptr] = value & MASK_24;
    }

    reg24_t pop() {
        reg24_t val = a_stack[stack_ptr] & MASK_24;
        stack_ptr = (stack_ptr + stack_depth - 1) % stack_depth;
        return val;
    }

    reg24_t top() const {
        return a_stack[stack_ptr] & MASK_24;
    }

    // ── Scratchpad (16 × 48-bit words = 16 × left24 + right24) ──────────
    struct ScratchpadWord {
        reg24_t left  = 0;
        reg24_t right = 0;
    };
    std::array<ScratchpadWord, SCRATCHPAD_WORDS> scratchpad{};

    // ── Pseudo-registers (I/O) ───────────────────────────────────────────
    reg24_t CMND_out = 0;   // Group 14 / Select 3  — write-only command
    reg24_t DATA_io  = 0;   // Group 15 / Select 2  — bidirectional I/O data
    reg24_t TOPM     = 0;   // Top of M-memory (B1720)

    // ── BICN, FLCN, XYCN, XYST (hardware-generated condition registers) ─
    // These are computed on read, not stored.  We cache them for testing.
    reg4_t BICN = 0;        // Group 12 / Select 0
    // FLCN, XYCN, XYST are computed dynamically (see Processor).

    // ── Temporary holding register for 7C memory ops ─────────────────────
    reg19_t TEMPB = 0;

    // ── M-register modification pending flag ─────────────────────────────
    bool     m_modify_pending = false;
    uint16_t m_modify_value   = 0;

    // ── Halt flag ────────────────────────────────────────────────────────
    bool halted = false;

    // ══════════════════════════════════════════════════════════════════════
    // Register read/write by group/select address
    // ══════════════════════════════════════════════════════════════════════

    // Read a register by its (group, select) address.
    // Returns value right-justified, zero-filled in 24 bits.
    // `processor` pointer needed for computed pseudo-registers; pass nullptr
    // if not available (will return cached values).
    uint32_t read(uint8_t group, uint8_t select) const {
        switch (group) {
            case  0: return get_T_nibble(select);           // TA–TD
            case  1: return get_T_nibble(4 + select);       // TE, TF, (reserved)
            case  2: return get_FB_nibble(select);           // FU, FT, FLC, FLD
            case  3: return get_FB_nibble(4 + select);       // FLE, FLF, (reserved)
            case  4:
                switch (select) {
                    case 0: return X & MASK_24;
                    case 1: return Y & MASK_24;
                    case 2: return T & MASK_24;
                    case 3: return L & MASK_24;
                }
                break;
            case  5:
                switch (select) {
                    case 0: return MAR & MASK_19;
                    case 1: return M & MASK_16;
                    case 2: return BR & MASK_24;
                    case 3: return LR & MASK_24;
                }
                break;
            case  6: return read_func_box_24(select);        // SUM, CMPX, CMPY, XANY
            case  7: return read_func_box_24(4 + select);    // XEQY, MSKX, MSKY, XORY
            case  8:
                switch (select) {
                    case 0: return FA & MASK_24;
                    case 1: return FB & MASK_24;
                    case 2: return FL();
                    case 3: return read_func_box_24(8);      // DIFF
                }
                break;
            case  9:
                switch (select) {
                    case 0: return MAXS;
                    case 1: return MAXM;
                    default: return 0;
                }
                break;
            case 10:
                switch (select) {
                    case 2: return top();                    // TAS (pop on read)
                    case 3: return U;
                    default: return 0;
                }
                break;
            case 11:
                if (select == 2) return CP;
                return 0;
            case 12:
                switch (select) {
                    case 0: return BICN;
                    case 1: return compute_FLCN();
                    case 2: return compute_XYCN();
                    case 3: return compute_XYST();
                }
                break;
            case 13:
                switch (select) {
                    case 0: return CA;
                    case 1: return CB;
                    case 2: return CC;
                    case 3: return CD;
                }
                break;
            case 14:
                switch (select) {
                    case 0: return CP >> 1;    // CPU
                    case 2: return 0;          // READ pseudo — not readable normally
                    case 3: return 0;          // CMND — write only
                    default: return 0;
                }
                break;
            case 15:
                switch (select) {
                    case 0: return 0;          // WRIT pseudo
                    case 1: return 0;          // NULL — always zero
                    case 2: return DATA_io;
                    default: return 0;
                }
                break;
        }
        return 0;
    }

    // Write a register by its (group, select) address.
    // Value should be right-justified; excess bits are truncated.
    void write(uint8_t group, uint8_t select, uint32_t value) {
        switch (group) {
            case  0: set_T_nibble(select, value & 0xF); return;
            case  1: set_T_nibble(4 + select, value & 0xF); return;
            case  2: set_FB_nibble(select, value & 0xF); return;
            case  3: set_FB_nibble(4 + select, value & 0xF); return;
            case  4:
                switch (select) {
                    case 0: X = value & MASK_24; return;
                    case 1: Y = value & MASK_24; return;
                    case 2: T = value & MASK_24; return;
                    case 3: L = value & MASK_24; return;
                }
                return;
            case  5:
                switch (select) {
                    case 0: MAR = value & MASK_19; return;
                    case 1:
                        // Writing to M: OR with next incoming micro
                        m_modify_pending = true;
                        m_modify_value   = static_cast<uint16_t>(value & MASK_16);
                        return;
                    case 2: BR = value & MASK_24; return;
                    case 3: LR = value & MASK_24; return;
                }
                return;
            case  6: return; // Function box outputs — read only
            case  7: return; // Function box outputs — read only
            case  8:
                switch (select) {
                    case 0: FA = value & MASK_24; return;
                    case 1: FB = value & MASK_24; return;
                    case 2: set_FL(static_cast<uint16_t>(value & MASK_16)); return;
                    case 3: return; // DIFF — read only
                }
                return;
            case  9: return; // MAXS, MAXM — read only
            case 10:
                switch (select) {
                    case 2: push(value & MASK_24); return;   // TAS (push on write)
                    default: return;
                }
                return;
            case 11:
                if (select == 2) { CP = value & MASK_8; return; }
                return;
            case 12: return; // Condition registers — read only
            case 13:
                switch (select) {
                    case 0: CA = value & 0xF; return;
                    case 1: CB = value & 0xF; return;
                    case 2: CC = value & 0xF; return;
                    case 3: CD = value & 0xF; return;
                }
                return;
            case 14:
                switch (select) {
                    case 0: set_CPU(value & 0x3); return;
                    case 3: CMND_out = value & MASK_24; return;  // CMND — triggers I/O
                    default: return;
                }
                return;
            case 15:
                switch (select) {
                    case 1: return;          // NULL — discard
                    case 2: DATA_io = value & MASK_24; return;
                    default: return;
                }
                return;
        }
    }

    // ── TAS special: read that pops ──────────────────────────────────────
    // The normal `read(10, 2)` returns top(). We handle pop in the processor
    // when we detect a TAS source read.  For register-move, a separate
    // `read_pop` can be used:
    reg24_t read_pop() { return pop(); }

private:
    // ── T register 4-bit nibble helpers ──────────────────────────────────
    // Index: 0=TA(23:20), 1=TB(19:16), 2=TC(15:12), 3=TD(11:8), 4=TE(7:4), 5=TF(3:0)
    uint8_t get_T_nibble(uint8_t idx) const {
        if (idx > 5) return 0;
        return (T >> ((5 - idx) * 4)) & 0xF;
    }

    void set_T_nibble(uint8_t idx, uint8_t val) {
        if (idx > 5) return;
        uint8_t shift = (5 - idx) * 4;
        T = (T & ~(0xFu << shift)) | ((val & 0xFu) << shift);
    }

    // ── FB register 4-bit nibble helpers ─────────────────────────────────
    // 0=FU(23:20), 1=FT(19:16), 2=FLC(15:12), 3=FLD(11:8), 4=FLE(7:4), 5=FLF(3:0)
    uint8_t get_FB_nibble(uint8_t idx) const {
        if (idx > 5) return 0;
        return (FB >> ((5 - idx) * 4)) & 0xF;
    }

    void set_FB_nibble(uint8_t idx, uint8_t val) {
        if (idx > 5) return;
        uint8_t shift = (5 - idx) * 4;
        FB = (FB & ~(0xFu << shift)) | ((val & 0xFu) << shift);
    }

    // ── Function box output computation ──────────────────────────────────
    // These are combinatorial — always reflect current X, Y, CP state.
    uint32_t read_func_box_24(uint8_t output_id) const {
        uint8_t cpl = CPL();
        if (cpl == 0) cpl = 24;
        uint32_t mask = (cpl >= 24) ? MASK_24 : ((1u << cpl) - 1);

        uint32_t mx = X & mask;
        uint32_t my = Y & mask;

        switch (output_id) {
            case 0: // SUM (Group 6/0): X + Y (binary) or BCD
                if (CPU() == 1) return bcd_add(mx, my, cpl) & mask;
                return (mx + my) & mask;
            case 1: // CMPX (Group 6/1): complement of X
                return (~mx) & mask;
            case 2: // CMPY (Group 6/2): complement of Y
                return (~my) & mask;
            case 3: // XANY (Group 6/3): X AND Y
                return mx & my;
            case 4: // XEQY (Group 7/0): X == Y → 0xFFFFFF if equal, else 0
                return (mx == my) ? mask : 0;
            case 5: // MSKX (Group 7/1): X masked by CPL
                return mx;
            case 6: // MSKY (Group 7/2): Y masked by CPL
                return my;
            case 7: // XORY (Group 7/3): X XOR Y
                return mx ^ my;
            case 8: // DIFF (Group 8/3): X - Y
                if (CPU() == 1) return bcd_sub(mx, my, cpl) & mask;
                return (mx - my) & mask;
            default:
                return 0;
        }
    }

    // BCD addition: treat operands as groups of 4-bit BCD digits
    static uint32_t bcd_add(uint32_t x, uint32_t y, uint8_t cpl) {
        uint8_t ndigits = (cpl + 3) / 4;
        uint32_t result = 0;
        uint32_t carry = 0;
        for (int i = 0; i < ndigits; ++i) {
            uint32_t dx = (x >> (i * 4)) & 0xF;
            uint32_t dy = (y >> (i * 4)) & 0xF;
            uint32_t sum = dx + dy + carry;
            if (sum >= 10) { sum -= 10; carry = 1; } else { carry = 0; }
            result |= (sum & 0xF) << (i * 4);
        }
        return result;
    }

    // BCD subtraction
    static uint32_t bcd_sub(uint32_t x, uint32_t y, uint8_t cpl) {
        uint8_t ndigits = (cpl + 3) / 4;
        uint32_t result = 0;
        int32_t borrow = 0;
        for (int i = 0; i < ndigits; ++i) {
            int32_t dx = (x >> (i * 4)) & 0xF;
            int32_t dy = (y >> (i * 4)) & 0xF;
            int32_t diff = dx - dy - borrow;
            if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; }
            result |= (diff & 0xF) << (i * 4);
        }
        return result;
    }

    // ── Computed condition pseudo-registers ───────────────────────────────
    uint8_t compute_FLCN() const {
        uint16_t fl  = FL();
        // SFL is the FL stored in scratchpad word 0 right half (bits 15:0)
        uint16_t sfl = static_cast<uint16_t>(scratchpad[0].right & MASK_16);
        uint8_t r = 0;
        if (fl == 0)    r |= 0x1;  // FLCN(0): FL = 0
        if (fl <  sfl)  r |= 0x2;  // FLCN(1): FL < SFL
        if (fl >  sfl)  r |= 0x4;  // FLCN(2): FL > SFL
        if (fl == sfl)  r |= 0x8;  // FLCN(3): FL = SFL
        return r;
    }

    uint8_t compute_XYCN() const {
        uint8_t cpl = CPL();
        if (cpl == 0) cpl = 24;
        uint32_t mask = (cpl >= 24) ? MASK_24 : ((1u << cpl) - 1);
        uint32_t mx = X & mask;
        uint32_t my = Y & mask;
        uint8_t r = 0;
        if (mx == my)  r |= 0x8;  // equality
        if (mx != my)  r |= 0x4;  // inequality
        if (mx <  my)  r |= 0x2;  // X < Y
        if (mx >  my)  r |= 0x1;  // X > Y
        return r;
    }

    uint8_t compute_XYST() const {
        // XYST(2) = INT OR = OR of all interrupt sources
        bool int_or = (CC & 0xF) != 0;
        uint8_t r = 0;
        if (int_or) r |= 0x4;  // bit 2
        return r;
    }
};

} // namespace b1700
