# 03 — Memory System

## 1. Overview

The B1700 memory system is one of its most distinctive features: all memory is
**bit-addressable** with no inherent word or byte boundaries visible to the
programmer. Data fields can be any length from 1 to 65,535 bits, and up to 24
bits can be transferred in parallel between memory and the processor.

The system has two types of memory:

| Memory Type | Technology | Capacity | Cycle Time | Availability |
|-------------|-----------|----------|------------|--------------|
| **S-Memory** (System/Main) | MOS semiconductor | 8K–262K bytes | 500 ns read (B1710) / 667 ns read (B1720) | All models |
| **M-Memory** (Control) | Bipolar semiconductor | 2K–8K bytes | 167 ns (or 225 ns) | B1720 only |

---

## 2. S-Memory (Main Memory)

### 2.1 Physical Organization

S-Memory is composed of pairs of storage boards plugged into the processor
backplane. Each board holds one or two 4K-byte modules using 1K-bit dynamic
RAM chips (18-pin DIP packages, 1024 bits each).

**Board layout:**
```
┌──────────────────────────────┐
│  MODULE 0        MODULE 1    │  Board 1
│  (Byte 0)       (Byte 1)    │
├──────────────────────────────┤
│  MODULE 2        MODULE 3    │  Board 2  
│  (Byte 2)       (Byte 3)    │
└──────────────────────────────┘
... up to 8 boards total
```

Each module is subdivided into 4 rows of 9 memory chips (including parity).
Each row = 1K bytes.

**Configuration rules:**
- Memory boards always come in **pairs** (never an odd number)
- Minimum: 2 boards = 8K bytes
- Maximum: 8 boards = 64K bytes (B1710) or 262K bytes (B1720)
- Increment: 8K bytes (B1710) or 16K/32K bytes (B1720)
- Half-populated boards are supported

### 2.2 Memory Capacity by Model

| Model | Min | Max | Increment |
|-------|-----|-----|-----------|
| B1705 | 24K | 64K | 8K |
| B1707 | 24K | 64K | 8K |
| B1709 | 48K | 64K | 8K |
| B1713 | 48K | 64K | 8K |
| B1717 | 32K | 128K | 16K or 32K |
| B1720-1 | 48K | 262K | 16K or 32K |
| B1724 | 96K | 262K | 16K or 32K |

### 2.3 Timing

| Operation | B1710 | B1720 |
|-----------|-------|-------|
| Read cycle | 1,000 ns | 667 ns |
| Write cycle | 1,500 ns | 1,000 ns |
| Bits per transfer | 1–24 | 1–24 |
| Physical bus width | 32 bits (24 data + 8 parity/overhead) | 32 bits |

Data is available before the completion of the read cycle.

### 2.4 Parity

Parity generation and checking is provided on a **byte basis**:
- Each 8-bit byte has a single parity bit
- Parity is **odd** (total bits including parity bit sum to odd)
- Parity is generated during write and checked during read
- A parity error on read sets **CD(3)** in the C register

### 2.5 Write Behavior

Even though a write of fewer than 24 bits may be specified, the actual write
cycle writes **32 bits** (to maintain correct parity across byte boundaries
caused by bit-addressability). This requires a **read-before-write** operation
to merge the new data with existing surrounding bits and regenerate parity.

Writing may be in **forward or reverse direction** from the specified address.
In either case, the least significant bit of the source register is always
stored at the most significant address position of the memory field.

### 2.6 Read Behavior

A read operation obtains 32 bits from memory. Only 24 of the 32 bits contain
valid data (or as few as 1 bit, since field length is variable 1–24 bits).
The Field Isolation Unit (rotator + mask) extracts the correct bits.

---

## 3. Bit Addressing

### 3.1 Address Model

Every bit in memory has a unique integer address starting from 0. A byte at
address N occupies bits N×8 through N×8+7. However, because the machine is
bit-addressable, data fields need not align to any boundary.

```
Memory layout (first few bytes, showing bit addresses):

Byte 0:  bits  7  6  5  4  3  2  1  0
Byte 1:  bits 15 14 13 12 11 10  9  8
Byte 2:  bits 23 22 21 20 19 18 17 16
Byte 3:  bits 31 30 29 28 27 26 25 24
...
```

