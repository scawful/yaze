#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/dungeon/ui_constants.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/palette_debug.h"

namespace yaze::editor {

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
    setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvVar() {
    if (had_original_) {
      setenv(name_.c_str(), original_value_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
  }

 private:
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
     CollectDungeonConnectedRoomLinksSkipsExitDoorsAndZeroTransportTargets) {
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

  ASSERT_EQ(links.size(), 2u);
  EXPECT_EQ(links[0].to_room_id, 0x30);
  EXPECT_EQ(links[0].type, DungeonConnectedLinkType::Door);
  EXPECT_EQ(links[1].to_room_id, 0x17);
  EXPECT_EQ(links[1].type, DungeonConnectedLinkType::Staircase);
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
