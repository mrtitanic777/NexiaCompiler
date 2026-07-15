// NexiaCompiler v2.0 — COFF Library Reader
// Parses Xbox 360 SDK .lib files (standard COFF archive format)

#include "coff_reader.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <unordered_set>

namespace nexia {

// Xbox 360 PPC machine type
static constexpr uint16_t IMAGE_FILE_MACHINE_POWERPCBE = 0x01F2;
static constexpr uint16_t IMAGE_FILE_MACHINE_POWERPC   = 0x01F0;

// COFF relocation types for PPC
static constexpr uint16_t IMAGE_REL_PPC_ADDR32    = 0x0002;
static constexpr uint16_t IMAGE_REL_PPC_ADDR16    = 0x0004;
static constexpr uint16_t IMAGE_REL_PPC_REL24     = 0x0006;
static constexpr uint16_t IMAGE_REL_PPC_REL14     = 0x0007;
static constexpr uint16_t IMAGE_REL_PPC_ADDR16_LO = 0x0008;
static constexpr uint16_t IMAGE_REL_PPC_ADDR16_HI = 0x0010;
static constexpr uint16_t IMAGE_REL_PPC_ADDR16_HA = 0x0012;
static constexpr uint16_t IMAGE_REL_PPC_TOCREL16  = 0x000A;
static constexpr uint16_t IMAGE_REL_PPC_SECREL    = 0x000B;
static constexpr uint16_t IMAGE_REL_PPC_SECTION   = 0x000C;

// Section characteristics
static constexpr uint32_t IMAGE_SCN_CNT_CODE          = 0x00000020;
static constexpr uint32_t IMAGE_SCN_CNT_INITIALIZED   = 0x00000040;
static constexpr uint32_t IMAGE_SCN_CNT_UNINITIALIZED = 0x00000080;

// Symbol storage classes
static constexpr uint8_t IMAGE_SYM_CLASS_EXTERNAL  = 2;
static constexpr uint8_t IMAGE_SYM_CLASS_STATIC    = 3;
static constexpr uint8_t IMAGE_SYM_CLASS_FUNCTION  = 101;
static constexpr uint8_t IMAGE_SYM_CLASS_FILE      = 103;

uint32_t CoffLibReader::read_be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
uint32_t CoffLibReader::read_le32(const uint8_t* p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
uint16_t CoffLibReader::read_le16(const uint8_t* p) {
    return p[0]|((uint16_t)p[1]<<8);
}
std::string CoffLibReader::read_string(const uint8_t* p, int maxLen) {
    std::string s;
    for (int i = 0; i < maxLen && p[i] != 0; i++) s += (char)p[i];
    return s;
}

bool CoffLibReader::load(const std::string& path) {
    filename_ = path;
    std::ifstream f(path, std::ios::binary);
    if (!f) { errors_.push_back("Cannot open: " + path); return false; }
    file_data_.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (file_data_.size() < 8) { errors_.push_back("File too small: " + path); return false; }
    return parse_archive();
}

bool CoffLibReader::parse_archive() {
    // Check signature: "!<arch>\n"
    if (memcmp(file_data_.data(), "!<arch>\n", 8) != 0) {
        errors_.push_back("Not a COFF archive: " + filename_);
        return false;
    }

    uint32_t pos = 8;
    int memberIndex = 0;

    while (pos + 60 <= file_data_.size()) {
        // Archive member header (60 bytes)
        const uint8_t* hdr = file_data_.data() + pos;

        // Name: 16 bytes, padded with spaces
        std::string name(reinterpret_cast<const char*>(hdr), 16);
        // Strip trailing spaces
        size_t end = name.find_last_not_of(' ');
        if (end != std::string::npos) name = name.substr(0, end + 1);
        else name.clear();

        // Size: 10 bytes ASCII decimal at offset 48
        std::string sizeStr(reinterpret_cast<const char*>(hdr + 48), 10);
        uint32_t memberSize = 0;
        try { memberSize = (uint32_t)std::stoul(sizeStr); } catch (...) { break; }

        // End marker: 0x60 0x0A at offset 58
        if (hdr[58] != '`' || hdr[59] != '\n') {
            // Not a valid header — try to skip
            pos += 2;
            continue;
        }

        uint32_t dataOffset = pos + 60;
        if (dataOffset + memberSize > file_data_.size()) break;

        if (memberIndex == 0) {
            // First linker member (symbol table) — big-endian
            parse_first_linker_member(file_data_.data() + dataOffset, memberSize);
        } else if (memberIndex == 1 && (name == "/" || name == "//")) {
            // Second linker member or long names
            parse_second_linker_member(file_data_.data() + dataOffset, memberSize);
        } else {
            // Regular object member
            CoffArchiveMember member;
            member.name = name;
            member.offset = dataOffset;
            member.size = memberSize;
            members_.push_back(member);
        }

        memberIndex++;
        // Advance past member data, aligned to 2 bytes
        pos = dataOffset + memberSize;
        if (pos % 2 != 0) pos++;
    }

    std::cout << "    Loaded " << filename_ << ": " << exported_symbols_.size()
              << " symbols, " << member_offsets_.size() << " members\n";
    return true;
}

bool CoffLibReader::parse_first_linker_member(const uint8_t* data, uint32_t size) {
    if (size < 4) return false;

    // First linker member: big-endian symbol count, then offsets, then strings
    uint32_t numSymbols = read_be32(data);
    if (4 + numSymbols * 4 > size) return false;

    // Read member offsets (big-endian)
    std::vector<uint32_t> offsets(numSymbols);
    for (uint32_t i = 0; i < numSymbols; i++)
        offsets[i] = read_be32(data + 4 + i * 4);

    // Read symbol names (null-terminated strings)
    uint32_t strPos = 4 + numSymbols * 4;
    for (uint32_t i = 0; i < numSymbols && strPos < size; i++) {
        std::string sym;
        while (strPos < size && data[strPos] != 0)
            sym += (char)data[strPos++];
        strPos++; // skip null terminator

        if (!sym.empty()) {
            exported_symbols_.push_back(sym);
            symbol_map_[sym] = offsets[i];

            // Also map the demangled/undecorated name
            // MSVC mangled: ?Name@@... or _Name@... or __imp_?Name@@...
            std::string demangled;
            std::string s = sym;
            // Strip __imp_ prefix (import thunk)
            if (s.substr(0, 6) == "__imp_") s = s.substr(6);
            if (!s.empty() && s[0] == '?') {
                // MSVC C++ mangled: ?Name@Scope@@...
                size_t at = s.find('@', 1);
                if (at != std::string::npos)
                    demangled = s.substr(1, at - 1);
            } else if (!s.empty() && s[0] == '_') {
                // C decorated: _Name or _Name@N (stdcall)
                demangled = s.substr(1);
                size_t atSign = demangled.find('@');
                if (atSign != std::string::npos)
                    demangled = demangled.substr(0, atSign);
            } else {
                demangled = s;
            }
            if (!demangled.empty() && demangled != sym) {
                // Only add if not already mapped (first match wins)
                if (!symbol_map_.count(demangled))
                    symbol_map_[demangled] = offsets[i];
            }

            if (std::find(member_offsets_.begin(), member_offsets_.end(), offsets[i]) == member_offsets_.end())
                member_offsets_.push_back(offsets[i]);
        }
    }

    return true;
}

bool CoffLibReader::parse_second_linker_member(const uint8_t* data, uint32_t size) {
    // Second linker member is little-endian and organized differently
    // We already have what we need from the first member
    (void)data; (void)size;
    return true;
}

CoffObject CoffLibReader::parse_coff_object(const uint8_t* data, uint32_t size, const std::string& name) {
    CoffObject obj;
    obj.name = name;

    if (size < 20) return obj; // COFF header is 20 bytes minimum

    obj.machine = read_le16(data);
    uint16_t numSections = read_le16(data + 2);
    uint32_t symbolTableOffset = read_le32(data + 8);
    uint32_t numSymbols = read_le32(data + 12);
    uint16_t optHeaderSize = read_le16(data + 16);

    // Parse sections
    uint32_t sectionOffset = 20 + optHeaderSize;
    for (uint16_t i = 0; i < numSections && sectionOffset + 40 <= size; i++) {
        const uint8_t* sh = data + sectionOffset;
        CoffSection sec;
        sec.name = read_string(sh, 8);
        sec.virtual_size = read_le32(sh + 8);
        sec.virtual_address = read_le32(sh + 12);
        sec.raw_data_size = read_le32(sh + 16);
        sec.raw_data_offset = read_le32(sh + 20);
        sec.reloc_offset = read_le32(sh + 24);
        sec.num_relocs = read_le16(sh + 32);
        sec.characteristics = read_le32(sh + 36);

        // Read section data
        if (sec.raw_data_offset > 0 && sec.raw_data_size > 0 &&
            sec.raw_data_offset + sec.raw_data_size <= size) {
            sec.data.assign(data + sec.raw_data_offset,
                          data + sec.raw_data_offset + sec.raw_data_size);
        }

        // Categorize section
        if (sec.name == ".text" || (sec.characteristics & IMAGE_SCN_CNT_CODE)) {
            obj.text.insert(obj.text.end(), sec.data.begin(), sec.data.end());
        } else if (sec.name == ".data" || sec.name == ".rdata" ||
                   (sec.characteristics & IMAGE_SCN_CNT_INITIALIZED)) {
            obj.data.insert(obj.data.end(), sec.data.begin(), sec.data.end());
        } else if (sec.name == ".bss" || (sec.characteristics & IMAGE_SCN_CNT_UNINITIALIZED)) {
            obj.bss.resize(obj.bss.size() + sec.virtual_size, 0);
        }

        obj.sections.push_back(std::move(sec));
        sectionOffset += 40;
    }

    // Parse symbol table
    if (symbolTableOffset > 0 && symbolTableOffset + numSymbols * 18 <= size) {
        // String table is right after the symbol table
        uint32_t strTableOffset = symbolTableOffset + numSymbols * 18;
        uint32_t strTableSize = 0;
        if (strTableOffset + 4 <= size)
            strTableSize = read_le32(data + strTableOffset);
        (void)strTableSize; // used for bounds checking if needed

        for (uint32_t i = 0; i < numSymbols; i++) {
            const uint8_t* se = data + symbolTableOffset + i * 18;
            CoffSymbol sym;

            // Name: first 4 bytes are 0 means it's in string table
            if (se[0] == 0 && se[1] == 0 && se[2] == 0 && se[3] == 0) {
                uint32_t strOffset = read_le32(se + 4);
                if (strTableOffset + strOffset < size)
                    sym.name = read_string(data + strTableOffset + strOffset, (int)(size - strTableOffset - strOffset));
            } else {
                sym.name = read_string(se, 8);
            }

            sym.value = (int32_t)read_le32(se + 8);
            sym.section_number = (int16_t)read_le16(se + 12);
            sym.type = read_le16(se + 14);
            sym.storage_class = se[16];
            sym.num_aux = se[17];
            sym.is_external = (sym.storage_class == IMAGE_SYM_CLASS_EXTERNAL);
            sym.is_function = ((sym.type >> 4) == 2); // DTYPE_FUNCTION

            obj.symbols.push_back(sym);

            // Skip auxiliary symbol entries
            i += sym.num_aux;
        }
    }

    return obj;
}

ObjectFile CoffLibReader::extract_member(uint32_t memberOffset) {
    // The memberOffset points into the archive file
    // We need to read the archive member header at that offset
    if (memberOffset + 60 > file_data_.size()) {
        return ObjectFile("<invalid>");
    }

    const uint8_t* hdr = file_data_.data() + memberOffset;

    // Parse member header
    std::string name(reinterpret_cast<const char*>(hdr), 16);
    size_t nameEnd = name.find_last_not_of(' ');
    if (nameEnd != std::string::npos) name = name.substr(0, nameEnd + 1);

    std::string sizeStr(reinterpret_cast<const char*>(hdr + 48), 10);
    uint32_t memberSize = 0;
    try { memberSize = (uint32_t)std::stoul(sizeStr); } catch (...) { return ObjectFile("<invalid>"); }

    uint32_t dataOffset = memberOffset + 60;
    if (dataOffset + memberSize > file_data_.size()) return ObjectFile("<invalid>");

    const uint8_t* memberData = file_data_.data() + dataOffset;

    // ---------------------------------------------------------------
    // Check for IMPORT_OBJECT_HEADER (import definition member)
    // These are short records that define a DLL import, not real COFF.
    // Layout: sig1(2) sig2(2) version(2) machine(2) timestamp(4)
    //         size_of_data(4) ordinal_hint(2) type/name_type(2)
    //         then two null-terminated strings: symbol name, DLL name
    // sig1 = 0x0000, sig2 = 0xFFFF
    // ---------------------------------------------------------------
    if (memberSize >= 20) {
        uint16_t sig1 = read_le16(memberData);
        uint16_t sig2 = read_le16(memberData + 2);

        if (sig1 == 0x0000 && sig2 == 0xFFFF) {
            // This is an import definition member
            uint16_t impVersion  = read_le16(memberData + 4);
            uint16_t impMachine  = read_le16(memberData + 6);
            // uint32_t timestamp = read_le32(memberData + 8);
            // uint32_t dataSize  = read_le32(memberData + 12);
            uint16_t ordinalHint = read_le16(memberData + 16);
            uint16_t typeInfo    = read_le16(memberData + 18);

            (void)impVersion;
            (void)impMachine;

            // Import type (bits 0-1): 0=code, 1=data, 2=const
            // Name type (bits 2-4): 0=ordinal, 1=name, 2=name_no_prefix, 3=name_undecorate
            uint16_t importType = typeInfo & 0x3;
            uint16_t nameType   = (typeInfo >> 2) & 0x7;
            (void)importType;

            // Read symbol name and DLL name (two null-terminated strings after header)
            std::string symbolName, dllName;
            uint32_t strPos = 20;
            while (strPos < memberSize && memberData[strPos] != 0)
                symbolName += (char)memberData[strPos++];
            strPos++; // skip null
            while (strPos < memberSize && memberData[strPos] != 0)
                dllName += (char)memberData[strPos++];

            // Strip version suffix from DLL name
            // Xbox 360 SDK libs use versioned names like "xam.xex@21256.0+1861.0"
            // or "xboxkrnl.exe@21256.0+1861" — strip everything from '@' onwards
            {
                size_t atPos = dllName.find('@');
                if (atPos != std::string::npos)
                    dllName = dllName.substr(0, atPos);
            }

            ObjectFile obj(name);
            obj.dll_imports.push_back({symbolName, dllName, ordinalHint});

            // Register both the raw symbol and common variants as "exported"
            // so the linker considers them resolved
            obj.exported_symbols.emplace_back(
                symbolName, LinkerSymbolKind::Function, 0, 0, LinkerSection::Text, true);

            // Also register __imp_ variant
            std::string impName = "__imp_" + symbolName;
            obj.exported_symbols.emplace_back(
                impName, LinkerSymbolKind::Function, 0, 0, LinkerSection::Text, true);

            // For name_type==3 (undecorate), also register the undecorated form
            if (nameType == 3 || nameType == 2) {
                // Strip leading _ or ? prefix
                std::string clean = symbolName;
                if (!clean.empty() && (clean[0] == '_' || clean[0] == '?'))
                    clean = clean.substr(1);
                // Strip everything after @ for stdcall/fastcall
                size_t at = clean.find('@');
                if (at != std::string::npos) clean = clean.substr(0, at);
                if (!clean.empty() && clean != symbolName) {
                    obj.exported_symbols.emplace_back(
                        clean, LinkerSymbolKind::Function, 0, 0, LinkerSection::Text, true);
                    obj.dll_imports.push_back({clean, dllName, ordinalHint});
                }
            }

            return obj;
        }
    }

    // Regular COFF object member
    CoffObject coff = parse_coff_object(memberData, memberSize, name);

    // Convert to our ObjectFile format
    ObjectFile obj(name);
    obj.code = coff.text;
    obj.data = coff.data;
    // Append rdata to data
    obj.data.insert(obj.data.end(), coff.rdata.begin(), coff.rdata.end());

    // Register exported symbols
    for (auto& sym : coff.symbols) {
        if (sym.is_external && sym.section_number > 0 && !sym.name.empty()) {
            LinkerSection sec = LinkerSection::Text;
            if (sym.section_number <= (int)coff.sections.size()) {
                auto& coffSec = coff.sections[sym.section_number - 1];
                if (coffSec.characteristics & IMAGE_SCN_CNT_CODE)
                    sec = LinkerSection::Text;
                else
                    sec = LinkerSection::Data;
            }
            obj.exported_symbols.emplace_back(
                sym.name, LinkerSymbolKind::Function, sym.value, 0, sec, true);
        }
        // Track imported (undefined external) symbols
        if (sym.is_external && sym.section_number == 0 && !sym.name.empty()) {
            obj.imported_symbols.push_back(sym.name);
        }
    }

    return obj;
}

std::vector<ObjectFile> CoffLibReader::extract_needed(const std::vector<std::string>& needed_symbols) {
    std::vector<ObjectFile> result;
    std::unordered_set<uint32_t> extracted_offsets;
    std::unordered_set<std::string> resolved;
    std::vector<std::string> pending = needed_symbols;

    // Iteratively extract members until all symbols are resolved
    // (extracted members may import new symbols that need resolving)
    int iterations = 0;
    while (!pending.empty() && iterations < 100) {
        iterations++;
        std::vector<std::string> new_pending;

        for (auto& sym : pending) {
            if (resolved.count(sym)) continue;

            auto it = symbol_map_.find(sym);
            if (it == symbol_map_.end()) continue; // not in this library

            uint32_t memberOff = it->second;
            if (extracted_offsets.count(memberOff)) {
                resolved.insert(sym);
                continue; // already extracted this member
            }

            extracted_offsets.insert(memberOff);
            ObjectFile obj = extract_member(memberOff);

            // Mark all exported symbols as resolved
            for (auto& exp : obj.exported_symbols)
                resolved.insert(exp.name);

            // Add imported symbols to new_pending
            for (auto& imp : obj.imported_symbols) {
                if (!resolved.count(imp))
                    new_pending.push_back(imp);
            }

            result.push_back(std::move(obj));
        }

        pending = std::move(new_pending);
    }

    return result;
}

} // namespace nexia
