#include "app/editor/dungeon/dungeon_room_edit.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_property_controls.h"
#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"
#include "app/editor/dungeon/ui/window/room_tag_editor_panel.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/icons.h"
#include "core/features.h"
#include "core/project.h"
#include "dungeon_workbench_test_peer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

constexpr std::array<const char*, 10> kRoomPropertyFields = {
    "RoomHeaderLayout",   "RoomHeaderBlockset", "RoomHeaderFloor1",
    "RoomHeaderFloor2",   "RoomHeaderPalette",  "RoomHeaderSpriteset",
    "RoomHeaderMessage",  "RoomHeaderBg2",      "RoomHeaderEffect",
    "RoomHeaderCollision"};

class DungeonRoomMetadataUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    old_labels_ = zelda3::GetResourceLabels();
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
      if (simulate_product_refresh_) {
        // The product stages a metadata snapshot, restores it into the loaded
        // room, then refreshes room views. Force the resulting render revision
        // advance here so a destination-only write cannot stale stair lookup.
        zelda3::Room staged;
        staged.RestoreMetadataSnapshot(
            rooms_[room_id].CaptureMetadataSnapshot());
        const auto status = ApplyRoomMetadataEdit(staged, request);
        if (!status.ok()) {
          return status;
        }
        rooms_[room_id].RestoreMetadataSnapshot(
            staged.CaptureMetadataSnapshot());
        rooms_[room_id].MarkCompositeDirty();
        return absl::OkStatus();
      }
      return ApplyRoomMetadataEdit(rooms_[room_id], request);
    };
    viewer_.SetMetadataEditCallback(edit);
    viewer_.SetRoomSwapCallback([](int, int) {});
    tag_panel_.SetMetadataEditCallback(edit);
    tag_panel_.SetRooms(&rooms_);
    tag_panel_.SetCurrentRoomId(room_id_);
    pit_table_.SetRoomIds({0, 1});
    pit_table_.ClearDirty();
    content_.SetPitDamageTableProvider([this]() { return &pit_table_; });
  }

  void TearDown() override {
    zelda3::GetResourceLabels() = old_labels_;
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
    ImGui::SetNextWindowSize(ImVec2(width_, height_), ImGuiCond_Always);
    if (requested_scroll_) {
      ImGui::SetNextWindowScroll(ImVec2(-1, *requested_scroll_));
      requested_scroll_.reset();
    }
    ImGui::Begin("MetadataHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    // Logging must not auto-open collapsed sections and mask default UI state.
    ImGui::GetCurrentContext()->LogDepthToExpand = 0;
    if (show_compact_properties_) {
      DrawDungeonRoomPropertyFields(viewer_, rooms_[room_id_], room_id_,
                                    RoomPropertyLayout::kCompact);
    } else if (show_destinations_) {
      DrawDungeonDestinationEditor(viewer_);
      EXPECT_LE(ImGui::GetCurrentWindow()->DC.CursorMaxPos.x,
                ImGui::GetCurrentWindow()->WorkRect.Max.x + 1);
    } else if (show_tag_panel_) {
      tag_panel_.Draw(nullptr);
    } else {
      if (persist_collapsed_sections_) {
        for (const char* label :
             {ICON_MD_TUNE " Room Properties",
              ICON_MD_ALT_ROUTE " Destinations", ICON_MD_WARNING " Pit Damage",
              "Room tags", "Details##RoomDetails",
              "Details##PitDamageDetails"}) {
          ImGui::GetStateStorage()->SetInt(ImGui::GetID(label), 0);
        }
      }
      DungeonWorkbenchContentTestPeer::DrawInspector(content_, viewer_,
                                                     compact_);
    }
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    const auto* host = ImGui::GetCurrentWindow();
    form_height_ = host->DC.CursorMaxPos.y - host->DC.CursorStartPos.y;
    rendered_hex_width_ =
        ImGui::CalcTextSize("FF").x + 2 * ImGui::GetStyle().FramePadding.x;
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* label) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(label);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if ((path.starts_with(show_tag_panel_ ? "Dungeon/RoomTags/"
                                            : "Dungeon/Workbench/") ||
           path.starts_with("Dungeon/Destinations/") ||
           path.starts_with("Dungeon/RoomProperties/")) &&
          widget.label == normalized) {
        return widget;
      }
    }
    return std::nullopt;
  }

  void ScrollTo(float value) {
    requested_scroll_ = value;
    DrawFrame();
    DrawFrame();
  }

  void Reveal(const char* label) {
    if (const auto widget = Widget(label); widget && widget->visible) {
      return;
    }
    const auto* host = ImGui::FindWindowByName("MetadataHost");
    ASSERT_NE(host, nullptr);
    const float maximum = host->ScrollMax.y;
    for (float position = 0; position < maximum + 200; position += 200) {
      ScrollTo(std::min(position, maximum));
      if (const auto widget = Widget(label); widget && widget->visible) {
        return;
      }
    }
  }

  void Click(const char* label) {
    Reveal(label);
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
  bool simulate_product_refresh_ = false;
  bool show_tag_panel_ = false;
  bool show_destinations_ = false;
  bool show_compact_properties_ = false;
  bool persist_collapsed_sections_ = false;
  bool compact_ = false;
  float width_ = 320;
  float height_ = 600;
  float form_height_ = 0;
  float rendered_hex_width_ = 0;
  std::optional<float> requested_scroll_;
  project::YazeProject project_;
  zelda3::ResourceLabelProvider old_labels_;
  zelda3::PitDamageTable pit_table_;
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

TEST_F(DungeonRoomMetadataUiTest,
       DefaultAndPersistedCollapsedInspectExposeEssentialsAtLaptopSize) {
  viewer_.SetProject(&project_);
  rooms_[0].SetTag1(static_cast<zelda3::TagKey>(0x34));
  rooms_[0].SetTag2(static_cast<zelda3::TagKey>(0x37));
  rooms_[0].SetStaircaseRoom(0, 0x11);
  rooms_[0].SetStaircaseRoom(3, 0x44);
  rooms_[0].ClearSaveDirtyState();
  ASSERT_TRUE(project_.hack_manifest
                  .LoadFromString(R"json({
    "version":1,"hack_name":"Oracle of Secrets",
    "room_tags":{"tags":[
      {"tag_id":"0x34","name":"Crumble Floor"},
      {"tag_id":"0x37","name":"Cart shutter","enabled":false}]}
  })json")
                  .ok());
  project_.resource_labels["room_tag"]["0x34"] = "Weekend Crumble";
  zelda3::ResourceLabelProvider::ProjectLabels foreign;
  foreign["room_tag"]["52"] = "Wrong session";
  zelda3::GetResourceLabels().SetProjectLabels(&foreign);

  for (const float width : {260.0f, 320.0f}) {
    width_ = width;
    for (const bool compact : {false, true}) {
      compact_ = compact;
      for (const bool collapsed : {false, true}) {
        persist_collapsed_sections_ = collapsed;
        SCOPED_TRACE(::testing::Message() << width << " compact=" << compact
                                          << " collapsed=" << collapsed);
        ScrollTo(0);
        EXPECT_NE(logged_text_.find("Weekend Crumble"), std::string::npos);
        EXPECT_NE(logged_text_.find("Cart shutter (disabled)"),
                  std::string::npos);
        EXPECT_EQ(logged_text_.find("Wrong session"), std::string::npos);
        EXPECT_NE(logged_text_.find("Pit damage"), std::string::npos);
        EXPECT_FALSE(Widget("Route"));
        EXPECT_EQ(logged_text_.find("Room matches ROM buffer"),
                  std::string::npos);
        EXPECT_EQ(logged_text_.find("Move damage to room"), std::string::npos);
        EXPECT_EQ(logged_text_.find("Apply with Pits scope enabled"),
                  std::string::npos);
        EXPECT_FALSE(content_.GetPitDamageControlRectsForTesting()
                         .replace_current.visible);
        std::vector<std::string> fields(kRoomPropertyFields.begin(),
                                        kRoomPropertyFields.end());
        fields.insert(fields.end(), {"RoomHeaderTag1", "RoomHeaderTag2"});
        for (int route = 0; route < 5; ++route) {
          fields.push_back("Room/" + std::to_string(route));
          fields.push_back("Plane/" + std::to_string(route));
          fields.push_back("Open/" + std::to_string(route));
        }
        // All controls are in the normal scroll flow. No section is opened to
        // discover them, including when old collapsed settings are restored.
        std::set<std::string> visible_fields;
        const auto* host = ImGui::FindWindowByName("MetadataHost");
        ASSERT_NE(host, nullptr);
        const float maximum = host->ScrollMax.y;
        for (float position = 0; position < maximum + 200; position += 200) {
          ScrollTo(std::min(position, maximum));
          for (const auto& field : fields) {
            const auto widget = Widget(field.c_str());
            if (!widget)
              continue;
            EXPECT_TRUE(widget->enabled) << field;
            EXPECT_GE(widget->bounds.min_x, 20) << field;
            EXPECT_LE(widget->bounds.max_x, 20 + width_) << field;
            if (widget->visible && widget->bounds.min_y >= 40 &&
                widget->bounds.max_y <= 20 + height_ - 8) {
              visible_fields.insert(field);
            }
          }
          EXPECT_FALSE(content_.GetPitDamageControlRectsForTesting()
                           .replace_current.visible);
          EXPECT_EQ(logged_text_.find("Room matches ROM buffer"),
                    std::string::npos);
        }
        for (const auto& field : fields)
          EXPECT_TRUE(visible_fields.contains(field)) << field;
      }
    }
  }
  project_.resource_labels.clear();
  ScrollTo(0);
  EXPECT_NE(logged_text_.find("Crumble Floor"), std::string::npos);
  viewer_.SetProject(nullptr);
  DrawFrame();
  EXPECT_NE(logged_text_.find("Holes 3"), std::string::npos);
  EXPECT_NE(logged_text_.find("Holes 6"), std::string::npos);
  EXPECT_EQ(logged_text_.find("Wrong session"), std::string::npos);
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  EXPECT_FALSE(pit_table_.dirty());
}

