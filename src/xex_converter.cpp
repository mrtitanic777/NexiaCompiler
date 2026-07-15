// NexiaCompiler v2.0 — XEX Converter
// Produces proper XEX2 with embedded PE image for Xbox 360 (RGH/devkit)
// Format: XEX2 Header → Optional Headers → Security Info → Basefile (PE Image)

#include "xex_converter.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <numeric>

namespace nexia {

uint32_t XexConverter::read_be32(const uint8_t* d, int o) { return ((uint32_t)d[o]<<24)|((uint32_t)d[o+1]<<16)|((uint32_t)d[o+2]<<8)|d[o+3]; }
uint16_t XexConverter::read_be16(const uint8_t* d, int o) { return ((uint16_t)d[o]<<8)|d[o+1]; }
void XexConverter::write_be32(uint8_t* d, int o, uint32_t v) { d[o]=(v>>24)&0xFF;d[o+1]=(v>>16)&0xFF;d[o+2]=(v>>8)&0xFF;d[o+3]=v&0xFF; }
void XexConverter::write_be32(std::vector<uint8_t>& b, uint32_t v) { b.push_back((v>>24)&0xFF);b.push_back((v>>16)&0xFF);b.push_back((v>>8)&0xFF);b.push_back(v&0xFF); }
int XexConverter::align(int v, int a) { return (v+a-1)&~(a-1); }

// Helper: write little-endian values (for PE image which is LE even on Xbox 360)
static void write_le16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v&0xFF); b.push_back((v>>8)&0xFF); }
static void write_le32(std::vector<uint8_t>& b, uint32_t v) { b.push_back(v&0xFF);b.push_back((v>>8)&0xFF);b.push_back((v>>16)&0xFF);b.push_back((v>>24)&0xFF); }
static void pad_to(std::vector<uint8_t>& b, size_t target) { while (b.size() < target) b.push_back(0); }

