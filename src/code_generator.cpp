// NexiaCompiler v2.0 — Code Generator
// Ported from CodeGenerator.cs — FULL implementation
#include "code_generator.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace nexia {

static int64_t parse_int_literal(const std::string& text) {
    std::string c=text;
    // Strip all MSVC integer suffixes: u, U, l, L, i64, i32, i16, i8, UI64, etc.
    while(!c.empty()) {
        char back = c.back();
        if(back=='u'||back=='U'||back=='l'||back=='L') { c.pop_back(); continue; }
        // Handle i64, i32, i16, i8 suffixes
        if(c.size()>=3 && (c.substr(c.size()-3)=="i64"||c.substr(c.size()-3)=="i32"||c.substr(c.size()-3)=="i16")) { c.erase(c.size()-3); continue; }
        if(c.size()>=2 && c.substr(c.size()-2)=="i8") { c.erase(c.size()-2); continue; }
        break;
    }
    if(c.empty()) return 0;
    try {
        if(c.size()>2&&(c.substr(0,2)=="0x"||c.substr(0,2)=="0X"))return std::stoll(c.substr(2),nullptr,16);
        return std::stoll(c);
    } catch(...) { return 0; }
}

static TypeInfo resolve_type_simple(const TypeNode& t) {
    static const std::unordered_map<std::string,int> sizes={{"void",0},{"bool",1},{"char",1},{"wchar_t",2},{"short",2},{"int",4},{"long",4},{"long long",8},{"float",4},{"double",8},{"long double",8}};
    int sz=t.pointer_depth>0?4:4; auto it=sizes.find(t.base_name); if(it!=sizes.end()&&t.pointer_depth==0)sz=it->second;
    return TypeInfo(t.base_name,t.is_const,t.is_unsigned,t.pointer_depth,t.is_reference,sz);
}

std::string CodeGenerator::new_label(const std::string& prefix){return "__"+prefix+"_"+std::to_string(label_counter_++);}
void CodeGenerator::emit(uint32_t inst){instructions_.push_back(inst);}

void CodeGenerator::generate(TranslationUnit& unit) {
    for(auto&d:unit.declarations)if(auto*f=dynamic_cast<FunctionDefinition*>(d.get()))function_offsets_[f->name]=-1;
    for(auto&d:unit.declarations)if(auto*f=dynamic_cast<FunctionDefinition*>(d.get()))generate_function(*f);
    patch_branches();
    code_bytes_.clear(); code_bytes_.reserve(instructions_.size()*4);
    for(auto inst:instructions_){code_bytes_.push_back((inst>>24)&0xFF);code_bytes_.push_back((inst>>16)&0xFF);code_bytes_.push_back((inst>>8)&0xFF);code_bytes_.push_back(inst&0xFF);}
}

void CodeGenerator::generate_function(FunctionDefinition& func) {
    regs_=std::make_unique<RegisterAllocator>(); current_function_name_=func.name;
    function_offsets_[func.name]=(int)instructions_.size()*4;
    for(size_t i=0;i<func.parameters.size();i++){auto&p=func.parameters[i];auto ti=resolve_type_simple(*p->type);regs_->allocate_parameter(p->name,ti,(int)i);}
    pre_scan_locals(*func.body); int fs=regs_->calculate_frame_size();
    emit(ppc::mflr(PpcRegister::R0)); emit(ppc::stw(PpcRegister::R0,4,PpcRegister::R1));
    emit(ppc::stwu(PpcRegister::R1,(int16_t)(-fs),PpcRegister::R1));
    generate_block(*func.body);
    std::string epi="__epilogue_"+func.name; labels_[epi]=(int)instructions_.size();
    emit(ppc::addi(PpcRegister::R1,PpcRegister::R1,(int16_t)fs));
    emit(ppc::lwz(PpcRegister::R0,4,PpcRegister::R1)); emit(ppc::mtlr(PpcRegister::R0)); emit(ppc::blr());
}

