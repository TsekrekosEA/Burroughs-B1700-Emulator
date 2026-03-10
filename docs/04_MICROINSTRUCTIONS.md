# 04 — Microinstruction Set

## 1. Overview

The B1700 has no fixed instruction set architecture. Instead, the hardware
implements a set of **micro-operators** (microinstructions) that provide
primitive operations on registers, memory, and ALU.

All microinstructions are **exactly 16 bits** wide.

The 16-bit word is divided into four 4-bit fields called **MC, MD, ME, MF**:

```
  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 ├───┬───┬───┬───┼───┬───┬───┬───┼───┬───┬───┬───┼───┬───┬───┬───┤
 │      MC       │      MD       │      ME       │      MF       │
 │  bits 15–12   │  bits 11–8    │   bits 7–4    │   bits 3–0    │
 └───────────────┴───────────────┴───────────────┴───────────────┘
```

The **MC field** (bits 15–12) determines the basic opcode. Further decode of
MD, ME, MF varies per instruction.

---

## 2. Instruction Identifier Convention

Microinstructions are identified by a code like **"7C"** or **"3E"**:
- The **number** is the value in bits 15–12 (MC) and sometimes bits 11–8 (MD)
- The **letter** denotes the instruction family/decode path

The letter suffixes:
- **C** = Primary micro-operators (most common)
- **D** = Secondary micro-operators (MC bits 15–14 = 00, bit 13 = 0)
- **E** = Tertiary micro-operators (MC = 0000, further decoded by MD)
- **F** = Quaternary micro-operators (MC = 0000, MD = 0000)

---

## 3. Timing

Execution times are measured in **clocks** (processor cycles):
- B1710: 1 clock = 250 ns
- B1720: 1 clock = 167 ns

Most micro-operators take **2–8 clocks**. The timing is documented per
instruction below.

---

## 4. Complete Microinstruction Reference

### 4.1 — 1C: Register Move

**Encoding:**
```
  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 │  SOURCE REG  │  SOURCE REG  │  DEST REG    │  DEST REG    │
 │   GROUP      │   SELECT     │    GROUP     │   SELECT     │
 │  (4 bits)    │  (4 bits*)   │   (4 bits)   │   (4 bits*)  │
```
*Note: Select uses only 2 bits; the remaining bits encode the variant.*

**Full encoding:**
```
MC [15:12] = Source Register Group (0–15)
MD [11:10] = Source Register Select (0–3)
MD [9:8]   = Variant (V)
ME [7:4]   = Destination Register Group (0–15)
MF [3:2]   = Destination Register Select (0–3)
MF [1:0]   = Reserved / additional variant
```

**Operation:** Move the contents of the source register to the destination
register. If registers are of unequal widths:
- Source wider than destination: data is **truncated from the left** (MSB)
- Source narrower than destination: data is **right-justified, zero-filled** on left

**Variants (V):**
- V=0: Simple move
- V=1: Move with skip (skip next micro if condition true)
- V=2: Move with 4-bit source from Function Box
- V=3: Other special forms

**Exceptions:**
- BICN, FLCN, XYCN, and XYST are **excluded** as operand registers when V=3
  or V=7
- When M is used as destination: incoming data is **OR'd** with the next
  incoming micro (instruction modification)
- MAR(A) as destination: adds 2 extra clocks (branch)

**Timing:** 2 clocks base. +2 if destination is MAR(A).

---

### 4.2 — 2C: Scratchpad Move

**Encoding:**
```
MC [15:12] = 0  (opcode identifier, further decoded)
MD [11:8]  = Scratchpad word address (0–15)
ME [7:4]   = Register Group
MF [3:2]   = Register Select
MF [1:0]   = Variant
```

**Variants:**
- V=0: Move Register → Left Scratchpad word (24 bits)
- V=1: Move Left Scratchpad word → Register
- V=2: Move Register → Right Scratchpad word (24 bits)
- V=3: Move Right Scratchpad word → Register

