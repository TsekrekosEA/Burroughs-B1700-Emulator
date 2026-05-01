#pragma once
// Burroughs B1700 Emulator — Processor Core
// Implements the full micro-instruction decode tree and execution for the B1710.

#include "types.h"
#include "memory.h"
#include "registers.h"
#include "io_bus.h"

#include <cstdio>
#include <functional>

namespace b1700 {

class Processor {
public:
    RegisterFile regs;
    Memory       mem;
    IOBus        io;
    uint64_t     cycles = 0;
    bool         trace_enabled = false;

    // Callback for halt
    std::function<void()> on_halt;

    // Callback for MONITOR instruction (9D) — used for S-language output
    // Called with current X register value.  Default: print to stdout.
    std::function<void(uint32_t)> on_monitor;

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

        // Tick I/O bus and update CC(1) from service requests
        io.tick(cycles);
        if (io.any_service_request()) {
            regs.CC |= 0x04;  // set CC(1) = bus interrupt (MSB-first: bit1=0x04)
        }
        regs.BICN = io.get_bicn();  // update bus status register

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
    // DECODE TREE  —  MC[15:12] IS the opcode (switch on MC)
    // ══════════════════════════════════════════════════════════════════════
    int decode_and_execute(MicroFields f) {
        uint8_t mc = f.MC();
        switch (mc) {
            case 0:  return decode_secondary(f);        // NOP/HALT/D-class/E-class
            case 1:  return exec_register_move(f);      // 1C
            case 2:  return exec_scratchpad_move(f);    // 2C
            case 3:  return exec_4bit_manipulate(f);    // 3C
            case 4:  return exec_bit_test_branch(f, false); // 4C (branch if false)
            case 5:  return exec_bit_test_branch(f, true);  // 5C (branch if true)
            case 6:  return exec_skip_when(f);          // 6C
            case 7:  return exec_memory_access(f);      // 7C
            case 8:  return exec_literal_8(f);          // 8C
            case 9:  return exec_literal_24(f);         // 9C
            case 10: return exec_shift_rotate_t(f);     // 10C
            case 11: return exec_extract_from_t(f);     // 11C
            case 12: case 13: return exec_branch(f);    // 12C/13C
            case 14: case 15: return exec_call(f);      // 14C/15C
        }
        return 2; // unreachable
    }

    // ── Secondary decode (MC=0) ─────────────────────────────────────────
    int decode_secondary(MicroFields f) {
        // F-class: MC=0, MD=0, ME=0 → special instructions
        if (f.raw == 0x0000) return 2;  // NOP
        if (f.raw == 0x0001) {          // OVERLAY: S-memory → M-memory transfer
            // Transfer data from S-memory (FA) to M-memory (L) for FB bits.
            // On the real B1700, this copies the loaded program from S-memory
            // into M-memory and then the M-memory interpreter takes over.
            uint32_t src = regs.FA & MASK_24;
            uint32_t dst = regs.L & MASK_24;
            uint32_t len = regs.FB & MASK_24;
            // At program start, FA=0 and FB=0 — this is the initial pass
            // through the OVERLAY instruction (it's at word 0, the entry point).
            // Skip the overlay and continue to the initialization code.
            // When called from .END after an M-load, FA=SBUF and FB=length.
            if (src != 0 || len != 0) {
                std::printf("[OVERLAY] S-memory %06X → M-memory %06X, %u bits — bootstrap complete\n",
                            src, dst, len);
                regs.halted = true;
                if (on_halt) on_halt();
            }
            return 2;
        }
        if (f.raw == 0x0002) {          // HALT
            regs.halted = true;
            if (on_halt) on_halt();
            return 2;
        }
        if (f.raw == 0x0003) return exec_normalize(f);  // 3F Normalize

        uint8_t md = f.MD();
        if (md != 0) {
            // D-class: decoded by MD
            switch (md) {
                case 0x1: return exec_dispatch(f);
                case 0x2: return 2; // 2E Cassette — NOP for now
                case 0x3: return exec_bias(f);            // 3E in some encodings
                case 0x4: return exec_shift_xy(f);
                case 0x5: return exec_shift_xy_concat(f);
                case 0x6: return exec_count_or_carry(f);
                case 0x7: return exec_exchange_doublepad(f);
                case 0x8: return exec_scratchpad_relate(f);
                case 0x9: // 9D Monitor — S-language output
                    if (on_monitor) {
                        on_monitor(regs.X & MASK_24);
                    } else {
                        std::printf("[MONITOR] X = %u (0x%06X)\n",
                                    regs.X & MASK_24, regs.X & MASK_24);
                    }
                    return 2;
                default:  return 2;
            }
        }

        uint8_t me = f.ME();
        if (me != 0) {
            // E-class: decoded by ME
            switch (me) {
                case 0x1: return exec_dispatch(f);  // 1E DISPATCH
                case 0x3: return exec_bias(f);      // 3E BIAS
                default:  return 2;
            }
        }

        return 2; // unknown MC=0 instruction
    }

