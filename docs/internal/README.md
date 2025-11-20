# YAZE Handbook

Internal documentation for planning, AI agents, research, and historical build notes. These
files are intentionally excluded from the public Doxygen site so they can remain verbose and
speculative without impacting the published docs.

## Sections
- `agents/` – z3ed and AI agent playbooks, command abstractions, and debugging guides.
- `blueprints/` – architectural proposals, refactors, and technical deep dives.
- `roadmaps/` – sequencing, feature parity analysis, and postmortems.
- `research/` – emulator investigations, timing analyses, web ideas, and development trackers.
- `legacy/` – superseded build guides and other historical docs kept for reference.
- `agents/` – includes the coordination board, personas, GH Actions remote guide, and helper scripts
  (`scripts/agents/`) for common agent workflows.

When adding new internal docs, place them under the appropriate subdirectory here instead of
`docs/`.

## Version Control & Safety Guidelines
- **Coordinate before forceful changes**: Never rewrite history on shared branches. Use dedicated
  feature/bugfix branches (see `docs/public/developer/git-workflow.md`) and keep `develop/master`
  clean.
- **Back up ROMs and assets**: Treat sample ROMs, palettes, and project files as irreplaceable. Work
  on copies, and enable the editor’s automatic backup setting before testing risky changes.
- **Run scripts/verify-build-environment.* after pulling significant build changes** to avoid
  drifting tooling setups.
- **Document risky operations**: When touching migrations, asset packers, or scripts that modify
  files in bulk, add notes under `docs/internal/roadmaps/` or `blueprints/` so others understand the
  impact.
- **Use the coordination board** for any change that affects multiple personas or large parts of the
  tree; log blockers and handoffs to reduce conflicting edits.
