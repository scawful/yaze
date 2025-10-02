# z3ed: AI-Powered CLI for YAZE

**Status**: Active Development  
**Version**: 0.1.0-alpha  
**Last Updated**: October 2, 2025 (IT-01 Complete, E2E Validation Phase)

## Overview

`z3ed` is a command-line interface for YAZE (Yet Another Zelda3 Editor) that enables AI-driven ROM modifications through a proposal-based workflow. It allows AI agents to suggest changes, which are then reviewed and accepted/rejected by human operators via the YAZE GUI.

## Documentation Index

### 🎯 Essential Documents (Start Here)
1. **[Next Priorities](NEXT_PRIORITIES_OCT2.md)** - 🚀 **CURRENT WORK** - Detailed Priority 1-3 tasks with implementation guides
2. **[Implementation Plan](E6-z3ed-implementation-plan.md)** - ⭐ **MASTER TRACKER** - Complete task backlog, progress, architecture
3. **[CLI Design](E6-z3ed-cli-design.md)** - 📐 **DESIGN DOC** - High-level vision, command structure, workflows
4. **[IT-01 Quickstart](IT-01-QUICKSTART.md)** - ⚡ **QUICK REFERENCE** - Test harness commands and examples

### � Archive
Historical documentation (design decisions, phase completions, technical notes) moved to `archive/` folder for reference.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│ z3ed CLI                                                │
│  └─ agent subcommand                                    │
│     ├─ run <prompt> [--sandbox]                         │
│     ├─ list                                             │
│     └─ test <prompt>                                    │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ Services Layer (Singleton Services)                     │
│  ├─ ProposalRegistry                                    │
│  │   ├─ CreateProposal()                                │
│  │   ├─ ListProposals()                                 │
│  │   └─ LoadProposalsFromDiskLocked()                   │
│  ├─ RomSandboxManager                                   │
│  │   ├─ CreateSandbox()                                 │
│  │   └─ FindSandbox()                                   │
│  └─ PolicyEvaluator (Planned)                           │
│      ├─ LoadPolicies()                                  │
│      └─ EvaluateProposal()                              │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ Filesystem Layer                                        │
│  ├─ /tmp/yaze/proposals/<id>/                          │
│  │   ├─ metadata.json                                   │
│  │   ├─ execution.log                                   │
│  │   └─ diff.txt                                        │
│  └─ /tmp/yaze/sandboxes/<id>/                          │
│      └─ zelda3.sfc (copy)                               │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│ YAZE GUI                                                │
│  └─ ProposalDrawer (400px right panel)                  │
│     ├─ List View (proposals from registry)              │
│     ├─ Detail View (metadata, diff, log)                │
│     └─ AcceptProposal() → ROM merging                   │
└─────────────────────────────────────────────────────────┘
```

## Current Status

### ✅ Phase 6: Resource Catalogue (COMPLETE)
- **Resource Catalog System**: Comprehensive schema for all CLI commands
- **Agent Describe**: Machine-readable API catalog export (JSON/YAML)
- **API Documentation**: `docs/api/z3ed-resources.yaml` for AI/LLM consumption

### ✅ AW-01 & AW-02: Proposal Infrastructure (COMPLETE)
- **ProposalRegistry**: Disk persistence with lazy loading
- **RomSandboxManager**: Isolated ROM copies for safe testing
- **Cross-Session Tracking**: Proposals persist between CLI runs

### ✅ AW-03: ProposalDrawer GUI (COMPLETE)
- **ProposalDrawer GUI**: Split view, proposal list, detail panel
- **ROM Merging**: Sandbox-to-main ROM data copy on acceptance
- **Full Lifecycle**: Create (CLI) → Review (GUI) → Accept/Reject → Commit

### ✅ IT-01: ImGuiTestHarness (COMPLETE) 🎉
**All 3 Phases Complete**: gRPC + TestManager + ImGuiTestEngine  
**Time Invested**: 11 hours total (Phase 1: 4h, Phase 2: 4h, Phase 3: 3h)

- **Phase 1** ✅: gRPC infrastructure with 6 RPC methods
- **Phase 2** ✅: TestManager integration with dynamic test registration
- **Phase 3** ✅: Full ImGuiTestEngine integration (Type/Wait/Assert RPCs)
- **Testing** ✅: E2E test script operational (`scripts/test_harness_e2e.sh`)
- **Documentation** ✅: Complete guides (QUICKSTART, PHASE3-COMPLETE)

**See**: [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md) for usage examples

### ✅ IT-02: CLI Agent Test Command (COMPLETE) 🎉
**Implementation Complete**: Natural language → automated GUI testing  
**Time Invested**: 4 hours (design + implementation + documentation)  
**Status**: Ready for validation

**Components**:
- **GuiAutomationClient**: gRPC wrapper for CLI usage (6 RPC methods)
- **TestWorkflowGenerator**: Natural language prompt parser (4 pattern types)
- **`z3ed agent test`**: End-to-end automation command

**Supported Prompts**:
1. "Open Overworld editor" → Click + Wait
2. "Open Dungeon editor and verify it loads" → Click + Wait + Assert
3. "Type 'zelda3.sfc' in filename input" → Click + Type
4. "Click Open ROM button" → Single click

**Example Usage**:
```bash
# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Run automated test
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Overworld editor"

