#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/features.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/rom.h"

namespace yaze::editor {

class DungeonEditorV2MinecartTrackTestPeer {
 public:
  static void SetMinecartTrackEditorPanel(DungeonEditorV2& editor,
                                          MinecartTrackEditorPanel* panel) {
    editor.minecart_track_editor_panel_ = panel;
  }
};

class MinecartTrackEditorPanelTestPeer {
 public:
  static absl::StatusOr<bool> CommitOverlayList(
      MinecartTrackEditorPanel& panel, std::string& input,
      std::vector<uint16_t> project::DungeonOverlaySettings::* member) {
    return panel.CommitOverlayList(input, member);
  }

  static absl::StatusOr<bool> CommitTrackTilesInput(
      MinecartTrackEditorPanel& panel) {
    return panel.CommitOverlayList(
        panel.overlay_track_tiles_input_,
        &project::DungeonOverlaySettings::track_tiles);
  }

  static absl::StatusOr<bool> ResetOverlaySettings(
      MinecartTrackEditorPanel& panel) {
    return panel.ResetOverlaySettings();
  }

  static absl::Status SaveProjectSettings(MinecartTrackEditorPanel& panel) {
    return panel.SaveProjectSettings();
  }

  static absl::Status NotifyProjectDraftChanged(
      MinecartTrackEditorPanel& panel) {
    return panel.NotifyProjectDraftChanged();
  }

  static bool HasPendingProjectDraftChanges(
      const MinecartTrackEditorPanel& panel) {
    return panel.HasPendingProjectDraftChanges();
  }

  static void InitializeOverlayInputs(MinecartTrackEditorPanel& panel) {
    panel.InitializeOverlayInputs();
  }

  static void SetTrackTilesInput(MinecartTrackEditorPanel& panel,
                                 std::string input) {
    panel.overlay_track_tiles_input_ = std::move(input);
  }

  static void SetOverlayInputs(MinecartTrackEditorPanel& panel,
                               std::array<std::string, 5> inputs) {
    panel.overlay_track_tiles_input_ = std::move(inputs[0]);
    panel.overlay_track_stop_tiles_input_ = std::move(inputs[1]);
    panel.overlay_track_switch_tiles_input_ = std::move(inputs[2]);
    panel.overlay_track_object_ids_input_ = std::move(inputs[3]);
    panel.overlay_minecart_sprite_ids_input_ = std::move(inputs[4]);
  }

  static std::array<std::string, 5> OverlayInputs(
      const MinecartTrackEditorPanel& panel) {
    return {panel.overlay_track_tiles_input_,
            panel.overlay_track_stop_tiles_input_,
            panel.overlay_track_switch_tiles_input_,
            panel.overlay_track_object_ids_input_,
            panel.overlay_minecart_sprite_ids_input_};
  }

  static const std::string& TrackTilesInput(
      const MinecartTrackEditorPanel& panel) {
    return panel.overlay_track_tiles_input_;
  }

  static const std::string& StatusMessage(
      const MinecartTrackEditorPanel& panel) {
    return panel.status_message_;
  }

  static bool ShowSuccess(const MinecartTrackEditorPanel& panel) {
    return panel.show_success_;
  }

  static void SetAuditDirty(MinecartTrackEditorPanel& panel, bool dirty) {
    panel.audit_dirty_ = dirty;
  }

  static bool AuditDirty(const MinecartTrackEditorPanel& panel) {
    return panel.audit_dirty_;
  }
};

namespace {

class CallbackEditor : public Editor {
 public:
  explicit CallbackEditor(std::function<absl::Status()> callback)
      : callback_(std::move(callback)) {
    type_ = EditorType::kDungeon;
    active_ = true;
  }

  void Initialize() override {}
  absl::Status Load() override { return absl::OkStatus(); }
  absl::Status Save() override { return absl::OkStatus(); }
  absl::Status Update() override { return callback_(); }
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  absl::Status Find() override { return absl::OkStatus(); }

 private:
  std::function<absl::Status()> callback_;
};

struct ScopedImGuiContext {
  ScopedImGuiContext() {
    context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (context != nullptr) {
      ImGui::DestroyContext(context);
    }
  }

  ImGuiContext* context = nullptr;
};

struct FeatureFlagsGuard {
  core::FeatureFlags::Flags previous = core::FeatureFlags::get();
  ~FeatureFlagsGuard() { core::FeatureFlags::get() = previous; }
};

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
    const absl::Status manifest_status = LoadManifest();
    if (!manifest_status.ok()) {
      throw std::runtime_error(std::string(manifest_status.message()));
    }
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
  std::filesystem::path source_path(const std::string& relative_path) const {
    return root_ / relative_path;
  }

