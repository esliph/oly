# Set up the toolchain

[Docs](../../README.md#how-to-guides) · Reference: [Commands](../../reference/contributor/commands.md)

Use this to build the engine and run its tests on a Windows machine. It assumes a checkout of the engine repository. There is no C++ toolchain on the host: everything runs in a container, driven by `./spur <task>` from Git Bash.

## 1. Install Git Bash and Docker

The host needs only these two. Open Git Bash at the repository root for every command below.

## 2. Build the images

```sh
./spur images
```

This builds the images with the versions of `toolchain.env`. Run it once, and again after `toolchain.env` changes. Three things to know:

- A variable of `toolchain.env` exported in the shell (`GCC_VERSION`, `CMAKE_VERSION`) wins over the file and builds a different image: unset it before `./spur images`.
- After `toolchain.env` or the `wasm` image changes, delete `build/schema-tool`: its CMake cache keeps the host compiler of the old image.
- `./spur images` is the only way to build the images. `docker compose build` has no `toolchain.env` and stops naming the missing variable.

## 3. Build and run the tests

```sh
./spur test
```

This configures, builds and runs ctest in the `debug-asan` preset: ASan, UBSan and asserts enabled.
To run less, give a filter to ctest:

```sh
./spur test -R '^ecs\.'           # one module
./spur test -R 'ecs.unit_world'   # one suite
```

## 4. Run the TypeScript and the desktop tests

```sh
./spur games-test          # ECS in WASM, capi and the TS tests
./spur native-games-test   # the native host and the desktop games
```

## 5. Add a test

Use the `add-test-case` skill: [`add-test-case`](../../../.claude/skills/add-test-case/SKILL.md) explains where a doctest suite, a `TEST_CASE` or a `contract_death` case goes and how to prove it runs.

## 6. Check the format

```sh
./spur format-check
```

It fails when a tracked `.cpp` or `.hpp` is not formatted. `./spur format <files>` formats them.

## See also

- [Commands](../../reference/contributor/commands.md): every task, service and preset.
- [Build, tests and tools](../../explanation/architecture/build-and-tests.md): how the build and the tests are organized.
- [Code style](../../reference/contributor/code-style.md).
- [Run the benchmarks](run-benchmarks.md).
