# Editor Review Sweep (2026-01)

Status: ACTIVE
Owner: imgui-frontend-engineer
Created: 2026-01-23
Last Reviewed: 2026-01-23
Next Review: 2026-02-06

## Summary
- Lead agent/persona: imgui-frontend-engineer
- Supporting agents: snes-emulator-expert (emulator/editor-side bugs), zelda3-hacking-expert (ROM data flows)
- Problem statement: We need a systematic, tracked review of every editor surface to confirm behavior, layout defaults, and mobile/desktop parity after recent UI + layout persistence changes.
- Success metrics: Every editor marked reviewed with notes, issues logged with actionable follow-ups, and nightly builds validated against the sweep.

## Scope
- In scope: All EditorType surfaces (Overworld, Dungeon, Graphics, Palette, Screen, Sprite, Music, Message, Assembly, Hex, Emulator, Agent, Settings), editor selection dialog, panel layouts, dockspace defaults, editor load/asset flow, and mobile size behavior.
- Out of scope: New feature work beyond fixes found during the sweep.
- Dependencies / upstream projects: yaze UI/panel system, layout persistence, ROM assets for ROM-dependent editors.

## Risks & Mitigations
- Risk: Editor regressions hidden by stale layout state. Mitigation: reset per-editor layout, verify with clean layout file, and document exact repro steps.
- Risk: ROM-dependent editors require data. Mitigation: use known-good ROM (oos/alttp) and record required ROM/version in notes.

## Testing & Validation
- Required test targets: manual editor open + panel render checks; smoke-build if code changes.
- ROM/test data requirements: Known-good ROM (per editor requirements), project config if needed.
- Manual validation steps:
  - Switch to editor from dashboard.
  - Verify panels render, inputs respond, and dockspace layout loads.
  - Toggle panel visibility and confirm persistence across editor switching.
  - Validate key editor-specific functionality (load/save, palette previews, rendering accuracy).

## Documentation Impact
- Public docs to update: changelog entries for any user-visible fixes.
- Internal docs/templates to update: this initiative file + coordination board entry.
- Coordination board entry link: docs/internal/agents/coordination-board.md
- Helper scripts to use/log: scripts/agents/smoke-build.sh (if applicable).

## Timeline / Checkpoints
- Milestone 1 (2026-01-23): Establish tracking checklist + confirm baseline for Overworld/Message.
- Milestone 2 (2026-01-30): Complete review of remaining editors and record fixes.

## Tracking Checklist

Legend: TODO | IN_PROGRESS | DONE | BLOCKED

| Editor | Status | Last Reviewed | Notes |
| --- | --- | --- | --- |
| Overworld | DONE | 2026-01-23 | Layout persistence + panel activation validated. |
| Message | DONE | 2026-01-23 | Message preview + font atlas fix verified. |
| Dungeon | IN_PROGRESS | 2026-01-23 | Removed unused Dungeon Controls panel references; default layout now centers Room Matrix with Room List left; needs runtime validation. |
| Graphics | IN_PROGRESS | 2026-01-23 | Removed polyhedral/prototype panels from default roster; aligned layout preset IDs with actual panel IDs (sheet browser v2, pixel editor, link sprite); added palette/gfx group/paletteset optional layout slots; needs runtime validation. |
| Palette | IN_PROGRESS | 2026-01-23 | Replaced hardcoded warning/error/info TextColored with theme colors; removed local panel-manager/minimize UI; needs runtime validation + check alpha/initial palette values (w=255) in DisplayPalette. |
| Screen | IN_PROGRESS | 2026-01-23 | Replaced hardcoded error colors + boss-room outline with theme colors; needs runtime validation of Title/Overworld/Dungeon screens + naming screen stub. |
| Sprite | TODO | - | - |
| Music | IN_PROGRESS | 2026-01-23 | Removed Audio Debug + Help panels from panel roster; needs runtime validation. |
| Assembly | TODO | - | - |
| Hex (Memory) | TODO | - | - |
| Emulator | TODO | - | - |
| Agent | TODO | - | - |
| Settings | TODO | - | - |

## Issues Log

- (empty)
