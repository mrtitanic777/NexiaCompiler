#pragma once
// NexiaCompiler v2.0 — XEX Converter
// Ported from XexConverter.cs
// Converts ELF executables to Xbox 360 XEX format.

#include <string>
#include <vector>
#include <cstdint>

namespace nexia {

struct XexOptions {
    uint32_t title_id    = 0xFFFFFFFF;
    uint32_t media_id    = 0;
    uint32_t disc_number = 0x00010001;
    std::string title_name = "NexiaCompiler Output";
    bool encrypt  = false;
    bool compress = false;
};

class XexConverter {
public:
    void convert(const std::string& elfPath, const std::string& xexPath,
                 const XexOptions& options = {});

    // Convert an existing PE executable to XEX (wraps PE as basefile)
    void convert_pe(const std::string& pePath, const std::string& xexPath,
                    const XexOptions& options = {});

    void convert_direct(const std::vector<uint8_t>& code, const std::vector<uint8_t>& data,
                        uint32_t entryPoint, const std::string& xexPath,
                        const XexOptions& options = {});

private:
    static constexpr uint32_t XEX2_MAGIC   = 0x58455832;
    static constexpr uint32_t BASE_ADDRESS = 0x82000000;
    static constexpr int PAGE_SIZE = 0x10000;

    std::vector<uint8_t> build_xex(const std::vector<uint8_t>& imageData,
                                    uint32_t entryPoint, const XexOptions& options);
    std::vector<uint8_t> build_xex_from_pe(const std::vector<uint8_t>& peImage,
                                            const XexOptions& options);
    std::vector<uint8_t> build_pe_image(const std::vector<uint8_t>& code,
                                         const std::vector<uint8_t>& data,
                                         uint32_t entryPoint);
    std::vector<uint8_t> build_execution_info(const XexOptions& options);

    struct SegmentInfo {
        std::vector<uint8_t> data;
        uint32_t base_addr;
    };
    SegmentInfo extract_loadable_segments(const std::vector<uint8_t>& elf);

    static uint32_t read_be32(const uint8_t* data, int offset);
    static uint16_t read_be16(const uint8_t* data, int offset);
    static void write_be32(uint8_t* data, int offset, uint32_t value);
    static void write_be32(std::vector<uint8_t>& buf, uint32_t value);
    static int align(int value, int alignment);
};

} // namespace nexia
