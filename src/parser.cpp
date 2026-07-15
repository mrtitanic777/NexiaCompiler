// NexiaCompiler v2.0 — Parser
// Ported from Parser.cs — FULL implementation
// Recursive descent with Pratt parsing for expressions.

#include <memory>
#include "parser.h"
#include <iostream>

namespace nexia {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), pos_(0) {}

const Token& Parser::current() const {
    if (pos_ < (int)tokens_.size()) return tokens_[pos_];
    return tokens_.back();
}
const Token& Parser::peek(int offset) const {
    int i = pos_ + offset;
    if (i < (int)tokens_.size()) return tokens_[i];
    return tokens_.back();
}
Token Parser::advance() {
    auto t = current();
    if (t.type != TokenType::Eof) pos_++;
    return t;
}
bool Parser::check(TokenType type) const { return current().type == type; }
bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}
Token Parser::expect(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    auto t = current();
    throw ParseError(msg + " (got " + token_type_name(t.type) + " \"" + t.value + "\")", t.line, t.column);
}

bool Parser::is_type_keyword(TokenType type) const {
    return type == TokenType::KW_Void || type == TokenType::KW_Bool ||
           type == TokenType::KW_Char || type == TokenType::KW_Short ||
           type == TokenType::KW_Int || type == TokenType::KW_Long ||
           type == TokenType::KW_Float || type == TokenType::KW_Double ||
           type == TokenType::KW_Signed || type == TokenType::KW_Unsigned ||
           type == TokenType::KW_Const || type == TokenType::KW_Volatile ||
           type == TokenType::KW_WcharT || type == TokenType::KW_Struct ||
           type == TokenType::KW_Class || type == TokenType::KW_Enum;
}

bool Parser::is_at_type_specifier() const {
    if (is_type_keyword(current().type)) return true;
    if (current().type == TokenType::Identifier && known_types_.count(current().value)) return true;
    if (current().type == TokenType::OpScope) return true;
    // Heuristic: Identifier followed by * or another Identifier is likely a type
    // e.g., D3DDevice* g_pd3dDevice;  or  XMMATRIX g_matWorld;
    if (current().type == TokenType::Identifier) {
        auto next = peek().type;
        if (next == TokenType::OpStar || next == TokenType::Identifier ||
            next == TokenType::OpBitAnd)
            return true;
    }
    return false;
}

bool Parser::looks_like_cast() {
    int i = pos_; int count = (int)tokens_.size();
    bool sawBase = false, sawPtr = false;
    while (i < count) {
        auto t = tokens_[i].type;
        if (t==TokenType::KW_Const||t==TokenType::KW_Volatile||t==TokenType::KW_Signed||
            t==TokenType::KW_Unsigned||t==TokenType::KW_Static||t==TokenType::KW_Extern||
            t==TokenType::KW_Register||t==TokenType::KW_Inline) { i++; continue; }
        if (is_type_keyword(t)&&t!=TokenType::KW_Const&&t!=TokenType::KW_Volatile) { sawBase=true; i++; continue; }
        if ((t==TokenType::KW_Struct||t==TokenType::KW_Class||t==TokenType::KW_Enum||t==TokenType::KW_Union)&&!sawBase) {
            sawBase=true; i++; if(i<count&&tokens_[i].type==TokenType::Identifier)i++; continue; }
        if (t==TokenType::Identifier&&!sawBase) { sawBase=true; i++;
            while(i+1<count&&tokens_[i].type==TokenType::OpScope&&tokens_[i+1].type==TokenType::Identifier) i+=2;
            continue; }
        if (sawBase&&(t==TokenType::OpStar||t==TokenType::OpBitAnd)) { sawPtr=true; i++; continue; }
        if (t==TokenType::RParen&&sawBase) {
            if (!sawPtr&&!is_type_keyword(tokens_[pos_].type)&&
                !(tokens_[pos_].type==TokenType::Identifier&&known_types_.count(tokens_[pos_].value))) return false;
            return true; }
        return false;
    }
    return false;
}

void Parser::skip_to_semicolon_or_brace() {
    while (!check(TokenType::Eof)) {
        if (check(TokenType::Semicolon)) { advance(); return; }
        if (check(TokenType::RBrace)) return;
        advance();
    }
}

std::string Parser::skip_function_pointer_declarator() {
    std::string name;
    advance();
    while (check(TokenType::OpStar)||check(TokenType::KW_Const)||check(TokenType::KW_Volatile)||
           (check(TokenType::Identifier)&&(current().value=="__cdecl"||current().value=="__stdcall"||
            current().value=="__fastcall"||current().value=="CALLBACK"||current().value=="WINAPI")))
        advance();
    if (check(TokenType::Identifier)) name = advance().value;
    expect(TokenType::RParen, "Expected ')' in fptr declarator");
    if (check(TokenType::LParen)) {
        advance(); int d=1;
        while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LParen))d++;if(check(TokenType::RParen))d--;if(d>0)advance();}
        expect(TokenType::RParen, "Expected ')'");
    }
    while (check(TokenType::LBracket)) {
        advance(); int d=1;
        while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LBracket))d++;if(check(TokenType::RBracket))d--;if(d>0)advance();}
        expect(TokenType::RBracket, "Expected ']'");
    }
    return name;
}

std::vector<std::unique_ptr<Parameter>> Parser::parse_parameter_list() {
    std::vector<std::unique_ptr<Parameter>> params;
    if (!check(TokenType::RParen)) {
        do {
            if (check(TokenType::Ellipsis)) {
                advance();
                auto vt = std::make_unique<TypeNode>("...",false,false,false,0,false,current().line,current().column);
                params.push_back(std::make_unique<Parameter>(std::move(vt),"",current().line,current().column));
                break;
            }
            int pl=current().line, pc=current().column;
            auto pt = parse_type();
            std::string pn;
            if (check(TokenType::LParen)) { pn = skip_function_pointer_declarator(); }
            else {
                if (check(TokenType::Identifier)) pn = advance().value;
                while (check(TokenType::LBracket)) { advance(); if(!check(TokenType::RBracket))parse_expression(); expect(TokenType::RBracket,"Expected ']'"); }
            }
            if (match(TokenType::OpAssign)) parse_assignment();
            params.push_back(std::make_unique<Parameter>(std::move(pt),pn,pl,pc));
        } while (match(TokenType::Comma));
    }
    return params;
}

// -----------------------------------------------------------------------
// Top-level parse()
// -----------------------------------------------------------------------

std::unique_ptr<TranslationUnit> Parser::parse() {
    std::vector<AstPtr> decls;
    int lastPct = -1, tc = (int)tokens_.size();
    while (!check(TokenType::Eof)) {
        int pct = (int)((long)pos_*100/tc);
        if (pct>=lastPct+2) { std::cout<<" "<<pct<<"%"<<std::flush; lastPct=pct; }
        if (check(TokenType::Semicolon)) { advance(); continue; }
        if (check(TokenType::RBrace)) { advance(); continue; }
        if (check(TokenType::LBracket)) {
            int d=0; while(!check(TokenType::Eof)){if(check(TokenType::LBracket))d++;if(check(TokenType::RBracket))d--;advance();if(d<=0)break;} continue; }
        if (check(TokenType::Preprocessor)) { auto t=advance(); decls.push_back(std::make_unique<PreprocessorDirective>(t.value,t.line,t.column)); continue; }
        if (check(TokenType::KW_Extern)) {
            int save=pos_; advance();
            if (check(TokenType::StringLiteral)&&(current().value=="C"||current().value=="C++")) {
                std::string lnk=current().value; advance();
                if (check(TokenType::LBrace)) {
                    if (lnk=="C++") { advance(); int bd=1; while(bd>0&&!check(TokenType::Eof)){if(check(TokenType::LBrace))bd++;if(check(TokenType::RBrace))bd--;if(bd>0)advance();} if(check(TokenType::RBrace))advance(); }
                    else advance();
                } else if (lnk=="C++") { int bd=0; while(!check(TokenType::Eof)){if(check(TokenType::LBrace))bd++;if(check(TokenType::RBrace)){if(bd>0)bd--;else break;}if(check(TokenType::Semicolon)&&bd==0){advance();break;}advance();} }
                continue;
            } else pos_=save;
        }
        if (check(TokenType::KW_Namespace)) { decls.push_back(parse_namespace()); continue; }
        if (check(TokenType::KW_Using)) { decls.push_back(parse_using_statement()); continue; }
        if (check(TokenType::KW_Typedef)) { decls.push_back(parse_typedef()); continue; }
        if (check(TokenType::KW_Class)||check(TokenType::KW_Struct)) { decls.push_back(parse_class_definition()); continue; }
        if (check(TokenType::KW_Union)) { decls.push_back(parse_union_definition()); continue; }
        if (check(TokenType::KW_Enum)) { decls.push_back(parse_enum_definition()); continue; }
        if (check(TokenType::KW_Template)) { decls.push_back(parse_template_declaration()); continue; }
        if (check(TokenType::KW_Extern)||check(TokenType::KW_Static)||check(TokenType::KW_Volatile)||check(TokenType::KW_Inline)) { advance(); continue; }
        if (check(TokenType::Identifier)&&(current().value=="__cdecl"||current().value=="__stdcall"||current().value=="__fastcall"||current().value=="WINAPI"||current().value=="CALLBACK")) { advance(); continue; }
        if (is_at_type_specifier()&&is_operator_overload_ahead()) { decls.push_back(parse_global_operator_overload()); continue; }
        try { decls.push_back(parse_declaration()); }
        catch (const ParseError&) { while(!check(TokenType::Semicolon)&&!check(TokenType::RBrace)&&!check(TokenType::Eof))advance(); if(check(TokenType::Semicolon))advance(); }
    }
    for (auto& e : typedef_enums_) decls.push_back(std::move(e));
    for (auto& s : typedef_structs_) decls.push_back(std::move(s));
    for (auto& a : pending_anonymous_decls_) decls.push_back(std::move(a));
    return std::make_unique<TranslationUnit>(std::move(decls), 1, 1);
}

