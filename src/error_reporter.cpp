// NexiaCompiler v2.0 — Error Reporter
// Ported from ErrorReporter.cs

#include "error_reporter.h"
#include <algorithm>

namespace nexia {

void ErrorReporter::load_source(const std::string& fileName, const std::string& source) {
    auto& lines = source_cache_[fileName];
    lines.clear();
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line))
        lines.push_back(line);
}

void ErrorReporter::report_error(const std::string& message, const std::string& fileName,
                                  int line, int column, int length) {
    error_count_++;
    auto diag = Diagnostic(DiagnosticLevel::Error, message, fileName,
                            line, column, get_source_line(fileName, line), length);
    diagnostics_.push_back(diag);
    print_diagnostic(diag);
}

void ErrorReporter::report_warning(const std::string& message, const std::string& fileName,
                                    int line, int column, int length) {
    warning_count_++;
    auto diag = Diagnostic(DiagnosticLevel::Warning, message, fileName,
                            line, column, get_source_line(fileName, line), length);
    diagnostics_.push_back(diag);
    print_diagnostic(diag);
}

void ErrorReporter::report_note(const std::string& message, const std::string& fileName,
                                 int line, int column, int length) {
    auto diag = Diagnostic(DiagnosticLevel::Note, message, fileName,
                            line, column, get_source_line(fileName, line), length);
    diagnostics_.push_back(diag);
    print_diagnostic(diag);
}

void ErrorReporter::report_fatal(const std::string& message, const std::string& fileName,
                                  int line, int column) {
    error_count_++;
    auto diag = Diagnostic(DiagnosticLevel::Fatal, message, fileName,
                            line, column, get_source_line(fileName, line));
    diagnostics_.push_back(diag);
    print_diagnostic(diag);
}

void ErrorReporter::print_summary() const {
    if (error_count_ == 0 && warning_count_ == 0) return;

    std::cerr << "\n";
    std::string parts;
    if (error_count_ > 0)
        parts += std::to_string(error_count_) + " error" + (error_count_ == 1 ? "" : "s");
    if (warning_count_ > 0) {
        if (!parts.empty()) parts += " and ";
        parts += std::to_string(warning_count_) + " warning" + (warning_count_ == 1 ? "" : "s");
    }
    std::cerr << parts << " generated.\n";
}

void ErrorReporter::print_diagnostic(const Diagnostic& diag) const {
    // Location prefix
    std::string location = diag.file_name + ":" + std::to_string(diag.line) +
                           ":" + std::to_string(diag.column) + ":";

    std::string levelStr;
    switch (diag.level) {
        case DiagnosticLevel::Error:
        case DiagnosticLevel::Fatal:
            levelStr = colorize(" error:", "\x1b[1;31m");
            break;
        case DiagnosticLevel::Warning:
            levelStr = colorize(" warning:", "\x1b[1;33m");
            break;
        case DiagnosticLevel::Note:
            levelStr = colorize(" note:", "\x1b[1;36m");
            break;
    }

    std::cerr << colorize(location, "\x1b[1;37m") << levelStr << " "
              << colorize(diag.message, "\x1b[1;37m") << "\n";

    if (!diag.source_line.empty()) {
        std::string lineNum = std::to_string(diag.line);
        while (lineNum.size() < 5) lineNum = " " + lineNum;
        std::string linePrefix = " " + lineNum + " | ";

        std::cerr << colorize(linePrefix, "\x1b[90m");
        print_highlighted_source_line(diag.source_line, diag.column, diag.length);
        std::cerr << "\n";

        std::string caretPrefix(linePrefix.size(), ' ');
        std::cerr << caretPrefix;
        print_caret(diag.column, diag.length, diag.level);
        std::cerr << "\n";
    }
}

void ErrorReporter::print_highlighted_source_line(const std::string& line, int column, int length) const {
    std::string clean = line;
    // Replace tabs with spaces
    for (auto& c : clean) if (c == '\t') c = ' ';

    if (column <= 0 || column > (int)clean.size()) {
        std::cerr << clean;
        return;
    }

    int start = column - 1;
    int end = std::min(start + length, (int)clean.size());

    std::cerr << clean.substr(0, start);
    std::cerr << colorize(clean.substr(start, end - start), "\x1b[1;37m");
    if (end < (int)clean.size())
        std::cerr << clean.substr(end);
}

void ErrorReporter::print_caret(int column, int length, DiagnosticLevel level) const {
    if (column <= 0) return;

    const char* color;
    switch (level) {
        case DiagnosticLevel::Error:
        case DiagnosticLevel::Fatal: color = "\x1b[1;31m"; break;
        case DiagnosticLevel::Warning: color = "\x1b[1;33m"; break;
        case DiagnosticLevel::Note: color = "\x1b[1;36m"; break;
        default: color = "\x1b[1;32m"; break;
    }

    std::cerr << std::string(column - 1, ' ');
    std::string caret = "^";
    if (length > 1) caret += std::string(length - 1, '~');
    std::cerr << colorize(caret, color);
}

std::string ErrorReporter::get_source_line(const std::string& fileName, int line) {
    if (line <= 0) return "";

    auto it = source_cache_.find(fileName);
    if (it != source_cache_.end()) {
        if (line <= (int)it->second.size())
            return it->second[line - 1];
    }

    // Try to read the file
    try {
        std::ifstream file(fileName);
        if (file) {
            auto& lines = source_cache_[fileName];
            std::string ln;
            while (std::getline(file, ln))
                lines.push_back(ln);
            if (line <= (int)lines.size())
                return lines[line - 1];
        }
    } catch (...) {}

    return "";
}

std::string ErrorReporter::colorize(const std::string& text, const char* code) const {
    if (!use_color_) return text;
    return std::string(code) + text + "\x1b[0m";
}

} // namespace nexia
