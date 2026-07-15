#pragma once
// NexiaCompiler v2.0 — Compiler driver
// Ported from Compiler.cs
// Pipeline: Preprocess -> Lex -> Parse -> Analyze -> Optimize -> CodeGen -> ObjectFile

#include "linker.h"
#include "optimizer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdint>

namespace nexia {

struct CompilerOptions {
    std::vector<std::string> include_paths;
    std::vector<std::string> lib_paths;     // .lib files to link
    std::vector<std::string> lib_dirs;      // -L library search directories
    std::unordered_map<std::string, std::string> defines;
    std::string output_file;
    bool verbose          = false;
    bool print_ast        = false;
    bool print_tokens     = false;
    bool preprocess_only  = false;
    bool compile_only     = false;
    OptimizationLevel opt_level = OptimizationLevel::O0;
    bool output_xex       = false;
    uint32_t xex_title_id = 0xFFFFFFFF;
    std::string xex_title_name = "NexiaCompiler Output";
    std::string gen_sdk_path;  // empty = don't generate
    bool generate_debug_info = false;
};

class Compiler {
public:
    explicit Compiler(CompilerOptions options);

    /// Compile a single source file. Returns nullptr on failure.
    std::unique_ptr<ObjectFile> compile_file(const std::string& sourceFile);

    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    CompilerOptions options_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
};

} // namespace nexia
