#include "lexer.h"
#include <algorithm>
#include <cstring>

namespace nexia {

// =====================================================================
// Inline character classification (avoid locale overhead of std::isdigit)
// =====================================================================

static inline bool is_digit(char c)    { return c >= '0' && c <= '9'; }
static inline bool is_hex(char c)      { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static inline bool is_alpha(char c)    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static inline bool is_alnum(char c)    { return is_alpha(c) || is_digit(c); }
static inline bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }

// =====================================================================
// Keyword lookup — sorted array with binary search
// =====================================================================

struct KeywordEntry {
    const char* text;
    TokenType   type;
};

// Must be sorted alphabetically for binary search
static constexpr KeywordEntry keywords[] = {
    {"auto",             TokenType::KW_Auto},
    {"bool",             TokenType::KW_Bool},
    {"break",            TokenType::KW_Break},
    {"case",             TokenType::KW_Case},
    {"catch",            TokenType::KW_Catch},
    {"char",             TokenType::KW_Char},
    {"class",            TokenType::KW_Class},
    {"const",            TokenType::KW_Const},
    {"const_cast",       TokenType::KW_ConstCast},
    {"continue",         TokenType::KW_Continue},
    {"default",          TokenType::KW_Default},
    {"delete",           TokenType::KW_Delete},
    {"do",               TokenType::KW_Do},
    {"double",           TokenType::KW_Double},
    {"dynamic_cast",     TokenType::KW_DynamicCast},
    {"else",             TokenType::KW_Else},
    {"enum",             TokenType::KW_Enum},
    {"explicit",         TokenType::KW_Explicit},
    {"extern",           TokenType::KW_Extern},
    {"false",            TokenType::KW_False},
    {"float",            TokenType::KW_Float},
    {"for",              TokenType::KW_For},
    {"friend",           TokenType::KW_Friend},
    {"goto",             TokenType::KW_Goto},
    {"if",               TokenType::KW_If},
    {"inline",           TokenType::KW_Inline},
    {"int",              TokenType::KW_Int},
    {"long",             TokenType::KW_Long},
    {"mutable",          TokenType::KW_Mutable},
    {"namespace",        TokenType::KW_Namespace},
    {"new",              TokenType::KW_New},
    {"operator",         TokenType::KW_Operator},
    {"private",          TokenType::KW_Private},
    {"protected",        TokenType::KW_Protected},
    {"public",           TokenType::KW_Public},
    {"register",         TokenType::KW_Register},
    {"reinterpret_cast", TokenType::KW_ReinterpretCast},
    {"return",           TokenType::KW_Return},
    {"short",            TokenType::KW_Short},
    {"signed",           TokenType::KW_Signed},
    {"sizeof",           TokenType::KW_Sizeof},
    {"static",           TokenType::KW_Static},
    {"static_cast",      TokenType::KW_StaticCast},
    {"struct",           TokenType::KW_Struct},
    {"switch",           TokenType::KW_Switch},
    {"template",         TokenType::KW_Template},
    {"this",             TokenType::KW_This},
    {"throw",            TokenType::KW_Throw},
    {"true",             TokenType::KW_True},
    {"try",              TokenType::KW_Try},
    {"typedef",          TokenType::KW_Typedef},
    {"typeid",           TokenType::KW_Typeid},
    {"typename",         TokenType::KW_Typename},
    {"union",            TokenType::KW_Union},
    {"unsigned",         TokenType::KW_Unsigned},
    {"using",            TokenType::KW_Using},
    {"virtual",          TokenType::KW_Virtual},
    {"void",             TokenType::KW_Void},
    {"volatile",         TokenType::KW_Volatile},
    {"wchar_t",          TokenType::KW_WcharT},
    {"while",            TokenType::KW_While},
};

static constexpr size_t num_keywords = sizeof(keywords) / sizeof(keywords[0]);

TokenType Lexer::lookup_keyword(std::string_view text) {
    // Binary search over the sorted keyword table
    int lo = 0, hi = static_cast<int>(num_keywords) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int cmp = text.compare(keywords[mid].text);
        if (cmp == 0) return keywords[mid].type;
        if (cmp < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return TokenType::Identifier; // Not a keyword
}

// =====================================================================
// Constructor
// =====================================================================

Lexer::Lexer(std::string_view source, std::string filename)
    : src_(source), filename_(std::move(filename)), pos_(0), line_(1), col_(1) {}

// =====================================================================
// Character helpers
// =====================================================================

char Lexer::advance() {
    char ch = src_[pos_];
    if (ch == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    pos_++;
    return ch;
}

bool Lexer::match(char expected) {
    if (current() == expected) {
        advance();
        return true;
    }
    return false;
}

// =====================================================================
// Whitespace and comment skipping
// =====================================================================

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < src_.size()) {
        char ch = current();

        // Whitespace
        if (is_whitespace(ch)) {
            advance();
            continue;
        }

        // Single-line comment: // ...
        if (ch == '/' && peek() == '/') {
            advance(); advance();
            while (!at_end() && current() != '\n')
                advance();
            continue;
        }

        // Multi-line comment: /* ... */
        if (ch == '/' && peek() == '*') {
            int start_line = line_;
            int start_col = col_;
            advance(); advance(); // skip /*
            while (pos_ < src_.size()) {
                if (current() == '*' && peek() == '/') {
                    advance(); advance(); // skip */
                    goto continue_outer;
                }
                advance();
            }
            throw LexerError("Unterminated block comment", start_line, start_col);
        continue_outer:
            continue;
        }

        // Stray */ outside of a block comment (from nested comments in XDK/CRT headers).
        if (ch == '*' && peek() == '/') {
            advance(); advance();
            continue;
        }

        break;
    }
}

// =====================================================================
// Number literals
// =====================================================================

Token Lexer::read_number() {
    int start_col = col_;
    int start_line = line_;
    std::string result;
    result.reserve(32);
    bool is_float = false;

    // Hex literal: 0x...
    if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
        result += advance(); // '0'
        result += advance(); // 'x'
        while (!at_end() && is_hex(current()))
            result += advance();
    } else {
        // Decimal digits
        while (!at_end() && is_digit(current()))
            result += advance();

        // Decimal point (but not ..)
        if (current() == '.' && peek() != '.') {
            is_float = true;
            result += advance(); // the '.'
            while (!at_end() && is_digit(current()))
                result += advance();
        }

        // Exponent (e.g. 1e10, 3.14e-2)
        if (current() == 'e' || current() == 'E') {
            is_float = true;
            result += advance();
            if (current() == '+' || current() == '-')
                result += advance();
            while (!at_end() && is_digit(current()))
                result += advance();
        }

        // Float suffix (f, F, l, L)
        if (current() == 'f' || current() == 'F' ||
            current() == 'l' || current() == 'L') {
            if (!is_float && (current() == 'f' || current() == 'F'))
                is_float = true;
            if (is_float) {
                result += advance();
            }
        }
    }

