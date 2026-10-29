#include "app/editor/overworld/tile16/tile16_editor_action_state.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/registry/undo_action.h"
#include "app/gui/automation/widget_id_registry.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class Tile16EditorActionRowTestPeer {
 public:
  static absl::Status Draw(Tile16Editor& editor, bool* debug, bool* advanced) {
    return editor.DrawCompactActionStatusRow(debug, advanced);
  }
};

namespace {

class Tile16EditorActionStateFrameTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(960, 600);
    io.DeltaTime = 1.0f / 60;
    io.ConfigInputTrickleEventQueue = false;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    // Only ImGui draw lists are produced; this sentinel reaches no renderer.
    io.Fonts->SetTexID((ImTextureID)0x4321);
    editor_.BindDocument(&definitions_, &history_, [&] { ++dispatches_; });
  }

  void TearDown() override {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }

  void Frame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(720, 220), ImGuiCond_Always);
    ImGui::Begin("Tile16ActionRowHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    const auto disabled_depth = context_->DisabledStackSize;
    const auto item_flags_depth = context_->ItemFlagsStack.Size;
    const float alpha = ImGui::GetStyle().Alpha;
    const auto status =
        Tile16EditorActionRowTestPeer::Draw(editor_, &debug_, &advanced_);
    EXPECT_TRUE(status.ok()) << status;
    EXPECT_EQ(context_->DisabledStackSize, disabled_depth);
    EXPECT_EQ(context_->ItemFlagsStack.Size, item_flags_depth);
    EXPECT_FLOAT_EQ(ImGui::GetStyle().Alpha, alpha);
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> FindButton(
      const char* label) {
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.type == "button" &&
          widget.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        return widget;
    }
    return std::nullopt;
  }

  void Click(const char* label, bool expected_enabled) {
    const auto target = FindButton(label);
    ASSERT_TRUE(target) << label;
    ASSERT_TRUE(target->bounds.valid) << label;
    EXPECT_EQ(target->enabled, expected_enabled) << label;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((target->bounds.min_x + target->bounds.max_x) / 2,
                        (target->bounds.min_y + target->bounds.max_y) / 2);
    Frame();
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
    Frame();
  }

  class SharedValueAction final : public UndoAction {
   public:
    explicit SharedValueAction(Tile16EditorActionStateFrameTest* owner)
        : owner_(owner) {}
    absl::Status Undo() override {
      owner_->value_ = 0;
      ++owner_->undos_;
      return absl::OkStatus();
    }
    absl::Status Redo() override {
      owner_->value_ = 1;
      ++owner_->redos_;
      return absl::OkStatus();
    }
    std::string Description() const override { return "Shared document edit"; }

   private:
    Tile16EditorActionStateFrameTest* owner_;
  };

  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  Rom rom_;
  gfx::Tilemap blockset_;
  Tile16Editor editor_{&rom_, &blockset_};
  std::vector<gfx::Tile16> definitions_;
  UndoManager history_;
  bool debug_ = false;
  bool advanced_ = false;
  int value_ = 1;
  int dispatches_ = 0;
  int undos_ = 0;
  int redos_ = 0;
};

TEST_F(Tile16EditorActionStateFrameTest,
       LastSharedUndoRedoMouseClicksKeepDisabledStackBalanced) {
  history_.Push(std::make_unique<SharedValueAction>(this));
  Frame();
  Frame();
  ASSERT_NO_FATAL_FAILURE(Click("Redo", false));
  EXPECT_EQ(dispatches_, 0);
  EXPECT_EQ(value_, 1);

  // This release frame consumes the last Undo action. The production row must
  // close the scope it opened before CanUndo changed during the button call.
  ASSERT_NO_FATAL_FAILURE(Click("Undo", true));
  EXPECT_EQ(value_, 0);
  EXPECT_EQ(undos_, 1);
  EXPECT_EQ(dispatches_, 1);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_EQ(history_.RedoStackSize(), 1u);
  ASSERT_NO_FATAL_FAILURE(Click("Undo", false));
  EXPECT_EQ(undos_, 1);
  EXPECT_EQ(dispatches_, 1);

  ASSERT_NO_FATAL_FAILURE(Click("Redo", true));
  EXPECT_EQ(value_, 1);
  EXPECT_EQ(redos_, 1);
  EXPECT_EQ(dispatches_, 2);
  EXPECT_EQ(history_.UndoStackSize(), 1u);
  EXPECT_EQ(history_.RedoStackSize(), 0u);
  ASSERT_NO_FATAL_FAILURE(Click("Redo", false));
  EXPECT_EQ(redos_, 1);
  EXPECT_EQ(dispatches_, 2);
}

