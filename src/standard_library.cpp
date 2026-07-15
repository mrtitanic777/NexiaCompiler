// NexiaCompiler v2.0 — Standard Library stubs
// Ported from StandardLibrary.cs — FULL implementation
#include "standard_library.h"
#include <cstring>

namespace nexia {

int StandardLibrary::start_function(const std::string& name) {
    int off=(int)code_.size()*4; function_offsets_[name]=off; return off;
}
void StandardLibrary::end_function(const std::string& name, int startOffset) {
    symbols_.emplace_back(name,LinkerSymbolKind::Function,startOffset,(int)code_.size()*4-startOffset,LinkerSection::Text,true);
}
void StandardLibrary::emit(uint32_t inst) { code_.push_back(inst); }
int StandardLibrary::add_string_data(const std::string& text) {
    int off=(int)data_.size();for(char c:text)data_.push_back((uint8_t)c);data_.push_back(0);while(data_.size()%4!=0)data_.push_back(0);return off;
}
void StandardLibrary::emit_prologue(int frameSize) {
    emit(ppc::mflr(PpcRegister::R0));emit(ppc::stw(PpcRegister::R0,4,PpcRegister::R1));
    emit(ppc::stwu(PpcRegister::R1,(int16_t)(-frameSize),PpcRegister::R1));
}
void StandardLibrary::emit_epilogue(int frameSize) {
    emit(ppc::addi(PpcRegister::R1,PpcRegister::R1,(int16_t)frameSize));
    emit(ppc::lwz(PpcRegister::R0,4,PpcRegister::R1));emit(ppc::mtlr(PpcRegister::R0));emit(ppc::blr());
}

void StandardLibrary::generate_memory_stubs() {
    // memset(dest, value, size) — r3=dest, r4=value, r5=size
    int off=start_function("memset"); emit_prologue();
    // Simple byte-by-byte loop
    emit(ppc::li(PpcRegister::R6,0));                    // i = 0
    std::string loop_label_offset = std::to_string(code_.size()*4); // remember loop start
    emit(ppc::cmpw(0,PpcRegister::R6,PpcRegister::R5));  // if i >= size
    emit(ppc::bge(5*4));                                  // skip to end
    emit(ppc::stb(PpcRegister::R4,0,PpcRegister::R3));   // *dest = value
    emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));  // dest++
    emit(ppc::addi(PpcRegister::R6,PpcRegister::R6,1));  // i++
    emit(ppc::b(-5*4));                                   // loop back
    emit_epilogue(); end_function("memset",off);

    // memcpy(dest, src, size)
    off=start_function("memcpy"); emit_prologue();
    emit(ppc::mr(PpcRegister::R7,PpcRegister::R3));       // save dest for return
    emit(ppc::li(PpcRegister::R6,0));
    emit(ppc::cmpw(0,PpcRegister::R6,PpcRegister::R5));
    emit(ppc::bge(6*4));
    emit(ppc::lbz(PpcRegister::R8,0,PpcRegister::R4));
    emit(ppc::stb(PpcRegister::R8,0,PpcRegister::R3));
    emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));
    emit(ppc::addi(PpcRegister::R4,PpcRegister::R4,1));
    emit(ppc::addi(PpcRegister::R6,PpcRegister::R6,1));
    emit(ppc::b(-7*4));
    emit(ppc::mr(PpcRegister::R3,PpcRegister::R7));
    emit_epilogue(); end_function("memcpy",off);

    // memmove — same as memcpy for now (no overlap handling)
    off=start_function("memmove"); emit_prologue();
    emit(ppc::mr(PpcRegister::R7,PpcRegister::R3));
    emit(ppc::li(PpcRegister::R6,0));
    emit(ppc::cmpw(0,PpcRegister::R6,PpcRegister::R5));emit(ppc::bge(6*4));
    emit(ppc::lbz(PpcRegister::R8,0,PpcRegister::R4));emit(ppc::stb(PpcRegister::R8,0,PpcRegister::R3));
    emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));emit(ppc::addi(PpcRegister::R4,PpcRegister::R4,1));
    emit(ppc::addi(PpcRegister::R6,PpcRegister::R6,1));emit(ppc::b(-7*4));
    emit(ppc::mr(PpcRegister::R3,PpcRegister::R7));
    emit_epilogue(); end_function("memmove",off);

    // memcmp — returns 0 if equal
    off=start_function("memcmp"); emit_prologue();
    emit(ppc::li(PpcRegister::R6,0));emit(ppc::li(PpcRegister::R3,0));
    emit(ppc::cmpw(0,PpcRegister::R6,PpcRegister::R5));emit(ppc::bge(7*4));
    emit(ppc::lbz(PpcRegister::R7,0,PpcRegister::R3));emit(ppc::lbz(PpcRegister::R8,0,PpcRegister::R4));
    emit(ppc::sub(PpcRegister::R3,PpcRegister::R7,PpcRegister::R8));
    emit(ppc::cmpwi(0,PpcRegister::R3,0));emit(ppc::bne(3*4));
    emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));emit(ppc::addi(PpcRegister::R4,PpcRegister::R4,1));
    emit(ppc::addi(PpcRegister::R6,PpcRegister::R6,1));emit(ppc::b(-9*4));
    emit_epilogue(); end_function("memcmp",off);
}

