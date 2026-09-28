#include "app/editor/overworld/maps/map_properties.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/overworld_editor.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {

struct ScopedTempDir {
  std::filesystem::path path;
  explicit ScopedTempDir(std::filesystem::path p) : path(std::move(p)) {
    std::filesystem::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

std::filesystem::path MakeTempDir(const std::string& stem) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (stem + "_" + std::to_string(nonce));
}

void WriteRomFile(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

gui::CanvasMenuItem* FindMenuItem(std::vector<gui::CanvasMenuItem>& items,
                                  const std::string& label_fragment) {
  for (auto& item : items) {
    if (item.label.find(label_fragment) != std::string::npos) {
      return &item;
    }
    if (auto* nested = FindMenuItem(item.subitems, label_fragment)) {
      return nested;
    }
  }
  return nullptr;
}

gui::CanvasMenuItem* FindMenuItem(gui::Canvas& canvas,
                                  const std::string& label_fragment) {
  for (auto& section : canvas.editor_menu().sections) {
    if (auto* found = FindMenuItem(section.items, label_fragment)) {
      return found;
    }
  }
  return nullptr;
}

gui::CanvasMenuItem* FindRootMenuItem(gui::Canvas& canvas,
                                      const std::string& label_fragment) {
  for (auto& section : canvas.editor_menu().sections) {
    for (auto& item : section.items) {
      if (item.label.find(label_fragment) != std::string::npos) {
        return &item;
      }
    }
  }
  return nullptr;
}

void ExpectTarget(const OverworldContextTarget& actual,
                  const OverworldContextTarget& expected) {
  EXPECT_EQ(actual.map_id, expected.map_id);
  EXPECT_EQ(actual.parent_map_id, expected.parent_map_id);
  EXPECT_EQ(actual.world, expected.world);
  EXPECT_EQ(actual.game_state, expected.game_state);
  EXPECT_FLOAT_EQ(actual.world_position.x, expected.world_position.x);
  EXPECT_FLOAT_EQ(actual.world_position.y, expected.world_position.y);
  EXPECT_EQ(actual.tile16_id, expected.tile16_id);
}

class MapPropertiesContextMenuTest : public ::testing::Test {
 protected:
  void SetUp() override {
    temp_dir_ =
        std::make_unique<ScopedTempDir>(MakeTempDir("yaze_context_target"));
    const auto path = temp_dir_->path / "test.sfc";
    WriteRomFile(path);
    ASSERT_TRUE(rom_.LoadFromFile(path.string()).ok());
    canvas_.Init("map_properties_test_canvas", ImVec2(512, 512));
    target_.map_id = 0x4B;
    target_.parent_map_id = 0x42;
    target_.world = 1;
    target_.game_state = 2;
    target_.world_position = ImVec2(1576, 536);
    target_.tile16_id = 0x123;
    system_.SetMapSelectionCallback([this](int map_id, bool respect_pin) {
      selected_map_ = map_id;
      respected_pin_ = respect_pin;
      ++selection_calls_;
    });
    system_.SetOpenMapPropertiesCallback([this]() { show_properties_ = true; });
  }

  void Build(int mode = 0) {
    system_.SetupCanvasContextMenu(canvas_, target_, map_lock_, mode);
  }

  void Invoke(const std::string& label) {
    auto* item = FindMenuItem(canvas_, label);
    ASSERT_NE(item, nullptr) << label;
    ASSERT_TRUE(item->enabled_condition()) << label;
    ASSERT_TRUE(item->callback) << label;
    item->callback();
  }

  Rom rom_;
  gui::Canvas canvas_;
  MapPropertiesSystem system_{nullptr, &rom_};
  std::unique_ptr<ScopedTempDir> temp_dir_;
  OverworldContextTarget target_;
  bool map_lock_ = false;
  bool show_properties_ = false;
  int selected_map_ = 0x02;
  bool respected_pin_ = true;
  int selection_calls_ = 0;
};

TEST_F(MapPropertiesContextMenuTest, LockItemMutatesReferencedMapLockState) {
  Build();
  auto* pin = FindMenuItem(canvas_, "Pin Map");
  ASSERT_NE(pin, nullptr);
  ASSERT_TRUE(pin->checked_condition);
  EXPECT_FALSE(pin->checked_condition());
  Invoke("Pin Map");
  EXPECT_TRUE(map_lock_);
  EXPECT_TRUE(pin->checked_condition());
  EXPECT_EQ(selected_map_, 0x4B);
  EXPECT_FALSE(respected_pin_);
  Build();
  Invoke("Pin Map");
  EXPECT_FALSE(map_lock_);
  EXPECT_EQ(selection_calls_, 1);
}

TEST_F(MapPropertiesContextMenuTest,
       MapPropertiesSelectsCapturedMapBeforeOpeningPanel) {
  bool selected_before_open = false;
  system_.SetMapSelectionCallback([&](int map_id, bool respect_pin) {
    selected_before_open = !show_properties_;
    selected_map_ = map_id;
    EXPECT_FALSE(respect_pin);
  });
  Build();
  auto* item = FindMenuItem(canvas_, "Map Properties");
  ASSERT_NE(item, nullptr);
  auto callback = item->callback;
  ASSERT_TRUE(callback);
  target_.map_id = 0x19;
  selected_map_ = 0x07;
  callback();
  EXPECT_TRUE(selected_before_open);
  EXPECT_EQ(selected_map_, 0x4B);
  EXPECT_TRUE(show_properties_);
}

TEST_F(MapPropertiesContextMenuTest,
       StoredEntitySampleAndEditCallbacksRetainCompleteTarget) {
  const auto expected = target_;
  std::vector<std::string> inserted_types;
  std::vector<OverworldContextTarget> inserted_targets;
  std::vector<OverworldContextTarget> sampled_targets;
  std::vector<OverworldContextTarget> edited_targets;
  system_.SetEntityCallbacks(
      [&](const std::string& type, const OverworldContextTarget& target) {
        inserted_types.push_back(type);
        inserted_targets.push_back(target);
      });
  system_.SetTile16SampleCallback([&](const OverworldContextTarget& target) {
    sampled_targets.push_back(target);
    return true;
  });
  system_.SetTile16EditCallback([&](const OverworldContextTarget& target) {
    edited_targets.push_back(target);
  });
  Build(1);  // Explicit context actions also work while using the brush tool.
  std::vector<std::function<void()>> callbacks;
  for (const auto* label : {"Entrance", "Hole", "Exit", "Item", "Sprite",
                            "Sample Tile16", "Edit Tile16"}) {
    auto* item = FindMenuItem(canvas_, label);
    ASSERT_NE(item, nullptr) << label;
    ASSERT_TRUE(item->enabled_condition()) << label;
    ASSERT_TRUE(item->callback) << label;
    callbacks.push_back(item->callback);
  }
  target_ = {};
  target_.map_id = 0x13;
  target_.parent_map_id = 0x13;
  target_.world_position = ImVec2(1608, 1056);
  target_.tile16_id = 7;
  selected_map_ = 0x13;
  Build();  // Stored actions must also survive replacement of menu items.
  for (const auto& callback : callbacks) {
    callback();
  }
  EXPECT_EQ(inserted_types, (std::vector<std::string>{
                                "entrance", "hole", "exit", "item", "sprite"}));
  ASSERT_EQ(inserted_targets.size(), 5);
  for (const auto& target : inserted_targets) {
    ExpectTarget(target, expected);
  }
  ASSERT_EQ(sampled_targets.size(), 1);
  ASSERT_EQ(edited_targets.size(), 1);
  ExpectTarget(sampled_targets[0], expected);
  ExpectTarget(edited_targets[0], expected);
}

TEST_F(MapPropertiesContextMenuTest,
       SampleAndEditRemainAvailableWithoutEntityInsertionCallback) {
  int sample_calls = 0;
  int edit_calls = 0;
  system_.SetTile16SampleCallback([&](const OverworldContextTarget& target) {
    ++sample_calls;
    ExpectTarget(target, target_);
    return true;
  });
  system_.SetTile16EditCallback([&](const OverworldContextTarget& target) {
    ++edit_calls;
    ExpectTarget(target, target_);
  });
  Build(1);
  Invoke("Sample Tile16");
  Invoke("Edit Tile16");
  EXPECT_EQ(sample_calls, 1);
  EXPECT_EQ(edit_calls, 1);
}

TEST_F(MapPropertiesContextMenuTest, UnavailableActionsAreDisabled) {
  system_.SetMapSelectionCallback({});
  Build();
  for (const auto* label :
       {"Select This Map", "Pin Map", "Map Properties", "Sample Tile16",
        "Edit Tile16", "Insert", "Zoom In", "Zoom Out"}) {
    auto* item = FindMenuItem(canvas_, label);
    ASSERT_NE(item, nullptr) << label;
    EXPECT_FALSE(item->enabled_condition()) << label;
  }
}

TEST_F(MapPropertiesContextMenuTest,
       MissingCapturedTileDisablesTileActionsEvenWithCallbacks) {
  system_.SetTile16SampleCallback(
      [](const OverworldContextTarget&) { return true; });
  system_.SetTile16EditCallback([](const OverworldContextTarget&) {});
  target_.tile16_id = -1;
  Build();
  for (const auto* label : {"Sample Tile16", "Edit Tile16"}) {
    auto* item = FindMenuItem(canvas_, label);
    ASSERT_NE(item, nullptr);
    EXPECT_FALSE(item->enabled_condition());
  }
}

TEST_F(MapPropertiesContextMenuTest, ViewSubmenuHoldsZoomAndToolbarToggles) {
  int reset_calls = 0;
  int zoom_in_calls = 0;
  int zoom_out_calls = 0;
  system_.SetContextNavigationCallbacks([&] { ++reset_calls; },
                                        [&] { ++zoom_in_calls; },
                                        [&] { ++zoom_out_calls; });
  bool grid = true;
  bool entities = false;
  bool overlay = true;
  using Toggle = MapPropertiesSystem::ContextViewToggle;
  system_.SetContextViewToggles(
      Toggle{[&] { return grid; }, [&] { grid = !grid; }},
      Toggle{[&] { return entities; }, [&] { entities = !entities; }},
      Toggle{[&] { return overlay; }, [&] { overlay = !overlay; }});
  Build();
  EXPECT_FALSE(canvas_.GetConfig().show_builtin_context_menu);
  auto* view = FindRootMenuItem(canvas_, "View");
  ASSERT_NE(view, nullptr);
  for (const auto* label :
       {"Zoom In", "Zoom Out", "Grid", "Entities", "Overlay Preview"}) {
    EXPECT_EQ(FindRootMenuItem(canvas_, label), nullptr) << label;
    EXPECT_NE(FindMenuItem(view->subitems, label), nullptr) << label;
  }
  // Removed: they did nothing on this canvas or duplicated the panel.
  for (const auto* label :
       {"Reset View", "Show Hex Labels", "Grid Size", "Custom Background Color",
        "Visual Effects", "Rename Map Label"}) {
    EXPECT_EQ(FindMenuItem(canvas_, label), nullptr) << label;
  }
  Invoke("Zoom In");
  Invoke("Zoom Out");
  EXPECT_EQ(zoom_in_calls, 1);
  EXPECT_EQ(zoom_out_calls, 1);
  EXPECT_EQ(reset_calls, 0);

  auto* grid_item = FindMenuItem(view->subitems, "Grid");
  ASSERT_TRUE(grid_item->checked_condition);
  EXPECT_TRUE(grid_item->checked_condition());
  Invoke("Grid");
  EXPECT_FALSE(grid);
  EXPECT_FALSE(grid_item->checked_condition());
  Invoke("Entities");
  EXPECT_TRUE(entities);
  Invoke("Overlay Preview");
  EXPECT_FALSE(overlay);
}

TEST_F(MapPropertiesContextMenuTest, TopLevelOrderAndCount) {
  system_.SetTile16SampleCallback(
      [](const OverworldContextTarget&) { return true; });
  system_.SetTile16EditCallback([](const OverworldContextTarget&) {});
  system_.SetEntityCallbacks(
      [](const std::string&, const OverworldContextTarget&) {});
  Build();
  std::vector<std::string> labels;
  for (auto& section : canvas_.editor_menu().sections) {
    for (auto& item : section.items) {
      labels.push_back(item.label);
    }
  }
  // Header rows (map, tile) come first; no overworld here, so the map row
  // is the bare ID and there are no Related Maps or clipboard rows.
  const std::vector<std::string> expected{"0x4B",
                                          "Tile16 0x123 | (2, 1)",
                                          "Sample Tile16",
                                          "Edit Tile16...",
                                          "Select This Map",
                                          "Map Properties...",
                                          "Pin Map",
                                          "Insert",
                                          "View"};
  EXPECT_EQ(labels, expected);
  auto* map_properties = FindRootMenuItem(canvas_, "Map Properties");
  ASSERT_NE(map_properties, nullptr);
  EXPECT_EQ(map_properties->shortcut, "Double-click");
}

TEST_F(MapPropertiesContextMenuTest, SelectThisMapHiddenWhenAlreadyCurrent) {
  int current = 0x4B;
  system_.SetCurrentMapProvider([&] { return current; });
  Build();
  auto* select = FindMenuItem(canvas_, "Select This Map");
  ASSERT_NE(select, nullptr);
  EXPECT_FALSE(select->visible_condition());
  current = 0x10;
  EXPECT_TRUE(select->visible_condition());
  Invoke("Select This Map");
  EXPECT_EQ(selected_map_, 0x4B);
  EXPECT_FALSE(respected_pin_);
}

TEST_F(MapPropertiesContextMenuTest,
       EntityActionQueuesCapturedTargetAndConsumesRequestOnce) {
  auto editor = std::make_unique<OverworldEditor>(&rom_);
  EXPECT_FALSE(editor->TakePendingEntityInsertion().has_value());
  system_.SetEntityCallbacks(
      [&](const std::string& type, const OverworldContextTarget& target) {
        editor->HandleEntityInsertion(type, target);
      });
  Build();
  const auto expected = target_;
  auto* insert = FindMenuItem(canvas_, "Sprite");
  ASSERT_NE(insert, nullptr);
  ASSERT_TRUE(insert->callback);
  const auto action = insert->callback;
  target_ = {};
  action();
  const auto request = editor->TakePendingEntityInsertion();
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->type, "sprite");
  ExpectTarget(request->target, expected);
  EXPECT_FALSE(editor->TakePendingEntityInsertion().has_value());
  EXPECT_FALSE(editor->overworld().is_loaded());
  EXPECT_FALSE(rom_.dirty());
}

TEST(OverworldContextTargetTest,
     ResolvesSameWorldPositionAcrossZoomScrollAndWorlds) {
  struct ViewSample {
    float scale;
    ImVec2 screen;
  };
  const std::vector<ViewSample> views{{0.5f, ImVec2(308.125f, 204.375f)},
                                      {1.0f, ImVec2(1096.25f, 472.75f)},
                                      {2.0f, ImVec2(2672.5f, 1009.5f)}};
  for (int world : {0, 1, 2}) {
    for (const auto& view : views) {
      SCOPED_TRACE(::testing::Message()
                   << "World " << world << ", scale " << view.scale);
      const auto target =
          ResolveOverworldContextTarget(world, 2, view.screen, ImVec2(120, 80),
                                        ImVec2(-600, -144), view.scale);
      ASSERT_TRUE(target.has_value());
      EXPECT_EQ(target->map_id, world * 64 + 11);
      EXPECT_EQ(target->parent_map_id, world * 64 + 11);
      EXPECT_EQ(target->world, world);
      EXPECT_EQ(target->game_state, 2);
      EXPECT_FLOAT_EQ(target->world_position.x, 1576.25f);
      EXPECT_FLOAT_EQ(target->world_position.y, 536.75f);
      EXPECT_EQ(target->tile16_id, -1);
      EXPECT_TRUE(target->valid());
    }
  }
}

TEST(OverworldContextTargetTest, RejectsPaddedSpecialRowsAndWorldEdges) {
  for (int world : {0, 1}) {
    const auto last = ResolveOverworldContextTarget(
        world, 0, ImVec2(4095, 4095), ImVec2(0, 0), ImVec2(0, 0), 1);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->map_id, world * 64 + 63);
  }
  const auto last_special = ResolveOverworldContextTarget(
      2, 0, ImVec2(4095, 2047), ImVec2(0, 0), ImVec2(0, 0), 1);
  ASSERT_TRUE(last_special.has_value());
  EXPECT_EQ(last_special->map_id, 159);
  EXPECT_FALSE(ResolveOverworldContextTarget(2, 0, ImVec2(4095, 2048),
                                             ImVec2(0, 0), ImVec2(0, 0), 1));
  for (const ImVec2 outside :
       {ImVec2(-0.5f, 0), ImVec2(0, -0.5f), ImVec2(4096, 0), ImVec2(0, 4096)}) {
    EXPECT_FALSE(ResolveOverworldContextTarget(0, 0, outside, ImVec2(0, 0),
                                               ImVec2(0, 0), 1));
  }
}

