#include "app/editor/dungeon/inspectors/dungeon_connection_editor.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_connection_edit.h"
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {

using Door = zelda3::Room::Door;
using Direction = zelda3::DoorDirection;
using Type = zelda3::DoorType;

Door MakeDoor(uint8_t position, Type type, Direction direction) {
  const auto [byte1, byte2] =
      zelda3::DoorPositionManager::EncodeDoorBytes(position, type, direction);
  return Door::FromRomBytes(byte1, byte2);
}

class DungeonConnectionEditorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    for (int room_id : {0x11, 0x01, 0x12, 0x02}) {
      rooms_[room_id] = zelda3::Room(room_id, &rom_);
      rooms_[room_id].SetLoaded(true);
    }
    rooms_[0x11].GetDoors().push_back(
        MakeDoor(0, Type::NormalDoor, Direction::North));
    rooms_[0x12].GetDoors().push_back(
        MakeDoor(1, Type::NormalDoorLower, Direction::North));
    viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 0x11);
    viewer_.SetDoorConnectionCallbacks(
        [this](const DungeonConnectionRequest& request)
            -> absl::StatusOr<DungeonConnectionPlan> {
          ++previews_;
          const auto& source = rooms_[request.source_room_id];
          const auto target = DungeonConnectionTargetRoom(
              request.source_room_id,
              source.GetDoors()[request.source_door_index]);
          if (!target.ok()) {
            return target.status();
          }
          return PlanDungeonDoorConnection(source, rooms_[*target], request);
        },
        [this](const DungeonConnectionPlan& plan) {
          ++attempts_;
          if (reject_) {
            return absl::FailedPreconditionError("Test target rejection");
          }
          rooms_[plan.request.source_room_id].GetDoors() = plan.source_after;
          rooms_[plan.target_room_id].GetDoors() = plan.target_after;
          ++changes_;
          return absl::OkStatus();
        });
    viewer_.SetRoomNavigationCallback(
        [this](int room_id) { opened_room_ = room_id; });
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(900, 900);
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
    ImGui::SetNextWindowSize(ImVec2(width_, 800), ImGuiCond_Always);
    ImGui::Begin("ConnectionEditorHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    if (shared_inspector_) {
      DrawDungeonEntityInspector(viewer_, jump_);
    } else {
      DrawDungeonConnectionEditor(viewer_, door_index_, jump_);
    }
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    const auto* window = ImGui::GetCurrentWindow();
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
      if (path.find("Dungeon/ConnectionEditor/") != std::string::npos &&
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

  void Prepare() {
    DrawFrame();
    DrawFrame();
  }

  void ChooseLower() {
    Click("ConnectionLayer");
    Click("ConnectionLower");
  }

  Rom rom_;
  DungeonRoomStore rooms_{&rom_};
  DungeonCanvasViewer viewer_{&rom_};
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  std::function<void(int, size_t)> jump_;
  std::string logged_text_;
  size_t door_index_ = 0;
  float width_ = 300;
  int previews_ = 0;
  int attempts_ = 0;
  int changes_ = 0;
  int opened_room_ = -1;
  bool reject_ = false;
  bool shared_inspector_ = false;
};

TEST_F(DungeonConnectionEditorTest, MissingReturnPreviewShowsBothActualSlots) {
  const auto before = rooms_[0x11].GetDoors();
  Prepare();
  EXPECT_NE(logged_text_.find("Source · room 011"), std::string::npos);
  EXPECT_NE(logged_text_.find("Return · room 001"), std::string::npos);
  EXPECT_NE(logged_text_.find("North · slot 00"), std::string::npos);
  EXPECT_NE(logged_text_.find("South · slot 06"), std::string::npos);
  EXPECT_NE(logged_text_.find("Missing return door"), std::string::npos);
  EXPECT_TRUE(SameDungeonDoors(before, rooms_[0x11].GetDoors()));
  EXPECT_TRUE(rooms_[0x01].GetDoors().empty());
  EXPECT_EQ(attempts_, 0);
  EXPECT_GT(previews_, 0);
}

