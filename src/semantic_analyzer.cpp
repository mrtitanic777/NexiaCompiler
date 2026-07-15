// NexiaCompiler v2.0 — Semantic Analyzer
// Ported from SemanticAnalyzer.cs — FULL implementation

#include "semantic_analyzer.h"
#include <algorithm>

namespace nexia {

const std::unordered_map<std::string, int>& SemanticAnalyzer::type_sizes() {
    static const std::unordered_map<std::string, int> sizes = {
        {"void", 0}, {"bool", 1}, {"char", 1}, {"wchar_t", 2},
        {"short", 2}, {"int", 4}, {"long", 4}, {"long long", 8},
        {"float", 4}, {"double", 8}, {"long double", 8},
    };
    return sizes;
}

int SemanticAnalyzer::get_type_size(const std::string& baseName) {
    auto& sizes = type_sizes();
    auto it = sizes.find(baseName);
    return it != sizes.end() ? it->second : 0;
}

SemanticAnalyzer::SemanticAnalyzer() {
    auto globalScope = std::make_unique<Scope>();
    current_scope_ = globalScope.get();
    owned_scopes_.push_back(std::move(globalScope));
    loop_depth_ = 0;
    switch_depth_ = 0;
    register_builtin_types();
}

void SemanticAnalyzer::push_scope() {
    auto child = std::make_unique<Scope>(current_scope_);
    current_scope_ = child.get();
    owned_scopes_.push_back(std::move(child));
}

void SemanticAnalyzer::pop_scope() {
    current_scope_ = current_scope_->parent();
}

void SemanticAnalyzer::error(const std::string& msg, int line, int col) {
    errors_.emplace_back(msg, line, col);
}

void SemanticAnalyzer::warning(const std::string& msg, int line, int col) {
    warnings_.emplace_back(msg, line, col);
}

void SemanticAnalyzer::register_builtin_types() {
    TypeInfo floatType("float", false, false, 0, false, 4);
    TypeInfo floatPtrType("float", false, false, 1, false, POINTER_SIZE);
    TypeInfo uintPtrType("int", false, true, 1, false, POINTER_SIZE);

    struct_members_["__vector4"] = {
        {"vector4_f32", floatPtrType}, {"vector4_u32", uintPtrType},
        {"v", floatPtrType},
        {"x", floatType}, {"y", floatType}, {"z", floatType}, {"w", floatType},
    };
    typedef_map_["__vector4"] = TypeInfo("__vector4", false, false, 0, false, 16);
    struct_members_["__vector4i"] = struct_members_["__vector4"];
    typedef_map_["__vector4i"] = TypeInfo("__vector4i", false, false, 0, false, 16);
    typedef_map_["XMVECTOR"] = TypeInfo("__vector4", false, false, 0, false, 16);

    for (const auto& vecName : {"XMVECTORF32", "XMVECTORU32", "XMVECTORI32"}) {
        struct_members_[vecName] = {
            {"v", TypeInfo("__vector4", false, false, 0, false, 16)},
            {"f", floatPtrType},
            {"u", uintPtrType},
        };
        typedef_map_[vecName] = TypeInfo(vecName, false, false, 0, false, 16);
    }
}

bool SemanticAnalyzer::analyze(TranslationUnit& unit) {
    // Pass 0: Collect ALL enum definitions
    collect_all_enums(unit.declarations);

    // Pass 1: Register forward declarations
    for (auto& decl : unit.declarations)
        register_forward_declaration(*decl);

    // Pass 2: Full analysis
    for (auto& decl : unit.declarations)
        analyze_node(*decl);

    return errors_.empty();
}

void SemanticAnalyzer::collect_all_enums(const std::vector<AstPtr>& nodes) {
    for (auto& node : nodes) {
        if (auto* e = dynamic_cast<EnumDefinition*>(node.get())) {
            register_enum_values(*e);
        } else if (auto* ns = dynamic_cast<NamespaceDefinition*>(node.get())) {
            collect_all_enums(ns->declarations);
        } else if (auto* cls = dynamic_cast<ClassDefinition*>(node.get())) {
            collect_all_enums_members(cls->members);
        } else if (auto* tmpl = dynamic_cast<TemplateDeclaration*>(node.get())) {
            if (tmpl->inner_declaration) {
                std::vector<AstPtr> tmp;
                // Can't move, just check directly
                if (auto* inner_enum = dynamic_cast<EnumDefinition*>(tmpl->inner_declaration.get()))
                    register_enum_values(*inner_enum);
            }
        }
    }
}

void SemanticAnalyzer::collect_all_enums_members(const std::vector<std::unique_ptr<ClassMember>>& members) {
    for (auto& m : members) {
        // ClassMembers don't contain enums directly in our AST structure
        (void)m;
    }
}

void SemanticAnalyzer::register_enum_values(EnumDefinition& enumDef) {
    TypeInfo intType("int", false, false, 0, false, 4);

    if (!enumDef.name.empty() && enumDef.name.substr(0, 7) != "__anon_")
        typedef_map_[enumDef.name] = intType;

    int nextValue = 0;
    for (auto& ev : enumDef.values) {
        if (ev.name.empty()) continue;

        if (ev.value) {
            auto evaluated = try_evaluate_constant_expr(*ev.value);
            if (evaluated.has_value())
                nextValue = (int)evaluated.value();
        }

        if (!current_scope_->lookup(ev.name)) {
            current_scope_->define(std::make_unique<Symbol>(
                ev.name, SymbolKind::Variable, intType, enumDef.line, enumDef.column));
        }
        nextValue++;
    }
}

std::optional<int64_t> SemanticAnalyzer::try_evaluate_constant_expr(Expression& expr) {
    if (auto* intLit = dynamic_cast<IntegerLiteral*>(&expr)) {
        std::string val = intLit->value;
        // Strip suffixes
        while (!val.empty() && (val.back() == 'u' || val.back() == 'U' ||
               val.back() == 'l' || val.back() == 'L'))
            val.pop_back();
        try {
            if (val.size() > 2 && (val.substr(0, 2) == "0x" || val.substr(0, 2) == "0X"))
                return std::stoll(val.substr(2), nullptr, 16);
            return std::stoll(val);
        } catch (...) { return std::nullopt; }
    }
    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        if (un->op == "-") {
            auto inner = try_evaluate_constant_expr(*un->operand);
            return inner.has_value() ? std::optional<int64_t>(-inner.value()) : std::nullopt;
        }
        if (un->op == "~") {
            auto inner = try_evaluate_constant_expr(*un->operand);
            return inner.has_value() ? std::optional<int64_t>(~inner.value()) : std::nullopt;
        }
    }
    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        auto left = try_evaluate_constant_expr(*bin->left);
        auto right = try_evaluate_constant_expr(*bin->right);
        if (!left.has_value() || !right.has_value()) return std::nullopt;
        int64_t l = left.value(), r = right.value();
        if (bin->op == "+") return l + r;
        if (bin->op == "-") return l - r;
        if (bin->op == "*") return l * r;
        if (bin->op == "/" && r != 0) return l / r;
        if (bin->op == "%" && r != 0) return l % r;
        if (bin->op == "<<") return l << (int)r;
        if (bin->op == ">>") return l >> (int)r;
        if (bin->op == "|") return l | r;
        if (bin->op == "&") return l & r;
        if (bin->op == "^") return l ^ r;
    }
    if (auto* cast = dynamic_cast<CastExpr*>(&expr))
        return try_evaluate_constant_expr(*cast->operand);
    return std::nullopt;
}

