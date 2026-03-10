# 01 — Architecture Overview

## 1. Introduction

The Burroughs B1700 (officially "Burroughs B 1700 Series") is a family of
small-scale business computer systems introduced in **June 1972**. Designed by
**Wayne T. Wilner, Ph.D.** at the Burroughs Santa Barbara Plant (Goleta, CA),
the B1700 is historically significant as one of the purest implementations of
**dynamically microprogrammed architecture**: the processor has no fixed
instruction set. Instead, the hardware provides a minimal set of ~24
**micro-operators** (16-bit microinstructions) that manipulate bit-addressable
memory and a small register file. All higher-level operations — including the
"machine language" for each programming language — are defined entirely by
**microcode interpreters** loaded at run-time.

Burroughs marketed this concept as **"variable micrologic"** and designated the
machine a **"Proper Language Processor."** The B1700 was described as a
"fourth generation" computer and was considered the most innovative small-scale
design since the IBM System/3 (1969).

---

## 2. Design Philosophy

### 2.1 The Problem with Conventional ISAs

Traditional computers define a single fixed instruction set architecture (ISA)
that all programming languages must compile to. This has several drawbacks:

- **Semantic gap**: High-level language constructs (e.g., COBOL record moves,
  FORTRAN array indexing) do not map cleanly to low-level machine instructions,
  requiring many instructions to express a single concept.
- **Memory waste**: Fixed word/byte boundaries waste storage when operand sizes
  vary.
- **Interpretation overhead**: A general-purpose ISA cannot be optimal for every
  language.

### 2.2 The Wilner Approach

Wilner's insight was to invert the relationship between hardware and language:

1. **Define an "ideal machine"** for each programming language — a virtual
   architecture whose instructions directly express that language's semantics.
   Burroughs called these virtual instruction sets **S-languages** (Secondary
   languages).

2. **Build minimal hardware** — just enough to support fast microcode execution:
   a bit-addressable memory, a 24-bit ALU, a field-isolation unit, and a small
   register file.

3. **Write a microcode interpreter** for each S-language. The interpreter
   fetches S-language instructions from memory, decodes them in microcode, and
   executes them using the micro-operators.

4. **Switch interpreters dynamically** when the operating system context-switches
   between programs written in different languages.

Because the S-language is tailored to each programming language, Burroughs
claimed that on average **only one-tenth** as many S-instructions need to be
executed compared to conventional machine-level code. The combined effect of
fewer instructions, denser encoding (thanks to bit-addressability), and
language-specific optimization yielded competitive performance from relatively
simple, inexpensive hardware.

### 2.3 Layered Execution Model

```
┌─────────────────────────────────────────────────┐
│            Application Source Code               │
│         (COBOL, FORTRAN, RPG, BASIC, SDL)        │
├─────────────────────────────────────────────────┤
│                   Compiler                       │
│  Translates source → S-language instructions     │
├─────────────────────────────────────────────────┤
│              S-Language Program                   │
│    Virtual machine code in main memory           │
├─────────────────────────────────────────────────┤
│         Interpreter (Microprogram)               │
│  Fetches & decodes S-instructions using micros   │
├─────────────────────────────────────────────────┤
│          Micro-Operator Hardware                 │
│  24 fixed 16-bit microinstructions               │
│  Bit-addressable memory, 24-bit ALU, FIU         │
└─────────────────────────────────────────────────┘
```

---

## 3. System Organization

### 3.1 Hardware Components

The B1700 system consists of:

| Component | Description |
|-----------|-------------|
| **S-Memory Processor** | The CPU. 9 logic cards (A–K), console, cassette loader. Contains all registers, ALU, FIU, sequencer, control logic |
| **S-Memory (Main Memory)** | MOS semiconductor storage, 8K–262K bytes, bit-addressable |
| **M-Memory (Control Memory)** | Bipolar high-speed storage, 2K–8K bytes (B1720 only). Stores active interpreter microcode |
| **Console** | Front panel with 24 lamps, 24 switches, mode switch, register select switches, cassette loader |
| **Cassette Tape Loader** | Bootstrap device, physically mounted on console. Loads initial microcode |
| **I/O Bus** | Connects up to 10 (expandable to 14) I/O controls |
| **I/O Controls** | Device-specific controllers for disk, tape, printer, card reader, communications |
| **Power Supply** | 24V AC / +5V / +19V / -2V / +23V |