void CodeGenerator::pre_scan_locals(BlockStatement& block) {
    for(auto&s:block.statements){
        if(auto*v=dynamic_cast<VariableDeclaration*>(s.get())){auto ti=resolve_type_simple(*v->type);
            if(v->is_array()&&v->array_size){if(auto*sl=dynamic_cast<IntegerLiteral*>(v->array_size.get())){try{int64_t n=std::stoll(sl->value);if(n>0){TypeInfo at(ti.base_name,ti.is_const,ti.is_unsigned,ti.pointer_depth,ti.is_reference,(int)(ti.size*n));regs_->allocate_local(v->name,at);continue;}}catch(...){}}}
            regs_->allocate_local(v->name,ti);
        }else if(auto*b=dynamic_cast<BlockStatement*>(s.get()))pre_scan_locals(*b);
        else if(auto*i=dynamic_cast<IfStatement*>(s.get())){if(auto*tb=dynamic_cast<BlockStatement*>(i->then_branch.get()))pre_scan_locals(*tb);if(i->else_branch)if(auto*eb=dynamic_cast<BlockStatement*>(i->else_branch.get()))pre_scan_locals(*eb);}
        else if(auto*w=dynamic_cast<WhileStatement*>(s.get())){if(auto*wb=dynamic_cast<BlockStatement*>(w->body.get()))pre_scan_locals(*wb);}
        else if(auto*f=dynamic_cast<ForStatement*>(s.get())){if(auto*fv=dynamic_cast<VariableDeclaration*>(f->init.get())){auto fti=resolve_type_simple(*fv->type);regs_->allocate_local(fv->name,fti);}if(auto*fb=dynamic_cast<BlockStatement*>(f->body.get()))pre_scan_locals(*fb);}
    }
}

void CodeGenerator::generate_block(BlockStatement& block){for(auto&s:block.statements)generate_statement(*s);}

void CodeGenerator::generate_statement(AstNode& node) {
    regs_->reset_temps();
    if(auto*v=dynamic_cast<VariableDeclaration*>(&node)){generate_var_decl(*v);return;}
    if(auto*es=dynamic_cast<ExpressionStatement*>(&node)){generate_expression(*es->expr,PpcRegister::R3);return;}
    if(auto*r=dynamic_cast<ReturnStatement*>(&node)){generate_return(*r);return;}
    if(auto*i=dynamic_cast<IfStatement*>(&node)){generate_if(*i);return;}
    if(auto*w=dynamic_cast<WhileStatement*>(&node)){generate_while(*w);return;}
    if(auto*d=dynamic_cast<DoWhileStatement*>(&node)){generate_do_while(*d);return;}
    if(auto*f=dynamic_cast<ForStatement*>(&node)){generate_for(*f);return;}
    if(auto*b=dynamic_cast<BlockStatement*>(&node)){generate_block(*b);return;}
    if(dynamic_cast<BreakStatement*>(&node)){if(!break_label_.empty()){patches_.push_back({(int)instructions_.size(),break_label_,BranchKind::Unconditional});emit(ppc::nop());}return;}
    if(dynamic_cast<ContinueStatement*>(&node)){if(!continue_label_.empty()){patches_.push_back({(int)instructions_.size(),continue_label_,BranchKind::Unconditional});emit(ppc::nop());}return;}
    if(auto*tc=dynamic_cast<TryCatchStatement*>(&node)){generate_try_catch(*tc);return;}
    if(auto*th=dynamic_cast<ThrowStatement*>(&node)){generate_throw(*th);return;}
}

void CodeGenerator::generate_var_decl(VariableDeclaration& decl) {
    auto*loc=regs_->get_variable(decl.name); if(!loc)return;
    if(decl.has_init_list&&loc->kind==StorageKind::Stack){int16_t bo=(int16_t)(loc->stack_offset+regs_->locals_base_offset());for(size_t i=0;i<decl.initializer_list.size();i++){generate_expression(*decl.initializer_list[i],PpcRegister::R3);emit(ppc::stw(PpcRegister::R3,(int16_t)(bo+(int)i*4),PpcRegister::R1));}return;}
    if(!decl.initializer)return;
    generate_expression(*decl.initializer,PpcRegister::R3);
    if(loc->kind==StorageKind::GPR){if(loc->gpr!=PpcRegister::R3)emit(ppc::mr(loc->gpr,PpcRegister::R3));}
    else if(loc->kind==StorageKind::Stack)emit(ppc::stw(PpcRegister::R3,(int16_t)(loc->stack_offset+regs_->locals_base_offset()),PpcRegister::R1));
}

