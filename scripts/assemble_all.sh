#!/bin/bash
# Assemble all Burroughs B1700 interpreters and demo programs.
# Run this from the build directory.

# Check if we are in the build directory by looking for the assemblers
if [ ! -f ./mil_asm ] || [ ! -f ./scalc_asm ] || [ ! -f ./sfort_asm ]; then
    echo "Error: Could not find assemblers. Please run this script from the build directory."
    # We use return instead of exit if sourced, but here it's a script.
    # To be safe for the environment, just let it end.
else

    echo "--- Assembling Interpreters ---"
    ./mil_asm ../artifacts/interpreters/s_calc_interp.mil s_calc.bin
    ./mil_asm ../artifacts/interpreters/s_fort_interp.mil s_fort.bin
    ./mil_asm ../artifacts/interpreters/s_fort_interp_gismo.mil s_fort_gismo.bin

    echo "--- Assembling Programs ---"
    ./mil_asm ../artifacts/cards/cold_start_loader.mil cold_start_loader.bin
    ./scalc_asm ../artifacts/programs/demo1.scalc demo1.bin
    ./scalc_asm ../artifacts/programs/gismo_calc.scalc calc.bin
    ./sfort_asm ../artifacts/programs/fibonacci.sfort fib.bin
    ./sfort_asm ../artifacts/programs/gismo_fib.sfort fib_gismo.bin

    echo "--- Assembly Complete ---"
    ls -lh *.bin
fi
