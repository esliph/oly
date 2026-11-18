# Commands

There is no C++ toolchain on the Windows host: everything runs in a container via Docker Compose, driven by `./spur <task>` from Git Bash. The host needs Git Bash and Docker. The services are described in [../architecture/build-and-tests.md](../architecture/build-and-tests.md#toolchain).

`./spur --list` shows the tasks, and `./spur --describe <task>` the help of one. Each task runs in the container its preset needs (`dev`: gcc + cmake + perf; `wasm`: emsdk + Clang + node + Python; `windows`: mingw-w64 + Wine); typed inside the wrong container, it exits with 64.

- Run `./spur images` once, and again after `toolchain.env` changes: it builds the images with the versions of that file. Images built before `./spur` existed lack `OLY_TOOLCHAIN` and stop with exit 69.
- A variable of `toolchain.env` exported in the shell (`GCC_VERSION`, `CMAKE_VERSION`) wins over the file and builds a different image: unset it before `./spur images`.
- After `toolchain.env` or the `wasm` image changes, delete `build/schema-tool`: its CMake cache keeps the host compiler of the old image.
- `-x` and `-n` given on the host show only the dispatch to the container. To trace a task, run it inside: `./spur dev ./spur -x test`.

## Native C++

```sh
./spur test                       # configure + build + ctest in the debug-asan preset (ASan+UBSan, asserts enabled)
./spur test -R '^ecs\.'           # one module (the arguments go to ctest)
./spur test -R 'ecs.unit_world'   # one suite
./spur test -R 'contract_death'   # all death cases
PRESET=release ./spur build       # builds tests + benchmarks in Release, asserts disabled
./spur dev ./build/debug-asan/tests/ecs_unit_world -tc="case name"   # one specific doctest TEST_CASE
./spur dev                        # a shell in the dev container
```

- Build in `build/<preset>/`. Test executables in `build/<preset>/tests/<module>_<suite>`; benchmark executables in `build/<preset>/bench/ecs_bench_*`.
- Fuzz (`fuzz_vs_model`, parts of `command_buffer`): seed and number of operations via `OLY_FUZZ_SEED` and `OLY_FUZZ_OPS` (`engine/runtime/ecs/tests/support/model.hpp`). The seed used is printed to stderr to reproduce a failure.

## Desktop games and Windows

```sh
./spur native-games-test                   # ctest in the native-games preset: native_host.*, drift.desktop and everything else
./spur desktop drift                       # builds native-games-release and prints build/native-games-release/games/drift_game
./spur windows-test                        # cross-compiles with MinGW and runs every test under Wine (preset windows)
PRESET=windows-release ./spur desktop drift   # build/windows-release/games/drift_game.exe, static
./spur windows                             # a shell in the windows container (mingw-w64 and Wine)
./spur windows x86_64-w64-mingw32-objdump -p build/windows-release/games/drift_game.exe
```

- `native-games-test` accepts only `native-games*` presets, and `windows-test` only `windows*` ones. The arguments go to ctest.
- A desktop executable opens a window, so run it on the host, outside the container. `drift_game --frames 600` runs headless with `SDL_VIDEO_DRIVER=dummy`.
- The first configure of a `native-games*` or `windows*` preset fetches SDL3.

## The engine as a subproject

```sh
./spur subproject-test          # builds cmake/tests/consumer native, then web, and runs both
./spur subproject-test native   # only the native one, in the dev container
./spur subproject-test web      # only the web one, in the wasm container
```

`cmake/tests/consumer` is a CMake project outside the engine tree that fetches it with `FetchContent` (here from this checkout, through `FETCHCONTENT_SOURCE_DIR_OLY`) and builds one game with `oly_add_game`. Each run must print `consumer ok`.

## The game tasks and the consumer test

```sh
./spur oly-tools-test             # the shell tests of tools/oly/ (launcher, install.sh, new, build arguments, doctor, Docker dispatch, npm pack), in the wasm container
./spur consumer-test              # native, web, then docker
./spur consumer-test native       # in the dev container
./spur consumer-test web          # in the wasm container
./spur consumer-test docker       # on the host
```

`consumer-test` creates a game through the launcher (`sh tools/oly/oly new consumer-game --version <version>`) in `build/consumer/<environment>/`, with `OLY_ENGINE_DIR` set to this checkout (local mode), then builds and tests it as a user of `oly` would. `web` first packs `@oly/core` and `@oly/web-host` into `build/consumer/release/download/v<version>/` and serves them on `127.0.0.1:8765`, which the game installs from through `OLY_RELEASE_URL`; it then checks that going back to the pinned version replaces the checkout's packages in `node_modules`. `docker` needs the games the two earlier runs left. It tags the local `cpp_services-*` images as `oly-local/oly-*:<version>` and runs `test --docker` in both games, `test --docker --platform windows` in the native one (MinGW and Wine), and `test --host` of the native game again, so the host and Docker build directories are shown to live side by side.

To run a game task from a checkout, enter through the launcher with `OLY_ENGINE_DIR` set: `OLY_ENGINE_DIR=/path/to/oly sh tools/oly/oly build`, run in the game folder. The launcher skips the download, uses `tools/oly/Spurfile` and `spur` of the checkout, and makes a relative `OLY_ENGINE_DIR` absolute first. Without `OLY_ENGINE_DIR` it installs `~/.oly/versions/<V>/` for the version `oly.env` pins; `OLY_HOME` and `OLY_RELEASE_URL` move the install root and the release server (a folder served over HTTP or `file://` with GitHub's `download/v<V>/` and `latest/download/` layout works).

## ECS benchmarks

```sh
PRESET=release ./spur build
./spur dev ./build/release/bench/ecs_bench_iterate --benchmark_repetitions=10 --benchmark_report_aggregates_only=true --benchmark_out=docs/benchmarks/ecs/<file>.json
./spur dev python3 -I tools/bench_ratio.py --json docs/benchmarks/ecs/<file>.json --name <bench> --baseline <bench> --max <ratio>
```

- Executables: `ecs_bench_iterate`, `ecs_bench_transitions`, `ecs_bench_fragmented`, `ecs_bench_flush`.
- `tools/perf_fragmented.sh` runs `perf stat` in the `perf` service (privileged).
- Always compare ratios from the same run; the WSL2 environment is noisy ([../architecture/performance.md](../architecture/performance.md)).

## WASM, TypeScript and games

```sh
./spur schema-tool        # builds oly-schema for the machine (preset schema-tool)
./spur ts                 # builds oly-schema, configures $PRESET (default wasm-games), writes each game's TS schema from its CMake target (oly_typescript_schemas) and the test-only schemas, and compiles the TS
./spur games-test         # ts + ctest in the wasm-games preset (ECS in WASM, capi, TS tests)
./spur game arena         # assembles build/wasm-games-release/web/arena/ (index.html, esbuild's main.js, arena_game.mjs/.wasm)
PRESET=wasm-games-release ./spur games-test -R '^ts\.'   # Release: only the TS validations catch the errors
./spur wasm node engine/testing/dist/bench/boundary.js --module build/wasm-games-release/games/fixture_game.mjs --out docs/benchmarks/bridge/<file>.json --commit $(git rev-parse --short HEAD)
./spur serve arena        # http://localhost:8000
```

Caveats:

- The `wasm-games*` presets require `./spur ts` (or `./spur schema-tool`) before configuring: `oly_add_game` runs `build/schema-tool/bin/oly-schema`.
- `./spur games-test`, `game` and `serve` only accept `wasm-games*` presets. Without `PRESET`, `games-test` uses `wasm-games` and `game` and `serve` use `wasm-games-release`.
- `./spur serve` needs an open stdin (interactive terminal): esbuild exits when stdin closes.
