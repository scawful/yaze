# AI-Assisted Development in YAZE

AI-assisted development in YAZE allows developers and ROM hackers to leverage AI agents for code assistance, debugging, and automation. This guide covers how to use these AI-powered features in your daily workflow.

## Overview

YAZE includes two primary AI assistance modes:

1. **Development Assistance** - Help with building, testing, and debugging yaze itself
2. **ROM Debugging Assistance** - Help debugging ROM patches, ASM code, and game state

Both modes use the same underlying AI service (Ollama, Gemini, OpenAI, or
Anthropic) and tool infrastructure, but target different workflows.

## Choosing the right agent persona
- Personas live in `.claude/agents/<agent-id>.md`; open the matching file as your system prompt before a session (available to all agents, not just Claude).
- **ai-infra-architect**: AI/agent infra, MCP/gRPC, z3ed tooling, model plumbing.
- **backend-infra-engineer**: Build/packaging/toolchains, CI reliability, release plumbing.
- **imgui-frontend-engineer**: ImGui/editor UI, renderer/backends, canvas/docking UX.
- **snes-emulator-expert**: Emulator core (CPU/APU/PPU), performance/accuracy/debugging.
- **zelda3-hacking-expert**: Gameplay/ROM logic, data formats, hacking workflows.
- **test-infrastructure-expert**: Test harnesses, CTest/gMock infra, flake/bloat triage.
- **docs-janitor**: Docs/process hygiene, onboarding, checklists.

## Prerequisites

### Build Requirements

AI-assisted features require the AI-enabled build preset:

```bash
cmake --preset mac-ai    # macOS
cmake --preset lin-ai    # Linux
cmake --preset win-ai    # Windows
```

This includes gRPC server support, the z3ed CLI tool, and all agent infrastructure.

### AI Provider Configuration

You need **at least one** AI provider configured:

#### Option 1: Local AI with Ollama (Recommended for Development)

Ollama provides free local AI models that run offline without API keys:

```bash
# Install Ollama
brew install ollama        # macOS
# Or download from https://ollama.ai for Linux/Windows

# Start the Ollama server
ollama serve

# In another terminal, pull a recommended model
ollama pull qwen2.5-coder:0.5b  # Fast, 0.5B parameter model
```

Then run z3ed with the model:

```bash
export OLLAMA_MODEL=qwen2.5-coder:0.5b
z3ed agent simple-chat --rom zelda3.sfc
```

#### Option 2: Cloud AI with Gemini (For Advanced Features)

For more capable AI with vision support (image analysis, ROM visualization):

```bash
# Get API key from https://ai.google.com/
export GEMINI_API_KEY=your_api_key_here
z3ed agent simple-chat --rom zelda3.sfc
```

#### Option 3: Cloud AI with OpenAI or Anthropic

For alternative hosted models (or if Gemini is unavailable), configure OpenAI
or Anthropic and pick a provider explicitly:

```bash
# OpenAI (https://platform.openai.com/)
export OPENAI_API_KEY=your_api_key_here
z3ed agent simple-chat --rom zelda3.sfc --ai_provider=openai --ai_model=gpt-4o-mini

# Anthropic (https://console.anthropic.com/)
export ANTHROPIC_API_KEY=your_api_key_here
z3ed agent simple-chat --rom zelda3.sfc --ai_provider=anthropic --ai_model=claude-3-5-sonnet-latest
```

### Build Verification

After configuring your build:

```bash
# Verify AI components built correctly
cmake --build --preset mac-ai --target z3ed
./build/bin/z3ed --help | grep -i agent
```

## Quick Start

### Example 1: Debug a Build Error

```bash
# You encounter a compilation error
cmake --build build
# [ERROR] src/app/gfx/snes_color.cc:45: error: 'Arena' was not declared

# Use z3ed to analyze and suggest fixes
z3ed agent simple-chat --rom zelda3.sfc
> My code has a compilation error on line 45 of snes_color.cc. Can you help?

AI: I can help! Let me examine that file...
[Reads source files automatically using FileSystemTool]

Found the issue! You're missing:
#include "app/gfx/arena.h"

The Arena class is defined in that header. Here's the fix:
[Shows code diff]
```

### Example 2: Debug ROM Crash

