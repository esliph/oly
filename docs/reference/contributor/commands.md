# Commands

[Reference](../../README.md#reference) · Concept: [Build, tests and tools](../../explanation/architecture/build-and-tests.md) · Setup: [Set up the toolchain](../../how-to/contributor/set-up-the-toolchain.md)

The `./spur` tasks of the engine repository, run from Git Bash at the repository root. Every task that builds or tests runs in a Docker container; the host needs only Git Bash and Docker.

## Run a task

- `./spur --list` shows the tasks, and `./spur --describe <task>` the help of one.
- Each task runs in the container its preset needs ([Services](#services)). Typed inside the wrong container, it exits with 64.
- `PRESET=<name>` chooses the preset of a task that takes one ([Presets](#presets)). The arguments after the task go to the tool the task runs (`ctest` for the test tasks).
- Images built before `./spur` existed lack `OLY_TOOLCHAIN` and stop with exit 69: run `./spur images`.
- `-x` and `-n` given on the host show only the dispatch to the container. To trace a task, run it inside: `./spur dev ./spur -x test`.

## Tasks

| Task | Runs in | What it does |
|---|---|---|
| `images` | host | builds the images from `toolchain.env`; the arguments go to `docker compose build` |
| `configure` | by preset | `cmake --preset $PRESET` (default `debug-asan`) |
| `build` | by preset | configures and builds `$PRESET` (default `debug-asan`) |
| `test` | by preset | builds and runs the tests of `$PRESET` (default `debug-asan`) |
| `native-games-test` | `dev` | the tests of a `native-games*` preset (default `native-games`): the native host and the desktop games |
| `windows-test` | `windows` | cross-compiles and runs the tests of a `windows*` preset (default `windows`) under Wine |
| `dev` | `dev` | a command, or a shell, in the `dev` container |
| `wasm` | `wasm` | a command, or a shell, in the `wasm` container |
| `windows` | `windows` | a command, or a shell, in the `windows` container |
| `format` | `dev` | clang-formats the given files, or every tracked `.cpp` and `.hpp` with no argument |
| `format-check` | `dev` | fails when a tracked `.cpp` or `.hpp` is not formatted (CI runs it) |
| `schema-tool` | `wasm` | builds `oly-schema` for the machine (preset `schema-tool`) |
| `ts` | `wasm` | builds `oly-schema`, configures `$PRESET` (default `wasm-games`), writes each test game's TS schema from its CMake target (`oly_typescript_schemas`) and the test-only schemas, and compiles the TS |
| `games-test` | `wasm` | `ts`, then the tests of a `wasm-games*` preset (default `wasm-games`) |
| `subproject-test` | `dev` and `wasm` | builds `cmake/tests/consumer`, a project outside the engine tree that fetches the engine with `FetchContent` |
| `oly-tools-test` | `wasm` | the shell tests of `tools/oly/` (launcher, `install.sh`, `new`, build arguments, doctor, Docker dispatch, `npm pack`) |
| `consumer-test` | `dev`, `wasm`, host | creates a game from the template in local mode, then builds and tests it |
| `release-test` | `dev` | the shell tests of `tools/release/` |
| `release` | host | writes a version into the tree and commits it |
| `stage-npm` | `wasm` | `npm pack` of `@oly/core` and `@oly/web-host` into a folder |
| `stage` | host | builds the release assets of a version into `build/release/<version>` |
| `staged-test` | `dev`, `wasm`, host | installs, creates, builds and tests a game from the staged release |

`subproject-test`, `consumer-test` and `staged-test` each have one task per half, which the parent task runs: `subproject-native` and `subproject-web`, `consumer-native`, `consumer-web` and `consumer-docker`, `staged-native`, `staged-web` and `staged-docker`. Pass the half as the argument of the parent (`./spur consumer-test web`).

## Services

| Service | Image | Use |
|---|---|---|
| `dev` | `Dockerfile` (`gcc`, CMake, perf) | native build and tests |
| `perf` | `Dockerfile`, privileged | `perf stat` |
| `wasm` | `Dockerfile.wasm` (`emscripten/emsdk`, emsdk's Clang as the native compiler, node, Python) | WASM, TS, cost study |
| `windows` | `Dockerfile.windows` (`debian:trixie-slim`, mingw-w64 GCC, Wine) | Windows cross-build and tests under Wine |

A preset runs in the `wasm` service when its name starts with `wasm` or `native-clang`, or is `schema-tool`; in the `windows` service when it starts with `windows` or is `schema-tool-windows`; in `dev` otherwise.

## Presets

| Preset | What for |
|---|---|
| `debug-asan` (default) | tests with ASan + UBSan and asserts enabled |
| `release` | tests and benchmarks with `OLY_ENABLE_ASSERTS=0` |
| `relwithdebinfo` | benchmarks for `perf` |
| `schema-tool` | `oly-schema` built for the machine in the `wasm` image (Release, no tests, target `oly-schema` only); `./spur schema-tool` |
| `schema-tool-linux`, `schema-tool-windows` | the `oly-schema` of a release: linux-x64 (static) and windows-x64 (MinGW, static); `./spur stage` builds both |
| `native-clang`, `native-clang-32`, `native-clang-32-scalar`, `native-clang-avx2`, `wasm-scalar`, `wasm-simd` | ladder of the native × WASM cost study |
| `wasm-debug` | ECS tests in WASM |
| `wasm-games`, `wasm-games-release` | the engine's test games, `capi`, TS tests |
| `native-games` (from `debug-asan`), `native-games-release` (from `release`, no tests) | the native host on Linux |
| `windows`, `windows-release` | the same, cross-compiled with `cmake/toolchains/mingw-w64.cmake` |

Build directories are `build/<preset>/`. Test executables are `build/<preset>/tests/<module>_<suite>` and benchmark executables `build/<preset>/bench/ecs_bench_*`.

## Native C++

```sh
./spur test                       # configure + build + ctest in the debug-asan preset (ASan+UBSan, asserts enabled)
./spur test -R '^ecs\.'           # one module (the arguments go to ctest)
./spur test -R 'ecs.unit_world'   # one suite
./spur test -R 'contract_death'   # all death cases
PRESET=release ./spur build       # builds tests + benchmarks in Release, asserts disabled
./spur dev ./build/debug-asan/tests/ecs_unit_world -tc="case name"   # one specific doctest TEST_CASE
./spur format <files>             # clang-format the given files; with no argument, every tracked .cpp/.hpp
./spur format-check               # fails when a tracked .cpp/.hpp is not formatted
```

The fuzz tests (`fuzz_vs_model`, parts of `command_buffer`) read the seed and the number of operations from `OLY_FUZZ_SEED` and `OLY_FUZZ_OPS` (`engine/runtime/ecs/tests/support/model.hpp`). The seed used is printed to stderr, to reproduce a failure.

## Native host and Windows

```sh
./spur native-games-test                   # ctest in the native-games preset: native_host.* and everything else
./spur windows-test                        # cross-compiles with MinGW and runs every test under Wine (preset windows)
```

`native-games-test` accepts only `native-games*` presets, and `windows-test` only `windows*` ones; otherwise they exit with 64. The arguments go to ctest. The first configure of a `native-games*` or `windows*` preset fetches SDL3.

## WASM and TypeScript

```sh
./spur schema-tool        # builds oly-schema for the machine (preset schema-tool)
./spur ts                 # generates the schemas and compiles the TS
./spur games-test         # ts + ctest in the wasm-games preset (ECS in WASM, capi, TS tests)
PRESET=wasm-games-release ./spur games-test -R '^ts\.'   # Release: only the TS validations catch the errors
```

- The `wasm-games*` presets require `./spur ts` (or `./spur schema-tool`) before configuring: `oly_add_game` runs `build/schema-tool/bin/oly-schema`.
- `./spur games-test` and `./spur ts` accept only `wasm-games*` presets and exit with 64 for another one. Without `PRESET`, they use `wasm-games`.
- To run a TS benchmark, see [Run the benchmarks](../../how-to/contributor/run-benchmarks.md).

## The engine as a subproject

```sh
./spur subproject-test          # builds cmake/tests/consumer native, then web, and runs both
./spur subproject-test native   # only the native one, in the dev container
./spur subproject-test web      # only the web one, in the wasm container
```

`cmake/tests/consumer` fetches the engine with `FetchContent`, here from this checkout through `FETCHCONTENT_SOURCE_DIR_OLY`, and builds one game with `oly_add_game`. Each run must print `consumer ok`.

## The game tasks and the consumer test

```sh
./spur oly-tools-test             # the shell tests of tools/oly/, in the wasm container
./spur consumer-test              # native, web, then docker
./spur consumer-test native       # in the dev container
./spur consumer-test web          # in the wasm container
./spur consumer-test docker       # on the host
```

`consumer-test` creates a game through the launcher (`sh tools/oly/oly new consumer-game --version <version>`) in `build/consumer/<environment>/`, with `OLY_ENGINE_DIR` set to this checkout (local mode), then builds and tests it as a user of `oly` would.

- `web` first packs `@oly/core` and `@oly/web-host` into `build/consumer/release/download/v<version>/` and serves them on `127.0.0.1:8765`, which the game installs from through `OLY_RELEASE_URL`. It then checks that going back to the pinned version replaces the checkout's packages in `node_modules`.
- `docker` needs the games the two earlier runs left. It tags the local `cpp_services-*` images as `oly-local/oly-*:<version>` and runs `test --docker` in both games, `test --docker --platform windows` in the native one (MinGW and Wine), and `test --host` of the native game again, so the host and Docker build directories are shown to live side by side.

To run a game task from a checkout, enter through the launcher with `OLY_ENGINE_DIR` set ([Build with an engine checkout](../../how-to/build-with-an-engine-checkout.md)). The launcher makes a relative `OLY_ENGINE_DIR` absolute first.

## The release tasks

Nothing here publishes anything: the GitHub Actions workflow `release.yml` does that, and only after the same checks have passed on Linux and on Windows. `release.yml` calls these tasks. The steps are in [Cut a release](../../how-to/contributor/cut-a-release.md).

```sh
./spur release 0.2.0                  # on the host: writes 0.2.0 into project(Oly VERSION), @oly/core, @oly/web-host and package-lock.json, and commits; no tag
./spur release-test                   # the shell tests of tools/release (version scripts, and the archive checks, which need ./spur stage of the version in CMakeLists.txt first)
./spur stage 0.1.0                    # on the host: builds both oly-schema, packs the npm tarballs and writes build/release/0.1.0/
./spur stage-npm <folder>             # only the npm pack of the two web packages
./spur staged-test 0.1.0              # web, native, then docker, against build/release/0.1.0
./spur staged-test 0.1.0 web          # in the wasm container; native: in the dev container; docker: on the host
```

- `./spur release` refuses a working tree with changes. A version that is not `MAJOR.MINOR.PATCH` exits with 64. If npm or the check fails after the files were rewritten, `git checkout .` drops the changes.
- `./spur stage <version>` fails unless the tree already carries `<version>` (`tools/release/check_version.sh`). `build/release/<version>/` has GitHub's layout: `download/v<version>/` holds `oly-tools-<version>-linux-x64.tar.gz`, `oly-tools-<version>-windows-x64.tar.gz`, `oly-core-<version>.tgz`, `oly-web-host-<version>.tgz`, `install.sh`, `oly` (the launcher stamped with the version) and `VERSION`; `latest/download/` holds copies of the last three, which is what GitHub serves from the assets of the latest release.
- `staged-test` is the consumer test against the staged tree, not the checkout: it serves `build/release/<version>` on `127.0.0.1:8766`, installs the launcher with `install.sh` into a fresh `OLY_HOME` under `build/staged/<environment>/`, runs `oly doctor`, `oly new` and `oly test`. `OLY_ENGINE_DIR` stays unset, so the tools are the downloaded ones; `OLY_ENGINE_SOURCE_DIR` is this checkout, as the tag does not exist yet. `npm` runs with a user config whose `@oly` registry is `http://127.0.0.1:9`, so a lookup of an `@oly` package in the registry fails the run. `docker` needs the two games `web` and `native` left; it builds `oly-local/oly-<image>:<version>` from the local `cpp_services-*` images plus the staged linux-x64 `oly-tools` (`tools/release/Dockerfile.tools`) and runs `oly test --docker` in both.

## Benchmarks

The benchmark commands are in [Run the benchmarks](../../how-to/contributor/run-benchmarks.md).
