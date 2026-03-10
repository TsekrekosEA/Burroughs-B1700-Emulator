# Artifacts Catalog

> A complete inventory of every tape image, microcode program, S-language program,  
> and reference document in the project, with provenance, format details, and usage instructions.

---

## Table of Contents

- [Overview](#overview)
- [Microcode Artifacts](#microcode-artifacts)
- [S-Language Programs](#s-language-programs)
- [Tape Images](#tape-images)
- [Reference Documents](#reference-documents)
- [Tape Format Reference](#tape-format-reference)
- [Using the Tape Reader](#using-the-tape-reader)

---

## Overview

```
artifacts/
├── cards/
│   ├── cold_start_loader.mil    # Original 1972 MIL source
│   └── cold_start_loader.bin    # Assembled binary (576 bytes)
├── interpreters/
│   ├── s_calc_interp.mil        # S-CALC stack machine interpreter
│   ├── s_fort_interp.mil        # S-FORT register machine interpreter
│   └── s_fort_interp_gismo.mil  # S-FORT variant for Gismo demo
├── programs/
│   ├── demo1.scalc              # 100 + 200 = 300
│   ├── demo2.scalc              # (3 + 4) × 2 = 14
│   ├── demo3.scalc              # Stack manipulation demo
│   ├── fibonacci.sfort          # First 10 Fibonacci numbers
│   ├── arith.sfort              # 3×5, 15-5, 15+10
│   ├── gismo_calc.scalc         # Cumulative addition (Gismo mode)
│   └── gismo_fib.sfort          # First 5 Fibonacci numbers (Gismo mode)
├── tape/
│   ├── fetest/                  # FE diagnostic test tape
│   ├── cm_series_cobol/         # COBOL compiler/runtime tape
│   ├── symco/                   # Symbolic debugger tape
│   ├── franks_trs/              # System image tape
│   ├── histep_III/              # Test pattern tape
│   └── b1000_mk10.0.tap        # Full B1000 Mark 10.0 system tape ★
└── zips/
    └── *.zip                    # Original archive files from bitsavers
```

---

## Microcode Artifacts

### Cold Start Loader (`artifacts/cards/cold_start_loader.mil`)

| Field | Value |
|-------|-------|
| **Origin** | bitsavers.org, `/pdf/burroughs/B1700/` |
| **Date** | October 1972 |
| **Size** | 13.7 KB (MIL source), 576 bytes (binary) |
| **Assembles to** | 288 words (16-bit micro-instructions) |
| **Authenticity** | Original Burroughs MIL source, real 1970s microcode |

The Cold Start Loader (CSL) is the B1700's first-stage bootloader. It runs directly on the bare hardware after GPCLR (power-on reset) and:

1. Establishes the micro-execution environment
2. Clears S-Memory
3. Initializes I/O on port 2 (card reader)
4. Reads the EMV card deck (operating system loader)
5. Hands off to the secondary loader via OVERLAY

This is the only surviving MIL source code for the B1700 known to be publicly available. A photo of the original punched card deck also exists in the bitsavers archive.

Usage:
```bash
./mil_asm artifacts/cards/cold_start_loader.mil csl.bin    # Assemble
./b1700 --load csl.bin --run                                # Execute (4,317 cycles)
./b1700 --load csl.bin --debug                              # Debug interactively
```

### S-CALC Interpreter (`artifacts/interpreters/s_calc_interp.mil`)

| Field | Value |
|-------|-------|
| **Origin** | Created for this project |
| **Size** | ~160 lines MIL, 240 bytes binary |
| **Assembles to** | 120 words |
| **Architecture** | Stack machine, 8 opcodes |

A from-scratch interpreter implementing a stack-based calculator language. Demonstrates the B1700's core concept: defining an instruction set entirely in microcode.

Key techniques:
- XCH context switching between interpreter data and S-code program
- Sequential opcode decode via LIT + comparison
- Stack operations via FA/FL manipulation in S-memory
- MONITOR instruction for output

Usage:
```bash
./mil_asm artifacts/interpreters/s_calc_interp.mil s_calc.bin
./scalc_asm artifacts/programs/demo1.scalc demo1.bin
./b1700 --interp s_calc.bin --scode demo1.bin --run
```

### S-FORT Interpreter (`artifacts/interpreters/s_fort_interp.mil`)

| Field | Value |
|-------|-------|
| **Origin** | Created for this project |
| **Size** | ~200 lines MIL, 422 bytes binary |
| **Assembles to** | 211 words |
| **Architecture** | Register machine (R0–R3), 10 opcodes |

A register-based interpreter demonstrating three-address instructions, subroutine calls, and jump table dispatch. More complex than S-CALC, closer to what a real S-FORTRAN interpreter would look like.

Key techniques:
- CALL/EXIT subroutines for register read/write dispatch
- Jump tables, 4-way branch on register ID
- Scratchpad registers, R0-R3 stored in scratchpad entries S4-S7
- 16-bit immediate operands, conditional branching

Usage:
```bash
./mil_asm artifacts/interpreters/s_fort_interp.mil s_fort.bin
./sfort_asm artifacts/programs/fibonacci.sfort fib.bin
./b1700 --interp s_fort.bin --scode fib.bin --run
```

### S-FORT Gismo Variant (`artifacts/interpreters/s_fort_interp_gismo.mil`)

Identical to `s_fort_interp.mil` except `S_START` is set to `0x6000` instead of `0x4000`. This places the S-FORT program at a different memory region, allowing it to coexist with an S-CALC program during Gismo interpreter switching.

**Used exclusively with the `--gismo` mode.**

---

## S-Language Programs

### S-CALC Programs

| File | Description | Expected Output | Cycles |
|------|-------------|-----------------|--------|
| `demo1.scalc` | PUSH 100, PUSH 200, ADD, PRINT | 300 | 330 |
| `demo2.scalc` | PUSH 3, PUSH 4, ADD, PUSH 2, MUL, PRINT | 14 | 589 |
| `demo3.scalc` | Stack manipulation: DUP, SWAP | (various) | ~400 |
| `gismo_calc.scalc` | Cumulative addition: 10, 10+10, 10+20, ... with DUP+PRINT | 10, 20, 30, 40, 50 | ~1,520 |

**Compiling S-CALC programs:**
```bash
./scalc_asm <source.scalc> <output.bin>
```

**Syntax:**
```
PUSH <decimal_number>    ; Push 24-bit value
ADD                      ; Pop two, push sum
SUB                      ; Pop two, push difference
MUL                      ; Pop two, push product
PRINT                    ; Pop and output
DUP                      ; Duplicate top
SWAP                     ; Swap top two
HALT                     ; Stop
; Comments with semicolons
```

### S-FORT Programs

| File | Description | Expected Output | Cycles |
|------|-------------|-----------------|--------|
| `fibonacci.sfort` | First 10 Fibonacci numbers | 1, 1, 2, 3, 5, 8, 13, 21, 34, 55 | 13,660 |
| `arith.sfort` | 3×5=15, 15-5=10, 15+10=25 | 15, 10, 25 | 1,429 |
| `gismo_fib.sfort` | First 5 Fibonacci numbers (shorter for Gismo demo) | 1, 1, 2, 3, 5 | ~6,980 |

**Assembling S-FORT programs:**
```bash
./sfort_asm <source.sfort> <output.bin>
```

**Syntax:**
```
label:                   ; Labels end with colon
    LOAD R0, 42          ; Rd, immediate16
    ADD R0, R1, R2       ; Rd, Rs1, Rs2
    SUB R0, R1, R2       ; Rd, Rs1, Rs2
    MUL R0, R1, R2       ; Rd, Rs1, Rs2
    MOV R0, R1           ; Rd, Rs
    CMP R0, R1           ; Compare (sets flag)
    BEQ label            ; Branch if equal
    BNE label            ; Branch if not equal
    PRINT R0             ; Output register
    HALT                 ; Stop
; Comments with semicolons
```

---

## Tape Images

All tape images were obtained from [bitsavers.org](http://bitsavers.org/bits/Burroughs/B1700/) and are in SIMH `.tap` format.

### fetest.tap: FE Diagnostic Tests

| Field | Value |
|-------|-------|
| **Source** | `bitsavers.org/bits/Burroughs/B1700/fetest.zip` |
| **Size** | 903 KB (60 files on tape) |
| **Format** | Burroughs labeled tape with SAVEDISK headers |
| **Content** | Field Engineering diagnostic test microcode |

Contains diagnostic microprograms used by Burroughs field engineers to test B1700 hardware. The "SAVEDISK" label in file headers suggests these are disk image captures rather than raw microcode files.

**Potential use**: If the binary format can be decoded, these diagnostics could be executed on the emulator to validate hardware fidelity.

### cm_series_cobol.tap: COBOL Compiler/Runtime

| Field | Value |
|-------|-------|
| **Source** | `bitsavers.org/bits/Burroughs/B1700/cm_series_cobol.zip` |
| **Size** | 293 KB (19 files) |
| **Content** | COBOL compiler and/or runtime components |

Likely contains S-COBOL interpreter microcode and/or the COBOL source-to-S-code translator.

### symco.tap: Symbolic Debugger

| Field | Value |
|-------|-------|
| **Source** | `bitsavers.org/bits/Burroughs/B1700/symco.zip` |
| **Size** | 7.7 MB (48 files) |
| **Content** | Burroughs symbolic debugger |

The symbolic debugger (SYMCO) was used for debugging S-language programs at the source level. This appears to be a complete software release.

### franks_trs.tap: System Image

| Field | Value |
|-------|-------|
| **Source** | `bitsavers.org/bits/Burroughs/B1700/franks_trs.zip` |
| **Size** | 15.4 MB |
| **Content** | System image (possibly a full disk dump) |
| **Known Issues** | 2 medium read errors during tape parsing |

The largest single artifact. May contain a complete system installation including MCP, interpreters, and user programs. The "franks" label suggests this was a personal backup from a Burroughs engineer or customer.

### histep_III.tap: Test Tape

| Field | Value |
|-------|-------|
| **Source** | `bitsavers.org/bits/Burroughs/B1700/histep_III.zip` |
| **Size** | 613 KB (3 files) |
| **Content** | Test pattern data (repeating ABCDEF pattern) |

Appears to be a tape hardware test, useful for validating tape reader implementations but does not contain executable content.

### b1000_mk10.0.tap: B1000 Mark 10.0 System Release

| Field | Value |
|-------|-------|
| **Source** | `bitsavers.org/bits/Burroughs/B1700/b1000_mk10.0.tap` |
| **Size** | 9.6 MB (339 files) |
| **Volume Label** | "KEVIN" |
| **Content** | Complete B1000 Mark 10.0 software release |
| **Known Issues** | 19 medium errors in files 323 and 341 |

This is the most important artifact in the collection. It contains a complete Burroughs B1000-series software release, including:

- GISMO, the interpreter dispatcher (scheduling and memory management)
- MCP II, the Master Control Program (operating system)
- SDL interpreter, System Definition Language
- S-COBOL interpreter, COBOL microcode
- S-FORTRAN interpreter, FORTRAN microcode
- S-RPG interpreter, RPG microcode
- COLDSTART microcode, the production cold start loader

The volume label "KEVIN" suggests this was a specific installation's system tape. Despite being labeled "B1000," the Burroughs B1000 series (B1700/B1800) shares architecture, and the interpreters may be directly usable on the B1700 emulator.

Stretch goal: Extract the S-COBOL interpreter from this tape, load it into the emulator, and feed it a simple S-COBOL program. This would be running authentic 1970s Burroughs microcode on the emulator.

**Exploring the tape:**
```bash
./tape_reader --list artifacts/tape/b1000_mk10.0.tap
./tape_reader --dump artifacts/tape/b1000_mk10.0.tap
```

---

## Reference Documents

These are the primary sources used to build the emulator. PDFs are in `resources/` (gitignored due to size, but available from bitsavers).

| Document | File | Size | Source |
|----------|------|------|--------|
| B1700 FE Technical Manual (1973) | `1053360_B1700_FE_Tech_May73.pdf` | ~24 MB | bitsavers.org |
| FE Technical Manual (OCR) | `1053360_B1700_FE_Tech_May73_ocr.pdf` | ~22 MB | Local OCR pass |
| Datapro Report (1974) | `70C-112-04_7412_Burroughs_B1700.pdf` | — | bitsavers.org |
| Datapro Report (1978) | `70C-112-04_7803_Burroughs_B1700.pdf` | — | bitsavers.org |
| FE Manual OCR text | `manual_ocr.txt` | — | Extracted text |
| Wilner 1974 paper | `paper_1974.txt` | — | Extracted text |
| Wilner 1978 paper | `paper_1978.txt` | — | Extracted text |

### Where to Get Them

All primary sources are freely available from [bitsavers.org](http://bitsavers.org/pdf/burroughs/B1700/):

- **Manuals**: `bitsavers.org/pdf/burroughs/B1700/`
- **Tape images**: `bitsavers.org/bits/Burroughs/B1700/`
- **Internal memos** (S-language specs, MCP internals): `bitsavers.org/pdf/burroughs/B1700/`, 8 volumes of internal memos

### Key References (External)

| Title | Author | Year | Notes |
|-------|--------|------|-------|
| "Design of the Burroughs B1700" | W.T. Wilner | 1972 | AFIPS Conference Proceedings |
| "Burroughs B1700 Memory Utilization" | W.T. Wilner | 1972 | AFIPS Conference Proceedings |
| "The B1700 — A Micro-Implementation of Languages" | W.T. Wilner | 1974 | Microprogramming paper |
| "Recursive Machines" | W.T. Wilner | 1976 | Advanced interpreter concepts |
| "Interpreting Machines" | E.I. Organick | 1978 | Textbook covering B1700/B1800 |

---

## Tape Format Reference

### SIMH .tap Format

All tape images use the SIMH tape format:

```
┌──────────────┬──────────────────────┬──────────────┐
│ Record Length │      Data            │ Record Length │
│ (4 bytes LE) │  (variable length)   │ (4 bytes LE) │
└──────────────┴──────────────────────┴──────────────┘

Special values:
  0x00000000  =  Tape mark (file separator)
  0xFFFFFFFF  =  End of medium
  Bit 31 set  =  Error flag (record may be corrupt)
```

Records are framed by their length (repeated after the data for reverse reading). Each logical "file" on tape is separated by a tape mark.

### Burroughs Labeled Tape Format

Within the SIMH container, Burroughs tapes use a labeled format:

```
VOL label (80 bytes)    Volume identification
TAPDIR    (variable)    Tape directory
For each file:
  HDR label (80 bytes)  File header (filename, attributes)
  Data records (1812 bytes typical, with 12-byte headers)
  EOF label (80 bytes)  End of file
```

Each data record has a 12-byte EBCDIC header:
- Bytes 0–3: Record length
- Bytes 4–7: Sequence number
- Bytes 8–11: File number

---

## Using the Tape Reader

```bash
# List all records on a tape
./tape_reader --list artifacts/tape/fetest/fetest.tap

# Dump tape structure with hex content
./tape_reader --dump artifacts/tape/b1000_mk10.0.tap

# Extract files from tape
./tape_reader --extract artifacts/tape/b1000_mk10.0.tap output_dir/

# Show EBCDIC text content
./tape_reader --ebcdic artifacts/tape/b1000_mk10.0.tap
```

The tape reader automatically handles:
- SIMH record framing
- Tape marks and end-of-medium detection
- Error flag detection (bit 31)
- EBCDIC-to-ASCII translation
- Burroughs labeled tape header parsing

---

*For the B1700 hardware architecture, see [ARCHITECTURE.md](Architecture). For emulator implementation details, see [IMPLEMENTATION.md](Implementation). For the development story, see [JOURNEY.md](Journey).*
