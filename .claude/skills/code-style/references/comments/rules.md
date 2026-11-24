# Comments

Code expresses **what** happens; the comment exists only for what does not fit in the syntax. Name, type, signature, structure, assertion and test are checked by some tool. The comment is the only artifact in the repository that nobody checks: when it diverges from reality, it does not break — it lies silently, with the same authority it had when it was true.

So every comment is one of two things:

1. **A failure of the code to express itself** → fix the code (rename, extract, type) and delete the comment.
2. **Information the language cannot carry** → the comment is the only place where it exists; write it well and preserve it.

Confusing the two produces both polluted code and mute code. The goal is not "few comments" nor "many": it is zero comments of type 1 and all of type 2.

**Applies to:** code in any language, docstrings, public API documentation, configuration files, and code blocks inside plans and specs.

**Philosophy:** `philosophy.md` has the full argument (verification asymmetry, answers to objections). Read it when you need to justify a decision to the user or settle a case this file does not cover.

## The decision test

Before writing a comment, ask two questions, in this order:

**1. Can this information be moved into the code?** Try, in this order:

| Form | Eliminates comments about |
| --- | --- |
| Rename | what the variable/function is or does |
| Extract function | what a block does (the comment becomes the name) |
| Name a constant | the meaning of a literal (`86400` → `SECONDS_PER_DAY`) |
| Strengthen the type | unit, range, meaning (`Milliseconds` instead of `int`) |
| Enum | the possible values of a field |
| Assertion | invariants that must hold at run time |
| Test with a descriptive name | expected behaviour |

If any of them works, apply it and do not comment.

**2. If it cannot, does the information exist somewhere else in the file?** If it does not, comment. If it already does (it is in the code, the name, the type), the comment is echo — do not write it.

When nothing in the table works, that is not a failure of whoever writes: it is a limit of the language.

## What to delete (or never write)

The common test: **if deleting the comment removes no information from the repository, it was echo.**

- **Paraphrase of the line** — `// increments the counter` above `++count`.
- **Paraphrase of the function** — `// returns true if of legal age` above `check(a)`. That calls for `is_adult(age)`, not a comment.
- **Signature redundancy** — `@param int x`, `@returns {string}` when the type is already declared.
- **Commented-out code** — version control exists for that.
- **Ceremonial noise** — author/date header, ASCII dividers, `// end of if`, `// constructor`, inline changelog.
- **Empty markers** — `TODO`, `NOTE`, `IMPORTANT` with no concrete action, reason or ticket.

### Agent-specific traps

Agents tend to produce their own class of echo comments. Do not write:

- **Narration of the edit** — `// changed to use the new cache`, `// fixed the bug`, `// added error handling`. That describes the change, not the code; it belongs in the commit message or in the answer to the user, and it becomes false the very next moment.
- **Comments addressed to the user of the conversation** — `// here you can adjust the value`, `// as requested`. The reader of the file is not the user of this session.
- **Tutorial step by step** — `// Step 1: read the file`, `// Step 2: process`. If the flow needs labels, extract functions with those names.
- **Docstring on every private function** just because the style "looks complete". Documenting everything hides what matters.
- **Placeholders** — `// implement logic here`, `// ...rest of the code`. Either implement it, or tell the user what is missing.

## What must exist

Information that is nowhere else in the file. Deleting it is loss, not cleanup.

- **Reason for the choice** — why this solution and not the obvious alternative. Without it, someone reintroduces the alternative thinking they improved the code.
  ```
  // Sequential on purpose: the parallel version lost in the tests because the
  // synchronization cost dominates for batches < 10k items.
  ```
- **Measured number** — origin, condition and date. A number without a date cannot be audited or invalidated.
  ```
  // 64 = saturation point measured in 2026-03; above it lock contention
  // cancels the gain. Redo the measurement if the worker pool changes.
  ```
- **Deliberately less readable code** — SoA instead of AoS, unrolled loop, alignment padding, linear search where a map would fit. Record the measurement that justifies it: a performance regression is the class of bug CI does not catch, and the "clean" form is exactly what a refactorer will want to restore.
- **External constraint** — hardware, erratum, protocol, ceiling of a neighbouring service, regulation.
- **Non-local invariant** — order required because another module does binary search, write/read pairing in concurrent code, mandatory call order. Prefer an assertion; comment when that is not possible.
- **Workaround** — reference to the bug and removal condition. Without an exit criterion, it becomes permanent by omission.
  ```
  // Workaround for bug #4821 in lib X (breaks in 2.1.0, fixed in 2.3.0).
  // Remove when the minimum version goes up to 2.3.
  ```
- **Deliberate violation of a convention** — the comment is what separates decision from carelessness.

### How to write so it lasts