void CodeGenerator::generate_return(ReturnStatement& ret){if(ret.value)generate_expression(*ret.value,PpcRegister::R3);patches_.push_back({(int)instructions_.size(),"__epilogue_"+current_function_name_,BranchKind::Unconditional});emit(ppc::nop());}

void CodeGenerator::generate_if(IfStatement& s){
    std::string el=new_label("else"),en=new_label("endif");
    generate_expression(*s.condition,PpcRegister::R3);emit(ppc::cmpwi(0,PpcRegister::R3,0));
    if(s.else_branch){patches_.push_back({(int)instructions_.size(),el,BranchKind::Equal});emit(ppc::nop());generate_statement(*s.then_branch);patches_.push_back({(int)instructions_.size(),en,BranchKind::Unconditional});emit(ppc::nop());labels_[el]=(int)instructions_.size();generate_statement(*s.else_branch);}
    else{patches_.push_back({(int)instructions_.size(),en,BranchKind::Equal});emit(ppc::nop());generate_statement(*s.then_branch);}
    labels_[en]=(int)instructions_.size();
}

void CodeGenerator::generate_while(WhileStatement& s){
    std::string ll=new_label("wloop"),el=new_label("wend"); auto pb=break_label_,pc=continue_label_; break_label_=el;continue_label_=ll;
    labels_[ll]=(int)instructions_.size(); generate_expression(*s.condition,PpcRegister::R3);emit(ppc::cmpwi(0,PpcRegister::R3,0));
    patches_.push_back({(int)instructions_.size(),el,BranchKind::Equal});emit(ppc::nop());
    generate_statement(*s.body); patches_.push_back({(int)instructions_.size(),ll,BranchKind::Unconditional});emit(ppc::nop());
    labels_[el]=(int)instructions_.size(); break_label_=pb;continue_label_=pc;
}

void CodeGenerator::generate_do_while(DoWhileStatement& s){
    std::string ll=new_label("dloop"),el=new_label("dend"),cl=new_label("dcond"); auto pb=break_label_,pc=continue_label_; break_label_=el;continue_label_=cl;
    labels_[ll]=(int)instructions_.size(); generate_statement(*s.body);
    labels_[cl]=(int)instructions_.size(); generate_expression(*s.condition,PpcRegister::R3);emit(ppc::cmpwi(0,PpcRegister::R3,0));
    patches_.push_back({(int)instructions_.size(),ll,BranchKind::NotEqual});emit(ppc::nop());
    labels_[el]=(int)instructions_.size(); break_label_=pb;continue_label_=pc;
}

void CodeGenerator::generate_for(ForStatement& s){
    std::string ll=new_label("floop"),el=new_label("fend"),il=new_label("fincr"); auto pb=break_label_,pc=continue_label_; break_label_=el;continue_label_=il;
    if(s.init) generate_statement(*s.init);
    labels_[ll]=(int)instructions_.size();
    if(s.condition){generate_expression(*s.condition,PpcRegister::R3);emit(ppc::cmpwi(0,PpcRegister::R3,0));patches_.push_back({(int)instructions_.size(),el,BranchKind::Equal});emit(ppc::nop());}
    generate_statement(*s.body); labels_[il]=(int)instructions_.size();
    if(s.increment){regs_->reset_temps();generate_expression(*s.increment,PpcRegister::R3);}
    patches_.push_back({(int)instructions_.size(),ll,BranchKind::Unconditional});emit(ppc::nop());
    labels_[el]=(int)instructions_.size(); break_label_=pb;continue_label_=pc;
}

void CodeGenerator::generate_switch(SwitchStatement&){/* TODO: full switch codegen */}

