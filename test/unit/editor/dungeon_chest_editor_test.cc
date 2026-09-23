#include "app/editor/dungeon/inspectors/dungeon_chest_editor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

class DungeonChestEditorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    old_labels_ = zelda3::GetResourceLabels();
    zelda3::GetResourceLabels().SetProjectLabels(nullptr);
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    rooms_[0] = zelda3::Room(0, &rom_);
    rooms_[0].SetLoaded(true);
    rooms_[0].LoadChests();
    rooms_[0].GetChests().push_back({0x24, false});
    rooms_[0].GetTileObjects().emplace_back(0xF99, 2, 4, 0, 0);
    viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 0);
    viewer_.SetChestEditCallback(
        [this](int room_id, size_t index, uint8_t item, bool big) {
          if (reject_) {
            return absl::FailedPreconditionError("Test rejection");
          }
          auto& chest = rooms_[room_id].GetChests()[index];
          if (chest.id != item || chest.size != big) {
            chest = {item, big};
            ++changes_;
            last_room_id_ = room_id;
          }
          return absl::OkStatus();
        });
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 700);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000, -1000);
  }

  void TearDown() override {
    ImGui::DestroyContext();
    gui::WidgetIdRegistry::Instance().Clear();
    zelda3::GetResourceLabels() = old_labels_;
  }

  void DrawFrame(int room_id = 0, zelda3::Room* override_room = nullptr) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 640), ImGuiCond_Always);
    ImGui::Begin("ChestEditorHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    DrawDungeonChestEditor(
        room_id, override_room ? *override_room : rooms_[room_id], viewer_);
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    auto* window = ImGui::GetCurrentWindow();
    EXPECT_LE(window->DC.CursorMaxPos.x, window->WorkRect.Max.x + 1);
    ImGui::End();
    ImGui::Render();
    EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
    EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* name) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(name);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (path.starts_with("Dungeon/ChestEditor/") &&
          widget.label == normalized) {
        return widget;
      }
    }
    return std::nullopt;
  }

  void Click(const char* name) {
    const auto widget = Widget(name);
    ASSERT_TRUE(widget.has_value()) << name;
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

  Rom rom_;
  DungeonRoomStore rooms_{&rom_};
  DungeonCanvasViewer viewer_{&rom_};
  zelda3::ResourceLabelProvider old_labels_;
  std::string logged_text_;
  int changes_ = 0;
  int last_room_id_ = -1;
  bool reject_ = false;
};

TEST_F(DungeonChestEditorTest, NamesUseReceiptIdsRatherThanInventoryIndices) {
  EXPECT_EQ(GetDungeonChestItemLabel(0x00), "Fighter Sword");
  EXPECT_EQ(GetDungeonChestItemLabel(0x09), "Hammer");
  EXPECT_EQ(GetDungeonChestItemLabel(0x24), "Small Key");
  EXPECT_EQ(GetDungeonChestItemLabel(0x25), "Compass");
  EXPECT_EQ(GetDungeonChestItemLabel(0x32), "Big Key");
  EXPECT_EQ(GetDungeonChestItemLabel(0x33), "Map");
  EXPECT_EQ(GetDungeonChestItemLabel(0x34), "1 Rupee");
  EXPECT_EQ(GetDungeonChestItemLabel(0x3F), "Heart Container (Sanctuary)");
  EXPECT_EQ(GetDungeonChestItemLabel(0x40), "100 Rupees");
  EXPECT_EQ(GetDungeonChestItemLabel(0x4B), "Pegasus Boots");
  EXPECT_EQ(GetDungeonChestItemLabel(0xFF), "Unknown item FF");
}

TEST_F(DungeonChestEditorTest, ProjectSpecificItemLabelsOverrideVanilla) {
  zelda3::ResourceLabelProvider::ProjectLabels labels;
  labels["item"]["9"] = "Magic Hammer";
  labels["item"]["0xE0"] = "Custom Treasure";
  zelda3::GetResourceLabels().SetProjectLabels(&labels);
  EXPECT_EQ(GetDungeonChestItemLabel(0x09), "Magic Hammer");
  EXPECT_EQ(GetDungeonChestItemLabel(0xE0), "Custom Treasure");
  EXPECT_EQ(GetDungeonChestItemLabel(0x24), "Small Key");
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);
}

