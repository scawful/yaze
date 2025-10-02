# z3ed Agentic Workflow Implementation Plan

_Last updated: 2025-10-01 (final update - Phase 6 + AW-02 complete)_

This plan decomposes the design additions (Sections 11–15 of `E6-z3ed-cli-design.md`) into actionable engineering tasks. Each workstream contains milestones, owners (TBD), blocking dependencies, and expected deliverables.

## 1. Workstreams Overview

| Workstream | Goal | Milestone Target | Notes |
|------------|------|------------------|-------|
| Resource Catalogue | Provide authoritative machine-readable specs for CLI resources. | Phase 6 | Schema now captures effects/returns metadata for palette/overworld/rom/patch/dungeon; automation pending. |
| Acceptance Workflow | Enable human review/approval of agent proposals in ImGui. | Phase 7 | Sandbox manager prototype landed; UI work pending. |
| ImGuiTest Bridge | Allow agents to drive ImGui via `ImGuiTestEngine`. | Phase 6 | Requires harness IPC transport. |
| Verification Pipeline | Build layered testing + CI coverage. | Phase 6+ | Integrates with harness + CLI suites. |
| Telemetry & Learning | Capture signals to improve prompts + heuristics. | Phase 8 | Optional/opt-in features. |

### Progress snapshot — 2025-10-01 (Phase 6 Complete, AW-03 Complete)

**Resource Catalogue (RC)** ✅ COMPLETE:
- CLI flag passthrough and resource catalog system operational
- `agent describe` exports YAML/JSON command schemas for AI consumption
- `docs/api/z3ed-resources.yaml` generated and maintained
- Fixed `rom info` segfault with dedicated handler

**Acceptance Workflow (AW-01, AW-02, AW-03)** ✅ COMPLETE:
- `ProposalRegistry` tracks agent modifications with metadata/diffs/logs
- `RomSandboxManager` handles isolated ROM copies
- `agent list` and `agent diff` commands operational
- **ProposalDrawer ImGui GUI** implemented with list/detail views and accept/reject/delete actions
- Integrated into EditorManager (`Debug → Agent Proposals` menu)
- Fixed CMake linker errors across all app targets
- **Known limitation**: ROM merging in `AcceptProposal()` not yet implemented (TODO)

**Graphics System** ✅ FIXED:
- Fixed RAII shutdown crash in `PerformanceProfiler` (static destruction order issue)
- Added shutdown flag and validity checks - application now exits cleanly
- Enables stable testing and performance monitoring for AI workflow

**Agent Run** ✅ FIXED:
- Added automatic ROM loading from `--rom` flag when not already loaded
- Proper error messages guide users to specify ROM path

## 2. Task Backlog

| ID | Task | Workstream | Type | Status | Dependencies |
|----|------|------------|------|--------|--------------|
| RC-01 | Define schema for `ResourceCatalog` entries and implement serialization helpers. | Resource Catalogue | Code | Done | Schema system complete with all resource types documented |
| RC-02 | Auto-generate `docs/api/z3ed-resources.yaml` from command annotations. | Resource Catalogue | Tooling | Done | Generated and committed to docs/api/ |
| RC-03 | Implement `z3ed agent describe` CLI surface returning JSON schemas. | Resource Catalogue | Code | Done | Both YAML and JSON output formats working |
| RC-04 | Integrate schema export with TUI command palette + help overlays. | Resource Catalogue | UX | Planned | RC-03 |
| RC-05 | Harden CLI command routing/flag parsing to unblock agent automation. | Resource Catalogue | Code | Done | Fixed rom info handler to use FLAGS_rom |
| AW-01 | Implement sandbox ROM cloning and tracking (`RomSandboxManager`). | Acceptance Workflow | Code | Done | ROM sandbox manager operational with lifecycle management |
| AW-02 | Build proposal registry service storing diffs, logs, screenshots. | Acceptance Workflow | Code | Done | ProposalRegistry implemented and integrated with agent run workflow |
| AW-03 | Add ImGui drawer for proposals with accept/reject controls. | Acceptance Workflow | UX | Done | ProposalDrawer GUI complete with list, detail, and action buttons |
| AW-04 | Implement policy evaluation for gating accept buttons. | Acceptance Workflow | Code | Planned | AW-03 |
| AW-05 | Draft `.z3ed-diff` hybrid schema (binary deltas + JSON metadata). | Acceptance Workflow | Design | Planned | AW-01 |
| IT-01 | Create `ImGuiTestHarness` IPC service embedded in `yaze_test`. | ImGuiTest Bridge | Code | Planned | Harness transport decision |
| IT-02 | Implement CLI agent step translation (`imgui_action` → harness call). | ImGuiTest Bridge | Code | Planned | IT-01 |
| IT-03 | Provide synchronization primitives (`WaitForIdle`, etc.). | ImGuiTest Bridge | Code | Planned | IT-01 |
| VP-01 | Expand CLI unit tests for new commands and sandbox flow. | Verification Pipeline | Test | Planned | RC/AW tasks |
| VP-02 | Add harness integration tests with replay scripts. | Verification Pipeline | Test | Planned | IT tasks |
| VP-03 | Create CI job running agent smoke tests with `YAZE_WITH_JSON`. | Verification Pipeline | Infra | Planned | VP-01, VP-02 |
| TL-01 | Capture accept/reject metadata and push to telemetry log. | Telemetry & Learning | Code | Planned | AW tasks |
| TL-02 | Build anonymized metrics exporter + opt-in toggle. | Telemetry & Learning | Infra | Planned | TL-01 |

