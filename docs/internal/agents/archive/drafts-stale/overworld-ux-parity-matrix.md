# OverworldEditor UX Parity: ZScreamDungeon vs yaze

Status: Draft  
Owner (Agent ID): `ai-infra-architect`  
Created: 2026-02-07  
Last Reviewed: 2026-02-07  
Next Review: 2026-02-21  

Exit criteria
- yaze has a ZScream-style "fill screen" tool wired in UI and shortcuts.
- ROM saving is resilient to partial writes (temp write + rename).
- This doc lists the top UX parity gaps and the next concrete actions.

## What ZScream Does Differently (High Level)
ZScream's OverworldEditor is tool-mode-forward (explicit Pen vs Fill), and it
leans on a persistent scratch pad workflow for rapid iteration.

yaze's OverworldEditor has stronger ROM-version awareness and customization
surfaces (ZSCustom v1-v3 feature gating, upgrade path, map properties panels),
but bulk tile workflows and tool discoverability lag.

## Parity Matrix (Overworld Editor)

| Capability | ZScreamDungeon OverworldEditor | yaze OverworldEditor | Notes / Improvement |
|---|---|---|---|
| Tool modes | Mouse, Pen, Fill (plus other scene modes) | Mouse, Brush (tile paint), Fill Screen | Keep explicit modes in toolbar; ensure shortcuts are discoverable. |
| Fill tool semantics | Fills the entire 32x32 tile16 "screen" with selected tile or repeating pattern (not flood fill) | Implemented as Fill Screen (fills hovered map screen) | Add a separate flood-fill (region) later; do not overload Fill Screen. |
| Pattern fill | Uses selection as repeating pattern in Fill mode | Uses selection rect as repeating pattern in Fill Screen | Add preview: highlight target screen and show pattern bounds. |
| Brush preview | Shows tile preview under cursor in Pen mode | Brush preview in DrawTile mode | Consider a different cursor/preview for Fill Screen to avoid confusion. |
| Eyedropper | Right-click sampling behavior (common in ZScream editors) | Right-click/selection workflows exist but are less explicit | Add a dedicated eyedropper tool or explicit shortcut (e.g. `I`) for clarity. |
| Scratch pad | Persistent `ScratchPad.dat` | Scratch space exists but is session-scoped | Persist scratch to project-local file (per project or per ROM) to match ZScream iteration loop. |
| Undo/redo | Mature undo/redo stacks per mode | Undo batching exists for tile paint; paste undo TODO | Batch undo for Fill Screen and Paste as single operations (one stack entry). |
| Screen selection | Strong screen-centric workflows | Map lock + map properties panel exist | Add "Operate on current screen" affordances (fill, replace, swap palettes) to reduce hunting. |
| ROM hack guardrails | N/A (editor-centric) | HackManifest write-conflict checks + project `rom_metadata.write_policy` | Keep guardrails on by default (`warn`) and surface "why blocked" with a link to the conflicting region source. |

## Save Stability (yaze)

Current improvement (2026-02-07)
- Atomic ROM write: write to `*.tmp` and rename into place to reduce corruption
  risk on crash/power loss during saves.
- Best-effort `fsync`: flush temp file and parent directory where supported to
  improve durability of the rename.

Next actions
1. Surface "last save path" and "save status" consistently in UI to reduce
   accidental saves to unexpected locations.

## ROM Hack Customization (yaze)

Existing strengths to build on
- ZSCustom overworld version detection and upgrade UX (v1-v3).
- HackManifest-based labels and write-policy guardrails for ASM-owned regions.

Next actions
1. Make project write policy visible and editable from the editor save dialog
   (allow, warn, block) with a short explanation.
2. Provide a "safe save" report: list planned write ranges and any conflicts
   (even when policy is `allow`).
