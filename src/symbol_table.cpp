// NexiaCompiler v2.0 — Symbol Table
// Ported from SymbolTable.cs

#include <memory>
#include "symbol_table.h"

namespace nexia {

// TypeInfo methods
bool TypeInfo::is_assignable_from(const TypeInfo& other) const {
    // Same type (ignoring const)
    if (base_name == other.base_name && pointer_depth == other.pointer_depth)
        return true;

    // Numeric types can be assigned to each other
    if (is_numeric() && other.is_numeric())
        return true;

    // Pointer to pointer (void* accepts anything)
    if (is_pointer() && other.is_pointer()) {
        if (base_name == "void" || other.base_name == "void")
            return true;
        if (base_name == other.base_name && pointer_depth == other.pointer_depth)
            return true;
    }

    // Integer to pointer and pointer to integer (C-style)
    if (is_pointer() && other.is_integer()) return true;
    if (is_integer() && other.is_pointer()) return true;

    // Bool from anything
    if (is_bool()) return true;

    return false;
}

std::string TypeInfo::to_string() const {
    std::string result;
    if (is_const)    result += "const ";
    if (is_unsigned) result += "unsigned ";
    result += base_name;
    for (int i = 0; i < pointer_depth; i++) result += "*";
    if (is_reference) result += "&";
    return result;
}

bool TypeInfo::operator==(const TypeInfo& other) const {
    return base_name == other.base_name &&
           is_unsigned == other.is_unsigned &&
           pointer_depth == other.pointer_depth &&
           is_reference == other.is_reference;
}

// Scope methods
bool Scope::define(std::unique_ptr<Symbol> symbol) {
    const std::string& name = symbol->name;
    if (symbols_.find(name) != symbols_.end())
        return false;
    symbols_[name] = std::move(symbol);
    return true;
}

Symbol* Scope::lookup(const std::string& name) const {
    auto it = symbols_.find(name);
    if (it != symbols_.end())
        return it->second.get();
    if (parent_)
        return parent_->lookup(name);
    return nullptr;
}

Symbol* Scope::lookup_local(const std::string& name) const {
    auto it = symbols_.find(name);
    if (it != symbols_.end())
        return it->second.get();
    return nullptr;
}

} // namespace nexia
