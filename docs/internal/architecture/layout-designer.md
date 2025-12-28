# Layout Designer (December 2025)

Canonical reference for the ImGui layout designer utility that lives in `src/lab/layout_designer/`. Use this in place of the older phase-by-phase notes and mockups.

## Current Capabilities
- **Two modes**: Panel Layout (dock graph editing) and Widget Design (panel internals) toggled in the toolbar of `LayoutDesignerWindow`.
- **Panel layout mode**: Palette from `PanelManager` descriptors with search/category filter, drag-and-drop into a dock tree with split drop-zones, selection + property editing, zoom controls, optional code preview, and a theme panel. JSON export writes via `LayoutSerializer::SaveToFile`; import/export dialogs are stubbed.
- **Widget design mode**: Palette from `yaze_widgets`, canvas + properties UI, and code generation through `WidgetCodeGenerator` (deletion/undo/redo are still TODOs).
- **Runtime import**: `ImportFromRuntime()` builds a flat layout from the registered `PanelDescriptor`s (no live dock positions yet). `PreviewLayout()` can apply DockBuilder ops against `MainDockSpace` when `LayoutManager` + `PanelManager` are wired, but it does not persist layouts.

## Integration Quick Start
- **Lab target**: Build with `-DYAZE_BUILD_LAB=ON` and run the `lab` executable. The lab host opens the designer by default and provides the `MainDockSpace` dockspace for preview/testing.
- **Embedding**: If you want to host the designer elsewhere, wire it with a `PanelManager` + `LayoutManager` and draw it each frame.

```cpp
#include "lab/layout_designer/layout_designer_window.h"

layout_designer::LayoutDesignerWindow layout_designer;
layout_designer.Initialize(&panel_manager, &layout_manager, nullptr);
layout_designer.Open();

if (layout_designer.IsOpen()) {
  layout_designer.Draw();
}
```

## Improvement Backlog (code-aligned)
1. **Preview/apply pipeline**: Extend `LayoutDesignerWindow::PreviewLayout()` to cover full `LayoutManager` integration (persisted layouts + session-aware visibility) and avoid relying solely on `MainDockSpace`.
2. **Serialization round-trip**: Finish `LayoutSerializer::FromJson()` and wire real open/save dialogs. Validate versions/author fields and surface parse errors in the UI. Add a simple JSON schema example to `layout_designer/README.md` once load works.
3. **Runtime import fidelity**: Replace the flat import in `ImportFromRuntime()` with actual dock sampling (dock nodes, split ratios, and current visible panels), filtering out dashboard/welcome. Capture panel visibility per session instead of assuming all-visible defaults.
4. **Editing polish**: Implement delete/undo/redo for panels/widgets, and make widget deletion/selection consistent across both modes. Reduce debug logging spam (`DragDrop` noise) once the drop pipeline is stable.
5. **Export path**: Hook `ExportCode()` to write the generated code preview to disk and optionally emit a `LayoutManager` preset stub for quick integration.
