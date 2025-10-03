# z3ed Agent Roadmap

**Latest Update**: October 3, 2025

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
- **Status**: Not started.

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
- **Status**: Some commands exist (`overworld get-tile`), but the suite needs to be expanded.

### 3. TUI and GUI Chat Interfaces
- **Description**: User-facing components for interacting with the `ConversationalAgentService`.
- **Components**:
    - **TUI**: A new full-screen component in `z3ed` using FTXUI, providing a rich chat experience in the terminal.
    - **GUI**: A new ImGui widget that can be docked into the main `yaze` application window.
- **Status**: Not started.

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

## Consolidated Next Steps

### Immediate Priorities (Next Session)
1.  **Implement Read-Only Agent Tools**:
    - Add `resource list` command.
    - Add `dungeon list-sprites` command.
    - Ensure all new commands have JSON output options for machine readability.
2.  **Stub out `ConversationalAgentService`**:
    - Create the basic class structure.
    - Implement simple chat history management.
3.  **Update `README.md` and Consolidate Docs**:
    - Update the main `README.md` to reflect this new roadmap.
    - Remove `IMPLEMENTATION-SESSION-OCT3-CONTINUED.md`.
    - Merge any other scattered planning documents into this roadmap.

### Short-Term Goals (This Week)
1.  **Build TUI Chat Interface**:
    - Create the FTXUI component.
    - Connect it to the `ConversationalAgentService`.
    - Implement basic input/output.
2.  **Integrate Tool Use with LLM**:
    - Modify the `AIService` to support function calling/tool use.
    - Teach the agent to call the new read-only commands to answer questions.

### Long-Term Vision (Next Week and Beyond)
1.  **Build GUI Chat Widget**:
    - Create the ImGui component.
    - Ensure it shares the same backend service as the TUI.
2.  **Full Integration with Proposal System**:
    - Implement the logic for the agent to transition from conversation to proposal generation.
3.  **Expand Tool Arsenal**:
    - Continuously add new read-only commands to give the agent more capabilities to inspect the ROM.
4.  **Multi-Modal Agent**:
    - Explore the possibility of the agent generating and displaying images (e.g., a map of a dungeon room) in the chat.
5.  **Advanced Configuration**:
    - Implement environment variables for selecting AI providers and models (e.g., `YAZE_AI_PROVIDER`, `OLLAMA_MODEL`).
    - Add CLI flags for overriding the provider and model on a per-command basis.
6.  **Performance and Cost-Saving**:
    - Implement a response cache to reduce latency and API costs.
    - Add token usage tracking and reporting.
