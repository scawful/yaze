# Layout Designer (December 2025)

Canonical reference for the ImGui layout designer utility that lives in `src/app/editor/layout_designer/`. Use this in place of the older phase-by-phase notes and mockups.

## Current Capabilities
- **Two modes**: Panel Layout (dock graph editing) and Widget Design (panel internals) toggled in the toolbar of `LayoutDesignerWindow`.
- **Panel layout mode**: Palette from `PanelManager` descriptors with search/category filter, drag-and-drop into a dock tree with split drop-zones, selection + property editing, zoom controls, optional code preview, and a theme panel. JSON export writes via `LayoutSerializer::SaveToFile`; import/export dialogs are stubbed.
- **Widget design mode**: Palette from `yaze_widgets`, canvas + properties UI, and code generation through `WidgetCodeGenerator` (deletion/undo/redo are still TODOs).
- **Runtime import**: `ImportFromRuntime()` builds a flat layout from the registered `PanelDescriptor`s (no live dock positions yet). `PreviewLayout()` is stubbed and does not apply to the running dockspace.

## Integration Quick Start
```cpp
// EditorManager member
layout_designer::LayoutDesignerWindow layout_designer_;

// Init once with the panel manager
layout_designer_.Initialize(&panel_manager_);

// Open from menu or shortcut
layout_designer_.Open();

// Draw every frame
if (layout_designer_.IsOpen()) {
  layout_designer_.Draw();
}
```

## Improvement Backlog (code-aligned)
1. **Preview/apply pipeline**: Implement `LayoutDesignerWindow::PreviewLayout()` to transform a `LayoutDefinition` into DockBuilder operations (use `PanelDescriptor::GetWindowTitle()` and the active dockspace ID from `LayoutManager`). Ensure session-aware panel IDs and call back into `LayoutManager`/`PanelManager` so visibility state stays in sync.
2. **Serialization round-trip**: Finish `LayoutSerializer::FromJson()` and wire real open/save dialogs. Validate versions/author fields and surface parse errors in the UI. Add a simple JSON schema example to `layout_designer/README.md` once load works.
3. **Runtime import fidelity**: Replace the flat import in `ImportFromRuntime()` with actual dock sampling (dock nodes, split ratios, and current visible panels), filtering out dashboard/welcome. Capture panel visibility per session instead of assuming all-visible defaults.
4. **Editing polish**: Implement delete/undo/redo for panels/widgets, and make widget deletion/selection consistent across both modes. Reduce debug logging spam (`DragDrop` noise) once the drop pipeline is stable.
5. **Export path**: Hook `ExportCode()` to write the generated code preview to disk and optionally emit a `LayoutManager` preset stub for quick integration.