TEST_F(DungeonConnectionEditorTest, ChangingPairLayerOnlyChangesPreview) {
  Prepare();
  ChooseLower();
  EXPECT_EQ(viewer_.connection_editor_state().layer,
            DungeonConnectionLayer::kLower);
  EXPECT_EQ(rooms_[0x11].GetDoors()[0].type, Type::NormalDoor);
  EXPECT_TRUE(rooms_[0x01].GetDoors().empty());
  EXPECT_NE(logged_text_.find("Normal Door (Lower)"), std::string::npos);
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonConnectionEditorTest, CreateDispatchesBothEndpointsOnce) {
  Prepare();
  ChooseLower();
  Click("ConnectionApply");
  EXPECT_EQ(attempts_, 1);
  EXPECT_EQ(changes_, 1);
  EXPECT_EQ(rooms_[0x11].GetDoors()[0].type, Type::NormalDoorLower);
  ASSERT_EQ(rooms_[0x01].GetDoors().size(), 1);
  EXPECT_EQ(rooms_[0x01].GetDoors()[0].type, Type::NormalDoorLower);
  EXPECT_EQ(rooms_[0x01].GetDoors()[0].direction, Direction::South);
  EXPECT_EQ(rooms_[0x01].GetDoors()[0].position, 6);
  ASSERT_TRUE(Widget("ConnectionApply"));
  EXPECT_FALSE(Widget("ConnectionApply")->enabled);
  EXPECT_NE(logged_text_.find("Both doors already match"), std::string::npos);
}

TEST_F(DungeonConnectionEditorTest, ExistingPairUpdatesWithoutAddingDoor) {
  rooms_[0x01].GetDoors().push_back(
      MakeDoor(6, Type::NormalDoor, Direction::South));
  Prepare();
  ASSERT_TRUE(Widget("ConnectionApply"));
  EXPECT_FALSE(Widget("ConnectionApply")->enabled);
  ChooseLower();
  ASSERT_TRUE(Widget("ConnectionApply"));
  EXPECT_TRUE(Widget("ConnectionApply")->enabled);
  Click("ConnectionApply");
  EXPECT_EQ(changes_, 1);
  EXPECT_EQ(rooms_[0x11].GetDoors()[0].type, Type::NormalDoorLower);
  ASSERT_EQ(rooms_[0x01].GetDoors().size(), 1);
  EXPECT_EQ(rooms_[0x01].GetDoors()[0].type, Type::NormalDoorLower);
}

TEST_F(DungeonConnectionEditorTest, RejectedApplyKeepsPreviewAndShowsReason) {
  reject_ = true;
  Prepare();
  ChooseLower();
  Click("ConnectionApply");
  EXPECT_EQ(attempts_, 1);
  EXPECT_EQ(changes_, 0);
  EXPECT_EQ(rooms_[0x11].GetDoors()[0].type, Type::NormalDoor);
  EXPECT_TRUE(rooms_[0x01].GetDoors().empty());
  EXPECT_NE(logged_text_.find("Edit not applied: Test target rejection"),
            std::string::npos);
  EXPECT_EQ(viewer_.connection_editor_state().layer,
            DungeonConnectionLayer::kLower);
}

TEST_F(DungeonConnectionEditorTest, AmbiguousReturnCannotBeApplied) {
  rooms_[0x01].GetDoors() = {
      MakeDoor(6, Type::NormalDoor, Direction::South),
      MakeDoor(6, Type::NormalDoorLower, Direction::South)};
  Prepare();
  EXPECT_NE(logged_text_.find("Connection unavailable"), std::string::npos);
  EXPECT_FALSE(Widget("ConnectionApply"));
  EXPECT_FALSE(Widget("ConnectionOpenTarget"));
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonConnectionEditorTest, UnsupportedDoorShowsReasonWithoutAction) {
  rooms_[0x11].GetDoors()[0].type = Type::SmallKeyDoor;
  Prepare();
  EXPECT_NE(logged_text_.find("Connection unavailable"), std::string::npos);
  EXPECT_FALSE(Widget("ConnectionApply"));
  ASSERT_TRUE(Widget("ConnectionLayer"));
  EXPECT_FALSE(Widget("ConnectionLayer")->enabled);
}