    // Integer suffixes: u, U, l, L, ul, ULL, etc.
    if (!is_float) {
        while (current() == 'u' || current() == 'U' ||
               current() == 'l' || current() == 'L')
            result += advance();

        // MSVC-specific integer suffixes: i64, i32, i16, i8
        if (current() == 'i' || current() == 'I') {
            char d1 = peek();
            if (d1 == '6' || d1 == '3' || d1 == '1' || d1 == '8') {
                advance(); // consume 'i'
                advance(); // consume first digit
                if (!at_end() && is_digit(current()))
                    advance(); // consume second digit (e.g., '4' in i64)
            }
        }
    }

    TokenType type = is_float ? TokenType::FloatLiteral : TokenType::IntegerLiteral;
    return Token(type, std::move(result), start_line, start_col);
}

// =====================================================================
// String and character literals
// =====================================================================

Token Lexer::read_string() {
    int start_col = col_;
    int start_line = line_;
    std::string result;
    result.reserve(64);
    advance(); // skip opening "

    while (!at_end() && current() != '"') {
        if (current() == '\\') {
            result += advance(); // backslash
            if (!at_end())
                result += advance(); // escaped character
        } else if (current() == '\n') {
            throw LexerError("Unterminated string literal", start_line, start_col);
        } else {
            result += advance();
        }
    }

    if (at_end())
        throw LexerError("Unterminated string literal", start_line, start_col);

    advance(); // skip closing "
    return Token(TokenType::StringLiteral, std::move(result), start_line, start_col);
}

