// Burroughs B1700 Emulator — SIMH .tap Tape Image Reader
// Enhanced with Burroughs labeled tape format parsing:
//   - VOL1/HDR1/EOF1/EOV1 ANSI/Burroughs tape labels
//   - TAPDIR directory record parsing
//   - Data record analysis (1812-byte blocks, 12-byte headers)
//   - File extraction by name
//   - Microcode vs. S-code vs. data classification
//
// SIMH .tap format:
//   Each record: [4-byte LE length] [data...] [4-byte LE length]
//   Tape mark:   0x00000000 (4 zero bytes)
//   End of medium: 0xFFFFFFFF
//   Error flag:  bit 31 set in length word
//
// Burroughs labeled tape format:
//   VOL1 — volume header: volume serial, owner
//   HDR1 — file header:   file name, section, sequence
//   Data records between HDR1 and EOF1
//   EOF1 — end of file marker
//   Double tape mark = end of volume

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
#include <map>
#include <set>
#include <optional>
#include <functional>

namespace fs = std::filesystem;

// ══════════════════════════════════════════════════════════════════════════
// EBCDIC TRANSLATION
// ══════════════════════════════════════════════════════════════════════════

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

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(' ');
    return s.substr(start, end - start + 1);
}

static void hex_dump_full(const uint8_t* data, size_t len, size_t offset = 0) {
    for (size_t i = 0; i < len; i += 16) {
        std::printf("  %06zX: ", offset + i);
        for (size_t j = 0; j < 16 && (i + j) < len; ++j) {
            std::printf("%02X ", data[i + j]);
        }
        for (size_t j = (i + 16 > len) ? len - i : 16; j < 16; ++j) {
            std::printf("   ");
        }
        std::printf(" |");
        for (size_t j = 0; j < 16 && (i + j) < len; ++j) {
            char c = ebcdic_to_ascii[data[i + j]];
            std::printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
        }
        std::printf("|\n");
    }
}

// ══════════════════════════════════════════════════════════════════════════
// SIMH TAPE FORMAT STRUCTURES
// ══════════════════════════════════════════════════════════════════════════

struct TapeRecord {
    uint32_t length;
    std::vector<uint8_t> data;
    bool error;
    uint64_t file_offset;
};

struct TapeFile {
    int file_number;
    std::vector<TapeRecord> records;
    uint64_t total_bytes;
};

// ══════════════════════════════════════════════════════════════════════════
// BURROUGHS LABELED TAPE FORMAT
// ══════════════════════════════════════════════════════════════════════════

enum class LabelType {
    VOL1, HDR1, HDR2, EOF1, EOF2, EOV1, UHL1, UTL1, UNKNOWN
};

struct TapeLabel {
    LabelType type;
    std::string raw_text;
    std::string volume_serial;
    std::string owner;
    std::string file_id;
    std::string file_set_id;
    int section_number = 0;
    int sequence_number = 0;
    std::string generation;
    std::string creation_date;
    std::string expiration_date;
    int block_count = 0;
};

enum class FileClass {
    LABEL, TAPDIR, MICROCODE, S_CODE, OBJECT_CODE, TEXT_DATA, BINARY_DATA, EMPTY
};

static const char* file_class_name(FileClass fc) {
    switch (fc) {
        case FileClass::LABEL:       return "LABEL";
        case FileClass::TAPDIR:      return "TAPDIR";
        case FileClass::MICROCODE:   return "MICROCODE";
        case FileClass::S_CODE:      return "S-CODE";
        case FileClass::OBJECT_CODE: return "OBJ-CODE";
        case FileClass::TEXT_DATA:   return "TEXT";
        case FileClass::BINARY_DATA: return "BINARY";
        case FileClass::EMPTY:       return "EMPTY";
    }
    return "???";
}

struct AnalyzedFile {
    int file_number;
    FileClass classification;
    std::string label_name;
    std::string description;
    std::vector<TapeRecord>* records;
    uint64_t total_bytes;
    std::optional<TapeLabel> hdr_label;
    std::optional<TapeLabel> eof_label;
};

