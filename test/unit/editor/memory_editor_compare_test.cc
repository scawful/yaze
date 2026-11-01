#include <gtest/gtest.h>

#include "app/editor/code/memory_editor.h"

namespace yaze::editor {
namespace {

TEST(MemoryEditorCompareTest, CancelledDialogPathIsIgnored) {
  MemoryEditor editor;
  EXPECT_FALSE(editor.LoadComparisonRom(""));
  EXPECT_FALSE(editor.show_compare_rom());
}

}  // namespace
}  // namespace yaze::editor