TEST(OverworldContextTargetTest, RejectsInvalidViewAndIdentityValues) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  for (float scale : {0.0f, -1.0f, nan, infinity}) {
    EXPECT_FALSE(ResolveOverworldContextTarget(
        0, 0, ImVec2(16, 16), ImVec2(0, 0), ImVec2(0, 0), scale));
  }
  for (const ImVec2 invalid : {ImVec2(nan, 0), ImVec2(0, infinity)}) {
    EXPECT_FALSE(ResolveOverworldContextTarget(0, 0, invalid, ImVec2(0, 0),
                                               ImVec2(0, 0), 1));
    EXPECT_FALSE(ResolveOverworldContextTarget(0, 0, ImVec2(16, 16), invalid,
                                               ImVec2(0, 0), 1));
    EXPECT_FALSE(ResolveOverworldContextTarget(0, 0, ImVec2(16, 16),
                                               ImVec2(0, 0), invalid, 1));
  }
  for (int invalid : {-1, 3}) {
    EXPECT_FALSE(ResolveOverworldContextTarget(invalid, 0, ImVec2(16, 16),
                                               ImVec2(0, 0), ImVec2(0, 0), 1));
    EXPECT_FALSE(ResolveOverworldContextTarget(0, invalid, ImVec2(16, 16),
                                               ImVec2(0, 0), ImVec2(0, 0), 1));
  }
  auto target = ResolveOverworldContextTarget(1, 2, ImVec2(1576, 536),
                                              ImVec2(0, 0), ImVec2(0, 0), 1);
  ASSERT_TRUE(target.has_value());
  target->map_id = 0x4A;
  EXPECT_FALSE(target->valid());
  target->map_id = 0x4B;
  target->parent_map_id = 0;
  EXPECT_FALSE(target->valid());
}

class CanvasContextMenuOpenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigInputTrickleEventQueue = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    canvas_ = std::make_unique<gui::Canvas>("CaptureCanvas", ImVec2(256, 256));
    canvas_->SetShowBuiltinContextMenu(false);
    canvas_->SetContextMenuOpenCallback([this](const ImVec2& position) {
      ++open_calls_;
      captured_position_ = position;
      if (!allow_open_) {
        return false;
      }
      const int captured_map = live_map_;
      canvas_->ClearContextMenuItems();
      gui::CanvasMenuItem item("Use Captured Map", [this, captured_map] {
        used_map_ = captured_map;
      });
      item.enabled_condition = [this, captured_map] {
        rendered_targets_.push_back(captured_map);
        return true;
      };
      canvas_->AddContextMenuItem(item);
      return true;
    });
    Frame(ImVec2(-100, -100));
    Frame(ImVec2(-100, -100));
  }

  void TearDown() override {
    canvas_.reset();
    ImGui::DestroyContext(context_);
  }

  void Frame(ImVec2 mouse, bool right_down = false, bool escape_down = false) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Right, right_down);
    io.AddKeyEvent(ImGuiKey_Escape, escape_down);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(80, 40), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Always);
    ImGui::Begin("ContextCaptureHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    if (no_padding_) {
      // What the overworld canvas does around BeginCanvas.
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    }
    canvas_->DrawBackground(ImVec2(256, 256));
    ImGui::LogToBuffer();
    canvas_->DrawContextMenu();
    rendered_text_ = context_->LogBuffer.c_str();
    ImGui::LogFinish();
    if (no_padding_) {
      ImGui::PopStyleVar(2);
    }
    popup_open_ = ImGui::IsPopupOpen("CaptureCanvasContext");
    popup_padding_ = ImVec2(-1, -1);
    for (ImGuiWindow* window : context_->Windows) {
      if (window->Active && (window->Flags & ImGuiWindowFlags_Popup) &&
          std::string(window->Name).find("##Popup_") == 0) {
        popup_padding_ = window->WindowPadding;
      }
    }
    ImGui::End();
    ImGui::Render();
  }

  ImVec2 At(float x, float y) const {
    return ImVec2(canvas_->zero_point().x + x, canvas_->zero_point().y + y);
  }

  void Open(ImVec2 position) {
    Frame(position);
    Frame(position, true);
    Frame(position);
  }

  ImGuiContext* context_ = nullptr;
  std::unique_ptr<gui::Canvas> canvas_;
  int live_map_ = 0x4B;
  int used_map_ = -1;
  int open_calls_ = 0;
  bool allow_open_ = true;
  bool popup_open_ = false;
  bool no_padding_ = false;
  ImVec2 popup_padding_{-1, -1};
  ImVec2 captured_position_{};
  std::vector<int> rendered_targets_;
  std::string rendered_text_;
};

