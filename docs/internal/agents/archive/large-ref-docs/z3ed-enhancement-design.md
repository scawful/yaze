# Z3ED CLI & Agent API Enhancement Design

## Executive Summary

This document outlines comprehensive enhancements to the z3ed CLI and agent APIs to significantly improve AI agent interaction with YAZE. The design focuses on enabling better automation, testing, and feature development through a robust command interface, programmatic editor access, and enhanced collaboration features.

## Current Architecture Analysis

### Existing Components
- **CLI Framework**: ModernCLI with CommandRegistry pattern
- **Command Handlers**: 70+ specialized handlers (hex, palette, sprite, music, dialogue, dungeon, overworld, gui, emulator)
- **Canvas Automation API**: Programmatic interface for tile operations, selection, and view control
- **Network Client**: WebSocket/HTTP fallback for collaborative editing
- **HTTP API**: REST endpoints for health, models, and basic operations
- **Model Integration**: Ollama and Gemini support through ModelRegistry

### Key Strengths
- Clean command handler abstraction with consistent execution pattern
- Canvas automation already supports tile operations and coordinate conversion
- Network infrastructure in place for collaboration
- Extensible model registry for multiple AI providers

### Gaps to Address
- Limited ROM direct manipulation commands
- No session persistence or REPL mode
- Minimal test generation capabilities
- Limited agent coordination features
- No batch operation support for complex workflows
- Missing introspection and discovery APIs

## 1. Z3ED CLI Enhancements

### 1.1 ROM Operations Commands

```bash
# Direct ROM manipulation
z3ed rom read --address <hex> [--length <bytes>] [--format hex|ascii|binary]
z3ed rom write --address <hex> --data <hex_string> [--verify]
z3ed rom validate [--checksums] [--headers] [--regions]
z3ed rom diff --base <rom1> --compare <rom2> [--output patch]
z3ed rom patch --input <rom> --patch <ips|bps> --output <patched_rom>
z3ed rom export --region <name> --start <hex> --end <hex> --output <file>
z3ed rom import --region <name> --address <hex> --input <file>

# ROM state management
z3ed rom snapshot --name <snapshot_name> [--compress]
z3ed rom restore --snapshot <name> [--verify]
z3ed rom list-snapshots [--details]
z3ed rom compare-snapshot --current --snapshot <name>
```

#### Implementation Details
```cpp
class RomReadCommandHandler : public CommandHandler {
protected:
  absl::Status ValidateArgs(const ArgumentParser& parser) override {
    RETURN_IF_ERROR(parser.RequireArgs({"address"}));
    if (auto len = parser.GetInt("length")) {
      if (*len <= 0 || *len > 0x10000) {
        return absl::InvalidArgumentError("Length must be 1-65536 bytes");
      }
    }
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const ArgumentParser& parser,
                      OutputFormatter& formatter) override {
    uint32_t address = parser.GetHex("address").value();
    int length = parser.GetInt("length").value_or(16);
    std::string format = parser.GetString("format").value_or("hex");

    std::vector<uint8_t> data;
    for (int i = 0; i < length; i++) {
      data.push_back(rom->ReadByte(address + i));
    }

    formatter.AddField("address", absl::StrFormat("0x%06X", address));
    formatter.AddField("data", FormatData(data, format));
    return absl::OkStatus();
  }
};
```

### 1.2 Editor Automation Commands

