// Burroughs B1700 Emulator — SIMH .tap Tape Image Reader
// Reads SIMH-format magnetic tape images and extracts file records.
//
// SIMH .tap format:
//   Each record: [4-byte LE length] [data...] [4-byte LE length]
//   Tape mark:   0x00000000 (4 zero bytes)
//   End of medium: 0xFFFFFFFF
//   Error flag:  bit 31 set in length word
//
// Files are separated by tape marks. Double tape mark = end of data.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

struct TapeRecord {
    uint32_t length;
    std::vector<uint8_t> data;
    bool error;    // medium error flag (bit 31)
};

struct TapeFile {
    int file_number;
    std::vector<TapeRecord> records;
    uint64_t total_bytes;
};

// EBCDIC to ASCII translation table
static const char ebcdic_to_ascii[256] = {
    /* 00 */ '\0', '\x01', '\x02', '\x03', ' ',  '\t', ' ',  '\x7f',
    /* 08 */ ' ',  ' ',    ' ',    '\x0b', '\f', '\r', ' ',  ' ',
    /* 10 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  '\x08', ' ',
    /* 18 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 20 */ ' ',  ' ',    ' ',    ' ',    ' ',  '\n', ' ',  '\x1b',
    /* 28 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 30 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 38 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 40 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 48 */ ' ',  ' ',    ' ',    '.',    '<',  '(',  '+',  '|',
    /* 50 */ '&',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 58 */ ' ',  ' ',    '!',    '$',    '*',  ')',  ';',  ' ',
    /* 60 */ '-',  '/',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 68 */ ' ',  ' ',    ' ',    ',',    '%',  '_',  '>',  '?',
    /* 70 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 78 */ ' ',  '`',    ':',    '#',    '@',  '\'', '=',  '"',
    /* 80 */ ' ',  'a',    'b',    'c',    'd',  'e',  'f',  'g',
    /* 88 */ 'h',  'i',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* 90 */ ' ',  'j',    'k',    'l',    'm',  'n',  'o',  'p',
    /* 98 */ 'q',  'r',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* A0 */ ' ',  '~',    's',    't',    'u',  'v',  'w',  'x',
    /* A8 */ 'y',  'z',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* B0 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* B8 */ ' ',  ' ',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* C0 */ '{',  'A',    'B',    'C',    'D',  'E',  'F',  'G',
    /* C8 */ 'H',  'I',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* D0 */ '}',  'J',    'K',    'L',    'M',  'N',  'O',  'P',
    /* D8 */ 'Q',  'R',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* E0 */ '\\', ' ',    'S',    'T',    'U',  'V',  'W',  'X',
    /* E8 */ 'Y',  'Z',    ' ',    ' ',    ' ',  ' ',  ' ',  ' ',
    /* F0 */ '0',  '1',    '2',    '3',    '4',  '5',  '6',  '7',
    /* F8 */ '8',  '9',    ' ',    ' ',    ' ',  ' ',  ' ',  ' '
};

static std::string ebcdic_to_string(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        char c = ebcdic_to_ascii[data[i]];
        result += (c >= 0x20 && c < 0x7f) ? c : '.';
    }
    return result;
}

static std::string hex_dump(const uint8_t* data, size_t len, size_t max_bytes = 64) {
    std::ostringstream oss;
    size_t show = std::min(len, max_bytes);
    for (size_t i = 0; i < show; ++i) {
        if (i > 0 && i % 16 == 0) oss << "\n        ";
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", data[i]);
        oss << buf;
    }
    if (show < len) oss << "...";
    return oss.str();
}

static bool read_u32le(std::ifstream& f, uint32_t& val) {
    uint8_t buf[4];
    if (!f.read(reinterpret_cast<char*>(buf), 4)) return false;
    val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    return true;
}