    // ══════════════════════════════════════════════════════════════════════
    // MICRO-OPERATOR IMPLEMENTATIONS
    // ══════════════════════════════════════════════════════════════════════

    // ── 1C: Register Move ────────────────────────────────────────────────
    int exec_register_move(MicroFields f) {
        uint8_t src_grp = f.r1c_src_group();
        uint8_t src_sel = f.r1c_src_select();
        uint8_t dst_grp = f.r1c_dst_group();
        uint8_t dst_sel = f.r1c_dst_select();

        // TAS (group 11 sel 2) pops on read
        uint32_t val;
        if (src_grp == 11 && src_sel == 2) {
            val = regs.read_pop();
        } else {
            val = regs.read(src_grp, src_sel);
        }

        regs.write(dst_grp, dst_sel, val);

        // MAR is group 4 sel 2
        bool dst_is_mar = (dst_grp == 4 && dst_sel == 2);
        return dst_is_mar ? 4 : 2;
    }

    // ── 2C: Scratchpad Move ──────────────────────────────────────────────
    int exec_scratchpad_move(MicroFields f) {
        uint8_t pad_addr = f.r2c_pad_addr();
        bool is_right    = f.r2c_is_right();
        bool from_pad    = f.r2c_from_pad();
        uint8_t reg_sel  = f.r2c_reg_select();
        uint8_t reg_grp  = f.r2c_reg_group();

        if (pad_addr >= SCRATCHPAD_WORDS) pad_addr = 0;
        auto& word = regs.scratchpad[pad_addr];

        if (from_pad) {
            uint32_t val = is_right ? word.right : word.left;
            regs.write(reg_grp, reg_sel, val);
        } else {
            uint32_t val = regs.read(reg_grp, reg_sel) & MASK_24;
            if (is_right) word.right = val; else word.left = val;
        }
        return 3;
    }

    // ── 3C: 4-Bit Manipulate ─────────────────────────────────────────────
    int exec_4bit_manipulate(MicroFields f) {
        uint8_t grp     = f.r3c_group();
        uint8_t sel     = f.r3c_select();
        uint8_t variant = f.r3c_variant();
        uint8_t literal = f.r3c_literal();

        uint8_t cur_val = regs.read(grp, sel) & 0xF;
        uint8_t result;
        bool skip = false;

        switch (variant) {
            case 0: result = literal; break;                         // SET
            case 1: result = cur_val & literal; break;               // AND
            case 2: result = cur_val | literal; break;               // OR
            case 3: result = cur_val ^ literal; break;               // EOR
            case 4: result = (cur_val + literal) & 0xF; break;      // INC
            case 5: {                                                // INC-T (skip on overflow)
                uint8_t sum = cur_val + literal;
                result = sum & 0xF;
                skip = (sum > 0xF);
                break;
            }
            case 6: result = (cur_val - literal) & 0xF; break;      // DEC
            case 7: {                                                // DEC-T (skip on underflow)
                result = (cur_val - literal) & 0xF;
                skip = (cur_val < literal);
                break;
            }
            default: result = cur_val; break;
        }

        regs.write(grp, sel, result & 0xF);

        if (skip) {
            regs.MAR = (regs.MAR + 16) & MASK_19;
        }
        return 2;
    }

