#pragma once
// NexiaCompiler v2.0 — AST node types
// Ported from AstNodes.cs
//
// The C# version used class hierarchies with inheritance.
// In C++ we use std::unique_ptr and std::variant-style polymorphism
// through the classic virtual-dispatch approach with shared_ptr
// for nodes that may be referenced from multiple places.

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include <utility>

namespace nexia {

// Forward declarations
class AstNode;
class Expression;
class Statement;
class TypeNode;
class BlockStatement;

// Owning pointer type used throughout the AST
using AstPtr      = std::unique_ptr<AstNode>;
using ExprPtr     = std::unique_ptr<Expression>;
using StmtPtr     = std::unique_ptr<Statement>;
using TypePtr     = std::unique_ptr<TypeNode>;
using BlockPtr    = std::unique_ptr<BlockStatement>;

// =========================================================================
// Visitor interface (for AstPrinter, CodeGenerator, SemanticAnalyzer, etc.)
// =========================================================================
class AstVisitor;

// =========================================================================
// Base class for all AST nodes
// =========================================================================
class AstNode {
public:
    int line;
    int column;

    AstNode(int ln, int col) : line(ln), column(col) {}
    virtual ~AstNode() = default;

    // Disable copy, allow move
    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
    AstNode(AstNode&&) = default;
    AstNode& operator=(AstNode&&) = default;
};

// =========================================================================
// Access specifier
// =========================================================================
enum class AccessSpecifier : uint8_t {
    Public,
    Protected,
    Private
};

// =========================================================================
// Template parameter kind
// =========================================================================
enum class TemplateParamKind : uint8_t {
    Type,       // template<typename T>
    NonType,    // template<int N>
    Template    // template<template<typename> class C>
};

// =========================================================================
// Type representation
// =========================================================================
class TypeNode : public AstNode {
public:
    std::string base_name;
    bool is_const;
    bool is_unsigned;
    bool is_signed;
    int  pointer_depth;
    bool is_reference;

    TypeNode(std::string baseName, bool isConst, bool isUnsigned, bool isSigned,
             int ptrDepth, bool isRef, int ln, int col)
        : AstNode(ln, col), base_name(std::move(baseName)),
          is_const(isConst), is_unsigned(isUnsigned), is_signed(isSigned),
          pointer_depth(ptrDepth), is_reference(isRef) {}

    std::string to_string() const;
};

// =========================================================================
// Template argument (type or value)
// =========================================================================
struct TemplateArgument : public AstNode {
    TypePtr  type_arg;   // If this is a type argument
    ExprPtr  value_arg;  // If this is a value argument (non-type)

    bool is_type() const { return type_arg != nullptr; }

    TemplateArgument(TypePtr typeArg, ExprPtr valueArg, int ln, int col)
        : AstNode(ln, col), type_arg(std::move(typeArg)), value_arg(std::move(valueArg)) {}
};

// =========================================================================
// Templated type node: vector<int>, map<string, int>
// =========================================================================
class TemplatedTypeNode : public TypeNode {
public:
    std::vector<std::unique_ptr<TemplateArgument>> template_args;

    TemplatedTypeNode(std::string baseName, bool isConst, bool isUnsigned, bool isSigned,
                      int ptrDepth, bool isRef,
                      std::vector<std::unique_ptr<TemplateArgument>> args,
                      int ln, int col)
        : TypeNode(std::move(baseName), isConst, isUnsigned, isSigned, ptrDepth, isRef, ln, col),
          template_args(std::move(args)) {}

    std::string to_string() const;
};

// =========================================================================
// Top-level nodes
// =========================================================================
class TranslationUnit : public AstNode {
public:
    std::vector<AstPtr> declarations;

    TranslationUnit(std::vector<AstPtr> decls, int ln, int col)
        : AstNode(ln, col), declarations(std::move(decls)) {}
};

class PreprocessorDirective : public AstNode {
public:
    std::string text;

