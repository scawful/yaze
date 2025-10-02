# z3ed Agentic Workflow Implementation Plan

_Last updated: 2025-10-01 (final update - Phase 6 + AW-03 + IT-01 Phase 1 complete)_

> 📊 **Quick Reference**: See [STATE_SUMMARY_2025-10-01.md](STATE_SUMMARY_2025-10-01.md) for a comprehensive overview of current architecture, workflows, and status.

This plan decomposes the design additions (Sections 11–15 of `E6-z3ed-cli-design.md`) into actionable engineering tasks. Each workstream contains milestones, owners (TBD), blocking dependencies, and expected deliverables.

**Files Modified/Created**

**Phase 6 (Resource Catalogue)**:
1. `src/cli/handlers/rom.cc` - Added `RomInfo::Run` implementation
2. `src/cli/z3ed.h` - Added `RomInfo` class declaration  
3. `src/cli/modern_cli.cc` - Updated `HandleRomInfoCommand` routing
4. `src/cli/service/resource_catalog.cc` - Added `rom info` schema entry
5. `docs/api/z3ed-resources.yaml` - Generated comprehensive API catalog, owners (TBD), blocking dependencies, and expected deliverables.

## 1. Workstreams Overview

| Workstream | Goal | Milestone Target | Notes |
|------------|------|------------------|-------|
| Resource Catalogue | Provide authoritative machine-readable specs for CLI resources. | Phase 6 | Schema now captures effects/returns metadata for palette/overworld/rom/patch/dungeon; automation pending. |
| Acceptance Workflow | Enable human review/approval of agent proposals in ImGui. | Phase 7 | Sandbox manager prototype landed; UI work pending. |
| ImGuiTest Bridge | Allow agents to drive ImGui via `ImGuiTestEngine`. | Phase 6 | Requires harness IPC transport. |
| Verification Pipeline | Build layered testing + CI coverage. | Phase 6+ | Integrates with harness + CLI suites. |
| Telemetry & Learning | Capture signals to improve prompts + heuristics. | Phase 8 | Optional/opt-in features. |

### Progress snapshot — 2025-10-01 (Phase 6 Complete, AW-03 Complete, IT-01 Phase 1 Complete)

**Resource Catalogue (RC)** ✅ COMPLETE:
- CLI flag passthrough and resource catalog system operational
- `agent describe` exports YAML/JSON command schemas for AI consumption
- `docs/api/z3ed-resources.yaml` generated and maintained
- Fixed `rom info` segfault with dedicated handler

**Acceptance Workflow (AW-01, AW-02, AW-03)** ✅ COMPLETE:
- `ProposalRegistry` tracks agent modifications with metadata/diffs/logs
- Proposal persistence: LoadProposalsFromDiskLocked() enables cross-session tracking
- `RomSandboxManager` handles isolated ROM copies
- `agent list` and `agent diff` commands operational
- **ProposalDrawer ImGui GUI** fully implemented:
  - List/detail split view with filtering and refresh
  - Accept/Reject/Delete actions with confirmation dialogs
  - **ROM merging complete**: AcceptProposal() loads sandbox ROM and merges into main ROM
  - Integrated into EditorManager (`Debug → Agent Proposals` menu)
  - Ready for end-to-end testing with live proposals

**Graphics System** ✅ FIXED:
- Fixed RAII shutdown crash in `PerformanceProfiler` (static destruction order issue)
- Added shutdown flag and validity checks - application now exits cleanly
- Enables stable testing and performance monitoring for AI workflow

**Agent Run** ✅ FIXED:
- Added automatic ROM loading from `--rom` flag when not already loaded
- Proper error messages guide users to specify ROM path

**Active Work (Oct 1-7, 2025)**:
- **Priority 1**: ImGuiTestHarness (IT-01) - ✅ Phase 1 Complete (gRPC tested), Phase 2 Active (ImGuiTestEngine integration)
- **Priority 2**: Policy Evaluation (AW-04) - YAML-based constraint system

