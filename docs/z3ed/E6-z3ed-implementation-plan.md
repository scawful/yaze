# z3ed Agentic Workflow Implementation Plan

**Last Updated**: October 2, 2025 (10:30 PM)  
**Status**: IT-01 Complete ✅ | IT-02 Complete ✅ | E2E Validation Ready 🎯

> 📋 **Quick Start**: See [README.md](README.md) for essential links and [NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md) for detailed task guides.

## Executive Summary

The z3ed CLI and AI agent workflow system has completed major infrastructure milestones:

**✅ Completed Phases**:
- **Phase 6**: Resource Catalogue - Machine-readable API specs for AI consumption
- **AW-01/02/03**: Acceptance Workflow - Proposal tracking, sandbox management, GUI review with ROM merging
- **IT-01**: ImGuiTestHarness - Full GUI automation via gRPC + ImGuiTestEngine (all 3 phases complete)
- **IT-02**: CLI Agent Test - Natural language → automated GUI testing (implementation complete)

**🔄 Active Phase**:
- **E2E Validation**: Testing complete proposal lifecycle with real GUI widgets (window detection debugging in progress)

**📋 Next Phases**:
- **Priority 1**: Complete E2E Validation - Fix window detection after menu actions (2-3 hours)
- **Priority 2**: Policy Evaluation Framework (AW-04) - YAML-based constraints for proposal acceptance (6-8 hours)

**Recent Accomplishments** (October 2, 2025):
- IT-02 implementation complete with async test queue pattern
- Build system fixes for z3ed target (gRPC integration)
- Documentation consolidated into clean structure
- E2E test script operational (5/6 RPCs working)
- Menu interaction verified via ImGuiTestEngine

**Known Issues**:
- Window detection timing after menu clicks needs refinement
- Screenshot RPC proto mismatch (non-critical)

**Time Investment**: 20.5 hours total (IT-01: 11h, IT-02: 7.5h, Docs: 2h)  
**Code Quality**: All targets compile cleanly, no crashes, partial test coverage

## Quick Reference