  absl::Status LoadManifest(
      const std::string& relative_path = kTrackSourceRelativePath) {
    return project_.hack_manifest.LoadFromString(absl::StrFormat(
        R"json({
          "manifest_version": 3,
          "minecart_tracks": {
            "source": {
              "format": "yaze-minecart-track-table",
              "version": 1,
              "path": "%s"
            }
          }
        })json",
        relative_path));
  }

  void WriteSource(const std::string& contents) {
    WriteSourceAt(kTrackSourceRelativePath, contents);
  }

  void WriteSourceAt(const std::string& relative_path,
                     const std::string& contents) {
    const std::filesystem::path path = source_path(relative_path);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
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

class ScopedManagerProject {
 public:
  ScopedManagerProject(const std::string& suffix, const std::string& name,
                       uint16_t track_tile) {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            absl::StrFormat("yaze_minecart_manager_%s_%d", suffix, nonce);
    std::filesystem::create_directories(root_);

    rom_path_ = root_ / (suffix + ".sfc");
    std::vector<uint8_t> rom_data(512 * 1024, 0);
    constexpr size_t kTitleOffset = 0x7FC0;
    for (size_t i = 0; i < name.size() && kTitleOffset + i < rom_data.size();
         ++i) {
      rom_data[kTitleOffset + i] = static_cast<uint8_t>(name[i]);
    }
    std::ofstream rom_file(rom_path_, std::ios::binary | std::ios::trunc);
    rom_file.write(reinterpret_cast<const char*>(rom_data.data()),
                   static_cast<std::streamsize>(rom_data.size()));
    rom_file.close();

    project_path_ = root_ / (suffix + ".yaze");
    project::YazeProject project;
    project.name = name;
    project.filepath = project_path_.string();
    project.rom_filename = rom_path_.string();
    project.dungeon_overlay.track_tiles = {track_tile};
    const absl::Status save_status = project.Save();
    if (!save_status.ok()) {
      throw std::runtime_error(std::string(save_status.message()));
    }
  }

  ~ScopedManagerProject() {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }

  const std::filesystem::path& project_path() const { return project_path_; }
  std::filesystem::path Path(const std::string& filename) const {
    return root_ / filename;
  }

  project::YazeProject ReadProject() const {
    return ReadProject(project_path_);
  }

  project::YazeProject ReadProject(
      const std::filesystem::path& project_path) const {
    project::YazeProject project;
    const absl::Status status = project.Open(project_path.string());
    if (!status.ok()) {
      throw std::runtime_error(std::string(status.message()));
    }
    return project;
  }

 private:
  std::filesystem::path root_;
  std::filesystem::path rom_path_;
  std::filesystem::path project_path_;
};

class ScopedBoundMinecartPanel {
 public:
  ScopedBoundMinecartPanel(EditorManager* manager, RomSession* session) {
    dungeon_ =
        session->editors.GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
    if (dungeon_ == nullptr) {
      throw std::runtime_error("Dungeon editor was not created");
    }
    DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(*dungeon_,
                                                                      &panel_);
    manager->ConfigureSession(session);
  }

  ~ScopedBoundMinecartPanel() {
    DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(*dungeon_,
                                                                      nullptr);
  }

  MinecartTrackEditorPanel* panel() { return &panel_; }

 private:
  DungeonEditorV2* dungeon_ = nullptr;
  MinecartTrackEditorPanel panel_;
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
     OverlayEditsNotifyProjectOnlyWhenSettingsChange) {
  ScopedTestProject fixture;
  auto* project_ptr = fixture.project();
  project_ptr->dungeon_overlay.track_tiles = {0xB0, 0xB1};
  project_ptr->dungeon_overlay.track_stop_tiles = {0xB7};

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(project_ptr).ok());

  int changed_calls = 0;
  project::DungeonOverlaySettings reported_overlay;
  panel.SetProjectChangedCallback(
      [&](const project::DungeonOverlaySettings& overlay) -> absl::Status {
        ++changed_calls;
        reported_overlay = overlay;
        return absl::OkStatus();
      });

  std::string unchanged_input = "0xB0, $00B1";
  auto unchanged = MinecartTrackEditorPanelTestPeer::CommitOverlayList(
      panel, unchanged_input, &project::DungeonOverlaySettings::track_tiles);
  ASSERT_TRUE(unchanged.ok()) << unchanged.status();
  EXPECT_FALSE(*unchanged);
  EXPECT_EQ(unchanged_input, "0xB0, 0xB1");
  EXPECT_EQ(changed_calls, 0);

  std::string changed_input = "$00C0, 0xC1";
  auto changed = MinecartTrackEditorPanelTestPeer::CommitOverlayList(
      panel, changed_input, &project::DungeonOverlaySettings::track_tiles);
  ASSERT_TRUE(changed.ok()) << changed.status();
  EXPECT_TRUE(*changed);
  EXPECT_EQ(changed_input, "0xC0, 0xC1");
  EXPECT_EQ(changed_calls, 1);
  EXPECT_EQ(project_ptr->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0, 0xC1}));
  EXPECT_EQ(reported_overlay.track_tiles,
            project_ptr->dungeon_overlay.track_tiles);
  EXPECT_EQ(reported_overlay.track_stop_tiles,
            project_ptr->dungeon_overlay.track_stop_tiles);

  auto reset = MinecartTrackEditorPanelTestPeer::ResetOverlaySettings(panel);
  ASSERT_TRUE(reset.ok()) << reset.status();
  EXPECT_TRUE(*reset);
  EXPECT_EQ(changed_calls, 2);
  EXPECT_TRUE(project_ptr->dungeon_overlay.track_tiles.empty());
  EXPECT_TRUE(project_ptr->dungeon_overlay.track_stop_tiles.empty());
  EXPECT_TRUE(reported_overlay.track_tiles.empty());
  EXPECT_TRUE(reported_overlay.track_stop_tiles.empty());

  auto unchanged_reset =
      MinecartTrackEditorPanelTestPeer::ResetOverlaySettings(panel);
  ASSERT_TRUE(unchanged_reset.ok()) << unchanged_reset.status();
  EXPECT_FALSE(*unchanged_reset);
  EXPECT_EQ(changed_calls, 2);
}