// -----------------------------------------------------------------------
// Type parsing
// -----------------------------------------------------------------------

TypePtr Parser::parse_type() {
    int sL=current().line, sC=current().column;
    bool isConst=false, isUnsigned=false, isSigned=false;
    std::string baseName;
    bool done=false;
    while (!done) {
        switch (current().type) {
        case TokenType::KW_Const: isConst=true; advance(); break;
        case TokenType::KW_Volatile: advance(); break;
        case TokenType::KW_Unsigned: isUnsigned=true; advance(); break;
        case TokenType::KW_Signed: isSigned=true; advance(); break;
        case TokenType::KW_Static: case TokenType::KW_Extern: case TokenType::KW_Inline: case TokenType::KW_Register: advance(); break;
        case TokenType::KW_Void: case TokenType::KW_Bool: case TokenType::KW_Char: case TokenType::KW_Short:
        case TokenType::KW_Int: case TokenType::KW_Long: case TokenType::KW_Float: case TokenType::KW_Double: case TokenType::KW_WcharT:
            if (!baseName.empty()) baseName += " ";
                baseName += advance().value;
                break;
        case TokenType::KW_Struct: case TokenType::KW_Class: case TokenType::KW_Enum: case TokenType::KW_Union:
        {
            advance();
            if (check(TokenType::Identifier)) {
                baseName = advance().value;
                known_types_.insert(baseName);
            } else if (check(TokenType::LBrace)) {
                // Anonymous struct/union/enum — skip the body, generate a placeholder name
                std::string anon = "__anon_" + std::to_string(sL) + "_" + std::to_string(sC);
                advance(); // consume {
                int depth = 1;
                while (depth > 0 && !check(TokenType::Eof)) {
                    if (check(TokenType::LBrace)) depth++;
                    if (check(TokenType::RBrace)) depth--;
                    if (depth > 0) advance();
                }
                if (check(TokenType::RBrace)) advance();
                baseName = anon;
            }
            done = true;
            break;
        }
        case TokenType::Identifier:
            if (baseName.empty()) {
                baseName=advance().value;
                while(check(TokenType::OpScope)){
                    if(pos_+2<(int)tokens_.size()&&tokens_[pos_+1].type==TokenType::Identifier&&tokens_[pos_+2].type==TokenType::LParen)break;
                    if(pos_+1<(int)tokens_.size()&&tokens_[pos_+1].type==TokenType::OpBitNot)break;
                    if(pos_+1<(int)tokens_.size()&&tokens_[pos_+1].type==TokenType::KW_Operator)break;
                    advance(); if(check(TokenType::Identifier)) baseName=advance().value;
                }
            }
            done=true; break;
        case TokenType::OpScope:
            if (baseName.empty()) { advance(); if(check(TokenType::Identifier)){baseName=advance().value;while(check(TokenType::OpScope)){advance();if(check(TokenType::Identifier))baseName=advance().value;}}}
            done=true; break;
        default: done=true; break;
        }
    }
    if (baseName.empty()&&(isUnsigned||isSigned)) baseName="int";
    if (baseName.empty()) { auto t=current(); throw ParseError("Expected type name, got "+std::string(token_type_name(t.type))+" \""+t.value+"\"",t.line,t.column); }

    // Template arguments
    std::vector<std::unique_ptr<TemplateArgument>>* targs = nullptr;
    if (check(TokenType::OpLess)&&(known_types_.count(baseName)||template_names_.count(baseName)))
        targs = try_parse_template_arguments();

    while(check(TokenType::KW_Const)||check(TokenType::KW_Volatile)){if(check(TokenType::KW_Const))isConst=true;advance();}
    int ptrD=0;
    while(check(TokenType::OpStar)){advance();ptrD++;while(check(TokenType::KW_Const)||check(TokenType::KW_Volatile)){if(check(TokenType::KW_Const))isConst=true;advance();}}
    while(check(TokenType::KW_Const)||check(TokenType::KW_Volatile)){if(check(TokenType::KW_Const))isConst=true;advance();}
    bool isRef=false; if(check(TokenType::OpBitAnd)){advance();isRef=true;}

    if (targs) {
        auto r=std::make_unique<TemplatedTypeNode>(baseName,isConst,isUnsigned,isSigned,ptrD,isRef,std::move(*targs),sL,sC);
        delete targs; return r;
    }
    return std::make_unique<TypeNode>(baseName,isConst,isUnsigned,isSigned,ptrD,isRef,sL,sC);
}

// -----------------------------------------------------------------------
// Declarations
// -----------------------------------------------------------------------

AstPtr Parser::parse_declaration() {
    int sL=current().line, sC=current().column;
    while(check(TokenType::KW_Static)||check(TokenType::KW_Extern)||check(TokenType::KW_Volatile)||check(TokenType::KW_Inline)) advance();
    auto type = parse_type();
    while(check(TokenType::Identifier)&&(current().value=="__cdecl"||current().value=="__stdcall"||current().value=="__fastcall"||current().value=="WINAPI"||current().value=="CALLBACK")) advance();

    // Handle out-of-class method: Type::Name or Type::~Name or Type::operator
    if (check(TokenType::OpScope)) {
        Token nameT(TokenType::Identifier, type->base_name, type->line, type->column);
        advance(); // ::
        if (check(TokenType::OpBitNot)) {
            advance(); if(check(TokenType::Identifier))advance();
            if(check(TokenType::LParen)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LParen))d++;if(check(TokenType::RParen))d--;if(d>0)advance();}if(check(TokenType::RParen))advance();}
            BlockPtr body; if(check(TokenType::LBrace))body=parse_block(); else{if(check(TokenType::Semicolon))advance();body=std::make_unique<BlockStatement>(std::vector<AstPtr>{},sL,sC);}
            return std::make_unique<FunctionDefinition>(std::move(type),nameT.value+"::~"+nameT.value,std::vector<std::unique_ptr<Parameter>>{},std::move(body),sL,sC);
        }
        if (check(TokenType::KW_Operator)) {
            std::string op=parse_operator_symbol(); std::string fn=nameT.value+"::operator"+op;
            Token syn(TokenType::Identifier,fn,sL,sC);
            if(check(TokenType::LParen)) return parse_function_definition(std::move(type),syn);
            expect(TokenType::Semicolon,"Expected ';'");
            return std::make_unique<FunctionDefinition>(std::move(type),fn,std::vector<std::unique_ptr<Parameter>>{},std::make_unique<BlockStatement>(std::vector<AstPtr>{},sL,sC),sL,sC);
        }
        Token meth=expect(TokenType::Identifier,"Expected method name after '::'");
        while(check(TokenType::OpScope)){advance();meth=expect(TokenType::Identifier,"Expected name after '::'");}
        if(check(TokenType::LParen)) return parse_function_definition(std::move(type),meth);
        return parse_variable_declaration(std::move(type),meth);
    }

    Token nameToken = expect(TokenType::Identifier, "Expected name after type");

    if (check(TokenType::OpScope)) {
        advance();
        if (check(TokenType::OpBitNot)) {
            advance(); if(check(TokenType::Identifier))advance();
            if(check(TokenType::LParen)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LParen))d++;if(check(TokenType::RParen))d--;if(d>0)advance();}if(check(TokenType::RParen))advance();}
            BlockPtr body; if(check(TokenType::LBrace))body=parse_block(); else{if(check(TokenType::Semicolon))advance();body=std::make_unique<BlockStatement>(std::vector<AstPtr>{},sL,sC);}
            return std::make_unique<FunctionDefinition>(std::move(type),nameToken.value+"::~"+nameToken.value,std::vector<std::unique_ptr<Parameter>>{},std::move(body),sL,sC);
        }
        if (check(TokenType::KW_Operator)) {
            std::string op=parse_operator_symbol(); std::string fn=nameToken.value+"::operator"+op;
            Token syn(TokenType::Identifier,fn,sL,sC);
            if(check(TokenType::LParen)) return parse_function_definition(std::move(type),syn);
            expect(TokenType::Semicolon,"Expected ';'");
            return std::make_unique<FunctionDefinition>(std::move(type),fn,std::vector<std::unique_ptr<Parameter>>{},std::make_unique<BlockStatement>(std::vector<AstPtr>{},sL,sC),sL,sC);
        }
        Token meth=expect(TokenType::Identifier,"Expected method name");
        while(check(TokenType::OpScope)){advance();meth=expect(TokenType::Identifier,"Expected name after '::'");}
        if(check(TokenType::LParen)) return parse_function_definition(std::move(type),meth);
        return parse_variable_declaration(std::move(type),meth);
    }

    if (check(TokenType::LParen)) return parse_function_definition(std::move(type), nameToken);
    if (check(TokenType::OpLess)) {
        advance(); int d=1;
        while(d>0&&!check(TokenType::Eof)){if(check(TokenType::OpLess))d++;if(check(TokenType::OpGreater))d--;if(check(TokenType::OpRShift))d-=2;if(d>0)advance();}
        if(check(TokenType::OpGreater))advance();
        if(check(TokenType::LParen)) return parse_function_definition(std::move(type),nameToken);
        return parse_variable_declaration(std::move(type),nameToken);
    }
    return parse_variable_declaration(std::move(type), nameToken);
}