static std::vector<TapeFile> read_tape(const std::string& path, bool verbose) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "Error: cannot open %s\n", path.c_str());
        return {};
    }

    std::vector<TapeFile> files;
    TapeFile current;
    current.file_number = 1;
    current.total_bytes = 0;

    int consecutive_marks = 0;
    uint64_t total_records = 0;
    uint64_t total_errors = 0;

    while (true) {
        uint32_t header;
        if (!read_u32le(f, header)) break;

        // End of medium
        if (header == 0xFFFFFFFF) {
            if (verbose) std::printf("  [End of Medium]\n");
            break;
        }

        // Tape mark
        if (header == 0x00000000) {
            consecutive_marks++;
            if (!current.records.empty()) {
                files.push_back(std::move(current));
                current = TapeFile{};
                current.file_number = static_cast<int>(files.size()) + 1;
                current.total_bytes = 0;
            }
            if (consecutive_marks >= 2) {
                if (verbose) std::printf("  [Double tape mark — end of data]\n");
                break;
            }
            continue;
        }

        consecutive_marks = 0;

        // Record
        bool error = (header & 0x80000000) != 0;
        uint32_t rec_len = header & 0x00FFFFFF;  // lower 24 bits

        TapeRecord rec;
        rec.length = rec_len;
        rec.error = error;
        rec.data.resize(rec_len);

        if (rec_len > 0) {
            if (!f.read(reinterpret_cast<char*>(rec.data.data()), rec_len)) {
                std::fprintf(stderr, "  Warning: truncated record (expected %u bytes)\n", rec_len);
                break;
            }
            // Skip padding byte if record length is odd
            if (rec_len & 1) {
                char pad;
                f.read(&pad, 1);
            }
        }

        // Read trailing length word
        uint32_t trailer;
        if (!read_u32le(f, trailer)) {
            std::fprintf(stderr, "  Warning: missing trailer word\n");
            break;
        }

        total_records++;
        if (error) total_errors++;

        current.records.push_back(std::move(rec));
        current.total_bytes += rec_len;
    }

    if (!current.records.empty()) {
        files.push_back(std::move(current));
    }

    std::printf("Tape: %s\n", path.c_str());
    std::printf("  Files: %zu, Total records: %lu, Errors: %lu\n",
                files.size(), (unsigned long)total_records, (unsigned long)total_errors);

    return files;
}

static void print_file_summary(const std::vector<TapeFile>& files) {
    std::printf("\n%-6s %-8s %-12s %-12s %s\n",
                "File#", "Records", "Bytes", "Rec Sizes", "First bytes (EBCDIC)");
    std::printf("%-6s %-8s %-12s %-12s %s\n",
                "-----", "-------", "----------", "-----------", "--------------------");

    for (const auto& tf : files) {
        uint32_t min_rec = UINT32_MAX, max_rec = 0;
        for (const auto& r : tf.records) {
            min_rec = std::min(min_rec, r.length);
            max_rec = std::max(max_rec, r.length);
        }
        if (tf.records.empty()) { min_rec = 0; max_rec = 0; }

        std::string first_text;
        if (!tf.records.empty() && !tf.records[0].data.empty()) {
            size_t show_len = std::min<size_t>(tf.records[0].data.size(), 40);
            first_text = ebcdic_to_string(tf.records[0].data.data(), show_len);
        }

        char rec_sizes[64];
        if (min_rec == max_rec)
            snprintf(rec_sizes, sizeof(rec_sizes), "%u", min_rec);
        else
            snprintf(rec_sizes, sizeof(rec_sizes), "%u–%u", min_rec, max_rec);

        std::printf("%-6d %-8zu %-12lu %-12s %.40s\n",
                    tf.file_number,
                    tf.records.size(),
                    (unsigned long)tf.total_bytes,
                    rec_sizes,
                    first_text.c_str());
    }
}

static void dump_file(const TapeFile& tf, const std::string& output_dir) {
    fs::create_directories(output_dir);
    for (size_t i = 0; i < tf.records.size(); ++i) {
        char fname[128];
        snprintf(fname, sizeof(fname), "file%03d_rec%04zu.bin",
                 tf.file_number, i);
        std::string path = output_dir + "/" + fname;
        std::ofstream out(path, std::ios::binary);
        if (out && !tf.records[i].data.empty()) {
            out.write(reinterpret_cast<const char*>(tf.records[i].data.data()),
                      tf.records[i].data.size());
        }
    }
}