```bash
# Dungeon editor automation
z3ed editor dungeon place-object --room <id> --type <object_id> --x <x> --y <y>
z3ed editor dungeon remove-object --room <id> --object-index <idx>
z3ed editor dungeon set-property --room <id> --property <name> --value <val>
z3ed editor dungeon list-objects --room <id> [--filter-type <type>]
z3ed editor dungeon validate-room --room <id> [--fix-issues]

# Overworld editor automation
z3ed editor overworld set-tile --map <id> --x <x> --y <y> --tile <tile_id>
z3ed editor overworld place-entrance --map <id> --x <x> --y <y> --target <room>
z3ed editor overworld modify-sprite --map <id> --sprite-index <idx> --property <prop> --value <val>
z3ed editor overworld generate-minimap --map <id> --output <file>

# Graphics editor automation
z3ed editor graphics import-sheet --sheet <id> --file <png> [--palette <id>]
z3ed editor graphics export-sheet --sheet <id> --output <png>
z3ed editor graphics modify-palette --palette <id> --color <idx> --rgb <#RRGGBB>

# Batch operations
z3ed editor batch --script <file> [--dry-run] [--parallel]
```

#### Batch Script Format (JSON)
```json
{
  "operations": [
    {
      "editor": "dungeon",
      "action": "place-object",
      "params": {
        "room": 1,
        "type": 0x22,
        "x": 10,
        "y": 15
      }
    },
    {
      "editor": "overworld",
      "action": "set-tile",
      "params": {
        "map": 0x00,
        "x": 20,
        "y": 30,
        "tile": 0x142
      }
    }
  ],
  "options": {
    "stop_on_error": false,
    "validate_after": true
  }
}
```

### 1.3 Testing Commands

```bash
# Test execution
z3ed test run --category <unit|integration|e2e> [--filter <pattern>]
z3ed test validate-feature --feature <name> [--rom <file>]
z3ed test generate --target <class|file> --output <test_file>
z3ed test coverage --report <html|json|text>

# Test recording
z3ed test record --name <test_name> --start
z3ed test record --stop [--save-as <file>]
z3ed test playback --file <test_file> [--speed <1-10>]

# Regression testing
z3ed test baseline --create --name <baseline>
z3ed test baseline --compare --name <baseline> [--threshold <percent>]
```

### 1.4 Build & Deploy Commands

```bash
# Build management
z3ed build --preset <preset> [--verbose] [--parallel <jobs>]
z3ed build clean [--all]
z3ed build test [--preset <preset>]
z3ed build package --platform <win|mac|linux> --output <dir>

# CI/CD integration
z3ed ci status [--workflow <name>]
z3ed ci trigger --workflow <name> [--branch <branch>]
z3ed ci logs --run-id <id> [--follow]
z3ed ci artifacts --run-id <id> --download <path>
```

### 1.5 Query & Introspection Interface

```bash
# System queries
z3ed query rom-info [--detailed]
z3ed query test-status [--failures-only]
z3ed query build-status [--preset <preset>]
z3ed query available-commands [--category <cat>] [--format tree|list|json]

# Data queries
z3ed query find-tiles --pattern <hex> [--context <bytes>]
z3ed query find-sprites --type <id> [--map <id>]
z3ed query find-text --search <string> [--case-sensitive]
z3ed query dependencies --entity <type:id>

# Statistics
z3ed query stats --type <rom|dungeon|overworld|sprites>
z3ed query usage --command <name> [--since <date>]
```

### 1.6 Interactive REPL Mode

```bash
# Start REPL
z3ed repl [--rom <file>] [--history <file>]

# REPL Features:
# - Persistent ROM state across commands
# - Command history with arrow keys
# - Tab completion for commands and parameters
# - Context-aware suggestions
# - Session recording/playback
# - Variable assignment ($var = command output)
# - Pipes and filters (command1 | command2)
```

#### REPL Implementation
```cpp
class ReplSession {
  Rom* rom_;
  std::map<std::string, json> variables_;
  std::vector<std::string> history_;

public:
  absl::Status ProcessLine(const std::string& line) {
    // Parse for variable assignment
    if (auto var_match = ParseVariableAssignment(line)) {
      auto result = ExecuteCommand(var_match->command);
      variables_[var_match->var_name] = result;
      return absl::OkStatus();
    }

    // Parse for pipes
    if (auto pipe_commands = ParsePipe(line)) {
      json previous_output;
      for (const auto& cmd : *pipe_commands) {
        auto expanded = ExpandVariables(cmd, previous_output);
        previous_output = ExecuteCommand(expanded);
      }
      return absl::OkStatus();
    }

    // Simple command
    return ExecuteCommand(line);
  }
};
```

