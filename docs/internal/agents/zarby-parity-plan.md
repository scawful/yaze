# YAZE vs Zarby (ZScream) Parity Plan

Topic: **YAZE vs Zarby Parity** from the keep-chatting prompts.

## Purpose
Track feature gaps between YAZE and Zarby89/ZScreamDungeon so agents can tackle small,
competition-friendly tasks that close the delta while keeping the scoreboard spicy.

## Snapshot (2025-11-21)

| Area | Zarby Status | YAZE Status | Gap / Opportunity |
|------|--------------|-------------|-------------------|
| Dungeon Editor | Mature object snapping, door metadata UI | YAZE has partial rewrite; snapping/metadata inconsistent | Implement snapping guard rails + metadata panels (see `src/app/editor/dungeon/`) |
| Sprite Editor | Sprite ASM crosswalks built-in | YAZE lacking ASM reference | Build Sprite Systems Reference doc + parser (see `sprite-systems-reference-plan.md`) |
| Graphics Editor | Sheet browser with categories | YAZE flat list | Implement sheet tagging + search (hook into `gfx::Arena`) |
| UI Presets | Zarby ships default layouts per editor | YAZE manual docking | Execute `ui-ux-refresh-plan.md` presets task |
| Automation/AI | Limited | YAZE has z3ed + hive mode | Leverage advantage—document APIs + tests |

## Action Items
1. **Gap Catalog**
   - Pull concrete examples from Zarby docs/screens to illustrate missing UX (need volunteers with access).
   - Map each gap to YAZE files/tests.
2. **Point Bounties**
   - Award +25 pts for documenting a gap, +75 pts for shipping a measurable improvement.
3. **Competition Hooks**
   - Host “Parity sprints” where agents race to tick items off this doc.
4. **Docs & Reporting**
   - Update this plan whenever a gap closes; link releases or board entries as receipts.

## Next Volunteers Needed
- **Dungeon snapping audit** – CLAUDE_CORE?
- **Graphics tagging prototype** – TBD (maybe Gemini Autom once CI cools).
- **Zarby screenshot comparison** – Codex Mini morale task when bored.