void SemanticAnalyzer::register_forward_declaration(AstNode& decl) {
    if (auto* func = dynamic_cast<FunctionDefinition*>(&decl)) {
        TypeInfo returnType = resolve_type(*func->return_type);
        std::vector<TypeInfo> paramTypes;
        std::vector<std::string> paramNames;
        for (auto& param : func->parameters) {
            paramTypes.push_back(resolve_type(*param->type));
            paramNames.push_back(param->name);
        }
        auto sig = std::make_unique<FunctionSignature>(returnType, std::move(paramTypes), std::move(paramNames));
        current_scope_->define(std::make_unique<Symbol>(
            func->name, SymbolKind::Function, returnType, func->line, func->column, std::move(sig)));
    }
    else if (auto* enumDef = dynamic_cast<EnumDefinition*>(&decl)) {
        TypeInfo intType("int", false, false, 0, false, 4);
        typedef_map_[enumDef->name] = intType;
        int nextValue = 0;
        for (auto& ev : enumDef->values) {
            if (ev.value) {
                if (auto* intLit = dynamic_cast<IntegerLiteral*>(ev.value.get())) {
                    try { nextValue = (int)std::stoll(intLit->value); } catch (...) {}
                }
            }
            current_scope_->define(std::make_unique<Symbol>(
                ev.name, SymbolKind::Variable, intType, enumDef->line, enumDef->column));
            nextValue++;
        }
    }
    else if (auto* varDecl = dynamic_cast<VariableDeclaration*>(&decl)) {
        TypeInfo varType = resolve_type(*varDecl->type);
        if (varDecl->is_array() && varDecl->array_size)
            varType = TypeInfo(varType.base_name, varType.is_const, varType.is_unsigned,
                               varType.pointer_depth + 1, varType.is_reference, 4);
        current_scope_->define(std::make_unique<Symbol>(
            varDecl->name, SymbolKind::Variable, varType, varDecl->line, varDecl->column));
    }
    else if (auto* ns = dynamic_cast<NamespaceDefinition*>(&decl)) {
        push_scope();
        for (auto& d : ns->declarations)
            register_forward_declaration(*d);
        pop_scope();
    }
    else if (auto* cls = dynamic_cast<ClassDefinition*>(&decl)) {
        std::vector<std::pair<std::string, TypeInfo>> members;
        for (auto& member : cls->members) {
            if (auto* mv = dynamic_cast<MemberVariableDecl*>(member.get())) {
                members.emplace_back(mv->name, resolve_type(*mv->type));
            } else if (auto* mf = dynamic_cast<MemberFunctionDecl*>(member.get())) {
                members.emplace_back(mf->name, resolve_type(*mf->return_type));
            }
        }
        // Inherit base class members
        for (auto& base : cls->base_classes) {
            auto it = struct_members_.find(base->class_name);
            if (it != struct_members_.end()) {
                for (auto& bm : it->second) {
                    bool found = false;
                    for (auto& m : members) if (m.first == bm.first) { found = true; break; }
                    if (!found) members.push_back(bm);
                }
            }
        }
        flatten_anonymous_members(members);
        struct_members_[cls->name] = members;
        TypeInfo classType(cls->name, false, false, 0, false, 0);
        current_scope_->define(std::make_unique<Symbol>(
            cls->name, SymbolKind::TypeName, classType, cls->line, cls->column));
    }
    else if (auto* uni = dynamic_cast<UnionDefinition*>(&decl)) {
        std::vector<std::pair<std::string, TypeInfo>> members;
        for (auto& member : uni->members) {
            if (auto* mv = dynamic_cast<MemberVariableDecl*>(member.get()))
                members.emplace_back(mv->name, resolve_type(*mv->type));
        }
        flatten_anonymous_members(members);
        if (!uni->name.empty()) struct_members_[uni->name] = members;
    }
    else if (auto* td = dynamic_cast<TypedefDeclaration*>(&decl)) {
        TypeInfo origType = resolve_type(*td->original_type);
        typedef_map_[td->alias_name] = origType;
        current_scope_->define(std::make_unique<Symbol>(
            td->alias_name, SymbolKind::TypeName, origType, td->line, td->column));
    }
}