**Operation:** Transfer 24 bits between a specified register and one half
(left or right) of a 48-bit Scratchpad word.

**Timing:** 3 clocks.

---

### 4.3 — 3C: 4-Bit Manipulate

**Encoding:**
```
MC [15:12] = Operand Register Group (4-bit register from Table I-3)
MD [11:10] = Operand Register Select
MD [9:8]   = Function Select
ME [7:4]   = Destination Register Group (4-bit)
MF [3:2]   = Destination Register Select
MF [1:0]   = 0, 1 (identifies 3C + variant)
```

**Functions (in MD[9:8]):**

| Code | Function | Operation |
|------|----------|-----------|
| 00 | SET | Dest ← Source (copy 4-bit value) |
| 01 | AND | Dest ← Dest AND Source |
| 10 | OR | Dest ← Dest OR Source |
| 11 | EOR/XOR | Dest ← Dest XOR Source |

Additional variant-dependent operations:
- **INC**: Increment 4-bit destination by 1 (modulo 16)
- **DEC**: Decrement 4-bit destination by 1 (modulo 16)

**Timing:** 2 clocks.

---

### 4.4 — 4C: Bit Test Relative Branch False

**Encoding:**
```
MC [15:14] = 01 (identifies 4C/5C family)
MC [13]    = 0 (branch false)
MC [12:9]  = Register index (4 bits)
MD [8:7]   = Bit position to test (0-3)
ME:MF [6:0] = Signed displacement (7-bit two's complement, in words)
```

Register index mapping:
| Index | Register | Group,Select |
|-------|----------|-------------|
| 0–3   | TA–TD    | (0,0)–(0,3) |
| 4–5   | TE–TF    | (1,0)–(1,1) |
| 6–7   | FU–FT    | (2,0)–(2,1) |
| 8–11  | BICN–XYST | (12,0)–(12,3) |
| 12    | CC       | (13,2) |
| 13–15 | CA,CB,CD | (13,0),(13,1),(13,3) |

**Operation:** Test a specified bit in a 4-bit register. If the bit is
**false** (zero), branch by the signed displacement. If the bit is true,
continue to next sequential micro.

> **Note:** MC\[15:14\]=01 overlaps with register groups 4–7 in the primary
> C-class decode. The emulator's assembler prefers the D-class **Bit Test
> Skip** encoding (MD=0xA, see §4.4a) for conditional skip patterns, emitting
> 4C/5C only when an explicit displacement is needed.

**Timing:** 4 clocks (including next micro fetch time).

---

### 4.4a — Bit Test Skip (D-class, MD=0xA)

**Encoding:**
```
MC [15:12] = 0000 (secondary class)
MD [11:8]  = 1010 (0xA — identifies bit-test-skip)
ME [7]     = Sense: 0=skip if bit is FALSE, 1=skip if bit is TRUE
ME [6:3]   = Register index (4 bits, same table as 4C/5C above)
MF [2:1]   = Bit position to test (0–3)
MF [0]     = Reserved (0)
```

**Operation:** Test a single bit in a 4-bit condition or nibble register.
If the sense matches the bit value, skip the next microinstruction
(advance MAR by +2). Otherwise continue sequentially.

Used by the assembler for all `IF cond THEN`, `IF cond GO TO` (paired with
a 12C branch), and `SKIP WHEN` patterns that involve condition registers
(XYCN, FLCN, XYST, CC, CA, BICN) or any nibble register in groups 0–2 and
12–13.

**XYCN bit meanings** (after X-Y comparison):
- Bit 0: X > Y
- Bit 1: X < Y
- Bit 2: X ≠ Y
- Bit 3: X = Y

**Timing:** 2 clocks (no skip) or 4 clocks (skip taken).

---

### 4.5 — 5C: Bit Test Relative Branch True

Identical to 4C except the branch is taken when the tested bit is **true** (one).

**Timing:** 4 clocks.

