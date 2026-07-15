#pragma once
// NexiaCompiler v2.0 — Code Generator
// Ported from CodeGenerator.cs
// Generates PPC machine code from the AST.

#include <memory>
#include "ast.h"
#include "ppc_instructions.h"
#include "register_allocator.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace nexia {

class CodeGenerator {
public:
    void generate(TranslationUnit& unit);

    const std::vector<uint8_t>& code() const { return code_bytes_; }
    const std::vector<uint8_t>& data() const { return data_section_; }
    const std::unordered_map<std::string, int>& function_offsets() const { return function_offsets_; }
    const std::vector<std::string>& external_calls() const { return external_calls_; }

private:
    std::vector<uint32_t> instructions_;
    std::vector<uint8_t> data_section_;
    std::unordered_map<std::string, int> string_literals_;
    std::unordered_map<std::string, int> function_offsets_;
    std::vector<std::string> external_calls_; // functions called but not defined in this TU
    std::unique_ptr<RegisterAllocator> regs_;

    std::unordered_map<std::string, int> labels_;

    enum class BranchKind { Unconditional, Equal, NotEqual, Less, Greater, LessEq, GreaterEq };
    struct PatchEntry {
        int instruction_index;
        std::string label;
        BranchKind kind;
    };
    std::vector<PatchEntry> patches_;
    int label_counter_ = 0;

    std::vector<uint8_t> code_bytes_;

    // Current function state
    std::string current_function_name_;
    std::string break_label_;
    std::string continue_label_;
    std::string current_exception_label_;

    std::string new_label(const std::string& prefix = "L");
    void emit(uint32_t instruction);
    void patch_branches();

    void generate_function(FunctionDefinition& func);
    void pre_scan_locals(BlockStatement& block);
    void generate_block(BlockStatement& block);
    void generate_statement(AstNode& node);
    void generate_var_decl(VariableDeclaration& decl);
    void generate_return(ReturnStatement& ret);
    void generate_if(IfStatement& ifStmt);
    void generate_while(WhileStatement& whileStmt);
    void generate_do_while(DoWhileStatement& doWhile);
    void generate_for(ForStatement& forStmt);
    void generate_switch(SwitchStatement& switchStmt);
    void generate_try_catch(TryCatchStatement& tryCatch);
    void generate_throw(ThrowStatement& throwStmt);

    void generate_expression(Expression& expr, PpcRegister destReg);
    void generate_int_literal(IntegerLiteral& lit, PpcRegister destReg);
    void generate_string_literal(StringLiteralExpr& str, PpcRegister destReg);
    void generate_binary(BinaryExpr& bin, PpcRegister destReg);
    void generate_unary(UnaryExpr& un, PpcRegister destReg);
    void generate_call(CallExpr& call, PpcRegister destReg);
    void generate_index(IndexExpr& idx, PpcRegister destReg);
    void generate_ternary(TernaryExpr& tern, PpcRegister destReg);
    void load_variable(const std::string& name, PpcRegister destReg);
};

} // namespace nexia