void SemanticAnalyzer::flatten_anonymous_members(std::vector<std::pair<std::string, TypeInfo>>& members) {
    bool changed = true;
    int maxIter = 10;
    while (changed && maxIter-- > 0) {
        changed = false;
        for (int i = (int)members.size() - 1; i >= 0; i--) {
            auto& [name, type] = members[i];
            if (type.base_name.substr(0, 7) == "__anon_" && name == type.base_name) {
                auto it = struct_members_.find(type.base_name);
                if (it != struct_members_.end()) {
                    members.erase(members.begin() + i);
                    for (auto& inner : it->second) {
                        bool found = false;
                        for (auto& m : members) if (m.first == inner.first) { found = true; break; }
                        if (!found) members.push_back(inner);
                    }
                    changed = true;
                }
            }
        }
    }
}

TypeInfo SemanticAnalyzer::resolve_type(const TypeNode& typeNode) {
    std::string baseName = typeNode.base_name;

    auto it = typedef_map_.find(baseName);
    if (it != typedef_map_.end()) {
        auto& resolved = it->second;
        int totalPtr = resolved.pointer_depth + typeNode.pointer_depth;
        bool isConst = typeNode.is_const || resolved.is_const;
        bool isUnsigned = typeNode.is_unsigned || resolved.is_unsigned;
        bool isRef = typeNode.is_reference || resolved.is_reference;
        int sz = totalPtr > 0 ? POINTER_SIZE : resolved.size;
        return TypeInfo(resolved.base_name, isConst, isUnsigned, totalPtr, isRef, sz);
    }

    int size;
    if (typeNode.pointer_depth > 0)
        size = POINTER_SIZE;
    else
        size = get_type_size(baseName);

    return TypeInfo(baseName, typeNode.is_const, typeNode.is_unsigned,
                    typeNode.pointer_depth, typeNode.is_reference, size);
}