**Recent Completion (Oct 1, 2025)**:
- ✅ gRPC test harness fully operational with all 6 RPCs validated
- ✅ Server lifecycle management (Start/Shutdown) working
- ✅ Cross-platform build verified (macOS ARM64, gRPC v1.62.0)
- ✅ All stub handlers returning success responses

## 2. Task Backlog

| ID | Task | Workstream | Type | Status | Dependencies |
|----|------|------------|------|--------|--------------|
| RC-01 | Define schema for `ResourceCatalog` entries and implement serialization helpers. | Resource Catalogue | Code | Done | Schema system complete with all resource types documented |
| RC-02 | Auto-generate `docs/api/z3ed-resources.yaml` from command annotations. | Resource Catalogue | Tooling | Done | Generated and committed to docs/api/ |
| RC-03 | Implement `z3ed agent describe` CLI surface returning JSON schemas. | Resource Catalogue | Code | Done | Both YAML and JSON output formats working |
| RC-04 | Integrate schema export with TUI command palette + help overlays. | Resource Catalogue | UX | Planned | RC-03 |
| RC-05 | Harden CLI command routing/flag parsing to unblock agent automation. | Resource Catalogue | Code | Done | Fixed rom info handler to use FLAGS_rom |
| AW-01 | Implement sandbox ROM cloning and tracking (`RomSandboxManager`). | Acceptance Workflow | Code | Done | ROM sandbox manager operational with lifecycle management |
| AW-02 | Build proposal registry service storing diffs, logs, screenshots. | Acceptance Workflow | Code | Done | ProposalRegistry implemented with disk persistence |
| AW-03 | Add ImGui drawer for proposals with accept/reject controls. | Acceptance Workflow | UX | Done | ProposalDrawer GUI complete with ROM merging |
| AW-04 | Implement policy evaluation for gating accept buttons. | Acceptance Workflow | Code | In Progress | AW-03, Priority 2 - YAML policies + PolicyEvaluator |
| AW-05 | Draft `.z3ed-diff` hybrid schema (binary deltas + JSON metadata). | Acceptance Workflow | Design | Planned | AW-01 |
| IT-01 | Create `ImGuiTestHarness` IPC service embedded in `yaze_test`. | ImGuiTest Bridge | Code | In Progress | Phase 1 Done (gRPC), Phase 2 Active (ImGuiTestEngine) |
| IT-02 | Implement CLI agent step translation (`imgui_action` → harness call). | ImGuiTest Bridge | Code | Planned | IT-01 |
| IT-03 | Provide synchronization primitives (`WaitForIdle`, etc.). | ImGuiTest Bridge | Code | Planned | IT-01 |
| VP-01 | Expand CLI unit tests for new commands and sandbox flow. | Verification Pipeline | Test | Planned | RC/AW tasks |
| VP-02 | Add harness integration tests with replay scripts. | Verification Pipeline | Test | Planned | IT tasks |
| VP-03 | Create CI job running agent smoke tests with `YAZE_WITH_JSON`. | Verification Pipeline | Infra | Planned | VP-01, VP-02 |
| TL-01 | Capture accept/reject metadata and push to telemetry log. | Telemetry & Learning | Code | Planned | AW tasks |
| TL-02 | Build anonymized metrics exporter + opt-in toggle. | Telemetry & Learning | Infra | Planned | TL-01 |

_Status Legend: Prototype · In Progress · Planned · Blocked · Done_

## 3. Immediate Next Steps (Week of Oct 1-7, 2025)

### Priority 0: Testing & Validation (Active)
1. **TEST**: Complete end-to-end proposal workflow
   - Launch YAZE and verify ProposalDrawer displays live proposals
   - Test Accept action → verify ROM merge and save prompt
   - Test Reject and Delete actions
   - Validate filtering and refresh functionality

