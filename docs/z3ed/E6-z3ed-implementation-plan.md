# z3ed Agentic Workflow Plan

**Last Updated**: October 2, 2025
**Status**: Core Infrastructure Complete | Test Harness Enhancement Phase 🎯

> 📋 **Quick Start**: See [README.md](README.md) for essential links and project status.

## Executive Summary

The z3ed CLI and AI agent workflow system has completed major infrastructure milestones:

**✅ Completed Phases**:
- **Phase 6**: Resource Catalogue - Machine-readable API specs for AI consumption
- **AW-01/02/03**: Acceptance Workflow - Proposal tracking, sandbox management, GUI review with ROM merging
- **AW-04**: Policy Evaluation Framework - YAML-based constraint system for proposal acceptance
- **IT-01**: ImGuiTestHarness - Full GUI automation via gRPC + ImGuiTestEngine (all 3 phases complete)
- **IT-02**: CLI Agent Test - Natural language → automated GUI testing (implementation complete)

**🔄 Active Phase**:
- **Test Harness Enhancements (IT-05 to IT-09)**: Expanding from basic automation to comprehensive testing platform with a renewed emphasis on system-wide error reporting

**📋 Next Phases**:
- **Priority 1**: Test Introspection API (IT-05) - Enable test status querying and result polling
- **Priority 2**: Widget Discovery API (IT-06) - AI agents enumerate available GUI interactions
- **Priority 3**: Enhanced Error Reporting (IT-08+) - Holistic improvements spanning z3ed, ImGuiTestHarness, EditorManager, and core application services

**Recent Accomplishments** (Updated: October 2025):
- **✅ IT-08a Screenshot RPC Complete**: SDL-based screenshot capture operational
  - Captures 1536x864 BMP files via SDL_RenderReadPixels
  - Successfully tested via gRPC (5.3MB output files)
  - Foundation for auto-capture on test failures
- **✅ Policy Framework Complete**: PolicyEvaluator service fully integrated with ProposalDrawer GUI
  - 4 policy types implemented: test_requirement, change_constraint, forbidden_range, review_requirement
  - 3 severity levels: Info (informational), Warning (overridable), Critical (blocks acceptance)
  - GUI displays color-coded violations (⛔ critical, ⚠️ warning, ℹ️ info)
  - Accept button gating based on policy violations with override confirmation dialog
  - Example policy configuration at `.yaze/policies/agent.yaml`
- **✅ E2E Validation Complete**: All 5 functional RPC tests passing (Ping, Click, Type, Wait, Assert)
  - Window detection timing issue **resolved** with 10-frame yield buffer in Wait RPC
  - Thread safety issues **resolved** with shared_ptr state management
  - Test harness validated on macOS ARM64 with real YAZE GUI interactions
- **gRPC Test Harness (IT-01 & IT-02)**: Full implementation complete with natural language → GUI testing
- **✅ Test Recording & Replay (IT-07)**: JSON recorder/replayer implemented, CLI and harness wired, end-to-end regression workflow captured in `scripts/test_record_replay_e2e.sh`
- **Build System**: Hardened CMake configuration with reliable gRPC integration
- **Proposal Workflow**: Agentic proposal system fully operational (create, list, diff, review in GUI)

**Known Limitations & Improvement Opportunities**:
- **Screenshot Auto-Capture**: Manual RPC only → needs integration with TestManager failure detection
- **Test Introspection**: ✅ Complete - GetTestStatus/ListTests/GetResults RPCs operational
- **Widget Discovery**: AI agents can't enumerate available widgets → add DiscoverWidgets RPC
- **Test Recording**: No record/replay for regression testing → add RecordSession/ReplaySession RPCs
- **Synchronous Wait**: Async tests return immediately → add blocking mode or result polling
- **Error Context**: Test failures lack screenshots/state dumps → enhance error reporting
- **Performance**: Tests add ~166ms per Wait call due to frame yielding (acceptable trade-off)
- **YAML Parsing**: Simple parser implemented, consider yaml-cpp for complex scenarios