TEST_F(DungeonRoomMetadataUiTest,
       DirectFormGeometryAtLaptopWidthsAndFontSizes) {
  for (const float font_size : {13.0f, 16.0f}) {
    ImGui::GetIO().FontGlobalScale = font_size / 13.0f;
    for (const float width : {260.0f, 320.0f}) {
      width_ = width;
      ScrollTo(0);
      const auto prefix = "width_" + std::to_string(static_cast<int>(width)) +
                          "_font_" +
                          std::to_string(static_cast<int>(font_size));
      SCOPED_TRACE(prefix);
      std::ostringstream metrics;
      metrics << "content_height=" << form_height_;
      const auto destination = Widget("Open/4");
      ASSERT_TRUE(destination);
      metrics << " last_destination_bottom=" << destination->bounds.max_y - 20;
      metrics << " minimum_hex_width=" << rendered_hex_width_;
      for (size_t i = 0; i < 6; ++i) {
        const auto widget = Widget(kRoomPropertyFields[i]);
        ASSERT_TRUE(widget) << kRoomPropertyFields[i];
        EXPECT_TRUE(widget->visible) << kRoomPropertyFields[i];
        const float input_width = widget->bounds.max_x - widget->bounds.min_x;
        metrics << " " << kRoomPropertyFields[i] << "=" << input_width;
        EXPECT_GE(input_width, rendered_hex_width_) << kRoomPropertyFields[i];
        EXPECT_GE(widget->bounds.min_x, 20);
        EXPECT_LE(widget->bounds.max_x, 20 + width_);
        if (i % 2 == 1) {
          const auto previous = Widget(kRoomPropertyFields[i - 1]);
          ASSERT_TRUE(previous);
          EXPECT_FLOAT_EQ(previous->bounds.min_y, widget->bounds.min_y);
          constexpr const char* right_labels[] = {"Blockset", "Floor 2",
                                                  "Spriteset"};
          EXPECT_GE(widget->bounds.min_x - previous->bounds.max_x,
                    ImGui::CalcTextSize(right_labels[i / 2]).x);
        }
      }
      RecordProperty(prefix, metrics.str());
      EXPECT_FALSE(Widget("RoomDetails"));
      EXPECT_EQ(logged_text_.find("Room matches ROM buffer"),
                std::string::npos);
    }
  }
}