TEST(MinecartTrackEditorPanelTest,
     OverlayParserRejectsWholeInvalidInputAndNormalizesAcceptedValues) {
  ScopedTestProject fixture;
  fixture.project()->dungeon_overlay.track_tiles = {0xB0, 0xB1};

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  int changed_calls = 0;
  panel.SetProjectChangedCallback(
      [&](const project::DungeonOverlaySettings&) -> absl::Status {
        ++changed_calls;
        return absl::OkStatus();
      });
  MinecartTrackEditorPanelTestPeer::SetAuditDirty(panel, false);

  for (const std::string& invalid :
       {"0x1G, 0x2", "65536", "0x10000", "0x1junk", "$", "0x1,", "0x1,,0x2",
        "999999999999999999999999999999"}) {
    std::string input = invalid;
    const auto result = MinecartTrackEditorPanelTestPeer::CommitOverlayList(
        panel, input, &project::DungeonOverlaySettings::track_tiles);
    EXPECT_FALSE(result.ok()) << invalid;
    EXPECT_EQ(input, invalid);
    EXPECT_EQ(fixture.project()->dungeon_overlay.track_tiles,
              (std::vector<uint16_t>{0xB0, 0xB1}));
    EXPECT_EQ(changed_calls, 0);
    EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::AuditDirty(panel));
    EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::ShowSuccess(panel));
    EXPECT_NE(MinecartTrackEditorPanelTestPeer::StatusMessage(panel).find(
                  "Overlay update rejected"),
              std::string::npos);
  }

  std::string accepted = " $00C0  0x00C1 ";
  auto changed = MinecartTrackEditorPanelTestPeer::CommitOverlayList(
      panel, accepted, &project::DungeonOverlaySettings::track_tiles);
  ASSERT_TRUE(changed.ok()) << changed.status();
  EXPECT_TRUE(*changed);
  EXPECT_EQ(accepted, "0xC0, 0xC1");
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0, 0xC1}));
  EXPECT_EQ(changed_calls, 1);
  EXPECT_TRUE(MinecartTrackEditorPanelTestPeer::AuditDirty(panel));

  std::string equivalent = "192, 193";
  auto unchanged = MinecartTrackEditorPanelTestPeer::CommitOverlayList(
      panel, equivalent, &project::DungeonOverlaySettings::track_tiles);
  ASSERT_TRUE(unchanged.ok()) << unchanged.status();
  EXPECT_FALSE(*unchanged);
  EXPECT_EQ(equivalent, "0xC0, 0xC1");
  EXPECT_EQ(changed_calls, 1);
}

TEST(MinecartTrackEditorPanelTest,
     RejectedOverlayCallbackRollsBackInputModelAndDirtyState) {
  ScopedTestProject fixture;
  fixture.project()->dungeon_overlay.track_tiles = {0xB0};

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  MinecartTrackEditorPanelTestPeer::InitializeOverlayInputs(panel);
  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(panel, "$00C0");
  MinecartTrackEditorPanelTestPeer::SetAuditDirty(panel, false);
  bool session_active = false;
  bool project_dirty = false;
  panel.SetProjectChangedCallback(
      [&](const project::DungeonOverlaySettings&) -> absl::Status {
        if (!session_active) {
          return absl::FailedPreconditionError(
              "minecart overlay session is not active");
        }
        project_dirty = true;
        return absl::OkStatus();
      });

  const auto result =
      MinecartTrackEditorPanelTestPeer::CommitTrackTilesInput(panel);
  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status())) << result.status();
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xB0}));
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::TrackTilesInput(panel), "0xB0");
  EXPECT_FALSE(project_dirty);
  EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::AuditDirty(panel));
  EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::ShowSuccess(panel));
  EXPECT_NE(MinecartTrackEditorPanelTestPeer::StatusMessage(panel).find(
                "session is not active"),
            std::string::npos);

  const auto reset =
      MinecartTrackEditorPanelTestPeer::ResetOverlaySettings(panel);
  EXPECT_FALSE(reset.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(reset.status())) << reset.status();
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xB0}));
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::TrackTilesInput(panel), "0xB0");
  EXPECT_FALSE(project_dirty);
  EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::AuditDirty(panel));
}

TEST(MinecartTrackEditorPanelTest,
     NoOpResetClearsRejectedOverlayInputsWithoutDirtyingProject) {
  ScopedTestProject fixture;

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  int changed_calls = 0;
  panel.SetProjectChangedCallback(
      [&](const project::DungeonOverlaySettings&) -> absl::Status {
        ++changed_calls;
        return absl::OkStatus();
      });
  MinecartTrackEditorPanelTestPeer::SetAuditDirty(panel, false);
  MinecartTrackEditorPanelTestPeer::SetOverlayInputs(
      panel, {"0x1G", "rejected-stop", "rejected-switch", "rejected-object",
              "rejected-sprite"});

  const auto rejected =
      MinecartTrackEditorPanelTestPeer::CommitTrackTilesInput(panel);
  ASSERT_FALSE(rejected.ok());
  ASSERT_FALSE(MinecartTrackEditorPanelTestPeer::StatusMessage(panel).empty());

  const auto reset =
      MinecartTrackEditorPanelTestPeer::ResetOverlaySettings(panel);
  ASSERT_TRUE(reset.ok()) << reset.status();
  EXPECT_FALSE(*reset);
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::OverlayInputs(panel),
            (std::array<std::string, 5>{"", "", "", "", ""}));
  EXPECT_TRUE(MinecartTrackEditorPanelTestPeer::StatusMessage(panel).empty());
  EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::ShowSuccess(panel));
  EXPECT_FALSE(MinecartTrackEditorPanelTestPeer::AuditDirty(panel));
  EXPECT_EQ(changed_calls, 0);
  EXPECT_TRUE(fixture.project()->dungeon_overlay.track_tiles.empty());
  EXPECT_TRUE(fixture.project()->dungeon_overlay.track_stop_tiles.empty());
  EXPECT_TRUE(fixture.project()->dungeon_overlay.track_switch_tiles.empty());
  EXPECT_TRUE(fixture.project()->dungeon_overlay.track_object_ids.empty());
  EXPECT_TRUE(fixture.project()->dungeon_overlay.minecart_sprite_ids.empty());
}