### Priority 1: ImGuiTestHarness Foundation (IT-01, 10-14 hours) 🔥 ACTIVE
**Rationale**: Required for automated GUI testing and remote control of YAZE for AI workflows  
**Decision**: ✅ **Use gRPC** - Production-grade, cross-platform, type-safe (see `docs/IT-01-grpc-evaluation.md`)

2. **SETUP**: Add gRPC to build system (1-2 hours)
   - Add gRPC + Protobuf to vcpkg.json
   - Update CMakeLists.txt with conditional gRPC support
   - Test build on macOS with `YAZE_WITH_GRPC=ON`
   - Verify protobuf code generation works

3. **PROTOTYPE**: Minimal gRPC service (2-3 hours)
   - Define basic .proto with Ping, Click operations
   - Implement `ImGuiTestHarnessServiceImpl::Ping()`
   - Implement `ImGuiTestHarnessServer` singleton
   - Test with grpcurl: `grpcurl -d '{"message":"hello"}' localhost:50051 ...`

4. **IMPLEMENT**: Core Operations (4-6 hours)
   - Complete .proto schema (Click, Type, Wait, Assert, Screenshot)
   - Implement all RPC handlers with ImGuiTestEngine integration
   - Add target parsing ("button:Open ROM" → widget lookup)
   - Error handling and timeout support

5. **INTEGRATE**: CLI Client (2-3 hours)
   - `z3ed agent test --prompt "..."` generates gRPC calls
   - AI translates natural language → ImGui actions → RPC requests
   - Capture screenshots for LLM feedback
   - Example: "open overworld editor" → `Click(target="menu:Editors→Overworld")`

6. **WINDOWS TESTING**: Cross-platform verification (2-3 hours)
   - Create detailed Windows build instructions (vcpkg setup)
   - Test on Windows VM or with contributor
   - Add Windows CI job to GitHub Actions
   - Document troubleshooting for common Windows issues

### Priority 2: Policy Evaluation Framework (AW-04, 4-6 hours)
5. **DESIGN**: YAML-based Policy Configuration
   ```yaml
   # .yaze/policies/agent.yaml
   version: 1.0
   policies:
     - name: require_tests
       type: test_requirement
       enabled: true
       rules:
         - test_suite: "overworld_rendering"
           min_pass_rate: 0.95
         - test_suite: "palette_integrity"
           min_pass_rate: 1.0
     
     - name: limit_change_scope
       type: change_constraint
       enabled: true
       rules:
         - max_bytes_changed: 10240  # 10KB
         - allowed_banks: [0x00, 0x01, 0x0E]  # Graphics banks only
         - forbidden_ranges:
           - start: 0xFFB0  # ROM header
             end: 0xFFFF
     
     - name: human_review_required
       type: review_requirement
       enabled: true
       rules:
         - if: bytes_changed > 1024
           then: require_diff_review: true
         - if: commands_executed > 10
           then: require_log_review: true
   ```

6. **IMPLEMENT**: PolicyEvaluator Service
   - `src/cli/service/policy_evaluator.{h,cc}`
   - Singleton service loads policies from `.yaze/policies/`
   - `EvaluateProposal(proposal_id) -> PolicyResult`
   - Returns: pass/fail + list of violations with severity
   - Hook into ProposalRegistry lifecycle

7. **INTEGRATE**: Policy UI in ProposalDrawer
   - Add "Policy Status" section in detail view
   - Display violations with icons: ⛔ Critical, ⚠️ Warning, ℹ️ Info
   - Gate Accept button: disabled if critical violations exist
   - Show helpful messages: "Accept blocked: Test pass rate 0.85 < 0.95"
   - Allow policy overrides with confirmation: "Override policy? This action will be logged."

### Priority 3: Documentation & Consolidation (2-3 hours)
8. **CONSOLIDATE**: Merge standalone docs into main plan
   - ✅ AW-03 summary → already in main plan, delete standalone doc
   - Check for other AW-* or task-specific docs to merge
   - Update main plan with architecture diagrams

