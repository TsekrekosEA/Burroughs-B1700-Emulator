#pragma once
// Burroughs B1700 Emulator — Interactive Debugger
// Provides a CLI debugger with:
//   - Single-step, run, run-to-address
//   - Breakpoints (by address, opcode, condition, cycle count)
//   - Register inspection and modification
//   - Memory viewer (hex dump)
//   - Disassembly view
//   - Execution trace with filtering
//   - Scratchpad and stack display
//   - Watch expressions (break/display on register/memory change)

#include "processor.h"
#include "disasm.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <csignal>
#include <optional>
#include <fstream>
#include <iostream>

namespace b1700 {

// ══════════════════════════════════════════════════════════════════════════
// BREAKPOINT TYPES
// ══════════════════════════════════════════════════════════════════════════

enum class BreakType {
    ADDRESS,         // Break when MAR == addr
    OPCODE,          // Break when MC == opcode
    CYCLE,           // Break at cycle count
    REGISTER_EQ,     // Break when register == value
    REGISTER_CHANGE, // Break when register changes
    MEMORY_CHANGE,   // Break when memory location changes
    HALT             // Break on HALT
};

struct Breakpoint {
    int id;
    BreakType type;
    bool enabled = true;
    uint32_t address = 0;      // for ADDRESS type
    uint8_t opcode = 0;        // for OPCODE type
    uint64_t cycle_target = 0; // for CYCLE type
    std::string reg_name;      // for REGISTER types
    uint32_t reg_value = 0;    // for REGISTER_EQ
    uint32_t last_value = 0;   // for change detection
    uint32_t mem_addr = 0;     // for MEMORY_CHANGE
    uint32_t mem_len = 8;      // bits to watch
    int hit_count = 0;
};

// ══════════════════════════════════════════════════════════════════════════
// WATCH EXPRESSION
// ══════════════════════════════════════════════════════════════════════════

struct Watch {
    int id;
    enum Type { REG, MEM } type;
    std::string reg_name;    // for REG type
    uint32_t mem_addr = 0;   // for MEM type
    uint32_t mem_len = 24;   // bits for MEM
    uint32_t last_value = 0;
    bool break_on_change = false;
};

// ══════════════════════════════════════════════════════════════════════════
// TRACE FILTER
// ══════════════════════════════════════════════════════════════════════════

struct TraceFilter {
    bool enabled = false;
    bool branches_only = false;
    bool memory_only = false;
    bool io_only = false;
    std::string output_file; // empty = stdout
    FILE* trace_fp = nullptr;
};

// ══════════════════════════════════════════════════════════════════════════
// CTRL-C HANDLER
// ══════════════════════════════════════════════════════════════════════════

static volatile sig_atomic_t g_interrupted = 0;

static void signal_handler(int /*sig*/) {
    g_interrupted = 1;
}

// ══════════════════════════════════════════════════════════════════════════
// DEBUGGER
// ══════════════════════════════════════════════════════════════════════════

class Debugger {
public:
    Processor& cpu;

    explicit Debugger(Processor& cpu_ref) : cpu(cpu_ref) {}

    ~Debugger() {
        if (trace_.trace_fp && trace_.trace_fp != stdout) {
            std::fclose(trace_.trace_fp);
        }
    }

    // ── Main debugger REPL ───────────────────────────────────────────────
    void run() {
        // Install signal handler for Ctrl-C
        struct sigaction sa{};
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, &old_sa_);

        print_banner();
        show_current();

        std::string line;
        while (true) {
            std::printf("b1700> ");
            std::fflush(stdout);

            if (!std::getline(std::cin, line)) {
                std::printf("\n");
                break;
            }

            line = trim_str(line);
            if (line.empty()) {
                // Repeat last command
                line = last_cmd_;
            }
            last_cmd_ = line;

            if (!dispatch(line)) break;
        }

        // Restore signal handler
        sigaction(SIGINT, &old_sa_, nullptr);
    }

private:
    std::vector<Breakpoint> breakpoints_;
    std::vector<Watch> watches_;
    TraceFilter trace_;
    int next_bp_id_ = 1;
    int next_watch_id_ = 1;
    std::string last_cmd_;
    struct sigaction old_sa_{};