// ══════════════════════════════════════════════════════════════════════════
// SIMH TAPE READER
// ══════════════════════════════════════════════════════════════════════════

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

    f.seekg(0, std::ios::end);
    auto file_size = f.tellg();
    f.seekg(0);

    std::vector<TapeFile> files;
    TapeFile current;
    current.file_number = 1;
    current.total_bytes = 0;

    int consecutive_marks = 0;
    uint64_t total_records = 0;
    uint64_t total_errors = 0;

    while (true) {
        uint64_t pos = f.tellg();
        uint32_t header;
        if (!read_u32le(f, header)) break;

        if (header == 0xFFFFFFFF) {
            if (verbose) std::printf("  [End of Medium at offset 0x%lX]\n", (unsigned long)pos);
            break;
        }

        if (header == 0x00000000) {
            consecutive_marks++;
            if (!current.records.empty()) {
                files.push_back(std::move(current));
                current = TapeFile{};
                current.file_number = static_cast<int>(files.size()) + 1;
                current.total_bytes = 0;
            }
            if (consecutive_marks >= 2) {
                if (verbose) std::printf("  [Double tape mark at offset 0x%lX]\n",
                                          (unsigned long)pos);
                break;
            }
            continue;
        }

        consecutive_marks = 0;

        bool error = (header & 0x80000000) != 0;
        uint32_t rec_len = header & 0x00FFFFFF;

        TapeRecord rec;
        rec.length = rec_len;
        rec.error = error;
        rec.file_offset = pos;
        rec.data.resize(rec_len);

        if (rec_len > 0) {
            if (!f.read(reinterpret_cast<char*>(rec.data.data()), rec_len)) {
                if (verbose)
                    std::fprintf(stderr, "  Warning: truncated record at offset 0x%lX "
                                 "(expected %u bytes)\n", (unsigned long)pos, rec_len);
                break;
            }
            if (rec_len & 1) {
                char pad;
                f.read(&pad, 1);
            }
        }

        uint32_t trailer;
        if (!read_u32le(f, trailer)) {
            if (verbose)
                std::fprintf(stderr, "  Warning: missing trailer at offset 0x%lX\n",
                             (unsigned long)pos);
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

    std::printf("Tape: %s (%s)\n", path.c_str(),
                file_size > 1024*1024 ?
                    (std::to_string(static_cast<long long>(file_size) / (1024*1024)) + " MB").c_str() :
                    (std::to_string(static_cast<long long>(file_size) / 1024) + " KB").c_str());
    std::printf("  Files: %zu, Total records: %lu, Errors: %lu\n",
                files.size(), (unsigned long)total_records, (unsigned long)total_errors);

    return files;
}

// ══════════════════════════════════════════════════════════════════════════
// BURROUGHS LABEL PARSER
// ══════════════════════════════════════════════════════════════════════════

static LabelType identify_label(const uint8_t* data, size_t len) {
    if (len < 4) return LabelType::UNKNOWN;
    std::string tag = ebcdic_to_string(data, 4);
    if (tag == "VOL1") return LabelType::VOL1;
    if (tag == "HDR1") return LabelType::HDR1;
    if (tag == "HDR2") return LabelType::HDR2;
    if (tag == "EOF1") return LabelType::EOF1;
    if (tag == "EOF2") return LabelType::EOF2;
    if (tag == "EOV1") return LabelType::EOV1;
    if (tag == "UHL1") return LabelType::UHL1;
    if (tag == "UTL1") return LabelType::UTL1;
    return LabelType::UNKNOWN;
}

static TapeLabel parse_label(const uint8_t* data, size_t len) {
    TapeLabel label;
    label.raw_text = ebcdic_to_string(data, std::min(len, (size_t)80));
    label.type = identify_label(data, len);

    switch (label.type) {
        case LabelType::VOL1:
            if (len >= 10) label.volume_serial = trim(ebcdic_to_string(data + 4, 6));
            if (len >= 51) label.owner = trim(ebcdic_to_string(data + 37, 14));
            break;

        case LabelType::HDR1:
        case LabelType::EOF1:
            if (len >= 21) label.file_id = trim(ebcdic_to_string(data + 4, 17));
            if (len >= 27) label.file_set_id = trim(ebcdic_to_string(data + 21, 6));
            if (len >= 31) {
                std::string s = trim(ebcdic_to_string(data + 27, 4));
                try { label.section_number = std::stoi(s); } catch (...) {}
            }
            if (len >= 35) {
                std::string s = trim(ebcdic_to_string(data + 31, 4));
                try { label.sequence_number = std::stoi(s); } catch (...) {}
            }
            if (len >= 39) label.generation = trim(ebcdic_to_string(data + 35, 4));
            if (len >= 47) label.creation_date = trim(ebcdic_to_string(data + 41, 6));
            if (len >= 53) label.expiration_date = trim(ebcdic_to_string(data + 47, 6));
            if (len >= 60 && label.type == LabelType::EOF1) {
                std::string s = trim(ebcdic_to_string(data + 54, 6));
                try { label.block_count = std::stoi(s); } catch (...) {}
            }
            break;

        default:
            break;
    }
    return label;
}

// ══════════════════════════════════════════════════════════════════════════
// FILE CLASSIFICATION
// ══════════════════════════════════════════════════════════════════════════

static bool is_ebcdic_text(const uint8_t* data, size_t len) {
    if (len == 0) return false;
    int printable = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t b = data[i];
        if (b == 0x40 || (b >= 0x4B && b <= 0x7F) ||
            (b >= 0x81 && b <= 0xA9) || (b >= 0xC1 && b <= 0xE9) ||
            (b >= 0xF0 && b <= 0xF9) || b == 0x00) {
            printable++;
        }
    }
    return (static_cast<int>(printable * 100 / len)) > 70;
}

