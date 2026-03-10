# 07 — MCP Operating System

## 1. Overview

The **Master Control Program (MCP)** is the B1700's operating system. Unlike
conventional operating systems that run in a privileged hardware mode, the MCP
on the B1700 is itself an **interpreted program** — it is written in SDL and
executes under its own S-SDL interpreter microprogram.

The MCP provides:
- Automatic multiprogramming (concurrent execution of multiple programs)
- Virtual memory management
- Interpreter loading and switching
- I/O management and device allocation
- Job scheduling and resource management
- Memory protection enforcement
- File system services

---

## 2. MCP Architecture

### 2.1 MCP as an Interpreted Program

The MCP is compiled from SDL source to S-SDL instructions. These S-SDL
instructions are interpreted by the SDL interpreter microprogram, which itself
executes as micro-operators on the hardware.

```
MCP (SDL source code)
    │
    ▼ compiled by SDL compiler
S-SDL code (MCP object code in S-memory)
    │
    ▼ interpreted by
SDL Interpreter (microprogram in memory)
    │
    ▼ executed on
Micro-operator hardware
```

Because the MCP is always active, the **SDL interpreter is always present** in
memory. If a user program is also written in SDL, no additional interpreter
loading is needed.

### 2.2 MCP II

The production version of the MCP is **MCP II**, which supports:
- Full multiprogramming (concurrent multi-language execution)
- Automatic interpreter switching between programs
- Dynamic memory allocation
- Virtual memory (disk-backed)
- Up to 10 I/O controls with device management

---

## 3. Multiprogramming

### 3.1 Concept

The B1700 can run multiple programs written in different languages concurrently.
For example, a COBOL payroll program, a FORTRAN engineering calculation, and an
RPG report generator can all be in memory and share the processor.

### 3.2 Context Switching

When the MCP decides to switch between programs:

1. **Save current context**: Store all registers, scratchpad, FA, FB, etc.
   to the program's save area in memory
2. **Check interpreter**: Does the next program need a different interpreter?
3. **Switch interpreter** (if needed): Load the new interpreter (see Gismo below)
4. **Restore context**: Load the new program's saved registers and scratchpad
5. **Resume execution**: Branch to the interpreter entry point

The actual context switch is managed by the SDL interpreter executing MCP code,
which uses standard micro-operators (1C moves, 2C scratchpad operations, 7C
memory reads/writes) to save and restore state.

### 3.3 Interpreter Coexistence

In a multiprogramming mix:
- **B1710**: All interpreters coexist in S-Memory. Each interpreter's microcode
  occupies a separate memory region. Switching interpreters only requires
  changing MAR(A) to point to the new interpreter's code.
  
- **B1720**: The active interpreter occupies M-Memory. When switching to a
  different language, the new interpreter must be copied from S-Memory into
  M-Memory (or it may already be there if M-Memory is large enough).

---

## 4. Gismo (Interpreter Dispatcher)

**Gismo** is the MCP component responsible for interpreter management:

### 4.1 Functions

- Track which interpreters are loaded and where
- Load interpreters from disk to memory on demand
- Manage M-Memory allocation (B1720)
- Handle interpreter switching during context switches

### 4.2 M-Memory Management Strategies (B1720)

Gismo uses adaptive strategies based on M-Memory pressure:

| Strategy | M-Memory Usage | Behavior |
|----------|---------------|----------|
| **Abundant** | <20% used | All active interpreters kept in M-Memory |
| **Ample** | 20–40% used | Most frequently used interpreters in M-Memory |
| **Adequate** | 40–60% used | Current interpreter + MCP interpreter in M-Memory |
| **Precious** | 60–80% used | Only hot paths of current interpreter |
| **Bare** | >80% used | Only MCP essential microcode; programs run from S-Memory |

### 4.3 Interpreter Loading

When a program is first dispatched:
1. Gismo checks if the required interpreter is available
2. If not in memory, the interpreter microcode is loaded from disk
3. On B1720, the interpreter is copied into M-Memory
4. Control passes to the interpreter

---

## 5. Memory Management

