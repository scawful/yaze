#ifndef YAZE_APP_GUI_WIDGETS_ICON_BROWSER_H_
#define YAZE_APP_GUI_WIDGETS_ICON_BROWSER_H_

namespace yaze {
namespace gui {

// Grid of Material Design icon buttons with a search field at the top.
// Draws the catalog from `kCommonIcons[]` (curated ~100-icon subset). A
// full-header codegen is a follow-up.
//
// Returns the selected glyph as a C string (the `ICON_MD_*` UTF-8 literal)
// when the user clicks a cell, or nullptr otherwise. Callers can paste the
// returned string directly into ImGui label strings:
//
//   if (const char* glyph = gui::IconBrowser("##icons")) {
//     selected_label_ = absl::StrCat(glyph, " Save");
//   }
//
// The returned pointer is valid for the lifetime of the process (it points
// into a static icon table). `label` participates in ImGui ID scoping.
const char* IconBrowser(const char* label);

namespace icon_browser_internal {

// Returns true when `search_key` matches every whitespace-separated term in
// `query` (case-insensitive substring). An empty query matches everything.
bool MatchesQuery(const char* search_key, const char* query);

}  // namespace icon_browser_internal

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_ICON_BROWSER_H_