static FileClass classify_file(const TapeFile& tf) {
    if (tf.records.empty()) return FileClass::EMPTY;

    // Check if it's a label
    if (!tf.records.empty() && tf.records[0].length >= 4) {
        auto lt = identify_label(tf.records[0].data.data(), tf.records[0].length);
        if (lt != LabelType::UNKNOWN) return FileClass::LABEL;
    }

    // Check for TAPDIR
    if (!tf.records.empty() && tf.records[0].length >= 10) {
        std::string text = ebcdic_to_string(tf.records[0].data.data(),
                                             std::min<size_t>(tf.records[0].length, 80));
        if (text.find("TAPDIR") != std::string::npos ||
            text.find("DIRECTORY") != std::string::npos) {
            return FileClass::TAPDIR;
        }
    }

    // Check for Burroughs 1812-byte data blocks or common sizes
    for (const auto& r : tf.records) {
        if (r.length == 1812 || r.length == 1800 || r.length == 180 ||
            r.length == 360 || r.length == 900 || r.length == 1200) {
            return FileClass::OBJECT_CODE;
        }
    }

    // Check if text
    if (!tf.records.empty() && is_ebcdic_text(tf.records[0].data.data(),
                                                std::min<size_t>(tf.records[0].length, 256))) {
        return FileClass::TEXT_DATA;
    }

    return FileClass::BINARY_DATA;
}

// ══════════════════════════════════════════════════════════════════════════
// FULL TAPE ANALYSIS
// ══════════════════════════════════════════════════════════════════════════

struct TapeAnalysis {
    std::string tape_path;
    std::string volume_serial;
    std::string volume_owner;
    std::vector<TapeFile> files;
    std::vector<AnalyzedFile> analyzed;
    uint64_t total_bytes = 0;
    uint64_t total_records = 0;
    uint64_t total_errors = 0;
    std::map<std::string, int> name_index;
};

