#pragma once
// NexiaCompiler v2.0 — Symbol Table
// Ported from SymbolTable.cs

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

namespace nexia {

enum class SymbolKind : uint8_t {
    Variable,
    Function,
    Parameter,
    TypeName
};

// Resolved type info used during semantic analysis
class TypeInfo {
public:
    std::string base_name;
    bool is_const;
    bool is_unsigned;
    int  pointer_depth;
    bool is_reference;
    int  size; // bytes on Xbox 360 / PPC

    TypeInfo()
        : is_const(false), is_unsigned(false), pointer_depth(0),
          is_reference(false), size(0) {}

    TypeInfo(std::string baseName, bool isConst, bool isUnsigned,
             int ptrDepth, bool isRef, int sz)
        : base_name(std::move(baseName)), is_const(isConst), is_unsigned(isUnsigned),
          pointer_depth(ptrDepth), is_reference(isRef), size(sz) {}

    bool is_pointer()        const { return pointer_depth > 0; }
    bool is_void()           const { return base_name == "void" && pointer_depth == 0; }
    bool is_bool()           const { return base_name == "bool" && pointer_depth == 0; }

    bool is_integer() const {
        return base_name == "char" || base_name == "short" || base_name == "int" ||
               base_name == "long" || base_name == "long long" ||
               base_name == "wchar_t" || base_name == "bool";
    }

    bool is_floating_point() const {
        return base_name == "float" || base_name == "double" || base_name == "long double";
    }

    bool is_numeric() const { return !is_pointer() && (is_integer() || is_floating_point()); }

    bool is_assignable_from(const TypeInfo& other) const;

    std::string to_string() const;
    bool operator==(const TypeInfo& other) const;
    bool operator!=(const TypeInfo& other) const { return !(*this == other); }
};

struct FunctionSignature {
    TypeInfo return_type;
    std::vector<TypeInfo> parameter_types;
    std::vector<std::string> parameter_names;

    FunctionSignature(TypeInfo retType, std::vector<TypeInfo> paramTypes,
                      std::vector<std::string> paramNames)
        : return_type(std::move(retType)), parameter_types(std::move(paramTypes)),
          parameter_names(std::move(paramNames)) {}
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    TypeInfo type;
    int declared_line;
    int declared_column;
    std::unique_ptr<FunctionSignature> func_signature;

    Symbol(std::string name, SymbolKind kind, TypeInfo type, int line, int col,
           std::unique_ptr<FunctionSignature> funcSig = nullptr)
        : name(std::move(name)), kind(kind), type(std::move(type)),
          declared_line(line), declared_column(col),
          func_signature(std::move(funcSig)) {}
};

class Scope {
public:
    explicit Scope(Scope* parent = nullptr) : parent_(parent) {}

    bool define(std::unique_ptr<Symbol> symbol);
    Symbol* lookup(const std::string& name) const;
    Symbol* lookup_local(const std::string& name) const;

    Scope* parent() const { return parent_; }

private:
    Scope* parent_;
    std::unordered_map<std::string, std::unique_ptr<Symbol>> symbols_;
};

} // namespace nexia
