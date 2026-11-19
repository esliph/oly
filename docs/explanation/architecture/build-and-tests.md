# Build, tests and tools

The tasks, services and presets are in [Commands](../../reference/contributor/commands.md). This page explains the organization.

## Toolchain

There is no C++ toolchain on the host. Everything runs in Docker Compose, in the services that [Commands](../../reference/contributor/commands.md#services) lists.

`toolchain.env` holds the toolchain versions, in one place: `./spur images` passes them to the Dockerfiles as build arguments (none has a default), and `cmake/OlyToolchain.cmake` checks the compiler, Emscripten, CMake and Ninja against the same file at configure time. Emscripten must equal `EMSDK_VERSION`; GCC, MinGW GCC, CMake and Ninja must be at least their `*_MINIMUM`. The minimums are the versions the images test with, not bisected. `./spur images` is the only way to build the images: a plain `docker compose build` has no `toolchain.env` and stops naming the missing variable. The `oly-schema` of the `wasm` image is built with emsdk's Clang (`CC=clang`), because Ubuntu's default GCC there is below `GCC_MINIMUM`.

## Presets

The presets are in [Commands](../../reference/contributor/commands.md#presets). Each rung of the cost study ladder changes a single factor (compiler, pointer width, WASM target, SIMD).

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
| `oly_add_plugin` (`OlyPlugin.cmake`) | creates the plugin `oly_<p>` (alias `oly::<p>`) from `src/*.cpp`, links its `DEPENDS` and `PLUGINS` and, with `SCHEMA`, generates `oly/<p>/schema.generated.hpp` ([plugins.md](../../reference/cmake.md)) |
| `oly_plugin_order` (`OlyPlugin.cmake`) | the install order of a plugin list: dependencies first, each plugin once, list order among independent ones |
| `oly_schema_tool` (`OlyPlugin.cmake`) | picks the `oly-schema` a custom command runs: the in-tree target, or `OLY_SCHEMA_TOOL` |
| `oly_add_game` | `ENVIRONMENT web`: runs `oly-schema` and links a game's `.mjs`/`.wasm` module, with `oly::web_host`. `ENVIRONMENT native`: runs `oly-schema` and builds an executable. The environment must be the build's `OLY_ENVIRONMENT`. `DIMENSION 2d|3d` (required): turns `essentials2d` or `essentials3d` on, first in the install order; listing either in `PLUGINS` is an error. `PLUGINS`: merges the plugins' `.oly`, links them and generates `game_setup.generated.cpp` (`setUpGame`). `TYPESCRIPT <file.ts>`: also writes the merged schema as TS, through the target `oly_typescript_schemas` |
| `oly_add_engine_test_game` | the same as `oly_add_game`, for the engine's own test games; the lint rejects it outside `engine/` |
| `oly_warnings`, `oly_wasm_exceptions`, `oly_wasm_node_env` | warnings and Emscripten settings |

## Tests

- **One doctest suite = one executable**, registered with `oly_add_test(MODULE <m> NAME <suite> SOURCES ... [SUPPORT <dirs>])` in the module's `tests/CMakeLists.txt`. A new test file must be added there; the sources in `src/*.cpp` come in by glob.
- **Names in ctest** carry the module prefix: `core.*`, `ecs.*`, `input.*`, `app.*`, `capi.*`, `schema.*`, `schema_typescript.*`, `<module>.contract_death.<case>`, `native_host.*`, `ts.<package>.<suite>` (`ts.core.unit`, `ts.web_host.unit`, `ts.testing.wasm`), `fixture.plugins`, `oly.cmake.plugin_order.<case>`, `oly.lint.boundaries` and `oly.lint.boundaries_selftest`, `oly.lint.links` and `oly.lint.links_selftest`.
- **Plugin tests.** `engine/testing/` is added on `web` with the games and on `native` with the tests, so every native preset with tests builds the test plugins ([plugins.md](plugins.md#test-plugins)). `fixture.plugins` (native) runs `plugin_fixture_game`, an engine test game with `PLUGINS doubling`; `ts.testing.wasm` checks the same plugins in the web fixture. `oly.cmake.plugin_order.<case>` runs `cmake/tests/plugin_order.cmake` in script mode, one case per test (order, shared dependency, cycle, unknown plugin).
- **Death tests**: registered with `oly_add_contract_death(MODULE <m> SOURCE <file> CASES ...)`. Each case is a command-line argument. An assert handler checks substrings of the message and exits with 0; if the case does not fire the assert, it fails. New case = function in the file + name in the `CASES` list. They exist in `ecs`, `input`, `app` and `capi`, only with asserts enabled. The handler of `OLY_ASSERT` is replaceable, which is what makes them possible.
- **Invariants**: `checkInvariants(const World&, std::string* why)` checks I-1 to I-13 (records, columns, indices, edges, queries, free list, iterating mode, observer flags, closure under implication). Tests call it after every structural operation.
- **Fuzz against a model**: `fuzz_vs_model` compares the `World` with a trivial map. Values derive from the `Entity`, so swapped rows show up as a wrong value. Seed and number of operations via `OLY_FUZZ_SEED`/`OLY_FUZZ_OPS`.
- **`LifeCounter`** counts constructions − destructions and catches undestroyed moved-from objects and double destroy.
- **TS tests** run in node against the real WASM, not against a mock.
- **Desktop tests** run SDL with `SDL_VIDEO_DRIVER=dummy`, so they need no display. `native_host.window` also checks that the X11 and Wayland drivers (Linux) or the `windows` driver are compiled in.
- **`capi.foreign_client`** is a plain C program (no doctest) that drives an engine only through `oly*`.

To add a test, use the `add-test-case` skill ([Set up the toolchain](../../how-to/contributor/set-up-the-toolchain.md#5-add-a-test)).

## The engine as a subproject, and the consumer tests

A game fetches the engine with `FetchContent` ([Publishing](publishing.md#the-engine-as-a-subproject)). Three things in this repository check that:

| Task | What it checks |
|---|---|
| `./spur subproject-test` | `cmake/tests/consumer`, a CMake project outside the engine tree that fetches it from this checkout (`FETCHCONTENT_SOURCE_DIR_OLY`) and builds one game with `oly_add_game`, native and web. Each run must print `consumer ok`. It covers the subproject defaults: no tests, no benchmarks, no games, no lint, no `-Werror` |
| `./spur consumer-test` | creates a game through the launcher (`oly new`) in local mode (`OLY_ENGINE_DIR` is this checkout), for `web` and for `native`, builds and tests it. A third run, `docker`, runs `test --docker` in both games, and `--platform windows` for the native one |
| `./spur staged-test <version>` | the same, against `./spur stage <version>` instead of the checkout: the launcher is installed with `install.sh` into a fresh `OLY_HOME`, and the tools, `oly-schema` and the npm tarballs are the downloaded ones |

`OLY_SCHEMA_TOOL` is an input of `oly_add_game` and `oly_schema_tool`: a web game gets the prebuilt `oly-schema` of its version, or the one local mode built (a desktop game builds it with itself), and the in-tree default stays for the engine's own build. `./spur oly-tools-test` runs the shell tests of `tools/oly/` (launcher, `install.sh`, `new`, doctor, Docker dispatch, `npm pack`) and `./spur release-test` those of `tools/release/`. The consumer test and its three runs are explained in [Publishing](publishing.md#the-consumer-test).

## Code quality

- Warnings, formatting and style rules: [Code style](../../reference/contributor/code-style.md).
- The boundaries lint runs outside Emscripten and in the `wasm-games*` presets, which have Python in the environment ([Boundary rules](../../reference/contributor/boundary-rules.md)).

## Claude Code automation (`.claude/`)

- Project skills: `add-test-case` (register a suite, a `TEST_CASE` or a `contract_death` case) and `bench-compare` (manual only, `/bench-compare <prefix>`: measures NF-P1/P2/P5 and records into `docs/reference/benchmarks/`).
- Agents: `spec-conformance-reviewer` (spec, invariants and `checkInvariants` coverage) and `perf-reviewer` (hot-path cost, checked in the assembly).
- `PostToolUse` hook `.claude/hooks/clang-format.sh`: runs `clang-format -i` in the `dev` container after every edit of a `.cpp` or `.hpp` file. If Docker is stopped, the hook fails. The same formatter is the one `./spur format-check` runs in CI.