TEST_F(DungeonConnectionEditorTest,
       VanillaRoom55ExitMarkerNeverOffersReturnDoorOrRoomNavigation) {
  for (const int id : {0x55, 0x65, 0x56}) {
    rooms_[id] = zelda3::Room(id, &rom_);
    rooms_[id].SetLoaded(true);
  }
  rooms_[0x55].GetDoors() = {MakeDoor(6, Type::NormalDoor, Direction::South),
                             MakeDoor(6, Type::ExitMarker, Direction::South),
                             MakeDoor(7, Type::NormalDoor, Direction::East)};
  viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 0x55);
  Prepare();
  EXPECT_NE(logged_text_.find("Connection unavailable"), std::string::npos);
  EXPECT_EQ(logged_text_.find("Create Return Door"), std::string::npos);
  EXPECT_FALSE(Widget("ConnectionApply"));
  EXPECT_FALSE(Widget("ConnectionOpenTarget"));
  EXPECT_EQ(attempts_, 0);
  EXPECT_EQ(opened_room_, -1);
  EXPECT_TRUE(rooms_[0x65].GetDoors().empty());

  // An exit elsewhere in the room does not disable a separate internal door.
  door_index_ = 2;
  Prepare();
  ASSERT_TRUE(Widget("ConnectionApply"));
  EXPECT_TRUE(Widget("ConnectionApply")->enabled);
  ASSERT_TRUE(Widget("ConnectionOpenTarget"));
  EXPECT_TRUE(Widget("ConnectionOpenTarget")->enabled);
  EXPECT_NE(logged_text_.find("Return · room 056"), std::string::npos);
}

TEST_F(DungeonConnectionEditorTest,
       ExplicitExitTypesNeverOfferReturnDoorOrRoomNavigation) {
  for (const auto type :
       {Type::ExitLower, Type::UnusedCaveExit, Type::WaterfallDoor,
        Type::FancyDungeonExit, Type::FancyDungeonExitLower, Type::CaveExit,
        Type::LitCaveExitLower, Type::BombableCaveExit, Type::ExitMarker}) {
    SCOPED_TRACE(static_cast<int>(type));
    rooms_[0x11].GetDoors() = {MakeDoor(6, type, Direction::South)};
    Prepare();
    EXPECT_NE(logged_text_.find("Connection unavailable"), std::string::npos);
    EXPECT_EQ(logged_text_.find("Create Return Door"), std::string::npos);
    EXPECT_FALSE(Widget("ConnectionApply"));
    EXPECT_FALSE(Widget("ConnectionOpenTarget"));
    ASSERT_TRUE(Widget("ConnectionLayer"));
    EXPECT_FALSE(Widget("ConnectionLayer")->enabled);
    EXPECT_EQ(attempts_, 0);
    EXPECT_EQ(opened_room_, -1);
  }
}

TEST_F(DungeonConnectionEditorTest, ReadOnlyViewCanInspectAndOpenTarget) {
  viewer_.SetHeaderReadOnly(true);
  Prepare();
  for (const char* name : {"ConnectionLayer", "ConnectionApply"}) {
    ASSERT_TRUE(Widget(name)) << name;
    EXPECT_FALSE(Widget(name)->enabled) << name;
  }
  Click("ConnectionOpenTarget");
  EXPECT_EQ(opened_room_, 0x01);
  EXPECT_EQ(attempts_, 0);
  EXPECT_NE(logged_text_.find("This view is read-only"), std::string::npos);
}

