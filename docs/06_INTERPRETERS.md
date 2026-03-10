# 06 — Interpreter & S-Language Architecture

## 1. The S-Language Concept

The B1700's most radical innovation is that it has **no fixed machine language**.
Instead, each programming language is compiled to its own virtual instruction set
called an **S-language** (Secondary Language). An S-language is roughly equivalent
to the machine code of a conventional computer, but it is custom-designed to
express the semantics of one specific programming language efficiently.

```
Source Code (COBOL)  ──compiler──►  S-COBOL instructions
Source Code (FORTRAN) ──compiler──► S-FORTRAN instructions
Source Code (RPG)    ──compiler──►  S-RPG instructions
Source Code (SDL)    ──compiler──►  S-SDL instructions
Source Code (BASIC)  ──compiler──►  S-BASIC instructions
```

Each S-language defines its own:
- Instruction formats (variable length, bit-aligned)
- Opcode set (tailored to the source language)
- Operand addressing modes
- Data types and field conventions

Because S-instructions are designed for a specific language, they can encode
complex operations (like a COBOL `MOVE CORRESPONDING` or a FORTRAN array
access with subscript computation) in a single dense instruction, whereas a
conventional ISA would require many instructions.

Burroughs claimed that **on average only 1/10th as many S-instructions** need to
be executed compared to conventional machine-level programs.

---

## 2. Interpreters

An **interpreter** is a microprogram written in micro-operators that implements
a specific S-language virtual machine. It is the "firmware" that makes the B1700
hardware behave like a COBOL machine, a FORTRAN machine, etc.

### 2.1 Interpreter Structure

A typical interpreter implements this loop:

```
1. Fetch the next S-language instruction from S-memory
   - Use FA to point to the current S-instruction bit address
   - Use 7C Read/Write Memory to fetch instruction fields
   - Use FB (FU/FL/FT) to describe the field format

2. Decode the S-instruction
   - Extract the opcode and operand fields
   - Branch to the appropriate handler micro-routine

3. Execute the S-instruction
   - Use micro-operators to perform the specified operation
   - May involve multiple memory accesses, arithmetic operations,
     stack manipulation, etc.

4. Advance to the next S-instruction
   - Update FA to point past the completed instruction
   - Loop back to step 1
```

### 2.2 Interpreter Storage

- **B1710**: Interpreters reside in **S-Memory** (main memory) alongside user
  data and S-code. This means micro-instruction fetches compete with data
  accesses for memory bandwidth.

- **B1720**: The active interpreter is loaded into **M-Memory** (bipolar control
  memory at 167 ns). Less frequently used micro-routines may remain in S-Memory.
  This separation gives ~6× faster microcode execution.

### 2.3 Available Interpreters

Burroughs developed interpreters for:

| S-Language | Source Language | Notes |
|------------|---------------|-------|
| S-COBOL | COBOL-74 | Primary business language |
| S-FORTRAN | FORTRAN IV | Scientific computing |
| S-RPG | RPG II | Report generation |
| S-BASIC | BASIC | Interactive computing (B1720 only initially) |
| S-SDL | SDL (Software Development Language) | System programming; MCP written in SDL |
| S-MCP | MCP operating system | Special interpreter for OS itself |

### 2.4 Interpreter Performance

Because each interpreter is optimized for its language:
- S-COBOL efficiently handles packed decimal, field-to-field moves, EBCDIC operations
- S-FORTRAN efficiently handles floating-point, array indexing
- S-RPG efficiently handles record-level I/O

This specialization means each virtual machine is highly efficient for its domain,
even though the underlying hardware is simple.

---

## 3. SDL and SDL2 (System Definition Language)

### 3.1 Purpose

SDL is Burroughs' primary system programming language and the language in which
the MCP operating system is written. It is also the meta-language used to
**formally define** other S-languages.

SDL2 is the evolved version. In documentation, "SDL" and "SDL2" are sometimes
used interchangeably.

### 3.2 SDL as S-Language Definer

SDL provides a BNF-like grammar notation for defining the formats of S-language
instructions. A typical SDL definition might specify:

```
ADD-INSTRUCTION ::=
  OPCODE(6 bits, value = 0x1A)
  SOURCE-ADDRESS(variable, 12-24 bits)
  DEST-ADDRESS(variable, 12-24 bits)
  LENGTH-FIELD(4 bits)
```

