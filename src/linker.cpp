// NexiaCompiler v2.0 — Linker
// Now supports COFF .lib archives from Xbox 360 SDK
// Generates PE with .idata import tables for xboxkrnl.exe and xam.xex
#include "linker.h"
#include "coff_reader.h"
#include "elf_writer.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <fstream>

namespace nexia {

// ===================================================================
// Xbox 360 Kernel Import Ordinal Tables
// These map function names to their ordinal numbers in the kernel DLLs.
// The Xbox 360 uses ordinal-based PE imports (not name-based).
// ===================================================================

struct KernelOrdinal {
    const char* name;
    uint16_t ordinal;
};

// xboxkrnl.exe ordinals (subset — most commonly used by homebrew)
static const KernelOrdinal kXboxKrnlOrdinals[] = {
    // Threading
    {"ExCreateThread",           24},
    {"ExTerminateThread",        27},
    {"KeDelayExecutionThread",  141},
    {"KeQuerySystemTime",       153},
    {"KeQueryPerformanceCounter",152},
    {"KeSetAffinityThread",     156},
    {"KeResumeThread",          155},
    {"KeSuspendThread",         161},
    {"KeWaitForSingleObject",   168},
    {"NtClose",                 187},
    {"NtCreateEvent",           188},
    {"NtSetEvent",              213},
    {"NtWaitForSingleObjectEx", 221},

    // Memory
    {"MmAllocatePhysicalMemoryEx",   173},
    {"MmFreePhysicalMemory",         176},
    {"MmGetPhysicalAddress",         178},
    {"MmMapIoSpace",                 180},
    {"NtAllocateVirtualMemory",      186},
    {"NtFreeVirtualMemory",          199},
    {"XMemAlloc",                    668},
    {"XMemFree",                     669},

    // I/O and filesystem
    {"NtCreateFile",           189},
    {"NtOpenFile",             202},
    {"NtReadFile",             207},
    {"NtWriteFile",            223},
    {"NtQueryInformationFile", 206},
    {"NtDeviceIoControlFile",  195},
    {"ObOpenObjectByName",     238},
    {"ObReferenceObjectByHandle",240},
    {"ObDereferenceObject",    236},

    // Debugging / output
    {"DbgPrint",                 7},
    {"RtlInitAnsiString",      309},
    {"RtlFreeAnsiString",      303},

    // Process / module
    {"XexGetModuleHandle",     405},
    {"XexGetProcedureAddress",  407},
    {"XexLoadImage",           409},
    {"XexUnloadImage",         417},

    // Graphics / D3D interop
    {"VdGetCurrentDisplayGamma",  	     560},
    {"VdGetCurrentDisplayInformation",   561},
    {"VdSetDisplayMode",                 571},

    // Content/XContent
    {"XeKeysGetKey",           547},

    // Misc kernel
    {"HalReturnToFirmware",    84},
    {"KeEnterCriticalRegion", 142},
    {"KeLeaveCriticalRegion", 146},
    {"KeInitializeSpinLock",  144},
    {"KeAcquireSpinLockAtRaisedIrql",136},
    {"KeReleaseSpinLockFromRaisedIrql",154},

    {nullptr, 0}
};

// xam.xex ordinals (subset — UI, content, networking)
static const KernelOrdinal kXamOrdinals[] = {
    // Content
    {"XamContentCreate",         59},
    {"XamContentCreateEx",       60},
    {"XamContentClose",          56},
    {"XamContentDelete",         57},
    {"XamContentFlush",          58},
    {"XamContentGetCreator",     61},
    {"XamContentGetThumbnail",   62},
    {"XamContentSetThumbnail",   74},
    {"XamContentCreateEnumerator",  65},

    // UI / XUI
    {"XamShowMessageBoxUI",     609},
    {"XamShowKeyboardUI",       606},
    {"XamShowDirtyDiscErrorUI", 604},
    {"XamLoaderLaunchTitle",    435},
    {"XamLoaderTerminateTitle", 436},

    // User / Profile
    {"XamUserGetSigninState",   570},
    {"XamUserGetXUID",          577},
    {"XamUserGetName",          574},
    {"XamUserCheckPrivilege",   557},

    // Notification
    {"XNotifyCreateListener",   651},
    {"XNotifyGetNext",          652},

    // Networking
    {"XNetStartup",             717},
    {"XNetCleanup",             716},

    {nullptr, 0}
};

// Look up an ordinal from the kernel tables. Returns 0 if not found.
static uint16_t find_kernel_ordinal(const std::string& name, const std::string& dll) {
    const KernelOrdinal* table = nullptr;
    if (dll == "xboxkrnl.exe") table = kXboxKrnlOrdinals;
    else if (dll == "xam.xex") table = kXamOrdinals;
    else return 0;

    // Try exact match first
    for (const KernelOrdinal* e = table; e->name; e++) {
        if (name == e->name) return e->ordinal;
    }
    // Try stripping leading underscore (C linkage decoration)
    if (!name.empty() && name[0] == '_') {
        std::string stripped = name.substr(1);
        for (const KernelOrdinal* e = table; e->name; e++) {
            if (stripped == e->name) return e->ordinal;
        }
    }
    // Try stripping __imp_ prefix
    if (name.size() > 6 && name.substr(0, 6) == "__imp_") {
        std::string stripped = name.substr(6);
        for (const KernelOrdinal* e = table; e->name; e++) {
            if (stripped == e->name) return e->ordinal;
        }
    }
    return 0;
}

void Linker::add_object(ObjectFile obj) { objects_.push_back(std::move(obj)); }
void Linker::add_library(const std::string& libPath) { lib_paths_.push_back(libPath); }

bool Linker::link(const std::string& outputPath) {
    if (objects_.empty()) { errors_.push_back("No object files to link"); return false; }

    // Phase 1: Collect all undefined symbols from compiled objects
    std::unordered_set<std::string> defined_symbols;
    std::vector<std::string> undefined_symbols;

    for (auto& obj : objects_) {
        for (auto& sym : obj.exported_symbols)
            defined_symbols.insert(sym.name);
    }
    for (auto& obj : objects_) {
        for (auto& imp : obj.imported_symbols) {
            if (!defined_symbols.count(imp))
                undefined_symbols.push_back(imp);
        }
    }

    // Phase 2: Resolve undefined symbols from .lib files
    if (!lib_paths_.empty()) {
        // Collect all undefined symbols
        for (auto& obj : objects_) {
            for (auto& imp : obj.imported_symbols) {
                if (!defined_symbols.count(imp))
                    undefined_symbols.push_back(imp);
            }
        }

        if (!undefined_symbols.empty()) {
            std::cout << "    Resolving " << undefined_symbols.size()
                      << " external symbols from " << lib_paths_.size() << " libraries...\n";

            for (auto& libPath : lib_paths_) {
                CoffLibReader reader;
                if (!reader.load(libPath)) {
                    for (auto& e : reader.errors()) warnings_.push_back(e);
                    continue;
                }

                // Find which undefined symbols this library provides
                // Try exact match first, then common decoration variants
                std::vector<std::string> needed;
                auto& symmap = reader.symbol_map();
                for (auto& sym : undefined_symbols) {
                    if (defined_symbols.count(sym)) continue;
                    // Exact match
                    if (symmap.count(sym)) { needed.push_back(sym); continue; }
                    // Try with _ prefix (C decoration)
                    if (symmap.count("_" + sym)) { needed.push_back("_" + sym); continue; }
                    // Try with __imp_ prefix (import thunk)
                    if (symmap.count("__imp_" + sym)) { needed.push_back("__imp_" + sym); continue; }
                    if (symmap.count("__imp__" + sym)) { needed.push_back("__imp__" + sym); continue; }
                    // Try stdcall variants: _Name@N for common arg counts
                    for (int n = 0; n <= 32; n += 4) {
                        std::string decorated = "_" + sym + "@" + std::to_string(n);
                        if (symmap.count(decorated)) { needed.push_back(decorated); break; }
                    }
                }

                if (needed.empty()) {
                    // Diagnostic: no symbols matched — show samples and search for partial matches
                    std::cout << "    No symbol matches in " << libPath << " — sample names:\n";
                    int shown = 0;
                    for (auto& sym : undefined_symbols) {
                        if (!defined_symbols.count(sym) && shown < 3) {
                            std::cout << "      Need: '" << sym << "'\n";
                            // Search for partial matches in the lib
                            int partials = 0;
                            for (auto& [name, off] : symmap) {
                                if (partials < 3 && name.find(sym) != std::string::npos) {
                                    std::cout << "        ~ partial: '" << name << "'\n";
                                    partials++;
                                }
                            }
                            shown++;
                        }
                    }
                    continue;
                }

                std::cout << "    Extracting " << needed.size() << " symbols from " << libPath << "\n";

                auto lib_objects = reader.extract_needed(needed);
                for (auto& obj : lib_objects) {
                    for (auto& sym : obj.exported_symbols)
                        defined_symbols.insert(sym.name);
                    objects_.push_back(std::move(obj));
                }

                // Update undefined list
                std::vector<std::string> still_undefined;
                for (auto& sym : undefined_symbols) {
                    if (!defined_symbols.count(sym))
                        still_undefined.push_back(sym);
                }
                undefined_symbols = std::move(still_undefined);
            }
        }
    }

    // Phase 3: Merge code and data sections, and collect DLL imports
    std::vector<uint8_t> mergedCode, mergedData;
    std::unordered_map<std::string, int> globalSymbols;

    // Collect all DLL imports from import definition members (.lib stubs)
    std::unordered_map<std::string, ImportModule> importModules; // dll -> module
    std::unordered_set<std::string> import_symbols; // symbols that are DLL imports

    for (auto& obj : objects_) {
        int codeOff = (int)mergedCode.size();
        int dataOff = (int)mergedData.size();

        // Only merge code/data from real objects, not import definitions
        if (obj.dll_imports.empty()) {
            mergedCode.insert(mergedCode.end(), obj.code.begin(), obj.code.end());
            mergedData.insert(mergedData.end(), obj.data.begin(), obj.data.end());
        }

        for (auto& sym : obj.exported_symbols) {
            int adjustedOffset = sym.offset + (sym.section == LinkerSection::Text ? codeOff : dataOff);
            if (!globalSymbols.count(sym.name))
                globalSymbols[sym.name] = adjustedOffset;
        }

        // Collect DLL import definitions
        for (auto& imp : obj.dll_imports) {
            import_symbols.insert(imp.symbol_name);
            auto& mod = importModules[imp.dll_name];
            if (mod.dll_name.empty()) mod.dll_name = imp.dll_name;
            // Avoid duplicate ordinals
            bool found = false;
            for (auto& e : mod.entries) {
                if (e.ordinal == imp.ordinal) { found = true; break; }
            }
            if (!found) {
                mod.entries.push_back({imp.symbol_name, imp.ordinal, 0});
            }
        }
    }

    // Report unresolved (excluding DLL imports and kernel fallbacks)
    std::unordered_set<std::string> truly_unresolved;
    std::unordered_set<std::string> reported;
    for (auto& sym : undefined_symbols) {
        if (globalSymbols.count(sym) || reported.count(sym) || import_symbols.count(sym)) continue;
        reported.insert(sym);
        // Also check if it's a known kernel/xam import by name (fallback)
        uint16_t ord = find_kernel_ordinal(sym, "xboxkrnl.exe");
        if (ord) {
            auto& mod = importModules["xboxkrnl.exe"];
            if (mod.dll_name.empty()) mod.dll_name = "xboxkrnl.exe";
            mod.entries.push_back({sym, ord, 0});
            import_symbols.insert(sym);
            continue;
        }
        ord = find_kernel_ordinal(sym, "xam.xex");
        if (ord) {
            auto& mod = importModules["xam.xex"];
            if (mod.dll_name.empty()) mod.dll_name = "xam.xex";
            mod.entries.push_back({sym, ord, 0});
            import_symbols.insert(sym);
            continue;
        }
        truly_unresolved.insert(sym);
    }
    if (!truly_unresolved.empty())
        warnings_.push_back(std::to_string(truly_unresolved.size()) + " unresolved external symbols");

    // Build sorted import module list
    std::vector<ImportModule> imports;
    for (auto& [dll, mod] : importModules) {
        std::sort(mod.entries.begin(), mod.entries.end(),
                  [](const ImportEntry& a, const ImportEntry& b) { return a.ordinal < b.ordinal; });
        imports.push_back(std::move(mod));
    }
    std::sort(imports.begin(), imports.end(),
              [](const ImportModule& a, const ImportModule& b) { return a.dll_name < b.dll_name; });

    if (!imports.empty()) {
        int totalImports = 0;
        for (auto& mod : imports) totalImports += (int)mod.entries.size();
        std::cout << "    " << totalImports << " DLL imports across " << imports.size() << " modules\n";
    }

    // Check for main/entry
    if (!globalSymbols.count("main"))
        warnings_.push_back("No 'main' function found");

    // Write output — PE for .exe, ELF for .elf
    std::unordered_map<std::string, int> funcOffsets;
    for (auto& [name, offset] : globalSymbols) funcOffsets[name] = offset;

    bool writePE = (outputPath.size() > 4 && outputPath.substr(outputPath.size() - 4) == ".exe");

    if (writePE) {
        write_pe(outputPath, mergedCode, mergedData, funcOffsets, "main", imports);
    } else {
        ElfWriter writer;
        writer.write(outputPath, mergedCode, mergedData, funcOffsets, "main");
    }

    std::cout << "  Linked " << objects_.size() << " objects: "
              << mergedCode.size() << " bytes code, " << mergedData.size() << " bytes data\n";
    return errors_.empty();
}

// ===================================================================
// Build import table from unresolved symbols
// Maps each symbol to xboxkrnl.exe or xam.xex by ordinal
// ===================================================================
std::vector<Linker::ImportModule> Linker::build_import_table(
    const std::unordered_set<std::string>& unresolved_symbols)
{
    std::unordered_map<std::string, ImportModule> modules;

    for (auto& sym : unresolved_symbols) {
        // Try xboxkrnl.exe first, then xam.xex
        uint16_t ord = find_kernel_ordinal(sym, "xboxkrnl.exe");
        std::string dll = "xboxkrnl.exe";
        if (!ord) {
            ord = find_kernel_ordinal(sym, "xam.xex");
            dll = "xam.xex";
        }
        if (!ord) continue; // not a known kernel import

        auto& mod = modules[dll];
        if (mod.dll_name.empty()) mod.dll_name = dll;
        mod.entries.push_back({sym, ord, 0});
    }

    // Sort entries by ordinal within each module for deterministic output
    std::vector<ImportModule> result;
    for (auto& [dll, mod] : modules) {
        std::sort(mod.entries.begin(), mod.entries.end(),
                  [](const ImportEntry& a, const ImportEntry& b) { return a.ordinal < b.ordinal; });
        result.push_back(std::move(mod));
    }
    // Put xboxkrnl.exe first
    std::sort(result.begin(), result.end(),
              [](const ImportModule& a, const ImportModule& b) { return a.dll_name < b.dll_name; });
    return result;
}

// ===================================================================
// Build .idata section with PE Import Directory Table
//
// Layout within .idata:
//   [Import Directory Table]  — one IMAGE_IMPORT_DESCRIPTOR per DLL + null terminator
//   [Import Lookup Tables]    — one ILT per DLL (array of 32-bit entries, null-terminated)
//   [Import Address Tables]   — one IAT per DLL (identical to ILT initially; loader overwrites)
//   [DLL name strings]        — null-terminated ASCII strings
//
// Xbox 360 uses ordinal imports: bit 31 set + ordinal in low 16 bits
// ===================================================================
Linker::IdataInfo Linker::build_idata_section(
    std::vector<ImportModule>& imports, uint32_t section_rva)
{
    IdataInfo info{};
    if (imports.empty()) return info;

    // Calculate sizes
    uint32_t numDlls = (uint32_t)imports.size();
    // IDT: 20 bytes per descriptor + 20-byte null terminator
    uint32_t idtSize = (numDlls + 1) * 20;

    // Count total import entries (for ILT + IAT sizing)
    uint32_t totalEntries = 0;
    for (auto& mod : imports)
        totalEntries += (uint32_t)mod.entries.size() + 1; // +1 for null terminator per DLL

    // ILT: 4 bytes per entry (PE32)
    uint32_t iltSize = totalEntries * 4;
    // IAT: identical layout
    uint32_t iatSize = totalEntries * 4;

    // DLL name strings: sum of (strlen + 1) for each, then aligned
    uint32_t namesSize = 0;
    for (auto& mod : imports)
        namesSize += (uint32_t)mod.dll_name.size() + 1;
    namesSize = (namesSize + 3) & ~3; // align to 4

    // Layout offsets within section
    uint32_t iltOffset = idtSize;
    uint32_t iatOffset = iltOffset + iltSize;
    uint32_t namesOffset = iatOffset + iatSize;
    uint32_t totalSize = namesOffset + namesSize;

    std::vector<uint8_t> idata(totalSize, 0);

    // Pass 1: Write DLL name strings and record their RVAs
    std::vector<uint32_t> nameRVAs;
    uint32_t namePos = namesOffset;
    for (auto& mod : imports) {
        nameRVAs.push_back(section_rva + namePos);
        memcpy(idata.data() + namePos, mod.dll_name.c_str(), mod.dll_name.size());
        namePos += (uint32_t)mod.dll_name.size() + 1; // null terminator already zeroed
    }

    // Pass 2: Write ILT and IAT (ordinal entries)
    uint32_t iltPos = iltOffset;
    uint32_t iatPos = iatOffset;
    std::vector<uint32_t> iltRVAs, iatRVAs;

    for (size_t d = 0; d < imports.size(); d++) {
        iltRVAs.push_back(section_rva + iltPos);
        iatRVAs.push_back(section_rva + iatPos);

        for (auto& entry : imports[d].entries) {
            // Ordinal import: bit 31 set | ordinal number
            uint32_t ordVal = 0x80000000 | entry.ordinal;

            // Write to ILT
            idata[iltPos+0] = ordVal & 0xFF;
            idata[iltPos+1] = (ordVal >> 8) & 0xFF;
            idata[iltPos+2] = (ordVal >> 16) & 0xFF;
            idata[iltPos+3] = (ordVal >> 24) & 0xFF;

            // Write to IAT (identical initially)
            idata[iatPos+0] = ordVal & 0xFF;
            idata[iatPos+1] = (ordVal >> 8) & 0xFF;
            idata[iatPos+2] = (ordVal >> 16) & 0xFF;
            idata[iatPos+3] = (ordVal >> 24) & 0xFF;

            // Record the IAT RVA for this entry (code references this address)
            entry.iat_rva = section_rva + iatPos;

            iltPos += 4;
            iatPos += 4;
        }
        // Null terminator for this DLL's ILT/IAT
        iltPos += 4; // already zeroed
        iatPos += 4;
    }

    // Pass 3: Write Import Directory Table entries
    for (size_t d = 0; d < imports.size(); d++) {
        uint32_t off = (uint32_t)(d * 20);
        // OriginalFirstThunk (ILT RVA)
        idata[off+0] = iltRVAs[d] & 0xFF;
        idata[off+1] = (iltRVAs[d] >> 8) & 0xFF;
        idata[off+2] = (iltRVAs[d] >> 16) & 0xFF;
        idata[off+3] = (iltRVAs[d] >> 24) & 0xFF;
        // TimeDateStamp
        // ForwarderChain
        // (both zero — already zeroed)
        // Name RVA at offset 12
        idata[off+12] = nameRVAs[d] & 0xFF;
        idata[off+13] = (nameRVAs[d] >> 8) & 0xFF;
        idata[off+14] = (nameRVAs[d] >> 16) & 0xFF;
        idata[off+15] = (nameRVAs[d] >> 24) & 0xFF;
        // FirstThunk (IAT RVA) at offset 16
        idata[off+16] = iatRVAs[d] & 0xFF;
        idata[off+17] = (iatRVAs[d] >> 8) & 0xFF;
        idata[off+18] = (iatRVAs[d] >> 16) & 0xFF;
        idata[off+19] = (iatRVAs[d] >> 24) & 0xFF;
    }
    // Null terminator entry (already zeroed by vector init)

    info.data = std::move(idata);
    info.idt_rva_offset = 0;
    info.idt_size = idtSize;
    info.iat_rva_offset = iatOffset;
    info.iat_size = iatSize;
    return info;
}

// ===================================================================
// Write PE executable for Xbox 360 (consumed by imagexex.exe)
// Sections: .text, .data, .idata (if imports exist)
// ===================================================================
void Linker::write_pe(const std::string& path, const std::vector<uint8_t>& code,
                       const std::vector<uint8_t>& data,
                       const std::unordered_map<std::string, int>& funcOffsets,
                       const std::string& entryName,
                       std::vector<ImportModule>& imports) {
    auto wle16 = [](std::vector<uint8_t>& b, uint16_t v) { b.push_back(v&0xFF); b.push_back((v>>8)&0xFF); };
    auto wle32 = [](std::vector<uint8_t>& b, uint32_t v) { b.push_back(v&0xFF);b.push_back((v>>8)&0xFF);b.push_back((v>>16)&0xFF);b.push_back((v>>24)&0xFF); };
    auto padto = [](std::vector<uint8_t>& b, size_t t) { while(b.size()<t) b.push_back(0); };
    auto alignv = [](uint32_t v, uint32_t a) -> uint32_t { return (v+a-1)&~(a-1); };

    const uint32_t SEC_ALIGN = 0x10000;   // Xbox 360 uses 64KB section alignment
    const uint32_t FILE_ALIGN = 0x200;     // 512 byte file alignment
    const uint32_t IMAGE_BASE = BASE_ADDRESS; // 0x82000000

    bool hasImports = !imports.empty();
    uint32_t numSections = hasImports ? 3 : 2; // .text, .data, [.idata]

    uint32_t textSize = alignv((uint32_t)code.size(), FILE_ALIGN);
    uint32_t dataSize = alignv((uint32_t)data.size(), FILE_ALIGN);
    uint32_t headersSize = alignv(0x400, FILE_ALIGN); // headers take first page

    uint32_t textRVA = SEC_ALIGN;  // .text at 0x10000
    uint32_t dataRVA = textRVA + alignv((uint32_t)code.size(), SEC_ALIGN);

    // .idata section placement
    uint32_t idataRVA = dataRVA + alignv((uint32_t)data.size(), SEC_ALIGN);

    // Build .idata section content (needs section RVA for internal pointers)
    IdataInfo idataInfo;
    uint32_t idataRawSize = 0;
    if (hasImports) {
        idataInfo = build_idata_section(imports, idataRVA);
        idataRawSize = alignv((uint32_t)idataInfo.data.size(), FILE_ALIGN);
    }

    uint32_t imageSize = idataRVA;
    if (hasImports)
        imageSize = idataRVA + alignv((uint32_t)idataInfo.data.size(), SEC_ALIGN);
    else
        imageSize = dataRVA + alignv((uint32_t)data.size(), SEC_ALIGN);

    uint32_t entryRVA = textRVA;
    auto it = funcOffsets.find(entryName);
    if (it != funcOffsets.end()) entryRVA = textRVA + it->second;

    std::vector<uint8_t> pe;

    // DOS Header
    wle16(pe, 0x5A4D); // MZ
    padto(pe, 60);
    wle32(pe, 0xE8);   // e_lfanew
    padto(pe, 0xE8);

    // PE Signature
    wle32(pe, 0x00004550); // PE\0\0

    // COFF Header
    wle16(pe, 0x01F2);        // Machine: PPC-BE
    wle16(pe, numSections);   // Number of sections
    wle32(pe, 0);              // TimeDateStamp
    wle32(pe, 0);              // PointerToSymbolTable
    wle32(pe, 0);              // NumberOfSymbols
    wle16(pe, 224);            // SizeOfOptionalHeader
    wle16(pe, 0x0102);         // Characteristics: EXECUTABLE | 32BIT

    // Optional Header (PE32)
    wle16(pe, 0x010B);    // Magic: PE32
    wle16(pe, 0x0E00);    // Linker version
    wle32(pe, textSize);  // SizeOfCode
    wle32(pe, dataSize + idataRawSize); // SizeOfInitializedData
    wle32(pe, 0);         // SizeOfUninitializedData
    wle32(pe, entryRVA);  // AddressOfEntryPoint
    wle32(pe, textRVA);   // BaseOfCode
    wle32(pe, dataRVA);   // BaseOfData
    wle32(pe, IMAGE_BASE); // ImageBase
    wle32(pe, SEC_ALIGN);  // SectionAlignment
    wle32(pe, FILE_ALIGN); // FileAlignment
    wle16(pe, 4); wle16(pe, 0); // OS version
    wle16(pe, 0); wle16(pe, 0); // Image version
    wle16(pe, 4); wle16(pe, 0); // Subsystem version
    wle32(pe, 0);          // Win32VersionValue
    wle32(pe, imageSize);  // SizeOfImage
    wle32(pe, headersSize); // SizeOfHeaders
    wle32(pe, 0);          // CheckSum
    wle16(pe, 14);         // Subsystem: XBOX
    wle16(pe, 0);          // DllCharacteristics
    wle32(pe, 0x40000);    // SizeOfStackReserve (256KB)
    wle32(pe, 0x1000);     // SizeOfStackCommit
    wle32(pe, 0x100000);   // SizeOfHeapReserve
    wle32(pe, 0x1000);     // SizeOfHeapCommit
    wle32(pe, 0);          // LoaderFlags
    wle32(pe, 16);         // NumberOfRvaAndSizes

    // Data directories (16 entries)
    // [0] Export — zero
    wle32(pe, 0); wle32(pe, 0);
    // [1] Import Directory Table
    if (hasImports) {
        wle32(pe, idataRVA + idataInfo.idt_rva_offset); // RVA of IDT
        wle32(pe, idataInfo.idt_size);                    // Size
    } else {
        wle32(pe, 0); wle32(pe, 0);
    }
    // [2]-[11] — zero
    for (int i = 2; i < 12; i++) { wle32(pe, 0); wle32(pe, 0); }
    // [12] Import Address Table (IAT)
    if (hasImports) {
        wle32(pe, idataRVA + idataInfo.iat_rva_offset); // RVA of IAT
        wle32(pe, idataInfo.iat_size);                    // Size
    } else {
        wle32(pe, 0); wle32(pe, 0);
    }
    // [13]-[15] — zero
    for (int i = 13; i < 16; i++) { wle32(pe, 0); wle32(pe, 0); }

    // --- Section headers ---

    // .text section header
    const char* textName = ".text\0\0\0";
    for (int i = 0; i < 8; i++) pe.push_back((uint8_t)textName[i]);
    wle32(pe, (uint32_t)code.size()); // VirtualSize
    wle32(pe, textRVA);               // VirtualAddress
    wle32(pe, textSize);              // SizeOfRawData
    wle32(pe, headersSize);           // PointerToRawData
    wle32(pe, 0); wle32(pe, 0);      // Relocations, Linenumbers
    wle16(pe, 0); wle16(pe, 0);
    wle32(pe, 0x60000020);            // CODE|EXECUTE|READ

    // .data section header
    const char* dataName = ".data\0\0\0";
    for (int i = 0; i < 8; i++) pe.push_back((uint8_t)dataName[i]);
    wle32(pe, (uint32_t)data.size());
    wle32(pe, dataRVA);
    wle32(pe, dataSize);
    wle32(pe, headersSize + textSize);
    wle32(pe, 0); wle32(pe, 0);
    wle16(pe, 0); wle16(pe, 0);
    wle32(pe, 0xC0000040);            // INITIALIZED_DATA|READ|WRITE

    // .idata section header (if imports exist)
    if (hasImports) {
        const char* idataName = ".idata\0\0";
        for (int i = 0; i < 8; i++) pe.push_back((uint8_t)idataName[i]);
        wle32(pe, (uint32_t)idataInfo.data.size()); // VirtualSize
        wle32(pe, idataRVA);                          // VirtualAddress
        wle32(pe, idataRawSize);                      // SizeOfRawData
        wle32(pe, headersSize + textSize + dataSize); // PointerToRawData
        wle32(pe, 0); wle32(pe, 0);
        wle16(pe, 0); wle16(pe, 0);
        wle32(pe, 0xC0000040);        // INITIALIZED_DATA|READ|WRITE
    }

    // Pad headers
    padto(pe, headersSize);

    // .text data
    pe.insert(pe.end(), code.begin(), code.end());
    padto(pe, headersSize + textSize);

    // .data data
    pe.insert(pe.end(), data.begin(), data.end());
    padto(pe, headersSize + textSize + dataSize);

    // .idata data
    if (hasImports) {
        pe.insert(pe.end(), idataInfo.data.begin(), idataInfo.data.end());
        padto(pe, headersSize + textSize + dataSize + idataRawSize);

        std::cout << "    .idata section: " << idataInfo.data.size() << " bytes, "
                  << imports.size() << " import DLLs\n";
        for (auto& mod : imports)
            std::cout << "      " << mod.dll_name << ": " << mod.entries.size() << " imports\n";
    }

    // Write
    std::ofstream out(path, std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(pe.data()), pe.size());

    std::cout << "    PE image: " << pe.size() << " bytes (" << numSections << " sections)\n";
}

} // namespace nexia