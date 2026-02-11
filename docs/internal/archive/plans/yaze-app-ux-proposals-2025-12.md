# YAZE Desktop App UX Proposals (Panels, Flags, ROM Loading)

Status: IN_PROGRESS  
Owner: ai-infra-architect  
Created: 2025-12-01  
Last Reviewed: 2025-12-01  
Next Review: 2025-12-08  
Board: docs/internal/agents/coordination-board.md (2025-12-01 ai-infra-architect – z3ed CLI UX/TUI Improvement Proposals)

## Summary
- Improve the desktop ImGui/SDL app startup, profiles, ROM onboarding, and layout/panel ergonomics separate from the z3ed CLI/TUI.
- Make service status (agent control, HTTP/collab, AI runtime) visible and scriptable; reduce brittle flag combinations.
- Provide safe ROM handling (recent list, mock fallback, hot-swap guard) and shareable layout presets for overworld/dungeon/graphics/testing workflows.

## Observations
- Startup relies on ad-hoc flags; no bundled profiles (dev/AI/ROM/viewer) and no in-app profile selector. Users must memorize combinations to enable agent control, autosave, collaboration, or stay read-only.
- ROM onboarding is brittle: errors surface via stderr; no first-run picker, recent ROM list, autodetect of common paths, or “use mock ROM” fallback; hot-swapping ROMs risks state loss without confirmation.
- Panel/layout presets are implicit—users rebuild layouts each session. No first-class presets for overworld, dungeon, graphics/palette, testing/doctor, or AI console. Export/import of layouts is absent.
- Runtime status is opaque: no unified HUD showing ROM title/version, profile/layout, mock-ROM flag, active services (agent/collab/HTTP), autosave status, or feature flags.
- Configuration surfaces are fragmented; parity between CLI flags and in-app toggles is unclear, making automation brittle.

## Improvement Proposals
- **Profiles & bundled flags**: Add `--profile {dev, ai, rom, viewer, wasm}` with an in-app profile picker. Each profile sets sane defaults (agent control on/off, autosave cadence, mock-ROM allowed, telemetry, collaboration) and selects a default layout preset. Persist per-user.
- **ROM onboarding & recovery**: Show a startup ROM picker with recent list and autodetect (`./zelda3.sfc`, `assets/zelda3.sfc`, env var). Validate and, on failure, offer retry/browse and “Use mock ROM” instead of exiting. Add `--rom-prompt` to force picker even when a path is supplied for shared environments.
- **Layout presets & persistence**: Ship named presets (Overworld Editing, Dungeon Editing, Graphics/Palette, Testing/Doctor, AI Agent Console). Provide `--layout <name>` and an in-app switcher; persist per profile and allow export/import for handoff.
- **Unified status HUD**: Add an always-visible status bar/dashboard summarizing ROM info, profile/layout, service state (agent/HTTP/collab), mock-ROM flag, autosave recency, and feature flags. Expose the same state via a lightweight JSON status endpoint/command for automation.
- **Safer ROM/context switching**: On ROM change, prompt with unsaved-change summary and autosave option; offer “clone to temp” for experiments; support `--readonly-rom` for analysis sessions.
- **Config discoverability**: Centralize runtime settings (ROM path, profile, feature toggles, autosave cadence, telemetry) in a single pane that mirrors CLI flags. Add `--export-config`/`--import-config` to script setups and share configurations.

## Exit Criteria (for this scope)
- Profiles and layout presets are selectable at startup and in-app, with persisted choices.
- ROM onboarding flow handles missing/invalid ROMs gracefully with mock fallback and recent list.
- Status HUD (and JSON endpoint/command) surfaces ROM/profile/service state for humans and automation.
- Layouts are exportable/importable; presets cover main workflows (overworld, dungeon, graphics, testing, AI console).
