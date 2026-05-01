// Burroughs B1700 Emulator — Main Entry Point
#include "processor.h"
#include "debugger.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace b1700;

// ── Read a binary file into a vector ─────────────────────────────────
static std::vector<uint8_t> read_binary(const char* path) {
    FILE* fp = std::fopen(path, "rb");
    if (!fp) return {};
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> buf(sz);
    std::fread(buf.data(), 1, sz, fp);
    std::fclose(fp);
    return buf;
}

// ═════════════════════════════════════════════════════════════════════
// GISMO: Interpreter Switching Demo — Phase 14
// ═════════════════════════════════════════════════════════════════════
//
// Demonstrates the B1700's defining feature: dynamically swapping the
// entire instruction set when switching between programs written in
// different S-languages.
//
// Memory layout:
//   Byte 0x0000: Interpreter microcode (overlaid on each switch)
//   Byte 0x0800: S-CALC program (S-code at bit 0x4000)
//   Byte 0x0C00: S-FORT program (S-code at bit 0x6000)
//   Byte 0x1000: S-CALC data stack (bit 0x8000)

struct ProcessState {
    reg24_t X = 0, Y = 0, T = 0, L = 0, FA = 0, BR = 0, FB = 0, LR = 0;
    uint32_t MAR = 0;
    reg16_t M = 0, U = 0;
    reg8_t CP = 0;
    RegisterFile::ScratchpadWord scratchpad[16] = {};
    std::array<reg24_t, RegisterFile::MAX_STACK> a_stack = {};
    uint8_t stack_ptr = 0;
    bool halted = false;
    size_t total_cycles = 0;
};

static void save_state(Processor& cpu, ProcessState& s) {
    s.X = cpu.regs.X;  s.Y = cpu.regs.Y;
    s.T = cpu.regs.T;  s.L = cpu.regs.L;
    s.FA = cpu.regs.FA; s.FB = cpu.regs.FB;
    s.BR = cpu.regs.BR; s.LR = cpu.regs.LR;
    s.MAR = cpu.regs.MAR;
    s.M = cpu.regs.M;  s.U = cpu.regs.U;
    s.CP = cpu.regs.CP;
    for (int i = 0; i < 16; i++) s.scratchpad[i] = cpu.regs.scratchpad[i];
    s.a_stack = cpu.regs.a_stack;
    s.stack_ptr = cpu.regs.stack_ptr;
    s.halted = cpu.regs.halted;
    s.total_cycles += cpu.cycles;
}

static void restore_state(Processor& cpu, const ProcessState& s) {
    cpu.regs.X = s.X;  cpu.regs.Y = s.Y;
    cpu.regs.T = s.T;  cpu.regs.L = s.L;
    cpu.regs.FA = s.FA; cpu.regs.FB = s.FB;
    cpu.regs.BR = s.BR; cpu.regs.LR = s.LR;
    cpu.regs.MAR = s.MAR;
    cpu.regs.M = s.M;  cpu.regs.U = s.U;
    cpu.regs.CP = s.CP;
    for (int i = 0; i < 16; i++) cpu.regs.scratchpad[i] = s.scratchpad[i];
    cpu.regs.a_stack = s.a_stack;
    cpu.regs.stack_ptr = s.stack_ptr;
    cpu.regs.halted = s.halted;
    cpu.cycles = 0;
}

