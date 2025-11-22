# Contributing to YAZE

The YAZE project reserves **master** for promoted releases and uses **develop**
for day‑to‑day work. Larger efforts should branch from `develop` and rebase
frequently. Follow the existing Conventional Commit subject format when pushing
history (e.g. `feat: add sprite tab filtering`, `fix: guard null rom path`).

The repository ships a clang-format and clang-tidy configuration (Google style,
2-space indentation, 80-column wrap). Always run formatter and address
clang-tidy warnings on the files you touch.

## Engineering Expectations

1. **Refactor deliberately**
   - Break work into reviewable patches.
   - Preserve behaviour while moving code; add tests before/after when possible.
   - Avoid speculative abstractions—prove value in the PR description.
   - When deleting or replacing major systems, document the migration (see
     `handbook/blueprints/editor-manager-architecture.md` for precedent).

2. **Verify changes**
   - Use the appropriate CMake preset for your platform (`mac-dbg`, `lin-dbg`,
     `win-dbg`, etc.).
   - Run the targeted test slice: `ctest --preset dev` for fast coverage; add new
     GoogleTest cases where feasible.
   - For emulator-facing work, exercise relevant UI flows manually before
     submitting.
   - Explicitly call out remaining manual-verification items in the PR.

3. **Work with the build & CI**
   - Honour the existing `cmake --preset` structure; avoid hardcoding paths.
   - Keep `vcpkg.json` and the CI workflows in sync when adding dependencies.
   - Use the deferred texture queue and arena abstractions rather than talking to
     SDL directly.

## Documentation Style

YAZE documentation is concise and factual. When updating `docs/public/`:

- Avoid marketing language, emojis, or decorative status badges.
- Record the actual project state (e.g., mark editors as **stable** vs
  **experimental** based on current source).
- Provide concrete references (file paths, function names) when describing
  behaviour.
- Prefer bullet lists and short paragraphs; keep quick-start examples runnable.
- Update cross-references when moving or renaming files.
- For review handoffs, capture what remains to be done instead of transfusing raw
  planning logs.

If you notice obsolete or inaccurate docs, fix them in the same change rather
than layering new “note” files.

## Pull Request Checklist

- [ ] Tests and builds succeed on your platform (note the preset/command used).
- [ ] New code is formatted (`clang-format`) and clean (`clang-tidy`).
- [ ] Documentation and comments reflect current behaviour.
- [ ] Breaking changes are mentioned in the PR and, if relevant, `docs/public/reference/changelog.md`.
- [ ] Any new assets or scripts are referenced in the repo’s structure guides.

Respect these guidelines and we can keep the codebase approachable, accurate,
and ready for the next set of contributors.