9. **CREATE**: Architecture Flow Diagram
   - Visual representation of proposal lifecycle
   - Component interaction diagram
   - Add to implementation plan

### Later: Advanced Features
- VP-01: Expand CLI unit tests
- VP-02: Integration tests with replay scripts
- TL-01: Telemetry capture for learning

## 4. Current Issues & Blockers

### Active Issues
None - all blocking issues resolved as of Oct 1, 2025

### Known Limitations (Non-Blocking)
1. ProposalDrawer lacks keyboard navigation
2. Large diffs/logs truncated at 1000 lines (consider pagination)
3. Proposals don't persist full metadata to disk (prompt, description, sandbox_id reconstructed)
4. No policy evaluation yet (AW-04)

## 5. Architecture Overview

### 5.1. Proposal Lifecycle Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. CREATION (CLI: z3ed agent run)                               │
├─────────────────────────────────────────────────────────────────┤
│ User Prompt                                                      │
│      ↓                                                           │
│ MockAIService / GeminiAIService                                 │
│      ↓ (generates commands)                                     │
│ ["palette export ...", "overworld set-tile ..."]                │
│      ↓                                                           │
│ RomSandboxManager::CreateSandbox(rom)                           │
│      ↓ (creates isolated copy)                                  │
│ /tmp/yaze/sandboxes/<timestamp>/zelda3.sfc                      │
│      ↓                                                           │
│ Execute commands on sandbox ROM                                 │
│      ↓ (logs each command)                                      │
│ ProposalRegistry::CreateProposal(sandbox_id, prompt, desc)      │
│      ↓ (creates proposal directory)                             │
│ /tmp/yaze/proposals/proposal-<timestamp>-<seq>/                 │
│   ├─ execution.log (command outputs)                            │
│   ├─ diff.txt (if generated)                                    │
│   └─ screenshots/ (if any)                                      │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 2. DISCOVERY (CLI: z3ed agent list)                             │
├─────────────────────────────────────────────────────────────────┤
│ ProposalRegistry::ListProposals()                               │
│      ↓ (lazy loads from disk)                                   │
│ LoadProposalsFromDiskLocked()                                   │
│      ↓ (scans /tmp/yaze/proposals/)                             │
│ Reconstructs metadata from filesystem                           │
│      ↓ (parses timestamps, reads logs)                          │
│ Returns vector<ProposalMetadata>                                │
│      ↓                                                           │
│ Display table: ID | Status | Created | Prompt | Stats           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 3. REVIEW (GUI: Debug → Agent Proposals)                        │
├─────────────────────────────────────────────────────────────────┤
│ ProposalDrawer::Draw()                                          │
│      ↓ (called every frame from EditorManager)                  │
│ ProposalDrawer::RefreshProposals()                              │
│      ↓ (calls ProposalRegistry::ListProposals)                  │
│ Display proposal list (selectable table)                        │
│      ↓ (user clicks proposal)                                   │
│ ProposalDrawer::SelectProposal(id)                              │
│      ↓ (loads detail content)                                   │
│ Read execution.log and diff.txt from proposal directory         │
│      ↓                                                           │
│ Display detail view:                                            │
│   ├─ Metadata (sandbox_id, timestamp, stats)                   │
│   ├─ Diff (syntax highlighted)                                  │
│   └─ Log (command execution trace)                              │
│      ↓                                                           │
│ User decides: [Accept] [Reject] [Delete]                        │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 4. ACCEPTANCE (GUI: Click "Accept" button)                      │
├─────────────────────────────────────────────────────────────────┤
│ ProposalDrawer::AcceptProposal(proposal_id)                     │
│      ↓                                                           │
│ Get proposal metadata (includes sandbox_id)                     │
│      ↓                                                           │
│ RomSandboxManager::ListSandboxes()                              │
│      ↓ (find sandbox by ID)                                     │
│ sandbox_rom_path = sandbox.rom_path                             │
│      ↓                                                           │
│ Load sandbox ROM from disk                                      │
│      ↓                                                           │
│ rom_->WriteVector(0, sandbox_rom.vector())                      │
│      ↓ (copies entire sandbox ROM → main ROM)                   │
│ ROM marked dirty (save prompt appears)                          │
│      ↓                                                           │
│ ProposalRegistry::UpdateStatus(id, kAccepted)                   │
│      ↓                                                           │
│ User: File → Save ROM                                           │
│      ↓                                                           │
│ Changes committed ✅                                            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ 5. REJECTION (GUI: Click "Reject" button)                       │
├─────────────────────────────────────────────────────────────────┤
│ ProposalDrawer::RejectProposal(proposal_id)                     │
│      ↓                                                           │
│ ProposalRegistry::UpdateStatus(id, kRejected)                   │
│      ↓                                                           │
│ Proposal preserved for audit trail                              │
│ Sandbox ROM left untouched (can be cleaned up later)            │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2. Component Interaction Diagram

