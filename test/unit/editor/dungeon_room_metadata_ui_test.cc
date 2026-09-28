#include "app/editor/dungeon/dungeon_room_edit.h"

#include <gtest/gtest.h>

#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"
#include "app/editor/dungeon/ui/window/room_tag_editor_panel.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/icons.h"
#include "dungeon_workbench_test_peer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {

class DungeonRoomMetadataUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 1400);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    rooms_[0].SetLoaded(true);
    rooms_[1].SetLoaded(true);
    rooms_[0].ClearSaveDirtyState();
    rooms_[1].ClearSaveDirtyState();
    viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, room_id_);
    const auto edit = [this](int room_id, const RoomMetadataEdit& request) {
      edits_.push_back({room_id, request});
      if (reject_) {
        return absl::FailedPreconditionError(
            "Metadata edit deliberately rejected");
      }
      return ApplyRoomMetadataEdit(rooms_[room_id], request);
    };
    viewer_.SetMetadataEditCallback(edit);
    tag_panel_.SetMetadataEditCallback(edit);
    tag_panel_.SetRooms(&rooms_);
    tag_panel_.SetCurrentRoomId(room_id_);
  }

  void TearDown() override {
    ImGui::DestroyContext();
    gui::WidgetIdRegistry::Instance().Clear();
  }

  void DrawFrame(const std::function<void(ImGuiIO&)>& events = {}) {
    if (events) {
      events(ImGui::GetIO());
    }
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width_, 1300), ImGuiCond_Always);
    ImGui::Begin("MetadataHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    if (show_destinations_) {
      DrawDungeonDestinationEditor(viewer_);
      EXPECT_LE(ImGui::GetCurrentWindow()->DC.CursorMaxPos.x,
                ImGui::GetCurrentWindow()->WorkRect.Max.x + 1);
    } else if (show_tag_panel_) {
      tag_panel_.Draw(nullptr);
    } else {
      ImGui::GetStateStorage()->SetInt(
          ImGui::GetID(ICON_MD_TUNE " Room Properties"), 1);
      ImGui::GetStateStorage()->SetInt(
          ImGui::GetID(ICON_MD_ALT_ROUTE " Destinations"), 1);
      DungeonWorkbenchContentTestPeer::DrawRoomInspector(content_, viewer_);
    }
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* label) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(label);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if ((path.starts_with(show_tag_panel_ ? "Dungeon/RoomTags/"
                                            : "Dungeon/Workbench/") ||
           path.starts_with("Dungeon/Destinations/")) &&
          widget.label == normalized) {
        return widget;
      }
    }
    return std::nullopt;
  }

  void Click(const char* label) {
    const auto widget = Widget(label);
    ASSERT_TRUE(widget.has_value()) << label;
    ASSERT_TRUE(widget->visible) << label;
    ASSERT_TRUE(widget->enabled) << label;
    const auto bounds = widget->bounds;
    ASSERT_TRUE(bounds.valid);
    const float x = (bounds.min_x + bounds.max_x) / 2;
    const float y = (bounds.min_y + bounds.max_y) / 2;
    DrawFrame([&](ImGuiIO& io) { io.AddMousePosEvent(x, y); });
    DrawFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
    DrawFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
  }

  void Type(const char* field, const char* value) {
    DrawFrame();
    DrawFrame();
    Click(field);
    DrawFrame([&](ImGuiIO& io) { io.AddInputCharactersUTF8(value); });
  }

  void Enter() {
    DrawFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });
    DrawFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, false); });
  }

  struct Edit {
    int room_id;
    RoomMetadataEdit request;
  };
  std::vector<Edit> edits_;
  bool reject_ = false;
  bool show_tag_panel_ = false;
  bool show_destinations_ = false;
  float width_ = 380;
  std::string logged_text_;
  int room_id_ = 0;
  std::deque<int> recent_;
  DungeonRoomStore rooms_;
  DungeonCanvasViewer viewer_;
  RoomTagEditorPanel tag_panel_;
  DungeonWorkbenchContent content_{
      nullptr,
      &room_id_,
      [](int) {},
      [](int, RoomSelectionIntent) {},
      [](int) {},
      []() {},
      [this]() { return &viewer_; },
      [this](int) { return &viewer_; },
      [this]() -> const std::deque<int>& { return recent_; },
      [](int) {},
      [](bool) {}};
};

