# C3 - z3ed Agent Architecture Guide

**Date**: October 12, 2025  
**Version**: v0.2.2-alpha  
**Status**: Core Features Integrated 

## Overview

This guide documents the architecture of the z3ed AI agent system, including learned knowledge, TODO management, advanced routing, pretraining, and agent handoff capabilities.

## Architecture Overview

```
┌───────────────────────────────────────────────────────────────┐
│              User / AI Agent                                   │
└────────────┬──────────────────────────────────────────────────┘
             │
             │ z3ed CLI commands
             │
┌────────────▼──────────────────────────────────────────────────┐
│         CLI Command Router (agent.cc)                          │
│                                                                │
│  Routes to:                                                    │
│  ├─ agent simple-chat    → SimpleChatCommand                  │
│  ├─ agent learn          → HandleLearnCommand                 │
│  ├─ agent todo           → HandleTodoCommand                  │
│  ├─ agent test           → HandleTestCommand                  │
│  ├─ agent plan/run/diff  → Proposal system                    │
│  └─ emulator-*           → EmulatorCommandHandler             │
└───────────┬───────────────────────────────────────────────────┘
            │
┌───────────▼───────────────────────────────────────────────────┐
│      ConversationalAgentService                                │
│                                                                │
│  Integrates:                                                   │
│  ├─ LearnedKnowledgeService  (preferences, patterns, memory)  │
│  ├─ TodoManager              (task tracking, dependencies)    │
│  ├─ AdvancedRouter           (response enhancement)           │
│  ├─ AgentPretraining         (knowledge injection)            │
│  └─ ToolDispatcher           (command execution)              │
└────────────┬──────────────────────────────────────────────────┘
             │
┌────────────▼──────────────────────────────────────────────────┐
│         Tool Dispatcher                                        │
│                                                                │
│  Routes tool calls to:                                         │
│  ├─ Resource Commands   (dungeon, overworld, sprites)         │
│  ├─ Emulator Commands   (breakpoints, memory, step)           │
│  ├─ GUI Commands        (automation, screenshots)             │
│  └─ Custom Tools        (extensible via CommandHandler)       │
└────────────┬──────────────────────────────────────────────────┘
             │
┌────────────▼──────────────────────────────────────────────────┐
│      Command Handlers (CommandHandler base class)              │
│                                                                │
│  Unified pattern:                                              │
│  1. Parse arguments (ArgumentParser)                           │
│  2. Get ROM context (CommandContext)                           │
│  3. Execute business logic                                     │
│  4. Format output (OutputFormatter)                            │
└────────────┬──────────────────────────────────────────────────┘
             │
┌────────────▼──────────────────────────────────────────────────┐
│      Persistent Storage                                        │
│                                                                │
│  ~/.yaze/agent/                                                │
│  ├─ preferences.json     (user preferences)                    │
│  ├─ patterns.json        (learned ROM patterns)                │
│  ├─ projects.json        (project contexts)                    │
│  ├─ memories.json        (conversation summaries)              │
│  ├─ todos.json           (task management)                     │
│  └─ sessions/            (collaborative chat history)          │
└────────────────────────────────────────────────────────────────┘
```

## Feature 1: Learned Knowledge Service

### What It Does

Persists information across agent sessions:
- **Preferences**: User's default settings (palette, tool choices)
- **ROM Patterns**: Learned behaviors (frequently accessed rooms, sprite patterns)
- **Project Context**: ROM-specific goals and notes
- **Conversation Memory**: Summaries of past discussions for continuity

### Integration Status:  Complete

**Files**:
- `cli/service/agent/learned_knowledge_service.{h,cc}` - Core service
- `cli/handlers/agent/general_commands.cc` - CLI handlers
- `cli/handlers/agent.cc` - Routing

### Usage Examples

```bash
# Save preference
z3ed agent learn --preference default_palette=2

# Get preference
z3ed agent learn --get-preference default_palette

# Save project context
z3ed agent learn --project "myrom" --context "Vanilla+ difficulty hack"

# Get project details
z3ed agent learn --get-project "myrom"

# Search past conversations
z3ed agent learn --search-memories "dungeon room 5"

# Export all learned data
z3ed agent learn --export learned_data.json

# View statistics
z3ed agent learn --stats
```

