#pragma once
// NexiaCompiler v2.0 — Parser
// Ported from Parser.cs
// Recursive descent with Pratt parsing for expressions.

#include <memory>
#include "token.h"
#include "ast.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <stdexcept>

namespace nexia {

class ParseError : public std::runtime_error {
public:
    int line;
    int column;

    ParseError(const std::string& message, int ln, int col)
        : std::runtime_error("Parse error at line " + std::to_string(ln) +
                             ", column " + std::to_string(col) + ": " + message),
          line(ln), column(col) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<TranslationUnit> parse();

    // Public expression parser (used by test suite)
    ExprPtr parse_expression();

private:
    std::vector<Token> tokens_;
    int pos_;

    // Typedef-parsed structs/enums that need to be added to the AST
    std::vector<std::unique_ptr<EnumDefinition>> typedef_enums_;
    std::vector<AstPtr> typedef_structs_;
    std::vector<AstPtr> pending_anonymous_decls_;

    // Known user-defined types and template names
    std::unordered_set<std::string> known_types_;
    std::unordered_set<std::string> template_names_;

    // Token helpers
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& errorMessage);

    bool is_type_keyword(TokenType type) const;
    bool is_at_type_specifier() const;
    bool looks_like_cast();

    // Type parsing
    TypePtr parse_type();

    // Declaration parsing
    AstPtr parse_declaration();
    std::unique_ptr<FunctionDefinition> parse_function_definition(TypePtr returnType, Token name);
    std::unique_ptr<VariableDeclaration> parse_variable_declaration(TypePtr type, Token name);
    std::unique_ptr<InitializerListExpr> parse_brace_initializer();
    std::vector<std::unique_ptr<Parameter>> parse_parameter_list();
    std::string skip_function_pointer_declarator();
    void skip_to_semicolon_or_brace();

    // Class/struct
    std::unique_ptr<ClassDefinition> parse_class_definition(bool fromTypedef = false);
    std::unique_ptr<BaseClassSpecifier> parse_base_class_specifier(bool parentIsStruct);
    std::unique_ptr<ClassMember> parse_class_member(const std::string& className,
                                                     AccessSpecifier access);
    // Overload for anonymous struct inner parsing
    std::unique_ptr<ClassMember> parse_class_member(AccessSpecifier access, bool isVirtual,
                                                     bool isStatic, bool isExplicit,
                                                     bool isFriend, bool isInline);
    std::unique_ptr<ConstructorDecl> parse_constructor(const std::string& className,
                                                        AccessSpecifier access,
                                                        bool isExplicit, int startLine, int startCol);
    std::unique_ptr<DestructorDecl> parse_destructor(const std::string& className,
                                                      AccessSpecifier access,
                                                      bool isVirtual, int startLine, int startCol);
    std::unique_ptr<MemberFunctionDecl> parse_member_function(AccessSpecifier access,
                                                               TypePtr returnType,
                                                               const std::string& name,
                                                               bool isVirtual, bool isStatic,
                                                               int startLine, int startCol);

    // Enum / Union
    std::unique_ptr<EnumDefinition> parse_enum_definition(bool fromTypedef = false);
    std::unique_ptr<UnionDefinition> parse_union_definition(bool fromTypedef = false);

    // Namespace
    std::unique_ptr<NamespaceDefinition> parse_namespace();
    AstPtr parse_using_statement();
    std::unique_ptr<TypedefDeclaration> parse_typedef();
    std::string expect_alias_name();

    // Operator overloading
    static const std::unordered_set<std::string>& overloadable_operators();
    bool is_operator_overload_ahead();
    std::string parse_operator_symbol();
    std::unique_ptr<ClassMember> parse_operator_overload_member(AccessSpecifier access,
                                                                  bool isVirtual, bool isFriend,
                                                                  bool isExplicit,
                                                                  int startLine, int startCol);
    std::unique_ptr<GlobalOperatorOverload> parse_global_operator_overload();

    // Templates
    AstPtr parse_template_declaration();
    std::vector<std::unique_ptr<TemplateParameter>> parse_template_parameter_list();
    std::unique_ptr<TemplateParameter> parse_single_template_parameter();
    std::vector<std::unique_ptr<TemplateArgument>>* try_parse_template_arguments();

    // Statements
    BlockPtr parse_block();
    AstPtr parse_statement_or_declaration();
    StmtPtr parse_statement();
    std::unique_ptr<ReturnStatement> parse_return_statement();
    std::unique_ptr<IfStatement> parse_if_statement();
    std::unique_ptr<WhileStatement> parse_while_statement();
    std::unique_ptr<DoWhileStatement> parse_do_while_statement();
    std::unique_ptr<ForStatement> parse_for_statement();
    std::unique_ptr<SwitchStatement> parse_switch_statement();
    std::unique_ptr<ExpressionStatement> parse_expression_statement();
    std::unique_ptr<TryCatchStatement> parse_try_catch();
    std::unique_ptr<CatchClause> parse_catch_clause();
    std::unique_ptr<ThrowStatement> parse_throw();

    // Expressions (Pratt/precedence climbing)
    ExprPtr parse_comma_expression();
    ExprPtr parse_assignment();
    ExprPtr parse_ternary();
    ExprPtr parse_logical_or();
    ExprPtr parse_logical_and();
    ExprPtr parse_bitwise_or();
    ExprPtr parse_bitwise_xor();
    ExprPtr parse_bitwise_and();
    ExprPtr parse_equality();
    ExprPtr parse_relational();
    ExprPtr parse_shift();
    ExprPtr parse_additive();
    ExprPtr parse_multiplicative();
    ExprPtr parse_unary();
    ExprPtr parse_postfix();
    ExprPtr parse_primary();
    std::unique_ptr<SizeofExpr> parse_sizeof();
    ExprPtr parse_new_expr();
    ExprPtr parse_delete_expr();

    bool is_assignment_op(TokenType type) const;
};

} // namespace nexia
