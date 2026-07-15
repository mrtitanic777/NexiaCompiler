// NexiaCompiler v2.0 — Register Allocator
// Ported from RegisterAllocator.cs
#include "register_allocator.h"
#include <stdexcept>

namespace nexia {

const PpcRegister RegisterAllocator::TEMP_GPRS[] = {
    PpcRegister::R3, PpcRegister::R4, PpcRegister::R5, PpcRegister::R6,
    PpcRegister::R7, PpcRegister::R8, PpcRegister::R9, PpcRegister::R10,
    PpcRegister::R11, PpcRegister::R12
};
const int RegisterAllocator::TEMP_GPR_COUNT = 10;

const PpcFloatRegister RegisterAllocator::TEMP_FPRS[] = {
    PpcFloatRegister::F1, PpcFloatRegister::F2, PpcFloatRegister::F3,
    PpcFloatRegister::F4, PpcFloatRegister::F5, PpcFloatRegister::F6,
    PpcFloatRegister::F7, PpcFloatRegister::F8
};
const int RegisterAllocator::TEMP_FPR_COUNT = 8;

StorageLocation StorageLocation::in_gpr(PpcRegister reg) {
    return StorageLocation(StorageKind::GPR, reg, PpcFloatRegister::F0, 0);
}
StorageLocation StorageLocation::in_fpr(PpcFloatRegister reg) {
    return StorageLocation(StorageKind::FPR, PpcRegister::R0, reg, 0);
}
StorageLocation StorageLocation::on_stack(int offset) {
    return StorageLocation(StorageKind::Stack, PpcRegister::R0, PpcFloatRegister::F0, offset);
}
std::string StorageLocation::to_string() const {
    switch (kind) {
        case StorageKind::GPR: return "r" + std::to_string((int)gpr);
        case StorageKind::FPR: return "f" + std::to_string((int)fpr);
        case StorageKind::Stack: return "[sp+" + std::to_string(stack_offset) + "]";
    }
    return "?";
}

RegisterAllocator::RegisterAllocator() : next_stack_offset_(0) {}

int RegisterAllocator::align(int value, int alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

StorageLocation RegisterAllocator::allocate_local(const std::string& name, const TypeInfo& type) {
    int size = type.is_pointer() ? 4 : type.size;
    if (size < 4) size = 4;
    next_stack_offset_ = align(next_stack_offset_ + size, 4);
    auto loc = StorageLocation::on_stack(-next_stack_offset_);
    variables_[name] = loc;
    return loc;
}

StorageLocation RegisterAllocator::allocate_parameter(const std::string& name, const TypeInfo& type, int paramIndex) {
    bool isFloat = type.is_floating_point() && !type.is_pointer();
    StorageLocation loc = isFloat
        ? StorageLocation::in_fpr(TEMP_FPRS[paramIndex < TEMP_FPR_COUNT ? paramIndex : 0])
        : StorageLocation::in_gpr(TEMP_GPRS[paramIndex < TEMP_GPR_COUNT ? paramIndex : 0]);

    if (paramIndex >= (isFloat ? TEMP_FPR_COUNT : TEMP_GPR_COUNT)) {
        int size = type.is_pointer() ? 4 : type.size;
        if (size < 4) size = 4;
        next_stack_offset_ = align(next_stack_offset_ + size, 4);
        loc = StorageLocation::on_stack(-next_stack_offset_);
    }
    variables_[name] = loc;
    return loc;
}

const StorageLocation* RegisterAllocator::get_variable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) return &it->second;
    return nullptr;
}

PpcRegister RegisterAllocator::get_temp_gpr() {
    if (next_temp_gpr_ >= TEMP_GPR_COUNT)
        next_temp_gpr_ = 0; // wrap around — reuse temps for deeply nested expressions
    return TEMP_GPRS[next_temp_gpr_++];
}

PpcFloatRegister RegisterAllocator::get_temp_fpr() {
    if (next_temp_fpr_ >= TEMP_FPR_COUNT)
        next_temp_fpr_ = 0; // wrap around
    return TEMP_FPRS[next_temp_fpr_++];
}

void RegisterAllocator::reset_temps() { next_temp_gpr_ = 0; next_temp_fpr_ = 0; }

int RegisterAllocator::calculate_frame_size() const {
    int size = next_stack_offset_ + 8; // lr save + back chain
    size += (int)used_callee_saved_gprs_.size() * 4;
    size += (int)used_callee_saved_fprs_.size() * 8;
    return align(size, 16);
}

int RegisterAllocator::locals_base_offset() const {
    return 8 + (int)used_callee_saved_gprs_.size() * 4 +
           (int)used_callee_saved_fprs_.size() * 8;
}

} // namespace nexia