### 3.2 Address Registers

- **FA** (Field Address, 24 bits): Holds the absolute bit-address of the data
  field being accessed. Only the lower 19 bits are significant for physical
  memory (since max memory = 262K bytes = ~2M bits ≈ 19 address bits).
  
- **MAR(A)** (19 bits): Receives the address from FA for memory operations.
  For micro-instruction fetch, the lower 4 bits are forced to zero (16-bit
  alignment).

### 3.3 Field Parameters

A memory access is fully described by:

| Parameter | Source | Description |
|-----------|--------|-------------|
| **Bit address** | FA → MAR(A) | Starting bit position in memory |
| **Field length** | 7C micro literal, or CPL, or FL | Number of bits (1–24 for single access) |
| **Field direction** | 7C micro direction bit | 0 = positive (forward), 1 = negative (reverse) |
| **Register** | 7C micro register select | X, Y, T, or L — source or destination |
| **Direction of transfer** | 7C micro direction bit | 0 = to register (read), 1 = to memory (write) |

### 3.4 Field Isolation Unit (FIU) — Rotator and Mask Generator

The FIU is the hardware mechanism that translates bit-addresses into physical
memory access patterns. It is located on Processor Card H.

**Components:**

1. **64-bit Rotator**: A barrel shifter/rotator that can rotate data by 0–23
   positions. It aligns arbitrarily-addressed bit fields to register
   boundaries.

2. **Mask Generator**: Generates a bit mask of the correct width (1–24 bits)
   to isolate the desired field from the 32 bits fetched from memory.

**Operation on Read:**
```
Memory (32 bits raw) → Rotator (align to bit 0) → Mask (isolate field) → Register
```

If the field is shorter than the register width (24 bits), the data is
**right-justified** with zero fill on the left (most significant bits).

**Operation on Write:**
```
Register → Rotator (position to correct bits) → Merge with existing memory → Write back
```

**On micro-instruction fetch (M-fetch):**
The rotator is **bypassed** — micro-instructions are always 16-bit aligned,
so no rotation is needed.

### 3.5 Byte Address to Bit Address Conversion

For an emulator, the relationship is:

```
bit_address = byte_address × 8
byte_offset = bit_address / 8
bit_offset_within_byte = bit_address % 8
```

### 3.6 Memory Access Counts (7C Micro Variants)

The 7C Read/Write Memory micro can optionally count FA and/or FL after the
memory operation:

| V (3 bits) | Action |
|------------|--------|
| 000 | No count |
| 001 | Count FA up |
| 010 | Count FL up |
| 011 | Count FA up, FL down |
| 100 | Count FA down, FL up |
| 101 | Count FA down |
| 110 | Count FL down |
| 111 | Count FA down, FL down |

The count amount is the field length transferred. This auto-increment/decrement
enables efficient sequential field processing without separate count instructions.

---

## 4. M-Memory (Control Memory) — B1720 Only

### 4.1 Purpose

M-Memory is high-speed bipolar storage dedicated to **microprogram (interpreter)
storage**. It dramatically improves micro-instruction fetch time:

- S-Memory micro fetch: ~1,000 ns (B1710 read cycle)
- M-Memory micro fetch: 167 ns (B1720 control memory cycle)

This gives the B1720 approximately **6× faster** microcode execution.

### 4.2 Configuration

| Property | Value |
|----------|-------|
| Technology | Bipolar semiconductor |
| Capacity | 2,048–8,192 bytes |
| Increment | 2,048 bytes |
| Cycle time | 167 ns (or 225 ns depending on configuration) |
| Bits per fetch | 1–16 |

### 4.3 Operation

- Only the **active interpreter** microcode resides in M-Memory
- Less frequently used microcode remains in S-Memory
- The MCP manages loading interpreters into M-Memory using one of five
  **M-Memory management strategies** (see [07 — MCP](07_MCP.md))
- When M-Memory is full, portions may be swapped to/from S-Memory

### 4.4 Port Interchange (B1720)

The B1720 includes an **8-position Port Interchange** that controls all accesses
to main memory, providing arbitration between the processor and I/O controls.

---

## 5. Memory Protection