TEST(MinecartTrackEditorPanelTest,
     DependencyReapplyRefreshesSamePointerProjectOverlayInputs) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeFlatSource(32));
  fixture.project()->dungeon_overlay.track_tiles = {0xB0};

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());
  MinecartTrack draft = panel.GetTracks().front();
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());
  ASSERT_TRUE(panel.HasUnpublishedChanges());
  MinecartTrackEditorPanelTestPeer::InitializeOverlayInputs(panel);
  ASSERT_EQ(MinecartTrackEditorPanelTestPeer::TrackTilesInput(panel), "0xB0");

  DungeonEditorV2 editor;
  DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(editor,
                                                                    &panel);
  EditorDependencies dependencies;
  dependencies.project = fixture.project();

  fixture.project()->dungeon_overlay.track_tiles = {0xD0, 0xD1};
  ASSERT_EQ(MinecartTrackEditorPanelTestPeer::TrackTilesInput(panel), "0xB0");
  editor.SetDependencies(dependencies);

  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::TrackTilesInput(panel),
            "0xD0, 0xD1");
  EXPECT_TRUE(panel.HasUnpublishedChanges());
  EXPECT_EQ(panel.GetTracks().front().room_id, 0x0777);
}

TEST(MinecartTrackEditorPanelTest,
     FailedManagerOwnedProjectSavePreservesDirtyOverlayForRetry) {
  ScopedTestProject fixture;
  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());

  bool project_dirty = false;
  panel.SetProjectChangedCallback(
      [&](const project::DungeonOverlaySettings&) -> absl::Status {
        project_dirty = true;
        return absl::OkStatus();
      });
  std::string switch_input = "$00D0, $00D1";
  const auto switch_changed =
      MinecartTrackEditorPanelTestPeer::CommitOverlayList(
          panel, switch_input,
          &project::DungeonOverlaySettings::track_switch_tiles);
  ASSERT_TRUE(switch_changed.ok()) << switch_changed.status();
  ASSERT_TRUE(*switch_changed);
  ASSERT_TRUE(project_dirty);

  int save_calls = 0;
  panel.SetProjectSaveCallback([&]() -> absl::Status {
    ++save_calls;
    return absl::UnavailableError("descriptor write failed");
  });
  const absl::Status failed_save =
      MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel);
  EXPECT_TRUE(absl::IsUnavailable(failed_save)) << failed_save;
  EXPECT_EQ(save_calls, 1);
  EXPECT_TRUE(project_dirty);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_switch_tiles,
            (std::vector<uint16_t>{0xD0, 0xD1}));

  panel.SetProjectSaveCallback([&]() -> absl::Status {
    ++save_calls;
    project_dirty = false;
    return absl::OkStatus();
  });
  EXPECT_TRUE(
      MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel).ok());
  EXPECT_EQ(save_calls, 2);
  EXPECT_FALSE(project_dirty);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_switch_tiles,
            (std::vector<uint16_t>{0xD0, 0xD1}));
}

TEST(MinecartTrackEditorPanelTest,
     SaveValidatesAllPendingOverlayInputsBeforeChangingProject) {
  ScopedTestProject fixture;
  fixture.project()->dungeon_overlay.track_tiles = {0xB0};
  fixture.project()->dungeon_overlay.track_stop_tiles = {0xB7};
  fixture.project()->dungeon_overlay.track_switch_tiles = {0xD0};
  fixture.project()->dungeon_overlay.track_object_ids = {0x31};
  fixture.project()->dungeon_overlay.minecart_sprite_ids = {0xA3};
  const project::DungeonOverlaySettings original =
      fixture.project()->dungeon_overlay;

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  MinecartTrackEditorPanelTestPeer::InitializeOverlayInputs(panel);
  MinecartTrackEditorPanelTestPeer::SetOverlayInputs(
      panel, {"$00C0", "$00C1", "not-hex", "$0032", "$00A4"});

  bool project_dirty = false;
  int changed_calls = 0;
  int save_calls = 0;
  panel.SetProjectChangedCallback(
      [&](const project::DungeonOverlaySettings&) -> absl::Status {
        ++changed_calls;
        project_dirty = true;
        return absl::OkStatus();
      });
  panel.SetProjectSaveCallback([&]() -> absl::Status {
    ++save_calls;
    project_dirty = false;
    return absl::OkStatus();
  });

  const absl::Status rejected =
      MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel);
  EXPECT_TRUE(absl::IsInvalidArgument(rejected)) << rejected;
  EXPECT_EQ(changed_calls, 0);
  EXPECT_EQ(save_calls, 0);
  EXPECT_FALSE(project_dirty);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_tiles,
            original.track_tiles);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_stop_tiles,
            original.track_stop_tiles);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_switch_tiles,
            original.track_switch_tiles);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_object_ids,
            original.track_object_ids);
  EXPECT_EQ(fixture.project()->dungeon_overlay.minecart_sprite_ids,
            original.minecart_sprite_ids);
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::OverlayInputs(panel),
            (std::array<std::string, 5>{"$00C0", "$00C1", "not-hex", "$0032",
                                        "$00A4"}));

  MinecartTrackEditorPanelTestPeer::SetOverlayInputs(
      panel, {"$00C0", "$00C1", "$00D1", "$0032", "$00A4"});
  EXPECT_TRUE(
      MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel).ok());
  EXPECT_EQ(changed_calls, 1);
  EXPECT_EQ(save_calls, 1);
  EXPECT_FALSE(project_dirty);
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_stop_tiles,
            (std::vector<uint16_t>{0xC1}));
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_switch_tiles,
            (std::vector<uint16_t>{0xD1}));
  EXPECT_EQ(fixture.project()->dungeon_overlay.track_object_ids,
            (std::vector<uint16_t>{0x32}));
  EXPECT_EQ(fixture.project()->dungeon_overlay.minecart_sprite_ids,
            (std::vector<uint16_t>{0xA4}));
}

