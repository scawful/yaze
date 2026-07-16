#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/editor/dungeon/ui_constants.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas_pipelines.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/palette_debug.h"

namespace yaze::editor {

struct PingRectSnapshot {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct PendingScrollFrameSnapshot {
  ImVec2 canvas_origin;
  ImVec2 applied_scroll;
  ImVec2 background_origin;
  ImVec2 overlay_origin;
  std::pair<int, int> background_origin_room_pixel;
};

class DungeonCanvasViewerTestPeer {
 public:
  static absl::Status PrepareIssueReportPopup(
      DungeonCanvasViewer& viewer, const std::string& title,
      const std::string& summary, const std::string& kind_label,
      const std::string& diagnostics, int room_id, int default_category_index) {
    return viewer.PrepareIssueReportPopup(title, summary, kind_label,
                                          diagnostics, room_id,
                                          default_category_index);
  }

  static const std::string& last_log_path(const DungeonCanvasViewer& viewer) {
    return viewer.issue_report_popup_last_log_path_;
  }

  static std::string BuildDrawIssueReport(const DungeonCanvasViewer& viewer,
                                          const zelda3::Room& room,
                                          int room_id) {
    return viewer.BuildDrawIssueReport(room, room_id);
  }

  static std::string BuildSelectionIssueReport(
      const DungeonCanvasViewer& viewer, const zelda3::Room& room,
      int room_id) {
    return viewer.BuildSelectionIssueReport(room, room_id);
  }

  static DungeonCanvasViewer::ConnectedRoomGraphData BuildConnectedRoomGraph(
      DungeonCanvasViewer& viewer, int start_room_id) {
    return viewer.BuildConnectedRoomGraph(start_room_id);
  }

  static int ApplyConnectedStaircaseIssueAutoFixes(DungeonCanvasViewer& viewer,
                                                   int start_room_id) {
    return viewer.ApplyConnectedStaircaseIssueAutoFixes(start_room_id);
  }

  static size_t ChangePingRectCount(const DungeonCanvasViewer& viewer) {
    return viewer.change_ping_rects_.size();
  }

  static PingRectSnapshot ChangePingRectAt(const DungeonCanvasViewer& viewer,
                                           size_t index) {
    const auto& rect = viewer.change_ping_rects_.at(index);
    return PingRectSnapshot{rect.x, rect.y, rect.w, rect.h};
  }

  static double ChangePingStartTime(const DungeonCanvasViewer& viewer) {
    return viewer.change_ping_start_time_;
  }

  static std::vector<std::string> InteractionContextMenuLabels(
      DungeonCanvasViewer& viewer, int room_id) {
    viewer.canvas_.ClearContextMenuItems();
    viewer.AddInteractionContextMenuItems(room_id);
    std::vector<std::string> labels;
    for (const auto& section : viewer.canvas_.editor_menu().sections) {
      for (const auto& item : section.items) {
        labels.push_back(item.label);
      }
    }
    return labels;
  }

  static std::vector<std::string> SelectionContextMenuLabels(
      DungeonCanvasViewer& viewer, int room_id) {
    std::vector<std::string> labels;
    for (const auto& item : viewer.BuildSelectionContextMenuItems(room_id)) {
      labels.push_back(item.label);
    }
    return labels;
  }

  static PendingScrollFrameSnapshot ConsumePendingScrollAndCaptureTransforms(
      DungeonCanvasViewer& viewer, int tile_x, int tile_y, float scale,
      ImVec2 initial_scroll, ImVec2 canvas_size) {
    viewer.canvas_.set_global_scale(scale);
    viewer.canvas_.set_scrolling(initial_scroll);
    viewer.ScrollToTile(tile_x, tile_y);

    gui::CanvasFrameOptions frame_opts;
    frame_opts.canvas_size = canvas_size;
    frame_opts.draw_context_menu = false;
    frame_opts.draw_grid = false;
    frame_opts.draw_overlay = false;
    frame_opts.render_popups = false;

    auto canvas_rt = gui::BeginCanvas(viewer.canvas_, frame_opts);
    viewer.ConsumePendingCanvasScroll(canvas_rt);
    canvas_rt.scrolling = viewer.canvas_.scrolling();

    gfx::Bitmap bitmap(16, 16, 8, std::vector<uint8_t>(16 * 16, 0));
    bitmap.set_texture(&bitmap);
    const int bitmap_vertex_start = canvas_rt.draw_list->VtxBuffer.Size;
    viewer.canvas_.DrawBitmap(bitmap, 0, 0, scale, 255);

    PendingScrollFrameSnapshot snapshot;
    snapshot.canvas_origin = canvas_rt.canvas_p0;
    snapshot.applied_scroll = canvas_rt.scrolling;
    if (canvas_rt.draw_list->VtxBuffer.Size >= bitmap_vertex_start + 4) {
      snapshot.background_origin =
          canvas_rt.draw_list->VtxBuffer[bitmap_vertex_start].pos;
    }

    const DungeonCanvasTransform overlay_and_input_transform(
        canvas_rt.canvas_p0, canvas_rt.scrolling, canvas_rt.scale);
    snapshot.overlay_origin =
        overlay_and_input_transform.RoomPixelsToScreen(ImVec2(0, 0));
    snapshot.background_origin_room_pixel =
        overlay_and_input_transform.ScreenToRoomPixelCoordinates(
            snapshot.background_origin);

    gui::EndCanvas(viewer.canvas_, canvas_rt, frame_opts);
    return snapshot;
  }
};

namespace {

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::string& value) : name_(name) {
    const char* existing = std::getenv(name_.c_str());
    if (existing) {
      had_original_ = true;
      original_value_ = existing;
    }
    SetEnv(value);
  }

  ~ScopedEnvVar() {
    if (had_original_) {
      SetEnv(original_value_);
    } else {
      UnsetEnv();
    }
  }

 private:
  void SetEnv(const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name_.c_str(), value.c_str());
#else
    setenv(name_.c_str(), value.c_str(), 1);
#endif
  }

  void UnsetEnv() {
#if defined(_WIN32)
    _putenv_s(name_.c_str(), "");
#else
    unsetenv(name_.c_str());
#endif
  }

  std::string name_;
  std::string original_value_;
  bool had_original_ = false;
};