### AI Agent Integration

The ConversationalAgentService now:
1. Initializes `LearnedKnowledgeService` on startup
2. Can inject learned context into prompts (when `inject_learned_context_=true`)
3. Can access preferences/patterns/memories during tool execution

**API**:
```cpp
ConversationalAgentService service;
service.learned_knowledge().SetPreference("palette", "2");
auto pref = service.learned_knowledge().GetPreference("palette");
```

### Data Persistence

**Location**: `~/.yaze/agent/`  
**Format**: JSON  
**Files**:
- `preferences.json` - Key-value pairs
- `patterns.json` - Timestamped ROM patterns with confidence scores
- `projects.json` - Project metadata and context
- `memories.json` - Conversation summaries (last 100)

### Current Integration

- `cli/service/agent/learned_knowledge_service.{h,cc}` is constructed inside `ConversationalAgentService`.
- CLI commands such as `z3ed agent learn …` and `agent recall …` exercise this API.
- JSON artifacts persist under `~/.yaze/agent/`.

## Feature 2: TODO Management System

### What It Does

Enables AI agents to break down complex tasks into executable steps with dependency tracking and prioritization.

### Current Integration

- Core service in `cli/service/agent/todo_manager.{h,cc}`.
- CLI routing in `cli/handlers/agent/todo_commands.{h,cc}` and `cli/handlers/agent.cc`.
- JSON storage at `~/.yaze/agent/todos.json`.

### Usage Examples

```bash
# Create TODO
z3ed agent todo create "Fix input handling" --category=emulator --priority=1

# List TODOs
z3ed agent todo list

# Filter by status
z3ed agent todo list --status=in_progress

# Update status
z3ed agent todo update 1 --status=completed

# Get next actionable task
z3ed agent todo next

# Generate dependency-aware execution plan
z3ed agent todo plan

# Clear completed
z3ed agent todo clear-completed
```

### AI Agent Integration

```cpp
ConversationalAgentService service;
service.todo_manager().CreateTodo("Debug A button", "emulator", 1);
auto next = service.todo_manager().GetNextActionableTodo();
```

### Storage

**Location**: `~/.yaze/agent/todos.json`  
**Format**: JSON array with dependencies:
```json
{
  "todos": [
    {
      "id": "1",
      "description": "Debug input handling",
      "status": "in_progress",
      "category": "emulator",
      "priority": 1,
      "dependencies": [],
      "tools_needed": ["emulator-set-breakpoint", "emulator-read-memory"]
    }
  ]
}
```

## Feature 3: Advanced Routing

### What It Does

Optimizes tool responses for AI consumption with:
- **Data type inference** (sprite data vs tile data vs palette)
- **Pattern extraction** (repeating values, structures)
- **Structured summaries** (high-level + detailed + next steps)
- **GUI action generation** (converts analysis → automation script)

### Status

- Implementation lives in `cli/service/agent/advanced_routing.{h,cc}` and is compiled via `cli/agent.cmake`.
- Hook-ups to `ToolDispatcher` / `ConversationalAgentService` remain on the backlog.

### How to Integrate

**Option 1: In ToolDispatcher (Automatic)**
```cpp
// In tool_dispatcher.cc, after tool execution:
auto result = handler->Run(args, rom_context_);
if (result.ok()) {
  std::string output = output_buffer.str();
  
  // Route through advanced router for enhanced response
  AdvancedRouter::RouteContext ctx;
  ctx.rom = rom_context_;
  ctx.tool_calls_made = {call.tool_name};
  
  if (call.tool_name == "hex-read") {
    auto routed = AdvancedRouter::RouteHexAnalysis(data, address, ctx);
    return absl::StrCat(routed.summary, "\n\n", routed.detailed_data);
  }
  
  return output;
}
```