TEST(MinecartTrackEditorPanelTest,
     SameRevisionRebindPreservesFocusedInputAndManagerSaveCommitsIt) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture("save_order", "Save Order", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(fixture.project_path().string()).ok());

  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  auto* dungeon =
      session->editors.GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);

  MinecartTrackEditorPanel panel;
  DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(*dungeon,
                                                                    &panel);
  manager->ConfigureSession(session);
  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(panel, "$00C0");

  // Benign same-revision dependency reapplication must not overwrite an
  // InputText buffer that has not emitted its deactivation event yet.
  manager->ConfigureSession(session);
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::TrackTilesInput(panel), "$00C0");

  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel).ok());
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));
  EXPECT_FALSE(session->project_dirty);
  EXPECT_EQ(fixture.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));

  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(panel, "$00C1");
  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::NotifyProjectDraftChanged(panel).ok());
  ASSERT_TRUE(manager->AutosaveActiveSession().ok());
  EXPECT_FALSE(session->project_dirty);
  EXPECT_EQ(fixture.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC1}));

  DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(*dungeon,
                                                                    nullptr);
}

TEST(MinecartTrackEditorPanelTest,
     GlobalProjectSaveCommitsFocusedOverlayDraftAndReadsBack) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture("global_save", "Global Save", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(fixture.project_path().string()).ok());
  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  ScopedBoundMinecartPanel binding(manager.get(), session);
  auto* panel = binding.panel();

  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(*panel, "$00C0");
  EXPECT_TRUE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));
  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::NotifyProjectDraftChanged(*panel).ok());
  EXPECT_TRUE(session->project_dirty);
  EXPECT_TRUE(manager->IsCurrentProjectDirty());

  ASSERT_TRUE(manager->SaveProject().ok());
  EXPECT_FALSE(session->project_dirty);
  EXPECT_FALSE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));
  EXPECT_EQ(fixture.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC0}));
}

TEST(MinecartTrackEditorPanelTest,
     SessionModifiedIncludesProjectWorkWithoutMaterializingCleanDungeon) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture("session_status", "Session Status", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(fixture.project_path().string()).ok());

  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  EXPECT_EQ(session->editors.GetExistingEditor(EditorType::kDungeon), nullptr);
  EXPECT_FALSE(manager->session_coordinator()->IsSessionModified(0));
  EXPECT_EQ(session->editors.GetExistingEditor(EditorType::kDungeon), nullptr);

  ScopedBoundMinecartPanel binding(manager.get(), session);
  auto* panel = binding.panel();
  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(*panel, "$00C0");
  ASSERT_FALSE(session->project_dirty);
  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));
  EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(0));

  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(*panel, "0xB0");
  ASSERT_FALSE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));
  session->project_dirty = true;
  EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(0));

  session->project_dirty = false;
  session->project_file_editor_state.initialized = true;
  session->project_file_editor_state.modified = true;
  EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(0));

  session->project_file_editor_state.modified = false;
  EXPECT_FALSE(manager->session_coordinator()->IsSessionModified(0));
}