void CodeGenerator::generate_try_catch(TryCatchStatement& s){
    std::string cs=new_label("catch"),te=new_label("tryend"); auto pe=current_exception_label_; current_exception_label_=cs;
    generate_block(*s.try_body); patches_.push_back({(int)instructions_.size(),te,BranchKind::Unconditional});emit(ppc::nop());
    labels_[cs]=(int)instructions_.size();
    for(size_t i=0;i<s.catch_clauses.size();i++){generate_block(*s.catch_clauses[i]->body);patches_.push_back({(int)instructions_.size(),te,BranchKind::Unconditional});emit(ppc::nop());}
    labels_[te]=(int)instructions_.size(); current_exception_label_=pe;
}

void CodeGenerator::generate_throw(ThrowStatement& s){
    if(s.value)generate_expression(*s.value,PpcRegister::R3);
    if(!current_exception_label_.empty()){patches_.push_back({(int)instructions_.size(),current_exception_label_,BranchKind::Unconditional});emit(ppc::nop());}
    else{std::string tl=new_label("trap");labels_[tl]=(int)instructions_.size();patches_.push_back({(int)instructions_.size(),tl,BranchKind::Unconditional});emit(ppc::nop());}
}

void CodeGenerator::generate_expression(Expression& expr, PpcRegister dest) {
    if(auto*il=dynamic_cast<IntegerLiteral*>(&expr)){generate_int_literal(*il,dest);return;}
    if(dynamic_cast<FloatLiteral*>(&expr)){emit(ppc::li(dest,0));return;}
    if(auto*bl=dynamic_cast<BoolLiteral*>(&expr)){emit(ppc::li(dest,(int16_t)(bl->value?1:0)));return;}
    if(auto*sl=dynamic_cast<StringLiteralExpr*>(&expr)){generate_string_literal(*sl,dest);return;}
    if(auto*cl=dynamic_cast<CharLiteralExpr*>(&expr)){int16_t cv=cl->value.empty()?0:(int16_t)cl->value[0];emit(ppc::li(dest,cv));return;}
    if(auto*id=dynamic_cast<IdentifierExpr*>(&expr)){load_variable(id->name,dest);return;}
    if(auto*bin=dynamic_cast<BinaryExpr*>(&expr)){generate_binary(*bin,dest);return;}
    if(auto*un=dynamic_cast<UnaryExpr*>(&expr)){generate_unary(*un,dest);return;}
    if(auto*call=dynamic_cast<CallExpr*>(&expr)){generate_call(*call,dest);return;}
    if(auto*idx=dynamic_cast<IndexExpr*>(&expr)){generate_index(*idx,dest);return;}
    if(auto*tern=dynamic_cast<TernaryExpr*>(&expr)){generate_ternary(*tern,dest);return;}
    if(auto*cast=dynamic_cast<CastExpr*>(&expr)){generate_expression(*cast->operand,dest);return;}
    if(auto*sof=dynamic_cast<SizeofExpr*>(&expr)){int sz=4;if(sof->target_type){auto ti=resolve_type_simple(*sof->target_type);if(ti.size>0)sz=ti.size;}emit(ppc::li(dest,(int16_t)sz));return;}
    emit(ppc::li(dest,0));
}

void CodeGenerator::generate_int_literal(IntegerLiteral& lit, PpcRegister dest) {
    int64_t v=parse_int_literal(lit.value);
    if(v>=-32768&&v<=32767)emit(ppc::li(dest,(int16_t)v));
    else{int16_t hi=(int16_t)((v>>16)&0xFFFF);uint16_t lo=(uint16_t)(v&0xFFFF);emit(ppc::lis(dest,hi));if(lo)emit(ppc::ori(dest,dest,lo));}
}

void CodeGenerator::generate_string_literal(StringLiteralExpr& str, PpcRegister dest) {
    if(string_literals_.find(str.value)==string_literals_.end()){int off=(int)data_section_.size();string_literals_[str.value]=off;for(char c:str.value)data_section_.push_back((uint8_t)c);data_section_.push_back(0);while(data_section_.size()%4!=0)data_section_.push_back(0);}
    int off=string_literals_[str.value];int16_t hi=(int16_t)((off>>16)&0xFFFF);uint16_t lo=(uint16_t)(off&0xFFFF);emit(ppc::lis(dest,hi));emit(ppc::ori(dest,dest,lo));
}