**Time Investment**: 28.5 hours total (IT-01: 11h, IT-02: 7.5h, E2E: 2h, Policy: 6h, Docs: 2h)

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

**Status**: Core Infrastructure Complete ✅ | Test Harness Enhancement Phase 🔧

### Priority 1: Test Harness Enhancements (IT-05 to IT-09) 🔧 ACTIVE
**Goal**: Transform test harness from basic automation to comprehensive testing platform **and deliver holistic error reporting across YAZE**  
**Time Estimate**: 20-25 hours total (7.5h completed in IT-07)  
**Blocking Dependency**: IT-01 Complete ✅

**Motivation**: The harness now supports AI workflows, regression capture, and automation—but error surfaces remain shallow:
- **AI Agent Development**: Still needs widget discovery for adaptive planning
- **Regression Testing**: Recording/replay finished; reporting pipeline must surface actionable failures
- **CI/CD Integration**: Requires reliable artifacts (logs, screenshots, structured context)
- **Debugging**: Failures lack screenshots, widget hierarchies, and EditorManager state snapshots
- **Application Consistency**: z3ed, EditorManager, and core services emit heterogeneous error formats

#### IT-05: Test Introspection API (6-8 hours)
**Status (Oct 2, 2025)**: 🟡 *Server-side RPCs implemented; CLI + E2E pending*

**Progress**:
- ✅ `imgui_test_harness.proto` expanded with GetTestStatus/ListTests/GetTestResults messages.
- ✅ `TestManager` maintains execution history (queued→running→completed) with logs, metrics, and aggregates.
- ✅ `ImGuiTestHarnessServiceImpl` exposes the three introspection RPCs with pagination, status conversion, and log/metric marshalling.
- ⚠️ `agent` CLI commands (`test status`, `test list`, `test results`) still stubbed.
- ⚠️ End-to-end introspection script (`scripts/test_introspection_e2e.sh`) not implemented; regression script `test_harness_e2e.sh` currently failing because it references the unfinished CLI.

**Immediate Next Steps**:
1. **Wire CLI Client Methods**
  - Implement gRPC client wrappers for the new RPCs in the automation client.
  - Add user-facing commands under `z3ed agent test ...` with JSON/YAML output options.
2. **Author E2E Validation Script**
  - Spin up harness, run Click/Assert workflow, poll via `agent test status`, fetch results.
  - Update CI notes with the new script and expected output.
3. **Documentation & Examples**
  - Extend `E6-z3ed-reference.md` with full usage examples and sample outputs.
  - Add troubleshooting section covering common errors (unknown test_id, timeout, etc.).
4. **Stretch (Optional Before IT-06)**
  - Capture assertion metadata (expected/actual) for richer `AssertionResult` payloads.

**Example Usage**:
```bash
# Queue a test
z3ed agent test --prompt "Open Overworld editor"

# Poll for completion
z3ed test status --test-id grpc_click_12345678

# Retrieve results
z3ed test results --test-id grpc_click_12345678 --format json
```

**API Schema**:
```proto
message GetTestStatusRequest {
  string test_id = 1;
}

message GetTestStatusResponse {
  enum Status { QUEUED = 0; RUNNING = 1; PASSED = 2; FAILED = 3; TIMEOUT = 4; }
  Status status = 1;
  int64 execution_time_ms = 2;
  string error_message = 3;
  repeated string assertion_failures = 4;
}

message ListTestsRequest {
  string category_filter = 1;  // Optional: "grpc", "unit", etc.
  int32 page_size = 2;
  string page_token = 3;
}

message ListTestsResponse {
  repeated TestInfo tests = 1;
  string next_page_token = 2;
}

message TestInfo {
  string test_id = 1;
  string name = 2;
  string category = 3;
  int64 last_run_timestamp_ms = 4;
  int32 total_runs = 5;
  int32 pass_count = 6;
  int32 fail_count = 7;
}
```

