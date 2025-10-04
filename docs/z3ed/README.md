# z3ed: AI-Powered CLI for YAZE

**Status**: Production Ready (AI Integration)  
**Latest Update**: October 3, 2025

## Overview

`z3ed` is a command-line interface for YAZE enabling AI-driven ROM modifications through a conversational interface. It provides natural language interaction for ROM inspection and editing with a safe proposal-based workflow.

**Core Capabilities**:
1. **Conversational Agent**: Chat with AI to explore ROM contents and plan changes
2. **GUI Test Automation**: Widget discovery, recording/replay, introspection
3. **Proposal System**: Sandbox editing with review workflow
4. **Multiple AI Backends**: Ollama (local), Gemini (cloud)

## Quick Start

### Build
```bash
# Full AI features (RECOMMENDED)
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed

# With GUI automation
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
cmake --build build --target z3ed
```

### AI Setup

**Ollama (Recommended for Development)**:
```bash
brew install ollama              # macOS
ollama pull qwen2.5-coder:7b    # Pull model
ollama serve                     # Start server
```

**Gemini (Cloud API)**:
```bash
export GEMINI_API_KEY="your-key-here"
# Get key from https://aistudio.google.com/apikey
```

### Example Commands

**Conversational Agent**:
```bash
# Interactive chat (FTXUI)
z3ed agent chat --rom zelda3.sfc

# Simple text mode (better for AI/automation)
z3ed agent simple-chat --rom zelda3.sfc

# Batch mode
z3ed agent simple-chat --file queries.txt --rom zelda3.sfc
```

**Direct Tool Usage**:
```bash
# List dungeons
z3ed agent resource-list --type dungeon --format json

# Find tiles
z3ed agent overworld-find-tile --tile 0x02E --map 0x05

# Inspect sprites
z3ed agent dungeon-list-sprites --room 0x012
```

**Proposal Workflow**:
```bash
# Generate from prompt
z3ed agent run --prompt "Place tree at 10,10" --rom zelda3.sfc --sandbox

# List proposals
z3ed agent list

# Review
z3ed agent diff --proposal-id <id>

# Accept
z3ed agent accept --proposal-id <id>
```

## Chat Modes

### 1. FTXUI Chat (`agent chat`)
Full-screen interactive terminal with:
- Table rendering for JSON results
- Syntax highlighting
- Scrollable history
- Best for manual exploration

### 2. Simple Chat (`agent simple-chat`)
Text-based REPL without FTXUI:
- Lightweight, no dependencies
- Scriptable and automatable
- Batch mode support
- Better for AI agent testing
- Commands: `quit`, `exit`, `reset`

### 3. GUI Chat Widget (In Progress)
ImGui widget in YAZE editor:
- Same backend as CLI
- Dockable interface
- History persistence
- Visual proposal review

## Available Tools

The agent can call these tools autonomously:

| Tool | Purpose | Example |
|------|---------|---------|
| `resource-list` | List labeled resources | "What dungeons exist?" |
| `dungeon-list-sprites` | Sprites in room | "Show soldiers in room 0x12" |
| `overworld-find-tile` | Find tile locations | "Where is tile 0x2E used?" |
| `overworld-describe-map` | Map metadata | "Describe map 0x05" |
| `overworld-list-warps` | List entrances/exits | "Show all cave entrances" |

## Documentation

- **[AGENT-ROADMAP.md](AGENT-ROADMAP.md)** - Vision, priorities, and technical architecture
- **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - CLI design and command structure
- **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - Complete command reference

## Recent Updates (Oct 3, 2025)

### ✅ Implemented
- **Simple Chat Mode**: Text-based REPL for automation
- **GUI Widget Fixes**: Corrected API usage, table rendering
- **Condensed Documentation**: Streamlined README and ROADMAP
- **Z3ED_AI Flag**: Simplified build with single master flag

### 🎯 Next Steps
1. **Live LLM Testing** (1-2h): Verify function calling works
2. **GUI Integration** (4-6h): Wire chat widget into main app
3. **Proposal Integration** (6-8h): Connect chat to ROM modification

## Troubleshooting

### "AI features not available"
**Solution**: Rebuild with `-DZ3ED_AI=ON`

