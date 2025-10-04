# z3ed Agent Roadmap

This document outlines the strategic vision and concrete next steps for the `z3ed` AI agent, focusing on a transition from a command-line tool to a fully interactive, conversational assistant for ROM hacking.

## Core Vision: The Conversational ROM Hacking Assistant

The next evolution of the `z3ed` agent is to create a chat-like interface where users can interact with the AI in a more natural and exploratory way. Instead of just issuing a single command, users will be able to have a dialogue with the agent to inspect the ROM, ask questions, and iteratively build up a set of changes.

This vision will be realized through a shared interface available in both the `z3ed` TUI and the main `yaze` GUI application.

### Key Features
1.  **Interactive Chat Interface**: A familiar chat window for conversing with the agent.
2.  **ROM Introspection**: The agent will be able to answer questions about the ROM, such as "What dungeons are defined in this project?" or "How many soldiers are in the Hyrule Castle throne room?".
3.  **Contextual Awareness**: The agent will maintain the context of the conversation, allowing for follow-up questions and commands.
4.  **Seamless Transition to Action**: When the user is ready to make a change, the agent will use the conversation history to generate a comprehensive proposal for editing the ROM.
5.  **Shared Experience**: The same conversational agent will be accessible from both the terminal and the graphical user interface, providing a consistent experience.

## Technical Implementation Plan

### 1. Conversational Agent Service
- **Description**: A new service that will manage the back-and-forth between the user and the LLM. It will maintain chat history and orchestrate the agent's different modes (Q&A vs. command generation).
- **Components**:
    - `ConversationalAgentService`: The main class for managing the chat session.
    - Integration with existing `AIService` implementations (Ollama, Gemini).
- **Status**: In progress — baseline service exists with chat history, tool loop handling, and structured response parsing. Next up: wiring in live ROM context and richer session state.

### 2. Read-Only "Tools" for the Agent
- **Description**: To enable the agent to answer questions, we need to expand `z3ed` with a suite of read-only commands that the LLM can call. This is aligned with the "tool use" or "function calling" capabilities of modern LLMs.
- **Example Tools to Implement**:
    - `resource list --type <dungeon|sprite|...>`: List all user-defined labels of a certain type.
    - `dungeon list-sprites --room <id|label>`: List all sprites in a given room.
    - `dungeon get-info --room <id|label>`: Get metadata for a specific room.
    - `overworld find-tile --tile <id>`: Find all occurrences of a specific tile on the overworld map.
- **Advanced Editing Tools (for future implementation)**:
    - `overworld set-area --map <id> --x <x> --y <y> --width <w> --height <h> --tile <id>`
    - `overworld replace-tile --map <id> --from <old_id> --to <new_id>`
    - `overworld blend-tiles --map <id> --pattern <name> --density <percent>`
- **Status**: Foundational commands (`resource-list`, `dungeon-list-sprites`) are live with JSON output. Focus is shifting to high-value Overworld and dialogue inspection tools.

### 3. TUI and GUI Chat Interfaces
- **Description**: User-facing components for interacting with the `ConversationalAgentService`.
- **Components**:
    - **TUI**: A new full-screen component in `z3ed` using FTXUI, providing a rich chat experience in the terminal.
    - **GUI**: A new ImGui widget that can be docked into the main `yaze` application window.
- **Status**: In progress — CLI/TUI and GUI chat widgets exist, now rendering tables/JSON with readable formatting. Need to improve input ergonomics and synchronized history navigation.

### 4. Integration with the Proposal Workflow
- **Description**: The final step is to connect the conversation to the action. When a user's prompt implies a desire to modify the ROM (e.g., "Okay, now add two more soldiers"), the `ConversationalAgentService` will trigger the existing `Tile16ProposalGenerator` (and future proposal generators for other resource types) to create a proposal.
- **Workflow**:
    1. User chats with the agent to explore the ROM.
    2. User asks the agent to make a change.
    3. `ConversationalAgentService` generates the commands and passes them to the appropriate `ProposalGenerator`.
    4. A new proposal is created and saved.
    5. The TUI/GUI notifies the user that a proposal is ready for review.
    6. User uses the `agent diff` and `agent accept` commands (or UI equivalents) to review and apply the changes.