std::unique_ptr<FunctionDefinition> Parser::parse_function_definition(TypePtr returnType, Token name) {
    expect(TokenType::LParen,"Expected '('");
    auto params = parse_parameter_list();
    expect(TokenType::RParen,"Expected ')'");
    while(check(TokenType::KW_Const)||check(TokenType::KW_Volatile))advance();
    if(check(TokenType::KW_Throw)){advance();if(check(TokenType::LParen)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LParen))d++;if(check(TokenType::RParen))d--;if(d>0)advance();}if(check(TokenType::RParen))advance();}}
    if(check(TokenType::Colon)){advance();while(!check(TokenType::LBrace)&&!check(TokenType::Semicolon)&&!check(TokenType::Eof))advance();}
    if(check(TokenType::Semicolon)){advance();return std::make_unique<FunctionDefinition>(std::move(returnType),name.value,std::move(params),std::make_unique<BlockStatement>(std::vector<AstPtr>{},name.line,name.column),name.line,name.column);}
    auto body = parse_block();
    return std::make_unique<FunctionDefinition>(std::move(returnType),name.value,std::move(params),std::move(body),name.line,name.column);
}

std::unique_ptr<VariableDeclaration> Parser::parse_variable_declaration(TypePtr type, Token name) {
    ExprPtr arraySize, init;
    std::vector<ExprPtr> initList;
    while(check(TokenType::LBracket)){
        advance();
        if(!check(TokenType::RBracket)){if(!arraySize)arraySize=parse_expression();else parse_expression();}
        else if(!arraySize) arraySize=std::make_unique<IntegerLiteral>("0",name.line,name.column);
        expect(TokenType::RBracket,"Expected ']'");
    }
    if(match(TokenType::OpAssign)){
        if(check(TokenType::LBrace)){
            advance();
            if(!check(TokenType::RBrace)){do{if(check(TokenType::RBrace))break;if(check(TokenType::LBrace))initList.push_back(parse_brace_initializer());else initList.push_back(parse_assignment());}while(match(TokenType::Comma));}
            expect(TokenType::RBrace,"Expected '}'");
        } else init=parse_expression();
    }
    // Comma-separated decls — skip extras
    while(match(TokenType::Comma)){while(check(TokenType::OpStar))advance();if(check(TokenType::Identifier))advance();
        while(check(TokenType::LBracket)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LBracket))d++;if(check(TokenType::RBracket))d--;if(d>0)advance();}if(check(TokenType::RBracket))advance();}
        if(match(TokenType::OpAssign)){if(check(TokenType::LBrace)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LBrace))d++;if(check(TokenType::RBrace))d--;if(d>0)advance();}if(check(TokenType::RBrace))advance();}else parse_assignment();}
    }
    if(!check(TokenType::Semicolon)){while(!check(TokenType::Semicolon)&&!check(TokenType::Eof)&&!check(TokenType::RBrace))advance();}
    if(check(TokenType::Semicolon))advance();
    return std::make_unique<VariableDeclaration>(std::move(type),name.value,std::move(init),name.line,name.column,std::move(arraySize),std::move(initList));
}

std::unique_ptr<InitializerListExpr> Parser::parse_brace_initializer() {
    int l=current().line,c=current().column;
    expect(TokenType::LBrace,"Expected '{'");
    std::vector<ExprPtr> vals;
    if(!check(TokenType::RBrace)){do{if(check(TokenType::RBrace))break;if(check(TokenType::LBrace))vals.push_back(parse_brace_initializer());else vals.push_back(parse_assignment());}while(match(TokenType::Comma));}
    expect(TokenType::RBrace,"Expected '}'");
    return std::make_unique<InitializerListExpr>(std::move(vals),l,c);
}

// -----------------------------------------------------------------------
// Enum / Union
// -----------------------------------------------------------------------

std::unique_ptr<EnumDefinition> Parser::parse_enum_definition(bool fromTypedef) {
    int sL=current().line,sC=current().column; advance();
    std::string name; if(check(TokenType::Identifier)){name=current().value;advance();known_types_.insert(name);}
    if(name.empty()) name="__anon_enum_"+std::to_string(sL)+"_"+std::to_string(sC);
    expect(TokenType::LBrace,"Expected '{'");
    std::vector<EnumValue> vals;
    while(!check(TokenType::RBrace)&&!check(TokenType::Eof)){
        Token vn=expect(TokenType::Identifier,"Expected enum value name");
        ExprPtr ve; if(match(TokenType::OpAssign)) ve=parse_expression();
        vals.push_back({vn.value,std::move(ve)});
        if(!match(TokenType::Comma))break;
    }
    expect(TokenType::RBrace,"Expected '}'");
    if(!fromTypedef)expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<EnumDefinition>(name,std::move(vals),sL,sC);
}

std::unique_ptr<UnionDefinition> Parser::parse_union_definition(bool fromTypedef) {
    int sL=current().line,sC=current().column; advance();
    std::string name; if(check(TokenType::Identifier)){name=advance().value;known_types_.insert(name);}
    if(match(TokenType::Semicolon)){if(name.empty())name="__anon_union_"+std::to_string(sL)+"_"+std::to_string(sC);return std::make_unique<UnionDefinition>(name,std::vector<std::unique_ptr<ClassMember>>{},sL,sC);}
    expect(TokenType::LBrace,"Expected '{'");
    std::vector<std::unique_ptr<ClassMember>> members;
    while(!check(TokenType::RBrace)&&!check(TokenType::Eof)){
        int mL=current().line,mC=current().column;
        try {
            auto t=parse_type(); Token mn=expect(TokenType::Identifier,"Expected member name");
            while(check(TokenType::LBracket)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LBracket))d++;if(check(TokenType::RBracket))d--;if(d>0)advance();}expect(TokenType::RBracket,"Expected ']'");}
            expect(TokenType::Semicolon,"Expected ';'");
            members.push_back(std::make_unique<MemberVariableDecl>(AccessSpecifier::Public,std::move(t),mn.value,nullptr,false,mL,mC));
        } catch(...) { skip_to_semicolon_or_brace(); }
    }
    expect(TokenType::RBrace,"Expected '}'");
    if(!fromTypedef)expect(TokenType::Semicolon,"Expected ';'");
    if(name.empty())name="__anon_union_"+std::to_string(sL)+"_"+std::to_string(sC);
    return std::make_unique<UnionDefinition>(name,std::move(members),sL,sC);
}

// -----------------------------------------------------------------------
// Class / Struct
// -----------------------------------------------------------------------

std::unique_ptr<ClassDefinition> Parser::parse_class_definition(bool fromTypedef) {
    int sL=current().line,sC=current().column;
    bool isStruct=check(TokenType::KW_Struct); advance();
    std::string className;
    if(check(TokenType::Identifier)){className=current().value;advance();known_types_.insert(className);}
    else if(!fromTypedef) expect(TokenType::Identifier,"Expected class/struct name");
    if(className.empty()) className="__anon_struct_"+std::to_string(sL)+"_"+std::to_string(sC);

    std::vector<std::unique_ptr<BaseClassSpecifier>> bases;
    if(match(TokenType::Colon)){do{bases.push_back(parse_base_class_specifier(isStruct));}while(match(TokenType::Comma));}
    if(match(TokenType::Semicolon)) return std::make_unique<ClassDefinition>(className,isStruct,std::move(bases),std::vector<std::unique_ptr<ClassMember>>{},sL,sC);

    expect(TokenType::LBrace,"Expected '{'");
    std::vector<std::unique_ptr<ClassMember>> members;
    AccessSpecifier curAccess = isStruct ? AccessSpecifier::Public : AccessSpecifier::Private;
    while(!check(TokenType::RBrace)&&!check(TokenType::Eof)){
        if(check(TokenType::Semicolon)){advance();continue;}
        if(check(TokenType::KW_Public)){advance();expect(TokenType::Colon,"Expected ':'");curAccess=AccessSpecifier::Public;continue;}
        if(check(TokenType::KW_Protected)){advance();expect(TokenType::Colon,"Expected ':'");curAccess=AccessSpecifier::Protected;continue;}
        if(check(TokenType::KW_Private)){advance();expect(TokenType::Colon,"Expected ':'");curAccess=AccessSpecifier::Private;continue;}
        try {
            members.push_back(parse_class_member(className,curAccess));
        } catch (const ParseError&) {
            // Error recovery: skip to next semicolon or closing brace
            while(!check(TokenType::Semicolon)&&!check(TokenType::RBrace)&&!check(TokenType::Eof))
                advance();
            if(check(TokenType::Semicolon)) advance();
        }
    }
    expect(TokenType::RBrace,"Expected '}'");
    if(!fromTypedef)expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<ClassDefinition>(className,isStruct,std::move(bases),std::move(members),sL,sC);
}