// --- Node analysis ---

void SemanticAnalyzer::analyze_node(AstNode& node) {
    if (dynamic_cast<PreprocessorDirective*>(&node)) return;

    if (auto* func = dynamic_cast<FunctionDefinition*>(&node)) { analyze_function(*func); return; }
    if (auto* var = dynamic_cast<VariableDeclaration*>(&node)) { analyze_variable_declaration(*var); return; }
    if (auto* block = dynamic_cast<BlockStatement*>(&node)) {
        push_scope();
        for (auto& s : block->statements) analyze_node(*s);
        pop_scope();
        return;
    }
    if (auto* es = dynamic_cast<ExpressionStatement*>(&node)) { analyze_expression(*es->expr); return; }
    if (auto* ret = dynamic_cast<ReturnStatement*>(&node)) { analyze_return(*ret); return; }
    if (auto* ifS = dynamic_cast<IfStatement*>(&node)) { analyze_if(*ifS); return; }
    if (auto* whS = dynamic_cast<WhileStatement*>(&node)) { analyze_while(*whS); return; }
    if (auto* dwS = dynamic_cast<DoWhileStatement*>(&node)) { analyze_do_while(*dwS); return; }
    if (auto* forS = dynamic_cast<ForStatement*>(&node)) { analyze_for(*forS); return; }
    if (auto* swS = dynamic_cast<SwitchStatement*>(&node)) { analyze_switch(*swS); return; }
    if (auto* brk = dynamic_cast<BreakStatement*>(&node)) {
        if (loop_depth_ == 0 && switch_depth_ == 0)
            error("'break' not inside loop or switch", brk->line, brk->column);
        return;
    }
    if (auto* cont = dynamic_cast<ContinueStatement*>(&node)) {
        if (loop_depth_ == 0)
            error("'continue' not inside a loop", cont->line, cont->column);
        return;
    }
    if (auto* tc = dynamic_cast<TryCatchStatement*>(&node)) { analyze_try_catch(*tc); return; }
    if (auto* th = dynamic_cast<ThrowStatement*>(&node)) { analyze_throw(*th); return; }
    if (auto* ns = dynamic_cast<NamespaceDefinition*>(&node)) {
        push_scope();
        for (auto& d : ns->declarations) analyze_node(*d);
        pop_scope();
        return;
    }
    // ClassDefinition, EnumDefinition, etc. — handled in forward pass
}

