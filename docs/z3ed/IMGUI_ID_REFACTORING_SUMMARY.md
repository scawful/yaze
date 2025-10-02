# ImGui ID Management Analysis & Implementation Summary

**Date**: October 2, 2025  
**Prepared for**: @scawful  
**Topic**: GUI widget ID refactoring for z3ed test automation

## Executive Summary

I've completed a comprehensive analysis of YAZE's ImGui ID management and created a complete refactoring plan to enable better test automation and eliminate duplicate ID issues.

### Key Findings

1. **100+ uses of `##` prefix** - Creates unnamed widgets that are hard to reference from tests
2. **Potential ID conflicts** - Multiple widgets with same label (`##table`, `##canvas`) in different scopes
3. **No centralized registry** - Test automation has no way to discover available widgets
4. **Inconsistent naming** - No convention across editors

### Proposed Solution

**Hierarchical Widget ID System** with:
- Automatic ID scoping via RAII helpers
- Centralized widget registry for discovery
- Stable, predictable widget paths
- Machine-readable catalog for AI agents

**Example transformation**:
```cpp
// Before
if (ImGui::BeginChild("##Canvas", ...)) { }

// After  
YAZE_WIDGET_SCOPE("Canvas");
if (ImGui::BeginChild("OverworldMap", ...)) {
  YAZE_REGISTER_WIDGET(canvas, "OverworldMap");
  // Widget path: Overworld/Main/Canvas/canvas:OverworldMap
}
```

## Deliverables Created

### 1. Comprehensive Design Document
**File**: `docs/z3ed/IMGUI_ID_MANAGEMENT_REFACTORING.md`