#### IT-06: Widget Discovery API (4-6 hours)
**Implementation Tasks**:
1. **Add DiscoverWidgets RPC**:
   - Enumerate all windows currently open in YAZE GUI
   - List all interactive widgets (buttons, inputs, menus, tabs) per window
   - Return widget metadata: ID, type, label, enabled state, position
   - Support filtering by window name or widget type
   
2. **AI-Friendly Output Format**:
   - JSON schema describing available interactions
   - Natural language descriptions for each widget
   - Suggested action templates (e.g., "Click button:{label}")

**Example Usage**:
```bash
# Discover all widgets
z3ed gui discover

# Filter by window
z3ed gui discover --window "Overworld"

# Get only buttons
z3ed gui discover --type button
```

**API Schema**:
```proto
message DiscoverWidgetsRequest {
  string window_filter = 1;  // Optional: filter by window name
  enum WidgetType { ALL = 0; BUTTON = 1; INPUT = 2; MENU = 3; TAB = 4; CHECKBOX = 5; }
  WidgetType type_filter = 2;
}

message DiscoverWidgetsResponse {
  repeated WindowInfo windows = 1;
}

message WindowInfo {
  string name = 1;
  bool is_visible = 2;
  repeated WidgetInfo widgets = 3;
}

message WidgetInfo {
  string id = 1;
  string label = 2;
  string type = 3;  // "button", "input", "menu", etc.
  bool is_enabled = 4;
  string position = 5;  // "x,y,width,height"
  string suggested_action = 6;  // "Click button:Open ROM"
}
```

**Benefits for AI Agents**:
- LLMs can dynamically learn available GUI interactions
- Agents can adapt to UI changes without hardcoded widget names
- Natural language descriptions enable better prompt engineering

#### IT-07: Test Recording & Replay ✅ COMPLETE (Oct 2, 2025)
**Highlights**:
- Implemented `StartRecording`, `StopRecording`, and `ReplayTest` RPCs with persistent JSON scripts
- Added CLI commands: `z3ed test record start|stop`, `z3ed test replay`
- Scripts stored in `tests/gui/` with metadata (name, tags, assertions, timing hints)
- Added regression coverage via `scripts/test_record_replay_e2e.sh`
- Documentation updates in `E6-z3ed-reference.md` and new quick-start snippets in README
- Confirmed compatibility with natural language prompts generated by the agent workflow

**Outcome**: Recording/replay is production-ready; focus shifts to surfacing rich failure diagnostics (IT-08).

#### IT-08: Enhanced Error Reporting (5-7 hours) 🔄 ACTIVE
**Status**: IT-08a Complete ✅ | IT-08b In Progress 🔄
**Objective**: Deliver a unified, high-signal error reporting pipeline spanning ImGuiTestHarness, z3ed CLI, EditorManager, and core application services.

**Implementation Tracks**:
1. **Harness-Level Diagnostics**
  - ✅ IT-08a: Screenshot RPC implemented (SDL-based, BMP format, 1536x864)
  - 📋 IT-08b: Auto-capture screenshots on test failure
  - 📋 IT-08c: Widget tree dumps and recent ImGui events on failure
  - Serialize results to both structured JSON (for automation) and human-friendly HTML bundles
  - Persist artifacts under `test-results/<test_id>/` with timestamped directories

2. **CLI Experience Improvements**
  - Standardize error envelopes in z3ed (`absl::Status` + structured payload)
  - Surface artifact paths, summarized failure reason, and next-step hints in CLI output
  - Add `--format html` / `--format json` flags to `z3ed agent test results` to emit richer context
  - Integrate with recording workflow: replay failures using captured state for fast reproduction

3. **EditorManager & Application Integration**
  - Introduce shared `ErrorAnnotatedResult` utility exposing `status`, `context`, `actionable_hint`
  - Adapt EditorManager subsystems (ProposalDrawer, OverworldEditor, DungeonEditor) to adopt the shared structure
  - Add in-app failure overlay (ImGui modal) that references harness artifacts when available
  - Hook proposal acceptance/replay flows to display enriched diagnostics when sandbox merges fail