TEST(MinecartTrackEditorPanelTest,
     InvalidGlobalSaveAndSaveAsPreserveDraftAndGuardDestructiveActions) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture_a("invalid_a", "Invalid A", 0xA0);
  ScopedManagerProject fixture_b("invalid_b", "Invalid B", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(
      manager->OpenRomOrProject(fixture_a.project_path().string()).ok());
  ASSERT_TRUE(
      manager->OpenRomOrProject(fixture_b.project_path().string()).ok());
  manager->SwitchToSession(0);

  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  ScopedBoundMinecartPanel binding(manager.get(), session);
  auto* panel = binding.panel();
  const project::DungeonOverlaySettings original_model =
      manager->GetCurrentProject()->dungeon_overlay;
  const std::string original_filepath = manager->GetCurrentProject()->filepath;
  auto invalid_inputs = MinecartTrackEditorPanelTestPeer::OverlayInputs(*panel);
  invalid_inputs[0] = "$00C0";
  invalid_inputs[1] = "not-hex";
  MinecartTrackEditorPanelTestPeer::SetOverlayInputs(*panel, invalid_inputs);
  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::NotifyProjectDraftChanged(*panel).ok());
  ASSERT_TRUE(session->project_dirty);

  const absl::Status save_status = manager->SaveProject();
  EXPECT_TRUE(absl::IsInvalidArgument(save_status)) << save_status;
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_tiles,
            original_model.track_tiles);
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_stop_tiles,
            original_model.track_stop_tiles);
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_switch_tiles,
            original_model.track_switch_tiles);
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_object_ids,
            original_model.track_object_ids);
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.minecart_sprite_ids,
            original_model.minecart_sprite_ids);
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::OverlayInputs(*panel),
            invalid_inputs);
  EXPECT_TRUE(session->project_dirty);
  EXPECT_TRUE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));
  const project::DungeonOverlaySettings disk_model =
      fixture_a.ReadProject().dungeon_overlay;
  EXPECT_EQ(disk_model.track_tiles, original_model.track_tiles);
  EXPECT_EQ(disk_model.track_stop_tiles, original_model.track_stop_tiles);
  EXPECT_EQ(disk_model.track_switch_tiles, original_model.track_switch_tiles);
  EXPECT_EQ(disk_model.track_object_ids, original_model.track_object_ids);
  EXPECT_EQ(disk_model.minecart_sprite_ids, original_model.minecart_sprite_ids);

  const std::filesystem::path save_as_path =
      fixture_a.Path("blocked-copy.yaze");
  const absl::Status save_as_status =
      manager->SaveProjectAs(save_as_path.string());
  EXPECT_TRUE(absl::IsInvalidArgument(save_as_status)) << save_as_status;
  EXPECT_FALSE(std::filesystem::exists(save_as_path));
  EXPECT_EQ(manager->GetCurrentProject()->filepath, original_filepath);
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::OverlayInputs(*panel),
            invalid_inputs);
  EXPECT_TRUE(session->project_dirty);

  manager->SwitchToSession(1);
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionId(), session->session_id());
  EXPECT_NE(manager->GetPendingUnsavedSessionActionPrompt().find(
                "uncommitted project editor draft"),
            std::string::npos);
  manager->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentSessionId(), session->session_id());
  EXPECT_EQ(MinecartTrackEditorPanelTestPeer::OverlayInputs(*panel),
            invalid_inputs);
  EXPECT_TRUE(session->project_dirty);

  manager->RemoveSession(0);
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->session_coordinator()->GetTotalSessionCount(), 2u);
  manager->CancelPendingUnsavedSessionAction();

  manager->Quit();
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_FALSE(manager->quit());
  manager->CancelPendingUnsavedSessionAction();
}

TEST(MinecartTrackEditorPanelTest,
     GlobalProjectSaveAsCommitsFocusedOverlayDraftAndReadsBack) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture("global_save_as", "Global Save As", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(fixture.project_path().string()).ok());
  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  ScopedBoundMinecartPanel binding(manager.get(), session);
  auto* panel = binding.panel();

  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(*panel, "$00C1");
  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::NotifyProjectDraftChanged(*panel).ok());
  const std::filesystem::path save_as_path = fixture.Path("saved-copy.yaze");
  ASSERT_TRUE(manager->SaveProjectAs(save_as_path.string()).ok());

  EXPECT_EQ(manager->GetCurrentProject()->filepath, save_as_path.string());
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->filepath, save_as_path.string());
  EXPECT_FALSE(session->project_dirty);
  EXPECT_FALSE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));
  EXPECT_EQ(fixture.ReadProject(save_as_path).dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC1}));
  EXPECT_EQ(fixture.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xB0}));
}

TEST(MinecartTrackEditorPanelTest,
     FailedGlobalProjectWriteKeepsCommittedOverlayDirtyForRetry) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture("failed_write", "Failed Write", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(fixture.project_path().string()).ok());
  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  ScopedBoundMinecartPanel binding(manager.get(), session);
  auto* panel = binding.panel();

  const std::filesystem::path failed_path =
      fixture.Path("missing-parent/retry.yaze");
  manager->GetCurrentProject()->filepath = failed_path.string();
  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(*panel, "$00C2");
  ASSERT_TRUE(
      MinecartTrackEditorPanelTestPeer::NotifyProjectDraftChanged(*panel).ok());

  const absl::Status failed_save = manager->SaveProject();
  EXPECT_FALSE(failed_save.ok());
  EXPECT_FALSE(std::filesystem::exists(failed_path));
  EXPECT_EQ(fixture.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xB0}));
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC2}));
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC2}));
  EXPECT_TRUE(session->project_dirty);
  EXPECT_FALSE(
      MinecartTrackEditorPanelTestPeer::HasPendingProjectDraftChanges(*panel));

  ASSERT_TRUE(std::filesystem::create_directories(failed_path.parent_path()));
  ASSERT_TRUE(manager->SaveProject().ok());
  EXPECT_FALSE(session->project_dirty);
  EXPECT_EQ(fixture.ReadProject(failed_path).dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xC2}));
}

