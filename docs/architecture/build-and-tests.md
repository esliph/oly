# Build, tests and tools

The everyday commands are in [../development/commands.md](../development/commands.md). This file explains the organization.

## Toolchain

There is no C++ toolchain on the host. Everything runs in Docker Compose:

| Service | Image | Use |
|---|---|---|
| `dev` | `Dockerfile` (`gcc:14`, CMake, perf) | native build and tests |
| `perf` | `Dockerfile`, privileged | `perf stat` |
| `wasm` | `Dockerfile.wasm` (`emscripten/emsdk:3.1.74`, emsdk's Clang as the native compiler, node 20.18, Python) | WASM, TS, cost study |
| `web` | `Dockerfile.wasm`, port 8000 | `./spur serve` |
| `windows` | `Dockerfile.windows` (`debian:trixie-slim`, mingw-w64 GCC, Wine) | Windows cross-build and tests under Wine |

## Presets

| Preset | What for |
|---|---|
| `debug-asan` (default) | tests with ASan + UBSan and asserts enabled |
| `release` | tests and benchmarks with `OLY_ENABLE_ASSERTS=0` |
| `relwithdebinfo` | benchmarks for `perf` |
| `schema-tool` | `oly-schema` built for the machine in the `wasm` image (Release, no tests, target `oly-schema` only); `./spur schema-tool` |
| `native-clang`, `native-clang-32`, `native-clang-32-scalar`, `native-clang-avx2`, `wasm-scalar`, `wasm-simd` | ladder of the native × WASM cost study |
| `wasm-debug` | ECS tests in WASM |
| `wasm-games`, `wasm-games-release` | games, `capi`, TS tests |
| `native-games` (from `debug-asan`), `native-games-release` (from `release`, no tests) | the native host and the desktop games on Linux |
| `windows`, `windows-release` | the same, cross-compiled with `cmake/toolchains/mingw-w64.cmake` |

Each rung of the cost study ladder changes a single factor (compiler, pointer width, WASM target, SIMD).

CMake options: `OLY_BUILD_TESTS`, `OLY_BUILD_BENCH`, `OLY_SANITIZE`, `OLY_WERROR`, `OLY_VEC_REPORT`, `OLY_ENABLE_ASSERTS`, `OLY_BUILD_GAMES`, `OLY_BUILD_NATIVE_HOST`.

`OLY_BUILD_NATIVE_HOST` (default `OFF`) builds `native_host` and fetches SDL3, so the other native presets never download it; it fails at configure on Emscripten, and `OLY_BUILD_GAMES` on a native build requires it. `OLY_ENVIRONMENT` is a variable, not an option: `web` under Emscripten, `native` otherwise.

SDL3 is a static library with only the video, events and render subsystems. The Windows toolchain links with `-static`, so the `.exe` needs no `libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll` or `SDL3.dll`; `objdump -p` lists only Windows system DLLs.

The native builds (Linux and Windows) run the in-tree `oly-schema` target. The WASM builds cannot run what they compile, so they use the `schema-tool` preset's. In the cross build, CMake prefixes a target `COMMAND` with `CMAKE_CROSSCOMPILING_EMULATOR` (`wine`), so `oly-schema.exe` runs under Wine, and so does every test executable.

## Build functions (`cmake/`)

| Function | What it does |
|---|---|
| `oly_add_module` | creates `oly_<m>` (alias `oly::<m>`) and links only the `DEPENDS` |
| `oly_add_test` | registers a doctest suite as an executable and a ctest test |
| `oly_add_contract_death` | registers a module's death cases |
| `oly_add_bench` | registers a benchmark executable |
| `oly_add_game` | `ENVIRONMENT web`: runs `oly-schema` and links a game's `.mjs`/`.wasm` module, with `oly::web_host`. `ENVIRONMENT native`: runs `oly-schema` and builds an executable. The environment must be the build's `OLY_ENVIRONMENT` |
| `oly_warnings`, `oly_wasm_exceptions`, `oly_wasm_node_env` | warnings and Emscripten settings |

## Tests

- **One doctest suite = one executable**, registered with `oly_add_test(MODULE <m> NAME <suite> SOURCES ... [SUPPORT <dirs>])` in the module's `tests/CMakeLists.txt`. A new test file must be added there; the sources in `src/*.cpp` come in by glob.
- **Names in ctest** carry the module prefix: `core.*`, `ecs.*`, `input.*`, `app.*`, `capi.*`, `schema.*`, `schema_typescript.*`, `<module>.contract_death.<case>`, `native_host.*`, `drift.desktop`, `ts.<package>.<suite>` (`ts.core.unit`, `ts.web_host.unit`, `ts.testing.wasm`, `ts.arena.smoke`) and `oly.lint.boundaries` / `oly.lint.boundaries_selftest`.
- **Death tests**: registered with `oly_add_contract_death(MODULE <m> SOURCE <file> CASES ...)`. Each case is a command-line argument. An assert handler checks substrings of the message and exits with 0; if the case does not fire the assert, it fails. New case = function in the file + name in the `CASES` list. They exist in `ecs`, `input`, `app` and `capi`, only with asserts enabled.
- **Invariants**: `checkInvariants(const World&, std::string* why)` checks I-1 to I-11 (records, columns, indices, edges, queries, free list, iterating mode). Tests call it after every structural operation.
- **Fuzz against a model**: `fuzz_vs_model` compares the `World` with a trivial map. Values derive from the `Entity`, so swapped rows show up as a wrong value. Seed and number of operations via `OLY_FUZZ_SEED`/`OLY_FUZZ_OPS`.
- **`LifeCounter`** counts constructions − destructions and catches undestroyed moved-from objects and double destroy.
- **TS tests** run in node against the real WASM, not against a mock.
- **Desktop tests** run SDL with `SDL_VIDEO_DRIVER=dummy`, so they need no display. `native_host.window` also checks that the X11 and Wayland drivers (Linux) or the `windows` driver are compiled in. `drift.desktop` runs the game's own executable for 600 frames.
- **`capi.foreign_client`** is a plain C program (no doctest) that drives an engine only through `oly*`.

## Code quality

- Warnings, formatting and style rules: [../development/code-style.md](../development/code-style.md).
- The boundaries lint runs outside Emscripten and in the `wasm-games*` presets, which have Python in the environment ([modules-and-boundaries.md](modules-and-boundaries.md)).

## Claude Code automation (`.claude/`)

- Project skills: `add-test-case` (register a suite, a `TEST_CASE` or a `contract_death` case) and `bench-compare` (manual only, `/bench-compare <prefix>`: measures NF-P1/P2/P5 and records into `docs/benchmarks/`).
- Agents: `spec-conformance-reviewer` (spec, invariants and `checkInvariants` coverage) and `perf-reviewer` (hot-path cost, checked in the assembly).
- `PostToolUse` hook `.claude/hooks/clang-format.sh`: formats `.cpp`/`.hpp` after every edit.
