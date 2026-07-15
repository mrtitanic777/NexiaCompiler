#pragma once
// NexiaCompiler v2.0 — COFF Library Reader
// Parses Xbox 360 SDK .lib files (COFF archive format)

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "linker.h"

namespace nexia {

// COFF archive member header (60 bytes)
struct CoffArchiveMember {
    std::string name;
    uint32_t offset;     // offset in archive file
    uint32_t size;       // size of member data
    std::vector<uint8_t> data;
};

// COFF section header
struct CoffSection {
    std::string name;
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t raw_data_size;
    uint32_t raw_data_offset;
    uint32_t reloc_offset;
    uint32_t num_relocs;
    uint32_t characteristics;
    std::vector<uint8_t> data;
};

// COFF symbol
struct CoffSymbol {
    std::string name;
    int32_t value;
    int16_t section_number;  // 1-based, 0=external, -1=absolute, -2=debug
    uint16_t type;
    uint8_t storage_class;
    uint8_t num_aux;
    bool is_external;
    bool is_function;
};

// COFF relocation entry
struct CoffRelocation {
    uint32_t virtual_address;
    uint32_t symbol_index;
    uint16_t type;
};

// Parsed COFF object file
struct CoffObject {
    std::string name;              // member name from archive
    uint16_t machine;              // should be 0x01F2 for PPC
    std::vector<CoffSection> sections;
    std::vector<CoffSymbol> symbols;
    std::vector<std::string> string_table;
    
    // Extracted code and data
    std::vector<uint8_t> text;
    std::vector<uint8_t> data;
    std::vector<uint8_t> rdata;
    std::vector<uint8_t> bss;

    // For import definition members: these have no real code,
    // just a reference to an external DLL + ordinal
    bool is_import_def = false;
    std::string import_dll;        // e.g. "xboxkrnl.exe"
    std::string import_name;       // symbol name
    uint16_t import_ordinal = 0;   // ordinal hint
};

// COFF archive (.lib) reader
class CoffLibReader {
public:
    // Parse a .lib archive file
    bool load(const std::string& path);
    
    // Get all exported symbol names
    const std::vector<std::string>& exported_symbols() const { return exported_symbols_; }
    
    // Get symbol -> archive member index mapping
    const std::unordered_map<std::string, uint32_t>& symbol_map() const { return symbol_map_; }
    
    // Extract a specific member as an ObjectFile for linking
    ObjectFile extract_member(uint32_t member_index);
    
    // Extract all members that define any of the given symbols
    std::vector<ObjectFile> extract_needed(const std::vector<std::string>& needed_symbols);
    
    const std::string& filename() const { return filename_; }
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::string filename_;
    std::vector<uint8_t> file_data_;
    std::vector<CoffArchiveMember> members_;
    std::vector<std::string> exported_symbols_;
    std::unordered_map<std::string, uint32_t> symbol_map_; // symbol name -> member offset
    std::vector<uint32_t> member_offsets_;                  // unique member file offsets
    std::vector<std::string> errors_;
    
    // Parsing helpers
    bool parse_archive();
    bool parse_first_linker_member(const uint8_t* data, uint32_t size);
    bool parse_second_linker_member(const uint8_t* data, uint32_t size);
    CoffObject parse_coff_object(const uint8_t* data, uint32_t size, const std::string& name);
    
    static uint32_t read_be32(const uint8_t* p);
    static uint32_t read_le32(const uint8_t* p);
    static uint16_t read_le16(const uint8_t* p);
    static std::string read_string(const uint8_t* p, int maxLen);
};

} // namespace nexia