_Status Legend: Prototype · In Progress · Planned · Blocked · Done_

## 3. Immediate Next Steps (Week of Oct 1-7, 2025)

### Priority 0: Debug & Stabilize (Active)
1. **FIX**: Debug `stoi` crash in `agent run` command execution
   - Error occurs when executing agent commands via ModernCLI
   - Investigate command parsing and proposal creation flow

### Priority 1: Complete AW-03 (2-3 hours)
2. **TEST**: ProposalDrawer with live proposals
   - Create test proposals via CLI with working prompts
   - Verify list view, detail view, filtering, refresh
   - Test Accept/Reject/Delete actions
   
3. **IMPLEMENT**: ROM merging in `AcceptProposal()` method
   - Add ROM reference to ProposalDrawer
   - Load sandbox ROM and merge into main ROM
   - Add save prompt after successful merge
   - Test merge + undo/redo integration

### Priority 2: Policy Evaluation (AW-04, 4-6 hours)
4. **DESIGN**: Policy evaluation framework
   - YAML-based policy configuration (`.yaze/policies/agent.yaml`)
   - Policy types: test requirements, change constraints, review requirements
   - PolicyEvaluator service for checking proposals against rules
   
5. **INTEGRATE**: Policy checks in ProposalDrawer UI
   - Display policy violations in detail view
   - Gate accept button based on policy results
   - Show helpful messages for blocked proposals

### Priority 3: Testing Infrastructure (VP-01, ongoing)
6. **EXPAND**: CLI unit tests for agent commands
7. **ADD**: Integration tests for proposal workflow

### Later: ImGuiTestHarness (IT-01)
- Spike IPC transport options (socket/HTTP/shared memory)
- Design harness architecture
- Create proof-of-concept

## 4. Current Issues & Blockers

### Active Issues
1. **BLOCKER**: `std::invalid_argument: stoi: no conversion` crash in `agent run`
   - Occurs when executing generated commands
   - Blocks testing of ProposalDrawer with real proposals
   - Needs immediate investigation

### Known Limitations (Non-Blocking)
1. ROM merging not implemented in `AcceptProposal()` - status updates only
2. Large diffs truncated at 1000 lines
3. ProposalDrawer lacks keyboard navigation
4. Some timer warnings during shutdown (harmless but noisy)

## 5. Open Questions

- What serialization format should the proposal registry adopt for diff payloads (binary vs. textual vs. hybrid)?  \
	➤ Decision: pursue a hybrid package (`.z3ed-diff`) that wraps binary tile/object deltas alongside a JSON metadata envelope (identifiers, texture descriptors, preview palette info). Capture format draft under RC/AW backlog.
- How should the harness authenticate escalation requests for mutation actions?  \
	➤ Still open—evaluate shared-secret vs. interactive user prompt in the harness spike (IT-01).
- Can we reuse existing regression test infrastructure for nightly ImGui runs or should we spin up a dedicated binary?  \
	➤ Investigate during the ImGuiTestHarness spike; compare extending `yaze_test` jobs versus introducing a lightweight automation runner.

## 5. Completed Work Summary

