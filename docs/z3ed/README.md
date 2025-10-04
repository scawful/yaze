# z3ed: AI-Powered CLI for YAZE

**Status**: Active Development | AI Integration Phase  
**Latest Update**: October 3, 2025

## Overview

`z3ed` is a command-line interface for YAZE that enables AI-driven ROM modifications through a proposal-based workflow. It provides both human-accessible commands for developers and machine-readable APIs for LLM integration.

**Core Capabilities**:
1. **AI-Driven Editing**: Natural language prompts → ROM modifications (overworld tile16, dungeon objects, sprites, palettes)
2. **GUI Test Automation**: Widget discovery, test recording/replay, introspection for debugging
3. **Proposal System**: Safe sandbox editing with accept/reject workflow
4. **Multiple AI Backends**: Ollama (local), Gemini (cloud), Claude (planned)

## Quick Start

### Build Options

```bash
# Basic z3ed (CLI only, no AI/testing features)
cmake --build build --target z3ed

# Full build with AI agent (RECOMMENDED - uses consolidated flag)
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed

# Full build with AI agent AND testing suite
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
cmake --build build --target z3ed
```

**Build Flags Explained**:
- `Z3ED_AI=ON` - **Master flag** for AI features (enables JSON, YAML, httplib for Ollama + Gemini)
- `YAZE_WITH_GRPC=ON` - Optional GUI automation and test harness (also enables JSON)
- `YAZE_WITH_JSON=ON` - Lower-level flag (auto-enabled by Z3ED_AI or GRPC)

**Dependencies for AI Features** (auto-managed by Z3ED_AI):
- nlohmann/json (JSON parsing for AI responses)
- yaml-cpp (Config file loading)
- httplib (HTTP/HTTPS API calls)
- OpenSSL (optional, for Gemini HTTPS - auto-detected on macOS/Linux)

### AI Agent Commands

```bash
# Generate commands from natural language prompt
z3ed agent plan --prompt "Place a tree at position 10, 10 on map 0"

# Execute in sandbox with auto-approval
z3ed agent run --prompt "Create a 3x3 water pond at 15, 20" --rom zelda3.sfc --sandbox

# Chat with the agent in the terminal (FTXUI prototype)
z3ed agent chat

# List all proposals
z3ed agent list

# View proposal details
z3ed agent diff --proposal <id>

# Inspect project metadata for the LLM toolchain
z3ed agent resource-list --type dungeon --format json

# Dump sprite placements for a dungeon room
z3ed agent dungeon-list-sprites --room 0x012

# Search overworld maps for a tile ID using shared agent tooling
z3ed agent overworld-find-tile --tile 0x02E --map 0x05
```

### GUI Testing Commands

```bash
# Run automated test
z3ed agent test --prompt "Open Overworld editor and verify it loads"

# Query test status
z3ed agent test status --test-id <id> --follow

# Record manual workflow
z3ed agent test record start --output tests/my_test.json
# ... perform actions in GUI ...
z3ed agent test record stop

# Replay recorded test
z3ed agent test replay tests/my_test.json

# Test conversational agent (batch mode, no TUI required)
z3ed agent test-conversation

# Test with custom conversation file
z3ed agent test-conversation --file my_tests.json
```

## AI Service Setup

### Ollama (Local LLM - Recommended for Development)

```bash
# Install Ollama
brew install ollama  # macOS
# or download from https://ollama.com

# Pull recommended model
ollama pull qwen2.5-coder:7b

# Start server
ollama serve

# z3ed will auto-detect Ollama at localhost:11434
z3ed agent plan --prompt "test"
```

### Gemini (Google Cloud API)

```bash
# Get API key from https://aistudio.google.com/apikey
export GEMINI_API_KEY="your-key-here"

# z3ed will auto-select Gemini when key is set
z3ed agent plan --prompt "test"
```

**Note**: Gemini requires OpenSSL (HTTPS). Build with `-DYAZE_WITH_GRPC=ON -DYAZE_WITH_JSON=ON` to enable SSL support. OpenSSL is auto-detected on macOS/Linux. Windows users can use Ollama instead.

### Example Prompts
Here are some example prompts you can try with either Ollama or Gemini:

