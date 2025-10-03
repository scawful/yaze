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

# Full build with AI agent and testing suite
cmake -B build-grpc-test -DYAZE_WITH_GRPC=ON -DYAZE_WITH_JSON=ON
cmake --build build-grpc-test --target z3ed
```

**Dependencies for Full Build**:
- gRPC (GUI automation)
- nlohmann/json (AI service communication)
- OpenSSL (optional, for Gemini HTTPS - auto-detected on macOS/Linux)

### AI Agent Commands

```bash
# Generate commands from natural language prompt
z3ed agent plan --prompt "Place a tree at position 10, 10 on map 0"

# Execute in sandbox with auto-approval
z3ed agent run --prompt "Create a 3x3 water pond at 15, 20" --rom zelda3.sfc --sandbox

# List all proposals
z3ed agent list

# View proposal details
z3ed agent diff --proposal <id>
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

## Core Documentation

### Essential Reads
1. **[E6-z3ed-cli-design.md](E6-z3ed-cli-design.md)** - Architecture, design philosophy, agentic workflow framework
2. **[E6-z3ed-reference.md](E6-z3ed-reference.md)** - Complete command reference and API documentation
3. **[AGENTIC-PLAN-STATUS.md](AGENTIC-PLAN-STATUS.md)** - Current implementation status and roadmap

### Quick References
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - Condensed command cheatsheet
- **[QUICK-START-GEMINI.md](QUICK-START-GEMINI.md)** - Gemini API setup and testing guide
- **[OVERWORLD-DUNGEON-AI-PLAN.md](OVERWORLD-DUNGEON-AI-PLAN.md)** - Tile16 editing strategy and ResourceLabels integration

### Implementation Guides
- **[LLM-INTEGRATION-PLAN.md](LLM-INTEGRATION-PLAN.md)** - LLM integration roadmap (Ollama, Gemini, Claude)
- **[LLM-IMPLEMENTATION-CHECKLIST.md](LLM-IMPLEMENTATION-CHECKLIST.md)** - Step-by-step implementation tasks
- **[IT-05-IMPLEMENTATION-GUIDE.md](IT-05-IMPLEMENTATION-GUIDE.md)** - Test introspection API (complete ✅)
- **[IT-08-IMPLEMENTATION-GUIDE.md](IT-08-IMPLEMENTATION-GUIDE.md)** - Enhanced error reporting (complete ✅)

## Current Status (October 2025)

### ✅ Complete
- **CLI Infrastructure**: Command parsing, handlers, TUI components
- **Proposal System**: Sandbox creation, diff generation, accept/reject workflow
- **AI Services**: Ollama integration, Gemini integration, PromptBuilder
- **GUI Automation**: Widget discovery, test recording/replay, gRPC harness
- **Test Introspection**: Status polling, results query, execution history
- **Error Reporting**: Screenshots, failure context, widget state dumps

### 🔄 In Progress
- **Tile16 Editing Workflow**: Accept/reject for overworld canvas edits
- **ResourceLabels Integration**: User-defined names for AI context
- **Dungeon Editing Support**: Object/sprite placement via AI

### 📋 Planned
- **Visual Diff Generation**: Before/after screenshots for proposals
- **Batch Operations**: Multiple tile16 changes in single proposal
- **Pattern Library**: Pre-defined tile patterns (rivers, forests, etc.)
- **Claude Integration**: Anthropic API support

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

### Documentation Consolidation
- ✅ Removed 10 outdated/redundant documents
- ✅ Consolidated status into AGENTIC-PLAN-STATUS.md
- ✅ Updated README with clear dependency requirements
- ✅ Added Windows compatibility notes

## Troubleshooting

### "OpenSSL not found" warning
**Impact**: Gemini API won't work (HTTPS required)  
**Solutions**:
- Use Ollama instead (no SSL needed, runs locally)
- Install OpenSSL: `brew install openssl` (macOS) or `apt-get install libssl-dev` (Linux)
- Windows: Build without gRPC/JSON, use Ollama

### "gRPC not available" error
**Impact**: GUI testing and automation disabled  
**Solution**: Rebuild with `-DYAZE_WITH_GRPC=ON`

### AI generates invalid commands
**Causes**: Vague prompt, unfamiliar tile IDs, missing context  
**Solutions**:
- Use specific coordinates and tile types
- Reference tile16 IDs from documentation
- Provide map context ("Light World", "map 0")
- Check ResourceLabels are loaded for your project

## Contributing

### Adding AI Prompt Examples
Edit `src/cli/service/prompt_builder.cc` → `LoadDefaultExamples()`
- Add practical, multi-step examples
- Include explanation of tile IDs and reasoning
- Test with both Ollama and Gemini

### Adding CLI Commands
1. Create handler in `src/cli/handlers/<category>.cc`
2. Register in command dispatcher
3. Add to `E6-z3ed-reference.md` documentation
4. Add example prompt to `prompt_builder.cc`

### Testing
```bash
# Run unit tests
cd build-grpc-test && ctest --output-on-failure

# Test AI integration
./bin/z3ed agent plan --prompt "test prompt" --verbose
```

---

**Getting Help**:
- Read [E6-z3ed-cli-design.md](E6-z3ed-cli-design.md) for architecture
- Check [AGENTIC-PLAN-STATUS.md](AGENTIC-PLAN-STATUS.md) for current status
- Review [QUICK-START-GEMINI.md](QUICK-START-GEMINI.md) for AI setup

**Quick Test** (verifies AI is working):
```bash
export GEMINI_API_KEY="your-key"  # or start ollama serve
./build-grpc-test/bin/z3ed agent plan --prompt "Place a tree at 10, 10"
```
