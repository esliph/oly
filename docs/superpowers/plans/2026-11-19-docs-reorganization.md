# Documentation reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize `docs/` by Diátaxis type (tutorials, how-to, reference, explanation), close the audit's verified findings, and
rewrite the root README for a first-time reader, with a link checker and a README example check that keep it that way.

**Architecture:** Type at the top, audience as a subfolder (`contributor/`, `explanation/architecture/`). Each concept page splits into
explanation (the contract), a C++ reference and a TS reference (the API), and how-to (the patterns). Files move with `git mv` first, then
split theme by theme; every commit leaves the link checker at 0 broken and no stale mention of a moved path.

**Tech Stack:** Markdown (GitHub rendering), Python 3 (`unittest`, standard library only), CMake/ctest, `./spur` (Docker).

**Spec:** `docs/superpowers/specs/2026-11-18-docs-reorganization-design.md`. Findings are cited from
`docs/superpowers/specs/2026-11-18-docs-audit.md` as **A1–A10**, **D1–D18**, **§3.2**, **§3.3**, **§4**, **§5.1**. The spec's §7 tables
map every section of every current page to its destination by line number: each task below names the §7 rows it executes and does not
repeat them.

## Global Constraints

- English, simple words, active voice, glossary names only (`CLAUDE.md`). New terms enter `reference/glossary.md` first.
- Game-agnostic examples: `<game>`, `Player`, `Position` (`guide-style.md:51-56`). No game names anywhere in `docs/`.
- Every command, message and signature quoted in the docs is copied from the code, not paraphrased. Before writing one, open the header,
  `.ts`, `.h`, `.cmake`, `Spurfile` or template file it comes from.
- Each commit leaves the link checker at 0 broken and the stale-path check (below) silent outside `docs/superpowers/`.
- `docs/superpowers/` stays at its path. Its specs and plans are not rewritten; only the two broken links of §5 change (Task 1) and a new
  `docs/superpowers/README.md` is added (Task 9).
- The toolchain runs only through `./spur` (`CLAUDE.md`); `./spur dev <command>` for Python.
- No code changes (spec §2 non-goals): no move into `detail/`, no fix of `engine_api.h:53-55`, no change to `release.yml`.
- Commit subjects follow the repository's style (`docs: <imperative, lowercase>`), with no audit or task ids in the subject. Every commit
  ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- No temporary worktree, clone or scratch file is needed: past changes are read with `git show <commit> -- docs/` (Task 3).

### Verification protocol (every task refers to it as V1–V3)

**V1, links.** `./spur dev python3 -I tools/check_links.py --root .` prints nothing and exits 0. (From Task 1 on.)

**V2, stale paths.** With the step staged and before `git commit`, run in Git Bash from the repository root:

```bash
stale_paths() {
  local old relative

  for old in $(git diff --cached -M --name-status --diff-filter=RD | awk '{print $2}' | grep -E '\.md$') "$@"; do
    relative=${old#docs/}
    git grep -nE "(^|[^/[:alnum:]_.-])(docs/)?${relative//./\\.}" -- ':!docs/superpowers'
  done
}
stale_paths <old directories moved in this step, if any, e.g. docs/benchmarks/>
```

It prints nothing. The leading class keeps `explanation/architecture/ecs.md` from matching the old `architecture/ecs.md`; relative
markdown links (`../architecture/ecs.md`) are V1's job.

**V3, lint tests** (tasks that touch `tools/` or `CMakeLists.txt`): `./spur test -R '^oly\.lint\.'` passes every test.

### Page contracts (interim rules; Task 11 writes them into `reference/contributor/docs-style.md`)

**`docs/README.md`** has exactly these sections, in this order, which other pages link by anchor: `## Tutorials` (`#tutorials`),
`## How-to guides` (`#how-to-guides`), `## Reference` (`#reference`), `## Explanation` (`#explanation`), `## Working on the engine`
(`#working-on-the-engine`), `## Conventions` (`#conventions`). Each section is a table `| Page | What it answers |`. Every task that
creates or deletes a page updates its row.

**Explanation concept page** (`docs/explanation/concepts/<theme>.md`): prose. A short C++ excerpt appears only where the contract has to be
seen, followed by one line `TypeScript: [<section>](../../reference/typescript/<theme>.md#<anchor>)`. Order of the chain:
environments-and-hosts → schema → world-and-entities → queries → engine-and-systems → events → input → schedule → states → performance →
plugins → transforms → web-boundary.

~~~markdown
# World and entities

[← Schema](schema.md) · [Concepts](README.md) · Next: [Queries →](queries.md)

...

## API

- C++: [World and entities in C++](../../reference/cpp/world-and-entities.md)
- TypeScript: [World and entities in TypeScript](../../reference/typescript/world-and-entities.md)

## Next

[Queries](queries.md): <what the next page teaches, one line>.
~~~

A theme with no TS form drops the TypeScript line; `performance` replaces `## API` with a link to `how-to/keep-the-hot-path-fast.md`.
While a neighbour is not split yet, the navigation link points to where it lives now.

**Reference page per language** (`docs/reference/cpp/<theme>.md`, `docs/reference/typescript/<theme>.md`): the same file names and the same
`##` section titles in both languages; a section with no form in one language is left out, not stubbed. No previous/next links.

~~~markdown
# World and entities in C++