### 3.2 Block Diagram

```
                          ┌──────────────────┐
                          │   CONSOLE        │
                          │  24 Lamps/Switch │
                          │  Mode Select     │
                          │  Cassette Loader │
                          └────────┬─────────┘
                                   │
┌──────────────────────────────────┼──────────────────────────────┐
│                     S-MEMORY PROCESSOR                          │
│                                  │                              │
│  ┌─────────┐  ┌──────────┐  ┌───┴────┐  ┌──────────────────┐  │
│  │Sequencer│  │ Control  │  │Register│  │  24-Bit Function  │  │
│  │& Decode │──│  Logic   │──│  File  │──│      Box (ALU)    │  │
│  └─────────┘  └──────────┘  └───┬────┘  └──────────────────┘  │
│                                  │                              │
│  ┌──────────────────┐    ┌──────┴──────┐  ┌────────────────┐  │
│  │ 4-Bit Function   │    │    Main     │  │  Rotator &     │  │
│  │    Box            │    │  Exchange   │  │  Mask Generator│  │
│  └──────────────────┘    │   (MX)      │  │    (FIU)       │  │
│                          └──────┬──────┘  └────────────────┘  │
│                                  │                              │
└──────────────────────────────────┼──────────────────────────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │              │              │
              ┌─────┴─────┐ ┌─────┴─────┐ ┌─────┴─────┐
              │ S-Memory  │ │ M-Memory  │ │  I/O Bus  │
              │ (Main)    │ │ (Control) │ │           │
              │ MOS       │ │ (B1720)   │ │ 10-14     │
              │ 8K–262K   │ │ Bipolar   │ │ Controls  │
              └───────────┘ │ 2K–8K     │ └───────────┘
                            └───────────┘
```

### 3.3 Processor Cards

The S-Memory Processor comprises 9 plug-in logic cards:

| Card | Function |
|------|----------|
| **A** | Local Memory LBUF, Left (A) Half |
| **B** | Local Memory Right (B) Half |
| **C** | Sequencer, Micro-operator Decode |
| **D** | Control Logic, Interrupt |
| **E** | Cassette Control, Tape Parity |
| **F** | 24-Bit Function Box (ALU), C Register (CC/CD/CP) |
| **G** | 4-Bit Function Box |
| **H** | Rotator and Mask Generator (FIU) |
| **J** | Memory Interface, Data paths |
| **K** | System Clock (8 MHz crystal), Real-time Clock, I/O Clock |

---

## 4. Processor Models

The B1700 series comprises two processor classes with multiple configurations:

### 4.1 B1710 Class (4 MHz, 250 ns cycle)

| Model | Primary Purpose | Memory Range | Notes |
|-------|----------------|--------------|-------|
| B1705 | Configurable entry-level | 24K–64K | Base system, any peripherals (no MICR) |
| B1707 | Magnetic entry | 24K–64K | Includes disk, printer, cassette, audit station |
| B1709 | Direct data entry | 48K–64K | Includes disk, printer, up to 8 CRT stations |
| B1713 | MICR entry | 48K–64K | Banking; MICR reader-sorter |
| B1717 | Extended memory | 32K–128K | 16K/32K increments, dual I/O bus support |

Common B1710 characteristics:
- 250 ns processor cycle time
- 16-deep × 24-bit Address Stack (A-Stack)
- 28 implemented microinstructions
- All microprograms reside in main memory (S-Memory)
- 1,000 ns memory read cycle / 1,500 ns write cycle
- Up to 10 I/O controls (14 with expansion)

### 4.2 B1720 Class (6 MHz, 167 ns cycle)

| Model | Primary Purpose | Memory Range | M-Memory | Notes |
|-------|----------------|--------------|----------|-------|
| B1720-1 | Configurable | 48K–262K | 2K–8K | Entry-level B1720 |
| B1724 | Configurable | 96K–262K | 4K–8K | Data communications; up to 32 comm lines |

