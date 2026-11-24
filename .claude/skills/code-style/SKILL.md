---
name: code-style
description: 'Personal code style standard at the syntax level, by topic: blank lines, comments (which stay, go out or become a name, type or constant), and other syntax patterns. Load BEFORE the first line of code of any task that writes, changes, plans or reviews code, of any size and language: one line, a rename, a bugfix, a test, a script, a whole feature. Invoking a design or implementation workflow skill (brainstorm, spec, plan, implement, review) is itself the trigger: load this one in the same batch, before any code exists. Also covers executing a plan task, refactoring, code snippets in answers, reviewing a diff or PR, delegating code to a subagent, readability, grouping of imports or members, Arrange/Act/Assert, formatter config (Prettier, Black, clang-format, gofmt, rustfmt, PHP-CS-Fixer), docstrings, public API docs and requests to comment, document or clean up code. Adds to language, project or workflow skills; loading one does not exempt this one. Skip only when no code is written, planned or reviewed.'
---

# Code style

This skill is the personal code style standard, at the level of syntax: how the code looks on the page, not how it is designed. It is organized by **topic**. This file holds what is common to every topic and the index; each topic's rules live in its own folder under `references/`.

What every topic shares: the reader scans the shape of the file first and reads the detail later, and no compiler checks style. A style error does not break the build — it misleads the reader silently. So each rule here exists to make the first reading true.

## Boundary

**Territory** — decides how code is written at the syntax level: vertical layout, comments, and each pattern listed in the topic index.

**Outside the boundary** — the architecture and design of the code, what the project's formatter already decides (indentation, line width, brace style), the format of commit messages, how commands are run in the repository, and the technical content of the decisions a comment records (that comes from the user or from measurement).

**Edge contract** — I receive code, diff, plan or config in any language; I deliver the same material in this style, or the rules in short text for whoever will write the code.

## Topic index

| Topic | Decides | Applies to | Files |
| --- | --- | --- | --- |
| Blank lines | where blank lines go in and out | every language with free vertical layout | `references/blank-lines/rules.md`, `references/blank-lines/philosophy.md` |
| Comments | whether a comment exists, becomes code or goes out; what public API docs and config files record | code, docstrings, public API, config files | `references/comments/rules.md`, `references/comments/philosophy.md` |

**How to read the topics.** For each topic whose "Applies to" matches the task, read its `rules.md` before writing, planning or reviewing the code. Read `philosophy.md` only when the rules do not settle a case or you need to justify a decision to the user.

## Rules for every topic

**The project's formatter wins.** If there is a `.clang-format`, `.prettierrc`, `pyproject.toml` (Black/Ruff), `.php-cs-fixer.php`, `rustfmt.toml`, `.editorconfig` etc., it prevails over personal preferences — consistency within the ecosystem is worth more. Apply this skill only to what the formatter does not decide.

**Do not restyle code you are not modifying.** A style-only diff in unrelated code makes review harder and dirties the history, even if the old code breaks these rules. If it is worth fixing, mention it to the user and propose a separate commit. In new or rewritten code within the edit, apply the rules normally.

**A style problem can be a design signal.** When a rule is hard to apply — a function with too many blocks, a comment that wants to be a title — the cause is usually the design. Say so; do not refactor on your own unless the request involves refactoring.

## How to act in each situation

**Writing new code.** Read the applicable topics, then write in the final style from the first draft. Start with names, types and structure; style is not a pass done at the end.

**Editing existing code.** Follow "Rules for every topic" and the "Editing existing code" section of each applicable topic.

**Reviewing code (diff/PR).** Use the review checklist of each applicable topic. Point out each problem by citing the violated rule ("split block: the opening and the error check form a single step"), not as personal taste, and propose the concrete fix (the new name, the moved blank line). Prioritize the problems that hide structure or state something false over the cosmetic ones, and do not ask for changes the project's formatter would already handle.

### Plans, specs and delegated tasks

The code that appears in a plan or a spec is real code: whoever executes the plan usually copies the block literally. A style error in the plan becomes the same error in the file. And whoever executes a delegated task works in an isolated context: they have not read this skill, so they only follow what is written in the task.

**Drafting the plan.**
- Write each code block in its final style, with every applicable topic applied. The block's names, types and constants already come out in their final form.
- Keep the steps ("Step 1", "now create X") in the prose, outside the block. Only what would stay in the file goes inside the block.
- When the plan chooses something non-obvious (algorithm, structure, layout, constant), write the reason in the task, next to the block. That way the implementer records it in the code instead of losing it.
- If the plan asks for public API documentation, say what it needs to cover (errors, side effects, preconditions).

**Delegating code writing.** Copy the "Delegation summary" of each applicable topic into the task text. Without it, the agent receiving the task uses its own default, which tends toward uniform spacing and echo comments.

**Executing a plan task.** The plan rules the behaviour, not the style. If the plan's block breaks a rule of this skill, do not copy it; apply the rule.

## Adding a topic

A topic is one syntax-level decision with its own rules — for example naming, ordering of members, error handling style, or the conventions of one language. To add one:

1. Check that it is syntax, not design. "How to name a boolean" is a topic; "when to create an interface" is not.
2. Check that no existing topic already decides it. If one does, extend that topic instead.
3. Create `references/<topic>/rules.md` from `references/TEMPLATE.md`. Add `philosophy.md` only when the long argument is worth keeping.
4. Add one row to the topic index above.
5. If the topic brings words a user would say when asking for it (a tool, a file type, a language), add them to the `description`.

A topic that applies to only one language says so in "Applies to"; it does not go into another topic's file.