TEST_F(DungeonRoomMetadataUiTest, BothVisibleTagSelectorsUseTypedEditBoundary) {
  DrawFrame();
  DrawFrame();
  Click("RoomHeaderTag1");
  DrawFrame();
  Click("RoomHeaderTag1/1");
  Click("RoomHeaderTag2");
  DrawFrame();
  Click("RoomHeaderTag2/2");
  ASSERT_EQ(edits_.size(), 2u);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kTag1);
  EXPECT_EQ(edits_[1].request.field, RoomMetadataField::kTag2);
  EXPECT_EQ(rooms_[0].tag1(), 1);
  EXPECT_EQ(rooms_[0].tag2(), 2);
  EXPECT_EQ(rooms_[1].tag1(), zelda3::Nothing);
  EXPECT_EQ(rooms_[1].tag2(), zelda3::Nothing);
  EXPECT_TRUE(rooms_[0].header_dirty());
}

TEST_F(DungeonRoomMetadataUiTest,
       NamedChoicesUseFullWidthAtNarrowWidthsAndKeepWideRowsCompact) {
  for (const float font_size : {13.0f, 16.0f}) {
    ImGui::GetIO().FontGlobalScale = font_size / 13.0f;
    for (const float width : {260.0f, 320.0f, 500.0f}) {
      width_ = width;
      ScrollTo(0);
      const auto* host = ImGui::FindWindowByName("MetadataHost");
      ASSERT_NE(host, nullptr);
      const float available = host->WorkRect.GetWidth();
      const bool stacked = available < ImGui::GetFontSize() * 25.0f;
      for (const char* field :
           {"RoomHeaderTag1", "RoomHeaderTag2", "RoomHeaderBg2",
            "RoomHeaderEffect", "RoomHeaderCollision"}) {
        Reveal(field);
        const auto widget = Widget(field);
        ASSERT_TRUE(widget) << field;
        EXPECT_TRUE(widget->visible) << field;
        EXPECT_GE(widget->bounds.min_x, host->WorkRect.Min.x - 1) << field;
        EXPECT_LE(widget->bounds.max_x, host->WorkRect.Max.x + 1) << field;
        const float field_width = widget->bounds.max_x - widget->bounds.min_x;
        if (stacked)
          EXPECT_GE(field_width, available - 2) << field;
        else
          EXPECT_LT(field_width, available - ImGui::CalcTextSize("Tag 1").x);
      }
    }
  }
  EXPECT_TRUE(edits_.empty());
}