// Build a minimal PE image from code + data
std::vector<uint8_t> XexConverter::build_pe_image(const std::vector<uint8_t>& code,
                                                    const std::vector<uint8_t>& data,
                                                    uint32_t entryPoint) {
    std::vector<uint8_t> pe;
    const uint32_t sectionAlignment = 0x1000;  // 4KB
    const uint32_t fileAlignment = 0x200;       // 512 bytes

    // DOS Header (64 bytes minimum)
    write_le16(pe, 0x5A4D);            // e_magic: "MZ"
    pad_to(pe, 60);
    write_le32(pe, 0x80);              // e_lfanew: PE header at offset 0x80
    pad_to(pe, 0x80);

    // PE Signature
    write_le32(pe, 0x00004550);        // "PE\0\0"

    // COFF Header (20 bytes)
    write_le16(pe, 0x01F2);            // Machine: IMAGE_FILE_MACHINE_POWERPCBE
    write_le16(pe, 2);                 // NumberOfSections: .text + .data
    write_le32(pe, 0);                 // TimeDateStamp
    write_le32(pe, 0);                 // PointerToSymbolTable
    write_le32(pe, 0);                 // NumberOfSymbols
    write_le16(pe, 224);               // SizeOfOptionalHeader (PE32)
    write_le16(pe, 0x0102);            // Characteristics: EXECUTABLE_IMAGE | 32BIT_MACHINE

    // Optional Header (PE32 — 224 bytes)
    write_le16(pe, 0x010B);            // Magic: PE32
    write_le16(pe, 0x0E00);            // Linker version (14.0)

    uint32_t textSize = align((int)code.size(), fileAlignment);
    uint32_t dataSize = align((int)data.size(), fileAlignment);

    write_le32(pe, textSize);          // SizeOfCode
    write_le32(pe, dataSize);          // SizeOfInitializedData
    write_le32(pe, 0);                 // SizeOfUninitializedData

    // Sections start after headers, aligned to sectionAlignment
    uint32_t headersSize = align(0x80 + 4 + 20 + 224 + 2 * 40, fileAlignment);
    uint32_t textRVA = sectionAlignment; // .text at 0x1000
    uint32_t dataRVA = textRVA + align((int)code.size(), sectionAlignment);

    write_le32(pe, textRVA + entryPoint); // AddressOfEntryPoint
    write_le32(pe, textRVA);           // BaseOfCode
    write_le32(pe, dataRVA);           // BaseOfData

    write_le32(pe, BASE_ADDRESS);      // ImageBase: 0x82000000
    write_le32(pe, sectionAlignment);  // SectionAlignment
    write_le32(pe, fileAlignment);     // FileAlignment
    write_le16(pe, 4); write_le16(pe, 0); // OS version (4.0)
    write_le16(pe, 0); write_le16(pe, 0); // Image version
    write_le16(pe, 4); write_le16(pe, 0); // Subsystem version
    write_le32(pe, 0);                 // Win32VersionValue

    uint32_t imageSize = dataRVA + align((int)data.size(), sectionAlignment);
    write_le32(pe, imageSize);         // SizeOfImage
    write_le32(pe, headersSize);       // SizeOfHeaders
    write_le32(pe, 0);                 // CheckSum (filled later or ignored on RGH)
    write_le16(pe, 14);                // Subsystem: XBOX
    write_le16(pe, 0);                 // DllCharacteristics
    write_le32(pe, 0x40000);           // SizeOfStackReserve (256KB — required for Xbox 360)
    write_le32(pe, 0x1000);            // SizeOfStackCommit
    write_le32(pe, 0x100000);          // SizeOfHeapReserve
    write_le32(pe, 0x1000);            // SizeOfHeapCommit
    write_le32(pe, 0);                 // LoaderFlags
    write_le32(pe, 16);                // NumberOfRvaAndSizes

    // Data directories (16 entries, all zero for now)
    for (int i = 0; i < 16; i++) {
        write_le32(pe, 0); // RVA
        write_le32(pe, 0); // Size
    }

    // Section headers
    // .text section
    {
        const char* name = ".text\0\0\0";
        for (int i = 0; i < 8; i++) pe.push_back((uint8_t)name[i]);
        write_le32(pe, (uint32_t)code.size()); // VirtualSize
        write_le32(pe, textRVA);               // VirtualAddress
        write_le32(pe, textSize);              // SizeOfRawData
        write_le32(pe, headersSize);           // PointerToRawData
        write_le32(pe, 0);                     // PointerToRelocations
        write_le32(pe, 0);                     // PointerToLinenumbers
        write_le16(pe, 0);                     // NumberOfRelocations
        write_le16(pe, 0);                     // NumberOfLinenumbers
        write_le32(pe, 0x60000020);            // Characteristics: CODE|EXECUTE|READ
    }

    // .data section
    {
        const char* name = ".data\0\0\0";
        for (int i = 0; i < 8; i++) pe.push_back((uint8_t)name[i]);
        write_le32(pe, (uint32_t)data.size()); // VirtualSize
        write_le32(pe, dataRVA);               // VirtualAddress
        write_le32(pe, dataSize);              // SizeOfRawData
        write_le32(pe, headersSize + textSize); // PointerToRawData
        write_le32(pe, 0); write_le32(pe, 0);
        write_le16(pe, 0); write_le16(pe, 0);
        write_le32(pe, 0xC0000040);            // Characteristics: INITIALIZED_DATA|READ|WRITE
    }

    // Pad headers to alignment
    pad_to(pe, headersSize);

    // .text section data
    pe.insert(pe.end(), code.begin(), code.end());
    pad_to(pe, headersSize + textSize);

    // .data section data
    pe.insert(pe.end(), data.begin(), data.end());
    pad_to(pe, headersSize + textSize + dataSize);

    return pe;
}

std::vector<uint8_t> XexConverter::build_execution_info(const XexOptions& options) {
    std::vector<uint8_t> info;
    write_be32(info, options.media_id);      // Media ID
    write_be32(info, 0x00020000);            // Version (2.0)
    write_be32(info, 0x00020000);            // Base version
    write_be32(info, options.title_id);      // Title ID
    write_be32(info, 0x000000FF);            // Platform (Xbox 360)
    write_be32(info, 0x00000000);            // Executable type (title)
    write_be32(info, options.disc_number);   // Disc number
    write_be32(info, 0);                     // Savegame ID
    return info;
}

