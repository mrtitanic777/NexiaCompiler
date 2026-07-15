#include "token.h"
#include <unordered_map>

namespace nexia {

const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::IntegerLiteral: return "IntegerLiteral";
        case TokenType::FloatLiteral:   return "FloatLiteral";
        case TokenType::StringLiteral:  return "StringLiteral";
        case TokenType::CharLiteral:    return "CharLiteral";
        case TokenType::Identifier:     return "Identifier";
        case TokenType::Preprocessor:   return "Preprocessor";
        case TokenType::Eof:            return "EOF";
        case TokenType::Unknown:        return "Unknown";

        // Keywords
        case TokenType::KW_Auto:          return "auto";
        case TokenType::KW_Bool:          return "bool";
        case TokenType::KW_Break:         return "break";
        case TokenType::KW_Case:          return "case";
        case TokenType::KW_Catch:         return "catch";
        case TokenType::KW_Char:          return "char";
        case TokenType::KW_Class:         return "class";
        case TokenType::KW_Const:         return "const";
        case TokenType::KW_ConstCast:     return "const_cast";
        case TokenType::KW_Continue:      return "continue";
        case TokenType::KW_Default:       return "default";
        case TokenType::KW_Delete:        return "delete";
        case TokenType::KW_Do:            return "do";
        case TokenType::KW_Double:        return "double";
        case TokenType::KW_DynamicCast:   return "dynamic_cast";
        case TokenType::KW_Else:          return "else";
        case TokenType::KW_Enum:          return "enum";
        case TokenType::KW_Explicit:      return "explicit";
        case TokenType::KW_Extern:        return "extern";
        case TokenType::KW_False:         return "false";
        case TokenType::KW_Float:         return "float";
        case TokenType::KW_For:           return "for";
        case TokenType::KW_Friend:        return "friend";
        case TokenType::KW_Goto:          return "goto";
        case TokenType::KW_If:            return "if";
        case TokenType::KW_Inline:        return "inline";
        case TokenType::KW_Int:           return "int";
        case TokenType::KW_Long:          return "long";
        case TokenType::KW_Mutable:       return "mutable";
        case TokenType::KW_Namespace:     return "namespace";
        case TokenType::KW_New:           return "new";
        case TokenType::KW_Operator:      return "operator";
        case TokenType::KW_Private:       return "private";
        case TokenType::KW_Protected:     return "protected";
        case TokenType::KW_Public:        return "public";
        case TokenType::KW_Register:      return "register";
        case TokenType::KW_ReinterpretCast: return "reinterpret_cast";
        case TokenType::KW_Return:        return "return";
        case TokenType::KW_Short:         return "short";
        case TokenType::KW_Signed:        return "signed";
        case TokenType::KW_Sizeof:        return "sizeof";
        case TokenType::KW_Static:        return "static";
        case TokenType::KW_StaticCast:    return "static_cast";
        case TokenType::KW_Struct:        return "struct";
        case TokenType::KW_Switch:        return "switch";
        case TokenType::KW_Template:      return "template";
        case TokenType::KW_This:          return "this";
        case TokenType::KW_Throw:         return "throw";
        case TokenType::KW_True:          return "true";
        case TokenType::KW_Try:           return "try";
        case TokenType::KW_Typedef:       return "typedef";
        case TokenType::KW_Typeid:        return "typeid";
        case TokenType::KW_Typename:      return "typename";
        case TokenType::KW_Union:         return "union";
        case TokenType::KW_Unsigned:      return "unsigned";
        case TokenType::KW_Using:         return "using";
        case TokenType::KW_Virtual:       return "virtual";
        case TokenType::KW_Void:          return "void";
        case TokenType::KW_Volatile:      return "volatile";
        case TokenType::KW_WcharT:        return "wchar_t";
        case TokenType::KW_While:         return "while";

        // Operators
        case TokenType::OpPlus:       return "+";
        case TokenType::OpMinus:      return "-";
        case TokenType::OpStar:       return "*";
        case TokenType::OpSlash:      return "/";
        case TokenType::OpPercent:    return "%";
        case TokenType::OpAssign:     return "=";
        case TokenType::OpEqual:      return "==";
        case TokenType::OpNotEqual:   return "!=";
        case TokenType::OpLess:       return "<";
        case TokenType::OpGreater:    return ">";
        case TokenType::OpLessEqual:  return "<=";
        case TokenType::OpGreaterEqual: return ">=";
        case TokenType::OpAnd:        return "&&";
        case TokenType::OpOr:         return "||";
        case TokenType::OpNot:        return "!";
        case TokenType::OpBitAnd:     return "&";
        case TokenType::OpBitOr:      return "|";
        case TokenType::OpBitXor:     return "^";
        case TokenType::OpBitNot:     return "~";
        case TokenType::OpLShift:     return "<<";
        case TokenType::OpRShift:     return ">>";
        case TokenType::OpPlusAssign:    return "+=";
        case TokenType::OpMinusAssign:   return "-=";
        case TokenType::OpStarAssign:    return "*=";
        case TokenType::OpSlashAssign:   return "/=";
        case TokenType::OpPercentAssign: return "%=";
        case TokenType::OpAndAssign:     return "&=";
        case TokenType::OpOrAssign:      return "|=";
        case TokenType::OpXorAssign:     return "^=";
        case TokenType::OpLShiftAssign:  return "<<=";
        case TokenType::OpRShiftAssign:  return ">>=";
        case TokenType::OpIncrement:  return "++";
        case TokenType::OpDecrement:  return "--";
        case TokenType::OpArrow:      return "->";
        case TokenType::OpDot:        return ".";
        case TokenType::OpScope:      return "::";
        case TokenType::OpQuestion:   return "?";
        case TokenType::OpPointerToMember:      return ".*";
        case TokenType::OpArrowPointerToMember: return "->*";

        // Punctuation
        case TokenType::LParen:    return "(";
        case TokenType::RParen:    return ")";
        case TokenType::LBrace:    return "{";
        case TokenType::RBrace:    return "}";
        case TokenType::LBracket:  return "[";
        case TokenType::RBracket:  return "]";
        case TokenType::Semicolon: return ";";
        case TokenType::Colon:     return ":";
        case TokenType::Comma:     return ",";
        case TokenType::Ellipsis:  return "...";
    }
    return "???";
}

} // namespace nexia