TEST_F(DungeonChestEditorTest, SearchSelectionCommitsCorrectByteOnce) {
  DrawFrame();
  DrawFrame();
  Click("ChestReward");
  // A late current reward must not scroll the search field out of view.
  Click("ChestSearch");
  ImGui::GetIO().AddInputCharactersUTF8("hammer");
  DrawFrame();
  DrawFrame();  // Apply the results child's scroll reset after filtering.
  ASSERT_STREQ(viewer_.chest_editor_state().search.data(), "hammer");
  ASSERT_TRUE(Widget("ChestReward09").has_value());
  EXPECT_FALSE(Widget("ChestReward08").has_value());
  Click("ChestReward09");
  EXPECT_EQ(rooms_[0].GetChests()[0].id, 0x09);
  EXPECT_EQ(changes_, 1);
  EXPECT_EQ(last_room_id_, 0);
  EXPECT_FALSE(rooms_[0].GetChests()[0].size);
  EXPECT_EQ(rooms_[0].GetTileObjects()[0].id_, 0xF99);
  EXPECT_FALSE(Widget("ChestSearch").has_value());
}

TEST_F(DungeonChestEditorTest, UnknownRewardSurvivesTypeEdit) {
  rooms_[0].GetChests()[0].id = 0xE0;
  DrawFrame();
  DrawFrame();
  EXPECT_NE(logged_text_.find("Unknown item E0"), std::string::npos);
  Click("ChestBig");
  EXPECT_EQ(rooms_[0].GetChests()[0].id, 0xE0);
  EXPECT_TRUE(rooms_[0].GetChests()[0].size);
  EXPECT_EQ(changes_, 1);
  EXPECT_EQ(rooms_[0].GetTileObjects()[0].id_, 0xF99);
  EXPECT_NE(logged_text_.find("record and object counts differ"),
            std::string::npos);
}

TEST_F(DungeonChestEditorTest, RejectedEditShowsErrorAndPreservesContents) {
  reject_ = true;
  DrawFrame();
  DrawFrame();
  Click("ChestBig");
  EXPECT_EQ(changes_, 0);
  EXPECT_FALSE(rooms_[0].GetChests()[0].size);
  EXPECT_NE(logged_text_.find("Edit not applied: Test rejection"),
            std::string::npos);
}

TEST_F(DungeonChestEditorTest, EmptyRoomHasNoRecordMutationControls) {
  rooms_[0].GetChests().clear();
  DrawFrame();
  DrawFrame();
  EXPECT_NE(logged_text_.find("No chest contents records"), std::string::npos);
  EXPECT_NE(logged_text_.find("Record and object counts differ"),
            std::string::npos);
  EXPECT_FALSE(Widget("ChestReward").has_value());
  EXPECT_FALSE(Widget("ChestBig").has_value());
  EXPECT_EQ(changes_, 0);
}

TEST_F(DungeonChestEditorTest, ForeignRoomReferenceCannotExposeEditableFields) {
  zelda3::Room foreign_room(0, &rom_);
  foreign_room.SetLoaded(true);
  foreign_room.LoadChests();
  foreign_room.GetChests().push_back({0xFF, true});
  DrawFrame(0, &foreign_room);
  EXPECT_FALSE(Widget("ChestReward").has_value());
  EXPECT_FALSE(Widget("ChestBig").has_value());
  EXPECT_EQ(changes_, 0);
}

TEST_F(DungeonChestEditorTest, RoomChangeResetsSelectionSearchAndError) {
  DrawFrame();
  auto& state = viewer_.chest_editor_state();
  state.selected_index = 5;
  std::strcpy(state.search.data(), "hammer");
  state.error = "Old room error";
  rooms_[1] = zelda3::Room(1, &rom_);
  rooms_[1].SetLoaded(true);
  rooms_[1].LoadChests();
  rooms_[1].GetChests().push_back({0x32, true});
  DrawFrame(1);
  EXPECT_EQ(state.room_id, 1);
  EXPECT_EQ(state.selected_index, 0);
  EXPECT_STREQ(state.search.data(), "");
  EXPECT_TRUE(state.error.empty());
  EXPECT_EQ(rooms_[0].GetChests()[0].id, 0x24);
  EXPECT_EQ(rooms_[1].GetChests()[0].id, 0x32);
}

}  // namespace
}  // namespace yaze::editor