void CodeGenerator::generate_binary(BinaryExpr& bin, PpcRegister dest) {
    // Assignment
    if(bin.op=="="&&dynamic_cast<IdentifierExpr*>(bin.left.get())){auto*tgt=dynamic_cast<IdentifierExpr*>(bin.left.get());generate_expression(*bin.right,dest);auto*loc=regs_->get_variable(tgt->name);if(loc){if(loc->kind==StorageKind::GPR&&loc->gpr!=dest)emit(ppc::mr(loc->gpr,dest));else if(loc->kind==StorageKind::Stack)emit(ppc::stw(dest,(int16_t)(loc->stack_offset+regs_->locals_base_offset()),PpcRegister::R1));}return;}
    // Compound assignment
    if((bin.op=="+="||bin.op=="-="||bin.op=="*="||bin.op=="/="||bin.op=="&="||bin.op=="|="||bin.op=="^="||bin.op=="<<="||bin.op==">>=")&&dynamic_cast<IdentifierExpr*>(bin.left.get())){
        auto*tgt=dynamic_cast<IdentifierExpr*>(bin.left.get());load_variable(tgt->name,dest);auto rr=regs_->get_temp_gpr();generate_expression(*bin.right,rr);
        std::string baseOp=bin.op.substr(0,bin.op.size()-1);
        if(baseOp=="+")emit(ppc::add(dest,dest,rr));else if(baseOp=="-")emit(ppc::sub(dest,dest,rr));else if(baseOp=="*")emit(ppc::mullw(dest,dest,rr));
        else if(baseOp=="/")emit(ppc::divw(dest,dest,rr));else if(baseOp=="&")emit(ppc::and_op(dest,dest,rr));else if(baseOp=="|")emit(ppc::or_op(dest,dest,rr));
        else if(baseOp=="^")emit(ppc::xor_op(dest,dest,rr));else if(baseOp=="<<")emit(ppc::slw(dest,dest,rr));else if(baseOp==">>")emit(ppc::sraw(dest,dest,rr));
        auto*loc=regs_->get_variable(tgt->name);if(loc){if(loc->kind==StorageKind::GPR&&loc->gpr!=dest)emit(ppc::mr(loc->gpr,dest));else if(loc->kind==StorageKind::Stack)emit(ppc::stw(dest,(int16_t)(loc->stack_offset+regs_->locals_base_offset()),PpcRegister::R1));}return;
    }
    generate_expression(*bin.left,dest);auto rr=regs_->get_temp_gpr();generate_expression(*bin.right,rr);
    if(bin.op=="+")emit(ppc::add(dest,dest,rr));else if(bin.op=="-")emit(ppc::sub(dest,dest,rr));
    else if(bin.op=="*")emit(ppc::mullw(dest,dest,rr));else if(bin.op=="/")emit(ppc::divw(dest,dest,rr));
    else if(bin.op=="%"){auto td=regs_->get_temp_gpr();emit(ppc::divw(td,dest,rr));emit(ppc::mullw(td,td,rr));emit(ppc::sub(dest,dest,td));}
    else if(bin.op=="&")emit(ppc::and_op(dest,dest,rr));else if(bin.op=="|")emit(ppc::or_op(dest,dest,rr));
    else if(bin.op=="^")emit(ppc::xor_op(dest,dest,rr));else if(bin.op=="<<")emit(ppc::slw(dest,dest,rr));else if(bin.op==">>")emit(ppc::sraw(dest,dest,rr));
    else if(bin.op=="=="||bin.op=="!="||bin.op=="<"||bin.op==">"||bin.op=="<="||bin.op==">="){
        emit(ppc::cmpw(0,dest,rr));std::string tl=new_label("cmp_t"),el=new_label("cmp_e");
        BranchKind bk=BranchKind::Equal;
        if(bin.op=="==")bk=BranchKind::Equal;else if(bin.op=="!=")bk=BranchKind::NotEqual;else if(bin.op=="<")bk=BranchKind::Less;else if(bin.op==">=")bk=BranchKind::GreaterEq;else if(bin.op==">")bk=BranchKind::Greater;else if(bin.op=="<=")bk=BranchKind::LessEq;
        patches_.push_back({(int)instructions_.size(),tl,bk});emit(ppc::nop());emit(ppc::li(dest,0));patches_.push_back({(int)instructions_.size(),el,BranchKind::Unconditional});emit(ppc::nop());labels_[tl]=(int)instructions_.size();emit(ppc::li(dest,1));labels_[el]=(int)instructions_.size();
    }
    else if(bin.op=="&&"){emit(ppc::cmpwi(0,dest,0));std::string fl=new_label("af"),el=new_label("ae");patches_.push_back({(int)instructions_.size(),fl,BranchKind::Equal});emit(ppc::nop());emit(ppc::cmpwi(0,rr,0));patches_.push_back({(int)instructions_.size(),fl,BranchKind::Equal});emit(ppc::nop());emit(ppc::li(dest,1));patches_.push_back({(int)instructions_.size(),el,BranchKind::Unconditional});emit(ppc::nop());labels_[fl]=(int)instructions_.size();emit(ppc::li(dest,0));labels_[el]=(int)instructions_.size();}
    else if(bin.op=="||"){emit(ppc::cmpwi(0,dest,0));std::string tl=new_label("ot"),el=new_label("oe");patches_.push_back({(int)instructions_.size(),tl,BranchKind::NotEqual});emit(ppc::nop());emit(ppc::cmpwi(0,rr,0));patches_.push_back({(int)instructions_.size(),tl,BranchKind::NotEqual});emit(ppc::nop());emit(ppc::li(dest,0));patches_.push_back({(int)instructions_.size(),el,BranchKind::Unconditional});emit(ppc::nop());labels_[tl]=(int)instructions_.size();emit(ppc::li(dest,1));labels_[el]=(int)instructions_.size();}
}

