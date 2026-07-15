// NexiaCompiler v2.0 — CLI entry point
// Ported from Program.cs
// Full pipeline: Preprocess -> Lex -> Parse -> Analyze -> Optimize -> CodeGen -> Link

#include <memory>
#include "compiler.h"
#include "lexer.h"
#include "preprocessor.h"
#include "linker.h"
#include "elf_writer.h"
#include "xex_converter.h"
#include "standard_library.h"
#include "sdk_headers.h"
#include "ast_printer.h"
#include "test_suite.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>

using namespace nexia;

// -----------------------------------------------------------------------
// Tee stream buffer — writes to both a file and the original stream
// -----------------------------------------------------------------------
class TeeBuf : public std::streambuf {
public:
    TeeBuf(std::streambuf* consoleBuf, std::streambuf* fileBuf)
        : console_(consoleBuf), file_(fileBuf) {}
protected:
    int overflow(int c) override {
        if (c == EOF) return !EOF;
        if (console_) console_->sputc((char)c);
        if (file_) file_->sputc((char)c);
        return c;
    }
    int sync() override {
        if (console_) console_->pubsync();
        if (file_) file_->pubsync();
        return 0;
    }
private:
    std::streambuf* console_;
    std::streambuf* file_;
};

static void print_usage() {
    std::cout << "NexiaCompiler v2.0.0 (C++ rewrite)\n"
              << "Usage: nexiac [options] <source.cpp> [source2.cpp ...]\n"
              << "\n"
              << "Options:\n"
              << "  -o <file>         Output file path\n"
              << "  -I <path>         Add include search path\n"
              << "  -L <path>         Add library search path\n"
              << "  -l<name>          Link against <name>.lib\n"
              << "  <file>.lib        Link against library directly\n"
              << "  -D <name[=val]>   Define preprocessor macro\n"
              << "  -E                Preprocess only\n"
              << "  -c                Compile only (don't link)\n"
              << "  -O0/-O1/-O2/-Os   Optimization level\n"
              << "  --lex-only        Tokenize only (dump tokens)\n"
              << "  --print-ast       Print AST after parsing\n"
              << "  --xex             Also produce XEX output\n"
              << "  --title-id <hex>  XEX title ID\n"
              << "  --title-name <n>  XEX title name\n"
              << "  --gen-sdk <path>  Generate SDK headers\n"
              << "  -g                Generate debug info\n"
              << "  --log <file>      Write all output to log file (default: nexiac_log.txt)\n"
              << "  -v                Verbose output\n"
              << "  --test [filter]   Run test suite\n"
              << "  --help            Show this message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    CompilerOptions options;
    std::vector<std::string> sourceFiles;
    bool lexOnly = false;
    bool runTests = false;
    std::string testFilter;
    std::string logFile = "nexiac_log.txt"; // default log file

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") { print_usage(); return 0; }
        else if (arg == "--lex-only") { lexOnly = true; }
        else if (arg == "--print-ast") { options.print_ast = true; }
        else if (arg == "-E") { options.preprocess_only = true; }
        else if (arg == "-c") { options.compile_only = true; }
        else if (arg == "-v") { options.verbose = true; }
        else if (arg == "-g") { options.generate_debug_info = true; }
        else if (arg == "-O0") { options.opt_level = OptimizationLevel::O0; }
        else if (arg == "-O1") { options.opt_level = OptimizationLevel::O1; }
        else if (arg == "-O2") { options.opt_level = OptimizationLevel::O2; }
        else if (arg == "-Os") { options.opt_level = OptimizationLevel::Os; }
        else if (arg == "--xex") { options.output_xex = true; }
        else if (arg == "-o" && i + 1 < argc) { options.output_file = argv[++i]; }
        else if (arg == "-I" && i + 1 < argc) { options.include_paths.push_back(argv[++i]); }
        else if (arg == "-L" && i + 1 < argc) { options.lib_dirs.push_back(argv[++i]); }
        else if (arg.substr(0, 2) == "-l" && arg.size() > 2) {
            // -ld3d9 -> d3d9.lib
            options.lib_paths.push_back(arg.substr(2) + ".lib");
        }
        else if (arg.size() > 4 && arg.substr(arg.size()-4) == ".lib") {
            // Direct .lib file path
            options.lib_paths.push_back(arg);
        }
        else if (arg == "-D" && i + 1 < argc) {
            std::string def = argv[++i];
            size_t eq = def.find('=');
            if (eq != std::string::npos)
                options.defines[def.substr(0, eq)] = def.substr(eq + 1);
            else
                options.defines[def] = "1";
        }
        else if (arg == "--title-id" && i + 1 < argc) {
            options.xex_title_id = (uint32_t)std::stoul(argv[++i], nullptr, 16);
        }
        else if (arg == "--title-name" && i + 1 < argc) { options.xex_title_name = argv[++i]; }
        else if (arg == "--gen-sdk" && i + 1 < argc) { options.gen_sdk_path = argv[++i]; }
        else if (arg == "--log" && i + 1 < argc) { logFile = argv[++i]; }
        else if (arg == "--test") {
            runTests = true;
            if (i + 1 < argc && argv[i+1][0] != '-') testFilter = argv[++i];
        }
        else if (arg[0] != '-') { sourceFiles.push_back(arg); }
        else { std::cerr << "Unknown option: " << arg << "\n"; return 1; }
    }

    // -----------------------------------------------------------------------
    // Set up log file — all cout/cerr output goes to both console and file
    // -----------------------------------------------------------------------
    std::ofstream logStream(logFile, std::ios::out | std::ios::trunc);
    std::streambuf* origCout = std::cout.rdbuf();
    std::streambuf* origCerr = std::cerr.rdbuf();
    TeeBuf coutTee(origCout, logStream.rdbuf());
    TeeBuf cerrTee(origCerr, logStream.rdbuf());
    std::cout.rdbuf(&coutTee);
    std::cerr.rdbuf(&cerrTee);

    std::cout << "NexiaCompiler v2.0.0 — Log started\n";
    std::cout << "Log file: " << logFile << "\n\n";

    // Generate SDK headers
    if (!options.gen_sdk_path.empty()) {
        std::cout << "Generating SDK headers to " << options.gen_sdk_path << "...\n";
        SdkHeaderGenerator gen(options.gen_sdk_path);
        gen.generate_all();
        std::cout << "Done.\n";
        if (sourceFiles.empty()) return 0;
    }

    // Run tests
    if (runTests) {
        std::cout << "NexiaCompiler v2.0.0 — Test Suite\n\n";
        TestSuite suite;
        int failures = suite.run_all(testFilter);
        return failures > 0 ? 1 : 0;
    }

    if (sourceFiles.empty()) {
        std::cerr << "Error: No source files specified.\n";
        return 1;
    }

    // Lex-only mode
    if (lexOnly) {
        for (const auto& filename : sourceFiles) {
            std::ifstream file(filename);
            if (!file) { std::cerr << "Error: Cannot open '" << filename << "'\n"; return 1; }
            std::ostringstream ss; ss << file.rdbuf();
            std::string source = ss.str();

            try {
                Lexer lexer(source, filename);
                auto tokens = lexer.tokenize();
                for (const auto& tok : tokens) {
                    std::cout << tok.line << ":" << tok.column << "  "
                              << token_type_name(tok.type);
                    if (!tok.value.empty() && tok.type != TokenType::Eof)
                        std::cout << "  '" << tok.value << "'";
                    std::cout << "\n";
                }
                std::cout << "\n" << tokens.size() << " tokens.\n";
            } catch (const LexerError& e) {
                std::cerr << e.what() << "\n";
                return 1;
            }
        }
        return 0;
    }

    // Preprocess-only mode (-E)
    if (options.preprocess_only) {
        for (const auto& filename : sourceFiles) {
            std::ifstream file(filename);
            if (!file) { std::cerr << "Error: Cannot open '" << filename << "'\n"; return 1; }
            std::ostringstream ss; ss << file.rdbuf();
            std::string source = ss.str();

            try {
                Preprocessor pp(options.include_paths);
                for (auto& [name, value] : options.defines)
                    pp.define(name, value);
                std::string processed = pp.process(source, filename);
                std::cout << processed;
            } catch (const PreprocessorError& e) {
                std::cerr << e.what() << "\n";
                return 1;
            }
        }
        std::cout.rdbuf(origCout);
        std::cerr.rdbuf(origCerr);
        return 0;
    }

    // Default output file
    if (options.output_file.empty()) {
        if (options.compile_only)
            options.output_file = "output.o";
        else if (options.output_xex)
            options.output_file = "output.exe"; // PE for imagexex
        else
            options.output_file = "output.elf";
    }

    std::cout << "NexiaCompiler v2.0.0\n";

    // Compile each source file
    Compiler compiler(options);
    std::vector<std::unique_ptr<ObjectFile>> objects;

    for (const auto& sourceFile : sourceFiles) {
        auto obj = compiler.compile_file(sourceFile);
        if (!obj) {
            std::cerr << "Compilation failed for " << sourceFile << "\n";
            for (const auto& err : compiler.errors())
                std::cerr << "  " << err << "\n";
            return 1;
        }
        objects.push_back(std::move(obj));
    }

    // Print warnings (only in verbose mode — SDK headers generate many harmless warnings)
    if (options.verbose) {
        for (const auto& w : compiler.warnings())
            std::cerr << "  " << w << "\n";
    } else if (!compiler.warnings().empty()) {
        std::cout << "  " << compiler.warnings().size() << " warnings (use -v to see them)\n";
    }

    if (options.compile_only) {
        std::cout << "Compilation successful.\n";
        return 0;
    }

    // Link
    std::cout << "  [Linker] Linking...\n";
    Linker linker;

    // Add standard library
    StandardLibrary stdlib;
    linker.add_object(stdlib.build());

    for (auto& obj : objects)
        linker.add_object(std::move(*obj));

    // Add SDK libraries
    for (auto& libPath : options.lib_paths) {
        // Search in lib_dirs first, then try as-is
        bool found = false;
        for (auto& dir : options.lib_dirs) {
            std::string fullPath = dir + "/" + libPath;
            std::ifstream test(fullPath);
            if (test) { linker.add_library(fullPath); found = true; break; }
            // Try with backslash too
            fullPath = dir + "\\" + libPath;
            test.open(fullPath);
            if (test) { linker.add_library(fullPath); found = true; break; }
        }
        if (!found) {
            std::ifstream test(libPath);
            if (test) linker.add_library(libPath);
            else std::cerr << "  Warning: Library not found: " << libPath << "\n";
        }
    }

    if (!linker.link(options.output_file)) {
        std::cerr << "Linking failed.\n";
        for (const auto& err : linker.errors())
            std::cerr << "  " << err << "\n";
        return 1;
    }

    std::cout << "Output: " << options.output_file << "\n";

    // Convert to XEX if requested
    if (options.output_xex) {
        std::string xexPath = options.output_file;
        size_t dot = xexPath.rfind('.');
        if (dot != std::string::npos) xexPath = xexPath.substr(0, dot);
        xexPath += ".xex";

        // Try to use Microsoft's imagexex.exe from the Xbox 360 SDK
        std::string imagexex;
        // Check common locations
        const char* paths[] = {
            "imagexex.exe",
            "C:\\Program Files (x86)\\Microsoft Xbox 360 SDK\\bin\\win32\\imagexex.exe",
            "C:\\Program Files\\Microsoft Xbox 360 SDK\\bin\\win32\\imagexex.exe",
        };
        for (auto& p : paths) {
            std::ifstream test(p);
            if (test) { imagexex = p; break; }
        }
        // Also check XEDK environment variable
        if (imagexex.empty()) {
            const char* xedk = std::getenv("XEDK");
            if (xedk) {
                std::string candidate = std::string(xedk) + "\\bin\\win32\\imagexex.exe";
                std::ifstream test(candidate);
                if (test) imagexex = candidate;
            }
        }

        if (!imagexex.empty()) {
            std::cout << "  [XEX] Using imagexex: " << imagexex << "\n";

            // Try imagexex with just /IN and /OUT — simplest invocation
            // If it works, the XEX is produced by the official tool
            // If it fails (wrong PE format, missing imports), fall back to built-in
            std::string cmd = "\"" + imagexex + "\""
                              " /IN:" + options.output_file +
                              " /OUT:" + xexPath;
            std::cout << "  [XEX] " << cmd << "\n";
            int ret = system(cmd.c_str());
            if (ret == 0) {
                std::cout << "  XEX output: " << xexPath << "\n";
            } else {
                std::cerr << "  Warning: imagexex failed (exit code " << ret << ")\n";
                std::cerr << "  Falling back to built-in XEX converter\n";
                XexConverter converter;
                XexOptions xexOpts;
                xexOpts.title_id = options.xex_title_id;
                xexOpts.title_name = options.xex_title_name;
                converter.convert(options.output_file, xexPath, xexOpts);
                std::cout << "  XEX output: " << xexPath << "\n";
            }
        } else {
            std::cout << "  [XEX] imagexex.exe not found, using built-in converter\n";
            XexConverter converter;
            XexOptions xexOpts;
            xexOpts.title_id = options.xex_title_id;
            xexOpts.title_name = options.xex_title_name;
            converter.convert(options.output_file, xexPath, xexOpts);
            std::cout << "  XEX output: " << xexPath << "\n";
        }
    }

    std::cout << "Build successful.\n";

    // Restore original stream buffers
    std::cout.rdbuf(origCout);
    std::cerr.rdbuf(origCerr);
    return 0;
}
