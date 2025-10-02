# z3ed: AI-Powered CLI for YAZE

**Status**: Active Development  
**Version**: 0.1.0-alpha  
**Last Updated**: October 2, 2025 (E2E Validation 80% Complete)

## Overview

z3ed is a command-line interface for YAZE that enables AI-driven ROM modifications through a proposal-based workflow. It provides both human-accessible commands and machine-readable APIs for LLM integration.

## Core Documentation

### Essential Documents (Read These First)

1. **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - **SOURCE OF TRUTH**
   - Architecture overview
   - Design goals and principles
   - Command structure
   - Agentic workflow framework

2. **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - **TECHNICAL REFERENCE**
   - Complete command reference
   - API documentation
   - Implementation guides
   - Troubleshooting

3. **[E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)** - **IMPLEMENTATION TRACKER**
   - Task backlog and roadmap
   - Progress tracking
   - Known issues
   - Next priorities

### Quick Start Guides

4. **[IT-01-QUICKSTART.md](IT-01-QUICKSTART.md)** - Test harness quick start
   - Starting the gRPC server
   - Testing with grpcurl
   - Common workflows

5. **[AGENT_TEST_QUICKREF.md](AGENT_TEST_QUICKREF.md)** - CLI agent test command
   - Supported prompt patterns
   - Example workflows
   - Error handling

6. **[E2E_VALIDATION_GUIDE.md](E2E_VALIDATION_GUIDE.md)** - Complete validation checklist
   - Testing procedures
   - Success criteria
   - Issue reporting

### Implementation Guides

7. **[IMGUI_ID_MANAGEMENT_REFACTORING.md](IMGUI_ID_MANAGEMENT_REFACTORING.md)** - GUI ID management refactoring
   - Hierarchical widget ID system
   - Widget registry for test automation
   - Migration guide for editors
   - Integration with z3ed agent

### Status Documents

8. **[PROJECT_STATUS_OCT2.md](PROJECT_STATUS_OCT2.md)** - Current project status
   - Component completion percentages
   - Performance metrics
   - Known limitations

9. **[NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md)** - Detailed next steps
   - Priority 0-3 task breakdowns
   - Implementation guides
   - Time estimates

## Quick Start

### Build z3ed

```bash
# Basic build (without gRPC)
cmake --build build --target z3ed -j8

# With gRPC support (for GUI automation)
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target z3ed -j$(sysctl -n hw.ncpu)
```

### Basic Usage

```bash
# Display ROM info
z3ed rom info --rom=zelda3.sfc

# Export a palette
z3ed palette export sprites_aux1 4 soldier.col

# Create an agent proposal
z3ed agent run --prompt "Make soldiers red" --rom=zelda3.sfc --sandbox

# List all proposals
z3ed agent list

# View proposal changes
z3ed agent diff

# Automated GUI testing (requires test harness)
z3ed agent test --prompt "Open Overworld editor and verify it loads"
```

### Start Test Harness (Optional)

```bash
# Terminal 1: Start YAZE with test harness
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &

# Terminal 2: Run automated test
./build-grpc-test/bin/z3ed agent test \
  --prompt "Open Overworld editor"
```

## Documentation Structure

```
docs/z3ed/
├── Core Documentation (3 files)
│   ├── E6-z3ed-cli-design.md        [Source of Truth]
│   ├── E6-z3ed-reference.md         [Technical Reference]
│   └── E6-z3ed-implementation-plan.md [Tracker]
│
├── Quick Start Guides (3 files)
│   ├── IT-01-QUICKSTART.md          [Test Harness]
│   ├── AGENT_TEST_QUICKREF.md       [CLI Agent Test]
│   └── E2E_VALIDATION_GUIDE.md      [Validation]
│
├── Implementation Guides (1 file)
│   └── IMGUI_ID_MANAGEMENT_REFACTORING.md [GUI ID System]
│
├── Status Documents (4 files)
│   ├── README.md                    [This file]
│   ├── PROJECT_STATUS_OCT2.md       [Current Status]
│   ├── NEXT_PRIORITIES_OCT2.md      [Next Steps]
│   └── WORK_SUMMARY_OCT2.md         [Recent Work]
│
└── Archive (15+ files)
    └── Historical documentation and implementation notes
```

## Key Features

### ✅ Completed (Production-Ready on macOS)

- **Resource-Oriented CLI**: Clean command structure (`z3ed <resource> <action>`)
- **Resource Catalogue**: Machine-readable API specs (`docs/api/z3ed-resources.yaml`)
- **Acceptance Workflow**: Proposal tracking, sandbox management, GUI review
- **ImGuiTestHarness (IT-01)**: gRPC-based GUI automation (6 RPC methods)
- **CLI Agent Test (IT-02)**: Natural language → automated GUI tests
- **ProposalDrawer**: Integrated proposal review UI in YAZE
- **ROM Operations**: info, validate, diff, generate-golden
- **Palette Operations**: export, import, list
- **Overworld Operations**: get-tile, set-tile
- **Dungeon Operations**: list-rooms, add-object

### 🔄 In Progress (80% Complete)

- **E2E Validation**: Full workflow testing (window detection needs fix)

### 📋 Planned (Next Priorities)

1. **Policy Evaluation Framework (AW-04)**: YAML-based constraints
2. **Windows Cross-Platform Testing**: Validate on Windows with vcpkg
3. **Production Readiness**: Telemetry, screenshot, expanded tests

## Architecture Highlights

### Proposal-Based Workflow

```
User Prompt → AI Service → Sandbox ROM → Execute Commands → 
Create Proposal → Review in GUI → Accept/Reject → Commit to ROM
```

### Component Stack

```
┌─────────────────────────────────┐
│ AI Agent (LLM)                  │
├─────────────────────────────────┤
│ z3ed CLI                        │
├─────────────────────────────────┤
│ Service Layer                   │
│  • ProposalRegistry             │
│  • RomSandboxManager            │
│  • GuiAutomationClient          │
│  • TestWorkflowGenerator        │
├─────────────────────────────────┤
│ ImGuiTestHarness (gRPC)         │
├─────────────────────────────────┤
│ YAZE GUI + ProposalDrawer       │
└─────────────────────────────────┘
```

## Resources

**Machine-Readable API**: `docs/api/z3ed-resources.yaml`  
**Proto Schema**: `src/app/core/proto/imgui_test_harness.proto`  
**Test Script**: `scripts/test_harness_e2e.sh`

## Contributing

See **[B1-contributing.md](../B1-contributing.md)** for general contribution guidelines.

For z3ed-specific development:
1. Read **E6-z3ed-cli-design.md** for architecture
2. Check **E6-z3ed-implementation-plan.md** for open tasks
3. Use **E6-z3ed-reference.md** for API details
4. Follow **NEXT_PRIORITIES_OCT2.md** for current work

## License

Same as YAZE - see `LICENSE` in repository root.

---

**Last Updated**: October 2, 2025  
**Contributors**: @scawful, GitHub Copilot  
**Next Milestone**: E2E Validation Complete (Est. Oct 3, 2025)
