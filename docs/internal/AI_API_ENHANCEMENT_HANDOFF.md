# AI API & Agentic Workflow Enhancement - Handoff Document

**Date**: 2025-01-XX  
**Status**: Phase 1 Complete, Phase 2-4 Pending  
**Branch**: (to be determined)

## Executive Summary

This document tracks progress on transforming Yaze into an AI-native platform with unified model management, API interface, and enhanced agentic workflows. Phase 1 (Unified Model Management) is complete. Phases 2-4 require implementation.

## Completed Work (Phase 1)

### 1. Unified AI Model Management ✅

#### Core Infrastructure
- **`ModelInfo` struct** (`src/cli/service/ai/common.h`)
  - Standardized model representation across all providers
  - Fields: `name`, `display_name`, `provider`, `description`, `family`, `parameter_size`, `quantization`, `size_bytes`, `is_local`
  
- **`ModelRegistry` class** (`src/cli/service/ai/model_registry.h/.cc`)
  - Singleton pattern for managing multiple `AIService` instances
  - `RegisterService()` - Add service instances
  - `ListAllModels()` - Aggregate models from all registered services
  - Thread-safe with mutex protection

#### AIService Interface Updates
- **`AIService::ListAvailableModels()`** - Virtual method returning `std::vector<ModelInfo>`
- **`AIService::GetProviderName()`** - Virtual method returning provider identifier
- Default implementations provided in base class

#### Provider Implementations
- **`OllamaAIService::ListAvailableModels()`**
  - Queries `/api/tags` endpoint
  - Maps Ollama's model structure to `ModelInfo`
  - Handles size, quantization, family metadata
  
- **`GeminiAIService::ListAvailableModels()`**
  - Queries Gemini API `/v1beta/models` endpoint
  - Falls back to known defaults if API key missing
  - Filters for `gemini*` models

#### UI Integration
- **`AgentChatWidget::RefreshModels()`**
  - Registers Ollama and Gemini services with `ModelRegistry`
  - Aggregates models from all providers
  - Caches results in `model_info_cache_`
  
- **Header updates** (`agent_chat_widget.h`)
  - Replaced `ollama_model_info_cache_` with unified `model_info_cache_`
  - Replaced `ollama_model_cache_` with `model_name_cache_`
  - Replaced `ollama_models_loading_` with `models_loading_`

### Files Modified
- `src/cli/service/ai/common.h` - Added `ModelInfo` struct
- `src/cli/service/ai/ai_service.h` - Added `ListAvailableModels()` and `GetProviderName()`
- `src/cli/service/ai/ollama_ai_service.h/.cc` - Implemented model listing
- `src/cli/service/ai/gemini_ai_service.h/.cc` - Implemented model listing
- `src/cli/service/ai/model_registry.h/.cc` - New registry class
- `src/app/editor/agent/agent_chat_widget.h/.cc` - Updated to use registry

## In Progress

### UI Rendering Updates (Partial)
The `RenderModelConfigControls()` function in `agent_chat_widget.cc` still references old Ollama-specific code. It needs to be updated to:
- Use unified `model_info_cache_` instead of `ollama_model_info_cache_`
- Display models from all providers in a single list
- Filter by provider when a specific provider is selected
- Show provider badges/indicators for each model

**Location**: `src/app/editor/agent/agent_chat_widget.cc:2083-2318`

**Current State**: Function still has provider-specific branches that should be unified.

## Remaining Work

### Phase 2: API Interface & Headless Mode

#### 2.1 HTTP Server Implementation
**Goal**: Expose Yaze functionality via REST API for external agents

**Tasks**:
1. Create `HttpServer` class in `src/cli/service/api/`
   - Use `httplib` (already in tree)
   - Start on configurable port (default 8080)
   - Handle CORS if needed
   