This formal definition drives both the compiler (which generates S-instructions
in the defined format) and the interpreter (which decodes them).

### 3.3 SDL as Programming Language

SDL is also a compiled programming language (similar to PL/I or ALGOL in style)
used for writing:
- The MCP itself
- System utilities
- Compilers
- User applications

The SDL interpreter is always present in the system because the MCP requires it.

---

## 4. MIL (Micro Implementation Language)

### 4.1 Purpose

MIL is the **microprogramming language** used to write interpreters. It is a
high-level, English-like language that compiles to sequences of 16-bit
micro-operators.

### 4.2 Characteristics

- **English-like syntax**: Source reads like imperative English sentences
  ```
  MOVE X TO Y
  MOVE LITERAL 5 TO CPL
  READ MEMORY FA FIELD LENGTH 8 TO X COUNT FA UP
  IF X EQUALS Y THEN BRANCH TO DONE
  ```
- **Compiled**: MIL source is compiled by the MIL compiler (which itself runs
  on the B1700 under the SDL interpreter)
- **One MIL statement ≈ one micro-operator**: The mapping is fairly direct
- **Hardware-aware**: MIL programmers must understand the register set, timing,
  and hardware capabilities

### 4.3 Significance for Emulation

Since all B1700 software ultimately runs as micro-operators, the emulator needs
only to implement the ~24 micro-operators correctly. If the micro-operators are
correct, any MIL-compiled interpreter will run, and any S-language program will
execute correctly through its interpreter.

**The emulator does NOT need to understand S-languages or MIL directly.** It only
needs to faithfully execute micro-operators.

---

## 5. Interpreter Switching (Gismo)

### 5.1 The Problem

In a multiprogramming environment, multiple programs written in different
languages may be running concurrently. Each needs its own interpreter. The
system must rapidly switch between interpreters when context-switching between
programs.

### 5.2 The Mechanism

The MCP uses a component called **Gismo** (or the interpreter dispatcher) to
manage interpreter switching:

1. When the MCP schedules a new program (or returns control to a suspended one),
   Gismo checks whether the required interpreter is already loaded
2. If the interpreter is in M-Memory (B1720) or in S-Memory (B1710), execution
   can begin immediately by setting MAR(A) to the interpreter's entry point
3. If the interpreter is not loaded, Gismo loads it from disk into memory
4. The processor's registers and scratchpad are restored to the saved context
   of the resuming program
5. Execution continues

### 5.3 M-Memory Management (B1720)

On the B1720, the MCP uses one of five **M-Memory management strategies**
based on available M-Memory space:

| Strategy | Condition | Description |
|----------|-----------|-------------|
| **Abundant** | >80% M-Memory free | Load all active interpreters |
| **Ample** | 60–80% free | Load most frequent interpreters |
| **Adequate** | 40–60% free | Load only current interpreter plus MCP |
| **Precious** | 20–40% free | Load only hot paths of current interpreter |
| **Bare** | <20% free | Run from S-Memory; M-Memory for MCP only |

This adaptive strategy ensures optimal use of the scarce high-speed M-Memory.

### 5.4 Interpreter Independence

Key architectural feature: **interpreters are location-independent**. They can
be loaded at any base address in memory and will execute correctly because all
internal references are relative (via branch displacements and scratchpad-relative
addressing).

This is essential for multiprogramming — multiple interpreters can coexist in
memory without conflict.

---

## 6. The Execution Model in Detail

### 6.1 Complete Execution Flow

```
┌─────────────────────────────────────────────────────────┐
│                    MCP (SDL Interpreter)                  │
│                                                           │
│  1. Load user program's S-code from disk to S-Memory     │
│  2. Load the appropriate interpreter into memory          │
│  3. Set up BR/LR for memory protection                   │
│  4. Load FA, FB, Scratchpad with program context         │
│  5. Branch to interpreter entry point                    │
│                                                           │
├─────────────────────────────────────────────────────────┤
│                 Interpreter (e.g., S-COBOL)               │
│                                                           │
│  6. Fetch S-instruction from memory at FA                │
│  7. Decode opcode: e.g., "ADD PACKED DECIMAL"            │
│  8. Execute: series of micro-operators:                  │
│     - Load operands from memory using 7C                 │
│     - Set up CPU=01 (BCD), CPL from field length         │
│     - Read SUM pseudo-register                           │
│     - Write result back to memory using 7C               │
│  9. Advance FA past the S-instruction                    │
│  10. Check for interrupts (test XYST INT OR bit)         │
│  11. If interrupt: save context, return to MCP           │
│  12. Otherwise: loop to step 6                           │
│                                                           │
├─────────────────────────────────────────────────────────┤
│              Micro-Operator Hardware                      │
│                                                           │
│  13. Each micro-operator executes in 2–8 clocks          │
│  14. Hardware: registers, ALU, FIU, memory               │
└─────────────────────────────────────────────────────────┘
```

