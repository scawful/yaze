#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/shortcut_configurator.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "imgui/imgui.h"

namespace yaze::editor {

class DungeonEditorV2ShortcutTestPeer {
 public:
  static bool HasQueuedDelete(const DungeonEditorV2& editor) {
    return editor.room_canvas_delete_shortcut_frame_.has_value();
  }

  static std::optional<int> QueuedDeleteFrame(const DungeonEditorV2& editor) {
    return editor.room_canvas_delete_shortcut_frame_;
  }

  static bool ConsumeQueuedDelete(DungeonEditorV2& editor,
                                  DungeonCanvasViewer& viewer) {
    return editor.ConsumeRoomCanvasDeleteShortcut(viewer);
  }

  static void ExpireStaleDelete(DungeonEditorV2& editor) {
    editor.ExpireStaleRoomCanvasDeleteShortcut();
  }
};

namespace {

class ShortcutConfiguratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1400.0f, 1000.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    renderer_ = std::make_unique<gfx::NullRenderer>();
    editor_manager_ = std::make_unique<EditorManager>();
    editor_manager_->Initialize(renderer_.get(), "");

    auto* window_manager = editor_manager_->GetWindowManager();
    window_manager->RegisterSession(0);
    window_manager->SetActiveSession(0);

    test_window_visible_ = false;
    WindowDescriptor descriptor;
    descriptor.card_id = "test.demo";
    descriptor.display_name = "Demo";
    descriptor.icon = "ICON_DEMO";
    descriptor.category = "Dungeon";
    descriptor.shortcut_hint = "Ctrl+Alt+M";
    descriptor.visibility_flag = &test_window_visible_;
    descriptor.priority = 1;
    window_manager->RegisterWindow(0, descriptor);
  }

  void TearDown() override {
    editor_manager_.reset();
    renderer_.reset();
    if (imgui_context_) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  ShortcutManager ConfigureShortcuts() {
    ShortcutManager shortcuts;
    ShortcutDependencies deps;
    deps.editor_manager = editor_manager_.get();
    deps.window_manager = editor_manager_->GetWindowManager();
    ConfigureEditorShortcuts(deps, &shortcuts);
    return shortcuts;
  }

  void RegisterSecondSessionDemoWindow() {
    auto* window_manager = editor_manager_->GetWindowManager();
    window_manager->RegisterSession(1);

    WindowDescriptor descriptor;
    descriptor.card_id = "test.demo";
    descriptor.display_name = "Demo";
    descriptor.icon = "ICON_DEMO";
    descriptor.category = "Dungeon";
    descriptor.shortcut_hint = "Ctrl+Alt+M";
    descriptor.visibility_flag = &second_window_visible_;
    descriptor.priority = 1;
    window_manager->RegisterWindow(1, descriptor);
  }

  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> editor_manager_;
  ImGuiContext* imgui_context_ = nullptr;
  bool test_window_visible_ = false;
  bool second_window_visible_ = false;
};

TEST_F(ShortcutConfiguratorTest, RegistersWindowBrowserAndDrawerAliases) {
  ShortcutManager shortcuts = ConfigureShortcuts();

  const Shortcut* panel_browser = shortcuts.FindShortcut("Panel Browser");
  const Shortcut* window_browser = shortcuts.FindShortcut("Window Browser");
  ASSERT_NE(panel_browser, nullptr);
  ASSERT_NE(window_browser, nullptr);
  EXPECT_EQ(window_browser->keys, panel_browser->keys);

  const Shortcut* panel_browser_alt =
      shortcuts.FindShortcut("Panel Browser (Alt)");
  const Shortcut* window_browser_alt =
      shortcuts.FindShortcut("Window Browser (Alt)");
  ASSERT_NE(panel_browser_alt, nullptr);
  ASSERT_NE(window_browser_alt, nullptr);
  EXPECT_EQ(window_browser_alt->keys, panel_browser_alt->keys);

  EXPECT_NE(shortcuts.FindShortcut("View: Previous Right Drawer"), nullptr);
  EXPECT_NE(shortcuts.FindShortcut("View: Next Right Drawer"), nullptr);
  EXPECT_NE(shortcuts.FindShortcut("View: Toggle Project Drawer"), nullptr);
  EXPECT_NE(shortcuts.FindShortcut("View: Show Window Browser"), nullptr);
}

TEST_F(ShortcutConfiguratorTest,
       RegistersWindowCommandsAndExecutesDrawerAlias) {
  ShortcutManager shortcuts = ConfigureShortcuts();

  const Shortcut* open_demo_window =
      shortcuts.FindShortcut("View: Open Demo Window");
  ASSERT_NE(open_demo_window, nullptr);
  EXPECT_TRUE(open_demo_window->keys.empty());

  const Shortcut* toggle_demo_window =
      shortcuts.FindShortcut("View: Toggle Demo Window");
  ASSERT_NE(toggle_demo_window, nullptr);
  EXPECT_TRUE(toggle_demo_window->keys.empty());

  auto* drawers = editor_manager_->right_drawer_manager();
  ASSERT_NE(drawers, nullptr);
  EXPECT_EQ(drawers->GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);

  shortcuts.ExecuteShortcut("View: Toggle Project Drawer");
  EXPECT_EQ(drawers->GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProject);

  shortcuts.ExecuteShortcut("View: Open Demo Window");
  EXPECT_TRUE(test_window_visible_);

  shortcuts.ExecuteShortcut("View: Toggle Demo Window");
  EXPECT_FALSE(test_window_visible_);
}

TEST_F(ShortcutConfiguratorTest,
       WindowActionsRouteToActiveSessionAtInvokeTime) {
  RegisterSecondSessionDemoWindow();
  ShortcutManager shortcuts = ConfigureShortcuts();

  ShortcutDependencies deps;
  deps.window_manager = editor_manager_->GetWindowManager();
  ConfigurePanelShortcuts(deps, &shortcuts);

  auto* window_manager = editor_manager_->GetWindowManager();
  window_manager->SetActiveSession(1);

  shortcuts.ExecuteShortcut("Show Dungeon Panels");
  EXPECT_FALSE(test_window_visible_);
  EXPECT_TRUE(second_window_visible_);

  second_window_visible_ = false;
  shortcuts.ExecuteShortcut("View: Open Demo Window");
  EXPECT_FALSE(test_window_visible_);
  EXPECT_TRUE(second_window_visible_);

  shortcuts.ExecuteShortcut("view.toggle.test.demo");
  EXPECT_FALSE(test_window_visible_);
  EXPECT_FALSE(second_window_visible_);
}

TEST_F(ShortcutConfiguratorTest,
       DungeonDeleteRequestExpiresWhenItsFramePassesWithoutCanvasOwnership) {
  DungeonEditorV2 dungeon_editor;

  ImGui::NewFrame();
  dungeon_editor.QueueRoomCanvasDeleteShortcut();
  const int queued_frame = ImGui::GetFrameCount();
  EXPECT_TRUE(DungeonEditorV2ShortcutTestPeer::HasQueuedDelete(dungeon_editor));
  EXPECT_EQ(DungeonEditorV2ShortcutTestPeer::QueuedDeleteFrame(dungeon_editor),
            queued_frame);
  EXPECT_TRUE(dungeon_editor.Update().ok());
  EXPECT_TRUE(DungeonEditorV2ShortcutTestPeer::HasQueuedDelete(dungeon_editor));
  ImGui::Render();

  ImGui::NewFrame();
  EXPECT_TRUE(dungeon_editor.Update().ok());
  EXPECT_FALSE(
      DungeonEditorV2ShortcutTestPeer::HasQueuedDelete(dungeon_editor));
  ImGui::Render();
}

TEST_F(ShortcutConfiguratorTest,
       DungeonDeleteWaitsForDetachedWorkbenchCanvasInTheSameFrame) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonEditorV2 dungeon_editor;
  DungeonCanvasViewer viewer(&rom);
  DungeonRoomSelector room_selector(&rom);
  int current_room_id = 0;
  const std::deque<int> recent_rooms{current_room_id};
  DungeonWorkbenchContent workbench(
      &room_selector, &current_room_id, [](int) {},
      [](int, RoomSelectionIntent) {}, [](int) {}, []() {},
      [&viewer]() { return &viewer; },
      [](int) -> DungeonCanvasViewer* { return nullptr; },
      [&recent_rooms]() -> const std::deque<int>& { return recent_rooms; },
      [](int) {}, [](bool) {}, &rom);