std::filesystem::path MakeTempHomeRoot() {
  return std::filesystem::temp_directory_path() /
         std::filesystem::path(
             "yaze_dungeon_canvas_viewer_test_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
}

struct ScopedImGuiContext {
  ImGuiContext* context = nullptr;

  ScopedImGuiContext() {
    context = ImGui::CreateContext();
    ImGui::GetIO().DisplaySize = ImVec2(1024, 768);
  }

  ~ScopedImGuiContext() {
    if (context != nullptr) {
      ImGui::DestroyContext(context);
    }
  }
};

size_t FindLabelIndex(const std::vector<std::string>& labels,
                      const std::string& label) {
  const auto it = std::find(labels.begin(), labels.end(), label);
  return it == labels.end() ? labels.size()
                            : static_cast<size_t>(it - labels.begin());
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

zelda3::Room::Door MakeDoor(zelda3::DoorDirection direction,
                            zelda3::DoorType type) {
  zelda3::Room::Door door{};
  door.direction = direction;
  door.type = type;
  door.position = 0;
  door.byte1 = 0;
  door.byte2 = static_cast<uint8_t>(type);
  return door;
}

std::vector<gfx::TileInfo> MakeObjectTiles(int count) {
  std::vector<gfx::TileInfo> tiles;
  tiles.reserve(count);
  for (int i = 0; i < count; ++i) {
    tiles.emplace_back(static_cast<uint16_t>(0x120 + i),
                       static_cast<uint8_t>(i % 8), false, (i % 2) == 0,
                       (i % 3) == 0);
  }
  return tiles;
}

void ClearRoomLinks(zelda3::Room* room) {
  for (int i = 0; i < 4; ++i) {
    room->SetStaircaseRoom(i, 0);
  }
  room->SetHolewarp(0);
}

}  // namespace

TEST(DungeonCanvasViewerNavigationTest, CanNavigateRoomsReflectsCallbacks) {
  DungeonCanvasViewer viewer;

  EXPECT_FALSE(viewer.CanNavigateRooms());

  viewer.SetRoomNavigationCallback([](int) {});
  EXPECT_TRUE(viewer.CanNavigateRooms());

  viewer.SetRoomNavigationCallback(nullptr);
  EXPECT_FALSE(viewer.CanNavigateRooms());

  viewer.SetRoomSwapCallback([](int, int) {});
  EXPECT_TRUE(viewer.CanNavigateRooms());
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomIgnoresInvalidTargets) {
  DungeonCanvasViewer viewer;
  int nav_calls = 0;
  int swap_calls = 0;
  viewer.SetRoomNavigationCallback([&](int) { ++nav_calls; });
  viewer.SetRoomSwapCallback([&](int, int) { ++swap_calls; });

  viewer.NavigateToRoom(-1);
  viewer.NavigateToRoom(zelda3::kNumberOfRooms);

  EXPECT_EQ(nav_calls, 0);
  EXPECT_EQ(swap_calls, 0);
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomPrefersSwapCallback) {
  DungeonCanvasViewer viewer;

  int nav_target = -1;
  int swap_old = -2;
  int swap_new = -2;
  viewer.SetRoomNavigationCallback([&](int target) { nav_target = target; });
  viewer.SetRoomSwapCallback([&](int old_room, int new_room) {
    swap_old = old_room;
    swap_new = new_room;
  });

  viewer.NavigateToRoom(0xA8);

  EXPECT_EQ(nav_target, -1);
  EXPECT_EQ(swap_old, -1);
  EXPECT_EQ(swap_new, 0xA8);
}

TEST(DungeonCanvasViewerNavigationTest, NavigateToRoomUsesNavigationFallback) {
  DungeonCanvasViewer viewer;

  int nav_target = -1;
  viewer.SetRoomNavigationCallback([&](int target) { nav_target = target; });

  viewer.NavigateToRoom(0xDA);

  EXPECT_EQ(nav_target, 0xDA);
}

TEST(DungeonCanvasViewerNavigationTest, ScrollToTileStoresPendingTarget) {
  DungeonCanvasViewer viewer;

  EXPECT_FALSE(viewer.HasPendingScrollTarget());

  viewer.ScrollToTile(12, 34);

  ASSERT_TRUE(viewer.HasPendingScrollTarget());
  ASSERT_TRUE(viewer.GetPendingScrollTarget().has_value());
  EXPECT_EQ(viewer.GetPendingScrollTarget()->first, 12);
  EXPECT_EQ(viewer.GetPendingScrollTarget()->second, 34);
}

TEST(DungeonCanvasViewerNavigationTest,
     PendingScrollAlignsBackgroundOverlayAndInputInSameFrame) {
  ScopedImGuiContext imgui;
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.0f / 60.0f;
  io.Fonts->AddFontDefault();
  unsigned char* font_pixels = nullptr;
  int font_width = 0;
  int font_height = 0;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(20.0f, 30.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(1300.0f, 1100.0f), ImGuiCond_Always);
  ImGui::Begin("PendingScrollCanvasHost", nullptr,
               ImGuiWindowFlags_NoSavedSettings);

  DungeonCanvasViewer viewer;
  const PendingScrollFrameSnapshot snapshot =
      DungeonCanvasViewerTestPeer::ConsumePendingScrollAndCaptureTransforms(
          viewer, 48, 40, 2.0f, ImVec2(-64.0f, -32.0f), ImVec2(512.0f, 512.0f));

  ImGui::End();
  ImGui::Render();

  EXPECT_FALSE(viewer.HasPendingScrollTarget());
  EXPECT_NEAR(snapshot.applied_scroll.x, -512.0f, 0.01f);
  EXPECT_NEAR(snapshot.applied_scroll.y, -384.0f, 0.01f);
  EXPECT_NEAR(snapshot.background_origin.x,
              snapshot.canvas_origin.x + snapshot.applied_scroll.x, 0.01f);
  EXPECT_NEAR(snapshot.background_origin.y,
              snapshot.canvas_origin.y + snapshot.applied_scroll.y, 0.01f);
  EXPECT_NEAR(snapshot.overlay_origin.x, snapshot.background_origin.x, 0.01f);
  EXPECT_NEAR(snapshot.overlay_origin.y, snapshot.background_origin.y, 0.01f);
  EXPECT_EQ(snapshot.background_origin_room_pixel, std::make_pair(0, 0));
}

TEST(DungeonCanvasViewerContextMenuTest, DirectActionsPrecedeInsertSubmenu) {
  ScopedImGuiContext imgui;
  DungeonCanvasViewer viewer;
  viewer.SetShowObjectPanelCallback([]() {});
  viewer.SetShowSpritePanelCallback([]() {});
  viewer.SetShowItemPanelCallback([]() {});

  const auto labels =
      DungeonCanvasViewerTestPeer::InteractionContextMenuLabels(viewer, 0);

  ASSERT_GE(labels.size(), 4u);
  EXPECT_EQ(labels[0], "Paste");
  EXPECT_EQ(labels[1], "Delete");
  EXPECT_EQ(labels[2], "Delete All");
  EXPECT_EQ(labels[3], "Insert");
}

TEST(DungeonCanvasViewerContextMenuTest,
     SelectedObjectUsesDirectZAndLayerLabels) {
  ScopedImGuiContext imgui;
  DungeonRoomStore rooms;
  rooms[0].GetTileObjects().push_back(
      zelda3::RoomObject{0x01, 10, 10, 0x00, 0});

  DungeonCanvasViewer viewer;
  viewer.SetRooms(&rooms);
  viewer.object_interaction().SetCurrentRoom(&rooms, 0);
  viewer.object_interaction().SetSelectedObjects({0});

  const auto labels =
      DungeonCanvasViewerTestPeer::SelectionContextMenuLabels(viewer, 0);

  const size_t bring_front = FindLabelIndex(labels, "Bring to Front");
  const size_t send_back = FindLabelIndex(labels, "Send to Back");
  const size_t z_order = FindLabelIndex(labels, "Z Order");
  const size_t layer_1 = FindLabelIndex(labels, "Layer 1");
  const size_t layer_3 = FindLabelIndex(labels, "Layer 3");

  ASSERT_LT(bring_front, labels.size());
  ASSERT_LT(send_back, labels.size());
  ASSERT_LT(z_order, labels.size());
  ASSERT_LT(layer_1, labels.size());
  ASSERT_LT(layer_3, labels.size());
  EXPECT_LT(bring_front, z_order);
  EXPECT_LT(send_back, z_order);
  EXPECT_LT(z_order, layer_1);
  EXPECT_LT(layer_1, layer_3);
  EXPECT_EQ(FindLabelIndex(labels, "Increase Z"), labels.size());
  EXPECT_EQ(FindLabelIndex(labels, "Send to Layer 1"), labels.size());
}

TEST(DungeonCanvasViewerPingTest, TriggerCanvasPingRectClampsToCanvasBounds) {
  DungeonCanvasViewer viewer;

  viewer.TriggerCanvasPingRect(510, -8, 64, 0);

  ASSERT_EQ(DungeonCanvasViewerTestPeer::ChangePingRectCount(viewer), 1u);
  const PingRectSnapshot rect =
      DungeonCanvasViewerTestPeer::ChangePingRectAt(viewer, 0);
  EXPECT_EQ(rect.x, 510);
  EXPECT_EQ(rect.y, 0);
  EXPECT_EQ(rect.w, 2);
  EXPECT_EQ(rect.h, 1);
  EXPECT_GE(DungeonCanvasViewerTestPeer::ChangePingStartTime(viewer), 0.0);
}

TEST(DungeonCanvasViewerPingTest,
     TriggerObjectChangePingIgnoresUnchangedSnapshots) {
  DungeonCanvasViewer viewer;
  const std::vector<zelda3::RoomObject> objects = {
      zelda3::RoomObject{0x01, 2, 3, 0x00, 0},
      zelda3::RoomObject{0x02, 4, 5, 0x00, 0},
  };

  viewer.TriggerObjectChangePing(objects, objects);

  EXPECT_EQ(DungeonCanvasViewerTestPeer::ChangePingRectCount(viewer), 0u);
  EXPECT_LT(DungeonCanvasViewerTestPeer::ChangePingStartTime(viewer), 0.0);
}

TEST(DungeonCanvasViewerPingTest,
     TriggerObjectChangePingCapturesPreviousAndNextChangedBounds) {
  DungeonCanvasViewer viewer;
  const std::vector<zelda3::RoomObject> before = {
      zelda3::RoomObject{0x01, 2, 3, 0x00, 0},
      zelda3::RoomObject{0x02, 4, 5, 0x00, 0},
  };
  const std::vector<zelda3::RoomObject> after = {
      zelda3::RoomObject{0x01, 2, 3, 0x00, 0},
      zelda3::RoomObject{0x02, 6, 5, 0x00, 0},
  };

  viewer.TriggerObjectChangePing(before, after);

  ASSERT_EQ(DungeonCanvasViewerTestPeer::ChangePingRectCount(viewer), 2u);
  const PingRectSnapshot before_rect =
      DungeonCanvasViewerTestPeer::ChangePingRectAt(viewer, 0);
  const PingRectSnapshot after_rect =
      DungeonCanvasViewerTestPeer::ChangePingRectAt(viewer, 1);
  EXPECT_GE(before_rect.w, 8);
  EXPECT_GE(before_rect.h, 8);
  EXPECT_GE(after_rect.w, 8);
  EXPECT_GE(after_rect.h, 8);
  EXPECT_GE(DungeonCanvasViewerTestPeer::ChangePingStartTime(viewer), 0.0);
}

TEST(DungeonCanvasViewerNavigationTest, EntranceRenderContextRoundTrips) {
  DungeonCanvasViewer viewer;

  EXPECT_EQ(viewer.current_entrance_id(), -1);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0xFF);

  viewer.SetEntranceRenderContext(/*entrance_id=*/0x12, /*blockset=*/0x2A);

  EXPECT_EQ(viewer.current_entrance_id(), 0x12);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0x2A);

  viewer.ClearEntranceRenderContext();

  EXPECT_EQ(viewer.current_entrance_id(), -1);
  EXPECT_EQ(viewer.current_entrance_blockset(), 0xFF);
}