**Overworld Tile16 Editing**:
- `"Place a tree at position 10, 20 on map 0"`
- `"Create a 3x3 water pond at coordinates 15, 10"`
- `"Add a dirt path from position 5,5 to 5,15"`
- `"Plant a row of trees horizontally at y=8 from x=20 to x=25"`

**Dungeon Editing (Label-Aware)**:
- `"Add 3 soldiers to the Eastern Palace entrance room"`
- `"Place a chest in Hyrule Castle treasure room"`

## Core Documentation

### Essential Reads
1. **[AGENT-ROADMAP.md](AGENT-ROADMAP.md)** - The primary source of truth for the AI agent's strategic vision, architecture, and next steps.
2. **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - Detailed architecture and design philosophy.
3. **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - Complete command reference and API documentation.

## Current Status (October 3, 2025)

The project is currently focused on implementing a conversational AI agent. See [AGENT-ROADMAP.md](AGENT-ROADMAP.md) for a detailed breakdown of what's complete, in progress, and planned.

### ✅ Completed
- **Conversational Agent Service**: ✅ Multi-step tool execution loop operational
- **TUI Chat Interface**: ✅ Production-ready with table/JSON rendering (`z3ed agent chat`)
- **Batch Testing Mode**: ✅ New `test-conversation` command for automated testing without TUI
- **Tool Dispatcher**: ✅ 5 read-only tools for ROM introspection
  - `resource-list`: Labeled resource enumeration
  - `dungeon-list-sprites`: Sprite inspection in dungeon rooms
  - `overworld-find-tile`: Tile16 search across overworld maps
  - `overworld-describe-map`: Comprehensive map metadata
  - `overworld-list-warps`: Entrance/exit/hole enumeration
- **AI Service Backends**: ✅ Ollama (local) and Gemini (cloud) operational
- **Enhanced Prompting**: ✅ Resource catalogue loading with system instruction generation
- **LLM Function Calling**: ✅ Complete - Tool schemas injected into system prompts, response parsing implemented
- **ImGui Test Harness**: ✅ gRPC service for GUI automation integrated and verified

### 🔄 In Progress (Priority Order)
1. **Live LLM Testing**: Ready for execution with new batch testing mode (use `./scripts/test_agent_conversation_live.sh`)
2. **GUI Chat Widget**: Not yet started - TUI exists, GUI integration pending (6-8h)
3. **Tool Coverage Expansion**: 5 tools working, 8+ planned (dialogue, sprites, regions) (8-10h)

### 📋 Next Steps (See AGENT-ROADMAP.md for details)
1. **Live LLM Testing** (1-2h): Verify function calling with real Ollama/Gemini
2. **Implement GUI Chat Widget** (6-8h): Create ImGui widget matching TUI experience
3. **Expand Tool Coverage** (8-10h): Add dialogue search, sprite info, region queries
4. **Performance Optimizations** (4-6h): Response caching, token tracking, streaming

### 📋 Future Plans
- **Dungeon Editing Support**: Object/sprite placement via AI (after tool foundation complete)
- **Visual Diff Generation**: Before/after screenshots for proposals
- **Multi-Modal Agent**: Image generation for dungeon room maps

## AI Editing Focus Areas

z3ed is optimized for practical ROM editing workflows:

### Overworld Tile16 Editing ⭐ PRIMARY FOCUS
**Why**: Simple data model (uint16 IDs), visual feedback, reversible, safe
- Single tile placement (trees, rocks, bushes)
- Area creation (water ponds, dirt patches)
- Path creation (connecting points with tiles)
- Pattern generation (tree rows, forests, boundaries)

### Dungeon Editing
- Sprite placement with label awareness ("eastern palace entrance")
- Object placement (chests, doors, switches)
- Entrance configuration
- Room property editing

### Palette Editing
- Color modification by index
- Sprite palette adjustments
- Export/import workflows

### Additional Capabilities
- Sprite data editing
- Compression/decompression
- ROM validation
- Patch application

## Example Workflows

### Basic Tile16 Edit
```bash
# AI generates command
z3ed agent plan --prompt "Place a tree at 10, 10"
# Output: overworld set-tile --map 0 --x 10 --y 10 --tile 0x02E

# Execute manually
z3ed overworld set-tile --map 0 --x 10 --y 10 --tile 0x02E

# Or auto-execute with sandbox
z3ed agent run --prompt "Place a tree at 10, 10" --rom zelda3.sfc --sandbox
```

