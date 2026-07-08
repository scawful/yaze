#include "app/editor/system/workspace/editor_activator.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/shell/feedback/toast_manager.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(EditorActivatorTest, RejectsInvalidOverworldMapJumpTargets) {
  EditorSet editor_set(nullptr, nullptr, nullptr, 0, nullptr);
  auto* overworld_editor = editor_set.GetOverworldEditor();
  ASSERT_NE(overworld_editor, nullptr);

  ToastManager toast_manager;
  EditorActivator activator;
  EditorActivator::Dependencies deps;
  deps.toast_manager = &toast_manager;
  deps.get_current_editor_set = [&editor_set]() {
    return &editor_set;
  };
  activator.Initialize(deps);

  activator.JumpToOverworldMap(0x103);

  EXPECT_EQ(overworld_editor->current_map_id(), 0);
  ASSERT_FALSE(toast_manager.GetHistory().empty());
  EXPECT_EQ(toast_manager.GetHistory().front().type, ToastType::kWarning);
  EXPECT_EQ(toast_manager.GetHistory().front().message,
            "Invalid overworld map ID: 259");
}

TEST(EditorActivatorTest, AppliesValidOverworldMapJumpTargets) {
  EditorSet editor_set(nullptr, nullptr, nullptr, 0, nullptr);
  auto* overworld_editor = editor_set.GetOverworldEditor();
  ASSERT_NE(overworld_editor, nullptr);

  ToastManager toast_manager;
  EditorActivator activator;
  EditorActivator::Dependencies deps;
  deps.toast_manager = &toast_manager;
  deps.get_current_editor_set = [&editor_set]() {
    return &editor_set;
  };
  activator.Initialize(deps);

  activator.JumpToOverworldMap(0x7F);

  EXPECT_EQ(overworld_editor->current_map_id(), 0x7F);
  EXPECT_TRUE(toast_manager.GetHistory().empty());
}

}  // namespace
}  // namespace yaze::editor