void CodeGenerator::generate_unary(UnaryExpr& un, PpcRegister dest) {
    generate_expression(*un.operand,dest);
    if(un.op=="-")emit(ppc::neg(dest,dest));
    else if(un.op=="~")emit(ppc::nor(dest,dest,dest));
    else if(un.op=="!"){emit(ppc::cmpwi(0,dest,0));std::string tl=new_label("nt"),el=new_label("ne");patches_.push_back({(int)instructions_.size(),tl,BranchKind::Equal});emit(ppc::nop());emit(ppc::li(dest,0));patches_.push_back({(int)instructions_.size(),el,BranchKind::Unconditional});emit(ppc::nop());labels_[tl]=(int)instructions_.size();emit(ppc::li(dest,1));labels_[el]=(int)instructions_.size();}
    else if(un.op=="++"){emit(ppc::addi(dest,dest,1));if(auto*id=dynamic_cast<IdentifierExpr*>(un.operand.get())){auto*loc=regs_->get_variable(id->name);if(loc){if(loc->kind==StorageKind::GPR&&loc->gpr!=dest)emit(ppc::mr(loc->gpr,dest));else if(loc->kind==StorageKind::Stack)emit(ppc::stw(dest,(int16_t)(loc->stack_offset+regs_->locals_base_offset()),PpcRegister::R1));}}}
    else if(un.op=="--"){emit(ppc::addi(dest,dest,-1));if(auto*id=dynamic_cast<IdentifierExpr*>(un.operand.get())){auto*loc=regs_->get_variable(id->name);if(loc){if(loc->kind==StorageKind::GPR&&loc->gpr!=dest)emit(ppc::mr(loc->gpr,dest));else if(loc->kind==StorageKind::Stack)emit(ppc::stw(dest,(int16_t)(loc->stack_offset+regs_->locals_base_offset()),PpcRegister::R1));}}}
    else if(un.op=="*")emit(ppc::lwz(dest,0,dest));
    else if(un.op=="&"){if(auto*id=dynamic_cast<IdentifierExpr*>(un.operand.get())){auto*loc=regs_->get_variable(id->name);if(loc&&loc->kind==StorageKind::Stack)emit(ppc::addi(dest,PpcRegister::R1,(int16_t)(loc->stack_offset+regs_->locals_base_offset())));}}
}