  bool delete_consumed = false;
  workbench.SetPrimaryCanvasDrawnCallback(
      [&](DungeonCanvasViewer& drawn_viewer) {
        delete_consumed = DungeonEditorV2ShortcutTestPeer::ConsumeQueuedDelete(
                              dungeon_editor, drawn_viewer) ||
                          delete_consumed;
      });

  auto draw_workbench = [&]() {
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1200.0f, 850.0f), ImGuiCond_Always);
    ImGui::Begin("##DetachedWorkbenchShortcutHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    workbench.Draw(nullptr);
    ImGui::End();
  };

  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(-1000.0f, -1000.0f);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  ImGui::NewFrame();
  draw_workbench();
  const ImVec2 canvas_origin = viewer.canvas().zero_point();
  const ImVec2 canvas_size = viewer.canvas().canvas_size();
  ASSERT_GT(canvas_size.x, 0.0f);
  ASSERT_GT(canvas_size.y, 0.0f);
  ImGui::Render();

  // Give the detached canvas shortcut ownership in the preceding frame.
  io.AddMousePosEvent(canvas_origin.x + canvas_size.x * 0.5f,
                      canvas_origin.y + canvas_size.y * 0.5f);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  ImGui::NewFrame();
  draw_workbench();
  EXPECT_TRUE(viewer.CanHandleRoomCanvasShortcut());
  ImGui::Render();

  // ShortcutManager queues before DungeonEditorV2::Update. The detached
  // Workbench canvas draws later, so the request must survive Update and be
  // consumed only after that canvas refreshes its ownership for this frame.
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  ImGui::NewFrame();
  dungeon_editor.QueueRoomCanvasDeleteShortcut();
  const int queued_frame = ImGui::GetFrameCount();
  DungeonEditorV2ShortcutTestPeer::ExpireStaleDelete(dungeon_editor);
  EXPECT_EQ(DungeonEditorV2ShortcutTestPeer::QueuedDeleteFrame(dungeon_editor),
            queued_frame);
  EXPECT_FALSE(delete_consumed);

  draw_workbench();
  EXPECT_TRUE(delete_consumed);
  EXPECT_FALSE(
      DungeonEditorV2ShortcutTestPeer::HasQueuedDelete(dungeon_editor));
  ImGui::Render();
}

}  // namespace
}  // namespace yaze::editor
