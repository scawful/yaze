#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "rom/rom.h"

namespace yaze::editor {

class DungeonEditorV2MinecartTrackTestPeer {
 public:
  static void SetMinecartTrackEditorPanel(DungeonEditorV2& editor,
                                          MinecartTrackEditorPanel* panel) {
    editor.minecart_track_editor_panel_ = panel;
  }
};

namespace {

constexpr char kTrackSourceRelativePath[] =
    "Sprites/Objects/data/minecart_tracks.asm";

class ScopedTestProject {
 public:
  ScopedTestProject() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_minecart_stoploss_" + std::to_string(nonce));
    outside_root_ = root_.string() + "_outside";
    std::filesystem::create_directories(root_);
    std::filesystem::create_directories(outside_root_);
    project_.name = "Minecart Test";
    project_.filepath = (root_ / "Oracle-of-Secrets.yaze").string();
    project_.code_folder = (root_ / "Core").string();
    std::ofstream(project_.filepath) << "name=Minecart Test\n";
  }

  ~ScopedTestProject() {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
    std::filesystem::remove_all(outside_root_, ec);
  }

  project::YazeProject* project() { return &project_; }
  const std::filesystem::path& root() const { return root_; }
  const std::filesystem::path& outside_root() const { return outside_root_; }
  std::filesystem::path source_path() const {
    return root_ / kTrackSourceRelativePath;
  }

  void WriteSource(const std::string& contents) {
    std::filesystem::create_directories(source_path().parent_path());
    std::ofstream file(source_path(), std::ios::binary | std::ios::trunc);
    file << contents;
  }

  std::string ReadSource() const {
    std::ifstream file(source_path(), std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

 private:
  std::filesystem::path root_;
  std::filesystem::path outside_root_;
  project::YazeProject project_;
};

void AppendDwValues(std::stringstream& stream, int first_value, int count) {
  for (int i = 0; i < count; i += 8) {
    stream << "  dw ";
    for (int j = 0; j < 8 && i + j < count; ++j) {
      if (j > 0) {
        stream << ", ";
      }
      stream << absl::StrFormat("$%04X", first_value + i + j);
    }
    stream << "\n";
  }
}

std::string MakeFlatSection(const std::string& label, int first_value,
                            int count) {
  std::stringstream stream;
  stream << "  " << label << "\n";
  AppendDwValues(stream, first_value, count);
  return stream.str();
}

std::string MakeGuardedSection(const std::string& label, int first_value,
                               int disabled_value, int enabled_count = 28,
                               int disabled_count = 28) {
  std::stringstream stream;
  stream << "  " << label << "\n";
  AppendDwValues(stream, first_value, 4);
  stream << "  if !ENABLE_MINECART_PLANNED_TRACK_TABLE == 1\n";
  AppendDwValues(stream, first_value + 4, enabled_count);
  stream << "  else\n";
  AppendDwValues(stream, disabled_value, disabled_count);
  stream << "  endif\n";
  return stream.str();
}

std::string MakeFlatSource(int count, int room_base = 0x0100) {
  return MakeFlatSection(".TrackStartingRooms", room_base, count) + "\n" +
         MakeFlatSection(".TrackStartingX", 0x1000, count) + "\n" +
         MakeFlatSection(".TrackStartingY", 0x2000, count);
}

std::string MakeGuardedSource(int room_base = 0x0200) {
  return MakeGuardedSection(".TrackStartingRooms", room_base, 0x0000) + "\n" +
         MakeGuardedSection(".TrackStartingX", 0x1100, 0x0000) + "\n" +
         MakeGuardedSection(".TrackStartingY", 0x2100, 0x0000);
}

std::string MakeGuardedSourceWithRoomBranchCounts(int enabled_count,
                                                  int disabled_count) {
  return MakeGuardedSection(".TrackStartingRooms", 0x0200, 0x0000,
                            enabled_count, disabled_count) +
         "\n" + MakeGuardedSection(".TrackStartingX", 0x1100, 0x0000) + "\n" +
         MakeGuardedSection(".TrackStartingY", 0x2100, 0x0000);
}

std::string MakeMalformedSource() {
  std::string source = MakeFlatSource(32);
  const size_t first_dw = source.find("dw ");
  source.replace(first_dw, 2, "db");
  return source;
}

}  // namespace

TEST(MinecartTrackEditorPanelTest,
     ResolvesCanonicalSourceFromProjectRootAndParsesGuardedFirstBranch) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeGuardedSource(/*room_base=*/0x0200));

