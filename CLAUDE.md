# CLAUDE.md

Oly: game engine in C++20 for 2D games, with 3D transforms in `essentials3d`, on top of a single-threaded archetype ECS (study and benchmark project). The C++ core runs in environments (`web`: WASM with a TS client; `native`: a desktop host), with a C API `oly*` for clients in other languages. Plugins (`oly_add_plugin`) add engine-level features that a game turns on with `oly_add_game(... PLUGINS ...)`; the generated `setUpGame` installs them. Every game also names its `DIMENSION` (`2d|3d`), which turns `essentials2d` or `essentials3d` on (`Position`, `Rotation`, `Scale`, `Parent`, `WorldTransform`). Plugins live in `plugins/`; the engine's own test plugins in `engine/testing/fixture/plugins/`.

## Documentation

Start with `docs/architecture/README.md`. It has the overview and the index of all documentation: architecture, glossary, commands, specs, plans and benchmarks.

## Rules that always apply

- Toolchain only in Docker, through `./spur <task>` from Git Bash (`./spur --list` shows the tasks; `./spur dev ...` for any other native C++ command, `./spur wasm ...` for WASM and TS, `./spur windows ...` for the MinGW cross-build and Wine). There is no C++ compiler on the host.
- Commits, comments and assert messages in English.
- Do not format `.cpp`/`.hpp` by hand: the `.claude/hooks/clang-format.sh` hook formats after each edit (it fails if Docker is stopped).
- Use the glossary names. No aliases or compatibility `using` with old names. Game concepts never in `engine/`; game names only in `engine/testing/fixture/` and `games/`.
- Plugins (`docs/architecture/plugins.md`): `engine/` never depends on `plugins/`; a plugin includes only public headers of the modules and plugins it declares, is its own CMake target with no path relative to the repository root, and its `.oly` reaches `oly-schema` as an argument. Game names never in `plugins/`.
- When changing `table`, `archetype`, `query*`, `command_buffer` or `world`: keep `checkInvariants` covering the change and run the `spec-conformance-reviewer` and `perf-reviewer` agents.
- New test: use the `add-test-case` skill.
- Any change that adds, changes or removes public API (the surfaces listed in `docs/development/guide-style.md`) updates the user guide `docs/guide/` in the same change, following that style.

## Code style

@docs/development/code-style.md