**Start Test Harness**:
```bash
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

**Test All RPCs**:
```bash
./scripts/test_harness_e2e.sh
```

**Create Proposal**:
```bash
./build/bin/z3ed agent run "Test prompt" --sandbox
./build/bin/z3ed agent list
./build/bin/z3ed agent diff --proposal-id <ID>
```

**Review in GUI**:
- Open YAZE → `Debug → Agent Proposals`
- Select proposal → Review → Accept/Reject/Delete

---

## 1. Current Priorities (Week of Oct 2-8, 2025)

**Status**: IT-01 Complete ✅ | IT-02 Complete ✅ | E2E Tests Running ⚡

### Priority 0: E2E Test Validation (IMMEDIATE) 🎯
**Goal**: Validate test harness with real YAZE widgets  
**Time Estimate**: 30-60 minutes  
**Status**: Test script running, needs real widget names

**Current Results**: 
- ✅ Ping RPC working
- ⚠️ Tests 2-5 using fake widget names
- 📋 Need to identify real widget names from YAZE source
- 🔧 Screenshot RPC needs proto fix

**Task Checklist**:
1. ✅ **E2E Test Script**: Already created (`scripts/test_harness_e2e.sh`)
2. 📋 **Manual Testing Workflow**:
   - Start YAZE with test harness enabled
   - Create proposal via CLI: `z3ed agent run "Test prompt" --sandbox`
   - Verify proposal appears in ProposalDrawer GUI
   - Test Accept → validate ROM merge and save prompt
   - Test Reject → validate status update
   - Test Delete → validate cleanup
3. 📋 **Real Widget Testing**:
   - Click actual YAZE buttons (Overworld, Dungeon, etc.)
   - Type into real input fields
   - Wait for actual windows to appear
   - Assert on real widget states
4. 📋 **Document Edge Cases**:
   - Widget not found scenarios
   - Timeout handling
   - Error recovery patterns

### Priority 2: CLI Agent Test Command (IT-02) 📋 NEXT
**Goal**: Natural language → automated GUI testing via gRPC  
**Time Estimate**: 4-6 hours  
**Blocking Dependency**: Priority 1 completion

**Implementation Tasks**:
1. **Create `z3ed agent test` command**:
   - Parse natural language prompt
   - Generate RPC call sequence (Click → Wait → Assert)
   - Execute via gRPC client
   - Capture results and screenshots
   
2. **Example Usage**:
   ```bash
   z3ed agent test --prompt "Open Overworld editor and verify it loads" \
     --rom zelda3.sfc
   
   # Generated workflow:
   # 1. Click "button:Overworld"
   # 2. Wait "window_visible:Overworld Editor" (5s)
   # 3. Assert "visible:Overworld Editor"
   # 4. Screenshot "full"
   ```

3. **Implementation Files**:
   - `src/cli/handlers/agent.cc` - Add `HandleTestCommand()`
   - `src/cli/service/gui_automation_client.{h,cc}` - gRPC client wrapper
   - `src/cli/service/test_workflow_generator.{h,cc}` - Prompt → RPC translator

### Priority 3: Policy Evaluation Framework (AW-04) 📋
**Goal**: YAML-based constraint system for gating proposal acceptance  
**Time Estimate**: 6-8 hours  
**Blocking Dependency**: None (can work in parallel)

> � **Detailed Guides**: See [NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md) for complete implementation breakdowns with code examples.

---

## 2. Workstreams Overview

This plan decomposes the design additions into actionable engineering tasks. Each workstream contains milestones, blocking dependencies, and expected deliverables.
1. `src/cli/handlers/rom.cc` - Added `RomInfo::Run` implementation
2. `src/cli/z3ed.h` - Added `RomInfo` class declaration  
3. `src/cli/modern_cli.cc` - Updated `HandleRomInfoCommand` routing
4. `src/cli/service/resource_catalog.cc` - Added `rom info` schema entry
---

## 2. Workstreams Overview

| Workstream | Goal | Status | Notes |
|------------|------|--------|-------|
| Resource Catalogue | Machine-readable CLI specs for AI consumption | ✅ Complete | `docs/api/z3ed-resources.yaml` generated |
| Acceptance Workflow | Human review/approval of agent proposals | ✅ Complete | ProposalDrawer with ROM merging operational |
| ImGuiTest Bridge | Automated GUI testing via gRPC | ✅ Complete | All 3 phases done (11 hours) |
| Verification Pipeline | Layered testing + CI coverage | 📋 In Progress | E2E validation phase |
| Telemetry & Learning | Capture signals for improvement | 📋 Planned | Optional/opt-in (Phase 8) |

### Completed Work Summary

**Resource Catalogue (RC)** ✅:
- CLI flag passthrough and resource catalog system
- `agent describe` exports YAML/JSON schemas
- `docs/api/z3ed-resources.yaml` maintained
- All ROM/Palette/Overworld/Dungeon/Patch commands documented

**Acceptance Workflow (AW-01/02/03)** ✅:
- `ProposalRegistry` with disk persistence and cross-session tracking
- `RomSandboxManager` for isolated ROM copies
- `agent list` and `agent diff` commands
- **ProposalDrawer GUI**: List/detail views, Accept/Reject/Delete, ROM merging
- Integrated into EditorManager (`Debug → Agent Proposals`)

**ImGuiTestHarness (IT-01)** ✅:
- Phase 1: gRPC infrastructure (6 RPC methods)
- Phase 2: TestManager integration with dynamic tests
- Phase 3: Full ImGuiTestEngine (Type/Wait/Assert RPCs)
- E2E test script: `scripts/test_harness_e2e.sh`
- Documentation: IT-01-QUICKSTART.md

---

## 3. Task Backlog

| ID | Task | Workstream | Type | Status | Dependencies |
|----|------|------------|------|--------|--------------|
| RC-01 | Define schema for `ResourceCatalog` entries and implement serialization helpers. | Resource Catalogue | Code | ✅ Done | Schema system complete with all resource types documented |
| RC-02 | Auto-generate `docs/api/z3ed-resources.yaml` from command annotations. | Resource Catalogue | Tooling | ✅ Done | Generated and committed to docs/api/ |
| RC-03 | Implement `z3ed agent describe` CLI surface returning JSON schemas. | Resource Catalogue | Code | ✅ Done | Both YAML and JSON output formats working |
| RC-04 | Integrate schema export with TUI command palette + help overlays. | Resource Catalogue | UX | 📋 Planned | RC-03 |
| RC-05 | Harden CLI command routing/flag parsing to unblock agent automation. | Resource Catalogue | Code | ✅ Done | Fixed rom info handler to use FLAGS_rom |
| AW-01 | Implement sandbox ROM cloning and tracking (`RomSandboxManager`). | Acceptance Workflow | Code | ✅ Done | ROM sandbox manager operational with lifecycle management |
| AW-02 | Build proposal registry service storing diffs, logs, screenshots. | Acceptance Workflow | Code | ✅ Done | ProposalRegistry implemented with disk persistence |
| AW-03 | Add ImGui drawer for proposals with accept/reject controls. | Acceptance Workflow | UX | ✅ Done | ProposalDrawer GUI complete with ROM merging |
| AW-04 | Implement policy evaluation for gating accept buttons. | Acceptance Workflow | Code | 📋 Next | AW-03, Priority 1 - YAML policies + PolicyEvaluator (6-8 hours) |
| AW-05 | Draft `.z3ed-diff` hybrid schema (binary deltas + JSON metadata). | Acceptance Workflow | Design | 📋 Planned | AW-01 |
| IT-01 | Create `ImGuiTestHarness` IPC service embedded in `yaze_test`. | ImGuiTest Bridge | Code | ✅ Done | Phase 1+2+3 Complete - Full GUI automation with gRPC + ImGuiTestEngine (11 hours) |
| IT-02 | Implement CLI agent step translation (`imgui_action` → harness call). | ImGuiTest Bridge | Code | ✅ Done | `z3ed agent test` command with natural language prompts (7.5 hours) |
| IT-03 | Provide synchronization primitives (`WaitForIdle`, etc.). | ImGuiTest Bridge | Code | ✅ Done | Wait RPC with condition polling already implemented in IT-01 Phase 3 |
| IT-04 | Complete E2E validation with real YAZE widgets | ImGuiTest Bridge | Test | 🔄 Active | IT-02, Fix window detection after menu actions (2-3 hours) |
| VP-01 | Expand CLI unit tests for new commands and sandbox flow. | Verification Pipeline | Test | 📋 Planned | RC/AW tasks |
| VP-02 | Add harness integration tests with replay scripts. | Verification Pipeline | Test | 📋 Planned | IT tasks |
| VP-03 | Create CI job running agent smoke tests with `YAZE_WITH_JSON`. | Verification Pipeline | Infra | 📋 Planned | VP-01, VP-02 |
| TL-01 | Capture accept/reject metadata and push to telemetry log. | Telemetry & Learning | Code | 📋 Planned | AW tasks |
| TL-02 | Build anonymized metrics exporter + opt-in toggle. | Telemetry & Learning | Infra | 📋 Planned | TL-01 |

_Status Legend: 🔄 Active · 📋 Planned · ✅ Done_

**Progress Summary**:
- ✅ Completed: 11 tasks (61%)
- 🔄 Active: 1 task (6%)
- 📋 Planned: 6 tasks (33%)
- **Total**: 18 tasks

## 3. Immediate Next Steps (Week of Oct 1-7, 2025)

### Priority 0: Testing & Validation (Active)
1. **TEST**: Complete end-to-end proposal workflow
   - Launch YAZE and verify ProposalDrawer displays live proposals
   - Test Accept action → verify ROM merge and save prompt
   - Test Reject and Delete actions
   - Validate filtering and refresh functionality

### Priority 1: ImGuiTestHarness Foundation (IT-01) ✅ PHASE 2 COMPLETE
**Rationale**: Required for automated GUI testing and remote control of YAZE for AI workflows  
**Decision**: ✅ **Use gRPC** - Production-grade, cross-platform, type-safe (see `IT-01-grpc-evaluation.md`)

**Status**: Phase 1 Complete ✅ | Phase 2 Complete ✅ | Phase 3 Planned �

#### Phase 1: gRPC Infrastructure ✅ COMPLETE
- ✅ Add gRPC to build system via FetchContent
- ✅ Create .proto schema (Ping, Click, Type, Wait, Assert, Screenshot)
- ✅ Implement gRPC server with all 6 RPC stubs
- ✅ Test with grpcurl - all RPCs responding
- ✅ Server lifecycle management (Start/Shutdown)
- ✅ Cross-platform build verified (macOS ARM64)

**See**: `GRPC_TEST_SUCCESS.md` for Phase 1 completion details

#### Phase 2: ImGuiTestEngine Integration ✅ COMPLETE
**Goal**: Replace stub RPC handlers with actual GUI automation  
**Status**: Infrastructure complete, dynamic test registration implemented  
**Time Spent**: ~4 hours

**Implementation Guide**: 📖 **[IT-01-PHASE2-IMPLEMENTATION-GUIDE.md](IT-01-PHASE2-IMPLEMENTATION-GUIDE.md)**

**Completed Tasks**:
1. ✅ **TestManager Integration** - gRPC service receives TestManager reference
2. ✅ **Build System** - Successfully compiles with ImGuiTestEngine support  
3. ✅ **Server Startup** - gRPC server starts correctly on macOS with test harness flag
4. ✅ **Dynamic Test Registration** - Click RPC uses `IM_REGISTER_TEST()` macro for dynamic tests
5. ✅ **Stub Handlers** - Type/Wait/Assert RPCs return success (implementation pending Phase 3)
6. ✅ **Ping RPC** - Fully functional, returns YAZE version and timestamp

**Key Learnings**:
- ImGuiTestEngine requires test registration - can't call test functions directly
- Test context provided by engine via `test->Output.Status` not `test->Status`
- YAZE uses custom flag system with `FLAGS_name->Get()` pattern
- Correct flags: `--enable_test_harness`, `--test_harness_port`, `--rom_file`

**Testing Results**:
```bash
# Server starts successfully
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Ping RPC working
grpcurl -plaintext -d '{"message":"test"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping
# Response: {"message":"Pong: test","timestampMs":"...","yazeVersion":"0.3.2"}
```

**Issues Fixed**:
- ❌→✅ SIGSEGV on TestManager initialization (deferred ImGuiTestEngine init to Phase 3)
- ❌→✅ ImGuiTestEngine API mismatch (switched to dynamic test registration)
- ❌→✅ Status field access (corrected to `test->Output.Status`)
- ❌→✅ Port conflicts (use port 50052, `killall yaze` to cleanup)
- ❌→✅ Flag naming (documented correct underscore format)

#### Phase 3: Full ImGuiTestEngine Integration ✅ COMPLETE (Oct 2, 2025)
**Goal**: Complete implementation of all GUI automation RPCs

**Completed Tasks**:
1. ✅ **Type RPC Implementation** - Full text input automation
   - ItemInfo API usage corrected (returns by value, not pointer)
   - Focus management with ItemClick before typing
   - Clear-first functionality with keyboard shortcuts
   - Dynamic test registration with timeout handling

2. ✅ **Wait RPC Implementation** - Condition polling with timeout
   - Three condition types: window_visible, element_visible, element_enabled
   - Configurable timeout (default 5000ms) and poll interval (default 100ms)
   - Proper Yield() calls to allow ImGui event processing
   - Extended timeout for test execution

3. ✅ **Assert RPC Implementation** - State validation with structured responses
   - Multiple assertion types: visible, enabled, exists, text_contains
   - Actual vs expected value reporting
   - Detailed error messages for debugging
   - text_contains partially implemented (text retrieval needs refinement)

4. ✅ **API Compatibility Fixes**
   - Corrected ItemInfo usage (by value, check ID != 0)
   - Fixed flag names (ItemFlags instead of StatusFlags)
   - Proper visibility checks using RectClipped dimensions
   - All dynamic tests properly registered and cleaned up

**Testing**: 
- Build successful on macOS ARM64
- All RPCs respond correctly
- Test script created: `scripts/test_harness_e2e.sh`
- See `IT-01-PHASE3-COMPLETE.md` for full implementation details

**Known Limitations**:
- Screenshot RPC not implemented (placeholder stub)
- text_contains assertion uses placeholder text retrieval
- Need end-to-end workflow testing with real YAZE widgets

6. **End-to-End Testing** (1 hour)
   - Create shell script workflow: start server → click button → wait for window → type text → assert state
   - Test with real YAZE editors (Overworld, Dungeon, etc.)
   - Document edge cases and troubleshooting

#### Phase 4: CLI Integration & Windows Testing (4-5 hours)
7. **CLI Client** (`z3ed agent test`)
   - Generate gRPC calls from AI prompts
   - Natural language → ImGui action translation
   - Screenshot capture for LLM feedback

8. **Windows Testing**
   - Detailed build instructions for vcpkg setup
   - Test on Windows VM or with contributor
   - Add Windows CI job to GitHub Actions
   - Document troubleshooting

### IT-01 Quick Reference

**Start YAZE with Test Harness**:
```bash
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

