// Burroughs B1700 Emulator — Main Entry Point
#include "processor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace b1700;

static void print_usage() {
    std::puts("Usage: b1700 [options]");
    std::puts("  --trace        Enable execution trace");
    std::puts("  --mem <KB>     Set memory size in kilobytes (default: 64)");
    std::puts("  --load <file>  Load binary file into memory at address 0");
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

        // 1C: src=X (grp 4, sel 0), dst=Y (grp 4, sel 1)
        // MC=4 (src_group), MD = sel0<<2 | variant0 = 0x00,
        // ME=4 (dst_group), MF = sel1<<2 | 0 = 0x04
        // Encoding: 0x4044
        uint16_t micro = 0x4044;
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

        // 8C: MC=dst_group(4), MD:ME = literal 0x42, MF[1:0] = 10 (8C class)
        // 8C forces dest_select = 2 → group 4, select 2 = T register
        // Encoding: 0x4422
        uint16_t micro = 0x4422;
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
        // Use a literal move: 8C to X, literal 0xFF, MF=0x02
        uint16_t bad_micro = 0x4FF2;  // 8C: grp4, lit=0xFF, MF=2
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

        // 1C: SUM → T  (SUM is group 6 select 0, T is group 4 select 2)
        // MC=6(src_grp), MD=0<<2|0=0(src_sel=0,var=0),
        // ME=4(dst_grp), MF=2<<2|0=8(dst_sel=2,var=0)
        // Encoding: 0x6048
        uint16_t sum_to_t = 0x6048;
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

        // addr 2 (bit 16):  X = 0x42 (executed AFTER return)
        uint16_t lit42 = 0x4422;  // 8C: grp4, lit=0x42, MF=2
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
        // 8C to group 5 → select 2 = BR: MC=5, MD:ME=0x99, MF[1:0]=10
        // Encoding: 0x5992
        uint16_t lit99_BR = 0x5992;
        cpu9.mem.write_byte(8, lit99_BR >> 8);
        cpu9.mem.write_byte(9, lit99_BR & 0xFF);

        // addr 10 (bit 80): Return = TAS → MAR(A)
        // 1C: src=TAS (grp 10, sel 2), dst=MAR (grp 5, sel 0)
        // MC=10=0xA, MD=sel2<<2|0=0x08, ME=5, MF=sel0<<2|0=0x00
        // Encoding: 0xA850
        uint16_t ret_micro = 0xA850;
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
    size_t mem_kb = 64;
    const char* load_file = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--trace") == 0) { do_trace = true; }
        else if (std::strcmp(argv[i], "--test") == 0)  { do_test = true; }
        else if (std::strcmp(argv[i], "--run") == 0)   { do_run = true; }
        else if (std::strcmp(argv[i], "--mem") == 0 && i+1 < argc)  { mem_kb = std::atoi(argv[++i]); }
        else if (std::strcmp(argv[i], "--load") == 0 && i+1 < argc) { load_file = argv[++i]; }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(); return 0;
        }
    }

    if (do_test) {
        run_self_test();
        return 0;
    }

    if (!load_file && !do_run) {
        print_usage();
        return 0;
    }

    ProcessorConfig cfg;
    cfg.s_memory_bytes = mem_kb * 1024;
    Processor cpu(cfg);
    cpu.trace_enabled = do_trace;

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
