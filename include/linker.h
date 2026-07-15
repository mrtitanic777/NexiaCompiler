#pragma once
// NexiaCompiler v2.0 — Linker
// Ported from Linker.cs

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <stdexcept>

namespace nexia {

enum class LinkerSymbolKind : uint8_t { Function, Variable, Label };
enum class LinkerSection : uint8_t { Text, Data, Bss, Rodata };
enum class RelocationType : uint8_t {
    PPC_ADDR32,
    PPC_ADDR16_LO, PPC_ADDR16_HI, PPC_ADDR16_HA,
    PPC_REL24, PPC_REL14
};

struct LinkerSymbol {
    std::string name;
    LinkerSymbolKind kind;
    int offset;
    int size;
    LinkerSection section;
    bool is_public;

    LinkerSymbol(std::string name, LinkerSymbolKind kind, int offset, int size,
                 LinkerSection section, bool isPublic)
        : name(std::move(name)), kind(kind), offset(offset), size(size),
          section(section), is_public(isPublic) {}
};

struct Relocation {
    std::string symbol_name;
    int offset;
    RelocationType type;
    int addend;

    Relocation(std::string symbolName, int off, RelocationType type, int addend = 0)
        : symbol_name(std::move(symbolName)), offset(off), type(type), addend(addend) {}
};

struct ObjectFile {
    std::string source_file;
    std::vector<uint8_t> code;
    std::vector<uint8_t> data;
    std::vector<LinkerSymbol> exported_symbols;
    std::vector<std::string> imported_symbols;
    std::vector<Relocation> relocations;

    // For symbols that came from import definition .lib members
    // (no real code — just a PE import reference)
    struct DllImport {
        std::string symbol_name;
        std::string dll_name;     // e.g. "xboxkrnl.exe"
        uint16_t ordinal;
    };
    std::vector<DllImport> dll_imports;

    explicit ObjectFile(std::string sourceFile) : source_file(std::move(sourceFile)) {}
};

class LinkerError : public std::runtime_error {
public:
    LinkerError(const std::string& message)
        : std::runtime_error("Linker error: " + message) {}
};

class Linker {
public:
    void add_object(ObjectFile obj);
    void add_library(const std::string& libPath);
    bool link(const std::string& outputPath);

    const std::vector<std::string>& errors() const { return errors_; }
    const std::vector<std::string>& warnings() const { return warnings_; }

private:
    static constexpr uint32_t BASE_ADDRESS = 0x82000000;

    std::vector<ObjectFile> objects_;
    std::vector<std::string> lib_paths_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;

    // Import table: DLL name -> list of (symbol_name, ordinal) pairs
    // Xbox 360 uses ordinal imports: xboxkrnl.exe and xam.xex
    struct ImportEntry {
        std::string symbol_name;
        uint16_t ordinal;        // ordinal number for the import
        uint32_t iat_rva;        // filled during PE build — RVA of this entry's IAT slot
    };
    struct ImportModule {
        std::string dll_name;    // e.g. "xboxkrnl.exe" or "xam.xex"
        std::vector<ImportEntry> entries;
    };

    // Collect unresolved externals into import modules
    std::vector<ImportModule> build_import_table(
        const std::unordered_set<std::string>& unresolved_symbols);

    // Build .idata section bytes and return the section data + RVA info
    struct IdataInfo {
        std::vector<uint8_t> data;        // raw .idata section bytes
        uint32_t idt_rva_offset;          // offset within .idata where IDT starts (always 0)
        uint32_t idt_size;                // size of the Import Directory Table
        uint32_t iat_rva_offset;          // offset within .idata where IAT starts
        uint32_t iat_size;                // size of the Import Address Table
    };
    IdataInfo build_idata_section(std::vector<ImportModule>& imports, uint32_t section_rva);

    void write_pe(const std::string& path, const std::vector<uint8_t>& code,
                   const std::vector<uint8_t>& data,
                   const std::unordered_map<std::string, int>& funcOffsets,
                   const std::string& entryName,
                   std::vector<ImportModule>& imports);

    void build_global_symbol_table(
        std::unordered_map<std::string, std::pair<int, int>>& symbolTable,
        const std::vector<int>& codeOffsets,
        const std::vector<int>& dataOffsets);

    void check_unresolved_symbols(
        const std::unordered_map<std::string, std::pair<int, int>>& symbolTable);
};

} // namespace nexia