    PreprocessorDirective(std::string txt, int ln, int col)
        : AstNode(ln, col), text(std::move(txt)) {}
};

// =========================================================================
// Declarations
// =========================================================================
class VariableDeclaration : public AstNode {
public:
    TypePtr type;
    std::string name;
    ExprPtr initializer;
    ExprPtr array_size;        // null if not an array
    std::vector<ExprPtr> initializer_list;  // empty if no brace-init
    bool has_init_list = false;

    bool is_array() const { return array_size != nullptr; }

    VariableDeclaration(TypePtr type, std::string name, ExprPtr init,
                        int ln, int col,
                        ExprPtr arraySize = nullptr,
                        std::vector<ExprPtr> initList = {})
        : AstNode(ln, col), type(std::move(type)), name(std::move(name)),
          initializer(std::move(init)), array_size(std::move(arraySize)),
          initializer_list(std::move(initList)) {
        has_init_list = !initializer_list.empty();
    }
};

class Parameter : public AstNode {
public:
    TypePtr type;
    std::string name;

    Parameter(TypePtr type, std::string name, int ln, int col)
        : AstNode(ln, col), type(std::move(type)), name(std::move(name)) {}
};

class FunctionDefinition : public AstNode {
public:
    TypePtr return_type;
    std::string name;
    std::vector<std::unique_ptr<Parameter>> parameters;
    BlockPtr body;

    FunctionDefinition(TypePtr retType, std::string name,
                       std::vector<std::unique_ptr<Parameter>> params,
                       BlockPtr body, int ln, int col)
        : AstNode(ln, col), return_type(std::move(retType)), name(std::move(name)),
          parameters(std::move(params)), body(std::move(body)) {}
};

// =========================================================================
// Statements
// =========================================================================
class Statement : public AstNode {
public:
    using AstNode::AstNode;
};

class BlockStatement : public Statement {
public:
    std::vector<AstPtr> statements;

    BlockStatement(std::vector<AstPtr> stmts, int ln, int col)
        : Statement(ln, col), statements(std::move(stmts)) {}
};

class ExpressionStatement : public Statement {
public:
    ExprPtr expr;

    ExpressionStatement(ExprPtr expr, int ln, int col)
        : Statement(ln, col), expr(std::move(expr)) {}
};

class ReturnStatement : public Statement {
public:
    ExprPtr value; // may be null

    ReturnStatement(ExprPtr val, int ln, int col)
        : Statement(ln, col), value(std::move(val)) {}
};

class IfStatement : public Statement {
public:
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch; // may be null

    IfStatement(ExprPtr cond, StmtPtr thenBr, StmtPtr elseBr, int ln, int col)
        : Statement(ln, col), condition(std::move(cond)),
          then_branch(std::move(thenBr)), else_branch(std::move(elseBr)) {}
};

class WhileStatement : public Statement {
public:
    ExprPtr condition;
    StmtPtr body;

    WhileStatement(ExprPtr cond, StmtPtr body, int ln, int col)
        : Statement(ln, col), condition(std::move(cond)), body(std::move(body)) {}
};

class DoWhileStatement : public Statement {
public:
    StmtPtr body;
    ExprPtr condition;

    DoWhileStatement(StmtPtr body, ExprPtr cond, int ln, int col)
        : Statement(ln, col), body(std::move(body)), condition(std::move(cond)) {}
};

class ForStatement : public Statement {
public:
    AstPtr  init;       // declaration or expression
    ExprPtr condition;
    ExprPtr increment;
    StmtPtr body;

    ForStatement(AstPtr init, ExprPtr cond, ExprPtr incr, StmtPtr body, int ln, int col)
        : Statement(ln, col), init(std::move(init)), condition(std::move(cond)),
          increment(std::move(incr)), body(std::move(body)) {}
};

class BreakStatement : public Statement {
public:
    BreakStatement(int ln, int col) : Statement(ln, col) {}
};

class ContinueStatement : public Statement {
public:
    ContinueStatement(int ln, int col) : Statement(ln, col) {}
};

class CatchClause : public AstNode {
public:
    TypePtr exception_type; // null for catch-all
    std::string variable_name;
    bool is_catch_all;
    BlockPtr body;

