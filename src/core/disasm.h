#pragma once
// Burroughs B1700 Emulator — Micro-Instruction Disassembler
// Decodes 16-bit micro-words back into human-readable MIL mnemonics.
//
// The B1710 instruction format uses MC[15:12] as the primary opcode,
// with secondary decode via MD, ME, and MF fields depending on the class.

#include "types.h"
#include "registers.h"
#include <cstdint>
#include <string>
#include <cstdio>

namespace b1700 {

// ══════════════════════════════════════════════════════════════════════════
// REGISTER NAME TABLES
// ══════════════════════════════════════════════════════════════════════════

// Group/Select → register name (from Table I-3)
inline const char* reg_name(uint8_t grp, uint8_t sel) {
    // Group 0: Program Flow
    if (grp == 0) {
        switch (sel) { case 0: return "FA"; case 1: return "FB"; case 2: return "MAR"; case 3: return "BKR"; }
    }
    // Group 1: S-memory pointers
    if (grp == 1) {
        switch (sel) { case 0: return "MAXS"; case 1: return "MAXM"; case 2: return "MINS"; case 3: return "MINM"; }
    }
    // Group 2: Data registers
    if (grp == 2) {
        switch (sel) { case 0: return "X"; case 1: return "Y"; case 2: return "T"; case 3: return "L"; }
    }
    // Group 3: Interrupt
    if (grp == 3) {
        switch (sel) { case 0: return "TSA"; case 1: return "TSQ"; case 2: return "IMA"; case 3: return "IMB"; }
    }
    // Group 4: 1C src/dst or 8C dst
    if (grp == 4) {
        switch (sel) { case 0: return "X"; case 1: return "Y"; case 2: return "T"; case 3: return "L"; }
    }
    // Group 5: Flow 2
    if (grp == 5) {
        switch (sel) { case 0: return "MAR"; case 1: return "TOPM"; case 2: return "BR"; case 3: return "CCR"; }
    }
    // Group 6: Function Box outputs (read-only)
    if (grp == 6) {
        switch (sel) { case 0: return "SUM"; case 1: return "CMPX"; case 2: return "CMPY"; case 3: return "XANY"; }
    }
    // Group 7: Function Box 2
    if (grp == 7) {
        switch (sel) { case 0: return "XEQY"; case 1: return "MSKX"; case 2: return "MSKY"; case 3: return "XORY"; }
    }
    // Group 8: Function Box 3
    if (grp == 8) {
        switch (sel) { case 0: return "DIFF"; case 1: return "BDIF"; case 2: return "BSUM"; case 3: return "BCMP"; }
    }
    // Group 9: Counters
    if (grp == 9) {
        switch (sel) { case 0: return "FA"; case 1: return "FB"; case 2: return "FL"; case 3: return "FU"; }
    }
    // Group 10: Conditions
    if (grp == 10) {
        switch (sel) { case 0: return "XYCN"; case 1: return "FLCN"; case 2: return "TAS"; case 3: return "XYST"; }
    }
    // Group 11: Stack
    if (grp == 11) {
        switch (sel) { case 0: return "CC"; case 1: return "BICN"; case 2: return "TAS"; case 3: return "STKD"; }
    }
    // Group 12: Control
    if (grp == 12) {
        switch (sel) { case 0: return "CPL"; case 1: return "CYF"; case 2: return "CPU"; case 3: return "M"; }
    }
    // Groups 13-15: typically aliases or reserved
    static char buf[16];
    snprintf(buf, sizeof(buf), "G%d.S%d", grp, sel);
    return buf;
}

// ══════════════════════════════════════════════════════════════════════════
// DISASSEMBLE ONE MICRO-INSTRUCTION
// ══════════════════════════════════════════════════════════════════════════

struct DisasmResult {
    std::string mnemonic;     // e.g. "MOVE X TO Y"
    std::string operands;     // expanded operand details
    int words;                // instruction length: 1 or 2 (for 9C)
};

inline DisasmResult disassemble(uint16_t micro, uint16_t next_word = 0) {
    MicroFields f{micro};
    DisasmResult r;
    r.words = 1;
    char buf[128];

    uint8_t mc = f.MC();

    switch (mc) {
    case 0: {
        // Secondary decode
        if (micro == 0x0000) { r.mnemonic = "NOP"; return r; }
        if (micro == 0x0001) { r.mnemonic = "OVERLAY"; return r; }
        if (micro == 0x0002) { r.mnemonic = "HALT"; return r; }
        if (micro == 0x0003) { r.mnemonic = "NORMALIZE"; return r; }

        uint8_t md = f.MD();
        uint8_t me = f.ME();
        uint8_t mf = f.MF();

        if (md == 0x1 || (md == 0 && me == 0x1)) {
            // DISPATCH
            uint8_t variant = (md == 0x1) ? me : me;
            if (md == 0) variant = me;
            else variant = me;
            switch (variant) {
                case 0: r.mnemonic = "DISPATCH LOCK SKIP WHEN UNLOCKED"; break;
                case 1: r.mnemonic = "DISPATCH WRITE"; break;
                case 2: r.mnemonic = "DISPATCH READ AND CLEAR"; break;
                default:
                    snprintf(buf, sizeof(buf), "DISPATCH V=%d", variant);
                    r.mnemonic = buf;
            }
            return r;
        }

        if (md == 0x3 || (md == 0 && me == 0x3)) {
            snprintf(buf, sizeof(buf), "BIAS V=%d", mf & 0x7);
            r.mnemonic = buf;
            return r;
        }

        if (md == 0x4) {
            bool is_y = (me >> 3) & 1;
            bool is_right = (me >> 2) & 1;
            bool is_rot = (me >> 1) & 1;
            uint8_t count = ((me & 1) << 4) | mf;
            snprintf(buf, sizeof(buf), "%s %s %s %d",
                     is_rot ? "ROTATE" : "SHIFT", is_y ? "Y" : "X",
                     is_right ? "RIGHT" : "LEFT", count);
            r.mnemonic = buf;
            return r;
        }

        if (md == 0x5) {
            snprintf(buf, sizeof(buf), "SHIFT X:Y %s 1",
                     (me & 1) ? "RIGHT" : "LEFT");
            r.mnemonic = buf;
            return r;
        }

        if (md == 0x6) {
            if (me == 0) {
                snprintf(buf, sizeof(buf), "CARRY FF V=%d", mf);
                r.mnemonic = buf;
            } else {
                uint8_t cv = (me >> 1) & 0x7;
                uint8_t lit = ((me & 1) << 4) | mf;
                const char* cv_names[] = {"NONE","FA+","FL+","FA+ FL-",
                                          "FA- FL+","FA-","FL-","FA- FL-"};
                snprintf(buf, sizeof(buf), "COUNT %s %d", cv_names[cv], lit);
                r.mnemonic = buf;
            }
            return r;
        }

        if (md == 0x7) {
            snprintf(buf, sizeof(buf), "EXCHANGE DOUBLEPAD [%d] [%d]", me, mf);
            r.mnemonic = buf;
            return r;
        }

        if (md == 0x8) {
            bool neg = (mf >> 3) & 1;
            snprintf(buf, sizeof(buf), "SCRATCHPAD RELATE [%d] %s",
                     me, neg ? "SUBTRACT" : "ADD");
            r.mnemonic = buf;
            return r;
        }

        if (md == 0x9) {
            r.mnemonic = "MONITOR";
            return r;
        }

        snprintf(buf, sizeof(buf), "MC=0 MD=%X ME=%X MF=%X", md, me, mf);
        r.mnemonic = buf;
        return r;
    }

    case 1: {
        // 1C: Register Move
        uint8_t sg = f.r1c_src_group();
        uint8_t ss = f.r1c_src_select();
        uint8_t dg = f.r1c_dst_group();
        uint8_t ds = f.r1c_dst_select();
        snprintf(buf, sizeof(buf), "MOVE %s TO %s",
                 reg_name(sg, ss), reg_name(dg, ds));
        r.mnemonic = buf;
        return r;
    }

    case 2: {
        // 2C: Scratchpad Move
        uint8_t pad = f.r2c_pad_addr();
        bool is_right = f.r2c_is_right();
        bool from_pad = f.r2c_from_pad();
        uint8_t sel = f.r2c_reg_select();
        uint8_t grp = f.r2c_reg_group();
        if (from_pad) {
            snprintf(buf, sizeof(buf), "LOAD %s FROM SCRATCHPAD[%d].%s",
                     reg_name(grp, sel), pad, is_right ? "R" : "L");
        } else {
            snprintf(buf, sizeof(buf), "STORE %s INTO SCRATCHPAD[%d].%s",
                     reg_name(grp, sel), pad, is_right ? "R" : "L");
        }
        r.mnemonic = buf;
        return r;
    }

    case 3: {
        // 3C: 4-bit Manipulate
        uint8_t grp = f.r3c_group();
        uint8_t sel = f.r3c_select();
        uint8_t var = f.r3c_variant();
        uint8_t lit = f.r3c_literal();
        const char* ops[] = {"SET","AND","OR","EOR","INC","INC-T","DEC","DEC-T"};
        snprintf(buf, sizeof(buf), "%s %s %d",
                 ops[var & 7], reg_name(grp, sel), lit);
        r.mnemonic = buf;
        return r;
    }

    case 4:
    case 5: {
        // 4C/5C: Bit Test Relative Branch
        uint8_t grp = f.r4c_group();
        uint8_t sel = f.r4c_select();
        uint8_t bit = f.r4c_bit();
        bool neg = f.r4c_neg();
        uint8_t disp = f.r4c_disp();
        snprintf(buf, sizeof(buf), "IF BIT(%d) OF %s %s GO %s%d",
                 bit, reg_name(grp, sel),
                 mc == 5 ? "SET" : "CLEAR",
                 neg ? "-" : "+", disp);
        r.mnemonic = buf;
        return r;
    }

    case 6: {
        // 6C: Skip When
        uint8_t grp = f.r6c_group();
        uint8_t sel = f.r6c_select();
        uint8_t var = f.r6c_variant();
        uint8_t mask = f.r6c_mask();
        const char* conds[] = {"ANY SET","EQUAL","EQUAL2","ANY SET THEN CLEAR",
                               "NO SET","NOT EQUAL","NOT EQUAL2","NO SET THEN CLEAR"};
        snprintf(buf, sizeof(buf), "SKIP WHEN %s %s 0x%X",
                 reg_name(grp, sel), conds[var & 7], mask);
        r.mnemonic = buf;
        return r;
    }

    case 7: {
        // 7C: Memory Access
        bool wr = f.r7c_is_write();
        uint8_t reg = f.r7c_register();
        bool rev = f.r7c_reverse();
        uint8_t flen = f.r7c_field_length();
        uint8_t cv = f.r7c_count_var();
        const char* regs[] = {"X","Y","T","L"};
        const char* cv_names[] = {"","FA+","FL+","FA+ FL-",
                                  "FA- FL+","FA-","FL-","FA- FL-"};
        snprintf(buf, sizeof(buf), "%s %s %d BITS%s%s",
                 wr ? "WRITE" : "READ", regs[reg & 3],
                 flen == 0 ? 0 : flen,
                 rev ? " REVERSE" : "",
                 cv ? (std::string(" ") + cv_names[cv]).c_str() : "");
        r.mnemonic = buf;
        return r;
    }

    case 8: {
        // 8C: 8-bit Literal
        uint8_t grp = f.r8c_group();
        uint8_t lit = f.r8c_literal();
        snprintf(buf, sizeof(buf), "SET %s = 0x%02X (%d)",
                 reg_name(grp, 2), lit, lit);
        r.mnemonic = buf;
        return r;
    }

    case 9: {
        // 9C: 24-bit Literal (two-word instruction)
        uint8_t grp = f.r9c_group();
        uint8_t hi8 = f.r9c_hi8();
        uint32_t val = (static_cast<uint32_t>(hi8) << 16) | next_word;
        r.words = 2;
        snprintf(buf, sizeof(buf), "SET %s = 0x%06X (%u)",
                 reg_name(grp, 2), val, val);
        r.mnemonic = buf;
        return r;
    }

    case 10: {
        // 10C: Shift/Rotate T
        uint8_t dg = f.r10c_dst_group();
        uint8_t ds = f.r10c_dst_select();
        bool rot = f.r10c_is_rotate();
        uint8_t cnt = f.r10c_count();
        snprintf(buf, sizeof(buf), "%s T LEFT %d TO %s",
                 rot ? "ROTATE" : "SHIFT", cnt, reg_name(dg, ds));
        r.mnemonic = buf;
        return r;
    }

    case 11: {
        // 11C: Extract from T
        uint8_t rot = f.r11c_rotate();
        uint8_t dst = f.r11c_dst();
        uint8_t width = f.r11c_width();
        const char* dests[] = {"X","Y","T","L"};
        snprintf(buf, sizeof(buf), "EXTRACT %d BITS AT %d FROM T TO %s",
                 width, rot, dests[dst & 3]);
        r.mnemonic = buf;
        return r;
    }

    case 12:
    case 13: {
        // 12C/13C: Branch
        bool neg = f.branch_sign();
        uint16_t disp = f.displacement();
        snprintf(buf, sizeof(buf), "GO TO %s%d",
                 neg ? "-" : "+", disp);
        r.mnemonic = buf;
        return r;
    }

    case 14:
    case 15: {
        // 14C/15C: Call
        bool neg = f.branch_sign();
        uint16_t disp = f.displacement();
        snprintf(buf, sizeof(buf), "CALL %s%d",
                 neg ? "-" : "+", disp);
        r.mnemonic = buf;
        return r;
    }

    default:
        snprintf(buf, sizeof(buf), "??? [%04X]", micro);
        r.mnemonic = buf;
        return r;
    }
}

// Disassemble a range of memory, returning a formatted string
inline std::string disassemble_range(const Memory& mem, uint32_t start_addr,
                                      int count) {
    std::string result;
    char line[256];
    uint32_t addr = start_addr;

    for (int i = 0; i < count; ++i) {
        uint16_t micro = mem.fetch_micro(addr);
        uint16_t next = (addr + 16 < 0x80000) ? mem.fetch_micro(addr + 16) : 0;
        auto d = disassemble(micro, next);

        if (d.words == 2) {
            snprintf(line, sizeof(line), "  %05X: %04X %04X  %s\n",
                     addr, micro, next, d.mnemonic.c_str());
            addr += 32;
            i++;  // skip next word
        } else {
            snprintf(line, sizeof(line), "  %05X: %04X       %s\n",
                     addr, micro, d.mnemonic.c_str());
            addr += 16;
        }
        result += line;
    }
    return result;
}

} // namespace b1700
