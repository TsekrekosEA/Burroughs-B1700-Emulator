# Burroughs B1700 Emulator Wiki

Welcome to the documentation wiki for the Burroughs B1700 Emulator, a cycle-level emulator of the world's first dynamically microprogrammed computer.

## Quick Links

- [Architecture Guide](Architecture): Deep-dive into B1700 hardware, bit-addressable memory, registers, micro-instruction encoding, decode tree, Function Box
- [Implementation Guide](Implementation): How the emulator was built, design decisions, C++ patterns, testing strategy
- [Interpreter Switching](Interpreters): S-CALC, S-FORT, and Gismo, the B1700's defining feature explained and demonstrated
- [Artifacts Catalog](Artifacts): Every tape image, MIL program, and S-language program with provenance and usage
- [Development Journey](Journey): The full story from scanned manuals to working interpreter switching

## Getting Started

```bash
git clone https://github.com/TsekrekosEA/Burroughs-B1700-Emulator.git
cd Burroughs-B1700-Emulator
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./b1700 --test
```

See the [README](https://github.com/TsekrekosEA/Burroughs-B1700-Emulator#readme) for full usage instructions.

## What Makes the B1700 Special?

The B1700 has no fixed instruction set. Instead of hardwiring instructions like ADD or JUMP, the CPU loads microcode interpreters from software. One for COBOL, another for FORTRAN, another for RPG. When the OS switches between programs, it replaces the CPU's entire instruction set. The same transistors become a "COBOL machine" then a "FORTRAN machine."

This emulator demonstrates this with two custom S-languages and Gismo-style interpreter switching.