**Test RPCs with grpcurl**:
```bash
# Ping - Health check
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"message":"test"}' 127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping

# Click - Click UI element
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"button:Overworld","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click

# Type - Input text
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"target":"input:Filename","text":"zelda3.sfc","clear_first":true}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Type

# Wait - Wait for condition
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"window_visible:Overworld Editor","timeout_ms":5000}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Wait

# Assert - Validate state
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"condition":"visible:Main Window"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Assert
```

**Troubleshooting**:
- **Port in use**: `killall yaze` or use `--test_harness_port=50053`
- **Connection refused**: Check server started with `lsof -i :50052`
- **Unrecognized flag**: Use underscores not hyphens (e.g., `--rom_file` not `--rom`)

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

**Active Documentation**:
- `E6-z3ed-cli-design.md` - Overall CLI design and architecture
- `NEXT_PRIORITIES_OCT2.md` - Current work priorities with detailed implementation guides
- `IT-01-QUICKSTART.md` - Test harness quick reference
- `docs/api/z3ed-resources.yaml` - Machine-readable API reference (generated)

**Source Code**:
- `src/cli/service/` - Core services (proposal registry, sandbox manager, resource catalog)
- `src/app/editor/system/proposal_drawer.{h,cc}` - GUI review panel
- `src/app/core/imgui_test_harness_service.{h,cc}` - gRPC automation server

**Historical Documentation** (archived):
- `archive/STATE_SUMMARY_*.md` - Historical state snapshots
- `archive/IT-01-PHASE*-COMPLETE.md` - Phase completion reports
- `archive/*-grpc-*.md` - gRPC design decisions and technical notes
- `archive/PROGRESS_SUMMARY_*.md` - Daily progress logs

---

**Last Updated**: October 2, 2025  
**Contributors**: @scawful, GitHub Copilot  
**License**: Same as YAZE (see ../../LICENSE)