TEST_F(Tile16EditorActionStateFrameTest,
       EmptySharedHistoryMouseClicksDoNotDispatchDisabledActions) {
  Frame();
  Frame();
  ASSERT_NO_FATAL_FAILURE(Click("Undo", false));
  ASSERT_NO_FATAL_FAILURE(Click("Redo", false));
  EXPECT_EQ(dispatches_, 0);
  EXPECT_EQ(undos_, 0);
  EXPECT_EQ(redos_, 0);
  EXPECT_EQ(value_, 1);
  EXPECT_EQ(history_.UndoStackSize(), 0u);
  EXPECT_EQ(history_.RedoStackSize(), 0u);
}

TEST(Tile16EditorActionStateTest, NoPendingNoUndoDisablesStagedActions) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/false,
      /*current_tile_pending=*/false,
      /*can_undo=*/false,
      /*can_redo=*/false);

  EXPECT_FALSE(state.can_write_pending);
  EXPECT_FALSE(state.can_discard_all);
  EXPECT_FALSE(state.can_discard_current);
  EXPECT_FALSE(state.can_undo);
  EXPECT_FALSE(state.can_redo);
}

TEST(Tile16EditorActionStateTest,
     PendingQueueEnablesWriteAndDiscardAllButNotCurrent) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/true,
      /*current_tile_pending=*/false,
      /*can_undo=*/true,
      /*can_redo=*/false);

  EXPECT_TRUE(state.can_write_pending);
  EXPECT_TRUE(state.can_discard_all);
  EXPECT_FALSE(state.can_discard_current);
  EXPECT_TRUE(state.can_undo);
  EXPECT_FALSE(state.can_redo);
}

TEST(Tile16EditorActionStateTest,
     CurrentTilePendingEnablesCurrentDiscardIndependently) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/false,
      /*current_tile_pending=*/true,
      /*can_undo=*/true,
      /*can_redo=*/false);

  EXPECT_FALSE(state.can_write_pending);
  EXPECT_FALSE(state.can_discard_all);
  EXPECT_TRUE(state.can_discard_current);
  EXPECT_TRUE(state.can_undo);
  EXPECT_FALSE(state.can_redo);
}

TEST(Tile16EditorActionStateTest, FullPendingStateEnablesAllRelevantActions) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/true,
      /*current_tile_pending=*/true,
      /*can_undo=*/false,
      /*can_redo=*/true);

  EXPECT_TRUE(state.can_write_pending);
  EXPECT_TRUE(state.can_discard_all);
  EXPECT_TRUE(state.can_discard_current);
  EXPECT_FALSE(state.can_undo);
  EXPECT_TRUE(state.can_redo);
}

TEST(Tile16EditorActionStateTest, CompactActionColumnsPreferReadableLabels) {
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(920.0f), 7);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(760.0f), 5);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(460.0f), 3);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(320.0f), 2);
  EXPECT_EQ(ComputeTile16CompactActionColumnCount(220.0f), 1);
}

TEST(Tile16EditorActionStateTest, ActionRowCountHandlesNarrowLayouts) {
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/7,
                                        /*column_count=*/7),
            1);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/7,
                                        /*column_count=*/5),
            2);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/7,
                                        /*column_count=*/2),
            4);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/0,
                                        /*column_count=*/2),
            0);
  EXPECT_EQ(ComputeTile16ActionRowCount(/*action_count=*/3,
                                        /*column_count=*/0),
            3);
}

TEST(Tile16EditorActionStateTest, PaletteButtonSizeStaysUsableWhenNarrow) {
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(260.0f), 32.0f);
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(144.0f), 32.0f);
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(80.0f), 24.0f);
  EXPECT_FLOAT_EQ(ComputeTile16PaletteButtonSize(0.0f), 24.0f);
}

}  // namespace
}  // namespace yaze::editor