  const std::filesystem::path wrong_source =
      std::filesystem::path(fixture.project()->code_folder) /
      kTrackSourceRelativePath;
  std::filesystem::create_directories(wrong_source.parent_path());
  std::ofstream(wrong_source) << MakeFlatSource(32, /*room_base=*/0x4400);

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());

  const auto& tracks = panel.GetTracks();
  ASSERT_EQ(tracks.size(), 32u);
  EXPECT_EQ(tracks.front().room_id, 0x0200);
  EXPECT_EQ(tracks[3].room_id, 0x0203);
  EXPECT_EQ(tracks[4].room_id, 0x0204);
  EXPECT_EQ(tracks.back().room_id, 0x021F);
  ASSERT_TRUE(panel.ResolveTrackSourcePath().ok());
  EXPECT_EQ(*panel.ResolveTrackSourcePath(),
            std::filesystem::canonical(fixture.source_path()));
  EXPECT_FALSE(panel.HasUnpublishedChanges());
}

TEST(MinecartTrackEditorPanelTest, FlatSourceRequiresExactly32Entries) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeFlatSource(32));

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());

  ASSERT_EQ(panel.GetTracks().size(), 32u);
  EXPECT_EQ(panel.GetTracks().front().room_id, 0x0100);
  EXPECT_EQ(panel.GetTracks().back().room_id, 0x011F);
  EXPECT_TRUE(absl::IsFailedPrecondition(panel.SaveTracks()));

  const std::string source_before = fixture.ReadSource();
  MinecartTrack draft = panel.GetTracks().front();
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());
  EXPECT_TRUE(absl::IsUnimplemented(panel.SaveTracks()));
  EXPECT_EQ(fixture.ReadSource(), source_before);
  EXPECT_TRUE(panel.HasUnpublishedChanges());
}

TEST(MinecartTrackEditorPanelTest,
     MalformedReloadPreservesPriorModelAndLoadedBaseline) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeFlatSource(32));

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());
  const std::vector<MinecartTrack> original = panel.GetTracks();

  for (const std::string& invalid_source :
       {MakeMalformedSource(), MakeFlatSource(0), MakeFlatSource(33),
        MakeGuardedSourceWithRoomBranchCounts(/*enabled_count=*/27,
                                              /*disabled_count=*/28),
        MakeGuardedSourceWithRoomBranchCounts(/*enabled_count=*/28,
                                              /*disabled_count=*/29)}) {
    fixture.WriteSource(invalid_source);
    const absl::Status reload_status = panel.ReloadTracks();
    EXPECT_FALSE(reload_status.ok()) << invalid_source;
    EXPECT_EQ(panel.GetTracks(), original);
    EXPECT_FALSE(panel.HasUnpublishedChanges());

    MinecartTrack changed = original.front();
    changed.room_id = 0x0777;
    ASSERT_TRUE(panel.UpdateTrack(0, changed).ok());
    EXPECT_TRUE(panel.HasUnpublishedChanges());
    ASSERT_TRUE(panel.DiscardUnpublishedChanges().ok());
    EXPECT_EQ(panel.GetTracks(), original);
    EXPECT_FALSE(panel.HasUnpublishedChanges());
  }
}

