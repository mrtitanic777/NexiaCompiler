// NexiaCompiler v2.0 — Compiler driver
// Ported from Compiler.cs
#include <memory>
#include "compiler.h"
#include "preprocessor.h"
#include "lexer.h"
#include "parser.h"
#include "semantic_analyzer.h"
#include "optimizer.h"
#include "code_generator.h"
#include "ast_printer.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>

namespace nexia {

Compiler::Compiler(CompilerOptions options) : options_(std::move(options)) {}

std::unique_ptr<ObjectFile> Compiler::compile_file(const std::string& sourceFile) {
    std::ifstream file(sourceFile);
    if (!file) {
        errors_.push_back("Cannot read '" + sourceFile + "'");
        return nullptr;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();

    if (options_.verbose)
        std::cout << "  Compiling: " << sourceFile << "\n";

    // Stage 0: Preprocess
    std::cout << "  [Preprocess] " << sourceFile << "...";
    std::string processed;
    try {
        Preprocessor pp(options_.include_paths);
        for (auto& [name, value] : options_.defines)
            pp.define(name, value);
        processed = pp.process(source, sourceFile);
        for (auto& w : pp.warnings)
            warnings_.push_back(w);
    } catch (const PreprocessorError& e) {
        std::cout << " FAILED\n";
        errors_.push_back(e.what());
        return nullptr;
    }
    std::cout << " done (" << processed.size() / 1024 << "KB)\n";

    // Stage 1: Lex
    std::cout << "  [Lexer] Tokenizing...";
    std::vector<Token> tokens;
    try {
        Lexer lexer(processed, sourceFile);
        tokens = lexer.tokenize();
    } catch (const LexerError& e) {
        std::cout << " FAILED\n";
        errors_.push_back(e.what());
        return nullptr;
    }
    std::cout << " done (" << tokens.size() << " tokens)\n";

    // Stage 2: Parse
    std::cout << "  [Parser] Building AST...";
    std::unique_ptr<TranslationUnit> ast;
    try {
        Parser parser(std::move(tokens));
        ast = parser.parse();
    } catch (const ParseError& e) {
        std::cout << " FAILED\n";
        errors_.push_back(e.what());
        return nullptr;
    }
    std::cout << " done (" << ast->declarations.size() << " declarations)\n";

    // Stage 3: Semantic analysis
    std::cout << "  [Semantic] Analyzing...";
    SemanticAnalyzer analyzer;
    bool ok = analyzer.analyze(*ast);
    for (auto& w : analyzer.warnings())
        warnings_.push_back(w.to_string());
    if (!ok) {
        std::cout << " FAILED\n";
        for (auto& e : analyzer.errors())
            errors_.push_back(e.what());
        return nullptr;
    }
    std::cout << " done\n";

    // Stage 4: Codegen
    std::cout << "  [CodeGen] Generating PPC...";
    CodeGenerator codegen;
    codegen.generate(*ast);
    std::cout << " done\n";

    auto obj = std::make_unique<ObjectFile>(sourceFile);
    obj->code = codegen.code();
    obj->data = codegen.data();
    for (auto& [name, offset] : codegen.function_offsets())
        obj->exported_symbols.emplace_back(name, LinkerSymbolKind::Function, offset, 0, LinkerSection::Text, true);
    obj->imported_symbols = codegen.external_calls();
    return obj;
}

} // namespace nexia
