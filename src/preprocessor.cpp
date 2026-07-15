// NexiaCompiler v2.0 — C/C++ Preprocessor
// Ported from Preprocessor.cs
// Full implementation: 1,220 lines C# -> C++ stub with TODO markers

#include "preprocessor.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace nexia {

Preprocessor::Preprocessor(std::vector<std::string> includePaths)
    : include_paths_(std::move(includePaths)) {
    define_predefined();
}

void Preprocessor::add_include_path(const std::string& path) {
    include_paths_.push_back(path);
}

void Preprocessor::define(const std::string& name, const std::string& value) {
    macros_[name] = MacroDefinition(name, {}, false, value, "<command-line>", 0);
}

void Preprocessor::define_function_macro(const std::string& name,
                                          std::vector<std::string> params,
                                          const std::string& body) {
    macros_[name] = MacroDefinition(name, std::move(params), true, body, "<built-in>", 0);
}

std::string Preprocessor::process(const std::string& source, const std::string& fileName) {
    std::string src = source;
    // Normalize line endings
    std::string normalized;
    normalized.reserve(src.size());
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] == '\r') {
            normalized += '\n';
            if (i + 1 < src.size() && src[i+1] == '\n') i++;
        } else {
            normalized += src[i];
        }
    }
    normalized = strip_comments(normalized);
    return process_internal(normalized, fileName, 0);
}

// TODO: Port the full bodies from Preprocessor.cs for these methods.
// The structure and signatures match exactly. Each method below has a
// placeholder that preserves compilation while marking work to be done.

std::string Preprocessor::strip_comments(const std::string& source) {
    std::string result;
    result.reserve(source.size());
    size_t i = 0;
    while (i < source.size()) {
        if (source[i] == '"') {
            result += source[i++];
            while (i < source.size() && source[i] != '"') {
                if (source[i] == '\\' && i + 1 < source.size()) result += source[i++];
                result += source[i++];
            }
            if (i < source.size()) result += source[i++];
            continue;
        }
        if (source[i] == '\'') {
            result += source[i++];
            while (i < source.size() && source[i] != '\'') {
                if (source[i] == '\\' && i + 1 < source.size()) result += source[i++];
                result += source[i++];
            }
            if (i < source.size()) result += source[i++];
            continue;
        }
        if (i + 1 < source.size() && source[i] == '/' && source[i+1] == '/') {
            while (i < source.size() && source[i] != '\n') i++;
            continue;
        }
        if (i + 1 < source.size() && source[i] == '/' && source[i+1] == '*') {
            i += 2;
            while (i < source.size()) {
                if (i + 1 < source.size() && source[i] == '*' && source[i+1] == '/') {
                    i += 2; break;
                }
                if (source[i] == '\n') result += '\n'; else result += ' ';
                i++;
            }
            continue;
        }
        result += source[i++];
    }
    return result;
}