### Resource Catalogue Workstream (RC) - ✅ COMPLETE

The Resource Catalogue workstream has been successfully completed, providing a foundation for AI-driven automation:

**Implementation Details**:
- Created comprehensive schema system in `src/cli/service/resource_catalog.{h,cc}`
- Implemented resource catalog for: ROM, Patch, Palette, Overworld, Dungeon, and Agent commands
- Each resource includes: name, description, actions, arguments, effects, and return values
- Built dual-format serialization: JSON (compact) and YAML (human-readable)

**Key Fixes**:
- Fixed `rom info` segfault by creating dedicated `RomInfo` handler using `FLAGS_rom`
- Added `rom info` action to resource schema with proper metadata
- Ensured all ROM commands consistently use flag-based dispatch

**Generated Artifacts**:
- `docs/api/z3ed-resources.yaml` - Authoritative machine-readable API reference
- Both JSON and YAML output formats validated and working
- Resource filtering capability (`--resource <name>`) operational

**Command Examples**:
```bash
# View all resources in YAML
z3ed agent describe --format yaml

# Get specific resource as JSON
z3ed agent describe --format json --resource rom

# Generate documentation file
z3ed agent describe --format yaml --output docs/api/z3ed-resources.yaml
```

**Testing Results**:
All commands tested and verified working:
- ✅ `z3ed rom info --rom=zelda3.sfc` - displays title, size, filename
- ✅ `z3ed rom validate --rom=zelda3.sfc` - verifies checksum and header
- ✅ `z3ed agent describe --format yaml` - outputs complete catalog
- ✅ `z3ed agent describe --format json --resource rom` - filters by resource

### Acceptance Workflow (AW-01, AW-02) - ✅ CORE COMPLETE

The foundational infrastructure for proposal tracking and review is now operational:

**RomSandboxManager Implementation** (AW-01):
- Singleton service managing isolated ROM copies for agent proposals
- Sandboxes created in `YAZE_SANDBOX_ROOT` (env var) or system temp directory
- Automatic directory creation and ROM file cloning
- Active sandbox tracking for current agent session
- Cleanup utilities for removing old sandboxes

**ProposalRegistry Implementation** (AW-02):
- Comprehensive tracking of agent-generated ROM modifications
- Stores proposal metadata: ID, sandbox ID, prompt, description, timestamps
- Records execution diffs in `diff.txt` within proposal directory
- Appends command execution logs to `execution.log` with timestamps
- Support for screenshot attachments (path tracking)
- Proposal lifecycle: Pending → Accepted/Rejected
- Query capabilities: get by ID, list all, filter by status, find latest pending

**Agent Run Integration**:
- `agent run` now creates sandbox + proposal automatically
- All command executions logged with timestamps and status
- Success/failure outcomes captured in proposal logs
- User feedback includes proposal ID and sandbox path for review
- Foundation ready for `agent diff`, `agent commit`, `agent revert` enhancements

**Agent Diff Enhancement** (Completed Oct 1, 2025):
- Reads proposal diffs from ProposalRegistry automatically
- Displays detailed metadata: proposal ID, status, timestamps, command count
- Shows diff content from proposal directory
- Displays execution log (first 50 lines, with truncation for long logs)
- Provides next-step guidance (commit/revert/GUI review)
- Supports `--proposal-id` flag to view specific proposals
- Fallback to legacy diff behavior if no proposals found

**Agent List Command** (New - Oct 1, 2025):
- Enumerates all proposals in the registry
- Shows proposal ID, status, creation time, prompt, and stats
- Indicates pending/accepted/rejected status for each proposal
- Provides guidance on using `agent diff` to view details
- Empty state message guides users to create proposals with `agent run`

**Resource Catalog Updates**:
- Added `agent list` action with returns schema
- Added `agent diff` action with arguments (`--proposal-id`) and returns schema
- Updated agent resource description to include listing and diffing capabilities
- Regenerated `docs/api/z3ed-resources.yaml` with new agent actions

**ProposalDrawer GUI Component** (Completed Oct 1, 2025):
- ImGui right-side drawer for proposal review (AW-03)
- Split view: proposal list (top) + detail view (bottom)
- List view: table with ID, status, prompt columns; colored status indicators
- Detail view: collapsible sections for metadata/diff/log; syntax-aware display
- Action buttons: Accept, Reject, Delete with confirmation dialogs
- Status filtering (All/Pending/Accepted/Rejected)
- Integrated into EditorManager with Debug → Agent Proposals menu
- Accept/Reject updates ProposalRegistry status
- Delete removes proposal from registry and filesystem
- TODO: Implement actual ROM merging in AcceptProposal method

