// NexiaCompiler v2.0 — Test Suite
// Ported from TestSuite.cs
#include "test_suite.h"
#include "lexer.h"
#include "parser.h"
#include "preprocessor.h"
#include <iostream>

namespace nexia {

TestSuite::TestSuite() {
    register_lexer_tests();
    register_preprocessor_tests();
    register_parser_tests();
}

int TestSuite::run_all(const std::string& filter) {
    pass_count_ = fail_count_ = skip_count_ = 0;
    for (auto& t : tests_) {
        if (!filter.empty() && t.category != filter) {
            skip_count_++;
            continue;
        }
        run_test(t);
    }
    std::cout << "\n" << pass_count_ << " passed, "
              << fail_count_ << " failed, "
              << skip_count_ << " skipped\n";
    return fail_count_;
}

void TestSuite::run_test(TestCase& test) {
    auto start = std::chrono::steady_clock::now();
    try {
        test.action();
        test.result = TestResult::Pass;
        pass_count_++;
        std::cout << "  PASS: " << test.name << "\n";
    } catch (const std::exception& e) {
        test.result = TestResult::Fail;
        test.failure_message = e.what();
        fail_count_++;
        std::cout << "  FAIL: " << test.name << " - " << e.what() << "\n";
    }
    auto end = std::chrono::steady_clock::now();
    test.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

TestCase& TestSuite::add_test(const std::string& category, const std::string& name,
                               std::function<void()> action) {
    tests_.emplace_back(name, category, std::move(action));
    return tests_.back();
}

void TestSuite::assert_true(bool cond, const std::string& msg) {
    if (!cond) throw TestAssertException(msg);
}

void TestSuite::assert_no_throw(std::function<void()> action, const std::string& ctx) {
    try {
        action();
    } catch (const std::exception& e) {
        throw TestAssertException(ctx + ": threw " + e.what());
    }
}

void TestSuite::register_lexer_tests() {
    add_test("Lexer", "Tokenize keywords", []() {
        Lexer lexer("int float if else while", "test.cpp");
        auto tokens = lexer.tokenize();
        assert_true(tokens.size() >= 6, "Expected at least 6 tokens");
        assert_true(tokens[0].type == TokenType::KW_Int, "Expected KW_Int");
    });

    add_test("Lexer", "Tokenize operators", []() {
        Lexer lexer("+ - * / == != < >", "test.cpp");
        auto tokens = lexer.tokenize();
        assert_true(tokens.size() >= 9, "Expected at least 9 tokens");
    });

    add_test("Lexer", "Tokenize string literal", []() {
        Lexer lexer("\"hello world\"", "test.cpp");
        auto tokens = lexer.tokenize();
        assert_true(tokens.size() >= 2, "Expected at least 2 tokens");
        assert_true(tokens[0].type == TokenType::StringLiteral, "Expected StringLiteral");
    });

    add_test("Lexer", "Tokenize integer literals", []() {
        Lexer lexer("42 0xFF 0777 0", "test.cpp");
        auto tokens = lexer.tokenize();
        assert_true(tokens.size() >= 5, "Expected at least 5 tokens");
        assert_true(tokens[0].type == TokenType::IntegerLiteral, "Expected IntegerLiteral");
    });
}

void TestSuite::register_preprocessor_tests() {
    add_test("Preprocessor", "Define and expand", []() {
        Preprocessor pp;
        pp.define("FOO", "42");
        std::string result = pp.process("int x = FOO;", "test.cpp");
        assert_true(result.find("42") != std::string::npos, "Expected 42 in output");
    });

    add_test("Preprocessor", "ifdef true", []() {
        Preprocessor pp;
        pp.define("FOO");
        std::string result = pp.process("#ifdef FOO\nint x = 1;\n#endif", "test.cpp");
        assert_true(result.find("int x = 1") != std::string::npos, "Expected code in output");
    });

    add_test("Preprocessor", "ifdef false", []() {
        Preprocessor pp;
        std::string result = pp.process("#ifdef FOO\nint x = 1;\n#endif", "test.cpp");
        assert_true(result.find("int x = 1") == std::string::npos, "Expected code to be excluded");
    });

    add_test("Preprocessor", "Function-like macro", []() {
        Preprocessor pp;
        pp.define_function_macro("ADD", {"a", "b"}, "((a) + (b))");
        std::string result = pp.process("int x = ADD(1, 2);", "test.cpp");
        assert_true(result.find("((1) + (2))") != std::string::npos, "Expected expanded macro");
    });
}

void TestSuite::register_parser_tests() {
    add_test("Parser", "Parse variable declaration", []() {
        Lexer lexer("int x = 42;", "test.cpp");
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        assert_true(ast->declarations.size() >= 1, "Expected at least 1 declaration");
    });

    add_test("Parser", "Parse function definition", []() {
        Lexer lexer("int main() { return 0; }", "test.cpp");
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        assert_true(ast->declarations.size() >= 1, "Expected at least 1 declaration");
    });

    add_test("Parser", "Parse if statement", []() {
        Lexer lexer("void f() { if (x > 0) { y = 1; } else { y = 2; } }", "test.cpp");
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        assert_true(ast->declarations.size() >= 1, "Expected at least 1 declaration");
    });
}

} // namespace nexia
