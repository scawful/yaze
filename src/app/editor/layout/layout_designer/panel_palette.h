#ifndef YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_PANEL_PALETTE_H_
#define YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_PANEL_PALETTE_H_

#include <string>
#include <vector>

namespace yaze {
namespace editor {
namespace layout_designer {

// One row in the palette. Sourced from ContentRegistry::Panels at draw
// time so the palette reflects exactly what REGISTER_PANEL knows about.
// The designer works against this snapshot — not the live
// WorkspaceWindowManager — which means panels that aren't yet registered
// in the active session still appear (registering is editor-switch-gated;
// authoring isn't).
struct PanelPaletteEntry {
  std::string panel_id;
  std::string display_name;
  std::string icon;
  std::string category;
};

// Collect palette entries from ContentRegistry::Panels. `exclude_panel_id`
// filters self-reference so the designer can't be dropped inside itself.
// Entries are sorted by (category, display_name) for a stable layout.
std::vector<PanelPaletteEntry> CollectPaletteEntries(
    const std::string& exclude_panel_id = {});

// Stateless widget. Draws a search box at top and a category-grouped list
// of entries (each group is a collapsing header). Returns the panel_id of
// the row the user clicked this frame, or empty string on no-click. The
// caller owns `*query` (the search string); pass a persistent member.
//
// Phase 6.1 contract: click returns the id. Phase 6.2 adds a drag-source
// around each row so dropping on the canvas authoring surface splits or
// appends to the dock tree.
std::string DrawPanelPalette(const std::vector<PanelPaletteEntry>& entries,
                             std::string* query);

namespace panel_palette_internal {

// Case-insensitive substring match over the whitespace-split terms in
// `query`. Empty query matches everything. Matches when the `entry`'s
// display_name, panel_id, OR category matches every term.
bool MatchesQuery(const PanelPaletteEntry& entry, const std::string& query);

}  // namespace panel_palette_internal

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_LAYOUT_DESIGNER_PANEL_PALETTE_H_