void Preprocessor::define_predefined() {
    define("__cplusplus", "199711L");
    define("_XBOX", "1");
    define("_XBOX_VER", "200");
    define("_M_PPC", "1");
    define("_M_PPCBE", "1");
    define("__BIG_ENDIAN__", "1");
    define("__VMX128_SUPPORTED", "1");
    define("_VMX128_INTRINSICS_", "1");
    define("_VMX128_IS_DIFFERENT_TYPE_", "1");
    define("__vector4", "__vector4");
    define("_WIN32", "1");
    define("_MSC_VER", "1500");
    define("__NEXIA__", "1");
    define("NULL", "0");
    define("__forceinline", "inline");
    define("__inline", "inline");
    define("__cdecl", "");
    define("__stdcall", "");
    define("__clrcall", "");
    define("__fastcall", "");
    define("__thiscall", "");
    define("__w64", "");
    define("__unaligned", "");
    define("__ptr64", "");
    define("__ptr32", "");
    define("__restrict", "");
    define("__sptr", "");
    define("__uptr", "");
    define("__int8", "char");
    define("__int16", "short");
    define("__int32", "int");
    define("__int64", "long long");
    define_function_macro("__declspec", {"x"}, "");
    define_function_macro("__pragma", {"x"}, "");
    define_function_macro("RtlZeroMemory", {"dest", "size"}, "memset((dest), 0, (size))");
    define_function_macro("RtlCopyMemory", {"dest", "src", "size"}, "memcpy((dest), (src), (size))");
    define_function_macro("RtlFillMemory", {"dest", "size", "val"}, "memset((dest), (val), (size))");

    // D3D macros that must expand before the parser sees them
    define_function_macro("D3DCOLOR_ARGB", {"a", "r", "g", "b"},
        "((unsigned long)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))");
    define_function_macro("D3DCOLOR_XRGB", {"r", "g", "b"},
        "((unsigned long)(((0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))");
    define_function_macro("D3DCOLOR_RGBA", {"r", "g", "b", "a"},
        "((unsigned long)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))");
    // D3DDECL_END() produces an initializer — expand inline
    define_function_macro("D3DDECL_END", {}, "{0xFF,0,0,0,0,0}");

    // GPU tag macros
    define_function_macro("D3DTAG_INDEX", {"tag"}, "(((tag) >> 0) & 0xFFFF)");
    define_function_macro("D3DTAG_COUNT", {"tag"}, "(((tag) >> 16) & 0x3FFF)");
    define_function_macro("D3DTAG_START", {"base", "count"},
        "(((base) & 0xFFFF) | (((count) & 0x3FFF) << 16) | (3 << 30))");
    define_function_macro("D3DTAG_MASKENCODE", {"mask"}, "((mask) & 0xFFFF)");
    define_function_macro("GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT", {"d3d", "hw"}, "((void)0)");
    define_function_macro("GPU_CONVERT_D3D_TO_HARDWARE_VERTEXFETCHCONSTANT", {"d3d", "hw"}, "((void)0)");

    // Compiler intrinsics
    define_function_macro("__debugbreak", {}, "((void)0)");
    define_function_macro("_mm_set_ps1", {"v"}, "(v)");
    define_function_macro("_mm_set1_ps", {"v"}, "(v)");
    define_function_macro("_mm_add_ps", {"a", "b"}, "((a)+(b))");
    define_function_macro("_mm_sub_ps", {"a", "b"}, "((a)-(b))");
    define_function_macro("_mm_mul_ps", {"a", "b"}, "((a)*(b))");
    define_function_macro("_mm_div_ps", {"a", "b"}, "((a)/(b))");
    define("__fctidz(x)", "((long long)(x))");
    define("__fcfid(x)", "((float)(x))");

    // Xbox math helpers
    define_function_macro("XMMax", {"a", "b"}, "((a) > (b) ? (a) : (b))");
    define_function_macro("XMMin", {"a", "b"}, "((a) < (b) ? (a) : (b))");
}

std::string Preprocessor::process_internal(const std::string& source,
                                            const std::string& fileName, int includeDepth) {
    if (includeDepth > MAX_INCLUDE_DEPTH)
        throw PreprocessorError("Maximum include depth exceeded", fileName, 0);

    macros_["__FILE__"] = MacroDefinition("__FILE__", {}, false,
        "\"" + fileName + "\"", fileName, 0);

    std::string output;
    std::istringstream stream(source);
    std::string line;
    int lineNum = 0;
    std::vector<CondState> condStack;

    while (std::getline(stream, line)) {
        lineNum++;

        // Strip trailing \r (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Handle backslash line continuation
        while (!line.empty() && line.back() == '\\') {
            line.pop_back(); // remove the backslash
            std::string next;
            if (std::getline(stream, next)) {
                lineNum++;
                if (!next.empty() && next.back() == '\r') next.pop_back();
                line += next;
            } else break;
        }

        macros_["__LINE__"] = MacroDefinition("__LINE__", {}, false,
            std::to_string(lineNum), fileName, lineNum);

        std::string trimmed = line;
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) trimmed = trimmed.substr(start);

        if (!trimmed.empty() && trimmed[0] == '#') {
            std::string directive = trimmed.substr(1);
            size_t ds = directive.find_first_not_of(" \t");
            if (ds != std::string::npos) directive = directive.substr(ds);

            if (try_handle_conditional(directive, condStack, fileName, lineNum)) {
                output += "\n";
                continue;
            }
            if (!condStack.empty() && !condStack.back().is_active) {
                output += "\n";
                continue;
            }
            handle_directive(directive, fileName, lineNum, includeDepth, output);
            continue;
        }

        if (!condStack.empty() && !condStack.back().is_active) {
            output += "\n";
            continue;
        }

        output += expand_macros(line, fileName, lineNum) + "\n";
    }

    return output;
}

