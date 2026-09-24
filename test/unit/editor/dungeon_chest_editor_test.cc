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
#include "zelda3/dungeon/chest_edit.h"
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
            for (size_t object_index = 0;
                 object_index < rooms_[room_id].GetTileObjects().size();
                 ++object_index) {
              if (zelda3::ChestIndexForObject(rooms_[room_id].GetTileObjects(),
                                              object_index) == index) {
                rooms_[room_id].GetTileObjects()[object_index].id_ =
                    big ? 0xFB1 : 0xF99;
              }
            }
            ++changes_;
            last_room_id_ = room_id;
          }
          return absl::OkStatus();
        });
    viewer_.SetChestDeleteCallback([this](int room_id, size_t index) {
      if (reject_) {
        return absl::FailedPreconditionError("Test rejection");
      }
      auto& room = rooms_[room_id];
      auto& objects = room.GetTileObjects();
      for (size_t object_index = 0; object_index < objects.size();
           ++object_index) {
        if (zelda3::ChestIndexForObject(objects, object_index) == index) {
          objects.erase(objects.begin() + object_index);
          room.GetChests().erase(room.GetChests().begin() + index);
          ++deletions_;
          last_room_id_ = room_id;
          return absl::OkStatus();
        }
      }
      return absl::FailedPreconditionError("No matching chest object");
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
  int deletions_ = 0;
  int last_room_id_ = -1;
  bool reject_ = false;
};

TEST_F(DungeonChestEditorTest, NamesUseReceiptIdsRatherThanInventoryIndices) {
  EXPECT_EQ(GetDungeonChestItemLabel(0x00), "Fighter Sword");
  EXPECT_EQ(GetDungeonChestItemLabel(0x09), "Hammer");
  EXPECT_EQ(zelda3::GetItemReceiptLabel(0x1B), "Power Glove");
  EXPECT_EQ(zelda3::GetItemReceiptLabel(0x3A), "Tossed Bow");
  EXPECT_STREQ(zelda3::GetItemReceiptLabelSource(0x32), "vanilla_receipt");
  EXPECT_STREQ(zelda3::GetItemReceiptLabelSource(0xFF), "unknown");
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
  EXPECT_EQ(zelda3::GetItemReceiptLabel(0xE0), "Custom Treasure");
  EXPECT_STREQ(zelda3::GetItemReceiptLabelSource(0xE0), "project");
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
  EXPECT_EQ(rooms_[0].GetTileObjects()[0].id_, 0xFB1);
  EXPECT_EQ(logged_text_.find("Chest objects and contents do not match"),
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
  EXPECT_NE(logged_text_.find("Chest objects and contents do not match"),
            std::string::npos);
  EXPECT_FALSE(Widget("ChestReward").has_value());
  EXPECT_FALSE(Widget("ChestBig").has_value());
  EXPECT_EQ(changes_, 0);
}

TEST_F(DungeonChestEditorTest,
       AddChestStartsCanvasPlacementWithoutMutatingRoom) {
  rooms_[0].GetChests().clear();
  rooms_[0].GetTileObjects().clear();
  DrawFrame();
  DrawFrame();
  Click("ChestAddSmall");
  ASSERT_NE(viewer_.object_interaction().GetPlacementPreview(), nullptr);
  EXPECT_EQ(viewer_.object_interaction().GetPlacementPreview()->id_, 0xF99);
  EXPECT_TRUE(zelda3::ValidateRoomObjectStreamEntryForSave(
                  *viewer_.object_interaction().GetPlacementPreview())
                  .ok());
  EXPECT_TRUE(viewer_.object_interaction().IsObjectLoaded());
  EXPECT_TRUE(viewer_.IsObjectInteractionEnabled());
  EXPECT_TRUE(rooms_[0].GetChests().empty());
  EXPECT_TRUE(rooms_[0].GetTileObjects().empty());
  EXPECT_NE(logged_text_.find("Click in the canvas to place a small chest"),
            std::string::npos);
  Click("ChestAddBig");
  ASSERT_NE(viewer_.object_interaction().GetPlacementPreview(), nullptr);
  EXPECT_EQ(viewer_.object_interaction().GetPlacementPreview()->id_, 0xFB1);
  EXPECT_TRUE(zelda3::ValidateRoomObjectStreamEntryForSave(
                  *viewer_.object_interaction().GetPlacementPreview())
                  .ok());
  EXPECT_NE(logged_text_.find("Click in the canvas to place a big chest"),
            std::string::npos);
  EXPECT_EQ(changes_, 0);
  EXPECT_EQ(deletions_, 0);
}

TEST_F(DungeonChestEditorTest, SelectInCanvasUsesChestStreamOrderAcrossLayers) {
  rooms_[0].GetTileObjects() = {zelda3::RoomObject(0xF99, 20, 22, 0, 2),
                                zelda3::RoomObject(0x000, 1, 1, 0, 0),
                                zelda3::RoomObject(0xFB1, 8, 10, 0, 0)};
  rooms_[0].GetChests() = {{0x32, true}, {0x24, false}};
  DrawFrame();
  DrawFrame();
  Click("ChestAddSmall");
  Click("ChestSelect");
  EXPECT_EQ(viewer_.object_interaction().GetSelectedObjectIndices(),
            std::vector<size_t>({2}));
  EXPECT_EQ(viewer_.GetPendingScrollTarget(), std::make_pair(8, 10));
  EXPECT_EQ(viewer_.object_interaction().GetPlacementPreview(), nullptr);
  EXPECT_EQ(changes_, 0);
  EXPECT_EQ(deletions_, 0);
}

TEST_F(DungeonChestEditorTest,
       CanvasSelectionFollowsChestWithoutOverridingRecordChoice) {
  rooms_[0].GetTileObjects().emplace_back(0xF99, 20, 22, 0, 0);
  rooms_[0].GetChests().push_back({0x09, false});
  DrawFrame();
  viewer_.object_interaction().SetSelectedObjects({1});
  DrawFrame();
  EXPECT_EQ(viewer_.chest_editor_state().selected_index, 1);
  // Choosing a record remains stable while the canvas selection is unchanged.
  viewer_.chest_editor_state().selected_index = 0;
  DrawFrame();
  EXPECT_EQ(viewer_.chest_editor_state().selected_index, 0);
  viewer_.object_interaction().ClearSelection();
  DrawFrame();
  viewer_.object_interaction().SetSelectedObjects({1});
  DrawFrame();
  EXPECT_EQ(viewer_.chest_editor_state().selected_index, 1);
}

TEST_F(DungeonChestEditorTest,
       DeleteLastChestRemovesBothPartsAndKeepsUiBalanced) {
  DrawFrame();
  DrawFrame();
  Click("ChestDelete");
  EXPECT_TRUE(rooms_[0].GetTileObjects().empty());
  EXPECT_TRUE(rooms_[0].GetChests().empty());
  EXPECT_EQ(deletions_, 1);
  EXPECT_EQ(last_room_id_, 0);
  EXPECT_FALSE(Widget("ChestReward").has_value());
  ASSERT_TRUE(Widget("ChestAddSmall").has_value());
  EXPECT_TRUE(Widget("ChestAddSmall")->enabled);
  EXPECT_EQ(viewer_.chest_editor_state().selected_index, 0);
}

TEST_F(DungeonChestEditorTest, RejectedDeleteKeepsBothPartsAndDisplaysReason) {
  reject_ = true;
  DrawFrame();
  DrawFrame();
  Click("ChestDelete");
  ASSERT_EQ(rooms_[0].GetTileObjects().size(), 1);
  ASSERT_EQ(rooms_[0].GetChests().size(), 1);
  EXPECT_EQ(rooms_[0].GetChests()[0].id, 0x24);
  EXPECT_EQ(deletions_, 0);
  EXPECT_NE(logged_text_.find("Edit not applied: Test rejection"),
            std::string::npos);
}

TEST_F(DungeonChestEditorTest, MismatchedDataAllowsRewardEditOnly) {
  rooms_[0].GetTileObjects()[0].id_ = 0xFB1;
  DrawFrame();
  DrawFrame();
  for (const char* control : {"ChestAddSmall", "ChestAddBig", "ChestSelect",
                              "ChestDelete", "ChestBig"}) {
    ASSERT_TRUE(Widget(control).has_value()) << control;
    EXPECT_FALSE(Widget(control)->enabled) << control;
  }
  ASSERT_TRUE(Widget("ChestReward").has_value());
  EXPECT_TRUE(Widget("ChestReward")->enabled);
  EXPECT_NE(logged_text_.find("Rewards remain editable"), std::string::npos);
  EXPECT_EQ(changes_, 0);
  EXPECT_EQ(deletions_, 0);
}

TEST_F(DungeonChestEditorTest, ReadOnlyRoomDisablesAllChestMutations) {
  viewer_.SetHeaderReadOnly(true);
  DrawFrame();
  DrawFrame();
  for (const char* control : {"ChestAddSmall", "ChestAddBig", "ChestDelete",
                              "ChestBig", "ChestReward"}) {
    ASSERT_TRUE(Widget(control).has_value()) << control;
    EXPECT_FALSE(Widget(control)->enabled) << control;
  }
  ASSERT_TRUE(Widget("ChestSelect").has_value());
  EXPECT_TRUE(Widget("ChestSelect")->enabled);
  EXPECT_FALSE(viewer_.DeleteChest(0, 0).ok());
  EXPECT_EQ(deletions_, 0);
}

TEST_F(DungeonChestEditorTest, OffscreenRoomCannotChangePlacementOrSelection) {
  rooms_[1] = zelda3::Room(1, &rom_);
  rooms_[1].SetLoaded(true);
  rooms_[1].LoadChests();
  rooms_[1].GetChests().push_back({0x32, true});
  rooms_[1].GetTileObjects().emplace_back(0xFB1, 2, 4, 0, 0);
  viewer_.SetPreviewObject(zelda3::RoomObject(0x001, 0, 0, 0));
  viewer_.object_interaction().SetSelectedObjects({0});
  DrawFrame(1);
  DrawFrame(1);
  for (const char* control : {"ChestAddSmall", "ChestAddBig", "ChestSelect"}) {
    ASSERT_TRUE(Widget(control).has_value()) << control;
    EXPECT_FALSE(Widget(control)->enabled) << control;
  }
  EXPECT_EQ(viewer_.object_interaction().GetPlacementPreview()->id_, 0x001);
  EXPECT_EQ(viewer_.object_interaction().GetSelectedObjectIndices(),
            std::vector<size_t>({0}));
  EXPECT_FALSE(viewer_.HasPendingScrollTarget());
}

TEST_F(DungeonChestEditorTest, DeleteRequiresLoadedRoomAndEditorCallback) {
  EXPECT_FALSE(viewer_.DeleteChest(1, 0).ok());
  viewer_.SetChestDeleteCallback({});
  EXPECT_FALSE(viewer_.DeleteChest(0, 0).ok());
  EXPECT_EQ(deletions_, 0);
  ASSERT_EQ(rooms_[0].GetChests().size(), 1);
  ASSERT_EQ(rooms_[0].GetTileObjects().size(), 1);
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
