#ifndef YAZE_APP_GUI_CORE_COMMON_ICONS_H_
#define YAZE_APP_GUI_CORE_COMMON_ICONS_H_

namespace yaze {
namespace gui {

// A curated Material Design icon from `icons.h`. The browser widget walks
// this table instead of the full ~2,100-icon header so the grid stays
// responsive and the set is easy to reason about. Extensions to the
// catalog are welcome; full codegen over the header is a follow-up.
struct CommonIcon {
  const char* macro_name;  // e.g. "ICON_MD_SAVE"
  const char* glyph;       // UTF-8 sequence (the ICON_MD_* literal)
  const char* category;    // "actions", "navigation", ...
  const char* search_key;  // space-separated keywords for fuzzy filtering
};

// Null-terminated (glyph == nullptr sentinel) for range-style iteration,
// with kCommonIconCount available for index-style iteration.
extern const CommonIcon kCommonIcons[];
extern const int kCommonIconCount;

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_COMMON_ICONS_H_