static void show_usage(const char* prog) {
    std::printf("Usage: %s [options] <tape.tap>\n\n", prog);
    std::printf("Options:\n");
    std::printf("  --list           List all files on tape (default)\n");
    std::printf("  --verbose        Show detailed record information\n");
    std::printf("  --dump <dir>     Extract all records as binary files\n");
    std::printf("  --hex <file#>    Hex dump first record of file\n");
    std::printf("  --extract <f#>   Extract file as single concatenated blob\n");
    std::printf("  --ebcdic <f#>    Print file as EBCDIC->ASCII text\n");
}

int main(int argc, char** argv) {
    if (argc < 2) { show_usage(argv[0]); return 1; }

    std::string tape_path;
    bool verbose = false;
    bool do_list = true;
    std::string dump_dir;
    int hex_file = -1;
    int extract_file = -1;
    int ebcdic_file = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose")      { verbose = true; }
        else if (arg == "--list")    { do_list = true; }
        else if (arg == "--dump" && i + 1 < argc) { dump_dir = argv[++i]; }
        else if (arg == "--hex"  && i + 1 < argc) { hex_file = std::atoi(argv[++i]); }
        else if (arg == "--extract" && i + 1 < argc) { extract_file = std::atoi(argv[++i]); }
        else if (arg == "--ebcdic" && i + 1 < argc)  { ebcdic_file = std::atoi(argv[++i]); }
        else if (arg[0] != '-') { tape_path = arg; }
        else { std::fprintf(stderr, "Unknown option: %s\n", arg.c_str()); show_usage(argv[0]); return 1; }
    }

    if (tape_path.empty()) { show_usage(argv[0]); return 1; }

    auto files = read_tape(tape_path, verbose);
    if (files.empty()) {
        std::printf("  (no files found on tape)\n");
        return 0;
    }

    if (do_list) {
        print_file_summary(files);
    }

    if (!dump_dir.empty()) {
        std::printf("\nDumping all records to: %s/\n", dump_dir.c_str());
        for (const auto& tf : files) {
            dump_file(tf, dump_dir);
        }
        std::printf("Done.\n");
    }

    if (hex_file > 0 && hex_file <= static_cast<int>(files.size())) {
        const auto& tf = files[hex_file - 1];
        std::printf("\nHex dump of file %d (%zu records, %lu bytes):\n",
                    hex_file, tf.records.size(), (unsigned long)tf.total_bytes);
        for (size_t i = 0; i < std::min<size_t>(tf.records.size(), 5); ++i) {
            const auto& r = tf.records[i];
            std::printf("  Record %zu (%u bytes%s):\n    Hex: %s\n    Txt: %s\n",
                i, r.length, r.error ? " ERROR" : "",
                hex_dump(r.data.data(), r.data.size(), 48).c_str(),
                ebcdic_to_string(r.data.data(), std::min<size_t>(r.data.size(), 48)).c_str());
        }
    }

    if (extract_file > 0 && extract_file <= static_cast<int>(files.size())) {
        const auto& tf = files[extract_file - 1];
        std::string out_name = "file" + std::to_string(extract_file) + ".bin";
        std::ofstream out(out_name, std::ios::binary);
        for (const auto& r : tf.records) {
            if (!r.data.empty())
                out.write(reinterpret_cast<const char*>(r.data.data()), r.data.size());
        }
        std::printf("Extracted file %d → %s (%lu bytes)\n",
                extract_file, out_name.c_str(), (unsigned long)tf.total_bytes);
    }

    if (ebcdic_file > 0 && ebcdic_file <= static_cast<int>(files.size())) {
        const auto& tf = files[ebcdic_file - 1];
        std::printf("\n=== File %d as EBCDIC text ===\n", ebcdic_file);
        for (const auto& r : tf.records) {
            if (!r.data.empty()) {
                std::string text = ebcdic_to_string(r.data.data(), r.data.size());
                // Split into 80-char lines (Burroughs card image format)
                for (size_t off = 0; off < text.size(); off += 80) {
                    size_t end = std::min(off + 80, text.size());
                    std::printf("%.*s\n", static_cast<int>(end - off), text.c_str() + off);
                }
            }
        }
    }

    return 0;
}