TEST_F(DungeonRoomMetadataUiTest,
       NameOnlyChoicesKeepDuplicateTagIdentityAndDistinctLayerModes) {
  viewer_.SetProject(&project_);
  project_.resource_labels["room_tag"]["1"] = "Shared behavior";
  project_.resource_labels["room_tag"]["2"] = "Shared behavior";
  DrawFrame();
  DrawFrame();
  Click("RoomHeaderTag1");
  DrawFrame();
  const auto first = Widget("RoomHeaderTag1/1");
  const auto second = Widget("RoomHeaderTag1/2");
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_NE(first->imgui_id, second->imgui_id);
  EXPECT_NE(first->bounds.min_y, second->bounds.min_y);
  EXPECT_EQ(logged_text_.find("01  Shared behavior"), std::string::npos);
  Click("RoomHeaderTag1/2");
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(edits_.back().request.field, RoomMetadataField::kTag1);
  EXPECT_EQ(edits_.back().request.value, 2);
  EXPECT_EQ(rooms_[0].tag1(), 2);

  Click("RoomHeaderEffect");
  DrawFrame();
  EXPECT_NE(logged_text_.find("One"), std::string::npos);
  EXPECT_EQ(logged_text_.find("01  One"), std::string::npos);
  Click("RoomHeaderEffect/1");
  ASSERT_EQ(edits_.size(), 2u);
  EXPECT_EQ(edits_.back().request.field, RoomMetadataField::kEffect);
  EXPECT_EQ(rooms_[0].effect(), 1);
  Click("RoomHeaderEffect");
  // Reopening restores the popup position on its next layout frame.
  DrawFrame();
  DrawFrame();
  Click("RoomHeaderEffect/0");
  ASSERT_EQ(edits_.size(), 3u);
  EXPECT_EQ(edits_.back().request.value, 0);
  EXPECT_EQ(rooms_[0].effect(), 0);
  EXPECT_EQ(rooms_[1].effect(), 0);
}

