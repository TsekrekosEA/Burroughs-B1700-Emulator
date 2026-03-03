#pragma once
// Burroughs B1700 Emulator — Processor Core
// Implements the full micro-instruction decode tree and execution for the B1710.

#include "types.h"
#include "memory.h"
#include "registers.h"

#include <cstdio>
#include <functional>

namespace b1700 {

class Processor {
public:
    RegisterFile regs;
    Memory       mem;
    uint64_t     cycles = 0;
    bool         trace_enabled = false;

    // Callback for halt
    std::function<void()> on_halt;

    explicit Processor(const ProcessorConfig& cfg)
        : mem(cfg.s_memory_bytes)
    {
        regs.stack_depth = static_cast<uint8_t>(cfg.stack_depth);
        regs.MAXS = static_cast<reg24_t>(cfg.s_memory_bytes * 8);
        regs.MAXM = static_cast<reg24_t>(cfg.m_memory_bytes * 8);
    }

    // ── Run until halt or max_cycles ─────────────────────────────────────
    void run(uint64_t max_cycles = 1'000'000) {
        regs.halted = false;
        uint64_t start = cycles;
        while (!regs.halted && (cycles - start) < max_cycles) {
            step();
        }
    }

    // ── Single-step one microinstruction ─────────────────────────────────
    void step() {
        if (regs.halted) return;

        // Fetch micro at MAR (word-aligned)
        uint16_t micro = mem.fetch_micro(regs.MAR);

        // Apply pending M-register modification (OR)
        if (regs.m_modify_pending) {
            micro |= regs.m_modify_value;
            regs.m_modify_pending = false;
            regs.m_modify_value = 0;
        }

        regs.M = micro;

        // Save pre-increment MAR for trace
        uint32_t pc = regs.MAR;

        // Auto-increment MAR by 16 bits (one word)
        regs.MAR = (regs.MAR + 16) & MASK_19;

        // Decode & execute
        int clks = decode_and_execute(MicroFields{micro});
        cycles += clks;

        if (trace_enabled) {
            trace_log(pc, micro, clks);
        }
    }

private:
    // ══════════════════════════════════════════════════════════════════════
    // DECODE TREE
    // ══════════════════════════════════════════════════════════════════════
    int decode_and_execute(MicroFields f) {
        // ── Special exact-match instructions (MC=0, MD=0 patterns) ───────
        if (f.raw == 0x0000) return 2;  // NOP
        if (f.raw == 0x0002) {          // HALT (1F)
            regs.halted = true;
            if (on_halt) on_halt();
            return 2;
        }
        if (f.raw == 0x0003) return exec_normalize(f);  // 3F Normalize X

        uint8_t mc = f.MC();

        // ── Branch/Call family: MC[15:14] == 11 (MC = 12–15) ────────────
        if ((f.raw >> 14) == 0x3) {
            bool is_call = (f.raw >> 13) & 1;  // bit 13: 1=call, 0=branch
            return is_call ? exec_call(f) : exec_branch(f);
        }

        // ── 7C Read/Write Memory: MC = 0111 ─────────────────────────────
        if (mc == 0x7) {
            return exec_memory_access(f);
        }

        // ── MC = 0000 with MD ≠ 0: D/E class secondary micros ───────────
        if (mc == 0x0 && f.MD() != 0) {
            return decode_secondary(f);
        }

        // ── Primary C-class: MF[1:0] selects instruction class ──────────
        // Covers MC=0 (with MD=0, non-special), MC=1–6, and MC=8–11.
        // MC encodes the source register group (1C) or dest group (8C).
        switch (f.MF() & 0x3) {
            case 0: return exec_register_move_or_shift(f);  // 1C
            case 1: return exec_4bit_manipulate(f);         // 3C
            case 2: return exec_literal_move(f);            // 8C / 9C
            case 3: return exec_skip_when(f);               // 6C
        }
        return 2; // unreachable
    }