Token Lexer::read_char_literal() {
    int start_col = col_;
    int start_line = line_;
    std::string result;
    advance(); // skip opening '

    if (current() == '\\') {
        result += advance(); // backslash
        if (!at_end())
            result += advance(); // escaped char
    } else if (!at_end() && current() != '\'') {
        result += advance();
    }

    if (current() != '\'')
        throw LexerError("Unterminated character literal", start_line, start_col);

    advance(); // skip closing '
    return Token(TokenType::CharLiteral, std::move(result), start_line, start_col);
}

// =====================================================================
// Identifiers and keywords
// =====================================================================

Token Lexer::read_identifier_or_keyword() {
    int start_col = col_;
    int start_line = line_;
    size_t start_pos = pos_;

    while (!at_end() && is_alnum(current()))
        advance();

    std::string_view text = src_.substr(start_pos, pos_ - start_pos);
    TokenType type = lookup_keyword(text);

    return Token(type, std::string(text), start_line, start_col);
}

// =====================================================================
// Preprocessor directives
// =====================================================================

Token Lexer::read_preprocessor() {
    int start_col = col_;
    int start_line = line_;
    std::string result;
    result.reserve(128);
    result += advance(); // the '#'

    while (!at_end()) {
        if (current() == '\\' && peek() == '\n') {
            result += advance(); // backslash
            result += advance(); // newline
            continue;
        }
        if (current() == '\n')
            break;
        result += advance();
    }

    // Trim trailing whitespace
    while (!result.empty() && is_whitespace(result.back()))
        result.pop_back();

    return Token(TokenType::Preprocessor, std::move(result), start_line, start_col);
}

// =====================================================================
// Operators and punctuation
// =====================================================================

Token Lexer::read_operator_or_punctuation() {
    int start_col = col_;
    int start_line = line_;
    char ch = current();

    // Three-character operators
    if (ch == '.' && peek() == '.' && peek(2) == '.') {
        advance(); advance(); advance();
        return Token(TokenType::Ellipsis, "...", start_line, start_col);
    }
    if (ch == '<' && peek() == '<' && peek(2) == '=') {
        advance(); advance(); advance();
        return Token(TokenType::OpLShiftAssign, "<<=", start_line, start_col);
    }
    if (ch == '>' && peek() == '>' && peek(2) == '=') {
        advance(); advance(); advance();
        return Token(TokenType::OpRShiftAssign, ">>=", start_line, start_col);
    }
    if (ch == '-' && peek() == '>' && peek(2) == '*') {
        advance(); advance(); advance();
        return Token(TokenType::OpArrowPointerToMember, "->*", start_line, start_col);
    }

    // Two-character operators
    char ch2 = peek();
    if (ch2 != '\0') {
        TokenType two_type = TokenType::Unknown;
        const char* two_str = nullptr;

        if      (ch == '=' && ch2 == '=') { two_type = TokenType::OpEqual;        two_str = "=="; }
        else if (ch == '!' && ch2 == '=') { two_type = TokenType::OpNotEqual;     two_str = "!="; }
        else if (ch == '<' && ch2 == '=') { two_type = TokenType::OpLessEqual;    two_str = "<="; }
        else if (ch == '>' && ch2 == '=') { two_type = TokenType::OpGreaterEqual; two_str = ">="; }
        else if (ch == '&' && ch2 == '&') { two_type = TokenType::OpAnd;          two_str = "&&"; }
        else if (ch == '|' && ch2 == '|') { two_type = TokenType::OpOr;           two_str = "||"; }
        else if (ch == '<' && ch2 == '<') { two_type = TokenType::OpLShift;       two_str = "<<"; }
        else if (ch == '>' && ch2 == '>') { two_type = TokenType::OpRShift;       two_str = ">>"; }
        else if (ch == '+' && ch2 == '=') { two_type = TokenType::OpPlusAssign;   two_str = "+="; }
        else if (ch == '-' && ch2 == '=') { two_type = TokenType::OpMinusAssign;  two_str = "-="; }
        else if (ch == '*' && ch2 == '=') { two_type = TokenType::OpStarAssign;   two_str = "*="; }
        else if (ch == '/' && ch2 == '=') { two_type = TokenType::OpSlashAssign;  two_str = "/="; }
        else if (ch == '%' && ch2 == '=') { two_type = TokenType::OpPercentAssign; two_str = "%="; }
        else if (ch == '&' && ch2 == '=') { two_type = TokenType::OpAndAssign;    two_str = "&="; }
        else if (ch == '|' && ch2 == '=') { two_type = TokenType::OpOrAssign;     two_str = "|="; }
        else if (ch == '^' && ch2 == '=') { two_type = TokenType::OpXorAssign;    two_str = "^="; }
        else if (ch == '+' && ch2 == '+') { two_type = TokenType::OpIncrement;    two_str = "++"; }
        else if (ch == '-' && ch2 == '-') { two_type = TokenType::OpDecrement;    two_str = "--"; }
        else if (ch == '-' && ch2 == '>') { two_type = TokenType::OpArrow;        two_str = "->"; }
        else if (ch == ':' && ch2 == ':') { two_type = TokenType::OpScope;        two_str = "::"; }
        else if (ch == '.' && ch2 == '*') { two_type = TokenType::OpPointerToMember; two_str = ".*"; }

        if (two_str) {
            advance(); advance();
            return Token(two_type, two_str, start_line, start_col);
        }
    }

    // Single-character operators and punctuation
    TokenType single_type = TokenType::Unknown;
    switch (ch) {
        case '+': single_type = TokenType::OpPlus;    break;
        case '-': single_type = TokenType::OpMinus;   break;
        case '*': single_type = TokenType::OpStar;    break;
        case '/': single_type = TokenType::OpSlash;   break;
        case '%': single_type = TokenType::OpPercent;  break;
        case '=': single_type = TokenType::OpAssign;   break;
        case '<': single_type = TokenType::OpLess;     break;
        case '>': single_type = TokenType::OpGreater;  break;
        case '!': single_type = TokenType::OpNot;      break;
        case '&': single_type = TokenType::OpBitAnd;   break;
        case '|': single_type = TokenType::OpBitOr;    break;
        case '^': single_type = TokenType::OpBitXor;   break;
        case '~': single_type = TokenType::OpBitNot;   break;
        case '?': single_type = TokenType::OpQuestion; break;
        case '.': single_type = TokenType::OpDot;      break;
        case '(': single_type = TokenType::LParen;     break;
        case ')': single_type = TokenType::RParen;     break;
        case '{': single_type = TokenType::LBrace;     break;
        case '}': single_type = TokenType::RBrace;     break;
        case '[': single_type = TokenType::LBracket;   break;
        case ']': single_type = TokenType::RBracket;   break;
        case ';': single_type = TokenType::Semicolon;  break;
        case ':': single_type = TokenType::Colon;      break;
        case ',': single_type = TokenType::Comma;      break;
        default: break;
    }

    advance();
    if (single_type != TokenType::Unknown) {
        return Token(single_type, std::string(1, ch), start_line, start_col);
    }

    return Token(TokenType::Unknown, std::string(1, ch), start_line, start_col);
}