TEST(MinecartTrackEditorPanelTest,
     TransientInactiveSessionCannotMutateOrSaveUserProjectContext) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture_a("context_a", "Context A", 0xA0);
  ScopedManagerProject fixture_b("context_b", "Context B", 0xB0);

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(
      manager->OpenRomOrProject(fixture_a.project_path().string()).ok());
  ASSERT_TRUE(
      manager->OpenRomOrProject(fixture_b.project_path().string()).ok());
  manager->SwitchToSession(0);

  auto* session_a =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  auto* session_b =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(1));
  ASSERT_NE(session_a, nullptr);
  ASSERT_NE(session_b, nullptr);
  ASSERT_TRUE(
      manager->IsCurrentProjectContextOwnedBySession(session_a->session_id()));
  ASSERT_FALSE(
      manager->IsCurrentProjectContextOwnedBySession(session_b->session_id()));

  auto* dungeon_b =
      session_b->editors.GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon_b, nullptr);
  MinecartTrackEditorPanel panel_b;
  DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(*dungeon_b,
                                                                    &panel_b);
  manager->ConfigureSession(session_b);
  MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(panel_b, "$00C0");

  absl::Status edit_status = absl::UnknownError("callback did not run");
  absl::Status draft_status = absl::UnknownError("callback did not run");
  absl::Status save_status = absl::UnknownError("callback did not run");
  CallbackEditor transient_editor([&]() -> absl::Status {
    draft_status =
        MinecartTrackEditorPanelTestPeer::NotifyProjectDraftChanged(panel_b);
    edit_status =
        MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel_b);
    MinecartTrackEditorPanelTestPeer::SetTrackTilesInput(panel_b, "0xB0");
    save_status =
        MinecartTrackEditorPanelTestPeer::SaveProjectSettings(panel_b);
    return absl::OkStatus();
  });
  session_b->editors.active_editors_.push_back(&transient_editor);

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280.0f, 720.0f);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();
  manager->session_coordinator()->UpdateSessions();
  ImGui::EndFrame();

  session_b->editors.active_editors_.erase(
      std::remove(session_b->editors.active_editors_.begin(),
                  session_b->editors.active_editors_.end(), &transient_editor),
      session_b->editors.active_editors_.end());
  DungeonEditorV2MinecartTrackTestPeer::SetMinecartTrackEditorPanel(*dungeon_b,
                                                                    nullptr);

  EXPECT_TRUE(absl::IsFailedPrecondition(draft_status)) << draft_status;
  EXPECT_TRUE(absl::IsFailedPrecondition(edit_status)) << edit_status;
  EXPECT_TRUE(absl::IsFailedPrecondition(save_status)) << save_status;
  EXPECT_EQ(manager->GetCurrentSessionId(), session_a->session_id());
  EXPECT_EQ(manager->GetCurrentProject()->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xA0}));
  ASSERT_TRUE(session_a->project_context.has_value());
  ASSERT_TRUE(session_b->project_context.has_value());
  EXPECT_EQ(session_a->project_context->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xA0}));
  EXPECT_EQ(session_b->project_context->dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xB0}));
  EXPECT_FALSE(session_a->project_dirty);
  EXPECT_FALSE(session_b->project_dirty);
  EXPECT_EQ(fixture_a.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xA0}));
  EXPECT_EQ(fixture_b.ReadProject().dungeon_overlay.track_tiles,
            (std::vector<uint16_t>{0xB0}));
}

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

TEST(MinecartTrackEditorPanelTest,
     FlatSourcePublishesExplicitlyAndRebasesTheDraft) {
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
  ASSERT_TRUE(panel.SaveTracks().ok());
  std::string expected_source = source_before;
  expected_source.replace(expected_source.find("$0100"), 5, "$0777");
  EXPECT_EQ(fixture.ReadSource(), expected_source);
  EXPECT_FALSE(panel.HasUnpublishedChanges());
  EXPECT_EQ(panel.GetTracks().front().room_id, 0x0777);
  EXPECT_TRUE(absl::IsFailedPrecondition(panel.SaveTracks()));
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

TEST(MinecartTrackEditorPanelTest,
     DescriptorRelocationRebindsWhenCleanAndPreservesDraftWhenDirty) {
  ScopedTestProject first_project;
  first_project.WriteSource(MakeFlatSource(32, /*room_base=*/0x0100));
  ScopedTestProject second_project;
  second_project.WriteSource(MakeFlatSource(32, /*room_base=*/0x0300));

  project::YazeProject* project = first_project.project();
  const std::string first_filepath = project->filepath;
  const std::string second_filepath = second_project.project()->filepath;
  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(project).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0100);

  project->filepath = second_filepath;
  ASSERT_TRUE(panel.ReloadTracks().ok());
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0300);
  ASSERT_TRUE(panel.ResolveTrackSourcePath().ok());
  EXPECT_EQ(*panel.ResolveTrackSourcePath(),
            std::filesystem::canonical(second_project.source_path()));

  MinecartTrack draft = panel.GetTracks()[0];
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());
  project->filepath = first_filepath;

  const absl::Status relocation_status = panel.ReloadTracks();
  EXPECT_TRUE(absl::IsFailedPrecondition(relocation_status))
      << relocation_status;
  EXPECT_NE(std::string(relocation_status.message()).find("descriptor moved"),
            std::string::npos);
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0777);
  EXPECT_TRUE(panel.HasUnpublishedChanges());
  EXPECT_TRUE(
      absl::IsFailedPrecondition(panel.ResolveTrackSourcePath().status()));

  project->filepath = second_filepath;
  ASSERT_TRUE(panel.ResolveTrackSourcePath().ok());
  EXPECT_EQ(*panel.ResolveTrackSourcePath(),
            std::filesystem::canonical(second_project.source_path()));
  ASSERT_TRUE(panel.DiscardUnpublishedChanges().ok());
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
  if (symlink_error) {
    GTEST_SKIP() << "Symlink creation is not permitted: "
                 << symlink_error.message();
  }

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  const absl::Status status = panel.ReloadTracks();

  EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
  EXPECT_TRUE(panel.GetTracks().empty());
  EXPECT_FALSE(panel.HasUnpublishedChanges());
}

