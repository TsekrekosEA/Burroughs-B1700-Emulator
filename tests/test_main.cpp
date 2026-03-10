// Burroughs B1700 Emulator — Unit Tests
// Standalone tests (no framework dependency).
// Returns 0 on success, 1 on failure.

#include "processor.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace b1700;

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::printf("  %-50s ", name); \
    } while(0)

#define PASS() \
    do { tests_passed++; std::puts("PASS"); } while(0)

#define FAIL(fmt, ...) \
    do { tests_failed++; std::printf("FAIL: " fmt "\n", ##__VA_ARGS__); } while(0)

// ══════════════════════════════════════════════════════════════════════════
// MEMORY TESTS
// ══════════════════════════════════════════════════════════════════════════

static void test_memory_byte_access() {
    TEST("memory: byte read/write");
    Memory mem(1024);
    mem.write_byte(0, 0xAB);
    mem.write_byte(1, 0xCD);
    if (mem.read_byte(0) == 0xAB && mem.read_byte(1) == 0xCD) PASS();
    else FAIL("byte read mismatch");
}

static void test_memory_bit_field_aligned() {
    TEST("memory: aligned 8-bit field");
    Memory mem(1024);
    mem.write_field(0, 8, 0xA5);
    uint32_t v = mem.read_field(0, 8);
    if (v == 0xA5) PASS();
    else FAIL("got 0x%X, expected 0xA5", v);
}

static void test_memory_bit_field_unaligned() {
    TEST("memory: unaligned 12-bit field at bit 5");
    Memory mem(1024);
    mem.write_field(5, 12, 0xABC);
    uint32_t v = mem.read_field(5, 12);
    if (v == 0xABC) PASS();
    else FAIL("got 0x%X, expected 0xABC", v);
}

static void test_memory_24bit_field() {
    TEST("memory: 24-bit field at bit 0");
    Memory mem(1024);
    mem.write_field(0, 24, 0xDEADBE);
    uint32_t v = mem.read_field(0, 24);
    if (v == 0xDEADBE) PASS();
    else FAIL("got 0x%X, expected 0xDEADBE", v);
}

static void test_memory_bit_field_reverse() {
    TEST("memory: reverse-direction field");
    Memory mem(1024);
    // Write 4 bits starting at bit 10, going backward
    mem.write_field(10, 4, 0xA, true);  // bits at 10,9,8,7
    uint32_t v = mem.read_field(10, 4, true);
    if (v == 0xA) PASS();
    else FAIL("got 0x%X, expected 0xA", v);
}

static void test_memory_micro_fetch() {
    TEST("memory: microinstruction fetch");
    Memory mem(1024);
    mem.write_byte(0, 0xC0);
    mem.write_byte(1, 0x03);
    uint16_t micro = mem.fetch_micro(0);
    if (micro == 0xC003) PASS();
    else FAIL("got 0x%04X, expected 0xC003", micro);
}

// ══════════════════════════════════════════════════════════════════════════
// REGISTER TESTS
// ══════════════════════════════════════════════════════════════════════════

static void test_register_read_write_basic() {
    TEST("registers: basic X/Y/T/L read/write");
    RegisterFile r;
    r.write(2, 0, 0x123456);  // X
    r.write(2, 1, 0xABCDEF);  // Y
    r.write(2, 2, 0x111111);  // T
    r.write(2, 3, 0x222222);  // L
    bool ok = r.read(2, 0) == 0x123456 &&
              r.read(2, 1) == 0xABCDEF &&
              r.read(2, 2) == 0x111111 &&
              r.read(2, 3) == 0x222222;
    if (ok) PASS(); else FAIL("register values mismatch");
}

static void test_register_t_nibbles() {
    TEST("registers: T nibble access (TA-TF)");
    RegisterFile r;
    r.T = 0xFEDCBA;
    // TA=F, TB=E, TC=D, TD=C, TE=B, TF=A
    bool ok = r.read(0, 0) == 0xF &&  // TA
              r.read(0, 1) == 0xE &&  // TB
              r.read(0, 2) == 0xD &&  // TC
              r.read(0, 3) == 0xC &&  // TD
              r.read(4, 0) == 0xB &&  // TE
              r.read(5, 0) == 0xA;    // TF
    if (ok) PASS(); else FAIL("nibble values mismatch");
}

static void test_register_t_nibble_write() {
    TEST("registers: T nibble write");
    RegisterFile r;
    r.T = 0;
    r.write(0, 0, 0xA);  // TA
    r.write(0, 3, 0x5);  // TD
    r.write(5, 0, 0x3);  // TF
    if (r.T == 0xA00503) PASS();
    else FAIL("T = 0x%06X, expected 0xA00503", r.T);
}

static void test_register_a_stack() {
    TEST("registers: A-stack push/pop LIFO");
    RegisterFile r;
    r.push(0x100);
    r.push(0x200);
    r.push(0x300);
    bool ok = r.pop() == 0x300 && r.pop() == 0x200 && r.pop() == 0x100;
    if (ok) PASS(); else FAIL("stack order wrong");
}

static void test_register_a_stack_wrap() {
    TEST("registers: A-stack wraps at depth 16");
    RegisterFile r;
    r.stack_depth = 16;
    for (int i = 0; i < 20; ++i) r.push(i);
    // After 20 pushes, ptr should have wrapped. Last 16 values: 4..19
    // Top should be 19
    if (r.pop() == 19) PASS();
    else FAIL("wrap failed");
}

static void test_register_cp_fields() {
    TEST("registers: CP field accessors (CPL/CPU/CYF)");
    RegisterFile r;
    r.set_CPL(24);
    r.set_CPU(1);
    r.set_CYF(1);
    bool ok = r.CPL() == 24 && r.CPU() == 1 && r.CYF() == 1;
    if (ok) PASS(); else FAIL("CPL=%d CPU=%d CYF=%d", r.CPL(), r.CPU(), r.CYF());
}

static void test_register_scratchpad() {
    TEST("registers: scratchpad left/right");
    RegisterFile r;
    r.scratchpad[5].left  = 0xAABBCC;
    r.scratchpad[5].right = 0xDDEEFF;
    bool ok = r.scratchpad[5].left == 0xAABBCC && r.scratchpad[5].right == 0xDDEEFF;
    if (ok) PASS(); else FAIL("scratchpad mismatch");
}

static void test_register_func_box_sum() {
    TEST("registers: function box SUM output");
    RegisterFile r;
    r.X = 100; r.Y = 200; r.set_CPL(24);
    uint32_t sum = r.read(3, 0);  // SUM
    if (sum == 300) PASS();
    else FAIL("SUM=%u, expected 300", sum);
}

static void test_register_func_box_diff() {
    TEST("registers: function box DIFF output");
    RegisterFile r;
    r.X = 500; r.Y = 200; r.set_CPL(24);
    uint32_t diff = r.read(8, 3);  // DIFF
    if (diff == 300) PASS();
    else FAIL("DIFF=%u, expected 300", diff);
}

static void test_register_func_box_and_xor() {
    TEST("registers: function box AND, XOR");
    RegisterFile r;
    r.X = 0xFF00FF; r.Y = 0x0F0F0F; r.set_CPL(24);
    uint32_t xany = r.read(3, 3);  // XANY = X AND Y
    uint32_t xory = r.read(7, 3);  // XORY = X XOR Y
    bool ok = xany == 0x0F000F && xory == 0xF00FF0;
    if (ok) PASS();
    else FAIL("AND=0x%06X XOR=0x%06X", xany, xory);
}

static void test_register_null() {
    TEST("registers: NULL register reads zero");
    RegisterFile r;
    r.write(15, 3, 0xABCDEF);  // write to NULL (discarded)
    uint32_t v = r.read(15, 3); // read NULL → always 0
    if (v == 0) PASS();
    else FAIL("NULL read = 0x%X", v);
}

// ══════════════════════════════════════════════════════════════════════════
// PROCESSOR / EXECUTION TESTS
// ══════════════════════════════════════════════════════════════════════════

static Processor make_cpu(size_t mem_bytes = 4096) {
    ProcessorConfig cfg;
    cfg.s_memory_bytes = mem_bytes;
    return Processor(cfg);
}

static void write_micro(Memory& mem, uint32_t word_addr, uint16_t micro) {
    uint32_t byte_addr = word_addr * 2;
    mem.write_byte(byte_addr, micro >> 8);
    mem.write_byte(byte_addr + 1, micro & 0xFF);
}

static void test_exec_nop_halt() {
    TEST("exec: NOP + HALT");
    auto cpu = make_cpu();
    write_micro(cpu.mem, 0, 0x0000);  // NOP
    write_micro(cpu.mem, 1, 0x0002);  // HALT
    cpu.regs.MAR = 0;
    cpu.run(100);
    if (cpu.regs.halted) PASS();
    else FAIL("did not halt");
}

static void test_exec_register_move() {
    TEST("exec: 1C register move X→Y");
    auto cpu = make_cpu();
    cpu.regs.X = 0x123456;

    // 1C: src=X(grp2,sel0), dst=Y(grp2,sel1)
    // encode_1C: MC=1, MD=dst_grp=2, ME[7:6]=dst_sel=1, ME[5:4]=src_sel=0, MF=src_grp=2
    // = 0x1000 | 0x0200 | 0x0040 | 0x0000 | 0x0002 = 0x1242
    write_micro(cpu.mem, 0, 0x1242);
    write_micro(cpu.mem, 1, 0x0002);  // HALT
    cpu.regs.MAR = 0;
    cpu.run(100);
    if (cpu.regs.Y == 0x123456) PASS();
    else FAIL("Y=0x%06X", cpu.regs.Y);
}

static void test_exec_literal_8bit() {
    TEST("exec: 8C 8-bit literal to T");
    auto cpu = make_cpu();
    // 8C: MC=8, MD=dst_grp=2 (group 2, always sel 2 = T), literal=0x42
    // encode_8C(2, 0x42) = (8<<12) | (2<<8) | 0x42 = 0x8242
    write_micro(cpu.mem, 0, 0x8242);
    write_micro(cpu.mem, 1, 0x0002);
    cpu.regs.MAR = 0;
    cpu.run(100);
    // 8C writes to group 2, sel 2 = T register
    if (cpu.regs.T == 0x42) PASS();
    else FAIL("T=0x%06X", cpu.regs.T);
}

static void test_exec_branch_forward() {
    TEST("exec: 12C branch forward");
    auto cpu = make_cpu();

    // addr 0: branch forward +2
    write_micro(cpu.mem, 0, 0xC002);

    // addr 1: bad (should be skipped)
    write_micro(cpu.mem, 1, 0x4FF2);  // would set T=0xFF

    // addr 2: bad
    write_micro(cpu.mem, 2, 0x4FF2);

    // addr 3: HALT (target: word 0 + 1 + 2 = word 3)
    write_micro(cpu.mem, 3, 0x0002);

    cpu.regs.T = 0;
    cpu.regs.MAR = 0;
    cpu.run(100);
    if (cpu.regs.halted && cpu.regs.T == 0) PASS();
    else FAIL("halted=%d T=0x%X", cpu.regs.halted, cpu.regs.T);
}

static void test_exec_call_return() {
    TEST("exec: 14C call + TAS→MAR return");
    auto cpu = make_cpu();

    // word 0: CALL forward +3
    write_micro(cpu.mem, 0, 0xE003);

    // word 1: set T=0x42 (executed AFTER return)
    // 8C to group 2, sel 2 = T: encode_8C(2, 0x42) = 0x8242
    write_micro(cpu.mem, 1, 0x8242);

    // word 2: HALT
    write_micro(cpu.mem, 2, 0x0002);

    // word 3: padding (skipped by call)
    write_micro(cpu.mem, 3, 0x0000);

    // word 4: subroutine target (word 0+1+3 = word 4)
    // TAS(grp11,sel2) → MAR(grp4,sel2)
    // 1C: MC=1, MD=dst_grp=4, ME[7:6]=dst_sel=2=10, ME[5:4]=src_sel=2=10, MF=src_grp=11
    // = 0x1000 | 0x0400 | 0x0080 | 0x0020 | 0x000B = 0x14AB
    write_micro(cpu.mem, 4, 0x14AB);

    cpu.regs.MAR = 0;
    cpu.regs.T = 0;
    cpu.run(100);

    // After call to word 4, it immediately returns to word 1.
    // Word 1: T=0x42, Word 2: HALT
    if (cpu.regs.halted && cpu.regs.T == 0x42) PASS();
    else FAIL("halted=%d T=0x%X", cpu.regs.halted, cpu.regs.T);
}

static void test_exec_memory_write_read() {
    TEST("exec: 7C write X to mem, read to Y");
    auto cpu = make_cpu();
    cpu.regs.X = 0xFACE00;
    cpu.regs.FA = 0x1000;   // bit address 4096 = byte 512
    cpu.regs.set_CPL(24);

    // 7C WRITE from X, len=0 (use CPL=24), count=0
    // MC=0111, dir=1(write), reg=00(X), sign=0  → MD = 1000 = 0x8
    // ME=0, MF=0 → field len concat = 0, count=0
    write_micro(cpu.mem, 0, 0x7800);

    // 7C READ to Y, len=0, count=0
    // MC=0111, dir=0(read), reg=01(Y), sign=0  → MD = 0010 = 0x2
    write_micro(cpu.mem, 1, 0x7200);

    // HALT
    write_micro(cpu.mem, 2, 0x0002);

    cpu.regs.MAR = 0;
    cpu.run(100);

    if (cpu.regs.Y == 0xFACE00) PASS();
    else FAIL("Y=0x%06X, expected 0xFACE00", cpu.regs.Y);
}

static void test_exec_4bit_manipulate() {
    TEST("exec: 3C SET literal to TA");
    auto cpu = make_cpu();

    cpu.regs.T = 0;  // T starts at zero, TA=0

    // 3C SET TA(grp0,sel0) to literal 0xA
    // encode_3C(0, 0, 0/*SET*/, 0xA) = (3<<12) | (0<<8) | (0<<7) | (0<<4) | 0xA = 0x300A
    write_micro(cpu.mem, 0, 0x300A);
    write_micro(cpu.mem, 1, 0x0002);

    cpu.regs.MAR = 0;
    cpu.run(100);

    uint8_t ta = (cpu.regs.T >> 20) & 0xF;
    if (ta == 0xA) PASS();
    else FAIL("TA=0x%X, expected 0xA", ta);
}

static void test_exec_shift_xy() {
    TEST("exec: 4D shift X right by 4");
    auto cpu = make_cpu();
    cpu.regs.X = 0x000100;

    // 4D: MC=0000, MD=0100 (identifies 4D)
    // ME[7]=0(X), ME[6]=1(right), ME[5]=0(shift), bits[4:0]=4(count)
    // raw = 0000_0100_0100_0100 = 0x0444
    write_micro(cpu.mem, 0, 0x0444);
    write_micro(cpu.mem, 1, 0x0002);

    cpu.regs.MAR = 0;
    cpu.run(100);

    if (cpu.regs.X == 0x000010) PASS();
    else FAIL("X=0x%06X, expected 0x000010", cpu.regs.X);
}

static void test_exec_exchange_doublepad() {
    TEST("exec: 7D exchange FA:FB with scratchpad");
    auto cpu = make_cpu();
    cpu.regs.FA = 0x111111;
    cpu.regs.FB = 0x222222;
    cpu.regs.scratchpad[3].left  = 0xAAAAAA;
    cpu.regs.scratchpad[3].right = 0xBBBBBB;

    // 7D: MC=0000, MD=0111, ME=3(src_pad), MF=3(dst_pad)
    // Same src and dst for a swap:
    write_micro(cpu.mem, 0, 0x0733);
    write_micro(cpu.mem, 1, 0x0002);

    cpu.regs.MAR = 0;
    cpu.run(100);

    bool ok = cpu.regs.FA == 0xAAAAAA && cpu.regs.FB == 0xBBBBBB &&
              cpu.regs.scratchpad[3].left == 0x111111 &&
              cpu.regs.scratchpad[3].right == 0x222222;
    if (ok) PASS();
    else FAIL("FA=0x%X FB=0x%X pad.L=0x%X pad.R=0x%X",
              cpu.regs.FA, cpu.regs.FB,
              cpu.regs.scratchpad[3].left, cpu.regs.scratchpad[3].right);
}

static void test_exec_bias() {
    TEST("exec: 3E bias sets CPL from FL");
    auto cpu = make_cpu();
    cpu.regs.set_FB_field(0, 0, 16);  // FU=0, FT=0, FL=16
    cpu.regs.set_CPL(0);

    // 3E: MC=0, MD=0011=3, ME/MF encode variant
    write_micro(cpu.mem, 0, 0x0301);
    write_micro(cpu.mem, 1, 0x0002);

    cpu.regs.MAR = 0;
    cpu.run(100);

    if (cpu.regs.CPL() == 16) PASS();
    else FAIL("CPL=%d, expected 16", cpu.regs.CPL());
}

// ══════════════════════════════════════════════════════════════════════════
// BCD TESTS
// ══════════════════════════════════════════════════════════════════════════

static void test_bcd_addition() {
    TEST("registers: BCD addition 99+1=100");
    RegisterFile r;
    r.X = 0x99;    // 99 in BCD
    r.Y = 0x01;    // 1 in BCD
    r.set_CPU(1);  // BCD mode
    r.set_CPL(12); // 3 BCD digits
    uint32_t sum = r.read(3, 0);  // SUM
    if (sum == 0x100) PASS();
    else FAIL("SUM=0x%X, expected 0x100", sum);
}

// ══════════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════════

int main() {
    std::puts("=== Burroughs B1700 Emulator — Unit Tests ===\n");

    // Memory tests
    std::puts("Memory:");
    test_memory_byte_access();
    test_memory_bit_field_aligned();
    test_memory_bit_field_unaligned();
    test_memory_24bit_field();
    test_memory_bit_field_reverse();
    test_memory_micro_fetch();

    // Register tests
    std::puts("\nRegisters:");
    test_register_read_write_basic();
    test_register_t_nibbles();
    test_register_t_nibble_write();
    test_register_a_stack();
    test_register_a_stack_wrap();
    test_register_cp_fields();
    test_register_scratchpad();
    test_register_func_box_sum();
    test_register_func_box_diff();
    test_register_func_box_and_xor();
    test_register_null();

    // Processor execution tests
    std::puts("\nExecution:");
    test_exec_nop_halt();
    test_exec_register_move();
    test_exec_literal_8bit();
    test_exec_branch_forward();
    test_exec_call_return();
    test_exec_memory_write_read();
    test_exec_4bit_manipulate();
    test_exec_shift_xy();
    test_exec_exchange_doublepad();
    test_exec_bias();

    // BCD tests
    std::puts("\nBCD:");
    test_bcd_addition();

    // Summary
    std::printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        std::printf(", %d FAILED", tests_failed);
    std::puts(" ===\n");

    return tests_failed > 0 ? 1 : 0;
}
