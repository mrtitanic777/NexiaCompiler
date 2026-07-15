#pragma once
// NexiaCompiler v2.0 — ELF Writer
// Ported from ElfWriter.cs
// Writes big-endian PPC ELF executables for Xbox 360.

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace nexia {

class ElfWriter {
public:
    void write(const std::string& outputPath, const std::vector<uint8_t>& code,
               const std::vector<uint8_t>& data,
               const std::unordered_map<std::string, int>& functionOffsets,
               const std::string& entryPoint = "main");

private:
    static constexpr uint8_t  ELFCLASS32 = 1;
    static constexpr uint8_t  ELFDATA2MSB = 2;
    static constexpr uint16_t ET_EXEC = 2;
    static constexpr uint16_t EM_PPC = 20;
    static constexpr uint32_t EV_CURRENT = 1;

    static constexpr uint32_t SHT_NULL     = 0;
    static constexpr uint32_t SHT_PROGBITS = 1;
    static constexpr uint32_t SHT_SYMTAB   = 2;
    static constexpr uint32_t SHT_STRTAB   = 3;
    static constexpr uint32_t SHT_NOBITS   = 8;

    static constexpr uint32_t SHF_WRITE     = 1;
    static constexpr uint32_t SHF_ALLOC     = 2;
    static constexpr uint32_t SHF_EXECINSTR = 4;

    static constexpr uint32_t PT_LOAD = 1;
    static constexpr uint32_t PF_X = 1;
    static constexpr uint32_t PF_W = 2;
    static constexpr uint32_t PF_R = 4;

    static constexpr uint32_t BASE_ADDRESS = 0x82000000;

    static std::vector<uint8_t> build_shstrtab();
    static void write_be16(std::vector<uint8_t>& buf, uint16_t value);
    static void write_be32(std::vector<uint8_t>& buf, uint32_t value);
    static void pad_to(std::vector<uint8_t>& buf, uint32_t targetOffset);
    static uint32_t align(uint32_t value, uint32_t alignment);
};

} // namespace nexia
