// NexiaCompiler v2.0 — AST node implementations
// Ported from AstNodes.cs

#include "ast.h"

namespace nexia {

std::string TypeNode::to_string() const {
    std::string result;
    if (is_const)    result += "const ";
    if (is_unsigned) result += "unsigned ";
    if (is_signed)   result += "signed ";
    result += base_name;
    for (int i = 0; i < pointer_depth; i++) result += "*";
    if (is_reference) result += "&";
    return result;
}

std::string TemplatedTypeNode::to_string() const {
    std::string args;
    for (size_t i = 0; i < template_args.size(); i++) {
        if (i > 0) args += ", ";
        if (template_args[i]->is_type() && template_args[i]->type_arg)
            args += template_args[i]->type_arg->to_string();
        else
            args += "expr";
    }
    return TypeNode::to_string() + "<" + args + ">";
}

} // namespace nexia
