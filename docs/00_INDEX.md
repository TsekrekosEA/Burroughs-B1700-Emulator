# Burroughs B1700 Architecture Documentation

## Complete Technical Reference for Emulator Development

This documentation provides a comprehensive, modern reference for the Burroughs B1700
computer architecture, assembled from the original Field Engineering Technical Manual
(1053360, May 1973), Wayne T. Wilner's design papers, Datapro research reports (1974/1978),
bitsavers.org archival documents, and supplementary online research.

The goal is to supply every detail required to implement a faithful C++ emulator of the
B1700 system.

---

### Document Index

| # | Document | Description |
|---|----------|-------------|
| 01 | [Architecture Overview](01_ARCHITECTURE_OVERVIEW.md) | Design philosophy, system organization, processor models |
| 02 | [Register Set](02_REGISTERS.md) | Complete register file: widths, encoding, semantics |
| 03 | [Memory System](03_MEMORY.md) | S-Memory, M-Memory, bit-addressing, FIU, virtual memory |
| 04 | [Microinstruction Set](04_MICROINSTRUCTIONS.md) | All 24+ micro-operators with binary encoding and semantics |
| 05 | [ALU and Data Paths](05_ALU_DATAPATH.md) | 24-bit and 4-bit function boxes, bias mechanism, rotator |
| 06 | [Interpreter & S-Language](06_INTERPRETERS.md) | SDL, S-machines, interpreter switching, Gismo |
| 07 | [MCP Operating System](07_MCP.md) | Master Control Program, job scheduling, multiprogramming |
| 08 | [I/O System](08_IO_SYSTEM.md) | I/O bus, peripherals, DMA, communications |
| 09 | [Interrupt System](09_INTERRUPTS.md) | Software interrupts, timer, console, bus interrupts |
| 10 | [Console & Bootstrap](10_CONSOLE_BOOTSTRAP.md) | Cassette loader, console panel, maintenance diagnostics |
| 11 | [Emulator Design Guide](11_EMULATOR_DESIGN.md) | C++ implementation strategy, data structures, testing |
| 12 | [Existing Software & Tools](12_EXISTING_SOFTWARE.md) | Known programs, compilers, manuals, emulator attempts |
| 13 | [References & Sources](13_REFERENCES.md) | Complete bibliography and source URLs |

---

### Key Facts at a Glance

| Property | Value |
|----------|-------|
| Manufacturer | Burroughs Corporation, Detroit, Michigan |
| Introduction | June 1972 |
| Designer | Wayne T. Wilner, Ph.D. (Santa Barbara Plant, Goleta, CA) |
| Architecture type | Dynamically microprogrammed, no fixed ISA |
| Microinstruction width | 16 bits (fixed) |
| Microinstruction count | 24 implemented (B1710); 28–32 (B1720) |
| Data path width | 24 bits |
| Memory addressing | Bit-addressable (no inherent word/byte boundaries) |
| Address space | 24-bit bit-addresses (up to 2 MB) |
| Main memory (S-Memory) | MOS semiconductor, 16K–262K bytes |
| Control memory (M-Memory) | Bipolar, 2K–8K bytes (B1720 only) |
| Register count | ~20 programmer-visible + scratchpad |
| A-Stack depth | 16 × 24-bit (B1710) / 32 × 24-bit (B1720) |
| Processor cycle time | 167 ns (B1720) – 500 ns (B1712) |
| Languages supported | COBOL, FORTRAN, RPG, BASIC, SDL/MCP |
| Emulators available | IBM 1401/1440/1460, B100/200/300/500, IBM 1130 |

---

*Generated February 2026. Based on primary sources listed in [References](13_REFERENCES.md).*
