#include "app/editor/dungeon/inspectors/dungeon_room_swap_dialog.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/cord.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui_internal.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {
namespace {

class DungeonRoomSwapDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    rooms_.reserve(3);
    for (int room_id = 0; room_id < 3; ++room_id) {
      rooms_.emplace_back(room_id, &rom_);
      rooms_.back().SetLoaded(true);
      rooms_.back().SetTileObjects(
          {zelda3::RoomObject(room_id, 4 + room_id, 4, 0, 0)});
    }
    rooms_[1].AddTileObject(zelda3::RoomObject(0x002, 8, 8, 0, 0));
    dialog_.SetCallbacks(
        [this](int first, int second, const DungeonRoomTransferOptions& options)
            -> absl::StatusOr<DungeonRoomSwapPlan> {
          ++previews_;
          last_first_ = first;
          last_second_ = second;
          last_options_ = options;
          if (shared_header_ && (options.domains & kTransferMetadata)) {
            auto status =
                absl::FailedPreconditionError("Shared room properties");
            status.SetPayload(kRoomTransferSharedHeaderPayload,
                              absl::Cord("1"));
            return status;
          }
          auto a = PlanDungeonRoomTransfer(
              rooms_[first], CaptureDungeonRoomDocument(rooms_[second]),
              options);
          if (!a.ok())
            return a.status();
          auto b = PlanDungeonRoomTransfer(
              rooms_[second], CaptureDungeonRoomDocument(rooms_[first]),
              options);
          if (!b.ok())
            return b.status();
          return DungeonRoomSwapPlan{*std::move(a), *std::move(b)};
        },
        [this](const DungeonRoomSwapPlan& plan) {
          ++attempts_;
          if (reject_apply_) {
            return absl::FailedPreconditionError("Test apply rejection");
          }
          ApplyDungeonRoomDocument(rooms_[plan.first.target_room_id],
                                   plan.first.after);
          ApplyDungeonRoomDocument(rooms_[plan.second.target_room_id],
                                   plan.second.after);
          return absl::OkStatus();
        });
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 1000);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000, -1000);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
    gui::WidgetIdRegistry::Instance().Clear();
  }

  void DrawFrame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Always);
    ImGui::Begin("RoomSwapHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    dialog_.Draw();
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
    EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* name) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(name);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (path.find("Dungeon/RoomSwap/") != std::string::npos &&
          widget.label == normalized) {
        return widget;
      }
    }
    return std::nullopt;
  }

  void Click(const char* name) {
    const auto widget = Widget(name);
    ASSERT_TRUE(widget) << name;
    ASSERT_TRUE(widget->enabled) << name;
    ASSERT_TRUE(widget->visible) << name;
    ASSERT_TRUE(widget->bounds.valid) << name;
    const auto bounds = widget->bounds;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((bounds.min_x + bounds.max_x) / 2,
                        (bounds.min_y + bounds.max_y) / 2);
    DrawFrame();
    io.AddMouseButtonEvent(0, true);
    DrawFrame();
    io.AddMouseButtonEvent(0, false);
    DrawFrame();
    DrawFrame();
  }

  void OpenDialog(int first = 0, int second = 1) {
    dialog_.Open(first, second);
    DrawFrame();
    DrawFrame();
  }

  Rom rom_;
  std::vector<zelda3::Room> rooms_;
  DungeonRoomSwapDialog dialog_;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  std::string logged_text_;
  int previews_ = 0;
  int attempts_ = 0;
  int last_first_ = -1;
  int last_second_ = -1;
  DungeonRoomTransferOptions last_options_;
  bool shared_header_ = false;
  bool reject_apply_ = false;
};

TEST_F(DungeonRoomSwapDialogTest, OpenShowsRoomsWithoutPreviewing) {
  EXPECT_FALSE(dialog_.is_open());
  OpenDialog(0, 1);
  EXPECT_TRUE(dialog_.is_open());
  EXPECT_EQ(dialog_.first_room_id(), 0);
  EXPECT_EQ(dialog_.second_room_id(), 1);
  EXPECT_EQ(previews_, 0);
  ASSERT_TRUE(Widget("SwapFirstRoom"));
  ASSERT_TRUE(Widget("SwapSecondRoom"));
  EXPECT_FALSE(Widget("SwapApply"));
}