## 2. Agent API Improvements

### 2.1 Enhanced Canvas Automation API

```cpp
namespace yaze {
namespace gui {

class EnhancedCanvasAutomationAPI : public CanvasAutomationAPI {
public:
  // Object selection by properties
  struct ObjectQuery {
    std::optional<int> type_id;
    std::optional<ImVec2> position_min;
    std::optional<ImVec2> position_max;
    std::optional<std::string> name_pattern;
    std::map<std::string, std::any> properties;
  };

  std::vector<ObjectHandle> FindObjects(const ObjectQuery& query) const;

  // Batch operations
  struct BatchOperation {
    enum Type { MOVE, MODIFY, DELETE, DUPLICATE };
    Type type;
    std::vector<ObjectHandle> objects;
    std::map<std::string, std::any> parameters;
  };

  absl::Status ExecuteBatch(const std::vector<BatchOperation>& ops);

  // Validation queries
  bool IsValidPlacement(ObjectHandle obj, ImVec2 position) const;
  std::vector<std::string> GetPlacementErrors(ObjectHandle obj, ImVec2 pos) const;

  // Event simulation
  void SimulateClick(ImVec2 position, int button = 0);
  void SimulateDrag(ImVec2 from, ImVec2 to);
  void SimulateKeyPress(ImGuiKey key, bool shift = false, bool ctrl = false);
  void SimulateContextMenu(ImVec2 position);

  // Advanced queries
  struct CanvasStatistics {
    int total_objects;
    std::map<int, int> objects_by_type;
    float canvas_coverage_percent;
    ImVec2 bounding_box_min;
    ImVec2 bounding_box_max;
  };

  CanvasStatistics GetStatistics() const;

  // Undo/Redo support
  bool CanUndo() const;
  bool CanRedo() const;
  void Undo();
  void Redo();
  std::vector<std::string> GetUndoHistory(int count = 10) const;
};

}  // namespace gui
}  // namespace yaze
```

### 2.2 Programmatic Editor Access

```cpp
namespace yaze {
namespace app {

class EditorAutomationAPI {
public:
  // Editor lifecycle
  absl::Status OpenEditor(EditorType type, const std::string& params = "");
  absl::Status CloseEditor(EditorHandle handle);
  std::vector<EditorHandle> GetOpenEditors() const;

  // State snapshots
  absl::StatusOr<EditorSnapshot> CaptureState(EditorHandle editor);
  absl::Status RestoreState(EditorHandle editor, const EditorSnapshot& snapshot);
  absl::Status CompareStates(const EditorSnapshot& s1, const EditorSnapshot& s2);

  // Query current state
  struct EditorState {
    EditorType type;
    std::string name;
    bool has_unsaved_changes;
    std::map<std::string, std::any> properties;
    std::vector<std::string> available_actions;
  };

  EditorState GetState(EditorHandle editor) const;

  // Execute operations
  absl::Status ExecuteAction(EditorHandle editor,
                             const std::string& action,
                             const json& parameters);

  // Event subscription
  using EventCallback = std::function<void(const EditorEvent&)>;

  void Subscribe(EditorHandle editor, EventType type, EventCallback cb);
  void Unsubscribe(EditorHandle editor, EventType type);

  // Validation
  absl::Status ValidateEditor(EditorHandle editor);
  std::vector<ValidationIssue> GetValidationIssues(EditorHandle editor);
};

}  // namespace app
}  // namespace yaze
```

### 2.3 Test Generation API

