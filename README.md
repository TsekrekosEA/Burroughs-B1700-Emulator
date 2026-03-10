<p align="center">
  <strong>Burroughs B1700 Emulator</strong><br>
  <em>A cycle-level emulator of the world's first dynamically microprogrammed computer</em>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white" alt="C++20">
  <img src="https://img.shields.io/badge/CMake-3.20+-064F8C?logo=cmake&logoColor=white" alt="CMake 3.20+">
  <img src="https://img.shields.io/badge/Platform-Linux-orange?logo=linux&logoColor=white" alt="Linux">
  <img src="https://img.shields.io/badge/License-GPL%20v3-green?logo=gnu&logoColor=white" alt="GPL v3">
  <img src="https://img.shields.io/badge/Tests-28%20passing-brightgreen" alt="Tests">
  <img src="https://img.shields.io/badge/Lines-~8000-informational" alt="Lines of Code">
</p>

---

## What Is This?

In 1972, Burroughs Corporation built a computer with no fixed instruction set. Instead of hardwiring instructions like ADD or JUMP into silicon, the B1700 loaded microcode interpreters from software. One for COBOL, another for FORTRAN, another for RPG. When the operating system switched between programs, it replaced the entire instruction set of the CPU.

This project is a faithful emulator of that machine, built from primary sources: the 429-page Field Engineering Technical Manual, Wilner's original design papers, and surviving microcode artifacts from [bitsavers.org](http://bitsavers.org/pdf/burroughs/B1700/).

The emulator can:
- Execute all 24 B1710 micro-operators at cycle-level fidelity
- Bootstrap the original 1972 Cold Start Loader (288 words, 4,317 cycles)
- Run two custom S-language interpreters (stack machine + register machine)
- Demonstrate Gismo-style interpreter switching, swapping the CPU's entire instruction set between processes at runtime
- Parse and analyze surviving Burroughs tape images
- Debug microcode interactively with breakpoints, watches, and trace filtering

<!-- SCREENSHOT_PLACEHOLDER: Terminal showing Gismo demo with interleaved S-CALC and S-FORT output -->

---

## Table of Contents