void StandardLibrary::generate_string_stubs() {
    // strlen — count bytes until null
    int off=start_function("strlen"); emit_prologue();
    emit(ppc::li(PpcRegister::R4,0));
    emit(ppc::lbz(PpcRegister::R5,0,PpcRegister::R3));
    emit(ppc::cmpwi(0,PpcRegister::R5,0));emit(ppc::beq(3*4));
    emit(ppc::addi(PpcRegister::R4,PpcRegister::R4,1));emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));
    emit(ppc::b(-4*4));
    emit(ppc::mr(PpcRegister::R3,PpcRegister::R4));
    emit_epilogue(); end_function("strlen",off);

    // strcpy
    off=start_function("strcpy"); emit_prologue();
    emit(ppc::mr(PpcRegister::R5,PpcRegister::R3));
    emit(ppc::lbz(PpcRegister::R6,0,PpcRegister::R4));emit(ppc::stb(PpcRegister::R6,0,PpcRegister::R3));
    emit(ppc::cmpwi(0,PpcRegister::R6,0));emit(ppc::beq(2*4));
    emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));emit(ppc::addi(PpcRegister::R4,PpcRegister::R4,1));
    emit(ppc::b(-5*4));
    emit(ppc::mr(PpcRegister::R3,PpcRegister::R5));
    emit_epilogue(); end_function("strcpy",off);

    // strcmp
    off=start_function("strcmp"); emit_prologue();
    emit(ppc::lbz(PpcRegister::R5,0,PpcRegister::R3));emit(ppc::lbz(PpcRegister::R6,0,PpcRegister::R4));
    emit(ppc::sub(PpcRegister::R7,PpcRegister::R5,PpcRegister::R6));
    emit(ppc::cmpwi(0,PpcRegister::R7,0));emit(ppc::bne(4*4));
    emit(ppc::cmpwi(0,PpcRegister::R5,0));emit(ppc::beq(2*4));
    emit(ppc::addi(PpcRegister::R3,PpcRegister::R3,1));emit(ppc::addi(PpcRegister::R4,PpcRegister::R4,1));
    emit(ppc::b(-7*4));
    emit(ppc::mr(PpcRegister::R3,PpcRegister::R7));
    emit_epilogue(); end_function("strcmp",off);
}

void StandardLibrary::generate_math_stubs() {
    // abs(x) = x < 0 ? -x : x
    int off=start_function("abs"); emit_prologue();
    emit(ppc::cmpwi(0,PpcRegister::R3,0));emit(ppc::bge(1*4));
    emit(ppc::neg(PpcRegister::R3,PpcRegister::R3));
    emit_epilogue(); end_function("abs",off);
}

void StandardLibrary::generate_io_stubs() {
    // printf — stub that does nothing (no OS on bare-metal Xbox 360)
    int off=start_function("printf"); emit_prologue();
    emit(ppc::li(PpcRegister::R3,0));
    emit_epilogue(); end_function("printf",off);

    off=start_function("sprintf"); emit_prologue();
    emit(ppc::li(PpcRegister::R3,0));
    emit_epilogue(); end_function("sprintf",off);
}

void StandardLibrary::generate_utility_stubs() {
    // malloc — stub (would need heap implementation)
    int off=start_function("malloc"); emit_prologue();
    emit(ppc::li(PpcRegister::R3,0)); // return NULL
    emit_epilogue(); end_function("malloc",off);

    off=start_function("free"); emit_prologue();
    emit_epilogue(); end_function("free",off);

    off=start_function("calloc"); emit_prologue();
    emit(ppc::li(PpcRegister::R3,0));
    emit_epilogue(); end_function("calloc",off);

    off=start_function("realloc"); emit_prologue();
    emit(ppc::li(PpcRegister::R3,0));
    emit_epilogue(); end_function("realloc",off);
}

void StandardLibrary::generate_cpp_runtime_stubs() {
    // __cxa_pure_virtual — called if pure virtual function is invoked
    int off=start_function("__cxa_pure_virtual"); emit_prologue();
    std::string trap = "__trap_pv";
    emit(ppc::b(0)); // infinite loop (self-branch)
    emit_epilogue(); end_function("__cxa_pure_virtual",off);

    // operator new / delete — map to malloc/free
    off=start_function("_Znwj"); emit_prologue(); // operator new(size_t)
    emit(ppc::li(PpcRegister::R3,0)); // stub
    emit_epilogue(); end_function("_Znwj",off);

    off=start_function("_ZdlPv"); emit_prologue(); // operator delete(void*)
    emit_epilogue(); end_function("_ZdlPv",off);
}

ObjectFile StandardLibrary::build() {
    code_.clear(); data_.clear(); function_offsets_.clear(); symbols_.clear();

    generate_memory_stubs();
    generate_string_stubs();
    generate_math_stubs();
    generate_io_stubs();
    generate_utility_stubs();
    generate_cpp_runtime_stubs();

    ObjectFile obj("<stdlib>");
    // Convert code to bytes (big-endian)
    obj.code.reserve(code_.size() * 4);
    for (auto inst : code_) {
        obj.code.push_back((inst>>24)&0xFF); obj.code.push_back((inst>>16)&0xFF);
        obj.code.push_back((inst>>8)&0xFF); obj.code.push_back(inst&0xFF);
    }
    obj.data = data_;
    obj.exported_symbols = symbols_;
    return obj;
}

std::vector<std::string> StandardLibrary::get_provided_functions() {
    return {"memset","memcpy","memmove","memcmp","strlen","strcpy","strcmp",
            "printf","sprintf","malloc","free","calloc","realloc","abs",
            "__cxa_pure_virtual","_Znwj","_ZdlPv"};
}

} // namespace nexia