std::unique_ptr<BaseClassSpecifier> Parser::parse_base_class_specifier(bool parentIsStruct) {
    int l=current().line,c=current().column;
    bool isVirtual=false;
    AccessSpecifier access = parentIsStruct ? AccessSpecifier::Public : AccessSpecifier::Private;
    if(match(TokenType::KW_Virtual))isVirtual=true;
    if(match(TokenType::KW_Public))access=AccessSpecifier::Public;
    else if(match(TokenType::KW_Protected))access=AccessSpecifier::Protected;
    else if(match(TokenType::KW_Private))access=AccessSpecifier::Private;
    if(match(TokenType::KW_Virtual))isVirtual=true;
    Token bn=expect(TokenType::Identifier,"Expected base class name");
    return std::make_unique<BaseClassSpecifier>(bn.value,access,isVirtual,l,c);
}

// Simplified class member parsing — handles most common cases
std::unique_ptr<ClassMember> Parser::parse_class_member(const std::string& className, AccessSpecifier access) {
    int sL=current().line,sC=current().column;
    bool isVirtual=false,isStatic=false,isExplicit=false;
    while(true){
        if(check(TokenType::KW_Virtual)){isVirtual=true;advance();}
        else if(check(TokenType::KW_Static)){isStatic=true;advance();}
        else if(check(TokenType::KW_Explicit)){isExplicit=true;advance();}
        else if(check(TokenType::KW_Inline)||check(TokenType::KW_Friend)||check(TokenType::KW_Volatile)||check(TokenType::KW_Const)||check(TokenType::KW_Extern))advance();
        else break;
    }
    if(check(TokenType::KW_Typedef)){auto td=parse_typedef();return std::make_unique<MemberVariableDecl>(access,std::make_unique<TypeNode>("void",false,false,false,0,false,td->line,td->column),"__typedef_"+td->alias_name,nullptr,true,td->line,td->column);}
    if(check(TokenType::KW_Template)){auto tp=parse_template_parameter_list();auto inner=parse_class_member(className,access);return std::make_unique<MemberTemplateDecl>(access,std::move(tp),std::move(inner),sL,sC);}
    if(check(TokenType::OpBitNot)){advance();expect(TokenType::Identifier,"Expected name after '~'");return parse_destructor(className,access,isVirtual,sL,sC);}
    if(check(TokenType::Identifier)&&current().value==className&&peek().type==TokenType::LParen)
        return parse_constructor(className,access,isExplicit,sL,sC);

    auto type=parse_type();
    if(check(TokenType::Colon)){advance();parse_expression();expect(TokenType::Semicolon,"Expected ';'");return std::make_unique<MemberVariableDecl>(access,std::move(type),"__anon_bitfield_"+std::to_string(sL),nullptr,isStatic,sL,sC);}

    // Check for operator overload: ReturnType operator+(...) or operator()(...) etc.
    if(check(TokenType::KW_Operator)) {
        std::string op = parse_operator_symbol();
        expect(TokenType::LParen, "Expected '('");
        auto params = parse_parameter_list();
        expect(TokenType::RParen, "Expected ')'");
        bool oc = match(TokenType::KW_Const);
        BlockPtr body;
        if (check(TokenType::LBrace)) body = parse_block();
        else { expect(TokenType::Semicolon, "Expected ';'"); }
        return std::make_unique<OperatorOverload>(access, std::move(type), op, std::move(params), std::move(body), oc, isVirtual, false, sL, sC);
    }

    Token mn=expect(TokenType::Identifier,"Expected member name");
    if(check(TokenType::LParen)) return parse_member_function(access,std::move(type),mn.value,isVirtual,isStatic,sL,sC);

    // Member variable (possibly array, bit-field, etc.)
    while(check(TokenType::LBracket)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LBracket))d++;if(check(TokenType::RBracket))d--;if(d>0)advance();}expect(TokenType::RBracket,"Expected ']'");}
    if(check(TokenType::Colon)){advance();parse_expression();}
    ExprPtr defVal; if(match(TokenType::OpAssign))defVal=parse_expression();
    while(match(TokenType::Comma)){while(check(TokenType::OpStar))advance();if(check(TokenType::Identifier))advance();while(check(TokenType::LBracket)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::LBracket))d++;if(check(TokenType::RBracket))d--;if(d>0)advance();}if(check(TokenType::RBracket))advance();}if(check(TokenType::Colon)){advance();parse_expression();}if(match(TokenType::OpAssign))parse_expression();}
    expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<MemberVariableDecl>(access,std::move(type),mn.value,std::move(defVal),isStatic,sL,sC);
}

// Unused overload required by header
std::unique_ptr<ClassMember> Parser::parse_class_member(AccessSpecifier, bool, bool, bool, bool, bool) {
    return std::make_unique<MemberVariableDecl>(AccessSpecifier::Public,std::make_unique<TypeNode>("int",false,false,false,0,false,0,0),"__error",nullptr,false,0,0);
}

std::unique_ptr<ConstructorDecl> Parser::parse_constructor(const std::string& className, AccessSpecifier access, bool isExplicit, int sL, int sC) {
    advance(); expect(TokenType::LParen,"Expected '('");
    auto params=parse_parameter_list(); expect(TokenType::RParen,"Expected ')'");
    std::vector<MemberInitializer> initList;
    if(match(TokenType::Colon)){do{Token mn=expect(TokenType::Identifier,"Expected member name");expect(TokenType::LParen,"Expected '('");std::vector<ExprPtr> args;if(!check(TokenType::RParen)){do{args.push_back(parse_assignment());}while(match(TokenType::Comma));}expect(TokenType::RParen,"Expected ')'");initList.push_back({mn.value,std::move(args)});}while(match(TokenType::Comma));}
    BlockPtr body; if(check(TokenType::LBrace))body=parse_block(); else expect(TokenType::Semicolon,"Expected ';' or '{'");
    return std::make_unique<ConstructorDecl>(access,className,std::move(params),std::move(initList),std::move(body),isExplicit,sL,sC);
}

std::unique_ptr<DestructorDecl> Parser::parse_destructor(const std::string& className, AccessSpecifier access, bool isVirtual, int sL, int sC) {
    expect(TokenType::LParen,"Expected '()'"); expect(TokenType::RParen,"Expected '()'");
    BlockPtr body; if(check(TokenType::LBrace))body=parse_block(); else expect(TokenType::Semicolon,"Expected ';' or '{'");
    return std::make_unique<DestructorDecl>(access,className,std::move(body),isVirtual,sL,sC);
}

std::unique_ptr<MemberFunctionDecl> Parser::parse_member_function(AccessSpecifier access, TypePtr retType, const std::string& name, bool isVirtual, bool isStatic, int sL, int sC) {
    expect(TokenType::LParen,"Expected '('"); auto params=parse_parameter_list(); expect(TokenType::RParen,"Expected ')'");
    bool isConst=match(TokenType::KW_Const);
    bool isPure=false; if(check(TokenType::OpAssign)&&peek().type==TokenType::IntegerLiteral&&peek().value=="0"){advance();advance();isPure=true;}
    BlockPtr body; if(check(TokenType::LBrace))body=parse_block(); else expect(TokenType::Semicolon,"Expected ';' or '{'");
    return std::make_unique<MemberFunctionDecl>(access,std::move(retType),name,std::move(params),std::move(body),isVirtual,isStatic,isConst,isPure,sL,sC);
}

// -----------------------------------------------------------------------
// Namespace / Using / Typedef
// -----------------------------------------------------------------------