TEST(DungeonCanvasViewerNavigationTest,
     OpeningIssueReportPersistsInitialDraft) {
  const std::filesystem::path temp_home = MakeTempHomeRoot();
  ASSERT_TRUE(std::filesystem::create_directories(temp_home));
  ScopedEnvVar scoped_home("HOME", temp_home.string());

  DungeonCanvasViewer viewer;
  const auto status = DungeonCanvasViewerTestPeer::PrepareIssueReportPopup(
      viewer, "Palette issue", "Room 001 palette mismatch",
      "Dungeon Palette Issue Report", "Room 0x001\nBG sheets: [1,2,3,4]", 0x01,
      0);
  ASSERT_TRUE(status.ok()) << status;

  const std::filesystem::path log_path =
      DungeonCanvasViewerTestPeer::last_log_path(viewer);
  ASSERT_FALSE(log_path.empty());
  ASSERT_TRUE(std::filesystem::exists(log_path));

  const std::string content = ReadFile(log_path);
  EXPECT_NE(content.find("Room 001 palette mismatch"), std::string::npos);
  EXPECT_NE(content.find("Dungeon Palette Issue Report"), std::string::npos);
  EXPECT_NE(content.find("Room 0x001\nBG sheets: [1,2,3,4]"),
            std::string::npos);

  std::filesystem::remove_all(temp_home);
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportIncludesRomContextAndSanePaletteEvents) {
  zelda3::PaletteDebugger::Get().Clear();

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());
  rom.set_filename("/tmp/palette_issue_test.sfc");

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;

  zelda3::PaletteDebugger::Get().LogPaletteApplication("Test::Palette", 0x12,
                                                       true);

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("ROM: palette_issue_test.sfc | CRC32:"),
            std::string::npos);
  EXPECT_NE(report.find("Path:/tmp/palette_issue_test.sfc"), std::string::npos);
  EXPECT_NE(report.find("[Test::Palette] palette=18"), std::string::npos);
  EXPECT_EQ(report.find("colors=1828354192"), std::string::npos);

  zelda3::PaletteDebugger::Get().Clear();
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportIncludesObjectTraceFootprint) {
  zelda3::PaletteDebugger::Get().Clear();

  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject obj(/*id=*/0xC8, /*x=*/4, /*y=*/5, /*size=*/0x11,
                         /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_ = MakeObjectTiles(8);
  room.GetTileObjects().push_back(obj);
  viewer.object_interaction().SetSelectedObjects({0});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("Object tiles: count=8"), std::string::npos);
  EXPECT_NE(report.find("Object geometry: selection_bounds_px="),
            std::string::npos);
  EXPECT_NE(report.find("Drawer trace: status=ok"), std::string::npos);
  EXPECT_NE(report.find("bounds_tiles="), std::string::npos);
  EXPECT_NE(report.find("layer_counts BG1="), std::string::npos);
  EXPECT_NE(report.find("write[0] BG1 tile="), std::string::npos);

  zelda3::PaletteDebugger::Get().Clear();
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportSamplesPaletteFromObjectTraceFootprint) {
  auto& palette_debugger = zelda3::PaletteDebugger::Get();
  palette_debugger.Clear();

  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject obj(/*id=*/0xC8, /*x=*/4, /*y=*/5, /*size=*/0x11,
                         /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_ = MakeObjectTiles(8);
  room.GetTileObjects().push_back(obj);
  viewer.object_interaction().SetSelectedObjects({0});

  std::vector<uint8_t> pixels(512 * 512, 0);
  pixels[(5 * 8) * 512 + (4 * 8)] = 0x71;
  pixels[(5 * 8 + 4) * 512 + (4 * 8 + 4)] = 0x01;

  gfx::Bitmap bitmap;
  bitmap.Create(/*width=*/512, /*height=*/512, /*depth=*/8, pixels);
  std::vector<SDL_Color> render_palette(256);
  for (int i = 0; i < 256; ++i) {
    render_palette[i] =
        SDL_Color{static_cast<uint8_t>(i), static_cast<uint8_t>(255 - i),
                  static_cast<uint8_t>(i / 2), 255};
  }
  bitmap.SetPalette(render_palette);
  palette_debugger.SetCurrentRenderPalette(render_palette);
  palette_debugger.SetCurrentBitmap(&bitmap);

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x72);

  EXPECT_NE(report.find("Palette sample object-trace[0] tile=(4,5) pal=0 "
                        "(36,44): idx=1"),
            std::string::npos);
  EXPECT_EQ(report.find("Palette sample selection-origin"), std::string::npos);
  EXPECT_EQ(report.find("(32,40): idx=113"), std::string::npos);

  palette_debugger.Clear();
}

