# Blank lines

A blank line is punctuation: it separates **ideas**, not lines. Each one states "a step ends here". If it appears where there is no change of step, it misleads the reader; if it is missing where there is one, it forces the reader to rebuild the structure alone. The reader first scans the visual shape of the function and only then reads the detail — blank lines are what give shape to that first reading.

**Applies to:** every language with free vertical layout (C, C++, Java, C#, PHP, TypeScript/JS, Python, Go, Rust, shell, SQL).

**Philosophy:** `philosophy.md` has examples in several languages, the table of grouping levels, per-ecosystem conventions and what formatters automate. Read it when a case is not covered here or when configuring a formatter.

## Principles

1. **Cohesion defines the block.** Lines that work together toward a single goal form a block — one step of the algorithm — and a block is never split in half. The blank line appears where the goal changes. The one exception tied to syntax is the multi-line control block (`if`, `for`, `while`, `switch`, `try`, `do`): it always gets a blank line before and after, because opening a scope is where the reader changes indentation level, and the space makes every scope visible in the first scan. A single-line statement (`if (x) return;`, `return`, a declaration) never gets a blank line just because of its kind.
2. **Mental title test.** Each block must accept a short name ("validates the input", "builds the query", "writes the result"). A block with two names is two blocks; two neighbouring blocks with the same name are one.
3. **A single intensity.** At most one blank line in a row. Larger hierarchy is expressed by the language's structure (functions, classes, modules), not by more space. Exception: formal conventions, such as PEP 8's 2 lines between top-level definitions in Python.
4. **Declaration near use.** A variable stays next to the lines that consume it, so each block reads on its own.
5. **Delimiters already separate.** No blank line right after opening a block (`{`, `:`, `begin`) nor right before closing it — the brace or the indentation already say that.
6. **Cohesion beats symmetry.** Space communicates structure, it does not "air out" the code. Dense and well grouped is better than uniformly spaced.

## Rules

Before typing a function body, think about the phases. Most follow **guard → preparation → processing → result**, and each phase is a block:

```ts
async function importLevel(path: string): Promise<Level> {
    if (!path.endsWith('.json')) throw new Error(`Invalid format: ${path}`)

    const raw = await readFile(path, 'utf8')
    const data = LevelSchema.parse(JSON.parse(raw))

    const entities = data.entities.map(toEntity)
    const tilemap = buildTilemap(data.tiles, data.width)

    return { name: data.name, entities, tilemap }
}
```

Recurring rules, all derived from the principles:

- **Imports:** one group per origin — standard library, external dependencies, project — separated by a blank line.
- **Constants:** one group per domain.
- **Type members:** one group per role (data, control state, construction, operations). Parallel members, such as the arrays of an SoA layout, stay together because they are a single idea.
- **Consecutive guards:** a single block ("reject invalid cases"), with no lines between them — also when each guard is a braced block whose body only leaves (`return`, `throw`, `continue`, `break`). The group as a whole gets the blank line before and after.
- **Multi-line loops and conditions:** a blank line right before the line that opens the block and right after the line that closes it. The declarations that exist only to feed the block (the expected value the `if` compares, the accumulator the `for` fills, the uninitialized variable the `if/else` chain assigns, the `clear()` before rebuilding) stay right above that blank line, as the step that prepares it — still next to their use, but not glued to the block. Exceptions: no blank line right after `{` or right before `}` (principle 5); `else`, `catch` and `finally` continue the same block; consecutive guards stay together (rule above). Single-line statements without braces are not covered: they follow the step rule.
- **Result:** the final `return` is a block of its own when the title of the last step does not include it — it delivers the product of earlier steps. It stays attached when it is the conclusion of its own step (`return` inside a guard, a single-step function, a value created on the previous line only to be returned).
- **Tests:** exactly three blocks — Arrange, Act, Assert. If the Act has more than one line, the test probably checks more than one behaviour.
- **Inseparable pairs stay together:** `lock`/`unlock`, declare/single use in the next statement, operation/its counterpart (destroy the row and fill the hole). When the second half is a multi-line block — the error check `if (!handle) { ... }` after the open — the loops and conditions rule wins and the blank line goes before the block.

```cpp
Entity entity = store.allocate();
EntityRecord& record = store.record(entity.index);

record.row = table.pushUninitialized(entity);
record.archetype = &archetype;

return entity;
```

The preparation stays above the blank line. Inside the `if`, the first statement follows the `{` with no blank line, and the `for` still gets its own:

```cpp
const Order& order = orders.at(id);

if (order.isOpen()) {
  double total = 0.0;

  for (const Item& item : order.items) {
    total += item.price * item.quantity;
  }

  return total;
}
```

**Depth:** the deeper the level, the rarer the blank line. A loop body that needs several inner blocks is asking to become a function.

## Blank lines as a design signal

A well-delimited block is a function that has not been extracted yet: the blank line is the cut point and the mental title is the name of the new function. Use this actively:

- Function with more than 3–4 blocks → consider extracting each block. The original function becomes a sequence of named calls and the inner blank lines almost disappear.
- Urge to write title comments (`// --- Validation ---`) inside a function → it is the same signal: each section wants to be a function with that name.
- Cannot decide where to put the blank lines → the responsibilities are mixed. The problem is design, not formatting; say so instead of just reformatting.

Do not extract on your own unless the request involves refactoring; suggest it.

## Editing existing code

- **Do not change blank lines in code you are not modifying.** A whitespace diff makes review harder and dirties the history, even if the old code violates these principles. If it is worth fixing, mention it to the user and propose a separate commit.
- **What the formatter leaves open** is only **where a logical step ends** — that is the part this topic decides.

## Anti-patterns

| Anti-pattern                                                                                                      | Why it is bad                                                                                 |
| ----------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| **Wall** — validation, I/O and computation without separation                                                     | The reader rebuilds the steps alone                                                           |
| **Uniform spacing** — a line between every statement                                                              | If everything is separated, nothing is grouped                                                |
| **Empty frame** — line after `{` or before `}`                                                                    | Duplicates the delimiter's information                                                        |
| **Split block** — inseparable pair with a line in the middle                                                      | States a change of step that does not exist                                                   |
| **Space by syntax** — line before every single-line `if` or `return`, or between a declaration and its single use | Visual symmetry instead of steps; splits a step that has no scope of its own                  |
| **Glued block** — multi-line `if`/`for`/`while` attached to the line above or below                               | Hides where the scope starts and ends in the first scan; mixes the preparation with the block |
| **Hierarchy by quantity** — 2–3 lines for an "important section"                                                  | Each author reads it differently; use the language's structure                                |
| **Title comments inside functions**                                                                               | Indicates functions not extracted                                                             |
| **Whitespace diff** — touching lines of unrelated code                                                            | Pollutes the history and the review                                                           |

Review priority: the ones that hide structure (wall, split block, function with too many blocks) before the cosmetic ones (empty frame).

## Review checklist

- [ ] Each block has a single, short mental title
- [ ] No block needs two titles; no neighbouring pair shares the same one
- [ ] At most 1 blank line in a row (unless a language convention says otherwise)
- [ ] No blank line right after opening or before closing a block
- [ ] Variables declared next to the block that consumes them
- [ ] Inseparable pairs kept together
- [ ] Multi-line control blocks with a blank line right before the opening line and right after the closing one (except after `{`, before `}`, and between consecutive guards)
- [ ] Final `return` separated only when it delivers the product of earlier steps
- [ ] Consecutive guards in a single block
- [ ] Tests with exactly three blocks: Arrange, Act, Assert
- [ ] Functions with more than 3–4 blocks considered for extraction
- [ ] The diff does not change blank lines in unrelated code

## Delegation summary

Blank lines separate steps, not statements; a multi-line control block gets one before and after; never after `{` or before `}`; at most one in a row; consecutive guards stay together; no whitespace changes in untouched code.