bool Preprocessor::try_handle_conditional(const std::string& directive,
                                           std::vector<CondState>& condStack,
                                           const std::string& fileName, int line) {
    if (directive.substr(0, 5) == "ifdef") {
        std::string name = directive.substr(5);
        size_t s = name.find_first_not_of(" \t");
        if (s != std::string::npos) name = name.substr(s);
        size_t e = name.find_first_of(" \t\r\n");
        if (e != std::string::npos) name = name.substr(0, e);
        bool parentActive = condStack.empty() || condStack.back().is_active;
        bool defined = macros_.count(name) > 0;
        bool active = parentActive && defined;
        condStack.push_back({active, active, true});
        return true;
    }
    if (directive.substr(0, 6) == "ifndef") {
        std::string name = directive.substr(6);
        size_t s = name.find_first_not_of(" \t");
        if (s != std::string::npos) name = name.substr(s);
        size_t e = name.find_first_of(" \t\r\n");
        if (e != std::string::npos) name = name.substr(0, e);
        bool parentActive = condStack.empty() || condStack.back().is_active;
        bool defined = macros_.count(name) > 0;
        bool active = parentActive && !defined;
        condStack.push_back({active, active, true});
        return true;
    }
    if (directive.substr(0, 2) == "if" && directive.substr(0, 5) != "ifdef" &&
        directive.substr(0, 6) != "ifndef" && directive.substr(0, 7) != "include") {
        std::string expr = directive.substr(2);
        size_t s = expr.find_first_not_of(" \t");
        if (s != std::string::npos) expr = expr.substr(s);
        bool parentActive = condStack.empty() || condStack.back().is_active;
        bool result = parentActive && evaluate_constant_expression(expr, fileName, line);
        condStack.push_back({result, result, true});
        return true;
    }
    if (directive.substr(0, 4) == "elif") {
        if (condStack.empty()) throw PreprocessorError("#elif without #if", fileName, line);
        auto prev = condStack.back(); condStack.pop_back();
        bool parentActive = condStack.empty() || condStack.back().is_active;
        std::string expr = directive.substr(4);
        size_t s = expr.find_first_not_of(" \t");
        if (s != std::string::npos) expr = expr.substr(s);
        bool result = false;
        if (parentActive && !prev.has_been_true)
            result = evaluate_constant_expression(expr, fileName, line);
        condStack.push_back({result, prev.has_been_true || result, true});
        return true;
    }
    if (directive.substr(0, 4) == "else") {
        if (condStack.empty()) throw PreprocessorError("#else without #if", fileName, line);
        auto prev = condStack.back(); condStack.pop_back();
        bool parentActive = condStack.empty() || condStack.back().is_active;
        bool active = parentActive && !prev.has_been_true;
        condStack.push_back({active, true, false});
        return true;
    }
    if (directive.substr(0, 5) == "endif") {
        if (condStack.empty()) throw PreprocessorError("#endif without #if", fileName, line);
        condStack.pop_back();
        return true;
    }
    return false;
}