TEST_F(DungeonConnectionEditorTest, ExistingReturnUsesSuppliedSelectionJump) {
  rooms_[0x01].GetDoors() = {MakeDoor(8, Type::NormalDoor, Direction::South),
                             MakeDoor(6, Type::NormalDoor, Direction::South)};
  size_t jumped_index = 100;
  jump_ = [this, &jumped_index](int room_id, size_t index) {
    opened_room_ = room_id;
    jumped_index = index;
  };
  Prepare();
  Click("ConnectionOpenTarget");
  EXPECT_EQ(opened_room_, 0x01);
  EXPECT_EQ(jumped_index, 1);
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonConnectionEditorTest, WorkbenchReturnUsesViewerSelectionJump) {
  rooms_[0x01].GetDoors() = {MakeDoor(8, Type::NormalDoor, Direction::South),
                             MakeDoor(6, Type::NormalDoor, Direction::South)};
  size_t jumped_index = 100;
  int jumped_room = -1;
  viewer_.SetDoorConnectionNavigationCallback(
      [&jumped_room, &jumped_index](int room_id, size_t index) {
        jumped_room = room_id;
        jumped_index = index;
      });
  Prepare();
  Click("ConnectionOpenTarget");
  EXPECT_EQ(jumped_room, 0x01);
  EXPECT_EQ(jumped_index, 1);
  EXPECT_EQ(opened_room_, -1);
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonConnectionEditorTest,
       MissingReturnOpensRoomWithoutSelectingDoor) {
  int selection_jumps = 0;
  viewer_.SetDoorConnectionNavigationCallback(
      [&selection_jumps](int, size_t) { ++selection_jumps; });
  Prepare();
  Click("ConnectionOpenTarget");
  EXPECT_EQ(selection_jumps, 0);
  EXPECT_EQ(opened_room_, 0x01);
  EXPECT_EQ(attempts_, 0);
}

TEST_F(DungeonConnectionEditorTest, NewSelectionDiscardsLayerChoiceAndError) {
  rooms_[0x11].GetDoors().push_back(
      MakeDoor(1, Type::NormalDoor, Direction::North));
  reject_ = true;
  Prepare();
  ChooseLower();
  Click("ConnectionApply");
  ASSERT_FALSE(viewer_.connection_editor_state().error.empty());
  door_index_ = 1;
  DrawFrame();
  EXPECT_TRUE(viewer_.connection_editor_state().error.empty());
  EXPECT_EQ(viewer_.connection_editor_state().layer,
            DungeonConnectionLayer::kUpper);
  EXPECT_EQ(logged_text_.find("Edit not applied"), std::string::npos);
}

TEST_F(DungeonConnectionEditorTest,
       AcceptedSourceChangeResetsDraftToCurrentDoor) {
  Prepare();
  ChooseLower();
  viewer_.connection_editor_state().error = "Stale error";
  rooms_[0x11].GetDoors()[0].position = 2;
  DrawFrame();
  EXPECT_EQ(viewer_.connection_editor_state().layer,
            DungeonConnectionLayer::kUpper);
  EXPECT_TRUE(viewer_.connection_editor_state().error.empty());
  EXPECT_NE(logged_text_.find("South · slot 08"), std::string::npos);
}

TEST_F(DungeonConnectionEditorTest, RoomRefreshDiscardsDraftAndError) {
  Prepare();
  viewer_.connection_editor_state().error = "Stale error";
  viewer_.RefreshRomBackedState(&rom_, nullptr, &rooms_, 0x12);
  DrawFrame();
  EXPECT_TRUE(viewer_.connection_editor_state().error.empty());
  EXPECT_EQ(viewer_.connection_editor_state().layer,
            DungeonConnectionLayer::kLower);
  EXPECT_NE(logged_text_.find("Source · room 012"), std::string::npos);
  EXPECT_NE(logged_text_.find("Return · room 002"), std::string::npos);
}

TEST_F(DungeonConnectionEditorTest, SharedInspectorIncludesConnectionControls) {
  viewer_.object_interaction().SelectEntity(EntityType::Door, 0);
  shared_inspector_ = true;
  width_ = 260;
  Prepare();
  EXPECT_NE(logged_text_.find("Room connection"), std::string::npos);
  ASSERT_TRUE(Widget("ConnectionApply"));
  EXPECT_TRUE(Widget("ConnectionApply")->enabled);
}

TEST_F(DungeonConnectionEditorTest, NarrowPreviewWrapsLongChangesAndError) {
  width_ = 240;
  reject_ = true;
  Prepare();
  ChooseLower();
  Click("ConnectionApply");
  EXPECT_NE(logged_text_.find("Test target rejection"), std::string::npos);
}

}  // namespace
}  // namespace yaze::editor