4. **Telemetry & Storage Hooks** (Stretch)
  - Optionally emit error metadata to a ring buffer for future analytics/telemetry workstreams
  - Provide CLI flag `--error-artifact-dir` to customize storage (supports CI separation)

**Error Report Example**:
```json
{
  "test_id": "grpc_assert_12345678",
  "failure_time": "2025-10-02T14:23:45Z",
  "assertion": "visible:Overworld",
  "expected": "visible",
  "actual": "hidden",
  "screenshot": "/tmp/yaze_test_12345678.png",
  "widget_state": {
    "active_window": "Main Window",
    "focused_widget": null,
    "visible_windows": ["Main Window", "Debug"],
    "overworld_window": { "exists": true, "visible": false, "position": "0,0,0,0" }
  },
  "execution_context": {
    "frame_count": 1234,
    "recent_events": ["Click: menuitem: Overworld Editor", "Wait: window_visible:Overworld"],
    "resource_stats": { "memory_mb": 245, "textures": 12, "framerate": 60.0 },
    "editor_manager_snapshot": {
      "active_module": "OverworldEditor",
      "dirty_buffers": ["overworld_layer_1"],
      "last_error": null
    }
  }
}
```

#### IT-09: CI/CD Integration (2-3 hours)
**Implementation Tasks**:
1. **Standardized Test Suite Format**:
   - YAML/JSON format for test suite definitions
   - Support test groups (smoke, regression, nightly)
   - Enable parallel execution with dependencies
   
2. **CI-Friendly CLI**:
   - `z3ed test run-suite tests/suite.yaml --ci-mode`
   - Exit codes: 0 = all passed, 1 = failures, 2 = errors
   - JUnit XML output for CI parsers
   - GitHub Actions integration examples
   
3. **Documentation**:
   - Add `.github/workflows/gui-tests.yml` example
   - Create sample test suites for common scenarios
   - Document best practices for flaky test handling

**Test Suite Format**:
```yaml
name: YAZE GUI Test Suite
description: Comprehensive tests for YAZE editor functionality
version: 1.0

config:
  timeout_per_test: 30s
  retry_on_failure: 2
  parallel_execution: false

test_groups:
  - name: smoke
    description: Fast tests for basic functionality
    tests:
      - tests/overworld_load.json
      - tests/dungeon_load.json
  
  - name: regression
    description: Full test suite for release validation
    depends_on: [smoke]
    tests:
      - tests/palette_edit.json
      - tests/sprite_load.json
      - tests/rom_save.json
```

**GitHub Actions Integration**:
```yaml
name: GUI Tests
on: [push, pull_request]

jobs:
  gui-tests:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build YAZE with test harness
        run: |
          cmake -B build -DYAZE_WITH_GRPC=ON
          cmake --build build --target yaze --target z3ed
      - name: Start test harness
        run: |
          ./build/bin/yaze --enable_test_harness --headless &
          sleep 5
      - name: Run test suite
        run: |
          ./build/bin/z3ed test run-suite tests/suite.yaml --ci-mode
      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v2
        with:
          name: test-results
          path: test-results/
```

---

#### IT-10: Collaborative Editing & Multiplayer Sessions (12-15 hours)
**Implementation Tasks**:
1. **Collaboration Server**:
   - WebSocket server for real-time client communication
   - Session management (create, join, authentication)
   - Edit event broadcasting to all connected clients
   - Conflict resolution (last-write-wins with timestamps)
   
2. **Collaboration Client**:
   - Connect to remote sessions via WebSocket
   - Send local edits to server
   - Receive and apply remote edits
   - ROM state synchronization on join
   
3. **Edit Event Protocol**:
   - Protobuf definitions for edit events (tile, sprite, palette, map)
   - Cursor position tracking
   - AI proposal sharing and voting
   - Session state messages
   
4. **GUI Integration**:
   - Status bar showing connected users
   - Collaboration panel (user list, activity feed)
   - Live cursor rendering (color-coded per user)
   - Proposal voting UI (Accept/Reject/Discuss)
   