# Output:
# === GUI Automation Test ===
# Prompt: Open Overworld editor
# ...
# [1/2] Click(button:Overworld) ... ✓ (125ms)
# [2/2] Wait(window_visible:Overworld Editor, 5000ms) ... ✓ (1250ms)
# ✅ Test passed in 1375ms
```

**See**: [IMPLEMENTATION_PROGRESS_OCT2.md](IMPLEMENTATION_PROGRESS_OCT2.md) for complete details

### 📋 Priority 1: End-to-End Workflow Validation (NEXT)
**Goal**: Test complete proposal lifecycle with real GUI and widgets  
**Time Estimate**: 2-3 hours  
**Status**: Ready to execute - all prerequisites complete

**Tasks**:
1. Run E2E test script and validate all RPCs
2. Test proposal workflow: Create → Review → Accept/Reject
3. Test GUI automation with real YAZE widgets
4. Validate CLI agent test command with multiple prompts
5. Document edge cases and troubleshooting

**See**: [E2E_VALIDATION_GUIDE.md](E2E_VALIDATION_GUIDE.md) for detailed checklist

### 📋 Priority 3: Policy Evaluation Framework (AW-04)
**Goal**: YAML-based constraint system for gating proposal acceptance  
**Time Estimate**: 6-8 hours  
**Blocking**: None (can work in parallel)

## Quick Start

### Build and Test
```bash
# Build z3ed CLI
cmake --build build --target z3ed -j8

# Build YAZE with test harness
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target yaze -j$(sysctl -n hw.ncpu)

# Run E2E tests
./scripts/test_harness_e2e.sh
```

### Create and Review Proposal
```bash
# 1. Create proposal
./build/bin/z3ed agent run "Test proposal" --sandbox

# 2. List proposals
./build/bin/z3ed agent list

# 3. Review in GUI
./build/bin/yaze.app/Contents/MacOS/yaze
# Open: Debug → Agent Proposals
```

### Test Harness Usage
```bash
# Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Test individual RPCs (see IT-01-QUICKSTART.md for full reference)
grpcurl -plaintext -import-path src/app/core/proto -proto imgui_test_harness.proto \
  -d '{"message":"test"}' 127.0.0.1:50052 yaze.test.ImGuiTestHarness/Ping
```

## Key Files & Components

**Core Services**:
- `src/cli/service/proposal_registry.{h,cc}` - Proposal tracking and persistence
- `src/cli/service/rom_sandbox_manager.{h,cc}` - Isolated ROM copies
- `src/cli/service/resource_catalog.{h,cc}` - Machine-readable API specs

**GUI Integration**:
- `src/app/editor/system/proposal_drawer.{h,cc}` - Proposal review panel
- `src/app/core/imgui_test_harness_service.{h,cc}` - gRPC test automation server

**CLI Handlers**:
- `src/cli/handlers/agent.cc` - Agent subcommand (run, list, diff, describe)
- `src/cli/handlers/rom.cc` - ROM commands (info, validate, diff)

**Configuration**:
- `docs/api/z3ed-resources.yaml` - Generated API catalog for AI/LLM
- `.yaze/policies/agent.yaml` - (Planned) Policy rules

## Development Guidelines

See `docs/B1-contributing.md` for general guidelines.

**z3ed-Specific**:
- Use singleton pattern for services (`Instance()` accessor)
- Return `absl::Status` or `absl::StatusOr<T>` for error handling
- Update `NEXT_PRIORITIES_OCT2.md` when starting new work
- Update `E6-z3ed-implementation-plan.md` task backlog when completing tasks

---

**Questions?** Open an issue or see implementation plan for detailed architecture.