TEST(DungeonCanvasViewerNavigationTest,
     DrawIssueReportFlagsStaleObjectSelection) {
  DungeonCanvasViewer viewer;
  zelda3::Room room;
  viewer.object_interaction().SetSelectedObjects({7});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildDrawIssueReport(viewer, room, 0x5A);

  EXPECT_NE(report.find("Selected objects: 1"), std::string::npos);
  EXPECT_NE(report.find("Selected object index 7 is stale"), std::string::npos);
  EXPECT_NE(report.find("room currently has 0 object(s)"), std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     SelectionIssueReportIncludesTraceSelectionDelta) {
  std::vector<uint8_t> rom_data(1024 * 1024, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonCanvasViewer viewer(&rom);
  zelda3::Room room;
  zelda3::RoomObject obj(/*id=*/0x71, /*x=*/29, /*y=*/14, /*size=*/0x04,
                         /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_ = MakeObjectTiles(1);
  room.GetTileObjects().push_back(obj);
  viewer.object_interaction().SetSelectedObjects({0});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildSelectionIssueReport(viewer, room,
                                                             0x59);

  EXPECT_NE(report.find("geometry: selection_bounds_px=(232,136,8,64) "
                        "object_origin_px=(232,112)"),
            std::string::npos);
  EXPECT_NE(report.find("trace: writes=8 unique_cells=8 "
                        "bounds_px=(232,136,8,64) "
                        "delta_vs_selection_px=(+0,+0,+0,+0)"),
            std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     SelectionIssueReportFlagsStaleObjectSelection) {
  DungeonCanvasViewer viewer;
  zelda3::Room room;
  viewer.object_interaction().SetSelectedObjects({11});

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildSelectionIssueReport(viewer, room,
                                                             0x5B);

  EXPECT_NE(report.find("Selected objects: 1"), std::string::npos);
  EXPECT_NE(report.find("- object[11] unavailable"), std::string::npos);
  EXPECT_NE(report.find("room currently has 0 object(s)"), std::string::npos);
}

TEST(DungeonCanvasViewerNavigationTest,
     SelectionIssueReportIncludesDoorGeometryAndEncoding) {
  DungeonCanvasViewer viewer;
  zelda3::Room room;
  auto door =
      MakeDoor(zelda3::DoorDirection::South, zelda3::DoorType::NormalDoor);
  door.position = 6;
  door.byte1 = 0x18;
  door.byte2 = 0x00;
  room.AddDoor(door);
  viewer.object_interaction().SelectEntity(EntityType::Door, 0);

  const std::string report =
      DungeonCanvasViewerTestPeer::BuildSelectionIssueReport(viewer, room,
                                                             0x72);

  EXPECT_NE(report.find("Selected door[0]:"), std::string::npos);
  EXPECT_NE(report.find("- tile_coords=("), std::string::npos);
  EXPECT_NE(report.find("- dims_tiles=(4,3) editor_dims_tiles=(4,3)"),
            std::string::npos);
  EXPECT_NE(report.find("- bounds_px=("), std::string::npos);
  EXPECT_NE(report.find("- encoded=(0x"), std::string::npos);
  EXPECT_NE(report.find("original=(0x18,0x00)"), std::string::npos);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDungeonConnectedRoomLinksUsesReciprocalDoorsAndTransportTargets) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::North, zelda3::DoorType::NormalDoor));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::BombableDoor));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::CaveExit));
  room.SetStaircaseRoom(0, 0x28);
  room.SetHolewarp(0x6A);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());

  const auto links = CollectDungeonConnectedRoomLinks(
      0x44, room, [](int neighbor_room_id, zelda3::DoorDirection dir) {
        return neighbor_room_id == 0x34 && dir == zelda3::DoorDirection::South;
      });

  ASSERT_EQ(links.size(), 3u);

  EXPECT_EQ(links[0].from_room_id, 0x44);
  EXPECT_EQ(links[0].to_room_id, 0x34);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(links[0].direction, zelda3::DoorDirection::North);

  EXPECT_EQ(links[1].from_room_id, 0x44);
  EXPECT_EQ(links[1].to_room_id, 0x28);
  EXPECT_EQ(links[1].type, DungeonConnectedLinkType::Staircase);

  EXPECT_EQ(links[2].from_room_id, 0x44);
  EXPECT_EQ(links[2].to_room_id, 0x6A);
  EXPECT_EQ(links[2].type, DungeonConnectedLinkType::Holewarp);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDungeonConnectedRoomLinksSkipsExitDoorsAndUnusedStairHeaders) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.AddDoor(MakeDoor(zelda3::DoorDirection::East,
                        zelda3::DoorType::FancyDungeonExit));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::South, zelda3::DoorType::NormalDoor));
  room.SetStaircaseRoom(0, 0);
  room.SetStaircaseRoom(1, 0x17);
  room.SetHolewarp(0);

  const auto links = CollectDungeonConnectedRoomLinks(
      0x20, room, [](int neighbor_room_id, zelda3::DoorDirection dir) {
        return neighbor_room_id == 0x30 && dir == zelda3::DoorDirection::North;
      });

  ASSERT_EQ(links.size(), 1u);
  EXPECT_EQ(links[0].to_room_id, 0x30);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Door);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDungeonConnectedRoomLinksUsesStraightInterroomStairObjects) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.SetStaircaseRoom(0, 0x17);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0xF9E, 4, 5, 0, 0)).ok());

  const auto links = CollectDungeonConnectedRoomLinks(0x20, room, nullptr);

  ASSERT_EQ(links.size(), 1u);
  EXPECT_EQ(links[0].from_room_id, 0x20);
  EXPECT_EQ(links[0].to_room_id, 0x17);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Staircase);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphKeepsCrossBlocksetStaircaseTargets) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetBlockset(0x01);
  start.SetStaircaseRoom(0, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& stair_target = rooms[0x40];
  ClearRoomLinks(&stair_target);
  stair_target.SetBlockset(0x02);
  stair_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x40]);
  ASSERT_EQ(graph.links.size(), 1u);
  EXPECT_EQ(graph.links[0].from_room_id, 0x10);
  EXPECT_EQ(graph.links[0].to_room_id, 0x40);
  EXPECT_EQ(graph.links[0].type, DungeonConnectedLinkType::Staircase);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsTagsLinksWithProvenanceFields) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::North, zelda3::DoorType::NormalDoor));
  room.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::BombableDoor));
  room.SetStaircaseRoom(0, 0x28);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  room.SetHolewarp(0x6A);

  const auto diagnostics = CollectDungeonConnectedRoomLinkDiagnostics(
      0x44, room, [](int, zelda3::DoorDirection) { return true; });

  ASSERT_EQ(diagnostics.links.size(), 4u);
  EXPECT_TRUE(diagnostics.staircase_issues.empty());

  EXPECT_EQ(diagnostics.links[0].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(diagnostics.links[0].direction, zelda3::DoorDirection::North);
  EXPECT_EQ(diagnostics.links[0].door_index, 0);
  EXPECT_EQ(diagnostics.links[0].door_type, zelda3::DoorType::NormalDoor);
  EXPECT_EQ(diagnostics.links[0].slot_index, -1);

  EXPECT_EQ(diagnostics.links[1].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(diagnostics.links[1].direction, zelda3::DoorDirection::West);
  EXPECT_EQ(diagnostics.links[1].door_index, 1);
  EXPECT_EQ(diagnostics.links[1].door_type, zelda3::DoorType::BombableDoor);

  EXPECT_EQ(diagnostics.links[2].type, DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(diagnostics.links[2].slot_index, 0);
  EXPECT_EQ(diagnostics.links[2].object_id, 0x138);
  EXPECT_EQ(diagnostics.links[2].to_room_id, 0x28);

  EXPECT_EQ(diagnostics.links[3].type, DungeonConnectedLinkType::Holewarp);
  EXPECT_EQ(diagnostics.links[3].to_room_id, 0x6A);
  EXPECT_EQ(diagnostics.links[3].slot_index, -1);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsEmitsStaleStaircaseEntriesForUnusedHeaderSlots) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  // Slot 0: header populated AND placed object → real link.
  // Slot 1: header populated but no second placed object → UnusedHeader.
  // Slot 2: header populated, also UnusedHeader.
  // Slot 3: header zero, no diagnostic.
  room.SetStaircaseRoom(0, 0x40);
  room.SetStaircaseRoom(1, 0x55);
  room.SetStaircaseRoom(2, 0x77);
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());

  const auto diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(0x10, room, nullptr);

  ASSERT_EQ(diagnostics.links.size(), 1u);
  EXPECT_EQ(diagnostics.links[0].type, DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(diagnostics.links[0].slot_index, 0);
  EXPECT_EQ(diagnostics.links[0].to_room_id, 0x40);

  ASSERT_EQ(diagnostics.staircase_issues.size(), 2u);
  EXPECT_EQ(diagnostics.staircase_issues[0].from_room_id, 0x10);
  EXPECT_EQ(diagnostics.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(diagnostics.staircase_issues[0].slot_index, 1);
  EXPECT_EQ(diagnostics.staircase_issues[0].header_room_id, 0x55);
  EXPECT_EQ(diagnostics.staircase_issues[1].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(diagnostics.staircase_issues[1].slot_index, 2);
  EXPECT_EQ(diagnostics.staircase_issues[1].header_room_id, 0x77);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsFlagsPlacedObjectWithUnsetHeaderAsMissingDestination) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  // Slot 0 header is 0 but a placed interroom-stair object consumes it →
  // runtime would jump to room 0x000, an invalid destination.
  ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());

  const auto diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(0x10, room, nullptr);

  EXPECT_TRUE(diagnostics.links.empty());
  ASSERT_EQ(diagnostics.staircase_issues.size(), 1u);
  EXPECT_EQ(diagnostics.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::MissingDestination);
  EXPECT_EQ(diagnostics.staircase_issues[0].slot_index, 0);
  EXPECT_EQ(diagnostics.staircase_issues[0].header_room_id, 0);
  EXPECT_EQ(diagnostics.staircase_issues[0].object_id, 0x138);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     CollectDiagnosticsFlagsExcessPlacedStaircaseObjectsAsExtra) {
  zelda3::Room room;
  ClearRoomLinks(&room);
  // 5 placed stair objects → only first 4 consume header slots, the 5th is
  // unreachable at runtime. Headers all valid so the first 4 produce real
  // links and slot indices 0..3 are exercised.
  room.SetStaircaseRoom(0, 0x21);
  room.SetStaircaseRoom(1, 0x22);
  room.SetStaircaseRoom(2, 0x23);
  room.SetStaircaseRoom(3, 0x24);
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(room.AddObject(zelda3::RoomObject(0x138, i, 5, 0, 0)).ok());
  }

  const auto diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(0x10, room, nullptr);

  ASSERT_EQ(diagnostics.links.size(), 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(diagnostics.links[i].type, DungeonConnectedLinkType::Staircase);
    EXPECT_EQ(diagnostics.links[i].slot_index, static_cast<int>(i));
  }
  ASSERT_EQ(diagnostics.staircase_issues.size(), 1u);
  EXPECT_EQ(diagnostics.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::ExtraPlacedObject);
  EXPECT_EQ(diagnostics.staircase_issues[0].object_id, 0x138);
  EXPECT_EQ(diagnostics.staircase_issues[0].slot_index, -1);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     FormatDungeonConnectedLinkDescriptionDescribesAllLinkTypes) {
  DungeonConnectedRoomLink door{};
  door.from_room_id = 0x10;
  door.to_room_id = 0x11;
  door.type = DungeonConnectedLinkType::Door;
  door.direction = zelda3::DoorDirection::South;
  door.door_index = 2;
  door.door_type = zelda3::DoorType::BombableDoor;
  EXPECT_EQ(FormatDungeonConnectedLinkDescription(door),
            "Door (South, type 0x2E) -> [011]");

  DungeonConnectedRoomLink stair{};
  stair.from_room_id = 0x10;
  stair.to_room_id = 0x40;
  stair.type = DungeonConnectedLinkType::Staircase;
  stair.slot_index = 1;
  stair.object_id = 0x138;
  EXPECT_EQ(FormatDungeonConnectedLinkDescription(stair),
            "Staircase slot 1 obj 0x138 -> [040]");

  DungeonConnectedRoomLink warp{};
  warp.from_room_id = 0x10;
  warp.to_room_id = 0x6A;
  warp.type = DungeonConnectedLinkType::Holewarp;
  EXPECT_EQ(FormatDungeonConnectedLinkDescription(warp), "Holewarp -> [06A]");
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     FormatDungeonStaircaseIssueDescribesAllKinds) {
  DungeonStaircaseIssue unused{};
  unused.from_room_id = 0x10;
  unused.kind = DungeonStaircaseIssueKind::UnusedHeader;
  unused.slot_index = 2;
  unused.header_room_id = 0x55;
  EXPECT_EQ(FormatDungeonStaircaseIssueDescription(unused),
            "Stale staircase slot 2 -> [055] (no placed interroom-stair "
            "object consumes this slot)");

  DungeonStaircaseIssue missing_unset{};
  missing_unset.kind = DungeonStaircaseIssueKind::MissingDestination;
  missing_unset.slot_index = 0;
  missing_unset.header_room_id = 0;
  missing_unset.object_id = 0x138;
  EXPECT_EQ(FormatDungeonStaircaseIssueDescription(missing_unset),
            "Missing staircase destination at slot 0 (placed object 0x138, "
            "header value 0 (unset))");

  DungeonStaircaseIssue missing_invalid{};
  missing_invalid.kind = DungeonStaircaseIssueKind::MissingDestination;
  missing_invalid.slot_index = 1;
  missing_invalid.header_room_id = 0xFFFF;
  missing_invalid.object_id = 0x138;
  EXPECT_EQ(FormatDungeonStaircaseIssueDescription(missing_invalid),
            "Missing staircase destination at slot 1 (placed object 0x138, "
            "header value 0xFFFF (out of range))");

  DungeonStaircaseIssue extra{};
  extra.kind = DungeonStaircaseIssueKind::ExtraPlacedObject;
  extra.object_id = 0x138;
  EXPECT_EQ(FormatDungeonStaircaseIssueDescription(extra),
            "Extra staircase object 0x138 beyond the 4 header slots "
            "(runtime cannot reach this stair)");
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphAggregatesStaircaseIssueEntries) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetStaircaseRoom(2, 0x55);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& target = rooms[0x40];
  ClearRoomLinks(&target);
  target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 2);
  EXPECT_FALSE(graph.dungeon_scope_active);
  ASSERT_EQ(graph.links.size(), 1u);
  EXPECT_EQ(graph.links[0].slot_index, 0);
  EXPECT_EQ(graph.links[0].object_id, 0x138);

  ASSERT_EQ(graph.staircase_issues.size(), 1u);
  EXPECT_EQ(graph.staircase_issues[0].from_room_id, 0x10);
  EXPECT_EQ(graph.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(graph.staircase_issues[0].slot_index, 2);
  EXPECT_EQ(graph.staircase_issues[0].header_room_id, 0x55);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphIgnoresUnusedStaircaseHeaderTargets) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetLoaded(true);

  auto& stale_target = rooms[0x40];
  ClearRoomLinks(&stale_target);
  stale_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 1);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_FALSE(graph.room_mask[0x40]);
  EXPECT_TRUE(graph.links.empty());

  ASSERT_EQ(graph.staircase_issues.size(), 1u);
  EXPECT_EQ(graph.staircase_issues[0].from_room_id, 0x10);
  EXPECT_EQ(graph.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);
  EXPECT_EQ(graph.staircase_issues[0].slot_index, 0);
  EXPECT_EQ(graph.staircase_issues[0].header_room_id, 0x40);
}

