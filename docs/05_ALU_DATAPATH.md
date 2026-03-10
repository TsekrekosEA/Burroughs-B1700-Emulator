# 05 — ALU and Data Paths

## 1. Overview

The B1700 has two arithmetic/logic units and a major data-transfer bus:

| Unit | Width | Location | Purpose |
|------|-------|----------|---------|
| **24-Bit Function Box** | 24 bits | Card F | General arithmetic/logic on X and Y |
| **4-Bit Function Box** | 4 bits | Card G | Nibble operations on 4-bit sub-registers |
| **Main Exchange (MX)** | 24 bits | — | Central data bus connecting all registers |

Additionally, the **Field Isolation Unit** (rotator + mask generator) on Card H
handles all bit-field alignment for memory operations.

---

## 2. The 24-Bit Function Box

### 2.1 Inputs and Outputs

The 24-Bit Function Box is a **combinatorial circuit** — its outputs reflect
the current register values at all times without requiring a clock trigger.

**Inputs:**
- **X register** (24 bits) — left operand
- **Y register** (24 bits) — right operand
- **CPU** (2 bits) — data type (binary or 4-bit BCD)
- **CPL** (5 bits) — parallelism length (1–24 bits)
- **CYF** (1 bit) — incoming carry

**Outputs (always available as pseudo-register reads):**

| Output | Group/Select | Operation | Description |
|--------|-------------|-----------|-------------|
| SUM | 6/0 | X + Y | Binary or BCD addition |
| DIFF | 8/3 | X − Y | Binary or BCD subtraction |
| CMPX | 6/1 | ~X | Bitwise complement of X |
| CMPY | 6/2 | ~Y | Bitwise complement of Y |
| XANY | 6/3 | X AND Y | Bitwise AND |
| XORY | 7/3 | X XOR Y | Bitwise exclusive OR |
| XEQY | 7/0 | X = Y | Equality comparison |
| MSKX | 7/1 | Masked X | X masked by CPL-width mask |
| MSKY | 7/2 | Masked Y | Y masked by CPL-width mask |

### 2.2 Data Type Control (CPU)

The CPU register (2 bits in CP) selects the arithmetic mode:

| CPU | Mode | Description |
|-----|------|-------------|
| 00 | Binary | Standard binary arithmetic on CPL bits |
| 01 | 4-bit BCD | BCD arithmetic: treats operands as groups of 4-bit BCD digits |
| 10 | Reserved | — |
| 11 | Reserved | — |

In **binary mode**, the function box performs standard 2's complement
arithmetic on the low CPL bits of X and Y, with zero-fill above CPL.

In **BCD mode**, each 4-bit nibble is treated as a BCD digit (0–9).
Carries propagate between nibbles with BCD correction (+6 for digits > 9).
The number of BCD digits processed = CPL/4 (rounded).

### 2.3 Parallelism Length (CPL)

CPL (5 bits, values 1–24) controls the effective width of the function box:

- Only the rightmost CPL bits participate in arithmetic/logic operations
- Bits above CPL position are masked/zeroed in outputs
- CPL=0 is treated as CPL=24 in some contexts (or uses a default)

The **3E Bias** micro is the primary way to set CPL from the field descriptor
(FU/FL) before arithmetic operations.

### 2.4 Carry Behavior

The carry flip-flop (CYF) is **not automatically set** by arithmetic operations.
It must be explicitly captured using the **6E Carry FF Manipulate** micro:

```
6E with V=4: CYF ← carry output from last add/subtract (CYL)
6E with V=8: CYF ← CYD = (X≠Y) + (X=Y)·CYF  (for multi-precision)
```

This design allows parallel computation without unwanted carry side effects.

### 2.5 Condition Outputs

Beyond the pseudo-register outputs, the function box produces condition signals
that feed into XYCN and XYST:

- **X = Y** (equality)
- **X ≠ Y** (inequality)  
- **Carry out** (CYL) from addition
- **Borrow** from subtraction
- Various overflow indicators

