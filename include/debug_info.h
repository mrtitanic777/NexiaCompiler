#pragma once
// NexiaCompiler v2.0 — Debug Info Generator (DWARF)
// Ported from DebugInfo.cs

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace nexia {

struct DebugFunction {
    std::string name;
    uint32_t low_pc;
    uint32_t high_pc;
    std::string source_file;
    int line;
    std::vector<std::pair<std::string, std::string>> parameters; // (name, type)
};

struct DebugLineEntry {
    uint32_t address;
    std::string file;
    int line;
    int column;
};

struct DebugVariable {
    std::string name;
    std::string type;
    int stack_offset;
};

class DebugInfoGenerator {
public:
    void add_function(const DebugFunction& func);
    void add_line_entry(const DebugLineEntry& entry);
    void add_global_variable(const DebugVariable& var);

    std::vector<uint8_t> generate_debug_info(const std::string& sourceFile,
                                              const std::string& compDir);
    std::vector<uint8_t> generate_debug_line();
    std::vector<uint8_t> generate_debug_abbrev();
    std::vector<uint8_t> generate_debug_str();

private:
    std::vector<uint8_t> debug_info_;
    std::vector<uint8_t> debug_line_;
    std::vector<uint8_t> debug_abbrev_;
    std::vector<uint8_t> debug_str_;
    std::unordered_map<std::string, int> string_offsets_;

    std::vector<DebugFunction> functions_;
    std::vector<DebugLineEntry> line_entries_;
    std::vector<DebugVariable> global_variables_;

    int next_abbrev_code_ = 1;

    int add_string(const std::string& str);
    void emit_uleb128(std::vector<uint8_t>& buf, uint32_t value);
    void emit_sleb128(std::vector<uint8_t>& buf, int32_t value);
};

} // namespace nexia