namespace {

// Writes a minimal dungeons.json under root/Docs/Dev/Planning/ describing the
// passed-in dungeon entries. Returns the temp root for cleanup.
std::filesystem::path MakeProjectRegistryRoot(const std::string& json_body) {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("yaze_connected_scope_test_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  std::filesystem::create_directories(planning);
  std::ofstream(planning / "dungeons.json") << json_body;
  return root;
}

absl::Status LoadOracleProject(project::YazeProject* project,
                               const std::filesystem::path& root) {
  project->name = "Oracle of Secrets";
  project->filepath = (root / "Oracle-of-Secrets.yaze").string();
  if (auto s = project->hack_manifest.LoadFromString(R"json({
        "manifest_version": 1,
        "hack_name": "Oracle of Secrets"
      })json");
      !s.ok()) {
    return s;
  }
  return project->hack_manifest.LoadProjectRegistry(root.string());
}

}  // namespace

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphScopesToCurrentDungeonWhenProjectRegistryLoaded) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [
      {
        "id": "D1",
        "name": "Test Dungeon",
        "rooms": [
          {"id": "0x10", "name": "Start"},
          {"id": "0x11", "name": "Adjacent"}
        ]
      }
    ]
  })json");

  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::NormalDoor));
  start.SetLoaded(true);

  auto& neighbor_in_scope = rooms[0x11];
  ClearRoomLinks(&neighbor_in_scope);
  neighbor_in_scope.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::NormalDoor));
  neighbor_in_scope.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x11]);
  EXPECT_TRUE(graph.out_of_scope_links.empty());

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphIncludesUnlinkedScopedRoomsWithFloorLabels) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [
      {
        "id": "D6",
        "name": "Goron Mines",
        "rooms": [
          {"id": "0x10", "name": "F1 Start", "floor": "F1", "grid_row": 1, "grid_col": 1},
          {"id": "0x11", "name": "F1 East", "floor": "F1", "grid_row": 1, "grid_col": 2},
          {"id": "0x40", "name": "B1 Side", "floor": "B1", "grid_row": 4, "grid_col": 1}
        ]
      }
    ]
  })json");

  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::NormalDoor));
  start.SetLoaded(true);

  auto& neighbor_in_scope = rooms[0x11];
  ClearRoomLinks(&neighbor_in_scope);
  neighbor_in_scope.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::NormalDoor));
  neighbor_in_scope.SetLoaded(true);

  auto& unlinked_b1 = rooms[0x40];
  ClearRoomLinks(&unlinked_b1);
  unlinked_b1.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 3);
  EXPECT_EQ(graph.unlinked_room_count, 1);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x11]);
  EXPECT_TRUE(graph.room_mask[0x40]);
  EXPECT_TRUE(graph.room_positions[0x10].connected_to_start);
  EXPECT_TRUE(graph.room_positions[0x11].connected_to_start);
  EXPECT_FALSE(graph.room_positions[0x40].connected_to_start);
  EXPECT_EQ(graph.room_positions[0x40].col, 1);
  EXPECT_EQ(graph.room_positions[0x40].row, 4);
  EXPECT_EQ(graph.room_floor_labels[0x10], "F1");
  EXPECT_EQ(graph.room_floor_labels[0x40], "B1");
  EXPECT_EQ(graph.floor_order, (std::vector<std::string>{"F1", "B1"}));

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphFlagsCrossDungeonStairLinksAsOutOfScope) {
  const std::filesystem::path root = MakeProjectRegistryRoot(R"json({
    "dungeons": [
      {"id": "D1", "name": "First", "rooms": [{"id": "0x10", "name": "Start"}]},
      {"id": "D2", "name": "Second", "rooms": [{"id": "0x40", "name": "Far"}]}
    ]
  })json");

  project::YazeProject project;
  ASSERT_TRUE(LoadOracleProject(&project, root).ok());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& far_target = rooms[0x40];
  ClearRoomLinks(&far_target);
  far_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 1);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_FALSE(graph.room_mask[0x40]);
  EXPECT_TRUE(graph.links.empty());

  ASSERT_EQ(graph.out_of_scope_links.size(), 1u);
  EXPECT_EQ(graph.out_of_scope_links[0].from_room_id, 0x10);
  EXPECT_EQ(graph.out_of_scope_links[0].to_room_id, 0x40);
  EXPECT_EQ(graph.out_of_scope_links[0].type,
            DungeonConnectedLinkType::Staircase);
  EXPECT_EQ(graph.out_of_scope_links[0].slot_index, 0);
  EXPECT_EQ(graph.out_of_scope_links[0].object_id, 0x138);

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     ApplyConnectedStaircaseIssueAutoFixesClearsOnlyUnusedHeaders) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetStaircaseRoom(1, 0x41);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.ClearHeaderDirty();
  start.SetLoaded(true);

  auto& linked_target = rooms[0x40];
  ClearRoomLinks(&linked_target);
  linked_target.SetLoaded(true);

  auto& stale_target = rooms[0x41];
  ClearRoomLinks(&stale_target);
  stale_target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto before =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);
  ASSERT_EQ(before.staircase_issues.size(), 1u);
  EXPECT_EQ(before.staircase_issues[0].kind,
            DungeonStaircaseIssueKind::UnusedHeader);

  EXPECT_EQ(DungeonCanvasViewerTestPeer::ApplyConnectedStaircaseIssueAutoFixes(
                viewer, 0x10),
            1);
  EXPECT_EQ(start.staircase_room(0), 0x40);
  EXPECT_EQ(start.staircase_room(1), 0);
  EXPECT_TRUE(start.header_dirty());

  const auto after =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);
  EXPECT_TRUE(after.staircase_issues.empty());
  EXPECT_EQ(after.room_count, 2);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphWithoutProjectFallsBackToFullTransitiveScope) {
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& target = rooms[0x40];
  ClearRoomLinks(&target);
  target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  // Intentionally no SetProject() call.

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_FALSE(graph.dungeon_scope_active);
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.out_of_scope_links.empty());
}

