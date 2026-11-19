# Boundary rules

[Reference](../../README.md#reference) · Concept: [Modules and boundaries](../../explanation/architecture/modules-and-boundaries.md)

The `oly.lint.boundaries` test runs `tools/check_boundaries.py` over the repository. Its self-test is `oly.lint.boundaries_selftest`. CMake registers both only when the engine is the top-level project, Python 3 is found, and the build is not Emscripten or has `OLY_BUILD_GAMES`.

The lint reads a manifest that CMake writes to `oly_boundaries.json`: the modules, plugins and games with their directories and dependencies. It scans `.hpp`, `.cpp`, `.h`, `.c` and `.inl` files as C++ and `.ts` files as TypeScript, and skips the folders `build`, `node_modules`, `.git`, `.superpowers`, `dist`, `dist-tests` and `.claude`. It prints one line per violation as `<file>:<line>: <reason>` and exits with 1; with no violation it prints nothing and exits with 0.

Module and plugin names in an include (`oly/<x>/`) may contain digits: `<x>` matches `[a-z0-9_]+`.

## C++ rules

| Rule | Reason printed |
|---|---|
| A module, plugin or game includes `oly/<x>/...` and did not declare `<x>` in `DEPENDS` (or `PLUGINS`). | `<owner> includes oly/<x> without declaring <x> in DEPENDS` |
| `oly/<x>/detail/` is included outside the directory of module or plugin `<x>`. Its own tests and benchmarks may include it. | `oly/<x>/detail is internal to module <x>` |
| A file in `engine/` includes a plugin whose directory is under `plugins/`. The test plugins in `engine/testing/fixture/plugins/` are engine code. | `engine cannot depend on plugin <x>` |
| A file in `engine/runtime/` or `engine/schema/` includes a module of `engine/environments/`. | `engine/runtime cannot depend on environment <e> (oly/<x>)` or `engine/schema cannot depend on environment <e> (oly/<x>)` |
| A file of one environment includes a module of another. | `environment <a> cannot depend on environment <b> (oly/<x>)` |
| `__EMSCRIPTEN__`, `EM_*`, `EMSCRIPTEN_*` or `<emscripten.h>` outside `engine/environments/web/` and the `OLY_API` line of `c_api.h`. A file under `tests/` or `bench/` may use `__EMSCRIPTEN__` alone, to skip a case on WASM. | `Emscripten (<match>) is allowed only in engine/environments/web/ and OLY_API` |
| `oly_add_engine_test_game` in a `CMakeLists.txt` outside `engine/`. | `oly_add_engine_test_game is only for engine/` |

## TypeScript rules

| Rule | Reason printed |
|---|---|
| `@oly/core/internal` imported outside `engine/`. | `@oly/core/internal can only be used inside engine/` |
| `@oly/core/generated` imported outside `*.generated.ts` and `@oly/core`. | `@oly/core/generated can only be imported by generated code (*.generated.ts)` |
| `@oly/web-host` imported in `@oly/core`. | `@oly/core cannot depend on @oly/web-host` |
| A relative import that leaves its package. | `relative import crosses packages; use the package name` |
| A package of `engine/runtime/` or `engine/schema/`, or of another environment, imported from the wrong side (the same environment rules as in C++, with the package name). | the environment reasons above, with `@oly/<name>` in place of `oly/<x>` |

## Game names in plugins

The lint builds the list of game names from the folder names under `games/` at the repository root, and rejects a name, as a whole word in any identifier style, in the path or the contents of any file under `plugins/`. The reason is `game name <name> in plugins/`. This repository has no `games/` folder (a game lives in its own repository), so the list is empty and the rule reports nothing until a `games/` folder appears.

## See also

- [Modules and boundaries](../../explanation/architecture/modules-and-boundaries.md): why the boundaries exist and the other two mechanisms that enforce them.
- [Code style](code-style.md).
