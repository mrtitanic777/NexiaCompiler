#pragma once
// NexiaCompiler v2.0 — SDK Header Generator
// Ported from SdkHeaders.cs

#include <string>
#include <vector>

namespace nexia {

class SdkHeaderGenerator {
public:
    explicit SdkHeaderGenerator(const std::string& outputDir);

    void generate_all();
    static std::vector<std::string> get_header_names();

private:
    std::string output_dir_;

    void write_header(const std::string& name, const std::string& content);

    std::string generate_xtypes();
    std::string generate_xtl();
    std::string generate_xbox_math();
    std::string generate_d3d9();
    std::string generate_xaudio2();
    std::string generate_xinput();
    std::string generate_xui();
    std::string generate_xam();
};

} // namespace nexia