TEST(
    DungeonCanvasViewerConnectedGraphTest,
    BuildConnectedRoomGraphPreservesDistinctStaircaseInstancesBetweenRoomPair) {
  // Two staircase objects in the same source room targeting the same
  // destination must each survive graph dedup with their own slot_index
  // / object_id provenance. Earlier the dedup keyed only on the
  // (minmax pair, type) tuple and silently collapsed both instances into
  // a single visible edge, hiding which slot was misconfigured.
  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.SetStaircaseRoom(0, 0x40);
  start.SetStaircaseRoom(1, 0x40);
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x138, 4, 5, 0, 0)).ok());
  ASSERT_TRUE(start.AddObject(zelda3::RoomObject(0x139, 6, 5, 0, 0)).ok());
  start.SetLoaded(true);

  auto& target = rooms[0x40];
  ClearRoomLinks(&target);
  target.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_EQ(graph.room_count, 2);
  ASSERT_EQ(graph.links.size(), 2u)
      << "Both staircase instances must be preserved in the graph; dedup "
         "should not collapse provenance-distinct edges";
  std::vector<int> seen_slots;
  std::vector<int16_t> seen_objects;
  for (const auto& link : graph.links) {
    EXPECT_EQ(link.type, DungeonConnectedLinkType::Staircase);
    EXPECT_EQ(link.from_room_id, 0x10);
    EXPECT_EQ(link.to_room_id, 0x40);
    seen_slots.push_back(link.slot_index);
    seen_objects.push_back(link.object_id);
  }
  std::sort(seen_slots.begin(), seen_slots.end());
  std::sort(seen_objects.begin(), seen_objects.end());
  EXPECT_EQ(seen_slots, (std::vector<int>{0, 1}));
  EXPECT_EQ(seen_objects, (std::vector<int16_t>{0x138, 0x139}));
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     BuildConnectedRoomGraphScopesViaProjectOpenWithoutHackManifest) {
  // Real Oracle .yaze projects ship a project_registry without a
  // hack_manifest_v1.json (mirrors ProjectPathsTest::
  // OpenInjectsOracleDungeonRoomLabelsIntoProjectFile). Connected-mode
  // scoping must work via YazeProject::Open() in that configuration —
  // earlier code gated registry lookups on hack_manifest.loaded() and
  // silently fell back to full transitive BFS for the real Oracle path.
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("yaze_connected_scope_real_open_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  std::filesystem::create_directories(planning);
  std::ofstream(planning / "dungeons.json") << R"json({
    "dungeons": [
      {
        "id": "D1",
        "name": "Real Dungeon",
        "rooms": [
          {"id": "0x10", "name": "Start"},
          {"id": "0x11", "name": "Adjacent"}
        ]
      }
    ]
  })json";

  const std::filesystem::path project_file = root / "Real.yaze";
  std::ofstream(project_file) << R"([project]