TEST(MinecartTrackEditorPanelTest,
     DraftBlocksReloadAndProjectSwitchUntilExplicitDiscard) {
  ScopedTestProject first_project;
  first_project.WriteSource(MakeFlatSource(32, /*room_base=*/0x0100));
  ScopedTestProject second_project;
  second_project.WriteSource(MakeFlatSource(32, /*room_base=*/0x0300));

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(first_project.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());

  MinecartTrack draft = panel.GetTracks()[0];
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());
  EXPECT_TRUE(panel.HasUnpublishedChanges());
  first_project.WriteSource(MakeFlatSource(32, /*room_base=*/0x0500));

  EXPECT_TRUE(absl::IsFailedPrecondition(panel.ReloadTracks()));
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0777);
  EXPECT_TRUE(
      absl::IsFailedPrecondition(panel.SetProject(second_project.project())));
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0777);

  ASSERT_TRUE(panel.DiscardUnpublishedChanges().ok());
  EXPECT_FALSE(panel.HasUnpublishedChanges());
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0100);
  ASSERT_TRUE(panel.ReloadTracks().ok());
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0500);
  ASSERT_TRUE(panel.SetProject(second_project.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0300);
}

TEST(MinecartTrackEditorPanelTest, RejectsSourceSymlinkOutsideProjectRoot) {
  ScopedTestProject fixture;
  std::filesystem::create_directories(fixture.source_path().parent_path());
  const std::filesystem::path outside_source =
      fixture.outside_root() / "minecart_tracks.asm";
  std::ofstream(outside_source) << MakeFlatSource(32);
  std::error_code symlink_error;
  std::filesystem::create_symlink(outside_source, fixture.source_path(),
                                  symlink_error);
  if (symlink_error == std::errc::operation_not_permitted ||
      symlink_error == std::errc::permission_denied) {
    GTEST_SKIP() << "Symlink creation is not permitted: "
                 << symlink_error.message();
  }
  ASSERT_FALSE(symlink_error) << symlink_error.message();

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  const absl::Status status = panel.ReloadTracks();

  EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
  EXPECT_TRUE(panel.GetTracks().empty());
  EXPECT_FALSE(panel.HasUnpublishedChanges());
}

TEST(MinecartTrackEditorPanelTest,
     GuardedPublicationAndDungeonSavesFailBeforeAnyMutation) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeGuardedSource());
  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());

  MinecartTrack draft = panel.GetTracks()[0];
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());
  ASSERT_TRUE(panel.HasUnpublishedChanges());
  const std::string source_before = fixture.ReadSource();

  const absl::Status publish_status = panel.SaveTracks();
  EXPECT_TRUE(absl::IsFailedPrecondition(publish_status)) << publish_status;
  EXPECT_NE(std::string(publish_status.message()).find("Guarded"),
            std::string::npos);
  EXPECT_EQ(fixture.ReadSource(), source_before);
  EXPECT_TRUE(panel.HasUnpublishedChanges());

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(editor,
                                                                    &panel);
  const std::vector<uint8_t> rom_before = rom.vector();
  const bool dirty_before = rom.dirty();

  EXPECT_TRUE(editor.HasPendingDungeonChanges());
  const absl::Status save_status = editor.Save();
  EXPECT_TRUE(absl::IsFailedPrecondition(save_status)) << save_status;
  EXPECT_NE(std::string(save_status.message()).find("Minecart"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), rom_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(fixture.ReadSource(), source_before);
  EXPECT_TRUE(panel.HasUnpublishedChanges());

  const absl::Status save_room_status = editor.SaveRoom(0);
  EXPECT_TRUE(absl::IsFailedPrecondition(save_room_status)) << save_room_status;
  EXPECT_NE(std::string(save_room_status.message()).find("Minecart"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), rom_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(fixture.ReadSource(), source_before);
  EXPECT_TRUE(panel.HasUnpublishedChanges());

  ASSERT_TRUE(panel.DiscardUnpublishedChanges().ok());
  EXPECT_FALSE(editor.HasPendingDungeonChanges());
}

}  // namespace yaze::editor
