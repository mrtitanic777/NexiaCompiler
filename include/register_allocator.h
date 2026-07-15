#pragma once
// NexiaCompiler v2.0 — Register Allocator
// Ported from RegisterAllocator.cs

#include <cstdint>
#include "ppc_instructions.h"
#include "symbol_table.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace nexia {

enum class StorageKind : uint8_t {
    GPR,       // General Purpose Register
    FPR,       // Floating Point Register
    Stack      // Stack-relative
};

struct StorageLocation {
    StorageKind kind;
    PpcRegister gpr;
    PpcFloatRegister fpr;
    int stack_offset;

    StorageLocation()
        : kind(StorageKind::Stack), gpr(PpcRegister::R0),
          fpr(PpcFloatRegister::F0), stack_offset(0) {}

    static StorageLocation in_gpr(PpcRegister reg);
    static StorageLocation in_fpr(PpcFloatRegister reg);
    static StorageLocation on_stack(int offset);

    std::string to_string() const;

    StorageLocation(StorageKind k, PpcRegister g, PpcFloatRegister f, int off)
        : kind(k), gpr(g), fpr(f), stack_offset(off) {}
};

class RegisterAllocator {
public:
    RegisterAllocator();

    StorageLocation allocate_local(const std::string& name, const TypeInfo& type);
    StorageLocation allocate_parameter(const std::string& name, const TypeInfo& type, int paramIndex);
    const StorageLocation* get_variable(const std::string& name) const;

    PpcRegister get_temp_gpr();
    PpcFloatRegister get_temp_fpr();
    void reset_temps();

    int locals_size() const { return next_stack_offset_; }
    int calculate_frame_size() const;
    int locals_base_offset() const;

    const std::vector<PpcRegister>& used_callee_saved_gprs() const { return used_callee_saved_gprs_; }
    const std::vector<PpcFloatRegister>& used_callee_saved_fprs() const { return used_callee_saved_fprs_; }

private:
    static const PpcRegister TEMP_GPRS[];
    static const int TEMP_GPR_COUNT;
    static const PpcFloatRegister TEMP_FPRS[];
    static const int TEMP_FPR_COUNT;

    int next_temp_gpr_ = 0;
    int next_temp_fpr_ = 0;
    int next_stack_offset_;

    std::unordered_map<std::string, StorageLocation> variables_;
    std::vector<PpcRegister> used_callee_saved_gprs_;
    std::vector<PpcFloatRegister> used_callee_saved_fprs_;

    static int align(int value, int alignment);
};

} // namespace nexia