name=Real Oracle Project

[files]
code_folder=Core
hack_manifest_file=hack_manifest.json
)";

  project::YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());
  ASSERT_FALSE(project.hack_manifest.loaded());
  ASSERT_TRUE(project.hack_manifest.HasProjectRegistry());

  std::vector<uint8_t> rom_data(0x8000, 0);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(rom_data).ok());

  DungeonRoomStore rooms(&rom);
  auto& start = rooms[0x10];
  ClearRoomLinks(&start);
  start.AddDoor(
      MakeDoor(zelda3::DoorDirection::East, zelda3::DoorType::NormalDoor));
  start.SetLoaded(true);

  auto& adjacent = rooms[0x11];
  ClearRoomLinks(&adjacent);
  adjacent.AddDoor(
      MakeDoor(zelda3::DoorDirection::West, zelda3::DoorType::NormalDoor));
  adjacent.SetLoaded(true);

  DungeonCanvasViewer viewer(&rom);
  viewer.SetRooms(&rooms);
  viewer.SetProject(&project);

  const auto graph =
      DungeonCanvasViewerTestPeer::BuildConnectedRoomGraph(viewer, 0x10);

  EXPECT_TRUE(graph.dungeon_scope_active)
      << "Scoping must activate from the project_registry alone, even when "
         "hack_manifest.loaded() is false";
  EXPECT_EQ(graph.room_count, 2);
  EXPECT_TRUE(graph.room_mask[0x10]);
  EXPECT_TRUE(graph.room_mask[0x11]);

  std::filesystem::remove_all(root);
}

TEST(DungeonCanvasViewerConnectedGraphTest,
     ShouldShowConnectedSidePanelAppliesHysteresisAroundMinWidth) {
  constexpr float kMin = dungeon_ui::kConnectedSidePanelApproxMinWidth;
  constexpr float kHyst = dungeon_ui::kConnectedSidePanelHideHysteresis;

  // From hidden: only reveal at or above the full min-threshold.
  EXPECT_FALSE(dungeon_ui::ShouldShowConnectedSidePanel(kMin - 0.001f, false));
  EXPECT_TRUE(dungeon_ui::ShouldShowConnectedSidePanel(kMin, false));

  // From visible: stay visible through the hysteresis band below min.
  EXPECT_TRUE(dungeon_ui::ShouldShowConnectedSidePanel(kMin - 0.001f, true));
  EXPECT_TRUE(dungeon_ui::ShouldShowConnectedSidePanel(kMin - kHyst, true));
  // Collapse only after crossing below (min - hysteresis).
  EXPECT_FALSE(
      dungeon_ui::ShouldShowConnectedSidePanel(kMin - kHyst - 0.001f, true));
}

}  // namespace yaze::editor