static TapeAnalysis analyze_tape(const std::string& path, bool verbose) {
    TapeAnalysis ta;
    ta.tape_path = path;

    ta.files = read_tape(path, verbose);

    std::optional<TapeLabel> pending_hdr;

    for (size_t i = 0; i < ta.files.size(); ++i) {
        auto& tf = ta.files[i];
        AnalyzedFile af;
        af.file_number = tf.file_number;
        af.records = &tf.records;
        af.total_bytes = tf.total_bytes;
        af.classification = classify_file(tf);

        ta.total_bytes += tf.total_bytes;
        ta.total_records += tf.records.size();
        for (const auto& r : tf.records) {
            if (r.error) ta.total_errors++;
        }

        if (af.classification == FileClass::LABEL && !tf.records.empty()) {
            TapeLabel label = parse_label(tf.records[0].data.data(),
                                           tf.records[0].data.size());
            switch (label.type) {
                case LabelType::VOL1:
                    ta.volume_serial = label.volume_serial;
                    ta.volume_owner = label.owner;
                    af.label_name = "VOL1";
                    af.description = "Volume: " + label.volume_serial +
                                     " Owner: " + label.owner;
                    break;
                case LabelType::HDR1:
                    pending_hdr = label;
                    af.label_name = "HDR1:" + label.file_id;
                    af.description = "Header: " + label.file_id +
                                     " seq=" + std::to_string(label.sequence_number);
                    af.hdr_label = label;
                    break;
                case LabelType::EOF1:
                    af.label_name = "EOF1";
                    af.description = "End of file" +
                        (label.block_count > 0 ?
                         " (" + std::to_string(label.block_count) + " blocks)" : "");
                    af.eof_label = label;
                    break;
                default:
                    af.label_name = label.raw_text.substr(0, 4);
                    af.description = "Label: " + label.raw_text.substr(0, 40);
                    break;
            }
        } else {
            if (pending_hdr.has_value()) {
                af.label_name = pending_hdr->file_id;
                af.hdr_label = pending_hdr;
                ta.name_index[pending_hdr->file_id] = static_cast<int>(ta.analyzed.size());
                pending_hdr.reset();
            }

            uint32_t min_rec = UINT32_MAX, max_rec = 0;
            for (const auto& r : tf.records) {
                min_rec = std::min(min_rec, r.length);
                max_rec = std::max(max_rec, r.length);
            }
            if (tf.records.empty()) { min_rec = 0; max_rec = 0; }

            char buf[128];
            switch (af.classification) {
                case FileClass::TAPDIR:
                    af.description = "Tape directory";
                    break;
                case FileClass::OBJECT_CODE:
                    snprintf(buf, sizeof(buf), "Object code (%zu recs, %u-%u bytes/rec)",
                             tf.records.size(), min_rec, max_rec);
                    af.description = buf;
                    break;
                case FileClass::TEXT_DATA:
                    snprintf(buf, sizeof(buf), "Text data (%zu recs, %u bytes/rec)",
                             tf.records.size(), min_rec);
                    af.description = buf;
                    break;
                case FileClass::BINARY_DATA:
                    snprintf(buf, sizeof(buf), "Binary data (%zu recs, %u-%u bytes)",
                             tf.records.size(), min_rec, max_rec);
                    af.description = buf;
                    break;
                case FileClass::EMPTY:
                    af.description = "Empty";
                    break;
                default:
                    break;
            }
        }

        ta.analyzed.push_back(std::move(af));
    }

    return ta;
}


// Backward-compatible simple list
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
            snprintf(rec_sizes, sizeof(rec_sizes), "%u-%u", min_rec, max_rec);

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

// ══════════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════════

static void show_usage(const char* prog) {
    std::printf("Burroughs B1700 Tape Image Reader\n\n");
    std::printf("Usage: %s [options] <tape.tap>\n\n", prog);
    std::printf("Display modes:\n");
    std::printf("  --list           Simple file listing\n");
    std::printf("  --analyze        Full Burroughs label analysis (default)\n");
    std::printf("  --data           Show only data files (skip labels)\n");
    std::printf("  --inspect <f#>   Detailed inspection of file #\n");
    std::printf("  --hex <f#>       Hex dump of file\n");
    std::printf("  --ebcdic <f#>    Print file as EBCDIC text\n");
    std::printf("\nExtraction:\n");
    std::printf("  --extract <f#>   Extract file as binary blob\n");
    std::printf("  --dump <dir>     Extract all records as individual files\n");
    std::printf("  --output <name>  Output filename for --extract\n");
    std::printf("\nOptions:\n");
    std::printf("  --verbose        Show detailed parsing information\n");
}

