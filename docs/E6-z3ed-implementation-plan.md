# z3ed Agentic Workflow Implementation Plan

_Last updated: 2025-10-01 (afternoon update)_

This plan decomposes the design additions (Sections 11–15 of `E6-z3ed-cli-design.md`) into actionable engineering tasks. Each workstream contains milestones, owners (TBD), blocking dependencies, and expected deliverables.

## 1. Workstreams Overview

| Workstream | Goal | Milestone Target | Notes |
|------------|------|------------------|-------|
| Resource Catalogue | Provide authoritative machine-readable specs for CLI resources. | Phase 6 | Schema now captures effects/returns metadata for palette/overworld/rom/patch/dungeon; automation pending. |
| Acceptance Workflow | Enable human review/approval of agent proposals in ImGui. | Phase 7 | Sandbox manager prototype landed; UI work pending. |
| ImGuiTest Bridge | Allow agents to drive ImGui via `ImGuiTestEngine`. | Phase 6 | Requires harness IPC transport. |
| Verification Pipeline | Build layered testing + CI coverage. | Phase 6+ | Integrates with harness + CLI suites. |
| Telemetry & Learning | Capture signals to improve prompts + heuristics. | Phase 8 | Optional/opt-in features. |

## 2. Task Backlog

| ID | Task | Workstream | Type | Status | Dependencies |
|----|------|------------|------|--------|--------------|
| RC-01 | Define schema for `ResourceCatalog` entries and implement serialization helpers. | Resource Catalogue | Code | In Progress | Palette/Overworld/ROM/Patch/Dungeon actions annotated with effects/returns; serialization covered by unit tests; CLI wiring ongoing |
| RC-02 | Auto-generate `docs/api/z3ed-resources.yaml` from command annotations. | Resource Catalogue | Tooling | Blocked | RC-01 |
| RC-03 | Implement `z3ed agent describe` CLI surface returning JSON schemas. | Resource Catalogue | Code | Prototype | `agent describe` outputs JSON; broaden resource coverage next |
| RC-04 | Integrate schema export with TUI command palette + help overlays. | Resource Catalogue | UX | Planned | RC-03 |
| AW-01 | Implement sandbox ROM cloning and tracking (`RomSandboxManager`). | Acceptance Workflow | Code | Prototype | ROM snapshot infra |
| AW-02 | Build proposal registry service storing diffs, logs, screenshots. | Acceptance Workflow | Code | Planned | AW-01 |
| AW-03 | Add ImGui drawer for proposals with accept/reject controls. | Acceptance Workflow | UX | Planned | AW-02 |
| AW-04 | Implement policy evaluation for gating accept buttons. | Acceptance Workflow | Code | Planned | AW-03 |
| IT-01 | Create `ImGuiTestHarness` IPC service embedded in `yaze_test`. | ImGuiTest Bridge | Code | Planned | Harness transport decision |
| IT-02 | Implement CLI agent step translation (`imgui_action` → harness call). | ImGuiTest Bridge | Code | Planned | IT-01 |
| IT-03 | Provide synchronization primitives (`WaitForIdle`, etc.). | ImGuiTest Bridge | Code | Planned | IT-01 |
| VP-01 | Expand CLI unit tests for new commands and sandbox flow. | Verification Pipeline | Test | Planned | RC/AW tasks |
| VP-02 | Add harness integration tests with replay scripts. | Verification Pipeline | Test | Planned | IT tasks |
| VP-03 | Create CI job running agent smoke tests with `YAZE_WITH_JSON`. | Verification Pipeline | Infra | Planned | VP-01, VP-02 |
| TL-01 | Capture accept/reject metadata and push to telemetry log. | Telemetry & Learning | Code | Planned | AW tasks |
| TL-02 | Build anonymized metrics exporter + opt-in toggle. | Telemetry & Learning | Infra | Planned | TL-01 |

_Status Legend: Prototype · In Progress · Planned · Blocked · Done_

## 3. Immediate Next Steps

1. Automate catalog export into `docs/api/z3ed-resources.yaml` and snapshot `agent describe` outputs for regression coverage (`RC-02`, `RC-03`).
2. Wire `RomSandboxManager` into proposal lifecycle (metadata persistence, cleanup) and validate with agent diff flow (`AW-01`).
3. Spike IPC options for `ImGuiTestHarness` (socket vs. HTTP vs. shared memory) and document findings.

## 4. Open Questions

- What serialization format should the proposal registry adopt for diff payloads (binary vs. textual vs. hybrid)?
- How should the harness authenticate escalation requests for mutation actions?
- Can we reuse existing regression test infrastructure for nightly ImGui runs or should we spin up a dedicated binary?

## 5. References

- `docs/E6-z3ed-cli-design.md`
- `docs/api/z3ed-resources.yaml`
- `src/cli/service/resource_catalog.h` (prototype)
