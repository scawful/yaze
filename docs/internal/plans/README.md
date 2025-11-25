# Plan Directory Guide

Purpose: keep plan/spec documents centralized and up to date.

## How to use this directory
- One plan per initiative. If a spec exists elsewhere (e.g., `agents/initiative-*.md`), link to it instead of duplicating.
- Add a header to each plan: `Status`, `Owner (Agent ID)`, `Created`, `Last Reviewed`, `Next Review` (≤14 days), and link to the coordination-board entry.
- Keep plans short: Summary, Decisions/Constraints, Deliverables, Exit Criteria, and Validation.
- Archive completed/idle (>14 days) plans to `archive/` with a dated filename. Avoid keeping multiple revisions at the root.

## Staying in sync
- Every plan should reference the coordination board entry that tracks the same work.
- When scope or status changes, update the plan in place—do not create a new Markdown file.
- If a plan is superseded by an initiative doc, add a pointer and move the older plan to `archive/`.

## Current priorities
- Active release/initiative specs live under `docs/internal/agents/` (e.g., `initiative-v040.md`). Start there before drafting a new plan here.
- Active plans in this directory: `web_port_strategy.md`, `ai-infra-improvements.md`.
- Archived plans (partially implemented or completed reference documents): see `archive/plans-2025-11/` for historical context.

## Naming
- Avoid ALL-CAPS filenames except established anchors (README, AGENTS, GEMINI, CLAUDE, CONTRIBUTING, etc.). Use kebab-case for new plans.