std::unique_ptr<NamespaceDefinition> Parser::parse_namespace() {
    int sL=current().line,sC=current().column; advance();
    Token name=expect(TokenType::Identifier,"Expected namespace name"); expect(TokenType::LBrace,"Expected '{'");
    std::vector<AstPtr> decls;
    while(!check(TokenType::RBrace)&&!check(TokenType::Eof)){
        if(check(TokenType::Semicolon)){advance();continue;}
        if(check(TokenType::KW_Namespace))decls.push_back(parse_namespace());
        else if(check(TokenType::KW_Using))decls.push_back(parse_using_statement());
        else if(check(TokenType::KW_Typedef))decls.push_back(parse_typedef());
        else if(check(TokenType::KW_Class)||check(TokenType::KW_Struct))decls.push_back(parse_class_definition());
        else if(check(TokenType::KW_Union))decls.push_back(parse_union_definition());
        else if(check(TokenType::KW_Enum))decls.push_back(parse_enum_definition());
        else if(check(TokenType::KW_Template))decls.push_back(parse_template_declaration());
        else if(check(TokenType::KW_Static)||check(TokenType::KW_Volatile)||check(TokenType::KW_Inline)||check(TokenType::KW_Extern)){advance();continue;}
        else if(check(TokenType::Preprocessor)){auto t=advance();decls.push_back(std::make_unique<PreprocessorDirective>(t.value,t.line,t.column));}
        else decls.push_back(parse_declaration());
    }
    expect(TokenType::RBrace,"Expected '}'");
    return std::make_unique<NamespaceDefinition>(name.value,std::move(decls),sL,sC);
}

AstPtr Parser::parse_using_statement() {
    int sL=current().line,sC=current().column; advance();
    if(match(TokenType::KW_Namespace)){std::string ns=expect(TokenType::Identifier,"Expected namespace").value;while(match(TokenType::OpScope)){ns+="::";ns+=expect(TokenType::Identifier,"Expected name").value;}expect(TokenType::Semicolon,"Expected ';'");return std::make_unique<UsingDirective>(ns,sL,sC);}
    std::string qn=expect(TokenType::Identifier,"Expected name").value;while(match(TokenType::OpScope)){qn+="::";qn+=expect(TokenType::Identifier,"Expected name").value;}expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<UsingDeclaration>(qn,sL,sC);
}

std::string Parser::expect_alias_name() {
    if(check(TokenType::Identifier)) return advance().value;
    if(check(TokenType::KW_WcharT)||check(TokenType::KW_Bool)||check(TokenType::KW_Char)||check(TokenType::KW_Short)||check(TokenType::KW_Int)||check(TokenType::KW_Long)||check(TokenType::KW_Float)||check(TokenType::KW_Double)||check(TokenType::KW_Void)) return advance().value;
    if(!check(TokenType::Semicolon)&&!check(TokenType::Eof)) return advance().value;
    throw ParseError("Expected typedef alias name",current().line,current().column);
}

std::unique_ptr<TypedefDeclaration> Parser::parse_typedef() {
    int sL=current().line,sC=current().column; advance();
    // typedef struct/class/enum/union ...
    if(check(TokenType::KW_Struct)||check(TokenType::KW_Class)){
        int save=pos_; advance(); std::string sn; if(check(TokenType::Identifier))sn=advance().value;
        if(check(TokenType::LBrace)||check(TokenType::Colon)){pos_=save;auto cd=parse_class_definition(true);typedef_structs_.push_back(std::move(cd));/* hack: re-get pointer */}
        else{int pd=0;while(check(TokenType::OpStar)){pd++;advance();}std::string al=expect_alias_name();known_types_.insert(al);if(!sn.empty())known_types_.insert(sn);}
        while(match(TokenType::Comma)){while(check(TokenType::OpStar))advance();if(!check(TokenType::Semicolon)&&!check(TokenType::Eof)){std::string ea=expect_alias_name();known_types_.insert(ea);}}
        if(!check(TokenType::Semicolon)){while(!check(TokenType::Semicolon)&&!check(TokenType::Eof)&&!check(TokenType::RBrace))advance();}
        if(check(TokenType::Semicolon))advance();
        auto tn=std::make_unique<TypeNode>("int",false,false,false,0,false,sL,sC);
        return std::make_unique<TypedefDeclaration>(std::move(tn),"__typedef",sL,sC);
    }
    if(check(TokenType::KW_Enum)){
        int save=pos_; advance(); std::string en; if(check(TokenType::Identifier))en=advance().value;
        if(check(TokenType::LBrace)){pos_=save;auto ed=parse_enum_definition(true);typedef_enums_.push_back(std::move(ed));}
        else{std::string al=expect_alias_name();known_types_.insert(al);if(!en.empty())known_types_.insert(en);}
        while(match(TokenType::Comma)){while(check(TokenType::OpStar))advance();if(!check(TokenType::Semicolon)&&!check(TokenType::Eof)){std::string ea=expect_alias_name();known_types_.insert(ea);}}
        if(!check(TokenType::Semicolon)){while(!check(TokenType::Semicolon)&&!check(TokenType::Eof)&&!check(TokenType::RBrace))advance();}
        if(check(TokenType::Semicolon))advance();
        auto tn=std::make_unique<TypeNode>("int",false,false,false,0,false,sL,sC);
        return std::make_unique<TypedefDeclaration>(std::move(tn),"__typedef",sL,sC);
    }
    if(check(TokenType::KW_Union)){
        int save=pos_; advance(); std::string un; if(check(TokenType::Identifier))un=advance().value;
        if(check(TokenType::LBrace)){pos_=save;auto ud=parse_union_definition(true);typedef_structs_.push_back(std::move(ud));}
        else{int pd=0;while(check(TokenType::OpStar)){pd++;advance();}std::string al=expect_alias_name();known_types_.insert(al);if(!un.empty())known_types_.insert(un);}
        while(match(TokenType::Comma)){while(check(TokenType::OpStar))advance();if(!check(TokenType::Semicolon)&&!check(TokenType::Eof)){std::string ea=expect_alias_name();known_types_.insert(ea);}}
        if(!check(TokenType::Semicolon)){while(!check(TokenType::Semicolon)&&!check(TokenType::Eof)&&!check(TokenType::RBrace))advance();}
        if(check(TokenType::Semicolon))advance();
        auto tn=std::make_unique<TypeNode>("int",false,false,false,0,false,sL,sC);
        return std::make_unique<TypedefDeclaration>(std::move(tn),"__typedef",sL,sC);
    }

    auto origType=parse_type();
    // Function pointer typedef — skip to semicolon
    if(check(TokenType::LParen)){int d=0;while(!check(TokenType::Eof)){if(check(TokenType::LParen))d++;if(check(TokenType::RParen))d--;advance();if(d<=0&&check(TokenType::Semicolon))break;if(d<=0&&check(TokenType::RParen)){advance();if(check(TokenType::Semicolon))break;}}
        if(!check(TokenType::Semicolon)){while(!check(TokenType::Semicolon)&&!check(TokenType::Eof)&&!check(TokenType::RBrace))advance();}if(check(TokenType::Semicolon))advance();
        return std::make_unique<TypedefDeclaration>(std::move(origType),"__skipped_fptr",sL,sC);}
    if(check(TokenType::Semicolon)){
        std::string bn=origType->base_name,alias=bn,newBase;
        size_t ls=bn.rfind(' ');if(ls!=std::string::npos){newBase=bn.substr(0,ls);alias=bn.substr(ls+1);}
        if(!newBase.empty())origType=std::make_unique<TypeNode>(newBase,origType->is_const,origType->is_unsigned,origType->is_signed,origType->pointer_depth,origType->is_reference,origType->line,origType->column);
        known_types_.insert(alias);if(check(TokenType::Semicolon))advance();
        return std::make_unique<TypedefDeclaration>(std::move(origType),alias,sL,sC);}
    while(check(TokenType::OpScope)){advance();if(check(TokenType::Identifier))advance();}
    std::string alias2=expect_alias_name();
    if(check(TokenType::LBracket)){while(!check(TokenType::Semicolon)&&!check(TokenType::Comma)&&!check(TokenType::Eof))advance();}
    known_types_.insert(alias2);
    while(match(TokenType::Comma)){while(check(TokenType::OpStar))advance();if(!check(TokenType::Semicolon)&&!check(TokenType::Eof)){std::string ea=expect_alias_name();known_types_.insert(ea);if(check(TokenType::LBracket)){while(!check(TokenType::Semicolon)&&!check(TokenType::Comma)&&!check(TokenType::Eof))advance();}}}
    if(!check(TokenType::Semicolon)){while(!check(TokenType::Semicolon)&&!check(TokenType::Eof)&&!check(TokenType::RBrace))advance();}
    if(check(TokenType::Semicolon))advance();
    return std::make_unique<TypedefDeclaration>(std::move(origType),alias2,sL,sC);
}

// -----------------------------------------------------------------------
// Operator overloading
// -----------------------------------------------------------------------

const std::unordered_set<std::string>& Parser::overloadable_operators() {
    static const std::unordered_set<std::string> ops = {"+","-","*","/","%","==","!=","<",">","<=",">=","&&","||","!","&","|","^","~","<<",">>","++","--","=","+=","-=","*=","/=","%=","&=","|=","^=","<<=",">>=","->","->*","[]","()",",","new","delete","new[]","delete[]"};
    return ops;
}

bool Parser::is_operator_overload_ahead() {
    int save=pos_;
    try{while(pos_<(int)tokens_.size()){auto t=current().type;if(t==TokenType::KW_Const||t==TokenType::KW_Unsigned||t==TokenType::KW_Signed||t==TokenType::KW_Static||t==TokenType::KW_Virtual||t==TokenType::KW_Inline||t==TokenType::KW_Extern||t==TokenType::KW_Friend||is_type_keyword(t)||t==TokenType::Identifier||t==TokenType::OpStar||t==TokenType::OpBitAnd){pos_++;}else break;}bool r=check(TokenType::KW_Operator);pos_=save;return r;}
    catch(...){pos_=save;return false;}
}