int main(int argc, char** argv) {
    if (argc < 2) { show_usage(argv[0]); return 1; }

    std::string tape_path;
    bool verbose = false;
    bool do_analyze = false;
    bool do_list = false;
    bool show_data_only = false;
    std::string dump_dir;
    std::string output_name;
    int hex_file = -1;
    int extract_file = -1;
    int ebcdic_file = -1;
    int inspect_file_num = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose")          { verbose = true; }
        else if (arg == "--list")        { do_list = true; }
        else if (arg == "--analyze")     { do_analyze = true; }
        else if (arg == "--data")        { show_data_only = true; do_analyze = true; }
        else if (arg == "--inspect" && i + 1 < argc)  { inspect_file_num = std::atoi(argv[++i]); }
        else if (arg == "--dump" && i + 1 < argc)     { dump_dir = argv[++i]; }
        else if (arg == "--hex"  && i + 1 < argc)     { hex_file = std::atoi(argv[++i]); }
        else if (arg == "--extract" && i + 1 < argc)  { extract_file = std::atoi(argv[++i]); }
        else if (arg == "--ebcdic" && i + 1 < argc)   { ebcdic_file = std::atoi(argv[++i]); }
        else if (arg == "--output" && i + 1 < argc)   { output_name = argv[++i]; }
        else if (arg == "--help" || arg == "-h")       { show_usage(argv[0]); return 0; }
        else if (arg[0] != '-')          { tape_path = arg; }
        else {
            std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            show_usage(argv[0]);
            return 1;
        }
    }

    if (tape_path.empty()) { show_usage(argv[0]); return 1; }

    // Default to --analyze
    if (!do_list && hex_file < 0 && extract_file < 0 && ebcdic_file < 0 &&
        inspect_file_num < 0 && dump_dir.empty() && !do_analyze) {
        do_analyze = true;
    }

    if (do_analyze || inspect_file_num > 0) {
        auto ta = analyze_tape(tape_path, verbose);

        if (inspect_file_num > 0) {
            if (inspect_file_num <= static_cast<int>(ta.files.size())) {
                inspect_file_detail(ta.files[inspect_file_num - 1],
                                     ta.analyzed[inspect_file_num - 1]);
            } else {
                std::fprintf(stderr, "File %d not found (tape has %zu files)\n",
                             inspect_file_num, ta.files.size());
            }
            return 0;
        }

        print_analysis_header(ta);

        if (show_data_only) {
            print_data_files_only(ta);
        } else {
            print_file_table(ta);
        }

        if (extract_file > 0 && extract_file <= static_cast<int>(ta.files.size())) {
            extract_file_by_number(ta.files[extract_file - 1], extract_file, output_name);
        }

        if (!dump_dir.empty()) {
            std::printf("\nDumping all records to: %s/\n", dump_dir.c_str());
            for (const auto& tf : ta.files) dump_file(tf, dump_dir);
            std::printf("Done.\n");
        }

    } else {
        auto files = read_tape(tape_path, verbose);
        if (files.empty()) {
            std::printf("  (no files found on tape)\n");
            return 0;
        }

        if (do_list) print_file_summary(files);

        if (!dump_dir.empty()) {
            std::printf("\nDumping all records to: %s/\n", dump_dir.c_str());
            for (const auto& tf : files) dump_file(tf, dump_dir);
            std::printf("Done.\n");
        }

        if (hex_file > 0 && hex_file <= static_cast<int>(files.size())) {
            const auto& tf = files[hex_file - 1];
            std::printf("\nHex dump of file %d (%zu records, %lu bytes):\n",
                        hex_file, tf.records.size(), (unsigned long)tf.total_bytes);
            for (size_t i = 0; i < std::min<size_t>(tf.records.size(), 5); ++i) {
                const auto& r = tf.records[i];
                std::printf("  Record %zu (%u bytes%s):\n",
                    i, r.length, r.error ? " ERROR" : "");
                hex_dump_full(r.data.data(), std::min<size_t>(r.data.size(), 128));
            }
        }

        if (extract_file > 0 && extract_file <= static_cast<int>(files.size())) {
            extract_file_by_number(files[extract_file - 1], extract_file, output_name);
        }

        if (ebcdic_file > 0 && ebcdic_file <= static_cast<int>(files.size())) {
            const auto& tf = files[ebcdic_file - 1];
            std::printf("\n=== File %d as EBCDIC text ===\n", ebcdic_file);
            for (const auto& r : tf.records) {
                if (!r.data.empty()) {
                    std::string text = ebcdic_to_string(r.data.data(), r.data.size());
                    for (size_t off = 0; off < text.size(); off += 80) {
                        size_t end = std::min(off + 80, text.size());
                        std::printf("%.*s\n", static_cast<int>(end - off),
                                    text.c_str() + off);
                    }
                }
            }
        }
    }

    return 0;
}