    // ── Command Dispatcher ───────────────────────────────────────────────
    bool dispatch(const std::string& line) {
        auto tokens = tokenize(line);
        if (tokens.empty()) return true;

        const auto& cmd = tokens[0];

        // Single-character shortcuts
        if (cmd == "s" || cmd == "step")       return cmd_step(tokens);
        if (cmd == "r" || cmd == "run")        return cmd_run(tokens);
        if (cmd == "n" || cmd == "next")       return cmd_step(tokens); // alias
        if (cmd == "c" || cmd == "continue")   return cmd_run(tokens);
        if (cmd == "q" || cmd == "quit" || cmd == "exit") return false;
        if (cmd == "h" || cmd == "help" || cmd == "?")    { cmd_help(); return true; }

        // Register commands
        if (cmd == "regs" || cmd == "reg" || cmd == "registers") { cmd_regs(tokens); return true; }
        if (cmd == "set")  { cmd_set(tokens); return true; }

        // Memory commands
        if (cmd == "mem" || cmd == "memory" || cmd == "x") { cmd_mem(tokens); return true; }
        if (cmd == "wmem" || cmd == "wm")    { cmd_wmem(tokens); return true; }

        // Disassembly
        if (cmd == "d" || cmd == "disasm" || cmd == "dis" || cmd == "u") { cmd_disasm(tokens); return true; }

        // Breakpoints
        if (cmd == "b" || cmd == "break" || cmd == "bp")  { cmd_break(tokens); return true; }
        if (cmd == "bl" || cmd == "blist")    { cmd_blist(); return true; }
        if (cmd == "bd" || cmd == "bdelete")  { cmd_bdelete(tokens); return true; }
        if (cmd == "be" || cmd == "benable")  { cmd_benable(tokens, true); return true; }
        if (cmd == "bx" || cmd == "bdisable") { cmd_benable(tokens, false); return true; }

        // Watch
        if (cmd == "w" || cmd == "watch")    { cmd_watch(tokens); return true; }
        if (cmd == "wl" || cmd == "wlist")   { cmd_wlist(); return true; }
        if (cmd == "wd" || cmd == "wdelete") { cmd_wdelete(tokens); return true; }

        // Stack and scratchpad
        if (cmd == "stack" || cmd == "as")   { cmd_stack(); return true; }
        if (cmd == "scratch" || cmd == "sp") { cmd_scratch(); return true; }

        // I/O
        if (cmd == "io")    { cmd_io(); return true; }

        // Trace
        if (cmd == "trace" || cmd == "t") { cmd_trace(tokens); return true; }

        // Info
        if (cmd == "info" || cmd == "i") { cmd_info(tokens); return true; }

        // Reset
        if (cmd == "reset") { cmd_reset(); return true; }

        // Run to address
        if (cmd == "until" || cmd == "g") { cmd_until(tokens); return true; }

        std::printf("Unknown command: %s  (type 'help' for commands)\n", cmd.c_str());
        return true;
    }

    // ══════════════════════════════════════════════════════════════════════
    // EXECUTION COMMANDS
    // ══════════════════════════════════════════════════════════════════════

    bool cmd_step(const std::vector<std::string>& tokens) {
        int count = 1;
        if (tokens.size() > 1) count = parse_int(tokens[1], 1);

        for (int i = 0; i < count && !cpu.regs.halted; ++i) {
            execute_one_step();
            if (check_breakpoints()) break;
        }
        show_current();
        show_watches();
        return true;
    }

    bool cmd_run(const std::vector<std::string>& tokens) {
        uint64_t max_cycles = 10'000'000;
        if (tokens.size() > 1) max_cycles = parse_int(tokens[1], 10'000'000);

        g_interrupted = 0;
        uint64_t start = cpu.cycles;

        std::printf("Running (max %lu cycles, Ctrl-C to break)...\n",
                    (unsigned long)max_cycles);

        while (!cpu.regs.halted && (cpu.cycles - start) < max_cycles) {
            if (g_interrupted) {
                std::printf("\n--- Interrupted at cycle %lu ---\n",
                            (unsigned long)cpu.cycles);
                break;
            }
            execute_one_step();
            if (check_breakpoints()) break;
        }

        if (cpu.regs.halted) {
            std::printf("--- HALTED at cycle %lu ---\n",
                        (unsigned long)cpu.cycles);
        } else if (!g_interrupted && (cpu.cycles - start) >= max_cycles) {
            std::printf("--- Timeout after %lu cycles ---\n",
                        (unsigned long)(cpu.cycles - start));
        }

        show_current();
        show_watches();
        return true;
    }

    bool cmd_until(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2) {
            std::printf("Usage: until <address>\n");
            return true;
        }
        uint32_t target = parse_int(tokens[1], 0);

        g_interrupted = 0;
        std::printf("Running until MAR=0x%05X...\n", target);