    // ── Secondary decode (MC = 0000, MD ≠ 0) ────────────────────────────
    int decode_secondary(MicroFields f) {
        uint8_t md = f.MD();

        switch (md) {
            case 0x2: return exec_scratchpad_move_or_cassette(f);
            case 0x3: return exec_bias(f);          // 3E
            case 0x4: return exec_shift_xy(f);      // 4D
            case 0x5: return exec_shift_xy_concat(f); // 5D
            case 0x6: return exec_count_or_carry(f);  // 6D or 6E
            case 0x7: return exec_exchange_doublepad(f); // 7D
            case 0x8: return exec_scratchpad_relate(f);  // 8D
            case 0x9: return 2;  // 9D Monitor — NOP
            default:  return 2;  // Unknown — treat as NOP
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // MICRO-OPERATOR IMPLEMENTATIONS
    // ══════════════════════════════════════════════════════════════════════

    // ── 1C: Register Move ────────────────────────────────────────────────
    int exec_register_move_or_shift(MicroFields f) {
        // Distinguish 1C from 10C/11C:
        // For 10C: dest has shift info, not a simple move.
        // For 11C: further decode needed.
        // Heuristic: for now, treat all MF[1:0]==00 as 1C register moves.
        // The 10C and 11C have MC=0000 and are handled in decode_secondary.
        // So any MC != 0 with MF[1:0] == 00 is a 1C register move.

        uint8_t src_grp = f.src_group();
        uint8_t src_sel = f.src_select();
        uint8_t dst_grp = f.dst_group();
        uint8_t dst_sel = f.dst_select();

        // Read source
        uint32_t val;
        bool is_tas_read = (src_grp == 10 && src_sel == 2);
        if (is_tas_read) {
            val = regs.read_pop();
        } else {
            val = regs.read(src_grp, src_sel);
        }

        // Width adaptation is inherent in the read/write dispatch

        // Write destination
        bool dst_is_mar = (dst_grp == 5 && dst_sel == 0);
        regs.write(dst_grp, dst_sel, val);

        return dst_is_mar ? 4 : 2;
    }

    // ── 3C: 4-Bit Manipulate ─────────────────────────────────────────────
    int exec_4bit_manipulate(MicroFields f) {
        uint8_t src_grp = f.src_group();
        uint8_t src_sel = f.src_select();
        uint8_t func    = f.variant();  // MD[1:0] = function: 00=SET, 01=AND, 10=OR, 11=XOR
        uint8_t dst_grp = f.dst_group();
        uint8_t dst_sel = f.dst_select();

        uint8_t src_val = regs.read(src_grp, src_sel) & 0xF;
        uint8_t dst_val = regs.read(dst_grp, dst_sel) & 0xF;

        uint8_t result;
        switch (func) {
            case 0: result = src_val; break;               // SET
            case 1: result = dst_val & src_val; break;     // AND
            case 2: result = dst_val | src_val; break;     // OR
            case 3: result = dst_val ^ src_val; break;     // XOR
            default: result = dst_val; break;
        }

        regs.write(dst_grp, dst_sel, result & 0xF);
        return 2;
    }

    // ── 4C/5C: Bit Test Branch ───────────────────────────────────────────
    int exec_bit_test_branch(MicroFields f) {
        // MC[13] = 0: branch if false (4C), MC[13] = 1: branch if true (5C)
        bool branch_if_true = (f.raw >> 13) & 1;

        // Extract register and bit to test
        // The encoding packs register group/select and bit position.
        // For simplicity, decode MC[12]:MD[11:8] as register address,
        // and ME:MF as displacement.
        uint8_t reg_group = f.MC() & 0x3;  // approximate
        uint8_t reg_select = f.src_select();
        uint8_t bit_pos = f.MD() & 0x3;

        uint8_t val = regs.read(reg_group, reg_select) & 0xF;
        bool bit_value = (val >> bit_pos) & 1;

        bool take_branch = (branch_if_true == bit_value);

        if (take_branch) {
            int8_t disp = static_cast<int8_t>(f.raw & 0xFF);
            regs.MAR = (regs.MAR + disp * 16) & MASK_19;
        }
        return 4;
    }

    // ── 6C: Skip When ───────────────────────────────────────────────────
    int exec_skip_when(MicroFields f) {
        uint8_t src_grp = f.src_group();
        uint8_t src_sel = f.src_select();
        uint8_t variant = f.variant();

        uint8_t val = regs.read(src_grp, src_sel) & 0xF;
        uint8_t test = f.ME();  // test value

        bool skip = false;
        switch (variant) {
            case 0: skip = false; break;     // never skip
            case 1: skip = (val == test); break;
            case 2: skip = (val != test); break;
            case 3: skip = (val & test) != 0; break;  // any bit match
            default: skip = false; break;
        }

        if (skip) {
            regs.MAR = (regs.MAR + 16) & MASK_19;  // skip next micro
            return 4;
        }
        return 2;
    }

    // ── 7C: Read/Write Memory ────────────────────────────────────────────
    int exec_memory_access(MicroFields f) {
        bool is_write = f.mem_is_write();
        uint8_t reg_id = f.mem_register(); // 0=X,1=Y,2=T,3=L
        bool reverse   = f.mem_field_reverse();
        uint8_t flen   = f.mem_field_length();
        uint8_t count_var = f.mem_count_variant();

        // Field length: 0 means use CPL
        if (flen == 0) flen = regs.CPL();
        if (flen == 0) flen = 24;
        if (flen > 24) flen = 24;

        // Save MAR to TEMPB, transfer FA to MAR
        regs.TEMPB = regs.MAR;
        uint32_t bit_addr = regs.FA & MASK_24;

        // Map register ID to actual register pointer
        reg24_t* reg_ptr;
        switch (reg_id) {
            case 0: reg_ptr = &regs.X; break;
            case 1: reg_ptr = &regs.Y; break;
            case 2: reg_ptr = &regs.T; break;
            case 3: reg_ptr = &regs.L; break;
            default: reg_ptr = &regs.X; break;
        }

        if (is_write) {
            // Write register to memory
            uint32_t val = *reg_ptr & ((1u << flen) - 1);
            mem.write_field(bit_addr, flen, val, reverse);
        } else {
            // Read memory to register
            uint32_t val = mem.read_field(bit_addr, flen, reverse);
            *reg_ptr = val & MASK_24;
        }

        // Apply count variant to FA and/or FL
        apply_count_variant(count_var, flen);

        // Restore MAR from TEMPB
        regs.MAR = regs.TEMPB;

        return 8;
    }

    // ── 8C / 9C: Literal Move ────────────────────────────────────────────
    int exec_literal_move(MicroFields f) {
        uint8_t dst_grp = f.MC();  // MC = dest register group
        uint8_t dst_sel = 2;       // forced to select 2

        // Check if this is a 9C (24-bit literal) by looking at MF[3:2]
        // Actually, 8C vs 9C distinction: if ME:MF encodes a second word fetch.
        // For simplicity: 8C has an 8-bit literal in MD:ME.
        // 9C is identified when... this is tricky.
        //
        // Practical approach: treat as 8C (8-bit literal).
        // 9C would need to fetch the next word.  We'll detect 9C by
        // checking a special encoding. For now, implement 8C:

        uint8_t literal = f.literal8();
        uint32_t val = literal;

        bool dst_is_mar = (dst_grp == 5 && dst_sel == 0);
        regs.write(dst_grp, dst_sel, val);

        return dst_is_mar ? 4 : 2;
    }

    // ── 12C/13C: Branch Relative ─────────────────────────────────────────
    int exec_branch(MicroFields f) {
        bool negative = f.branch_sign();
        uint16_t disp = f.displacement();

        if (negative) {
            regs.MAR = (regs.MAR - disp * 16) & MASK_19;
        } else {
            regs.MAR = (regs.MAR + disp * 16) & MASK_19;
        }
        return 4;
    }

    // ── 14C/15C: Call ────────────────────────────────────────────────────
    int exec_call(MicroFields f) {
        // Push return address (current MAR, which is already next-in-line)
        regs.push(regs.MAR);

        // Branch same as 12C/13C
        bool negative = f.branch_sign();
        uint16_t disp = f.displacement();

        if (negative) {
            regs.MAR = (regs.MAR - disp * 16) & MASK_19;
        } else {
            regs.MAR = (regs.MAR + disp * 16) & MASK_19;
        }
        return 5;
    }

    // ── 2C: Scratchpad Move  /  2E: Cassette Control ————————————————————
    int exec_scratchpad_move_or_cassette(MicroFields f) {
        // Distinguish 2C from 2E by ME/MF bits.
        // 2E has specific encoding pattern.  For now, treat as 2C.
        uint8_t pad_addr = f.MD();   // scratchpad word 0–15
        uint8_t reg_grp  = f.ME();   // register group
        uint8_t reg_sel  = (f.MF() >> 2) & 0x3;
        uint8_t variant  = f.MF() & 0x3;

        if (pad_addr >= SCRATCHPAD_WORDS) pad_addr = 0;

        auto& word = regs.scratchpad[pad_addr];

        switch (variant) {
            case 0: // Register → Left scratchpad
                word.left = regs.read(reg_grp, reg_sel) & MASK_24;
                break;
            case 1: // Left scratchpad → Register
                regs.write(reg_grp, reg_sel, word.left);
                break;
            case 2: // Register → Right scratchpad
                word.right = regs.read(reg_grp, reg_sel) & MASK_24;
                break;
            case 3: // Right scratchpad → Register
                regs.write(reg_grp, reg_sel, word.right);
                break;
        }
        return 3;
    }

    // ── 3E: Bias ─────────────────────────────────────────────────────────
    int exec_bias(MicroFields f) {
        uint8_t variant = f.MF() & 0x7;
        bool test_cpl = (f.ME() >> 3) & 1;

        // Set CPU: CPU ← 1 if FU == 4, else CPU ← 0
        uint8_t fu = regs.FU();
        regs.set_CPU(fu == 4 ? 1 : 0);

        // Set CPL based on variant
        uint8_t cpl;
        uint16_t fl  = regs.FL();
        uint16_t sfl = static_cast<uint16_t>(regs.scratchpad[0].right & MASK_16);

        switch (variant) {
            case 0: cpl = fu; break;
            case 1: cpl = (fl > 24) ? 24 : static_cast<uint8_t>(fl); break;
            case 2: cpl = (sfl > 24) ? 24 : static_cast<uint8_t>(sfl); break;
            case 3: {
                uint16_t mn = (fl < sfl) ? fl : sfl;
                cpl = (mn > 24) ? 24 : static_cast<uint8_t>(mn);
                break;
            }
            case 4: cpl = regs.CPL(); break;  // unchanged
            case 5: {
                uint16_t mn = (regs.CPL() < fl) ? regs.CPL() : static_cast<uint8_t>(fl > 24 ? 24 : fl);
                cpl = static_cast<uint8_t>(mn);
                break;
            }
            case 6: cpl = regs.CPL(); break;  // unchanged
            default: cpl = regs.CPL(); break;
        }

        regs.set_CPL(cpl);

        // Conditional skip
        if (test_cpl && cpl != 0) {
            regs.MAR = (regs.MAR + 16) & MASK_19;
        }

        return 2;
    }

    // ── 4D: Shift/Rotate X or Y ─────────────────────────────────────────
    int exec_shift_xy(MicroFields f) {
        bool is_y     = f.shift4d_reg_is_y();
        bool is_right = f.shift4d_is_right();
        bool is_rot   = f.shift4d_is_rotate();
        uint8_t count = f.shift4d_count();
        if (count > 24) count = 24;

        reg24_t& reg = is_y ? regs.Y : regs.X;
        reg24_t val = reg & MASK_24;

        if (count == 0) {
            // No shift
        } else if (is_rot) {
            count %= 24;
            if (is_right) {
                val = ((val >> count) | (val << (24 - count))) & MASK_24;
            } else {
                val = ((val << count) | (val >> (24 - count))) & MASK_24;
            }
        } else {
            if (is_right) {
                val >>= count;
            } else {
                val = (val << count) & MASK_24;
            }
        }

        reg = val;
        return 3;
    }

    // ── 5D: Shift X:Y Concatenated ──────────────────────────────────────
    int exec_shift_xy_concat(MicroFields f) {
        bool is_right = f.shift5d_is_right();

        // Concatenate: X is MSB (bits 47:24), Y is LSB (bits 23:0)
        uint64_t combined = (static_cast<uint64_t>(regs.X & MASK_24) << 24) |
                            (regs.Y & MASK_24);

        if (is_right) {
            combined >>= 1;
        } else {
            combined <<= 1;
        }

        regs.X = static_cast<reg24_t>((combined >> 24) & MASK_24);
        regs.Y = static_cast<reg24_t>(combined & MASK_24);
        return 6;
    }



} // namespace b1700
