#pragma once
// NexiaCompiler v2.0 — AST Printer (debug tool)
// Ported from AstPrinter.cs

#include "ast.h"
#include <string>
#include <iostream>
#include <functional>

namespace nexia {

class AstPrinter {
public:
    void print(const AstNode& node);

private:
    int indent_ = 0;

    void print_expr(const Expression& expr);
    void print_line(const std::string& text);
    void indented(std::function<void()> action);
};

} // namespace nexia