void SemanticAnalyzer::analyze_function(FunctionDefinition& func) {
    TypeInfo returnType = resolve_type(*func.return_type);
    std::vector<TypeInfo> paramTypes;
    std::vector<std::string> paramNames;
    for (auto& p : func.parameters) {
        paramTypes.push_back(resolve_type(*p->type));
        paramNames.push_back(p->name);
    }
    auto sig = std::make_unique<FunctionSignature>(returnType, paramTypes, paramNames);
    current_scope_->define(std::make_unique<Symbol>(
        func.name, SymbolKind::Function, returnType, func.line, func.column, std::move(sig)));

    push_scope();

    // Inject class members for out-of-class methods
    std::string className;
    size_t scopeIdx = func.name.find("::");
    if (scopeIdx != std::string::npos) className = func.name.substr(0, scopeIdx);
    if (!className.empty()) {
        auto it = struct_members_.find(className);
        if (it != struct_members_.end()) {
            for (auto& [mname, mtype] : it->second)
                current_scope_->define(std::make_unique<Symbol>(mname, SymbolKind::Variable, mtype, func.line, func.column));
        }
    }

    // Define parameters
    for (size_t i = 0; i < func.parameters.size(); i++) {
        auto& p = func.parameters[i];
        if (!p->name.empty())
            current_scope_->define(std::make_unique<Symbol>(p->name, SymbolKind::Parameter, paramTypes[i], p->line, p->column));
    }

    auto prevReturn = std::move(stored_return_type_);
    stored_return_type_ = std::make_unique<TypeInfo>(returnType);
    current_function_return_type_ = stored_return_type_.get();

    for (auto& stmt : func.body->statements)
        analyze_node(*stmt);

    stored_return_type_ = std::move(prevReturn);
    current_function_return_type_ = stored_return_type_ ? stored_return_type_.get() : nullptr;
    pop_scope();
}

void SemanticAnalyzer::analyze_variable_declaration(VariableDeclaration& varDecl) {
    TypeInfo varType = resolve_type(*varDecl.type);
    if (varDecl.is_array())
        varType = TypeInfo(varType.base_name, varType.is_const, varType.is_unsigned,
                           varType.pointer_depth + 1, varType.is_reference, 4);

    if (varType.is_void())
        warning("Variable '" + varDecl.name + "' cannot have void type", varDecl.line, varDecl.column);

    if (varDecl.initializer) {
        TypeInfo initType = analyze_expression(*varDecl.initializer);
        if (!varType.is_assignable_from(initType))
            warning("Cannot initialize '" + varDecl.name + "' of type '" + varType.to_string() +
                  "' with '" + initType.to_string() + "'", varDecl.initializer->line, varDecl.initializer->column);
    }

    for (auto& e : varDecl.initializer_list)
        analyze_expression(*e);

    auto sym = std::make_unique<Symbol>(varDecl.name, SymbolKind::Variable, varType, varDecl.line, varDecl.column);
    if (!current_scope_->define(std::move(sym)) && current_scope_->parent() != nullptr)
        error("Variable '" + varDecl.name + "' already defined in this scope", varDecl.line, varDecl.column);
}

void SemanticAnalyzer::analyze_return(ReturnStatement& ret) {
    if (!current_function_return_type_) {
        error("'return' outside of a function", ret.line, ret.column);
        return;
    }
    if (ret.value) {
        TypeInfo valueType = analyze_expression(*ret.value);
        if (current_function_return_type_->is_void())
            error("Cannot return a value from void function", ret.line, ret.column);
        else if (!current_function_return_type_->is_assignable_from(valueType))
            warning("Returning '" + valueType.to_string() + "' from function returning '" +
                    current_function_return_type_->to_string() + "'", ret.value->line, ret.value->column);
    } else {
        if (!current_function_return_type_->is_void())
            warning("Non-void function should return a value", ret.line, ret.column);
    }
}

void SemanticAnalyzer::analyze_if(IfStatement& ifStmt) {
    analyze_expression(*ifStmt.condition);
    analyze_node(*ifStmt.then_branch);
    if (ifStmt.else_branch) analyze_node(*ifStmt.else_branch);
}

void SemanticAnalyzer::analyze_while(WhileStatement& whileStmt) {
    analyze_expression(*whileStmt.condition);
    loop_depth_++;
    analyze_node(*whileStmt.body);
    loop_depth_--;
}