    CatchClause(TypePtr excType, std::string varName, bool catchAll,
                BlockPtr body, int ln, int col)
        : AstNode(ln, col), exception_type(std::move(excType)),
          variable_name(std::move(varName)), is_catch_all(catchAll),
          body(std::move(body)) {}
};

class TryCatchStatement : public Statement {
public:
    BlockPtr try_body;
    std::vector<std::unique_ptr<CatchClause>> catch_clauses;

    TryCatchStatement(BlockPtr tryBody, std::vector<std::unique_ptr<CatchClause>> clauses,
                      int ln, int col)
        : Statement(ln, col), try_body(std::move(tryBody)),
          catch_clauses(std::move(clauses)) {}
};

class ThrowStatement : public Statement {
public:
    ExprPtr value; // null for rethrow

    ThrowStatement(ExprPtr val, int ln, int col)
        : Statement(ln, col), value(std::move(val)) {}
};

class SwitchCase : public AstNode {
public:
    ExprPtr value; // null for default
    std::vector<AstPtr> body;

    SwitchCase(ExprPtr val, std::vector<AstPtr> body, int ln, int col)
        : AstNode(ln, col), value(std::move(val)), body(std::move(body)) {}
};

class SwitchStatement : public Statement {
public:
    ExprPtr value;
    std::vector<std::unique_ptr<SwitchCase>> cases;

    SwitchStatement(ExprPtr val, std::vector<std::unique_ptr<SwitchCase>> cases, int ln, int col)
        : Statement(ln, col), value(std::move(val)), cases(std::move(cases)) {}
};

// =========================================================================
// Expressions
// =========================================================================
class Expression : public AstNode {
public:
    using AstNode::AstNode;
};

class IntegerLiteral : public Expression {
public:
    std::string value;
    IntegerLiteral(std::string val, int ln, int col)
        : Expression(ln, col), value(std::move(val)) {}
};

class FloatLiteral : public Expression {
public:
    std::string value;
    FloatLiteral(std::string val, int ln, int col)
        : Expression(ln, col), value(std::move(val)) {}
};

class StringLiteralExpr : public Expression {
public:
    std::string value;
    StringLiteralExpr(std::string val, int ln, int col)
        : Expression(ln, col), value(std::move(val)) {}
};

class CharLiteralExpr : public Expression {
public:
    std::string value;
    CharLiteralExpr(std::string val, int ln, int col)
        : Expression(ln, col), value(std::move(val)) {}
};

class BoolLiteral : public Expression {
public:
    bool value;
    BoolLiteral(bool val, int ln, int col)
        : Expression(ln, col), value(val) {}
};

class IdentifierExpr : public Expression {
public:
    std::string name;
    IdentifierExpr(std::string name, int ln, int col)
        : Expression(ln, col), name(std::move(name)) {}
};

class BinaryExpr : public Expression {
public:
    ExprPtr left;
    std::string op;
    ExprPtr right;

    BinaryExpr(ExprPtr left, std::string op, ExprPtr right, int ln, int col)
        : Expression(ln, col), left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
};

class UnaryExpr : public Expression {
public:
    std::string op;
    ExprPtr operand;
    bool is_prefix;

    UnaryExpr(std::string op, ExprPtr operand, bool isPrefix, int ln, int col)
        : Expression(ln, col), op(std::move(op)), operand(std::move(operand)), is_prefix(isPrefix) {}
};

class CallExpr : public Expression {
public:
    ExprPtr callee;
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr callee, std::vector<ExprPtr> args, int ln, int col)
        : Expression(ln, col), callee(std::move(callee)), arguments(std::move(args)) {}
};

class IndexExpr : public Expression {
public:
    ExprPtr object;
    ExprPtr index;

    IndexExpr(ExprPtr obj, ExprPtr idx, int ln, int col)
        : Expression(ln, col), object(std::move(obj)), index(std::move(idx)) {}
};

class MemberAccessExpr : public Expression {
public:
    ExprPtr object;
    std::string member_name;
    bool is_arrow; // true for ->, false for .

    MemberAccessExpr(ExprPtr obj, std::string member, bool isArrow, int ln, int col)
        : Expression(ln, col), object(std::move(obj)),
          member_name(std::move(member)), is_arrow(isArrow) {}
};

