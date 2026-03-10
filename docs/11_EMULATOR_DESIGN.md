# 11 — Emulator Design Guide

## 1. Project Goals

Build a faithful, cycle-approximate emulator of the Burroughs B1700 (B1710
processor variant) in C++, with the ability to:

1. Execute all 24 micro-operators correctly
2. Boot from a cassette tape image
3. Run the SDL interpreter and MCP II
4. Support at least disk and console I/O
5. Provide debugging and single-step capabilities
6. Optionally extend to B1720 (M-Memory, 32 micro-operators)

---

## 2. Architecture Decisions

### 2.1 Emulation Granularity

| Approach | Description | Recommendation |
|----------|-------------|----------------|
| Gate-level | Simulate individual logic gates | Unnecessary — too slow, too complex |
| Cycle-accurate | Track exact clock-cycle timing | Ideal but complex |
| **Instruction-accurate** | Execute each micro correctly, approximate timing | **Recommended for first version** |
| High-level | Interpret S-languages directly | Would skip the micro layer — defeats the purpose |

**Recommended**: Instruction-accurate emulation with cycle counting. Each
micro-operator executes atomically with the correct number of clocks
charged. This is sufficient for software compatibility while being
achievable.

### 2.2 Language & Standard

- **C++20** or later (for `std::span`, concepts, `<bit>` utilities)
- Build system: **CMake** (cross-platform)
- Testing: **Google Test** or **Catch2**
- GUI (optional): **SDL2** or **Dear ImGui** for console display

### 2.3 Project Structure

```
burroughs-b1700-emulator/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                 # Entry point, CLI parsing
│   ├── core/
│   │   ├── processor.h/.cpp     # Main CPU loop, decode, execute
│   │   ├── registers.h/.cpp     # RegisterFile, read/write dispatch
│   │   ├── memory.h/.cpp        # BitAddressableMemory, FIU
│   │   ├── alu.h/.cpp           # 24-bit and 4-bit function boxes
│   │   ├── microinstruction.h   # MicroFields decoder
│   │   └── types.h              # Type aliases, constants
│   ├── io/
│   │   ├── io_bus.h/.cpp        # I/O bus, control slot management
│   │   ├── console.h/.cpp       # Console emulation
│   │   ├── cassette.h/.cpp      # Cassette tape loader
│   │   ├── disk.h/.cpp          # Disk controller emulation
│   │   └── io_control.h         # IOControl abstract interface
│   ├── system/
│   │   ├── clock.h/.cpp         # Cycle counter, timer interrupt
│   │   └── interrupts.h/.cpp    # Interrupt state management
│   └── debug/
│       ├── debugger.h/.cpp      # Interactive debugger (breakpoints, step)
│       ├── disassembler.h/.cpp  # Micro-operator disassembly
│       └── trace.h/.cpp         # Execution trace logging
├── tests/
│   ├── test_registers.cpp
│   ├── test_memory.cpp
│   ├── test_alu.cpp
│   ├── test_microops.cpp
│   └── test_integration.cpp
├── tools/
│   ├── assembler.cpp            # Simple micro-assembler for test programs
│   └── tape_maker.cpp           # Create cassette tape images
├── media/                       # Tape images, disk images
└── docs/                        # This documentation
```

---

## 3. Core Data Structures

### 3.1 Types

```cpp
#pragma once
#include <cstdint>

// Register types (stored values use only the low N bits)
using reg24_t = uint32_t;   // 24-bit: mask with 0x00FFFFFF
using reg19_t = uint32_t;   // 19-bit: mask with 0x0007FFFF
using reg16_t = uint16_t;   // 16-bit
using reg8_t  = uint8_t;    // 8-bit
using reg4_t  = uint8_t;    // 4-bit: mask with 0x0F
using bit_addr_t = uint32_t;// Bit address (24-bit)

// Masks
constexpr uint32_t MASK_24 = 0x00FFFFFF;
constexpr uint32_t MASK_19 = 0x0007FFFF;
constexpr uint32_t MASK_16 = 0x0000FFFF;
constexpr uint32_t MASK_8  = 0x000000FF;
constexpr uint32_t MASK_4  = 0x0000000F;

// Configuration
constexpr size_t B1710_STACK_DEPTH = 16;
constexpr size_t B1720_STACK_DEPTH = 32;
constexpr size_t SCRATCHPAD_WORDS  = 16;
constexpr size_t SCRATCHPAD_BITS   = 48;
```