- **Status**: The proposal workflow itself is mostly implemented. This task involves integrating it with the new conversational service.

## Next Steps

### Immediate Priorities
1.  **✅ Build System Consolidation** (COMPLETE - Oct 3, 2025):
    - ✅ Created Z3ED_AI master flag for simplified builds
    - ✅ Fixed Gemini crash with graceful degradation
    - ✅ Updated documentation with new build instructions
    - ✅ Tested both Ollama and Gemini backends
    - **Next**: Update CI/CD workflows to use `-DZ3ED_AI=ON`
2.  **Live LLM Testing** (NEXT UP - 1-2 hours):
    - Verify function calling works with real Ollama/Gemini
    - Test multi-step tool execution
    - Validate all 5 tools with natural language prompts
3.  **Expand Overworld Tool Coverage**:
    - ✅ Ship read-only tile searches (`overworld find-tile`) with shared formatting for CLI and agent calls.
    - Next: add area summaries, teleport destination lookups, and keep JSON/Text parity for all new tools.
4.  **Polish the TUI Chat Experience**:
    - Tighten keyboard shortcuts, scrolling, and copy-to-clipboard behaviour.
    - Align log file output with on-screen formatting for easier debugging.
5.  **Document & Test the New Tooling**:
    - Update the main `README.md` and relevant docs to cover the new chat formatting.
    - Add regression tests (unit or golden JSON fixtures) for the new Overworld tools.
5.  **Build GUI Chat Widget**:
    - Create the ImGui component.
    - Ensure it shares the same backend service as the TUI.
6.  **Full Integration with Proposal System**:
    - Implement the logic for the agent to transition from conversation to proposal generation.
7.  **Expand Tool Arsenal**:
    - Continuously add new read-only commands to give the agent more capabilities to inspect the ROM.
8.  **Multi-Modal Agent**:
    - Explore the possibility of the agent generating and displaying images (e.g., a map of a dungeon room) in the chat.
9.  **Advanced Configuration**:
    - Implement environment variables for selecting AI providers and models (e.g., `YAZE_AI_PROVIDER`, `OLLAMA_MODEL`).
    - Add CLI flags for overriding the provider and model on a per-command basis.
10.  **Performance and Cost-Saving**:
    - Implement a response cache to reduce latency and API costs.
    - Add token usage tracking and reporting.

## Current Status & Next Steps (Updated: October 3, 2025)

We have made significant progress in laying the foundation for the conversational agent.

### ✅ Completed
- **Build System Consolidation**: ✅ **NEW** Z3ED_AI master flag (Oct 3, 2025)
  - Single flag enables all AI features: `-DZ3ED_AI=ON`
  - Auto-manages dependencies (JSON, YAML, httplib, OpenSSL)
  - Fixed Gemini crash when API key set but JSON disabled
  - Graceful degradation with clear error messages
  - Backward compatible with old flags
  - Ready for build modularization (enables optional `libyaze_agent.a`)
  - **Docs**: `docs/z3ed/Z3ED_AI_FLAG_MIGRATION.md`
- **`ConversationalAgentService`**: ✅ Fully operational with multi-step tool execution loop
  - Handles tool calls with automatic JSON output format
  - Prevents recursion through proper tool result replay
  - Supports conversation history and context management
- **TUI Chat Interface**: ✅ Production-ready (`z3ed agent chat`)
  - Renders tables from JSON tool results
  - Pretty-prints JSON payloads with syntax formatting
  - Scrollable history with user/agent distinction
