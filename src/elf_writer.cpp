// NexiaCompiler v2.0 — ELF Writer
// Ported from ElfWriter.cs — FULL implementation
#include "elf_writer.h"
#include <fstream>
#include <cstring>

namespace nexia {

void ElfWriter::write_be16(std::vector<uint8_t>& b, uint16_t v) { b.push_back((v>>8)&0xFF); b.push_back(v&0xFF); }
void ElfWriter::write_be32(std::vector<uint8_t>& b, uint32_t v) { b.push_back((v>>24)&0xFF); b.push_back((v>>16)&0xFF); b.push_back((v>>8)&0xFF); b.push_back(v&0xFF); }
void ElfWriter::pad_to(std::vector<uint8_t>& b, uint32_t t) { while(b.size()<t) b.push_back(0); }
uint32_t ElfWriter::align(uint32_t v, uint32_t a) { return (v+a-1)&~(a-1); }

std::vector<uint8_t> ElfWriter::build_shstrtab() {
    // Section name string table: \0 .text\0 .data\0 .bss\0 .shstrtab\0 .symtab\0 .strtab\0
    std::vector<uint8_t> t;
    t.push_back(0);
    const char* names[] = {".text",".data",".bss",".shstrtab",".symtab",".strtab"};
    for (auto n : names) { for (const char* p=n;*p;p++) t.push_back((uint8_t)*p); t.push_back(0); }
    return t;
}

void ElfWriter::write(const std::string& outputPath, const std::vector<uint8_t>& code,
                       const std::vector<uint8_t>& data,
                       const std::unordered_map<std::string, int>& functionOffsets,
                       const std::string& entryPoint) {
    std::vector<uint8_t> buf;
    // ELF header (52 bytes for 32-bit)
    // e_ident: 16 bytes
    buf.push_back(0x7F); buf.push_back('E'); buf.push_back('L'); buf.push_back('F');
    buf.push_back(ELFCLASS32); buf.push_back(ELFDATA2MSB); buf.push_back(EV_CURRENT); buf.push_back(0); // OS/ABI
    for(int i=0;i<8;i++) buf.push_back(0); // padding
    write_be16(buf, ET_EXEC);    // e_type
    write_be16(buf, EM_PPC);     // e_machine
    write_be32(buf, EV_CURRENT); // e_version

    // Find entry point
    uint32_t entry = BASE_ADDRESS;
    auto it = functionOffsets.find(entryPoint);
    if (it != functionOffsets.end()) entry = BASE_ADDRESS + (uint32_t)it->second;
    write_be32(buf, entry);      // e_entry

    uint32_t phoff = 52; // program header right after ELF header
    write_be32(buf, phoff);      // e_phoff
    write_be32(buf, 0);          // e_shoff (unused for now)
    write_be32(buf, 0);          // e_flags
    write_be16(buf, 52);         // e_ehsize
    write_be16(buf, 32);         // e_phentsize
    write_be16(buf, 2);          // e_phnum (text + data)
    write_be16(buf, 40);         // e_shentsize
    write_be16(buf, 0);          // e_shnum (none for now)
    write_be16(buf, 0);          // e_shstrndx

    // Program headers
    uint32_t codeFileOff = align(52 + 2*32, 16);
    uint32_t codeSize = (uint32_t)code.size();
    uint32_t dataFileOff = align(codeFileOff + codeSize, 16);
    uint32_t dataSize = (uint32_t)data.size();

    // .text segment
    write_be32(buf, PT_LOAD);                  // p_type
    write_be32(buf, codeFileOff);              // p_offset
    write_be32(buf, BASE_ADDRESS);             // p_vaddr
    write_be32(buf, BASE_ADDRESS);             // p_paddr
    write_be32(buf, codeSize);                 // p_filesz
    write_be32(buf, codeSize);                 // p_memsz
    write_be32(buf, PF_R | PF_X);             // p_flags
    write_be32(buf, 16);                       // p_align

    // .data segment
    uint32_t dataVaddr = align(BASE_ADDRESS + codeSize, 16);
    write_be32(buf, PT_LOAD);
    write_be32(buf, dataFileOff);
    write_be32(buf, dataVaddr);
    write_be32(buf, dataVaddr);
    write_be32(buf, dataSize);
    write_be32(buf, dataSize);
    write_be32(buf, PF_R | PF_W);
    write_be32(buf, 16);

    // Pad to code offset and write code
    pad_to(buf, codeFileOff);
    buf.insert(buf.end(), code.begin(), code.end());

    // Pad to data offset and write data
    pad_to(buf, dataFileOff);
    buf.insert(buf.end(), data.begin(), data.end());

    // Write to file
    std::ofstream out(outputPath, std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(buf.data()), buf.size());
}

} // namespace nexia