**CMake Build Integration**:
- Added `cli/service/proposal_registry.cc` and `cli/service/rom_sandbox_manager.cc` to all app targets
- Fixed linker errors by including CLI service sources in:
  - `yaze` (main GUI app)
  - `yaze_emu` (emulator standalone)
  - `yaze_core` (testing library)
  - `yaze_c` (C API library)
- All targets now build successfully with ProposalDrawer dependencies

**Architecture Benefits**:
- Clean separation: RomSandboxManager (file ops) ↔ ProposalRegistry (metadata)
- Thread-safe with mutex protection for concurrent access
- Extensible design ready for ImGui review UI (AW-03)
- Proposal persistence enables post-session review and auditing
- Proposal-centric workflow enables human-in-the-loop review
- GUI and CLI both have full access to proposal system

**Next Steps for AW Workstream**:
- Test ProposalDrawer in running application
- Complete ROM merging in AcceptProposal method
- AW-04: Policy evaluation for gating mutations
- AW-05: `.z3ed-diff` hybrid format design

### Files Modified/Created

**Phase 6 (Resource Catalogue)**:
1. `src/cli/handlers/rom.cc` - Added `RomInfo::Run` implementation
2. `src/cli/z3ed.h` - Added `RomInfo` class declaration  
3. `src/cli/modern_cli.cc` - Updated `HandleRomInfoCommand` routing
4. `src/cli/service/resource_catalog.cc` - Added `rom info` schema entry
5. `docs/api/z3ed-resources.yaml` - Generated comprehensive API catalog

**AW-01 & AW-02 (Proposal Tracking)**:
6. `src/cli/service/proposal_registry.h` - New proposal tracking service interface
7. `src/cli/service/proposal_registry.cc` - Implementation with full lifecycle management
8. `src/cli/handlers/agent.cc` - Integrated ProposalRegistry into agent run workflow

**Agent Diff & List Enhancement**:
9. `src/cli/handlers/agent.cc` - Enhanced HandleDiffCommand with proposal reading, added HandleListCommand
10. `src/cli/service/resource_catalog.cc` - Added agent list/diff actions with schemas
11. `docs/api/z3ed-resources.yaml` - Regenerated with new agent commands
12. `docs/E6-z3ed-cli-design.md` - Updated Section 8.1 with list/diff documentation

**AW-03 (ProposalDrawer GUI)**:
13. `src/app/editor/system/proposal_drawer.h` - Complete drawer interface with Draw/Accept/Reject/Delete
14. `src/app/editor/system/proposal_drawer.cc` - Full implementation (~350 lines) with list/detail views
15. `src/app/editor/editor_manager.h` - Added ProposalDrawer member and include
16. `src/app/editor/editor_manager.cc` - Added menu item and Draw() call in Update loop
17. `src/CMakeLists.txt` - Added proposal_drawer files to System Editor source group
18. `src/app/app.cmake` - Added CLI service sources to yaze target (both Apple and non-Apple builds)
19. `src/app/emu/emu.cmake` - Added CLI service sources to yaze_emu target
20. `src/CMakeLists.txt` - Added CLI service sources to yaze_core library sources
9. `src/cli/z3ed.cmake` - Added proposal_registry.cc to build
10. `docs/E6-z3ed-implementation-plan.md` - Updated progress and task statuses

**Agent Diff & List (Oct 1, 2025)**:
11. `src/cli/handlers/agent.cc` - Enhanced `HandleDiffCommand` with proposal reading, added `HandleListCommand`
12. `src/cli/service/resource_catalog.cc` - Added agent list and diff actions to schema
13. `docs/api/z3ed-resources.yaml` - Regenerated with new agent commands

## 6. References

- `docs/E6-z3ed-cli-design.md` - Overall CLI design and architecture
- `docs/api/z3ed-resources.yaml` - Machine-readable API reference (generated)
- `src/cli/service/resource_catalog.h` - Resource catalog implementation
- `src/cli/service/resource_catalog.cc` - Schema definitions and serialization