```cpp
namespace yaze {
namespace test {

class TestGenerationAPI {
public:
  // Record interactions
  void StartRecording(const std::string& test_name);
  void StopRecording();
  void PauseRecording();
  void ResumeRecording();

  // Generate tests from recordings
  absl::StatusOr<std::string> GenerateTestCode(
      const std::string& test_name,
      TestFramework framework = TestFramework::GTEST);

  // Generate tests from specifications
  struct TestSpecification {
    std::string class_under_test;
    std::vector<std::string> methods_to_test;
    bool include_edge_cases = true;
    bool include_error_cases = true;
    bool generate_mocks = true;
  };

  absl::StatusOr<std::string> GenerateTests(const TestSpecification& spec);

  // Test fixtures from state
  absl::StatusOr<std::string> GenerateFixture(EditorHandle editor);

  // Regression test generation
  absl::StatusOr<std::string> GenerateRegressionTest(
      const std::string& bug_description,
      const std::vector<std::string>& repro_steps);

  // Test execution
  struct TestResult {
    bool passed;
    std::string output;
    double execution_time_ms;
    std::vector<std::string> failures;
  };

  absl::StatusOr<TestResult> RunGeneratedTest(const std::string& test_code);
};

}  // namespace test
}  // namespace yaze
```

## 3. Agent UI Enhancements

### 3.1 Status Dashboard

```cpp
class AgentStatusDashboard : public Panel {
public:
  void Draw() override {
    // Real-time agent activity
    DrawAgentActivity();

    // Test execution progress
    DrawTestProgress();

    // Build/CI status
    DrawBuildStatus();

    // Recent changes
    DrawRecentChanges();

    // Performance metrics
    DrawPerformanceMetrics();
  }

private:
  struct AgentActivity {
    std::string agent_name;
    std::string current_task;
    float progress_percent;
    std::chrono::steady_clock::time_point started_at;
  };

  std::vector<AgentActivity> active_agents_;

  void DrawAgentActivity() {
    ImGui::Text("Active Agents");
    for (const auto& agent : active_agents_) {
      ImGui::ProgressBar(agent.progress_percent / 100.0f,
                        ImVec2(-1, 0),
                        agent.current_task.c_str());
    }
  }
};
```

### 3.2 Agent Control Panel

```cpp
class AgentControlPanel : public Panel {
public:
  void Draw() override {
    // Agent task management
    if (ImGui::Button("Start New Task")) {
      ShowTaskDialog();
    }

    // Active tasks
    DrawActiveTasks();

    // Agent logs
    DrawAgentLogs();

    // Manual intervention
    DrawInterventionControls();

    // Collaboration coordination
    DrawCollaborationStatus();
  }

private:
  void ShowTaskDialog() {
    ImGui::OpenPopup("New Agent Task");
    if (ImGui::BeginPopupModal("New Agent Task")) {
      static char task_name[256];
      ImGui::InputText("Task Name", task_name, sizeof(task_name));

      static int selected_agent = 0;
      ImGui::Combo("Agent", &selected_agent, available_agents_);

      if (ImGui::Button("Start")) {
        StartAgentTask(task_name, selected_agent);
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
};
```

## 4. Network/Collaboration Features

### 4.1 Multi-Agent Coordination

```cpp
namespace yaze {
namespace agent {

class MultiAgentCoordinator {
public:
  // Agent registration
  absl::Status RegisterAgent(const AgentInfo& info);
  absl::Status UnregisterAgent(const std::string& agent_id);

  // Work queue management
  absl::Status QueueTask(const Task& task);
  absl::StatusOr<Task> ClaimTask(const std::string& agent_id);
  absl::Status CompleteTask(const std::string& task_id, const TaskResult& result);

  // Shared state
  absl::Status UpdateSharedState(const std::string& key, const json& value);
  absl::StatusOr<json> GetSharedState(const std::string& key);
  absl::Status SubscribeToState(const std::string& key, StateCallback cb);

  // Conflict resolution
  enum ConflictStrategy {
    LAST_WRITE_WINS,
    MERGE,
    MANUAL_RESOLUTION,
    QUEUE_SEQUENTIAL
  };

  absl::Status SetConflictStrategy(ConflictStrategy strategy);
  absl::StatusOr<Resolution> ResolveConflict(const Conflict& conflict);

  // Agent discovery
  std::vector<AgentInfo> DiscoverAgents(const AgentQuery& query);
  absl::StatusOr<AgentCapabilities> GetCapabilities(const std::string& agent_id);
};

}  // namespace agent
}  // namespace yaze
```