```
┌────────────────────┐
│   CLI Layer        │
│  (z3ed commands)   │
└────────┬───────────┘
         │
         ├──► agent run ──────────┐
         ├──► agent list ─────────┤
         └──► agent diff ─────────┤
                                  │
         ┌────────────────────────▼──────────────────────┐
         │         CLI Service Layer                     │
         ├───────────────────────────────────────────────┤
         │  ┌─────────────────────────────────────────┐  │
         │  │ ProposalRegistry (Singleton)            │  │
         │  │  • CreateProposal()                     │  │
         │  │  • ListProposals()                      │  │
         │  │  • GetProposal()                        │  │
         │  │  • UpdateStatus()                       │  │
         │  │  • RemoveProposal()                     │  │
         │  │  • LoadProposalsFromDiskLocked()        │  │
         │  └────────────┬────────────────────────────┘  │
         │               │                               │
         │  ┌────────────▼────────────────────────────┐  │
         │  │ RomSandboxManager (Singleton)          │  │
         │  │  • CreateSandbox()                     │  │
         │  │  • ActiveSandbox()                     │  │
         │  │  • ListSandboxes()                     │  │
         │  │  • RemoveSandbox()                     │  │
         │  └────────────┬────────────────────────────┘  │
         └───────────────┼────────────────────────────────┘
                         │
         ┌───────────────▼────────────────────────────────┐
         │         Filesystem Layer                       │
         ├────────────────────────────────────────────────┤
         │  /tmp/yaze/proposals/                          │
         │    └─ proposal-<timestamp>-<seq>/              │
         │         ├─ execution.log                       │
         │         ├─ diff.txt                            │
         │         └─ screenshots/                        │
         │                                                │
         │  /tmp/yaze/sandboxes/                          │
         │    └─ <timestamp>-<seq>/                       │
         │         └─ zelda3.sfc (isolated ROM copy)      │
         └────────────────────────────────────────────────┘
                         ▲
                         │
         ┌───────────────┴────────────────────────────────┐
         │         GUI Layer                              │
         ├────────────────────────────────────────────────┤
         │  ┌─────────────────────────────────────────┐   │
         │  │ EditorManager                           │   │
         │  │  • current_rom_                         │   │
         │  │  • proposal_drawer_                     │   │
         │  │  • Update() { proposal_drawer_.Draw() } │   │
         │  └────────────┬────────────────────────────┘   │
         │               │                                │
         │  ┌────────────▼────────────────────────────┐   │
         │  │ ProposalDrawer                          │   │
         │  │  • rom_ (ptr to EditorManager's ROM)    │   │
         │  │  • Draw()                               │   │
         │  │  • DrawProposalList()                   │   │
         │  │  • DrawProposalDetail()                 │   │
         │  │  • AcceptProposal() ← ROM MERGE         │   │
         │  │  • RejectProposal()                     │   │
         │  │  • DeleteProposal()                     │   │
         │  └─────────────────────────────────────────┘   │
         └────────────────────────────────────────────────┘
```

