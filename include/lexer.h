#pragma once
// NexiaCompiler v2.0 — Lexer
// Tokenizes C++03 source code. Ported from Lexer.cs.
//
// Optimized for large preprocessed files (100K+ lines from XDK headers):
//   - Uses string_view over the source buffer (zero-copy)
//   - Keyword lookup via perfect hash (sorted array + binary search)
//   - Pre-reserves token vector capacity
//   - Inline character classification

#include "token.h"
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>

namespace nexia {

/// Thrown when the lexer encounters something it can't tokenize.
class LexerError : public std::runtime_error {
public:
    int line;
    int column;

    LexerError(const std::string& message, int ln, int col)
        : std::runtime_error("Lexer error at line " + std::to_string(ln) +
                             ", column " + std::to_string(col) + ": " + message),
          line(ln), column(col) {}
};

/// Tokenizes C++03 source code into a vector of tokens.
///
/// Usage:
///     Lexer lexer(source, "main.cpp");
///     auto tokens = lexer.tokenize();
///
class Lexer {
public:
    /// Construct a lexer over the given source text.
    /// The source string must outlive the lexer (we take a view into it).
    explicit Lexer(std::string_view source, std::string filename = "<unknown>");

    /// Process the entire source and return all tokens (including trailing EOF).
    std::vector<Token> tokenize();

private:
    std::string_view src_;
    std::string      filename_;
    size_t           pos_;
    int              line_;
    int              col_;

    // Character helpers (all inline for speed)
    char  current() const  { return pos_ < src_.size() ? src_[pos_] : '\0'; }
    char  peek(int offset = 1) const {
        size_t i = pos_ + offset;
        return i < src_.size() ? src_[i] : '\0';
    }
    bool  at_end() const   { return pos_ >= src_.size(); }
    char  advance();
    bool  match(char expected);

    // Scanning
    void  skip_whitespace_and_comments();
    Token read_number();
    Token read_string();
    Token read_char_literal();
    Token read_identifier_or_keyword();
    Token read_preprocessor();
    Token read_operator_or_punctuation();

    // Keyword lookup
    static TokenType lookup_keyword(std::string_view text);
};

} // namespace nexia
