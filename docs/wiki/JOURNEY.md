# Development Journey

> The full story of building a Burroughs B1700 emulator from 1970s manuals,
> including the dead ends, the breakthroughs, and the bugs that rewrote the architecture.

---

## Table of Contents

- [Why the B1700?](#why-the-b1700)
- [Phase 1 — Reading the Manual](#phase-1--reading-the-manual)
- [Phase 2 — The First Instructions](#phase-2--the-first-instructions)
- [Phase 3 — The Decode Tree Problem](#phase-3--the-decode-tree-problem)
- [Phase 4 — The Toolchain](#phase-4--the-toolchain)
- [Phase 5 — Deciphering the Source](#phase-5--deciphering-the-source)
- [Phase 6 — Three Wrong Approaches to Conditionals](#phase-6--three-wrong-approaches-to-conditionals)
- [Phase 7 — Getting Closer](#phase-7--getting-closer)
- [Phase 8 — The Big Rewrite](#phase-8--the-big-rewrite)
- [Phase 9 — The Bootstrap Moment](#phase-9--the-bootstrap-moment)
- [Phase 10 — Discovering the Tapes](#phase-10--discovering-the-tapes)
- [Phase 11 — The Debugger](#phase-11--the-debugger)
- [Phase 12 — Building a Stack Machine](#phase-12--building-a-stack-machine)
- [Phase 13 — Building a Register Machine](#phase-13--building-a-register-machine)
- [Phase 14 — Interpreter Switching](#phase-14--interpreter-switching)
- [Reflections](#reflections)

---

## Why the B1700?

The Burroughs B1700 is one of the most unusual computers ever built. It has no fixed instruction set. The CPU's entire personality is defined by loaded microcode. When I first read Wilner's 1972 AFIPS paper describing a computer that could literally become a different machine for each programming language, I knew I wanted to build one.

The problem: no working emulator existed. The B1700 was produced from 1972 to 1986, used by banks, hospitals, and government agencies around the world, and then quietly faded into history. The documentation survived, barely, as scanned PDFs on bitsavers.org. But nobody had assembled these fragments into a running machine.

This is the story of building one.

---

## Phase 1 — Reading the Manual

The primary source is the B1700 Field Engineering Technical Manual (document 1053360, May 1973). It's 429 pages of circuit descriptions, timing diagrams, and register specifications, designed for repair technicians rather than emulator builders.

The first challenge was simply reading it. The scanned PDF is a photocopy of a typewritten document, and the text quality varies considerably. Characters are misread, register names lose characters, and bit positions occasionally shift by one. Some sections required going back to the original scan images to figure out what was actually written.

I spent the first two weeks reading and transcribing. I built an internal reference (13 documents covering every subsystem) cross-referencing the FE Manual against Wilner's papers and Organick's 1978 textbook.

Key discoveries from the reading phase:
- Every address is a bit address. Not bytes, not words, bits. This is fundamental to the architecture and I almost glossed over it.
- The function box is combinatorial. There is no "execute ADD" instruction. SUM, DIFF, XORY, and the rest are always available as register reads. You just read SUM and you get X+Y. This was so different from how I think about ALUs that it took several re-reads to sink in.
- 24 micro-operators is really all there is. I kept looking for hidden instructions, special modes, undocumented features. There aren't any. The machine is genuinely minimal at the micro level.

---

## Phase 2 — The First Instructions

I chose C++20 and a header-only design. The core emulator would be a set of `.h` files that any tool (emulator, tests, assembler) could include. No linking, no library management, no shared state problems.

The first working instruction was 1C MOVE, transferring a 24-bit value between registers. Getting this right required implementing the register file, which required implementing Table I-3 (the register map), a 16x4 matrix with irregular population and special semantics for half the entries.

The function box was the first "aha moment." Instead of a separate ALU execution stage, I implemented SUM, DIFF, and the rest as computed properties:

```cpp
reg24_t compute_SUM() const { return (X + Y) & MASK_24; }
```

Any instruction that reads from group 6, select 0 gets the current sum. No opcode needed. This is elegant in a way that modern ISAs aren't. The ALU is always working, you just choose when to look at its output.

---

## Phase 3 — The Decode Tree Problem

The first real bug appeared immediately: 7C memory instructions were being intercepted by the branch decoder.

The B1700's decode tree is not a simple opcode lookup. MC[15:12] is a 4-bit field, but instruction classes claim overlapping bit patterns. A 7C instruction (MC=0111) has bit 14 set (MC[15:14]=01), and at the time, my decoder checked for MC[15:14]=01 as a branch instruction before checking for MC=0111.

The fix was simple (reorder the checks), but the lesson was profound: the decode tree is a priority hierarchy, not a lookup table. This would come back to haunt me in Phase 6.

---

## Phase 4 — The Toolchain

Before I could test the emulator properly, I needed tools.

### The MIL Assembler

MIL (Micro Implementation Language) is an English-like language designed by Burroughs for writing microcode. It looks like this:

```
MOVE X TO Y
LIT H61 TO T
READ 8 BITS TO X INC FA
IF X EQL Y GO TO DONE
```

Each line maps to one micro-instruction (usually). Building the assembler forced me to understand every encoding in detail, which bits go where, which fields overlap, which constraints apply.

The assembler grew to nearly 2,000 lines. The two-pass design (parse+emit, then fixup labels) is standard, but `emit_literal()` was not. The B1700 has a bizarre constraint: 8C and 9C literal loads always write to select-2 of the specified group. If you say `LIT 42 TO X`, and X is at (group 4, select 0), the hardware actually writes to (group 4, select 2) = T.

The assembler transparently handles this by emitting `LIT to T` + `MOVE T to X`, but discovering this constraint and understanding why it exists took two days.

### The Tape Reader

Burroughs tape images use the SIMH `.tap` format (4-byte LE record frames) containing a Burroughs labeled tape format (VOL/HDR/EOF labels, 1812-byte records with EBCDIC headers).

I built a tape reader that could parse this format, discovering along the way that the `b1000_mk10.0.tap` file, originally downloaded as a "probably not interesting" system tape, contained 339 files including GISMO, MCP II, and interpreters for COBOL, FORTRAN, RPG, BASIC, and SDL. This was the most exciting discovery of the project, and it happened while building a diagnostic tool.

---

## Phase 5 — Deciphering the Source

With the assembler built, I pointed it at the real 1972 Cold Start Loader source. The first assembly produced 5 errors and 27 warnings.

Every single error was caused by artifacts in the scanned manual from which the MIL source had been transcribed:

1. **Character literals**: The assembler didn't handle character literals. MIL uses `?text?` for EBCDIC encoding, and `?//M?` is the "//M" card header in EBCDIC (0x6161D4). I had to build a full ASCII-to-EBCDIC translation table.

2. **Mangled DEFINE names**: The scans fused separators between names and values. What should have been `FLAG` appeared as `FLAG_`, with an extra trailing underscore the assembler treated as part of the name.

3. **Garbled keywords**: Some instruction mnemonics were corrupted in the scan, with characters dropped or merged. I added pattern recognition for common corruptions.

4. **Column-2 labels**: The CSL source has labels that start in column 2 (one leading space) rather than column 1. The assembler's label detection only looked for column-1 identifiers. I added a heuristic: if a word in column 2 contains hyphens and isn't a keyword, it's probably a label.

After Phase 5: 0 errors, 18 warnings, 304 words. The remaining 18 warnings were all conditional instructions, which brought about the next crisis.

---

## Phase 6 — Three Wrong Approaches to Conditionals

This was the most humbling phase. I tried three different approaches to encoding conditional instructions, and the first two were completely wrong.

### Attempt 1: 6C for Everything

My first idea: use 6C (conditional skip) for all conditions, including XYCN (the comparison register in group 12). The problem: XYCN is in group 12, and group 12 means MC[15:12]=1100, but MC=12 is a branch instruction. The decoder sees a branch before it ever considers 6C.

I had hit the decode tree priority problem again. Groups 12-15 in the MC field collide with branch and call opcodes.

### Attempt 2: 4C/5C Bit Test Branch

Maybe I could use 4C/5C (bit test and branch) instead? These use MC[15:14]=01. But MC[15:14]=01 means MC=4,5,6,7, which overlaps with groups 4-7. Another collision.

### Attempt 3: D-Class Bit Test Skip (The Solution)

The breakthrough came from re-reading the FE Manual's section on D-class secondaries. These instructions have MC=0000, with the instruction type in the MD field. Since MC=0 doesn't collide with any other instruction class, D-class is the escape hatch for encodings that don't fit the primary decode tree.

Bit Test Skip (MD=0xA) tests a single bit in any 4-bit register and conditionally skips the next instruction. It works for any register group because MC is always 0.

The final scheme:
- **Groups 0–3**: Use 6C (no collision, because MC=6 is the 6C opcode)
- **Groups 4+**: Use Bit Test Skip (MD=0xA in D-class)
- **IF...THEN blocks**: Invert skip + forward-jump placeholder + backpatch at END

This was also where I discovered that 6C variants must be 2 bits (V=0-3), not 3 bits. V>=4 corrupts the select field by spilling into its bit positions. No documentation mentioned this; it was discovered by staring at hex dumps of incorrectly-assembled instructions.

After Phase 6: 0 errors, 0 warnings, 316 words. Every CSL instruction assembled cleanly.

---

## Phase 7 — Getting Closer

Two emulator bugs emerged when actually executing the CSL:

**Bug 1: 7C field_len in wrong bits.** The 7C instruction encodes field length in bits [7:3] and count variant in [2:0]. My implementation read the entire byte [7:0] as field_len. So `READ 8 BITS` (encoded as (8<<3)|0 = 64) was being interpreted as field_len=64, capped to 24. The fix was a single shift: `(raw >> 3) & 0x1F`.

**Bug 2: Bit Test Skip did nothing.** When the skip condition was true, the emulator didn't advance MAR past the next instruction. The skip "succeeded" but the next instruction executed anyway. One line fix: `regs.MAR = (regs.MAR + 16) & MASK_19`.

Both bugs were found by single-stepping the CSL in the debugger and watching instructions produce wrong results. The CSL was becoming my best test suite.
---

## Phase 8 — The Big Rewrite

This was the crisis that nearly killed the project.

### The Smoking Gun

I was debugging why `SET CD TO 4` produced the wrong encoding. CD is register (13, 3). In my encoding model, MC[15:12] held the register group, so MC would be 13. But MC=13 is a branch instruction.

Suddenly, every high-group register operation would be misinterpreted. This wasn't a bug. The entire encoding model was wrong.

### The Revelation

I went back to the FE Manual, to the original scan images rather than the text extracts, and traced through the decode logic circuit by circuit. The truth was:

> MC[15:12] is always the opcode. The register group goes in MD[11:8].

This was the opposite of my assumption. I had built the entire emulator, assembler, and test suite around the wrong model, and it had mostly worked because many instructions had small group numbers that happened to be valid opcodes.

### The Rewrite

Over three days, I rewrote:
- types.h: entirely new per-instruction field accessors
- registers.h: complete remap of Table I-3 to correct (group, select) addresses
- processor.h: new decode tree, all execution functions rewritten
- mil_asm.cpp: ~75% rewritten with new REGISTERS map and all encode functions
- test_main.cpp: all test expectations updated

Approximately 75% of the codebase was rewritten. This was Phase 8.

### The Validation

After the rewrite:
- 28/28 unit tests pass (with corrected expectations)
- CSL assembles: 291 words, 0 errors, 0 warnings
- CSL executes 362 cycles correctly, reaching the I/O wait loop

Before Phase 8, the CSL had crashed immediately. After Phase 8, it ran. The encoding model was finally correct.

### What I Learned

If I could go back, I would build the assembler first and derive the emulator's decode tree from the assembler's encoding logic. I would trust the hardware documentation over my intuition about "how instruction sets should work." And I would test with real code (the CSL) from day one instead of hand-crafted test vectors.

The CSL found this bug; unit tests never would have.

---

## Phase 9 — The Bootstrap Moment

With the encoding fixed, the CSL ran to the I/O wait loop (362 cycles). But it wasn't done. It was waiting for a card reader that didn't exist.

### EMV I/O Protocol

The CSL expects a card reader on I/O port 2 using the EMV (Emulator/Virtual) protocol. I implemented `EMVHostControl`:

1. CSL sends an I/O command, EMV provides a preamble with unit type 0x0C (card reader)
2. CSL requests cards, EMV serves 80-byte EBCDIC card images
3. CSL validates "//M" header, processes data cards, detects "/EN" sentinel
4. CSL sets FA=0x5000, OVERLAY, halt (bootstrap complete)

### Five More Assembler Bugs

Getting the CSL to bootstrap also exposed five assembler bugs in conditional encoding:

1. **IF...THEN BEGIN...END**: The assembler had no concept of block conditionals. I had to implement condition inversion + forward-jump placeholder + backpatch at END.
2. **6C variant selection was wrong twice**: Once in `asm_if` (used V=2 instead of V=0) and once in `asm_skip`.
3. **6C inversion mask corrupted the select field**: Mask was 0xFF0F instead of 0xFF8F, and the extra bits spilled into the select field.
4. **6C inversion table was incomplete**: V=0↔V=4 and V=1↔V=5 (toggle bit 2).

Each bug was found by the CSL taking the wrong branch. The debugging cycle was: single-step → find wrong branch → trace to assembler output → find encoding error → fix → reassemble → re-run.

### The Moment

After fixing the last bug, I ran the CSL with full I/O:

```
Cycle     1: OVERLAY pass-through (FA=FB=0)
Cycle   847: EMV-READ-INITIATE on port 2
Cycle  2100: Preamble received, unit type 0x0C (card reader)
Cycle  2800: "//M" header card validated
...
Cycle  4317: OVERLAY — FA=0x5000 — bootstrap complete
```

4,317 cycles. The 1972 Cold Start Loader, the first program ever written for the B1700, assembled from its original MIL source and executed on an emulator for the first time in decades.

---

## Phase 10 — Discovering the Tapes

With the bootstrap working, I turned to the tape images. Building the tape reader was routine, but what I found was not.

The `b1000_mk10.0.tap` file, a 9.6 MB system tape that I had downloaded without much expectation, turned out to contain a complete B1000 Mark 10.0 software release: 339 files including GISMO (the interpreter dispatcher), MCP II (the operating system), and interpreters for SDL, COBOL, FORTRAN, RPG, and BASIC.

The volume label was "KEVIN", someone's system tape, preserved for 40+ years, containing the entire software stack of a Burroughs B1000-series computer.

This discovery changed the project's trajectory. Instead of writing toy interpreters as the endgame, there was now a real possibility of extracting and running authentic Burroughs microcode on the emulator.

---

## Phase 11 — The Debugger

Before tackling interpreters, I built a proper debugging environment. The interactive debugger supports:

- Breakpoints on address, opcode, cycle, register value, register change, memory access, and halt
- Watch expressions with optional break-on-change
- Trace filtering (full, branches-only, memory-only, I/O-only) with file output
- Live inspection of all registers, memory hex dumps, disassembly, scratchpad, and A-stack
- Ctrl-C interrupt to break into the REPL mid-execution

The disassembler handles all 16 MC classes, resolving register names from (group, select) pairs and correctly disassembling two-word 9C instructions.

The debugger was essential for Phases 12-14. Developing microcode interpreters without it would have been impractical.

---

## Phase 12 — Building a Stack Machine

With the debugger ready, it was time to demonstrate the B1700's defining feature: software-defined instruction sets.

### S-CALC Design

I designed S-CALC as the simplest possible S-language: a zero-address stack machine with 8 opcodes (HALT, PUSH, ADD, SUB, MUL, PRINT, DUP, SWAP). Every operand comes from and goes to a stack in S-memory.

### Writing the Interpreter

The S-CALC interpreter is 120 microcode words, 120 of the B1700's primitive micro-operators arranged to fetch, decode, and execute S-CALC instructions.

The most important technique is XCH context switching. The interpreter needs to read from two different memory regions: the S-code program (to fetch instructions) and the stack (to push/pop operands). Each region is tracked by an FA:FB pair stored in the scratchpad:

```
XCH S1 F S0    ; Swap FA:FB with scratchpad entry S0
               ; Now FA points to the S-code program
READ 8 BITS    ; Fetch opcode
XCH S1 F S0    ; Swap back — FA returns to interpreter context
```

This is a microcosm of what the real B1700 does: the interpreter lives in one world, the user's program in another, and XCH is the bridge between them.

### The MONITOR Instruction

For output, I added a MONITOR instruction (MC=0, MD=9, encoding 0x0900). On real hardware, MONITOR was a breakpoint/trace aid. In the emulator, it triggers a host callback that displays the X register value, which is how S-CALC's PRINT instruction works.

### First Run

```
./b1700 --interp s_calc.bin --scode demo1.bin --run
[MONITOR] PRINT: 300 (0x00012C)
System halted at cycle 330  MAR=0x000F0
```

PUSH 100, PUSH 200, ADD, PRINT, and out comes 300. Correct.

The emulator just became a calculator, but a calculator whose instruction set was loaded from software.

---

## Phase 13 — Building a Register Machine

S-CALC showed the concept, but it's too simple to be convincing. A stack machine with no registers doesn't look like a "real" computer. So I designed S-FORT: a three-address register machine with LOAD, ADD, SUB, MUL, MOV, CMP, BEQ, BNE, and PRINT.

### The Register Access Problem

S-FORT has four registers (R0-R3), stored in scratchpad entries S4-S7. But the B1700 has no indirect addressing for the scratchpad. You can't say "load from scratchpad entry N" where N is a variable.

The solution: CALL/EXIT jump tables. A `READ_REG` subroutine takes a register ID in X and dispatches to one of four scratchpad loads:

```
READ_REG:
    IF X == 0 → LOAD FROM S4A → EXIT
    IF X == 1 → LOAD FROM S5A → EXIT
    IF X == 2 → LOAD FROM S6A → EXIT
    LOAD FROM S7A → EXIT
```

This is called from every instruction that accesses registers, and a matching `WRITE_REG` handles stores. It's not elegant by modern standards, but it's how real B1700 interpreters worked. Lacking indirect addressing, you dispatch explicitly.

### Fibonacci

The proof that S-FORT is a real programming language:

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
```

13,660 micro-cycles. Each S-FORT instruction takes 20-60 micro-operators, and that's the overhead of interpretation. It's the same overhead the real B1700 paid for running COBOL or FORTRAN.

---

## Phase 14 — Interpreter Switching

The final demonstration: **Gismo-style interpreter switching.**

### The Setup

Two programs running concurrently on the same CPU:
- Process 0: S-CALC program computing cumulative additions (10, 20, 30, 40, 50)
- Process 1: S-FORT program computing Fibonacci numbers (1, 1, 2, 3, 5)

Each gets a 50-micro-cycle quantum before Gismo preempts it and switches to the other.

### The Implementation

Every context switch:
1. Saves all processor state: X, Y, T, L, FA, FB, BR, LR, MAR, M, U, CP, all 16 scratchpad entries, the entire A-stack, and the stack pointer
2. Clears the microcode store (addresses 0x000-0x3FF)
3. Loads the other interpreter's binary into the microcode store
4. Restores the other process's state
5. Resumes execution

This is faithful to what the real Gismo did. The entire CPU personality changes on each switch.

### The Result

```
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

Context switches: 28  |  S-CALC: 1,520 cycles  |  S-FORT: 6,980 cycles
```

The interleaved output tells the story: S-CALC runs for a quantum, outputs some values. Gismo saves its state, overlays the S-FORT interpreter, restores S-FORT's state, and lets it run. Back and forth, 28 times, until both programs complete correctly.

This is the B1700's defining feature, made visible for the first time on a modern system.
---

## Reflections

### What Worked

- Testing with real software (the CSL) from the earliest possible moment. This found architectural bugs that no amount of unit testing would have caught. The Phase 8 encoding rewrite was discovered entirely because the CSL produced the wrong encoding for `SET CD TO 4`.

- Building the assembler alongside the processor. They validate each other. An assembler bug produces wrong encodings; a processor bug produces wrong execution. When both are wrong in the same way, you know the encoding model is wrong.

- The header-only design. Being able to `#include "processor.h"` in any tool kept the codebase simple and the edit-compile-test cycle fast.

### What I'd Do Differently

- Start with the correct encoding model. The Phase 8 rewrite cost three days and touched 75% of the code. If I'd spent more time upfront verifying the MC field's meaning against the hardware decode circuits rather than inferring it from instruction descriptions, this could have been avoided.

- Build the debugger earlier. I built it in Phase 11, but I needed it desperately in Phases 6-9. Debugging microcode without a proper debugger meant manually reading hex dumps and counting cycles by hand.

- Take the source quality seriously. Half the Phase 5 bugs came from artifacts in the scanned documents. A systematic cleanup pass at the beginning would have saved time.

### The Bigger Picture

The B1700 represents a road not taken in computer architecture. The idea of a CPU that changes its instruction set per program is still radical today. We have JIT compilers and hardware virtualization, but nothing as clean as "load a 200-word microcode interpreter and the CPU becomes a COBOL machine."

Whether Wilner's approach was commercially practical is debatable (Burroughs eventually discontinued the B1700 line in 1986). But as a demonstration that the boundary between hardware and software is more fluid than we assume, it remains powerful.

Building this emulator was an exercise in computational archaeology. The artifacts are fragmentary, the documentation is degraded, and the people who designed the machine have largely moved on. But the design itself is preserved in those 24 micro-operators, and with enough patience, it runs again.

---

### Project Statistics

| Metric | Value |
|--------|-------|
| Total C++ source | ~8,000 lines |
| Build targets | 6 (emulator, tests, assembler, tape reader, 2 S-lang tools) |
| Unit tests | 28 (all passing) |
| MIL test programs | 6 |
| CSL bootstrap cycles | 4,317 |
| S-CALC interpreter | 120 microcode words |
| S-FORT interpreter | 211 microcode words |
| Gismo context switches | 28 |
| Phases of development | 14 |
| Major rewrites | 1 (Phase 8 — 75% of codebase) |
| Bugs found by CSL | Too many to count |

---

*For the B1700 hardware architecture, see [ARCHITECTURE.md](Architecture). For emulator implementation details, see [IMPLEMENTATION.md](Implementation). For to the S-language interpreters, see [INTERPRETERS.md](Interpreters).*