void Preprocessor::handle_directive(const std::string& directive, const std::string& fileName,
                                     int line, int includeDepth, std::string& output) {
    if (directive.substr(0, 7) == "include") {
        handle_include(directive.substr(7), fileName, line, includeDepth, output);
        return;
    }
    if (directive.substr(0, 6) == "define") {
        std::string arg = directive.substr(6);
        size_t s = arg.find_first_not_of(" \t");
        if (s != std::string::npos) arg = arg.substr(s);
        handle_define(arg, fileName, line);
        output += "\n";
        return;
    }
    if (directive.substr(0, 5) == "undef") {
        std::string name = directive.substr(5);
        size_t s = name.find_first_not_of(" \t");
        if (s != std::string::npos) name = name.substr(s);
        size_t e = name.find_first_of(" \t\r\n");
        if (e != std::string::npos) name = name.substr(0, e);
        macros_.erase(name);
        output += "\n";
        return;
    }
    if (directive.substr(0, 6) == "pragma") {
        std::string pragma = directive.substr(6);
        size_t s = pragma.find_first_not_of(" \t");
        if (s != std::string::npos) pragma = pragma.substr(s);
        pragmas_.push_back(pragma);
        if (pragma == "once") included_files_.insert(fileName);
        output += "\n";
        return;
    }
    if (directive.substr(0, 5) == "error") {
        throw PreprocessorError("#error " + directive.substr(5), fileName, line);
    }
    // Unknown directive — skip
    output += "\n";
}

void Preprocessor::handle_include(const std::string& argument, const std::string& fileName,
                                   int line, int includeDepth, std::string& output) {
    std::string arg = argument;
    size_t s = arg.find_first_not_of(" \t");
    if (s != std::string::npos) arg = arg.substr(s);

    std::string includePath;
    bool isSystem = false;

    if (!arg.empty() && arg[0] == '"') {
        size_t end = arg.find('"', 1);
        if (end == std::string::npos) throw PreprocessorError("Invalid #include", fileName, line);
        includePath = arg.substr(1, end - 1);
    } else if (!arg.empty() && arg[0] == '<') {
        size_t end = arg.find('>');
        if (end == std::string::npos) throw PreprocessorError("Invalid #include", fileName, line);
        includePath = arg.substr(1, end - 1);
        isSystem = true;
    } else {
        throw PreprocessorError("Invalid #include syntax", fileName, line);
    }

    std::string resolved = resolve_include(includePath, fileName, isSystem);
    if (resolved.empty()) {
        warnings.push_back(fileName + ":" + std::to_string(line) + ": Cannot find: " + argument);
        output += "// [NexiaCompiler] Skipped: #include " + argument + "\n";
        return;
    }

    if (included_files_.count(resolved)) { output += "\n"; return; }

    std::ifstream file(resolved);
    if (!file) throw PreprocessorError("Cannot read " + resolved, fileName, line);
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    content = strip_comments(content);
    included_files_.insert(resolved);
    output += process_internal(content, resolved, includeDepth + 1);
}

void Preprocessor::handle_define(const std::string& argument, const std::string& fileName, int line) {
    if (argument.empty()) throw PreprocessorError("Empty #define", fileName, line);
    size_t nameEnd = 0;
    while (nameEnd < argument.size() && (std::isalnum(argument[nameEnd]) || argument[nameEnd] == '_'))
        nameEnd++;
    std::string name = argument.substr(0, nameEnd);
    if (name.empty()) throw PreprocessorError("Invalid macro name", fileName, line);

    std::string rest = nameEnd < argument.size() ? argument.substr(nameEnd) : "";
    std::vector<std::string> params;
    bool isFuncLike = false;

    if (!rest.empty() && rest[0] == '(') {
        isFuncLike = true;
        size_t parenEnd = rest.find(')');
        if (parenEnd == std::string::npos) throw PreprocessorError("Unterminated params", fileName, line);
        std::string paramStr = rest.substr(1, parenEnd - 1);
        if (!paramStr.empty()) {
            std::istringstream ps(paramStr);
            std::string p;
            while (std::getline(ps, p, ',')) {
                size_t s = p.find_first_not_of(" \t");
                size_t e = p.find_last_not_of(" \t");
                if (s != std::string::npos) p = p.substr(s, e - s + 1);
                if (p == "...") p = "__VA_ARGS__";
                params.push_back(p);
            }
        }
        rest = rest.substr(parenEnd + 1);
    }

    std::string body = rest;
    size_t bs = body.find_first_not_of(" \t");
    if (bs != std::string::npos) body = body.substr(bs); else body = "";

    macros_[name] = MacroDefinition(name, std::move(params), isFuncLike, body, fileName, line);
}

