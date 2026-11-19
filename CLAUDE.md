# CLAUDE.md

Oly: game engine in C++20 for 2D games, with 3D transforms in `essentials3d`, on top of a single-threaded archetype ECS (study and benchmark project, published for the author's own games: `docs/explanation/architecture/publishing.md`). The C++ core runs in environments (`web`: WASM with a TS client; `native`: a desktop host), with a C API `oly*` for clients in other languages. Plugins (`oly_add_plugin`) add engine-level features that a game turns on with `oly_add_game(... PLUGINS ...)`; the generated `setUpGame` installs them. Every game also names its `DIMENSION` (`2d|3d`), which turns `essentials2d` or `essentials3d` on (`Position`, `Rotation`, `Scale`, `Parent`, `WorldTransform`). Plugins live in `plugins/`; the engine's own test plugins in `engine/testing/fixture/plugins/`. A game in its own repository uses `oly` (`tools/oly/`: the launcher and the game tasks; `templates/game/`: what `oly new` copies), and `toolchain.env` holds the toolchain versions.

## Documentation

Start with `docs/README.md`. It is the index of all documentation: tutorials, how-to guides, reference and explanation.

## Rules that always apply

- Toolchain only in Docker, through `./spur <task>` from Git Bash (`./spur images` builds the images, never `docker compose build`; `./spur --list` shows the tasks; `./spur dev ...` for any other native C++ command, `./spur wasm ...` for WASM and TS, `./spur windows ...` for the MinGW cross-build and Wine). There is no C++ compiler on the host.
- Use the glossary names. No aliases or compatibility `using` with old names. Game concepts never in `engine/`; game names only in `engine/testing/fixture/`.
- Plugins (`docs/explanation/architecture/plugins.md`): `engine/` never depends on `plugins/`; a plugin includes only public headers of the modules and plugins it declares, is its own CMake target with no path relative to the repository root, and its `.oly` reaches `oly-schema` as an argument. Game names never in `plugins/`.
- When changing `table`, `archetype`, `query*`, `command_buffer` or `world`: keep `checkInvariants` covering the change and run the `spec-conformance-reviewer` and `perf-reviewer` agents.
- New test: use the `add-test-case` skill.
- Any change that adds, changes or removes public API (the public surface of `docs/reference/contributor/docs-style.md`) updates `docs/` in the same change, following that page.

## Code style

@docs/reference/contributor/code-style.md
