# Repository Agent Instructions

## Code style

- Preserve the author's existing style and make new code look native to the
  project. Before editing, inspect nearby code and relevant history, then match
  its naming, terminology, declaration order, control-flow idioms, indentation,
  line wrapping, and comment style.
- Do not impose a generic personal style or run broad automatic formatting.
  Keep unrelated existing formatting intact.
- Keep code clean, clear, simple, and easy to follow. Prefer the smallest
  direct solution that expresses the required behavior.
- Do not add speculative abstractions, redundant state, helper layers, wrapper
  types, flags, or fields when existing data already expresses the same fact.
- Keep class properties before methods, and place class declarations in header
  files, following the established project structure.
- Use tabs for indentation and preserve the surrounding brace and spacing
  conventions.
- In a multiline ternary expression, keep `?` and `:` at the ends of their
  respective lines:

```cpp
auto value = condition ?
	trueValue :
	falseValue;
```

- Prefer the project's concise names and existing vocabulary. Names must remain
  clear in context without unnecessary implementation-detail prefixes or
  suffixes.
- Use `std::format` for formatting and `spdlog` for logs. Do not introduce
  `printf`-style output or parallel debug helpers.
- In instruction-decoder tests and examples, comment fixture bytes with the
  actual assembly instruction they encode.

## Changes and verification

- Preserve unrelated worktree changes and keep edits scoped to the request.
- Add focused regression coverage for behavior changes and verify native and
  WebAssembly/TypeScript paths when shared code or public results change.
- Before reporting completion, review the complete diff and run
  `git diff --check`.
- Commit only when the user explicitly requests it.