### Complex Multi-Step Edit
```bash
# AI generates multiple commands
z3ed agent plan --prompt "Create a 3x3 water pond at 15, 20"

# Review proposal
z3ed agent diff --latest

# Accept and apply
z3ed agent accept --latest
```

### Locate Existing Tiles
```bash
# Find every instance of tile 0x02E across the overworld
z3ed overworld find-tile --tile 0x02E --format json

# Narrow search to Light World map 0x05
z3ed overworld find-tile --tile 0x02E --map 0x05

# Ask the agent to perform the same lookup (returns JSON by default)
z3ed agent overworld-find-tile --tile 0x02E --map 0x05
```

### Label-Aware Dungeon Edit
```bash
# AI uses ResourceLabels from your project
z3ed agent plan --prompt "Add 3 soldiers to my custom fortress entrance"
# AI explains: "Using label 'custom_fortress' for dungeon 0x04"
```

## Dependencies Guard

AI agent features require:
- `YAZE_WITH_GRPC=ON` - GUI automation and test harness
- `YAZE_WITH_JSON=ON` - AI service communication
- OpenSSL (optional) - Gemini HTTPS support (auto-detected)

**Windows Compatibility**: Build without gRPC/JSON for basic z3ed functionality. Use Ollama (localhost) instead of Gemini for AI features without SSL dependency.

## Recent Changes (Oct 3, 2025)

### SSL/HTTPS Support
- ✅ OpenSSL now optional (guarded by YAZE_WITH_GRPC + YAZE_WITH_JSON)
- ✅ Graceful degradation when OpenSSL not found (Ollama still works)
- ✅ Windows builds work without SSL dependencies

### Prompt Engineering
- ✅ Refocused examples on tile16 editing workflows
- ✅ Added dungeon editing with label awareness
- ✅ Inline tile16 reference for AI knowledge
- ✅ Practical multi-step examples (water ponds, paths, patterns)

### Conversational Loop
- ✅ Tool dispatcher defaults to JSON when invoked by the agent, keeping outputs machine-readable.
- ✅ Conversational service now replays tool results without recursion, improving chat stability.
- ✅ Mock AI service issues sample tool calls so the loop can be exercised without a live LLM.

## Troubleshooting

### "OpenSSL not found" warning
**Impact**: Gemini API won't work (HTTPS required)  
**Solutions**:
- Use Ollama instead (no SSL needed, runs locally) - **RECOMMENDED**
- Install OpenSSL: `brew install openssl` (macOS) or `apt-get install libssl-dev` (Linux)
- Windows: Use Ollama (localhost) instead of Gemini

### "Build with -DZ3ED_AI=ON" warning
**Impact**: AI agent features disabled (no Ollama or Gemini)  
**Solution**: Rebuild with AI support:
```bash
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed
```

### "gRPC not available" error
**Impact**: GUI testing and automation disabled  
**Solution**: Rebuild with `-DYAZE_WITH_GRPC=ON` (also requires Z3ED_AI)

### AI generates invalid commands
**Causes**: Vague prompt, unfamiliar tile IDs, missing context  
**Solutions**:
- Use specific coordinates and tile types
- Reference tile16 IDs from documentation
- Provide map context ("Light World", "map 0")
- Check ResourceLabels are loaded for your project

### Testing the conversational agent
**Problem**: TUI chat requires interactive input  
**Solution**: Use the new batch testing mode:
```bash
# Run with default test cases (no interaction required)
z3ed agent test-conversation --rom zelda3.sfc

# Or use the automated test script
./scripts/test_agent_conversation_live.sh
```

### Verifying ImGui test harness
**Problem**: Unsure if GUI automation is working  
**Solution**: Run the verification script:
```bash
./scripts/test_imgui_harness.sh
```

#### Gemini-Specific Issues
- **"Cannot reach Gemini API"**: Check your internet connection, API key, and that you've built with SSL support.
- **"Invalid Gemini API key"**: Regenerate your key at `aistudio.google.com/apikey`.