### 4.2 Remote z3ed Access

```yaml
# OpenAPI 3.0 Specification
openapi: 3.0.0
info:
  title: Z3ED Remote API
  version: 1.0.0

paths:
  /api/v1/command:
    post:
      summary: Execute z3ed command
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                command:
                  type: string
                  example: "rom read --address 0x1000 --length 16"
                session_id:
                  type: string
                timeout_ms:
                  type: integer
                  default: 30000
      responses:
        200:
          description: Command executed successfully
          content:
            application/json:
              schema:
                type: object
                properties:
                  result:
                    type: object
                  execution_time_ms:
                    type: number

  /api/v1/session:
    post:
      summary: Create new z3ed session
      requestBody:
        content:
          application/json:
            schema:
              type: object
              properties:
                rom_path:
                  type: string
                persist:
                  type: boolean
                  default: false
      responses:
        200:
          description: Session created
          content:
            application/json:
              schema:
                type: object
                properties:
                  session_id:
                    type: string
                  expires_at:
                    type: string
                    format: date-time

  /api/v1/websocket:
    get:
      summary: WebSocket endpoint for real-time updates
      responses:
        101:
          description: Switching Protocols
```

### 4.3 WebSocket Protocol

```typescript
// WebSocket message types
interface Z3edWebSocketMessage {
  type: 'command' | 'event' | 'subscribe' | 'unsubscribe';
  id: string;
  payload: any;
}

// Command execution
interface CommandMessage {
  type: 'command';
  id: string;
  payload: {
    command: string;
    args: string[];
    stream: boolean;  // Stream output as it's generated
  };
}

// Event subscription
interface SubscribeMessage {
  type: 'subscribe';
  id: string;
  payload: {
    events: Array<'editor.changed' | 'test.completed' | 'build.status'>;
  };
}

// Server events
interface EventMessage {
  type: 'event';
  id: string;
  payload: {
    event: string;
    data: any;
    timestamp: string;
  };
}
```

## 5. Implementation Plan

### Phase 1: Foundation (Weeks 1-2)
1. Implement core ROM operations commands
2. Add REPL infrastructure
3. Enhance Canvas Automation API with batch operations
4. Create command discovery/introspection system

### Phase 2: Editor Integration (Weeks 3-4)
1. Implement editor automation commands
2. Add programmatic editor access API
3. Create test recording infrastructure
4. Build event subscription system

### Phase 3: Testing & CI (Weeks 5-6)
1. Implement test generation API
2. Add test execution commands
3. Create CI/CD integration commands
4. Build regression test framework

### Phase 4: Collaboration (Weeks 7-8)
1. Implement multi-agent coordinator
2. Add REST API endpoints
3. Create WebSocket real-time protocol
4. Build conflict resolution system

### Phase 5: UI & Polish (Weeks 9-10)
1. Create Agent Status Dashboard
2. Build Agent Control Panel
3. Add comprehensive documentation
4. Create example workflows

## 6. Example Workflows

### Workflow 1: Automated Dungeon Testing
```bash
# Start REPL session
z3ed repl --rom zelda3.sfc

# Record baseline
> rom snapshot --name baseline
> editor dungeon --room 0

# Start test recording
> test record --name dungeon_placement_test --start

# Perform operations
> editor dungeon place-object --room 0 --type 0x22 --x 10 --y 15
> editor dungeon place-object --room 0 --type 0x23 --x 20 --y 15
> query stats --type dungeon

# Stop and generate test
> test record --stop
> test generate --from-recording dungeon_placement_test --output test_dungeon.cc
```