[Reference](../../README.md#reference) · Concept: [World and entities](../../explanation/concepts/world-and-entities.md) · TypeScript: [World and entities in TypeScript](../typescript/world-and-entities.md)
~~~

The TS page's line is `[TypeScript client](README.md) · Concept: [...](../../explanation/concepts/<theme>.md) · C++: [... in C++](../cpp/<theme>.md)`.
Each entry is `### \`name\``, then the signature in a code block, then one paragraph: what it does, preconditions, the error and its
message, then an example only when correct use is not obvious. Contract rules are not repeated: one line links to the explanation section.

**How-to page**: `# <task, as a verb phrase>`; the first paragraph says the goal and what it assumes; then the steps; it ends with
`## See also` (explanation and reference links).

**Tutorial**: `# Your first <web|desktop> game`; "What you will build", "Before you start", then numbered `## N. <step>` sections, each
ending with what the reader sees; it ends with `## Next steps`. Every code block is the template file (or a marked excerpt of it).

## Review Focus

1. **Heading slugs with symbols.** Headings such as ``## `oly new` ``, `## Systems in C++`, `## Menu → playing → game over`,
   `## Diátaxis types` must slug exactly as GitHub does (`oly-new`, `systems-in-c`, `menu--playing--game-over`, `diátaxis-types`).
   Test: `test_slug_rules` (Task 1).
2. **A link whose case differs from the file.** It resolves on Windows and on the NTFS bind mount, then 404s on GitHub. Test:
   `test_case_mismatch_fails` (Task 1).
3. **Code in other fence forms.** `~~~`, four or more backticks, fences indented inside list items, an unclosed fence, double-backtick
   inline code: links and headings inside them are not links or anchors. Test: `test_code_is_ignored` (Task 1).
4. **Link forms beyond `[t](path)`.** A title (`"..."`), angle brackets, `%20`, images, reference definitions (`[ref]: path`) and directory
   targets (`plugins/`). Tests: `test_link_forms`, `test_directory_link_passes` (Task 1).
5. **CRLF and BOM in a Windows checkout** (`.gitattributes` pins LF only for `*.sh`, `*.oly`, `*.env`, `spur`, `Spurfile`): anchors and the
   README-to-template comparison must not depend on line endings. Tests: `test_crlf_and_bom` (Task 1), `test_crlf_template_matches`
   (Task 12).

---

### Task 1: Link checker, the three broken links, and its ctest registration

Spec §8 and §10 step 1. The checker lands first, not at the end of step 1: the audit's Node script is not in the repository, so it is the
only V1 available; the three broken links are fixed in the same commit, so it registers with no exclusion list, as §8 requires.

**Files:**
- Create: `tools/check_links.py`, `tools/test_check_links.py`
- Modify: `CMakeLists.txt:104-111` (inside `if(Python3_FOUND)`, next to `oly.lint.boundaries`)
- Modify: `docs/guide/environments/web/boundary.md:57` (A9), `docs/superpowers/plans/2026-11-13-multi-environment-phase3-schema.md:1953`,
  `docs/superpowers/plans/2026-11-14-multi-environment-phase5-docs.md:221`

**Interfaces:**
- Produces: `python3 -I tools/check_links.py --root <repo>`: prints `<file>:<line> | <target> | <reason>` per broken link, sorted by file
  then line, `<file>` relative to the root with `/`; `<reason>` is `file not found` or `anchor not found`; exits 1 if any, else prints
  nothing and exits 0. Module functions: `slugify(heading: str) -> str`, `anchors_of(text: str) -> set[str]`,
  `links_of(text: str) -> list[tuple[int, str]]`, `check(root: str) -> list[str]`, `main(argv: list[str]) -> int`, and
  `iter_lines(text: str) -> Iterator[tuple[int, str, bool]]` (1-based line number, the line, whether it is inside a fence or is a fence
  line), which Task 12 reuses.
- Produces: ctest tests `oly.lint.links`, `oly.lint.links_selftest`.

- [ ] **Step 1: Pre-flight.** `git status --short --untracked-files=all -- '*.md'` prints nothing: the checker walks the working tree, so
  an untracked `.md` would be checked locally and not in CI. (The two root drafts `docs-audit.md` and `docs-plan.md` were deleted on
  2026-11-19, at the user's request.) If one appears, ask the user before deleting it.

- [ ] **Step 2: Write the failing tests** in `tools/test_check_links.py`, following `tools/test_check_boundaries.py` (a temporary root per
  test, a `write(relativePath, content)` helper, `main` called with `--root`, stdout captured, `unittest.main()` at the bottom):

```python
def run_check(self):
    output = io.StringIO()

    with contextlib.redirect_stdout(output):
        code = check_links.main(["--root", self.root])

    return code, output.getvalue()

def test_valid_relative_link_passes(self):
    self.write("a.md", "[b](b.md)\n")
    self.write("b.md", "# B\n")

    code, output = self.run_check()

    self.assertEqual((code, output), (0, ""))

def test_missing_file_fails(self):
    self.write("a.md", "text\n[b](missing.md)\n")

    code, output = self.run_check()

    self.assertEqual((code, output), (1, "a.md:2 | missing.md | file not found\n"))
```

The other tests, each with its assertion:

| Test | Input | Assertion |
|---|---|---|
| `test_missing_anchor_fails` | `b.md` = `# Title`; `a.md` = `[b](b.md#other)` | output `a.md:1 \| b.md#other \| anchor not found\n`, code 1 |
| `test_same_file_anchor` | `a.md` = `# Title\n[x](#title)\n[y](#nope)\n` | output is only the `#nope` line |
| `test_slug_rules` | `slugify` | `"Systems in C++"`→`systems-in-c`; ``"`oly new`"``→`oly-new`; `"Menu → playing → game over"`→`menu--playing--game-over`; `"Diátaxis types"`→`diátaxis-types`; `"WASM and TypeScript"`→`wasm-and-typescript`; `"[Link](x.md) text"`→`link-text`; `"snake_case name"`→`snake_case-name`; `"**Bold** part"`→`bold-part` |
| `test_duplicate_headings_get_suffix` | two `## Next` | `anchors_of` contains `next` and `next-1` |
| `test_html_anchor` | `<a name="custom"></a>` and `<a id="other"></a>` | both in `anchors_of` |
| `test_setext_heading` | `Title\n=====\n` and `Sub\n---\n` after a paragraph line | `title`, `sub` in `anchors_of`; a `---` after a blank line adds nothing |
| `test_code_is_ignored` | broken links inside a ```` ``` ```` fence, a `~~~` fence, a ```` ```` ```` fence holding ```` ``` ````, a fence indented 4 spaces in a list item, `` `[x](m.md)` `` and ``` ``[x](m.md)`` ```; a `# Fake` inside a fence and a link to `#fake` outside | only the `#fake` link is reported |
| `test_unclosed_fence_runs_to_end` | a fence never closed, then `[x](missing.md)` | code 0 (GitHub renders the rest as code) |
| `test_link_forms` | `[t](b.md "Title")`, `[t](<b c.md>)`, `[t](b%20c.md)`, `![i](pic.png)`, `[ref]: b.md`, each target existing; then the same five pointing at missing files | first five pass; the second five are reported with their raw target |
| `test_directory_link_passes` | `[d](sub/)` and `[d](sub)` with `sub/` existing | code 0 |
| `test_case_mismatch_fails` | file `Guide.md`; link `[g](guide.md)` | `file not found` (compare each path component with `os.listdir`) |
| `test_external_and_scheme_links_skipped` | `https://x/missing`, `mailto:a@b`, `//host/x` | code 0 |
| `test_non_markdown_anchor_not_checked` | `[x](code.cpp#L10)` with `code.cpp` existing | code 0 |
| `test_root_relative_link` | `[x](/docs/a.md)` from `sub/b.md`, with `docs/a.md` existing | code 0 (GitHub resolves `/` at the repository root) |
| `test_ignored_directories` | broken links in `.git/x.md`, `node_modules/x.md`, `build/x.md`, `build-asan/x.md`, `.superpowers/x.md` | code 0 |
| `test_crlf_and_bom` | `b.md` = BOM + `# Title\r\n`; `a.md` = `[b](b.md#title)\r\n` | code 0 |
| `test_output_sorted` | broken links in `z.md:1`, `a.md:3`, `a.md:1` | lines in that order: `a.md:1`, `a.md:3`, `z.md:1` |

- [ ] **Step 3: Run the tests to verify they fail.**
  Run: `./spur dev python3 -I tools/test_check_links.py`
  Expected: FAIL with `ModuleNotFoundError: No module named 'check_links'`.

- [ ] **Step 4: Implement `tools/check_links.py`.** Module docstring and CLI shape as `tools/check_boundaries.py`. Decisions the tests do
  not pin:
  - Walk with `os.walk`, pruning directories named `.git`, `node_modules`, `.superpowers` or starting with `build`; read every `.md` as
    UTF-8 with `utf-8-sig`, normalize `\r\n` to `\n`.
  - One pass per file tracks fences: an opening line matches `^\s*(`{3,}|~{3,})`; it closes on a line of the same character, at least as
    long, with only whitespace after. Outside fences, remove inline code spans (a run of N backticks to the next run of exactly N) before
    looking for links; keep line numbers.
  - `slugify`: drop markdown (inline code markers, `[text](url)` → `text`, `*`/`_` emphasis markers only when they wrap text, HTML tags),
    lowercase, keep a character when `ch.isalnum()` or it is `-`, `_` or a space, then each space becomes `-` (no collapsing).
  - Targets: strip `<>`; skip a target with a scheme (`^[A-Za-z][A-Za-z0-9+.-]*:`) or starting with `//`; split at the first `#`;
    `urllib.parse.unquote` the path; an empty path is the same file; a leading `/` resolves from the root. Anchors are checked only when
    the target is a `.md` file.

- [ ] **Step 5: Run the tests to verify they pass.**
  Run: `./spur dev python3 -I tools/test_check_links.py`
  Expected: `OK`.

- [ ] **Step 6: Run the checker on the repository.**
  Run: `./spur dev python3 -I tools/check_links.py --root .`
  Expected: exactly three lines, the §5 links: `docs/guide/environments/web/boundary.md:57`, `...phase3-schema.md:1953`,
  `...phase5-docs.md:221`. Any other line is a checker bug or a real broken link: compare it with the audit's coverage (547 links, 3 broken)
  before going on.

- [ ] **Step 7: Fix the three links** (link targets only): `#wasm-typescript-and-games` → `#wasm-and-typescript`;
  `02-schema.md#naming-rules` → `../../../engine/schema/format/README.md#naming-rules-and-limits` (the normative home, which outlives the
  D10 deletion in Task 4); `../../../troubleshooting.md` → `../../guide/troubleshooting.md` (Task 8 retargets it once more, to
  `../../reference/errors.md`). Run V1: no output.

- [ ] **Step 8: Register the tests** in `CMakeLists.txt`, inside the `if(Python3_FOUND)` block after `oly.lint.boundaries_selftest`:
  `oly.lint.links` runs `${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/check_links.py --root ${PROJECT_SOURCE_DIR}`;
  `oly.lint.links_selftest` runs `-I .../tools/test_check_links.py`. Run V3: all `oly.lint.*` pass.

- [ ] **Step 9: Commit.**

```bash
git add tools/check_links.py tools/test_check_links.py CMakeLists.txt docs/guide/environments/web/boundary.md \
  docs/superpowers/plans/2026-11-13-multi-environment-phase3-schema.md docs/superpowers/plans/2026-11-14-multi-environment-phase5-docs.md
git commit -m "test: check the relative links and anchors of every markdown file"
```

---

### Task 2: Skeleton and the files that only move

Spec §10 step 1; §7.2–§7.4 rows with action **move**; §7.5 path updates for the moved files. No text changes except links and paths.

**Files:**
- Create: `docs/README.md` (skeleton)
- Move (`git mv`, content unchanged):

```bash
mkdir -p docs/how-to/typescript docs/reference/typescript docs/reference/contributor docs/explanation/architecture
git mv docs/guide/recipes.md docs/how-to/gameplay-recipes.md
git mv docs/guide/environments/web/testing.md docs/how-to/test-a-web-game.md
git mv docs/guide/environments/web/typescript/recipes.md docs/how-to/typescript/gameplay-recipes.md

for page in README schema world-and-entities queries engine-and-systems events input schedule states; do
  git mv docs/guide/environments/web/typescript/$page.md docs/reference/typescript/$page.md
done

for page in app ecs input schema-and-codegen decisions; do
  git mv docs/architecture/$page.md docs/explanation/architecture/$page.md
done

git mv docs/architecture/glossary.md docs/reference/glossary.md
git mv docs/architecture/limits.md docs/reference/limits.md
git mv docs/architecture/repository.md docs/reference/contributor/repository.md
git mv docs/development/code-style.md docs/reference/contributor/code-style.md
git mv docs/benchmarks docs/reference/benchmarks
```

- Modify (paths of moved files only): `CLAUDE.md:22` (`@docs/reference/contributor/code-style.md`), `CONTRIBUTING.md:21,48,65`,
  `README.md:132,140,142,143`, `.github/ISSUE_TEMPLATE/feature_request.yml:8`, `.claude/agents/perf-reviewer.md:19,73,94,112`,
  `.claude/agents/spec-conformance-reviewer.md:20,27`, `.claude/skills/add-test-case/SKILL.md:130,143`,
  `.claude/skills/bench-compare/SKILL.md:10,20,53,54,71,83,98`, `tools/run_ladder.sh:23`, and every `.md` whose relative links V1 reports.

**Interfaces:**
- Produces: the paths above; `docs/README.md` with the six sections of the page contract.

- [ ] **Step 1: Run the moves** above, then `git status` shows only renames.
- [ ] **Step 2: Write `docs/README.md`.** The six sections, each with the moved pages it holds; until Tasks 8 and 9, `## Reference`,
  `## How-to guides` and `## Explanation` each end with one row pointing to `guide/README.md` (game user) and `architecture/README.md`
  (contributor) for pages not yet moved. `## Conventions` stays empty until Task 8.
- [ ] **Step 3: Fix links** until V1 prints nothing. Change link targets only.
- [ ] **Step 4: Update the plain-text paths** of the Modify list. Run V2 with `stale_paths docs/benchmarks/`: no output.
- [ ] **Step 5: Check the tools.** `bash -n tools/run_ladder.sh`; `git grep -n "docs/benchmarks" -- tools .claude` prints nothing.
- [ ] **Step 6: Commit** (`git add -A docs CLAUDE.md CONTRIBUTING.md README.md .github .claude tools`):
  `docs: move the pages that keep their content into the Diátaxis tree`.

---

### Task 3: Pilot split of `world-and-entities`, and the abandon check

Spec §10 step 2 pilot; §7.1 row `concepts/world-and-entities.md`. Pilot theme: `world-and-entities`, the theme with the most public API
changes in the history; the two replayed changes are `8dfec4c` (observers and implied components: new API) and `bc76c6c` (a flush
behaviour fix).

**Files:**
- Create: `docs/explanation/concepts/README.md` (reading order: the 13 pages of the chain, each linking to where it lives now),
  `docs/explanation/concepts/world-and-entities.md`, `docs/reference/cpp/world-and-entities.md`, `docs/how-to/debug-the-world.md`
- Modify: `docs/reference/typescript/world-and-entities.md` (nav line, section titles mirror the C++ page, contract rules become a link,
  `:23` and `:101`), `docs/README.md`, every page linking to the old path
- Delete: `docs/guide/concepts/world-and-entities.md`

**Interfaces:**
- Consumes: page contracts; `docs/README.md` sections.
- Produces: `docs/explanation/concepts/README.md`; the anchor of the direct × deferred section in the explanation page, which Task 4
  (`engine-and-systems`) links to.

- [ ] **Step 1: Failing checks.** Run:

```bash
git grep -n "isAlive(e); // false" -- docs
for symbol in commandCount isIterating OnRemove '.generation'; do git grep -qF -- "$symbol" docs/reference || echo "MISSING: $symbol"; done
```

  Expected: the first finds `docs/guide/concepts/world-and-entities.md:89`; the loop prints four `MISSING` lines.

- [ ] **Step 2: Write the three pages** per the §7.1 row. A2: the explanation has one section on direct and deferred changes with a table,
  one row per context type that offers `destroy`/`add`/`remove` (read the types from `engine/runtime/app/include/oly/app/*.hpp` and
  `ecs/world.hpp`), columns: direct or deferred; when it takes effect; `isAlive` right after `destroy`; what a dead or stale handle does
  (`World` and direct contexts: `OLY_ASSERT`, `world.hpp:95-97`; `CommandBuffer`: ignored at flush, `world.hpp:128-130`; `SystemContext`
  defers to the `CommandBuffer`, `system.hpp:63-65`). The reference adds `CommandBuffer::empty/clear/commandCount`, `Entity.index`,
  `.generation`, `World::isIterating`, direct `World::add/remove/destroy`, `ObserverKind::OnRemove`.
- [ ] **Step 3: Update the TS page.** `:23`: only `spawn/destroy/add/remove` are refused during `frame` (`world.ts:112-132`). `:101`: only
  `raw()` goes stale on memory growth; `ReadField`/`WriteField` re-read the heap (`fields.ts:15,26-27`).
- [ ] **Step 4: Delete the old page, fix links, update `docs/README.md`.** Run Step 1 again: no grep hit, no `MISSING`. Run V1, V2.
- [ ] **Step 5: Abandon check** (spec §10). For each of `8dfec4c` and `bc76c6c`, read `git show <commit> -- docs/` and count the doc files a
  correct update touches:
  - **Baseline:** files under `docs/guide/`, plus `capi.md` and the cheat sheet, that the `guide-style.md` table names for that kind of
    change; `troubleshooting.md` when it adds an error; the glossary only when the change adds a name.
  - **New tree:** each fact of the diff, placed in its home in the new tree (explanation, `reference/cpp`, `reference/typescript`, how-to,
    `reference/errors.md`, the glossary under the same rule). `explanation/architecture/` is left out on both sides: it moves 1:1.
  - If the new count is higher than the baseline for either commit, **stop**: report both counts to the user and propose the spec's
    fallback (the pilot merges back into one `explanation/concepts/world-and-entities.md`; Tasks 4–6 become `git mv` of each concept page
    whole, with their "Fix on the way" items). Do not switch without the user's answer.
- [ ] **Step 6: Commit:** `docs: split world and entities into explanation, reference and how-to`. The body states the counts:
  `Abandon check: 8dfec4c baseline N, new M; bc76c6c baseline N, new M.`

---

### Task 4: Split `schema`, `queries`, `engine-and-systems`

Spec §7.1 rows of the three themes; one commit per theme, in this order.

**Files (per theme):**
- Create: `docs/explanation/concepts/<theme>.md`, `docs/reference/cpp/<theme>.md`; `docs/how-to/drive-the-loop-yourself.md`
  (engine-and-systems `:272`)
- Modify: `docs/reference/typescript/<theme>.md` (nav, mirrored titles, contract rules → link), `docs/how-to/gameplay-recipes.md` and
  `docs/how-to/typescript/gameplay-recipes.md` (queries patterns `:117-218`, with a row each in the table at the top),
  `docs/explanation/concepts/README.md`, `docs/README.md`, inbound links
- Delete: `docs/guide/concepts/<theme>.md`

**Interfaces:**
- Consumes: Task 3's explanation chain and README.
- Produces: `docs/how-to/drive-the-loop-yourself.md`, which Task 5 extends (input `:144`).

- [ ] **Step 1: Failing checks.**

```bash
git grep -n "// Generated: do not edit" -- docs
git grep -qF -- 'systemId()' docs/reference || echo "MISSING: systemId()"
```

  Expected: one hit in `docs/guide/concepts/schema.md:101`; `MISSING: systemId()`.

- [ ] **Step 2: Schema.** A7: the listing is marked as an excerpt of `schema.generated.hpp`; its first line is
  `// Generated by oly-schema: do not edit.` (`cpp_emitter.cpp:85`); no `struct Position` (it comes from the plugin header the game's
  header includes); `schema_registry.generated.cpp` is listed only for web targets (`cmake/OlyGame.cmake:116,124-125`). D10: field types
  and name rules (`:45-82`) are deleted; the page links to `engine/schema/format/README.md` (relative link from the new page). Commit:
  `docs: split schema into explanation and reference`.
- [ ] **Step 3: Queries.** Commit: `docs: split queries and move its patterns to the recipes`.
- [ ] **Step 4: Engine and systems.** Add `Engine::emit` from the host (`engine.hpp:185`), `systemId()`, and the `setSystemStateMask`
  overload that takes a `StateMask` (`engine.hpp:233`; only the `initializer_list` form is documented today). The deferred-change rule links to Task 3's section instead of restating it. Commit:
  `docs: split engine and systems and add the own-loop how-to`.
- [ ] **Step 5: Verify after each commit:** V1, V2; Step 1's checks print nothing after the last one.

---

### Task 5: Split `events`, `input`, `schedule`, `states`

Spec §7.1 rows of the four themes; one commit per theme.

**Files (per theme):** as Task 4. Also: `docs/how-to/gameplay-recipes.md` (input rebind `:121` merges into the existing key-rebinding
recipe; states pattern `:143` becomes a recipe), `docs/how-to/drive-the-loop-yourself.md` (input `:144`).

**Interfaces:**
- Consumes: Task 4's `drive-the-loop-yourself.md`.

- [ ] **Step 1: Failing check.**

```bash
for symbol in 'eventType()' 'handlerId()' 'eventData()' setHandlerStateMask ActionKind ActionId actionCount pendingCount \
  MaxTaskPayloadSize MaxTaskPayloadAlignment declareState stateCount StateMask MaxStates 'hookId()' 'userData()'; do
  git grep -qF -- "$symbol" docs/reference || echo "MISSING: $symbol"
done
```

  Expected: one `MISSING` line per symbol not yet in `docs/reference/` (at least `eventData()`, `ActionKind`, `pendingCount`,
  `MaxTaskPayloadAlignment`).

- [ ] **Step 2: Events** (`eventType()`, `handlerId()`, `eventData()`, `setHandlerStateMask` with a mask). Commit:
  `docs: split events into explanation and reference`.
- [ ] **Step 3: Input** (`ActionKind`, `ActionId` in C++ and TS, `ActionMap::actionCount/kind/name`). Commit:
  `docs: split input and move rebinding to the recipes`.
- [ ] **Step 4: Schedule** (`Scheduler::schedule(delay, interval, fn, payload, discard)`, `pendingCount`, `MaxTaskPayloadSize` and
  `MaxTaskPayloadAlignment` with the values of `app/task.hpp:18,20`). Commit: `docs: split schedule into explanation and reference`.
- [ ] **Step 5: States** (`declareState`, `stateCount`, `StateMask`, `MaxStates`, `hookId()`, `name()`, `userData()`). Commit:
  `docs: split states and move the game-flow pattern to the recipes`.
- [ ] **Step 6: Verify after each commit:** V1, V2; Step 1's loop prints nothing after the last one.

---

### Task 6: Split `performance`, `plugins`, `transforms`; the web boundary

Spec §7.1 rows of the three themes; §7.2 row `environments/web/boundary.md`; §7.3 row `performance.md` (only the consequence for a game,
`architecture/performance.md:23`). One commit per theme, then one for the boundary.

**Files:**
- Create: `docs/explanation/concepts/{performance,plugins,transforms,web-boundary}.md`, `docs/reference/cpp/{plugins,transforms}.md`,
  `docs/how-to/{use-a-plugin,write-a-plugin,keep-the-hot-path-fast}.md`
- Modify: `docs/architecture/performance.md` (`:23` leaves; the rest moves in Task 9), `docs/explanation/concepts/README.md` (every
  entry now inside `explanation/concepts/`), `docs/README.md`, inbound links
- Delete: `docs/guide/concepts/{performance,plugins,transforms}.md`, `docs/guide/environments/web/boundary.md`; `docs/guide/concepts/` is
  then empty

**Interfaces:**
- Produces: `docs/explanation/concepts/web-boundary.md`, which Task 7 extends (web getting-started `:180`);
  `docs/how-to/keep-the-hot-path-fast.md` with a `## TypeScript` section (`boundary.md` `:9`, `:37-61`).

- [ ] **Step 1: Failing check.** `git grep -n "oly/app/context.hpp" -- docs/guide/concepts/plugins.md` finds `:108`.
- [ ] **Step 2: Performance.** Commit: `docs: split performance into explanation and how-to`.
- [ ] **Step 3: Plugins.** The include is `oly/app/system.hpp`. The navigation chain is closed (§5.1). Commit:
  `docs: split plugins into explanation, reference and how-to`.
- [ ] **Step 4: Transforms.** `:20`: a game's own `Position` stops `oly-schema` at the merge, with the message of
  `engine/schema/codegen/src/merge.cpp:28` copied. The `World` that outlives the `Engine` keeps the `Position` → `WorldTransform`
  implication and stops updating it (`plugins/essentials2d/native/include/oly/essentials2d/plugin.hpp:25-27`): read the header before
  writing it; if the header does not say it, record it as deferred (Task 13). Commit: `docs: split transforms into explanation and reference`.
- [ ] **Step 5: Web boundary.** The why goes to `web-boundary.md`; the techniques to `keep-the-hot-path-fast.md`. Commit:
  `docs: split the web boundary into explanation and how-to`.
- [ ] **Step 6: Verify after each commit:** V1, V2. After the last, `ls docs/guide/concepts` fails (no such directory).

---

### Task 7: Tutorials, the game template reference, and testing a game

Spec §10 step 3; §7.2 rows `environments/web/getting-started.md`, `environments/native/getting-started.md`, `environments/web/testing.md`
(now `docs/how-to/test-a-web-game.md`), the TS `states.md:78` and `typescript/recipes.md:334` fixes.

**Files:**
- Create: `docs/tutorials/first-web-game.md`, `docs/tutorials/first-desktop-game.md`, `docs/reference/game-template.md`,
  `docs/reference/native-host.md`, `docs/how-to/test-a-desktop-game.md`, `docs/how-to/export-your-own-c-functions.md`,
  `docs/explanation/concepts/environments-and-hosts.md`
- Modify: `docs/how-to/test-a-web-game.md` (A1), `docs/reference/typescript/states.md:78`, `docs/how-to/typescript/gameplay-recipes.md:334`,
  `docs/reference/cpp/engine-and-systems.md` (native `:122`), `docs/explanation/concepts/web-boundary.md` (web `:180`),
  `docs/explanation/concepts/README.md`, `docs/README.md`, inbound links
- Delete: `docs/guide/environments/web/getting-started.md`, `docs/guide/environments/native/getting-started.md`

**Interfaces:**
- Consumes: `templates/game/**` as the only source of code; `docs/explanation/concepts/web-boundary.md` (Task 6).
- Produces: `docs/tutorials/first-web-game.md#<the TS system section>` — Task 12's README links its TS equivalent there;
  `docs/reference/game-template.md` with `## Web` and `## Native` sections.

- [ ] **Step 1: Failing checks.**

```bash
git grep -n "installGame(" -- docs | grep -vF "installGame(engine, drawer)"
git grep -nE "640, ?480|640x480" -- docs
git grep -qF WindowConfig docs/reference || echo "MISSING: WindowConfig"
```

  Expected: hits in `test-a-web-game.md`, `reference/typescript/states.md`, `how-to/typescript/gameplay-recipes.md`; a hit in the native
  getting-started page; `MISSING: WindowConfig`.

- [ ] **Step 2: Tutorials.** `docs/how-to/gameplay-recipes.md:692`: `${nativeSources}` is introduced as the template's variable
  (`templates/game/web/cmake/game.cmake:1`). A6: the desktop tutorial's smallest game is `templates/game/native/desktop/main.cpp` with `movePlayer`,
  the include of `systems.hpp` and the template's window size. Every code block is a copy or a labelled excerpt of a template file; commands
  are the `oly` commands of `tools/oly/Spurfile`.
- [ ] **Step 3: Game template reference.** One row per file of `templates/game/{common,web,native}` (`find templates/game -type f`), with
  where `oly new` puts it (`tools/oly/Spurfile:543` copies `common/.` into `<game>/`).
- [ ] **Step 4: Testing.** A1: the web harness uses `installGame(engine, drawer)` and the `Drawer { clear, square }` of
  `templates/game/web/web/src/game.ts:8-13`, as `templates/game/web/web/tests/smoke.test.ts` does. The two other signatures
  (`states.md:78`, TS recipes `:334`) either use the template's or say in one line that the example is a bigger game.
- [ ] **Step 5: The rest of both pages** goes per the §7.2 rows; `reference/native-host.md` names `WindowConfig`
  (`native_host/window.hpp:9`). Delete both getting-started pages; fix links.
- [ ] **Step 6: Verify.** Step 1's first two greps print only lines that say "a bigger game"; no `MISSING`. Diff each tutorial code block
  that claims to be a whole file against the template file by eye (`templates/game/...`). V1, V2.
- [ ] **Step 7: Commit:** `docs: write the tutorials from the game template`.

---

### Task 8: `cli.md`, troubleshooting, cheat sheet and the guide index; `docs/guide/` stops existing

Spec §10 step 4; §7.2 rows `README.md`, `cli.md`, `cheat-sheet.md`, `troubleshooting.md`; decision 2 (delete the cheat sheet).

**Files:**
- Create: `docs/reference/cli.md`, `docs/reference/errors.md`, `docs/reference/c-api.md` (the cheat sheet's C API section; Task 10 merges
  `capi.md` into it), `docs/how-to/fix-common-problems.md`, `docs/how-to/upgrade-the-engine.md`, `docs/how-to/set-up-game-ci.md`,
  `docs/how-to/build-with-an-engine-checkout.md`
- Modify: `docs/reference/cpp/*.md`, `docs/reference/typescript/*.md` (names only the cheat sheet has), `docs/reference/game-template.md`
  (`cli.md:100`), `docs/explanation/concepts/environments-and-hosts.md` (the map, guide README `:22-31`), `docs/README.md` (paths and
  tables `:33-82`, conventions `:134`; the transitional guide rows leave), `README.md` (`:7` and the `docs/guide/` row of `:138` point
  to `docs/reference/cli.md` and `docs/README.md`; guide README "What you can build" `:7-20` becomes `## Features`),
  `docs/development/guide-style.md` (its links to the deleted pages point to their new homes; Task 11 rewrites it),
  `docs/reference/glossary.md:62` (the "`create` does not exist as a name" rule is restricted to entities),
  `docs/superpowers/plans/2026-11-14-multi-environment-phase5-docs.md:221` (link target → `../../reference/errors.md`)
- Delete: `docs/guide/cli.md`, `docs/guide/troubleshooting.md`, `docs/guide/cheat-sheet.md`, `docs/guide/README.md`

**Interfaces:**
- Produces: `docs/reference/cli.md` with an `## Install` section (Task 12 links `docs/reference/cli.md#install`);
  `docs/reference/errors.md` with `## C++`, `## TypeScript`, `## oly` sections; `docs/reference/c-api.md`.

- [ ] **Step 1: Failing checks.**

```bash
for symbol in 'oly sync' 'oly platform'; do git grep -qF -- "$symbol" docs/reference || echo "MISSING: $symbol"; done
git grep -n "./spur ts" -- docs/guide/troubleshooting.md
git grep -nE "frame is not reentrant|last frame was interrupted" -- docs
```

  Expected: two `MISSING`; a hit at `troubleshooting.md:16`; hits in the troubleshooting table.

- [ ] **Step 2: `reference/cli.md`** per the §7.2 row; adds `oly sync` (`tools/oly/Spurfile:593`) and `oly platform` (`:564`). D7: the
  local mode table lives only here. D16: the `curl … install.sh | sh` line lives only here (the README keeps one copy for first contact;
  Task 12).
- [ ] **Step 3: `reference/errors.md`** (spec §7.6 scope). A3: each row says its origin and gives the text in every language that has it;
  the C++ messages come from `engine/runtime/app/src/engine.cpp:48,83,125-127,138` and `ecs/world.hpp:352`; the two TS-only messages are
  labelled TS. A8: the `SchemaMismatchError` fix is `oly build`.
- [ ] **Step 4: Cheat sheet merge.** Each name goes to its theme's reference page unless already there; `Key::W` and `Key::Space` are
  qualified (`oly::input::Key::W`); the plugin skeleton declares `install` in the public section; D11: cross-language lookup stays in
  `reference/glossary.md`.
- [ ] **Step 5: Guide index.** Per the §7.2 row; "The big picture" (`:84-132`) is deleted. §5.1: the reading paths in `docs/README.md`
  list every concept page per environment (today's TypeScript path, `:39`, leaves out performance, plugins and transforms).
- [ ] **Step 6: Verify.** Step 1's checks print nothing (the `frame is not reentrant` row may remain only under `## TypeScript`).
  `ls docs/guide` fails. V1; V2 with `stale_paths docs/guide/`.
- [ ] **Step 7: Commit:** `docs: split the command, error and cheat sheet pages and remove the guide folder`.

---

### Task 9: Contributor docs, the architecture overview, and the superpowers status page

Spec §7.3 rows `README.md`, `modules-and-boundaries.md`, `performance.md`, `publishing.md`, `core.md`, `build-and-tests.md`,
`ts-packages.md`; §7.4 rows `development/commands.md`, `benchmarks/README.md` (now `docs/reference/benchmarks/`), `superpowers/*`;
decision 1 (`core.md` becomes one line).

**Files:**
- Create: `docs/explanation/architecture/README.md`, `docs/explanation/architecture/{modules-and-boundaries,publishing,build-and-tests,ts-packages}.md`,
  `docs/reference/contributor/commands.md`, `docs/reference/contributor/boundary-rules.md`,
  `docs/how-to/contributor/{set-up-the-toolchain,run-benchmarks,cut-a-release}.md`, `docs/superpowers/README.md`
- Modify: `docs/reference/benchmarks/README.md`, `docs/reference/benchmarks/bridge/README.md` (commands leave), `docs/reference/cli.md`
  (receives nothing: D7 already there; `publishing.md:89-95` is deleted), `docs/reference/typescript/README.md` (ts-packages exports),
  `docs/README.md` (`## Working on the engine`; the transitional architecture rows leave, except for `capi.md` and `plugins.md`, which
  Task 10 moves), `CLAUDE.md:7` (`Start with docs/README.md`), `.claude/skills/add-test-case/SKILL.md:161`, `CONTRIBUTING.md:20,35`,
  `README.md:139,141`
- Delete: `docs/architecture/{README,modules-and-boundaries,performance,publishing,core,build-and-tests,ts-packages}.md`,
  `docs/development/commands.md`

**Interfaces:**
- Produces: `docs/reference/contributor/commands.md` (single source of D1, D2, and the engine-level CMake options Task 10 adds);
  `docs/how-to/contributor/cut-a-release.md` (Task 13 adds the CHANGELOG step); `docs/superpowers/README.md` with a
  `## Documentation audit` section (Task 13 fills it).

- [ ] **Step 1: Failing checks.**

```bash
git grep -qE "^\| *\`configure\`" docs/development/commands.md || echo "MISSING: configure"
git grep -n "games/" -- docs/architecture/modules-and-boundaries.md
git grep -n "esbuild" -- docs/architecture/ts-packages.md
```

  Expected: `MISSING: configure` (audit §3.1); a hit at `:58`; a hit at `:3`.

- [ ] **Step 2: Write the pages** per the rows. Fixes: the `configure` task (`Spurfile:100`) gets its row; `:58` describes what the rule
  checks today (`tools/check_boundaries.py:102-105`); `ts-packages.md:3` drops esbuild from the workspace's pins; `:10` adds
  `createEngine(config)`. D4: the `clang-format` hook is described only in `explanation/architecture/build-and-tests.md`.
- [ ] **Step 3: `docs/superpowers/README.md`.** A status banner: the specs and plans are the historical record; the current state is in
  `docs/explanation/architecture/`; the ECS spec (`R-*`, `I-*`, `NF-*`) remains the requirement in force. Then the table of specs from
  `architecture/README.md:65-80`, plus the publishing spec, the audit and the reorganization design. No "source of truth" wording. An empty
  `## Documentation audit` heading for Task 13.
- [ ] **Step 4: Verify.** `git grep -qE "^\| *\`configure\`" docs/reference/contributor/commands.md || echo "MISSING: configure"` and
  `git grep -n esbuild -- docs/explanation/architecture/ts-packages.md` print nothing; the `games/` line of
  `explanation/architecture/modules-and-boundaries.md` no longer describes a folder that exists. V1; V2. `ls docs/development` shows only
  `guide-style.md`.
- [ ] **Step 5: Commit:** `docs: move the contributor pages and the architecture overview into the new tree`.

---

### Task 10: User references (CMake, C API, web host) and the last architecture splits

Spec §10 step 5; §7.3 rows `capi.md`, `plugins.md`; §7.6 rows `reference/cmake.md`, `reference/c-api.md`, `reference/typescript/web-host.md`;
decision 8 (CMake: only what a game sets).

**Files:**
- Create: `docs/reference/cmake.md`, `docs/reference/typescript/web-host.md`, `docs/explanation/architecture/{capi,plugins}.md`
- Modify: `docs/reference/c-api.md` (all 61 functions with signatures from `c_api.h`/`engine_api.h`, plus the 17 types and constants),
  `docs/reference/contributor/commands.md` (engine-level options: `OLY_WERROR`, `OLY_SANITIZE`, `OLY_BUILD_BENCH`, `OLY_VEC_REPORT`,
  `OLY_BUILD_GAMES`), `docs/explanation/architecture/modules-and-boundaries.md` (D14: `plugins.md:155-168`),
  `docs/reference/typescript/*.md` (TS §3.2 items), `docs/README.md` (the last transitional rows leave), `CLAUDE.md:15`,
  `CONTRIBUTING.md:55`, inbound links
- Delete: `docs/architecture/capi.md`, `docs/architecture/plugins.md`; `docs/architecture/` is then empty

**Interfaces:**
- Produces: `docs/reference/cmake.md` with `## oly_add_game`, `## oly_add_plugin`, `## Options`; `docs/reference/c-api.md` complete.

- [ ] **Step 1: Failing check.**

```bash
for symbol in OLY_BUILD_NATIVE_HOST OLY_BUILD_TESTS OLY_ENABLE_ASSERTS OLY_NO_STATE OLY_INVALID_STATE_HOOK OLY_STATE_ENTER \
  OLY_STATE_EXIT OLY_INVALID_SYSTEM OlyTaskHandle OlyActionState OLY_ACTION_PRESSED OLY_ACTION_HELD OLY_ACTION_RELEASED \
  OLY_ACTION_VALID OLY_ACTION_BUTTON OLY_ACTION_AXIS2D OLY_ACTION_POINTER OlyQuery OlyEntity createEngine SpawnValues ValueOf \
  runInBrowser attachBrowserInput moduleFactoryFromUrl; do
  git grep -qF -- "$symbol" docs/reference || echo "MISSING: $symbol"
done
```

  Expected: one `MISSING` line per symbol not yet in `docs/reference/`.

- [ ] **Step 2: `reference/cmake.md`** from `architecture/plugins.md:67-118`, `cmake/OlyGame.cmake`, `cmake/OlyOptions.cmake:5-13`; only the
  options a game sets (`OLY_BUILD_NATIVE_HOST`, `OLY_BUILD_TESTS`, `OLY_ENABLE_ASSERTS`), each with its default copied from
  `OlyOptions.cmake`. `oly_typescript_schemas` and `oly_add_engine_test_game` are not documented (§3.3).
- [ ] **Step 3: `reference/c-api.md`** (D12: the only list of C API functions). One entry per function: the signature copied from the
  header, what it does, ownership and the error behaviour. The design points and the invoker go to `explanation/architecture/capi.md`.
- [ ] **Step 4: TS references.** `web-host.md`: `runInBrowser`, `attachBrowserInput`, `moduleFactoryFromUrl` with the parameters a game
  passes; the injection parameters and the layout and view types are left out (decision 8). The other §3.2 TS items
  (`Runtime.createEngine`, `Runtime.schema`, `World.handle`, `EventBatch.type`, `Schema.types/events/actions/states/hash`, `SpawnValues`,
  `ValueOf`) go to their theme pages. The type names `SystemFunction`, `TaskFunction`, `StateHookFunction`, `HandlerRegistration`,
  `SystemRegistration` are named where their members are documented.
- [ ] **Step 5: Plugins split** per the §7.3 row (D18: the install order and bands keep their why in
  `explanation/architecture/plugins.md`; the contract stays in `explanation/concepts/plugins.md`).
- [ ] **Step 6: Verify.** Step 1's loop prints nothing. `ls docs/architecture` fails. V1; V2 with `stale_paths docs/architecture/`.
- [ ] **Step 7: Commit:** `docs: add the CMake, C API and web host references`.

---

### Task 11: `docs-style.md`, `CLAUDE.md`, `CONTRIBUTING.md`, the PR template

Spec §10 step 6; §7.4 row `development/guide-style.md`; §7.5 rows `CONTRIBUTING.md`, `CLAUDE.md`, `.github/pull_request_template.md`;
decision 1 and 8 (the public surface and its exclusion list).

**Files:**
- Create: `docs/reference/contributor/docs-style.md`
- Modify: `CLAUDE.md`, `CONTRIBUTING.md`, `.github/pull_request_template.md`, `.github/ISSUE_TEMPLATE/bug_report.yml:21`, `docs/README.md`
- Delete: `docs/development/guide-style.md`; `docs/development/` is then empty

**Interfaces:**
- Consumes: the page contracts of this plan's Global Constraints.
- Produces: `docs/reference/contributor/docs-style.md` with `## Public surface` (Task 13 links it from the audit section).

- [ ] **Step 1: Write `docs-style.md`.** Sections: the four types and what each page may hold; the tree; the page contracts (navigation,
  `## API`, reference entries, how-to and tutorial shapes); the cpp/ts mirror rule; "What to update" — the `guide-style.md` table rewritten
  for the new homes (concept change → explanation; signature → `reference/cpp` and `reference/typescript`; C API → `reference/c-api.md`;
  error → `reference/errors.md`; `oly`/template/`toolchain.env` → `reference/cli.md`, `reference/game-template.md`, `reference/errors.md`;
  new page → `docs/README.md` and the neighbours' navigation); `## Public surface`: the names `reference/` documents, and the §3.3 groups
  of every surface listed by name as "internal, pending a code decision (move to `detail/`)", so a reviewer tells an omission from an
  exclusion. Language, game-agnostic and example rules carry over from `guide-style.md`.
- [ ] **Step 2: `CLAUDE.md`.** The public API rule points to `docs/reference/contributor/docs-style.md` and to `docs/` instead of
  `docs/guide/`. D3: lines 12-13 leave (they are in the imported `code-style.md`).
- [ ] **Step 3: `CONTRIBUTING.md`.** Stays the entry door: what is welcome, workflow, commits, one link per theme. One line and a link
  replace each of D1 (`reference/contributor/commands.md`), D2 (`how-to/contributor/run-benchmarks.md`), D3 (`code-style.md`), D13
  (`docs-style.md`), D14 (`explanation/architecture/modules-and-boundaries.md`). D5: the checklist leaves; one line links the PR template.
  D15 stays here as its single human-facing source (audit §6): the spec lists D15 among the duplications that leave, which would leave the
  `checkInvariants` rule with no home outside `CLAUDE.md`; this plan keeps it and says so in the commit body. The "source of truth" wording
  (`:22`) leaves.
- [ ] **Step 4: PR template.** One checklist (D5): today's items plus "a hot-path change reports the benchmark" and "the branch starts
  from `develop`"; the docs item says `docs/` and names `docs-style.md` by path. `bug_report.yml:21`: `render: cpp` becomes
  `render: text`, which fits C++, TS and commands alike.
- [ ] **Step 5: Verify.** `git grep -n "guide-style" -- ':!docs/superpowers'` prints nothing. `ls docs/development` fails. V1; V2.
- [ ] **Step 6: Commit:** `docs: replace the guide style with a style for all of docs`.

---

### Task 12: The README and its example check

Spec §9 (sections, sources, sizes) and §8 (the README example test); decision 4.

**Files:**
- Create: `tools/check_readme_example.py`, `tools/test_check_readme_example.py`
- Modify: `README.md` (rewrite), `CMakeLists.txt` (two tests next to `oly.lint.links`), `docs/README.md`
- Move out of the README, into the homes §7.5 names: "What already exists" → `docs/reference/contributor/repository.md`; "How the ECS
  works" → `docs/explanation/architecture/ecs.md` (D9: only what it lacks); the ECS-only example → `docs/reference/cpp/`; the TS example →
  `docs/tutorials/first-web-game.md` if it adds anything; "Build and test" and "Benchmarks" → `docs/how-to/contributor/`

**Interfaces:**
- Consumes: `docs/reference/cli.md#install`, `docs/tutorials/first-web-game.md#<TS system section>`, `docs/README.md` anchors,
  `docs/reference/limits.md`, `docs/reference/benchmarks/README.md`, `SUPPORT.md` (created in Task 13: the README links it only after
  Task 13; until then the Contributing section lists the other three files).
- Produces: `python3 -I tools/check_readme_example.py --root <repo>`; ctest `oly.lint.readme_example`,
  `oly.lint.readme_example_selftest`. A block is tied to its template by the line right before its opening fence:
  `<!-- template: templates/game/common/game.oly -->` (invisible on GitHub).

- [ ] **Step 1: Write the failing tests** (same shape as Task 1's):

| Test | Input | Assertion |
|---|---|---|
| `test_matching_block_passes` | README section with a marked block equal to the template file | code 0, no output |
| `test_different_block_fails` | one changed line | `README.md:<fence line> \| templates/x.cpp \| block differs from the template`, code 1 |
| `test_crlf_template_matches` | template with CRLF, README with LF | code 0 |
| `test_trailing_blank_lines_ignored` | template ending with two `\n` | code 0 (both sides `.strip()`ped) |
| `test_block_without_marker_fails` | a fence in the section with no marker line | `README.md:<line> \| - \| block names no template` |
| `test_missing_template_fails` | marker names a missing file | `... \| templates/missing.cpp \| template not found` |
| `test_missing_section_fails` | no `## Minimal example` | `README.md \| ## Minimal example \| section not found` |
| `test_section_without_blocks_fails` | section with prose only | `README.md \| ## Minimal example \| section has no code block` |
| `test_blocks_outside_section_ignored` | an unmarked fence in `## Install` | code 0 |

- [ ] **Step 2: Run them to verify they fail.** `./spur dev python3 -I tools/test_check_readme_example.py`: `ModuleNotFoundError`.
- [ ] **Step 3: Implement** `check(root) -> list[str]` and `main(argv) -> int` in `tools/check_readme_example.py`. The section runs from
  `## Minimal example` to the next `## ` heading; fences come from `check_links.iter_lines` (Task 1), imported after
  `sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))`, since `-I` leaves the script's folder off the path.
- [ ] **Step 4: Run them to verify they pass:** `OK`. Register both in `CMakeLists.txt`.
- [ ] **Step 5: Rewrite `README.md`** with the eight sections of spec §9, in order, within each row's size. Install: requirements per OS,
  the `curl -fsSL https://github.com/esliph/oly/releases/latest/download/install.sh | sh` line copied from the current `README.md:7`,
  `oly doctor`, a link to `docs/reference/cli.md#install`. Minimal example: `templates/game/common/game.oly` and
  `templates/game/common/native/movement.cpp`, each marked; one sentence says they land at `<game>/game.oly` and
  `<game>/native/movement.cpp`; a link to the TS equivalent in the web tutorial. Documentation: four type rows plus "Working on the engine".
  Drop "source of truth" (`README.md:144`).
- [ ] **Step 6: Verify.** V3 (all `oly.lint.*`, the new two included). Change one character in a README block, run
  `./spur dev python3 -I tools/check_readme_example.py --root .`: one `block differs` line; undo it (`git diff README.md` shows only the
  intended rewrite). V1; V2.
- [ ] **Step 7: Commit:** `docs: rewrite the README for a first-time reader`.

---

### Task 13: SUPPORT, CHANGELOG, issue templates, and the audit close-out

Spec §10 step 8 and the success criterion; §7.5 rows `.github/ISSUE_TEMPLATE/*`; §7.6 rows `SUPPORT.md`, `CHANGELOG.md`, `CODEOWNERS`; A4.

**Files:**
- Create: `SUPPORT.md`, `CHANGELOG.md`
- Modify: `.github/ISSUE_TEMPLATE/config.yml` (a contact link to `SUPPORT.md`), `README.md` (Contributing section adds SUPPORT),
  `docs/how-to/contributor/cut-a-release.md` (update the CHANGELOG before tagging), `docs/superpowers/README.md`
  (`## Documentation audit`)
- Not created: `.github/CODEOWNERS` (optional in the spec; a single maintainer gets no automatic review request from it). Ask the user in
  the handoff instead of adding it.

- [ ] **Step 1: `SUPPORT.md`.** Where to ask (issues; Discussions only if the repository has them enabled — ask the user, it is not
  verifiable locally), what is not support (security: `SECURITY.md`), what to include in a question.
- [ ] **Step 2: `CHANGELOG.md`.** One section per `0.x` version, each with `### Breaking` and `### Migration`. Today: `## 0.1.0
  (unreleased)` (`CMakeLists.txt:2`; no tag exists). The cut-a-release how-to gains the step "move the unreleased entries under the new
  version".
- [ ] **Step 3: Coverage check** — every §3.2 symbol is in the new tree:

```bash
for symbol in 'Engine::emit' commandCount declareState stateCount setSystemStateMask setHandlerStateMask StateMask MaxStates \
  'eventType()' 'handlerId()' 'eventData()' 'systemId()' 'hookId()' pendingCount MaxTaskPayloadSize MaxTaskPayloadAlignment \
  '.generation' isIterating OnRemove ActionKind ActionId actionCount WindowConfig 'oly sync' 'oly platform' OLY_BUILD_NATIVE_HOST \
  OLY_BUILD_TESTS OLY_ENABLE_ASSERTS OLY_NO_STATE OLY_INVALID_SYSTEM OlyTaskHandle OlyActionState OLY_ACTION_POINTER OlyQuery \
  OlyEntity createEngine SpawnValues ValueOf; do
  git grep -qF -- "$symbol" docs/reference docs/explanation || echo "MISSING: $symbol"
done
```

  Expected: no output.

- [ ] **Step 4: Classification redo.** Classify every page under `docs/` (outside `superpowers/`) with the audit's §2 method (type,
  audience, sections). No page is "Mixed". A page that is mixed is fixed in this task, not recorded.
- [ ] **Step 5: Findings ledger.** Walk A1–A10, §4, §5, §5.1, D1–D18, §7 of the audit. Each verified finding is closed (name the page
  that closes it, for yourself) or deferred. `## Documentation audit` in `docs/superpowers/README.md` lists only the deferred ones, each
  with its reason; it starts with one line saying every other verified finding is closed. Known deferrals: A10's code half and
  `engine_api.h:53-55` (code decisions, spec §2); the suspected items not confirmed on the way (spec §11).
- [ ] **Step 6: Verify.** V1, V2, V3. `./spur format-check` is untouched by this plan, so not run.
- [ ] **Step 7: Commit:** `docs: add support and changelog files and record the open audit items`.
