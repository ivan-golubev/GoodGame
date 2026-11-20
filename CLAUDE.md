# GoodGame

A C++23 renderer with two backends, D3D12 and Vulkan, built with MSVC as C++20 modules (`.ixx` interfaces, `.cpp` implementations). Shared code is in `projects/Common`, the backends in `projects/RendererD3D12` and `projects/RendererVulkan`, the executable in `projects/Game`. HLSL shaders are in `shaders/` and are compiled for both backends by a post-build step (`shaders/compile_shaders.ps1`).

## Conventions

Follow every rule in @Conventions.md. In short: no global mutable state, string constants are `constexpr std::string_view`, no exceptions (`std::expected<T, Error>`), no naked `new`, raw pointers never own, no Hungarian notation, HLSL structs mirror their C++ structs field by field.

## Layout the formatter does not decide

clang-format keeps the line breaks you write (`ColumnLimit: 0`), so write them in this shape; `tools/check_conventions.py` reports deviations.

- Lines are at most 140 columns (tabs count as 4).
- A call, declaration or braced list that does not fit breaks right after `(` / `{`: one item per line, trailing commas, `)` / `}` on its own line.
- Long expressions break before the operator: `| || && ? : +` start the continuation line.
- Keep `return std::unexpected(Error{` on one line, followed by `.code` / `.message` and `});`.
- clang-format joins calls split inside a braced initializer (`Error{ .message = std::format(...) }`). If such a line is too long, build the value in a local first: `auto const message{ std::format(...) };`.
- A multi-line declaration inside a group of aligned declarations pads its neighbours; separate it with blank lines.
- Prefer `if (...)` with the statement on the next line.

## Checks

The clang tools are not on PATH; use the ones that ship with Visual Studio:

```
python tools/code_checks.py --clang-format "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-format.exe" --clang-tidy "C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/clang-tidy.exe"
```

This runs clang-format, `tools/check_conventions.py` and clang-tidy over everything in `projects/`. `--fix` applies formatting; `--format`, `--conventions` and `--tidy` run one check. `python tools/check_conventions.py --help` lists the convention rules.

When Claude Code edits a C++ file under `projects/`, hooks in `.claude/settings.local.json` format it, check conventions and run clang-tidy. Findings come back as hook errors: fix them before moving on.

clang-tidy cannot import the MSVC-built modules, so code that depends on types from them (mostly method bodies in `.cpp` files) is only partly checked. Check naming in such code by eye.

## Build

```
"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" GoodGame.sln -p:Configuration=Debug -p:Platform=x64 -m
```

Configurations: `Debug`, `Release`, `Final`. A change that touches both backends should build in at least Debug and Release. Compiling does not prove rendering works: changes to startup, shutdown, resource uploads or shaders also need a run of each renderer.

## Abbreviations

Names may start with an abbreviation listed in `tools/naming_abbreviations.txt` (`MVP`, `GPU`, `DSVFormat`). After editing that list, run `python tools/naming_abbreviations.py` to regenerate the matching section of `.clang-tidy`.