**Option 2: In ConversationalAgentService (Selective)**
```cpp
// After getting tool results, enhance the response:
ChatMessage ConversationalAgentService::EnhanceResponse(
    const ChatMessage& response, 
    const std::string& user_message) {
  
  AdvancedRouter::RouteContext ctx;
  ctx.rom = rom_context_;
  ctx.user_intent = user_message;
  
  // Use advanced router to synthesize multi-tool responses
  auto routed = AdvancedRouter::SynthesizeMultiToolResponse(
      tool_results_, ctx);
  
  ChatMessage enhanced = response;
  enhanced.message = routed.summary;
  // Attach routed.gui_actions as metadata
  
  return enhanced;
}
```

## Feature 4: Agent Pretraining

### What It Does

Injects structured knowledge into the agent's first message to teach it about:
- ROM structure (memory map, data formats)
- Hex analysis patterns (how to recognize sprites, tiles, palettes)
- Map editing workflows (tile placement, warp creation)
- Tool usage best practices

### Status

- Pretraining scaffolding (`cli/service/agent/agent_pretraining.{h,cc}`) builds today.
- The one-time injection step in `ConversationalAgentService` is still disabled.

### How to Integrate

**In ConversationalAgentService::SendMessage()**:
```cpp
absl::StatusOr<ChatMessage> ConversationalAgentService::SendMessage(
    const std::string& message) {
  
  // One-time pretraining injection on first message
  if (inject_pretraining_ && !pretraining_injected_ && rom_context_) {
    std::string pretraining = AgentPretraining::GeneratePretrainingPrompt(rom_context_);
    
    ChatMessage pretraining_msg;
    pretraining_msg.sender = ChatMessage::Sender::kUser;
    pretraining_msg.message = pretraining;
    pretraining_msg.is_internal = true;  // Don't show to user
    
    history_.insert(history_.begin(), pretraining_msg);
    pretraining_injected_ = true;
  }
  
  // Continue with normal message processing...
}
```

### Knowledge Modules

```cpp
auto modules = AgentPretraining::GetModules();
for (const auto& module : modules) {
  std::cout << "Module: " << module.name << std::endl;
  std::cout << "Required: " << (module.required ? "Yes" : "No") << std::endl;
  std::cout << module.content << std::endl;
}
```

Modules include:
- `rom_structure` - Memory map, data formats
- `hex_analysis` - Pattern recognition for sprites/tiles/palettes
- `map_editing` - Overworld/dungeon editing workflows
- `tool_usage` - Best practices for tool calling

## Feature 5: Agent Handoff

Handoff covers CLI ↔ GUI transfers, specialised agent delegation, and human/AI ownership changes. The proposed `HandoffContext` structure (see code listing earlier) captures conversation history, ROM state, TODOs, and transient tool data. Serialization, cross-surface loading, and persona-specific workflows remain unimplemented.

## Current Integration Snapshot

Integrated components:
- Learned knowledge service (`cli/service/agent/learned_knowledge_service.{h,cc}`) with CLI commands and JSON persistence under `~/.yaze/agent/`.
- TODO manager (`cli/service/agent/todo_manager.{h,cc}` plus CLI handlers) with storage at `~/.yaze/agent/todos.json`.
- Emulator debugging gRPC service; 20 of 24 methods are implemented (see `E9-ai-agent-debugging-guide.md`).

Pending integration:
- Advanced router (`cli/service/agent/advanced_routing.{h,cc}`) needs wiring into `ToolDispatcher` or `ConversationalAgentService`.
- Agent pretraining (`cli/service/agent/agent_pretraining.{h,cc}`) needs the one-time injection path enabled.
- Handoff serialization and import/export tooling are still design-only.

## References

- **Main CLI Guide**: C1-z3ed-agent-guide.md
- **Debugging Guide**: E9-ai-agent-debugging-guide.md
- **Changelog**: H1-changelog.md (v0.2.2 section)
- **Learned Knowledge**: `cli/service/agent/learned_knowledge_service.{h,cc}`
- **TODO Manager**: `cli/service/agent/todo_manager.{h,cc}`
- **Advanced Routing**: `cli/service/agent/advanced_routing.{h,cc}`
- **Pretraining**: `cli/service/agent/agent_pretraining.{h,cc}`
- **Agent Service**: `cli/service/agent/conversational_agent_service.{h,cc}`

---

**Last Updated**: October 12, 2025  
**In progress**: Context injection for pretraining, advanced routing integration, agent handoff implementation.