TEST_F(DungeonRoomMetadataUiTest, NumericEditCommitsOnceThroughTypedBoundary) {
  Type("RoomHeaderFloor1", "0B");
  EXPECT_TRUE(edits_.empty());
  EXPECT_EQ(rooms_[0].floor1(), 0);
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].room_id, 0);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kFloor1);
  EXPECT_EQ(edits_[0].request.value, 0x0B);
  EXPECT_EQ(rooms_[0].floor1(), 0x0B);
  EXPECT_EQ(rooms_[1].floor1(), 0);
}

TEST_F(DungeonRoomMetadataUiTest, RoomSwitchDiscardsEqualValuedPendingEdit) {
  Type("RoomHeaderFloor1", "0B");
  ASSERT_TRUE(edits_.empty());
  room_id_ = 1;
  viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, room_id_);
  DrawFrame();
  Enter();
  EXPECT_TRUE(edits_.empty());
  EXPECT_EQ(rooms_[0].floor1(), 0);
  EXPECT_EQ(rooms_[1].floor1(), 0);
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  EXPECT_FALSE(rooms_[1].HasUnsavedChanges());
}

TEST_F(DungeonRoomMetadataUiTest,
       InvalidNumericEditIsRejectedWithoutTruncation) {
  Type("RoomHeaderFloor1", "100");
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.value, 0x100);
  EXPECT_EQ(rooms_[0].floor1(), 0);
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  EXPECT_NE(logged_text_.find("Edit not applied"), std::string::npos);
}

TEST_F(DungeonRoomMetadataUiTest, DestinationEditPreservesCorrectSlot) {
  DrawFrame();
  DrawFrame();
  Click("Route");
  DrawFrame();
  Click("Route/3");
  Type("Room", "A5");
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircaseRoom);
  EXPECT_EQ(edits_[0].request.index, 2);
  EXPECT_EQ(rooms_[0].staircase_room(2), 0xA5);
  EXPECT_EQ(rooms_[0].staircase_room(0), 0);
}

TEST_F(DungeonRoomMetadataUiTest,
       DungeonSpriteGraphicsRejectFirstMissingGroup) {
  Type("RoomHeaderSpriteset", "50");
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kSpriteset);
  EXPECT_EQ(edits_[0].request.value, 0x50);
  EXPECT_EQ(rooms_[0].spriteset(), 0);
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  EXPECT_NE(logged_text_.find("Edit not applied"), std::string::npos);
}

TEST_F(DungeonRoomMetadataUiTest,
       DungeonSpriteGraphicsAcceptLastExistingGroup) {
  Type("RoomHeaderSpriteset", "4F");
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(rooms_[0].spriteset(), 0x4F);
  EXPECT_TRUE(rooms_[0].header_dirty());
}

TEST_F(DungeonRoomMetadataUiTest, NamedBg2ChoiceUsesMetadataBoundary) {
  DrawFrame();
  DrawFrame();
  Click("RoomHeaderBg2");
  DrawFrame();
  Click("RoomHeaderBg2/4");
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kBg2);
  EXPECT_EQ(edits_[0].request.value, 4);
  EXPECT_EQ(static_cast<int>(rooms_[0].bg2()), 4);
}

TEST_F(DungeonRoomMetadataUiTest,
       PitLayerUsesSharedEditAndFitsNarrowInspector) {
  show_destinations_ = true;
  width_ = 260;
  DrawFrame();
  DrawFrame();
  Click("Plane");
  DrawFrame();
  EXPECT_FALSE(Widget("Plane/3"));
  Click("Plane/2");
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kPitPlane);
  EXPECT_EQ(rooms_[0].CaptureMetadataSnapshot().pit_target_layer, 2);
  EXPECT_EQ(rooms_[0].staircase_plane(0), 0);
}

TEST_F(DungeonRoomMetadataUiTest, LegacyPlaneIsDisplayedWithoutMutation) {
  show_destinations_ = true;
  rooms_[0].SetPitsTargetLayer(3);
  rooms_[0].ClearSaveDirtyState();
  DrawFrame();
  DrawFrame();
  EXPECT_NE(logged_text_.find("Unmapped vanilla plane"), std::string::npos);
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
}