class CastExpr : public Expression {
public:
    TypePtr target_type;
    ExprPtr operand;

    CastExpr(TypePtr type, ExprPtr operand, int ln, int col)
        : Expression(ln, col), target_type(std::move(type)), operand(std::move(operand)) {}
};

class SizeofExpr : public Expression {
public:
    TypePtr target_type;  // set if sizing a type
    ExprPtr operand;      // set if sizing an expression

    SizeofExpr(TypePtr type, ExprPtr operand, int ln, int col)
        : Expression(ln, col), target_type(std::move(type)), operand(std::move(operand)) {}
};

class TernaryExpr : public Expression {
public:
    ExprPtr condition;
    ExprPtr then_expr;
    ExprPtr else_expr;

    TernaryExpr(ExprPtr cond, ExprPtr thenE, ExprPtr elseE, int ln, int col)
        : Expression(ln, col), condition(std::move(cond)),
          then_expr(std::move(thenE)), else_expr(std::move(elseE)) {}
};

class InitializerListExpr : public Expression {
public:
    std::vector<ExprPtr> values;

    InitializerListExpr(std::vector<ExprPtr> vals, int ln, int col)
        : Expression(ln, col), values(std::move(vals)) {}
};

class NewExpr : public Expression {
public:
    TypePtr alloc_type;
    std::vector<ExprPtr> constructor_args; // empty if none
    ExprPtr array_size;
    bool is_array;

    NewExpr(TypePtr type, std::vector<ExprPtr> ctorArgs, ExprPtr arraySize,
            bool isArray, int ln, int col)
        : Expression(ln, col), alloc_type(std::move(type)),
          constructor_args(std::move(ctorArgs)), array_size(std::move(arraySize)),
          is_array(isArray) {}
};

class DeleteExpr : public Expression {
public:
    ExprPtr operand;
    bool is_array;

    DeleteExpr(ExprPtr operand, bool isArray, int ln, int col)
        : Expression(ln, col), operand(std::move(operand)), is_array(isArray) {}
};

class ThisExpr : public Expression {
public:
    ThisExpr(int ln, int col) : Expression(ln, col) {}
};

class ScopeResolutionExpr : public Expression {
public:
    std::string scope; // may be empty for global ::
    std::string name;

    ScopeResolutionExpr(std::string scope, std::string name, int ln, int col)
        : Expression(ln, col), scope(std::move(scope)), name(std::move(name)) {}
};

// =========================================================================
// Class / Struct / Enum / Union declarations
// =========================================================================

class ClassMember : public AstNode {
public:
    AccessSpecifier access;

    ClassMember(AccessSpecifier acc, int ln, int col)
        : AstNode(ln, col), access(acc) {}
};

class MemberVariableDecl : public ClassMember {
public:
    TypePtr type;
    std::string name;
    ExprPtr default_value;
    bool is_static;

    MemberVariableDecl(AccessSpecifier acc, TypePtr type, std::string name,
                       ExprPtr defVal, bool isStatic, int ln, int col)
        : ClassMember(acc, ln, col), type(std::move(type)), name(std::move(name)),
          default_value(std::move(defVal)), is_static(isStatic) {}
};

class MemberFunctionDecl : public ClassMember {
public:
    TypePtr return_type;
    std::string name;
    std::vector<std::unique_ptr<Parameter>> parameters;
    BlockPtr body; // null for declaration-only
    bool is_virtual;
    bool is_static;
    bool is_const;
    bool is_pure_virtual;

    MemberFunctionDecl(AccessSpecifier acc, TypePtr retType, std::string name,
                       std::vector<std::unique_ptr<Parameter>> params, BlockPtr body,
                       bool isVirtual, bool isStatic, bool isConst, bool isPureVirtual,
                       int ln, int col)
        : ClassMember(acc, ln, col), return_type(std::move(retType)), name(std::move(name)),
          parameters(std::move(params)), body(std::move(body)),
          is_virtual(isVirtual), is_static(isStatic), is_const(isConst),
          is_pure_virtual(isPureVirtual) {}
};