TEST(MinecartTrackEditorPanelTest,
     MissingManifestSourceFailsClosedBeforeLoading) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeFlatSource(32));
  const std::string source_before = fixture.ReadSource();
  fixture.project()->hack_manifest.Clear();

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  const absl::Status status = panel.ReloadTracks();

  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_NE(std::string(status.message()).find("minecart_tracks.source"),
            std::string::npos);
  EXPECT_TRUE(panel.GetTracks().empty());
  EXPECT_EQ(fixture.ReadSource(), source_before);
}

TEST(MinecartTrackEditorPanelTest,
     ManifestIdentityChangeRejectsPublishAndPreservesDraft) {
  constexpr char kAlternateSource[] = "Data/alternate_minecart_tracks.asm";
  ScopedTestProject fixture;
  fixture.WriteSource(MakeFlatSource(32));
  fixture.WriteSourceAt(kAlternateSource,
                        MakeFlatSource(32, /*room_base=*/0x0300));
  const std::string original_source = fixture.ReadSource();
  const std::string alternate_source = [&]() {
    std::ifstream file(fixture.source_path(kAlternateSource), std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }();

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());
  MinecartTrack draft = panel.GetTracks()[0];
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());
  ASSERT_TRUE(fixture.LoadManifest(kAlternateSource).ok());

  const absl::Status status = panel.SaveTracks();
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_NE(std::string(status.message()).find("changed"), std::string::npos);
  EXPECT_EQ(fixture.ReadSource(), original_source);
  std::ifstream alternate_file(fixture.source_path(kAlternateSource),
                               std::ios::binary);
  std::stringstream alternate_buffer;
  alternate_buffer << alternate_file.rdbuf();
  EXPECT_EQ(alternate_buffer.str(), alternate_source);
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0777);
  EXPECT_TRUE(panel.HasUnpublishedChanges());
}

TEST(MinecartTrackEditorPanelTest,
     ExternalSourceChangeFailsCasAndPreservesExternalBytesAndDraft) {
  ScopedTestProject fixture;
  fixture.WriteSource(MakeFlatSource(32));

  MinecartTrackEditorPanel panel;
  ASSERT_TRUE(panel.SetProject(fixture.project()).ok());
  ASSERT_TRUE(panel.ReloadTracks().ok());
  MinecartTrack draft = panel.GetTracks()[0];
  draft.room_id = 0x0777;
  ASSERT_TRUE(panel.UpdateTrack(0, draft).ok());

  const std::string external_source =
      fixture.ReadSource() + "\n; external author update\n";
  fixture.WriteSource(external_source);
  const absl::Status status = panel.SaveTracks();

  EXPECT_TRUE(absl::IsAborted(status)) << status;
  EXPECT_NE(std::string(status.message()).find("CAS failed"),
            std::string::npos);
  EXPECT_EQ(fixture.ReadSource(), external_source);
  EXPECT_EQ(panel.GetTracks()[0].room_id, 0x0777);
  EXPECT_TRUE(panel.HasUnpublishedChanges());
}

TEST(MinecartTrackEditorPanelTest,
     DungeonSavesBlockDraftsThenGuardedPublicationRebases) {
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
  EXPECT_NE(std::string(save_status.message())
                .find("ROM Save/Apply never publishes ASM source"),
            std::string::npos)
      << save_status;
  EXPECT_NE(std::string(save_status.message()).find("Publish Tracks"),
            std::string::npos)
      << save_status;
  EXPECT_NE(std::string(save_status.message()).find("discard the drafts"),
            std::string::npos)
      << save_status;
  EXPECT_EQ(rom.vector(), rom_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(fixture.ReadSource(), source_before);
  EXPECT_TRUE(panel.HasUnpublishedChanges());

  const absl::Status save_room_status = editor.SaveRoom(0);
  EXPECT_TRUE(absl::IsFailedPrecondition(save_room_status)) << save_room_status;
  EXPECT_NE(std::string(save_room_status.message())
                .find("ROM Save/Apply never publishes ASM source"),
            std::string::npos)
      << save_room_status;
  EXPECT_NE(std::string(save_room_status.message()).find("Publish Tracks"),
            std::string::npos)
      << save_room_status;
  EXPECT_NE(std::string(save_room_status.message()).find("discard the drafts"),
            std::string::npos)
      << save_room_status;
  EXPECT_EQ(rom.vector(), rom_before);
  EXPECT_EQ(rom.dirty(), dirty_before);
  EXPECT_EQ(fixture.ReadSource(), source_before);
  EXPECT_TRUE(panel.HasUnpublishedChanges());

  ASSERT_TRUE(panel.SaveTracks().ok());
  std::string expected_source = source_before;
  expected_source.replace(expected_source.find("$0200"), 5, "$0777");
  EXPECT_EQ(fixture.ReadSource(), expected_source);
  EXPECT_FALSE(panel.HasUnpublishedChanges());
  EXPECT_FALSE(editor.HasPendingDungeonChanges());
}

}  // namespace yaze::editor