- **Tool Dispatcher**: ✅ Complete with 5 read-only tools
  - `resource-list`: Enumerate labeled resources (dungeons, sprites, palettes)
  - `dungeon-list-sprites`: Inspect sprites in dungeon rooms
  - `overworld-find-tile`: Search for tile16 IDs across maps
  - `overworld-describe-map`: Get comprehensive map metadata
  - `overworld-list-warps`: List entrances/exits/holes with filtering
- **Structured Output Rendering**: ✅ Both TUI formats support tables and JSON
  - Automatic table generation from JSON arrays/objects
  - Column-aligned formatting with headers
  - Graceful fallback to text for malformed data
- **ROM Context Integration**: ✅ Tools can access loaded ROM or load from `--rom` flag
  - Shared ROM context passed through ConversationalAgentService
  - Automatic ROM loading with error handling
- **AI Service Foundation**: ✅ Ollama and Gemini services operational
  - Enhanced prompting system with resource catalogue loading
  - System instruction generation with examples
  - Health checks and model availability validation
  - Both backends tested and working in production

### 🚧 In Progress
- **Live LLM Testing**: Ready to execute with real Ollama/Gemini
  - All infrastructure complete (function calling, tool schemas, response parsing)
  - Need to verify multi-step tool execution with live models
  - Test scenarios prepared for all 5 tools
  - **Estimated Time**: 1-2 hours
- **GUI Chat Widget**: Not yet started
  - TUI implementation complete and can serve as reference
  - Should reuse table/JSON rendering logic from TUI
  - Target: `src/app/gui/debug/agent_chat_widget.{h,cc}`
  - **Estimated Time**: 6-8 hours

### 🚀 Next Steps (Priority Order)

#### Priority 1: Live LLM Testing with Function Calling (1-2 hours)
**Goal**: Verify Ollama/Gemini can autonomously invoke tools in production

**Infrastructure Complete** ✅:
- ✅ Tool schema generation (`BuildFunctionCallSchemas()`)
- ✅ System prompts include function definitions
- ✅ AI services parse `tool_calls` from responses
- ✅ ConversationalAgentService dispatches to ToolDispatcher
- ✅ All 5 tools tested independently

**Testing Tasks**:
1. **Gemini Testing** (30 min)
   - Verify Gemini 2.0 generates correct `tool_calls` JSON
   - Test prompt: "What dungeons are in this ROM?"
   - Verify tool result fed back into conversation
   - Test multi-step: "Now list sprites in the first dungeon"

2. **Ollama Testing** (30 min)
   - Verify qwen2.5-coder discovers and calls tools
   - Same test prompts as Gemini
   - Compare response quality between models

3. **Tool Coverage Testing** (30 min)
   - Exercise all 5 tools with natural language prompts
   - Verify JSON output formats correctly
   - Test error handling (invalid room IDs, etc.)

**Success Criteria**:
- LLM autonomously calls tools without explicit command syntax
- Tool results incorporated into follow-up responses
- Multi-turn conversations work with context

#### Priority 2: Implement GUI Chat Widget (6-8 hours)
**Goal**: Unified chat experience in YAZE application

1. **Create ImGui Chat Widget** (4 hours)
   - File: `src/app/gui/debug/agent_chat_widget.{h,cc}`
   - Reuse table/JSON rendering logic from TUI implementation
   - Add to Debug menu: `Debug → Agent Chat`
   - Share `ConversationalAgentService` instance with TUI

2. **Add Chat History Persistence** (2 hours)
   - Save chat history to `.yaze/agent_chat_history.json`
   - Load on startup, display in GUI/TUI
   - Add "Clear History" button

3. **Polish Input Experience** (2 hours)
   - Multi-line input support (Shift+Enter for newline, Enter to send)
   - Keyboard shortcuts: Ctrl+L to clear, Ctrl+C to copy last response
   - Auto-scroll to bottom on new messages

#### Priority 3: Expand Tool Coverage (8-10 hours)
**Goal**: Enable deeper ROM introspection for level design questions