std::string Parser::parse_operator_symbol() {
    expect(TokenType::KW_Operator,"Expected 'operator'");
    if(check(TokenType::LParen)){advance();expect(TokenType::RParen,"Expected ')'");return "()";}
    if(check(TokenType::LBracket)){advance();expect(TokenType::RBracket,"Expected ']'");return "[]";}
    if(check(TokenType::KW_New)){advance();if(check(TokenType::LBracket)){advance();expect(TokenType::RBracket,"Expected ']'");return "new[]";}return "new";}
    if(check(TokenType::KW_Delete)){advance();if(check(TokenType::LBracket)){advance();expect(TokenType::RBracket,"Expected ']'");return "delete[]";}return "delete";}
    auto tok=current();
    std::string two=tok.value+(peek().value);
    if(overloadable_operators().count(two)){advance();advance();return two;}
    if(overloadable_operators().count(tok.value)){advance();return tok.value;}
    if(is_type_keyword(tok.type)||tok.type==TokenType::Identifier){auto ct=parse_type();return ct->base_name;}
    throw ParseError("Invalid operator: '"+tok.value+"'",tok.line,tok.column);
}

std::unique_ptr<ClassMember> Parser::parse_operator_overload_member(AccessSpecifier access, bool isVirtual, bool, bool isExplicit, int sL, int sC) {
    // Check for conversion operator
    int save=pos_; bool isConversion=false;
    if(check(TokenType::KW_Operator)){pos_++;if(is_type_keyword(current().type)||(current().type==TokenType::Identifier&&known_types_.count(current().value)))isConversion=true;}
    pos_=save;
    if(isConversion){advance();auto tt=parse_type();expect(TokenType::LParen,"Expected '('");expect(TokenType::RParen,"Expected ')'");bool ic=match(TokenType::KW_Const);BlockPtr body;if(check(TokenType::LBrace))body=parse_block();else expect(TokenType::Semicolon,"Expected ';'");return std::make_unique<ConversionOperator>(access,std::move(tt),std::move(body),ic,isExplicit,sL,sC);}
    auto retType=parse_type();std::string op=parse_operator_symbol();expect(TokenType::LParen,"Expected '('");auto params=parse_parameter_list();expect(TokenType::RParen,"Expected ')'");bool oc=match(TokenType::KW_Const);BlockPtr body;if(check(TokenType::LBrace))body=parse_block();else expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<OperatorOverload>(access,std::move(retType),op,std::move(params),std::move(body),oc,isVirtual,false,sL,sC);
}

std::unique_ptr<GlobalOperatorOverload> Parser::parse_global_operator_overload() {
    int sL=current().line,sC=current().column;
    auto retType=parse_type();std::string op=parse_operator_symbol();expect(TokenType::LParen,"Expected '('");auto params=parse_parameter_list();expect(TokenType::RParen,"Expected ')'");
    BlockPtr body;if(check(TokenType::LBrace))body=parse_block();else expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<GlobalOperatorOverload>(std::move(retType),op,std::move(params),std::move(body),sL,sC);
}

// -----------------------------------------------------------------------
// Templates
// -----------------------------------------------------------------------

AstPtr Parser::parse_template_declaration() {
    int sL=current().line,sC=current().column;
    auto tparams=parse_template_parameter_list();
    AstPtr inner;
    if(check(TokenType::KW_Class)||check(TokenType::KW_Struct)){inner=parse_class_definition();if(auto*cls=dynamic_cast<ClassDefinition*>(inner.get()))template_names_.insert(cls->name);}
    else if(check(TokenType::KW_Enum))inner=parse_enum_definition();
    else{inner=parse_declaration();if(auto*func=dynamic_cast<FunctionDefinition*>(inner.get()))template_names_.insert(func->name);}
    return std::make_unique<TemplateDeclaration>(std::move(tparams),std::move(inner),sL,sC);
}

std::vector<std::unique_ptr<TemplateParameter>> Parser::parse_template_parameter_list() {
    expect(TokenType::KW_Template,"Expected 'template'"); expect(TokenType::OpLess,"Expected '<'");
    std::vector<std::unique_ptr<TemplateParameter>> params;
    if(check(TokenType::OpGreater)){advance();return params;}
    do{params.push_back(parse_single_template_parameter());}while(match(TokenType::Comma));
    expect(TokenType::OpGreater,"Expected '>'");
    return params;
}

std::unique_ptr<TemplateParameter> Parser::parse_single_template_parameter() {
    int sL=current().line,sC=current().column;
    if(check(TokenType::KW_Template)){auto ip=parse_template_parameter_list();if(check(TokenType::KW_Class)||check(TokenType::KW_Typename))advance();std::string n;if(check(TokenType::Identifier))n=advance().value;return std::make_unique<TemplateParameter>(TemplateParamKind::Template,n,nullptr,nullptr,std::move(ip),sL,sC);}
    if(check(TokenType::KW_Typename)||check(TokenType::KW_Class)){advance();std::string n;if(check(TokenType::Identifier)){n=advance().value;if(!n.empty())known_types_.insert(n);}AstPtr dv;if(match(TokenType::OpAssign))dv=parse_type();return std::make_unique<TemplateParameter>(TemplateParamKind::Type,n,nullptr,std::move(dv),std::vector<std::unique_ptr<TemplateParameter>>{},sL,sC);}
    auto pt=parse_type();std::string pn;if(check(TokenType::Identifier))pn=advance().value;AstPtr dv;if(match(TokenType::OpAssign))dv=parse_expression();
    return std::make_unique<TemplateParameter>(TemplateParamKind::NonType,pn,std::move(pt),std::move(dv),std::vector<std::unique_ptr<TemplateParameter>>{},sL,sC);
}

std::vector<std::unique_ptr<TemplateArgument>>* Parser::try_parse_template_arguments() {
    int save=pos_;
    try{
        expect(TokenType::OpLess,"Expected '<'");
        auto* args=new std::vector<std::unique_ptr<TemplateArgument>>();
        if(check(TokenType::OpGreater)){advance();return args;}
        do{int aL=current().line,aC=current().column;int ts=pos_;
            try{if(is_at_type_specifier()||is_type_keyword(current().type)){auto ta=parse_type();if(check(TokenType::Comma)||check(TokenType::OpGreater)||check(TokenType::OpRShift)){args->push_back(std::make_unique<TemplateArgument>(std::move(ta),nullptr,aL,aC));continue;}pos_=ts;}else pos_=ts;}catch(...){pos_=ts;}
            auto va=parse_assignment();args->push_back(std::make_unique<TemplateArgument>(nullptr,std::move(va),aL,aC));
        }while(match(TokenType::Comma));
        if(check(TokenType::OpRShift)){advance();return args;}
        expect(TokenType::OpGreater,"Expected '>'");return args;
    }catch(...){pos_=save;return nullptr;}
}

// -----------------------------------------------------------------------
// Statements
// -----------------------------------------------------------------------

BlockPtr Parser::parse_block() {
    int sL=current().line,sC=current().column; expect(TokenType::LBrace,"Expected '{'");
    std::vector<AstPtr> stmts;
    while(!check(TokenType::RBrace)&&!check(TokenType::Eof)){
        try{stmts.push_back(parse_statement_or_declaration());}
        catch(const ParseError&){while(!check(TokenType::Semicolon)&&!check(TokenType::RBrace)&&!check(TokenType::Eof))advance();if(check(TokenType::Semicolon))advance();}
    }
    expect(TokenType::RBrace,"Expected '}'");
    return std::make_unique<BlockStatement>(std::move(stmts),sL,sC);
}

AstPtr Parser::parse_statement_or_declaration() {
    if(check(TokenType::KW_Class)||check(TokenType::KW_Struct)) return parse_class_definition();
    if(check(TokenType::KW_Union)) return parse_union_definition();
    if(check(TokenType::KW_Enum)) return parse_enum_definition();
    if(check(TokenType::KW_Static)||check(TokenType::KW_Extern)||check(TokenType::KW_Volatile)){if(peek().type!=TokenType::Semicolon)return parse_declaration();}
    if(is_at_type_specifier()){
        if(check(TokenType::OpScope)){int s=pos_;advance();if(check(TokenType::KW_New)||check(TokenType::KW_Delete)){pos_=s;return parse_statement();}if(check(TokenType::Identifier)){advance();if(check(TokenType::LParen)){pos_=s;return parse_statement();}}pos_=s;}
        if(check(TokenType::Identifier)&&!is_type_keyword(current().type)){auto nt=peek().type;if(nt!=TokenType::Identifier&&nt!=TokenType::OpStar&&nt!=TokenType::OpBitAnd&&nt!=TokenType::OpScope&&nt!=TokenType::KW_Const&&nt!=TokenType::KW_Volatile)return parse_statement();}
        return parse_declaration();
    }
    return parse_statement();
}