---

### 4.6 — 6C: Skip When

**Encoding:**
```
MC [15:12] = Source Register Group (0–3 ONLY; groups ≥4 conflict with decode)
MD [11:10] = Source Register Select (0–3)
MD [9:8]   = Variant V (0–3)
ME [7:4]   = Test value / mask
MF [3:0]   = Additional encoding (MF[1:0]=11 identifies 6C class)
```

> **Constraint:** Only register groups 0–3 can be used with 6C. Groups 4–7
> conflict with 4C/5C in the decode tree, and groups 12–15 conflict with
> 12C–15C branch/call. For condition registers in groups 12–13, use the
> Bit Test Skip (§4.4a) encoding instead.

**Variants:**
- V=0: Skip if (val & mask) ≠ 0 (any masked bit set)
- V=1: Skip if val == mask (exact nibble equality)
- V=2: Skip if (val & mask) == 0 (no masked bits set; inverse of V=0)
- V=3: Skip if (val & mask) ≠ 0, then clear matched bits

**Operation:** Test a condition involving a 4-bit register nibble against
a mask/value. If the condition is true, **skip** the next microinstruction.

For `SKIP WHEN reg EQL val FALSE` (skip when not equal to a non-zero value),
the assembler emits a **two-word pattern**: V=1 (skip the next word if equal)
followed by V=2 mask=0 (unconditional skip of the target instruction).

**Timing:** 2 clocks (no skip) or 4 clocks (skip taken).

---

### 4.7 — 7C: Read/Write Memory

**Encoding:**
```
  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 │ 0│ 1│ 1│ 1│Dir│Reg │Rev│  Field Length  │  Count Var│
 │  MC=7       │   │Sel │   │    (0–31)     │   (0–7)   │
```

Detailed bit layout:
```
MC [15:12] = 0111 (7C identifier)
MD [11]    = Direction: 0 = to register (READ), 1 = to memory (WRITE)
MD [10:9]  = Register number: 00=X, 01=Y, 10=T, 11=L
MD [8]     = Memory field direction sign: 0=forward, 1=reverse
ME:MF [7:3] = Field length in bits (0–31; 0 means use CPL register)
MF [2:0]    = Count Variant (V)
```

**Operation:**
1. Transfer FA to MAR(A) (save current MAR in TEMPB)
2. Access memory at the bit address in MAR(A)
3. Pass data through the Field Isolation Unit (rotator + mask)
4. For READ: right-justify in register with zero-fill on left
5. For WRITE: read-modify-write with parity regeneration
6. Optionally count FA and/or FL per variant

If the field is longer than 24 bits, only 24 bits are transferred. If the
source register data exceeds the field length, data from the register is
**truncated from the left**.

**Count Variants (V, 3 bits):**

| V | Action |
|---|--------|
| 000 | No count |
| 001 | Count FA up by field length |
| 010 | Count FL up by field length |
| 011 | Count FA up, FL down |
| 100 | Count FA down, FL up |
| 101 | Count FA down |
| 110 | Count FL down |
| 111 | Count FA down, FL down |

**Timing:** 8 clocks.

---

### 4.8 — 8C: Move 8-Bit Literal

**Encoding:**
```
  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 │  DEST REG   │      8-BIT LITERAL         │  DEST REG    │
 │   GROUP     │                             │   SELECT=2   │
```

```
MC [15:12] = Destination Register Group
MD [11:8]  = 8-bit literal (upper nibble)
ME [7:4]   = 8-bit literal (lower nibble)
MF [3:0]   = Destination Register Select (assumed = 2)
```

*Wait — re-examining the manual: the register select is **assumed to be 2**.*

**Operation:** Move the 8-bit literal embedded in the instruction to the
destination register. If the register is wider than 8 bits, the literal is
**right-justified with zero fill** on the left.

**Valid destinations:** X, Y, T, L, MAR(A), BR, LR, FA, FB, FL, TAS, CP only.