Common B1720 characteristics:
- 167 ns processor cycle time (6× micro execution speed vs. B1710)
- 32-deep × 24-bit Address Stack
- 32 implemented microinstructions (4 more than B1710)
- 4 additional hardware registers
- Separate **M-Memory** (bipolar control memory, 167/225 ns cycle)
  for microcode storage
- 8-position Port Interchange for memory access control
- 667 ns memory read cycle / 1,000 ns write cycle
- Up to 32 communications lines
- 16K or 32K byte memory increments

### 4.3 Historical Models (Discontinued)

| Model | Introduced | Replaced By |
|-------|-----------|-------------|
| B1712 | June 1972 | B1705/B1707/B1709 |
| B1714 | June 1972 | B1705/B1707/B1709/B1713 |
| B1716 | March 1975 | B1717 |
| B1718 | July 1974 | B1720-1 |
| B1726 | July 1973 | B1720-1/B1724 |
| B1728 | July 1973 | B1724 |
| B1776 | April 1976 | B1724 |

---

## 5. Clock System

The system clock is an **8 MHz crystal-controlled oscillator** on Card K.

| Signal | Frequency | Description |
|--------|-----------|-------------|
| CLK8..KO | 8 MHz | Master clock |
| CLK4..KO | 4 MHz | Primary processor clock (÷2 of CLK8) |
| CLK4/.KO | 4 MHz | Inverted CLK4 |
| I/OCL.K. | — | I/O clock, 30 ns delayed from CLK4/ (via 50 ns delay line, 30 ns tap) |

One **processor clock cycle** = 250 ns (B1710) or 167 ns (B1720).

Microinstruction execution times are measured in **clocks** (multiples of one
processor cycle). Most micro-operators take 2–8 clocks.

### Real-Time Clock

A real-time clock interrupt fires every **100 milliseconds**, derived from the
primary power frequency (60 Hz rectified). This sets bit CC(2) (Timer
Interrupt) in the C register.

---

## 6. Modes of Operation

The processor operates in three modes, selected by the front-panel Mode Switch:

| Mode | Behavior |
|------|----------|
| **Run** | Continuous micro-instruction execution from S-Memory (or M-Memory on B1720). Stops on Halt button, Halt micro, or error |
| **Step** | Execute one micro-instruction per Start button press. Machine halts with next micro in M register |
| **Tape** | Execute micro-instructions from cassette tape. Stops on Halt, Halt micro, tape error, or Cassette Stop micro. Tape continues to next gap before stopping |

Additionally, the processor can be in:
- **Halt state**: Not executing. Console lamps show selected register contents. Console switches can load registers
- **Load state**: Transferring data from console switches to a selected register

---

## 7. Key Architectural Innovations Summary

| Innovation | Description |
|------------|-------------|
| **No fixed ISA** | Machine instructions are defined by loaded microcode, not hardware |
| **Bit-addressable memory** | No word/byte boundaries; any field from 1 to 65,535 bits |
| **Language-specific virtual machines** | Each programming language has its own "ideal machine" (S-language) |
| **Dynamic interpreter switching** | OS context-switches between language interpreters in microseconds |
| **Writable control store** | Microcode can be loaded from disk, tape, or cassette at any time |
| **S-language density** | ~10× fewer instructions than conventional machine code |
| **Universal emulation** | Can emulate other computer architectures (IBM 1401, B300, IBM 1130, etc.) |
| **Soft interrupts** | No hardware-automatic interrupt handling; fully software-controlled |

---

## 8. Emulator Target

For this emulator project, the **primary target is the B1710 processor** (as
documented in the 1973 Field Engineering Technical Manual), with provisions to
extend to the B1720 class. The B1710 is the simpler architecture and is fully
documented in the available primary sources. Key implementation targets:

- 24 micro-operators (16-bit fixed format)
- Bit-addressable main memory (S-Memory), minimum 8K bytes
- Full register set (~20 registers)
- 24-bit and 4-bit function boxes (ALU)
- 64-bit rotator and mask generator (Field Isolation Unit)
- Console simulation (lamp/switch display)
- Cassette tape bootstrap loader
- I/O bus with at least disk and console I/O controls
- MCP II operating system loading and execution
- At least one S-language interpreter (SDL recommended as first target, since MCP requires it)

---

*Next: [02 — Register Set](02_REGISTERS.md)*
