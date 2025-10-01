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

### Progress snapshot — 2025-10-01 final update (Phase 6 + Agent Diff & List Complete)

- ✅ CLI global flag passthrough now preserves subcommand options, letting `agent describe` and palette routines accept both space-separated and `--flag=value` styles alongside the updated help text.
- ✅ `agent describe --format yaml` writes catalog data end-to-end; JSON format also working correctly.
- ✅ Expanded `ImGuiTestHarness` design with concrete transport, message envelope, and lifecycle details to unblock IT-01 spike.
- ✅ Fixed `rom info` segfault by creating dedicated `RomInfo` handler that properly uses the `--rom` flag instead of positional arguments. Command now works correctly with flag-based dispatch.
- ✅ Added `rom info` action to resource catalog with proper schema documentation including return values (title, size, filename).
- ✅ Generated and committed `docs/api/z3ed-resources.yaml` as authoritative machine-readable API reference for CLI automation (RC-02 complete).
- ✅ **Implemented `ProposalRegistry` service** for tracking agent-generated ROM modifications with metadata, diffs, logs, and screenshots.
- ✅ **Integrated ProposalRegistry into `agent run` workflow** - all command executions are now logged and tracked.
- ✅ **RomSandboxManager fully operational** with lifecycle management for proposal tracking.
- ✅ **Enhanced `agent diff` command** to read and display proposal diffs from ProposalRegistry with detailed metadata, execution logs, and next-step guidance.
- ✅ **Added `--proposal-id` flag to `agent diff`** for viewing specific proposals (not just latest pending).
- ✅ **Implemented `agent list` command** to enumerate all proposals with status filtering and metadata display.
- ✅ **Updated resource catalog** with agent list and diff actions including comprehensive argument and return schemas.
- ✅ **Regenerated API documentation** (`docs/api/z3ed-resources.yaml`) with all new agent commands.

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
| AW-03 | Add ImGui drawer for proposals with accept/reject controls. | Acceptance Workflow | UX | Planned | AW-02 |
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

## 3. Immediate Next Steps

1. ✅ **COMPLETED**: Automated catalog export into `docs/api/z3ed-resources.yaml` - both JSON and YAML formats work correctly (RC-02, RC-03).
2. ✅ **COMPLETED**: Fixed `rom info` crash - created dedicated `RomInfo` handler that uses `FLAGS_rom` instead of positional arguments (RC-05).
3. ✅ **COMPLETED**: Wired `RomSandboxManager` and `ProposalRegistry` into agent run workflow with full logging and metadata tracking (AW-01, AW-02).
4. ✅ **COMPLETED**: Enhanced `agent diff` command to read and display proposal diffs from ProposalRegistry with formatted output, execution logs, and next-step guidance.
5. ✅ **COMPLETED**: Added `agent list` command to enumerate all proposals with status filtering.
6. ✅ **COMPLETED**: Added `--proposal-id` flag to `agent diff` for viewing specific proposals.
7. ✅ **COMPLETED**: Updated resource catalog with agent list and diff actions including arguments and return schemas.
8. **PLANNED**: Add ImGui drawer for proposals with accept/reject controls (AW-03).
9. **PLANNED**: Spike IPC options for `ImGuiTestHarness` (socket vs. HTTP vs. shared memory) and document findings (IT-01).
10. **PLANNED**: Integrate schema export with TUI command palette + help overlays (RC-04).

## 4. Open Questions

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

**Architecture Benefits**:
- Clean separation: RomSandboxManager (file ops) ↔ ProposalRegistry (metadata)
- Thread-safe with mutex protection for concurrent access
- Extensible design ready for ImGui review UI (AW-03)
- Proposal persistence enables post-session review and auditing
- Proposal-centric workflow enables human-in-the-loop review

**Next Steps for AW Workstream**:
- AW-03: ImGui drawer with accept/reject controls
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