1. **Dialogue/Text Tools** (3 hours)
   - `dialogue-search --text "search term"`: Find text in ROM dialogue
   - `dialogue-get --id 0x...`: Get dialogue by message ID

2. **Sprite Tools** (3 hours)
   - `sprite-get-info --id 0x...`: Sprite metadata (HP, damage, AI)
   - `overworld-list-sprites --map 0x...`: Sprites on overworld map

3. **Advanced Overworld Tools** (4 hours)
   - `overworld-get-region --map 0x...`: Region boundaries and properties
   - `overworld-list-transitions --from-map 0x...`: Map transitions/scrolling
   - `overworld-get-tile-at --map 0x... --x N --y N`: Get specific tile16 value

#### Priority 4: Performance and Caching (4-6 hours)

1. **Response Caching** (3 hours)
   - Implement LRU cache for identical prompts
   - Cache tool results by (tool_name, args) key
   - Configurable TTL (default: 5 minutes for ROM introspection)

2. **Token Usage Tracking** (2 hours)
   - Log tokens per request (Ollama and Gemini APIs provide this)
   - Display in chat footer: "Last response: 1234 tokens, ~$0.02"
   - Add `--show-token-usage` flag to CLI commands

3. **Streaming Responses** (optional, 3-4 hours)
   - Use Ollama/Gemini streaming APIs
   - Update GUI/TUI to show partial responses as they arrive
   - Improves perceived latency for long responses

## z3ed Build Quick Reference

```bash
# Full AI features (Ollama + Gemini)
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed

# AI + GUI automation/testing
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
cmake --build build --target z3ed

# Minimal build (no AI)
cmake -B build
cmake --build build --target z3ed
```

## Build Flags Explained

| Flag | Purpose | Dependencies | When to Use |
|------|---------|--------------|-------------|
| `Z3ED_AI=ON` | **Master flag** for AI features | JSON, YAML, httplib, (OpenSSL*) | Want Ollama or Gemini support |
| `YAZE_WITH_GRPC=ON` | GUI automation & testing | gRPC, Protobuf, (auto-enables JSON) | Want GUI test harness |
| `YAZE_WITH_JSON=ON` | Low-level JSON support | nlohmann_json | Auto-enabled by above flags |

*OpenSSL optional - required for Gemini (HTTPS), Ollama works without it

## Feature Matrix

| Feature | No Flags | Z3ED_AI | Z3ED_AI + GRPC |
|---------|----------|---------|----------------|
| Basic CLI | ✅ | ✅ | ✅ |
| Ollama (local) | ❌ | ✅ | ✅ |
| Gemini (cloud) | ❌ | ✅* | ✅* |
| TUI Chat | ❌ | ✅ | ✅ |
| GUI Test Automation | ❌ | ❌ | ✅ |
| Tool Dispatcher | ❌ | ✅ | ✅ |
| Function Calling | ❌ | ✅ | ✅ |

*Requires OpenSSL for HTTPS

## Common Build Scenarios

### Developer (AI features, no GUI testing)
```bash
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed -j8
```

### Full Stack (AI + GUI automation)
```bash
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
cmake --build build --target z3ed -j8
```

### CI/CD (minimal, fast)
```bash
cmake -B build -DYAZE_MINIMAL_BUILD=ON
cmake --build build -j$(nproc)
```

### Release Build (optimized)
```bash
cmake -B build -DZ3ED_AI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target z3ed -j8
```

## Migration from Old Flags

### Before (Confusing)
```bash
cmake -B build -DYAZE_WITH_GRPC=ON -DYAZE_WITH_JSON=ON
```

### After (Clear Intent)
```bash
cmake -B build -DZ3ED_AI=ON -DYAZE_WITH_GRPC=ON
```

**Note**: Old flags still work for backward compatibility!

## Troubleshooting

