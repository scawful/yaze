# GUI Abstraction Layer Reference

Technical reference for `src/app/gui/`, the custom ImGui abstraction layer used throughout Yaze.

## Module Structure

```
src/app/gui/
├── canvas/          # Pannable/zoomable canvas system (37 files)
├── widgets/         # Reusable themed widgets (12 files)
├── style/           # Theme and styling
├── core/            # Core GUI utilities
├── automation/      # GUI automation/testing
├── app/             # Application-level GUI components
└── plots/           # Data visualization
```

## Canvas System

The `Canvas` class (`canvas/canvas.h`) provides a pannable, zoomable drawing surface for tile editing, map viewing, and graphics manipulation.

### Core Types

```cpp
enum class CanvasType { kTile, kBlock, kMap };
enum class CanvasMode { kPaint, kSelect };
enum class CanvasGridSize { k8x8, k16x16, k32x32, k64x64 };
```

### CanvasRuntime State

Transient state updated each frame:

```cpp
struct CanvasRuntime {
  ImDrawList* draw_list;      // ImGui draw list
  ImVec2 canvas_p0;           // Canvas origin in screen space
  ImVec2 canvas_sz;           // Canvas size in pixels
  ImVec2 scrolling;           // Pan offset
  ImVec2 mouse_pos_local;     // Mouse position in canvas space
  bool hovered;               // Mouse over canvas
  float grid_step;            // Grid cell size
  float scale;                // Zoom level
  ImVec2 content_size;        // Content dimensions
};
```

### Basic Usage

```cpp
gui::Canvas canvas("my_canvas", ImVec2(512, 512));

// Configure
canvas.SetCanvasSize(ImVec2(512, 512));
canvas.set_draggable(true);  // Enable pan

// In draw loop
canvas.DrawBackground();  // Set up canvas
canvas.DrawBitmap(my_bitmap, 0, 0);  // Draw content
canvas.DrawGrid();  // Optional grid overlay
canvas.DrawOverlay();  // Selection rectangles, etc.

// Handle interaction
if (canvas.IsHovered() && ImGui::IsMouseClicked(0)) {
  auto pos = canvas.GetMousePosInCanvas();
  // Handle click at pos
}
```

### Frame-Based API

For more control, use the frame-based API:

```cpp
gui::CanvasFrameOptions opts;
opts.canvas_size = ImVec2(512, 512);
opts.draw_grid = true;
opts.draw_context_menu = true;

if (canvas.BeginFrame(opts)) {
  // Draw content
  canvas.DrawBitmap(bitmap);

  // Custom drawing via ImDrawList
  auto* draw_list = canvas.runtime().draw_list;
  draw_list->AddRect(...);

  canvas.EndFrame();
}
```

### Tile Selection

For tile-based editing:

```cpp
canvas.SetTileSize(16);  // 16x16 tiles

// Get tile under mouse
int tile_x = canvas.GetTileX(mouse_x);
int tile_y = canvas.GetTileY(mouse_y);

// Draw selection highlight
canvas.DrawTileHighlight(tile_x, tile_y, highlight_color);
```

### Zoom and Pan

```cpp
// Zoom
canvas.SetZoom(2.0f);  // 2x zoom
float zoom = canvas.GetZoom();

// Pan
canvas.SetScrolling(ImVec2(100, 50));  // Scroll offset
ImVec2 scroll = canvas.scrolling();

// Zoom to fit content
canvas.ZoomToFit(content_size);
```

### Canvas Components

The canvas is composed of modular components:

| Component | File | Purpose |
|-----------|------|---------|
| `CanvasInteractionHandler` | `canvas_interaction_handler.h` | Mouse/keyboard input |
| `CanvasRendering` | `canvas_rendering.h` | Draw operations |
| `CanvasContextMenu` | `canvas_context_menu.h` | Right-click menus |
| `CanvasGeometry` | `canvas_geometry.h` | Coordinate transforms |
| `CanvasPopup` | `canvas_popup.h` | Modal dialogs |
| `CanvasState` | `canvas_state.h` | Selection state |
| `CanvasUsageTracker` | `canvas_usage_tracker.h` | Analytics |