### 3.2 Processor Configuration

```cpp
struct ProcessorConfig {
    enum class Model { B1710, B1720 };
    Model model = Model::B1710;

    size_t s_memory_bytes = 65536;     // 64K default for B1710
    size_t m_memory_bytes = 0;         // 0 for B1710, 2K-8K for B1720
    size_t stack_depth = B1710_STACK_DEPTH;
    uint8_t num_microinstructions = 24; // 24 for B1710, 32 for B1720
    uint32_t maxs_value = 0;           // Computed from s_memory_bytes
    uint32_t maxm_value = 0;
    uint64_t clock_hz = 4'000'000;     // 4 MHz for B1710
};
```

---

## 4. Implementation Roadmap

### Phase 1: Core Engine (Minimum Viable)

1. **Memory system**
   - Byte-array backed bit-addressable memory
   - Read/write field operations (1–24 bits at any bit offset)
   - Parity generation/checking
   - Micro-instruction fetch (16-bit aligned, no rotation)

2. **Register file**
   - All registers with correct widths
   - Group/select decode table
   - Read/write dispatch with width adaptation
   - Scratchpad (16 × 48-bit)
   - A-Stack (16 × 24-bit with push/pop semantics)

3. **24-Bit Function Box**
   - Binary addition/subtraction with CPL masking
   - Complement, AND, OR, XOR
   - Equality comparison
   - Masked outputs
   - Lazy evaluation (compute on read)

4. **4-Bit Function Box**
   - SET, AND, OR, XOR, INC, DEC
   - Access to all 4-bit registers

5. **Micro-operator execution**
   - All 24 B1710 micro-operators
   - Correct decode tree
   - Cycle counting

6. **Basic test harness**
   - Load raw binary into memory
   - Execute from address 0
   - Print register state on halt

### Phase 2: Console & Bootstrap

7. **Console emulation**
   - Text-based (terminal) console: display lamps as hex, read switches from stdin
   - Step/Run/Tape mode support

8. **Cassette loader**
   - Read binary tape image files
   - Bit-serial accumulation into U register
   - Tape mode execution

### Phase 3: I/O & MCP

9. **I/O bus**
   - IOControl interface
   - Console I/O control
   - Disk I/O control (read/write disk image files)

10. **MCP boot**
    - Bootstrap from cassette → load SDL interpreter → load MCP
    - Requires sourcing original software images

### Phase 4: Advanced Features

11. **B1720 extensions**
    - M-Memory (bipolar control store)
    - Additional micro-operators (28→32)
    - 32-deep A-Stack
    - Port Interchange

12. **Debugger**
    - Interactive CLI debugger
    - Breakpoints (by address, by micro-opcode)
    - Single-step
    - Register/memory inspection
    - Execution trace

13. **GUI console**
    - Graphical lamp/switch display
    - Mode controls
    - Memory hex viewer

14. **Peripheral devices**
    - Printer (output to file)
    - Card reader (input from file)
    - Communications (serial port or TCP socket)

---

## 5. Critical Implementation Details

### 5.1 Microinstruction Fetch Cycle

```cpp
void Processor::fetch_and_execute() {
    // 1. Fetch micro from memory at current MAR
    //    MAR low 4 bits ignored (16-bit alignment)
    uint32_t fetch_addr = regs.MAR & ~0xF;  // word-align
    uint16_t micro = memory.fetch_micro(fetch_addr >> 4); // byte address / 2

    // 2. Store in M register
    regs.M = micro;

    // 3. Auto-increment MAR by 16 bits (1 word)
    regs.MAR = (regs.MAR + 16) & MASK_19;

    // 4. Decode and execute
    int clocks = decode_and_execute(micro);

    // 5. Advance cycle counter
    total_cycles += clocks;

    // 6. Check timer
    check_timer_interrupt();
}
```

### 5.2 Register Move (1C) — The Most Common Micro

The 1C register move is the most frequently executed micro. Its encoding
overloads the MC field with the source register group, making it look like
many different opcodes. The decode must:

1. Determine this is a 1C move (not a branch, not 7C, etc.)
2. Extract source group, source select, dest group, dest select, variant
3. Read the source register
4. Handle width adaptation
5. Write to the destination register
6. Handle special cases (M as dest = OR, MAR as dest = extra clocks, etc.)

### 5.3 Memory Operations (7C)

The 7C micro is the most complex. Key implementation concerns:

1. **Save MAR to TEMPB**: Before memory access, save current program counter
2. **Transfer FA to MAR**: Set up the bit address
3. **Perform the field access**: Use the FIU to extract/insert the bit field
4. **Handle directions**: Forward vs. reverse field direction
5. **Width adaptation**: If field < register width, right-justify with zero fill
6. **Count variants**: After access, optionally count FA ±, FL ±
7. **Restore MAR from TEMPB**: Return to program execution
8. **Parity check on read**: Set CD(3) on failure

### 5.4 M Register Write (Instruction Modification)

When M is the destination of a 1C or 10C move:
```cpp
// Data is OR'd with the NEXT incoming micro (not the current one)
// Implementation: set a flag, and on next fetch, OR the saved value
void Processor::handle_M_write(uint16_t value) {
    m_modify_pending = true;
    m_modify_value = value;
}

// In fetch:
if (m_modify_pending) {
    micro |= m_modify_value;  // OR with fetched micro
    m_modify_pending = false;
}
```

### 5.5 Conditional Skip & Branch (Bit Test Skip, 6C, 4C/5C)

The B1700 has three conditional mechanisms, each with specific encoding
constraints due to the decode tree:

**Decode tree priority:**
1. MC\[15:14\]=11 → Branch/Call (12C–15C)
2. MC=0111 → Memory (7C)
3. MC=0000 + MD≠0 → D-class secondary micros
4. Otherwise: MF\[1:0\] selects C-class (0=1C, 1=3C, 2=8C/9C, 3=6C)

**Constraint:** 6C uses MC\[15:12\] as the register group, so only groups 0–3
are safe. Groups 4–7 conflict with 4C/5C, groups 12–15 conflict with
branch/call. Condition registers (XYCN, FLCN, XYST in group 12; CC, CA in
group 13) cannot use 6C.

**Solution:** The assembler uses D-class Bit Test Skip (MD=0xA) for all
single-bit condition tests on registers in groups 12–13. This routes through
the unambiguous secondary decode path (MC=0000, MD≠0). For multi-bit
mask tests on groups 0–3 nibble registers, standard 6C encoding is used.

For "not equal to non-zero value" patterns (`SKIP WHEN reg EQL val FALSE`),
the assembler emits a **two-word sequence**: 6C V=1 (skip if equal) + 6C V=2
mask=0 (unconditional skip), effectively implementing "skip if not equal".

### 5.6 BCD Arithmetic

The 24-Bit Function Box in BCD mode (CPU=01) performs decimal arithmetic:

```cpp
uint32_t bcd_add(uint32_t x, uint32_t y, uint8_t num_digits) {
    uint32_t result = 0;
    uint32_t carry = 0;
    for (int i = 0; i < num_digits; i++) {
        uint32_t dx = (x >> (i * 4)) & 0xF;
        uint32_t dy = (y >> (i * 4)) & 0xF;
        uint32_t sum = dx + dy + carry;
        if (sum >= 10) {
            sum -= 10;
            carry = 1;
        } else {
            carry = 0;
        }
        result |= (sum & 0xF) << (i * 4);
    }
    // carry is the final carry-out (CYL)
    return result;
}
```

### 5.6 Bit-Field Memory Access

Core implementation for the FIU:

```cpp
uint32_t BitAddressableMemory::read_field(uint32_t bit_addr,
                                            uint8_t field_len,
                                            bool reverse) const {
    if (field_len == 0 || field_len > 24) return 0;

    uint32_t result = 0;
    for (int i = 0; i < field_len; i++) {
        uint32_t addr = reverse ? (bit_addr - i) : (bit_addr + i);
        uint32_t byte_idx = addr / 8;
        uint8_t  bit_idx  = addr % 8;

        if (byte_idx < size_bytes) {
            bool bit_val = (data[byte_idx] >> (7 - bit_idx)) & 1;
            // LSB of source → MSB address position
            // Result is right-justified
            if (bit_val) {
                result |= (1u << (field_len - 1 - i));
            }
        }
    }
    return result;
}
```