**Timing:** 2 clocks. +2 if destination is MAR(A).

---

### 4.9 — 9C: Move 24-Bit Literal

**Encoding:**
This is a **two-word** (32-bit) instruction:
```
Word 1 (16 bits): MC/MD = encoded fields, ME/MF = MSB 8 bits of literal
Word 2 (16 bits): remaining 16 bits of literal
```

```
MC [15:12] = Destination Register Group
MD [11:8]  = Most significant 8 bits of 24-bit literal
ME [7:4]   = ...literal bits continued...
MF [3:0]   = Destination Register Select (assumed = 2)
```

**Operation:** Move the 24-bit literal to the destination register. If the
register is shorter than 24 bits, the literal is truncated from the left.

**Valid destinations:** X, Y, T, L, MAR(A), BR, LR, FA, FB, FL, TAS only.

Register select is assumed to be 2.

**Timing:** 6 clocks (fetches second word).

---

### 4.10 — 10C: Shift/Rotate T Register Left

**Encoding:**
```
MC [15:12] = Destination Register Group
MD [11:10] = Destination Register Select
MD [9]     = Shift/Rotate specifier: 0=shift, 1=rotate
MD [8]     = (part of count)
ME [7:4]   = Shift/Rotate count (combined with MD[8])
MF [3:0]   = ...count continued / destination
```

Full count is 5 bits, encoding 0–23 (if 0, uses CPL instead).

**Operation:**
1. Shift (or rotate) register T left by the specified bit count
2. Move the 24-bit result to the destination register

- **Shift**: Zero fill on the right, truncation on the left
- **Rotate**: Bits shifted off the left re-enter on the right
- If destination ≠ T, T is unchanged (result goes to destination)
- If shift count = 0, CPL value is used instead

**Width mismatch:** If destination is narrower than 24 bits, right-justify
with left truncation.

**Exceptions:**
1. When M is used as destination: operation becomes a **bit-OR** which
   modifies the next micro-operation
2. BICN, FLCN, XYCN, XYST are excluded as destinations

**Timing:** 3 clocks. +2 if destination is MAR(A).

---

### 4.11 — 11C: Extract from T Register

**Encoding:**
```
MC [15:14] = 00 (identifies 11C family via further decode)
MD [13:10] = Rotate bit count (0–23; 24 equivalent to 0)
ME [9:5]   = Width of extraction field (0–24 bits)
MF [3:2]   = Destination code: 00=X, 01=Y, 10=T, 11=L
MF [1:0]   = (variant)
```

**Operation:**
1. Rotate T left by the specified number of bits
2. Extract the specified number of rightmost bits from the rotated result
3. Move the result to the destination register, right-justified with zero fill

T is unchanged unless T is also the destination.

A rotation of 24 is equivalent to 0 (full rotation).

**Timing:** 3 clocks.

---

### 4.12 — 12C / 13C: Branch Relative

**Encoding:**
```
  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 │ 1│ 1│ 0│ S │          12-bit displacement                  │
```

```
MC [15:13] = 110 (12C) or 111 (13C, when bit 12 = 1)
MC [12]    = Displacement sign: 0=positive, 1=negative → becomes 13C
MD:ME:MF   = 12-bit displacement value (0–4095)
```

**Operation:** Fetch the next micro-instruction from the location computed by
adding the **signed displacement** (in 16-bit word units) to the word address
of the next in-line micro.

```
new_address = (MAR + 16) + (sign ? -displacement : +displacement) × 16
```

*Note: displacement is in 16-bit words, so multiply by 16 to get bit offset.*

**Timing:** 4 clocks.

---

### 4.13 — 14C / 15C: Call (Subroutine)

**Encoding:**
```
  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 │ 1│ 1│ 1│ S │          12-bit displacement                  │
```

```
MC [15:13] = 111 (14C) or bit 12=1 → 15C
MC [12]    = Displacement sign
MD:ME:MF   = 12-bit displacement
```

