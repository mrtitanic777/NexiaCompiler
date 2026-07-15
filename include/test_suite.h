#pragma once
// NexiaCompiler v2.0 — Test Suite
// Ported from TestSuite.cs

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <chrono>

namespace nexia {

enum class TestResult : uint8_t { Pass, Fail, Skip };

struct TestCase {
    std::string name;
    std::string category;
    TestResult result = TestResult::Skip;
    std::string failure_message;
    long elapsed_ms = 0;
    std::function<void()> action;

    TestCase(std::string name, std::string category, std::function<void()> action)
        : name(std::move(name)), category(std::move(category)), action(std::move(action)) {}
};

class TestAssertException : public std::runtime_error {
public:
    TestAssertException(const std::string& message) : std::runtime_error(message) {}
};

class TestSuite {
public:
    TestSuite();

    int run_all(const std::string& categoryFilter = "");

private:
    std::vector<TestCase> tests_;
    int pass_count_ = 0;
    int fail_count_ = 0;
    int skip_count_ = 0;

    void run_test(TestCase& test);
    TestCase& add_test(const std::string& category, const std::string& name,
                       std::function<void()> action);

    static void assert_true(bool condition, const std::string& message);
    template<typename T>
    static void assert_equal(const T& expected, const T& actual, const std::string& context = "");
    template<typename ExcType>
    static void assert_throws(std::function<void()> action, const std::string& context = "");
    static void assert_no_throw(std::function<void()> action, const std::string& context = "");

    void register_lexer_tests();
    void register_preprocessor_tests();
    void register_parser_tests();
    void register_semantic_tests();
    void register_codegen_tests();
    void register_linker_tests();
    void register_integration_tests();
};

} // namespace nexia
