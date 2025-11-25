# Phase 5: Advanced AI Agent Tools - Handoff Document

**Status:** Ready for Implementation  
**Owner:** TBD  
**Created:** 2025-11-25  
**Last Reviewed:** 2025-11-25  
**Next Review:** 2025-12-09  

## Overview

This document provides implementation guidance for the Phase 5 advanced AI agent tools. The foundational infrastructure has been completed in Phases 1-4, including tool integration, discoverability, schemas, context management, batching, validation, and ROM diff tools.

## Prerequisites Completed

### Infrastructure in Place

| Component | Location | Purpose |
|-----------|----------|---------|
| Tool Dispatcher | `src/cli/service/agent/tool_dispatcher.h/cc` | Routes tool calls, supports batching |
| Tool Schemas | `src/cli/service/agent/tool_schemas.h` | LLM-friendly documentation generation |
| Agent Context | `src/cli/service/agent/agent_context.h` | State preservation, caching, edit tracking |
| Validation Tools | `src/cli/service/agent/tools/validation_tool.h/cc` | ROM integrity checks |
| ROM Diff Tools | `src/cli/service/agent/tools/rom_diff_tool.h/cc` | Semantic ROM comparison |
| Test Helper CLI | `src/cli/handlers/tools/test_helpers_commands.h/cc` | z3ed tools subcommands |

### Key Patterns to Follow

1. **Tool Registration**: Add new `ToolCallType` enums in `tool_dispatcher.h`, add mappings in `tool_dispatcher.cc`
2. **Command Handlers**: Inherit from `resources::CommandHandler`, implement `GetName()`, `GetUsage()`, `ValidateArgs()`, `Execute()`
3. **Schema Registration**: Add schemas in `ToolSchemaRegistry::RegisterBuiltinSchemas()`

## Phase 5 Tools to Implement

### 5.1 Visual Analysis Tool

**Purpose:** Tile pattern recognition, sprite sheet analysis, palette usage statistics.

**Suggested Implementation:**

```
src/cli/service/agent/tools/visual_analysis_tool.h
src/cli/service/agent/tools/visual_analysis_tool.cc
```

**Tool Commands:**

| Tool Name | Description | Priority |
|-----------|-------------|----------|
| `visual-find-similar-tiles` | Find tiles with similar patterns | High |
| `visual-analyze-spritesheet` | Identify unused graphics regions | Medium |
| `visual-palette-usage` | Stats on palette usage across maps | Medium |
| `visual-tile-histogram` | Frequency analysis of tile usage | Low |

**Key Implementation Details:**

```cpp
class TileSimilarityTool : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "visual-find-similar-tiles"; }
  
  // Compare tiles using simple pixel difference or structural similarity
  // Output: JSON array of {tile_id, similarity_score, location}
};

class SpritesheetAnalysisTool : public resources::CommandHandler {
  // Analyze 8x8 or 16x16 tile regions
  // Identify contiguous unused (0x00 or 0xFF) regions
  // Report in JSON with coordinates and sizes
};
```

**Dependencies:**
- `app/gfx/snes_tile.h` - Tile manipulation
- `app/gfx/bitmap.h` - Pixel access
- Overworld/Dungeon loaders for context

**Estimated Effort:** 4-6 hours

---

### 5.2 Code Generation Tool

**Purpose:** Generate ASM patches, Asar scripts, and template-based code.

**Suggested Implementation:**

```
src/cli/service/agent/tools/code_gen_tool.h
src/cli/service/agent/tools/code_gen_tool.cc
```

**Tool Commands:**

| Tool Name | Description | Priority |
|-----------|-------------|----------|
| `codegen-asm-hook` | Generate ASM hook at address | High |
| `codegen-freespace-patch` | Generate patch using freespace | High |
| `codegen-sprite-template` | Generate sprite ASM template | Medium |
| `codegen-event-handler` | Generate event handler code | Medium |

**Key Implementation Details:**

```cpp
struct AsmTemplate {
  std::string name;
  std::string code_template;  // With {{PLACEHOLDER}} syntax
  std::vector<std::string> required_params;
};

class CodeGenTool : public resources::CommandHandler {
 private:
  // Template library for common patterns
  static const std::vector<AsmTemplate> kTemplates;
  
  std::string GenerateHook(uint32_t address, const std::string& label);
  std::string GenerateFreespaceBlock(size_t size, const std::string& code);
  std::string SubstitutePlaceholders(const std::string& tmpl, 
                                     const std::map<std::string, std::string>& params);
};
```

**Templates to Include:**

```asm
; Hook template
org ${{ADDRESS}}
  JSL {{LABEL}}
  
freecode
{{LABEL}}:
  {{CODE}}
  RTL

; Sprite template  
{{SPRITE_NAME}}:
  PHB
  PHK
  PLB
  {{SPRITE_CODE}}
  PLB
  RTL
```

**Dependencies:**
- ROM free space detection from `validation_tool.cc`
- Address validation from memory inspector

**Estimated Effort:** 6-8 hours

---

### 5.3 Project Tool

**Purpose:** Multi-file edit coordination, versioning, project state export/import.

**Suggested Implementation:**

```
src/cli/service/agent/tools/project_tool.h
src/cli/service/agent/tools/project_tool.cc
```

**Tool Commands:**