**Operation:**
1. **Push** the address of the next in-line micro-instruction onto the A-Stack
   (via TAS)
2. Branch to the computed address (same calculation as 12C/13C)

**Return method:** Use 1C Register Move: `TAS → MAR(A)` to pop the return
address and branch back.

**Timing:** 5 clocks (includes next micro fetch).

---

### 4.13a — DISPATCH (D-class, MD=0x1)

**Encoding:**
```
MC [15:12] = 0000 (secondary class)
MD [11:8]  = 0001 (identifies DISPATCH)
ME [7:4]   = Variant
MF [3:0]   = Additional encoding
```

**Variants:**
| ME | Operation | Description |
|----|-----------|-------------|
| 0x00 | LOCK + SKIP WHEN UNLOCKED | Lock the I/O bus; skip next if was unlocked |
| 0x10 | WRITE | Write data to I/O bus |
| 0x20 | READ AND CLEAR | Read I/O bus data, clear bus status |

**Operation:** Controls the I/O bus for DMA-style data transfers. DISPATCH
LOCK acquires the bus and conditionally skips. DISPATCH WRITE sends data.
DISPATCH READ AND CLEAR receives data and resets bus flags.

**Timing:** 2 clocks.

---

### 4.14 — 4D: Shift/Rotate X or Y

**Encoding:**
```
MC [15:12] = 0000
MD [11]    = Register specifier: 0=X, 1=Y
MD [10]    = Left/Right specifier: 0=left, 1=right
MD [9]     = Shift/Rotate specifier: 0=shift, 1=rotate
MD [8:4]   = Shift/Rotate count (0–24)
ME/MF      = Must be zeros (except count bits)
```

**Operation:** Shift (or rotate) register X or Y left or right by the
specified count.

- **Left shift**: Zero fill right, truncate left
- **Right shift**: Zero fill left, truncate right
- **Rotate**: Bits wrap around
- If count = 0, the shift amount is **zero** (unlike 10C which uses CPL)

**Timing:** 3 clocks (constant, regardless of count).

---

### 4.15 — 5D: Shift Concatenated X:Y

**Encoding:**
```
MC [15:12] = 0000
MD [11:9]  = 010 (identifies 5D)
MD [8]     = Left/Right variant: 0=left, 1=right
ME [7:4]   = Must be zeros
MF [3:0]   = Shift bit count (must be 1)
```

**Operation:** Shift the concatenated 48-bit register X:Y (X is MSB half, Y
is LSB half) left or right by **one bit**.

- Left: Zero fill on right (Y bit 0), truncate on left (X bit 23)
- Right: Zero fill on left (X bit 23), truncate on right (Y bit 0)
- If count ≠ 1, the result is **undefined**

**Timing:** 6 clocks.

---

### 4.16 — 6D: Count FA/FL

**Encoding:**
```
MC [15:12] = 0000
MD [11:8]  = 0110 (identifies 6D)
ME [7:5]   = Count variants (V, same as 7C)
ME [4:0]/MF = Literal count value (0–31)
```

If the literal value is 0, the value of CPL is used instead.

**Operation:** Increment or decrement FA and/or FL by the literal (or CPL).

**Count variants (same as 7C):**

| V | Action |
|---|--------|
| 000 | No count |
| 001 | Count FA up |
| 010 | Count FL up |
| 011 | Count FA up, FL down |
| 100 | Count FA down, FL up |
| 101 | Count FA down |
| 110 | Count FL down |
| 111 | Count FA down, FL down |

FA: No overflow/underflow detection; wraps around both ways.
FL: No overflow detection; wraps around on overflow. Underflow **is** detected
and will not wrap around (FL stays at zero).

**Timing:** 4 clocks.

---

### 4.17 — 7D: Exchange Doublepad Word