TEST_F(DungeonRoomMetadataUiTest,
       ReadOnlyAllowsDestinationNavigationIncludingRoomZero) {
  show_destinations_ = true;
  viewer_.SetHeaderReadOnly(true);
  int navigated = -1;
  viewer_.SetRoomSwapCallback([&](int source, int destination) {
    EXPECT_EQ(source, 0);
    navigated = destination;
  });
  DrawFrame();
  DrawFrame();
  ASSERT_TRUE(Widget("Room"));
  EXPECT_FALSE(Widget("Room")->enabled);
  EXPECT_FALSE(Widget("Plane")->enabled);
  Click("Open");
  EXPECT_EQ(navigated, 0);
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
}

TEST_F(DungeonRoomMetadataUiTest, DestinationRejectsOverflowWithoutTruncation) {
  show_destinations_ = true;
  Type("Room", "100");
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.value, 0x100);
  EXPECT_EQ(rooms_[0].holewarp(), 0);
  EXPECT_NE(logged_text_.find("Destination edit rejected"), std::string::npos);
}

TEST_F(DungeonRoomMetadataUiTest, StairPlaneEditTargetsOnlySelectedHeaderSlot) {
  show_destinations_ = true;
  DrawFrame();
  DrawFrame();
  Click("Route");
  DrawFrame();
  Click("Route/4");
  Click("Plane");
  DrawFrame();
  Click("Plane/1");
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircasePlane);
  EXPECT_EQ(edits_[0].request.index, 3);
  EXPECT_EQ(rooms_[0].staircase_plane(3), 1);
  EXPECT_EQ(rooms_[0].staircase_plane(0), 0);
  EXPECT_EQ(rooms_[0].CaptureMetadataSnapshot().pit_target_layer, 0);
}

TEST_F(DungeonRoomMetadataUiTest, SelectedStairLookupOpensSlotWithoutEditing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
  rooms_[0].SetRom(&rom);
  rooms_[1].SetRom(&rom);
  viewer_.RefreshRomBackedState(&rom, nullptr, &rooms_, room_id_);
  ASSERT_TRUE(
      rooms_[0].AddObject(zelda3::RoomObject(0x138, 10, 10, 0, 1)).ok());
  ASSERT_TRUE(
      rooms_[0].AddObject(zelda3::RoomObject(0x138, 20, 20, 0, 0)).ok());
  rooms_[0].ClearSaveDirtyState();
  viewer_.object_interaction().SetSelectedObjects({0});
  show_destinations_ = true;
  width_ = 260;
  DrawFrame();
  DrawFrame();
  Click("FindStairSlot");
  EXPECT_NE(logged_text_.find("Stair slot 2"), std::string::npos);
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  Click("Plane");
  DrawFrame();
  Click("Plane/1");
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(edits_[0].request.index, 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircasePlane);
}

TEST_F(DungeonRoomMetadataUiTest,
       StairLookupIsDisabledWithoutRomOrWithCustomCollision) {
  show_destinations_ = true;
  viewer_.object_interaction().SetSelectedObjects({0});
  DrawFrame();
  DrawFrame();
  ASSERT_TRUE(Widget("FindStairSlot"));
  EXPECT_FALSE(Widget("FindStairSlot")->enabled);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
  rooms_[0].SetRom(&rom);
  viewer_.RefreshRomBackedState(&rom, nullptr, &rooms_, room_id_);
  rooms_[0].set_has_custom_collision(true);
  viewer_.object_interaction().SetSelectedObjects({0});
  DrawFrame();
  EXPECT_FALSE(Widget("FindStairSlot")->enabled);
  EXPECT_TRUE(edits_.empty());
}

TEST_F(DungeonRoomMetadataUiTest, TagPanelUsesCallbackAndShowsRejectedEdit) {
  show_tag_panel_ = true;
  reject_ = true;
  DrawFrame();
  DrawFrame();
  Click("Tag1");
  DrawFrame();
  Click("Tag1/1");
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].room_id, 0);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kTag1);
  EXPECT_EQ(edits_[0].request.value, 1);
  EXPECT_EQ(rooms_[0].tag1(), zelda3::Nothing);
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  DrawFrame();
  EXPECT_NE(logged_text_.find("Metadata edit deliberately rejected"),
            std::string::npos);
}

}  // namespace
}  // namespace yaze::editor