The closer the comment is to what the code already says, the faster it rots. A paraphrase breaks at the next edit; a reason survives whole rewrites.

- Write at the level of the **decision**, not of the instruction.
- **Date** measurements.
- Give an **exit criterion** to what is temporary.
- **Point** to the ticket/ADR/issue instead of copying its content.
- **Place it at the point of risk** — next to the part someone will want to "clean up", not at the top of the file.

**Never invent the origin of a number or the reason for a choice.** A comment with a fabricated measurement is worse than none: it has all the authority of a record and none of the truth. If you do not know where a value you found came from, do not write a plausible justification — ask the user or leave an actionable TODO (`// TODO: origin of 64 unknown; measure pool saturation before changing`). If you chose the value yourself without measuring, say so in the comment (`// initial value, not measured; calibrate with benchmark X`).

## Public API is another category

Public interface documentation is a **contract**, read by people who will never open the file (editor, autocomplete, generated docs, `help()`). The "delete echo" rules do not apply the same way: there the documentation is the only visible surface.

Document what the signature does not say:

- error semantics (what it throws, when, what the caller should do);
- null/empty contract (absence, error, legitimate result?);
- side effects (disk, network, argument mutation, global state);
- pre- and postconditions;
- performance/concurrency guarantees when they are part of the contract (complexity, thread-safety, reentrancy);
- resource ownership (who frees what — critical in C/C++);
- example, when correct usage is not obvious.

Two limits: document the **public** interface, not every private function; and do not repeat what the type already says.

## Configuration files

Configuration has no code to refactor — only key and value. `max_connections` already has the best possible name, and still `64` does not explain itself: it may be a measurement, an infrastructure limit, an agreement with another system or an old guess, and that difference is what decides whether someone may touch it.

Two layers:

1. **What the field is** (type, range, whether required, default) → schema validated at load (JSON Schema, config type with validation). That is internalizing: it fails at startup, not in production.
2. **Why this value** → comment with origin and change condition.
   ```
   # 64 = ceiling of the database pool in production. Raising it here without raising it there
   # makes the application queue instead of failing fast.
   max_connections = 64
   ```

Strict JSON does not accept comments: the reason goes into the schema (`description`) or into a neighbouring documentation file. Per-environment values: the reason stays at the source, not replicated in each environment.

## Editing existing code

- **Comments touched by your change must remain true.** Update or remove the ones that became false; changing code without changing the comment leaves the file inconsistent.
- **Do not do a broad comment cleanup outside the requested scope.** If you notice many echo comments, mention it to the user instead of rewriting the whole file unasked.
- **Before deleting, ask: *where else is this information recorded?*** If you cannot answer, you are not cleaning up — you are deleting the only copy. When in doubt about a comment that seems to explain a reason, measurement or constraint, preserve it. Converting into code (name, constant, type, assertion) is always better than just deleting.

## When the user asks "comment this code"

Do not add line-by-line paraphrases — that produces exactly the noise described above. Apply the decision test: improve names where the code does not explain itself, comment reasons, measurements, constraints and invariants, and document the public API. If the request is clearly didactic (the user wants to understand the code, not maintain it), explain in the conversation reply instead of polluting the file — or ask which of the two they want.

## Conversion example

Before:
```cpp
// checks whether the buffer is full
// 4096 = maximum size
bool check(const Buf& b) { return b.n >= 4096; }

for (...) {
    // process entities
    ...
}
```

After:
```cpp
// 4096 = target page size; larger buffers cross pages and the
// flush loses ~30% of throughput (measured in 2026-05, x86-64, Linux 6.x).
constexpr std::size_t kMaxBufferBytes = 4096;

bool is_full(const Buffer& buffer) { return buffer.size >= kMaxBufferBytes; }

process_entities(world);
```

The paraphrase of `check` became a name; the literal became a named constant; the loop label became a function. The only remaining comment carries what no name expresses: where 4096 came from and when to revalidate.

When pointing out a comment problem in review, propose the concrete internalized form like this (the new name, the constant, the type), not just "remove this comment".

## Review checklist

- [ ] Were comments that describe what the code does removed or converted into a name, type, constant or test?
- [ ] Would each remaining comment survive a rewrite of the implementation?
- [ ] Do the comments touched by the diff remain true?
- [ ] Are optimized code, non-obvious structure or convention violations justified?
- [ ] Do calibrated numbers carry origin and date?
- [ ] Do workarounds carry a reference and a removal condition?
- [ ] Does new/changed public API document errors, side effects and preconditions?
- [ ] Is there leftover commented-out code, divider, authorship header or empty marker?

## Delegation summary

Comments only for reason, measurement, external constraint or non-local invariant; what the code can say goes into a name, type or constant; no narration of the edit, no step by step, no placeholders; never invent the origin of a number; a comment touched by the change remains true.
