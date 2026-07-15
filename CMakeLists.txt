cmake_minimum_required(VERSION 3.14)
project(NexiaCompiler VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Toolchain detection
# ---------------------------------------------------------------------------
if(MINGW)
    message(STATUS "Building with MinGW (${CMAKE_CXX_COMPILER})")

    # MinGW needs these for std::regex, std::filesystem, etc.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

    # Avoid MinGW "too many sections" error on large translation units
    add_compile_options(-Wa,-mbig-obj)
endif()

# Optimization for large files (preprocessed XDK headers can be 100K+ lines)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /GL)
        add_link_options(/LTCG)
    else()
        add_compile_options(-O2 -flto)
        add_link_options(-flto)
    endif()
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# MinGW: link ws2_32 if we ever need sockets, and use Win32 threads
if(MINGW)
    link_libraries(ws2_32)
endif()

# ---------------------------------------------------------------------------
# Main compiler library (shared between CLI and tests)
# ---------------------------------------------------------------------------
add_library(nexia_lib STATIC
    src/token.cpp
    src/lexer.cpp
    src/ast.cpp
    src/error_reporter.cpp
    src/symbol_table.cpp
    src/preprocessor.cpp
    src/parser.cpp
    src/semantic_analyzer.cpp
    src/code_generator.cpp
    src/optimizer.cpp
    src/ppc_instructions.cpp
    src/register_allocator.cpp
    src/elf_writer.cpp
    src/linker.cpp
    src/standard_library.cpp
    src/sdk_headers.cpp
    src/xex_converter.cpp
    src/debug_info.cpp
    src/ast_printer.cpp
    src/compiler.cpp
    src/test_suite.cpp
)

target_include_directories(nexia_lib PUBLIC include)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(nexiac src/main.cpp)
target_link_libraries(nexiac PRIVATE nexia_lib)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(NEXIA_BUILD_TESTS "Build test suite" ON)

if(NEXIA_BUILD_TESTS)
    add_executable(nexia_tests tests/test_lexer.cpp)
    target_link_libraries(nexia_tests PRIVATE nexia_lib)
    target_include_directories(nexia_tests PRIVATE include)
endif()