### "Build with -DZ3ED_AI=ON" warning
**Symptom**: AI commands fail with "JSON support required"  
**Fix**: Rebuild with AI flag
```bash
rm -rf build && cmake -B build -DZ3ED_AI=ON && cmake --build build
```

### "OpenSSL not found" warning
**Symptom**: Gemini API doesn't work  
**Impact**: Only affects Gemini (cloud). Ollama (local) works fine  
**Fix (optional)**:
```bash
# macOS
brew install openssl

# Linux
sudo apt install libssl-dev

# Then rebuild
cmake -B build -DZ3ED_AI=ON && cmake --build build
```

### Ollama vs Gemini not auto-detecting
**Symptom**: Wrong backend selected  
**Fix**: Set explicit provider
```bash
# Force Ollama
export YAZE_AI_PROVIDER=ollama
./build/bin/z3ed agent plan --prompt "test"

# Force Gemini
export YAZE_AI_PROVIDER=gemini
export GEMINI_API_KEY="your-key"
./build/bin/z3ed agent plan --prompt "test"
```

## Environment Variables

| Variable | Default | Purpose |
|----------|---------|---------|
| `YAZE_AI_PROVIDER` | auto | Force `ollama` or `gemini` |
| `GEMINI_API_KEY` | - | Gemini API key (enables Gemini) |
| `OLLAMA_MODEL` | `qwen2.5-coder:7b` | Override Ollama model |
| `GEMINI_MODEL` | `gemini-2.5-flash` | Override Gemini model |

## Platform-Specific Notes

### macOS
- OpenSSL auto-detected via Homebrew
- Keychain integration for SSL certs
- Recommended: `brew install openssl ollama`

### Linux
- OpenSSL typically pre-installed
- Install via: `sudo apt install libssl-dev`
- Ollama: Download from https://ollama.com

### Windows
- Use Ollama (no SSL required)
- Gemini requires OpenSSL (harder to setup on Windows)
- Recommend: Focus on Ollama for Windows builds

## Performance Tips

### Faster Incremental Builds
```bash
# Use Ninja instead of Make
cmake -B build -GNinja -DZ3ED_AI=ON
ninja -C build z3ed

# Enable ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake -B build -DZ3ED_AI=ON
```

### Reduce Build Scope
```bash
# Only build z3ed (not full yaze app)
cmake --build build --target z3ed

# Parallel build
cmake --build build --target z3ed -j$(nproc)
```

## Related Documentation

- **Migration Guide**: [Z3ED_AI_FLAG_MIGRATION.md](Z3ED_AI_FLAG_MIGRATION.md)
- **Technical Roadmap**: [AGENT-ROADMAP.md](AGENT-ROADMAP.md)
- **Main README**: [README.md](README.md)
- **Build Modularization**: `../../build_modularization_plan.md`

## Quick Test

Verify your build works:

```bash
# Check z3ed runs
./build/bin/z3ed --version

# Test AI detection
./build/bin/z3ed agent plan --prompt "test" 2>&1 | head -5

# Expected output (with Z3ED_AI=ON):
# 🤖 Using Gemini AI with model: gemini-2.5-flash
# or
# 🤖 Using Ollama AI with model: qwen2.5-coder:7b
# or
# 🤖 Using MockAIService (no LLM configured)
```

## Support

If you encounter issues:
1. Check this guide's troubleshooting section
2. Review [Z3ED_AI_FLAG_MIGRATION.md](Z3ED_AI_FLAG_MIGRATION.md)
3. Verify CMake output for warnings
4. Open an issue with build logs

## Summary

**Recommended for most users**:
```bash
cmake -B build -DZ3ED_AI=ON
cmake --build build --target z3ed -j8
./build/bin/z3ed agent chat
```

This gives you:
- ✅ Ollama support (local, free)
- ✅ Gemini support (cloud, API key required)
- ✅ TUI chat interface
- ✅ Tool dispatcher with 5 commands
- ✅ Function calling support
- ✅ All AI agent features