TEST_F(DungeonRoomMetadataUiTest,
       VisibleTagSelectorsRespectReadOnlyAndRejection) {
  reject_ = true;
  DrawFrame();
  DrawFrame();
  Click("RoomHeaderTag1");
  DrawFrame();
  Click("RoomHeaderTag1/1");
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(rooms_[0].tag1(), zelda3::Nothing);
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  DrawFrame();
  EXPECT_NE(logged_text_.find("Edit not applied"), std::string::npos);
  viewer_.SetHeaderReadOnly(true);
  DrawFrame();
  ASSERT_TRUE(Widget("RoomHeaderTag1"));
  ASSERT_TRUE(Widget("RoomHeaderTag2"));
  EXPECT_FALSE(Widget("RoomHeaderTag1")->enabled);
  EXPECT_FALSE(Widget("RoomHeaderTag2")->enabled);
}

TEST_F(DungeonRoomMetadataUiTest, InspectPitActionUsesExistingMembershipSwap) {
  DrawFrame();
  DrawFrame();
  EXPECT_FALSE(
      content_.GetPitDamageControlRectsForTesting().replace_current.visible);
  Click("PitDamageEnabled");
  DrawFrame();
  const auto rect =
      content_.GetPitDamageControlRectsForTesting().replace_current;
  ASSERT_TRUE(rect.visible);
  const float x = (rect.min_x + rect.max_x) / 2;
  const float y = (rect.min_y + rect.max_y) / 2;
  DrawFrame([&](ImGuiIO& io) { io.AddMousePosEvent(x, y); });
  DrawFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
  DrawFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
  EXPECT_FALSE(pit_table_.Contains(0));
  EXPECT_TRUE(pit_table_.Contains(1));
  EXPECT_TRUE(pit_table_.dirty());
  EXPECT_TRUE(edits_.empty());
  DrawFrame();
  EXPECT_FALSE(Widget("PitDamageDetails"));
  EXPECT_FALSE(
      content_.GetPitDamageControlRectsForTesting().add_current.visible);
  EXPECT_FALSE(
      content_.GetPitDamageControlRectsForTesting().replace_current.visible);
  EXPECT_NE(logged_text_.find("Pending pit damage change"), std::string::npos);
}

TEST_F(DungeonRoomMetadataUiTest,
       PairedRoomPropertiesKeepIndependentEditTargets) {
  width_ = 260;
  Type("RoomHeaderFloor1", "0B");
  Enter();
  Type("RoomHeaderFloor2", "0C");
  Enter();
  ASSERT_EQ(edits_.size(), 2u);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kFloor1);
  EXPECT_EQ(edits_[1].request.field, RoomMetadataField::kFloor2);
  EXPECT_EQ(rooms_[0].floor1(), 0x0B);
  EXPECT_EQ(rooms_[0].floor2(), 0x0C);
  EXPECT_EQ(rooms_[1].floor1(), 0);
  EXPECT_EQ(rooms_[1].floor2(), 0);
}

TEST_F(DungeonRoomMetadataUiTest, DirectRoomPropertiesRespectReadOnly) {
  viewer_.SetHeaderReadOnly(true);
  DrawFrame();
  DrawFrame();
  for (const auto* field : kRoomPropertyFields) {
    Reveal(field);
    const auto widget = Widget(field);
    ASSERT_TRUE(widget) << field;
    EXPECT_TRUE(widget->visible) << field;
    EXPECT_FALSE(widget->enabled) << field;
  }
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
}