struct MemberInitializer {
    std::string member_name;
    std::vector<ExprPtr> args;
};

class ConstructorDecl : public ClassMember {
public:
    std::string class_name;
    std::vector<std::unique_ptr<Parameter>> parameters;
    std::vector<MemberInitializer> initializer_list;
    BlockPtr body;
    bool is_explicit;

    ConstructorDecl(AccessSpecifier acc, std::string className,
                    std::vector<std::unique_ptr<Parameter>> params,
                    std::vector<MemberInitializer> initList,
                    BlockPtr body, bool isExplicit, int ln, int col)
        : ClassMember(acc, ln, col), class_name(std::move(className)),
          parameters(std::move(params)), initializer_list(std::move(initList)),
          body(std::move(body)), is_explicit(isExplicit) {}
};

class DestructorDecl : public ClassMember {
public:
    std::string class_name;
    BlockPtr body;
    bool is_virtual;

    DestructorDecl(AccessSpecifier acc, std::string className,
                   BlockPtr body, bool isVirtual, int ln, int col)
        : ClassMember(acc, ln, col), class_name(std::move(className)),
          body(std::move(body)), is_virtual(isVirtual) {}
};

class BaseClassSpecifier : public AstNode {
public:
    std::string class_name;
    AccessSpecifier access;
    bool is_virtual;

    BaseClassSpecifier(std::string name, AccessSpecifier acc, bool isVirtual, int ln, int col)
        : AstNode(ln, col), class_name(std::move(name)), access(acc), is_virtual(isVirtual) {}
};

class ClassDefinition : public AstNode {
public:
    std::string name;
    bool is_struct;
    std::vector<std::unique_ptr<BaseClassSpecifier>> base_classes;
    std::vector<std::unique_ptr<ClassMember>> members;

    AccessSpecifier default_access() const {
        return is_struct ? AccessSpecifier::Public : AccessSpecifier::Private;
    }

    ClassDefinition(std::string name, bool isStruct,
                    std::vector<std::unique_ptr<BaseClassSpecifier>> bases,
                    std::vector<std::unique_ptr<ClassMember>> members,
                    int ln, int col)
        : AstNode(ln, col), name(std::move(name)), is_struct(isStruct),
          base_classes(std::move(bases)), members(std::move(members)) {}
};

struct EnumValue {
    std::string name;
    ExprPtr value; // may be null
};

class EnumDefinition : public AstNode {
public:
    std::string name;
    std::vector<EnumValue> values;

    EnumDefinition(std::string name, std::vector<EnumValue> vals, int ln, int col)
        : AstNode(ln, col), name(std::move(name)), values(std::move(vals)) {}
};

class UnionDefinition : public AstNode {
public:
    std::string name;
    std::vector<std::unique_ptr<ClassMember>> members;

    UnionDefinition(std::string name, std::vector<std::unique_ptr<ClassMember>> members,
                    int ln, int col)
        : AstNode(ln, col), name(std::move(name)), members(std::move(members)) {}
};

// =========================================================================
// Operator overloading
// =========================================================================
class OperatorOverload : public ClassMember {
public:
    TypePtr return_type;
    std::string op;
    std::vector<std::unique_ptr<Parameter>> parameters;
    BlockPtr body;
    bool is_const;
    bool is_virtual;
    bool is_friend;

    OperatorOverload(AccessSpecifier acc, TypePtr retType, std::string op,
                     std::vector<std::unique_ptr<Parameter>> params, BlockPtr body,
                     bool isConst, bool isVirtual, bool isFriend, int ln, int col)
        : ClassMember(acc, ln, col), return_type(std::move(retType)), op(std::move(op)),
          parameters(std::move(params)), body(std::move(body)),
          is_const(isConst), is_virtual(isVirtual), is_friend(isFriend) {}
};

class GlobalOperatorOverload : public AstNode {
public:
    TypePtr return_type;
    std::string op;
    std::vector<std::unique_ptr<Parameter>> parameters;
    BlockPtr body;

