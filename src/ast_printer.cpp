// NexiaCompiler v2.0 — AST Printer
// Ported from AstPrinter.cs — FULL implementation
#include "ast_printer.h"

namespace nexia {

void AstPrinter::print_line(const std::string& text) {
    for(int i=0;i<indent_;i++)std::cout<<"  ";
    std::cout<<text<<"\n";
}

void AstPrinter::indented(std::function<void()> action) { indent_++; action(); indent_--; }

void AstPrinter::print(const AstNode& node) {
    if(auto*tu=dynamic_cast<const TranslationUnit*>(&node)){print_line("TranslationUnit");indented([&]{for(auto&d:tu->declarations)print(*d);});return;}
    if(auto*pp=dynamic_cast<const PreprocessorDirective*>(&node)){print_line("Preprocessor: "+pp->text);return;}
    if(auto*fn=dynamic_cast<const FunctionDefinition*>(&node)){print_line("Function: "+fn->return_type->to_string()+" "+fn->name+"()");indented([&]{for(auto&p:fn->parameters)print_line("Param: "+p->type->to_string()+" "+p->name);if(fn->body)print(*fn->body);});return;}
    if(auto*vd=dynamic_cast<const VariableDeclaration*>(&node)){print_line("VarDecl: "+vd->type->to_string()+" "+vd->name);if(vd->initializer)indented([&]{print_line("Init:");indented([&]{print_expr(*vd->initializer);});});return;}
    if(auto*bs=dynamic_cast<const BlockStatement*>(&node)){print_line("Block");indented([&]{for(auto&s:bs->statements)print(*s);});return;}
    if(auto*es=dynamic_cast<const ExpressionStatement*>(&node)){print_line("ExprStmt");indented([&]{print_expr(*es->expr);});return;}
    if(auto*rs=dynamic_cast<const ReturnStatement*>(&node)){print_line("Return");if(rs->value)indented([&]{print_expr(*rs->value);});return;}
    if(auto*is=dynamic_cast<const IfStatement*>(&node)){print_line("If");indented([&]{print_line("Cond:");indented([&]{print_expr(*is->condition);});print_line("Then:");indented([&]{print(*is->then_branch);});if(is->else_branch){print_line("Else:");indented([&]{print(*is->else_branch);});}});return;}
    if(auto*ws=dynamic_cast<const WhileStatement*>(&node)){print_line("While");indented([&]{print_line("Cond:");indented([&]{print_expr(*ws->condition);});print_line("Body:");indented([&]{print(*ws->body);});});return;}
    if(auto*fs=dynamic_cast<const ForStatement*>(&node)){print_line("For");indented([&]{if(fs->init){print_line("Init:");indented([&]{print(*fs->init);});}if(fs->condition){print_line("Cond:");indented([&]{print_expr(*fs->condition);});}if(fs->increment){print_line("Incr:");indented([&]{print_expr(*fs->increment);});}print_line("Body:");indented([&]{print(*fs->body);});});return;}
    if(auto*sw=dynamic_cast<const SwitchStatement*>(&node)){print_line("Switch");indented([&]{print_expr(*sw->value);for(auto&c:sw->cases){if(c->value){print_line("Case:");indented([&]{print_expr(*c->value);});}else print_line("Default:");indented([&]{for(auto&s:c->body)print(*s);});}});return;}
    if(dynamic_cast<const BreakStatement*>(&node)){print_line("Break");return;}
    if(dynamic_cast<const ContinueStatement*>(&node)){print_line("Continue");return;}
    if(auto*cd=dynamic_cast<const ClassDefinition*>(&node)){print_line("Class: "+cd->name+(cd->is_struct?" (struct)":""));return;}
    if(auto*ed=dynamic_cast<const EnumDefinition*>(&node)){print_line("Enum: "+ed->name);indented([&]{for(auto&v:ed->values)print_line(v.name);});return;}
    if(auto*nd=dynamic_cast<const NamespaceDefinition*>(&node)){print_line("Namespace: "+nd->name);indented([&]{for(auto&d:nd->declarations)print(*d);});return;}
    if(auto*td=dynamic_cast<const TypedefDeclaration*>(&node)){print_line("Typedef: "+td->original_type->to_string()+" -> "+td->alias_name);return;}
    if(auto*ud=dynamic_cast<const UsingDirective*>(&node)){print_line("Using namespace: "+ud->namespace_name);return;}
    if(auto*tmpl=dynamic_cast<const TemplateDeclaration*>(&node)){print_line("Template");indented([&]{print(*tmpl->inner_declaration);});return;}
    print_line("Unknown node");
}

void AstPrinter::print_expr(const Expression& expr) {
    if(auto*il=dynamic_cast<const IntegerLiteral*>(&expr)){print_line("Int: "+il->value);return;}
    if(auto*fl=dynamic_cast<const FloatLiteral*>(&expr)){print_line("Float: "+fl->value);return;}
    if(auto*sl=dynamic_cast<const StringLiteralExpr*>(&expr)){print_line("String: \""+sl->value+"\"");return;}
    if(auto*cl=dynamic_cast<const CharLiteralExpr*>(&expr)){print_line("Char: '"+cl->value+"'");return;}
    if(auto*bl=dynamic_cast<const BoolLiteral*>(&expr)){print_line(std::string("Bool: ")+(bl->value?"true":"false"));return;}
    if(auto*id=dynamic_cast<const IdentifierExpr*>(&expr)){print_line("Id: "+id->name);return;}
    if(auto*bin=dynamic_cast<const BinaryExpr*>(&expr)){print_line("Binary: "+bin->op);indented([&]{print_expr(*bin->left);print_expr(*bin->right);});return;}
    if(auto*un=dynamic_cast<const UnaryExpr*>(&expr)){print_line("Unary: "+un->op+(un->is_prefix?" (prefix)":" (postfix)"));indented([&]{print_expr(*un->operand);});return;}
    if(auto*call=dynamic_cast<const CallExpr*>(&expr)){print_line("Call");indented([&]{print_line("Callee:");indented([&]{print_expr(*call->callee);});for(auto&a:call->arguments){print_line("Arg:");indented([&]{print_expr(*a);});}});return;}
    if(auto*idx=dynamic_cast<const IndexExpr*>(&expr)){print_line("Index");indented([&]{print_expr(*idx->object);print_expr(*idx->index);});return;}
    if(auto*mem=dynamic_cast<const MemberAccessExpr*>(&expr)){print_line(std::string("Member: ")+(mem->is_arrow?"->":".")+mem->member_name);indented([&]{print_expr(*mem->object);});return;}
    if(auto*cast=dynamic_cast<const CastExpr*>(&expr)){print_line("Cast: "+cast->target_type->to_string());indented([&]{print_expr(*cast->operand);});return;}
    if(auto*tern=dynamic_cast<const TernaryExpr*>(&expr)){print_line("Ternary");indented([&]{print_expr(*tern->condition);print_expr(*tern->then_expr);print_expr(*tern->else_expr);});return;}
    if(dynamic_cast<const ThisExpr*>(&expr)){print_line("this");return;}
    if(auto*sr=dynamic_cast<const ScopeResolutionExpr*>(&expr)){print_line("Scope: "+sr->scope+"::"+sr->name);return;}
    if(auto*ne=dynamic_cast<const NewExpr*>(&expr)){print_line("New: "+ne->alloc_type->to_string());return;}
    if(dynamic_cast<const DeleteExpr*>(&expr)){print_line("Delete");return;}
    if(dynamic_cast<const SizeofExpr*>(&expr)){print_line("Sizeof");return;}
    print_line("Expr(unknown)");
}

} // namespace nexia