### Workflow 2: Multi-Agent ROM Editing
```python
import z3ed_client

# Agent 1: Overworld specialist
agent1 = z3ed_client.Agent("overworld_agent")
agent1.connect("localhost:8080")

# Agent 2: Dungeon specialist
agent2 = z3ed_client.Agent("dungeon_agent")
agent2.connect("localhost:8080")

# Coordinator assigns tasks
coordinator = z3ed_client.Coordinator()
coordinator.queue_task({
    "type": "overworld",
    "action": "optimize_tilemap",
    "map_id": 0x00
})
coordinator.queue_task({
    "type": "dungeon",
    "action": "validate_rooms",
    "rooms": range(0, 296)
})

# Agents work in parallel
results = coordinator.wait_for_completion()
```

### Workflow 3: AI-Powered Test Generation
```bash
# Analyze class for test generation
z3ed test analyze --class OverworldEditor

# Generate comprehensive tests
z3ed test generate \
  --target OverworldEditor \
  --include-edge-cases \
  --include-mocks \
  --framework gtest \
  --output overworld_editor_test.cc

# Run generated tests
z3ed test run --file overworld_editor_test.cc --verbose

# Create regression test from bug
z3ed test regression \
  --bug "Tiles corrupt when placing entrance at map boundary" \
  --repro-steps "1. Open map 0x00" "2. Place entrance at x=511,y=511" \
  --output regression_boundary_test.cc
```

## 7. Security Considerations

### Authentication & Authorization
- API key authentication for remote access
- Role-based permissions (read-only, editor, admin)
- Session management with expiration
- Rate limiting per API key

### Input Validation
- Command injection prevention
- Path traversal protection
- Memory address validation
- File size limits for imports

### Audit Logging
- All commands logged with timestamp and user
- ROM modifications tracked
- Rollback capability for destructive operations
- Export audit trail for compliance

## 8. Performance Optimizations

### Caching
- Command result caching for repeated queries
- ROM state caching for snapshots
- Compiled test cache
- WebSocket connection pooling

### Batch Processing
- Aggregate multiple operations into transactions
- Parallel execution for independent commands
- Lazy loading for large data sets
- Progressive streaming for long operations

### Resource Management
- Connection limits per client
- Memory quotas for sessions
- CPU throttling for intensive operations
- Graceful degradation under load

## 9. Documentation Requirements

### API Reference
- Complete command reference with examples
- REST API OpenAPI specification
- WebSocket protocol documentation
- Error code reference

### Tutorials
- "Getting Started with z3ed REPL"
- "Automating ROM Testing"
- "Multi-Agent Collaboration"
- "Building Custom Commands"

### Integration Guides
- Python client library
- JavaScript/TypeScript SDK
- CI/CD integration examples
- VS Code extension

### Best Practices
- Command naming conventions
- Error handling patterns
- Performance optimization tips
- Security guidelines

## 10. Success Metrics

### Functionality
- 100% coverage of editor operations via CLI
- < 100ms command execution for simple operations
- < 1s for complex batch operations
- 99.9% API availability

### Developer Experience
- Tab completion for all commands
- Comprehensive error messages
- Interactive help system
- Example for every command

### Testing
- 90% code coverage for new components
- Automated regression tests for all commands
- Performance benchmarks for critical paths
- Integration tests for multi-agent scenarios

## Conclusion

These enhancements will transform z3ed from a basic CLI tool into a comprehensive automation platform for YAZE. The design prioritizes developer experience, AI agent capabilities, and robust testing infrastructure while maintaining backwards compatibility and performance.

The modular implementation plan allows for incremental delivery of value, with each phase providing immediately useful functionality. The foundation laid here will enable future innovations in ROM hacking automation and collaborative editing.