static int run_gismo(const char* scalc_interp_path, const char* sfort_interp_path,
                     const char* scalc_scode_path, const char* sfort_scode_path,
                     size_t quantum, bool trace) {
    auto scalc_interp = read_binary(scalc_interp_path);
    auto sfort_interp = read_binary(sfort_interp_path);
    auto scalc_scode  = read_binary(scalc_scode_path);
    auto sfort_scode  = read_binary(sfort_scode_path);

    if (scalc_interp.empty() || sfort_interp.empty() ||
        scalc_scode.empty()  || sfort_scode.empty()) {
        std::fputs("Error: could not read one or more Gismo input files\n", stderr);
        return 1;
    }

    ProcessorConfig cfg;
    cfg.s_memory_bytes = 64 * 1024;
    Processor cpu(cfg);
    cpu.trace_enabled = trace;

    // Load both S-code programs (they stay in memory at all times)
    cpu.mem.load_bytes(0x800, scalc_scode.data(), scalc_scode.size());   // bit 0x4000
    cpu.mem.load_bytes(0xC00, sfort_scode.data(), sfort_scode.size());   // bit 0x6000

    // MONITOR handler tagged with current process name
    const char* current_proc = "???";
    cpu.on_monitor = [&current_proc](uint32_t val) {
        std::printf("  [%s] PRINT: %u\n", current_proc, val);
    };

    auto load_interp = [&](const std::vector<uint8_t>& interp) {
        // Clear micro-store region, then load interpreter
        for (size_t i = 0; i < 0x400; i++) cpu.mem.write_byte(i, 0);
        cpu.mem.load_bytes(0, interp.data(), interp.size());
    };

    ProcessState proc_a, proc_b;
    int switches = 0;

    // ── Banner ──────────────────────────────────────────────────────
    std::puts("");
    std::puts("╔═══════════════════════════════════════════════════════════════╗");
    std::puts("║       GISMO — Generalized Interpreter for S-Machine         ║");
    std::puts("║       Operations  (Interpreter Switching Demo)               ║");
    std::puts("╠═══════════════════════════════════════════════════════════════╣");
    std::puts("║  Process A: S-CALC (stack machine)       S-code @ 0x4000    ║");
    std::puts("║  Process B: S-FORT (register machine)    S-code @ 0x6000    ║");
    std::puts("║  Quantum per switch: variable cycles                        ║");
    std::puts("║  Microcode: overlaid at address 0 on each context switch    ║");
    std::puts("╚═══════════════════════════════════════════════════════════════╝");
    std::printf("  S-CALC interp: %zu bytes from %s\n", scalc_interp.size(), scalc_interp_path);
    std::printf("  S-FORT interp: %zu bytes from %s\n", sfort_interp.size(), sfort_interp_path);
    std::printf("  S-CALC S-code: %zu bytes from %s\n", scalc_scode.size(), scalc_scode_path);
    std::printf("  S-FORT S-code: %zu bytes from %s\n", sfort_scode.size(), sfort_scode_path);
    std::printf("  Quantum: %zu cycles\n\n", quantum);

    // ── Phase 1: Initialize S-CALC (run INIT code) ─────────────────
    load_interp(scalc_interp);
    cpu.regs = RegisterFile();
    cpu.regs.MAR = 0;
    cpu.regs.halted = false;
    cpu.cycles = 0;
    current_proc = "S-CALC";
    std::puts("── GISMO: Initializing S-CALC interpreter... ──");
    cpu.run(quantum);
    save_state(cpu, proc_a);
    std::printf("   S-CALC: %zu cycles, MAR=%05X FA=%06X %s\n",
                cpu.cycles, proc_a.MAR, proc_a.FA,
                proc_a.halted ? "[HALTED]" : "");

    // ── Phase 2: Initialize S-FORT (run INIT code) ─────────────────
    load_interp(sfort_interp);
    cpu.regs = RegisterFile();
    cpu.regs.MAR = 0;
    cpu.regs.halted = false;
    cpu.cycles = 0;
    current_proc = "S-FORT";
    std::puts("── GISMO: Initializing S-FORT interpreter... ──");
    cpu.run(quantum);
    save_state(cpu, proc_b);
    std::printf("   S-FORT: %zu cycles, MAR=%05X FA=%06X %s\n",
                cpu.cycles, proc_b.MAR, proc_b.FA,
                proc_b.halted ? "[HALTED]" : "");
    std::puts("");

    // ── Phase 3: Round-robin switching ──────────────────────────────
    while (!proc_a.halted || !proc_b.halted) {
        if (!proc_a.halted) {
            switches++;
            load_interp(scalc_interp);
            restore_state(cpu, proc_a);
            current_proc = "S-CALC";
            std::printf("═══ GISMO switch #%d → S-CALC  (MAR=%05X FA=%06X) ═══\n",
                        switches, proc_a.MAR, proc_a.FA);
            cpu.run(quantum);
            save_state(cpu, proc_a);
            if (proc_a.halted)
                std::printf("   *** S-CALC program HALTED after %zu total cycles ***\n",
                            proc_a.total_cycles);
        }

        if (!proc_b.halted) {
            switches++;
            load_interp(sfort_interp);
            restore_state(cpu, proc_b);
            current_proc = "S-FORT";
            std::printf("═══ GISMO switch #%d → S-FORT  (MAR=%05X FA=%06X) ═══\n",
                        switches, proc_b.MAR, proc_b.FA);
            cpu.run(quantum);
            save_state(cpu, proc_b);
            if (proc_b.halted)
                std::printf("   *** S-FORT program HALTED after %zu total cycles ***\n",
                            proc_b.total_cycles);
        }
    }

    // ── Summary ─────────────────────────────────────────────────────
    std::puts("");
    std::puts("╔═══════════════════════════════════════════════════════════════╗");
    std::puts("║                    GISMO — Run Complete                      ║");
    std::puts("╠═══════════════════════════════════════════════════════════════╣");
    std::printf("║  Context switches:   %-6d                                 ║\n", switches);
    std::printf("║  S-CALC total cycles: %-8zu                               ║\n", proc_a.total_cycles);
    std::printf("║  S-FORT total cycles: %-8zu                               ║\n", proc_b.total_cycles);
    std::puts("║                                                             ║");
    std::puts("║  The same physical CPU executed TWO different instruction    ║");
    std::puts("║  sets — stack-based S-CALC and register-based S-FORT —      ║");
    std::puts("║  by swapping microcode interpreters at each context switch.  ║");
    std::puts("╚═══════════════════════════════════════════════════════════════╝");

    return 0;
}

