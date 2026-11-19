# Contributing to Oly

Thank you for your interest. Oly is a study and benchmark project, published for the author's own games: every data representation decision has a measured cost, and contributions are judged by that same rule. This page says what is welcome, how to set up the toolchain and what a pull request needs to be merged.

By taking part you agree to follow the [code of conduct](CODE_OF_CONDUCT.md). Security problems are never reported in a public issue: see [SECURITY.md](SECURITY.md).

## What is welcome

| Kind | How |
|---|---|
| Bug fix | Open a pull request directly. If the fix is not small, open an issue first with the reproduction. |
| Test, benchmark or documentation | Open a pull request directly. |
| New feature, new plugin, change to a public API | **Open an issue first** and wait until it is accepted. A pull request for a feature that was not discussed may be closed, however good it is, because it may not fit the scope ([limits.md](docs/reference/limits.md)) or the design ([decisions.md](docs/explanation/architecture/decisions.md)). |
| Performance change | Open an issue with the measurement that motivates it. A performance change is merged on numbers, not on reasoning. |

Questions about using the engine go in an issue too.

## Before you start

1. Read the [architecture overview](docs/architecture/README.md). It explains the parts, the boundaries between them and the index of every other document.
2. Use the names of the [glossary](docs/reference/glossary.md). No aliases, and no compatibility `using` with old names.
3. The specs in [`docs/superpowers/specs/`](docs/superpowers/specs) are the source of truth for the requirements (`R-*`) and invariants (`I-*`). A change that breaks one is a change to the spec, and is discussed in an issue first.

## Toolchain

The host needs only **Git Bash** (or any POSIX shell on Linux and macOS) and **Docker** with Docker Compose. The compilers, CMake, Emscripten and Node live in the images, with the versions of `toolchain.env`. Never install a compiler on the host to build the engine.

```sh
./spur images          # once, and again after toolchain.env changes
./spur --list          # every task
./spur test            # native C++: build and every test with ASan + UBSan
./spur games-test      # WASM + TypeScript packages + integration tests
```

Every command is in [docs/development/commands.md](docs/development/commands.md), and how the build and the tests are organized is in [build-and-tests.md](docs/architecture/build-and-tests.md).

## Workflow

1. Fork the repository and create a branch from **`develop`**. `master` only receives releases.
2. Make the change, with its tests and documentation (below).
3. Run the checks of [the pull request checklist](#pull-request-checklist) locally.
4. Open the pull request against `develop`. CI (`.github/workflows/ci.yml`) builds the `dev` and `wasm` images and runs `./spur format-check`, `./spur test` and `./spur games-test`. The Windows cross-build, the subproject and the consumer tests run in the release workflow; when a change touches them (`cmake/`, `tools/`, `templates/`, the native host), run them locally too ([commands](docs/development/commands.md)).

Keep one change per pull request. A refactoring that a fix needs goes in its own commit, before the fix.

## Code rules

The full rules are in [docs/reference/contributor/code-style.md](docs/reference/contributor/code-style.md). The ones a review asks about most:

- Commits, comments and assert messages in **English**.
- `-Wall -Wextra -Wpedantic -Werror` on every target: a warning breaks the build. TypeScript is `strict`.
- Formatting is `.clang-format`, applied by the `dev` image's clang-format. Do not format by hand: `./spur format <files>` formats the `.cpp` and `.hpp` files you changed (with no argument, every tracked one), and `./spur format-check` is the check CI runs.
- Comments only for the why or for a non-obvious contract. Nothing that repeats the code.
- `OLY_ASSERT` messages name the operation, the `Entity` and the type.
- Boundaries ([modules-and-boundaries.md](docs/architecture/modules-and-boundaries.md), [plugins.md](docs/architecture/plugins.md)): `engine/` never depends on `plugins/`; game concepts never go in `engine/`; game names only in `engine/testing/fixture/`. The boundary lint runs with the tests.

## Tests

Every behaviour change comes with a test. New suites, new cases and contract violations (`contract_death`) each have a place and a registration: [`.claude/skills/add-test-case/SKILL.md`](.claude/skills/add-test-case/SKILL.md) describes the three cases step by step, and is readable without Claude Code.

A change to `table`, `archetype`, `query*`, `command_buffer` or `world` keeps `checkInvariants` covering what it changes.

## Performance

The performance targets are ratios against a baseline measured in the same run ([docs/reference/benchmarks/README.md](docs/reference/benchmarks/README.md)). A change on a hot path (the ECS iteration, transitions, the `CommandBuffer` flush, the client boundary) states in the pull request the benchmark it ran before and after, on the same machine, in the `release` preset:

```sh
PRESET=release ./spur build
./spur dev ./build/release/bench/ecs_bench_iterate
```

Numbers from different machines or different runs are not compared.

## Documentation

- A change that adds, changes or removes public API (the surfaces listed in [docs/development/guide-style.md](docs/development/guide-style.md)) updates the user guide in [`docs/guide/`](docs/guide/README.md) in the same pull request, following that style.
- A change to how a part works updates its page in [`docs/architecture/`](docs/architecture/README.md). Those pages describe the engine as it is today, not its history.

## Commits

The history uses [Conventional Commits](https://www.conventionalcommits.org/): `type(scope): subject`, with types such as `feat`, `fix`, `test`, `docs`, `build`, `ci` and `refactor`, and the module or area as the scope (`ecs`, `app`, `capi`, `oly`, `release`). The subject is short, in the imperative, and says what changed, not which issue or plan it came from. The body, when needed, says why.

```
fix(ecs): keep the query cache when an archetype is created during flush
```

## Pull request checklist

- [ ] The branch starts from `develop` and the pull request targets `develop`.
- [ ] A feature or public API change has an accepted issue, linked in the description.
- [ ] `./spur test` and `./spur games-test` pass locally.
- [ ] `./spur format-check` passes.
- [ ] Each behaviour change has a test.
- [ ] A public API change updates `docs/guide/`; a change of design updates `docs/architecture/`.
- [ ] A hot path change reports the benchmark before and after.

## Working with Claude Code

The repository carries a [Claude Code](https://claude.com/claude-code) setup in `.claude/` and `CLAUDE.md`: a hook that formats each edited C++ file in the `dev` container, the project skills (`add-test-case`, `bench-compare`) and two review agents (`spec-conformance-reviewer`, `perf-reviewer`). It is optional. The rules it enforces are the ones on this page, and every pull request is held to them whatever tool wrote it.

## License

Oly is distributed under the [MIT license](LICENSE). By opening a pull request you agree that your contribution is licensed under the same terms.
