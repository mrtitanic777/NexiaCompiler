# ===========================================================================
# NexiaCompiler v2.0 — Makefile for MinGW (no CMake required)
# ===========================================================================
#
# Usage:
#   mingw32-make              Build everything (Release)
#   mingw32-make debug        Build with debug symbols
#   mingw32-make test         Build and run tests
#   mingw32-make clean        Remove build artifacts
#
# If your g++ is not on PATH, set it:
#   mingw32-make CXX=C:/mingw64/bin/g++
#

# Toolchain
CXX      ?= g++
AR       ?= ar

# Directories
SRCDIR   = src
INCDIR   = include
BUILDDIR = build
OBJDIR   = $(BUILDDIR)/obj
TESTDIR  = tests

# Output
TARGET   = $(BUILDDIR)/nexiac.exe
TESTBIN  = $(BUILDDIR)/nexia_tests.exe
LIBRARY  = $(BUILDDIR)/libnexia.a

# Flags
CXXFLAGS = -std=c++17 -I$(INCDIR) -Wall -Wextra
LDFLAGS  = -static -static-libgcc -static-libstdc++

# Release by default
CXXFLAGS += -O2
ifdef DEBUG
CXXFLAGS  = -std=c++17 -I$(INCDIR) -Wall -Wextra -g -O0 -DDEBUG
endif

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
LIB_SRCS = \
	$(SRCDIR)/token.cpp \
	$(SRCDIR)/lexer.cpp \
	$(SRCDIR)/ast.cpp \
	$(SRCDIR)/error_reporter.cpp \
	$(SRCDIR)/symbol_table.cpp \
	$(SRCDIR)/preprocessor.cpp \
	$(SRCDIR)/parser.cpp \
	$(SRCDIR)/semantic_analyzer.cpp \
	$(SRCDIR)/code_generator.cpp \
	$(SRCDIR)/optimizer.cpp \
	$(SRCDIR)/ppc_instructions.cpp \
	$(SRCDIR)/register_allocator.cpp \
	$(SRCDIR)/elf_writer.cpp \
	$(SRCDIR)/linker.cpp \
	$(SRCDIR)/standard_library.cpp \
	$(SRCDIR)/sdk_headers.cpp \
	$(SRCDIR)/xex_converter.cpp \
	$(SRCDIR)/debug_info.cpp \
	$(SRCDIR)/ast_printer.cpp \
	$(SRCDIR)/compiler.cpp \
	$(SRCDIR)/coff_reader.cpp \
	$(SRCDIR)/test_suite.cpp

MAIN_SRC  = $(SRCDIR)/main.cpp
TEST_SRC  = $(TESTDIR)/test_lexer.cpp

# Object files
LIB_OBJS  = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(LIB_SRCS))
MAIN_OBJ  = $(OBJDIR)/main.o
TEST_OBJ  = $(OBJDIR)/test_lexer.o

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
.PHONY: all clean debug test help

all: $(TARGET)
	@echo.
	@echo  ============================================
	@echo   Build successful!
	@echo   Output: $(TARGET)
	@echo  ============================================
	@echo.
	@echo  Try:
	@echo    $(TARGET) --help
	@echo    $(TARGET) --lex-only yourfile.cpp
	@echo    $(TARGET) --test
	@echo.

debug:
	$(MAKE) DEBUG=1 all

# Compiler executable
$(TARGET): $(LIBRARY) $(MAIN_OBJ)
	@echo  [LINK] $@
	@$(CXX) $(CXXFLAGS) $(MAIN_OBJ) -L$(BUILDDIR) -lnexia $(LDFLAGS) -o $@

# Test executable
$(TESTBIN): $(LIBRARY) $(TEST_OBJ)
	@echo  [LINK] $@
	@$(CXX) $(CXXFLAGS) $(TEST_OBJ) -L$(BUILDDIR) -lnexia $(LDFLAGS) -o $@

# Static library (all compiler modules)
$(LIBRARY): $(LIB_OBJS)
	@echo  [AR]   $@
	@$(AR) rcs $@ $(LIB_OBJS)

# Compile library sources
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo  [CXX]  $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test sources
$(OBJDIR)/test_lexer.o: $(TESTDIR)/test_lexer.cpp | $(OBJDIR)
	@echo  [CXX]  $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directories
$(OBJDIR):
	@if not exist "$(BUILDDIR)" mkdir "$(BUILDDIR)"
	@if not exist "$(OBJDIR)" mkdir "$(OBJDIR)"

# Run tests
test: $(TESTBIN)
	@echo.
	@echo  Running tests...
	@echo  ------------------------------------------
	@$(TESTBIN)
	@echo  ------------------------------------------

# Clean
clean:
	@echo  Cleaning...
	@if exist "$(BUILDDIR)" rmdir /s /q "$(BUILDDIR)"
	@echo  Done.

# Help
help:
	@echo.
	@echo  NexiaCompiler v2.0 Build System
	@echo  ================================
	@echo.
	@echo  Targets:
	@echo    all     - Build the compiler (default)
	@echo    debug   - Build with debug symbols
	@echo    test    - Build and run test suite
	@echo    clean   - Remove build artifacts
	@echo    help    - Show this message
	@echo.
	@echo  Examples:
	@echo    mingw32-make
	@echo    mingw32-make debug
	@echo    mingw32-make test
	@echo    mingw32-make clean
	@echo.