static void print_usage() {
    std::puts("Usage: b1700 [options]");
    std::puts("  --trace        Enable execution trace");
    std::puts("  --debug        Start interactive debugger");
    std::puts("  --mem <KB>     Set memory size in kilobytes (default: 64)");
    std::puts("  --load <file>  Load binary file into memory at address 0");
    std::puts("  --interp <file> Load interpreter microcode at bit address 0");
    std::puts("  --scode <file>  Load S-code program at bit address 0x4000");
    std::puts("  --gismo <4 files> Run Gismo interpreter switching demo");
    std::puts("                    (scalc_interp sfort_interp scalc_scode sfort_scode)");
    std::puts("  --quantum <N>  Gismo time slice in cycles (default: 300)");
    std::puts("  --run          Start execution at address 0");
    std::puts("  --test         Run built-in self-test");
}

static void run_self_test() {
    std::puts("=== B1700 Emulator Self-Test ===\n");

    ProcessorConfig cfg;
    cfg.s_memory_bytes = 4096;
    Processor cpu(cfg);

    // ── Test 1: NOP & HALT ───────────────────────────────────────────────
    {
        std::puts("[Test 1] NOP + HALT");
        cpu.mem.write_byte(0, 0x00); cpu.mem.write_byte(1, 0x00);  // NOP
        cpu.mem.write_byte(2, 0x00); cpu.mem.write_byte(3, 0x02);  // HALT
        cpu.regs.MAR = 0;
        cpu.regs.halted = false;
        cpu.run(100);
        if (cpu.regs.halted && cpu.cycles >= 4) {
            std::puts("  PASS\n");
        } else {
            std::puts("  FAIL\n");
        }
    }

    // ── Test 2: Register Move (X → Y via 1C) ────────────────────────────
    {
        std::puts("[Test 2] Register Move X -> Y");
        ProcessorConfig cfg2;
        cfg2.s_memory_bytes = 4096;
        Processor cpu2(cfg2);

        cpu2.regs.X = 0x123456;
        cpu2.regs.Y = 0;

        // 1C: src=X (grp 2, sel 0), dst=Y (grp 2, sel 1)
        // MC=1, MD=dst_grp=2, ME[7:6]=dst_sel=1, ME[5:4]=src_sel=0, MF=src_grp=2
        // Encoding: 0x1242
        uint16_t micro = 0x1242;
        cpu2.mem.write_byte(0, micro >> 8);
        cpu2.mem.write_byte(1, micro & 0xFF);
        cpu2.mem.write_byte(2, 0x00);
        cpu2.mem.write_byte(3, 0x02);  // HALT

        cpu2.regs.MAR = 0;
        cpu2.run(100);

        if (cpu2.regs.Y == 0x123456) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (Y = 0x%06X, expected 0x123456)\n\n", cpu2.regs.Y);
        }
    }

    // ── Test 3: 8C Literal Move ──────────────────────────────────────────
    {
        std::puts("[Test 3] 8-Bit Literal Move (8C)");
        ProcessorConfig cfg3;
        cfg3.s_memory_bytes = 4096;
        Processor cpu3(cfg3);

        // 8C: MC=8, MD=dst_grp=2 (T register), literal=0x42
        // Encoding: 0x8242
        uint16_t micro = 0x8242;
        cpu3.mem.write_byte(0, micro >> 8);
        cpu3.mem.write_byte(1, micro & 0xFF);
        cpu3.mem.write_byte(2, 0x00);
        cpu3.mem.write_byte(3, 0x02);  // HALT

        cpu3.regs.MAR = 0;
        cpu3.run(100);

        if (cpu3.regs.T == 0x42) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (T = 0x%06X, expected 0x000042)\n\n", cpu3.regs.T);
        }
    }

    // ── Test 4: Memory Write & Read (7C) ─────────────────────────────────
    {
        std::puts("[Test 4] Memory Write + Read (7C)");
        ProcessorConfig cfg4;
        cfg4.s_memory_bytes = 4096;
        Processor cpu4(cfg4);

        cpu4.regs.X = 0xABCDEF;
        cpu4.regs.FA = 0x200;  // bit address 512 = byte 64
        cpu4.regs.set_CPL(24);

        // 7C WRITE from X, 24 bits, count=0 (no count)
        // MC=0111, direction=1(write), reg=00(X), sign=0,
        // field_len=0 (use CPL=24), count=000
        // bits: 0111 1000 0000 0000 = 0x7800
        uint16_t write_micro = 0x7800;

        // 7C READ to Y, 24 bits, count=0
        // MC=0111, direction=0(read), reg=01(Y), sign=0, len=0, count=000
        // bits: 0111 0010 0000 0000 = 0x7200
        uint16_t read_micro = 0x7200;

        // Program: write X→mem, change FA (no, keep same), read mem→Y, halt
        // But we need to reset FA.  Let's use count variant to not change FA.
        cpu4.mem.write_byte(0, write_micro >> 8);
        cpu4.mem.write_byte(1, write_micro & 0xFF);
        // Now FA might have changed.  Use 8C to reload FA? Too complex.
        // Simpler: use count=0 (no count), FA stays unchanged.
        cpu4.mem.write_byte(2, read_micro >> 8);
        cpu4.mem.write_byte(3, read_micro & 0xFF);
        cpu4.mem.write_byte(4, 0x00);
        cpu4.mem.write_byte(5, 0x02); // HALT

        cpu4.regs.MAR = 0;
        cpu4.run(100);

        if (cpu4.regs.Y == 0xABCDEF) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (Y = 0x%06X, expected 0xABCDEF)\n\n", cpu4.regs.Y);
        }
    }

    // ── Test 5: Branch & Call ────────────────────────────────────────────
    {
        std::puts("[Test 5] Branch Forward (12C)");
        ProcessorConfig cfg5;
        cfg5.s_memory_bytes = 4096;
        Processor cpu5(cfg5);

        // addr 0: Branch forward by 2 words (skip 2 instructions)
        // 12C: MC[15:13]=110, sign=0, displacement=2
        // bits: 1100 0000 0000 0010 = 0xC002
        uint16_t branch = 0xC002;
        cpu5.mem.write_byte(0, branch >> 8);
        cpu5.mem.write_byte(1, branch & 0xFF);

        // addr 2: this should be skipped — set X = 0xFFFFFF (bad)
        // Use a literal move: 8C to group 2 (sel 2 = T), literal 0xFF
        uint16_t bad_micro = 0x82FF;
        cpu5.mem.write_byte(2, bad_micro >> 8);
        cpu5.mem.write_byte(3, bad_micro & 0xFF);

        // addr 4: also skipped
        cpu5.mem.write_byte(4, bad_micro >> 8);
        cpu5.mem.write_byte(5, bad_micro & 0xFF);

        // addr 6: HALT (branch target: 0 + 16 + 2*16 = bit 48 = byte 6)
        cpu5.mem.write_byte(6, 0x00);
        cpu5.mem.write_byte(7, 0x02);

        cpu5.regs.X = 0;
        cpu5.regs.MAR = 0;
        cpu5.run(100);

        if (cpu5.regs.halted && cpu5.regs.X == 0) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (halted=%d, X=0x%06X)\n\n",
                        cpu5.regs.halted, cpu5.regs.X);
        }
    }

    // ── Test 6: Function Box — SUM ───────────────────────────────────────
    {
        std::puts("[Test 6] Function Box SUM (X+Y)");
        ProcessorConfig cfg6;
        cfg6.s_memory_bytes = 4096;
        Processor cpu6(cfg6);

        cpu6.regs.X = 100;
        cpu6.regs.Y = 200;
        cpu6.regs.set_CPL(24);

        // 1C: SUM → T  (SUM is group 3 select 0, T is group 2 select 2)
        // MC=1, MD=dst_grp=2, ME[7:6]=dst_sel=2, ME[5:4]=src_sel=0, MF=src_grp=3
        // Encoding: 0x1283
        uint16_t sum_to_t = 0x1283;
        cpu6.mem.write_byte(0, sum_to_t >> 8);
        cpu6.mem.write_byte(1, sum_to_t & 0xFF);
        cpu6.mem.write_byte(2, 0x00);
        cpu6.mem.write_byte(3, 0x02); // HALT

        cpu6.regs.MAR = 0;
        cpu6.run(100);

        if (cpu6.regs.T == 300) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (T = %u, expected 300)\n\n", cpu6.regs.T);
        }
    }

    // ── Test 7: Bit-addressable memory ───────────────────────────────────
    {
        std::puts("[Test 7] Bit-Addressable Memory");
        ProcessorConfig cfg7;
        cfg7.s_memory_bytes = 4096;
        Processor cpu7(cfg7);

        // Write a 12-bit value at bit address 100
        cpu7.mem.write_field(100, 12, 0xABC);
        uint32_t readback = cpu7.mem.read_field(100, 12);

        if (readback == 0xABC) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (read = 0x%X, expected 0xABC)\n\n", readback);
        }
    }

    // ── Test 8: A-Stack push/pop ─────────────────────────────────────────
    {
        std::puts("[Test 8] A-Stack Push/Pop");
        ProcessorConfig cfg8;
        cfg8.s_memory_bytes = 4096;
        Processor cpu8(cfg8);

        cpu8.regs.push(0x111111);
        cpu8.regs.push(0x222222);
        cpu8.regs.push(0x333333);

        uint32_t v3 = cpu8.regs.pop();
        uint32_t v2 = cpu8.regs.pop();
        uint32_t v1 = cpu8.regs.pop();

        if (v3 == 0x333333 && v2 == 0x222222 && v1 == 0x111111) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (%X %X %X)\n\n", v3, v2, v1);
        }
    }

    // ── Test 9: Call and Return ──────────────────────────────────────────
    {
        std::puts("[Test 9] Call and Return (14C + TAS→MAR)");
        ProcessorConfig cfg9;
        cfg9.s_memory_bytes = 4096;
        Processor cpu9(cfg9);

        // addr 0 (bit 0):   CALL forward by 3 words
        // 14C: MC[15:13]=111, sign=0, displacement=3
        // bits: 1110 0000 0000 0011 = 0xE003
        uint16_t call_micro = 0xE003;
        cpu9.mem.write_byte(0, call_micro >> 8);
        cpu9.mem.write_byte(1, call_micro & 0xFF);

        // addr 2 (bit 16):  T = 0x42 (executed AFTER return)
        uint16_t lit42 = 0x8242;  // 8C: grp2, lit=0x42
        cpu9.mem.write_byte(2, lit42 >> 8);
        cpu9.mem.write_byte(3, lit42 & 0xFF);

        // addr 4 (bit 32):  HALT
        cpu9.mem.write_byte(4, 0x00);
        cpu9.mem.write_byte(5, 0x02);

        // addr 6 (bit 48):  NOP (padding)
        cpu9.mem.write_byte(6, 0x00);
        cpu9.mem.write_byte(7, 0x00);

        // addr 8 (bit 64):  subroutine target (0 + 16 + 3*16 = 64)
        // Set BR = 0x99 (prove subroutine ran)
        // 8C to group 6 → select 2 = BR: MC=8, MD=6, literal=0x99
        // Encoding: 0x8699
        uint16_t lit99_BR = 0x8699;
        cpu9.mem.write_byte(8, lit99_BR >> 8);
        cpu9.mem.write_byte(9, lit99_BR & 0xFF);

        // addr 10 (bit 80): Return = TAS → MAR(A)
        // 1C: src=TAS (grp 11, sel 2), dst=MAR (grp 4, sel 2)
        // MC=1, MD=dst_grp=4, ME[7:6]=dst_sel=2, ME[5:4]=src_sel=2, MF=src_grp=11
        // Encoding: 0x14AB
        uint16_t ret_micro = 0x14AB;
        cpu9.mem.write_byte(10, ret_micro >> 8);
        cpu9.mem.write_byte(11, ret_micro & 0xFF);

        cpu9.regs.MAR = 0;
        cpu9.regs.X = 0;
        cpu9.regs.T = 0;
        cpu9.run(100);

        // After call+return: BR=0x99 (set in subroutine), T=0x42 (set after return)
        if (cpu9.regs.halted && cpu9.regs.BR == 0x99 && cpu9.regs.T == 0x42) {
            std::puts("  PASS\n");
        } else {
            std::printf("  FAIL (halted=%d, BR=0x%X, T=0x%X)\n\n",
                        cpu9.regs.halted, cpu9.regs.BR, cpu9.regs.T);
        }
    }

    std::puts("=== Self-Test Complete ===");
}

