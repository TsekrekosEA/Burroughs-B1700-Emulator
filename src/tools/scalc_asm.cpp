// Burroughs B1700 — S-CALC Assembler
// Translates S-CALC source (stack-based calculator language) to binary S-code.
//
// Syntax:
//   PUSH <value>      Push a 24-bit integer literal
//   ADD               Pop two, push sum
//   SUB               Pop two, push difference (second - top)
//   MUL               Pop two, push product
//   PRINT             Pop one, output to console
//   DUP               Duplicate top of stack
//   SWAP              Swap top two elements
//   HALT              Stop execution
//   ; comment         Lines starting with ; are comments
//
// Output: binary file with S-CALC opcodes.
//   PUSH: 0x01 followed by 3 bytes (24-bit big-endian)
//   ADD:  0x02
//   SUB:  0x03
//   MUL:  0x04
//   PRINT: 0x05
//   DUP:  0x06
//   SWAP: 0x07
//   HALT: 0x00

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string to_upper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

static void print_usage() {
    std::puts("S-CALC Assembler — Burroughs B1700");
    std::puts("Usage: scalc_asm <source.scalc> [-o <output.bin>] [-v]");
    std::puts("");
    std::puts("Instructions: PUSH <n>, ADD, SUB, MUL, PRINT, DUP, SWAP, HALT");
}

int main(int argc, char* argv[]) {
    const char* input_file = nullptr;
    std::string output_file;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (std::strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        print_usage();
        return 1;
    }

    // Default output: replace extension with .bin
    if (output_file.empty()) {
        output_file = input_file;
        auto dot = output_file.rfind('.');
        if (dot != std::string::npos)
            output_file = output_file.substr(0, dot);
        output_file += ".bin";
    }

    std::ifstream in(input_file);
    if (!in) {
        std::fprintf(stderr, "Error: cannot open %s\n", input_file);
        return 1;
    }

    std::vector<uint8_t> code;
    int line_num = 0;
    int errors = 0;
    std::string line;

    while (std::getline(in, line)) {
        line_num++;
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '*')
            continue;

        // Strip inline comments
        auto comment_pos = line.find(';');
        if (comment_pos != std::string::npos)
            line = trim(line.substr(0, comment_pos));
        if (line.empty()) continue;

        // Tokenize
        std::istringstream iss(line);
        std::string opcode_str;
        iss >> opcode_str;
        opcode_str = to_upper(opcode_str);

        if (opcode_str == "HALT") {
            code.push_back(0x00);
            if (verbose) std::printf("  %04zX: HALT\n", code.size() - 1);
        }
        else if (opcode_str == "PUSH") {
            std::string val_str;
            iss >> val_str;
            if (val_str.empty()) {
                std::fprintf(stderr, "Error line %d: PUSH requires a value\n", line_num);
                errors++;
                continue;
            }

            uint32_t val = 0;
            std::string vs = to_upper(val_str);
            if (vs.size() > 1 && vs[0] == 'H') {
                val = static_cast<uint32_t>(std::stoul(vs.substr(1), nullptr, 16));
            } else if (vs.size() > 2 && vs[0] == '0' && vs[1] == 'X') {
                val = static_cast<uint32_t>(std::stoul(vs.substr(2), nullptr, 16));
            } else {
                val = static_cast<uint32_t>(std::stoul(vs, nullptr, 0));
            }
            val &= 0xFFFFFF; // 24-bit

            size_t pos = code.size();
            code.push_back(0x01);
            code.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
            code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            code.push_back(static_cast<uint8_t>(val & 0xFF));
            if (verbose) std::printf("  %04zX: PUSH %u (0x%06X)\n", pos, val, val);
        }
        else if (opcode_str == "ADD") {
            code.push_back(0x02);
            if (verbose) std::printf("  %04zX: ADD\n", code.size() - 1);
        }
        else if (opcode_str == "SUB") {
            code.push_back(0x03);
            if (verbose) std::printf("  %04zX: SUB\n", code.size() - 1);
        }
        else if (opcode_str == "MUL") {
            code.push_back(0x04);
            if (verbose) std::printf("  %04zX: MUL\n", code.size() - 1);
        }
        else if (opcode_str == "PRINT") {
            code.push_back(0x05);
            if (verbose) std::printf("  %04zX: PRINT\n", code.size() - 1);
        }
        else if (opcode_str == "DUP") {
            code.push_back(0x06);
            if (verbose) std::printf("  %04zX: DUP\n", code.size() - 1);
        }
        else if (opcode_str == "SWAP") {
            code.push_back(0x07);
            if (verbose) std::printf("  %04zX: SWAP\n", code.size() - 1);
        }
        else {
            std::fprintf(stderr, "Error line %d: unknown instruction '%s'\n",
                          line_num, opcode_str.c_str());
            errors++;
        }
    }

    if (errors > 0) {
        std::fprintf(stderr, "%d error(s)\n", errors);
        return 1;
    }

    // Write binary output
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "Error: cannot create %s\n", output_file.c_str());
        return 1;
    }
    out.write(reinterpret_cast<const char*>(code.data()), code.size());

    std::printf("S-CALC: %zu bytes assembled from %s → %s\n",
                code.size(), input_file, output_file.c_str());

    return 0;
}