void SemanticAnalyzer::analyze_do_while(DoWhileStatement& doWhile) {
    loop_depth_++;
    analyze_node(*doWhile.body);
    loop_depth_--;
    analyze_expression(*doWhile.condition);
}

void SemanticAnalyzer::analyze_for(ForStatement& forStmt) {
    push_scope();
    if (forStmt.init) analyze_node(*forStmt.init);
    if (forStmt.condition) analyze_expression(*forStmt.condition);
    if (forStmt.increment) analyze_expression(*forStmt.increment);
    loop_depth_++;
    analyze_node(*forStmt.body);
    loop_depth_--;
    pop_scope();
}

void SemanticAnalyzer::analyze_switch(SwitchStatement& switchStmt) {
    TypeInfo valType = analyze_expression(*switchStmt.value);
    if (!valType.is_integer() && !valType.is_pointer())
        warning("Switch value must be integer or enum", switchStmt.line, switchStmt.column);

    switch_depth_++;
    for (auto& c : switchStmt.cases) {
        if (c->value) analyze_expression(*c->value);
        for (auto& s : c->body) analyze_node(*s);
    }
    switch_depth_--;
}

void SemanticAnalyzer::analyze_try_catch(TryCatchStatement& tryCatch) {
    push_scope();
    for (auto& s : tryCatch.try_body->statements) analyze_node(*s);
    pop_scope();

    bool hasCatchAll = false;
    for (auto& cc : tryCatch.catch_clauses) {
        if (hasCatchAll) warning("Catch after catch-all will never execute", cc->line, cc->column);
        if (cc->is_catch_all) hasCatchAll = true;
        push_scope();
        if (!cc->is_catch_all && cc->exception_type && !cc->variable_name.empty()) {
            TypeInfo exType = resolve_type(*cc->exception_type);
            current_scope_->define(std::make_unique<Symbol>(cc->variable_name, SymbolKind::Variable, exType, cc->line, cc->column));
        }
        for (auto& s : cc->body->statements) analyze_node(*s);
        pop_scope();
    }
}

void SemanticAnalyzer::analyze_throw(ThrowStatement& throwStmt) {
    if (throwStmt.value) analyze_expression(*throwStmt.value);
}

// --- Expression analysis ---

TypeInfo SemanticAnalyzer::analyze_expression(Expression& expr) {
    if (dynamic_cast<IntegerLiteral*>(&expr)) return TypeInfo("int", false, false, 0, false, 4);
    if (auto* fl = dynamic_cast<FloatLiteral*>(&expr)) {
        if (!fl->value.empty() && (fl->value.back() == 'f' || fl->value.back() == 'F'))
            return TypeInfo("float", false, false, 0, false, 4);
        return TypeInfo("double", false, false, 0, false, 8);
    }
    if (dynamic_cast<StringLiteralExpr*>(&expr)) return TypeInfo("char", true, false, 1, false, POINTER_SIZE);
    if (dynamic_cast<CharLiteralExpr*>(&expr)) return TypeInfo("char", false, false, 0, false, 1);
    if (dynamic_cast<BoolLiteral*>(&expr)) return TypeInfo("bool", false, false, 0, false, 1);
    if (auto* id = dynamic_cast<IdentifierExpr*>(&expr)) return analyze_identifier(*id);
    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) return analyze_binary(*bin);
    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) return analyze_unary(*un);
    if (auto* call = dynamic_cast<CallExpr*>(&expr)) return analyze_call(*call);
    if (auto* idx = dynamic_cast<IndexExpr*>(&expr)) return analyze_index(*idx);
    if (auto* mem = dynamic_cast<MemberAccessExpr*>(&expr)) return analyze_member_access(*mem);
    if (auto* cast = dynamic_cast<CastExpr*>(&expr)) {
        analyze_expression(*cast->operand);
        return resolve_type(*cast->target_type);
    }
    if (dynamic_cast<SizeofExpr*>(&expr)) return TypeInfo("int", false, true, 0, false, 4);
    if (auto* tern = dynamic_cast<TernaryExpr*>(&expr)) return analyze_ternary(*tern);
    if (dynamic_cast<ThisExpr*>(&expr)) return TypeInfo("void", false, false, 1, false, POINTER_SIZE);
    if (auto* initList = dynamic_cast<InitializerListExpr*>(&expr)) {
        for (auto& e : initList->values) analyze_expression(*e);
        return TypeInfo("void", false, false, 0, false, 0);
    }
    if (auto* newE = dynamic_cast<NewExpr*>(&expr)) {
        for (auto& a : newE->constructor_args) analyze_expression(*a);
        if (newE->array_size) analyze_expression(*newE->array_size);
        TypeInfo allocType = resolve_type(*newE->alloc_type);
        return TypeInfo(allocType.base_name, false, allocType.is_unsigned, allocType.pointer_depth + 1, false, POINTER_SIZE);
    }
    if (auto* delE = dynamic_cast<DeleteExpr*>(&expr)) {
        analyze_expression(*delE->operand);
        return TypeInfo("void", false, false, 0, false, 0);
    }
    if (auto* scope = dynamic_cast<ScopeResolutionExpr*>(&expr)) {
        Symbol* sym = current_scope_->lookup(scope->name);
        if (sym) return sym->type;
        return TypeInfo("int", false, false, 0, false, 4);
    }
    return TypeInfo("int", false, false, 0, false, 4);
}