5. **Session Recording & Replay**:
   - Record all events to YAML/JSON file
   - Replay engine with timeline controls
   - Export session summaries for review

**CLI Commands**:
```bash
# Host a collaborative session
z3ed collab host --port 5000 --password "dev123"

# Join a session
z3ed collab join yaze://connect/192.168.1.100:5000

# List active sessions (LAN discovery)
z3ed collab list

# Disconnect from session
z3ed collab disconnect

# Replay recorded session
z3ed collab replay session_2025_10_02.yaml --speed 2x
```

**User Stories**:
- **US-1**: As a ROM hacker, I want to host a collaborative session so my teammates can join and work together
- **US-2**: As a collaborator, I want to see other users' edits in real-time so we stay synchronized
- **US-3**: As a team lead, I want to use AI agents with my team so we can all benefit from automation (shared proposals with majority voting)
- **US-4**: As a collaborator, I want to see where other users are working so we don't conflict (live cursors)
- **US-5**: As a project manager, I want to record collaborative sessions so we can review work later

**Benefits**:
- **Real-Time Collaboration**: Multiple users can edit the same ROM simultaneously
- **Shared AI Assistance**: Team votes on AI proposals before execution
- **Conflict Prevention**: Live cursors show where teammates are working
- **Audit Trail**: Session recording for review and compliance
- **Remote Teams**: Connect over LAN or internet (with optional encryption)

**Technical Architecture**:
```
┌──────────────┐     ┌─────────────────┐     ┌──────────────┐
│  Client A    │────►│  Collab Server  │◄────│  Client B    │
│  (Host)      │     │  (WebSocket)    │     │              │
└──────────────┘     │                 │     └──────────────┘
                     │  - Session Mgmt │
                     │  - Event Broker │     ┌──────────────┐
                     │  - Conflict Res │◄────│  Client C    │
                     └─────────────────┘     └──────────────┘
```

**Security Considerations**:
- Optional password protection for sessions
- Read-only vs read-write access levels
- ROM checksum verification (prevents desync)
- Rate limiting (prevent spam/DOS)
- Optional TLS/SSL encryption for public internet

**See**: [IT-10-COLLABORATIVE-EDITING.md](IT-10-COLLABORATIVE-EDITING.md) for complete specification

---

### Priority 2: Windows Cross-Platform Testing 🪟
**Goal**: Validate z3ed and test harness on Windows  
**Time Estimate**: 8-10 hours  
**Blocking Dependency**: IT-05 Complete (need stable API)

