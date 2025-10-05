#ifndef YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_ENHANCED_H_
#define YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_ENHANCED_H_

#include "app/editor/code/memory_editor.h"
#include "app/gui/icons.h"

namespace yaze {
namespace editor {

struct MemoryBookmark {
  uint32_t address;
  std::string name;
  std::string description;
};

class EnhancedMemoryEditor : public MemoryEditorWithDiffChecker {
 public:
  explicit EnhancedMemoryEditor(Rom* rom) : MemoryEditorWithDiffChecker(rom) {}
  
  void UpdateEnhanced(bool& show);
  
 private:
  void DrawToolbar();
  void DrawJumpToAddress();
  void DrawSearch();
  void DrawBookmarks();
  
  std::vector<MemoryBookmark> bookmarks_;
  char jump_address_[16] = "0x000000";
  char search_pattern_[256] = "";
  uint32_t current_address_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_ENHANCED_H_