        uint64_t limit = 10'000'000;
        uint64_t start = cpu.cycles;
        while (!cpu.regs.halted && (cpu.cycles - start) < limit) {
            if (g_interrupted) {
                std::printf("\n--- Interrupted ---\n");
                break;
            }
            execute_one_step();
            if (cpu.regs.MAR == target) {
                std::printf("--- Reached target 0x%05X ---\n", target);
                break;
            }
            if (check_breakpoints()) break;
        }
        show_current();
        return true;
    }

    // ══════════════════════════════════════════════════════════════════════
    // REGISTER COMMANDS
    // ══════════════════════════════════════════════════════════════════════

    void cmd_regs(const std::vector<std::string>& tokens) {
        if (tokens.size() > 1) {
            // Show specific register
            auto val = read_named_reg(tokens[1]);
            if (val.has_value()) {
                std::printf("  %s = 0x%06X (%u)\n", tokens[1].c_str(),
                            *val, *val);
            } else {
                std::printf("  Unknown register: %s\n", tokens[1].c_str());
            }
            return;
        }

        // Full register dump
        std::printf("╔════════════════════════════════════════════════════════╗\n");
        std::printf("║  REGISTERS                              Cycle: %-7lu║\n",
                    (unsigned long)cpu.cycles);
        std::printf("╠════════════════════════════════════════════════════════╣\n");
        std::printf("║  X=%06X  Y=%06X  T=%06X  L=%06X          ║\n",
                    cpu.regs.X & MASK_24, cpu.regs.Y & MASK_24,
                    cpu.regs.T & MASK_24, cpu.regs.L & MASK_24);
        std::printf("║  FA=%06X FB=%06X FL=%04X  FU=%X  CPL=%02X        ║\n",
                    cpu.regs.FA & MASK_24, cpu.regs.FB & MASK_24,
                    cpu.regs.FL(), cpu.regs.FU(), cpu.regs.CPL());
        std::printf("║  MAR=%05X M=%04X   BR=%06X  CC=%X  BICN=%X       ║\n",
                    cpu.regs.MAR, cpu.regs.M,
                    cpu.regs.BR & MASK_24, cpu.regs.CC & 0xF, cpu.regs.BICN & 0xF);
        std::printf("║  MAXS=%06X MAXM=%06X                              ║\n",
                    cpu.regs.MAXS, cpu.regs.MAXM);
        std::printf("║  TOPM=%06X   CYF=%X   CPU=%X   Halted=%s         ║\n",
                    cpu.regs.TOPM & MASK_24, cpu.regs.CYF(), cpu.regs.CPU(),
                    cpu.regs.halted ? "YES" : "no");
        std::printf("║  SUM=%06X CMPX=%06X CMPY=%06X DIFF=%06X    ║\n",
                    cpu.regs.read(3, 0) & MASK_24,
                    cpu.regs.read(3, 1) & MASK_24,
                    cpu.regs.read(3, 2) & MASK_24,
                    cpu.regs.read(8, 3) & MASK_24);
        std::printf("╚════════════════════════════════════════════════════════╝\n");
    }

    void cmd_set(const std::vector<std::string>& tokens) {
        if (tokens.size() < 3) {
            std::printf("Usage: set <register> <value>\n");
            std::printf("  Registers: X, Y, T, L, FA, FB, MAR, BR, CC, CPL, CYF\n");
            return;
        }
        std::string name = to_upper(tokens[1]);
        uint32_t val = parse_int(tokens[2], 0);

        if (write_named_reg(name, val)) {
            std::printf("  %s = 0x%06X\n", name.c_str(), val & MASK_24);
        } else {
            std::printf("  Cannot set register: %s\n", name.c_str());
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // MEMORY COMMANDS
    // ══════════════════════════════════════════════════════════════════════

    void cmd_mem(const std::vector<std::string>& tokens) {
        uint32_t addr = cpu.regs.FA;  // default: show at FA
        int bytes = 64;

        if (tokens.size() > 1) addr = parse_int(tokens[1], addr);
        if (tokens.size() > 2) bytes = parse_int(tokens[2], bytes);

        std::printf("Memory at bit address 0x%06X (%d bytes):\n", addr, bytes);

        for (int i = 0; i < bytes; i += 16) {
            std::printf("  %06X: ", addr + i * 8);
            for (int j = 0; j < 16 && (i + j) < bytes; ++j) {
                uint8_t b = cpu.mem.read_field(addr + (i + j) * 8, 8);
                std::printf("%02X ", b);
            }
            std::printf(" |");
            for (int j = 0; j < 16 && (i + j) < bytes; ++j) {
                uint8_t b = cpu.mem.read_field(addr + (i + j) * 8, 8);
                char c = (b >= 0x20 && b < 0x7f) ? static_cast<char>(b) : '.';
                std::printf("%c", c);
            }
            std::printf("|\n");
        }
    }

    void cmd_wmem(const std::vector<std::string>& tokens) {
        if (tokens.size() < 3) {
            std::printf("Usage: wmem <bit_addr> <value> [bits=8]\n");
            return;
        }
        uint32_t addr = parse_int(tokens[1], 0);
        uint32_t val = parse_int(tokens[2], 0);
        uint8_t bits = tokens.size() > 3 ? parse_int(tokens[3], 8) : 8;

        cpu.mem.write_field(addr, bits, val);
        std::printf("  Wrote 0x%X (%d bits) at 0x%06X\n", val, bits, addr);
    }

    // ══════════════════════════════════════════════════════════════════════
    // DISASSEMBLY
    // ══════════════════════════════════════════════════════════════════════

    void cmd_disasm(const std::vector<std::string>& tokens) {
        uint32_t addr = cpu.regs.MAR;
        int count = 16;

        if (tokens.size() > 1) addr = parse_int(tokens[1], addr);
        if (tokens.size() > 2) count = parse_int(tokens[2], count);

        std::printf("Disassembly at 0x%05X:\n", addr);
        std::string out = disassemble_range(cpu.mem, addr, count);
        std::printf("%s", out.c_str());
    }

    // ══════════════════════════════════════════════════════════════════════
    // BREAKPOINTS
    // ══════════════════════════════════════════════════════════════════════

    void cmd_break(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2) {
            std::printf("Usage:\n");
            std::printf("  break <address>          Break at MAR address\n");
            std::printf("  break opcode <MC>        Break on micro-opcode\n");
            std::printf("  break cycle <count>      Break at cycle count\n");
            std::printf("  break reg <name> <value> Break when reg==value\n");
            std::printf("  break change <name>      Break when reg changes\n");
            std::printf("  break halt               Break on HALT\n");
            return;
        }

        Breakpoint bp;
        bp.id = next_bp_id_++;

        if (tokens[1] == "opcode" && tokens.size() > 2) {
            bp.type = BreakType::OPCODE;
            bp.opcode = parse_int(tokens[2], 0);
            std::printf("  Breakpoint %d: break on opcode MC=%d\n", bp.id, bp.opcode);
        } else if (tokens[1] == "cycle" && tokens.size() > 2) {
            bp.type = BreakType::CYCLE;
            bp.cycle_target = parse_int(tokens[2], 0);
            std::printf("  Breakpoint %d: break at cycle %lu\n", bp.id,
                        (unsigned long)bp.cycle_target);
        } else if (tokens[1] == "reg" && tokens.size() > 3) {
            bp.type = BreakType::REGISTER_EQ;
            bp.reg_name = to_upper(tokens[2]);
            bp.reg_value = parse_int(tokens[3], 0);
            std::printf("  Breakpoint %d: break when %s == 0x%06X\n",
                        bp.id, bp.reg_name.c_str(), bp.reg_value);
        } else if (tokens[1] == "change" && tokens.size() > 2) {
            bp.type = BreakType::REGISTER_CHANGE;
            bp.reg_name = to_upper(tokens[2]);
            auto v = read_named_reg(bp.reg_name);
            bp.last_value = v.value_or(0);
            std::printf("  Breakpoint %d: break when %s changes (now=0x%06X)\n",
                        bp.id, bp.reg_name.c_str(), bp.last_value);
        } else if (tokens[1] == "halt") {
            bp.type = BreakType::HALT;
            std::printf("  Breakpoint %d: break on HALT\n", bp.id);
        } else {
            // Numeric = address breakpoint
            bp.type = BreakType::ADDRESS;
            bp.address = parse_int(tokens[1], 0);
            std::printf("  Breakpoint %d: break at MAR=0x%05X\n", bp.id, bp.address);
        }

        breakpoints_.push_back(bp);
    }

    void cmd_blist() {
        if (breakpoints_.empty()) {
            std::printf("  No breakpoints set.\n");
            return;
        }
        std::printf("  ID  Enabled  Type            Target            Hits\n");
        std::printf("  ──  ───────  ──────────────  ────────────────  ────\n");
        for (const auto& bp : breakpoints_) {
            char target[64];
            switch (bp.type) {
                case BreakType::ADDRESS:
                    snprintf(target, sizeof(target), "MAR=0x%05X", bp.address); break;
                case BreakType::OPCODE:
                    snprintf(target, sizeof(target), "MC=%d", bp.opcode); break;
                case BreakType::CYCLE:
                    snprintf(target, sizeof(target), "cycle=%lu", (unsigned long)bp.cycle_target); break;
                case BreakType::REGISTER_EQ:
                    snprintf(target, sizeof(target), "%s==0x%06X", bp.reg_name.c_str(), bp.reg_value); break;
                case BreakType::REGISTER_CHANGE:
                    snprintf(target, sizeof(target), "%s changes", bp.reg_name.c_str()); break;
                case BreakType::MEMORY_CHANGE:
                    snprintf(target, sizeof(target), "mem[0x%06X]", bp.mem_addr); break;
                case BreakType::HALT:
                    snprintf(target, sizeof(target), "HALT"); break;
            }
            std::printf("  %-3d %-8s %-15s %-17s %d\n",
                        bp.id, bp.enabled ? "yes" : "NO",
                        bp.type == BreakType::ADDRESS ? "address" :
                        bp.type == BreakType::OPCODE ? "opcode" :
                        bp.type == BreakType::CYCLE ? "cycle" :
                        bp.type == BreakType::REGISTER_EQ ? "reg-equal" :
                        bp.type == BreakType::REGISTER_CHANGE ? "reg-change" :
                        bp.type == BreakType::MEMORY_CHANGE ? "mem-change" : "halt",
                        target, bp.hit_count);
        }
    }

    void cmd_bdelete(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2) {
            std::printf("Usage: bdelete <id|all>\n");
            return;
        }
        if (tokens[1] == "all") {
            breakpoints_.clear();
            std::printf("  All breakpoints deleted.\n");
            return;
        }
        int id = parse_int(tokens[1], -1);
        auto it = std::remove_if(breakpoints_.begin(), breakpoints_.end(),
                                  [id](const Breakpoint& bp) { return bp.id == id; });
        if (it != breakpoints_.end()) {
            breakpoints_.erase(it, breakpoints_.end());
            std::printf("  Deleted breakpoint %d.\n", id);
        } else {
            std::printf("  Breakpoint %d not found.\n", id);
        }
    }

    void cmd_benable(const std::vector<std::string>& tokens, bool enable) {
        if (tokens.size() < 2) {
            std::printf("Usage: %s <id|all>\n", enable ? "benable" : "bdisable");
            return;
        }
        if (tokens[1] == "all") {
            for (auto& bp : breakpoints_) bp.enabled = enable;
            std::printf("  All breakpoints %s.\n", enable ? "enabled" : "disabled");
            return;
        }
        int id = parse_int(tokens[1], -1);
        for (auto& bp : breakpoints_) {
            if (bp.id == id) {
                bp.enabled = enable;
                std::printf("  Breakpoint %d %s.\n", id, enable ? "enabled" : "disabled");
                return;
            }
        }
        std::printf("  Breakpoint %d not found.\n", id);
    }

    // ══════════════════════════════════════════════════════════════════════
    // WATCH EXPRESSIONS
    // ══════════════════════════════════════════════════════════════════════

    void cmd_watch(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2) {
            std::printf("Usage:\n");
            std::printf("  watch <register>          Display register after each step\n");
            std::printf("  watch mem <addr> [bits]    Display memory after each step\n");
            std::printf("  watch break <register>     Break when register changes\n");
            return;
        }

        Watch w;
        w.id = next_watch_id_++;

        if (tokens[1] == "mem" && tokens.size() > 2) {
            w.type = Watch::MEM;
            w.mem_addr = parse_int(tokens[2], 0);
            w.mem_len = tokens.size() > 3 ? parse_int(tokens[3], 24) : 24;
            w.last_value = cpu.mem.read_field(w.mem_addr, w.mem_len);
            std::printf("  Watch %d: mem[0x%06X] (%d bits) = 0x%X\n",
                        w.id, w.mem_addr, w.mem_len, w.last_value);
        } else if (tokens[1] == "break" && tokens.size() > 2) {
            w.type = Watch::REG;
            w.reg_name = to_upper(tokens[2]);
            w.break_on_change = true;
            auto v = read_named_reg(w.reg_name);
            w.last_value = v.value_or(0);
            std::printf("  Watch %d: break when %s changes (now=0x%06X)\n",
                        w.id, w.reg_name.c_str(), w.last_value);
        } else {
            w.type = Watch::REG;
            w.reg_name = to_upper(tokens[1]);
            auto v = read_named_reg(w.reg_name);
            w.last_value = v.value_or(0);
            std::printf("  Watch %d: %s = 0x%06X\n",
                        w.id, w.reg_name.c_str(), w.last_value);
        }

        watches_.push_back(w);
    }

    void cmd_wlist() {
        if (watches_.empty()) {
            std::printf("  No watches set.\n");
            return;
        }
        for (const auto& w : watches_) {
            if (w.type == Watch::REG) {
                auto v = read_named_reg(w.reg_name);
                std::printf("  Watch %d: %s = 0x%06X%s\n",
                            w.id, w.reg_name.c_str(), v.value_or(0),
                            w.break_on_change ? " [break]" : "");
            } else {
                uint32_t v = cpu.mem.read_field(w.mem_addr, w.mem_len);
                std::printf("  Watch %d: mem[0x%06X] = 0x%X (%d bits)\n",
                            w.id, w.mem_addr, v, w.mem_len);
            }
        }
    }

    void cmd_wdelete(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2) {
            std::printf("Usage: wdelete <id|all>\n");
            return;
        }
        if (tokens[1] == "all") {
            watches_.clear();
            std::printf("  All watches deleted.\n");
            return;
        }
        int id = parse_int(tokens[1], -1);
        auto it = std::remove_if(watches_.begin(), watches_.end(),
                                  [id](const Watch& w) { return w.id == id; });
        if (it != watches_.end()) {
            watches_.erase(it, watches_.end());
            std::printf("  Deleted watch %d.\n", id);
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // STACK AND SCRATCHPAD
    // ══════════════════════════════════════════════════════════════════════

    void cmd_stack() {
        std::printf("A-Stack (depth=%d, SP=%d):\n",
                    cpu.regs.stack_depth, cpu.regs.stack_ptr);
        for (int i = 0; i < cpu.regs.stack_depth; ++i) {
            std::printf("  [%2d] %06X%s\n", i, cpu.regs.a_stack[i] & MASK_24,
                        i == cpu.regs.stack_ptr ? "  <-- SP" : "");
        }
    }

    void cmd_scratch() {
        std::printf("Scratchpad (16 × 48-bit words):\n");
        std::printf("  Pad   Left       Right\n");
        std::printf("  ───   ──────     ──────\n");
        for (int i = 0; i < 16; ++i) {
            uint32_t l = cpu.regs.scratchpad[i].left & MASK_24;
            uint32_t r = cpu.regs.scratchpad[i].right & MASK_24;
            if (l != 0 || r != 0) {
                std::printf("  [%2d]  %06X     %06X\n", i, l, r);
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // I/O BUS STATUS
    // ══════════════════════════════════════════════════════════════════════

    void cmd_io() {
        std::printf("I/O Bus Status:\n");
        std::printf("  Bus locked: %s\n", cpu.io.locked() ? "YES" : "no");
        std::printf("  BICN: 0x%X\n", cpu.io.get_bicn());
        std::printf("  Service requests: %s\n",
                    cpu.io.any_service_request() ? "YES" : "none");
        for (int i = 0; i < IOBus::MAX_CONTROLS; ++i) {
            auto* ctrl = cpu.io.get(i);
            if (ctrl) {
                std::printf("  Port %2d: %s [SR=%s]\n",
                            i, ctrl->name(),
                            ctrl->has_service_request() ? "PENDING" : "clear");
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // TRACE CONTROL
    // ══════════════════════════════════════════════════════════════════════

    void cmd_trace(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2) {
            std::printf("Trace: %s\n", trace_.enabled ? "ON" : "OFF");
            std::printf("  trace on            Enable full trace\n");
            std::printf("  trace off           Disable trace\n");
            std::printf("  trace file <path>   Trace to file\n");
            std::printf("  trace branches      Trace only branches\n");
            std::printf("  trace memory        Trace only memory ops\n");
            std::printf("  trace io            Trace only I/O ops\n");
            return;
        }
        if (tokens[1] == "on") {
            trace_.enabled = true;
            trace_.branches_only = false;
            trace_.memory_only = false;
            trace_.io_only = false;
            std::printf("  Trace enabled (full).\n");
        } else if (tokens[1] == "off") {
            trace_.enabled = false;
            if (trace_.trace_fp && trace_.trace_fp != stdout) {
                std::fclose(trace_.trace_fp);
                trace_.trace_fp = nullptr;
            }
            std::printf("  Trace disabled.\n");
        } else if (tokens[1] == "file" && tokens.size() > 2) {
            if (trace_.trace_fp && trace_.trace_fp != stdout) {
                std::fclose(trace_.trace_fp);
            }
            trace_.trace_fp = std::fopen(tokens[2].c_str(), "w");
            trace_.enabled = true;
            std::printf("  Tracing to %s\n", tokens[2].c_str());
        } else if (tokens[1] == "branches") {
            trace_.enabled = true;
            trace_.branches_only = true;
            std::printf("  Trace enabled (branches only).\n");
        } else if (tokens[1] == "memory") {
            trace_.enabled = true;
            trace_.memory_only = true;
            std::printf("  Trace enabled (memory ops only).\n");
        } else if (tokens[1] == "io") {
            trace_.enabled = true;
            trace_.io_only = true;
            std::printf("  Trace enabled (I/O ops only).\n");
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // INFO COMMAND
    // ══════════════════════════════════════════════════════════════════════

    void cmd_info(const std::vector<std::string>& tokens) {
        if (tokens.size() < 2 || tokens[1] == "cpu") {
            std::printf("CPU Status:\n");
            std::printf("  Cycle:  %lu\n", (unsigned long)cpu.cycles);
            std::printf("  Halted: %s\n", cpu.regs.halted ? "yes" : "no");
            std::printf("  MAR:    0x%05X (byte %d)\n", cpu.regs.MAR, cpu.regs.MAR / 8);
            std::printf("  M-mem:  %u bytes (%u bits)\n",
                        cpu.regs.MAXM / 8, cpu.regs.MAXM);
            return;
        }
        if (tokens[1] == "breaks" || tokens[1] == "bp") { cmd_blist(); return; }
        if (tokens[1] == "watches") { cmd_wlist(); return; }
        if (tokens[1] == "io") { cmd_io(); return; }
        if (tokens[1] == "stack") { cmd_stack(); return; }
    }

    // ══════════════════════════════════════════════════════════════════════
    // RESET
    // ══════════════════════════════════════════════════════════════════════

    void cmd_reset() {
        cpu.regs.MAR = 0;
        cpu.regs.halted = false;
        cpu.cycles = 0;
        std::printf("  CPU reset (MAR=0, halted=false, cycles=0).\n");
    }

    // ══════════════════════════════════════════════════════════════════════
    // HELP
    // ══════════════════════════════════════════════════════════════════════

    void cmd_help() {
        std::printf(
            "╔══════════════════════════════════════════════════════════════╗\n"
            "║  B1700 DEBUGGER COMMANDS                                    ║\n"
            "╠══════════════════════════════════════════════════════════════╣\n"
            "║  EXECUTION                                                  ║\n"
            "║    s [N]             Step N instructions (default 1)        ║\n"
            "║    r [max]           Run (with optional cycle limit)        ║\n"
            "║    until <addr>      Run until MAR reaches address          ║\n"
            "║    reset             Reset CPU state                        ║\n"
            "║                                                             ║\n"
            "║  REGISTERS                                                  ║\n"
            "║    regs              Show all registers                     ║\n"
            "║    regs <name>       Show one register                      ║\n"
            "║    set <reg> <val>   Modify register value                  ║\n"
            "║                                                             ║\n"
            "║  MEMORY                                                     ║\n"
            "║    mem [addr] [len]  Hex dump (bit address, byte count)     ║\n"
            "║    wmem <addr> <val> Write to memory                        ║\n"
            "║                                                             ║\n"
            "║  DISASSEMBLY                                                ║\n"
            "║    d [addr] [count]  Disassemble micro-instructions         ║\n"
            "║                                                             ║\n"
            "║  BREAKPOINTS                                                ║\n"
            "║    b <address>       Break at MAR address                   ║\n"
            "║    b opcode <MC>     Break on micro-opcode class            ║\n"
            "║    b cycle <N>       Break at cycle N                       ║\n"
            "║    b reg <r> <val>   Break when register equals value       ║\n"
            "║    b change <r>      Break when register changes            ║\n"
            "║    b halt            Break on HALT instruction              ║\n"
            "║    bl                List breakpoints                       ║\n"
            "║    bd <id|all>       Delete breakpoint(s)                   ║\n"
            "║    be/bx <id|all>    Enable/disable breakpoint(s)           ║\n"
            "║                                                             ║\n"
            "║  WATCHES                                                    ║\n"
            "║    w <register>      Watch register value                   ║\n"
            "║    w mem <addr>      Watch memory location                  ║\n"
            "║    w break <reg>     Break when register changes            ║\n"
            "║    wl                List watches                           ║\n"
            "║    wd <id|all>       Delete watch(es)                       ║\n"
            "║                                                             ║\n"
            "║  DISPLAY                                                    ║\n"
            "║    stack             Show A-stack contents                  ║\n"
            "║    scratch           Show scratchpad contents               ║\n"
            "║    io                Show I/O bus status                    ║\n"
            "║    info [topic]      CPU info, breaks, watches, io, stack   ║\n"
            "║                                                             ║\n"
            "║  TRACE                                                      ║\n"
            "║    trace on/off      Enable/disable execution trace         ║\n"
            "║    trace file <f>    Trace to file                          ║\n"
            "║    trace branches    Trace branches only                    ║\n"
            "║    trace memory      Trace memory ops only                  ║\n"
            "║    trace io          Trace I/O ops only                     ║\n"
            "║                                                             ║\n"
            "║  q                   Quit debugger                          ║\n"
            "║  <Enter>             Repeat last command                    ║\n"
            "╚══════════════════════════════════════════════════════════════╝\n"
        );
    }

    // ══════════════════════════════════════════════════════════════════════
    // INTERNAL HELPERS
    // ══════════════════════════════════════════════════════════════════════

    void execute_one_step() {
        // Save pre-step state for trace
        uint32_t pc = cpu.regs.MAR;
        uint64_t pre_cycles = cpu.cycles;

        cpu.step();

        // Emit trace if enabled
        if (trace_.enabled) {
            uint16_t micro = cpu.mem.fetch_micro(pc);
            uint16_t next_word = (pc + 16 < 0x80000) ? cpu.mem.fetch_micro(pc + 16) : 0;
            auto d = disassemble(micro, next_word);
            uint8_t mc = MicroFields{micro}.MC();

            bool should_trace = true;
            if (trace_.branches_only) {
                should_trace = (mc >= 12 && mc <= 15) || (mc == 4) || (mc == 5);
            }
            if (trace_.memory_only) {
                should_trace = (mc == 7);
            }
            if (trace_.io_only) {
                should_trace = (micro & 0xFF00) == 0x0100;  // DISPATCH
            }

            if (should_trace) {
                FILE* fp = trace_.trace_fp ? trace_.trace_fp : stdout;
                std::fprintf(fp, "  %08lu  %05X: %04X  %-35s  X=%06X Y=%06X T=%06X L=%06X FA=%06X\n",
                            (unsigned long)pre_cycles, pc, micro,
                            d.mnemonic.c_str(),
                            cpu.regs.X & MASK_24, cpu.regs.Y & MASK_24,
                            cpu.regs.T & MASK_24, cpu.regs.L & MASK_24,
                            cpu.regs.FA & MASK_24);
            }
        }
    }

    bool check_breakpoints() {
        for (auto& bp : breakpoints_) {
            if (!bp.enabled) continue;

            bool hit = false;
            switch (bp.type) {
                case BreakType::ADDRESS:
                    hit = (cpu.regs.MAR == bp.address);
                    break;
                case BreakType::OPCODE: {
                    uint16_t m = cpu.mem.fetch_micro(cpu.regs.MAR);
                    hit = (MicroFields{m}.MC() == bp.opcode);
                    break;
                }
                case BreakType::CYCLE:
                    hit = (cpu.cycles >= bp.cycle_target);
                    break;
                case BreakType::REGISTER_EQ: {
                    auto v = read_named_reg(bp.reg_name);
                    hit = v.has_value() && (*v == bp.reg_value);
                    break;
                }
                case BreakType::REGISTER_CHANGE: {
                    auto v = read_named_reg(bp.reg_name);
                    if (v.has_value() && *v != bp.last_value) {
                        hit = true;
                        bp.last_value = *v;
                    }
                    break;
                }
                case BreakType::MEMORY_CHANGE: {
                    uint32_t v = cpu.mem.read_field(bp.mem_addr, bp.mem_len);
                    if (v != bp.last_value) {
                        hit = true;
                        bp.last_value = v;
                    }
                    break;
                }
                case BreakType::HALT:
                    hit = cpu.regs.halted;
                    break;
            }

            if (hit) {
                bp.hit_count++;
                std::printf("*** Breakpoint %d hit", bp.id);
                switch (bp.type) {
                    case BreakType::ADDRESS:
                        std::printf(" (MAR=0x%05X)", bp.address); break;
                    case BreakType::OPCODE:
                        std::printf(" (opcode MC=%d)", bp.opcode); break;
                    case BreakType::CYCLE:
                        std::printf(" (cycle %lu)", (unsigned long)bp.cycle_target); break;
                    case BreakType::REGISTER_EQ:
                        std::printf(" (%s==0x%06X)", bp.reg_name.c_str(), bp.reg_value); break;
                    case BreakType::REGISTER_CHANGE:
                        std::printf(" (%s changed to 0x%06X)", bp.reg_name.c_str(), bp.last_value); break;
                    case BreakType::MEMORY_CHANGE:
                        std::printf(" (mem[0x%06X] changed)", bp.mem_addr); break;
                    case BreakType::HALT:
                        std::printf(" (HALT)"); break;
                }
                std::printf(" ***\n");
                return true;
            }
        }

        // Check watch break-on-change
        for (auto& w : watches_) {
            if (!w.break_on_change) continue;
            uint32_t cur;
            if (w.type == Watch::REG) {
                auto v = read_named_reg(w.reg_name);
                cur = v.value_or(0);
            } else {
                cur = cpu.mem.read_field(w.mem_addr, w.mem_len);
            }
            if (cur != w.last_value) {
                std::printf("*** Watch %d: %s changed 0x%06X → 0x%06X ***\n",
                            w.id, w.reg_name.c_str(), w.last_value, cur);
                w.last_value = cur;
                return true;
            }
        }

        return false;
    }

    void show_current() {
        uint16_t micro = cpu.mem.fetch_micro(cpu.regs.MAR);
        uint16_t next_word = cpu.mem.fetch_micro(cpu.regs.MAR + 16);
        auto d = disassemble(micro, next_word);

        std::printf("  [%lu] %05X: %04X  %s    X=%06X Y=%06X T=%06X FA=%06X\n",
                    (unsigned long)cpu.cycles,
                    cpu.regs.MAR, micro, d.mnemonic.c_str(),
                    cpu.regs.X & MASK_24, cpu.regs.Y & MASK_24,
                    cpu.regs.T & MASK_24, cpu.regs.FA & MASK_24);
    }

    void show_watches() {
        for (auto& w : watches_) {
            uint32_t cur;
            std::string name;
            if (w.type == Watch::REG) {
                auto v = read_named_reg(w.reg_name);
                cur = v.value_or(0);
                name = w.reg_name;
            } else {
                cur = cpu.mem.read_field(w.mem_addr, w.mem_len);
                char buf[32];
                snprintf(buf, sizeof(buf), "mem[0x%06X]", w.mem_addr);
                name = buf;
            }
            bool changed = (cur != w.last_value);
            if (changed) {
                std::printf("  *** %s: 0x%06X → 0x%06X\n", name.c_str(), w.last_value, cur);
            } else {
                std::printf("  %s = 0x%06X\n", name.c_str(), cur);
            }
            w.last_value = cur;
        }
    }

    void print_banner() {
        std::printf(
            "╔══════════════════════════════════════════════════════════════╗\n"
            "║           BURROUGHS B1700 INTERACTIVE DEBUGGER              ║\n"
            "║  Type 'help' for commands, 'q' to quit                     ║\n"
            "╚══════════════════════════════════════════════════════════════╝\n\n"
        );
    }

    // ── Named register read/write ────────────────────────────────────────

    std::optional<uint32_t> read_named_reg(const std::string& name_in) {
        std::string name = to_upper(name_in);
        if (name == "X")    return cpu.regs.X & MASK_24;
        if (name == "Y")    return cpu.regs.Y & MASK_24;
        if (name == "T")    return cpu.regs.T & MASK_24;
        if (name == "L")    return cpu.regs.L & MASK_24;
        if (name == "FA")   return cpu.regs.FA & MASK_24;
        if (name == "FB")   return cpu.regs.FB & MASK_24;
        if (name == "MAR")  return cpu.regs.MAR;
        if (name == "BR")   return cpu.regs.BR & MASK_24;
        if (name == "CC")   return (uint32_t)(cpu.regs.CC & 0xF);
        if (name == "BICN") return (uint32_t)(cpu.regs.BICN & 0xF);
        if (name == "M")    return (uint32_t)cpu.regs.M;
        if (name == "CPL")  return (uint32_t)cpu.regs.CPL();
        if (name == "FL")   return (uint32_t)cpu.regs.FL();
        if (name == "FU")   return (uint32_t)cpu.regs.FU();
        if (name == "CYF")  return (uint32_t)cpu.regs.CYF();
        if (name == "CPU_REG" || name == "CPU") return (uint32_t)cpu.regs.CPU();
        if (name == "MAXS") return cpu.regs.MAXS;
        if (name == "MAXM") return cpu.regs.MAXM;

        if (name == "TOPM") return cpu.regs.TOPM & MASK_24;
        if (name == "SUM")  return cpu.regs.read(3, 0) & MASK_24;
        if (name == "CMPX") return cpu.regs.read(3, 1) & MASK_24;
        if (name == "CMPY") return cpu.regs.read(3, 2) & MASK_24;
        if (name == "DIFF") return cpu.regs.read(8, 3) & MASK_24;
        return std::nullopt;
    }

    bool write_named_reg(const std::string& name, uint32_t val) {
        if (name == "X")    { cpu.regs.X = val & MASK_24; return true; }
        if (name == "Y")    { cpu.regs.Y = val & MASK_24; return true; }
        if (name == "T")    { cpu.regs.T = val & MASK_24; return true; }
        if (name == "L")    { cpu.regs.L = val & MASK_24; return true; }
        if (name == "FA")   { cpu.regs.FA = val & MASK_24; return true; }
        if (name == "FB")   { cpu.regs.FB = val & MASK_24; return true; }
        if (name == "MAR")  { cpu.regs.MAR = val & MASK_19; return true; }
        if (name == "BR")   { cpu.regs.BR = val & MASK_24; return true; }
        if (name == "CC")   { cpu.regs.CC = val & 0xF; return true; }
        if (name == "CPL")  { cpu.regs.set_CPL(val & 0x1F); return true; }
        if (name == "CYF")  { cpu.regs.set_CYF(val & 1); return true; }
        if (name == "TOPM") { cpu.regs.TOPM = val & MASK_24; return true; }
        return false;
    }

    // ── Utility ──────────────────────────────────────────────────────────

    static std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        return tokens;
    }

    static std::string trim_str(const std::string& s) {
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

    static uint32_t parse_int(const std::string& s, uint32_t def) {
        if (s.empty()) return def;
        try {
            if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
            return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
        } catch (...) {
            return def;
        }
    }
};

} // namespace b1700