### 6.2 Example: BCD Addition at the Micro Level

From the manual's Figure I-8, here's how a 4-bit BCD sum (from an S-language
ADD instruction) translates to micro-operators:

```
; Interpreter routine for S-language "ADD" instruction
; Operands are packed BCD in memory

; 1. Load first operand digit from memory
7C: READ MEMORY → X, field=4 bits, count FA up

; 2. Load second operand digit from memory  
7C: READ MEMORY → Y, field=4 bits, count FA up

; 3. Set up for BCD arithmetic
3E: BIAS (set CPU from FU, CPL from FL)

; 4. Clear carry if needed
6E: SET CYF TO 0

; 5. Read sum (combinatorial - just read the SUM pseudo-register)
1C: MOVE SUM → T

; 6. Store overflow bit
6E: SET CYF TO CYL (capture carry out)

; 7. Store result digit to memory
7C: WRITE T → MEMORY, field=4 bits, count FA up

; 8. Count FL down, loop if more digits
6D: COUNT FL DOWN
; test FL, branch if not zero to step 1

; 9. Return to interpreter fetch routine
1C: MOVE TAS → MAR(A)  ; pop return address
```

---

## 7. Emulator Implications

### 7.1 What to Implement

The emulator needs **only** the micro-operator level:

| Must Implement | Not Needed (handled by microprograms) |
|----------------|---------------------------------------|
| 24 micro-operators | S-language instruction decode |
| Register file | Field descriptor processing |
| Memory (bit-addressable) | BCD arithmetic rules |
| 24-bit and 4-bit function boxes | COBOL/FORTRAN semantics |
| FIU (rotator + mask) | File I/O |
| I/O bus protocol | String operations |

If the micro-operators are implemented correctly, the interpreters (loaded as
data into memory) will handle everything else.

### 7.2 Testing Strategy

1. **Micro-level tests**: Test each micro-operator independently
2. **Load the SDL interpreter**: If it boots and the MCP starts, the basic
   micro-operators are working
3. **Run SDL programs**: Validates the interpreter loop
4. **Load other interpreters**: COBOL, FORTRAN, etc.
5. **Run standard test suites**: Burroughs provided diagnostic microcode on
   cassette tape

### 7.3 Where to Find Interpreter Microcode

Interpreter microcode is stored on disk as part of the B1700 software
distribution. The bitsavers archive has some software images:
- `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/B1000_Software/`
- Individual interpreter specifications in internal memos
- SDL specifications that define S-language formats

---

## 8. Emulation Software

### 8.1 User-Designed S-Languages

The B1700 architecture explicitly supports user-designed S-languages and
interpreters. A user could:

1. Define a custom S-language using SDL notation
2. Write a custom interpreter in MIL
3. Compile the interpreter
4. Load it and run programs in the custom language

However, custom interpreters may not operate under MCP control — the user must
also develop their own bootstrap loader and I/O routines.

### 8.2 Emulation of Other Computers

Burroughs released emulators for other computer architectures:

| Emulated System | Type | Notes |
|-----------------|------|-------|
| IBM 1401/1440/1460 | Stand-alone or MCP interpreter | Popular upgrade path |
| Burroughs B100/200/300/500 | Stand-alone or MCP interpreter | Migration from older Burroughs |
| IBM 1130 | MCP interpreter | Scientific computing |
| Burroughs B3500 | MCP interpreter | Migration from medium systems |

These "emulators" are actually just interpreters whose S-language happens to be
the instruction set of another computer. This demonstrates the power of the
B1700's interpreter architecture — any instruction set can be implemented as a
microprogram.

---

*Previous: [05 — ALU and Data Paths](05_ALU_DATAPATH.md) | Next: [07 — MCP Operating System](07_MCP.md)*