TEST_F(CanvasContextMenuOpenTest, RespectsBuiltinMenuVisibilityAtRenderTime) {
  const ImVec2 position = At(64, 64);
  Open(position);
  Frame(position);
  ASSERT_TRUE(popup_open_);
  EXPECT_NE(rendered_text_.find("Use Captured Map"), std::string::npos);
  EXPECT_EQ(rendered_text_.find("View Controls"), std::string::npos);
  canvas_->SetShowBuiltinContextMenu(true);
  Frame(position);
  EXPECT_NE(rendered_text_.find("View Controls"), std::string::npos);
}

TEST_F(CanvasContextMenuOpenTest,
       CapturesOnceBeforeRenderingAndRetainsTargetUntilNextOpen) {
  canvas_->set_global_scale(1.5f);
  canvas_->set_scrolling(ImVec2(-300, -120));
  const ImVec2 first_position = At(64, 64);
  Open(first_position);
  ASSERT_TRUE(popup_open_);
  ASSERT_EQ(open_calls_, 1);
  ASSERT_FALSE(rendered_targets_.empty());
  EXPECT_EQ(rendered_targets_.front(), 0x4B);
  EXPECT_FLOAT_EQ(captured_position_.x, first_position.x);
  EXPECT_FLOAT_EQ(captured_position_.y, first_position.y);

  live_map_ = 0x13;
  canvas_->set_scrolling(ImVec2(-900, -700));
  Frame(At(96, 96));
  Frame(At(200, 200));
  ASSERT_TRUE(popup_open_);
  EXPECT_EQ(open_calls_, 1);
  for (int rendered_target : rendered_targets_) {
    EXPECT_EQ(rendered_target, 0x4B);
  }
  auto* action = FindMenuItem(*canvas_, "Use Captured Map");
  ASSERT_NE(action, nullptr);
  ASSERT_TRUE(action->callback);
  action->callback();
  EXPECT_EQ(used_map_, 0x4B);
  EXPECT_FLOAT_EQ(captured_position_.x, first_position.x);
  EXPECT_FLOAT_EQ(captured_position_.y, first_position.y);

  Frame(At(200, 200), false, true);
  Frame(At(200, 200));
  ASSERT_FALSE(popup_open_);
  const ImVec2 next_position = At(192, 160);
  Open(next_position);
  EXPECT_TRUE(popup_open_);
  EXPECT_EQ(open_calls_, 2);
  EXPECT_FLOAT_EQ(captured_position_.x, next_position.x);
  EXPECT_FLOAT_EQ(captured_position_.y, next_position.y);
  ASSERT_FALSE(rendered_targets_.empty());
  EXPECT_EQ(rendered_targets_.back(), 0x13);
}