```bash
# Start z3ed with ROM debugging capabilities
z3ed agent debug-rom --rom=my_hack.sfc --emulator-port=50051

# You interact with the ROM
> My patch crashes when spawning enemies

AI: Let me connect to the emulator and analyze the crash...
[Emulator pauses at crash point]

I see the issue! Your code writes to $7E:A000 which is out of WRAM bounds.
The register X contains $8000 when it should be < $2000.

[Shows register state, memory contents, and suggests root cause]
```

### Example 3: Generate Tests

```bash
# Ask the agent to generate tests for your changes
z3ed agent simple-chat --rom zelda3.sfc
> I just added a function Process(int input) that returns input * 2.
> Can you write a test for it?

AI: Based on your description, here's a test:

TEST(MyClass, ProcessDoublesInput) {
  MyClass obj;
  EXPECT_EQ(obj.Process(5), 10);
  EXPECT_EQ(obj.Process(0), 0);
  EXPECT_EQ(obj.Process(-3), -6);
}
```

## Mode 1: Development Assistance

Use AI assistance while developing yaze itself.

### Build Error Resolution

The agent automatically analyzes compilation failures:

```bash
z3ed agent simple-chat --rom zelda3.sfc
> cmake --build build failed with:
>   error: 'gfx::Arena' has not been declared in snes_color.cc:45

# AI will:
# 1. Search for the Arena class definition
# 2. Check your include statements
# 3. Suggest the missing header
# 4. Show the exact code change needed
```

### Test Automation

Generate tests or run existing tests through the agent:

```bash
z3ed agent simple-chat --rom zelda3.sfc
> Run the stable test suite and tell me if anything failed

# AI will:
# 1. Run ctest with appropriate filters
# 2. Parse test results
# 3. Report pass/fail status
# 4. Analyze any failures
```

### Crash Analysis

Get help understanding segmentation faults and assertions:

```bash
z3ed agent simple-chat --rom zelda3.sfc
> My program crashed with segfault in graphics_arena.cc:234
> [Paste stack trace]

# AI will:
# 1. Read the relevant source files
# 2. Analyze the call chain
# 3. Identify likely root causes
# 4. Suggest memory access issues or uninitialized variables
```

### Performance Analysis

Identify performance regressions:

```bash
z3ed agent simple-chat --rom zelda3.sfc
> My tile rendering is 3x slower than before. What changed?

# AI will:
# 1. Search for recent changes to tile rendering code
# 2. Identify performance-sensitive operations
# 3. Suggest optimizations (loop unrolling, caching, etc.)
```

## Mode 2: ROM Debugging Assistance

Use AI assistance while debugging ROM patches and modifications.

### ASM Patch Analysis

Get explanations of what your assembly code does:

```bash
z3ed agent debug-rom --rom=my_hack.sfc
> What does this routine do?
> [LDA #$01]
> [JSL $0A9000]

# AI will:
# 1. Decode each instruction
# 2. Explain register effects
# 3. Describe what the routine accomplishes
# 4. Identify potential issues (stack imbalance, etc.)
```

### Memory State Analysis

Understand memory corruption:

```bash
z3ed agent debug-rom --rom=my_hack.sfc
> My sprite data is corrupted at $7E:7000. Help me debug.

# AI will:
# 1. Read memory from the emulator
# 2. Compare against known structures
# 3. Trace what modified this address (via watchpoints)
# 4. Identify the cause and suggest fixes
```

### Breakpoint Analysis

Analyze game state at breakpoints:

```bash
z3ed agent debug-rom --rom=my_hack.sfc
> [Breakpoint hit at $0A:8234]
> Can you explain what's happening?

# AI will:
# 1. Disassemble the current instruction
# 2. Show register/memory state
# 3. Display the call stack
# 4. Explain the code's purpose
```

### Routine Reverse Engineering

Document undocumented game routines:

```bash
z3ed agent debug-rom --rom=my_hack.sfc
> Trace through this routine and document what it does
> [Set breakpoint at $0A:8000, trace until return]

# AI will:
# 1. Step through instructions
# 2. Document register usage
# 3. Map memory accesses to structures
# 4. Generate routine documentation
```

## Configuration Options

### Environment Variables

