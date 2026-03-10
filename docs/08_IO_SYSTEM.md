# 08 — I/O System

## 1. Overview

The B1700 I/O system is a **bus-based architecture** where the processor
communicates with peripheral devices through dedicated **I/O Controls**
connected to the **I/O Bus**.

| Property | Value |
|----------|-------|
| Bus type | Parallel I/O bus |
| Max I/O controls | 10 (basic) / 14 (with expansion) |
| Data width | 24 bits |
| Protocol | Command/Response with processor-generated RC signal |
| DMA | Via I/O controls (not processor-managed) |

**Important design note**: The B1700's I/O system is described in a separate
manual — the **B1700 System I/O Sub-System Technical Manual** — not in the
S-Memory Processor manual. The FE Technical Manual for the processor covers
only the processor-side I/O interface.

---

## 2. I/O Bus Protocol

### 2.1 Bus Signals

The I/O bus connects the processor to up to 14 I/O controls. Key signals:

| Signal | Direction | Description |
|--------|-----------|-------------|
| **CA** (Command Active) | Processor → I/O | Indicates a command word is on the bus |
| **RC** (Response Complete) | Processor → I/O | Acknowledges data transfer |
| **SR** (Service Request) | I/O → Processor | I/O control requests attention |
| **Data Bus** | Bidirectional | 24-bit data/command words |

### 2.2 Commands

A command is issued by writing to the **CMND** pseudo-register:

```
1C: MOVE <source_register> → CMND
```

This places the 24-bit command word on the I/O bus and asserts Command Active (CA).
The addressed I/O control receives the command and takes appropriate action.

Command word format is defined by each I/O control type and typically includes:
- Control address/select bits
- Operation code (read, write, seek, status, etc.)
- Parameters

### 2.3 Data Transfer

Data is transferred using the **DATA** pseudo-register:

**Read from I/O** (device → processor):
```
1C: MOVE DATA → <destination_register>
```
The processor asserts RC (Response Complete) and accepts 24 bits from the bus.

**Write to I/O** (processor → device):
```
1C: MOVE <source_register> → DATA
```
The processor places 24 bits on the bus and asserts RC.

### 2.4 Interrupt-Driven I/O

I/O controls signal completion or attention via:
1. **Service Request (SR)**: The I/O control asserts SR on the bus
2. **CC(1)** (Bus Interrupt bit): Hardware sets this bit when any SR is active
3. **XYST(2)** (INT OR): The combined interrupt OR includes CC(1)
4. **Software polling**: The interpreter periodically checks XYST(2), then
   examines CC and CD to determine the specific interrupt source

There is no hardware interrupt vectoring — all I/O interrupt handling is
under software/microcode control.

---

## 3. I/O Controls

Each peripheral device type requires a specific I/O control unit. The I/O
control handles device-specific protocols, buffering, and timing.

### 3.1 Supported Devices

| Device | I/O Control | Type |
|--------|-------------|------|
| Disk Subsystem | B9480 Disk Cartridge Control | Storage |
| Disk Subsystem | B9370 Disk Pack Control | Storage |
| Magnetic Tape | B9485 Tape Cassette Control | Storage |
| Line Printer | B9246/B9249 Printer Control | Output |
| Card Reader | B9112/B9122 Card Reader Control | Input |
| Card Punch | B9122 Card Read Punch Control | I/O |
| CRT Display | B1700 Display Control | I/O |
| Communications | B1700 Data Comm Control | I/O (up to 32 lines on B1720) |
| MICR Reader-Sorter | MICR Control | Special |
| Audit Entry Station | AE501 Control | Special |

### 3.2 Disk Subsystem

Primary storage device for the B1700. Stores:
- MCP operating system
- S-language programs
- Interpreter microcode
- User data and files

**B9480 Disk Cartridge Subsystem:**
- 4.6 megabyte removable cartridges
- Up to 4 subsystems per system
- Used for program and data storage

**B9370 Disk Pack:**
- Larger capacity
- Available on B1720 configurations

### 3.3 Configuration Rules

| Model | Max I/O Controls | Max Disk | Max Printer | Max Communications |
|-------|-----------------|----------|-------------|-------------------|
| B1705 | 10 | varies | varies | 4 lines |
| B1707 | 10 | 4 | 2 | 4 |
| B1709 | 10 | varies | varies | 4 |
| B1713 | 10 | varies | varies | 4 |
| B1717 | 10+4 expansion | varies | varies | 4 |
| B1720-1 | 10+4 expansion | varies | varies | 32 |
| B1724 | 10+4 expansion | varies | varies | 32 |

---

## 4. I/O Bus Conditions (BICN Register)

The **BICN** pseudo-register (4 bits, Group 12/Select 0) provides I/O bus
status information:

| Bit | Condition |
|-----|-----------|
| 0 | Bus status bit 0 |
| 1 | Bus status bit 1 |
| 2 | Bus status bit 2 |
| 3 | Bus status bit 3 |

The specific meanings are defined by the I/O control protocol. Software
reads BICN to determine I/O device status.

---

## 5. I/O Clock

The I/O clock (I/OCL.K.) is derived from the system clock:
- Generated on Card K
- 30 ns delayed from CLK4/.KO (via 50 ns delay line, 30 ns tap)
- Distributed to the I/O Distribution Card via coaxial cable
- Synchronizes I/O data transfers

---

## 6. Port Interchange (B1720)

The B1720 includes an **8-position Port Interchange** that arbitrates memory
access between the processor and I/O controls. This allows DMA-like transfers
where I/O controls can access S-Memory independently of the processor.

---

## 7. C++ Implementation Notes

### 7.1 I/O Bus Abstraction

```cpp
// I/O Control interface - each device type implements this
class IOControl {
public:
    virtual ~IOControl() = default;

    // Receive a command from the processor (24-bit command word)
    virtual void receive_command(uint32_t command) = 0;

    // Provide data TO the processor (DATA register read)
    virtual uint32_t read_data() = 0;

    // Accept data FROM the processor (DATA register write)
    virtual void write_data(uint32_t data) = 0;

    // Check if this control has a pending service request
    virtual bool has_service_request() const = 0;

    // Get status for BICN
    virtual uint8_t get_bus_status() const = 0;

    // Clock cycle update (for timing simulation)
    virtual void tick(uint64_t cycle) = 0;
};

class IOBus {
    std::array<std::unique_ptr<IOControl>, 14> controls;
    uint8_t num_installed = 0;

public:
    void send_command(uint32_t command);
    uint32_t read_data();
    void write_data(uint32_t data);

    bool any_service_request() const;
    uint8_t get_bicn() const;

    void install_control(int slot, std::unique_ptr<IOControl> ctrl);
};
```

### 7.2 Minimal I/O for Emulation

For initial emulator development, implement:

1. **Console I/O**: Simulates the front panel and operator terminal
2. **Disk I/O**: Reads/writes disk images (required for MCP boot)
3. **Cassette I/O**: Reads cassette tape images (required for bootstrap)

Other devices (printer, card reader, etc.) can be stubbed initially.

### 7.3 Disk Image Format

The emulator will need to work with B1700 disk images. Original media:
- B9480 disk cartridge: 4.6 MB
- Format: sector/track/cylinder organized
- File system: managed by MCP

If original disk images are available from bitsavers or other archives,
the emulator should support loading them directly.

---

*Previous: [07 — MCP Operating System](07_MCP.md) | Next: [09 — Interrupt System](09_INTERRUPTS.md)*
