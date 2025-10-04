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
1.  **Expand Overworld Tool Coverage**:
    - ✅ Ship read-only tile searches (`overworld find-tile`) with shared formatting for CLI and agent calls.
    - Next: add area summaries, teleport destination lookups, and keep JSON/Text parity for all new tools.
2.  **Polish the TUI Chat Experience**:
    - Tighten keyboard shortcuts, scrolling, and copy-to-clipboard behaviour.
    - Align log file output with on-screen formatting for easier debugging.
3.  **Integrate Tool Use with LLM**:
    - Modify the `AIService` to support function calling/tool use.
    - Teach the agent to call the new read-only commands to answer questions.
4.  **Document & Test the New Tooling**:
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

### 🚧 In Progress
- **GUI Chat Widget**: ⚠️ **NOT YET IMPLEMENTED**
  - No `AgentChatWidget` found in `src/app/gui/` directory
  - TUI implementation exists but GUI integration is pending
  - **Action Required**: Create `src/app/gui/debug/agent_chat_widget.{h,cc}`
- **LLM Function Calling**: ⚠️ **PARTIALLY IMPLEMENTED**
  - ToolDispatcher exists and is used by ConversationalAgentService
  - AI services (Ollama, Gemini) parse tool calls from responses
  - **Gap**: LLM prompt needs explicit tool schema definitions for function calling
  - **Action Required**: Add tool definitions to system prompts (see Next Steps)

### 🚀 Next Steps (Priority Order)

#### Priority 1: Complete LLM Function Calling Integration ✅ COMPLETE (Oct 3, 2025)
**Goal**: Enable Ollama/Gemini to autonomously invoke read-only tools

**Completed Tasks:**
1. ✅ **Tool Schema Generation** - Added `BuildFunctionCallSchemas()` method
   - Generates OpenAI-compatible function calling schemas from tool specifications
   - Properly formats parameters with types, descriptions, and examples
   - Marks required vs optional arguments
   - **File**: `src/cli/service/ai/prompt_builder.{h,cc}`

2. ✅ **System Prompt Enhancement** - Injected tool definitions
   - Updated `BuildConstraintsSection()` to include tool schemas
   - Added tool usage guidance (tools for questions, commands for modifications)
   - Included example tool call in JSON format
   - **File**: `src/cli/service/ai/prompt_builder.cc`

3. ✅ **LLM Response Parsing** - Already implemented
   - Both `OllamaAIService` and `GeminiAIService` parse `tool_calls` from JSON
   - Populate `AgentResponse.tool_calls` with parsed ToolCall objects
   - **Files**: `src/cli/service/ai/{ollama,gemini}_ai_service.cc`

4. ✅ **Infrastructure Verification** - Created test scripts
   - `scripts/test_tool_schemas.sh` - Verifies tool definitions in catalogue
   - `scripts/test_agent_mock.sh` - Validates component integration
   - All 5 tools properly defined with arguments and examples
   - **Status**: Ready for live LLM testing

**What's Working:**
- ✅ Tool definitions loaded from `assets/agent/prompt_catalogue.yaml`
- ✅ Function schemas generated in OpenAI format
- ✅ System prompts include tool definitions with usage guidance
- ✅ AI services parse tool_calls from LLM responses
- ✅ ConversationalAgentService dispatches tools via ToolDispatcher
- ✅ Tools return JSON results that feed back into conversation

**Next Step: Live LLM Testing** (1-2 hours)
- Test with Ollama: Verify qwen2.5-coder can discover and invoke tools
- Test with Gemini: Verify Gemini 2.0 generates correct tool_calls
- Create example prompts that exercise all 5 tools
- Verify multi-step tool execution (agent asks follow-up questions)

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