*Note: This naive bit-by-bit implementation is correct but slow. Optimize
later with barrel-shift/mask approach for the common case (aligned, forward,
≤ 24 bits).*

---

## 6. Testing Strategy

### 6.1 Unit Tests

| Component | Test Cases |
|-----------|------------|
| Memory | Read/write individual bits, byte-aligned fields, unaligned fields, reverse direction, parity |
| Registers | Group/select decode, width adaptation, A-Stack push/pop/wrap, scratchpad operations |
| 24-Bit FB | Binary add/sub, BCD add/sub, AND/OR/XOR, complement, masked outputs, CPL width control |
| 4-Bit FB | SET/AND/OR/XOR/INC/DEC on all 4-bit registers |
| Each Micro | At least 3 test cases per micro-operator (normal, edge, error) |

### 6.2 Integration Tests

1. **Simple micro programs**: Hand-written sequences that test register moves,
   memory ops, branches, calls
2. **BCD arithmetic**: Multi-digit BCD addition matching known results
3. **Field operations**: Read/write fields at various bit offsets and lengths
4. **Stack operations**: Nested calls and returns
5. **Interrupt polling**: Verify CC/CD bits set correctly, XYST INT OR computed

### 6.3 System Tests

1. **Diagnostic cassette**: If available, load and run Burroughs diagnostic
   microcode
2. **SDL interpreter boot**: Load the SDL interpreter and verify it starts
3. **MCP boot**: Full system bootstrap

---

## 7. Debugging Facilities

### 7.1 Disassembler

Write a micro-operator disassembler that converts 16-bit micro values to
readable text:

```
0x0000 → NOP
0x0002 → HALT
0x0003 → NORMALIZE X
0x4127 → MOVE X(grp4/sel0) → Y(grp4/sel1) ; 1C register move
0x7... → READ/WRITE MEMORY ...
0xC... → BRANCH +displacement
0xE... → CALL +displacement
```

### 7.2 Trace Logging

Optionally log each micro execution:
```
CYCLE  000042  MAR=0x00120  M=0x4127  MOVE X→Y  X=0x00ABCD Y=0x000000→0x00ABCD  [2 clks]
CYCLE  000044  MAR=0x00130  M=0x7183  READ MEM→X  FA=0x001000 FL=8  X=0x0000FF  [8 clks]
```

### 7.3 Interactive Debugger

Commands:
- `step` / `s` — single-step one micro
- `run` — run until halt/breakpoint
- `break <addr>` — set breakpoint at micro address
- `regs` — display all registers
- `mem <addr> <len>` — display memory (bit or byte view)
- `scratch` — display scratchpad
- `stack` — display A-Stack
- `trace on/off` — toggle trace logging

---

## 8. Performance Considerations

The B1710 at 4 MHz executes micro-operators at roughly 1–4 million per second
(2–8 clocks each). A modern x86 processor can easily sustain 100+ million
emulated micro-operations per second, so performance should not be a concern
for instruction-accurate emulation.

Optimization priorities (only if needed):
1. **Micro-instruction fetch**: Hot path — minimize indirection
2. **Memory read_field**: Second hottest — use fast barrel-shift for common cases
3. **Function box outputs**: Cache results, invalidate on X/Y/CP write
4. **Branch prediction**: Not applicable at emulator level, but minimize
   conditional branches in the decode tree

---

## 9. External Dependencies

| Dependency | Purpose | Required? |
|-----------|---------|-----------|
| C++20 compiler | Language standard | Yes |
| CMake ≥3.20 | Build system | Yes |
| Google Test / Catch2 | Testing | Recommended |
| SDL2 | GUI console (optional) | No |
| Dear ImGui | Debug UI (optional) | No |
| fmtlib / std::format | Formatted output | Recommended |
| spdlog | Logging | Optional |

---

*Previous: [10 — Console & Bootstrap](10_CONSOLE_BOOTSTRAP.md) | Next: [12 — Existing Software & Tools](12_EXISTING_SOFTWARE.md)*