**Encoding:**
```
MC [15:12] = 0000
MD [11:8]  = 0111 (identifies 7D)
ME [7:4]   = Source 48-bit scratchpad word address (0–15)
MF [3:0]   = Destination 48-bit scratchpad word address (0–15)
```

**Operation:**
1. Save FA and FB into a holding register (MIR)
2. Load FA from the **left** half and FB from the **right** half of the
   source scratchpad word
3. Store the saved values into the left and right halves of the destination
   scratchpad word

This performs a **swap** of FA:FB with a scratchpad double-word, useful for
fast context switching of field descriptors.

**Timing:** 4 clocks (estimated).

---

### 4.18 — 8D: Scratchpad Relate

**Encoding:**
```
MC [15:12] = 0000
MD [11:8]  = 1000 (identifies 8D)
ME [7:4]   = Left Scratchpad word address
MF [3]     = Sign: 0=positive, 1=negative
MF [2:0]   = Reserved (must be zeros)
```

**Operation:** Replace FA with FA + (signed scratchpad left-word value).
This adds the contents of a scratchpad register to FA, enabling base-relative
field addressing.

**Timing:** 4 clocks.

---

### 4.19 — 9D: Monitor

**Encoding:**
```
MC [15:12] = 0000
MD [11:8]  = 1001 (identifies 9D)
ME [7:0]   = Literal occurrence identifier
```

**Operation:** **No programmatic function** — treated as a NOP by the hardware.
Used as a software breakpoint or monitor trap point. The literal identifier
can be used by diagnostic/debug tools to identify which monitor point was hit.

**Timing:** 2 clocks.

---

### 4.20 — 2E: Cassette Control

**Encoding:**
```
MC [15:8]  = 00000010 (identifies 2E)
ME [7:5]   = Must be zeros
ME [4:2]   = Variants (V)
MF [1:0]   = Must be zeros
```

**Variants:**
| V | Operation |
|---|-----------|
| 0 | Start Tape |
| 1 | Stop Tape (processor also halts in Tape Mode) |
| 2 | Stop Tape if X ≠ Y |
| 3–7 | Undefined |

All stop variants cause tape to halt at the next available inter-record gap.

**Timing:** 2 clocks.

---

### 4.21 — 3E: Bias

**Encoding:**
```
MC [15:8]  = 00000011 (identifies 3E)
ME [7:5]   = Must be zeros
ME [4:3]   = Test CPL flag: 0=no test, 1=test CPL result
ME [2:0]   = Variants (V)
```

**Operation:** The Bias micro sets CPU and CPL based on FU and FL values:

1. **Set CPU**: CPU ← 1 if FU = 4, else CPU ← 0
   - Exception: For V=2, SFU (from scratchpad) is used instead of FU

2. **Set CPL** to the value (or smallest of values) from this table:

| V | CPL set to |
|---|------------|
| 0 | FU |
| 1 | min(24, FL) |
| 2 | min(24, SFL) |
| 3 | min(24, FL, SFL) |
| 4 | CPL (unchanged) |
| 5 | min(24, CPL, FL) |
| 6 | CPL (unchanged) |
| 7 | Not defined |

3. **Conditional skip**: If the test flag = 1 and the final CPL value is not
   zero, the next 16-bit micro-instruction is **skipped**.

**Timing:** 2 clocks.

**Importance:** This micro is critical for setting up the ALU data width before
arithmetic operations. It translates the abstract field descriptor (FU/FL) into
the concrete parallelism parameters (CPU/CPL) that the 24-Bit Function Box uses.

---

### 4.22 — 6E: Carry Flip-Flop Manipulate

**Encoding:**
```
MC [15:8]  = 00000110 (identifies 6E)
ME [7:4]   = Must be zeros
MF [3:0]   = Variants (V, 4-bit value)
```

**Variants:**

| V bit | Operation |
|-------|-----------|
| 1 | Set CYF to 0 |
| 2 | Set CYF to 1 |
| 4 | Set CYF to CYL (carry from function box add/sub, lower) |
| 8 | Set CYF to CYD: `CYD = (X ≠ Y) + (X = Y) × CYF` |