### 5.1 Program Structure in Memory

A running program occupies S-Memory as follows:

```
┌────────────────────────────────┐
│ Interpreter Microcode          │  (micro-operators)
│ (may be in M-Memory on B1720) │
├────────────────────────────────┤
│ S-Language Object Code         │  (program instructions)
├────────────────────────────────┤
│ User Data Area                 │  (variables, buffers)
├────────────────────────────────┤
│ Stack / Working Storage        │
└────────────────────────────────┘
     ▲                    ▲
     BR                  BR+LR
  (Base)               (Limit)
```

### 5.2 Memory Protection

- **BR** (Base Register): Points to the start of the program's allocated memory
- **LR** (Limit Register): Defines the size of the allocated region
- Software (interpreter + MCP) enforces that all memory accesses fall within
  [BR, BR+LR)

The hardware does NOT automatically trap on out-of-bounds accesses. The
interpreter must explicitly compare addresses against BR and LR and handle
violations in software.

### 5.3 Location Independence

All S-code and interpreter microcode is **position-independent**:
- S-code uses FA-relative addressing (absolute bit addresses rebased through BR)
- Interpreter microcode uses PC-relative branches (12C/13C/14C/15C)
- Scratchpad provides per-program register save area

This allows the MCP to relocate programs freely in memory.

### 5.4 Virtual Memory

The MCP implements virtual memory through:
- Disk-backed segments that are paged in/out of S-Memory
- Page tables maintained in MCP's own data structures
- On-demand loading of S-code segments
- All managed through software (SDL code) executing as micro-operators

---

## 6. Job Scheduling

The MCP schedules jobs based on:
- Priority levels (system vs. user)
- I/O wait state (programs blocked on I/O are suspended)
- Interpreter availability
- Memory availability
- Timer interrupts (100 ms real-time clock drives preemptive scheduling)

The scheduling algorithm runs as MCP code (SDL) whenever:
- A timer interrupt occurs
- An I/O operation completes
- A program requests an OS service
- A program terminates

---

## 7. Software Stack

The complete B1700 software stack:

```
┌─────────────────────────────┐
│    User Application          │  (COBOL, FORTRAN, RPG, etc.)
├─────────────────────────────┤
│    Compiler                  │  (generates S-code)
├─────────────────────────────┤
│    S-Language                │  (virtual machine code)
├─────────────────────────────┤
│    Interpreter               │  (microprogram interpreting S-code)
├─────────────────────────────┤
│    MCP II                    │  (operating system in SDL)
├─────────────────────────────┤
│    SDL Interpreter           │  (interprets MCP's own S-SDL code)
├─────────────────────────────┤
│    Micro-operators           │  (hardware)
└─────────────────────────────┘
```

---

## 8. Emulator Implications

### 8.1 Bootstrapping

To run the MCP on an emulator:

1. **Bootstrap from cassette**: Load the initial microcode bootstrap from tape
2. **Bootstrap loads SDL interpreter**: The cassette microcode loads the SDL
   interpreter into memory
3. **SDL interpreter loads MCP**: MCP S-SDL code is loaded from disk
4. **MCP initializes**: Sets up memory management, I/O, job scheduling
5. **MCP loads user programs**: As submitted by the operator

### 8.2 Minimum Viable System

For initial emulator testing, a minimal system would be:
1. Micro-operator execution engine
2. Memory (S-Memory, at least 32K bytes)
3. Console I/O (for operator commands)
4. Cassette tape (for bootstrap)
5. Disk I/O (for MCP and program loading)
6. SDL interpreter microcode
7. MCP II software

### 8.3 Progressive Testing

Without the full MCP, testing can proceed by:
1. Hand-crafting micro-operator sequences and running them directly
2. Loading just the SDL interpreter and feeding it simple S-SDL programs
3. Loading diagnostic microcode (available on cassette tapes from Burroughs)
4. Eventually loading the full MCP

---

*Previous: [06 — Interpreter & S-Language](06_INTERPRETERS.md) | Next: [08 — I/O System](08_IO_SYSTEM.md)*
