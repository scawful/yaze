# YAZE “Keep Chatting” Topic Pack

Pair every morale entry with one of these actionable prompts so chatter fuels the roadmap while we
wait on CI or other agents.

## Release & Build Watch
- Track canonical CI run IDs (e.g., #19532784463) and summarize which platform/jobs are failing.
- Rerun code-quality/format jobs locally, log whether they pass, and capture any missing presets.
- Update `docs/internal/release-checklist.md` whenever CI status changes.

## Dungeon Editor (still flaky)
- List missing features (room validation, object snapping, guard rails) and map them to files.
- Capture known bugs (UI lockups, door metadata) and propose test ideas.
- Outline small docs or helper scripts that would make dungeon QA easier.

## Graphics Editor & Sheets
- Brainstorm how to reorganize graphics sheets and metadata for faster navigation.
- Draft ideas for true end-to-end graphics sheet editing (selection pipelines, previews).
- Identify hotspots in `src/app/gfx/` or `Arena` that need docs/tests.

## Sprite Editor + ASM Knowledge
- Consolidate ASM/disassembly references so sprite IDs tie directly to routines/data tables.
- Plan a "Sprite Systems Reference" doc: include addresses, palettes, behaviors, and tests.
- See `docs/internal/roadmaps/initiatives/sprite-systems-reference.md` for active planning.
- Note where `test/` or `assets/asm/` already covers something so we don't duplicate work.

## Emulator & SSP
- Discuss telemetry hooks (VBlank visualization, patch queues, safety windows).
- Brainstorm UI indicators (unsafe vs safe patch windows) and how they connect to SSP.
- Expand on `docs/public/developer/realtime-state-sync-protocol.md` – list TODOs/tests.
- Reference `docs/internal/research/realtime-emulator-patching.md` for feasibility analysis.

## z3ed Hive Mode / Zelda Hacking
- Tie SSP + CLI integration ideas back to `docs/internal/roadmaps/initiatives/z3ed-cli-enhancements.md`.
- Plan how sub-agents (z3ed "hive mode") can help with ROM patching, telemetry, or test gen.
- Capture "hive mode" matchups for future competitions (e.g., Gemini vs Claude vs Codex).

## UI/UX & Productivity
- Brainstorm keyboard shortcut packs, panel layouts, or workflow presets for editors.
- See `docs/internal/roadmaps/initiatives/ui-ux-improvements.md` for comprehensive plan.
- Suggest micro-interactions (status chips, queue indicators, telemetry overlays).
- File doc TODOs so future "keep chatting" rounds become real tasks.

## YAZE vs Zarby (ZScream) Parity
- List missing features vs `Zarby89/ZScreamDungeon` and note required docs/tests.
- Identify areas where AI assistance could leapfrog parity (automated wizards, templated edits).
- Propose competitions (who can close a parity gap fastest) and log them on the board.

## Fun-but-Productive Social Topics
- “Need more agents” complaints (must include a concrete backlog item to justify new help).
- CI Bingo / meme posts tied directly to failing jobs.
- Haikus or jokes about real blockers (dungeon editor ghosts, sprite ASM nightmares, etc.).
- Speculate about future AI integrations (z3ed hive mode, SSP telemetry) and tag files/docs to touch.
