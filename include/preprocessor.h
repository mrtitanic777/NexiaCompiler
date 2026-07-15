#pragma once
// NexiaCompiler v2.0 — C/C++ Preprocessor
// Ported from Preprocessor.cs
//
// Handles: #include, #define (object/function-like), #undef,
//          #ifdef/#ifndef/#if/#else/#elif/#endif, #pragma, #error, #line
//          Macro expansion with ##, #, __VA_ARGS__

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <memory>

namespace nexia {

class PreprocessorError : public std::runtime_error {
public:
    std::string file_name;
    int line;

    PreprocessorError(const std::string& message, const std::string& fileName, int ln)
        : std::runtime_error("Preprocessor error in " + fileName + " at line " +
                             std::to_string(ln) + ": " + message),
          file_name(fileName), line(ln) {}
};

struct MacroDefinition {
    std::string name;
    std::vector<std::string> parameters; // empty for object-like
    bool is_function_like;
    std::string body;
    std::string file_name;
    int line;

    MacroDefinition() : is_function_like(false), line(0) {}
    MacroDefinition(std::string name, std::vector<std::string> params,
                    bool isFuncLike, std::string body,
                    std::string fileName, int ln)
        : name(std::move(name)), parameters(std::move(params)),
          is_function_like(isFuncLike), body(std::move(body)),
          file_name(std::move(fileName)), line(ln) {}
};

class Preprocessor {
public:
    explicit Preprocessor(std::vector<std::string> includePaths = {});

    void add_include_path(const std::string& path);
    void define(const std::string& name, const std::string& value = "1");
    void define_function_macro(const std::string& name, std::vector<std::string> params,
                               const std::string& body);

    std::string process(const std::string& source, const std::string& fileName);

    std::vector<std::string> warnings;

private:
    static constexpr int MAX_INCLUDE_DEPTH = 64;

    std::vector<std::string> include_paths_;
    std::unordered_map<std::string, MacroDefinition> macros_;
    std::unordered_set<std::string> included_files_;
    std::vector<std::string> pragmas_;

    void define_predefined();

    static std::string strip_comments(const std::string& source);

    std::string process_internal(const std::string& source, const std::string& fileName,
                                 int includeDepth);

    // Conditional directives
    struct CondState {
        bool is_active;
        bool has_been_true;
        bool else_allowed;
    };

    bool try_handle_conditional(const std::string& directive,
                                std::vector<CondState>& condStack,
                                const std::string& fileName, int line);

    void handle_directive(const std::string& directive, const std::string& fileName,
                          int line, int includeDepth, std::string& output);

    void handle_include(const std::string& argument, const std::string& fileName,
                        int line, int includeDepth, std::string& output);
    void handle_define(const std::string& argument, const std::string& fileName, int line);

    std::string resolve_include(const std::string& includePath,
                                const std::string& currentFile, bool isSystem) const;

    // Macro expansion
    std::string expand_macros(const std::string& text, const std::string& fileName,
                              int line, std::unordered_set<std::string>* expanding = nullptr);
    std::string expand_function_macro(const MacroDefinition& macro,
                                      const std::vector<std::string>& args);
    std::vector<std::string> parse_macro_arguments(const std::string& text,
                                                    size_t openParen, size_t& afterClose);

    // Constant expression evaluation
    bool evaluate_constant_expression(const std::string& expr,
                                      const std::string& fileName, int line);
    int64_t eval_simple_expr(const std::string& expr);
    int find_operator(const std::string& expr, const std::string& op);
    int find_operator_rtl(const std::string& expr, const std::string& ops);
    int find_matching_paren(const std::string& text, int openPos);

    // String/char literal helpers
    static size_t find_string_end(const std::string& text, size_t start);
    static size_t find_char_end(const std::string& text, size_t start);
    static bool is_inside_string(const std::string& text, size_t pos);
};

} // namespace nexia
