# Plan Directory Guide

Purpose: keep plan/spec documents centralized and up to date.

## How to use this directory
- One plan per initiative. If a spec exists elsewhere (e.g., `agents/initiative-*.md`), link to it instead of duplicating.
- Add a header to each active plan: `Status`, `Owner (Agent ID)`, `Created`, `Last Reviewed`, `Next Review` (≤14 days), and the universe task ID.
- Keep plans short: Summary, Decisions/Constraints, Deliverables, Exit Criteria, and Validation.
- Archive completed/idle (>14 days) plans to `archive/` with a dated filename. Avoid keeping multiple revisions at the root.

## Staying in sync
- Every active plan should reference its universe task. Use `scripts/agents/coord`; the legacy markdown board is historical.
- When scope or status changes, update the plan in place—do not create a new Markdown file.
- If a plan is superseded by an initiative doc, add a pointer and move the older plan to `archive/`.

## Current priorities
- Start with the [editor capability parity plan](editor-capability-parity-plan.md)
  for the Hyrule Magic / ZScream baseline, work-package IDs, dependencies,
  implementation instructions, and acceptance. `DA-1` through `DA-5` define
  the active 0.8.0 dungeon authoring sequence.
- The [roadmap](../roadmap.md) owns priorities; the
  [release ladder](release-ladder-0x-2026.md) maps later editor milestones.
- The [dungeon completion backlog](dungeon-0.8.0-issue-test-backlog-2026-06-28.md)
  preserves detailed rendering evidence and `DA-5` qualification packets.
  Dated test counts are evidence for those commits, not current release gates.
- Some older initiative specs live under `docs/internal/agents/`. Reuse an
  existing active owner before creating a second plan for the same scope.
- Active plans in this directory include:
  - [Editor capability parity](editor-capability-parity-plan.md)
  - `release-ladder-0x-2026.md`
  - `z3dk-integration-0.8.0.md`
  - `oracle-yaze-integration.md`
  - `ai-infra-improvements.md`
  - `yaze-beta-feedback-backlog-2026-07-01.md`
- The [HM / Parallel Worlds proposal](hyrule-magic-support-plan.md) is historical
  research with unverified layout assumptions. Follow `CO-1` before using it.
- Archived plans: see [../archive/](../archive/) for historical context.

## Naming
- Avoid ALL-CAPS filenames except established anchors (README, AGENTS, GEMINI, CLAUDE, CONTRIBUTING, etc.). Use kebab-case for new plans.
