// Burroughs B1700 — S-FORT Assembler
// Translates S-FORT source (register-based FORTRAN-like) to binary S-code.
//
// Syntax:
//   HALT                   Stop execution
//   LOAD Rd, imm           Load 16-bit immediate into register
//   ADD Rd, Rs1, Rs2       Rd = Rs1 + Rs2
//   SUB Rd, Rs1, Rs2       Rd = Rs1 - Rs2
//   MUL Rd, Rs1, Rs2       Rd = Rs1 * Rs2
//   MOV Rd, Rs             Rd = Rs
//   CMP Rs1, Rs2           Compare Rs1 and Rs2 (sets flag)
//   BEQ label              Branch if equal
//   BNE label              Branch if not equal
//   PRINT Rs               Output register value
//   label:                 Define a label
//   ; comment              Comment
//
// Registers: R0, R1, R2, R3
//
// Branch targets are emitted as 16-bit bit offsets from S_START.

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
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

// Strip commas from a token
static std::string strip_comma(const std::string& s) {
    if (!s.empty() && s.back() == ',') return s.substr(0, s.size() - 1);
    return s;
}

// Parse register name (R0=0, R1=1, R2=2, R3=3)
static int parse_reg(const std::string& s) {
    std::string u = to_upper(strip_comma(s));
    if (u == "R0") return 0;
    if (u == "R1") return 1;
    if (u == "R2") return 2;
    if (u == "R3") return 3;
    return -1;
}

struct Instruction {
    int line_num;
    std::string opcode;
    std::vector<std::string> operands;
    size_t byte_pos;  // position in output
};