**Contents**:
- Current state analysis (100+ ##-prefixed widgets cataloged)
- Hierarchical ID scheme design
- 4-phase implementation plan (17-26 hours)
- Testing strategy
- Integration with z3ed agent workflow
- Backwards compatibility approach

**Key sections**:
- Pattern analysis of existing code
- Proposed naming convention: `<Editor>/<Tab>/<Section>/<WidgetType>:<Name>`
- Benefits for AI-driven automation
- Migration timeline and priorities

### 2. Core Infrastructure Implementation
**Files**: 
- `src/app/gui/widget_id_registry.h` (177 lines)
- `src/app/gui/widget_id_registry.cc` (193 lines)

**Features**:
- `WidgetIdScope` - RAII helper for automatic ID push/pop
- `WidgetIdRegistry` - Singleton registry with discovery methods
- Thread-safe ID stack management
- Pattern matching for widget lookup
- YAML/JSON export for z3ed agent

**API Highlights**:
```cpp
// RAII scoping
YAZE_WIDGET_SCOPE("Overworld");
YAZE_WIDGET_SCOPE("Canvas");

// Widget registration
YAZE_REGISTER_WIDGET(button, "DrawTile");

// Discovery
auto matches = registry.FindWidgets("*/button:*");
std::string catalog = registry.ExportCatalog("yaml");
```

### 3. Documentation Updates
**Updated**: `docs/z3ed/README.md`
- Added new "Implementation Guides" section
- Updated documentation structure
- Cross-references to refactoring guide

## Implementation Plan Summary

### Phase 1: Core Infrastructure (2-3 hours) ⚡
**Priority**: P0 - Immediate  
**Status**: Code complete, needs build integration

**Tasks**:
- ✅ Created WidgetIdScope RAII helper
- ✅ Created WidgetIdRegistry with discovery
- 📋 Add to CMake build system
- 📋 Write unit tests

### Phase 2: Overworld Editor Refactoring (3-4 hours)
**Priority**: P0 - This week  
**Rationale**: Most complex, most tested, immediate value for E2E validation

**Tasks**:
- Add `YAZE_WIDGET_SCOPE` at function boundaries
- Replace `##name` with meaningful names
- Register all interactive widgets
- Test with z3ed agent test

### Phase 3: Test Harness Integration (1-2 hours)
**Priority**: P0 - This week

**Tasks**:
- Add `DiscoverWidgets` RPC to proto
- Update Click/Type/Assert handlers to use registry
- Add widget suggestions on lookup failures
- Test with grpcurl

### Phase 4: Gradual Migration (4-6 hours per editor)
**Priority**: P1-P2 - Next 2-3 weeks

**Order**:
1. Overworld Editor (P0)
2. Dungeon Editor (P1)
3. Palette Editor (P1)
4. Graphics Editor (P1)
5. Remaining editors (P2)

## Benefits for z3ed Agent Workflow

### 1. Stable Widget References
```bash
# Before: brittle string matching
z3ed agent test --prompt "Click button:Overworld"

# After: hierarchical path resolution
z3ed agent test --prompt "Click the DrawTile tool"
# Resolves to: Overworld/Main/Toolset/button:DrawTile
```

### 2. Widget Discovery for AI
```bash
z3ed agent describe --widgets --format yaml > docs/api/yaze-widgets.yaml
```

**Output includes**:
- Full widget paths
- Widget types (button, input, canvas, etc.)
- Hierarchical context (editor, tab, section)
- Available actions (click, type, drag, etc.)

### 3. Automated Test Generation
AI agents can:
- Query widget catalog to understand UI structure
- Generate commands with stable widget references
- Use partial matching for fuzzy lookups
- Get helpful suggestions when widgets not found

## Integration with Existing Work

### Complements IT-01 (ImGuiTestHarness)
- Test harness can now discover widgets dynamically
- Widget registry provides stable IDs for Click/Type/Assert RPCs
- Better error messages with suggested alternatives

### Enables IT-02 (CLI Agent Test)
- Natural language prompts can resolve to exact widget paths
- TestWorkflowGenerator can query available widgets
- LLM can read widget catalog to understand UI

### Supports E2E Validation
- Fixes window detection issues with proper ID scoping
- Eliminates duplicate ID warnings
- Provides foundation for comprehensive GUI testing

## Next Steps

### Immediate (Tonight/Tomorrow) - 3 hours
1. Add widget_id_registry to CMakeLists.txt
2. Write unit tests for WidgetIdScope and WidgetIdRegistry
3. Build and verify no compilation errors

### This Week - 6 hours
4. Refactor Overworld Editor (Phase 2)
   - Start with DrawToolset() and DrawOverworldCanvas()
   - Add scoping and registration
   - Test with existing E2E tests

5. Integrate with test harness (Phase 3)
   - Add DiscoverWidgets RPC
   - Update Click handler to use registry
   - Test widget discovery via grpcurl

### Next Week - 8 hours
6. Continue editor migration (Dungeon, Palette, Graphics)
7. Write comprehensive documentation
8. Update z3ed guides with widget path examples

## Success Metrics

**Technical**:
- ✅ Zero duplicate ImGui ID warnings
- ✅ All interactive widgets registered and discoverable
- ✅ Test automation can reference any widget
- ✅ No performance regression

**UX**:
- ✅ Natural language prompts work reliably
- ✅ Error messages suggest correct widget paths
- ✅ AI agents can understand UI structure
- ✅ Tests are maintainable across refactors

## Risk Mitigation

### Backwards Compatibility
- Fallback mechanism for legacy string lookups
- Gradual migration, no breaking changes
- Both systems work during transition

### Performance
- Registry overhead minimal (hash map lookup)
- Thread-local storage for ID stack
- Lazy registration (only interactive widgets)

### Maintenance
- RAII helpers prevent scope leaks
- Macros hide complexity from editor code
- Centralized registry simplifies updates

## Code Review Notes

### Design Decisions

**Why RAII for scoping?**
- Automatic push/pop prevents mistakes
- Matches ImGui's own ID scoping semantics
- Clean, exception-safe

**Why thread_local for ID stack?**
- ImGui contexts are per-thread
- Avoids race conditions
- Allows multiple test instances

**Why singleton for registry?**
- Single source of truth
- Easy access from any editor
- Matches ImGui's singleton pattern

**Why hierarchical paths?**
- Natural organization (editor/tab/section/widget)
- Easy to understand and remember
- Supports partial matching
- Mirrors filesystem conventions

### Future Enhancements

1. **Widget State Tracking**
   - Track enabled/disabled state
   - Track visibility
   - Track value changes

2. **Action Recording**
   - Record user interactions
   - Generate tests from recordings
   - Replay for regression testing

3. **Visual Tree Inspector**
   - ImGui debug window showing widget hierarchy
   - Click to highlight in UI
   - Real-time registration updates

## References

**Related z3ed Documents**:
- [E6-z3ed-cli-design.md](E6-z3ed-cli-design.md) - Agent architecture
- [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md) - Test harness usage  
- [NEXT_PRIORITIES_OCT2.md](NEXT_PRIORITIES_OCT2.md) - Current priorities
- [PROJECT_STATUS_OCT2.md](PROJECT_STATUS_OCT2.md) - Project status

**ImGui Documentation**:
- [ImGui FAQ - Widget IDs](https://github.com/ocornut/imgui/blob/master/docs/FAQ.md#q-how-can-i-have-multiple-widgets-with-the-same-label)
- [ImGui Test Engine](https://github.com/ocornut/imgui_test_engine) - Reference implementation

---

**Prepared by**: GitHub Copilot  
**Review Status**: Ready for implementation  
**Estimated Total Effort**: 17-26 hours over 2-3 weeks  
**Immediate Priority**: Phase 1 build integration (3 hours)