std::string Preprocessor::resolve_include(const std::string& includePath,
                                           const std::string& currentFile, bool isSystem) const {
    if (!isSystem) {
        size_t lastSlash = currentFile.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            std::string candidate = currentFile.substr(0, lastSlash + 1) + includePath;
            std::ifstream f(candidate);
            if (f) return candidate;
        }
    }
    for (const auto& searchPath : include_paths_) {
        std::string candidate = searchPath + "/" + includePath;
        std::ifstream f(candidate);
        if (f) return candidate;
    }
    return "";
}

std::string Preprocessor::expand_macros(const std::string& text, const std::string& fileName,
                                         int line, std::unordered_set<std::string>* expanding) {
    std::unordered_set<std::string> localExpanding;
    if (!expanding) expanding = &localExpanding;

    std::string result;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '"') { size_t end = find_string_end(text, i); result += text.substr(i, end - i); i = end; continue; }
        if (text[i] == '\'') { size_t end = find_char_end(text, i); result += text.substr(i, end - i); i = end; continue; }
        if (std::isalpha(text[i]) || text[i] == '_') {
            size_t start = i;
            while (i < text.size() && (std::isalnum(text[i]) || text[i] == '_')) i++;
            std::string id = text.substr(start, i - start);
            auto it = macros_.find(id);
            if (it != macros_.end() && expanding->count(id) == 0) {
                if (it->second.is_function_like) {
                    size_t peek = i;
                    while (peek < text.size() && text[peek] == ' ') peek++;
                    if (peek < text.size() && text[peek] == '(') {
                        size_t afterClose;
                        auto args = parse_macro_arguments(text, peek, afterClose);
                        i = afterClose;
                        std::string expanded = expand_function_macro(it->second, args);
                        expanding->insert(id);
                        expanded = expand_macros(expanded, fileName, line, expanding);
                        expanding->erase(id);
                        result += expanded;
                        continue;
                    }
                } else {
                    expanding->insert(id);
                    std::string expanded = expand_macros(it->second.body, fileName, line, expanding);
                    expanding->erase(id);
                    result += expanded;
                    continue;
                }
            }
            result += id;
            continue;
        }
        result += text[i++];
    }
    return result;
}

// Helper: check if character is a C identifier character
static bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Replace all whole-word occurrences of 'word' with 'replacement' in 'text'.
// This is the C++ equivalent of C#'s Regex.Replace(text, @"\bword\b", replacement).
static std::string replace_whole_word(const std::string& text, const std::string& word,
                                       const std::string& replacement) {
    if (word.empty()) return text;
    std::string result;
    size_t i = 0;
    while (i < text.size()) {
        size_t pos = text.find(word, i);
        if (pos == std::string::npos) {
            result += text.substr(i);
            break;
        }
        // Check word boundaries
        bool left_ok = (pos == 0) || !is_ident_char(text[pos - 1]);
        bool right_ok = (pos + word.size() >= text.size()) ||
                        !is_ident_char(text[pos + word.size()]);
        if (left_ok && right_ok) {
            result += text.substr(i, pos - i);
            result += replacement;
            i = pos + word.size();
        } else {
            result += text.substr(i, pos - i + 1);
            i = pos + 1;
        }
    }
    return result;
}

