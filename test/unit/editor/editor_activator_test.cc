#include "app/editor/system/workspace/editor_activator.h"

#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <gtest/gtest.h>

#include <vector>

namespace yaze::editor {
namespace {

class EditorActivatorLayoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();

    window_manager_.RegisterSession(0);
    window_manager_.SetActiveSession(0);
    layout_manager_.SetWindowManager(&window_manager_);

    EditorActivator::Dependencies deps;
    deps.window_manager = &window_manager_;
    deps.layout_manager = &layout_manager_;
    deps.get_current_editor_set = [this]() {
      return &editor_set_;
    };
    deps.get_current_session_id = [this]() {
      return window_manager_.GetActiveSessionId();
    };
    deps.queue_deferred_action = [this](std::function<void()> action) {
      if (defer_layout_actions_) {
        deferred_layout_actions_.push_back(std::move(action));
      } else {
        action();
      }
    };
    activator_.Initialize(deps);
  }

  void TearDown() override {
    ImGui::DockBuilderRemoveNode(ImGui::GetID("MainDockSpace"));
    ImGui::EndFrame();
    ImGui::DestroyContext(imgui_context_);
    imgui_context_ = nullptr;
  }

  void RegisterDungeonPanel(size_t session_id, const char* panel_id,
                            bool* visible) {
    RegisterPanel(session_id, panel_id, "Dungeon", visible);
  }

  void RegisterPanel(size_t session_id, const char* panel_id,
                     const char* category, bool* visible) {
    WindowDescriptor descriptor{};
    descriptor.card_id = panel_id;
    descriptor.display_name = panel_id;
    descriptor.icon = "ICON_MD_ACCOUNT_TREE";
    descriptor.category = category;
    descriptor.priority = 1;
    descriptor.visibility_flag = visible;
    window_manager_.RegisterWindow(session_id, descriptor);
  }

  ImGuiContext* imgui_context_ = nullptr;
  bool session_zero_workbench_visible_ = false;
  bool session_one_workbench_visible_ = false;
  bool session_one_object_selector_visible_ = false;
  bool overworld_canvas_visible_ = false;
  bool overworld_tile_selector_visible_ = false;
  bool overworld_properties_visible_ = false;
  bool defer_layout_actions_ = false;
  std::vector<std::function<void()>> deferred_layout_actions_;
  WorkspaceWindowManager window_manager_;
  LayoutManager layout_manager_;
  EditorSet editor_set_{nullptr, nullptr, nullptr, 0, nullptr};
  EditorActivator activator_;
};

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

TEST_F(EditorActivatorLayoutTest,
       RepeatedDungeonActivationRefreshesLazyDockingForActiveSession) {
  RegisterDungeonPanel(0, LayoutPresets::Panels::kDungeonWorkbench,
                       &session_zero_workbench_visible_);
  Editor* dungeon_editor = editor_set_.GetEditor(EditorType::kDungeon);
  ASSERT_NE(dungeon_editor, nullptr);

  activator_.SwitchToEditor(EditorType::kDungeon, true);
  ASSERT_TRUE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));

  window_manager_.RegisterSession(1);
  RegisterDungeonPanel(1, LayoutPresets::Panels::kDungeonWorkbench,
                       &session_one_workbench_visible_);
  RegisterDungeonPanel(1, LayoutPresets::Panels::kDungeonObjectSelector,
                       &session_one_object_selector_visible_);
  window_manager_.SetActiveSession(1);

  activator_.SwitchToEditor(EditorType::kDungeon, true);

  EXPECT_TRUE(layout_manager_.DockDefaultPositionOnFirstOpen(
      1, LayoutPresets::Panels::kDungeonObjectSelector));
}

TEST_F(EditorActivatorLayoutTest,
       DeactivatingForegroundEditorRefreshesFallbackLayoutContext) {
  RegisterDungeonPanel(0, LayoutPresets::Panels::kDungeonWorkbench,
                       &session_zero_workbench_visible_);
  RegisterPanel(0, LayoutPresets::Panels::kOverworldCanvas, "Overworld",
                &overworld_canvas_visible_);
  RegisterPanel(0, LayoutPresets::Panels::kOverworldTile16Selector, "Overworld",
                &overworld_tile_selector_visible_);
  RegisterPanel(0, LayoutPresets::Panels::kOverworldMapProperties, "Overworld",
                &overworld_properties_visible_);

  auto* dungeon_editor = editor_set_.GetDungeonEditor();
  auto* overworld_editor = editor_set_.GetOverworldEditor();
  ASSERT_NE(dungeon_editor, nullptr);
  ASSERT_NE(overworld_editor, nullptr);

  activator_.SwitchToEditor(EditorType::kDungeon, true);
  activator_.SwitchToEditor(EditorType::kOverworld, true);
  ASSERT_EQ(window_manager_.GetActiveCategory(), "Overworld");

  window_manager_.RegisterSession(1);
  RegisterDungeonPanel(1, LayoutPresets::Panels::kDungeonWorkbench,
                       &session_one_workbench_visible_);
  RegisterDungeonPanel(1, LayoutPresets::Panels::kDungeonObjectSelector,
                       &session_one_object_selector_visible_);
  window_manager_.SetActiveSession(1);

  activator_.SwitchToEditor(EditorType::kOverworld);

  EXPECT_EQ(window_manager_.GetActiveCategory(), "Dungeon");
  EXPECT_TRUE(layout_manager_.DockDefaultPositionOnFirstOpen(
      1, LayoutPresets::Panels::kDungeonObjectSelector));
}

TEST_F(EditorActivatorLayoutTest,
       DeferredLayoutInitializationDoesNotCrossRomSessions) {
  RegisterDungeonPanel(0, LayoutPresets::Panels::kDungeonWorkbench,
                       &session_zero_workbench_visible_);
  ASSERT_NE(editor_set_.GetDungeonEditor(), nullptr);

  defer_layout_actions_ = true;
  activator_.SwitchToEditor(EditorType::kDungeon, true);
  ASSERT_EQ(deferred_layout_actions_.size(), 1u);
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));

  window_manager_.RegisterSession(1);
  window_manager_.SetActiveSession(1);
  auto action = std::move(deferred_layout_actions_.front());
  deferred_layout_actions_.clear();
  action();

  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));
}

TEST_F(EditorActivatorLayoutTest, DeferredEditorSwitchDoesNotCrossRomSessions) {
  RegisterDungeonPanel(0, LayoutPresets::Panels::kDungeonWorkbench,
                       &session_zero_workbench_visible_);
  Editor* dungeon_editor = editor_set_.GetEditor(EditorType::kDungeon);
  ASSERT_NE(dungeon_editor, nullptr);
  dungeon_editor->set_active(false);

  ImGui::EndFrame();
  defer_layout_actions_ = true;
  activator_.SwitchToEditor(EditorType::kDungeon, true);
  ASSERT_EQ(deferred_layout_actions_.size(), 1u);

  window_manager_.RegisterSession(1);
  window_manager_.SetActiveSession(1);
  ImGui::NewFrame();
  auto action = std::move(deferred_layout_actions_.front());
  deferred_layout_actions_.clear();
  action();

  EXPECT_FALSE(*dungeon_editor->active());
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));
}

}  // namespace
}  // namespace yaze::editor