    GlobalOperatorOverload(TypePtr retType, std::string op,
                           std::vector<std::unique_ptr<Parameter>> params, BlockPtr body,
                           int ln, int col)
        : AstNode(ln, col), return_type(std::move(retType)), op(std::move(op)),
          parameters(std::move(params)), body(std::move(body)) {}
};

class ConversionOperator : public ClassMember {
public:
    TypePtr target_type;
    BlockPtr body;
    bool is_const;
    bool is_explicit;

    ConversionOperator(AccessSpecifier acc, TypePtr targetType, BlockPtr body,
                       bool isConst, bool isExplicit, int ln, int col)
        : ClassMember(acc, ln, col), target_type(std::move(targetType)),
          body(std::move(body)), is_const(isConst), is_explicit(isExplicit) {}
};

// =========================================================================
// Namespaces
// =========================================================================
class NamespaceDefinition : public AstNode {
public:
    std::string name;
    std::vector<AstPtr> declarations;

    NamespaceDefinition(std::string name, std::vector<AstPtr> decls, int ln, int col)
        : AstNode(ln, col), name(std::move(name)), declarations(std::move(decls)) {}
};

class UsingDirective : public AstNode {
public:
    std::string namespace_name;

    UsingDirective(std::string nsName, int ln, int col)
        : AstNode(ln, col), namespace_name(std::move(nsName)) {}
};

class UsingDeclaration : public AstNode {
public:
    std::string qualified_name;

    UsingDeclaration(std::string qName, int ln, int col)
        : AstNode(ln, col), qualified_name(std::move(qName)) {}
};

class TypedefDeclaration : public AstNode {
public:
    TypePtr original_type;
    std::string alias_name;

    TypedefDeclaration(TypePtr origType, std::string alias, int ln, int col)
        : AstNode(ln, col), original_type(std::move(origType)), alias_name(std::move(alias)) {}
};

// =========================================================================
// Templates
// =========================================================================
class TemplateParameter : public AstNode {
public:
    TemplateParamKind kind;
    std::string name;
    TypePtr non_type_type;  // for NonType params
    AstPtr default_value;
    std::vector<std::unique_ptr<TemplateParameter>> inner_params; // for template template params

    TemplateParameter(TemplateParamKind kind, std::string name,
                      TypePtr nonTypeType, AstPtr defaultValue,
                      std::vector<std::unique_ptr<TemplateParameter>> innerParams,
                      int ln, int col)
        : AstNode(ln, col), kind(kind), name(std::move(name)),
          non_type_type(std::move(nonTypeType)), default_value(std::move(defaultValue)),
          inner_params(std::move(innerParams)) {}
};

class TemplateDeclaration : public AstNode {
public:
    std::vector<std::unique_ptr<TemplateParameter>> parameters;
    AstPtr inner_declaration;

    TemplateDeclaration(std::vector<std::unique_ptr<TemplateParameter>> params,
                        AstPtr inner, int ln, int col)
        : AstNode(ln, col), parameters(std::move(params)),
          inner_declaration(std::move(inner)) {}
};

class TemplateSpecialization : public AstNode {
public:
    std::vector<std::unique_ptr<TemplateParameter>> parameters;
    AstPtr inner_declaration;
    std::vector<std::unique_ptr<TemplateArgument>> specialization_args;

    TemplateSpecialization(std::vector<std::unique_ptr<TemplateParameter>> params,
                           AstPtr inner,
                           std::vector<std::unique_ptr<TemplateArgument>> specArgs,
                           int ln, int col)
        : AstNode(ln, col), parameters(std::move(params)),
          inner_declaration(std::move(inner)),
          specialization_args(std::move(specArgs)) {}
};

class MemberTemplateDecl : public ClassMember {
public:
    std::vector<std::unique_ptr<TemplateParameter>> template_params;
    std::unique_ptr<ClassMember> inner_member;

    MemberTemplateDecl(AccessSpecifier acc,
                       std::vector<std::unique_ptr<TemplateParameter>> params,
                       std::unique_ptr<ClassMember> inner, int ln, int col)
        : ClassMember(acc, ln, col), template_params(std::move(params)),
          inner_member(std::move(inner)) {}
};

} // namespace nexia