std::string Preprocessor::expand_function_macro(const MacroDefinition& macro,
                                                 const std::vector<std::string>& args) {
    std::string result = macro.body;
    for (size_t i = 0; i < macro.parameters.size() && i < args.size(); i++) {
        const std::string& param = macro.parameters[i];
        std::string arg = args[i];
        // Trim arg
        size_t s = arg.find_first_not_of(" \t");
        size_t e = arg.find_last_not_of(" \t");
        if (s != std::string::npos) arg = arg.substr(s, e - s + 1); else arg = "";

        // Handle # (stringize) operator
        std::string stringized = "#" + param;
        size_t pos = result.find(stringized);
        if (pos != std::string::npos)
            result.replace(pos, stringized.size(), "\"" + arg + "\"");

        // Handle ## (token pasting) operator
        std::string paste1 = param + "##";
        while ((pos = result.find(paste1)) != std::string::npos)
            result.replace(pos, paste1.size(), arg);
        std::string paste2 = "##" + param;
        while ((pos = result.find(paste2)) != std::string::npos)
            result.replace(pos, paste2.size(), arg);

        // Regular replacement with word boundary matching
        result = replace_whole_word(result, param, arg);
    }

    // Handle __VA_ARGS__ for variadic macros
    if (!macro.parameters.empty() && macro.parameters.back() == "__VA_ARGS__" &&
        args.size() > macro.parameters.size() - 1) {
        std::string vaArgs;
        for (size_t i = macro.parameters.size() - 1; i < args.size(); i++) {
            if (i > macro.parameters.size() - 1) vaArgs += ", ";
            vaArgs += args[i];
        }
        result = replace_whole_word(result, "__VA_ARGS__", vaArgs);
    }

    // Clean up remaining ##
    size_t pos;
    while ((pos = result.find("##")) != std::string::npos)
        result.erase(pos, 2);
    return result;
}

std::vector<std::string> Preprocessor::parse_macro_arguments(const std::string& text,
                                                              size_t openParen, size_t& afterClose) {
    std::vector<std::string> args;
    std::string current;
    int depth = 0;
    size_t i = openParen + 1;
    while (i < text.size()) {
        char c = text[i];
        if (c == '(' || c == '[' || c == '{') { depth++; current += c; }
        else if (c == ')' || c == ']' || c == '}') {
            if (c == ')' && depth == 0) { args.push_back(current); afterClose = i + 1; return args; }
            depth--; current += c;
        }
        else if (c == ',' && depth == 0) { args.push_back(current); current.clear(); }
        else { current += c; }
        i++;
    }
    args.push_back(current);
    afterClose = text.size();
    return args;
}

// Helper: parse "defined(NAME)" and "defined NAME" in preprocessor #if expressions.
// Replaces them with "1" or "0". Must run BEFORE macro expansion per C standard.
static std::string replace_defined_operator(const std::string& expr,
                                             const std::unordered_map<std::string, MacroDefinition>& macros) {
    std::string result;
    size_t i = 0;
    while (i < expr.size()) {
        // Look for "defined"
        if (i + 7 <= expr.size() && expr.substr(i, 7) == "defined") {
            // Make sure it's a whole word
            if (i > 0 && is_ident_char(expr[i - 1])) {
                result += expr[i++];
                continue;
            }
            size_t after = i + 7;
            if (after < expr.size() && is_ident_char(expr[after])) {
                result += expr[i++];
                continue;
            }

            // Skip whitespace
            size_t j = after;
            while (j < expr.size() && (expr[j] == ' ' || expr[j] == '\t')) j++;

            std::string name;
            if (j < expr.size() && expr[j] == '(') {
                // defined(NAME) form
                j++; // skip (
                while (j < expr.size() && (expr[j] == ' ' || expr[j] == '\t')) j++;
                size_t nameStart = j;
                while (j < expr.size() && is_ident_char(expr[j])) j++;
                name = expr.substr(nameStart, j - nameStart);
                while (j < expr.size() && (expr[j] == ' ' || expr[j] == '\t')) j++;
                if (j < expr.size() && expr[j] == ')') j++; // skip )
            } else {
                // defined NAME form
                size_t nameStart = j;
                while (j < expr.size() && is_ident_char(expr[j])) j++;
                name = expr.substr(nameStart, j - nameStart);
            }

            result += (macros.count(name) > 0) ? "1" : "0";
            i = j;
        } else {
            result += expr[i++];
        }
    }
    return result;
}