The XYCN pseudo-register captures a snapshot of these conditions for software
testing via 4C/5C bit tests or 6C skip-when micros.

### 2.6 Masked Outputs (MSKX, MSKY)

MSKX and MSKY apply a mask derived from CPL to X and Y respectively:
- Bits 0 through CPL-1 pass through unchanged
- Bits CPL through 23 are forced to zero

This allows extracting the "active" portion of a register without modifying
the original.

---

## 3. The 4-Bit Function Box

### 3.1 Purpose

The 4-Bit Function Box operates on 4-bit sub-registers (nibbles) of the T, L,
C, and other 4-bit-addressable registers. It is used for:

- Setting, ANDing, ORing, XORing 4-bit values (3C micro)
- Incrementing/decrementing 4-bit counters
- Testing and branching on individual bits (4C/5C micros)
- Condition code manipulation

### 3.2 Inputs

The 4-Bit Function Box accepts any register addressable as a 4-bit entity:

| Register | 4-bit Fields |
|----------|-------------|
| T | TA, TB, TC, TD, TE, TF |
| L | LA, LB, LC, LD, LE, LF |
| C | CA, CB, CC, CD |
| FB | FU, FT, FLC, FLD, FLE, FLF |
| CP | CP (8 bits, accessed as 4-bit halves) |
| CPU | (2 bits, zero-padded) |
| FLCN | (4 bits, read-only) |
| XYCN | (4 bits, read-only) |
| XYST | (4 bits, read-only) |
| BICN | (4 bits, read-only) |

Second input can be a literal or another 4-bit register.

### 3.3 Operations

| Operation | Description |
|-----------|-------------|
| SET | Copy source to destination |
| AND | Dest ← Dest AND Source |
| OR | Dest ← Dest OR Source |
| EOR/XOR | Dest ← Dest XOR Source |
| INC | Dest ← Dest + 1 (mod 16) |
| DEC | Dest ← Dest − 1 (mod 16) |
| BINARY SUM MOD 16 | Dest ← (Dest + Source) mod 16 |
| BINARY DIFF MOD 16 | Dest ← (Dest − Source) mod 16 |

### 3.4 BCD Arithmetic

When CPU = 01, the 4-bit function box performs BCD-corrected addition and
subtraction on individual digits.

---

## 4. Main Exchange (MX)

The Main Exchange is the 24-bit central data bus that interconnects all
registers and functional units:

```
┌──────┐    ┌──────┐    ┌──────┐    ┌──────────────┐    ┌──────────┐
│  X   │    │  Y   │    │  T   │    │ 24-Bit FB    │    │ S-Memory │
│      │◄──►│      │◄──►│      │◄──►│ SUM/DIFF/etc │◄──►│  via FIU │
└──┬───┘    └──┬───┘    └──┬───┘    └──────┬───────┘    └──────┬───┘
   │           │           │               │                   │
   └───────────┴───────────┴───────┬───────┘                   │
                                   │                           │
                          ┌────────┴────────┐                  │
                          │  MAIN EXCHANGE  │◄─────────────────┘
                          │    (24 bits)    │
                          └────────┬────────┘
                                   │
   ┌───────────┬───────────┬───────┴───┬───────────┬───────────┐
   │           │           │           │           │           │
┌──┴──┐  ┌────┴──┐  ┌─────┴─┐  ┌──────┴─┐  ┌─────┴──┐  ┌────┴──┐
│ FA  │  │ FB/FL │  │ MAR(A)│  │ BR/LR  │  │ TAS/   │  │ I/O   │
│     │  │       │  │       │  │        │  │ A-Stack│  │ DATA/ │
│     │  │       │  │       │  │        │  │        │  │ CMND  │
└─────┘  └───────┘  └───────┘  └────────┘  └────────┘  └───────┘
```

**Data path width:** 24 bits for all register-to-register transfers.

**Width mismatch rules** (applied by hardware during transfer):
- Source wider than destination: **truncate from left** (MSB)
- Source narrower than destination: **right-justify, zero-fill left**

---