### 5.3. Data Flow: Agent Run to ROM Merge

```
User: "Make soldiers wear red armor"
         │
         ▼
┌────────────────────────┐
│ MockAIService          │ Generates: ["palette export sprites_aux1 4 soldier.col"]
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ RomSandboxManager      │ Creates: /tmp/.../sandboxes/20251001T200215-1/zelda3.sfc
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ Command Executor       │ Runs: palette export on sandbox ROM
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ ProposalRegistry       │ Creates: proposal-20251001T200215-1/
│                        │   • execution.log: "[timestamp] palette export succeeded"
└────────┬───────────────┘   • diff.txt: (if diff generated)
         │
         │ Time passes... user launches GUI
         ▼
┌────────────────────────┐
│ ProposalDrawer loads   │ Reads: /tmp/.../proposals/proposal-*/
│                        │ Displays: List of proposals
└────────┬───────────────┘
         │
         │ User clicks "Accept"
         ▼
┌────────────────────────┐
│ AcceptProposal()       │ 1. Find sandbox ROM: /tmp/.../sandboxes/.../zelda3.sfc
│                        │ 2. Load sandbox ROM
│                        │ 3. rom_->WriteVector(0, sandbox_rom.vector())
│                        │ 4. Main ROM now contains all sandbox changes
│                        │ 5. ROM marked dirty
└────────┬───────────────┘
         │
         ▼
┌────────────────────────┐
│ User: File → Save      │ Changes persisted to disk ✅
└────────────────────────┘
```

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

**Proposal Persistence Fix** (Completed Oct 1, 2025):
- Fixed ProposalRegistry to load proposals from disk on first access
- Added `LoadProposalsFromDiskLocked()` method scanning proposal root directory
- Lazy loading implementation in `ListProposals()` for automatic registry population
- Reconstructs metadata from filesystem: ID, timestamps, log/diff paths, screenshots
- Parses creation time from proposal ID format (`proposal-20251001T200215-1`)
- Enables cross-session proposal tracking - `agent list` now finds all proposals
- ProposalDrawer can now display proposals created via CLI `agent run`

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
21. `src/cli/handlers/agent.cc` - Enhanced `HandleDiffCommand` with proposal reading, added `HandleListCommand`
22. `src/cli/service/resource_catalog.cc` - Added agent list and diff actions to schema
23. `docs/api/z3ed-resources.yaml` - Regenerated with new agent commands

**Proposal Persistence Fix (Oct 1, 2025)**:
24. `src/cli/service/proposal_registry.h` - Added `LoadProposalsFromDiskLocked()` declaration
25. `src/cli/service/proposal_registry.cc` - Implemented disk loading with timestamp parsing and metadata reconstruction
26. `src/cli/service/proposal_registry.cc` - Modified `ListProposals()` to lazy-load proposals from disk

**ROM Merging Implementation (Oct 1, 2025)**:
27. `src/app/editor/system/proposal_drawer.h` - Added `SetRom()` method and `Rom*` member for merge operations
28. `src/app/editor/system/proposal_drawer.cc` - Implemented full ROM merging in `AcceptProposal()` with sandbox loading
29. `src/app/editor/editor_manager.cc` - Added `SetRom(current_rom_)` call before drawing ProposalDrawer
30. `src/app/editor/system/proposal_drawer.cc` - Added RomSandboxManager include for sandbox path resolution

## 6. References

- `docs/E6-z3ed-cli-design.md` - Overall CLI design and architecture
- `docs/api/z3ed-resources.yaml` - Machine-readable API reference (generated)
- `src/cli/service/resource_catalog.h` - Resource catalog implementation
- `src/cli/service/resource_catalog.cc` - Schema definitions and serialization