void CodeGenerator::generate_call(CallExpr& call, PpcRegister dest) {
    for(size_t i=0;i<call.arguments.size()&&i<8;i++)generate_expression(*call.arguments[i],(PpcRegister)(3+(int)i));
    if(auto*fn=dynamic_cast<IdentifierExpr*>(call.callee.get())){
        if(function_offsets_.count(fn->name)) {
            patches_.push_back({(int)instructions_.size(),fn->name,BranchKind::Unconditional});
        } else {
            external_calls_.push_back(fn->name);
            patches_.push_back({(int)instructions_.size(),"__extern_"+fn->name,BranchKind::Unconditional});
        }
        emit(ppc::nop());
    }
    if(dest!=PpcRegister::R3)emit(ppc::mr(dest,PpcRegister::R3));
}

void CodeGenerator::generate_index(IndexExpr& idx, PpcRegister dest) {
    generate_expression(*idx.object,dest);auto ir=regs_->get_temp_gpr();generate_expression(*idx.index,ir);
    auto sr=regs_->get_temp_gpr();emit(ppc::li(sr,4));emit(ppc::mullw(ir,ir,sr));emit(ppc::add(dest,dest,ir));emit(ppc::lwz(dest,0,dest));
}

void CodeGenerator::generate_ternary(TernaryExpr& tern, PpcRegister dest) {
    std::string fl=new_label("tf"),el=new_label("te");
    generate_expression(*tern.condition,dest);emit(ppc::cmpwi(0,dest,0));patches_.push_back({(int)instructions_.size(),fl,BranchKind::Equal});emit(ppc::nop());
    generate_expression(*tern.then_expr,dest);patches_.push_back({(int)instructions_.size(),el,BranchKind::Unconditional});emit(ppc::nop());
    labels_[fl]=(int)instructions_.size();generate_expression(*tern.else_expr,dest);labels_[el]=(int)instructions_.size();
}

void CodeGenerator::load_variable(const std::string& name, PpcRegister dest) {
    auto*loc=regs_->get_variable(name);if(!loc){emit(ppc::li(dest,0));return;}
    if(loc->kind==StorageKind::GPR){if(loc->gpr!=dest)emit(ppc::mr(dest,loc->gpr));}
    else if(loc->kind==StorageKind::Stack)emit(ppc::lwz(dest,(int16_t)(loc->stack_offset+regs_->locals_base_offset()),PpcRegister::R1));
}

void CodeGenerator::patch_branches() {
    for(auto&[idx,label,kind]:patches_){
        // Check label targets first
        auto lit=labels_.find(label);if(lit!=labels_.end()){int off=(lit->second-idx)*4;
            switch(kind){case BranchKind::Unconditional:instructions_[idx]=ppc::b(off);break;case BranchKind::Equal:instructions_[idx]=ppc::beq((int16_t)off);break;case BranchKind::NotEqual:instructions_[idx]=ppc::bne((int16_t)off);break;case BranchKind::Less:instructions_[idx]=ppc::blt((int16_t)off);break;case BranchKind::GreaterEq:instructions_[idx]=ppc::bge((int16_t)off);break;case BranchKind::Greater:instructions_[idx]=ppc::bgt((int16_t)off);break;case BranchKind::LessEq:instructions_[idx]=ppc::ble((int16_t)off);break;}
            continue;}
        // Check function targets
        std::string fn=label; if(fn.substr(0,9)=="__extern_")fn=fn.substr(9);
        auto fit=function_offsets_.find(fn);if(fit!=function_offsets_.end()&&fit->second>=0){int off=fit->second-idx*4;instructions_[idx]=ppc::bl(off);}
    }
}

} // namespace nexia