    // ── 4C/5C: Bit Test Relative Branch ──────────────────────────────────
    int exec_bit_test_branch(MicroFields f, bool branch_if_true) {
        uint8_t grp     = f.r4c_group();
        uint8_t sel     = f.r4c_select();
        uint8_t bit_pos = f.r4c_bit();
        bool    neg     = f.r4c_neg();
        uint8_t disp    = f.r4c_disp();

        uint8_t val = regs.read(grp, sel) & 0xF;
        // B1700 uses MSB-first numbering: bit(0) = MSB = value 8
        bool bit_value = (val >> (3 - bit_pos)) & 1;
        bool take_branch = (branch_if_true == bit_value);

        if (take_branch) {
            if (neg) {
                regs.MAR = (regs.MAR - disp * 16) & MASK_19;
            } else {
                regs.MAR = (regs.MAR + disp * 16) & MASK_19;
            }
        }
        return 4;
    }

    // ── 6C: Skip When ───────────────────────────────────────────────────
    int exec_skip_when(MicroFields f) {
        uint8_t grp     = f.r6c_group();
        uint8_t sel     = f.r6c_select();
        uint8_t variant = f.r6c_variant();
        uint8_t mask    = f.r6c_mask();

        uint8_t val = regs.read(grp, sel) & 0xF;
        bool skip = false;

        switch (variant) {
            case 0: skip = (val & mask) != 0; break;           // any masked bit set
            case 1: skip = (val == mask); break;                // exact match
            case 2: skip = (val == mask); break;                // same as V=1 per manual
            case 3:                                             // any match, then clear
                skip = (val & mask) != 0;
                if (skip) regs.write(grp, sel, val & ~mask);
                break;
            case 4: skip = (val & mask) == 0; break;           // no masked bits set
            case 5: skip = (val != mask); break;                // not equal
            case 6: skip = (val != mask); break;                // same as V=5
            case 7:                                             // no match, then clear
                skip = (val & mask) == 0;
                if (skip) regs.write(grp, sel, val & ~mask);
                break;
        }

        if (skip) {
            regs.MAR = (regs.MAR + 16) & MASK_19;
            return 4;
        }
        return 2;
    }

    // ── 7C: Read/Write Memory ────────────────────────────────────────────
    int exec_memory_access(MicroFields f) {
        bool is_write   = f.r7c_is_write();
        uint8_t reg_id  = f.r7c_register(); // 0=X,1=Y,2=T,3=L
        bool reverse    = f.r7c_reverse();
        uint8_t flen    = f.r7c_field_length();
        uint8_t count_var = f.r7c_count_var();

        if (flen == 0) flen = regs.CPL();
        if (flen == 0) flen = 24;
        if (flen > 24) flen = 24;

        regs.TEMPB = regs.MAR;
        uint32_t bit_addr = regs.FA & MASK_24;

        reg24_t* reg_ptr;
        switch (reg_id) {
            case 0: reg_ptr = &regs.X; break;
            case 1: reg_ptr = &regs.Y; break;
            case 2: reg_ptr = &regs.T; break;
            case 3: reg_ptr = &regs.L; break;
            default: reg_ptr = &regs.X; break;
        }

        if (is_write) {
            uint32_t val = *reg_ptr & ((1u << flen) - 1);
            mem.write_field(bit_addr, flen, val, reverse);
        } else {
            uint32_t val = mem.read_field(bit_addr, flen, reverse);
            *reg_ptr = val & MASK_24;
        }

        apply_count_variant(count_var, flen);
        regs.MAR = regs.TEMPB;
        return 8;
    }

    // ── 8C: Move 8-bit Literal ───────────────────────────────────────────
    int exec_literal_8(MicroFields f) {
        uint8_t dst_grp = f.r8c_group();
        uint8_t literal = f.r8c_literal();

        // 8C always writes to select 2 of the group
        bool dst_is_mar = (dst_grp == 4);  // group 4 sel 2 = MAR(A)
        regs.write(dst_grp, 2, static_cast<uint32_t>(literal));
        return dst_is_mar ? 4 : 2;
    }

