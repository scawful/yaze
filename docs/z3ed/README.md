# z3ed: AI-Powered CLI for YAZE

**Status**: Active Development | Test Harness Enhancement Phase

## Overview

`z3ed` is a command-line interface for YAZE that enables AI-driven ROM modifications through a proposal-based workflow. It provides both human-accessible commands for developers and machine-readable APIs for LLM integration, forming the backbone of an agentic development ecosystem.

**Recent Focus**: Evolving the ImGuiTestHarness from basic GUI automation into a comprehensive testing platform that serves dual purposes:
1. **AI-Driven Workflows**: Widget discovery, test introspection, and dynamic interaction learning
2. **Traditional GUI Testing**: Test recording/replay, CI/CD integration, and regression testing

**🤖 Why This Matters**: These enhancements are **critical for AI agent autonomy**. Without them, AI agents can't verify their changes worked (no test polling), discover UI elements dynamically (hardcoded names), learn from demonstrations (no recording), or debug failures (no screenshots). The test harness evolution enables **fully autonomous agents** that can execute → verify → self-correct without human intervention.

**📋 Implementation Status**: Core infrastructure complete (Phases 1-6, AW-01 to AW-04, IT-01 to IT-04). Currently in **Test Harness Enhancement Phase** (IT-05 to IT-09). See [IMPLEMENTATION_CONTINUATION.md](IMPLEMENTATION_CONTINUATION.md) for the detailed roadmap and LLM integration plans (Ollama, Gemini, Claude).

This directory contains the primary documentation for the `z3ed` system.

## Core Documentation

Start here to understand the architecture, learn how to use the commands, and see the current development status.

1.  **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - **Design & Architecture**
    *   The "source of truth" for the system's architecture, design goals, and the agentic workflow framework. Read this first to understand *why* the system is built the way it is.

2.  **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - **Technical Reference & Guides**
    *   A complete command reference, API documentation, implementation guides, and troubleshooting tips. Use this as your day-to-day manual for working with `z3ed`.

3.  **[E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)** - **Roadmap & Status**
    *   The project's task backlog, roadmap, progress tracking, and a list of known issues. Check this document for current priorities and to see what's next.

4.  **[IMPLEMENTATION_CONTINUATION.md](IMPLEMENTATION_CONTINUATION.md)** - **Current Phase & Next Steps** ⭐
    *   Detailed continuation plan for test harness enhancements (IT-05 to IT-09). Start here to resume implementation with clear task breakdowns and success criteria.

## Quick Start

### Build z3ed

```bash
# Basic build (without GUI automation support)
cmake --build build --target z3ed

# Build with gRPC support (for GUI automation)
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON
cmake --build build-grpc-test --target z3ed
```

### Common Commands

```bash
# Create an agent proposal in a safe sandbox
z3ed agent run --prompt "Make all soldier armor red" --rom=zelda3.sfc --sandbox

# List all active and past proposals
z3ed agent list

# View the changes for the latest proposal
z3ed agent diff

# Run an automated GUI test (requires test harness to be running)
z3ed agent test --prompt "Open the Overworld editor and verify it loads"

# Discover available GUI widgets for AI interaction
z3ed agent gui discover --window "Overworld" --type button

# Record a test session for regression testing
z3ed agent test record start --output tests/overworld_load.json
# ... perform actions ...
z3ed agent test record stop

# Replay recorded test
z3ed agent test replay tests/overworld_load.json

# Query test execution status
z3ed agent test status --test-id grpc_click_12345678 --follow
```

See the **[Technical Reference](E6-z3ed-reference.md)** for a full command list.

## Recent Enhancements

**Latest Progress (Oct 2, 2025)**
- ✅ Implemented server-side wiring for `GetTestStatus`, `ListTests`, and `GetTestResults` RPCs, including execution history tracking inside `TestManager`.
- ✅ Added gRPC status mapping helper to surface accurate error codes back to clients.
- ⚠️ Pending CLI integration, end-to-end introspection tests, and documentation updates for new commands.

**Test Harness Evolution** (In Progress: IT-05 to IT-09):
- **Test Introspection**: Query test status, results, and execution history
- **Widget Discovery**: AI agents can enumerate available GUI interactions dynamically
- **Test Recording**: Capture manual workflows as JSON scripts for regression testing
- **Enhanced Debugging**: Screenshot capture, widget state dumps, execution context on failures
- **CI/CD Integration**: Standardized test suite format with JUnit XML output

See **[E6-z3ed-cli-design.md § 9](E6-z3ed-cli-design.md#9-test-harness-evolution-from-automation-to-platform)** for detailed architecture and implementation roadmap.

## Quick Navigation

**📖 Getting Started**:
- **New to z3ed?** Start with this [README.md](README.md) then [E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)
- **Want to use z3ed?** See [QUICK_REFERENCE.md](QUICK_REFERENCE.md) for all commands
- **Resume implementation?** Read [IMPLEMENTATION_CONTINUATION.md](IMPLEMENTATION_CONTINUATION.md)

**🔧 Implementation Guides**:
- [IT-05-IMPLEMENTATION-GUIDE.md](IT-05-IMPLEMENTATION-GUIDE.md) - Test Introspection API (next priority)
- [STATUS_REPORT_OCT2.md](STATUS_REPORT_OCT2.md) - Complete progress summary

**📚 Reference**:
- [E6-z3ed-reference.md](E6-z3ed-reference.md) - Technical reference and API docs
- [E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md) - Task backlog and roadmap
