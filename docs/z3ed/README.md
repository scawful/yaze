# z3ed: AI-Powered CLI for YAZE

**Status**: Active Development  
**Version**: 0.1.0-alpha  
**Last Updated**: October 1, 2025

## Overview

`z3ed` is a command-line interface for YAZE (Yet Another Zelda3 Editor) that enables AI-driven ROM modifications through a proposal-based workflow. It allows AI agents to suggest changes, which are then reviewed and accepted/rejected by human operators via the YAZE GUI.

## Documentation Index

### Getting Started
- **[State Summary](STATE_SUMMARY_2025-10-01.md)** - 📊 **START HERE** - Complete current state, architecture, and workflows
- **[Implementation Plan](E6-z3ed-implementation-plan.md)** - Master tracking document with architecture, priorities, and progress
- **[CLI Design](E6-z3ed-cli-design.md)** - Command structure, service architecture, and API design

### Implementation Guides
- **[IT-01: gRPC Evaluation](IT-01-grpc-evaluation.md)** - Detailed analysis of gRPC for ImGuiTestHarness IPC
- **[IT-01: Getting Started with gRPC](IT-01-getting-started-grpc.md)** - Step-by-step implementation guide
- **[gRPC Technical Notes](GRPC_TECHNICAL_NOTES.md)** - Build issues and solutions reference
- **[gRPC Test Success](GRPC_TEST_SUCCESS.md)** - Complete testing log and validation

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

### ✅ Completed (AW-03)
- **ProposalRegistry**: Disk persistence with lazy loading
- **ProposalDrawer GUI**: Split view, proposal list, detail panel
- **ROM Merging**: Sandbox-to-main ROM data copy on acceptance
- **Cross-Session Tracking**: Proposals persist between CLI runs

### 🔥 Active (IT-01)
- **gRPC Evaluation**: Decision made, implementation ready
- **ImGuiTestHarness**: IPC design for automated GUI testing
- **Cross-Platform Setup**: Ensuring vcpkg compatibility (Windows/macOS/Linux)

### 📋 Planned (AW-04)
- **Policy Evaluation Framework**: YAML-based rule engine
- **Change Constraints**: Byte limits, bank restrictions, protected regions
- **Review Requirements**: Human approval thresholds

## Quick Start

### Building z3ed
```bash
cd /Users/scawful/Code/yaze
cmake --build build --target z3ed -j8
```

### Running Commands
```bash
# List all proposals (shows in-memory + disk proposals)
./build/bin/z3ed agent list

# Create a proposal from AI prompt (with sandbox)
./build/bin/z3ed agent run "Fix palette corruption in overworld tile $1234" --sandbox

# Future: Test GUI operations
./build/bin/z3ed agent test "Open ROM and navigate to Overworld Editor"
```

### Reviewing Proposals in GUI
1. Launch YAZE: `./build/bin/yaze.app/Contents/MacOS/yaze`
2. Open ROM: `File → Open ROM`
3. Open drawer: `Debug → Agent Proposals` (or `Cmd+Shift+P`)
4. Select proposal → Review diff/log → Click `Accept` or `Reject`

## Key Files

### CLI Entry Points
- `src/cli/main.cc` - z3ed binary entry point
- `src/cli/command_runner.cc` - Command dispatcher
- `src/cli/handlers/agent_handler.cc` - Agent subcommand handler

### Services
- `src/cli/service/proposal_registry.{h,cc}` - Proposal tracking singleton
- `src/cli/service/rom_sandbox_manager.{h,cc}` - Isolated ROM copies
- `src/cli/service/policy_evaluator.{h,cc}` - (Planned) Policy rules engine

### GUI Integration
- `src/app/editor/system/proposal_drawer.{h,cc}` - Right-side proposal panel
- `src/app/editor/editor_manager.{h,cc}` - Integration point for drawer

### Configuration
- `.yaze/policies/agent.yaml` - (Planned) Policy rules
- `docs/api/z3ed-resources.yaml` - API catalog and examples

## Development Workflow

### Adding a New Feature
1. Update `E6-z3ed-implementation-plan.md` with task estimate
2. Create implementation branch: `git checkout -b feature/task-name`
3. Implement code following YAZE style guide
4. Update documentation in this folder
5. Test with real proposals
6. Create PR and link to implementation plan section

### Testing Changes
```bash
# Build and test CLI
cmake --build build --target z3ed -j8
./build/bin/z3ed agent list

# Build and test GUI integration
cmake --build build --target yaze -j8
./build/bin/yaze.app/Contents/MacOS/yaze

# Future: Run automated tests
cmake --build build --target yaze_test -j8
./build/bin/yaze_test --gtest_filter=ProposalRegistry*
```

## Next Steps

### Immediate (This Week)
1. **IT-01 Phase 1**: Add gRPC to vcpkg with careful cross-platform setup
2. **IT-01 Phase 2**: Implement Ping service and test on macOS
3. **Documentation**: Update this README as implementation progresses

### Short Term (Next 2 Weeks)
1. **IT-01 Complete**: Full gRPC service with Click/Type/Wait/Assert
2. **Windows Testing**: Validate vcpkg setup on Windows VM
3. **AW-04 Design**: Policy YAML schema and PolicyEvaluator API

### Long Term (Next Month)
1. **Policy Framework**: Complete AW-04 implementation
2. **CLI Testing**: Integration tests for agent workflow
3. **Production Readiness**: Error handling, logging, telemetry

## Contributing

See parent `docs/B1-contributing.md` for general contribution guidelines.

### z3ed-Specific Guidelines
- **CLI Design**: Follow existing `z3ed agent` subcommand pattern
- **Services**: Use singleton pattern with `Instance()` accessor
- **Error Handling**: Return `absl::Status` or `absl::StatusOr<T>`
- **Documentation**: Update this README and implementation plan
- **Testing**: Add test cases before merging (when test harness ready)

## Resources

- **Parent Docs**: `../` (YAZE editor documentation)
- **API Catalog**: `../api/z3ed-resources.yaml`
- **Build Guide**: `../02-build-instructions.md`
- **Platform Compatibility**: `../B2-platform-compatibility.md`

## License

Same as YAZE - See `../../LICENSE` for details.

---

**Questions?** Open an issue or discuss in #yaze-dev Discord channel.
