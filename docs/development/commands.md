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
./spur format <files>             # clang-format the given files; with no argument, every tracked .cpp/.hpp
./spur format-check               # fails when a tracked .cpp/.hpp is not formatted (run by CI)
```

- Build in `build/<preset>/`. Test executables in `build/<preset>/tests/<module>_<suite>`; benchmark executables in `build/<preset>/bench/ecs_bench_*`.
- Fuzz (`fuzz_vs_model`, parts of `command_buffer`): seed and number of operations via `OLY_FUZZ_SEED` and `OLY_FUZZ_OPS` (`engine/runtime/ecs/tests/support/model.hpp`). The seed used is printed to stderr to reproduce a failure.

## Native host and Windows

```sh
./spur native-games-test                   # ctest in the native-games preset: native_host.* and everything else
./spur windows-test                        # cross-compiles with MinGW and runs every test under Wine (preset windows)
./spur windows                             # a shell in the windows container (mingw-w64 and Wine)
```

- `native-games-test` accepts only `native-games*` presets, and `windows-test` only `windows*` ones. The arguments go to ctest.
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

## The release

Nothing here publishes anything: the GitHub Actions workflow `release.yml` does that, and only after the same checks have passed on Linux and on Windows. `release.yml` calls these tasks.

```sh
./spur release 0.2.0                  # on the host: writes 0.2.0 into project(Oly VERSION), @oly/core, @oly/web-host and package-lock.json, and commits; no tag
./spur release-test                   # the shell tests of tools/release (version scripts, and the archive checks, which need ./spur stage of the version in CMakeLists.txt first), in the dev container
./spur stage 0.1.0                    # on the host: builds both oly-schema, packs the npm tarballs and writes build/release/0.1.0/
./spur stage-npm <folder>             # only the npm pack of the two web packages
./spur staged-test 0.1.0              # web, native, then docker, against build/release/0.1.0
./spur staged-test 0.1.0 web          # in the wasm container; native: in the dev container; docker: on the host
```

- `./spur release` refuses a working tree with changes. A version that is not `MAJOR.MINOR.PATCH` exits with 64. If npm or the check fails after the files were rewritten, `git checkout .` drops the changes.
- `./spur stage <version>` fails unless the tree already carries `<version>` (`tools/release/check_version.sh`). `build/release/<version>/` has GitHub's layout: `download/v<version>/` holds `oly-tools-<version>-linux-x64.tar.gz`, `oly-tools-<version>-windows-x64.tar.gz`, `oly-core-<version>.tgz`, `oly-web-host-<version>.tgz`, `install.sh`, `oly` (the launcher stamped with the version) and `VERSION`; `latest/download/` holds copies of the last three, which is what GitHub serves from the assets of the latest release. The `oly-tools` archives are built in the dev container, because Git Bash on Windows cannot give `bin/oly-schema` the executable bit.
- The Linux `oly-schema` (preset `schema-tool-linux`) links statically: a dynamic one needs the glibc of the image it was built in, and the `wasm` image has an older one. The Windows one (`schema-tool-windows`, MinGW) is static through the toolchain file.
- `staged-test` is the consumer test against the staged tree, not the checkout: it serves `build/release/<version>` on `127.0.0.1:8766`, installs the launcher with `install.sh` into a fresh `OLY_HOME` under `build/staged/<environment>/`, runs `oly doctor`, `oly new` and `oly test`. `OLY_ENGINE_DIR` stays unset, so the tools are the downloaded ones; `OLY_ENGINE_SOURCE_DIR` is this checkout, as the tag does not exist yet. `npm` runs with a user config whose `@oly` registry is `http://127.0.0.1:9`, so a lookup of an `@oly` package in the registry fails the run. `docker` needs the two games `web` and `native` left; it builds `oly-local/oly-<image>:<version>` from the local `cpp_services-*` images plus the staged linux-x64 `oly-tools` (`tools/release/Dockerfile.tools`) and runs `oly test --docker` in both.

### Cutting a release

1. On a branch, `./spur release <version>`, then merge the commit into `master`.
2. `./spur stage <version>` then `./spur staged-test <version>` for a check on the laptop (optional).
3. In GitHub: Actions, `release`, `Run workflow` on `master` with `version`. `dry_run` starts true: `check`, `linux` and `windows` run and `publish` is skipped. Run again with `dry_run` false to tag `v<version>` on the tested commit and publish the images (`ghcr.io/esliph/oly-{dev,wasm,windows}:<version>`) and the release assets.

The first publish of each image creates a GHCR package that is private until its visibility is set to public (package settings on GitHub): do that once, or games cannot pull it without a token. The `check` job fails when the dispatch is not on `master` or the tag exists; `publish` pushes the images the `linux` job tested and saved (it fails before any push when a loaded image ID differs).

`release.yml` runs one release at a time (`concurrency`). When `publish` fails after pushing the images, use "Re-run failed jobs": the tag and the release are created only if missing (an existing release gets its assets replaced). A patch of an older line is not made the "latest" release, so `latest/download` keeps serving the newest version.

## ECS benchmarks

```sh
PRESET=release ./spur build
./spur dev ./build/release/bench/ecs_bench_iterate --benchmark_repetitions=10 --benchmark_report_aggregates_only=true --benchmark_out=docs/benchmarks/ecs/<file>.json
./spur dev python3 -I tools/bench_ratio.py --json docs/benchmarks/ecs/<file>.json --name <bench> --baseline <bench> --max <ratio>
```

- Executables: `ecs_bench_iterate`, `ecs_bench_transitions`, `ecs_bench_fragmented`, `ecs_bench_flush`.
- `tools/perf_fragmented.sh` runs `perf stat` in the `perf` service (privileged).
- Always compare ratios from the same run; the WSL2 environment is noisy ([../architecture/performance.md](../architecture/performance.md)).

## WASM and TypeScript

```sh
./spur schema-tool        # builds oly-schema for the machine (preset schema-tool)
./spur ts                 # builds oly-schema, configures $PRESET (default wasm-games), writes each test game's TS schema from its CMake target (oly_typescript_schemas) and the test-only schemas, and compiles the TS
./spur games-test         # ts + ctest in the wasm-games preset (ECS in WASM, capi, TS tests)
PRESET=wasm-games-release ./spur games-test -R '^ts\.'   # Release: only the TS validations catch the errors
./spur wasm node engine/testing/dist/bench/boundary.js --module build/wasm-games-release/games/fixture_game.mjs --out docs/benchmarks/bridge/<file>.json --commit $(git rev-parse --short HEAD)
```

Caveats:

- The `wasm-games*` presets require `./spur ts` (or `./spur schema-tool`) before configuring: `oly_add_game` runs `build/schema-tool/bin/oly-schema`.
- `./spur games-test` only accepts `wasm-games*` presets. Without `PRESET`, it uses `wasm-games`.