TEST_F(DungeonRoomMetadataUiTest, RoutineRoomStatusDoesNotAddASection) {
  DrawFrame();
  DrawFrame();
  EXPECT_EQ(logged_text_.find("Room matches ROM buffer"), std::string::npos);
  EXPECT_FALSE(Widget("RoomDetails"));
  rooms_[0].set_floor1(1);
  DrawFrame();
  EXPECT_EQ(logged_text_.find("Room matches ROM buffer"), std::string::npos);
  EXPECT_FALSE(Widget("RoomDetails"));
  EXPECT_TRUE(edits_.empty());
}

TEST_F(DungeonRoomMetadataUiTest, SharedCompactFieldsUseValidatedEdits) {
  show_compact_properties_ = true;
  width_ = 260;
  Type("BS", "03");
  EXPECT_TRUE(edits_.empty());
  Enter();
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kBlockset);
  EXPECT_EQ(rooms_[0].blockset(), 3);
  EXPECT_EQ(rooms_[1].blockset(), 0);

  viewer_.SetHeaderReadOnly(true);
  DrawFrame();
  for (const char* field : {"BS", "Pal", "Lyr", "SS"}) {
    const auto widget = Widget(field);
    ASSERT_TRUE(widget) << field;
    EXPECT_FALSE(widget->enabled) << field;
    EXPECT_GE(widget->bounds.min_x, 20);
    EXPECT_LE(widget->bounds.max_x, 20 + width_);
  }
}

TEST_F(DungeonRoomMetadataUiTest, SharedCompactPendingEditKeepsRoomIdentity) {
  show_compact_properties_ = true;
  Type("BS", "03");
  ASSERT_TRUE(edits_.empty());
  room_id_ = 1;
  viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, room_id_);
  DrawFrame();
  Enter();
  EXPECT_TRUE(edits_.empty());
  EXPECT_EQ(rooms_[0].blockset(), 0);
  EXPECT_EQ(rooms_[1].blockset(), 0);
}

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
  Type("Room/3", "A5");
  Enter();
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircaseRoom);
  EXPECT_EQ(edits_[0].request.index, 2);
  EXPECT_EQ(rooms_[0].staircase_room(2), 0xA5);
  EXPECT_EQ(rooms_[0].staircase_room(0), 0);
}

TEST_F(DungeonRoomMetadataUiTest,
       EveryVisibleDestinationEditsItsOwnHeaderField) {
  for (int row = 0; row < 5; ++row) {
    const auto label = "Room/" + std::to_string(row);
    const auto value = std::to_string(10 + row);
    Type(label.c_str(), value.c_str());
    Enter();
    ASSERT_EQ(edits_.size(), row + 1);
    EXPECT_EQ(edits_.back().request.field,
              row == 0 ? RoomMetadataField::kHolewarp
                       : RoomMetadataField::kStaircaseRoom);
    EXPECT_EQ(edits_.back().request.index, row == 0 ? 0 : row - 1);
  }
  EXPECT_EQ(rooms_[0].holewarp(), 0x10);
  for (int slot = 0; slot < 4; ++slot)
    EXPECT_EQ(rooms_[0].staircase_room(slot), 0x11 + slot);
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
  Click("Plane/0");
  DrawFrame();
  EXPECT_FALSE(Widget("Plane/0/3"));
  Click("Plane/0/2");
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
  EXPECT_NE(logged_text_.find("03 Unmapped"), std::string::npos);
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
  ASSERT_TRUE(Widget("Room/0"));
  EXPECT_FALSE(Widget("Room/0")->enabled);
  EXPECT_FALSE(Widget("Plane/0")->enabled);
  Click("Open/0");
  EXPECT_EQ(navigated, 0);
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
}

TEST_F(DungeonRoomMetadataUiTest, DestinationRejectsOverflowWithoutTruncation) {
  show_destinations_ = true;
  Type("Room/0", "100");
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
  Click("Plane/4");
  DrawFrame();
  Click("Plane/4/1");
  ASSERT_EQ(edits_.size(), 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircasePlane);
  EXPECT_EQ(edits_[0].request.index, 3);
  EXPECT_EQ(rooms_[0].staircase_plane(3), 1);
  EXPECT_EQ(rooms_[0].staircase_plane(0), 0);
  EXPECT_EQ(rooms_[0].CaptureMetadataSnapshot().pit_target_layer, 0);
}

