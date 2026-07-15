#pragma once
// NexiaCompiler v2.0 — Semantic Analyzer
// Ported from SemanticAnalyzer.cs

#include <cstdint>
#include <memory>
#include "ast.h"
#include "symbol_table.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <optional>

namespace nexia {

class SemanticError : public std::runtime_error {
public:
    int line;
    int column;

    SemanticError(const std::string& message, int ln, int col)
        : std::runtime_error("Semantic error at line " + std::to_string(ln) +
                             ", column " + std::to_string(col) + ": " + message),
          line(ln), column(col) {}
};

struct SemanticWarning {
    std::string message;
    int line;
    int column;

    SemanticWarning(std::string msg, int ln, int col)
        : message(std::move(msg)), line(ln), column(col) {}

    std::string to_string() const {
        return "Warning at line " + std::to_string(line) + ", column " +
               std::to_string(column) + ": " + message;
    }
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    bool analyze(TranslationUnit& ast);

    const std::vector<SemanticError>& errors() const { return errors_; }
    const std::vector<SemanticWarning>& warnings() const { return warnings_; }

private:
    Scope* current_scope_;
    std::vector<std::unique_ptr<Scope>> owned_scopes_;
    std::vector<SemanticError> errors_;
    std::vector<SemanticWarning> warnings_;

    TypeInfo* current_function_return_type_ = nullptr;
    std::unique_ptr<TypeInfo> stored_return_type_;
    int loop_depth_ = 0;
    int switch_depth_ = 0;

    // Struct/class member registry
    std::unordered_map<std::string, std::vector<std::pair<std::string, TypeInfo>>> struct_members_;

    // Typedef alias map
    std::unordered_map<std::string, TypeInfo> typedef_map_;

    // Xbox 360 type sizes
    static const std::unordered_map<std::string, int>& type_sizes();
    static constexpr int POINTER_SIZE = 4;

    void push_scope();
    void pop_scope();

    void error(const std::string& msg, int line, int col);
    void warning(const std::string& msg, int line, int col);

    // Setup
    void register_builtin_types();

    // Forward declaration pass
    void collect_all_enums(const std::vector<AstPtr>& nodes);
    void collect_all_enums_members(const std::vector<std::unique_ptr<ClassMember>>& members);
    void register_enum_values(EnumDefinition& enumDef);
    std::optional<int64_t> try_evaluate_constant_expr(Expression& expr);
    void register_forward_declaration(AstNode& decl);
    void flatten_anonymous_members(std::vector<std::pair<std::string, TypeInfo>>& members);

    // Type resolution
    TypeInfo resolve_type(const TypeNode& typeNode);

    // Node analysis
    void analyze_node(AstNode& node);
    void analyze_function(FunctionDefinition& func);
    void analyze_variable_declaration(VariableDeclaration& varDecl);
    void analyze_return(ReturnStatement& ret);
    void analyze_if(IfStatement& ifStmt);
    void analyze_while(WhileStatement& whileStmt);
    void analyze_do_while(DoWhileStatement& doWhile);
    void analyze_for(ForStatement& forStmt);
    void analyze_switch(SwitchStatement& switchStmt);
    void analyze_try_catch(TryCatchStatement& tryCatch);
    void analyze_throw(ThrowStatement& throwStmt);

    // Expression analysis
    TypeInfo analyze_expression(Expression& expr);
    TypeInfo analyze_identifier(IdentifierExpr& id);
    TypeInfo analyze_binary(BinaryExpr& bin);
    TypeInfo analyze_unary(UnaryExpr& un);
    TypeInfo analyze_call(CallExpr& call);
    TypeInfo analyze_index(IndexExpr& idx);
    TypeInfo analyze_member_access(MemberAccessExpr& mem);
    TypeInfo analyze_ternary(TernaryExpr& tern);

    int get_type_size(const std::string& baseName);
};

} // namespace nexia
