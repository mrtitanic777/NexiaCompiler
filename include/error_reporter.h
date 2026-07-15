#pragma once
// NexiaCompiler v2.0 — Error Reporter
// Ported from ErrorReporter.cs
// Clang/GCC-style diagnostics with source context and carets.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace nexia {

enum class DiagnosticLevel : uint8_t {
    Note,
    Warning,
    Error,
    Fatal
};

struct Diagnostic {
    DiagnosticLevel level;
    std::string message;
    std::string file_name;
    int line;
    int column;
    std::string source_line; // empty if unavailable
    int length;

    Diagnostic(DiagnosticLevel lvl, std::string msg, std::string fname,
               int ln, int col, std::string srcLine = "", int len = 1)
        : level(lvl), message(std::move(msg)), file_name(std::move(fname)),
          line(ln), column(col), source_line(std::move(srcLine)),
          length(std::max(len, 1)) {}
};

class ErrorReporter {
public:
    explicit ErrorReporter(bool useColor = true) : use_color_(useColor) {}

    void load_source(const std::string& fileName, const std::string& source);

    void report_error(const std::string& message, const std::string& fileName,
                      int line, int column, int length = 1);
    void report_warning(const std::string& message, const std::string& fileName,
                        int line, int column, int length = 1);
    void report_note(const std::string& message, const std::string& fileName,
                     int line, int column, int length = 1);
    void report_fatal(const std::string& message, const std::string& fileName,
                      int line, int column);

    void print_summary() const;

    int error_count() const { return error_count_; }
    int warning_count() const { return warning_count_; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

private:
    std::vector<Diagnostic> diagnostics_;
    std::unordered_map<std::string, std::vector<std::string>> source_cache_;
    bool use_color_;
    int error_count_ = 0;
    int warning_count_ = 0;

    void print_diagnostic(const Diagnostic& diag) const;
    void print_highlighted_source_line(const std::string& line, int column, int length) const;
    void print_caret(int column, int length, DiagnosticLevel level) const;
    std::string get_source_line(const std::string& fileName, int line);
    std::string colorize(const std::string& text, const char* code) const;
};

} // namespace nexia