TEST_F(DungeonRoomMetadataUiTest,
       SelectedStairLookupIdentifiesSlotWithoutHidingOtherRows) {
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
  EXPECT_NE(logged_text_.find("header slot 2"), std::string::npos);
  EXPECT_TRUE(edits_.empty());
  EXPECT_FALSE(rooms_[0].HasUnsavedChanges());
  Click("Plane/2");
  DrawFrame();
  Click("Plane/2/1");
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(edits_[0].request.index, 1);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircasePlane);
}

TEST_F(DungeonRoomMetadataUiTest,
       MovingSelectedStairWarnsWithoutRetargetingExplicitHeaderRows) {
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
  ASSERT_NE(logged_text_.find("Selected stair: header slot"),
            std::string::npos);
  ASSERT_NE(logged_text_.find("header slot 2"), std::string::npos);

  auto moved = rooms_[0].GetTileObjects()[0];
  moved.set_x(11);
  ASSERT_TRUE(rooms_[0].UpdateObject(0, moved).ok());
  DrawFrame();
  EXPECT_EQ(logged_text_.find("Selected stair: header slot"),
            std::string::npos);
  EXPECT_NE(logged_text_.find("Selected stair mapping is stale"),
            std::string::npos);
  ASSERT_TRUE(Widget("Room/2"));
  EXPECT_TRUE(Widget("Room/2")->enabled);
  ASSERT_TRUE(Widget("Plane/2"));
  EXPECT_TRUE(Widget("Plane/2")->enabled);
  EXPECT_TRUE(edits_.empty());

  // Explicit slot 3 stays usable even when selected-object mapping is stale.
  Click("Plane/3");
  DrawFrame();
  Click("Plane/3/1");
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircasePlane);
  EXPECT_EQ(edits_[0].request.index, 2);
}

TEST_F(DungeonRoomMetadataUiTest,
       CustomObjectModeChangeStalesSelectedStairLookup) {
  struct RestoreCustomMode {
    bool previous = core::FeatureFlags::get().kEnableCustomObjects;
    ~RestoreCustomMode() {
      core::FeatureFlags::get().kEnableCustomObjects = previous;
    }
  } restore_custom_mode;
  core::FeatureFlags::get().kEnableCustomObjects = false;
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
  ASSERT_NE(logged_text_.find("Selected stair: header slot"),
            std::string::npos);

  core::FeatureFlags::get().kEnableCustomObjects = true;
  DrawFrame();
  EXPECT_NE(logged_text_.find("Selected stair mapping is stale"),
            std::string::npos);
  ASSERT_TRUE(Widget("Room/0"));
  EXPECT_TRUE(Widget("Room/0")->enabled);
  ASSERT_TRUE(Widget("Plane/0"));
  EXPECT_TRUE(Widget("Plane/0")->enabled);
  EXPECT_TRUE(edits_.empty());
}

TEST_F(DungeonRoomMetadataUiTest,
       DestinationEditKeepsInspectedStairSlotForNextPlaneEdit) {
  simulate_product_refresh_ = true;
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
  ASSERT_NE(logged_text_.find("header slot 2"), std::string::npos);

  Type("Room/2", "12");
  Enter();
  ASSERT_EQ(edits_.size(), 1u);
  EXPECT_EQ(edits_[0].request.field, RoomMetadataField::kStaircaseRoom);
  EXPECT_EQ(edits_[0].request.index, 1);
  DrawFrame();
  EXPECT_TRUE(Widget("Plane/2")->enabled);
  EXPECT_EQ(logged_text_.find("Selected stair mapping is stale"),
            std::string::npos);
  Click("Plane/2");
  DrawFrame();
  Click("Plane/2/1");
  ASSERT_EQ(edits_.size(), 2u);
  EXPECT_EQ(edits_[1].request.field, RoomMetadataField::kStaircasePlane);
  EXPECT_EQ(edits_[1].request.index, 1);
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
  width_ = 380;
  height_ = 1300;
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
