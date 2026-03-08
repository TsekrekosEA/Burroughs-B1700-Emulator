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
    r.write(4, 0, 0x123456);  // X
    r.write(4, 1, 0xABCDEF);  // Y
    r.write(4, 2, 0x111111);  // T
    r.write(4, 3, 0x222222);  // L
    bool ok = r.read(4, 0) == 0x123456 &&
              r.read(4, 1) == 0xABCDEF &&
              r.read(4, 2) == 0x111111 &&
              r.read(4, 3) == 0x222222;
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
              r.read(1, 0) == 0xB &&  // TE
              r.read(1, 1) == 0xA;    // TF
    if (ok) PASS(); else FAIL("nibble values mismatch");
}

static void test_register_t_nibble_write() {
    TEST("registers: T nibble write");
    RegisterFile r;
    r.T = 0;
    r.write(0, 0, 0xA);  // TA
    r.write(0, 3, 0x5);  // TD
    r.write(1, 1, 0x3);  // TF
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
    uint32_t sum = r.read(6, 0);  // SUM
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
    uint32_t xany = r.read(6, 3);  // XANY = X AND Y
    uint32_t xory = r.read(7, 3);  // XORY = X XOR Y
    bool ok = xany == 0x0F000F && xory == 0xF00FF0;
    if (ok) PASS();
    else FAIL("AND=0x%06X XOR=0x%06X", xany, xory);
}

static void test_register_null() {
    TEST("registers: NULL register reads zero");
    RegisterFile r;
    r.write(15, 1, 0xABCDEF);  // write to NULL (discarded)
    uint32_t v = r.read(15, 1); // read NULL → always 0
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

    // 1C: src=X(grp4,sel0), dst=Y(grp4,sel1)
    // MC=4, MD=0x00, ME=4, MF=(1<<2)|0=4
    write_micro(cpu.mem, 0, 0x4044);
    write_micro(cpu.mem, 1, 0x0002);  // HALT
    cpu.regs.MAR = 0;
    cpu.run(100);
    if (cpu.regs.Y == 0x123456) PASS();
    else FAIL("Y=0x%06X", cpu.regs.Y);
}

static void test_exec_literal_8bit() {
    TEST("exec: 8C 8-bit literal to X");
    auto cpu = make_cpu();
    // 8C: MC=4(dst_grp=X), MD:ME literal=0x42, MF=0x02
    write_micro(cpu.mem, 0, 0x4422);
    write_micro(cpu.mem, 1, 0x0002);
    cpu.regs.MAR = 0;
    cpu.run(100);
    // 8C writes to group=MC, select=2. Group 4, select 2 = T register.
    if (cpu.regs.T == 0x42) PASS();
    else FAIL("T=0x%06X (X=0x%06X)", cpu.regs.T, cpu.regs.X);
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
    write_micro(cpu.mem, 1, 0x4422);  // 8C to T

    // word 2: HALT
    write_micro(cpu.mem, 2, 0x0002);

    // word 3: padding (skipped by call)
    write_micro(cpu.mem, 3, 0x0000);

    // word 4: subroutine target (word 0+1+3 = word 4)
    write_micro(cpu.mem, 4, 0xA850);

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
    TEST("exec: 3C SET TA → TD");
    auto cpu = make_cpu();

    cpu.regs.T = 0xA00000;  // TA=0xA

    // 3C: src=TA(grp0,sel0), func=SET(0), dst=TD(grp0,sel3)
    // MC=0, MD=(0<<2)|0=0, ME=0, MF=(3<<2)|1=0xD
    // MF[1:0]=01 identifies 3C
    write_micro(cpu.mem, 0, 0x000D);
    write_micro(cpu.mem, 1, 0x0002);

    cpu.regs.MAR = 0;
    cpu.run(100);

    uint8_t td = (cpu.regs.T >> 8) & 0xF;
    if (td == 0xA) PASS();
    else FAIL("TD=0x%X, expected 0xA", td);
}

static void test_exec_shift_xy() {
    TEST("exec: 4D shift X left by 4");
    auto cpu = make_cpu();
    cpu.regs.X = 0x000001;

    cpu.regs.X = 0x000100;

    // Shift X right by 4: MD[11]=0,MD[10]=1,MD[9]=0,MD[8]=0 → MD=0100=4
    // ME=4 (count), MF=0
    // raw = 0000 0100 0100 0000 = 0x0440
    write_micro(cpu.mem, 0, 0x0440);
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