TypeInfo SemanticAnalyzer::analyze_identifier(IdentifierExpr& id) {
    Symbol* sym = current_scope_->lookup(id.name);
    if (!sym) {
        warning("Undeclared identifier '" + id.name + "'", id.line, id.column);
        return TypeInfo("int", false, false, 0, false, 4);
    }
    return sym->type;
}

TypeInfo SemanticAnalyzer::analyze_binary(BinaryExpr& bin) {
    TypeInfo leftType = analyze_expression(*bin.left);
    TypeInfo rightType = analyze_expression(*bin.right);

    if (bin.op == "=" || bin.op == "+=" || bin.op == "-=" || bin.op == "*=" ||
        bin.op == "/=" || bin.op == "%=" || bin.op == "&=" || bin.op == "|=" ||
        bin.op == "^=" || bin.op == "<<=" || bin.op == ">>=") {
        if (leftType.is_const) warning("Cannot assign to const variable", bin.left->line, bin.left->column);
        if (bin.op == "=" && !leftType.is_assignable_from(rightType))
            warning("Assigning '" + rightType.to_string() + "' to '" + leftType.to_string() + "'", bin.line, bin.column);
        return leftType;
    }
    if (bin.op == "==" || bin.op == "!=" || bin.op == "<" || bin.op == ">" ||
        bin.op == "<=" || bin.op == ">=" || bin.op == "&&" || bin.op == "||")
        return TypeInfo("bool", false, false, 0, false, 1);
    if (bin.op == "<<" || bin.op == ">>") return leftType;
    if (bin.op == "&" || bin.op == "|" || bin.op == "^") return leftType;
    if (bin.op == "+" || bin.op == "-" || bin.op == "*" || bin.op == "/" || bin.op == "%") {
        if (leftType.is_pointer() && rightType.is_integer()) return leftType;
        if (leftType.is_integer() && rightType.is_pointer()) return rightType;
        if (leftType.is_pointer() && rightType.is_pointer() && bin.op == "-")
            return TypeInfo("int", false, false, 0, false, 4);
        if (leftType.is_floating_point() || rightType.is_floating_point()) {
            if (leftType.base_name == "double" || rightType.base_name == "double")
                return TypeInfo("double", false, false, 0, false, 8);
            return TypeInfo("float", false, false, 0, false, 4);
        }
        return leftType.size >= rightType.size ? leftType : rightType;
    }
    return leftType;
}