StmtPtr Parser::parse_statement() {
    if(check(TokenType::LBrace)) return parse_block();
    if(check(TokenType::KW_Return)) return parse_return_statement();
    if(check(TokenType::KW_If)) return parse_if_statement();
    if(check(TokenType::KW_While)) return parse_while_statement();
    if(check(TokenType::KW_Do)) return parse_do_while_statement();
    if(check(TokenType::KW_For)) return parse_for_statement();
    if(check(TokenType::KW_Switch)) return parse_switch_statement();
    if(check(TokenType::KW_Break)){auto t=advance();expect(TokenType::Semicolon,"Expected ';'");return std::make_unique<BreakStatement>(t.line,t.column);}
    if(check(TokenType::KW_Continue)){auto t=advance();expect(TokenType::Semicolon,"Expected ';'");return std::make_unique<ContinueStatement>(t.line,t.column);}
    if(check(TokenType::KW_Try)) return parse_try_catch();
    if(check(TokenType::KW_Throw)) return parse_throw();
    return parse_expression_statement();
}

std::unique_ptr<ReturnStatement> Parser::parse_return_statement() {
    auto t=advance(); ExprPtr val; if(!check(TokenType::Semicolon))val=parse_expression();
    expect(TokenType::Semicolon,"Expected ';'"); return std::make_unique<ReturnStatement>(std::move(val),t.line,t.column);
}
std::unique_ptr<IfStatement> Parser::parse_if_statement() {
    auto t=advance();expect(TokenType::LParen,"Expected '('");auto cond=parse_expression();expect(TokenType::RParen,"Expected ')'");
    auto then=parse_statement();StmtPtr el;if(match(TokenType::KW_Else))el=parse_statement();
    return std::make_unique<IfStatement>(std::move(cond),std::move(then),std::move(el),t.line,t.column);
}
std::unique_ptr<WhileStatement> Parser::parse_while_statement() {
    auto t=advance();expect(TokenType::LParen,"Expected '('");auto cond=parse_expression();expect(TokenType::RParen,"Expected ')'");auto body=parse_statement();
    return std::make_unique<WhileStatement>(std::move(cond),std::move(body),t.line,t.column);
}
std::unique_ptr<DoWhileStatement> Parser::parse_do_while_statement() {
    auto t=advance();auto body=parse_statement();expect(TokenType::KW_While,"Expected 'while'");expect(TokenType::LParen,"Expected '('");auto cond=parse_expression();expect(TokenType::RParen,"Expected ')'");expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<DoWhileStatement>(std::move(body),std::move(cond),t.line,t.column);
}
std::unique_ptr<ForStatement> Parser::parse_for_statement() {
    auto t=advance();expect(TokenType::LParen,"Expected '('");
    AstPtr init;if(!check(TokenType::Semicolon)){if(is_at_type_specifier()){auto ft=parse_type();auto fn=expect(TokenType::Identifier,"Expected name");init=parse_variable_declaration(std::move(ft),fn);}else{init=std::make_unique<ExpressionStatement>(parse_comma_expression(),current().line,current().column);expect(TokenType::Semicolon,"Expected ';'");}}else advance();
    ExprPtr cond;if(!check(TokenType::Semicolon))cond=parse_expression();expect(TokenType::Semicolon,"Expected ';'");
    ExprPtr incr;if(!check(TokenType::RParen))incr=parse_comma_expression();expect(TokenType::RParen,"Expected ')'");
    auto body=parse_statement();return std::make_unique<ForStatement>(std::move(init),std::move(cond),std::move(incr),std::move(body),t.line,t.column);
}
std::unique_ptr<SwitchStatement> Parser::parse_switch_statement() {
    auto t=advance();expect(TokenType::LParen,"Expected '('");auto val=parse_expression();expect(TokenType::RParen,"Expected ')'");expect(TokenType::LBrace,"Expected '{'");
    std::vector<std::unique_ptr<SwitchCase>> cases;
    while(!check(TokenType::RBrace)&&!check(TokenType::Eof)){
        if(check(TokenType::KW_Case)){auto ct=advance();auto cv=parse_expression();expect(TokenType::Colon,"Expected ':'");std::vector<AstPtr> body;while(!check(TokenType::KW_Case)&&!check(TokenType::KW_Default)&&!check(TokenType::RBrace)&&!check(TokenType::Eof))body.push_back(parse_statement_or_declaration());cases.push_back(std::make_unique<SwitchCase>(std::move(cv),std::move(body),ct.line,ct.column));}
        else if(check(TokenType::KW_Default)){auto dt=advance();expect(TokenType::Colon,"Expected ':'");std::vector<AstPtr> body;while(!check(TokenType::KW_Case)&&!check(TokenType::KW_Default)&&!check(TokenType::RBrace)&&!check(TokenType::Eof))body.push_back(parse_statement_or_declaration());cases.push_back(std::make_unique<SwitchCase>(nullptr,std::move(body),dt.line,dt.column));}
        else throw ParseError("Expected 'case' or 'default'",current().line,current().column);
    }
    expect(TokenType::RBrace,"Expected '}'");return std::make_unique<SwitchStatement>(std::move(val),std::move(cases),t.line,t.column);
}
std::unique_ptr<ExpressionStatement> Parser::parse_expression_statement() {
    int l=current().line,c=current().column;auto e=parse_expression();expect(TokenType::Semicolon,"Expected ';'");return std::make_unique<ExpressionStatement>(std::move(e),l,c);
}
std::unique_ptr<TryCatchStatement> Parser::parse_try_catch() {
    auto t=advance();auto tryBody=parse_block();if(!check(TokenType::KW_Catch))throw ParseError("Expected 'catch'",current().line,current().column);
    std::vector<std::unique_ptr<CatchClause>> clauses;while(check(TokenType::KW_Catch))clauses.push_back(parse_catch_clause());
    return std::make_unique<TryCatchStatement>(std::move(tryBody),std::move(clauses),t.line,t.column);
}
std::unique_ptr<CatchClause> Parser::parse_catch_clause() {
    int sL=current().line,sC=current().column;advance();expect(TokenType::LParen,"Expected '('");
    TypePtr et;std::string vn;bool ca=false;
    if(check(TokenType::Ellipsis)){advance();ca=true;}else{et=parse_type();if(check(TokenType::Identifier))vn=advance().value;}
    expect(TokenType::RParen,"Expected ')'");auto body=parse_block();
    return std::make_unique<CatchClause>(std::move(et),vn,ca,std::move(body),sL,sC);
}
std::unique_ptr<ThrowStatement> Parser::parse_throw() {
    auto t=advance();ExprPtr val;if(!check(TokenType::Semicolon))val=parse_expression();expect(TokenType::Semicolon,"Expected ';'");
    return std::make_unique<ThrowStatement>(std::move(val),t.line,t.column);
}

// -----------------------------------------------------------------------
// Expressions
// -----------------------------------------------------------------------