## 5. Field Isolation Unit (FIU) Detail

### 5.1 The 64-Bit Rotator

Located on Card H, the rotator is a 64-bit barrel shifter that aligns
arbitrary bit fields from memory to register boundaries and vice versa.

For a read operation:
1. 32 bits are fetched from memory (raw)
2. The rotator shifts the data by `bit_address mod 24` positions to align
   the field's LSB with bit 0
3. A mask of width `field_length` is applied (right-justified)
4. The result is placed on the Main Exchange

For a write operation:
1. Register data is rotated to the correct position
2. A mask isolates the bits to be written
3. The rotated data is merged with existing memory contents
4. The merged result is written back (with parity regeneration)

### 5.2 Mask Generation

The mask generator produces a right-justified mask of 1 to 24 bits:

```
field_length=1:  000000000000000000000001
field_length=8:  000000000000000011111111
field_length=24: 111111111111111111111111
```

### 5.3 Micro-Instruction Fetch Bypass

When fetching micro-instructions (M-fetch), the rotator is **bypassed**
because micros are always 16-bit aligned. The 16 bits from memory go directly
to the M register without rotation.

---

## 6. Data Transfer Timing

| Operation Type | Base Clocks | Extra |
|---------------|-------------|-------|
| Register → Register (1C) | 2 | +2 if dest = MAR(A) |
| Register ↔ Scratchpad (2C) | 3 | — |
| 4-Bit Manipulate (3C) | 2 | — |
| Memory Read/Write (7C) | 8 | — |
| 8-Bit Literal Load (8C) | 2 | +2 if dest = MAR(A) |
| 24-Bit Literal Load (9C) | 6 | (fetches 2nd word) |
| Shift/Rotate T (10C) | 3 | +2 if dest = MAR(A) |
| Branch (12C/13C) | 4 | — |
| Call (14C/15C) | 5 | — |

---

## 7. C++ Implementation Notes

### 7.1 Function Box (Combinatorial)

```cpp
struct FunctionBox24 {
    // Always-available outputs computed from current X, Y, CPU, CPL, CYF
    uint32_t compute_sum(uint32_t x, uint32_t y, uint8_t cpu, uint8_t cpl,
                          uint8_t cyf) const;
    uint32_t compute_diff(uint32_t x, uint32_t y, uint8_t cpu, uint8_t cpl) const;
    uint32_t compute_complement_x(uint32_t x) const;
    uint32_t compute_complement_y(uint32_t y) const;
    uint32_t compute_and(uint32_t x, uint32_t y) const;
    uint32_t compute_xor(uint32_t x, uint32_t y) const;
    bool     compute_equal(uint32_t x, uint32_t y, uint8_t cpl) const;
    uint32_t compute_masked(uint32_t val, uint8_t cpl) const;

    // Carry output from last add/subtract
    bool carry_out;  // CYL
    bool borrow_out;

    // BCD addition (CPU=01)
    uint32_t bcd_add(uint32_t x, uint32_t y, uint8_t num_digits) const;
    uint32_t bcd_sub(uint32_t x, uint32_t y, uint8_t num_digits) const;
};
```

### 7.2 Key Insight: Combinatorial vs. Clocked

The function box outputs are **always valid** — they change instantly when X, Y,
CPU, or CPL change. An emulator does NOT need to "execute" the function box.
Instead, when code reads SUM, DIFF, etc., the emulator should compute the result
on-the-fly from the current register state.

This can be implemented either:
1. **Lazy evaluation**: Compute on read (simplest, recommended)
2. **Eager evaluation**: Recompute all outputs on any X/Y/CP write (wasteful)

### 7.3 CPL Mask Helper

```cpp
inline uint32_t cpl_mask(uint8_t cpl) {
    if (cpl == 0 || cpl >= 24) return 0x00FFFFFF;
    return (1u << cpl) - 1;
}
```

---

*Previous: [04 — Microinstruction Set](04_MICROINSTRUCTIONS.md) | Next: [06 — Interpreter & S-Language](06_INTERPRETERS.md)*