// The overworld canvas draws inside zero WindowPadding/FramePadding; its
// context menu must still get the theme's popup padding.
TEST_F(CanvasContextMenuOpenTest, MenuKeepsThemePaddingInsideNoPaddingCanvas) {
  const ImVec2 theme_padding = ImGui::GetStyle().WindowPadding;
  ASSERT_GT(theme_padding.x, 0.0f);
  no_padding_ = true;
  const ImVec2 position = At(64, 64);
  Open(position);
  Frame(position);
  ASSERT_TRUE(popup_open_);
  EXPECT_FLOAT_EQ(popup_padding_.x, theme_padding.x);
  EXPECT_FLOAT_EQ(popup_padding_.y, theme_padding.y);
}

TEST_F(CanvasContextMenuOpenTest, CaptureCallbackCanVetoOpening) {
  allow_open_ = false;
  Open(At(64, 64));
  EXPECT_EQ(open_calls_, 1);
  EXPECT_FALSE(popup_open_);
  EXPECT_TRUE(rendered_targets_.empty());
  Frame(At(128, 128));
  EXPECT_EQ(open_calls_, 1);

  allow_open_ = true;
  Open(At(128, 128));
  EXPECT_EQ(open_calls_, 2);
  EXPECT_TRUE(popup_open_);
  EXPECT_FALSE(rendered_targets_.empty());
}

TEST_F(CanvasContextMenuOpenTest, OutsideClicksAndRightDragDoNotCaptureOrOpen) {
  Open(ImVec2(10, 10));
  EXPECT_EQ(open_calls_, 0);
  EXPECT_FALSE(popup_open_);

  Frame(At(32, 32));
  Frame(At(32, 32), true);
  Frame(At(128, 128), true);
  Frame(At(128, 128));
  EXPECT_EQ(open_calls_, 0);
  EXPECT_FALSE(popup_open_);
  EXPECT_TRUE(rendered_targets_.empty());
}

}  // namespace
}  // namespace yaze::editor