| Tool Name | Description | Priority |
|-----------|-------------|----------|
| `project-status` | Show current project state | High |
| `project-snapshot` | Create named checkpoint | High |
| `project-restore` | Restore to checkpoint | High |
| `project-export` | Export project as archive | Medium |
| `project-import` | Import project archive | Medium |
| `project-diff` | Compare project states | Low |

**Key Implementation Details:**

```cpp
struct ProjectSnapshot {
  std::string name;
  std::string description;
  std::chrono::system_clock::time_point created;
  std::vector<RomEdit> edits;
  std::map<std::string, std::string> metadata;
};

class ProjectManager {
 public:
  // Integrate with AgentContext
  void SetContext(AgentContext* ctx);
  
  absl::Status CreateSnapshot(const std::string& name);
  absl::Status RestoreSnapshot(const std::string& name);
  std::vector<ProjectSnapshot> ListSnapshots() const;
  
  // Export as JSON + binary patches
  absl::Status ExportProject(const std::string& path);
  absl::Status ImportProject(const std::string& path);
  
 private:
  AgentContext* context_ = nullptr;
  std::vector<ProjectSnapshot> snapshots_;
  std::filesystem::path project_dir_;
};
```

**Project File Format:**

```yaml
# .yaze-project/project.yaml
version: 1
name: "My ROM Hack"
base_rom: "zelda3.sfc"
snapshots:
  - name: "initial"
    created: "2025-11-25T12:00:00Z"
    edits_file: "snapshots/initial.bin"
  - name: "dungeon-1-complete"
    created: "2025-11-25T14:30:00Z"
    edits_file: "snapshots/dungeon-1.bin"
```

**Dependencies:**
- `AgentContext` for edit tracking
- YAML/JSON serialization
- Binary diff/patch format

**Estimated Effort:** 8-10 hours

---

## Integration Points

### Adding New Tools to Dispatcher

1. Add enum values in `tool_dispatcher.h`:
```cpp
enum class ToolCallType {
  // ... existing ...
  // Visual Analysis
  kVisualFindSimilarTiles,
  kVisualAnalyzeSpritesheet,
  kVisualPaletteUsage,
  // Code Generation
  kCodeGenAsmHook,
  kCodeGenFreespacePatch,
  // Project
  kProjectStatus,
  kProjectSnapshot,
  kProjectRestore,
};
```

2. Add tool name mappings in `tool_dispatcher.cc`:
```cpp
if (tool_name == "visual-find-similar-tiles")
  return ToolCallType::kVisualFindSimilarTiles;
```

3. Add handler creation:
```cpp
case ToolCallType::kVisualFindSimilarTiles:
  return std::make_unique<TileSimilarityTool>();
```

4. Add preference flags if needed:
```cpp
struct ToolPreferences {
  // ... existing ...
  bool visual_analysis = true;
  bool code_gen = true;
  bool project = true;
};
```

### Registering Schemas

Add to `ToolSchemaRegistry::RegisterBuiltinSchemas()`:

```cpp
Register({.name = "visual-find-similar-tiles",
          .category = "visual",
          .description = "Find tiles with similar patterns",
          .arguments = {{.name = "tile_id",
                         .type = "number",
                         .description = "Reference tile ID",
                         .required = true},
                        {.name = "threshold",
                         .type = "number",
                         .description = "Similarity threshold (0-100)",
                         .default_value = "90"}},
          .examples = {"z3ed visual-find-similar-tiles --tile_id=42 --threshold=85"}});
```

## Testing Strategy

### Unit Tests

Create in `test/unit/tools/`:
- `visual_analysis_tool_test.cc`
- `code_gen_tool_test.cc`
- `project_tool_test.cc`

### Integration Tests

Add to `test/integration/agent/`:
- `visual_analysis_integration_test.cc`
- `code_gen_integration_test.cc`
- `project_workflow_test.cc`

### AI Evaluation Tasks

Add to `scripts/ai/eval-tasks.yaml`:

```yaml
categories:
  visual_analysis:
    description: "Visual analysis and pattern recognition"
    tasks:
      - id: "find_similar_tiles"
        prompt: "Find tiles similar to tile 42 in the ROM"
        required_tool: "visual-find-similar-tiles"
        
  code_generation:
    description: "Code generation tasks"
    tasks:
      - id: "generate_hook"
        prompt: "Generate an ASM hook at $8000 that calls my_routine"
        required_tool: "codegen-asm-hook"
```

## Implementation Order

1. **Week 1:** Visual Analysis Tool (most straightforward)
2. **Week 2:** Code Generation Tool (builds on validation/freespace)
3. **Week 3:** Project Tool (requires more design for versioning)

## Success Criteria

- [ ] All three tools implemented with at least core commands
- [ ] Unit tests passing for each tool
- [ ] Integration tests with real ROM data
- [ ] AI evaluation tasks added and baseline scores recorded
- [ ] Documentation updated in `scripts/README.md`

## Open Questions

1. **Visual Analysis:** Should we support external image comparison libraries (OpenCV) or keep it pure C++?
2. **Code Generation:** What Asar-specific features should we support?
3. **Project Tool:** Should snapshots include graphics/binary data or just edit logs?

## Related Documents

- [Agent Protocol](./personas.md)
- [Coordination Board](./coordination-board.md)
- [Test Infrastructure Plan](../../test/README.md)
- [AI Evaluation Suite](../../scripts/README.md#ai-model-evaluation-suite)

