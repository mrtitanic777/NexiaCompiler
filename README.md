<h1 align="center">NexiaCompiler</h1>

<p align="center">
  <strong>A from-scratch C++ compiler that turns C++03 source into runnable Xbox 360 executables (XEX).</strong><br>
  No MSVC, no XEDK compiler — just <code>nexiac</code>.
</p>

<p align="center">
  <a href="#what-it-does">What it does</a> •
  <a href="#building-the-compiler">Build</a> •
  <a href="#usage">Usage</a> •
  <a href="#compiling-for-xbox-360">Xbox 360 target</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#license">License</a>
</p>

---

## What it does

NexiaCompiler (`nexiac`) is a hand-written C++ compiler toolchain that takes C++
source and emits **PowerPC (Xenon) code packaged as an Xbox 360 XEX** — the
format the console actually loads. It implements the whole pipeline itself
rather than shelling out to an existing compiler:

**Preprocess → Lex → Parse → Semantic analysis → Optimize → PPC code generation → ELF → XEX**

Highlights:

- **Complete front end** — preprocessor (macros, includes, conditionals),
  lexer, recursive-descent parser, semantic analyzer, and symbol tables.
- **PowerPC back end** — instruction selection, a register allocator, and an
  optimizer, targeting the Xbox 360's Xenon CPU (big-endian PPC).
- **Object + link + convert** — writes ELF objects, links them, and converts the
  result into a loadable `.xex` with a configurable title ID and title name.
- **Reads COFF** — can consume `.obj`/`.lib` COFF inputs (`coff_reader`).
- **Self-contained** — the compiler builds with nothing but a C++17 host
  compiler (MinGW g++). No Visual Studio or Xbox SDK is required to build
  `nexiac` itself.

> Origin: `nexiac` began life as a C# project (`Program.cs`) and was rewritten
> from the ground up in C++ for v2.0.

## Building the compiler

You only need **MinGW-w64 (g++ with C++17)** and `make`. Nothing else.

### Windows (easiest)

```bat
build.bat
```

`build.bat` checks for `g++` / `mingw32-make` on your PATH (and tells you where
to get them if missing), then builds the release binary to `build\nexiac.exe`.

### Make directly

```bash
mingw32-make            # release build  -> build/nexiac.exe
mingw32-make debug      # with debug symbols
mingw32-make test       # build and run the test suite
mingw32-make clean
```

### CMake (alternative)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Don't have MinGW? Grab it from [WinLibs](https://winlibs.com) or
[MSYS2](https://www.msys2.org) (`pacman -S mingw-w64-x86_64-gcc`) and add its
`bin\` folder to your PATH.

## Usage

```
nexiac [options] <source.cpp> [source2.cpp ...]

  -o <file>         Output file path
  -I <path>         Add include search path
  -L <path>         Add library search path
  -l<name>          Link against <name>.lib
  -D <name[=val]>   Define preprocessor macro
  -E                Preprocess only
  -c                Compile only (don't link)
  -O0/-O1/-O2/-Os   Optimization level
  --lex-only        Tokenize only (dump tokens)
  --print-ast       Print the AST after parsing
  --xex             Also produce XEX output
  --title-id <hex>  XEX title ID
  --title-name <n>  XEX title name
  --gen-sdk <path>  Generate SDK headers
  -g                Generate debug info
  -v                Verbose output
  --test [filter]   Run the test suite
  --help            Show this message
```

Example — compile a source file straight to a titled XEX:

```bash
nexiac main.cpp --xex --title-id 0x12345678 --title-name "Hello360" -o hello.xex
```

## Compiling for Xbox 360

⚠️ **The Xbox 360 XDK is NOT included in this repository.**

The `lib/`, `sdk/`, and `Extractor/d3d9_extracted/` folders contain Microsoft
Xbox 360 XDK libraries and headers. Those are **Microsoft proprietary** and are
deliberately excluded (and git-ignored) — this repo ships only the compiler
source.

To build the `nexiac` compiler you need **none of that**. But to compile a real
Xbox 360 program that `#include`s Xbox headers and links Xbox libraries, you
must supply your own legally-obtained XDK:

1. Install/locate your own Xbox 360 XDK.
2. Point `nexiac` at it with include and library search paths:
   ```bash
   nexiac game.cpp -I "path\to\xdk\include" -L "path\to\xdk\lib" -lxboxkrnl --xex -o game.xex
   ```
3. Optionally regenerate SDK header shims with `--gen-sdk <path>`.

The `Extractor/` scripts (`nlt.bat`, `nex.ps1`) are research tools for inspecting
XDK `.lib`/COFF objects; they do not contain any Microsoft binaries themselves.

## Architecture

| Stage | Source |
|---|---|
| Preprocessing | `preprocessor.cpp` |
| Lexing | `lexer.cpp`, `token.cpp` |
| Parsing / AST | `parser.cpp`, `ast.cpp`, `ast_printer.cpp` |
| Semantic analysis | `semantic_analyzer.cpp`, `symbol_table.cpp` |
| Optimization | `optimizer.cpp` |
| Code generation | `code_generator.cpp`, `ppc_instructions.cpp`, `register_allocator.cpp` |
| Object / SDK | `elf_writer.cpp`, `coff_reader.cpp`, `sdk_headers.cpp`, `standard_library.cpp` |
| Link + package | `linker.cpp`, `xex_converter.cpp` |
| Debug info | `debug_info.cpp` |
| Driver | `compiler.cpp`, `main.cpp` |

## Status

`nexiac` v2.0 is under active development. Expect rough edges around advanced
C++ features and complex link scenarios. Bug reports and PRs welcome.

## License

License TBD — see repository for details once added. Regardless of license,
you must supply your own legally-obtained Xbox 360 XDK to target the console;
no Microsoft SDK material is distributed in this repository.