int main(int argc, char* argv[]) {
    bool do_trace = false;
    bool do_test  = false;
    bool do_run   = false;
    bool do_debug = false;
    bool do_gismo = false;
    size_t mem_kb = 64;
    size_t quantum = 300;
    const char* load_file   = nullptr;
    const char* interp_file = nullptr;
    const char* scode_file  = nullptr;
    const char* gismo_files[4] = {};  // scalc_interp, sfort_interp, scalc_scode, sfort_scode

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--trace") == 0) { do_trace = true; }
        else if (std::strcmp(argv[i], "--test") == 0)  { do_test = true; }
        else if (std::strcmp(argv[i], "--run") == 0)   { do_run = true; }
        else if (std::strcmp(argv[i], "--debug") == 0)  { do_debug = true; }
        else if (std::strcmp(argv[i], "--mem") == 0 && i+1 < argc)    { mem_kb = std::atoi(argv[++i]); }
        else if (std::strcmp(argv[i], "--quantum") == 0 && i+1 < argc) { quantum = std::atoi(argv[++i]); }
        else if (std::strcmp(argv[i], "--load") == 0 && i+1 < argc)   { load_file = argv[++i]; }
        else if (std::strcmp(argv[i], "--interp") == 0 && i+1 < argc) { interp_file = argv[++i]; }
        else if (std::strcmp(argv[i], "--scode") == 0 && i+1 < argc)  { scode_file = argv[++i]; }
        else if (std::strcmp(argv[i], "--gismo") == 0 && i+4 < argc) {
            do_gismo = true;
            gismo_files[0] = argv[++i];
            gismo_files[1] = argv[++i];
            gismo_files[2] = argv[++i];
            gismo_files[3] = argv[++i];
        }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(); return 0;
        }
    }

    if (do_test) {
        run_self_test();
        return 0;
    }

    if (do_gismo) {
        return run_gismo(gismo_files[0], gismo_files[1],
                         gismo_files[2], gismo_files[3],
                         quantum, do_trace);
    }

    if (!load_file && !interp_file && !do_run && !do_debug) {
        print_usage();
        return 0;
    }

    ProcessorConfig cfg;
    cfg.s_memory_bytes = mem_kb * 1024;
    Processor cpu(cfg);
    cpu.trace_enabled = do_trace;

    // Install an EMVHostControl at port 2 (the standard EMV port/channel
    // used by the cold start loader: T = 0x20 → port 2, channel 0).
    // This provides the minimal EMV protocol responses so the CSL can
    // progress through its initialization and M-load sequence.
    auto emv = std::make_unique<EMVHostControl>(20);

    // Provide minimal M-load card deck:
    //   Card 1: "//M" header card (EBCDIC: / = 0x61, M = 0xD4)
    //   Card 2: "/EN" end sentinel (EBCDIC: / = 0x61, E = 0xC5, N = 0xD5)
    // This tells the CSL "M-load with zero data cards" — it will proceed
    // to the OVERLAY phase immediately.
    std::vector<std::vector<uint8_t>> cards;
    // Card 1: //M header (80-column card image, first 3 bytes = "//M")
    std::vector<uint8_t> mheader(80, 0x40);  // pad with EBCDIC spaces
    mheader[0] = 0x61; mheader[1] = 0x61; mheader[2] = 0xD4; // "//M"
    cards.push_back(std::move(mheader));
    // Card 2: /EN sentinel
    std::vector<uint8_t> en_card(80, 0x40);
    en_card[0] = 0x61; en_card[1] = 0xC5; en_card[2] = 0xD5; // "/EN"
    cards.push_back(std::move(en_card));
    emv->load_cards(std::move(cards));

    cpu.io.install(2, std::move(emv));

    if (load_file) {
        FILE* fp = std::fopen(load_file, "rb");
        if (!fp) {
            std::fprintf(stderr, "Error: cannot open %s\n", load_file);
            return 1;
        }
        std::fseek(fp, 0, SEEK_END);
        long sz = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (sz > 0 && static_cast<size_t>(sz) <= cfg.s_memory_bytes) {
            std::vector<uint8_t> buf(sz);
            std::fread(buf.data(), 1, sz, fp);
            cpu.mem.load_bytes(0, buf.data(), sz);
            std::printf("Loaded %ld bytes from %s\n", sz, load_file);
        }
        std::fclose(fp);
    }

    // ── S-language loader: --interp loads microcode at 0, --scode at 0x4000 ──
    auto load_binary = [&](const char* path, size_t byte_offset, const char* desc) -> bool {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) {
            std::fprintf(stderr, "Error: cannot open %s (%s)\n", path, desc);
            return false;
        }
        std::fseek(fp, 0, SEEK_END);
        long sz = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (sz > 0 && byte_offset + static_cast<size_t>(sz) <= cfg.s_memory_bytes) {
            std::vector<uint8_t> buf(sz);
            std::fread(buf.data(), 1, sz, fp);
            cpu.mem.load_bytes(byte_offset, buf.data(), sz);
            std::printf("  %s: %ld bytes at byte 0x%04zX (bit 0x%04zX) from %s\n",
                        desc, sz, byte_offset, byte_offset * 8, path);
        } else {
            std::fprintf(stderr, "Error: %s too large or offset out of range\n", path);
            std::fclose(fp);
            return false;
        }
        std::fclose(fp);
        return true;
    };

    if (interp_file) {
        std::puts("Loading S-language interpreter + program:");
        if (!load_binary(interp_file, 0, "Interpreter")) return 1;
        if (scode_file) {
            // S-code loaded at byte 0x800 = bit address 0x4000
            if (!load_binary(scode_file, 0x800, "S-code")) return 1;
        }
        // Install S-language MONITOR handler (decimal output)
        cpu.on_monitor = [](uint32_t val) {
            std::printf("[MONITOR] PRINT: %u (0x%06X)\n", val, val);
        };
        // Auto-run if --run not specified
        if (!do_run && !do_debug) do_run = true;
    }

    if (do_debug) {
        cpu.regs.MAR = 0;
        Debugger dbg(cpu);
        dbg.run();
        return 0;
    }

    if (do_run) {
        cpu.regs.MAR = 0;
        std::puts("Starting execution at address 0...");
        cpu.run(10'000'000);
        std::printf("\nExecution %s after %lu cycles.\n",
                    cpu.regs.halted ? "halted" : "timed out",
                    static_cast<unsigned long>(cpu.cycles));
        std::printf("X=%06X Y=%06X T=%06X L=%06X FA=%06X FL=%04X MAR=%05X\n",
                    cpu.regs.X, cpu.regs.Y, cpu.regs.T, cpu.regs.L,
                    cpu.regs.FA, cpu.regs.FL(), cpu.regs.MAR);
    }

    return 0;
}
