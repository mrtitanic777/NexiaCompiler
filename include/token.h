#pragma once
// NexiaCompiler v2.0 — Token types and Token class
// Ported from TokenTypes.cs

#include <string>
#include <cstdint>

namespace nexia {

/// Every kind of token the lexer can produce.
enum class TokenType : uint8_t {
    // Literals
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,

    // Identifier (variable names, function names, etc.)
    Identifier,

    // C++03 Keywords
    KW_Auto,
    KW_Bool,
    KW_Break,
    KW_Case,
    KW_Catch,
    KW_Char,
    KW_Class,
    KW_Const,
    KW_ConstCast,
    KW_Continue,
    KW_Default,
    KW_Delete,
    KW_Do,
    KW_Double,
    KW_DynamicCast,
    KW_Else,
    KW_Enum,
    KW_Explicit,
    KW_Extern,
    KW_False,
    KW_Float,
    KW_For,
    KW_Friend,
    KW_Goto,
    KW_If,
    KW_Inline,
    KW_Int,
    KW_Long,
    KW_Mutable,
    KW_Namespace,
    KW_New,
    KW_Operator,
    KW_Private,
    KW_Protected,
    KW_Public,
    KW_Register,
    KW_ReinterpretCast,
    KW_Return,
    KW_Short,
    KW_Signed,
    KW_Sizeof,
    KW_Static,
    KW_StaticCast,
    KW_Struct,
    KW_Switch,
    KW_Template,
    KW_This,
    KW_Throw,
    KW_True,
    KW_Try,
    KW_Typedef,
    KW_Typeid,
    KW_Typename,
    KW_Union,
    KW_Unsigned,
    KW_Using,
    KW_Virtual,
    KW_Void,
    KW_Volatile,
    KW_WcharT,
    KW_While,

    // Operators
    OpPlus,             // +
    OpMinus,            // -
    OpStar,             // *
    OpSlash,            // /
    OpPercent,          // %
    OpAssign,           // =
    OpEqual,            // ==
    OpNotEqual,         // !=
    OpLess,             // <
    OpGreater,          // >
    OpLessEqual,        // <=
    OpGreaterEqual,     // >=
    OpAnd,              // &&
    OpOr,               // ||
    OpNot,              // !
    OpBitAnd,           // &
    OpBitOr,            // |
    OpBitXor,           // ^
    OpBitNot,           // ~
    OpLShift,           // <<
    OpRShift,           // >>
    OpPlusAssign,       // +=
    OpMinusAssign,      // -=
    OpStarAssign,       // *=
    OpSlashAssign,      // /=
    OpPercentAssign,    // %=
    OpAndAssign,        // &=
    OpOrAssign,         // |=
    OpXorAssign,        // ^=
    OpLShiftAssign,     // <<=
    OpRShiftAssign,     // >>=
    OpIncrement,        // ++
    OpDecrement,        // --
    OpArrow,            // ->
    OpDot,              // .
    OpScope,            // ::
    OpQuestion,         // ?
    OpPointerToMember,       // .*
    OpArrowPointerToMember,  // ->*

    // Punctuation
    LParen,             // (
    RParen,             // )
    LBrace,             // {
    RBrace,             // }
    LBracket,           // [
    RBracket,           // ]
    Semicolon,          // ;
    Colon,              // :
    Comma,              // ,
    Ellipsis,           // ...

    // Preprocessor
    Preprocessor,       // #include, #define, etc.

    // Special
    Eof,                // End of file
    Unknown             // Anything we can't recognize
};

/// Return a human-readable name for a token type (for diagnostics).
const char* token_type_name(TokenType t);

/// A single token produced by the lexer.
struct Token {
    TokenType   type;
    std::string value;
    int         line;
    int         column;

    Token() : type(TokenType::Eof), line(0), column(0) {}

    Token(TokenType t, std::string v, int ln, int col)
        : type(t), value(std::move(v)), line(ln), column(col) {}
};

} // namespace nexia