// Helper: replace all remaining C identifiers with "0" (per C standard for #if).
static std::string replace_identifiers_with_zero(const std::string& expr) {
    std::string result;
    size_t i = 0;
    while (i < expr.size()) {
        if (std::isalpha(static_cast<unsigned char>(expr[i])) || expr[i] == '_') {
            // Skip the identifier
            while (i < expr.size() && is_ident_char(expr[i])) i++;
            result += "0";
        } else {
            result += expr[i++];
        }
    }
    return result;
}

bool Preprocessor::evaluate_constant_expression(const std::string& expr,
                                                 const std::string& fileName, int line) {
    // IMPORTANT: In C/C++, the 'defined' operator must be processed
    // BEFORE macro expansion, otherwise defined(MACRO) would expand
    // MACRO first and then check if the expansion is defined.
    std::string processed = replace_defined_operator(expr, macros_);

    // NOW expand macros in the expression
    std::string expanded = expand_macros(processed, fileName, line);

    // Replace any remaining identifiers with 0 (per C standard)
    expanded = replace_identifiers_with_zero(expanded);

    try {
        return eval_simple_expr(expanded) != 0;
    } catch (...) {
        warnings.push_back(fileName + ":" + std::to_string(line) + ": Cannot evaluate #if: " + expr);
        return false;
    }
}

int64_t Preprocessor::eval_simple_expr(const std::string& expr) {
    std::string e = expr;
    // Trim
    size_t s = e.find_first_not_of(" \t");
    size_t end = e.find_last_not_of(" \t");
    if (s == std::string::npos) return 0;
    e = e.substr(s, end - s + 1);

    if (e.empty()) return 0;

    // Unary NOT
    if (e[0] == '!') return eval_simple_expr(e.substr(1)) == 0 ? 1 : 0;
    // Unary minus
    if (e[0] == '-' && e.size() > 1 && std::isdigit(e[1])) return -eval_simple_expr(e.substr(1));

    // Try as number
    std::string num = e;
    while (!num.empty() && (num.back() == 'L' || num.back() == 'l' || num.back() == 'U' || num.back() == 'u'))
        num.pop_back();
    if (num.size() > 2 && (num.substr(0, 2) == "0x" || num.substr(0, 2) == "0X"))
        return std::stoll(num.substr(2), nullptr, 16);
    try { return std::stoll(num); } catch (...) {}
    return 0;
}

int Preprocessor::find_operator(const std::string&, const std::string&) { return -1; }
int Preprocessor::find_operator_rtl(const std::string&, const std::string&) { return -1; }
int Preprocessor::find_matching_paren(const std::string& text, int openPos) {
    int depth = 0;
    for (int i = openPos; i < (int)text.size(); i++) {
        if (text[i] == '(') depth++;
        else if (text[i] == ')') depth--;
        if (depth == 0) return i;
    }
    return -1;
}

size_t Preprocessor::find_string_end(const std::string& text, size_t start) {
    size_t i = start + 1;
    while (i < text.size()) {
        if (text[i] == '\\') { i += 2; continue; }
        if (text[i] == '"') return i + 1;
        i++;
    }
    return text.size();
}

size_t Preprocessor::find_char_end(const std::string& text, size_t start) {
    size_t i = start + 1;
    while (i < text.size()) {
        if (text[i] == '\\') { i += 2; continue; }
        if (text[i] == '\'') return i + 1;
        i++;
    }
    return text.size();
}

bool Preprocessor::is_inside_string(const std::string& text, size_t pos) {
    bool in = false;
    for (size_t i = 0; i < pos; i++) {
        if (text[i] == '"' && (i == 0 || text[i-1] != '\\')) in = !in;
    }
    return in;
}

} // namespace nexia
