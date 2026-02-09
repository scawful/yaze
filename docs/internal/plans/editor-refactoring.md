# Refactoring Plan: Decoupling EditorManager & EditorSet

## Objective
Refactor the core editor architecture (`EditorManager`, `EditorSet`) to move from a tightly coupled, monolithic design to a data-driven, polymorphic architecture. This will allow adding new editors without modifying core headers and enable dynamic loading/configuration.

## Current Issues
1.  **Static Coupling**: `EditorSet` (in `session_types.h`) contains specific `std::unique_ptr` members for every editor type.
2.  **Manual Instantiation**: `EditorSet` constructor manually `new`s every editor.
3.  **Inconsistent Inheritance**: `MemoryEditor` does not inherit from the base `Editor` class.
4.  **Leaky Abstractions**: `ScreenEditor` exposes `dungeon_maps_` publicly.
5.  **Manual Wiring**: `EditorSet::ApplyDependencies` contains hardcoded logic.
6.  **God-Class Dependency**: `EditorManager` depends on concrete editor headers.

## Detailed Implementation Steps

### Phase 1: Standardization & Inheritance [COMPLETE]
**Target**: `src/app/editor/code/memory_editor.h`, `src/app/editor/graphics/screen_editor.h`

1.  **MemoryEditor**: [DONE]
    *   Inherit from `yaze::editor::Editor`.
    *   Change `Update(bool&)` to `absl::Status Update()`.
2.  **ScreenEditor**: [DONE]
    *   Move `dungeon_maps_` saving logic into `ScreenEditor::Save()`.
3.  **DungeonEditorV2**: [DONE]
    *   Standardize `Initialize(renderer, rom)` to `Initialize()`.
4.  **AssemblyEditor**: [DONE]
    Standardized `Update()` signature.

### Phase 2: Dynamic Editor Container
**Target**: `src/app/editor/session_types.h`, `src/app/editor/session_types.cc`

1.  **Refactor `EditorSet`**:
    *   Replace named members with `std::unordered_map<EditorType, std::unique_ptr<Editor>> editors_`.
2.  **Generic Accessors**:
    *   Implement `Editor* GetEditor(EditorType type) const`.
    *   Implement `template <typename T> T* GetEditorAs(EditorType type)`.

### Phase 3: Registry-Based Instantiation
**Target**: `src/app/editor/system/editor_registry.h`, `src/app/editor/editor_manager.cc`

1.  **Factory Registry**:
    *   Add `RegisterFactory` to `EditorRegistry`.
2.  **Initialization**:
    *   Register factories in `EditorManager::Initialize`.
    *   Update `EditorSet` constructor to use the registry.

### Phase 4: Polymorphic Wiring
**Target**: `src/app/editor/editor.h`, specific editor implementations

1.  **Virtual Configuration**:
    *   Make `Editor::SetDependencies` virtual.
2.  **Self-Wiring overrides**:
    *   Override `SetDependencies` in `MusicEditor`, `SettingsPanel`, `MemoryEditor` to extract specific dependencies.
3.  **Simplify `ApplyDependencies`**:
    *   Reduce to a generic loop.

### Phase 5: Decoupling EditorManager
**Target**: `src/app/editor/editor_manager.cc`

1.  **Event-Driven Navigation**:
    *   Use `EventBus` for `JumpToRoom` and `JumpToMap`.
2.  **Cleanup**:
    *   Remove specific editor headers.

## Testing & Verification Strategy (TDD)

To ensure stability and reduce ambiguity, each phase will be accompanied by specific tests.

### Phase 1: Standardization Tests
*   **Verification**: Ensure `MemoryEditor` compiles as an `Editor` subclass.
*   **Unit Test**: Update `test/unit/editor/editor_manager_test.cc` to ensure `Initialize` still passes with the modified `DungeonEditorV2` signature.

### Phase 2: Dynamic Container Tests
*   **New Test File**: `test/unit/editor/editor_set_test.cc`
    *   **Test**: `ContainerOperations`: Store a MockEditor, retrieve it via `GetEditor` and `GetEditorAs`.
    *   **Test**: `InvalidAccess`: Ensure retrieval of non-existent editor type returns `nullptr`.

### Phase 3: Registry Tests
*   **New Test File**: `test/unit/editor/editor_registry_test.cc`
    *   **Test**: `FactoryRegistration`: Register a dummy factory and verify it can create an instance.
    *   **Test**: `FactoryExecution`: Verify the factory receives the correct `Rom*` pointer.

### Phase 4: Wiring Tests
*   **Update**: `test/unit/editor/editor_set_test.cc`
    *   **Test**: `DependencyPropagation`: Create a `MockEditor` that tracks `SetDependencies` calls. Call `EditorSet::ApplyDependencies` and verify the mock received it.

### Phase 5: Decoupling Verification
*   **Code Audit**: `grep` check on `editor_manager.cc` to ensure no specific editor headers (e.g., `#include "app/editor/dungeon/dungeon_editor_v2.h"`) remain.
*   **Integration Test**: Run the full `EditorManagerTest` suite to ensure no regressions in application startup.

## "Done" Criteria
1.  All code compiles without warnings.
2.  `test/unit/editor/editor_set_test.cc` and `test/unit/editor/editor_registry_test.cc` are implemented and passing.
3.  Existing `EditorManager` tests pass.
4.  `EditorManager.cc` contains zero specific editor includes.