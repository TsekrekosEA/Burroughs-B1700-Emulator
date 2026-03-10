# 02 — Register Set

## 1. Register Addressing

Every register is addressed by two coordinates encoded in the microinstruction:

- **Register Group** (4 bits, 0–15): selects one of 16 groups
- **Register Select** (2 bits, 0–3): selects one of 4 registers within that group

This yields a 6-bit register address space of 64 possible registers, though many
slots are reserved or have special meanings.

---

## 2. Register Address Map (Table I-3)

This is the complete register decode table from the manual. Each row is a
Register Group; columns are Register Select values 0–3.

| Group | Select 0 | Select 1 | Select 2 | Select 3 |
|------:|----------|----------|----------|----------|
| 0 | TA | TB | TC | TD |
| 1 | TE | TF | — | — |
| 2 | FU | FT | FLC | FLD |
| 3 | FLE | FLF | — | — |
| 4 | X | Y | T | L |
| 5 | MAR(A) | M | BR | LR |
| 6 | SUM | CMPX | CMPY | XANY |
| 7 | XEQY | MSKX | MSKY | XORY |
| 8 | FA | FB | FL | DIFF |
| 9 | MAXS | MAXM | — | — |
| 10 | — | — | TAS | U |
| 11 | — | — | CP | — |
| 12 | BICN | FLCN | XYCN | XYST |
| 13 | CA | CB | CC | CD |
| 14 | CPU | — | READ | CMND |
| 15 | WRIT | NULL | DATA | — |

**Notes:**
- Entries marked "—" are reserved. Addressing them may yield undefined results
  or zeros.
- Groups 0–1 address the T register in 4-bit sub-fields (TA–TF).
- Groups 2–3 address the FB register in 4-bit sub-fields (FU, FT, FLC–FLF).
- Groups 6–7 address function-box outputs (read-only pseudo-registers).
- Groups 12–15 contain control/status registers and pseudo-registers.

---

## 3. Register Descriptions

### 3.1 M Register (Micro Register)