Multiple bits can be set (operations applied in order).

**CYD explanation:**
- If X ≠ Y: CYD = 1
- If X = Y: CYD = current CYF value

This is used for multi-precision arithmetic carry propagation.

**Timing:** 2 clocks.

---

### 4.23 — 1F: Halt

**Encoding:**
```
All 16 bits: 0000 0000 0000 0010  (= 0x0002)
```

All bits except bit 1 are zero.

**Operation:** Stop execution. The machine halts with the next micro-instruction
in the M register.

**Timing:** 2 clocks.

---

### 4.24 — 3F: Normalize X

**Encoding:**
```
All bits: 0000 0000 0000 0011  (= 0x0003)
```

**Operation:** Shift X left while counting FL down until:
- FL = 0, **or**
- The bit in X referenced by CPL is a 1

Zeros are shifted into the rightmost end of X.

CPL = 1 references X bit 0 (rightmost); CPL = 24 references X bit 23 (leftmost).

**Timing:** 6 clocks per bit shifted, plus:
- +2 clocks if FL reaches 0
- +4 clocks if the MSB of X (as referenced by CPL) is already 1

---

### 4.25 — Zero: No Operation (NOP)

**Encoding:**
```
All 16 bits: 0000 0000 0000 0000  (= 0x0000)
```

**Operation:** Skip to the next sequential instruction. No other effect.

**Timing:** 2 clocks.

---

## 5. Instruction Encoding Summary Table

| Opcode | MC[15:12] | Name | Clocks | Brief Description |
|--------|-----------|------|--------|-------------------|
| 1C | varies* | Register Move | 2(+2) | Move between registers |
| 2C | 0000** | Scratchpad Move | 3 | Move register ↔ scratchpad |
| 3C | varies* | 4-Bit Manipulate | 2 | SET/AND/OR/XOR/INC/DEC on 4-bit regs |
| 4C | 01x0 | Bit Test Branch False | 4 | Test bit, branch if false |
| 5C | 01x1 | Bit Test Branch True | 4 | Test bit, branch if true |
| 6C | varies* | Skip When | 2–4 | Conditional skip |
| 7C | 0111 | Read/Write Memory | 8 | Memory access with FIU |
| 8C | varies* | Move 8-Bit Literal | 2(+2) | Load 8-bit immediate |
| 9C | varies* | Move 24-Bit Literal | 6 | Load 24-bit immediate (2-word) |
| 10C | varies* | Shift/Rotate T Left | 3(+2) | Shift or rotate T, store to dest |
| 11C | 00** | Extract from T | 3 | Rotate + extract from T |
| 12C | 110x | Branch Relative | 4 | PC-relative branch |
| 13C | 1101 | Branch Relative (neg) | 4 | PC-relative branch (negative) |
| 14C | 1110 | Call | 5 | Push return addr, branch |
| 15C | 1111 | Call (negative disp) | 5 | Push return addr, branch (negative) |
| 4D | 0000 | Shift/Rotate X or Y | 3 | Shift/rotate X or Y any direction |
| 5D | 0000 | Shift X:Y Concatenated | 6 | Shift 48-bit X:Y by 1 bit |
| 6D | 0000 | Count FA/FL | 4 | Increment/decrement FA and/or FL |
| 7D | 0000 | Exchange Doublepad | 4 | Swap FA:FB with scratchpad |
| 8D | 0000 | Scratchpad Relate | 4 | FA += scratchpad value |
| 9D | 0000 | Monitor (NOP) | 2 | Debug/monitor trap point |
| 2E | 0000 | Cassette Control | 2 | Start/stop tape |
| 3E | 0000 | Bias | 2 | Set CPU/CPL from FU/FL |
| 6E | 0000 | Carry FF Manipulate | 2 | Set/clear/compute carry |
| 1F | 0000 | Halt | 2 | Stop processor |
| 3F | 0000 | Normalize X | 6n+2/4 | Left-normalize X, count FL |
| 0 | 0000 | NOP | 2 | No operation |

