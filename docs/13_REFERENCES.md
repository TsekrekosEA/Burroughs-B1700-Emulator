# 13 — References & Bibliography

## 1. Primary Sources (In This Project)

These are the OCR'd/extracted documents in the `resources/` directory that
served as the primary reference material for this documentation set.

| File | Original Document | Pages | Year |
|------|-------------------|-------|------|
| `1053360_B1700_FE_Tech_May73_ocr.pdf` | B1700 Systems — Field Engineering Technical Manual | 429 | 1973 |
| `manual_ocr.txt` | OCR text extraction of the above | 81,792 lines | — |
| `paper_1974.txt` | Datapro 70: Burroughs B 1700 Series | ~50 | 1974 |
| `paper_1978.txt` | Datapro 70: Burroughs B 1700 Series (updated) | ~60 | 1978 |

---

## 2. Official Burroughs Documentation

### 2.1 Field Engineering / Hardware

- **B1700 Systems — Field Engineering Technical Manual** (May 1973). Document
  number 1053360. The master reference for this documentation project. Covers
  registers, micro-operators, memory, function boxes, I/O, console, and
  bootstrap in exhaustive detail.

- **B1700 Systems Reference Manual**. Burroughs Corporation. General system
  architecture overview for system programmers.

### 2.2 Software / Languages

- **MIL Reference Manual**. Burroughs Corporation. Specification of the Micro
  Instruction Language used to write interpreters. Available at bitsavers.

- **SDL Reference Manual**. Burroughs Corporation. Specification of the System
  Definition Language used to define S-languages and write system software.

- **MCP II Reference Manual**. Burroughs Corporation. Master Control Program
  operating system documentation.

- **COBOL S-Language Specification**. Burroughs Corporation. Defines the
  S-language instruction set for the COBOL interpreter.

---

## 3. Academic Papers

- Wilner, Wayne T. **"Design of the Burroughs B1700."** *AFIPS Conference
  Proceedings*, Vol. 41, Part I, pp. 489–497, 1972. SJCC (Spring Joint
  Computer Conference). The foundational paper describing the B1700's
  language-directed architecture.

- Wilner, Wayne T. **"Burroughs B1700 Memory Utilization."** *AFIPS
  Conference Proceedings*, 1972. Analysis of memory efficiency through
  bit-addressable, variable-length fields.

- Wilner, Wayne T. **"B1700 Micro-Implementation of Computer Languages."**
  *Proceedings of the ACM Annual Conference*, 1972. Details on how high-level
  language interpreters are structured in microcode.

- Wilner, Wayne T. **"Recursive Machines."** Technical report, Burroughs
  Corporation, 1976. Theoretical extensions of the dynamically programmable
  architecture concept.

---

## 4. Trade Publications

- **Datapro 70: Burroughs B 1700 Series** (January 1974). Datapro Research
  Corporation, Delran NJ. Model specifications and pricing for B1710, B1714,
  B1720, B1724, B1728.

- **Datapro 70: Burroughs B 1700 Series** (July 1978). Datapro Research
  Corporation, Delran NJ. Updated specifications including B1705, B1726,
  B1830, B1840. Contains detailed configuration tables, peripheral pricing,
  and competitor analysis.

---

## 5. Online Archives

### 5.1 Bitsavers

The primary online archive for Burroughs documentation.

- **B1000 family index**:
  `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/`

- **B1000 Software documentation**:
  `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/B1000_Software/`

- **B1000 Internal Memos**:
  `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/B1000_Internal_Memos/`

- **B1000 Hardware**:
  `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/B1000_Hardware/`

### 5.2 Wikipedia

- **Burroughs B1700**: `https://en.wikipedia.org/wiki/Burroughs_B1700`
  Good overview of the architecture, models, and historical context.

- **Burroughs large systems**: `https://en.wikipedia.org/wiki/Burroughs_large_systems`
  Context for the broader Burroughs product line.

---

## 6. Related Work

### 6.1 Other Microprogrammed Architecture Emulators

For implementation reference, these emulators of other microprogrammed or
unusual architectures may be useful:

| Project | Architecture | Language | URL |
|---------|-------------|----------|-----|
| SIMH | Many (PDP-11, VAX, etc.) | C | https://github.com/simh/simh |
| B5500 Emulator | Burroughs B5500 | JavaScript | https://github.com/retro-b5500/retro-b5500 |
| Xerox Alto Emulator | Xerox Alto (microprogrammed) | Various | Multiple implementations |
| MAME | Arcade/micro (many CPUs) | C++ | https://github.com/mamedev/mame |

### 6.2 Emulator Design References

- Ahl, David H. **"Writing a Computer Emulator."** Various vintage computing
  publications.

- **SIMH Documentation** — `http://simh.trailing-edge.com/` — Design patterns
  for historical computer emulation.

---

## 7. Document Revision History

| Date | Author | Changes |
|------|--------|---------|
| 2024 | Generated from primary sources | Initial documentation set (docs 00–13) |

---

*Previous: [12 — Existing Software & Tools](12_EXISTING_SOFTWARE.md) | Index: [00 — Master Index](00_INDEX.md)*