| Property | Value |
|----------|-------|
| Width | 16 bits |
| Group/Select | 5/1 |
| Readable | Yes |
| Writable | Yes (OR'd with incoming micro) |

Holds the currently executing microinstruction. Fetched from S-Memory (via ML
latches) in Run mode or from the U register in Tape mode.

**Write behavior**: When M is used as a destination, the incoming data is
**bit-OR'd** with the next incoming micro-operator. This allows modification
of the next microinstruction (instruction patching). It does *not* modify the
instruction as stored in memory.

**Sources:**
- S-Memory via Memory Latch Register (MLR) — normal run mode
- U Register — tape mode

### 3.2 MAR(A) Register (Memory Address / Address Register)

| Property | Value |
|----------|-------|
| Width | 19 bits (bit-address) / 15 bits (micro-address) |
| Group/Select | 5/0 (as MAR), also known as A |
| Readable | Yes (all 19 bits on Main Exchange) |
| Writable | Yes |

Dual-purpose register:

1. **As Program Counter (A Register)**: Addresses 16-bit micro-operators.
   Lower 4 bits are ignored (forced to 0), treating all micros as aligned to
   16-bit boundaries. Auto-incremented by +16 (bits) during run mode.

2. **As Memory Address Register (MAR)**: Provides the full 19-bit bit-address
   for 7C Read/Write Memory operations. FA is transferred to MAR(A) for
   memory accesses.

**Branch behavior**: Any move into MAR(A) constitutes a branch. When MAR(A) is
used for a memory access, the current program counter is saved in TEMPB
(Temporary Buffer holding register).

**Wrap-around**: MAR(A) can wrap around its address space; this is permitted.

**Special timing**: Moves to MAR(A) add 2 extra clocks to the base execution
time due to pipeline effects.

### 3.3 TAS Register (Top of A-Stack)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 10/2 (as TAS) |
| Readable | Yes (pops stack) |
| Writable | Yes (pushes stack) |

TAS is not a separate register but the current top element of the A-Stack.
It is accessed through a stack pointer:

- **Write (push)**: Stack pointer is incremented first, then data stored at
  new pointer location.
- **Read (pop)**: Data is read from current pointer location, then pointer
  is decremented.

### 3.4 A-Stack

| Property | Value |
|----------|-------|
| Depth | 16 words (B1710) / 32 words (B1720) |
| Word width | 24 bits |
| Structure | Last-In-First-Out (LIFO) push-down stack |

The stack pointer wraps around: 16 consecutive pushes (B1710) return the
pointer to its original position. Primary use is Call/Return subroutine
linkage — the CALL micro (14C/15C) pushes the return address; returning is
accomplished by moving TAS to MAR(A).

Although designed for address storage, the 24-bit width allows limited operand
storage. Scratchpad is preferred for data storage.

### 3.5 X Register

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 4/0 |
| Readable | Yes |
| Writable | Yes |

General-purpose operand register. Primary use is as the **left operand** (addend
/ minuend) input to the 24-Bit Function Box. Also used as:
- Source/destination for 7C memory operations
- Shift/rotate target for 4D, 5D, 3F micro-operators
- Left (most significant) half of concatenated 48-bit X:Y register for 5D shifts

### 3.6 Y Register

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 4/1 |
| Readable | Yes |
| Writable | Yes |

General-purpose operand register. Primary use is as the **right operand** (augend
/ subtrahend) input to the 24-Bit Function Box. Also used as:
- Source/destination for 7C memory operations
- Shift/rotate target for 4D, 5D micro-operators
- Right (least significant) half of concatenated 48-bit X:Y for 5D shifts

### 3.7 T Register (Transform Register)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 4/2 |
| Readable | Yes (24-bit) and as 4-bit groups (TA–TF) |
| Writable | Yes (24-bit) and as 4-bit groups (TA–TF) |

General-purpose register. Unique capabilities:
- Addressable in **six 4-bit sub-fields**: TA (bits 23–20), TB (19–16),
  TC (15–12), TD (11–8), TE (7–4), TF (3–0)
- Subject to 10C Shift/Rotate T Left micro
- Subject to 11C Extract from T micro
- Source/destination for 7C memory operations
- 4-bit sub-fields accessible to the 4-Bit Function Box

**4-bit sub-field addressing:**

| Sub-field | Bits | Group | Select |
|-----------|------|-------|--------|
| TA | 23–20 | 0 | 0 |
| TB | 19–16 | 0 | 1 |
| TC | 15–12 | 0 | 2 |
| TD | 11–8 | 0 | 3 |
| TE | 7–4 | 1 | 0 |
| TF | 3–0 | 1 | 1 |

### 3.8 L Register (Logical Register)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 4/3 |
| Readable | Yes (24-bit) and as 4-bit groups (LA–LF) |
| Writable | Yes (24-bit) and as 4-bit groups (LA–LF) |

General-purpose register. Same 4-bit sub-field structure as T:

| Sub-field | Bits | Group | Select |
|-----------|------|-------|--------|
| LA | 23–20 | 8 | 0 |
| LB | 19–16 | 8 | 1 |
| LC | 15–12 | 8 | 2 |
| LD | 11–8 | 8 | 3 |
| LE | 7–4 | 9 | 0 |
| LF | 3–0 | 9 | 1 |

*Note: The manual assigns LA–LF to groups 8–9 in select positions 0–3/0–1. This
overlaps with FA/FB/FL/DIFF in the 24-bit register map. The 4-bit sub-field
access uses the Group 0–1 style encoding when the 4-Bit Function Box is active.*

Source/destination for 7C memory operations. 4-bit sub-fields accessible to
4-Bit Function Box.

### 3.9 FA Register (Field Address Register)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 8/0 |
| Readable | Yes |
| Writable | Yes |

Holds an **absolute bit-address** for main memory. Can directly address any bit
in the installed memory.

- Transferred to MAR(A) for 7C Read/Write Memory operations
- Upper 5 bits truncated (only 19 bits used by MAR), but FA can address more
  than the physical memory space
- Counted up/down by the 7C micro's count variants or by 6D Count FA/FL
- Can be loaded/stored/swapped with Scratchpad via 7D Exchange Doublepad Word
- No overflow/underflow detection: wraps around in both directions

### 3.10 FB Register (Field Descriptor)

| Property | Value |
|----------|-------|
| Width | 24 bits total |
| Group/Select | 8/1 (as FB full 24-bit) |
| Sub-fields | FU (4 bits), FT (4 bits), FL (16 bits) |

This register describes the current data field being processed:

```
┌────┬────┬──────────────────────┐
│ FU │ FT │          FL          │
│4bit│4bit│       16 bits        │
└────┴────┴──────────────────────┘
 23  20 19 16 15                0
```

**FU** (Field Unit, bits 23–20): Defines the unit size of data (e.g., binary,
4-bit BCD). Used by the 3E Bias micro to set CPU and CPL.

**FT** (Field Type, bits 19–16): No specific hardware meaning; semantics defined
by software/interpreter.

**FL** (Field Length, bits 15–0): Total length of the data field in bits (1 to
65,535). Used by 7C memory operations when the literal field is zero. Can be
counted up/down by 6D Count FA/FL.

Sub-field addressing:

| Sub-field | Width | Group | Select |
|-----------|-------|-------|--------|
| FU | 4 bits | 2 | 0 |
| FT | 4 bits | 2 | 1 |
| FLC | 4 bits | 2 | 2 |
| FLD | 4 bits | 2 | 3 |
| FLE | 4 bits | 3 | 0 |
| FLF | 4 bits | 3 | 1 |
| FL | 16 bits | 8 | 2 |
| FB | 24 bits | 8 | 1 |

*FLC–FLF divide the 16-bit FL into four 4-bit nibbles.*

### 3.11 BR Register (Base Register)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 5/2 |
| Readable | Yes |
| Writable | Yes |

Used for **memory protection** and base-relative addressing. Software checks
that memory addresses fall within the range [BR, BR+LR). If an address is
outside this range, the software decides whether to allow the access.

### 3.12 LR Register (Limit Register)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 5/3 |
| Readable | Yes |
| Writable | Yes |

Used with BR for memory protection. Defines the upper limit of the permitted
address range.

### 3.13 MAXS Register (Maximum S-Memory)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 9/0 |
| Readable | Yes (source only) |
| Writable | No (hardwired constant) |

Wired to a constant value indicating the installed S-Memory size. Set by the
field engineer. Read-only.

### 3.14 MAXM Register (Maximum M-Memory)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 9/1 |
| Readable | Yes (source only) |
| Writable | No (hardwired constant) |

On B1710 (no M-Memory), reads as all zeros. On B1720, indicates the installed
M-Memory size.

### 3.15 C Register (Control Register)

| Property | Value |
|----------|-------|
| Width | 24 bits total (NOT directly addressable as 24-bit unit) |
| Sub-registers | CA, CB, CC, CD (4 bits each), CP (8 bits) |

The C register is divided into separately addressable portions:

```
┌────┬────┬────┬────┬─────┬───┬───┐
│ CA │ CB │ CC │ CD │ CPL │CPU│CYF│
│4bit│4bit│4bit│4bit│5bit │2b │1b │
└────┴────┴────┴────┴─────┴───┴───┘
 23   20  19  16 15  12 11   8  7  3 2  1  0
```

#### CA (bits 23–20) — Group 13, Select 0
General-purpose 4-bit storage. No specific hardware function.

#### CB (bits 19–16) — Group 13, Select 1
General-purpose 4-bit storage. No specific hardware function.

#### CC (bits 15–12) — Group 13, Select 2 — Interrupt Status

| Bit | Name | Description |
|-----|------|-------------|
| CC(0) | Console Interrupt | Set by console interrupt button press |
| CC(1) | Bus Interrupt | Set by I/O control service request on I/O bus |
| CC(2) | Timer Interrupt | Set every 100 ms by Real-Time Clock |
| CC(3) | Console State Lamp | Software-controlled; illuminates console state lamp |

#### CD (bits 11–8) — Group 13, Select 3 — Error/Status

| Bit | Name | Description |
|-----|------|-------------|
| CD(3) | Memory Parity Error | Set if parity error on processor-initiated memory read |
| CD(2–0) | Reserved | Not defined |

#### CP (bits 7–0) — Group 11, Select 2

CP is further divided:

**CPL** (Control Parallelism Length, 5 bits, bits 7–3):
Defines the data width for the 24-Bit Function Box (1 to 24 bits). Also used
as a default length when the literal in a 7C micro is zero, and for other
length-dependent operations.

**CPU** (Control Parallelism Unit, 2 bits, bits 2–1):
Defines the data unit type:

| CPU | Data Type |
|-----|-----------|
| 00 | Binary |
| 01 | 4-bit BCD |
| 10 | Reserved |
| 11 | Reserved |

**CYF** (Carry Flip-Flop, 1 bit, bit 0):
Stores overflow/carry from 24-Bit Function Box add/subtract operations. Must
be explicitly set by the 6E Carry FF Manipulate micro (not set automatically).

### 3.16 U Register

| Property | Value |
|----------|-------|
| Width | 16 bits |
| Group/Select | 10/3 |
| Readable | Yes (source only; only via 1C Register Move) |
| Writable | No |

Accumulates bit-serial input from the cassette tape loader. If data is not yet
available when read, the 1C micro stalls until the U register is full.

In Tape mode, U's contents are transferred directly to the M register for
execution. For 9C (24-bit literal) micros in tape mode, execution waits for
the additional 16 literal bits to accumulate.

### 3.17 FLCN Register (Field Length Conditions)

| Property | Value |
|----------|-------|
| Width | 4 bits |
| Group/Select | 12/1 |
| Readable | Yes (source only) |
| Writable | No (hardware-generated) |

A **pseudo-register** that provides static comparison results between FL (in FB)
and SFL (in first word of right Scratchpad):

| Bit | Condition |
|-----|-----------|
| FLCN(0) | FL = 0 |
| FLCN(1) | FL < SFL |
| FLCN(2) | FL > SFL |
| FLCN(3) | FL = SFL |

### 3.18 XYCN Register (XY Conditions)

| Property | Value |
|----------|-------|
| Width | 4 bits |
| Group/Select | 12/2 |
| Readable | Yes |
| Writable | No (hardware-generated) |

Condition register reflecting the 24-Bit Function Box comparison state of X
and Y registers.

### 3.19 XYST Register (XY State)

| Property | Value |
|----------|-------|
| Width | 4 bits |
| Group/Select | 12/3 |
| Readable | Yes |
| Writable | No (hardware-generated) |

Processor state and interrupt summary register:

| Bit | Name | Description |
|-----|------|-------------|
| XYST(2) | INT OR | Logical OR of all interrupt conditions. If false, no interrupt checking needed |

### 3.20 BICN Register (Bus I/O Conditions)

| Property | Value |
|----------|-------|
| Width | 4 bits |
| Group/Select | 12/0 |
| Readable | Yes |
| Writable | No |

Contains I/O bus status conditions.

### 3.21 CMND Register (Command Register)

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 14/3 |
| Readable | No (destination only) |
| Writable | Yes |

A pseudo-register. Moving data to CMND causes:
1. The data is placed on the I/O bus
2. The Command Active (CA) signal is generated
3. The I/O control on the bus receives the command

### 3.22 DATA Register

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 15/2 |
| Readable | Yes (source and destination) |
| Writable | Yes |

A pseudo-register for I/O data transfer:
- **As source**: Processor generates Response Complete (RC) signal and accepts
  24 bits from I/O bus
- **As destination**: Processor generates RC signal and places data on I/O bus

### 3.23 NULL Register

| Property | Value |
|----------|-------|
| Width | 24 bits |
| Group/Select | 15/1 |
| Readable | Yes (always reads as all zeros) |
| Writable | Yes (data discarded) |

Used for:
- Clearing registers: move NULL → register
- Console display: in halt mode, hardware generates move from register → NULL to
  display register contents on console lamps
- Console load: in halt mode, move NULL → register loads console switch values

### 3.24 READ / WRIT Pseudo-Registers

| Register | Group/Select | Direction |
|----------|-------------|-----------|
| READ | 14/2 | Source only |
| WRIT | 15/0 | Destination only |

These are pseudo-registers related to memory operations (used internally by the
7C micro).

### 3.25 Function Box Output Pseudo-Registers (Read-Only)

| Register | Group | Select | Description |
|----------|-------|--------|-------------|
| SUM | 6 | 0 | X + Y result from 24-bit Function Box |
| CMPX | 6 | 1 | Complement of X |
| CMPY | 6 | 2 | Complement of Y |
| XANY | 6 | 3 | X AND Y |
| XEQY | 7 | 0 | X equal Y (comparison) |
| MSKX | 7 | 1 | Masked X |
| MSKY | 7 | 2 | Masked Y |
| XORY | 7 | 3 | X XOR Y |
| DIFF | 8 | 3 | X − Y result from 24-bit Function Box |

These are **combinatorial outputs** — they reflect the current values of X and Y
through the 24-Bit Function Box at all times. Reading them does not trigger a
computation; they are always available.

### 3.26 Scratchpad Memory

| Property | Value |
|----------|-------|
| Size | 16 × 48 bits (or 32 × 24 bits) |
| Access | Via 2C Scratchpad Move, 7D Exchange Doublepad, 8D Scratchpad Relate |

General-purpose high-speed storage. Typical use: S-language stack pointers,
frequently-manipulated virtual processor registers.

The **first cell of right Scratchpad** contains two special values:
- **SFL**: Corresponds to FL; used for FLCN comparisons
- **SFU**: Corresponds to FU; used by 3E Bias micro when appropriate variant selected

### 3.27 CPU Register (Standalone Access)

| Property | Value |
|----------|-------|
| Width | 2 bits (in CP, at bits 2–1) |
| Group/Select | 14/0 |
| Readable | Yes |
| Writable | Yes |

Accessible separately under group 14, select 0 for 4-bit function box
operations (padded with zeros).

---

## 4. Register Summary Table

| Register | Width | Group/Sel | R/W | Description |
|----------|-------|-----------|-----|-------------|
| M | 16 | 5/1 | R/W* | Microinstruction register (*write = OR with next micro) |
| MAR(A) | 19 | 5/0 | R/W | Program counter / Memory address register |
| TAS | 24 | 10/2 | R/W | Top of A-Stack (push/pop) |
| A-Stack | 24×16 | — | — | Hardware stack (via TAS only) |
| X | 24 | 4/0 | R/W | General purpose / ALU left operand |
| Y | 24 | 4/1 | R/W | General purpose / ALU right operand |
| T | 24 | 4/2 | R/W | Transform register (shift/extract target) |
| L | 24 | 4/3 | R/W | Logical register |
| FA | 24 | 8/0 | R/W | Field bit-address |
| FB | 24 | 8/1 | R/W | Field descriptor (FU+FT+FL) |
| FL | 16 | 8/2 | R/W | Field length (sub-field of FB) |
| BR | 24 | 5/2 | R/W | Base register (memory protection) |
| LR | 24 | 5/3 | R/W | Limit register (memory protection) |
| MAXS | 24 | 9/0 | R | Maximum S-Memory (hardwired) |
| MAXM | 24 | 9/1 | R | Maximum M-Memory (hardwired; 0 if none) |
| CA | 4 | 13/0 | R/W | General purpose (in C register) |
| CB | 4 | 13/1 | R/W | General purpose (in C register) |
| CC | 4 | 13/2 | R/W | Interrupt conditions |
| CD | 4 | 13/3 | R/W | Error conditions |
| CP | 8 | 11/2 | R/W | CPL(5) + CPU(2) + CYF(1) |
| U | 16 | 10/3 | R | Cassette tape accumulator |
| FLCN | 4 | 12/1 | R | Field length conditions (pseudo) |
| XYCN | 4 | 12/2 | R | XY conditions (pseudo) |
| XYST | 4 | 12/3 | R | XY state / interrupt OR (pseudo) |
| BICN | 4 | 12/0 | R | Bus I/O conditions (pseudo) |
| CMND | 24 | 14/3 | W | Command to I/O bus (pseudo) |
| DATA | 24 | 15/2 | R/W | I/O data transfer (pseudo) |
| NULL | 24 | 15/1 | R/W | Zero source / console gateway (pseudo) |
| SUM | 24 | 6/0 | R | X+Y (function box output) |
| DIFF | 24 | 8/3 | R | X−Y (function box output) |
| CMPX | 24 | 6/1 | R | Complement X (function box output) |
| CMPY | 24 | 6/2 | R | Complement Y (function box output) |
| XANY | 24 | 6/3 | R | X AND Y (function box output) |
| XEQY | 24 | 7/0 | R | X = Y (function box output) |
| MSKX | 24 | 7/1 | R | Masked X (function box output) |
| MSKY | 24 | 7/2 | R | Masked Y (function box output) |
| XORY | 24 | 7/3 | R | X XOR Y (function box output) |
| Scratchpad | 48×16 | via micro | R/W | High-speed local storage |

---

## 5. C++ Implementation Notes

### 5.1 Suggested Data Types

```cpp
// Core register types
using reg24_t = uint32_t;  // 24-bit register (stored in low 24 bits)
using reg19_t = uint32_t;  // 19-bit MAR (stored in low 19 bits)
using reg16_t = uint16_t;  // 16-bit registers (M, FL)
using reg8_t  = uint8_t;   // 8-bit CP register
using reg4_t  = uint8_t;   // 4-bit registers (low nibble used)
using reg2_t  = uint8_t;   // 2-bit CPU
using reg1_t  = uint8_t;   // 1-bit CYF

// Constants
constexpr uint32_t MASK_24  = 0x00FFFFFF;
constexpr uint32_t MASK_19  = 0x0007FFFF;
constexpr uint16_t MASK_16  = 0xFFFF;
constexpr uint8_t  MASK_8   = 0xFF;
constexpr uint8_t  MASK_4   = 0x0F;
```

### 5.2 Register File Structure

```cpp
struct RegisterFile {
    // Microinstruction register
    reg16_t M;                  // Current microinstruction

    // Address registers
    reg19_t MAR;                // Memory Address / Program Counter
    reg24_t TEMPB;              // MAR holding register during memory ops

    // General-purpose 24-bit registers
    reg24_t X, Y, T, L;

    // Field registers
    reg24_t FA;                 // Field bit-address
    reg24_t FB;                 // Field descriptor: FU(4) | FT(4) | FL(16)

    // Memory protection
    reg24_t BR, LR;

    // Hardware constants (set at init)
    reg24_t MAXS, MAXM;

    // Control register (24 bits, accessed in sub-fields)
    reg4_t  CA, CB, CC, CD;
    reg8_t  CP;                 // CPL(5) | CPU(2) | CYF(1)

    // Stack
    reg24_t A_Stack[32];        // 16 for B1710, 32 for B1720
    uint8_t  stack_ptr;         // Stack pointer (wraps at depth)
    uint8_t  stack_depth;       // 16 or 32

    // Cassette
    reg16_t U;                  // Cassette tape accumulator

    // Scratchpad
    uint64_t scratchpad[16];    // 16 × 48-bit words (use uint64_t, mask to 48)

    // I/O pseudo-registers
    reg24_t DATA_reg;           // I/O data

    // --- Helper accessors ---

    // Get 4-bit sub-field of T register
    uint8_t get_T_nibble(int n) const {  // n=0 is TA (MSB), n=5 is TF (LSB)
        return (T >> (20 - 4*n)) & 0x0F;
    }

    // Set 4-bit sub-field of T register
    void set_T_nibble(int n, uint8_t val) {
        int shift = 20 - 4*n;
        T = (T & ~(0x0Fu << shift)) | ((val & 0x0F) << shift);
    }

    // FU, FT, FL accessors
    uint8_t  FU()  const { return (FB >> 20) & 0x0F; }
    uint8_t  FT()  const { return (FB >> 16) & 0x0F; }
    uint16_t FL()  const { return FB & 0xFFFF; }

    void set_FU(uint8_t v)  { FB = (FB & 0x00FFFFFF & ~(0x0Fu << 20)) | ((v & 0x0F) << 20); }
    void set_FT(uint8_t v)  { FB = (FB & ~(0x0Fu << 16)) | ((v & 0x0F) << 16); }
    void set_FL(uint16_t v) { FB = (FB & 0x00FF0000) | v; }

    // CPL, CPU, CYF accessors
    uint8_t CPL() const { return (CP >> 3) & 0x1F; }
    uint8_t CPU_val() const { return (CP >> 1) & 0x03; }
    uint8_t CYF() const { return CP & 0x01; }

    void set_CPL(uint8_t v) { CP = (CP & 0x07) | ((v & 0x1F) << 3); }
    void set_CPU(uint8_t v) { CP = (CP & ~0x06) | ((v & 0x03) << 1); }
    void set_CYF(uint8_t v) { CP = (CP & ~0x01) | (v & 0x01); }

    // Read register by group/select
    uint32_t read_register(uint8_t group, uint8_t select) const;

    // Write register by group/select
    void write_register(uint8_t group, uint8_t select, uint32_t value);
};
```

### 5.3 Register Read/Write Dispatch

The `read_register` and `write_register` methods should implement the full
decode table from Section 2. Key considerations:

- **Width mismatch on write**: If the source width > destination width, truncate
  from the left (MSB). If source < destination, right-justify with zero fill on
  the left.
- **M register writes**: OR incoming data with the next micro fetch.
- **MAR(A) writes**: Any write constitutes a branch.
- **TAS reads**: Pop the A-Stack.
- **TAS writes**: Push the A-Stack.
- **CMND writes**: Generate I/O command.
- **DATA reads**: Generate Response Complete and accept I/O data.
- **NULL reads**: Return 0. In halt mode, return console switches.
- **Function box outputs** (groups 6–7, DIFF): Read-only combinatorial results.
- **MAXS, MAXM, FLCN, XYCN, XYST, BICN, U**: Read-only.

---

*Previous: [01 — Architecture Overview](01_ARCHITECTURE_OVERVIEW.md) | Next: [03 — Memory System](03_MEMORY.md)*
