# Documentation Hygiene & Spec Rules

Purpose: keep `docs/internal` lean, discoverable, and aligned with active work. Use this when creating or updating any internal spec/plan.

## Canonical sources first
- Check existing coverage: update an existing spec before adding a new file. Search `docs/internal` for keywords and reuse templates.
- One source of truth per initiative: tie work to a board entry and, if multi-day, a single spec (e.g., `initiative-*.md` or a plan under `docs/internal/plans/`).
- Ephemeral task lists live in `z3ed agent todo` or the coordination board, not new Markdown stubs.

## Spec & plan shape
- Header block: `Status (ACTIVE/IN_PROGRESS/ARCHIVE)`, `Owner (Agent ID)`, `Created`, `Last Reviewed`, `Next Review` (≤14 days by default), and link to the coordination-board entry.
- Keep the front page tight: Summary, Decisions/Constraints, Deliverables, and explicit Exit Criteria. Push deep design notes into appendices.
- Use the templates: `initiative-template.md` for multi-day efforts, `release-checklist-template.md` for releases. Avoid custom one-off formats.

## Lifecycle & archiving
- Archive on completion/abandonment: move finished or idle (>14 days without update) specs to `docs/internal/agents/archive/` (or the relevant sub-archive). Add the date to the filename when moving.
- Consolidate duplicates: if multiple docs cover the same area, merge into the newest spec and drop redirects into the archive with a short pointer.
- Weekly sweep: during board maintenance, prune outdated docs referenced on the board and archive any that no longer have an active entry.

## Coordination hygiene
- Every spec links back to the coordination board entry; every board entry points to its canonical spec. No parallel shadow docs.
- Release/initiative docs must include a test/validation section instead of ad-hoc checklists scattered across files.
- When adding references in other docs, point to the canonical spec instead of copying sections.

## Anti-spam guardrails
- No gamified/leaderboard or duplicative status pages in `agents/`—keep status in the board and the canonical spec.
- Prefer updating `docs/internal/README.md` or the nearest index with short summaries instead of creating new directories.
- Cap new doc creation per initiative to one spec + one handoff; everything else belongs in comments/PRs or the board.
- Filenames: avoid ALL-CAPS except established anchors (README, AGENTS, GEMINI, CLAUDE, CONTRIBUTING, etc.); use kebab-case for new docs.