*"varies" = the MC field encodes the register group, which makes it look like
different opcodes for different register operands.*

**The D/E/F class micro-operators all have MC=0000 and are further decoded by
MD, ME, and MF fields.**

---

## 6. Instruction Decode Algorithm

For an emulator, the decode logic follows this tree:

```
Read 16-bit micro from memory at MAR(A)

If MC[15:14] == 11:
    If MC[13] == 0: → 12C/13C BRANCH RELATIVE (bit 12 = sign)
    If MC[13] == 1: → 14C/15C CALL (bit 12 = sign)

If MC[15:14] == 01:
    → 4C or 5C BIT TEST BRANCH (further decode MC[12])

If MC[15:12] == 0111:
    → 7C READ/WRITE MEMORY

If MC[15:12] == 0000 (all zero in MC):
    Decode by MD[11:8]:
        0000: → Further decode by ME/MF (NOP, HALT, NORMALIZE, etc.)
        0001: → (variant)
        0010: → 2C SCRATCHPAD MOVE (or 2E CASSETTE CONTROL)
        0011: → 3E BIAS
        0100: → 4D SHIFT/ROTATE X OR Y
        0101: → 5D SHIFT XY CONCATENATED
        0110: → 6D COUNT FA/FL (or 6E CARRY FF)
        0111: → 7D EXCHANGE DOUBLEPAD
        1000: → 8D SCRATCHPAD RELATE
        1001: → 9D MONITOR
        Other: reserved

Otherwise:
    MC encodes source register group for 1C/3C/6C/8C/9C/10C/11C
    Further decode by ME/MF to determine specific instruction
```

**Important:** The exact decode is complex because the register group in MC
overlaps with the opcode identification. In practice, the hardware uses a
multi-level decode tree that the emulator should replicate.

---

## 7. C++ Implementation Notes

```cpp
// Microinstruction field extraction helpers
struct MicroFields {
    uint16_t raw;

    uint8_t MC()  const { return (raw >> 12) & 0xF; }
    uint8_t MD()  const { return (raw >> 8)  & 0xF; }
    uint8_t ME()  const { return (raw >> 4)  & 0xF; }
    uint8_t MF()  const { return  raw        & 0xF; }

    // Common field extractions
    uint8_t  src_group()   const { return MC(); }
    uint8_t  src_select()  const { return (MD() >> 2) & 0x3; }
    uint8_t  dst_group()   const { return ME(); }
    uint8_t  dst_select()  const { return (MF() >> 2) & 0x3; }
    uint8_t  variant()     const { return MD() & 0x3; }

    // For branch/call
    bool     is_negative() const { return (raw >> 12) & 1; }
    uint16_t displacement() const { return raw & 0x0FFF; }

    // For 7C memory
    bool     mem_direction()     const { return (raw >> 11) & 1; }
    uint8_t  mem_register()      const { return (raw >> 9) & 0x3; }
    bool     mem_field_sign()    const { return (raw >> 8) & 1; }
    uint8_t  mem_field_length()  const { return raw & 0xFF; } // approximate
    uint8_t  mem_count_variant() const { return raw & 0x7; }
};
```

The core execute loop:

```cpp
void Processor::execute_one_micro() {
    uint16_t micro = memory.fetch_micro(regs.MAR);
    regs.M = micro;

    MicroFields f{micro};

    // Advance PC (auto-increment by 16 bits = 1 word)
    regs.MAR = (regs.MAR + 16) & MASK_19;

    // Decode and execute
    decode_and_execute(f);

    // Update cycle counter
    cycles += instruction_clocks;
}
```

---

*Previous: [03 — Memory System](03_MEMORY.md) | Next: [05 — ALU and Data Paths](05_ALU_DATAPATH.md)*
