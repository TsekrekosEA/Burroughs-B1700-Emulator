# 12 — Existing Software & Tools

*Last updated: March 2026 — comprehensive web survey*

## 1. Known Emulator Projects

As of March 2026, **no fully functional B1700 emulator exists publicly**.

| Project | URL | Status |
|---------|-----|--------|
| TsekrekosEA/Burroughs-B1700-Emulator | https://github.com/TsekrekosEA/Burroughs-B1700-Emulator | **This project** — the emulator documented here. |
| allanhewitt/b1700 | https://github.com/allanhewitt/b1700 | Empty repository — no description, no code. Last updated Aug 2023. |

No other B1700 emulator projects were found on GitHub, Archive.org, or any
retrocomputing forums. The SIMH project (https://github.com/simh/simh)
includes a **Burroughs B5500** emulator by Richard Cornwell, but nothing for
the B1700/B1000 family. (Note: SIMH's "CDC1700" simulator is for a
completely unrelated machine.)

The B1700 remains one of the few historically significant architectures with
no publicly available emulator. This is likely because:

- The architecture requires microcode (interpreters) to do anything useful
- Original microcode binary images are extremely rare
- The bit-addressable memory and FIU are non-trivial to implement
- Very few machines were produced compared to mainframes of the era

---

## 2. Surviving Binary Software & Images

### 2.1 ★ B1700 Cold Start Loader — MIL Source Code

**This is the single most important software artifact found.**

| Item | Details |
|------|---------|
| **URL** | http://www.bitsavers.org/bits/Burroughs/B1700/cards/B1700_cold_start_loader_obsolete.txt |
| **Size** | 13 KB |
| **Format** | Plain-text MIL (Micro Implementation Language) source code |
| **Date** | October 1972 |
| **Title** | "Emulation Vehicle Cold Start Loader — B1700" |

This is **real, complete MIL source code** for the bootstrap program that
loaded B1700 system software from a host B1500 via the emulation vehicle
port. It demonstrates:

- MIL syntax: `DEFINE`, `MOVE`, `WRITE n BITS FROM`, `READ n BITS TO`,
  `IF...THEN`, `GO TO`, `CALL`, `EXIT`, `HALT`, `DISPATCH`, `OVERLAY`
- Register usage: `X`, `Y`, `T`, `L`, `FA`, `FB`, `FL`, `CP`, `CD`, `CA`,
  `LR`, `BR`, `TOPM`, etc.
- Bit-level operations: `WRITE (25) BITS FROM L INC FA`, `READ 24 BITS TO Y`,
  `SHIFT T LEFT BY 12 BITS TO X`, `EXTRACT 8 BITS FROM T(16) TO T`
- I/O operations: `DISPATCH LOCK`, `DISPATCH WRITE`, `DISPATCH READ AND CLEAR`
- Memory addressing: hex literals (`LIT HFFFFFF TO X`), bit-addressed
  constants, S-memory and M-memory loading
- Card reader protocol: translates punched-card load format to EBCDIC,
  decodes M-memory image words from card data
- Subroutine structure: `CALL XCBUF`, `CALL MOVE-B1500-DATA`, labeled
  routines with `EXIT` returns

There is also a companion photograph:

| Item | Details |
|------|---------|
| **URL** | http://www.bitsavers.org/bits/Burroughs/B1700/cards/B1700_cold_start_loader.JPG |
| **Size** | 563 KB |
| **Format** | JPEG photograph of the actual punched card deck |

The `/bits/Burroughs/B1700/cassettes/` directory was created in June 2025 but
is currently empty, suggesting **active preservation work is in progress**.

### 2.2 ★ B1000 MK10.0 System Tape Image

| Item | Details |
|------|---------|
| **URL** | http://www.bitsavers.org/bits/Burroughs/B1000/B1000_MK10.0_System_1981/b1000_mk10.0_1981.tap |
| **Size** | 9.2 MB |
| **Format** | `.tap` (SIMH-style tape image) |
| **Date** | 1981 |
| **Contents** | B1000 Mark 10.0 system software distribution tape |

This is a **real tape image** of a complete B1000 MK10.0 system release.
However, the accompanying error log (`b1000_mk10.0_1981.txt`, 908 bytes)
records 19 medium errors during tape reading (files 323 and 341), meaning
some data may be corrupted. The directory also includes a photograph of the
tape label (`label.jpg`, 188K).

The MK10.0 release (1982) was for B1800/B1900 systems, so this tape likely
contains S-code programs and interpreters in the later B1000 format, not
original B1700 microcode. However, the S-language formats, MCP structures,
and SDL programs on this tape would be directly relevant for emulation.

### 2.3 B1000 Miscellaneous Zip Files

Located at `http://www.bitsavers.org/bits/Burroughs/B1000/`:

| File | Size | Date | Likely Contents |
|------|------|------|-----------------|
| `cm_series_cobol.zip` | 225K | 2014-08-22 | COBOL-related material |
| `fetest.zip` | 287K | 2014-08-22 | Front-end test programs |
| `franks_trs.zip` | 2.7M | 2014-08-23 | Unknown (largest file) |
| `histep_III.zip` | 144K | 2014-08-22 | HISTEP diagnostic |
| `symco.zip` | 1.2M | 2014-08-22 | Possibly symbolic code/compiler output |

These zip files have not been examined in detail. They were uploaded in 2014
and could contain actual binary object code, source listings, or tape
extracts. **Downloading and analyzing these is a high priority.**

---

## 3. Related Emulators (Other Burroughs Machines)

The B1700 supported emulation of other architectures via microcode
interpreters. These may serve as reference for I/O conventions and programming
style:

| Emulated Machine | B1700 Support | Notes |
|------------------|---------------|-------|
| IBM 1401 | Emulator interpreter available | One of the primary selling points |
| IBM 1440 | Emulator interpreter available | |
| IBM 1460 | Emulator interpreter available | |
| IBM 1130 | Emulator interpreter available | |
| Burroughs B100 | Emulator interpreter available | |
| Burroughs B200 | Emulator interpreter available | |
| Burroughs B300 | Emulator interpreter available | |
| Burroughs B500 | Emulator interpreter available | |
| Burroughs B3500 | Emulator interpreter available | |

The SIMH B5500 emulator (by Richard Cornwell) is the closest related working
emulator. It may be useful as a reference for Burroughs I/O device emulation
conventions.

---

## 4. Original Burroughs Software

### 4.1 System Software

| Software | Description |
|----------|-------------|
| **MCP II** | Master Control Program — OS for the B1700 family |
| **SDL Interpreter** | System Definition Language interpreter (runs in microcode) |
| **MIL Assembler** | Micro Instruction Language assembler (runs under SDL/MCP) |
| **SDL Compiler** | Compiles SDL source to S-code |
| **SDL2 Compiler** | Later version of SDL compiler |
| **COBOL Interpreter** | Runs ANSI COBOL S-programs; later COBOL74 |
| **FORTRAN Interpreter** | Runs FORTRAN S-programs; later FORTRAN77 |
| **RPG Interpreter** | Runs RPG S-programs |
| **BASIC Interpreter** | Runs BASIC S-programs; later Interactive BASIC (IBASIC) |
| **Pascal Interpreter** | Runs Pascal S-programs (from MK11 release, ~1984) |
| **1400 Interpreter** | IBM 1401/1440/1460 emulation interpreter |
| **B500 Interpreter** | Burroughs B500 emulation interpreter |
| **CANDE** | Command AND Edit — interactive terminal editor |
| **WFL** | Work Flow Language — job control |
| **DMSII** | Database Management System II |
| **GEMCOS** | Generalized Message Control System |
| **SMCS** | Supervisory Message Control System |
| **SORT** | Sort/merge utility |
| **NDL** | Network Definition Language |
| **SYCOM** | System Communications module |
| **Diagnostic Microcode** | Factory test routines loaded from cassette |

### 4.2 Software Distribution History

Original software was distributed on:
- **Cassette tapes** (bootstrap loaders, diagnostic microcode)
- **Disk packs** (MCP, interpreters, compilers)
- **Magnetic tape** (distribution media)

Software releases were numbered by "Mark" level:

| Release | Year | Key Notes |
|---------|------|-----------|
| Mark III (Rel. 03) | 1974 | First production release documented at bitsavers |
| Mark IV (Rel. 04) | 1975 | |
| Mark V (Rel. 05) | 1976 | |
| Mark VI (Rel. 06) | 1977 | |
| Mark VII (Rel. 07) | 1978 | B1800/B1700 combined release |
| Mark IX (Rel. 09) | 1980 | |
| Mark X (Rel. 10) | 1982 | **Tape image exists** (see §2.2) |
| Mark XI (Rel. 11) | 1984 | |
| Mark XII (Rel. 12) | 1985 | |
| Mark XIII (Rel. 13) | 1986 | Final release; B1000 migration to A-Series |

---

## 5. Documentation Archives

### 5.1 Bitsavers — Binary/Software Archive

**Base URL**: `http://www.bitsavers.org/bits/Burroughs/`

| Path | Contents | Status |
|------|----------|--------|
| `B1700/cards/` | Cold start loader MIL source + photo | **Available** |
| `B1700/cassettes/` | (empty — created June 2025) | Preservation in progress? |
| `B1000/B1000_MK10.0_System_1981/` | 9.2 MB tape image + label | **Available** (with errors) |
| `B1000/cm_series_cobol.zip` | 225K COBOL material | **Available** |
| `B1000/fetest.zip` | 287K front-end tests | **Available** |
| `B1000/franks_trs.zip` | 2.7M unknown | **Available** |
| `B1000/histep_III.zip` | 144K diagnostic | **Available** |
| `B1000/symco.zip` | 1.2M unknown | **Available** |

### 5.2 Bitsavers — PDF Documentation

**Base URL**: `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/`

#### B1700 Hardware Documentation (`B1700/`)

| Document | Size | Date | Significance |
|----------|------|------|--------------|
| `1057155_B1700_System_Reference_Manual_197204.pdf` | 7.2M | Apr 1972 | **Primary architecture reference** |
| `1057155_B1700_System_Reference_Manual_197301.pdf` | 10M | Jan 1973 | Revised edition |
| `1053360_B1700_FE_Technical_Manual_197211.pdf` | 22M | Nov 1972 | Field engineering — hardware details |
| `1053360_B1700_FE_Technical_Manual_197305.pdf` | 24M | May 1973 | Revised FE manual |
| `1066941_B1720_Technical_Manual_197808.pdf` | 28M | Aug 1978 | B1720 successor details |
| `1053378_B1700_Installation_197602.pdf` | 7.5M | Feb 1976 | Installation guide |
| `1057692_B1700_Parts_197510.pdf` | 19M | Oct 1975 | Parts catalog (schematics?) |
| `1070281_B1700_Power_Subsystem_197611.pdf` | 14M | Nov 1976 | Power supply details |
| `2102380_B1700_Card_Tester_197609.pdf` | 4.3M | Sep 1976 | Card tester |
| `Organick_-_Interpreting_Machines_1978.pdf` | 15M | 1978 | Organick textbook |
| `Patent_3739352.pdf` | 895K | — | Wilner patent |
| `Auerbach_Burroughs_B1700_197407.pdf` | 817K | Jul 1974 | Independent analysis |
| `1093671_B1714_Maintenance_and_Basic_Software_Training_197603.pdf` | 2.0M | Mar 1976 | Training material |

#### B1800 Documentation (`B1800/`)

| Document | Size | Significance |
|----------|------|--------------|
| `1090644_B1870_B1860_Systems_Reference_Manual_197903.pdf` | 16M | B1800 series architecture |
| `1098282_B1870_Technical_Manual_Vol1_Operation_and_Maint_197711.pdf` | 10M | B1870 maintenance |
| `Connection_Port_Functional_Design_19790328.pdf` | 1.9M | Inter-processor connection |

#### MIL Reference (`B1000_Software/MIL/`)

| Document | Size | Date | Significance |
|----------|------|------|--------------|
| `1072568_B1700_Micro_Implementation_Language_MIL_Reference_197312.pdf` | 4.5M | Dec 1973 | **Original MIL reference** |
| `1072568_B1700_MIL_Micro_Implementation_Language_197705.pdf` | 5.7M | May 1977 | Revised MIL reference |

#### SDL/UPL Reference (`B1000_Software/SPL_UPL/`)

| Document | Size | Date | Significance |
|----------|------|------|--------------|
| `1081346_B1700_SDL_Reference_197412.pdf` | 11M | Dec 1974 | **Original SDL reference** |
| `1067170_B1700_User_Programming_Language_UPL_Reference_197312.pdf` | 6.1M | Dec 1973 | UPL reference |
| `5000847_B1700_Software_Development_Language_BNF_Version_197406.pdf` | 12M | Jun 1974 | **SDL BNF grammar** |
| `1137833_B1000_SDL_UPL_Reference_Manual_198207.pdf` | 17M | Jul 1982 | Combined later reference |

#### SDL2 Compiler (`B1000_Software/SDL2/`)

| Document | Size | Date |
|----------|------|------|
| `2228-3519_B1000_SDL2_Compiler_198104.pdf` | 4.1M | Apr 1981 |
| `2228-3519B_B1000_SDL2_Compiler_198211.pdf` | 5.7M | Nov 1982 |
| `2228-3519C_B1000_SDL2_Compiler_198406.pdf` | 7.1M | Jun 1984 |

#### Language Software Documentation

| Directory | Key Documents |
|-----------|--------------|
| `COBOL/` | B1700 COBOL Reference (1975, 1978, 1979), B1000 COBOL74 Reference (1982, 1984), B1700 COBOL Compiler Documentation (1973) |
| `FORTRAN/` | B1000 FORTRAN Reference (1982), B1000 Fortran77 (1983, 1984) |
| `BASIC/` | B1700 BASIC Reference (1973), B1800/B1700 BASIC (1979), B1000 BASIC (1982), Interactive BASIC (1981, 1984) |
| `RPG/` | B1700 RPG Reference (1975), B1800/B1700 RPG (1979), B1000 RPG (1984) |
| `Pascal/` | B1000 Pascal Language Manual (1985, 1986), Pascal S-Machine (1982) |
| `CANDE/` | B1700 CANDE (1976, 1977), B1000 CANDE Users Manual (1980, 1982) |
| `WFL/` | B1000 Work Flow Language Manual (1986) |
| `DMS/` | B1700 DMSII Reference (1976), B1000 DMSII (1984), DASDL (1979) |
| `Communications/` | NDL (1975, 1978), HASP (1976, 1978), BNA (1981), SYCOM (1978), RJE, Data Comm |
| `GEMCOS/` | B1700 GEMCOS Users Manual (1976), B1800/B1700 GEMCOS (1980), B1000 GEMCOS (1985) |
| `SMCS/` | SMCS Reference (1983), SMCS Installation (1985) |
| `SORT/` | B1000 SORT Language Manual (1983) |
| `MONITOR/` | B1000 MONITOR Installation (1985) |
| `1400_Interpreter/` | B1700 IBM 1401/1440/1460 Emulator preliminary (1975), B1000 1400 Interpreter (1981) |
| `B500_Interpreter/` | B1000 B500 Interpreter Reference (1981) |
| `ODESY/` | B1700 On-Line Data Entry System (1977), B1000 ODESY (1983) |
| `TMCS/` | Text Management and Communications System (1982) |
| `Migration/` | B1000 to B2000/B3000/B4000 (1984), B1000 to A-Series (1984, 1987) |
| `CUBE/` | CUBE XXXII MCP Memory Management (1978), Inside Your Living MCP (1987) |

#### Software Release Documentation (`Release_NN_YYYY/`)

| Release | Key Documents |
|---------|--------------|
| Release 03 (1974) | B1700 System Software Operational Guide (11M) |
| Release 07 (1978) | B1800/B1700 System Software Release Mark VII.0 (8.7M), MCP Control Syntax (11M) |
| Release 10 (1982) | System Software Operations Guide Vol.2 (12M), MCP Communicates & Structures (6.6M), MCPII Mark 10 Internal Reference (18M), Patch releases |
| Release 13 (1986) | System Software Operations Guide Mark 13.0 Vol.1 (19M), Release Letters (8.7M) |

### 5.3 Internal Memos — The Crown Jewels

**Base URL**: `http://www.bitsavers.org/pdf/burroughs/SmallSystems/B1000/B1000_Internal_Memos/`

The 8 books of Burroughs Internal Memos are the most detailed engineering
documentation available. They contain **S-Language specifications** (the
actual binary format each interpreter consumes), compiler logic descriptions,
MCP internals, and utility specifications. These are essential for
reimplementing B1700 software from scratch.

#### Top-level Internal Memos (not in books)

| Document | Size | Significance |
|----------|------|--------------|
| `2212-5298_MIL_Micro_Implementation_Language.pdf` | 8.6M | **MIL internal specification** |
| `2212-5462E_B1000_MCP_II_10.0_Specification.pdf` | 21M | **Complete MCP II specification** |
| `2212-5405F_B1700_SDLBNF_198006.pdf` | 8.9M | SDL BNF grammar |
| `2219-0334_B1700_INTERP_MONITOR_197708.pdf` | 670K | Interpreter monitor |
| `2228-3543A_BOLT_Executive_198201.pdf` | 8.5M | BOLT executive |
| `B1700_System_Class_1974.pdf` | 12M | System classification |
| `Wilner_B1700_Design_and_Implementation_197205.pdf` | 1.8M | **Original Wilner design document** |
| `McCrea_-_Software_Development_Language_for_the_Burroughs_B1700.pdf` | 3.4M | SDL design paper |
| `B1500_Microprogramming_197101.pdf` | 2.9M | B1500 predecessor microprogramming |
| `B1500_Microprogramming_Manual.pdf` | 1.2M | B1500 microprogramming manual |
| `B1700_DOCTOR_Document_Formatter_197707.pdf` | 1.1M | Document formatter |
| `Language_Development_Memos_1974-75.pdf` | 2.0M | Language development history |
| `M-Processor3_Design_Specification_19760326.pdf` | 5.1M | M-Processor design |
| `1913-1747_M-Memory_197405.pdf` | 2.3M | M-Memory specification |
| `M-Memory_Proc_19760109.pdf` | 2.7M | M-Memory processor |
| `2228-3584B_SYSTEM_ODT.pdf` | 445K | System ODT (debugging) |
| `_memos_by_number.txt` | 5.2K | Index of memos by number |

#### Book 1 — S-Language Specifications

| Document | Size | Significance |
|----------|------|--------------|
| `2201-2389L_SDL_S-Language_198006.pdf` | 11M | **SDL S-Language binary format** |
| `2201-6729E_COBOL_S-Language_197602.pdf` | 6.5M | **COBOL S-Language binary format** |
| `2201-6737D_FORTRAN_S-Language_197807.pdf` | 5.0M | **FORTRAN S-Language binary format** |
| `2201-6752J_SORT_Language.pdf` | 3.8M | SORT S-Language |
| `2205-1155C_Report_Program_Generator_197908.pdf` | 264K | RPG |
| `2210-0135D_BASIC_S-Language_197709.pdf` | 4.3M | **BASIC S-Language binary format** |

#### Book 2 — Utilities and Loaders

| Document | Significance |
|----------|--------------|
| `2212-5140C_CASSETTE_LOADER_197706.pdf` | **Cassette loader — bootstrap details** |
| `2212-5157E_CASSETTE_MAKER_197807.pdf` | **Cassette maker — tape format details** |
| `2212-5165E_SSLOAD_MAKCAS_197908.pdf` | System software loader & cassette maker |
| `2212-5116H_DMPALL_198202.pdf` | Dump utility |
| `2212-5132B_FILE_LOADER_197609.pdf` | File loader |
| `2212-5124C_FILE_PUNCHER_197708.pdf` | File puncher |
| `2212-5199C_SYCOPY_197609.pdf` | System copy |
| `2212-5113B_B500_Interpreter_Environment_197904.pdf` | B500 interpreter environment |

#### Book 3 — Compilers and MIL

| Document | Size | Significance |
|----------|------|--------------|
| `2212-5298E_MIL_198201.pdf` | 21M | **Full MIL assembler internal spec** |
| `2212-5306C_RPG_S-Language_197901.pdf` | 6.4M | RPG S-Language |
| `2212-5314C_COBOL_Compiler_197802.pdf` | 2.4M | COBOL compiler internals |
| `2212-5322B_FORTRAN_Compiler_197802.pdf` | 1.6M | FORTRAN compiler internals |
| `2212-5280C_BASIC_Compiler_197706.pdf` | 870K | BASIC compiler internals |
| `2212-5264F_Disk_Sort_198012.pdf` | 2.3M | Disk sort |

#### Book 4 — Interpreters and SDL

| Document | Size | Significance |
|----------|------|--------------|
| `2212-5348D_B500_Interpreter_197802.pdf` | 2.8M | **B500 interpreter internals** |
| `2212-5355D_1400_Interpreter_197904.pdf` | 6.2M | **1400 interpreter internals** |
| `2212-5389C_SDL_UPL_Compiler_198201.pdf` | 818K | SDL/UPL compiler internals |
| `2212-5405G_B1000_SDL_BNF_Version_198203.pdf` | 18M | **SDL BNF version** |
| `2212-5371D_Sort_Collate_197608.pdf` | 1.1M | Sort collate |
| `2212-5397C_COBOL_Compiler_Logic_197803.pdf` | 9.7M | COBOL compiler logic |

#### Book 5 — MCP II and System Software

| Document | Size | Significance |
|----------|------|--------------|
| `2212-5462E_MCP_II_198012.pdf` | 34M | **Complete MCP II internal spec** |
| `2212-5504F_1400_IEP_197904.pdf` | 2.0M | 1400 interpreter execution |
| `2212-5454A_Data_Communications_197802.pdf` | 10M | Data communications |
| `2212-5488E_SYSTEM_DISK.INIT_197906.pdf` | 1.7M | Disk initialization |

#### Book 6 — Utilities and System Programs

Key items: CANDE internals, MCP Control Syntax (24M!), Cold Start Disk,
TAPECOPY, File Security, Host RJE System.

#### Book 7 — Networking and Database

Key items: System PATCH, Interpreter Monitor, DASDL, SYCOM (20M), SMCS,
Tape Archive, DMSII Audit and Recovery.

#### Book 8 — Compilers, Diagnostics, and Late Utilities

| Document | Significance |
|----------|--------------|
| `2222-2780A_DIAGNOSTIC_MICRO_LANGUAGE_197807.pdf` | **Diagnostic micro-language** |
| `2222-2731B_B500_INTERPRETER_ENV_197904.pdf` | B500 interpreter environment |
| `2222-2749B_FORTRAN77_S-LANGUAGE_198009.pdf` | FORTRAN77 S-Language |
| `2222-3051B_COBOL74_Compiler_198101.pdf` | COBOL74 compiler |
| `2222-3069B_COBOL74_S-LANGUAGE_198104.pdf` | COBOL74 S-Language |
| `2228-3410C_IBASIC_S-LANGUAGE_198202.pdf` | Interactive BASIC S-Language |
| `2228-3402A_SYSTEM_LOAD.CAS_197909.pdf` | **System LOAD.CAS cassette loader** |
| `2233-2811A_MLC4_5_Software_Loader_198109.pdf` | Multi-Line Controller loader |

### 5.4 Archive.org

Archive.org has 446 full-text search results for `"Burroughs B1700"`, but
all appear to be **mirrors of bitsavers PDFs**, not unique material.
Examples:

- `bitsavers_burroughsBkCtlSep72_5019421` — B1700 DiskCtl Sep72
- `bitsavers_burroughsBechMay73_30785140` — B1700 FE Tech May73
- `bitsavers_burroughsBdTstrSep76_6575031` — B1700 Card Tester Sep76

The Archive.org copies serve as redundant backups for bitsavers content.

### 5.5 Academic Papers

| Paper | Author(s) | Year | Significance |
|-------|-----------|------|--------------|
| "Design of the B1700" | W. T. Wilner | 1972 | Original design paper (AFIPS) |
| "Burroughs B1700 Memory Utilization" | W. T. Wilner | 1972 | Memory architecture analysis |
| "B1700 Micro-Implementation of Languages" | Wilner | 1972 | Interpreter design philosophy |
| "Microprogramming Environment on the Burroughs B1700" | Wilner | 1972 | IEEE CompCon '72 |
| "Unconventional Architecture" | Wilner | 1976 | ACM Annual Conference |
| "Recursive Machines" | Wilner | 1976 | Theoretical foundations |
| "Interpreting Machines" | Organick | 1978 | Textbook treatment (15M PDF at bitsavers) |
| "B1700 Design and Implementation" | Wilner | 1972 | Internal memo (1.8M PDF at bitsavers) |

---

## 6. Tools for Development

### 6.1 MIL Assembler (To Be Created)

No standalone MIL assembler exists outside the B1700 system itself. The
surviving cold start loader source (§2.1) provides a real-world example of
MIL syntax to guide cross-assembler development. An essential companion tool
for the emulator is a cross-assembler that can:

1. Parse MIL syntax (as demonstrated in the cold start loader)
2. Generate 16-bit microinstruction words
3. Produce binary images loadable into the emulator
4. Support labels, symbolic register names, DEFINE macros, comments

Example of **actual MIL syntax** from the cold start loader:

```
DEFINE EMV-PORT-CHANNEL      _ S5A     * TEMPORARY STORAGE FOR PORT
DEFINE RDEND            _ H61C5D5      * CONSTANT -/EN-
DEFINE IO-WAIT-FLAG     _ CC (0)       * I/O WAIT INDICATOR
DEFINE ENDING-ADDRESS   _ S1A          * STORAGE FOR LAST M ADDRESS

     SET CD TO 4             * SET MEMORY OUT-OF-BOUNDS OVERRIDE
     MOVE 24 TO CP           * SET 24 BIT BINARY MODE
     MOVE MAXS TO LR
     MOVE 0 TO BR            * SET MEMORY LIMITS
     MOVE H20 TO T           * PORT 2 CHANNEL 0

     CLEAR L FA              * INITIALIZE FOR CLEAR OPERATION
     MOVE MAXS TO Y          * UPPER BOUNDARY OF CLEAR
.MORE
     WRITE (25) BITS FROM L INC FA    * SET GOOD PARITY AND CLEAR 24 BITS
     MOVE FA TO X            * PREPARE TO CHECK FOR END OF CLEAR
     IF X LSS Y  GO TO -MORE * CHECK IT...
```

### 6.2 Tape Image Tools

A tool to read the `.tap` format tape image (§2.2) and extract individual
files. The SIMH `.tap` format uses 4-byte little-endian record length
headers. The B1000 tape would contain multiple files in the B1000 system
format.

A cassette tape image tool based on the CASSETTE_LOADER and CASSETTE_MAKER
internal memos (Book 2) is also needed.

### 6.3 Disassembler

A standalone tool to disassemble 16-bit micro words from binary dumps or
memory images. Useful for analyzing the MK10.0 tape image and any original
microcode that surfaces.

---

## 7. Community & Resources

### 7.1 Online Communities

| Resource | URL | Notes |
|----------|-----|-------|
| Bitsavers | http://www.bitsavers.org/ | **Primary preservation source** — active uploads as of 2025 |
| Classiccmp (cctech) | http://classiccmp.org/pipermail/cctech/ | Mailing list archive through June 2022 |
| VCFed Forums | https://forum.vcfed.org/ | Vintage Computer Federation — no B1700-specific subforum found |
| Reddit r/retrocomputing | https://reddit.com/r/retrocomputing | No B1700-specific discussions found |
| Wikipedia | https://en.wikipedia.org/wiki/Burroughs_B1700 | Overview article with references |

### 7.2 Museums and Archives

| Institution | Location | Holdings |
|-------------|----------|----------|
| **Computer History Museum** | Mountain View, CA | Catalog search did not return B1700-specific results; may have uncatalogued holdings |
| **Charles Babbage Institute** (CBI) | University of Minnesota, Minneapolis | Finding aid CBI-90 covers Burroughs Corporation records, including "ETM 313: Proper Language Processor for Small Systems" (Bunker, et al., 1968) — the PLP project that became the B1700. Access requires in-person visit. |
| **National Museum of Computing** | Bletchley Park, UK | Possible holdings |

### 7.3 Key People

Per Wikipedia, the B1700 was designed primarily at Burroughs' Santa Barbara
plant, with initial research ("Proper Language Processor" / PLP) at the
Pasadena plant. The B1830 was designed at Liège, Belgium. Key names:

- **Wayne T. Wilner** — Primary B1700 architect, author of all major design papers
- **Bunker et al.** — PLP project team at Pasadena (1968 ETM 313)
- **McCrea** — SDL designer

---

## 8. Summary of What Exists for an Emulator

### Immediately Usable

| Artifact | Type | Usefulness |
|----------|------|------------|
| Cold start loader MIL source | Source code | **Direct input to MIL cross-assembler**; demonstrates real MIL syntax |
| MIL Reference Manuals (1973, 1977) | PDF | **Defines all micro-instructions** |
| B1700 System Reference Manual | PDF | **Defines complete hardware architecture** |
| S-Language specifications (SDL, COBOL, FORTRAN, BASIC, RPG) | PDF | **Defines binary formats for all interpreters** |
| MCP II specification (34M) | PDF | **Complete OS specification** |
| SDL BNF grammar | PDF | **Formal language definition** |

### Requires Analysis

| Artifact | Type | Potential |
|----------|------|-----------|
| B1000 MK10.0 tape image (9.2M) | `.tap` file | May contain S-code binaries, interpreters, MCP |
| 5 zip files in B1000/bits | Unknown | May contain binary object code or source listings |

### Not Yet Found

| Artifact | Notes |
|----------|-------|
| B1700 microcode binary images | No interpreter binaries (SDL, COBOL, etc.) have surfaced as loadable images |
| Disk pack images | No B1700/B1800 disk images found anywhere |
| Complete cassette tape images | bitsavers `/cassettes/` directory is empty |
| Original MIL assembler source | Only the cold start loader MIL source has been found |

---

## 9. Recommended Bootstrap Strategy

Given the available artifacts, the recommended approach is:

1. **Build a MIL cross-assembler** — Use the cold start loader as a syntax reference and the MIL Reference Manual for instruction encoding
2. **Parse the MK10.0 tape image** — Write a `.tap` file reader; extract and catalog all files on the tape
3. **Download and analyze the B1000 zip files** — May contain surprise binary content
4. **Write test micro-programs** — Exercise each micro-operator using the cross-assembler
5. **Implement the SDL interpreter in MIL** — Using the SDL S-Language spec from Book 1 and the SDL Reference Manual
6. **Write a minimal MCP** — Enough to load and run simple programs, based on the MCP II specification
7. **Implement COBOL/FORTRAN S-language interpreters** — Based on S-Language specifications from Book 1
8. **Contact bitsavers** — The recently-created (June 2025) `/cassettes/` directory suggests active preservation work; someone may be in the process of imaging B1700 cassette tapes

This approach re-creates the B1700 software stack from the ground up, which
is consistent with the original design philosophy of the machine as a
"language-directed architecture."

---

*Previous: [11 — Emulator Design Guide](11_EMULATOR_DESIGN.md) | Next: [13 — References & Bibliography](13_REFERENCES.md)*