- [Why the B1700 Matters](#why-the-b1700-matters)
- [Features](#features)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Architecture Overview](#architecture-overview)
- [Project Structure](#project-structure)
- [Documentation](#documentation)
- [Build Targets](#build-targets)
- [Testing](#testing)
- [Sample Output](#sample-output)
- [Acknowledgments](#acknowledgments)
- [License](#license)

---

## Why the B1700 is so unique

Most computers execute a fixed instruction set: x86, ARM, RISC-V. The programmer and the hardware agree on what ADD means, and that's that.

Wayne T. Wilner had a different idea. Instead of compromising between languages, he proposed building a machine that becomes the ideal computer for each language. Compile COBOL to "S-COBOL" (a custom bytecode), write a microcode interpreter that executes S-COBOL natively, and the CPU literally is a COBOL machine. Overlay new microcode and the CPU becomes a FORTRAN machine instead. Same transistors, completely different instruction set.

This concept, dynamically microprogrammed architecture, anticipated ideas that wouldn't become mainstream for decades. JIT compilation (the interpreter is the JIT layer), hardware virtualization (the CPU's ISA is software-defined), and language-oriented design where the machine adapts to the language instead of the other way around.

The B1700 was produced from 1972 to 1986, yet no working emulator existed publicly before this project. The original documentation is fragmented across scanned manuals, academic papers, and unlabeled tape images. Building this emulator required reconstructing the architecture from these primary sources, a process documented in [JOURNEY.md](docs/JOURNEY.md).

---

## Features

### Processor Core
- All 24 B1710 micro-operators, 1C through 15C including D-class secondaries
- Bit-addressable memory faithful to the B1700's unique bit-level Field Isolation Unit
- 24-bit data path with combinatorial Function Box (SUM, DIFF, CMPX, CMPY, XORY, XANY, XEQY, MSKX, MSKY)
- 16-entry scratchpad (48-bit words, left/right halves)
- 16-deep A-stack for CALL/EXIT subroutine linkage
- Complete register file, all 64 register slots from Table I-3 of the FE Manual

### Bootstrap & I/O
- EMV I/O protocol with card reader emulation on port 2
- Cold Start Loader bootstrap that loads and executes the authentic 1972 CSL (4,317 cycles to OVERLAY)
- OVERLAY instruction for microcode-level bootstrap handoff

### S-Language Interpreters
- S-CALC: stack-based calculator (8 opcodes, 120 microcode words)
- S-FORT: register-based FORTRAN-like machine (10 opcodes, 211 microcode words, CALL/EXIT subroutines)
- Gismo interpreter switching with full process state save/restore and microcode overlay, demonstrating the B1700's defining feature

### Toolchain
- MIL cross-assembler, a two-pass assembler for the Micro Implementation Language (~2,000 lines)
- S-CALC compiler for source-to-binary translation of the stack language
- S-FORT assembler with two-pass label resolution for the register language
- Tape reader for Burroughs labeled tape format with EBCDIC support

### Debugger
- Interactive REPL with Ctrl-C interrupt handling
- Breakpoints on address, opcode, cycle count, register value, register change, memory access, and halt
- Watch expressions with optional break-on-change
- Trace filtering (full, branches-only, memory-only, I/O-only) with file output
- Live inspection of registers, memory hex dump, disassembly, scratchpad, A-stack, and I/O ports

---

## Quick Start

### Prerequisites

- **C++20** compiler (GCC 12+ or Clang 15+)
- **CMake** 3.20+
- Linux (tested on Ubuntu 24.04)

### Build

```bash
git clone https://github.com/TsekrekosEA/Burroughs-B1700-Emulator.git
cd Burroughs-B1700-Emulator
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

This produces 6 executables:

| Binary | Description |
|--------|-------------|
| `b1700` | Main emulator |
| `b1700_tests` | Unit test suite (28 tests) |
| `mil_asm` | MIL microcode cross-assembler |
| `tape_reader` | Burroughs tape image parser |
| `scalc_asm` | S-CALC source compiler |
| `sfort_asm` | S-FORT source assembler |

### Verify

```bash
./b1700 --test          # Run 28 built-in tests
./b1700_tests           # Run unit test suite
```

---

## Usage

### Run the Cold Start Loader Bootstrap

```bash
# Assemble the CSL from original 1972 MIL source
./mil_asm ../artifacts/cards/cold_start_loader.mil cold_start_loader.bin

# Boot it
./b1700 --load cold_start_loader.bin --run
```

Expected output: CSL executes 4,317 cycles, processes EMV card deck, reaches OVERLAY with FA=0x5000.

### Run S-Language Programs

```bash
# Assemble an interpreter and compile a program
./mil_asm ../artifacts/interpreters/s_calc_interp.mil s_calc.bin
./scalc_asm ../artifacts/programs/demo1.scalc demo1.bin

# Run: load interpreter microcode + S-code program
./b1700 --interp s_calc.bin --scode demo1.bin --run
```

```
[MONITOR] PRINT: 300 (0x00012C)
System halted at cycle 330  MAR=0x000F0
```

### Gismo Interpreter Switching Demo

```bash
# Assemble both interpreters
./mil_asm ../artifacts/interpreters/s_calc_interp.mil s_calc.bin
./mil_asm ../artifacts/interpreters/s_fort_interp_gismo.mil s_fort.bin

# Compile both programs
./scalc_asm ../artifacts/programs/gismo_calc.scalc calc.bin
./sfort_asm ../artifacts/programs/gismo_fib.sfort fib.bin

# Run with interpreter switching (quantum = 50 micro-cycles per slice)
./b1700 --gismo s_calc.bin calc.bin s_fort.bin fib.bin --quantum 50
```

```
╔══════════════════════════════════════════════════════════════╗
║          BURROUGHS B1700 — GISMO INTERPRETER SWITCHING      ║
╚══════════════════════════════════════════════════════════════╝

 Process 0 : S-CALC  (stack machine)     interp=240 B  code=44 B
 Process 1 : S-FORT  (register machine)  interp=422 B  code=62 B
 Quantum   : 50 micro-cycles per slice

[S-CALC ] PRINT: 10 (0x00000A)
[S-CALC ] PRINT: 20 (0x000014)
[S-CALC ] PRINT: 30 (0x00001E)
[S-FORT ] PRINT: 1 (0x000001)
[S-FORT ] PRINT: 1 (0x000001)
[S-CALC ] PRINT: 40 (0x000028)
[S-CALC ] PRINT: 50 (0x000032)
[S-FORT ] PRINT: 2 (0x000002)
[S-FORT ] PRINT: 3 (0x000003)
[S-FORT ] PRINT: 5 (0x000005)

── Summary ─────────────────────────────────────────────────
  Context switches : 28
  S-CALC cycles    : 1520
  S-FORT cycles    : 6980
  Both programs    : completed correctly
```

### Interactive Debugger

```bash
./b1700 --load cold_start_loader.bin --debug
```

```
B1700 Debugger — type 'help' for commands
[0x00000]> break addr 0x00120
Breakpoint #1: address 0x00120
[0x00000]> run
Hit breakpoint #1 at MAR=0x00120, cycle 847
[0x00120]> regs
  X=006161  Y=000000  T=000000  L=000000
  FA=005000  FB=000000  FL=0000
  MAR=00120  BR=000000  LR=000000
  M=0A00  CP=00  U=0000
[0x00120]> disasm 5
  0x00120: 1C  MOVE X TO Y          ; 1420
  0x00130: 8C  LIT 0x61 TO T        ; 8461
  0x00140: 6C  SKIP WHEN T EQL Y    ; 6012
  ...
[0x00120]> step
```

### Parse Tape Images

```bash
./tape_reader --list ../artifacts/tape/fetest/fetest.tap
./tape_reader --dump ../artifacts/tape/b1000_mk10.0.tap
```

---

## Architecture Overview

The B1700 is radically different from conventional computers:

```
┌─────────────────────────────────────────────────────────┐
│                    CONVENTIONAL CPU                      │
│                                                         │
│   Source Code ──► Compiler ──► Fixed ISA ──► Hardware   │
│   (COBOL)         (gcc)       (x86 ADD)    (transistors)│
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                    BURROUGHS B1700                       │
│                                                         │
│   Source Code ──► Compiler ──► S-Language ──► Interpreter ──► Micro-ops ──► Hardware│
│   (COBOL)                     (S-COBOL)     (microcode)      (24 ops)    (wires)   │
│                                                                                     │
│   Source Code ──► Compiler ──► S-Language ──► Interpreter ──► Micro-ops ──► Hardware│
│   (FORTRAN)                   (S-FORT)      (different µ!)   (same 24)   (same!)   │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

The 24 micro-operators are the only fixed operations. Everything above them, including what the programmer thinks of as "instructions", is defined by software-loaded microcode.

For deep technical details, see the [Architecture Guide](docs/ARCHITECTURE.md).

---

## Project Structure

```
Burroughs-B1700-Emulator/
├── src/
│   ├── core/                    # Emulator core (header-only library)
│   │   ├── types.h              # Micro-instruction field accessors (176 lines)
│   │   ├── memory.h             # Bit-addressable memory model (103 lines)
│   │   ├── registers.h          # Full register file + function box (524 lines)
│   │   ├── processor.h          # Decode tree + all 24 micro-operators (809 lines)
│   │   ├── io_bus.h             # I/O bus + EMV host control (355 lines)
│   │   ├── disasm.h             # Disassembler for all 16 MC classes (384 lines)
│   │   └── debugger.h           # Interactive debugger REPL (1,077 lines)
│   ├── tools/
│   │   ├── mil_asm.cpp          # MIL cross-assembler (1,985 lines)
│   │   ├── tape_reader.cpp      # Burroughs tape parser (868 lines)
│   │   ├── scalc_asm.cpp        # S-CALC compiler (199 lines)
│   │   └── sfort_asm.cpp        # S-FORT assembler (334 lines)
│   └── main.cpp                 # CLI + CSL boot + Gismo orchestrator (677 lines)
├── artifacts/
│   ├── cards/
│   │   ├── cold_start_loader.mil    # Original 1972 CSL source
│   │   └── cold_start_loader.bin    # Assembled binary
│   ├── interpreters/
│   │   ├── s_calc_interp.mil        # S-CALC microcode interpreter
│   │   ├── s_fort_interp.mil        # S-FORT microcode interpreter
│   │   └── s_fort_interp_gismo.mil  # S-FORT variant for Gismo demo
│   ├── programs/                    # S-language demo programs
│   ├── tape/                        # Burroughs tape images
│   └── zips/                        # Original archive files
├── tests/
│   ├── test_main.cpp            # 28 unit tests
│   └── mil/                     # 6 MIL test programs
├── docs/                        # Documentation (see below)
├── CMakeLists.txt
├── LICENSE                      # GPL v3
└── README.md
```

---

## Documentation

Detailed documentation is available both in the [docs/](docs/) directory and on the [GitHub Wiki](../../wiki):

| Document | Description |
|----------|-------------|
| **[Architecture Guide](docs/ARCHITECTURE.md)** | Deep-dive into B1700 hardware: bit-addressable memory, register file, micro-instruction encoding, decode tree, Function Box |
| **[Implementation Guide](docs/IMPLEMENTATION.md)** | How the emulator was built: design decisions, C++ patterns, testing strategy. A guide for anyone building a similar emulator |
| **[Interpreter Switching](docs/INTERPRETERS.md)** | S-CALC, S-FORT, and Gismo: the B1700's defining feature explained and demonstrated |
| **[Artifacts Catalog](docs/ARTIFACTS.md)** | Every tape image, MIL program, and S-language program with provenance, format details, and usage instructions |
| **[Development Journey](docs/JOURNEY.md)** | The full story: from scanned 1970s manuals to working interpreter switching, including the bugs, dead ends, and breakthroughs |

---

## Build Targets

| Target | Command | Description |
|--------|---------|-------------|
| `b1700` | `make b1700` | Main emulator with all execution modes |
| `b1700_tests` | `make b1700_tests` | Unit test suite |
| `mil_asm` | `make mil_asm` | MIL microcode cross-assembler |
| `tape_reader` | `make tape_reader` | Burroughs labeled tape parser |
| `scalc_asm` | `make scalc_asm` | S-CALC source-to-binary compiler |
| `sfort_asm` | `make sfort_asm` | S-FORT two-pass assembler |

---

## Testing

```bash
cd build

# Unit tests (28 tests covering memory, registers, ALU, processor, I/O)
./b1700_tests

# Built-in self-tests (9 micro-operator verification tests)
./b1700 --test

# Assemble all MIL test programs (6 programs)
for f in ../tests/mil/test*.mil; do
    echo "--- $(basename $f) ---"
    ./mil_asm "$f" /dev/null
done

# Full integration: CSL bootstrap
./mil_asm ../artifacts/cards/cold_start_loader.mil csl.bin
./b1700 --load csl.bin --run
```

---

## Sample Output

### CSL Bootstrap (4,317 cycles)

```
Burroughs B1700 Emulator - Cold Start Loader
Loading 288 words (576 bytes) at address 0x00000
EMV: Card reader attached on port 2 (6 cards)
Cycle     1: OVERLAY pass-through (FA=FB=0)
...
Cycle  4317: OVERLAY — bootstrap complete (FA=0x5000)
System halted.
```

### Fibonacci Sequence (S-FORT)

```bash
./b1700 --interp s_fort.bin --scode fibonacci.bin --run
```
```
[MONITOR] PRINT: 1 (0x000001)
[MONITOR] PRINT: 1 (0x000001)
[MONITOR] PRINT: 2 (0x000002)
[MONITOR] PRINT: 3 (0x000003)
[MONITOR] PRINT: 5 (0x000005)
[MONITOR] PRINT: 8 (0x000008)
[MONITOR] PRINT: 13 (0x00000D)
[MONITOR] PRINT: 21 (0x000015)
[MONITOR] PRINT: 34 (0x000022)
[MONITOR] PRINT: 55 (0x000037)
System halted at cycle 13660  MAR=0x00070
```

---

## Acknowledgments

- Wayne T. Wilner, designer of the B1700, whose papers made this reconstruction possible
- [bitsavers.org](http://bitsavers.org/pdf/burroughs/B1700/) for preserving the B1700 Field Engineering Technical Manual, MIL source code, and tape images
- Al Kossow for his tireless work preserving computing history at bitsavers
- The original Burroughs Corporation engineers at the Santa Barbara Plant, Goleta, CA
- Claude Opus. This was a hobby project, but the AI helped it run along smoothly by rewriting messy comments, formatting my documents, creating an awesome working debugger and introducing a bunch of bugs to the MIL assembler that I had to spend hours fixing.

### References

- *B1700 Field Engineering Technical Manual* (1053360, May 1973), 429 pages, primary source
- Wilner, W.T. "Design of the Burroughs B1700" (AFIPS 1972)
- Wilner, W.T. "Burroughs B1700 Memory Utilization" (AFIPS 1972)
- Organick, E.I. "Interpreting Machines: Architecture and Programming of the B1700/B1800 Series" (1978)
- Datapro Reports 70C-112-04 (1974, 1978)

---

## License

Copyright (C) 2025–2026 Andrianos Tsekrekos

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License v3.0** as published by the Free Software Foundation.

See [LICENSE](LICENSE) for the full text.

---

<p align="center">
  <em>The B1700 had no fixed instruction set, and now, 50 years later, it still doesn't. But at least it runs again.</em>
</p>