    // ── 9C: Move 24-bit Literal ──────────────────────────────────────────
    int exec_literal_24(MicroFields f) {
        uint8_t dst_grp = f.r9c_group();
        uint8_t hi8     = f.r9c_hi8();

        // Fetch the next 16-bit word for the lower 16 bits
        uint16_t lo16 = mem.fetch_micro(regs.MAR);
        regs.MAR = (regs.MAR + 16) & MASK_19;

        uint32_t val = (static_cast<uint32_t>(hi8) << 16) | lo16;

        bool dst_is_mar = (dst_grp == 4);
        regs.write(dst_grp, 2, val);
        return dst_is_mar ? 6 : 4;
    }

    // ── 10C: Shift/Rotate T → Destination ────────────────────────────────
    int exec_shift_rotate_t(MicroFields f) {
        uint8_t dst_grp = f.r10c_dst_group();
        uint8_t dst_sel = f.r10c_dst_select();
        bool is_rotate  = f.r10c_is_rotate();
        uint8_t count   = f.r10c_count();

        if (count == 0) count = regs.CPL();
        if (count == 0) count = 24;
        if (count > 24) count = 24;

        uint32_t val = regs.T & MASK_24;

        if (is_rotate) {
            count %= 24;
            val = ((val << count) | (val >> (24 - count))) & MASK_24;
        } else {
            val = (val << count) & MASK_24;
        }

        regs.write(dst_grp, dst_sel, val);
        return 3;
    }

    // ── 11C: Extract from T ──────────────────────────────────────────────
    int exec_extract_from_t(MicroFields f) {
        uint8_t rotate = f.r11c_rotate();
        uint8_t dst    = f.r11c_dst();  // 0=X, 1=Y, 2=T, 3=L
        uint8_t width  = f.r11c_width();

        if (width == 0) width = regs.CPL();
        if (width == 0) width = 24;
        if (width > 24) width = 24;
        if (rotate > 24) rotate = 0;

        uint32_t val = regs.T & MASK_24;
        if (rotate > 0) {
            val = ((val >> rotate) | (val << (24 - rotate))) & MASK_24;
        }
        uint32_t mask = (width >= 24) ? MASK_24 : ((1u << width) - 1);
        val &= mask;

        // Write to group 2: X(0), Y(1), T(2), L(3)
        regs.write(2, dst, val);
        return 4;
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
        regs.push(regs.MAR);
        bool negative = f.branch_sign();
        uint16_t disp = f.displacement();

        if (negative) {
            regs.MAR = (regs.MAR - disp * 16) & MASK_19;
        } else {
            regs.MAR = (regs.MAR + disp * 16) & MASK_19;
        }
        return 5;
    }