TypeInfo SemanticAnalyzer::analyze_unary(UnaryExpr& un) {
    TypeInfo operandType = analyze_expression(*un.operand);
    if (un.op == "!") return TypeInfo("bool", false, false, 0, false, 1);
    if (un.op == "~") return operandType;
    if (un.op == "-" || un.op == "+") return operandType;
    if (un.op == "++" || un.op == "--") return operandType;
    if (un.op == "*") {
        if (!operandType.is_pointer()) { warning("Dereference of non-pointer", un.line, un.column); return operandType; }
        return TypeInfo(operandType.base_name, operandType.is_const, operandType.is_unsigned,
                        operandType.pointer_depth - 1, false,
                        operandType.pointer_depth - 1 > 0 ? POINTER_SIZE : get_type_size(operandType.base_name));
    }
    if (un.op == "&")
        return TypeInfo(operandType.base_name, operandType.is_const, operandType.is_unsigned,
                        operandType.pointer_depth + 1, false, POINTER_SIZE);
    return operandType;
}

TypeInfo SemanticAnalyzer::analyze_call(CallExpr& call) {
    TypeInfo calleeType = analyze_expression(*call.callee);
    if (auto* id = dynamic_cast<IdentifierExpr*>(call.callee.get())) {
        Symbol* sym = current_scope_->lookup(id->name);
        if (sym && sym->func_signature) {
            auto& sig = *sym->func_signature;
            if (call.arguments.size() != sig.parameter_types.size())
                warning("Function '" + id->name + "' expects " + std::to_string(sig.parameter_types.size()) +
                      " args, got " + std::to_string(call.arguments.size()), call.line, call.column);
            else {
                for (size_t i = 0; i < call.arguments.size(); i++) {
                    TypeInfo argType = analyze_expression(*call.arguments[i]);
                    if (!sig.parameter_types[i].is_assignable_from(argType))
                        warning("Arg " + std::to_string(i + 1) + " of '" + id->name + "': passing '" +
                                argType.to_string() + "' to '" + sig.parameter_types[i].to_string() + "'",
                                call.arguments[i]->line, call.arguments[i]->column);
                }
            }
            return sig.return_type;
        }
    }
    for (auto& arg : call.arguments) analyze_expression(*arg);
    return TypeInfo("int", false, false, 0, false, 4);
}

TypeInfo SemanticAnalyzer::analyze_index(IndexExpr& idx) {
    TypeInfo objType = analyze_expression(*idx.object);
    TypeInfo indexType = analyze_expression(*idx.index);
    if (!indexType.is_integer()) warning("Array index must be integer", idx.index->line, idx.index->column);
    if (objType.is_pointer())
        return TypeInfo(objType.base_name, objType.is_const, objType.is_unsigned,
                        objType.pointer_depth - 1, false,
                        objType.pointer_depth - 1 > 0 ? POINTER_SIZE : get_type_size(objType.base_name));
    warning("Subscript requires pointer/array type", idx.object->line, idx.object->column);
    return objType;
}

TypeInfo SemanticAnalyzer::analyze_member_access(MemberAccessExpr& mem) {
    TypeInfo objType = analyze_expression(*mem.object);
    if (mem.is_arrow && !objType.is_pointer())
        error("'->' requires pointer type", mem.object->line, mem.object->column);

    std::string typeName = objType.base_name;
    std::string resolved = typeName;
    int maxChain = 10;
    while (maxChain-- > 0 && struct_members_.find(resolved) == struct_members_.end()) {
        auto it = typedef_map_.find(resolved);
        if (it == typedef_map_.end()) break;
        resolved = it->second.base_name;
    }

    auto it = struct_members_.find(resolved);
    if (it != struct_members_.end()) {
        for (auto& [mname, mtype] : it->second)
            if (mname == mem.member_name) return mtype;
        warning("'" + typeName + "' has no member '" + mem.member_name + "'", mem.line, mem.column);
    }
    return TypeInfo("int", false, false, 0, false, 4);
}

TypeInfo SemanticAnalyzer::analyze_ternary(TernaryExpr& tern) {
    analyze_expression(*tern.condition);
    TypeInfo thenType = analyze_expression(*tern.then_expr);
    TypeInfo elseType = analyze_expression(*tern.else_expr);
    if (thenType.is_floating_point() || elseType.is_floating_point()) {
        if (thenType.base_name == "double" || elseType.base_name == "double")
            return TypeInfo("double", false, false, 0, false, 8);
        return TypeInfo("float", false, false, 0, false, 4);
    }
    return thenType;
}

} // namespace nexia