TEST_F(DungeonRoomSwapDialogTest, PreviewThenApplyExchangesBothRoomsOnce) {
  OpenDialog(0, 1);
  Click("SwapPreview");
  ASSERT_TRUE(dialog_.preview());
  EXPECT_EQ(previews_, 1);
  EXPECT_EQ(last_first_, 0);
  EXPECT_EQ(last_second_, 1);
  EXPECT_EQ(last_options_.domains, kTransferCore);
  EXPECT_FALSE(last_options_.copy_destinations);
  EXPECT_EQ(attempts_, 0);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1u);
  EXPECT_NE(logged_text_.find("receives room"), std::string::npos);

  Click("SwapApply");
  EXPECT_EQ(attempts_, 1);
  EXPECT_FALSE(dialog_.preview());
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 2u);
  EXPECT_EQ(rooms_[1].GetTileObjects().size(), 1u);
  EXPECT_EQ(rooms_[1].GetTileObjects().front().id_, 0);
  EXPECT_FALSE(Widget("SwapApply"));
  EXPECT_NE(dialog_.status().find("Undo restores both"), std::string::npos);
}

TEST_F(DungeonRoomSwapDialogTest, ExchangeAndDomainChangesRequireNewPreview) {
  OpenDialog(0, 1);
  Click("SwapPreview");
  ASSERT_TRUE(dialog_.preview());
  Click("SwapExchangeRooms");
  EXPECT_FALSE(dialog_.preview());
  EXPECT_EQ(dialog_.first_room_id(), 1);
  EXPECT_EQ(dialog_.second_room_id(), 0);
  Click("SwapPreview");
  ASSERT_TRUE(dialog_.preview());
  EXPECT_EQ(last_first_, 1);
  Click("TransferSprites");
  EXPECT_FALSE(dialog_.preview());
  EXPECT_EQ(dialog_.domains(), kTransferCore & ~kTransferSprites);
  Click("SwapPreview");
  EXPECT_EQ(last_options_.domains, kTransferCore & ~kTransferSprites);
  EXPECT_EQ(previews_, 3);
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonRoomSwapDialogTest, ReopeningForDifferentRoomsDropsPreview) {
  OpenDialog(0, 1);
  Click("SwapPreview");
  ASSERT_TRUE(dialog_.preview());
  OpenDialog(0, 1);
  EXPECT_TRUE(dialog_.preview());
  OpenDialog(2, 1);
  EXPECT_FALSE(dialog_.preview());
  EXPECT_EQ(dialog_.first_room_id(), 2);
}

TEST_F(DungeonRoomSwapDialogTest, SameRoomCannotBePreviewed) {
  OpenDialog(1, 1);
  ASSERT_TRUE(Widget("SwapPreview"));
  EXPECT_FALSE(Widget("SwapPreview")->enabled);
  EXPECT_NE(logged_text_.find("two different rooms"), std::string::npos);
  EXPECT_EQ(previews_, 0);
}

TEST_F(DungeonRoomSwapDialogTest, SharedHeaderOffersPreviewWithoutProperties) {
  shared_header_ = true;
  OpenDialog(0, 1);
  Click("TransferDestinations");
  Click("SwapPreview");
  EXPECT_FALSE(dialog_.preview());
  EXPECT_NE(logged_text_.find("Shared room properties"), std::string::npos);
  Click("SwapPreviewWithoutProperties");
  ASSERT_TRUE(dialog_.preview());
  EXPECT_EQ(dialog_.domains(), kTransferCore & ~kTransferMetadata);
  EXPECT_FALSE(last_options_.copy_destinations);
  // Apply is drawn above the retry button, so it appears a frame later.
  DrawFrame();
  EXPECT_FALSE(Widget("SwapPreviewWithoutProperties"));
  Click("SwapApply");
  EXPECT_EQ(attempts_, 1);
}

TEST_F(DungeonRoomSwapDialogTest, ApplyRejectionLeavesRoomsAndNeedsNewPreview) {
  reject_apply_ = true;
  OpenDialog(0, 1);
  Click("SwapPreview");
  Click("SwapApply");
  EXPECT_EQ(attempts_, 1);
  EXPECT_FALSE(dialog_.preview());
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1u);
  EXPECT_NE(dialog_.error().find("Test apply rejection"), std::string::npos);
  EXPECT_TRUE(dialog_.status().empty());
}

TEST_F(DungeonRoomSwapDialogTest, ReadOnlyDisablesPreviewAndRoomInputs) {
  dialog_.SetReadOnly(true);
  OpenDialog(0, 1);
  for (const char* name : {"SwapPreview", "SwapFirstRoom", "SwapExchangeRooms",
                           "TransferObjects"}) {
    ASSERT_TRUE(Widget(name)) << name;
    EXPECT_FALSE(Widget(name)->enabled) << name;
  }
  EXPECT_TRUE(Widget("SwapClose")->enabled);
}

TEST_F(DungeonRoomSwapDialogTest, CloseButtonDismissesModal) {
  OpenDialog(0, 1);
  Click("SwapClose");
  EXPECT_FALSE(dialog_.is_open());
  EXPECT_EQ(previews_, 0);
}

}  // namespace
}  // namespace yaze::editor