std::vector<uint8_t> XexConverter::build_xex(const std::vector<uint8_t>& imageData,
                                               uint32_t entryPoint, const XexOptions& options) {
    // Build the PE image
    std::vector<uint8_t> peImage = build_pe_image(imageData,
        std::vector<uint8_t>(), // data section already merged
        entryPoint);

    // Actually, imageData contains both code and data already merged from the ELF.
    // Rebuild properly: use imageData as the combined .text content
    peImage = build_pe_image(imageData, std::vector<uint8_t>(), entryPoint);

    // Page-align the PE image (64KB pages on Xbox 360)
    int peAligned = align((int)peImage.size(), PAGE_SIZE);
    peImage.resize(peAligned, 0);

    // Build execution info
    auto execInfo = build_execution_info(options);

    // XEX2 structure:
    // [XEX Header 24 bytes]
    // [Optional Header Entries - 8 bytes each]
    // [Optional Header Data (execution info, etc.)]
    // [Security Info (placeholder for RGH)]
    // [Page descriptors]
    // [PE Image (basefile)]

    // Count optional headers
    int numOptHeaders = 5; // entry point, base addr, exec info, system flags, original PE name

    // Build optional header data area
    std::vector<uint8_t> optData;

    // Execution info (variable size — pointed to by opt header)
    optData.insert(optData.end(), execInfo.begin(), execInfo.end());

    // Calculate sizes
    uint32_t xexHeaderSize = 24;
    uint32_t optHeadersSize = numOptHeaders * 8;
    uint32_t afterOptHeaders = xexHeaderSize + optHeadersSize;
    uint32_t optDataStart = afterOptHeaders;
    uint32_t afterOptData = optDataStart + (uint32_t)optData.size();

    // Security info (simplified for RGH — 296 bytes of zeros is accepted)
    uint32_t securityInfoSize = 296;
    uint32_t securityInfoOffset = align((int)afterOptData, 4);
    uint32_t afterSecurity = securityInfoOffset + securityInfoSize;

    // Page descriptors
    uint32_t numPages = (uint32_t)(peAligned / PAGE_SIZE);
    uint32_t pageDescStart = align((int)afterSecurity, 4);
    uint32_t pageDescSize = numPages * 24; // each descriptor is 24 bytes

    // PE image offset (basefile)
    uint32_t peOffset = align((int)(pageDescStart + pageDescSize), PAGE_SIZE);

    // Now build the XEX
    std::vector<uint8_t> xex;

    // XEX2 Header (24 bytes)
    write_be32(xex, XEX2_MAGIC);             // "XEX2"
    write_be32(xex, 0x00000001);             // Module flags (title module)
    write_be32(xex, peOffset);               // Data offset (PE image start)
    write_be32(xex, 0);                      // Reserved
    write_be32(xex, securityInfoOffset);     // Security info offset
    write_be32(xex, numOptHeaders);          // Optional header count

    // Optional header entries
    // 1. Entry point (immediate value — key encodes that value follows directly)
    write_be32(xex, 0x000101FF);             // XEX_HEADER_ENTRY_POINT
    write_be32(xex, BASE_ADDRESS + 0x1000 + entryPoint); // entry = image base + .text RVA + offset

    // 2. Image base address
    write_be32(xex, 0x000100FF);             // XEX_HEADER_IMAGE_BASE_ADDRESS
    write_be32(xex, BASE_ADDRESS);

    // 3. Execution info (pointer to data)
    write_be32(xex, 0x00040006);             // XEX_HEADER_EXECUTION_INFO
    write_be32(xex, optDataStart);           // offset to execution info data

    // 4. System flags
    write_be32(xex, 0x000300FF);             // XEX_HEADER_SYSTEM_FLAGS
    write_be32(xex, 0x00000000);             // No special flags

    // 5. Original PE name
    write_be32(xex, 0x000183FF);             // XEX_HEADER_ORIGINAL_PE_NAME
    write_be32(xex, 0);                      // Will not be used

    // Optional header data
    pad_to(xex, optDataStart);
    xex.insert(xex.end(), optData.begin(), optData.end());

    // Security info (zeroed for RGH — the kernel on RGH doesn't validate signatures)
    pad_to(xex, securityInfoOffset);
    // Security info structure (296 bytes):
    // - Header hash (20 bytes) — zero
    // - Image size (4 bytes)
    // - Image info (various)
    uint32_t secStart = (uint32_t)xex.size();
    for (uint32_t i = 0; i < 20; i++) xex.push_back(0); // header hash
    write_be32(xex, (uint32_t)peImage.size());           // image size
    // Image info:
    for (uint32_t i = 0; i < 20; i++) xex.push_back(0); // image hash
    write_be32(xex, 0);                                   // load address
    for (uint32_t i = 0; i < 20; i++) xex.push_back(0); // import table hash
    write_be32(xex, 0);                                   // import table count
    // Media ID, region, etc
    write_be32(xex, options.media_id);
    // AES key (16 bytes zero)
    for (int i = 0; i < 16; i++) xex.push_back(0);
    // Page descriptor count
    write_be32(xex, numPages);
    // Pad rest of security info
    pad_to(xex, secStart + securityInfoSize);

    // Page descriptors
    pad_to(xex, pageDescStart);
    for (uint32_t i = 0; i < numPages; i++) {
        // Each page descriptor: hash (20 bytes) + info (4 bytes)
        for (int j = 0; j < 20; j++) xex.push_back(0); // SHA1 hash (zeroed for RGH)
        write_be32(xex, 0x00000001);                     // Page info: uncompressed
    }

    // PE image (basefile)
    pad_to(xex, peOffset);
    xex.insert(xex.end(), peImage.begin(), peImage.end());

    return xex;
}