## Themed Widgets

Reusable widgets in `widgets/` follow consistent theming.

### PaletteEditorWidget

Color palette editing with live preview:

```cpp
gui::PaletteEditorWidget palette_editor;

// Initialize with palette data
palette_editor.Initialize(palette);

// Draw in ImGui context
palette_editor.Update();  // Handles all UI
```

### TileSelectorWidget

Tile selection grid:

```cpp
gui::TileSelectorWidget selector("tiles", tileset_bitmap, 16);

// Draw and get selection
selector.Update();
if (selector.HasSelection()) {
  int tile_id = selector.GetSelectedTile();
}
```

### AssetBrowser

Resource browsing with filtering:

```cpp
gui::AssetBrowser browser;
browser.SetAssets(asset_list);
browser.Update();

if (browser.HasSelection()) {
  auto& asset = browser.GetSelected();
}
```

### TextEditor

Syntax-highlighted text editing:

```cpp
gui::TextEditor editor;
editor.SetLanguage("asm65816");  // Syntax highlighting
editor.SetText(source_code);
editor.Render("editor_id");

if (editor.IsTextChanged()) {
  std::string new_text = editor.GetText();
}
```

## Theme System

All UI uses the `AgentUITheme` system for consistent colors.

### Using Themes

```cpp
const auto& theme = AgentUI::GetTheme();

// Use semantic colors
ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
ImGui::PushStyleColor(ImGuiCol_Text, theme.text_color);
// ... draw UI ...
ImGui::PopStyleColor(2);
```

### Available Theme Colors

```cpp
struct AgentUITheme {
  ImVec4 panel_bg_color;
  ImVec4 text_color;
  ImVec4 text_muted_color;
  ImVec4 border_color;
  ImVec4 accent_color;
  ImVec4 status_success;
  ImVec4 status_error;
  ImVec4 status_warning;
  ImVec4 provider_ollama;
  ImVec4 provider_gemini;
  // ...
};
```

### Helper Functions

```cpp
// Panel styling
AgentUI::PushPanelStyle();
// ... panel content ...
AgentUI::PopPanelStyle();

// Section headers
AgentUI::RenderSectionHeader(ICON_FA_COG, "Settings", theme.accent_color);

// Status indicators
AgentUI::RenderStatusIndicator(status == "ok" ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1));

// Styled buttons
if (AgentUI::StyledButton("Submit", theme.accent_color)) {
  // Handle click
}
```

## GUI Automation

For testing and external control, the canvas exposes an automation API:

```cpp
class CanvasAutomationAPI {
 public:
  // Simulate clicks
  void SimulateClick(ImVec2 pos);
  void SimulateDrag(ImVec2 start, ImVec2 end);

  // Query state
  ImVec2 GetCanvasPosition() const;
  ImVec2 GetSelectionRect() const;

  // Capture
  std::vector<uint8_t> CaptureScreenshot();
};
```

This is used by the gRPC automation server for remote GUI testing.

## Performance Notes

1. **Texture caching**: Bitmaps are uploaded to GPU once, reused until invalidated
2. **Draw batching**: Multiple draw calls batched per frame
3. **Lazy loading**: Large tilemaps loaded progressively via `gfx::Arena`
4. **Viewport culling**: Only visible content is drawn

### Arena Integration

For large assets, use the global Arena for deferred loading:

```cpp
// Queue for background loading
gfx::Arena::Get().QueueDeferredTexture(bitmap, priority);

// Process each frame (in Update())
auto batch = gfx::Arena::Get().GetNextDeferredTextureBatch(4, 2);
for (auto& tex : batch) {
  tex->EnsureTexture();
}
```

## Related Documentation

- `docs/public/developer/canvas-system.md` - Public canvas guide
- `docs/public/developer/architecture.md` - Architecture patterns
- `src/app/editor/agent/agent_ui_theme.h` - Theme definitions
