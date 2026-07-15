#pragma once
// NexiaCompiler v2.0 — Standard Library stubs
// Ported from StandardLibrary.cs

#include <cstdint>
#include "linker.h"
#include "ppc_instructions.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace nexia {

class StandardLibrary {
public:
    ObjectFile build();
    static std::vector<std::string> get_provided_functions();

private:
    std::vector<uint32_t> code_;
    std::vector<uint8_t> data_;
    std::unordered_map<std::string, int> function_offsets_;
    std::vector<LinkerSymbol> symbols_;
    std::unordered_map<std::string, std::string> stub_source_;

    int start_function(const std::string& name);
    void end_function(const std::string& name, int startOffset);
    void emit(uint32_t instruction);
    int add_string_data(const std::string& text);
    void emit_prologue(int frameSize = 64);
    void emit_epilogue(int frameSize = 64);

    void generate_memory_stubs();
    void generate_string_stubs();
    void generate_math_stubs();
    void generate_io_stubs();
    void generate_utility_stubs();
    void generate_cpp_runtime_stubs();
};

} // namespace nexia