    // ── DISPATCH: I/O Bus Control (MC=0000, MD=0001) ─────────────────────
    int exec_dispatch(MicroFields f) {
        // ME[7:4] encodes variant:
        //   0 = LOCK + SKIP WHEN UNLOCKED
        //   1 = WRITE (initiate I/O operation via descriptor in L)
        //   2 = READ AND CLEAR (read I/O result, clear interrupt)
        uint8_t variant = f.ME();
        switch (variant) {
            case 0: // DISPATCH LOCK SKIP WHEN UNLOCKED
                if (!io.locked()) {
                    io.lock();
                    // Lock succeeded — skip next instruction
                    regs.MAR = (regs.MAR + 16) & MASK_19;
                } // else: bus busy, fall through (execute next = typically HALT)
                break;

            case 1: { // DISPATCH WRITE — initiate I/O operation
                // The I/O descriptor address is in L register.
                // T register contains port/channel selection:
                //   T[7:4] = port number, T[3:0] = channel
                // In the cold start loader: T = 0x20 = port 2, channel 0.
                uint32_t desc_addr = regs.L;
                int port = (regs.T >> 4) & 0xF;

                // Read the full 7-word I/O descriptor from memory
                IODescriptor desc = read_descriptor(mem, desc_addr);

                if (trace_enabled) {
                    std::printf("  DISPATCH WRITE port=%d op=%06X ria=%06X "
                                "A=%06X B=%06X link=%06X\n",
                                port, desc.op_word, desc.ria,
                                desc.a_addr, desc.b_addr, desc.link);
                }

                // Send the descriptor to the addressed I/O control
                io.dispatch_write(port, desc, mem);

                // Unlock the bus after initiating
                io.unlock();
                break;
            }

            case 2: { // DISPATCH READ AND CLEAR — read result, clear interrupt
                // Find which port has a service request
                int port = io.find_requesting_port();
                if (port >= 0) {
                    auto* ctrl = io.get(port);
                    if (ctrl) {
                        // Set L = RIA + 23 (last address of the RSW field).
                        // The CSL reads the RSW with READ 24 BITS REVERSE from L,
                        // which reads addresses [L, L-1, ..., L-23] = [RIA+23..RIA].
                        uint32_t ria = ctrl->completed_ria();
                        regs.L = (ria + 23) & MASK_24;

                        // T = port/channel status (bit 23 = access error)
                        regs.T = (static_cast<uint32_t>(port) << 20) & MASK_24;

                        if (trace_enabled) {
                            std::printf("  DISPATCH READ AND CLEAR port=%d ria=%06X L=%06X\n",
                                        port, ria, regs.L);
                        }

                        ctrl->clear_service_request();
                    }
                } else {
                    // No service request — T(23) = 1 (access error)
                    // B1700 uses MSB-first bit numbering, so bit 23 is the LSB.
                    regs.T = 0x000001;
                }

                // Clear CC(1) bus interrupt bit (MSB-first: bit1=0x04)
                regs.CC &= ~0x04;
                break;
            }

            default:
                break; // unknown variant — NOP
        }
        return 2;
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
        bool is_y     = f.r4d_is_y();
        bool is_right = f.r4d_is_right();
        bool is_rot   = f.r4d_is_rot();
        uint8_t count = f.r4d_count();
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
        bool is_right = f.r5d_is_right();

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

    // ── 6D: Count FA/FL  /  6E: Carry FF ────────────────────────────────
    int exec_count_or_carry(MicroFields f) {
        // Distinguish 6D from 6E by further decode.
        // 6E: MC=0000 MD=0110 ME=0000 MF=variant
        // 6D: MC=0000 MD=0110 ME[7:5]=count_var ME[4:0]:MF=literal
        // If ME == 0 and MF has carry bits, it's 6E.
        // Actually, re-reading: 6E has MD=0110 and specific pattern.
        // Let's check: if the top of ME is 0 and the variant bits are in MF:
        uint8_t me = f.ME();
        uint8_t mf = f.MF();

        // Heuristic: 6E is when ME == 0 (no count variant used)
        if (me == 0) {
            return exec_carry_ff(f);
        }

        // 6D: Count FA/FL
        uint8_t count_var = (me >> 1) & 0x7;  // variant in ME[3:1]
        uint8_t literal = ((me & 1) << 4) | mf;
        if (literal == 0) literal = regs.CPL();

        apply_count_variant(count_var, literal);
        return 4;
    }

    int exec_carry_ff(MicroFields f) {
        uint8_t v = f.MF();
        uint8_t cpl = regs.CPL();
        if (cpl == 0) cpl = 24;
        uint32_t mask = (cpl >= 24) ? MASK_24 : ((1u << cpl) - 1);
        uint32_t mx = regs.X & mask;
        uint32_t my = regs.Y & mask;

        if (v & 1) regs.set_CYF(0);       // clear
        if (v & 2) regs.set_CYF(1);       // set
        if (v & 4) {
            // CYF ← CYL (carry from add)
            uint32_t sum = mx + my;
            bool carry = sum > mask;
            regs.set_CYF(carry ? 1 : 0);
        }
        if (v & 8) {
            // CYD: if X≠Y then 1, else CYF
            if (mx != my)
                regs.set_CYF(1);
            // else CYF unchanged
        }
        return 2;
    }

    // ── 7D: Exchange Doublepad Word ──────────────────────────────────────
    int exec_exchange_doublepad(MicroFields f) {
        uint8_t src_pad = f.ME();
        uint8_t dst_pad = f.MF();
        if (src_pad >= SCRATCHPAD_WORDS) src_pad = 0;
        if (dst_pad >= SCRATCHPAD_WORDS) dst_pad = 0;

        // Save FA:FB
        reg24_t save_fa = regs.FA;
        reg24_t save_fb = regs.FB;

        // Load from source scratchpad
        regs.FA = regs.scratchpad[src_pad].left;
        regs.FB = regs.scratchpad[src_pad].right;

        // Store saved values to destination
        regs.scratchpad[dst_pad].left  = save_fa;
        regs.scratchpad[dst_pad].right = save_fb;

        return 4;
    }

    // ── 8D: Scratchpad Relate ────────────────────────────────────────────
    int exec_scratchpad_relate(MicroFields f) {
        uint8_t pad_addr = f.ME();
        bool negative = (f.MF() >> 3) & 1;
        if (pad_addr >= SCRATCHPAD_WORDS) pad_addr = 0;

        reg24_t pad_val = regs.scratchpad[pad_addr].left;
        if (negative)
            regs.FA = (regs.FA - pad_val) & MASK_24;
        else
            regs.FA = (regs.FA + pad_val) & MASK_24;

        return 4;
    }

    // ── 3F: Normalize X ──────────────────────────────────────────────────
    int exec_normalize(MicroFields /*f*/) {
        uint8_t cpl = regs.CPL();
        if (cpl == 0) cpl = 24;
        uint16_t fl = regs.FL();
        int clks = 0;

        // Shift X left while FL > 0 and the bit at CPL position is 0
        uint32_t check_bit = 1u << (cpl - 1);

        if (regs.X & check_bit) {
            return 4;  // already normalized
        }

        while (fl > 0 && !(regs.X & check_bit)) {
            regs.X = (regs.X << 1) & MASK_24;
            fl--;
            clks += 6;
        }

        regs.set_FL(fl);
        return clks + (fl == 0 ? 2 : 4);
    }

    // ══════════════════════════════════════════════════════════════════════
    // HELPERS
    // ══════════════════════════════════════════════════════════════════════

    void apply_count_variant(uint8_t v, uint8_t amount) {
        // 7 variants for counting FA and/or FL
        if (v & 0x1) {
            // bit 0: FA up (for v=001,011) or FA down (for v=100,101,111)
            // Actually the encoding is:
            // 000=none, 001=FA+, 010=FL+, 011=FA+ FL-,
            // 100=FA- FL+, 101=FA-, 110=FL-, 111=FA- FL-
        }

        switch (v) {
            case 0: break;
            case 1: regs.FA = (regs.FA + amount) & MASK_24; break;
            case 2: {
                uint16_t fl = regs.FL();
                regs.set_FL(fl + amount);
                break;
            }
            case 3:
                regs.FA = (regs.FA + amount) & MASK_24;
                {
                    uint16_t fl = regs.FL();
                    if (fl >= amount) regs.set_FL(fl - amount);
                    else regs.set_FL(0);
                }
                break;
            case 4: {
                regs.FA = (regs.FA - amount) & MASK_24;
                uint16_t fl = regs.FL();
                regs.set_FL(fl + amount);
                break;
            }
            case 5:
                regs.FA = (regs.FA - amount) & MASK_24;
                break;
            case 6: {
                uint16_t fl = regs.FL();
                if (fl >= amount) regs.set_FL(fl - amount);
                else regs.set_FL(0);
                break;
            }
            case 7: {
                regs.FA = (regs.FA - amount) & MASK_24;
                uint16_t fl = regs.FL();
                if (fl >= amount) regs.set_FL(fl - amount);
                else regs.set_FL(0);
                break;
            }
        }
    }

    void trace_log(uint32_t pc, uint16_t micro, int clks) {
        std::printf("CYCLE %08lu  MAR=0x%05X  M=0x%04X  [%d clks]  "
                    "X=%06X Y=%06X T=%06X L=%06X FA=%06X FL=%04X\n",
                    static_cast<unsigned long>(cycles),
                    pc, micro, clks,
                    regs.X & MASK_24, regs.Y & MASK_24,
                    regs.T & MASK_24, regs.L & MASK_24,
                    regs.FA & MASK_24, regs.FL());
    }
};

} // namespace b1700
