# 10 — Console & Bootstrap

## 1. Console

### 1.1 Physical Layout

The B1700 console is the front-panel operator interface. It contains:

| Component | Description |
|-----------|-------------|
| **24 Console Lamps** | Display contents of the 24-bit Main Exchange |
| **24 Console Switches** | Enter data into registers or memory |
| **Register Group Switch** | Large rotary switch; selects one of 16 register groups |
| **Register Select Switch** | Selects one of 4 registers within the group |
| **Mode Switch** | 3-position: Step / Run / Tape |
| **Start Button** | Begin execution in current mode |
| **Halt Button** | Stop execution |
| **Load Button** | Load console switch data into selected register |
| **Read/Write Button** | Trigger memory read or write from console |
| **Interrupt Button** | Generate console interrupt (CC(0)) |
| **Power On/Off Switch** | System power |
| **Run Indicator** | Lit when processor is executing |
| **BOT Indicator** | Lit when cassette tape is at Beginning of Tape |
| **Tape Parity Indicator** | Lit on cassette tape parity error |
| **Cassette On/Off Switch** | Power for cassette loader |
| **Rewind Button** | Rewind cassette tape to BOT |

### 1.2 Console Lamp Display

In **Halt Mode**:
- Lamps display the contents of the register selected by the Group and Select
  switches
- Hardware automatically generates a `1C: MOVE <selected_reg> → NULL` to place
  the register contents on the Main Exchange, which drives the lamps

In **Run Mode / Tape Mode**:
- Lamps show whatever data is currently on the Main Exchange
- Display changes rapidly with each micro-instruction and is not human-readable

### 1.3 Console Data Entry

In **Halt Mode**, the operator can:

1. **Load a register**: Set the 24 switches to the desired value, select the
   target register with Group/Select switches, press LOAD. Hardware generates
   `1C: MOVE NULL → <selected_reg>` which transfers the switch values.

2. **Write to memory**: Set switches to data value, set register select to
   point to the memory address register, press Read/Write (with write mode
   selected). Hardware generates a `7C WRITE` from NULL (console switches) to
   memory.

3. **Display a register**: Select the register and observe the lamps. No
   button press needed — the display is automatic.

### 1.4 Console Hardware Logic

The console operations are implemented as forced micro-instructions:

| Action | Generated Micro |
|--------|----------------|
| Display register (automatic in halt) | `1C: <selected_reg> → NULL` |
| Load register (LOAD button) | `1C: NULL → <selected_reg>` |
| Write memory (Read/Write button) | `7C: NULL → MEMORY` |

The LOAD..CO pulse has a width of 500 ns (two 4 MHz clocks).

### 1.5 Processor Clear

Power-on or manual reset triggers **General Processor Clear (GPCLR)**:
- Clears all flip-flops and registers
- Resets the sequencer
- Returns processor to Halt mode at address 0
- Signal is generated on Card C and distributed to all processor cards

---

## 2. Cassette Tape Loader

### 2.1 Purpose

The cassette tape loader is the **bootstrap device** for the B1700. It loads
initial microcode into memory when the system is first powered on or needs to
be restarted from scratch.

### 2.2 Physical Description

- Physically mounted on the console
- Standard audio-style cassette mechanism
- Bit-serial data transfer at cassette speed
- Data accumulated bit-by-bit in the **U register** (16 bits)

### 2.3 Tape Mode Operation

1. Operator inserts bootstrap cassette
2. Mode switch set to **Tape**
3. Press **Start** — tape starts moving, processor begins executing
4. Micro-instructions are read bit-serially into the U register
5. When U is full (16 bits), contents transfer to the M register for execution
6. Execution continues until:
   - Halt micro (1F) is encountered
   - Halt button is pressed
   - Tape error occurs
   - Cassette Stop micro (2E) is decoded
   - Tape stops at next inter-record gap

### 2.4 Cassette Data Format

Data on the cassette is encoded as a serial bit stream with:
- Inter-record gaps (IRG) between data blocks
- Parity over the serial data (detected by Card E logic)
- The tape parity indicator illuminates on error

### 2.5 Cassette Control Micro (2E)

The 2E micro provides software control of the cassette:

| Variant | Operation |
|---------|-----------|
| V=0 | Start Tape |
| V=1 | Stop Tape (processor also halts in Tape mode) |
| V=2 | Stop Tape if X ≠ Y |
| V=3–7 | Undefined |

All stop variants cause tape to halt at the next inter-record gap.

### 2.6 Bootstrap Sequence

The typical bootstrap process:

```
1. Power on → GPCLR → Processor halted at address 0
2. Insert bootstrap cassette
3. Set Mode to Tape
4. Press Start

5. Cassette reads initial bootstrap microcode → U → M → execute
6. Bootstrap micro-operators:
   a. Read more microcode from cassette into S-Memory
   b. Set up FA, FB for memory addressing
   c. Potentially switch from Tape mode to Run mode

7. Bootstrap loads a secondary loader from disk
8. Secondary loader loads the SDL interpreter
9. SDL interpreter loads the MCP
10. MCP initializes the system
11. System ready for operator commands
```

### 2.7 U Register in Tape Mode

In Tape mode:
- U accumulates bits serially from the cassette
- When full (16 bits), U is automatically transferred to M
- M executes the micro-instruction
- For 9C (24-bit literal) micros: execution waits for the additional
  16-bit literal word to accumulate in U

---

## 3. Modes of Operation (Detailed)

### 3.1 Halt Mode

- Processor stopped
- M register contains the next micro to execute
- Console lamps display selected register
- Console switches can load registers
- Memory can be read/written via console

### 3.2 Step Mode

- Press Start → execute exactly one micro-instruction
- Processor returns to Halt mode
- M register contains the next micro
- Used for debugging: step through microcode one instruction at a time

### 3.3 Run Mode

- Press Start → continuous execution from current MAR(A)
- Micros fetched from S-Memory (or M-Memory on B1720)
- Continues until:
  - Halt button pressed
  - Halt micro (1F) decoded
  - Other stop condition

### 3.4 Tape Mode

- Press Start → cassette starts, micros from tape
- Continues until:
  - Halt button pressed
  - Halt micro decoded
  - Tape error
  - Cassette Stop micro (2E)
  - Tape physically stops (end of tape)

---

## 4. Maintenance and Diagnostics

### 4.1 Processor Maintenance Procedures

The FE Technical Manual (Section V) provides detailed maintenance procedures:
- Memory testing routines
- Register verification
- Function box testing
- Clock timing verification
- I/O logic interface testing

### 4.2 Maintenance Aids

- Console step mode for single-stepping through microcode
- Register display/load for state inspection
- Diagnostic cassette tapes with test microcode
- Console lamps for real-time observation

### 4.3 Diagnostic Strategy

Diagnostics are themselves microprograms loaded from cassette. They:
1. Test each register by writing known patterns and reading back
2. Test memory by writing/reading patterns at various addresses
3. Test the function box by computing known results
4. Test the rotator/mask generator
5. Test I/O bus connectivity

---

## 5. C++ Implementation Notes

### 5.1 Console Emulation

```cpp
class Console {
public:
    // Lamp state (24 bits)
    uint32_t lamps = 0;

    // Switch state (24 bits)
    uint32_t switches = 0;

    // Register select
    uint8_t register_group = 0;   // 0-15
    uint8_t register_select = 0;  // 0-3

    // Mode
    enum class Mode { STEP, RUN, TAPE };
    Mode mode = Mode::STEP;

    // State
    bool running = false;
    bool halted = true;

    // Cassette
    bool cassette_power = false;
    bool cassette_bot = true;
    bool tape_parity_error = false;

    // Buttons (edge-triggered)
    void press_start();
    void press_halt();
    void press_load();
    void press_read_write(bool write_mode);
    void press_interrupt();   // sets CC(0)
    void press_rewind();

    // Update lamp display (called in halt mode)
    void update_display(const RegisterFile& regs);

    // Get switch value (for NULL register source in halt mode)
    uint32_t get_switch_value() const { return switches & 0x00FFFFFF; }
};
```

### 5.2 Cassette Emulation

```cpp
class CassetteLoader {
    std::vector<uint8_t> tape_data;  // Raw tape image
    size_t position = 0;             // Current bit position
    bool running = false;
    bool at_bot = true;

public:
    // Load a tape image file
    void load_tape(const std::string& filename);

    // Read next bit from tape (bit-serial)
    int read_bit();

    // Check if at inter-record gap
    bool at_gap() const;

    // Control
    void start();
    void stop();
    void rewind();

    // Fill U register (returns true when 16 bits accumulated)
    bool accumulate_to_U(uint16_t& u_register);
};
```

### 5.3 Bootstrap Implementation

For the emulator, the bootstrap can be simplified:
1. Load a cassette tape image file
2. Feed data into U register bit-by-bit
3. Transfer to M and execute when U is full
4. Continue until tape mode ends

If no original cassette images are available, craft test microcode manually
as binary data and load it as a simulated cassette.

---

*Previous: [09 — Interrupt System](09_INTERRUPTS.md) | Next: [11 — Emulator Design Guide](11_EMULATOR_DESIGN.md)*