void XexConverter::convert(const std::string& elfPath, const std::string& xexPath,
                            const XexOptions& options) {
    std::ifstream f(elfPath, std::ios::binary);
    if (!f) { std::cerr << "Cannot open: " << elfPath << "\n"; return; }
    std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Detect PE vs ELF by magic
    if (fileData.size() >= 2 && fileData[0] == 'M' && fileData[1] == 'Z') {
        // This is a PE file — use convert_pe path
        f.close();
        convert_pe(elfPath, xexPath, options);
        return;
    }

    auto seg = extract_loadable_segments(fileData);
    if (seg.data.empty()) { std::cerr << "No loadable segments in ELF\n"; return; }
    uint32_t entry = read_be32(fileData.data(), 24);
    uint32_t entryOffset = entry - seg.base_addr;
    auto xex = build_xex(seg.data, entryOffset, options);
    std::ofstream out(xexPath, std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(xex.data()), xex.size());
    std::cout << "  XEX written: " << xex.size() << " bytes (" << xex.size()/1024 << "KB)\n";
}

void XexConverter::convert_pe(const std::string& pePath, const std::string& xexPath,
                               const XexOptions& options) {
    std::ifstream f(pePath, std::ios::binary);
    if (!f) { std::cerr << "Cannot open PE: " << pePath << "\n"; return; }
    std::vector<uint8_t> peImage((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (peImage.size() < 2 || peImage[0] != 'M' || peImage[1] != 'Z') {
        std::cerr << "Not a valid PE file: " << pePath << "\n";
        return;
    }

    auto xex = build_xex_from_pe(peImage, options);
    std::ofstream out(xexPath, std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(xex.data()), xex.size());
    std::cout << "  XEX written: " << xex.size() << " bytes (" << xex.size()/1024 << "KB)\n";
}

// Helper to read little-endian from a byte buffer
static uint32_t read_le32(const uint8_t* d, int o) {
    return d[o] | ((uint32_t)d[o+1]<<8) | ((uint32_t)d[o+2]<<16) | ((uint32_t)d[o+3]<<24);
}

std::vector<uint8_t> XexConverter::build_xex_from_pe(const std::vector<uint8_t>& peImage,
                                                       const XexOptions& options) {
    // Parse the PE to extract entry point RVA
    uint32_t peHeaderOffset = read_le32(peImage.data(), 60); // e_lfanew
    uint32_t entryRVA = read_le32(peImage.data(), peHeaderOffset + 4 + 20 + 16); // AddressOfEntryPoint
    uint32_t imageBase = read_le32(peImage.data(), peHeaderOffset + 4 + 20 + 28); // ImageBase
    uint32_t entryPoint = imageBase + entryRVA;

    // Page-align the PE image (64KB pages on Xbox 360)
    std::vector<uint8_t> peAligned = peImage;
    int alignedSize = align((int)peAligned.size(), PAGE_SIZE);
    peAligned.resize(alignedSize, 0);

    // Build execution info
    auto execInfo = build_execution_info(options);

    // Count optional headers
    int numOptHeaders = 5;

    // Build optional header data area
    std::vector<uint8_t> optData;
    optData.insert(optData.end(), execInfo.begin(), execInfo.end());

    // Calculate sizes
    uint32_t xexHeaderSize = 24;
    uint32_t optHeadersSize = numOptHeaders * 8;
    uint32_t afterOptHeaders = xexHeaderSize + optHeadersSize;
    uint32_t optDataStart = afterOptHeaders;
    uint32_t afterOptData = optDataStart + (uint32_t)optData.size();

    uint32_t securityInfoSize = 296;
    uint32_t securityInfoOffset = align((int)afterOptData, 4);
    uint32_t afterSecurity = securityInfoOffset + securityInfoSize;

    uint32_t numPages = (uint32_t)(alignedSize / PAGE_SIZE);
    uint32_t pageDescStart = align((int)afterSecurity, 4);
    uint32_t pageDescSize = numPages * 24;

    uint32_t peOffset = align((int)(pageDescStart + pageDescSize), PAGE_SIZE);

    // Build XEX
    std::vector<uint8_t> xex;

    // XEX2 Header
    write_be32(xex, XEX2_MAGIC);
    write_be32(xex, 0x00000001);             // Module flags (title)
    write_be32(xex, peOffset);
    write_be32(xex, 0);                      // Reserved
    write_be32(xex, securityInfoOffset);
    write_be32(xex, numOptHeaders);

    // Optional headers
    write_be32(xex, 0x000101FF);             // ENTRY_POINT
    write_be32(xex, entryPoint);

    write_be32(xex, 0x000100FF);             // IMAGE_BASE_ADDRESS
    write_be32(xex, imageBase);

    write_be32(xex, 0x00040006);             // EXECUTION_INFO
    write_be32(xex, optDataStart);

    write_be32(xex, 0x000300FF);             // SYSTEM_FLAGS
    write_be32(xex, 0x00000000);

    write_be32(xex, 0x000183FF);             // ORIGINAL_PE_NAME
    write_be32(xex, 0);

    // Optional header data
    pad_to(xex, optDataStart);
    xex.insert(xex.end(), optData.begin(), optData.end());

    // Security info
    pad_to(xex, securityInfoOffset);
    uint32_t secStart = (uint32_t)xex.size();
    for (uint32_t i = 0; i < 20; i++) xex.push_back(0); // header hash
    write_be32(xex, (uint32_t)peAligned.size());          // image size
    for (uint32_t i = 0; i < 20; i++) xex.push_back(0); // image hash
    write_be32(xex, 0);                                   // load address
    for (uint32_t i = 0; i < 20; i++) xex.push_back(0); // import table hash
    write_be32(xex, 0);                                   // import table count
    write_be32(xex, options.media_id);
    for (int i = 0; i < 16; i++) xex.push_back(0);       // AES key (zeroed for RGH)
    write_be32(xex, numPages);
    pad_to(xex, secStart + securityInfoSize);

    // Page descriptors
    pad_to(xex, pageDescStart);
    for (uint32_t i = 0; i < numPages; i++) {
        for (int j = 0; j < 20; j++) xex.push_back(0);
        write_be32(xex, 0x00000001);                      // uncompressed
    }

    // PE image (basefile)
    pad_to(xex, peOffset);
    xex.insert(xex.end(), peAligned.begin(), peAligned.end());

    return xex;
}

void XexConverter::convert_direct(const std::vector<uint8_t>& code, const std::vector<uint8_t>& data,
                                   uint32_t entryPoint, const std::string& xexPath, const XexOptions& options) {
    std::vector<uint8_t> image = code;
    int padded = align((int)image.size(), 16);
    image.resize(padded, 0);
    image.insert(image.end(), data.begin(), data.end());
    auto xex = build_xex(image, entryPoint, options);
    std::ofstream out(xexPath, std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(xex.data()), xex.size());
}

XexConverter::SegmentInfo XexConverter::extract_loadable_segments(const std::vector<uint8_t>& elf) {
    if (elf.size() < 52) return {{}, 0};
    uint32_t phoff = read_be32(elf.data(), 28);
    uint16_t phnum = read_be16(elf.data(), 44);
    uint16_t phentsize = read_be16(elf.data(), 42);
    std::vector<uint8_t> combined;
    uint32_t baseAddr = 0xFFFFFFFF;
    for (int i = 0; i < phnum; i++) {
        int off = (int)phoff + i * phentsize;
        uint32_t type = read_be32(elf.data(), off);
        if (type != 1) continue;
        uint32_t foff = read_be32(elf.data(), off + 4);
        uint32_t vaddr = read_be32(elf.data(), off + 8);
        uint32_t filesz = read_be32(elf.data(), off + 16);
        uint32_t memsz = read_be32(elf.data(), off + 20);
        if (vaddr < baseAddr) baseAddr = vaddr;
        uint32_t segEnd = (vaddr - baseAddr) + memsz;
        if (combined.size() < segEnd) combined.resize(segEnd, 0);
        if (foff + filesz <= elf.size())
            memcpy(combined.data() + (vaddr - baseAddr), elf.data() + foff, filesz);
    }
    return {combined, baseAddr};
}

} // namespace nexia