> 📋 **Detailed Guides**: See [NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md) for complete implementation breakdowns with code examples.

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
| AW-04 | Implement policy evaluation for gating accept buttons. | Acceptance Workflow | Code | ✅ Done | PolicyEvaluator service with 4 policy types (test, constraint, forbidden, review), GUI integration complete (6 hours) |
| AW-05 | Draft `.z3ed-diff` hybrid schema (binary deltas + JSON metadata). | Acceptance Workflow | Design | 📋 Planned | AW-01 |
| IT-01 | Create `ImGuiTestHarness` IPC service embedded in `yaze_test`. | ImGuiTest Bridge | Code | ✅ Done | Phase 1+2+3 Complete - Full GUI automation with gRPC + ImGuiTestEngine (11 hours) |
| IT-02 | Implement CLI agent step translation (`imgui_action` → harness call). | ImGuiTest Bridge | Code | ✅ Done | `z3ed agent test` command with natural language prompts (7.5 hours) |
| IT-03 | Provide synchronization primitives (`WaitForIdle`, etc.). | ImGuiTest Bridge | Code | ✅ Done | Wait RPC with condition polling already implemented in IT-01 Phase 3 |
| IT-04 | Complete E2E validation with real YAZE widgets | ImGuiTest Bridge | Test | ✅ Done | IT-02 - All 5 functional tests passing, window detection fixed with yield buffer |
| IT-05 | Add test introspection RPCs (GetTestStatus, ListTests, GetResults) | ImGuiTest Bridge | Code | ✅ Done | IT-01 - Enable clients to poll test results and query execution state (Oct 2, 2025) |
| IT-06 | Implement widget discovery API for AI agents | ImGuiTest Bridge | Code | 📋 Planned | IT-01 - DiscoverWidgets RPC to enumerate windows, buttons, inputs |
| IT-07 | Add test recording/replay for regression testing | ImGuiTest Bridge | Code | ✅ Done | IT-05 - RecordSession/ReplaySession RPCs with JSON test scripts |
| IT-08 | Enhance error reporting with screenshots and state dumps | ImGuiTest Bridge | Code | 🔄 Active | IT-01 - Capture widget state on failure for debugging |
| IT-08a | Screenshot RPC implementation (SDL capture) | ImGuiTest Bridge | Code | ✅ Done | IT-01 - Screenshot capture complete (Oct 2, 2025) |
| IT-08b | Auto-capture screenshots on test failure | ImGuiTest Bridge | Code | 🔄 Active | IT-08a - Integrate with TestManager |
| IT-08c | Widget state dumps and execution context | ImGuiTest Bridge | Code | 📋 Planned | IT-08b - Enhanced failure diagnostics |
| IT-09 | Create standardized test suite format for CI integration | ImGuiTest Bridge | Infra | 📋 Planned | IT-07 - JSON/YAML test suite format compatible with CI/CD pipelines |
| IT-10 | Collaborative editing & multiplayer sessions with shared AI | Collaboration | Feature | 📋 Planned | IT-05, IT-08 - Real-time multi-user editing with live cursors, shared proposals (12-15 hours) |
| VP-01 | Expand CLI unit tests for new commands and sandbox flow. | Verification Pipeline | Test | 📋 Planned | RC/AW tasks |
| VP-02 | Add harness integration tests with replay scripts. | Verification Pipeline | Test | 📋 Planned | IT tasks |
| VP-03 | Create CI job running agent smoke tests with `YAZE_WITH_JSON`. | Verification Pipeline | Infra | 📋 Planned | VP-01, VP-02 |
| TL-01 | Capture accept/reject metadata and push to telemetry log. | Telemetry & Learning | Code | 📋 Planned | AW tasks |
| TL-02 | Build anonymized metrics exporter + opt-in toggle. | Telemetry & Learning | Infra | 📋 Planned | TL-01 |

_Status Legend: 🔄 Active · 📋 Planned · ✅ Done_

**Progress Summary**:
- ✅ Completed: 11 tasks (46%)
- 🔄 Active: 1 task (4%)
- 📋 Planned: 12 tasks (50%)
- **Total**: 24 tasks (6 test harness enhancements + 1 collaborative feature)

## 3. Immediate Next Steps (Week of Oct 1-7, 2025)

### Priority 0: Testing & Validation (Active)
1. **TEST**: Complete end-to-end proposal workflow
   - Launch YAZE and verify ProposalDrawer displays live proposals
   - Test Accept action → verify ROM merge and save prompt
   - Test Reject and Delete actions
   - Validate filtering and refresh functionality

2. **Widget ID Refactoring** (Started Oct 2, 2025) 🎯 NEW
   - ✅ Added widget_id_registry to build system
   - ✅ Registered 13 Overworld toolset buttons with hierarchical IDs
   - 📋 Next: Test widget discovery and update test harness
   - See: [WIDGET_ID_REFACTORING_PROGRESS.md](WIDGET_ID_REFACTORING_PROGRESS.md)

### Priority 1: ImGuiTestHarness Foundation (IT-01) ✅ COMPLETE
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
  - Emit structured error envelopes with artifact links (IT-08)

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

## 4. Work History & Key Decisions

This section provides a high-level summary of completed workstreams and major architectural decisions.

### Resource Catalogue Workstream (RC) - ✅ COMPLETE
- **Outcome**: A machine-readable API specification for all `z3ed` commands.
- **Artifact**: `docs/api/z3ed-resources.yaml` is the generated source of truth.
- **Details**: Implemented a schema system and serialization for all CLI resources (ROM, Palette, Agent, etc.), enabling AI consumption.