### "OpenSSL not found"
**Impact**: Gemini won't work  
**Solutions**:
- Use Ollama (no SSL needed)
- Install OpenSSL: `brew install openssl`

### Chat mode freezes
**Solution**: Use `agent simple-chat` instead of `agent chat`

### Tool not being called
**Cause**: Model doesn't support function calling  
**Solution**: Use qwen2.5-coder (Ollama) or Gemini 2.0

## Example Workflows

### Explore ROM
```bash
$ z3ed agent simple-chat --rom zelda3.sfc
You: What dungeons are defined?
Agent: <calls resource-list --type dungeon>
  ID    Label                   
  ----  ------------------------
  0x00  eastern_palace          
  0x01  desert_palace           
  ...

You: Show me sprites in the first dungeon room 0x012
Agent: <calls dungeon-list-sprites --room 0x012>
  ...
```

### Make Changes
```bash
$ z3ed agent run --prompt "Add a tree at position 10,10 on map 0" --sandbox
Proposal created: abc123

$ z3ed agent diff --proposal-id abc123
Commands:
  overworld set-tile --map 0 --x 10 --y 10 --tile 0x02E

$ z3ed agent accept --proposal-id abc123
✅ Proposal accepted
```

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
1. **[BUILD_QUICK_REFERENCE.md](BUILD_QUICK_REFERENCE.md)** - **NEW!** Fast build guide with Z3ED_AI flag examples
2. **[AGENT-ROADMAP.md](AGENT-ROADMAP.md)** - The primary source of truth for the AI agent's strategic vision, architecture, and next steps
3. **[Z3ED_AI_FLAG_MIGRATION.md](Z3ED_AI_FLAG_MIGRATION.md)** - **NEW!** Complete guide to Z3ED_AI flag and crash fixes
4. **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - Detailed architecture and design philosophy
5. **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - Complete command reference and API documentation

## Current Status (October 3, 2025)

### ✅ Production Ready
- **Build System**: ✅ Z3ED_AI flag consolidation complete
  - Single flag for all AI features
  - Graceful degradation when dependencies missing
  - Clear error messages and build status
  - Backward compatible with old flags
- **AI Backends**: ✅ Both Ollama and Gemini operational
  - Auto-detection based on environment
  - Health checks and error handling
  - Tested with real API calls
- **Conversational Agent**: ✅ Multi-step tool execution loop
  - Chat history management
  - Tool result replay without recursion
  - JSON/table rendering in TUI
- **Tool Dispatcher**: ✅ 5 read-only tools operational
  - Resource listing, sprite inspection, tile search
  - Map descriptions, warp enumeration
  - Machine-readable JSON output

### � In Progress (Priority Order)
1. **Live LLM Testing** (1-2h): Verify function calling with real models
2. **GUI Chat Widget** (6-8h): ImGui integration (TUI exists as reference)
3. **Tool Coverage Expansion** (8-10h): Dialogue, sprites, regions

### 📋 Next Steps
See [AGENT-ROADMAP.md](AGENT-ROADMAP.md) for detailed technical roadmap.

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

### Z3ED_AI Build Flag (Major Improvement)
- ✅ **Consolidated Build Flags**: New `-DZ3ED_AI=ON` replaces multiple flags
  - Old: `-DYAZE_WITH_GRPC=ON -DYAZE_WITH_JSON=ON`
  - New: `-DZ3ED_AI=ON` (simpler, clearer intent)
- ✅ **Fixed Gemini Crash**: Graceful degradation when dependencies missing
- ✅ **Better Error Messages**: Clear guidance on missing dependencies
- ✅ **Production Ready**: Both backends tested and operational

### Build System
- ✅ Auto-manages dependencies (JSON, YAML, httplib, OpenSSL)
- ✅ Backward compatible with old flags
- ✅ Ready for build modularization (optional `libyaze_agent.a`)

### Documentation
- ✅ Updated build instructions with Z3ED_AI flag
- ✅ Added migration guide: [Z3ED_AI_FLAG_MIGRATION.md](Z3ED_AI_FLAG_MIGRATION.md)
- ✅ Clear troubleshooting section with common issues

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