static void print_usage() {
    std::puts("S-FORT Assembler — Burroughs B1700");
    std::puts("Usage: sfort_asm <source.sfort> [-o <output.bin>] [-v]");
    std::puts("");
    std::puts("Instructions: HALT, LOAD, ADD, SUB, MUL, MOV, CMP, BEQ, BNE, PRINT");
    std::puts("Registers: R0, R1, R2, R3");
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

    if (!input_file) { print_usage(); return 1; }

    if (output_file.empty()) {
        output_file = input_file;
        auto dot = output_file.rfind('.');
        if (dot != std::string::npos) output_file = output_file.substr(0, dot);
        output_file += ".bin";
    }

    std::ifstream in(input_file);
    if (!in) {
        std::fprintf(stderr, "Error: cannot open %s\n", input_file);
        return 1;
    }

    // ── Pass 1: Parse instructions, collect labels ──────────────────────
    std::vector<Instruction> instructions;
    std::map<std::string, size_t> labels;  // label → byte position
    int errors = 0;
    int line_num = 0;

    // First pass: compute byte positions
    std::string line;
    size_t byte_pos = 0;

    while (std::getline(in, line)) {
        line_num++;
        line = trim(line);

        // Skip empty and comment lines
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '*')
            continue;

        // Strip inline comments
        auto cp = line.find(';');
        if (cp != std::string::npos) line = trim(line.substr(0, cp));
        if (line.empty()) continue;

        // Check for label (ends with ':')
        if (line.back() == ':') {
            std::string label = to_upper(trim(line.substr(0, line.size() - 1)));
            if (labels.count(label)) {
                std::fprintf(stderr, "Error line %d: duplicate label '%s'\n",
                             line_num, label.c_str());
                errors++;
            }
            labels[label] = byte_pos;
            if (verbose) std::printf("  Label: %s = byte 0x%04zX (bit 0x%04zX)\n",
                                     label.c_str(), byte_pos, byte_pos * 8);
            continue;
        }

        // Tokenize
        std::istringstream iss(line);
        std::string tok;
        std::vector<std::string> tokens;
        while (iss >> tok) tokens.push_back(tok);
        if (tokens.empty()) continue;

        std::string op = to_upper(tokens[0]);
        std::vector<std::string> ops(tokens.begin() + 1, tokens.end());

        Instruction inst{line_num, op, ops, byte_pos};
        instructions.push_back(inst);

        // Compute instruction size
        if (op == "HALT")       byte_pos += 1;
        else if (op == "LOAD")  byte_pos += 4;  // op + Rd + hi + lo
        else if (op == "ADD" || op == "SUB" || op == "MUL")
                                byte_pos += 4;  // op + Rd + Rs1 + Rs2
        else if (op == "MOV")   byte_pos += 3;  // op + Rd + Rs
        else if (op == "CMP")   byte_pos += 3;  // op + Rs1 + Rs2
        else if (op == "BEQ" || op == "BNE")
                                byte_pos += 3;  // op + target_hi + target_lo
        else if (op == "PRINT") byte_pos += 2;  // op + Rs
        else {
            std::fprintf(stderr, "Error line %d: unknown instruction '%s'\n",
                         line_num, op.c_str());
            errors++;
        }
    }

    if (errors > 0) {
        std::fprintf(stderr, "%d error(s)\n", errors);
        return 1;
    }

    // ── Pass 2: Emit binary ─────────────────────────────────────────────
    std::vector<uint8_t> code;

    for (auto& inst : instructions) {
        auto& op = inst.opcode;
        auto& ops = inst.operands;

        if (op == "HALT") {
            code.push_back(0x00);
            if (verbose) std::printf("  %04zX: HALT\n", inst.byte_pos);
        }
        else if (op == "LOAD") {
            // LOAD Rd, imm
            if (ops.size() < 2) {
                std::fprintf(stderr, "Error line %d: LOAD requires Rd, imm\n", inst.line_num);
                errors++; continue;
            }
            int rd = parse_reg(ops[0]);
            if (rd < 0) {
                std::fprintf(stderr, "Error line %d: bad register '%s'\n",
                             inst.line_num, ops[0].c_str());
                errors++; continue;
            }
            std::string vs = to_upper(strip_comma(ops[1]));
            uint32_t val = 0;
            if (vs.size() > 1 && vs[0] == 'H')
                val = static_cast<uint32_t>(std::stoul(vs.substr(1), nullptr, 16));
            else if (vs.size() > 2 && vs[0] == '0' && vs[1] == 'X')
                val = static_cast<uint32_t>(std::stoul(vs.substr(2), nullptr, 16));
            else
                val = static_cast<uint32_t>(std::stoul(vs, nullptr, 0));
            val &= 0xFFFF;

            code.push_back(0x01);
            code.push_back(static_cast<uint8_t>(rd));
            code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            code.push_back(static_cast<uint8_t>(val & 0xFF));
            if (verbose) std::printf("  %04zX: LOAD R%d, %u\n", inst.byte_pos, rd, val);
        }
        else if (op == "ADD" || op == "SUB" || op == "MUL") {
            // ADD/SUB/MUL Rd, Rs1, Rs2
            if (ops.size() < 3) {
                std::fprintf(stderr, "Error line %d: %s requires Rd, Rs1, Rs2\n",
                             inst.line_num, op.c_str());
                errors++; continue;
            }
            int rd = parse_reg(ops[0]);
            int rs1 = parse_reg(ops[1]);
            int rs2 = parse_reg(ops[2]);
            if (rd < 0 || rs1 < 0 || rs2 < 0) {
                std::fprintf(stderr, "Error line %d: bad register in %s\n",
                             inst.line_num, op.c_str());
                errors++; continue;
            }
            uint8_t opc = (op == "ADD") ? 0x02 : (op == "SUB") ? 0x03 : 0x04;
            code.push_back(opc);
            code.push_back(static_cast<uint8_t>(rd));
            code.push_back(static_cast<uint8_t>(rs1));
            code.push_back(static_cast<uint8_t>(rs2));
            if (verbose) std::printf("  %04zX: %s R%d, R%d, R%d\n",
                                     inst.byte_pos, op.c_str(), rd, rs1, rs2);
        }
        else if (op == "MOV") {
            if (ops.size() < 2) {
                std::fprintf(stderr, "Error line %d: MOV requires Rd, Rs\n", inst.line_num);
                errors++; continue;
            }
            int rd = parse_reg(ops[0]);
            int rs = parse_reg(ops[1]);
            if (rd < 0 || rs < 0) {
                std::fprintf(stderr, "Error line %d: bad register in MOV\n", inst.line_num);
                errors++; continue;
            }
            code.push_back(0x05);
            code.push_back(static_cast<uint8_t>(rd));
            code.push_back(static_cast<uint8_t>(rs));
            if (verbose) std::printf("  %04zX: MOV R%d, R%d\n", inst.byte_pos, rd, rs);
        }
        else if (op == "CMP") {
            if (ops.size() < 2) {
                std::fprintf(stderr, "Error line %d: CMP requires Rs1, Rs2\n", inst.line_num);
                errors++; continue;
            }
            int rs1 = parse_reg(ops[0]);
            int rs2 = parse_reg(ops[1]);
            if (rs1 < 0 || rs2 < 0) {
                std::fprintf(stderr, "Error line %d: bad register in CMP\n", inst.line_num);
                errors++; continue;
            }
            code.push_back(0x06);
            code.push_back(static_cast<uint8_t>(rs1));
            code.push_back(static_cast<uint8_t>(rs2));
            if (verbose) std::printf("  %04zX: CMP R%d, R%d\n", inst.byte_pos, rs1, rs2);
        }
        else if (op == "BEQ" || op == "BNE") {
            if (ops.size() < 1) {
                std::fprintf(stderr, "Error line %d: %s requires label\n",
                             inst.line_num, op.c_str());
                errors++; continue;
            }
            std::string label = to_upper(strip_comma(ops[0]));
            auto it = labels.find(label);
            if (it == labels.end()) {
                std::fprintf(stderr, "Error line %d: undefined label '%s'\n",
                             inst.line_num, label.c_str());
                errors++; continue;
            }
            // Target is bit offset from S_START
            uint16_t bit_offset = static_cast<uint16_t>(it->second * 8);

            uint8_t opc = (op == "BEQ") ? 0x07 : 0x08;
            code.push_back(opc);
            code.push_back(static_cast<uint8_t>((bit_offset >> 8) & 0xFF));
            code.push_back(static_cast<uint8_t>(bit_offset & 0xFF));
            if (verbose) std::printf("  %04zX: %s %s (bit 0x%04X)\n",
                                     inst.byte_pos, op.c_str(), label.c_str(), bit_offset);
        }
        else if (op == "PRINT") {
            if (ops.size() < 1) {
                std::fprintf(stderr, "Error line %d: PRINT requires Rs\n", inst.line_num);
                errors++; continue;
            }
            int rs = parse_reg(ops[0]);
            if (rs < 0) {
                std::fprintf(stderr, "Error line %d: bad register in PRINT\n", inst.line_num);
                errors++; continue;
            }
            code.push_back(0x09);
            code.push_back(static_cast<uint8_t>(rs));
            if (verbose) std::printf("  %04zX: PRINT R%d\n", inst.byte_pos, rs);
        }
    }

    if (errors > 0) {
        std::fprintf(stderr, "%d error(s)\n", errors);
        return 1;
    }

    // Write output
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "Error: cannot create %s\n", output_file.c_str());
        return 1;
    }
    out.write(reinterpret_cast<const char*>(code.data()), code.size());

    std::printf("S-FORT: %zu bytes assembled from %s → %s\n",
                code.size(), input_file, output_file.c_str());

    return 0;
}