2. Implement endpoints:
   - `GET /api/v1/models` - List all available models (delegate to `ModelRegistry`)
   - `POST /api/v1/chat` - Send prompt to agent
     - Request: `{ "prompt": "...", "provider": "ollama", "model": "...", "history": [...] }`
     - Response: `{ "text_response": "...", "tool_calls": [...], "commands": [...] }`
   - `POST /api/v1/tool/{tool_name}` - Execute specific tool
     - Request: `{ "args": {...} }`
     - Response: `{ "result": "...", "status": "ok|error" }`
   - `GET /api/v1/health` - Health check
   - `GET /api/v1/rom/status` - ROM loading status

3. Integration points:
   - Initialize server in `yaze.cc` main() or via CLI flag
   - Share `Rom*` context with API handlers
   - Use `ConversationalAgentService` for chat endpoint
   - Use `ToolDispatcher` for tool endpoint

**Files to Create**:
- `src/cli/service/api/http_server.h`
- `src/cli/service/api/http_server.cc`
- `src/cli/service/api/api_handlers.h`
- `src/cli/service/api/api_handlers.cc`

**Dependencies**: `httplib`, `nlohmann/json` (already available)

### Phase 3: Enhanced Agentic Workflows

#### 3.1 Tool Expansion

**FileSystemTool** (`src/cli/handlers/tools/filesystem_commands.h/.cc`)
- **Purpose**: Allow agent to read/write files outside ROM (e.g., `src/` directory)
- **Safety**: Require user confirmation or explicit scope configuration
- **Commands**:
  - `filesystem-read <path>` - Read file contents
  - `filesystem-write <path> <content>` - Write file (with confirmation)
  - `filesystem-list <directory>` - List directory contents
  - `filesystem-search <pattern>` - Search for files matching pattern

**BuildTool** (`src/cli/handlers/tools/build_commands.h/.cc`)
- **Purpose**: Trigger builds from within agent
- **Commands**:
  - `build-cmake <build_dir>` - Run cmake configuration
  - `build-ninja <build_dir>` - Run ninja build
  - `build-status` - Check build status
  - `build-errors` - Parse and return compilation errors

**Integration**:
- Add to `ToolDispatcher::ToolCallType` enum
- Register in `ToolDispatcher::CreateHandler()`
- Add to `ToolDispatcher::ToolPreferences` struct
- Update UI toggles in `AgentChatWidget::RenderToolingControls()`

#### 3.2 Editor State Context
**Goal**: Feed editor state (open files, compilation errors) into agent context

**Tasks**:
1. Create `EditorState` struct capturing:
   - Open file paths
   - Active editor type
   - Compilation errors (if any)
   - Recent changes

2. Inject into agent prompts:
   - Add to `PromptBuilder::BuildPromptFromHistory()`
   - Include in system prompt when editor state changes

3. Update `ConversationalAgentService`:
   - Add `SetEditorState(EditorState*)` method
   - Pass to `PromptBuilder` when building prompts

**Files to Create/Modify**:
- `src/cli/service/agent/editor_state.h` (new)
- `src/cli/service/ai/prompt_builder.h/.cc` (modify)

### Phase 4: Refactoring

#### 4.1 ToolDispatcher Structured Output
**Goal**: Return JSON instead of capturing stdout

**Current State**: `ToolDispatcher::Dispatch()` returns `absl::StatusOr<std::string>` by capturing stdout from command handlers.

**Proposed Changes**:
1. Create `ToolResult` struct:
   ```cpp
   struct ToolResult {
     std::string output;  // Human-readable output
     nlohmann::json data;  // Structured data (if applicable)
     bool success;
     std::vector<std::string> warnings;
   };
   ```

2. Update command handlers to return `ToolResult`:
   - Modify base `CommandHandler` interface
   - Update each handler implementation
   - Keep backward compatibility with `OutputFormatter` for CLI

3. Update `ToolDispatcher::Dispatch()`:
   - Return `absl::StatusOr<ToolResult>`
   - Convert to JSON for API responses
   - Keep string output for CLI compatibility