ExprPtr Parser::parse_comma_expression() {
    auto left=parse_assignment();while(check(TokenType::Comma)){auto op=advance();auto right=parse_assignment();left=std::make_unique<BinaryExpr>(std::move(left),",",std::move(right),op.line,op.column);}return left;
}
ExprPtr Parser::parse_expression() { return parse_assignment(); }
bool Parser::is_assignment_op(TokenType t) const {
    return t==TokenType::OpAssign||t==TokenType::OpPlusAssign||t==TokenType::OpMinusAssign||t==TokenType::OpStarAssign||t==TokenType::OpSlashAssign||t==TokenType::OpPercentAssign||t==TokenType::OpAndAssign||t==TokenType::OpOrAssign||t==TokenType::OpXorAssign||t==TokenType::OpLShiftAssign||t==TokenType::OpRShiftAssign;
}
ExprPtr Parser::parse_assignment() {
    auto left=parse_ternary();if(is_assignment_op(current().type)){auto op=advance();auto right=parse_assignment();return std::make_unique<BinaryExpr>(std::move(left),op.value,std::move(right),op.line,op.column);}return left;
}
ExprPtr Parser::parse_ternary() {
    auto e=parse_logical_or();if(match(TokenType::OpQuestion)){auto then=parse_expression();expect(TokenType::Colon,"Expected ':'");auto el=parse_ternary();return std::make_unique<TernaryExpr>(std::move(e),std::move(then),std::move(el),e->line,e->column);}return e;
}
ExprPtr Parser::parse_logical_or() { auto l=parse_logical_and();while(check(TokenType::OpOr)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_logical_and(),o.line,o.column);}return l; }
ExprPtr Parser::parse_logical_and() { auto l=parse_bitwise_or();while(check(TokenType::OpAnd)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_bitwise_or(),o.line,o.column);}return l; }
ExprPtr Parser::parse_bitwise_or() { auto l=parse_bitwise_xor();while(check(TokenType::OpBitOr)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_bitwise_xor(),o.line,o.column);}return l; }
ExprPtr Parser::parse_bitwise_xor() { auto l=parse_bitwise_and();while(check(TokenType::OpBitXor)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_bitwise_and(),o.line,o.column);}return l; }
ExprPtr Parser::parse_bitwise_and() { auto l=parse_equality();while(check(TokenType::OpBitAnd)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_equality(),o.line,o.column);}return l; }
ExprPtr Parser::parse_equality() { auto l=parse_relational();while(check(TokenType::OpEqual)||check(TokenType::OpNotEqual)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_relational(),o.line,o.column);}return l; }
ExprPtr Parser::parse_relational() { auto l=parse_shift();while(check(TokenType::OpLess)||check(TokenType::OpGreater)||check(TokenType::OpLessEqual)||check(TokenType::OpGreaterEqual)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_shift(),o.line,o.column);}return l; }
ExprPtr Parser::parse_shift() { auto l=parse_additive();while(check(TokenType::OpLShift)||check(TokenType::OpRShift)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_additive(),o.line,o.column);}return l; }
ExprPtr Parser::parse_additive() { auto l=parse_multiplicative();while(check(TokenType::OpPlus)||check(TokenType::OpMinus)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_multiplicative(),o.line,o.column);}return l; }
ExprPtr Parser::parse_multiplicative() { auto l=parse_unary();while(check(TokenType::OpStar)||check(TokenType::OpSlash)||check(TokenType::OpPercent)){auto o=advance();l=std::make_unique<BinaryExpr>(std::move(l),o.value,parse_unary(),o.line,o.column);}return l; }
ExprPtr Parser::parse_unary() {
    if(check(TokenType::OpNot)||check(TokenType::OpBitNot)||check(TokenType::OpMinus)||check(TokenType::OpPlus)||check(TokenType::OpIncrement)||check(TokenType::OpDecrement)||check(TokenType::OpStar)||check(TokenType::OpBitAnd)){auto o=advance();return std::make_unique<UnaryExpr>(o.value,parse_unary(),true,o.line,o.column);}
    if(check(TokenType::KW_Sizeof))return parse_sizeof();
    return parse_postfix();
}
std::unique_ptr<SizeofExpr> Parser::parse_sizeof() {
    auto t=advance();expect(TokenType::LParen,"Expected '('");
    if(is_at_type_specifier()){auto tp=parse_type();expect(TokenType::RParen,"Expected ')'");return std::make_unique<SizeofExpr>(std::move(tp),nullptr,t.line,t.column);}
    auto e=parse_expression();expect(TokenType::RParen,"Expected ')'");return std::make_unique<SizeofExpr>(nullptr,std::move(e),t.line,t.column);
}
ExprPtr Parser::parse_postfix() {
    auto e=parse_primary();
    while(true){
        if(check(TokenType::OpLess)&&dynamic_cast<IdentifierExpr*>(e.get())){auto*id=dynamic_cast<IdentifierExpr*>(e.get());if(template_names_.count(id->name)||known_types_.count(id->name)){advance();int d=1;while(d>0&&!check(TokenType::Eof)){if(check(TokenType::OpLess))d++;if(check(TokenType::OpGreater)){d--;if(d<=0)break;}if(check(TokenType::OpRShift)){d-=2;if(d<=0)break;}advance();}if(check(TokenType::OpGreater))advance();continue;}}
        if(check(TokenType::LParen)){advance();std::vector<ExprPtr> args;if(!check(TokenType::RParen)){do{args.push_back(parse_assignment());}while(match(TokenType::Comma));}expect(TokenType::RParen,"Expected ')'");e=std::make_unique<CallExpr>(std::move(e),std::move(args),e->line,e->column);}
        else if(check(TokenType::LBracket)){advance();auto idx=parse_expression();expect(TokenType::RBracket,"Expected ']'");e=std::make_unique<IndexExpr>(std::move(e),std::move(idx),e->line,e->column);}
        else if(check(TokenType::OpDot)){advance();if(check(TokenType::OpBitNot)){advance();auto dn=expect(TokenType::Identifier,"Expected name");e=std::make_unique<MemberAccessExpr>(std::move(e),"~"+dn.value,false,e->line,e->column);}else{auto m=expect(TokenType::Identifier,"Expected member");e=std::make_unique<MemberAccessExpr>(std::move(e),m.value,false,e->line,e->column);}}
        else if(check(TokenType::OpArrow)){advance();if(check(TokenType::OpBitNot)){advance();auto dn=expect(TokenType::Identifier,"Expected name");e=std::make_unique<MemberAccessExpr>(std::move(e),"~"+dn.value,true,e->line,e->column);}else{auto m=expect(TokenType::Identifier,"Expected member");e=std::make_unique<MemberAccessExpr>(std::move(e),m.value,true,e->line,e->column);}}
        else if(check(TokenType::OpIncrement)){auto o=advance();e=std::make_unique<UnaryExpr>("++",std::move(e),false,o.line,o.column);}
        else if(check(TokenType::OpDecrement)){auto o=advance();e=std::make_unique<UnaryExpr>("--",std::move(e),false,o.line,o.column);}
        else break;
    }
    return e;
}
ExprPtr Parser::parse_primary() {
    auto tok=current();
    if(check(TokenType::IntegerLiteral)){advance();return std::make_unique<IntegerLiteral>(tok.value,tok.line,tok.column);}
    if(check(TokenType::FloatLiteral)){advance();return std::make_unique<FloatLiteral>(tok.value,tok.line,tok.column);}
    if(check(TokenType::StringLiteral)){advance();return std::make_unique<StringLiteralExpr>(tok.value,tok.line,tok.column);}
    if(check(TokenType::CharLiteral)){advance();return std::make_unique<CharLiteralExpr>(tok.value,tok.line,tok.column);}
    if(check(TokenType::KW_True)){advance();return std::make_unique<BoolLiteral>(true,tok.line,tok.column);}
    if(check(TokenType::KW_False)){advance();return std::make_unique<BoolLiteral>(false,tok.line,tok.column);}
    if(check(TokenType::Identifier)){advance();if(check(TokenType::OpScope)){advance();auto m=expect(TokenType::Identifier,"Expected name after '::'");return std::make_unique<ScopeResolutionExpr>(tok.value,m.value,tok.line,tok.column);}return std::make_unique<IdentifierExpr>(tok.value,tok.line,tok.column);}
    if(check(TokenType::KW_This)){advance();return std::make_unique<ThisExpr>(tok.line,tok.column);}
    if(check(TokenType::OpScope)){advance();if(check(TokenType::KW_New))return parse_new_expr();if(check(TokenType::KW_Delete))return parse_delete_expr();auto n=expect(TokenType::Identifier,"Expected name");std::string fn=n.value;while(check(TokenType::OpScope)){advance();if(check(TokenType::Identifier))fn=advance().value;}return std::make_unique<IdentifierExpr>(fn,tok.line,tok.column);}
    if(check(TokenType::KW_StaticCast)||check(TokenType::KW_ReinterpretCast)||check(TokenType::KW_DynamicCast)||check(TokenType::KW_ConstCast)){advance();expect(TokenType::OpLess,"Expected '<'");auto ct=parse_type();expect(TokenType::OpGreater,"Expected '>'");expect(TokenType::LParen,"Expected '('");auto op=parse_expression();expect(TokenType::RParen,"Expected ')'");return std::make_unique<CastExpr>(std::move(ct),std::move(op),tok.line,tok.column);}
    if(check(TokenType::KW_New)) return parse_new_expr();
    if(check(TokenType::KW_Delete)) return parse_delete_expr();
    if(check(TokenType::LParen)){advance();if((is_at_type_specifier()||(check(TokenType::Identifier)&&peek().type==TokenType::OpStar))&&looks_like_cast()){auto ct=parse_type();expect(TokenType::RParen,"Expected ')'");auto op=parse_unary();return std::make_unique<CastExpr>(std::move(ct),std::move(op),tok.line,tok.column);}auto e=parse_expression();expect(TokenType::RParen,"Expected ')'");return e;}
    throw ParseError("Unexpected token: "+std::string(token_type_name(tok.type))+" \""+tok.value+"\"",tok.line,tok.column);
}
ExprPtr Parser::parse_new_expr() {
    auto t=advance();auto at=parse_type();
    if(check(TokenType::LBracket)){advance();auto sz=parse_expression();expect(TokenType::RBracket,"Expected ']'");return std::make_unique<NewExpr>(std::move(at),std::vector<ExprPtr>{},std::move(sz),true,t.line,t.column);}
    std::vector<ExprPtr> args;if(check(TokenType::LParen)){advance();if(!check(TokenType::RParen)){do{args.push_back(parse_assignment());}while(match(TokenType::Comma));}expect(TokenType::RParen,"Expected ')'");}
    return std::make_unique<NewExpr>(std::move(at),std::move(args),nullptr,false,t.line,t.column);
}
ExprPtr Parser::parse_delete_expr() {
    auto t=advance();bool isArr=false;if(check(TokenType::LBracket)){advance();expect(TokenType::RBracket,"Expected ']'");isArr=true;}
    auto op=parse_unary();return std::make_unique<DeleteExpr>(std::move(op),isArr,t.line,t.column);
}

} // namespace nexia