### 5.1 Base and Limit Registers

Memory protection is implemented through the **BR** (Base Register) and **LR**
(Limit Register):

```
Valid address range: BR ≤ address < BR + LR
```

All S-memory write operations are permitted only within the limits defined by
BR and LR. If an address falls outside this range, it is a **software** function
(not hardware) to determine whether the access should be allowed.

This is a key emulator consideration: the hardware does *not* trap on out-of-range
accesses. The interpreter/MCP must explicitly check boundaries.

### 5.2 MAXS Register

The MAXS register (hardwired constant) indicates the total installed S-Memory
size. Software can check addresses against MAXS to prevent physical memory
overrun.

---

## 6. Virtual Memory

The B1700 MCP implements virtual memory through software management of
S-Memory. Programs and data are stored on disk and paged into main memory as
needed. The interpreters and S-code are **location-independent** — they can be
loaded at any base address and operate correctly through base-relative
addressing.

Virtual memory is entirely a **software mechanism** managed by the MCP;
the hardware provides only BR/LR and bit-addressing as building blocks.

---

## 7. C++ Implementation Notes

### 7.1 Memory Model

```cpp
class BitAddressableMemory {
    std::vector<uint8_t> data;       // Byte storage
    std::vector<uint8_t> parity;     // One parity bit per byte (stored as byte)
    size_t size_bytes;

public:
    explicit BitAddressableMemory(size_t bytes)
        : data(bytes, 0), parity(bytes, 0), size_bytes(bytes) {}

    // Read a field of 1-24 bits starting at bit_address
    // Returns right-justified value with MSB zero-fill
    uint32_t read_field(uint32_t bit_address, uint8_t field_length,
                        bool reverse_direction = false) const;

    // Write a field of 1-24 bits starting at bit_address
    // Source value is right-justified; MSBs truncated if field < 24
    // Performs read-modify-write to maintain parity
    void write_field(uint32_t bit_address, uint8_t field_length,
                     uint32_t value, bool reverse_direction = false);

    // Fetch a 16-bit micro-instruction (always 16-bit aligned)
    // Bypasses rotator - direct 16-bit fetch
    uint16_t fetch_micro(uint32_t word_address) const;

    // Check parity on read
    bool check_parity(uint32_t byte_address) const;

    // Get raw byte for debugging
    uint8_t get_byte(size_t addr) const { return data[addr]; }
    void set_byte(size_t addr, uint8_t val);  // Regenerates parity
};
```

### 7.2 Key Implementation Details

1. **Bit extraction**: Reading N bits at arbitrary bit offset requires reading
   up to 4 bytes and shifting/masking. Use a 32-bit or 64-bit intermediate.

2. **Parity**: Generate odd parity per byte on every write. Check on every read.
   Set CD(3) on parity failure.

3. **Micro-instruction fetch**: Optimize this path — it's the hottest code path.
   Micros are 16-bit aligned, so byte-aligned 2-byte reads suffice.

4. **Read-before-write**: On sub-byte writes, must read the surrounding byte(s),
   merge the new bits, and write back with correct parity.

5. **Memory size**: Use MAXS to enforce bounds. For B1710 emulation, 64K bytes
   (512K bits) is the maximum. Store as a flat byte array.

6. **Reverse direction**: When field direction = negative, bits are stored in
   decreasing address order from the starting position.

### 7.3 Field Isolation Unit (FIU)

```cpp
struct FieldIsolationUnit {
    // Rotate value right-justified into position at bit_offset
    static uint32_t rotate_to_position(uint32_t value, uint8_t bit_offset,
                                        uint8_t field_length);

    // Generate mask of field_length bits (right-justified)
    static uint32_t generate_mask(uint8_t field_length);

    // Extract field from raw memory word
    static uint32_t extract_field(uint32_t raw_word, uint8_t bit_offset,
                                   uint8_t field_length);

    // Merge field into existing memory word
    static uint32_t merge_field(uint32_t existing, uint32_t new_value,
                                 uint8_t bit_offset, uint8_t field_length);
};
```

---

*Previous: [02 — Register Set](02_REGISTERS.md) | Next: [04 — Microinstruction Set](04_MICROINSTRUCTIONS.md)*