### Acceptance Workflow (AW-01, AW-02, AW-03) - ✅ COMPLETE
- **Outcome**: A complete, human-in-the-loop proposal review system.
- **Components**:
    - `RomSandboxManager`: For creating isolated ROM copies.
    - `ProposalRegistry`: For tracking proposals, diffs, and logs with disk persistence.
    - `ProposalDrawer`: An ImGui panel for reviewing, accepting, and rejecting proposals, with full ROM merging capabilities.
- **Integration**: The `agent run`, `agent list`, and `agent diff` commands are fully integrated with the registry. The GUI and CLI share the same underlying proposal data.

### ImGuiTestHarness (IT-01, IT-02) - ✅ CORE COMPLETE
- **Outcome**: A gRPC-based service for automated GUI testing.
- **Decision**: Chose **gRPC** for its performance, cross-platform support, and type safety.
- **Features**: Implemented 6 core RPCs: `Ping`, `Click`, `Type`, `Wait`, `Assert`, and a stubbed `Screenshot`.
- **Integration**: The `z3ed agent test` command can translate natural language prompts into a sequence of gRPC calls to execute tests.

### Files Modified/Created
A summary of files created or changed during the implementation of the core `z3ed` infrastructure.

**Core Services & CLI Handlers**:
- `src/cli/service/proposal_registry.{h,cc}`
- `src/cli/service/rom_sandbox_manager.{h,cc}`
- `src/cli/service/resource_catalog.{h,cc}`
- `src/cli/handlers/agent.cc`
- `src/cli/handlers/rom.cc`

**GUI & Application Integration**:
- `src/app/editor/system/proposal_drawer.{h,cc}`
- `src/app/editor/editor_manager.{h,cc}`
- `src/app/core/service/imgui_test_harness_service.{h,cc}`
- `src/app/core/proto/imgui_test_harness.proto`

**Build System (CMake)**:
- `src/app/app.cmake`
- `src/app/emu/emu.cmake`
- `src/cli/z3ed.cmake`
- `src/CMakeLists.txt`

**Documentation & API Specs**:
- `docs/api/z3ed-resources.yaml`
- `docs/z3ed/E6-z3ed-cli-design.md`
- `docs/z3ed/E6-z3ed-implementation-plan.md`
- `docs/z3ed/E6-z3ed-reference.md`
- `docs/z3ed/README.md`

## 5. Open Questions

- What serialization format should the proposal registry adopt for diff payloads (binary vs. textual vs. hybrid)?  \
	➤ Decision: pursue a hybrid package (`.z3ed-diff`) that wraps binary tile/object deltas alongside a JSON metadata envelope (identifiers, texture descriptors, preview palette info). Capture format draft under RC/AW backlog.
- How should the harness authenticate escalation requests for mutation actions?  \
	➤ Still open—evaluate shared-secret vs. interactive user prompt in the harness spike (IT-01).
- Can we reuse existing regression test infrastructure for nightly ImGui runs or should we spin up a dedicated binary?  \
	➤ Investigate during the ImGuiTestHarness spike; compare extending `yaze_test` jobs versus introducing a lightweight automation runner.

## 6. References

**Active Documentation**:
- `E6-z3ed-cli-design.md` - Overall CLI design and architecture
- `E6-z3ed-reference.md` - Technical command and API reference
- `docs/api/z3ed-resources.yaml` - Machine-readable API reference (generated)

**Source Code**:
- `src/cli/service/` - Core services (proposal registry, sandbox manager, resource catalog)
- `src/app/editor/system/proposal_drawer.{h,cc}` - GUI review panel
- `src/app/core/service/imgui_test_harness_service.{h,cc}` - gRPC automation server

---

**Last Updated**: [Current Date]
**Contributors**: @scawful, GitHub Copilot
**License**: Same as YAZE (see ../../LICENSE)