// =====================================================================
// Main tokenize method
// =====================================================================

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    // Pre-reserve: rough estimate of ~1 token per 5 chars of source
    // For a 400KB preprocessed file this avoids many reallocs
    tokens.reserve(src_.size() / 5);

    while (pos_ < src_.size()) {
        skip_whitespace_and_comments();

        if (pos_ >= src_.size())
            break;

        char ch = current();

        // Preprocessor directive
        if (ch == '#') {
            tokens.push_back(read_preprocessor());
        }
        // Number literal
        else if (is_digit(ch)) {
            tokens.push_back(read_number());
        }
        // String literal (with adjacent string concatenation)
        else if (ch == '"') {
            Token str_tok = read_string();
            // C/C++ adjacent string literal concatenation: "abc" "def" => "abcdef"
            while (true) {
                size_t save_pos = pos_;
                int save_line = line_;
                int save_col = col_;
                skip_whitespace_and_comments();
                if (pos_ < src_.size() && current() == '"') {
                    Token next = read_string();
                    str_tok.value += next.value;
                } else {
                    pos_ = save_pos;
                    line_ = save_line;
                    col_ = save_col;
                    break;
                }
            }
            tokens.push_back(std::move(str_tok));
        }
        // Character literal
        else if (ch == '\'') {
            tokens.push_back(read_char_literal());
        }
        // Identifier or keyword
        else if (is_alpha(ch)) {
            tokens.push_back(read_identifier_or_keyword());
        }
        // Operator or punctuation
        else {
            tokens.push_back(read_operator_or_punctuation());
        }
    }

    // Add EOF token
    tokens.emplace_back(TokenType::Eof, "", line_, col_);

    return tokens;
}

} // namespace nexia
