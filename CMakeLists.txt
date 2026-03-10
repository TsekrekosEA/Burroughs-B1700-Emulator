cmake_minimum_required(VERSION 3.20)
project(b1700_emulator
    VERSION   0.1.0
    LANGUAGES CXX
    DESCRIPTION "Burroughs B1700 Emulator")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Core library (header-only for now) ────────────────────────────────────
add_library(b1700_core INTERFACE)
target_include_directories(b1700_core INTERFACE src/core)

# ── Main executable ───────────────────────────────────────────────────────
add_executable(b1700 src/main.cpp)
target_link_libraries(b1700 PRIVATE b1700_core)

# ── Tools ─────────────────────────────────────────────────────────────────
add_executable(tape_reader src/tools/tape_reader.cpp)
add_executable(mil_asm     src/tools/mil_asm.cpp)
add_executable(scalc_asm   src/tools/scalc_asm.cpp)
add_executable(sfort_asm   src/tools/sfort_asm.cpp)
# ── Tests ─────────────────────────────────────────────────────────────────
enable_testing()
add_executable(b1700_tests tests/test_main.cpp)
target_link_libraries(b1700_tests PRIVATE b1700_core)
add_test(NAME b1700_tests COMMAND b1700_tests)
