# ContentRegistry Panel Scope + Frame Event Split (Tier 1 Panel Porting)

Status: Proposed  
Owner: imgui-frontend-engineer  
Created: 2026-01-19  
Last Reviewed: 2026-01-19  
Next Review: 2026-02-02  
Exit Criteria:
- ContentRegistry panels register descriptors for every session (no missing panels after switching).
- Global panels remain visible across session switches without duplication.
- Deferred actions run after ImGui frame start (no ImGui assertions or layout regressions).
- Tier 1 panels are registered via ContentRegistry (no manual registration in editors).

## Summary
We need two foundational changes before broad ContentRegistry adoption:
1) **Panel scope + session registration** so registry panels appear in every session and global panels remain shared.
2) **Frame event split** so deferred ImGui calls execute after `ImGui::NewFrame`/dockspace creation.

This doc also outlines the Tier 1 panel porting workflow so Claude can move panels over safely.

## Scope
In scope:
- Add a global vs session panel scope and register descriptors for all sessions.
- Split panel instance registration from descriptor registration.
- Fix frame event ordering for deferred ImGui actions.
- Tier 1 panel porting checklist and candidate list.

Out of scope:
- Unified View hierarchy or plugin system.
- Broad EventBus refactors beyond frame events.
- Per-session panel instances (beyond scope for Tier 1).

## Workstream A: Panel Scope + Session Registration

### Proposed API Changes
- Add `enum class PanelScope { kSession, kGlobal };`
- Add `virtual PanelScope GetScope() const { return PanelScope::kSession; }`
  to `EditorPanel`.
- Extend `PanelDescriptor` (or registration path) to respect scope:
  - `kSession`: panel IDs are session-prefixed.
  - `kGlobal`: panel IDs are not prefixed (shared across sessions).

### Registration Flow (Target)
1) **Register instances once** (ContentRegistry panel factories create panel instances).
2) **Register descriptors per session**:
   - On session creation, register descriptors for all session-scoped panels.
   - On session switch, ensure descriptors exist (no-op if already created).
3) **Register global descriptors once** for `PanelScope::kGlobal` panels.

### Implementation Notes
- Add a `PanelManager::RegisterPanelInstance(std::unique_ptr<EditorPanel>)`
  that only stores the panel instance.
- Add a `PanelManager::RegisterDescriptorForSession(size_t session_id, const EditorPanel&)`
  that creates `PanelDescriptor` entries and visibility flags without re-owning
  the panel instance.
- Update `PanelManager::RegisterPanel(...)` to bypass prefixing for global scope.
- Update `ContentRegistry::Panels::CreateAll()` to copy factories under the mutex
  and invoke them outside the lock (avoid deadlocks when constructors read Context).

### Session Integration
- In `EditorManager` (or `SessionCoordinator`), call the new descriptor registration:
  - On app start: register descriptors for the initial session.
  - On `SessionCreatedEvent`: register descriptors for the new session.
  - On `SessionSwitchedEvent`: ensure descriptors exist (or lazy-create on switch).
- Fix `SessionSwitchedEvent.old_index` (currently uses updated active index).
- On closing the active session, emit a switch event for the new active session
  so ContentRegistry/RightPanel state stays in sync.

## Workstream B: Frame Event Split (Deferred ImGui Safety)

### Goal
Ensure deferred actions that call ImGui (e.g., `ImGui::GetID`, DockBuilder)
run after ImGui frame setup and dockspace creation.

### Proposed Event Flow
- Add `FramePreUpdateEvent` (no ImGui usage; published in `Application::Tick`).
- Add `FrameGuiBeginEvent` (published in `Controller::OnLoad` after:
  `ImGui::NewFrame` and dockspace window creation).
- Move deferred action processing (`LayoutCoordinator`, `EditorManager`) to
  `FrameGuiBeginEvent`.

### Migration Steps
- Update `Application::Tick` to publish only pre-frame events.
- Publish `FrameGuiBeginEvent` after the dockspace window is created.
- Update subscribers to use the new event.

## Tier 1 Panel Porting (Claude Checklist)

### Tier 1 Criteria
- Default-constructible panel OR convertible to use `ContentRegistry::Context`.
- Minimal per-session state (safe to read active session data each draw).
- No editor-owned pointers required in constructor.

### Porting Steps (Claude)
1) Make panel default-constructible if possible.
2) Replace direct editor pointers with `ContentRegistry::Context` access.
3) Add `REGISTER_PANEL(...)` or `REGISTER_PANEL_FACTORY(...)`.
4) Remove manual `RegisterEditorPanel(...)` calls in editor initialization.
5) Set `GetScope()` if the panel should be global.
6) Validate panel visibility across session switches.

### Tier 1 Candidates (Start Here)
| Panel | Scope | Status | Notes |
| --- | --- | --- | --- |
| AboutPanel | Global | REGISTER_PANEL done | Set `GetScope()` to `kGlobal`. |
| UsageStatisticsPanel | Session | REGISTER_PANEL done | Verify per-session descriptor registration. |
| DebugWindowPanel | Session | REGISTER_PANEL done | Verify per-session descriptor registration. |
| V3SettingsPanel | Session | REGISTER_PANEL done | Verify per-session descriptor registration. |

Next wave (after scope changes land):
- Panels with light dependencies that can switch to Context + factory registration.
- Any panel currently taking only `Editor*` or `Rom*` in constructor.

## Risks & Mitigations
- Risk: Global vs session visibility conflicts.  
  Mitigation: Separate ID namespace for global panels and avoid prefixing.
- Risk: ImGui ordering regressions on WASM or headless.  
  Mitigation: Keep a pre-frame event for non-ImGui tasks; gate GUI event on
  `ImGui::NewFrame` being called.
- Risk: Panels with hidden per-session state bleed across sessions.  
  Mitigation: Keep Tier 1 limited to panels that read state from Context each draw.

## Testing & Validation
Required:
- Desktop app: open/close panels, switch sessions, verify visibility persists.
- Create new session and confirm registry panels appear without manual registration.
- Verify no ImGui assert/log spam during layout rebuilds.

Optional:
- Run `yaze_test` if practical; validate no regressions in UI tests.

## Documentation Impact
- Update this plan when scope lands and Tier 1 porting starts.
- Add coordination board entry if this becomes a multi-day initiative.

## Timeline / Checkpoints
- M1: Panel scope + descriptor registration changes land.
- M2: Frame event split merged and deferred actions moved to GUI-safe phase.
- M3: Tier 1 panels fully ported; manual registrations removed.
