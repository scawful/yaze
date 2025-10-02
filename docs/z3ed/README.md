# z3ed: AI-Powered CLI for YAZE

**Status**: Active Development

## Overview

`z3ed` is a command-line interface for YAZE that enables AI-driven ROM modifications through a proposal-based workflow. It provides both human-accessible commands for developers and machine-readable APIs for LLM integration, forming the backbone of an agentic development ecosystem.

This directory contains the primary documentation for the `z3ed` system.

## Core Documentation

Start here to understand the architecture, learn how to use the commands, and see the current development status.

1.  **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - **Design & Architecture**
    *   The "source of truth" for the system's architecture, design goals, and the agentic workflow framework. Read this first to understand *why* the system is built the way it is.

2.  **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - **Technical Reference & Guides**
    *   A complete command reference, API documentation, implementation guides, and troubleshooting tips. Use this as your day-to-day manual for working with `z3ed`.

3.  **[E6-z3ed-implementation-plan.md](E6-z3ed-implementation-plan.md)** - **Roadmap & Status**
    *   The project's task backlog, roadmap, progress tracking, and a list of known issues. Check this document for current priorities and to see what's next.

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
```

See the **[Technical Reference](E6-z3ed-reference.md)** for a full command list.