```bash
# Use specific AI model (Ollama)
export OLLAMA_MODEL=qwen2.5-coder:0.5b

# Use Gemini/OpenAI/Anthropic instead
export GEMINI_API_KEY=your_key_here
export OPENAI_API_KEY=your_key_here
export ANTHROPIC_API_KEY=your_key_here

```

### Command-Line Flags

Most z3ed agent commands support these options:

```bash
# Logging and debugging
z3ed agent simple-chat --log-file agent.log --debug

# ROM and workspace configuration
z3ed agent simple-chat --rom zelda3.sfc --sandbox

# Model selection (Ollama)
z3ed agent simple-chat --ai_model qwen2.5-coder:1b

# Force a cloud provider
z3ed agent simple-chat --ai_provider openai --ai_model gpt-4o-mini

# Emulator debugging (ROM Debug Mode)
z3ed agent debug-rom --emulator-port 50051
```

### Configuration File (Planned)

The CLI currently reads flags and environment variables only. For persistent
settings, use a shell profile or wrapper script until a TOML config loader
lands.

## Troubleshooting

### Problem: Agent chat hangs after prompt

**Cause**: AI provider not running or configured

**Solution**:
```bash
# Check Ollama is running
ollama serve &

# Or verify a cloud API key
echo $GEMINI_API_KEY    # Gemini
echo $OPENAI_API_KEY    # OpenAI
echo $ANTHROPIC_API_KEY # Anthropic

# Specify model explicitly
z3ed agent simple-chat --ai_model qwen2.5-coder:0.5b --rom zelda3.sfc
```

### Problem: z3ed command not found

**Cause**: Using wrong build preset or build directory

**Solution**:
```bash
# Use AI-enabled preset
cmake --preset mac-ai
cmake --build --preset mac-ai --target z3ed

# Try the full path
./build/bin/z3ed --help
```

### Problem: FileSystemTool can't read my source files

**Cause**: Path outside project directory or binary file

**Solution**:
```bash
# Always use paths relative to project root
z3ed agent simple-chat
> [Give paths like src/app/rom.cc, not /Users/name/Code/yaze/src/...]

# For binary files, ask for analysis instead
> Can you explain what the graphics in assets/graphics.bin contains?
```

### Problem: Emulator won't connect in ROM Debug Mode

**Cause**: GUI test harness not enabled or wrong port

**Solution**:
```bash
# Enable test harness in GUI
./build/bin/yaze --rom_file zelda3.sfc --enable_test_harness

# Use correct port (default 50051)
z3ed agent debug-rom --rom my_hack.sfc --emulator-port 50051
```

### Problem: Out of memory errors during large batch operations

**Cause**: Processing too much data at once

**Solution**:
```bash
# Use smaller batches
z3ed agent simple-chat --max_batch_size 100

# Process one ROM at a time
z3ed agent simple-chat --rom hack1.sfc
# ... finish ...
z3ed agent simple-chat --rom hack2.sfc
```

## Advanced Topics

### Integration with CI/CD

Use AI assistance in GitHub Actions:

```yaml
name: AI-Assisted Build Check
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup build
        run: |
          cmake --preset lin-ai
          cmake --build --preset lin-ai --target yaze z3ed
      - name: Analyze build
        run: |
          z3ed agent simple-chat --ci-mode \
            --prompt "Check if build succeeded and suggest fixes"
```

### Batch Processing Multiple ROMs

Process multiple ROM hacks automatically:

```bash
#!/bin/bash
for rom in hacks/*.sfc; do
  z3ed agent simple-chat --rom "$rom" \
    --prompt "Run tests and report status"
done
```

### Custom Tool Integration

Extend z3ed with your own tools:

```bash
# Call custom analysis tools
z3ed agent simple-chat --rom zelda3.sfc
> Can you run my custom analysis tool on this ROM?
> [Describe your tool]

# AI will integrate with the tool dispatcher
```

## Related Documentation

- **Build Guide**: [Build & Test Quick Reference](../build/quick-reference.md)
- **z3ed CLI**: [z3ed CLI Guide](../usage/z3ed-cli.md)
- **Testing**: [Testing Guide](testing-guide.md)
- **Debugging**: [Debugging Guide](debugging-guide.md)
- **Technical Details**: See `docs/internal/agents/` for architecture documentation