**Files to Modify**:
- `src/cli/service/agent/tool_dispatcher.h/.cc`
- `src/cli/handlers/*/command_handlers.h/.cc` (all handlers)
- `src/cli/service/agent/command_handler.h` (base interface)

**Migration Strategy**:
- Add new `ExecuteStructured()` method alongside existing `Execute()`
- Gradually migrate handlers
- Keep old path for CLI until migration complete

## Technical Notes

### Model Registry Usage Pattern
```cpp
// Register services
auto& registry = cli::ModelRegistry::GetInstance();
registry.RegisterService(std::make_shared<OllamaAIService>(ollama_config));
registry.RegisterService(std::make_shared<GeminiAIService>(gemini_config));

// List all models
auto models_or = registry.ListAllModels();
// Returns unified list sorted by name
```

### API Key Management
- Gemini API key: Currently stored in `AgentConfigState::gemini_api_key`
- Consider: Environment variable fallback, secure storage
- Future: Support multiple API keys for different providers

### Thread Safety
- `ModelRegistry` uses mutex for thread-safe access
- `HttpServer` should handle concurrent requests (httplib supports this)
- `ToolDispatcher` may need locking if shared across threads

## Testing Checklist

### Phase 1 (Model Management)
- [ ] Verify Ollama models appear in unified list
- [ ] Verify Gemini models appear in unified list
- [ ] Test model refresh with multiple providers
- [ ] Test provider filtering in UI
- [ ] Test model selection and configuration

### Phase 2 (API)
- [ ] Test `/api/v1/models` endpoint
- [ ] Test `/api/v1/chat` with different providers
- [ ] Test `/api/v1/tool/*` endpoints
- [ ] Test error handling (missing ROM, invalid tool, etc.)
- [ ] Test concurrent requests
- [ ] Test CORS if needed

### Phase 3 (Tools)
- [ ] Test FileSystemTool with read operations
- [ ] Test FileSystemTool write confirmation flow
- [ ] Test BuildTool cmake/ninja execution
- [ ] Test BuildTool error parsing
- [ ] Test editor state injection into prompts

### Phase 4 (Refactoring)
- [ ] Verify all handlers return structured output
- [ ] Test API endpoints with new format
- [ ] Verify CLI still works with old format
- [ ] Performance test (no regressions)

## Known Issues

1. **UI Rendering**: `RenderModelConfigControls()` still has provider-specific code that should be unified
2. **Model Info Display**: Some fields from `ModelInfo` (like `quantization`, `modified_at`) are not displayed in unified view
3. **Error Handling**: Model listing failures are logged but don't prevent other providers from loading

## Next Steps (Priority Order)

1. **Complete UI unification** - Update `RenderModelConfigControls()` to use unified model list
2. **Implement HTTP Server** - Start with basic server and `/api/v1/models` endpoint
3. **Add chat endpoint** - Wire up `ConversationalAgentService` to API
4. **Add tool endpoint** - Expose `ToolDispatcher` via API
5. **Implement FileSystemTool** - Start with read-only operations
6. **Implement BuildTool** - Basic cmake/ninja execution
7. **Refactor ToolDispatcher** - Begin structured output migration

## References

- Plan document: `plan-yaze-api-agentic-workflow-enhancement.plan.md`
- Model Registry: `src/cli/service/ai/model_registry.h`
- AIService interface: `src/cli/service/ai/ai_service.h`
- ToolDispatcher: `src/cli/service/agent/tool_dispatcher.h`
- httplib docs: (in `ext/httplib/`)

## Questions for Next Developer

1. Should the HTTP server be enabled by default or require a flag?
2. What port should be used? (8080 suggested, but configurable?)
3. Should FileSystemTool require explicit user approval per operation or a "trusted scope"?
4. Should BuildTool be limited to specific directories (e.g., `build/`) for safety?
5. How should API authentication work? (API key? Localhost-only? None?)

---

**Last Updated**: 2025-01-XX  
**Contact**: (to be filled)

