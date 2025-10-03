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
    - Add read-only commands for tile searches, area summaries, and teleport destinations.
    - Guarantee each tool returns both JSON and human-readable summaries for the chat renderers.
2.  **Document & Test the New Tooling**:
    - Update the main `README.md` and relevant docs to cover the new chat formatting.
    - Add regression tests (unit or golden JSON fixtures) for the new Overworld tools.

3.  **Polish the TUI Chat Experience**:
    - Tighten keyboard shortcuts, scrolling, and copy-to-clipboard behaviour.
    - Align log file output with on-screen formatting for easier debugging.
4.  **Integrate Tool Use with LLM**:
    - Modify the `AIService` to support function calling/tool use.
    - Teach the agent to call the new read-only commands to answer questions.
5.  **Land Overworld Tooling**:
    - Ship at least two Overworld inspection commands with comprehensive tests.

6.  **Build GUI Chat Widget**:
    - Create the ImGui component.
    - Ensure it shares the same backend service as the TUI.
7.  **Full Integration with Proposal System**:
    - Implement the logic for the agent to transition from conversation to proposal generation.
8.  **Expand Tool Arsenal**:
    - Continuously add new read-only commands to give the agent more capabilities to inspect the ROM.
9.  **Multi-Modal Agent**:
    - Explore the possibility of the agent generating and displaying images (e.g., a map of a dungeon room) in the chat.
10.  **Advanced Configuration**:
    - Implement environment variables for selecting AI providers and models (e.g., `YAZE_AI_PROVIDER`, `OLLAMA_MODEL`).
    - Add CLI flags for overriding the provider and model on a per-command basis.
11.  **Performance and Cost-Saving**:
    - Implement a response cache to reduce latency and API costs.
    - Add token usage tracking and reporting.

## Current Status & Next Steps (As of Oct 3, Session 2)

We have made significant progress in laying the foundation for the conversational agent.

### ✅ Completed
- **Initial `ConversationalAgentService`**: The basic service is in place.
- **TUI Chat Stub**: A functional `agent chat` command exists.
- **GUI Chat Widget Stub**: An `AgentChatWidget` is integrated into the main GUI.
- **Initial Agent "Tools"**: `resource-list` and `dungeon-list-sprites` commands are implemented.
- **Tool Use Foundation**: The `ToolDispatcher` is implemented, and the AI services are aware of the new tool call format.
 - **Tool Loop Improvements**: Conversational flow now handles multi-step tool calls with default JSON output, allowing results to feed back into the chat without recursion.
- **Structured Tool Output Rendering**: Both the TUI and GUI chat widgets now display tables and JSON payloads with friendly formatting, drastically improving readability.
- **Overworld Inspection Suite**: Added `overworld describe-map` and `overworld list-warps` commands producing text/JSON summaries for map metadata and warp points, with agent tooling hooks.

### ✅ Build Configuration Issue Resolved
The linker error is fixed. Both the CLI and GUI targets now link against `yaze_agent`, so the shared agent handlers (`HandleResourceListCommand`, `HandleDungeonListSpritesCommand`, etc.) compile once and are available to `ToolDispatcher` everywhere.

### 🚀 Next Steps
1.  **Share ROM Context with the Agent**: Inject the active GUI ROM into `ConversationalAgentService` so tool calls work even when `--rom` flags are unavailable. Analyze the `src/app/rom.cc` and `src/app/rom.h` and `src/app/editor/editor_manager.cc` files for guidance on accessing the current project/ROM.
2.  **Expand Tool Coverage**: Target Overworld navigation helpers (`overworld find-tile`, `overworld list-warps`, region summaries) and dialogue inspectors. Prioritize commands that unblock common level-design questions and emit concise table/JSON payloads.