// Product save/reopen for a disposable Oracle ROM and project.
//
// SaveAsPopupTest drives the Save As popup with subsystem serialization turned
// off. That proves destination and session handling. The cases here leave the
// project save flags in place, edit through the editor undo stack, and commit
// with EditorManager::SaveRom / SaveRomAs.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/minecart_track_source.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_pixel_command.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui/navigation/overworld_sidebar.h"
#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "app/editor/shell/feedback/popup_manager.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/background_renderer.h"
#include "core/features.h"
#include "core/hack_manifest.h"
#include "editor_test_support.h"
#include "framework/rom_save_fault.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "oracle_rom_fixture.h"
#include "rom/snes.h"
#include "test_utils.h"
#include "testing.h"
#include "unique_temp_path.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/graphics/resolved_graphics_context.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

class AuthoringSaveWorkflowTestPeer {
 public:
  static ObjectTileEditorPanel* OpenObjectTiles(DungeonEditorV2& editor,
                                                int room_id, int id) {
    zelda3::RoomObject object(id, 0, 0, 0, 0);
    const auto status = editor.OpenObjectTileEditorForObject(room_id, object);
    EXPECT_TRUE(status.ok()) << status;
    return status.ok() ? editor.object_tile_editor_panel_ : nullptr;
  }
  static void PublishFirstWallTile(ObjectTileEditorPanel& panel) {
    auto& cell = panel.current_layout_.cells.front();
    cell.tile_info.id_ ^= 0x10;
    cell.modified = true;
    panel.ApplyChanges();
    if (panel.pending_shared_confirmation_)
      panel.ApplyChanges(false);
    EXPECT_FALSE(panel.HasUnappliedChanges()) << panel.action_status_message_;
  }

  static void SelectMapFloor(ScreenEditor& editor, int dungeon, int floor) {
    editor.selected_dungeon = dungeon;
    editor.floor_number = floor;
    editor.select_floor_tab_ = true;
  }
  static void DrawMapTabs(ScreenEditor& editor) {
    editor.DrawDungeonMapsTabs();
  }
  static ScreenSnapshot MapSnapshot(const ScreenEditor& editor) {
    return editor.CaptureDungeonMapSnapshot();
  }

  static absl::Status OpenScreenGraphics(ScreenEditor& editor) {
    editor.selected_tile8_ = 64;
    return editor.OpenDungeonMapTileGraphics();
  }
  static absl::Status OpenSpriteGraphics(SpriteEditor& editor) {
    editor.current_frame_ = 0;
    editor.selected_tile_index_ = 0;
    return editor.OpenSelectedOamGraphics();
  }
  static void OpenObjectGraphics(DungeonEditorV2& editor, int room_id,
                                 const zelda3::RoomObject& object) {
    editor.OpenGraphicsEditorForObject(room_id, object);
  }

  static absl::Status LoadDungeonRoom(DungeonEditorV2& editor, int room_id) {
    return editor.room_loader_.LoadRoom(room_id, editor.rooms_[room_id]);
  }

  static void SynchronizePatternAssets(DungeonEditorV2& editor) {
    editor.SynchronizeCustomObjectAssets();
  }

  static GfxGroupEditor& MainGroups(GraphicsEditor& editor) {
    return *editor.gfx_group_panel_;
  }
  static GfxGroupEditor& MainGroups(OverworldEditor& editor) {
    return editor.gfx_group_editor_;
  }
  static void DrawRoomUsage(GraphicsEditor& editor) {
    editor.usage_preview_view_->Draw();
  }
  static std::array<uint8_t, 16> RoomUsageBlocks(GraphicsEditor& editor,
                                                 int room_id) {
    return editor.usage_preview_view_->room_blocks_.at(room_id);
  }

  static usage_preview::PreviewImage RoomUsageImage(GraphicsEditor& editor) {
    return editor.usage_preview_view_->room_preview_.Snapshot();
  }
  static std::vector<usage_preview::RoomTileReference> RoomUsageReferences(
      GraphicsEditor& editor) {
    return editor.usage_preview_view_->room_preview_.tile_references();
  }
  static std::string RoomUsageContextLabel(GraphicsEditor& editor, int room) {
    const auto& contexts = editor.usage_preview_view_->live_rooms_;
    const auto found = contexts.find(room);
    return usage_preview::RoomContextLabel(
        found == contexts.end() ? nullptr : &found->second);
  }

  static void ChooseRoomUsage(GraphicsEditor& editor, int room_id) {
    auto& view = *editor.usage_preview_view_;
    const auto found = std::find(view.rooms_for_sheet_.begin(),
                                 view.rooms_for_sheet_.end(), room_id);
    ASSERT_NE(found, view.rooms_for_sheet_.end());
    view.room_choice_ = static_cast<int>(found - view.rooms_for_sheet_.begin());
    view.dirty_ = true;
  }

  static GraphicsEditorState& GraphicsState(GraphicsEditor& editor) {
    return editor.state_;
  }

  static void PaintGraphicsPixel(GraphicsEditor& editor, int x, int y,
                                 uint8_t color, bool finish = true) {
    editor.state_.current_color_index = color;
    auto& panel = *editor.pixel_editor_panel_;
    panel.BeginStroke();
    panel.ApplyPencil(x, y);
    if (finish)
      panel.EndStroke();
  }

  static void EndGraphicsStroke(GraphicsEditor& editor) {
    editor.pixel_editor_panel_->EndStroke();
  }

  static void DrawGraphics(GraphicsEditor& editor) {
    EXPECT_OK(editor.pixel_editor_panel_->Update());
  }

  static void DrawGraphicsPalette(GraphicsEditor& editor) {
    editor.pixel_editor_panel_->DrawColorPicker();
  }

  static gfx::Bitmap& GraphicsDisplay(GraphicsEditor& editor) {
    return editor.pixel_editor_panel_->DisplaySheet();
  }

  static void DrawUsage(GraphicsEditor& editor) {
    editor.usage_preview_view_->Draw();
  }

  static absl::StatusOr<MinecartTrackEditorPanel*> MinecartPanel(
      DungeonEditorV2& editor) {
    const auto status = editor.EnsureMinecartTrackEditorPanel();
    if (!status.ok())
      return status;
    return editor.minecart_track_editor_panel_;
  }

  static DungeonCanvasViewer* Viewer(DungeonEditorV2& editor, int room_id) {
    return editor.GetViewerForRoom(room_id);
  }

  static void ActivateDungeonRoom(DungeonEditorV2& editor, int room) {
    editor.current_room_id_ = room;
    editor.SyncPanelsToRoom(room);
  }

  static void SelectDungeonRoom(DungeonEditorV2& editor, int room) {
    editor.OnRoomSelected(room);
  }

  static void SelectDungeonEntrance(DungeonEditorV2& editor, int slot) {
    editor.OnEntranceSelected(slot);
  }
  static void PreviewDungeonEntrance(DungeonEditorV2& editor, int slot) {
    editor.SetGraphicsPreviewEntrance(slot);
  }

  static absl::Status EditAreaGraphics(OverworldEditor& editor, int map_id,
                                       int group) {
    return editor.ApplyOverworldPropertyEdit(
        {map_id, OverworldPropertyField::kAreaGraphics, 0, group});
  }

  static EditingMode EditingModeOf(OverworldEditor& editor) {
    return editor.current_mode;
  }

  static void DrawToolbar(OverworldEditor& editor) {
    editor.toolbar_->Draw(
        editor.current_world_, editor.current_map_, editor.current_map_lock_,
        editor.current_mode, editor.entity_edit_mode_,
        editor.dependencies_.window_manager, editor.rom_, &editor.overworld_,
        editor.dependencies_.project, editor.game_state_, [&]() {
          editor.sidebar_->DrawQuickProperties(
              editor.current_map_, editor.current_map_lock_,
              editor.dependencies_.project, true);
        });
  }

  static absl::Status DrawOverworld(OverworldEditor& editor) {
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    ImGui::Begin("AuthoringOverworldHost");
    const auto result = editor.Update();
    ImGui::End();
    ImGui::Render();
    return result;
  }

  static absl::Status EditAreaPalette(OverworldEditor& editor, int map_id,
                                      int value) {
    return editor.ApplyOverworldPropertyEdit(
        {map_id, OverworldPropertyField::kAreaPalette, 0, value});
  }

  static absl::Status EditAreaSize(OverworldEditor& editor, int map_id,
                                   zelda3::AreaSizeEnum size) {
    return editor.ApplyOverworldPropertyEdit(
        {map_id, OverworldPropertyField::kAreaSize, 0, static_cast<int>(size)});
  }

  struct PaintedTile {
    int x = 0;
    int y = 0;
    uint16_t original = 0;
    uint16_t edited = 0;
  };

  // One light-world tile through the paint undo action the canvas uses.
  static PaintedTile PaintTile(OverworldEditor& editor, int x, int y) {
    auto& overworld = editor.overworld();
    overworld.set_current_world(0);
    overworld.set_current_map(0);
    PaintedTile painted;
    painted.x = x;
    painted.y = y;
    painted.original = overworld.GetTile(x, y);
    const uint16_t neighbor = overworld.GetTile(x + 1, y);
    painted.edited = neighbor != painted.original
                         ? neighbor
                         : static_cast<uint16_t>(painted.original == 0
                                                     ? 1
                                                     : painted.original - 1);
    overworld.SetTile(x, y, painted.edited);
    editor.CreateUndoPoint(0, 0, x, y, painted.original);
    editor.FinalizePaintOperation();
    return painted;
  }

  static uint16_t TileAt(OverworldEditor& editor, int x, int y) {
    editor.overworld().set_current_world(0);
    editor.overworld().set_current_map(0);
    return editor.overworld().GetTile(x, y);
  }

  // Exercise the production canvas paste handler with headless ImGui input.
  static absl::Status PastePattern(OverworldEditor& editor, int x, int y,
                                   const TileBrush& brush, int map_id = 0) {
    editor.SelectMapForEditing(map_id, /*respect_pin=*/false);
    auto& canvas = editor.ow_map_canvas_;
    canvas.set_global_scale(1.0f);
    canvas.set_scrolling(ImVec2(-x * 16.0f, -y * 16.0f));
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    absl::Status result =
        absl::FailedPreconditionError("Paste was not invoked");
    for (int frame = 0; frame < 3; ++frame) {
      const auto origin = canvas.zero_point();
      ImGui::GetIO().AddMousePosEvent(frame < 2 ? -100.0f : origin.x + 8.0f,
                                      frame < 2 ? -100.0f : origin.y + 8.0f);
      ImGui::NewFrame();
      ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_Always);
      ImGui::Begin("AuthoringPasteHost", nullptr,
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize);
      canvas.DrawBackground(ImVec2(256, 256));
      if (frame == 2) {
        result = editor.tile_painting_->PasteBrush(brush);
      }
      ImGui::End();
      ImGui::Render();
    }
    return result;
  }

  static absl::Status EditBeginningMusic(OverworldEditor& editor, int map_id,
                                         int value) {
    return editor.ApplyOverworldPropertyEdit(
        {map_id, OverworldPropertyField::kMusic, 0, value});
  }
};

namespace {

constexpr const char* kSharedRom =
    "/Users/scawful/src/hobby/oracle-of-secrets/Roms/oos168.sfc";
constexpr const char* kSharedProject =
    "/Users/scawful/src/hobby/oracle-of-secrets/Oracle-of-Secrets.yaze";
constexpr const char* kSharedManifest =
    "/Users/scawful/src/hobby/oracle-of-secrets/Roms/hack_manifest.json";
constexpr const char* kSharedCollision =
    "/Users/scawful/src/hobby/oracle-of-secrets/Data/dungeons/"
    "custom_collision.json";
constexpr const char* kSharedTrackSource =
    "/Users/scawful/src/hobby/oracle-of-secrets/Sprites/Objects/data/"
    "minecart_tracks.asm";
constexpr int kOverworldMap = 48;
constexpr int kDungeonRoom = 5;

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
  }
  ~ScopedImGuiContext() { ImGui::DestroyContext(ctx); }
};

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>());
}

void ReplaceAssignment(std::string* text, std::string_view key,
                       std::string_view value) {
  const std::string prefix = absl::StrCat(key, "=");
  const auto pos = text->find(prefix);
  ASSERT_NE(pos, std::string::npos) << prefix;
  const auto end = text->find('\n', pos);
  ASSERT_NE(end, std::string::npos) << prefix;
  text->replace(pos, end - pos, prefix + std::string(value));
}

void ExpectRangeEqual(const std::vector<uint8_t>& before,
                      const std::vector<uint8_t>& after, int start, int length,
                      std::string_view label) {
  ASSERT_GE(start, 0) << label;
  ASSERT_LE(static_cast<size_t>(start + length), before.size()) << label;
  EXPECT_TRUE(std::equal(before.begin() + start,
                         before.begin() + start + length,
                         after.begin() + start))
      << label;
}

bool IsProtectionRefusal(const absl::Status& status) {
  if (absl::IsPermissionDenied(status)) {
    return true;
  }
  if (!absl::IsCancelled(status)) {
    return false;
  }
  const absl::string_view message = status.message();
  return message.find("conflict") != absl::string_view::npos ||
         message.find("Write conflict") != absl::string_view::npos;
}

void ExpectProtectedRegionsMatch(
    const std::vector<uint8_t>& before, const std::vector<uint8_t>& after,
    const std::vector<core::ProtectedRegion>& regions) {
  ASSERT_FALSE(regions.empty());
  ASSERT_EQ(before.size(), after.size());
  for (const auto& region : regions) {
    const uint32_t start = SnesToPc(region.start);
    const uint32_t end = SnesToPc(region.end);
    ASSERT_LT(start, end) << region.module;
    ASSERT_LE(static_cast<size_t>(end), before.size()) << region.module;
    EXPECT_TRUE(std::equal(before.begin() + start, before.begin() + end,
                           after.begin() + start))
        << region.module << " SNES " << absl::StrFormat("%06x", region.start);
  }
}

struct DisposableOracleProject {
  std::filesystem::path root;
  std::filesystem::path rom;
  std::filesystem::path project;
  std::string shared_hash;
  std::string source_hash;
};

class AuthoringSaveWorkflowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    original_custom_state_ = zelda3::CustomObjectManager::Get().SnapshotState();
    original_custom_context_ =
        zelda3::CustomObjectManager::Get().active_runtime_context_id();
    original_flags_ = core::FeatureFlags::get();
    // NullRenderer cannot fulfill texture uploads. Do not retry work whose
    // Bitmap owners belonged to a previous application fixture.
    gfx::Arena::Get().ClearTextureQueue();
    if (!std::filesystem::exists(kSharedRom) ||
        !std::filesystem::exists(kSharedProject) ||
        !std::filesystem::exists(kSharedManifest) ||
        !std::filesystem::exists(kSharedCollision)) {
      GTEST_SKIP() << "Oracle base ROM/project fixture is not available";
    }
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    manager_.reset();
    auto& custom = zelda3::CustomObjectManager::Get();
    if (original_custom_context_) {
      custom.ActivateRuntimeContext(*original_custom_context_,
                                    original_custom_state_);
    } else {
      custom.ActivateStandaloneContext();
      custom.RestoreState(original_custom_state_);
    }
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get() = original_flags_;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    if (!root_.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(root_, ec);
    }
  }

  DisposableOracleProject CopyProjectFrom(
      const char* name, const std::filesystem::path& rom_source,
      const std::filesystem::path& project_source,
      const std::filesystem::path& manifest_source,
      const std::filesystem::path& collision_source) {
    root_ = test::UniqueTempPath(name);
    std::filesystem::create_directories(root_ / "Backups");
    DisposableOracleProject fixture;
    fixture.root = root_;
    fixture.rom = root_ / "oos168.sfc";
    fixture.project = root_ / "authoring.yaze";
    std::filesystem::copy_file(rom_source, fixture.rom);
    std::filesystem::copy_file(manifest_source, root_ / "hack_manifest.json");
    std::filesystem::copy_file(collision_source,
                               root_ / "custom_collision.json");
    std::ifstream input(project_source);
    std::string text((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
    ReplaceAssignment(&text, "rom_filename", "oos168.sfc");
    ReplaceAssignment(&text, "hack_manifest_file", "hack_manifest.json");
    ReplaceAssignment(&text, "custom_collision_json", "custom_collision.json");
    ReplaceAssignment(&text, "symbols_filename", "");
    ReplaceAssignment(&text, "output_folder", ".");
    ReplaceAssignment(&text, "build_target", "patched-output.sfc");
    ReplaceAssignment(&text, "rom_backup_folder", "Backups");
    ReplaceAssignment(&text, "cutscene_shots", "");
    if (text.find("dungeon_tile_patterns=") != std::string::npos) {
      ReplaceAssignment(&text, "dungeon_tile_patterns", "");
    }
    ReplaceAssignment(&text, "code_folder", ".");
    ReplaceAssignment(&text, "assets_folder", ".");
    ReplaceAssignment(&text, "patches_folder", ".");
    ReplaceAssignment(&text, "custom_objects_folder", ".");
    {
      std::ofstream output(fixture.project);
      output << text;
    }
    fixture.shared_hash = util::ComputeFileSha1Hex(rom_source.string());
    fixture.source_hash = util::ComputeFileSha1Hex(fixture.rom.string());
    EXPECT_EQ(fixture.shared_hash, fixture.source_hash);
    EXPECT_FALSE(fixture.shared_hash.empty());
    return fixture;
  }

  DisposableOracleProject CopyProject(const char* name) {
    return CopyProjectFrom(name, kSharedRom, kSharedProject, kSharedManifest,
                           kSharedCollision);
  }

  void Open(const DisposableOracleProject& fixture) {
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_OK(manager_->OpenRomOrProject(fixture.project.string()));
    ASSERT_NE(manager_->GetCurrentProject(), nullptr);
    ASSERT_TRUE(manager_->GetCurrentProject()->hack_manifest.loaded());
    EXPECT_EQ(manager_->GetProjectRomWritePolicy(),
              project::RomWritePolicy::kBlock);
  }

  void QualifySharedGraphicsAssignment(
      bool custom_sheets, EditorType host_type = EditorType::kDungeon);
  void QualifyMainGraphicsAssignment(bool custom_sheets);

  void QualifySpecialArea(zelda3::AreaSizeEnum size, int child, int x, int y);
  void QualifyPaletteWordRoundTrip(bool dungeon_source);

  void QualifySourcePixelRoundTrip(const DisposableOracleProject& fixture) {
    auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
    auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
    ASSERT_TRUE(state.source_context);
    ASSERT_TRUE(state.SourceTarget());
    const auto context = *state.source_context;
    const auto target = *state.SourceTarget();
    ASSERT_EQ(target.palette.size(), 8u);
    const int x = (target.tile_index % 16) * 8;
    const int y = (target.tile_index / 16) * 8;
    const int offset = y * 128 + x;
    const auto original =
        state.game_data->sheet_store.Sheet(target.sheet_id)[offset];
    const uint8_t changed = original == 1 ? 2 : 1;
    const auto before = context.render_preview();
    ASSERT_OK(before.status());
    ASSERT_FALSE(before->empty());
    const auto revision =
        state.game_data->sheet_store.Revision(target.sheet_id);
    AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(*graphics, x, y, changed,
                                                      false);
    const auto painted = context.render_preview();
    ASSERT_OK(painted.status());
    EXPECT_EQ(state.game_data->sheet_store.Revision(target.sheet_id), revision);
    EXPECT_NE(painted->pixels, before->pixels);
    AuthoringSaveWorkflowTestPeer::EndGraphicsStroke(*graphics);
    ASSERT_OK(graphics->Undo());
    const auto undone = context.render_preview();
    ASSERT_OK(undone.status());
    EXPECT_EQ(undone->ToRgba(), before->ToRgba());
    ASSERT_OK(graphics->Redo());
    const auto redone = context.render_preview();
    ASSERT_OK(redone.status());
    EXPECT_EQ(redone->ToRgba(), painted->ToRgba());
    ASSERT_TRUE(context.return_to_source);
    context.return_to_source();
    const auto destination = fixture.root / "source-pixels.sfc";
    ASSERT_OK(manager_->SaveRomAs(destination.string()));
    ExpectProtectedRegionsMatch(
        ReadFile(fixture.rom), ReadFile(destination),
        manager_->GetCurrentProject()->hack_manifest.protected_regions());
    auto reopened = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*reopened, &renderer_);
    reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
    ASSERT_OK(reopened->EnsureEditorAssetsLoaded(EditorType::kGraphics));
    auto& fresh = AuthoringSaveWorkflowTestPeer::GraphicsState(
        *reopened->GetCurrentEditorSet()->GetGraphicsEditor());
    EXPECT_EQ(fresh.game_data->sheet_store.Sheet(target.sheet_id)[offset],
              changed);
    EXPECT_FALSE(fresh.source_context);
    reopened.reset();
    EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
              fixture.source_hash);
    EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  }

  std::filesystem::path root_;
  core::FeatureFlags::Flags original_flags_{};
  zelda3::CustomObjectManager::State original_custom_state_;
  std::optional<uint64_t> original_custom_context_;
  ScopedImGuiContext imgui_;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
};

TEST_F(AuthoringSaveWorkflowTest,
       DungeonMapFloorControlsUndoRetrySaveAsReopen) {
  const auto fixture = CopyProject("yaze-map-floor-save");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kScreen));
  auto* screen = manager_->GetCurrentEditorSet()->GetEditorAs<ScreenEditor>(
      EditorType::kScreen);
  ASSERT_NE(screen, nullptr);
  // This fixture explicitly opts into the bounded map-record save domain.
  // All other project save settings remain as the real authoring project sets them.
  core::FeatureFlags::get().kSaveDungeonMaps = true;
  const auto control_path = fixture.root / "control.sfc";
  ASSERT_OK(manager_->SaveRomAs(control_path.string()));
  const auto control = ReadFile(control_path);
  const auto all_before = screen->dungeon_maps_;
  ASSERT_EQ(all_before.size(), 14u);
  AuthoringSaveWorkflowTestPeer::SelectMapFloor(*screen, 0, 0);

  auto& io = ImGui::GetIO();
  io.ConfigInputTrickleEventQueue = false;
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  const auto frame = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(700, 700), ImGuiCond_Always);
    ImGui::Begin("DungeonMapFloorControls", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    AuthoringSaveWorkflowTestPeer::DrawMapTabs(*screen);
    ImGui::End();
    ImGui::Render();
  };
  frame();
  frame();
  const auto before = AuthoringSaveWorkflowTestPeer::MapSnapshot(*screen);
  std::optional<gui::WidgetIdRegistry::WidgetInfo> button;
  for (const auto& [path, item] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    if (item.label == gui::WidgetIdRegistry::NormalizeLabel("Add Basement"))
      button = item;
  }
  ASSERT_TRUE(button);
  io.AddMousePosEvent((button->bounds.min_x + button->bounds.max_x) / 2,
                      (button->bounds.min_y + button->bounds.max_y) / 2);
  frame();
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frame();
  frame();
  const auto after = AuthoringSaveWorkflowTestPeer::MapSnapshot(*screen);
  ASSERT_EQ(after.dungeon_map.map_data.nbr_of_basement,
            before.dungeon_map.map_data.nbr_of_basement + 1);
  EXPECT_EQ(after.dungeon_map.floor_index, before.dungeon_map.floor_index + 1);
  EXPECT_EQ(after.dungeon_map.map_data.floor_rooms[1],
            before.dungeon_map.map_data.floor_rooms[0]);
  EXPECT_TRUE(screen->HasPendingDungeonMapChanges());
  ASSERT_OK(screen->Undo());
  frame();
  frame();
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::MapSnapshot(*screen)
                .dungeon_map.floor_index,
            before.dungeon_map.floor_index);
  EXPECT_EQ(screen->dungeon_maps_[0].floor_rooms,
            before.dungeon_map.map_data.floor_rooms);
  ASSERT_OK(screen->Redo());
  frame();
  frame();
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::MapSnapshot(*screen)
                .dungeon_map.floor_index,
            after.dungeon_map.floor_index);
  const auto expected_maps = screen->dungeon_maps_;
  const auto plan = screen->PrepareDungeonMapSave();
  ASSERT_OK(plan.status());
  ASSERT_FALSE(plan->writes.empty());
  const auto undo_count = screen->undo_manager().UndoStackSize();
  const auto destination = fixture.root / "dungeon-map.sfc";
  const auto rom_before = manager_->GetCurrentRom()->vector();
  {
    test::ScopedRomStagingFailure fail;
    EXPECT_FALSE(manager_->SaveRomAs(destination.string()).ok());
  }
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), rom_before);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), control_path.string());
  EXPECT_EQ(screen->undo_manager().UndoStackSize(), undo_count);
  EXPECT_TRUE(screen->HasPendingDungeonMapChanges());
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_FALSE(screen->HasPendingScreenChanges());
  EXPECT_EQ(screen->undo_manager().UndoStackSize(), undo_count);
  const auto saved = ReadFile(destination);
  ASSERT_EQ(saved.size(), control.size());
  // Account for every byte changed by map serialization and ROM checksum.
  for (size_t pc = 0; pc < saved.size(); ++pc) {
    if (saved[pc] == control[pc] || (pc >= 0x7FDC && pc < 0x7FE0))
      continue;
    EXPECT_TRUE(std::any_of(
        plan->writes.begin(), plan->writes.end(),
        [pc](const auto& write) {
          return pc >= static_cast<size_t>(write.pc_address) &&
                 pc < static_cast<size_t>(write.pc_address) + write.data.size();
        }))
        << "Unexpected changed PC byte " << pc;
  }
  ExpectProtectedRegionsMatch(
      control, saved,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());

  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  ASSERT_OK(reopened->EnsureEditorAssetsLoaded(EditorType::kScreen));
  auto* fresh = reopened->GetCurrentEditorSet()->GetEditorAs<ScreenEditor>(
      EditorType::kScreen);
  ASSERT_EQ(fresh->dungeon_maps_.size(), expected_maps.size());
  for (size_t d = 0; d < expected_maps.size(); ++d) {
    const auto& actual = fresh->dungeon_maps_[d];
    const auto& expected = expected_maps[d];
    EXPECT_EQ(actual.boss_room, expected.boss_room);
    EXPECT_EQ(actual.nbr_of_basement, expected.nbr_of_basement);
    EXPECT_EQ(actual.nbr_of_floor, expected.nbr_of_floor);
    EXPECT_EQ(actual.floor_rooms, expected.floor_rooms);
    EXPECT_EQ(actual.floor_gfx, expected.floor_gfx);
    if (d != 0) {
      EXPECT_EQ(actual.floor_rooms, all_before[d].floor_rooms);
      EXPECT_EQ(actual.floor_gfx, all_before[d].floor_gfx);
    }
  }
  EXPECT_FALSE(fresh->HasPendingScreenChanges());
  reopened.reset();
  ASSERT_OK(screen->Undo());
  EXPECT_TRUE(screen->HasPendingDungeonMapChanges());
  ASSERT_OK(screen->Redo());
  EXPECT_EQ(screen->dungeon_maps_[0].floor_rooms, expected_maps[0].floor_rooms);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest, ScreenSourcePixelsUndoReturnSaveAsReopen) {
  const auto fixture = CopyProject("yaze-screen-source-pixels");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kScreen));
  auto* screen = manager_->GetCurrentEditorSet()->GetEditorAs<ScreenEditor>(
      EditorType::kScreen);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::OpenScreenGraphics(*screen));
  EXPECT_FALSE(screen->HasPendingScreenChanges());
  QualifySourcePixelRoundTrip(fixture);
  EXPECT_FALSE(screen->HasPendingScreenChanges());
}

TEST_F(AuthoringSaveWorkflowTest, GoronPaletteWordUndoRetrySaveAsReopen) {
  QualifyPaletteWordRoundTrip(/*dungeon_source=*/true);
}

TEST_F(AuthoringSaveWorkflowTest, OverworldPaletteWordUndoRetrySaveAsReopen) {
  QualifyPaletteWordRoundTrip(/*dungeon_source=*/false);
}

void AuthoringSaveWorkflowTest::QualifyPaletteWordRoundTrip(
    bool dungeon_source) {
  const auto fixture = CopyProject("yaze-palette-word-roundtrip");
  Open(fixture);
  const auto editor_type =
      dungeon_source ? EditorType::kDungeon : EditorType::kOverworld;
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(editor_type));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* editors = manager_->GetCurrentEditorSet();
  auto* graphics = editors->GetGraphicsEditor();
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  constexpr int kRoom = 0x89;
  constexpr int kMap = 0;
  int palette_id = -1;
  if (dungeon_source) {
    auto* dungeon = editors->GetEditorAs<DungeonEditorV2>(editor_type);
    AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kRoom);
    ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
    auto& room = dungeon->rooms()[kRoom];
    room.PrepareForRender();
    palette_id = room.ResolveDungeonPaletteId();
    for (const auto& object : room.GetTileObjects()) {
      AuthoringSaveWorkflowTestPeer::OpenObjectGraphics(*dungeon, kRoom,
                                                        object);
      if (state.source_context)
        break;
    }
  } else {
    auto* ow = editors->GetEditorAs<OverworldEditor>(editor_type);
    ow->SelectMapForEditing(kMap, false);
    ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
    palette_id = ow->overworld().overworld_map(kMap)->render_main_palette();
    for (int x = 0; x < 32 && !state.source_context; ++x) {
      for (int y = 0; y < 32 && !state.source_context; ++y) {
        const auto tile = ow->overworld().GetMapTiles(0)[x][y];
        for (int quadrant = 0; quadrant < 4 && !state.source_context;
             ++quadrant)
          (void)ow->OpenGraphicsForTile16Quadrant(tile, quadrant);
      }
    }
  }
  ASSERT_TRUE(state.source_context);
  ASSERT_TRUE(state.SourceTarget());
  ASSERT_TRUE(state.source_context->render_preview);
  auto& palettes = gfx::PaletteManager::Get();
  ASSERT_TRUE(palettes.IsManaging(state.game_data));
  ASSERT_TRUE(core::FeatureFlags::get().kSaveAllPalettes ||
              core::FeatureFlags::get().dungeon.kSavePalettes);

  // Establish a product-saved control: existing serializers can normalize data
  // on the first save. The color edit must add only its word to this baseline.
  const auto control_path = fixture.root / "control.sfc";
  ASSERT_OK(manager_->SaveRomAs(control_path.string()));
  const auto control = ReadFile(control_path);
  const auto before = state.source_context->render_preview();
  ASSERT_OK(before.status());
  ASSERT_FALSE(before->empty());
  ASSERT_EQ(before->format, usage_preview::PreviewImage::Format::kIndexed);
  const std::string group = dungeon_source ? "dungeon_main" : "ow_main";
  const int colors_per_row = dungeon_source ? 15 : 7;
  const int last_row = dungeon_source ? 7 : 6;
  // Choose a color actually drawn in the unmodified room/map, rather than
  // inserting a synthetic Tile16 or changing the source object's tilewords.
  const auto used = std::find_if(
      before->pixels.begin(), before->pixels.end(), [&](uint8_t pixel) {
        return pixel / 16 >= 2 && pixel / 16 <= last_row && pixel % 16 >= 1 &&
               pixel % 16 <= colors_per_row;
      });
  ASSERT_NE(used, before->pixels.end());
  const int display_index = *used;
  const int color_index =
      (display_index / 16 - 2) * colors_per_row + display_index % 16 - 1;
  const auto original = palettes.GetColor(group, palette_id, color_index);
  const gfx::SnesColor changed(original.snes() ^ 0x7FFF);
  const auto address = gfx::GetPaletteAddress(group, palette_id, color_index);
  ASSERT_LT(address + 1, control.size());
  ASSERT_EQ(control[address] | (control[address + 1] << 8), original.snes());
  const auto sheet =
      state.game_data->sheet_store.Sheet(state.SourceTarget()->sheet_id);
  const std::vector<uint8_t> original_pixels(sheet.begin(), sheet.end());
  const auto before_rgba = before->ToRgba();
  auto check_preview = [&](const gfx::SnesColor& color, bool edited) {
    ASSERT_OK(graphics->Update());
    ASSERT_TRUE(state.source_context->palette_error.empty());
    const auto image = state.source_context->render_preview();
    ASSERT_OK(image.status());
    ASSERT_LT(display_index, image->colors.size());
    EXPECT_EQ(image->pixels, before->pixels);
    const auto rgb = color.rgb();
    EXPECT_EQ(image->colors[display_index].r, static_cast<uint8_t>(rgb.x));
    EXPECT_EQ(image->colors[display_index].g, static_cast<uint8_t>(rgb.y));
    EXPECT_EQ(image->colors[display_index].b, static_cast<uint8_t>(rgb.z));
    EXPECT_EQ(image->ToRgba() == before_rgba, !edited);
    EXPECT_EQ(
        state.game_data->sheet_store.Sheet(state.SourceTarget()->sheet_id),
        absl::MakeConstSpan(original_pixels));
    EXPECT_FALSE(graphics->HasPendingGraphicsChanges());
  };
  check_preview(original, false);
  const auto undo_count = palettes.GetUndoStackSize();
  ASSERT_OK(palettes.BeginColorEdit(group, palette_id, color_index));
  ASSERT_OK(palettes.SetColor(group, palette_id, color_index,
                              gfx::SnesColor(original.snes() ^ 0x001F)));
  ASSERT_OK(palettes.SetColor(group, palette_id, color_index, changed));
  check_preview(changed, true);
  palettes.CommitColorEdit();
  EXPECT_EQ(palettes.GetUndoStackSize(), undo_count + 1);
  palettes.Undo();
  check_preview(original, false);
  palettes.Redo();
  check_preview(changed, true);
  ASSERT_TRUE(palettes.HasUnsavedChanges());
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), control);

  const auto destination = fixture.root / "palette-edited.sfc";
  const auto filename = manager_->GetCurrentRom()->filename();
  {
    test::ScopedRomStagingFailure fault;
    const auto failed = manager_->SaveRomAs(destination.string());
    EXPECT_FALSE(failed.ok());
    EXPECT_NE(failed.message().find("staging failure injected for testing"),
              absl::string_view::npos)
        << failed;
  }
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), filename);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), control);
  EXPECT_EQ(ReadFile(control_path), control);
  EXPECT_TRUE(palettes.HasUnsavedChanges());
  EXPECT_EQ(palettes.GetUndoStackSize(), undo_count + 1);
  check_preview(changed, true);
  palettes.Undo();
  check_preview(original, false);
  palettes.Redo();
  check_preview(changed, true);
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  const auto saved = ReadFile(destination);
  ASSERT_EQ(saved.size(), control.size());
  auto expected = control;
  expected[address] = changed.snes() & 0xFF;
  expected[address + 1] = changed.snes() >> 8;
  // The normal file save may update the SNES checksum/complement.
  std::copy_n(saved.begin() + 0x7FDC, 4, expected.begin() + 0x7FDC);
  EXPECT_EQ(saved, expected);
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), saved,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());

  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  ASSERT_OK(reopened->EnsureEditorAssetsLoaded(editor_type));
  EXPECT_EQ(palettes.GetColor(group, palette_id, color_index).snes(),
            changed.snes());
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  if (dungeon_source) {
    auto* dungeon =
        reopened->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
            editor_type);
    AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kRoom);
    ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
    auto& room = dungeon->rooms()[kRoom];
    room.PrepareForRender();
    EXPECT_EQ(room.ResolveDungeonPaletteId(), palette_id);
    EXPECT_EQ(room.rendered_dungeon_palette()[color_index].snes(),
              changed.snes());
  } else {
    auto* ow = reopened->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
        editor_type);
    ow->SelectMapForEditing(kMap, false);
    ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
    EXPECT_EQ(ow->map_bitmap(kMap).palette()[display_index].snes(),
              changed.snes());
  }
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  RecordProperty("palette_word_pc", address);
  RecordProperty("palette_group", group);
  RecordProperty("palette_id", palette_id);
  RecordProperty("color_index", color_index);
  RecordProperty("original_word", original.snes());
  RecordProperty("saved_word", changed.snes());
  RecordProperty("saved_rom_sha1",
                 util::ComputeFileSha1Hex(destination.string()));
}

TEST_F(AuthoringSaveWorkflowTest,
       OverworldPaletteColorsRefreshCanvasAndOpenGraphicsWithoutSave) {
  const auto fixture = CopyProject("yaze-overworld-live-colors");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* ow = manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  ow->SelectMapForEditing(0, false);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
  auto& world = ow->overworld();
  ASSERT_OK(world.EnsureMapBuilt(0x40));
  // A declared disposable Tile16 variant addresses main-palette color 0:
  // slot 1 uses the left half, row 2, raw pixel index 1 -> display index 33.
  const gfx::TileInfo info(64, 2, false, false, false);
  (*world.mutable_tiles16())[0] = gfx::Tile16(info, info, info, info);
  ASSERT_OK(ow->OpenGraphicsForTile16Quadrant(0, 0));
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  ASSERT_TRUE(state.SourceTarget());
  const auto target = *state.SourceTarget();
  const auto* map = world.overworld_map(0);
  const auto* other_world = world.overworld_map(0x40);
  const int palette_id = map->render_main_palette();
  const auto unrelated_before = other_world->current_palette()[33];
  auto& palettes = gfx::PaletteManager::Get();
  ASSERT_TRUE(palettes.IsManaging(state.game_data));
  const auto original = palettes.GetColor("ow_main", palette_id, 0);
  ASSERT_EQ(target.palette[1].snes(), original.snes());
  const gfx::SnesColor changed(original.snes() ^ 0x7FFF);
  const auto bytes = manager_->GetCurrentRom()->vector();
  const auto indices = ow->map_bitmap(0).vector();
  const int offset =
      (target.tile_index / 16) * 8 * 128 + (target.tile_index % 16) * 8;
  const int before_pixel =
      state.game_data->sheet_store.Sheet(target.sheet_id)[offset];
  AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(
      *graphics, offset % 128, offset / 128, (before_pixel + 1) % 8);
  const auto sheet_revision =
      state.game_data->sheet_store.Revision(target.sheet_id);
  auto check = [&](const gfx::SnesColor& color) {
    EXPECT_OK(graphics->Update());
    EXPECT_TRUE(state.source_context->palette_error.empty())
        << state.source_context->palette_error;
    EXPECT_EQ(state.SourceTarget()->palette[1].snes(), color.snes());
    EXPECT_EQ(map->current_palette()[33].snes(), color.snes());
    EXPECT_EQ(ow->map_bitmap(0).palette()[33].snes(), color.snes());
    const auto preview = state.source_context->render_preview();
    ASSERT_OK(preview.status());
    const auto rgb = color.rgb();
    EXPECT_EQ(preview->colors[33].r, static_cast<uint8_t>(rgb.x));
    EXPECT_EQ(preview->colors[33].g, static_cast<uint8_t>(rgb.y));
    EXPECT_EQ(preview->colors[33].b, static_cast<uint8_t>(rgb.z));
  };
  ASSERT_OK(palettes.SetColor("ow_main", palette_id, 0, changed));
  check(changed);
  palettes.Undo();
  check(original);
  palettes.Redo();
  check(changed);
  palettes.DiscardAllChanges();
  check(original);
  EXPECT_EQ(ow->map_bitmap(0).vector(), indices);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), bytes);
  EXPECT_EQ(state.game_data->sheet_store.Revision(target.sheet_id),
            sheet_revision);
  EXPECT_TRUE(graphics->HasPendingGraphicsChanges());
  if (other_world->render_main_palette() != palette_id)
    EXPECT_EQ(other_world->current_palette()[33].snes(),
              unrelated_before.snes());

  // Activating another actual editor session must not recolor this canvas.
  auto other = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*other, &renderer_);
  other->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(other->OpenRomOrProject(fixture.project.string()));
  ASSERT_OK(other->EnsureEditorAssetsLoaded(EditorType::kPalette));
  ASSERT_OK(palettes.SetColor("ow_main", palette_id, 0, changed));
  EXPECT_EQ(map->current_palette()[33].snes(), original.snes());
  EXPECT_EQ(ow->map_bitmap(0).palette()[33].snes(), original.snes());
  gfx::Arena::Get().ClearTextureQueue();
  other.reset();
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
}

TEST_F(AuthoringSaveWorkflowTest, OverworldSourcePixelsUndoReturnSaveAsReopen) {
  const auto fixture = CopyProject("yaze-overworld-source-pixels");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* overworld =
      manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
          EditorType::kOverworld);
  overworld->SetCurrentMap(0);
  auto& world = overworld->overworld();
  absl::Status opened = absl::NotFoundError("No editable tile on map");
  OverworldContextTarget accepted_target;
  for (int y = 0; y < 32 && !opened.ok(); ++y) {
    for (int x = 0; x < 32 && !opened.ok(); ++x) {
      const auto tile = world.GetMapTiles(0)[x][y];
      for (int quadrant = 0; quadrant < 4 && !opened.ok(); ++quadrant) {
        OverworldContextTarget clicked;
        clicked.map_id = 0;
        clicked.parent_map_id = world.overworld_map(0)->parent();
        clicked.world = 0;
        clicked.game_state = 0;
        clicked.world_position =
            ImVec2(x * 16 + (quadrant % 2) * 8, y * 16 + (quadrant / 2) * 8);
        clicked.tile16_id = tile;
        // Navigation since menu opening must not choose another map's colors.
        overworld->SetCurrentMap(0x30);
        opened = overworld->OpenGraphicsForContextTile16(clicked);
        if (opened.ok()) {
          accepted_target = clicked;
        }
      }
    }
  }
  ASSERT_OK(opened);
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  ASSERT_TRUE(state.source_context);
  EXPECT_EQ(state.source_context->resolved.id, 0);
  EXPECT_EQ(state.source_context->resolved.slots.size(), 16u);
  EXPECT_EQ(overworld->current_map_id(), 0);
  ASSERT_FALSE(state.source_context->targets.empty());
  EXPECT_NE(state.SourceTarget()->source_orientation_mask, 0);
  state.selection.is_floating = true;
  overworld->SetCurrentMap(0x30);
  const auto refused = overworld->OpenGraphicsForContextTile16(accepted_target);
  EXPECT_EQ(refused.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(refused.message().find("floating paste"), absl::string_view::npos);
  EXPECT_EQ(overworld->current_map_id(), 0x30);
  EXPECT_TRUE(state.selection.is_floating);
  state.selection.is_floating = false;
  ASSERT_OK(overworld->OpenGraphicsForContextTile16(accepted_target));
  QualifySourcePixelRoundTrip(fixture);
}

TEST_F(AuthoringSaveWorkflowTest, SpriteSourcePixelsUndoReturnSaveAsReopen) {
  const auto fixture = CopyProject("yaze-sprite-source-pixels");
  zsprite::ZSprite source;
  source.editor.Frames.resize(1);
  source.editor.Frames[0].Tiles.emplace_back(128, 112, false, false, 0x100, 0,
                                             false, 3);
  const auto asset = fixture.root / "context-sprite.zsm";
  ASSERT_OK(source.Save(asset.string()));
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kSprite));
  auto* sprite = manager_->GetCurrentEditorSet()->GetEditorAs<SpriteEditor>(
      EditorType::kSprite);
  ASSERT_OK(sprite->OpenSpriteAsset(asset.string()));
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::OpenSpriteGraphics(*sprite));
  QualifySourcePixelRoundTrip(fixture);
}

TEST_F(AuthoringSaveWorkflowTest,
       DungeonSheetAssignmentUndoAfterNavigationSaveAsReopen) {
  QualifySharedGraphicsAssignment(/*custom_sheets=*/true);
}

TEST_F(AuthoringSaveWorkflowTest,
       SharedDungeonOverworldAssignmentUndoRedoSaveAsReopen) {
  QualifySharedGraphicsAssignment(/*custom_sheets=*/false);
}

TEST_F(AuthoringSaveWorkflowTest, RoomsetGraphicsHostSaveAsReopen) {
  QualifySharedGraphicsAssignment(true, EditorType::kGraphics);
}

TEST_F(AuthoringSaveWorkflowTest, RoomsetOverworldHostSaveAsReopen) {
  QualifySharedGraphicsAssignment(false, EditorType::kOverworld);
}

void AuthoringSaveWorkflowTest::QualifySharedGraphicsAssignment(
    bool custom_sheets, EditorType host_type) {
  auto fixture = CopyProject("yaze-room-sheet-assignment");
  if (!custom_sheets) {
    // Declared legacy-assignment fixture: Oracle normally overrides every
    // background slot. Disable that feature only in this disposable copy.
    std::fstream rom(fixture.rom,
                     std::ios::binary | std::ios::in | std::ios::out);
    ASSERT_TRUE(rom.good());
    rom.seekp(zelda3::OverworldCustomTileGFXGroupEnabled);
    rom.put(0);
    rom.close();
    fixture.source_hash = util::ComputeFileSha1Hex(fixture.rom.string());
  }
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().kSaveGfxGroups);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  constexpr int kRoom = 0x89;
  constexpr int kSlot = 3;
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, 5));
  auto& room = dungeon->rooms()[kRoom];
  AuthoringSaveWorkflowTestPeer::ActivateDungeonRoom(*dungeon, kRoom);
  room.PrepareForRender();
  auto* data = room.game_data();
  const int group = room.blockset();
  ASSERT_TRUE(data->gfx_groups_loaded);
  // Reading usage must not initialize the other editor merely to show counts.
  const bool overworld_was_created =
      manager_->GetCurrentEditorSet()->GetExistingEditor(
          EditorType::kOverworld) != nullptr;
  auto room_usage = dungeon->GetRoomGraphicsUsage(group);
  ASSERT_OK(room_usage.status());
  const auto measure_usage = [&](const char* property) {
    const auto start = std::chrono::steady_clock::now();
    for (int scan = 0; scan < 20; ++scan) {
      EXPECT_OK(dungeon->GetRoomGraphicsUsage(group).status());
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);
    RecordProperty(property, static_cast<int>(elapsed.count() / 20));
  };
  measure_usage("room_usage_rom_mean_us");
  const auto zero_group_before = dungeon->GetRoomGraphicsUsage(0);
  ASSERT_OK(zero_group_before.status());
  ASSERT_FALSE(dungeon->rooms()[0x100].IsLoaded());
  const auto zero_group_after = dungeon->GetRoomGraphicsUsage(0);
  ASSERT_OK(zero_group_after.status());
  EXPECT_EQ(zero_group_before->rooms, zero_group_after->rooms);
  EXPECT_FALSE(room_usage->uses_live_overworld);
  EXPECT_EQ(manager_->GetCurrentEditorSet()->GetExistingEditor(
                EditorType::kOverworld) != nullptr,
            overworld_was_created);
  EXPECT_NE(
      std::find(room_usage->rooms.begin(), room_usage->rooms.end(), kRoom),
      room_usage->rooms.end());
  const int other_group = (group + 1) % data->room_blockset_ids.size();
  ASSERT_OK(dungeon->EditRoomMetadata(
      kRoom, {RoomMetadataField::kBlockset, other_group}));
  room_usage = dungeon->GetRoomGraphicsUsage(group);
  ASSERT_OK(room_usage.status());
  EXPECT_EQ(
      std::find(room_usage->rooms.begin(), room_usage->rooms.end(), kRoom),
      room_usage->rooms.end());
  ASSERT_OK(dungeon->Undo());
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* overworld =
      manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
          EditorType::kOverworld);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::EditAreaGraphics(*overworld, 0,
                                                            other_group));
  auto other_usage = dungeon->GetRoomGraphicsUsage(group);
  ASSERT_OK(other_usage.status());
  EXPECT_TRUE(other_usage->uses_live_overworld);
  EXPECT_EQ(
      std::find(other_usage->ow_areas.begin(), other_usage->ow_areas.end(), 0),
      other_usage->ow_areas.end());
  EXPECT_EQ(std::find(other_usage->overridden_ow_areas.begin(),
                      other_usage->overridden_ow_areas.end(), 0),
            other_usage->overridden_ow_areas.end());
  // Explicitly share the room's group through the normal property edit path.
  ASSERT_OK(
      AuthoringSaveWorkflowTestPeer::EditAreaGraphics(*overworld, 0, group));
  overworld->SelectMapForEditing(0, false);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*overworld));
  const auto shared_usage = dungeon->GetRoomGraphicsUsage(group);
  ASSERT_OK(shared_usage.status());
  measure_usage("room_usage_live_mean_us");
  EXPECT_TRUE(shared_usage->uses_live_overworld);
  EXPECT_EQ(shared_usage->unscanned_rooms, 0);
  EXPECT_EQ(shared_usage->unscanned_ow_areas, 0);
  const auto& map_users = custom_sheets ? shared_usage->overridden_ow_areas
                                        : shared_usage->ow_areas;
  EXPECT_NE(std::find(map_users.begin(), map_users.end(), 0), map_users.end());
  if (custom_sheets)
    EXPECT_TRUE(shared_usage->ow_areas.empty());
  auto capture_overworld = [&]() {
    const auto* map = overworld->overworld().overworld_map(0);
    EXPECT_EQ(map->game_state(), overworld->game_state());
    std::vector<uint16_t> palette;
    for (const auto& color : map->current_palette())
      palette.push_back(color.snes());
    return std::make_tuple(
        map->current_graphics(), overworld->map_bitmap(0).vector(),
        overworld->tile16_blockset().atlas.vector(), palette);
  };
  auto expect_overworld = [&](const auto& expected, const char* phase) {
    SCOPED_TRACE(phase);
    const auto actual = capture_overworld();
    const auto compare = [](const auto& a, const auto& b, const char* surface) {
      SCOPED_TRACE(surface);
      ASSERT_EQ(a.size(), b.size());
      const auto mismatch = std::mismatch(a.begin(), a.end(), b.begin());
      EXPECT_TRUE(mismatch.first == a.end())
          << "first differing element "
          << std::distance(a.begin(), mismatch.first) << ", actual "
          << static_cast<int>(*mismatch.first) << ", expected "
          << static_cast<int>(*mismatch.second);
    };
    compare(std::get<0>(actual), std::get<0>(expected), "resolved graphics");
    compare(std::get<1>(actual), std::get<1>(expected), "map bitmap");
    compare(std::get<2>(actual), std::get<2>(expected), "Tile16 atlas");
    compare(std::get<3>(actual), std::get<3>(expected), "palette");
  };
  const auto original_overworld = capture_overworld();
  ASSERT_FALSE(std::get<1>(original_overworld).empty());
  ASSERT_FALSE(std::get<2>(original_overworld).empty());
  const auto original_groups = data->room_blockset_ids;
  const auto original_main = data->main_blockset_ids;
  const auto original_sprites = data->spriteset_ids;
  const auto original_pixels = room.get_gfx_buffer();
  const auto original_sheet = room.blocks()[kSlot];
  int replacement = 1;
  while (replacement < 113 &&
         std::equal(data->sheet_store.Sheet(replacement).begin(),
                    data->sheet_store.Sheet(replacement).end(),
                    data->sheet_store.Sheet(original_sheet).begin()))
    ++replacement;
  ASSERT_LT(replacement, 113);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(host_type));
  Editor* host = manager_->GetCurrentEditorSet()->GetEditor(host_type);
  GfxGroupEditor* group_panel = nullptr;
  if (host_type == EditorType::kGraphics) {
    group_panel = &AuthoringSaveWorkflowTestPeer::MainGroups(
        *static_cast<GraphicsEditor*>(host));
  } else if (host_type == EditorType::kOverworld) {
    group_panel = &AuthoringSaveWorkflowTestPeer::MainGroups(*overworld);
  }
  if (group_panel) {
    group_panel->SetSelectedRoomset(group);
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(1100, 1400), ImGuiCond_Always);
    ImGui::Begin("Roomset shared usage acceptance");
    ImGui::LogToBuffer(2);
    group_panel->DrawRoomsetViewer();
    const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    EXPECT_NE(text.find(absl::StrFormat(
                  "Shared by %zu rooms and %zu map screens",
                  shared_usage->rooms.size(), shared_usage->ow_areas.size())),
              std::string::npos);
  }
  const auto edit = [&](int slot, int sheet) {
    return group_panel
               ? group_panel->EditRoomsetGraphicsSheet(group, slot - 3, sheet)
               : dungeon->EditRoomGraphicsSheet(kRoom, slot, sheet);
  };
  ASSERT_OK(edit(kSlot, replacement));
  room.PrepareForRender();
  EXPECT_EQ(room.blocks()[kSlot], replacement);
  EXPECT_NE(room.get_gfx_buffer(), original_pixels);
  const auto edited_pixels = room.get_gfx_buffer();
  const auto edited_groups = data->room_blockset_ids;
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*overworld));
  const auto edited_overworld = capture_overworld();
  EXPECT_EQ(overworld->current_map_id(), 0);
  if (custom_sheets) {
    EXPECT_EQ(edited_overworld, original_overworld);
  } else {
    EXPECT_NE(std::get<0>(edited_overworld), std::get<0>(original_overworld));
    EXPECT_NE(std::get<1>(edited_overworld), std::get<1>(original_overworld));
    EXPECT_NE(std::get<2>(edited_overworld), std::get<2>(original_overworld));
    EXPECT_EQ(overworld->overworld().overworld_map(0)->static_graphics(kSlot),
              replacement);
  }
  EXPECT_EQ(std::get<3>(edited_overworld), std::get<3>(original_overworld));
  const auto context = zelda3::DescribeOverworldGraphicsContext(
      *overworld->overworld().overworld_map(0));
  EXPECT_TRUE(context.loader_agrees);
  EXPECT_EQ(context.slots[kSlot].sheet_id,
            overworld->overworld().overworld_map(0)->static_graphics(kSlot));
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  if (group_panel)
    group_panel->SetSelectedRoomset(other_group);
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, 5);
  ASSERT_OK(host->Undo());
  room.PrepareForRender();
  EXPECT_EQ(dungeon->current_room_id(), 5);
  EXPECT_EQ(data->room_blockset_ids, original_groups);
  EXPECT_EQ(room.get_gfx_buffer(), original_pixels);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*overworld));
  expect_overworld(original_overworld, "Undo");
  ASSERT_OK(host->Redo());
  room.PrepareForRender();
  EXPECT_EQ(room.get_gfx_buffer(), edited_pixels);
  EXPECT_EQ(data->room_blockset_ids, edited_groups);
  // Selection is also a refresh boundary: do not require a canvas draw first.
  overworld->SelectMapForEditing(0, false);
  expect_overworld(edited_overworld, "Redo and select");
  // No-op and invalid edits do not add undo steps or alter neighboring groups.
  ASSERT_OK(edit(kSlot, replacement));
  EXPECT_FALSE(edit(7, replacement).ok());
  EXPECT_FALSE(edit(kSlot, 113).ok());
  EXPECT_EQ(data->main_blockset_ids, original_main);
  EXPECT_EQ(data->spriteset_ids, original_sprites);
  // Zero is an inheritance instruction, not a request for physical sheet zero.
  ASSERT_OK(edit(kSlot, 0));
  room.PrepareForRender();
  EXPECT_EQ(room.blocks()[kSlot],
            original_main[room.resolved_main_blockset()][kSlot]);
  ASSERT_OK(host->Undo());
  // A newer assignment outside this undo stack must survive a failed Undo.
  const uint8_t external = replacement == 2 ? 3 : 2;
  data->room_blockset_ids[group][0] = external;
  EXPECT_TRUE(absl::IsFailedPrecondition(host->Undo()));
  EXPECT_EQ(data->room_blockset_ids[group][0], external);
  data->room_blockset_ids[group][0] = replacement;
  ASSERT_OK(host->Undo());
  EXPECT_EQ(data->room_blockset_ids, original_groups);
  if (host_type != EditorType::kOverworld)
    EXPECT_FALSE(
        host->CanUndo());  // Overworld retains its earlier selector edits.
  ASSERT_OK(host->Redo());
  room.PrepareForRender();
  EXPECT_EQ(room.get_gfx_buffer(), edited_pixels);
  const auto destination = fixture.root / "room-sheets.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  ASSERT_OK(manager_->SwapProjectRom(destination.string()));
  manager_.reset();
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  auto& reopened = dungeon->rooms()[kRoom];
  reopened.PrepareForRender();
  EXPECT_EQ(reopened.game_data()->room_blockset_ids, edited_groups);
  EXPECT_EQ(reopened.blocks()[kSlot], replacement);
  EXPECT_EQ(reopened.get_gfx_buffer(), edited_pixels);
  EXPECT_EQ(reopened.game_data()->main_blockset_ids, original_main);
  EXPECT_EQ(reopened.game_data()->spriteset_ids, original_sprites);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  overworld = manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
  overworld->SelectMapForEditing(0, false);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*overworld));
  expect_overworld(edited_overworld, "Fresh reopen");
  const auto reopened_usage = dungeon->GetRoomGraphicsUsage(group);
  ASSERT_OK(reopened_usage.status());
  EXPECT_EQ(reopened_usage->rooms, shared_usage->rooms);
  EXPECT_EQ(reopened_usage->ow_areas, shared_usage->ow_areas);
  EXPECT_EQ(reopened_usage->overridden_ow_areas,
            shared_usage->overridden_ow_areas);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest, MainAssignmentGraphicsHostSaveAsReopen) {
  QualifyMainGraphicsAssignment(/*custom_sheets=*/true);
}

TEST_F(AuthoringSaveWorkflowTest, MainAssignmentOverworldHostSaveAsReopen) {
  QualifyMainGraphicsAssignment(/*custom_sheets=*/false);
}

void AuthoringSaveWorkflowTest::QualifyMainGraphicsAssignment(
    bool custom_sheets) {
  auto fixture = CopyProject("yaze-main-sheet-assignment");
  if (!custom_sheets) {
    // Deliberate disposable legacy-table variant. Actual Oracle uses custom8.
    std::fstream rom(fixture.rom,
                     std::ios::binary | std::ios::in | std::ios::out);
    rom.seekp(zelda3::OverworldCustomTileGFXGroupEnabled);
    rom.put(0);
    rom.close();
    fixture.source_hash = util::ComputeFileSha1Hex(fixture.rom.string());
  }
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin")
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
  }
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().kSaveGfxGroups);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* set = manager_->GetCurrentEditorSet();
  auto* ow = set->GetEditorAs<OverworldEditor>(EditorType::kOverworld);
  auto* dungeon = set->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  auto* graphics = set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ow->SelectMapForEditing(0, false);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
  const int group = ow->overworld().overworld_map(0)->main_gfx_id();
  constexpr int kRoom = 0x89;
  constexpr int kOtherRoom = 5;
  constexpr int kUnaffected = 6;
  for (const int id : {kRoom, kOtherRoom, kUnaffected})
    ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, id));
  auto& room = dungeon->rooms()[kRoom];
  auto& other = dungeon->rooms()[kOtherRoom];
  auto& unaffected = dungeon->rooms()[kUnaffected];
  // Explicit preview entrance groups exercise the same session main group in
  // dungeon and OW. These are preview settings, not authored entrance edits.
  room.PrepareForRender(group);
  other.PrepareForRender(group);
  unaffected.PrepareForRender((group + 1) % 37);
  const auto unaffected_pixels = unaffected.get_gfx_buffer();
  const auto original_pixels = room.get_gfx_buffer();
  const auto other_pixels = other.get_gfx_buffer();
  auto* data = room.game_data();
  const auto original_main = data->main_blockset_ids;
  const auto original_roomsets = data->room_blockset_ids;
  const auto original_spritesets = data->spriteset_ids;
  auto capture_ow = [&] {
    const auto* map = ow->overworld().overworld_map(0);
    std::vector<uint16_t> palette;
    for (const auto& color : map->current_palette())
      palette.push_back(color.snes());
    return std::make_tuple(map->current_graphics(), ow->map_bitmap(0).vector(),
                           ow->tile16_blockset().atlas.vector(), palette);
  };
  const auto before_ow = capture_ow();
  const int original = original_main[group][0];
  int replacement = 0;
  while (replacement < 113 &&
         std::equal(data->sheet_store.Sheet(replacement).begin(),
                    data->sheet_store.Sheet(replacement).end(),
                    data->sheet_store.Sheet(original).begin()))
    ++replacement;
  ASSERT_LT(replacement, 113);
  auto& panel = custom_sheets
                    ? AuthoringSaveWorkflowTestPeer::MainGroups(*graphics)
                    : AuthoringSaveWorkflowTestPeer::MainGroups(*ow);
  Editor* host =
      custom_sheets ? static_cast<Editor*>(graphics) : static_cast<Editor*>(ow);
  panel.SetSelectedBlockset(group);
  const auto usage_text = [&] {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(900, 1400), ImGuiCond_Always);
    ImGui::Begin("Main group usage acceptance");
    ImGui::LogToBuffer(2);
    panel.DrawBlocksetViewer();
    const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    return text;
  };
  // The production EditorSet callback reads unsaved explicit preview choices.
  EXPECT_NE(usage_text().find("Explicit room previews (2)"), std::string::npos);
  other.SetRenderEntranceBlockset((group + 1) % 37);
  EXPECT_NE(usage_text().find("Explicit room previews (1)"), std::string::npos);
  other.SetRenderEntranceBlockset(group);
  ASSERT_OK(panel.EditMainGraphicsSheet(group, 0, replacement));
  room.PrepareForRender();
  other.PrepareForRender();
  unaffected.PrepareForRender();
  EXPECT_NE(room.get_gfx_buffer(), original_pixels);
  EXPECT_NE(other.get_gfx_buffer(), other_pixels);
  EXPECT_EQ(unaffected.get_gfx_buffer(), unaffected_pixels);
  const auto edited_pixels = room.get_gfx_buffer();
  const auto edited_groups = data->main_blockset_ids;
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
  const auto edited_ow = capture_ow();
  if (custom_sheets) {
    EXPECT_EQ(edited_ow, before_ow);
  } else {
    EXPECT_NE(std::get<0>(edited_ow), std::get<0>(before_ow));
    EXPECT_NE(std::get<1>(edited_ow), std::get<1>(before_ow));
    EXPECT_NE(std::get<2>(edited_ow), std::get<2>(before_ow));
  }
  EXPECT_EQ(std::get<3>(edited_ow), std::get<3>(before_ow));
  panel.SetSelectedBlockset((group + 1) % 37);
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kUnaffected);
  ASSERT_OK(host->Undo());
  room.PrepareForRender();
  other.PrepareForRender();
  EXPECT_EQ(room.get_gfx_buffer(), original_pixels);
  EXPECT_EQ(other.get_gfx_buffer(), other_pixels);
  EXPECT_EQ(dungeon->current_room_id(), kUnaffected);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
  EXPECT_EQ(capture_ow(), before_ow);
  ASSERT_OK(host->Redo());
  room.PrepareForRender();
  EXPECT_EQ(room.get_gfx_buffer(), edited_pixels);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
  EXPECT_EQ(capture_ow(), edited_ow);
  EXPECT_EQ(data->room_blockset_ids, original_roomsets);
  EXPECT_EQ(data->spriteset_ids, original_spritesets);
  const auto destination = fixture.root / "main-sheets.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  ASSERT_OK(manager_->SwapProjectRom(destination.string()));
  // ROM replacement reuses session object addresses. Old undo must not apply
  // even when the newly loaded table has exactly the expected edited byte.
  ASSERT_TRUE(absl::IsFailedPrecondition(host->Undo()));
  EXPECT_EQ(data->main_blockset_ids, edited_groups);
  manager_.reset();
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  set = manager_->GetCurrentEditorSet();
  dungeon = set->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  auto& reopened = dungeon->rooms()[kRoom];
  reopened.PrepareForRender(group);
  EXPECT_EQ(reopened.game_data()->main_blockset_ids, edited_groups);
  EXPECT_EQ(reopened.get_gfx_buffer(), edited_pixels);
  EXPECT_EQ(reopened.game_data()->room_blockset_ids, original_roomsets);
  EXPECT_EQ(reopened.game_data()->spriteset_ids, original_spritesets);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  ow = set->GetEditorAs<OverworldEditor>(EditorType::kOverworld);
  ow->SelectMapForEditing(0, false);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::DrawOverworld(*ow));
  EXPECT_EQ(capture_ow(), edited_ow);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       GraphicsBrowserToggleRestoresDockAndPreservesPendingPixels) {
  const auto fixture = CopyProject("yaze-graphics-browser");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  ASSERT_NE(graphics, nullptr);
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  auto& windows = manager_->window_manager();
  constexpr const char* kBrowser = "graphics.sheet_browser_v2";
  ASSERT_TRUE(windows.OpenWindow(kBrowser));
  windows.SetWindowPinned(kBrowser, true);
  state.SetZoom(5);
  const auto original = state.game_data->sheet_store.Sheet(0);
  AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(*graphics, 0, 0,
                                                    (original[0] + 1) % 8);
  const auto authored = state.game_data->sheet_store.Sheet(0);
  const auto revision = state.game_data->sheet_store.Revision(0);
  const auto rom = manager_->GetCurrentRom()->vector();
  state.selection.is_active = true;
  state.selection.x = state.selection.y = 8;
  state.selection.width = state.selection.height = 8;

  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigInputTrickleEventQueue = false;
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  bool create_dock = true;
  ImGuiID browser_dock = 0;
  const auto frame = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_Always);
    ImGui::Begin(
        "GraphicsDockHost", nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
    const auto dock = ImGui::GetID("GraphicsDock");
    if (create_dock) {
      ImGui::DockBuilderRemoveNode(dock);
      ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dock, ImGui::GetContentRegionAvail());
      ImGuiID center = dock;
      browser_dock = ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.27f,
                                                 nullptr, &center);
      ImGui::DockBuilderDockWindow("Browser", browser_dock);
      ImGui::DockBuilderDockWindow("Pixels", center);
      ImGui::DockBuilderFinish(dock);
      create_dock = false;
    }
    ImGui::DockSpace(dock);
    ImGui::End();
    if (windows.IsWindowOpen(kBrowser)) {
      ImGui::Begin("Browser");
      ImGui::TextUnformatted("Existing sheet browser dock");
      ImGui::End();
    }
    ImGui::Begin("Pixels");
    AuthoringSaveWorkflowTestPeer::DrawGraphics(*graphics);
    ImGui::End();
    ImGui::Render();
  };
  const auto settle = [&] {
    for (int i = 0; i < 5; ++i)
      frame();
  };
  const auto toggle = [&] {
    std::optional<gui::WidgetIdRegistry::WidgetInfo> button;
    for (const auto& [path, item] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (item.label ==
          gui::WidgetIdRegistry::NormalizeLabel("Toggle Sheet Browser"))
        button = item;
    }
    ASSERT_TRUE(button);
    io.AddMousePosEvent((button->bounds.min_x + button->bounds.max_x) / 2,
                        (button->bounds.min_y + button->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    settle();
  };
  settle();
  auto* canvas = ImGui::FindWindowByName("Pixels");
  ASSERT_NE(canvas, nullptr);
  const float original_width = canvas->Size.x;
  toggle();
  EXPECT_FALSE(windows.IsWindowOpen(kBrowser));
  EXPECT_GT(canvas->Size.x, original_width + 250);
  EXPECT_TRUE(windows.IsWindowPinned(kBrowser));
  toggle();
  EXPECT_TRUE(windows.IsWindowOpen(kBrowser));
  EXPECT_NEAR(canvas->Size.x, original_width, 1);
  EXPECT_EQ(ImGui::FindWindowByName("Browser")->DockId, browser_dock);
  // A menu/close action and the toolbar use the same visibility owner.
  ASSERT_TRUE(windows.CloseWindow(kBrowser));
  settle();
  toggle();
  EXPECT_TRUE(windows.IsWindowOpen(kBrowser));
  EXPECT_EQ(state.game_data->sheet_store.Sheet(0), authored);
  EXPECT_EQ(state.game_data->sheet_store.Revision(0), revision);
  EXPECT_TRUE(state.HasUnsavedChanges());
  EXPECT_TRUE(state.selection.is_active);
  EXPECT_EQ(state.selection.x, 8);
  EXPECT_FLOAT_EQ(state.zoom_level, 5);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), rom);
  ASSERT_OK(graphics->Undo());
  EXPECT_EQ(state.game_data->sheet_store.Sheet(0), original);
  ASSERT_OK(graphics->Redo());
  EXPECT_EQ(state.game_data->sheet_store.Sheet(0), authored);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       DungeonObjectGraphicsKeepsContextThroughPixelsUndoAndReturn) {
  auto fixture = CopyProject("yaze-graphics-source");
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  ASSERT_TRUE(std::filesystem::is_directory(assets));
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* editors = manager_->GetCurrentEditorSet();
  auto* dungeon = editors->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  auto* graphics = editors->GetGraphicsEditor();
  ASSERT_NE(dungeon, nullptr);
  ASSERT_NE(graphics, nullptr);
  manager_->window_manager().CloseWindow("graphics.prototype_viewer");
  manager_->window_manager().CloseWindow("graphics.polyhedral");
  manager_->window_manager().CloseWindow("graphics.usage_preview");
  constexpr int kRoom = 5;
  auto& room = dungeon->rooms()[kRoom];
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  AuthoringSaveWorkflowTestPeer::ActivateDungeonRoom(*dungeon, kRoom);
  AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  room.PrepareForRender();
  ASSERT_FALSE(room.GetTileObjects().empty());
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  size_t chosen = 0;
  for (; chosen < room.GetTileObjects().size(); ++chosen) {
    AuthoringSaveWorkflowTestPeer::OpenObjectGraphics(
        *dungeon, kRoom, room.GetTileObjects()[chosen]);
    if (state.source_context)
      break;
  }
  ASSERT_LT(chosen, room.GetTileObjects().size());
  ASSERT_TRUE(state.SourceTarget());
  EXPECT_TRUE(manager_->window_manager().IsWindowOpen("graphics.pixel_editor"));
  EXPECT_FALSE(
      manager_->window_manager().IsWindowOpen("graphics.usage_preview"));
  // A deliberate comparison window remains open on subsequent source entry.
  manager_->window_manager().OpenWindow("graphics.usage_preview");
  ASSERT_OK(graphics->OpenInContext(*state.source_context));
  EXPECT_TRUE(
      manager_->window_manager().IsWindowOpen("graphics.usage_preview"));
  EXPECT_FALSE(
      manager_->window_manager().IsWindowOpen("graphics.prototype_viewer"));
  EXPECT_FALSE(manager_->window_manager().IsWindowOpen("graphics.polyhedral"));
  const auto inspector = graphics->BuildContextSnapshot();
  EXPECT_EQ(inspector.category, "Graphics");
  EXPECT_TRUE(std::any_of(inspector.metadata.begin(), inspector.metadata.end(),
                          [](const auto& value) {
                            return value.id == "gfx.raw_palette" &&
                                   !value.value.empty();
                          }));
  const auto context = *state.source_context;
  EXPECT_EQ(context.resolved.id, kRoom);
  EXPECT_EQ(context.resolved.slots.size(), 16u);
  EXPECT_EQ(context.owner, state.game_data);
  const auto target = *state.SourceTarget();
  ASSERT_FALSE(context.drawing_description.empty());
  ASSERT_NE(target.source_orientation_mask, 0);
  ASSERT_EQ(target.palette.size(), 8u);
  auto& arena_sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(target.sheet_id);
  const auto original_palette = arena_sheet.palette();
  const auto original_pixels = arena_sheet.vector();
  const auto before = context.render_preview();
  ASSERT_OK(before.status());
  ASSERT_FALSE(before->empty());

  unsigned char* font_pixels = nullptr;
  int font_width = 0, font_height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width,
                                           &font_height);
  auto frame = [&]() {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_Always);
    ImGui::Begin("ContextGraphics", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer(2);
    AuthoringSaveWorkflowTestPeer::DrawGraphics(*graphics);
    const std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    EXPECT_NE(text.find("Source preview"), std::string::npos);
    EXPECT_NE(text.find("Return to source"), std::string::npos);
    // Detailed pattern metadata is available through Source details; it no
    // longer consumes permanent canvas height.
    EXPECT_EQ(text.find("Drawing: " + context.drawing_description),
              std::string::npos);
    ImGui::End();
    ImGui::Begin("ContextUsage", nullptr, ImGuiWindowFlags_NoSavedSettings);
    AuthoringSaveWorkflowTestPeer::DrawUsage(*graphics);
    ImGui::End();
    ImGui::Render();
  };
  frame();
  frame();
  ASSERT_EQ(state.current_sheet_id, target.sheet_id);
  const auto& displayed =
      AuthoringSaveWorkflowTestPeer::GraphicsDisplay(*graphics);
  EXPECT_EQ(displayed.vector(), original_pixels);
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_EQ(displayed.palette()[i].snes(), target.palette[i].snes());
  }
  ASSERT_EQ(arena_sheet.palette().size(), original_palette.size());
  for (size_t i = 0; i < original_palette.size(); ++i) {
    EXPECT_EQ(arena_sheet.palette()[i].snes(), original_palette[i].snes());
  }

  // Recolor the room's full dungeon bank in one batch so whichever standard
  // wall tile this fixture selects must refresh from that bank. This changes
  // only temporary in-memory palette words, then discards them before the
  // existing pixel Save As/reopen proof.
  auto& palette_manager = gfx::PaletteManager::Get();
  const int palette_id = room.ResolveDungeonPaletteId();
  const auto original_bank =
      state.game_data->palette_groups.dungeon_main.palette(palette_id);
  palette_manager.BeginBatch();
  for (size_t i = 0; i < original_bank.size(); ++i) {
    ASSERT_OK(palette_manager.SetColor(
        "dungeon_main", palette_id, i,
        gfx::SnesColor(original_bank[i].snes() ^ 0x7FFF)));
  }
  palette_manager.EndBatch();
  ASSERT_OK(graphics->Update());
  ASSERT_TRUE(state.source_context->palette_error.empty())
      << state.source_context->palette_error;
  bool recolored_target = false;
  for (size_t i = 0; i < context.targets.size(); ++i) {
    for (size_t c = 1; c < 8; ++c) {
      recolored_target |= context.targets[i].palette[c].snes() !=
                          state.source_context->targets[i].palette[c].snes();
    }
  }
  EXPECT_TRUE(recolored_target);
  EXPECT_EQ(arena_sheet.vector(), original_pixels);
  palette_manager.DiscardAllChanges();
  ASSERT_OK(graphics->Update());
  for (size_t i = 0; i < context.targets.size(); ++i) {
    for (size_t c = 0; c < 8; ++c) {
      EXPECT_EQ(context.targets[i].palette[c].snes(),
                state.source_context->targets[i].palette[c].snes());
    }
  }

  // A stroke on a used tile, through the production pixel/undo path.
  const int x = (target.tile_index % 16) * 8;
  const int y = (target.tile_index / 16) * 8;
  const uint8_t color = (original_pixels[y * 128 + x] + 1) % 8;
  const auto original_graphics = room.get_gfx_buffer();
  const auto revision = state.game_data->sheet_store.Revision(target.sheet_id);
  AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(*graphics, x, y, color,
                                                    /*finish=*/false);
  auto edited = context.render_preview();
  ASSERT_OK(edited.status());
  EXPECT_EQ(state.game_data->sheet_store.Revision(target.sheet_id), revision);
  EXPECT_NE(room.get_gfx_buffer(), original_graphics);
  AuthoringSaveWorkflowTestPeer::EndGraphicsStroke(*graphics);
  // Some object tiles are covered by later room objects, so compare the loaded
  // source buffer as well as the full preview's successful rendering.
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id)[y * 128 + x],
            color);
  const auto changed_graphics = room.get_gfx_buffer();
  ASSERT_OK(graphics->Undo());
  auto undone = context.render_preview();
  ASSERT_OK(undone.status());
  EXPECT_EQ(undone->pixels, before->pixels);
  EXPECT_EQ(undone->ToRgba(), before->ToRgba());
  EXPECT_EQ(arena_sheet.vector(), original_pixels);
  ASSERT_OK(graphics->Redo());
  auto redone = context.render_preview();
  ASSERT_OK(redone.status());
  EXPECT_EQ(redone->pixels, edited->pixels);
  EXPECT_EQ(room.get_gfx_buffer(), changed_graphics);
  ASSERT_OK(graphics->Undo());

  // A real unsaved source-room edit must appear, rather than a ROM reload.
  auto metadata = room.CaptureMetadataSnapshot();
  const auto old_palette = room.palette();
  room.SetPalette(static_cast<uint8_t>(old_palette ^ 1));
  auto recolored = context.render_preview();
  ASSERT_OK(recolored.status());
  EXPECT_NE(recolored->ToRgba(), before->ToRgba());
  room.RestoreMetadataSnapshot(metadata);
  ASSERT_OK(context.render_preview().status());

  zelda3::GameData foreign;
  auto wrong_session = context;
  wrong_session.owner = &foreign;
  EXPECT_TRUE(
      absl::IsFailedPrecondition(graphics->OpenInContext(wrong_session)));
  EXPECT_EQ(state.source_context->owner, context.owner);
  state.selection.is_floating = true;
  EXPECT_TRUE(absl::IsFailedPrecondition(graphics->OpenInContext(context)));
  state.selection.Clear();
  context.return_to_source();
  EXPECT_EQ(dungeon->current_room_id(), kRoom);
  ASSERT_OK(graphics->Redo());
  const auto destination = fixture.root / "graphics-edited.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_FALSE(graphics->HasPendingGraphicsChanges());
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  ASSERT_OK(reopened->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* reopened_graphics =
      reopened->GetCurrentEditorSet()->GetGraphicsEditor();
  auto& reopened_state =
      AuthoringSaveWorkflowTestPeer::GraphicsState(*reopened_graphics);
  EXPECT_EQ(
      reopened_state.game_data->sheet_store.Sheet(target.sheet_id)[y * 128 + x],
      color);
  EXPECT_FALSE(reopened_state.source_context);
  reopened.reset();
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  graphics->SetGameData(&foreign);
  EXPECT_FALSE(state.source_context);
  graphics->SetGameData(const_cast<zelda3::GameData*>(context.owner));
}

TEST_F(AuthoringSaveWorkflowTest,
       OverworldPaletteUndoRedoSaveAsReopensOnFreshEditor) {
  const auto fixture = CopyProject("yaze_ow_authoring_save");
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldProperties);

  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* overworld =
      manager_->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
          EditorType::kOverworld);
  ASSERT_NE(overworld, nullptr);
  const auto* map = overworld->overworld().overworld_map(kOverworldMap);
  ASSERT_NE(map, nullptr);
  const int parent = map->parent() == 0xFF ? kOverworldMap : map->parent();
  const auto* edited_map = overworld->overworld().overworld_map(parent);
  ASSERT_NE(edited_map, nullptr);
  const int original_palette = edited_map->area_palette();
  const int original_graphics = edited_map->area_graphics();
  const int edited_palette = original_palette == 0x21 ? 0x22 : 0x21;
  const auto profile_before =
      zelda3::DetectOverworldRomProfile(*manager_->GetCurrentRom());
  ASSERT_TRUE(profile_before.has_expanded_tile16);
  ASSERT_TRUE(profile_before.has_expanded_tile32);

  ASSERT_OK(AuthoringSaveWorkflowTestPeer::EditAreaPalette(
      *overworld, kOverworldMap, edited_palette));
  EXPECT_EQ(overworld->overworld().overworld_map(parent)->area_palette(),
            edited_palette);
  ASSERT_OK(overworld->Undo());
  EXPECT_EQ(overworld->overworld().overworld_map(parent)->area_palette(),
            original_palette);
  ASSERT_OK(overworld->Redo());
  EXPECT_EQ(overworld->overworld().overworld_map(parent)->area_palette(),
            edited_palette);
  EXPECT_EQ(overworld->overworld().overworld_map(parent)->area_graphics(),
            original_graphics);

  const auto destination = fixture.root / "overworld-authoring.sfc";
  const auto blocked = manager_->SaveRomAs(destination.string());
  if (!blocked.ok()) {
    std::string conflicts;
    const auto& pending = manager_->pending_write_conflicts();
    const size_t shown = std::min<size_t>(pending.size(), 40);
    for (size_t i = 0; i < shown; ++i) {
      absl::StrAppend(
          &conflicts,
          absl::StrFormat("%06x %s\n", pending[i].address, pending[i].module));
    }
    FAIL() << blocked << "\nconflicts=" << pending.size() << "\n" << conflicts;
  }
  auto* project = manager_->GetCurrentProject();

  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), destination.string());

  const auto source_bytes = ReadFile(fixture.rom);
  const auto saved_bytes = ReadFile(destination);
  ExpectProtectedRegionsMatch(source_bytes, saved_bytes,
                              project->hack_manifest.protected_regions());
  ExpectRangeEqual(source_bytes, saved_bytes, zelda3::kAreaGfxIdPtr, 64,
                   "area graphics");
  ExpectRangeEqual(source_bytes, saved_bytes, zelda3::kMap16TilesExpanded,
                   zelda3::NumberOfMap16Ex * 8, "expanded tile16");
  ExpectRangeEqual(source_bytes, saved_bytes, zelda3::kMap32TileTRExpanded,
                   zelda3::kMap32TileCountExpanded, "expanded tile32 TR");
  ExpectRangeEqual(source_bytes, saved_bytes, zelda3::kMap32TileBLExpanded,
                   zelda3::kMap32TileCountExpanded, "expanded tile32 BL");
  ExpectRangeEqual(source_bytes, saved_bytes, zelda3::kMap32TileBRExpanded,
                   zelda3::kMap32TileCountExpanded, "expanded tile32 BR");
  const int palette_address = zelda3::kOverworldPalettesScreenToSetNew + parent;
  ASSERT_LT(static_cast<size_t>(palette_address), saved_bytes.size());
  EXPECT_EQ(saved_bytes[palette_address], static_cast<uint8_t>(edited_palette));
  EXPECT_NE(saved_bytes[palette_address], source_bytes[palette_address]);
  const auto saved_profile =
      zelda3::DetectOverworldRomProfile(*manager_->GetCurrentRom());
  EXPECT_TRUE(saved_profile.has_expanded_tile16);
  EXPECT_TRUE(saved_profile.has_expanded_tile32);
  EXPECT_EQ(saved_profile.has_tail_map_expansion,
            profile_before.has_tail_map_expansion);

  manager_.reset();
  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  ASSERT_OK(reopened->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  auto* reopened_overworld =
      reopened->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
          EditorType::kOverworld);
  ASSERT_NE(reopened_overworld, nullptr);
  const auto* reopened_map =
      reopened_overworld->overworld().overworld_map(parent);
  ASSERT_NE(reopened_map, nullptr);
  EXPECT_EQ(reopened_map->area_palette(), edited_palette);
  EXPECT_EQ(reopened_map->area_graphics(), original_graphics);
  const auto reopened_profile =
      zelda3::DetectOverworldRomProfile(*reopened->GetCurrentRom());
  EXPECT_TRUE(reopened_profile.has_expanded_tile16);
  EXPECT_TRUE(reopened_profile.has_expanded_tile32);
  reopened.reset();
}

TEST_F(AuthoringSaveWorkflowTest,
       MinecartStartUndoPublishSaveAsReopensOnFreshEditor) {
  if (!std::filesystem::exists(kSharedTrackSource)) {
    GTEST_SKIP() << "Oracle minecart source fixture is not available";
  }
  constexpr int kRoom = 0x89;
  constexpr int kTrack = 10;
  const auto fixture = CopyProject("yaze_oracle_minecart_save");
  const auto source_path =
      fixture.root / "Sprites/Objects/data/minecart_tracks.asm";
  std::filesystem::create_directories(source_path.parent_path());
  std::filesystem::copy_file(kSharedTrackSource, source_path);
  // The room and connected overview render the project's existing rail art.
  // Copy those inputs too; missing assets must not masquerade as a canvas test.
  const auto object_assets =
      std::filesystem::path(kSharedProject).parent_path() /
      "Dungeons/Objects/Data";
  ASSERT_TRUE(std::filesystem::is_directory(object_assets));
  for (const auto& entry : std::filesystem::directory_iterator(object_assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  const auto shared_source_hash = util::ComputeFileSha1Hex(kSharedTrackSource);
  const auto source_before = ReadFile(source_path);
  const auto rom_before = ReadFile(fixture.rom);
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().dungeon.kSaveSprites);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto panel_or = AuthoringSaveWorkflowTestPeer::MinecartPanel(*dungeon);
  ASSERT_OK(panel_or.status());
  auto* panel = *panel_or;
  ASSERT_TRUE(panel->ResolveTrackSourcePath().ok());
  EXPECT_EQ(*panel->ResolveTrackSourcePath(),
            std::filesystem::weakly_canonical(source_path));

  auto loaded = zelda3::LoadRoomFromRom(manager_->GetCurrentRom(), kRoom);
  ASSERT_TRUE(loaded.IsLoaded());
  dungeon->rooms()[kRoom] = std::move(loaded);
  auto& room = dungeon->rooms()[kRoom];
  room.EnsureSpritesLoaded();
  const auto collision_before = room.custom_collision().tiles;
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  viewer->SetHeaderVisible(false);
  viewer->SetHeaderHiddenMetadataHudVisible(false);
  viewer->canvas().set_global_scale(1.25f);
  viewer->canvas().set_scrolling(ImVec2(-24, -40));
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  ImGuiID tracks_toggle = 0;
  auto frame = [&](bool connected = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1100, 690), ImGuiCond_Always);
    ImGui::Begin("MinecartAuthoringCanvas", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize);
    tracks_toggle = ImGui::GetID("Tracks##canvas_tracks");
    if (connected) {
      viewer->DrawConnectedRoomMatrix(kRoom);
    } else {
      viewer->DrawDungeonCanvas(kRoom);
    }
    ImGui::End();
    ImGui::Render();
  };
  frame();
  frame();
  viewer->object_interaction().mode_manager().SetMode(
      InteractionMode::PaintCollision);
  // Reach the production Tracks control with real keyboard navigation. The
  // inspector has not been opened; enabling the canvas must load its source.
  for (int step = 0;
       step < 30 && ImGui::GetCurrentContext()->NavId != tracks_toggle;
       ++step) {
    io.AddKeyEvent(ImGuiKey_Tab, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Tab, false);
    frame();
  }
  ASSERT_EQ(ImGui::GetCurrentContext()->NavId, tracks_toggle);
  io.AddKeyEvent(ImGuiKey_Space, true);
  frame();
  io.AddKeyEvent(ImGuiKey_Space, false);
  frame();
  ASSERT_TRUE(panel->canvas_editing());
  EXPECT_TRUE(viewer->object_interaction().mode_manager().IsSelectMode());
  const auto before = panel->GetTracks().at(kTrack);
  ASSERT_EQ(before.room_id, kRoom);
  ASSERT_EQ(before.start_x, 0x1270);
  ASSERT_EQ(before.start_y, 0x1110);
  ASSERT_TRUE(room.has_custom_collision());
  EXPECT_EQ(room.custom_collision().tiles[34 * 64 + 14], 0xB8);
  EXPECT_EQ(room.custom_collision().tiles[16 * 64 + 48], 0xB7);
  const auto matching_cart = [&room](int x, int y) {
    return std::count_if(room.GetSprites().begin(), room.GetSprites().end(),
                         [=](const zelda3::Sprite& sprite) {
                           return !sprite.deleted() && sprite.id() == 0xA3 &&
                                  sprite.subtype() == kTrack &&
                                  sprite.layer() == 0 && sprite.x() == x &&
                                  sprite.y() == y;
                         });
  };
  ASSERT_EQ(matching_cart(7, 17), 1);
  ASSERT_EQ(matching_cart(24, 8), 0);
  auto after = before;
  after.start_x = 0x1380;
  after.start_y = 0x1080;
  auto point = [&](int x, int y) {
    const auto& canvas = viewer->canvas();
    const auto origin = canvas.zero_point();
    const auto scroll = canvas.scrolling();
    io.AddMousePosEvent(origin.x + scroll.x + (x + 8) * canvas.global_scale(),
                        origin.y + scroll.y + (y + 8) * canvas.global_scale());
    frame();
  };
  point(112, 272);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  frame();
  EXPECT_EQ(panel->selected_track(), kTrack);
  point(384, 128);
  EXPECT_EQ(panel->GetTracks().at(kTrack), before);
  EXPECT_EQ(matching_cart(7, 17), 1);
  frame(/*connected=*/true);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  frame();
  EXPECT_EQ(panel->GetTracks().at(kTrack), before);
  EXPECT_EQ(matching_cart(7, 17), 1);
  point(112, 272);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  frame();
  point(384, 128);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  frame();
  EXPECT_EQ(room.custom_collision().tiles, collision_before);
  EXPECT_EQ(panel->GetTracks().at(kTrack), after);
  EXPECT_EQ(matching_cart(24, 8), 1);
  ASSERT_TRUE(dungeon->CanUndo());
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(panel->GetTracks().at(kTrack), before);
  EXPECT_EQ(matching_cart(7, 17), 1);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(panel->GetTracks().at(kTrack), after);
  EXPECT_EQ(matching_cart(24, 8), 1);
  EXPECT_EQ(ReadFile(source_path), source_before);
  ASSERT_OK(panel->SaveTracks());
  const auto published = ReadFile(source_path);
  EXPECT_NE(published, source_before);
  const auto parsed = MinecartTrackSourceDocument::Parse(
      std::string(published.begin(), published.end()));
  ASSERT_OK(parsed.status());
  EXPECT_EQ(parsed->tracks().at(kTrack), after);

  const auto destination = fixture.root / "minecart-authoring.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedTrackSource), shared_source_hash);
  ExpectProtectedRegionsMatch(
      rom_before, ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());

  manager_.reset();
  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  auto reopened_room =
      zelda3::LoadRoomFromRom(reopened->GetCurrentRom(), kRoom);
  ASSERT_TRUE(reopened_room.IsLoaded());
  reopened_room.EnsureSpritesLoaded();
  EXPECT_EQ(std::count_if(reopened_room.GetSprites().begin(),
                          reopened_room.GetSprites().end(),
                          [](const zelda3::Sprite& sprite) {
                            return !sprite.deleted() && sprite.id() == 0xA3 &&
                                   sprite.subtype() == kTrack &&
                                   sprite.layer() == 0 && sprite.x() == 24 &&
                                   sprite.y() == 8;
                          }),
            1);
  EXPECT_EQ(std::count_if(reopened_room.GetSprites().begin(),
                          reopened_room.GetSprites().end(),
                          [](const zelda3::Sprite& sprite) {
                            return !sprite.deleted() && sprite.id() == 0xA3 &&
                                   sprite.subtype() == kTrack &&
                                   sprite.layer() == 0 && sprite.x() == 7 &&
                                   sprite.y() == 17;
                          }),
            0);
  EXPECT_EQ(ReadFile(source_path), published);
}

TEST_F(AuthoringSaveWorkflowTest, RailPaletteStampUndoSaveAsReopens) {
  constexpr int kRoom = 0x89;
  const auto fixture = CopyProject("yaze_rail_canvas_save");
  const auto source_path =
      fixture.root / "Sprites/Objects/data/minecart_tracks.asm";
  std::filesystem::create_directories(source_path.parent_path());
  std::filesystem::copy_file(kSharedTrackSource, source_path);
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin")
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
  }
  const auto rom_before = ReadFile(fixture.rom);
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto panel_or = AuthoringSaveWorkflowTestPeer::MinecartPanel(*dungeon);
  ASSERT_OK(panel_or.status());
  auto* panel = *panel_or;
  auto& room = dungeon->rooms()[kRoom];
  room = zelda3::LoadRoomFromRom(manager_->GetCurrentRom(), kRoom);
  room.EnsureObjectsLoaded();
  const auto collision_before = room.custom_collision();
  ASSERT_EQ(collision_before.tiles[16 * 64 + 48], 0xB7);
  ASSERT_EQ(collision_before.tiles[18 * 64 + 48], 0);
  const size_t object_count = room.GetTileObjects().size();
  AuthoringSaveWorkflowTestPeer::ActivateDungeonRoom(*dungeon, kRoom);
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  viewer->SetHeaderVisible(false);
  viewer->SetHeaderHiddenMetadataHudVisible(false);
  viewer->canvas().set_global_scale(1.25f);
  viewer->canvas().set_scrolling(ImVec2(-24, -40));
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  ImGuiID art_id = 0, stamp_id = 0;
  auto frame = [&](bool focus_tools = false) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 690), ImGuiCond_Always);
    if (focus_tools)
      ImGui::SetNextWindowFocus();
    ImGui::Begin("RailTools", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushID("MinecartRailPalette");
    art_id = ImGui::GetID("##Rail0");
    ImGui::PopID();
    stamp_id = ImGui::GetID("Stamp rail behavior");
    panel->DrawRailTools();
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(320, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(900, 690), ImGuiCond_Always);
    ImGui::Begin("RailCanvas", nullptr, ImGuiWindowFlags_NoSavedSettings);
    viewer->DrawDungeonCanvas(kRoom);
    ImGui::End();
    ImGui::Render();
  };
  frame();
  frame(true);
  auto activate = [&](ImGuiID id) {
    frame(true);
    for (int step = 0; step < 45 && ImGui::GetCurrentContext()->NavId != id;
         ++step) {
      io.AddKeyEvent(ImGuiKey_Tab, true);
      frame();
      io.AddKeyEvent(ImGuiKey_Tab, false);
      frame();
    }
    ASSERT_EQ(ImGui::GetCurrentContext()->NavId, id);
    io.AddKeyEvent(ImGuiKey_Space, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Space, false);
    frame();
  };
  auto point = [&](int x, int y) {
    const auto& canvas = viewer->canvas();
    io.AddMousePosEvent(canvas.zero_point().x + canvas.scrolling().x +
                            (x * 8 + 1) * canvas.global_scale(),
                        canvas.zero_point().y + canvas.scrolling().y +
                            (y * 8 + 1) * canvas.global_scale());
    frame();
  };
  auto click = [&]() {
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
  };
  activate(art_id);
  point(36, 10);
  EXPECT_EQ(room.GetTileObjects().size(), object_count);  // Ghost only.
  click();
  ASSERT_EQ(room.GetTileObjects().size(), object_count + 1);
  const auto placed = room.GetTileObjects().back();
  EXPECT_EQ(placed.id_, 0x31);
  EXPECT_EQ(placed.size_, 0);
  EXPECT_EQ(placed.x_, 36);
  EXPECT_EQ(placed.y_, 10);
  EXPECT_EQ(room.custom_collision().tiles, collision_before.tiles);
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.GetTileObjects().size(), object_count);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(room.GetTileObjects().size(), object_count + 1);

  activate(stamp_id);
  ASSERT_TRUE(panel->collision_stamping());
  point(48, 18);
  EXPECT_EQ(room.custom_collision().tiles,
            collision_before.tiles);  // Hover only.
  click();
  auto expected = collision_before;
  for (int y = 18; y < 20; ++y)
    for (int x = 48; x < 50; ++x)
      expected.tiles[y * 64 + x] = 0xB1;
  EXPECT_EQ(room.custom_collision().tiles, expected.tiles);
  EXPECT_EQ(room.GetTileObjects().size(),
            object_count + 1);  // No stray object.
  point(48, 16);
  click();  // Refuse overwriting the neighboring stop.
  EXPECT_EQ(room.custom_collision().tiles, expected.tiles);
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.custom_collision().tiles, collision_before.tiles);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(room.custom_collision().tiles, expected.tiles);
  io.AddKeyEvent(ImGuiKey_Escape, true);
  frame();
  io.AddKeyEvent(ImGuiKey_Escape, false);
  frame();
  EXPECT_FALSE(panel->collision_stamping());

  // Normal Oracle builds validate the tracked JSON against the edited ROM.
  const auto collision_source_before =
      ReadFile(fixture.root / "custom_collision.json");
  ASSERT_OK(manager_->SaveRom());
  const auto collision_source_after =
      ReadFile(fixture.root / "custom_collision.json");
  EXPECT_NE(collision_source_after, collision_source_before);
  const auto canonical =
      zelda3::DumpCustomCollisionSourceFromRom(manager_->GetCurrentRom());
  ASSERT_OK(canonical.status());
  EXPECT_EQ(
      std::string(collision_source_after.begin(), collision_source_after.end()),
      *canonical);
  const auto authored_hash = util::ComputeFileSha1Hex(fixture.rom.string());
  EXPECT_NE(authored_hash, fixture.source_hash);
  const auto destination = fixture.root / "rails-authored.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      rom_before, ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()), authored_hash);
  EXPECT_EQ(ReadFile(fixture.root / "custom_collision.json"),
            collision_source_after);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  manager_.reset();
  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  auto fresh = zelda3::LoadRoomFromRom(reopened->GetCurrentRom(), kRoom);
  fresh.EnsureObjectsLoaded();
  EXPECT_EQ(fresh.custom_collision().tiles, expected.tiles);
  ASSERT_EQ(fresh.GetTileObjects().size(), object_count + 1);
  const auto saved = fresh.GetTileObjects().back();
  EXPECT_EQ(saved.id_, placed.id_);
  EXPECT_EQ(saved.size_, placed.size_);
  EXPECT_EQ(saved.x_, placed.x_);
  EXPECT_EQ(saved.y_, placed.y_);
}

TEST_F(AuthoringSaveWorkflowTest,
       SelectedEntranceKeepsOracleWallFamilyAcrossRoomNavigation) {
  const auto custom_state = zelda3::CustomObjectManager::Get().SnapshotState();
  absl::Cleanup restore_custom_state = [&] {
    manager_.reset();
    zelda3::CustomObjectManager::Get().RestoreState(custom_state);
  };
  const auto fixture = CopyProject("yaze_entrance_preview");
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  ASSERT_TRUE(std::filesystem::is_directory(assets));
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin")
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
  }
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto* data = dungeon->game_data();
  ASSERT_NE(data, nullptr);
  const auto bytes = manager_->GetCurrentRom()->vector();
  ASSERT_OK(manager_->GetCurrentProject()->hack_manifest.LoadProjectRegistry(
      std::filesystem::path(kSharedProject).parent_path().string()));
  // Auto resolves project membership immediately, including a direct jump to
  // an interior room. Preview changes must not navigate to the entrance room.
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, 0x89);
  EXPECT_EQ(dungeon->current_room_id(), 0x89);
  auto* automatic_viewer =
      AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, 0x89);
  ASSERT_NE(automatic_viewer, nullptr);
  EXPECT_EQ(automatic_viewer->current_entrance_blockset(), 4);
  auto& automatic_room = dungeon->rooms()[0x89];
  automatic_room.PrepareForRender();
  EXPECT_EQ(automatic_room.blocks()[2], data->main_blockset_ids[4][2]);
  AuthoringSaveWorkflowTestPeer::PreviewDungeonEntrance(
      *dungeon, zelda3::kNumDungeonSpawnPoints);
  EXPECT_EQ(dungeon->current_room_id(), 0x89);
  EXPECT_EQ(automatic_viewer->current_entrance_blockset(), 3);
  AuthoringSaveWorkflowTestPeer::PreviewDungeonEntrance(*dungeon, -1);
  EXPECT_EQ(dungeon->current_room_id(), 0x89);
  EXPECT_EQ(automatic_viewer->current_entrance_blockset(), 4);
  for (const auto [room_id, main] : {std::pair{0x90, 1}, std::pair{0x89, 4}}) {
    AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, room_id);
    EXPECT_EQ(AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, room_id)
                  ->current_entrance_blockset(),
              main);
  }
  // 00 and 06 isolate preview persistence; 29 and 2D also exercise Oracle's
  // relocated records, whose main sets differ from the stale vanilla tables.
  // Expected sheets come from the ROM group table reached by PC6237.
  for (const auto [entrance, main, wall_sheet] :
       {std::tuple{0x00, 0x03, 0x13}, std::tuple{0x06, 0x06, 0x12},
        std::tuple{0x29, 0x01, 0x15}, std::tuple{0x2D, 0x0D, 0x12},
        std::tuple{0x00, 0x03, 0x13}}) {
    AuthoringSaveWorkflowTestPeer::SelectDungeonEntrance(
        *dungeon, zelda3::kNumDungeonSpawnPoints + entrance);
    if (entrance == 0x29)
      EXPECT_EQ(dungeon->current_room_id(), 0x90);
    for (const int room_id : {0x089, 0x098, 0x005, 0x089}) {
      SCOPED_TRACE(
          absl::StrFormat("entrance %02X room %03X", entrance, room_id));
      AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, room_id);
      auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, room_id);
      ASSERT_NE(viewer, nullptr);
      auto& room = dungeon->rooms()[room_id];
      const auto header = room.CaptureMetadataSnapshot();
      const auto dirty = room.CaptureSaveDirtySnapshot();
      room.PrepareForRender();
      EXPECT_EQ(viewer->current_entrance_blockset(), main);
      EXPECT_EQ(room.render_entrance_blockset(), main);
      EXPECT_EQ(room.blocks()[2], wall_sheet);
      for (int slot = 3; slot <= 6; ++slot) {
        const auto override_sheet =
            data->room_blockset_ids[room.blockset()][slot - 3];
        EXPECT_EQ(room.blocks()[slot], override_sheet == 0
                                           ? data->main_blockset_ids[main][slot]
                                           : override_sheet);
      }
      EXPECT_EQ(room.CaptureMetadataSnapshot(), header);
      EXPECT_EQ(room.CaptureSaveDirtySnapshot().header, dirty.header);
      EXPECT_EQ(room.CaptureSaveDirtySnapshot().object_stream,
                dirty.object_stream);
    }
  }
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), bytes);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest, AssertedWallSourceRendersCompleteRooms) {
  const auto fixture = CopyProject("yaze-asserted-wall-render");
  const auto source_root = std::filesystem::path(kSharedProject).parent_path();
  const auto source = source_root / "Dungeons/house_walls.asm";
  const auto original_source = ReadFile(source);
  const auto patterns_path = fixture.root / "walls.asm";
  std::filesystem::copy_file(source, patterns_path);
  for (const auto& entry : std::filesystem::directory_iterator(
           source_root / "Dungeons/Objects/Data")) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  {
    std::ofstream project(fixture.project, std::ios::app);
    project << "\n[files]\ndungeon_tile_patterns=walls.asm\n";
  }
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  ASSERT_NE(dungeon, nullptr);
  auto& custom = zelda3::CustomObjectManager::Get();
  const auto patterns = custom.GetTilePatterns();
  ASSERT_OK(patterns.status());
  ASSERT_NE(*patterns, nullptr);
  EXPECT_EQ((*patterns)->words.size(), 96u);

  // The guards are zero-byte directives. The same literal words without the
  // guards must render identically, including layout walls and room objects.
  const auto reference_path = fixture.root / "unguarded-walls.asm";
  {
    std::ifstream input(patterns_path);
    std::ofstream output(reference_path);
    for (std::string line; std::getline(input, line);) {
      if (line.find("assert pc()") == std::string::npos)
        output << line << '\n';
    }
  }
  const auto bytes = manager_->GetCurrentRom()->vector();
  const char* capture_path = std::getenv("YAZE_DUNGEON_RENDER_CAPTURE");
  for (int id : {0x05, 0x27, 0x38, 0x89, 0xE6}) {
    SCOPED_TRACE(absl::StrFormat("room $%03X", id));
    AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, id);
    auto& room = dungeon->rooms()[id];
    auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, id);
    ASSERT_NE(viewer, nullptr);
    RoomCompositeOutput output;
    auto& composite = output.Prepare(room, viewer->GetRoomLayerManager(id));
    const auto pixels = composite.vector();
    const auto& coverage = room.object_bg1_buffer().coverage_data();
    EXPECT_GT(std::count_if(coverage.begin(), coverage.end(),
                            [](uint8_t value) { return value != 0; }),
              256);
    if (capture_path && *capture_path) {
      std::filesystem::create_directories(capture_path);
      const auto path = std::filesystem::path(capture_path) /
                        absl::StrFormat("%03X-composite.bmp", id);
      ASSERT_EQ(SDL_SaveBMP(composite.surface(), path.string().c_str()), 0)
          << SDL_GetError();
    }
    custom.SetTilePatternsSource(reference_path.string());
    room.MarkGraphicsDirty();
    const auto& reference =
        output.Prepare(room, viewer->GetRoomLayerManager(id));
    EXPECT_EQ(reference.vector(), pixels);
    custom.SetTilePatternsSource(patterns_path.string());
  }
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), bytes);
  EXPECT_EQ(ReadFile(source), original_source);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest, OracleRoom038PreservesSubmergedDetails) {
  constexpr int kRoom = 0x38;
  const auto fixture = CopyProject("yaze-water-composite");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  ASSERT_OK(manager_->GetCurrentProject()->hack_manifest.LoadProjectRegistry(
      std::filesystem::path(kSharedProject).parent_path().string()));
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kRoom);
  auto& room = dungeon->rooms()[kRoom];
  room.PrepareForRender();
  ASSERT_EQ(room.layer2_mode(), 4);
  ASSERT_TRUE(room.GameLayerRegisters().UpperTilemapBlended());
  const auto bytes = manager_->GetCurrentRom()->vector();
  const auto dirty = room.CaptureSaveDirtySnapshot();
  const auto upper = room.bg1_buffer().bitmap().vector();
  const auto upper_objects = room.object_bg1_buffer().bitmap().vector();

  zelda3::RoomLayerManager layers;
  layers.ApplyLayerMerging(room.layer_merging());
  layers.ApplyRoomEffect(room.effect());
  layers.ApplyGameLayerRegisters(room.GameLayerRegisters());
  gfx::Bitmap output;
  room.RenderComposite(layers, output);
  ASSERT_FALSE(output.IsIndexed());
  const auto blended = output.ToRgba();
  layers.SetPriorityCompositing(false);
  room.RenderComposite(layers, output);
  EXPECT_EQ(output.ToRgba(), blended)
      << "Turning off tile priority must not disable water blending";
  layers.SetLayerVisible(zelda3::LayerType::BG1_Layout, false);
  layers.SetLayerVisible(zelda3::LayerType::BG1_Objects, false);
  room.RenderComposite(layers, output);
  const auto water_only = output.ToRgba();
  const auto& layout_mask = room.bg1_buffer().bg1_reveal_mask_data();
  const auto& object_mask = room.object_bg1_buffer().bg1_reveal_mask_data();
  int submerged_detail_pixels = 0;
  for (size_t i = 0; i < layout_mask.size(); ++i) {
    // These are precisely the water-covered pixels the legacy rectangular
    // masks erased. Require visible RGB differences, not just distinct indices.
    if ((layout_mask[i] | object_mask[i]) == 0)
      continue;
    if (!std::equal(blended.begin() + i * 4, blended.begin() + i * 4 + 3,
                    water_only.begin() + i * 4))
      ++submerged_detail_pixels;
  }
  EXPECT_GT(submerged_detail_pixels, 1024)
      << "Room 038's submerged architecture must survive its D9 water masks";
  EXPECT_EQ(room.bg1_buffer().bitmap().vector(), upper);
  EXPECT_EQ(room.object_bg1_buffer().bitmap().vector(), upper_objects);
  EXPECT_EQ(room.CaptureSaveDirtySnapshot().header, dirty.header);
  EXPECT_EQ(room.CaptureSaveDirtySnapshot().object_stream, dirty.object_stream);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), bytes);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       PotHandlerDisabledSaveRejectsThenScopeRetryReopens) {
  const auto fixture = CopyProject("yaze-pot-scope-retry");
  Open(fixture);
  ASSERT_FALSE(core::FeatureFlags::get().dungeon.kSavePotItems);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  std::vector<std::vector<zelda3::PotItem>> original;
  int selected = -1;
  for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
    zelda3::Room room(id, manager_->GetCurrentRom());
    room.LoadPotItems();
    original.push_back(room.GetPotItems());
    if (selected < 0 && !original.back().empty())
      selected = id;
  }
  ASSERT_GE(selected, 0);
  dungeon->add_room(selected);
  auto& room = dungeon->rooms()[selected];
  room.LoadPotItems();
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, selected);
  ASSERT_NE(viewer, nullptr);
  viewer->object_interaction().SetCurrentRoom(&dungeon->rooms(), selected);
  auto& handler =
      viewer->object_interaction().entity_coordinator().item_handler();
  const auto old_type = room.GetPotItems()[0].item;
  const uint8_t changed = old_type == 1 ? 2 : 1;
  ASSERT_TRUE(handler.MutateItemType(0, changed));
  ASSERT_TRUE(room.pot_items_dirty());
  ASSERT_TRUE(dungeon->CanUndo());
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.GetPotItems()[0].item, old_type);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(room.GetPotItems()[0].item, changed);
  const auto history = dungeon->undo_manager().UndoStackSize();
  const auto loaded = dungeon->LoadedRoomCount();
  ASSERT_LT(loaded, zelda3::kNumberOfRooms);
  const auto memory = manager_->GetCurrentRom()->vector();
  const auto project_bytes = ReadFile(fixture.project);
  const auto binding = manager_->GetCurrentRom()->filename();
  const auto target = fixture.root / "pot-scope.sfc";
  // Existing destination bytes must also survive a refused Save As.
  {
    std::ofstream out(target, std::ios::binary);
    out << "existing destination";
  }
  const auto target_before = ReadFile(target);
  const auto blocked = manager_->SaveRomAs(target.string());
  EXPECT_TRUE(absl::IsFailedPrecondition(blocked)) << blocked;
  EXPECT_NE(blocked.message().find("File > Save Scope"),
            absl::string_view::npos);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory);
  EXPECT_EQ(ReadFile(target), target_before);
  EXPECT_EQ(ReadFile(fixture.project), project_bytes);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), binding);
  EXPECT_EQ(dungeon->undo_manager().UndoStackSize(), history);
  EXPECT_TRUE(room.pot_items_dirty());
  EXPECT_EQ(dungeon->LoadedRoomCount(), loaded);
  EXPECT_TRUE(absl::IsFailedPrecondition(manager_->SaveRom()));
  EXPECT_TRUE(absl::IsFailedPrecondition(dungeon->SaveRoom(selected)));
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory);
  EXPECT_EQ(dungeon->undo_manager().UndoStackSize(), history);
  ASSERT_FALSE(HasFailure());

  // Reach the existing File > Save Scope checkbox using headless keyboard input.
  auto* popup = manager_->popup_manager();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  popup->Show(PopupID::kSaveScope);
  auto frame = [&]() {
    ImGui::NewFrame();
    popup->DrawPopups();
    ImGui::Render();
  };
  frame();
  frame();
  auto* window = ImGui::FindWindowByName(PopupID::kSaveScope);
  ASSERT_NE(window, nullptr);
  const auto checkbox = window->GetID("Save Pot Items");
  for (int i = 0; i < 80 && ImGui::GetCurrentContext()->NavId != checkbox;
       ++i) {
    io.AddKeyEvent(ImGuiKey_Tab, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Tab, false);
    frame();
  }
  ASSERT_EQ(ImGui::GetCurrentContext()->NavId, checkbox);
  io.AddKeyEvent(ImGuiKey_Space, true);
  frame();
  io.AddKeyEvent(ImGuiKey_Space, false);
  frame();
  ASSERT_TRUE(core::FeatureFlags::get().dungeon.kSavePotItems);
  popup->Hide(PopupID::kSaveScope);
  ASSERT_OK(manager_->SaveRomAs(target.string()));
  EXPECT_FALSE(manager_->HasPendingPotItemSaveConfirmation());
  EXPECT_EQ(dungeon->LoadedRoomCount(), loaded);
  EXPECT_FALSE(room.pot_items_dirty());
  auto fresh = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*fresh, &renderer_);
  fresh->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(fresh->OpenRomOrProject(target.string()));
  for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
    zelda3::Room reread(id, fresh->GetCurrentRom());
    reread.LoadPotItems();
    const auto& actual = reread.GetPotItems();
    ASSERT_EQ(actual.size(), original[id].size()) << id;
    for (size_t j = 0; j < actual.size(); ++j) {
      EXPECT_EQ(actual[j].position, original[id][j].position) << id;
      EXPECT_EQ(actual[j].item,
                id == selected && j == 0 ? changed : original[id][j].item)
          << id;
    }
  }
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), ReadFile(target),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  // Enabled Apply Room uses the same sparse serializer without a disk prompt.
  ASSERT_TRUE(handler.MutateItemType(0, old_type));
  ASSERT_OK(dungeon->SaveRoom(selected));
  EXPECT_FALSE(room.pot_items_dirty());
  zelda3::Room applied(selected, manager_->GetCurrentRom());
  applied.LoadPotItems();
  EXPECT_EQ(applied.GetPotItems()[0].item, old_type);
  EXPECT_EQ(dungeon->LoadedRoomCount(), loaded);
}

TEST_F(AuthoringSaveWorkflowTest,
       PotScopeEnabledWithoutEditsSavesWithoutLoadingRooms) {
  const auto fixture = CopyProject("yaze-pot-no-edits");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  const auto loaded = dungeon->LoadedRoomCount();
  core::FeatureFlags::get().dungeon.kSavePotItems = true;
  ASSERT_OK(manager_->SaveRomAs((fixture.root / "unchanged.sfc").string()));
  EXPECT_FALSE(manager_->HasPendingPotItemSaveConfirmation());
  EXPECT_EQ(dungeon->LoadedRoomCount(), loaded);
}

TEST_F(AuthoringSaveWorkflowTest,
       DungeonObjectUndoRedoSaveReopensOnFreshEditor) {
  const auto fixture = CopyProject("yaze_dungeon_authoring_save");
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().dungeon.kSaveObjects);
  ASSERT_FALSE(core::FeatureFlags::get().dungeon.kSavePotItems);

  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  auto loaded =
      zelda3::LoadRoomFromRom(manager_->GetCurrentRom(), kDungeonRoom);
  ASSERT_TRUE(loaded.IsLoaded());
  dungeon->rooms()[kDungeonRoom] = std::move(loaded);
  auto& objects = dungeon->rooms()[kDungeonRoom].GetTileObjects();
  ASSERT_FALSE(objects.empty());
  const int original_x = objects.front().x_;
  const int original_y = objects.front().y_;
  const int delta_x = original_x >= 63 ? -1 : 1;

  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kDungeonRoom);
  ASSERT_NE(viewer, nullptr);
  viewer->object_interaction().SetCurrentRoom(&dungeon->rooms(), kDungeonRoom);
  viewer->object_interaction().entity_coordinator().tile_handler().MoveObjects(
      kDungeonRoom, {0}, delta_x, 0);
  EXPECT_EQ(dungeon->rooms()[kDungeonRoom].GetTileObjects().front().x_,
            original_x + delta_x);
  ASSERT_TRUE(dungeon->CanUndo());
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(dungeon->rooms()[kDungeonRoom].GetTileObjects().front().x_,
            original_x);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(dungeon->rooms()[kDungeonRoom].GetTileObjects().front().x_,
            original_x + delta_x);
  EXPECT_EQ(dungeon->rooms()[kDungeonRoom].GetTileObjects().front().y_,
            original_y);

  const auto before_save = ReadFile(fixture.rom);
  const auto profile_before =
      zelda3::DetectOverworldRomProfile(*manager_->GetCurrentRom());
  ASSERT_OK(manager_->SaveRom());
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_NE(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);

  const auto saved_bytes = ReadFile(fixture.rom);
  ExpectProtectedRegionsMatch(
      before_save, saved_bytes,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  ASSERT_GT(saved_bytes.size(),
            static_cast<size_t>(zelda3::kAreaGfxIdPtr + 64));
  EXPECT_TRUE(std::equal(before_save.begin() + zelda3::kAreaGfxIdPtr,
                         before_save.begin() + zelda3::kAreaGfxIdPtr + 64,
                         saved_bytes.begin() + zelda3::kAreaGfxIdPtr));
  const auto profile_after =
      zelda3::DetectOverworldRomProfile(*manager_->GetCurrentRom());
  EXPECT_EQ(profile_after.has_expanded_tile16,
            profile_before.has_expanded_tile16);
  EXPECT_EQ(profile_after.has_expanded_tile32,
            profile_before.has_expanded_tile32);
  EXPECT_EQ(profile_after.has_tail_map_expansion,
            profile_before.has_tail_map_expansion);

  manager_.reset();
  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(fixture.rom.string()));
  ASSERT_OK(reopened->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto reopened_room =
      zelda3::LoadRoomFromRom(reopened->GetCurrentRom(), kDungeonRoom);
  ASSERT_TRUE(reopened_room.IsLoaded());
  ASSERT_FALSE(reopened_room.GetTileObjects().empty());
  EXPECT_EQ(reopened_room.GetTileObjects().front().x_, original_x + delta_x);
  EXPECT_EQ(reopened_room.GetTileObjects().front().y_, original_y);
  reopened.reset();
}

TEST_F(AuthoringSaveWorkflowTest,
       SourceWallPatternsPreviewMoveUndoSaveAsReopenPreservesRomPatterns) {
  constexpr int kRoom = 0xE6;
  const auto fixture = CopyProject("yaze-source-wall-patterns");
  const auto source = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/house_walls.asm";
  ASSERT_TRUE(std::filesystem::is_regular_file(source));
  const auto patterns_path = fixture.root / "walls.asm";
  std::filesystem::copy_file(source, patterns_path);
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  {
    std::ofstream project(fixture.project, std::ios::app);
    project << "\n[files]\ndungeon_tile_patterns=walls.asm\n";
  }
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  dungeon->add_room(kRoom);
  auto& room = dungeon->rooms()[kRoom];
  const auto memory_before = manager_->GetCurrentRom()->vector();
  AuthoringSaveWorkflowTestPeer::SynchronizePatternAssets(*dungeon);
  room.PrepareForRender();
  const auto preview = room.object_bg1_buffer().bitmap().vector();
  const auto source_patterns =
      zelda3::CustomObjectManager::Get().GetTilePatterns();
  ASSERT_TRUE(source_patterns.ok()) << source_patterns.status();
  ASSERT_NE(*source_patterns, nullptr);
  EXPECT_EQ((*source_patterns)->words.size(), 96);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before);
  EXPECT_EQ(ReadFile(patterns_path), ReadFile(source));

  zelda3::RoomObject corner(0x100, 0, 0, 0, 0);
  corner.SetRom(manager_->GetCurrentRom());
  const auto graphics_context =
      MakeDungeonGraphicsEditContext(room, corner, room.game_data());
  ASSERT_TRUE(graphics_context.ok()) << graphics_context.status();
  ASSERT_FALSE(graphics_context->targets.empty());
  // Source corner starts with $C89E: slot 2, tile $1E, palette row 2.
  EXPECT_EQ(graphics_context->targets.front().sheet_id, room.blocks()[2]);
  EXPECT_EQ(graphics_context->targets.front().tile_index, 0x1E);

  // A raw ROM view has the original patterns. Changing the preview does not
  // change the object's cached source payload or the ROM's dirty bytes.
  zelda3::CustomObjectManager::Get().SetTilePatternsSource("");
  AuthoringSaveWorkflowTestPeer::SynchronizePatternAssets(*dungeon);
  room.PrepareForRender();
  EXPECT_NE(room.object_bg1_buffer().bitmap().vector(), preview);
  zelda3::CustomObjectManager::Get().SetTilePatternsSource(
      patterns_path.string());
  AuthoringSaveWorkflowTestPeer::SynchronizePatternAssets(*dungeon);
  room.PrepareForRender();
  EXPECT_EQ(room.object_bg1_buffer().bitmap().vector(), preview);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before);

  // Exercise the same tile editor action used by the catalog and canvas.
  // Publish twice in one session to verify its source snapshot is refreshed.
  auto* tiles =
      AuthoringSaveWorkflowTestPeer::OpenObjectTiles(*dungeon, kRoom, 0x100);
  ASSERT_NE(tiles, nullptr);
  AuthoringSaveWorkflowTestPeer::PublishFirstWallTile(*tiles);
  const auto changed_patterns = ReadFile(patterns_path);
  EXPECT_NE(changed_patterns, ReadFile(source));
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before);
  room.PrepareForRender();
  EXPECT_NE(room.object_bg1_buffer().bitmap().vector(), preview);
  AuthoringSaveWorkflowTestPeer::PublishFirstWallTile(*tiles);
  EXPECT_EQ(ReadFile(patterns_path), ReadFile(source));
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before);
  AuthoringSaveWorkflowTestPeer::PublishFirstWallTile(*tiles);

  const auto object = room.GetTileObjects().front();
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  viewer->object_interaction().SetCurrentRoom(&dungeon->rooms(), kRoom);
  viewer->object_interaction().entity_coordinator().tile_handler().MoveObjects(
      kRoom, {0}, 1, 0);
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_);
  ASSERT_OK(dungeon->Redo());
  room.PrepareForRender();
  const auto edited_preview = room.object_bg1_buffer().bitmap().vector();
  const auto destination = fixture.root / "walls-authoring.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  const auto saved = ReadFile(destination);
  const auto original = ReadFile(fixture.rom);
  for (const auto& [pc, word] : (*source_patterns)->words) {
    ASSERT_LT(pc + 1, saved.size());
    EXPECT_EQ(saved[pc], original[pc]) << pc;
    EXPECT_EQ(saved[pc + 1], original[pc + 1]) << pc;
  }
  ExpectProtectedRegionsMatch(
      original, saved,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  ASSERT_OK(manager_->SwapProjectRom(destination.string()));
  manager_.reset();
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  dungeon->add_room(kRoom);
  auto& reopened = dungeon->rooms()[kRoom];
  reopened.PrepareForRender();
  EXPECT_EQ(reopened.GetTileObjects().front().x_, object.x_ + 1);
  EXPECT_EQ(reopened.object_bg1_buffer().bitmap().vector(), edited_preview);
  EXPECT_EQ(ReadFile(patterns_path), changed_patterns);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest, RoomUsageDefersUnselectedSharedSheetRender) {
  constexpr int kSelected = 0x89;
  constexpr int kOther = 0x4A;
  const auto fixture = CopyProject("yaze_lazy_room_usage");
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kOther);
  ASSERT_OK(
      AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kSelected));
  // Hold an explicit entrance while checking rendering, independently of the
  // background census that can refine Automatic entrance selection.
  AuthoringSaveWorkflowTestPeer::PreviewDungeonEntrance(
      *dungeon, zelda3::kNumDungeonSpawnPoints);
  graphics->SelectSheet(0);
  auto draw = [&] {
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    ImGui::Begin("Lazy room usage");
    AuthoringSaveWorkflowTestPeer::DrawRoomUsage(*graphics);
    ImGui::End();
    ImGui::EndFrame();
  };
  draw();
  const auto references =
      AuthoringSaveWorkflowTestPeer::RoomUsageReferences(*graphics);
  ASSERT_FALSE(references.empty());
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kSelected);
  draw();
  const auto selected_blocks =
      AuthoringSaveWorkflowTestPeer::RoomUsageBlocks(*graphics, kSelected);
  auto& other = dungeon->rooms()[kOther];
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  bool visible_edit = false;
  int attempts = 0;
  for (const auto& use : references) {
    const int tile = use.tile_word & 0x3FF;
    if (use.sheet >= 0x73 || (tile >= 0x1B0 && tile < 0x1D0) ||
        std::find(selected_blocks.begin(), selected_blocks.begin() + 8,
                  use.sheet) == selected_blocks.begin() + 8) {
      continue;
    }
    if (++attempts > 16) {
      break;
    }
    ASSERT_EQ(use.sheet, other.blocks()[use.slot]);
    graphics->SelectSheet(use.sheet);
    draw();
    AuthoringSaveWorkflowTestPeer::ChooseRoomUsage(*graphics, kOther);
    draw();
    const auto before =
        AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics);
    ASSERT_FALSE(before.empty());
    AuthoringSaveWorkflowTestPeer::ChooseRoomUsage(*graphics, kSelected);
    draw();
    const auto other_revision = other.graphics_revision();
    const int x = (tile % 16) * 8 + 3;
    const int y = ((tile % 64) / 16) * 8 + 3;
    const auto old_pixel =
        state.game_data->sheet_store.Sheet(use.sheet)[y * 128 + x];
    AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(*graphics, x, y,
                                                      old_pixel == 7 ? 1 : 7);
    draw();
    EXPECT_EQ(other.graphics_revision(), other_revision)
        << "Index refresh rasterized an unselected loaded room";
    EXPECT_TRUE(other.SourceSheetsChanged());
    AuthoringSaveWorkflowTestPeer::ChooseRoomUsage(*graphics, kOther);
    draw();
    EXPECT_NE(other.graphics_revision(), other_revision);
    EXPECT_FALSE(other.SourceSheetsChanged());
    visible_edit = AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                       .CountDifferentPixels(before) > 0;
    ASSERT_OK(graphics->Undo());
    draw();
    EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                  .CountDifferentPixels(before),
              0);
    if (visible_edit) {
      break;
    }
  }
  ASSERT_TRUE(visible_edit) << "No visible shared background pixel qualified";

  // Use a shared main slot so changing an auxiliary assignment cannot filter
  // the other room out of the chooser. Both rooms use the explicit entrance.
  graphics->SelectSheet(other.blocks()[0]);
  draw();
  AuthoringSaveWorkflowTestPeer::ChooseRoomUsage(*graphics, kSelected);
  draw();
  const auto assignment_revision = other.graphics_revision();
  auto& group = state.game_data->room_blockset_ids[other.blockset()];
  group[0] = other.blocks()[3] == 0x11 ? 0x12 : 0x11;
  draw();
  EXPECT_EQ(other.graphics_revision(), assignment_revision);
  EXPECT_EQ(other.blocks()[3], group[0]);
  AuthoringSaveWorkflowTestPeer::ChooseRoomUsage(*graphics, kOther);
  draw();
  EXPECT_NE(other.graphics_revision(), assignment_revision);
}

TEST_F(AuthoringSaveWorkflowTest, RoomUsageFollowsUnsavedMinesContext) {
  constexpr int kRoom = 0x89;
  const auto fixture = CopyProject("yaze_live_room_usage");
  const auto source_root = std::filesystem::path(kSharedProject).parent_path();
  for (const auto& entry : std::filesystem::directory_iterator(
           source_root / "Dungeons/Objects/Data")) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin")
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
  }
  std::filesystem::copy_file(source_root / "Dungeons/house_walls.asm",
                             fixture.root / "house_walls.asm");
  auto project_text_bytes = ReadFile(fixture.project);
  std::string project_text(project_text_bytes.begin(),
                           project_text_bytes.end());
  ReplaceAssignment(&project_text, "dungeon_tile_patterns", "house_walls.asm");
  {
    std::ofstream output(fixture.project);
    output << project_text;
  }
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().dungeon.kSaveRoomHeaders);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kRoom);
  auto& room = dungeon->rooms()[kRoom];
  ASSERT_TRUE(room.IsLoaded());
  room.PrepareForRender();
  const auto saved_blocks = room.blocks();
  graphics->SelectSheet(saved_blocks[2]);
  manager_->window_manager().OpenWindow("graphics.usage_preview");
  auto draw = [](GraphicsEditor& editor) {
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();
    ImGui::Begin("Live room usage");
    AuthoringSaveWorkflowTestPeer::DrawRoomUsage(editor);
    ImGui::End();
    ImGui::EndFrame();
  };
  draw(*graphics);
  const auto original_image =
      AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics);
  ASSERT_FALSE(original_image.empty());
  const auto idle_revision = room.composite_source_revision();
  for (int frame = 0; frame < 3; ++frame)
    draw(*graphics);
  EXPECT_EQ(room.composite_source_revision(), idle_revision)
      << "Idle usage frames must not rerender the room";
  EXPECT_NE(
      AuthoringSaveWorkflowTestPeer::RoomUsageContextLabel(*graphics, kRoom)
          .find("Live ·"),
      std::string::npos);

  AuthoringSaveWorkflowTestPeer::PreviewDungeonEntrance(
      *dungeon, zelda3::kNumDungeonSpawnPoints);
  draw(*graphics);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageBlocks(*graphics, kRoom),
            room.blocks());
  EXPECT_GT(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                .CountDifferentPixels(original_image),
            0);
  AuthoringSaveWorkflowTestPeer::PreviewDungeonEntrance(*dungeon, -1);
  draw(*graphics);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                .CountDifferentPixels(original_image),
            0);

  const int new_blockset = room.blockset() == 1 ? 2 : 1;
  ASSERT_OK(dungeon->EditRoomMetadata(
      kRoom, {RoomMetadataField::kBlockset, new_blockset}));
  draw(*graphics);
  ASSERT_NE(room.blocks(), saved_blocks);
  const auto usage =
      AuthoringSaveWorkflowTestPeer::RoomUsageBlocks(*graphics, kRoom);
  for (int slot = 0; slot < 16; ++slot)
    EXPECT_EQ(usage[slot], room.blocks()[slot]) << "slot " << slot;
  const auto roomset_image =
      AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics);
  EXPECT_GT(roomset_image.CountDifferentPixels(original_image), 0);
  ASSERT_OK(dungeon->Undo());
  draw(*graphics);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                .CountDifferentPixels(original_image),
            0);
  ASSERT_OK(dungeon->Redo());
  draw(*graphics);

  // Choose a traced source and prove visibility separately from sheet loading.
  const auto references =
      AuthoringSaveWorkflowTestPeer::RoomUsageReferences(*graphics);
  ASSERT_FALSE(references.empty());
  bool changed_reference = false;
  for (const auto& use : references) {
    ASSERT_GE(use.slot, 0);
    ASSERT_LT(use.slot, 8);
    EXPECT_EQ(use.sheet, room.blocks()[use.slot]);
    EXPECT_EQ(use.slot, (use.tile_word & 0x3FF) / 64);
    changed_reference |= use.sheet != saved_blocks[use.slot];
  }
  EXPECT_TRUE(changed_reference)
      << "Affected-object feedback kept the old sheets";
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  int edited_sheet = -1, edited_x = 0, edited_y = 0, edited_color = 0;
  usage_preview::PreviewImage edited_image;
  int attempts = 0;
  for (const auto& use : references) {
    if (++attempts > 16)
      break;
    ASSERT_EQ(use.sheet, room.blocks()[use.slot]);
    ASSERT_EQ(use.slot, (use.tile_word & 0x3FF) / 64);
    if (use.sheet >= 0x73)
      continue;  // This fixture qualifies the background 3bpp persistence lane.
    graphics->SelectSheet(use.sheet);
    draw(*graphics);
    const auto before =
        AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics);
    const int tile = use.tile_word & 63;
    const int x = (tile % 16) * 8 + 3;
    const int y = (tile / 16) * 8 + 3;
    const int old_color =
        state.game_data->sheet_store.Sheet(use.sheet)[y * 128 + x];
    const int color = old_color == 7 ? 1 : 7;
    AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(*graphics, x, y, color);
    draw(*graphics);
    const auto after = AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics);
    if (after.CountDifferentPixels(before) > 0) {
      edited_sheet = use.sheet;
      edited_x = x;
      edited_y = y;
      edited_color = color;
      edited_image = after;
      ASSERT_OK(graphics->Undo());
      draw(*graphics);
      EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                    .CountDifferentPixels(before),
                0);
      ASSERT_OK(graphics->Redo());
      draw(*graphics);
      EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                    .CountDifferentPixels(after),
                0);
      break;
    }
    ASSERT_OK(graphics->Undo());
  }
  ASSERT_GE(edited_sheet, 0) << "No visible background pixel qualified";
  EXPECT_TRUE(
      manager_->window_manager().IsWindowOpen("graphics.usage_preview"));
  const auto destination = fixture.root / "usage-edited.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  ASSERT_OK(manager_->SwapProjectRom(destination.string()));
  manager_.reset();
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kRoom);
  EXPECT_EQ(dungeon->rooms()[kRoom].blockset(), new_blockset);
  graphics->SelectSheet(edited_sheet);
  draw(*graphics);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                .CountDifferentPixels(edited_image),
            0);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics)
                .game_data->sheet_store.Sheet(
                    edited_sheet)[edited_y * 128 + edited_x],
            edited_color);

  // A second session resolves its own original room, not the first session's
  // unsaved/live context or cached renderer. Revert also rebinds same addresses.
  const auto second_path = fixture.root / "second.sfc";
  std::filesystem::copy_file(fixture.rom, second_path);
  ASSERT_OK(manager_->OpenRomOrProject(second_path.string()));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* second_dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  auto* second_graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  ASSERT_NE(second_graphics, graphics);
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*second_dungeon, kRoom);
  second_graphics->SelectSheet(edited_sheet);
  draw(*second_graphics);
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::RoomUsageBlocks(*second_graphics, kRoom),
      second_dungeon->rooms()[kRoom].blocks());
  EXPECT_NE(second_dungeon->rooms()[kRoom].blockset(), new_blockset);
  ASSERT_OK(manager_->RevertRomToSaved());
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*second_dungeon, kRoom);
  draw(*second_graphics);
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::RoomUsageBlocks(*second_graphics, kRoom),
      second_dungeon->rooms()[kRoom].blocks());
  manager_->SwitchToSession(0);
  draw(*graphics);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::RoomUsageImage(*graphics)
                .CountDifferentPixels(edited_image),
            0);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       GoronMinesGraphicsAndLayersUndoRedoSaveAsReopen) {
  constexpr int kRoom = 0x89;
  const auto fixture = CopyProject("yaze_mines_graphics_save_as");
  const auto assets = std::filesystem::path(kSharedProject).parent_path() /
                      "Dungeons/Objects/Data";
  ASSERT_TRUE(std::filesystem::is_directory(assets));
  for (const auto& entry : std::filesystem::directory_iterator(assets)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
    }
  }
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().dungeon.kSaveRoomHeaders);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  dungeon->add_room(kRoom);
  auto& room = dungeon->rooms()[kRoom];
  ASSERT_TRUE(room.IsLoaded());
  ASSERT_NE(room.game_data(), nullptr);
  const auto entrance = room.game_data()->room_default_entrances.at(kRoom);
  ASSERT_GE(entrance.entrance_id, 0);
  const int entrance_slot =
      entrance.entrance_id + zelda3::kNumDungeonSpawnPoints;
  AuthoringSaveWorkflowTestPeer::SelectDungeonEntrance(*dungeon, entrance_slot);
  dungeon->add_room(kRoom);
  room.PrepareForRender();
  const int original_palette = room.palette();
  const int original_bg2 = room.layer_merging().ID;
  const int edited_palette = original_palette == 1 ? 0 : 1;
  const int edited_bg2 = original_bg2 == 1 ? 0 : 1;
  const auto original_sheets = room.blocks();
  ASSERT_FALSE(room.GetTileObjects().empty());
  const auto object = room.GetTileObjects().front();
  const int delta_x = object.x_ >= 63 ? -1 : 1;
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  viewer->object_interaction().SetCurrentRoom(&dungeon->rooms(), kRoom);
  viewer->object_interaction().entity_coordinator().tile_handler().MoveObjects(
      kRoom, {0}, delta_x, 0);
  ASSERT_OK(dungeon->EditRoomMetadataBatch({
      {kRoom, {RoomMetadataField::kPalette, edited_palette}},
      {kRoom, {RoomMetadataField::kBg2, edited_bg2}},
  }));
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.palette(), original_palette);
  EXPECT_EQ(room.layer_merging().ID, original_bg2);
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_);
  ASSERT_OK(dungeon->Redo());
  ASSERT_OK(dungeon->Redo());
  room.PrepareForRender();
  EXPECT_EQ(room.palette(), edited_palette);
  EXPECT_EQ(room.layer_merging().ID, edited_bg2);
  EXPECT_EQ(room.blocks(), original_sheets);
  const auto bg1 = room.bg1_buffer().bitmap().vector();
  const auto bg2 = room.bg2_buffer().bitmap().vector();
  const auto palette = room.rendered_dungeon_palette();
  const auto registers = room.GameLayerRegisters();
  ASSERT_FALSE(bg1.empty());
  ASSERT_FALSE(bg2.empty());
  ASSERT_FALSE(palette.empty());
  const auto destination = fixture.root / "mines-authoring.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  ExpectProtectedRegionsMatch(
      ReadFile(fixture.rom), ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  // Save As creates a copy. Bind the disposable project to it explicitly before
  // reopening with the same custom assets and entrance context.
  ASSERT_OK(manager_->SwapProjectRom(destination.string()));
  manager_.reset();
  Open(fixture);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), destination.string());
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  AuthoringSaveWorkflowTestPeer::SelectDungeonEntrance(*dungeon, entrance_slot);
  dungeon->add_room(kRoom);
  auto& reopened = dungeon->rooms()[kRoom];
  reopened.PrepareForRender();
  EXPECT_EQ(reopened.GetTileObjects().front().x_, object.x_ + delta_x);
  EXPECT_EQ(reopened.GetTileObjects().front().y_, object.y_);
  EXPECT_EQ(reopened.palette(), edited_palette);
  EXPECT_EQ(reopened.layer_merging().ID, edited_bg2);
  EXPECT_EQ(reopened.blocks(), original_sheets);
  EXPECT_EQ(reopened.bg1_buffer().bitmap().vector(), bg1);
  EXPECT_EQ(reopened.bg2_buffer().bitmap().vector(), bg2);
  EXPECT_EQ(reopened.rendered_dungeon_palette(), palette);
  EXPECT_EQ(reopened.GameLayerRegisters().tm, registers.tm);
  EXPECT_EQ(reopened.GameLayerRegisters().ts, registers.ts);
  EXPECT_EQ(reopened.GameLayerRegisters().cgadsub, registers.cgadsub);
}

std::filesystem::path ResolveOracleEditRom() {
  const std::string discovered = test::FindOracleRomFixture();
  if (discovered.empty()) {
    return {};
  }
  std::filesystem::path path(discovered);
  // FindOracleRomFixture prefers the patched emulator ROM. Editing uses the
  // sibling base ROM that the project manifest guards.
  if (path.filename() == "oos168x.sfc") {
    const auto base = path.parent_path() / "oos168.sfc";
    if (std::filesystem::exists(base)) {
      return base;
    }
    return {};
  }
  return path;
}

OverworldEditor* LoadedOverworld(EditorManager* manager) {
  EXPECT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kOverworld));
  if (manager->GetCurrentEditorSet() == nullptr) {
    return nullptr;
  }
  return manager->GetCurrentEditorSet()->GetEditorAs<OverworldEditor>(
      EditorType::kOverworld);
}

TEST_F(AuthoringSaveWorkflowTest,
       PalettePixelCommandLiveSourceAtomicSaveAsFreshReopen) {
  const auto fixture = CopyProject("yaze-palette-pixel-command");
  Open(fixture);
  auto* ow = LoadedOverworld(manager_.get());
  ASSERT_NE(ow, nullptr);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  ow->SetCurrentMap(0);
  int tile16 = -1, quadrant = -1;
  PalettePermutation permutation{0, 1, 2, 3, 4, 5, 6, 7};
  // Select an actual map tile with an addressable palette and a used nonzero
  // index whose color differs from another slot. No synthetic art is inserted.
  for (int y = 0; y < 32 && tile16 < 0; ++y) {
    for (int x = 0; x < 32 && tile16 < 0; ++x) {
      const int id = ow->overworld().GetMapTiles(0)[x][y];
      for (int q = 0; q < 4 && tile16 < 0; ++q) {
        if (!ow->OpenGraphicsForTile16Quadrant(id, q).ok())
          continue;
        const auto* t = state.SourceTarget();
        if (!t || !t->writable_palette ||
            t->writable_palette->colors[1]->group !=
                zelda3::PaletteAddressGroup::kOverworldMain)
          continue;
        const auto pixels = state.game_data->sheet_store.Sheet(t->sheet_id);
        const int start =
            (t->tile_index / 16) * 8 * 128 + (t->tile_index % 16) * 8;
        for (int py = 0; py < 8 && tile16 < 0; ++py)
          for (int px = 0; px < 8 && tile16 < 0; ++px) {
            const int raw = pixels[start + py * 128 + px];
            if (raw == 0 || raw > 7)
              continue;
            for (int other = 1; other < 8; ++other) {
              if (t->palette[raw].snes() == t->palette[other].snes())
                continue;
              std::swap(permutation[raw], permutation[other]);
              tile16 = id;
              quadrant = q;
              break;
            }
          }
      }
    }
  }
  ASSERT_GE(tile16, 0);
  const auto target = *state.SourceTarget();
  RecordProperty("tile16", tile16);
  RecordProperty("quadrant", quadrant);
  RecordProperty("sheet", target.sheet_id);
  const auto control_path = fixture.root / "control.sfc";
  ASSERT_OK(manager_->SaveRomAs(control_path.string()));
  const auto control = ReadFile(control_path);
  auto plan =
      PlanPalettePixelPermutation(*state.source_context, {0}, permutation);
  ASSERT_OK(plan.status());
  ASSERT_FALSE(plan->colors.empty());
  ASSERT_FALSE(plan->sheets.empty());
  const auto span = state.game_data->sheet_store.Sheet(target.sheet_id);
  const std::vector<uint8_t> before(span.begin(), span.end());
  auto& palettes = gfx::PaletteManager::Get();
  const auto palette_undo = palettes.GetUndoStackSize();
  const auto graphics_undo = graphics->undo_manager().UndoStackSize();

  // Retained negative reproduction: the display-only refresh still resolves
  // the old copied map. The mutation boundary must consult the actual parent.
  auto* parent = ow->overworld().mutable_overworld_map(
      ow->overworld().overworld_map(0)->parent());
  const auto old_assignment = parent->main_palette();
  ASSERT_GT(state.game_data->palette_groups.overworld_main.size(), 1u);
  parent->set_main_palette(
      (old_assignment + 1) %
      state.game_data->palette_groups.overworld_main.size());
  ASSERT_OK(state.source_context->refresh_palettes(*state.source_context));
  EXPECT_EQ(state.SourceTarget()->sheet_id, target.sheet_id);
  const auto refused = graphics->ReorderSourcePalette({0}, permutation);
  EXPECT_FALSE(refused.ok())
      << "Captured preview must not authorize a stale assignment";
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id),
            absl::MakeConstSpan(before));
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  EXPECT_EQ(graphics->undo_manager().UndoStackSize(), graphics_undo);
  parent->set_main_palette(old_assignment);

  // Drive the actual Pixel Editor control and the callback installed by
  // GraphicsEditor::Initialize, not a private encoder or test-only UI.
  auto& io = ImGui::GetIO();
  io.ConfigInputTrickleEventQueue = false;
  unsigned char* font_pixels;
  int font_width, font_height;
  io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
  const auto frame = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_Always);
    ImGui::Begin("PaletteReorderHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    AuthoringSaveWorkflowTestPeer::DrawGraphicsPalette(*graphics);
    ImGui::End();
    ImGui::Render();
  };
  const auto click = [&](const std::string& label) {
    std::optional<gui::WidgetIdRegistry::WidgetInfo> button;
    for (const auto& [path, item] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (item.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        button = item;
    }
    ASSERT_TRUE(button) << label;
    io.AddMousePosEvent((button->bounds.min_x + button->bounds.max_x) / 2,
                        (button->bounds.min_y + button->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    frame();
  };
  frame();
  frame();
  click("Reorder source colors");
  for (int slot = 1; slot < 8; ++slot)
    if (permutation[slot] != slot)
      click(absl::StrFormat("Reorder after slot %d", slot));
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id),
            absl::MakeConstSpan(before));
  EXPECT_FALSE(palettes.HasUnsavedChanges());
  click("Apply palette reorder");
  EXPECT_EQ(graphics->undo_manager().UndoStackSize(), graphics_undo + 1);
  EXPECT_EQ(palettes.GetUndoStackSize(), palette_undo);
  EXPECT_TRUE(graphics->HasPendingGraphicsChanges());
  EXPECT_TRUE(palettes.HasUnsavedChanges());
  const auto changed_span = state.game_data->sheet_store.Sheet(target.sheet_id);
  const std::vector<uint8_t> after(changed_span.begin(), changed_span.end());
  auto appearance = [&](const std::vector<uint8_t>& pixels,
                        const gfx::SnesPalette& colors) {
    std::vector<uint16_t> words;
    const int start =
        (target.tile_index / 16) * 8 * 128 + (target.tile_index % 16) * 8;
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        words.push_back(colors[pixels[start + y * 128 + x]].snes());
    return words;
  };
  const auto expected_appearance = appearance(before, target.palette);
  GraphicsEditContext fresh;
  ASSERT_OK(state.source_context->resolve_live_source(fresh));
  EXPECT_EQ(appearance(after, fresh.targets[0].palette), expected_appearance);
  const int tile_x = (target.tile_index % 16) * 8;
  const int tile_y = (target.tile_index / 16) * 8;
  AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(
      *graphics, tile_x, tile_y, after[tile_y * 128 + tile_x], false);
  EXPECT_FALSE(graphics->Undo().ok());
  EXPECT_FALSE(graphics->ReorderSourcePalette({0}, permutation).ok());
  EXPECT_EQ(graphics->undo_manager().UndoStackSize(), graphics_undo + 1);
  AuthoringSaveWorkflowTestPeer::EndGraphicsStroke(*graphics);
  ASSERT_OK(graphics->Undo());
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id),
            absl::MakeConstSpan(before));
  for (const auto& change : plan->colors)
    EXPECT_EQ(palettes
                  .GetColor(change.group_name, change.palette_index,
                            change.color_index)
                  .snes(),
              change.original_color.snes());
  ASSERT_OK(graphics->Redo());
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id),
            absl::MakeConstSpan(after));
  const auto destination = fixture.root / "palette-pixel.sfc";
  const auto filename = manager_->GetCurrentRom()->filename();
  {
    test::ScopedRomStagingFailure failure;
    const auto failed = manager_->SaveRomAs(destination.string());
    EXPECT_FALSE(failed.ok());
    EXPECT_NE(failed.message().find("staging failure injected for testing"),
              absl::string_view::npos)
        << failed;
  }
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), filename);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), control);
  EXPECT_EQ(ReadFile(control_path), control);
  EXPECT_EQ(graphics->undo_manager().UndoStackSize(), graphics_undo + 1);
  EXPECT_TRUE(graphics->HasPendingGraphicsChanges());
  EXPECT_TRUE(palettes.HasUnsavedChanges());
  ASSERT_OK(graphics->Undo());
  ASSERT_OK(graphics->Redo());
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  const auto saved = ReadFile(destination);
  for (const auto& change : plan->colors) {
    const auto pc = gfx::GetPaletteAddress(
        change.group_name, change.palette_index, change.color_index);
    ASSERT_LT(pc + 1, saved.size());
    EXPECT_EQ(saved[pc] | (saved[pc + 1] << 8), change.new_color.snes());
  }
  ExpectProtectedRegionsMatch(
      control, saved,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  EXPECT_FALSE(graphics->HasPendingGraphicsChanges());
  EXPECT_FALSE(palettes.HasUnsavedChanges());

  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  auto* reopened_ow = LoadedOverworld(reopened.get());
  ASSERT_NE(reopened_ow, nullptr);
  reopened_ow->SetCurrentMap(0);
  ASSERT_OK(reopened_ow->OpenGraphicsForTile16Quadrant(tile16, quadrant));
  auto& loaded = AuthoringSaveWorkflowTestPeer::GraphicsState(
      *reopened->GetCurrentEditorSet()->GetGraphicsEditor());
  ASSERT_TRUE(loaded.SourceTarget());
  EXPECT_EQ(loaded.SourceTarget()->sheet_id, target.sheet_id);
  EXPECT_EQ(loaded.SourceTarget()->tile_index, target.tile_index);
  EXPECT_EQ(loaded.game_data->sheet_store.Sheet(target.sheet_id),
            absl::MakeConstSpan(after));
  EXPECT_EQ(appearance(after, loaded.SourceTarget()->palette),
            expected_appearance);
  EXPECT_TRUE(ValidateGraphicsPaletteAddressing(*loaded.SourceTarget(),
                                                loaded.game_data)
                  .ok());
  // The original session's command cannot operate while the second owns Arena.
  EXPECT_FALSE(graphics->Undo().ok());
  EXPECT_EQ(graphics->undo_manager().UndoStackSize(), graphics_undo + 1);
  gfx::Arena::Get().ClearTextureQueue();
  reopened.reset();
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       DungeonWallPalettePatternSaveAsFreshReopenPreservesAppearance) {
  constexpr int kRoom = 0x89;
  const auto fixture = CopyProject("yaze-wall-palette-permutation");
  const auto oracle = std::filesystem::path(kSharedProject).parent_path();
  std::filesystem::copy_file(oracle / "Dungeons/house_walls.asm",
                             fixture.root / "walls.asm");
  for (const auto& entry :
       std::filesystem::directory_iterator(oracle / "Dungeons/Objects/Data")) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin")
      std::filesystem::copy_file(entry.path(),
                                 fixture.root / entry.path().filename());
  }
  {
    std::ofstream project(fixture.project, std::ios::app);
    project << "\n[files]\ndungeon_tile_patterns=walls.asm\n";
  }
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  AuthoringSaveWorkflowTestPeer::SelectDungeonRoom(*dungeon, kRoom);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  AuthoringSaveWorkflowTestPeer::SynchronizePatternAssets(*dungeon);
  zelda3::RoomObject corner(0x100, 0, 0, 0, 0);
  corner.SetRom(manager_->GetCurrentRom());
  AuthoringSaveWorkflowTestPeer::OpenObjectGraphics(*dungeon, kRoom, corner);
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  ASSERT_TRUE(state.source_context);
  const auto source = *state.source_context;
  ASSERT_GT(source.targets.size(), 1u);
  std::vector<size_t> targets;
  for (size_t i = 0; i < source.targets.size(); ++i)
    targets.push_back(i);
  std::optional<PalettePermutation> permutation;
  for (uint8_t a = 1; a < 8 && !permutation; ++a) {
    for (uint8_t b = a + 1; b < 8 && !permutation; ++b) {
      PalettePermutation candidate{0, 1, 2, 3, 4, 5, 6, 7};
      std::swap(candidate[a], candidate[b]);
      auto plan = PlanPalettePixelPermutation(source, targets, candidate);
      if (plan.ok() && !plan->colors.empty() && !plan->sheets.empty())
        permutation = candidate;
    }
  }
  ASSERT_TRUE(permutation) << "Custom corner needs an addressable used palette";
  const auto plan = PlanPalettePixelPermutation(source, targets, *permutation);
  ASSERT_OK(plan.status());
  auto appearance = [](const zelda3::GameData& data,
                       const GraphicsEditTarget& target) {
    std::vector<uint16_t> colors;
    const auto pixels = data.sheet_store.Sheet(target.sheet_id);
    const int start =
        (target.tile_index / 16) * 8 * 128 + (target.tile_index % 16) * 8;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        const int raw = pixels[start + y * 128 + x];
        if (raw == 0) {
          colors.push_back(0);  // Fixed transparency.
        } else {
          const auto color = zelda3::ReadPaletteColor(
              data, *target.writable_palette->colors[raw]);
          EXPECT_TRUE(color.ok()) << color.status();
          colors.push_back(color.ok() ? color->snes() : 0xffff);
        }
      }
    }
    return colors;
  };
  std::vector<std::vector<uint16_t>> before;
  for (const auto& target : source.targets)
    before.push_back(appearance(*state.game_data, target));
  const auto control_path = fixture.root / "control.sfc";
  ASSERT_OK(manager_->SaveRomAs(control_path.string()));
  const auto undo_depth = graphics->undo_manager().UndoStackSize();
  ASSERT_OK(graphics->ReorderSourcePalette(targets, *permutation));
  EXPECT_EQ(graphics->undo_manager().UndoStackSize(), undo_depth + 1);
  for (size_t i = 0; i < source.targets.size(); ++i)
    EXPECT_EQ(appearance(*state.game_data, source.targets[i]), before[i]);
  ASSERT_OK(graphics->Undo());
  ASSERT_OK(graphics->Redo());
  const auto destination = fixture.root / "wall-palette.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      ReadFile(control_path), ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  manager_.reset();
  gfx::Arena::Get().ClearTextureQueue();
  manager_ = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager_, &renderer_);
  manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager_->OpenRomOrProject(destination.string()));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  const auto* loaded = manager_->GetCurrentGameData();
  for (size_t i = 0; i < source.targets.size(); ++i)
    EXPECT_EQ(appearance(*loaded, source.targets[i]), before[i]);
  for (const auto& diff : plan->sheets) {
    const auto pixels = loaded->sheet_store.Sheet(diff.sheet);
    for (size_t i = 0; i < diff.offsets.size(); ++i)
      EXPECT_EQ(pixels[diff.offsets[i]], diff.after[i]);
  }
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
}

TEST_F(AuthoringSaveWorkflowTest, MainMenusStayOpenWithRightSettingsDrawer) {
  const auto fixture = CopyProject("yaze-menu-settings");
  Open(fixture);
  ASSERT_NE(LoadedOverworld(manager_.get()), nullptr);
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  manager_->SwitchToEditor(EditorType::kOverworld, true);
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  auto* drawer = manager_->right_drawer_manager();
  ASSERT_NE(drawer, nullptr);
  const auto frame = [&]() {
    gui::WidgetIdRegistry::Instance().BeginFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(48, 0));
    ImGui::SetNextWindowSize(ImVec2(1280 - 48 - drawer->GetDrawerWidth(), 720));
    ImGui::Begin("DockSpaceWindow", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);
    gui::DockSpaceRenderer::BeginEnhancedDockSpace(
        ImGui::GetID("MainDockSpace"), ImVec2(0, 0),
        ImGuiDockNodeFlags_PassthruCentralNode);
    manager_->DrawMainMenuBar();
    gui::DockSpaceRenderer::EndEnhancedDockSpace();
    ImGui::End();
    EXPECT_OK(manager_->Update());
    ImGui::Render();
    gui::WidgetIdRegistry::Instance().EndFrame();
  };
  for (int i = 0; i < 3; ++i)
    frame();
  drawer->OpenDrawer(RightDrawerManager::DrawerType::kSettings);
  for (int i = 0; i < 30; ++i)
    frame();
  for (const char* name :
       {"File", "Edit", "View", "Windows", "Tools", "Help"}) {
    SCOPED_TRACE(name);
    const auto search = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        "Settings/input:Search");
    ASSERT_TRUE(search.has_value());
    ASSERT_TRUE(search->bounds.valid);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((search->bounds.min_x + search->bounds.max_x) / 2,
                        (search->bounds.min_y + search->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    ASSERT_EQ(GImGui->ActiveId, search->imgui_id)
        << "hovered="
        << (GImGui->HoveredWindow ? GImGui->HoveredWindow->Name : "none")
        << " nav=" << (GImGui->NavWindow ? GImGui->NavWindow->Name : "none")
        << " popups=" << GImGui->OpenPopupStack.Size
        << " search=" << search->bounds.min_x << ',' << search->bounds.min_y;
    const auto info = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        std::string("MainMenu/menu:") + name);
    ASSERT_TRUE(info.has_value());
    ASSERT_TRUE(info->bounds.valid);
    io.AddMousePosEvent((info->bounds.min_x + info->bounds.max_x) / 2,
                        (info->bounds.min_y + info->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    for (int i = 0; i < 3; ++i)
      frame();
    ASSERT_EQ(drawer->GetActiveDrawer(),
              RightDrawerManager::DrawerType::kSettings);
    ASSERT_FALSE(GImGui->OpenPopupStack.empty());
    EXPECT_EQ(GImGui->OpenPopupStack[0].PopupId, info->imgui_id);
    // Dismiss back into the drawer. An arbitrary canvas coordinate can raise
    // the synthetic undocked Dashboard over it in this headless host.
    io.AddMousePosEvent((search->bounds.min_x + search->bounds.max_x) / 2,
                        (search->bounds.min_y + search->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
  }
}

TEST_F(AuthoringSaveWorkflowTest,
       OverworldToolbarStaysOneRowAtLaptopDockWidths) {
  const auto fixture = CopyProject("yaze-compact-toolbar");
  Open(fixture);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  for (float dock_width : {320.0f, 480.0f, 680.0f, 980.0f}) {
    SCOPED_TRACE(dock_width);
    gui::WidgetIdRegistry::Instance().Clear();
    gui::WidgetIdRegistry::Instance().BeginFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(dock_width, 600));
    ImGui::Begin("ToolbarLaptopHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    const auto start = ImGui::GetCursorScreenPos();
    const float right = start.x + ImGui::GetContentRegionAvail().x;
    AuthoringSaveWorkflowTestPeer::DrawToolbar(*overworld);
    const auto* window = ImGui::GetCurrentWindow();
    EXPECT_LE(window->DC.CursorMaxPos.x, right);
    EXPECT_LE(window->DC.CursorMaxPos.y - start.y, ImGui::GetFontSize() + 6.0f);
    if (dock_width >= 680.0f) {
      for (const char* tool : {"Select", "Paint", "Fill", "Insert Item",
                               "Entities", "View", "Windows"}) {
        const auto info = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
            std::string("OverworldToolbar/button:") + tool);
        ASSERT_TRUE(info.has_value()) << tool;
        EXPECT_TRUE(info->bounds.valid) << tool;
        EXPECT_LE(info->bounds.max_x, right) << tool;
      }
      EXPECT_FALSE(gui::WidgetIdRegistry::Instance()
                       .GetWidgetInfo("OverworldToolbar/button:Tools")
                       .has_value());
    }
    ImGui::End();
    ImGui::Render();
    gui::WidgetIdRegistry::Instance().EndFrame();
  }
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       NarrowToolbarKeepsAppearanceAndStoryVisibleAndPaintReachable) {
  const auto fixture = CopyProject("yaze-toolbar-overflow");
  Open(fixture);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  overworld->SetEditingMode(EditingMode::MOUSE);
  const auto items_before = overworld->overworld().all_items().size();
  const auto rom_before = ReadFile(fixture.rom);
  float dock_width = 480.0f;
  const auto frame = [&]() {
    gui::WidgetIdRegistry::Instance().BeginFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(dock_width, 600));
    ImGui::Begin("ToolbarInputHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    AuthoringSaveWorkflowTestPeer::DrawToolbar(*overworld);
    ImGui::End();
    ImGui::Render();
    gui::WidgetIdRegistry::Instance().EndFrame();
  };
  const auto click = [&](const char* name) {
    const auto info = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        std::string("OverworldToolbar/") + name);
    ASSERT_TRUE(info.has_value()) << name;
    ASSERT_TRUE(info->bounds.valid);
    const auto b = info->bounds;
    ImGui::GetIO().AddMousePosEvent((b.min_x + b.max_x) / 2,
                                    (b.min_y + b.max_y) / 2);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
    frame();
  };
  frame();
  frame();
  for (const char* id : {"field:Graphics", "field:Palette", "combo:Story"}) {
    const auto info = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        std::string("OverworldToolbar/") + id);
    ASSERT_TRUE(info.has_value()) << id;
    EXPECT_TRUE(info->bounds.valid) << id;
    EXPECT_LT(info->bounds.max_x, 480.0f) << id;
  }
  for (int state : {0, 2, 1}) {
    click("combo:Story");
    click(absl::StrFormat("story:%d", state).c_str());
    EXPECT_EQ(overworld->game_state(), state);
    // Sprite rendering and insertion both use the editor's selected state.
    for (int id : {0, 0x30, 0x40, 0x80}) {
      EXPECT_EQ(overworld->overworld().overworld_map(id)->game_state(), state);
    }
  }
  EXPECT_EQ(overworld->overworld().all_items().size(), items_before);
  EXPECT_EQ(ReadFile(fixture.rom), rom_before);
  click("button:Tools");
  click("button:Paint");
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::EditingModeOf(*overworld),
            EditingMode::DRAW_TILE);

  // Widening the dock restores direct access without another Tools click.
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  frame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  frame();
  dock_width = 680.0f;
  gui::WidgetIdRegistry::Instance().Clear();
  frame();
  frame();
  click("button:Fill");
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::EditingModeOf(*overworld),
            EditingMode::FILL_TILE);
  click("button:Select");
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::EditingModeOf(*overworld),
            EditingMode::MOUSE);
  click("button:View");
  ASSERT_FALSE(GImGui->OpenPopupStack.empty());
  ASSERT_NE(GImGui->OpenPopupStack.back().Window, nullptr);
  EXPECT_TRUE(GImGui->OpenPopupStack.back().Window->Active);

  gui::WidgetIdRegistry::Instance().Clear();
}

TEST_F(AuthoringSaveWorkflowTest, TileEditSurvivesLateSaveFailureRetryAndUndo) {
  const std::string vanilla =
      test::TestRomManager::GetRomPath(test::RomRole::kVanilla);
  ASSERT_FALSE(vanilla.empty())
      << test::TestRomManager::GetRomRoleHint(test::RomRole::kVanilla);
  const std::string vanilla_hash = util::ComputeFileSha1Hex(vanilla);
  ASSERT_FALSE(vanilla_hash.empty());

  root_ = test::UniqueTempPath("yaze_tile_save_retry");
  std::filesystem::create_directories(root_);
  const auto source = root_ / "vanilla.sfc";
  std::filesystem::copy_file(vanilla, source);
  const std::string source_hash = util::ComputeFileSha1Hex(source.string());

  manager_ = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager_, &renderer_);
  manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager_->OpenRomOrProject(source.string()));
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);
  const auto painted =
      AuthoringSaveWorkflowTestPeer::PaintTile(*overworld, 4, 4);
  ASSERT_NE(painted.edited, painted.original);
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
      painted.edited);
  ASSERT_TRUE(overworld->CanUndo());

  const auto destination = root_ / "tile-retry.sfc";
  {
    test::ScopedRomStagingFailure fault;
    const auto failed = manager_->SaveRomAs(destination.string());
    EXPECT_FALSE(failed.ok()) << failed;
  }
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(util::ComputeFileSha1Hex(source.string()), source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(vanilla), vanilla_hash);
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
      painted.edited);

  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_EQ(util::ComputeFileSha1Hex(vanilla), vanilla_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(source.string()), source_hash);

  {
    auto reopened = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*reopened, &renderer_);
    reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
    auto* reopened_overworld = LoadedOverworld(reopened.get());
    ASSERT_NE(reopened_overworld, nullptr);
    EXPECT_EQ(AuthoringSaveWorkflowTestPeer::TileAt(*reopened_overworld,
                                                    painted.x, painted.y),
              painted.edited);
  }

  ASSERT_OK(overworld->Undo());
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
      painted.original);
  ASSERT_OK(manager_->SaveRom());
  EXPECT_EQ(util::ComputeFileSha1Hex(vanilla), vanilla_hash);

  auto undone = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*undone, &renderer_);
  undone->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(undone->OpenRomOrProject(destination.string()));
  auto* undone_overworld = LoadedOverworld(undone.get());
  ASSERT_NE(undone_overworld, nullptr);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::TileAt(*undone_overworld, painted.x,
                                                  painted.y),
            painted.original);
}

TEST_F(AuthoringSaveWorkflowTest, OracleCanvasPatternUndoRedoSaveAsReopens) {
  const auto fixture = CopyProject("yaze_oracle_pattern_save");
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  auto* editor = LoadedOverworld(manager_.get());
  ASSERT_NE(editor, nullptr);
  const auto original = *editor->overworld().mutable_map_tiles();

  // Cross the map 0/1 screen seam with existing Tile16s. This must be one
  // undoable operation even though two destination screens are refreshed.
  constexpr int kX = 31;
  constexpr int kY = 4;
  TileBrush brush{2, 2, {}};
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      const auto old = original.light_world[kX + x][kY + y];
      brush.tile_ids.push_back(old == 0 ? 1 : 0);
    }
  }
  ASSERT_OK(
      AuthoringSaveWorkflowTestPeer::PastePattern(*editor, kX, kY, brush));
  auto expected = original;
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      expected.light_world[kX + x][kY + y] = brush.at(x, y);
    }
  }
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->light_world,
            expected.light_world);
  ASSERT_OK(editor->Undo());
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->light_world,
            original.light_world);
  ASSERT_OK(editor->Redo());
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->light_world,
            expected.light_world);

  const auto source = ReadFile(fixture.rom);
  const auto destination = fixture.root / "oracle-pattern.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      source, ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);

  manager_.reset();
  manager_ = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager_, &renderer_);
  manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager_->OpenRomOrProject(destination.string()));
  auto* reopened = LoadedOverworld(manager_.get());
  ASSERT_NE(reopened, nullptr);
  const auto& actual = *reopened->overworld().mutable_map_tiles();
  EXPECT_EQ(actual.light_world, expected.light_world);
  EXPECT_EQ(actual.dark_world, expected.dark_world);
  EXPECT_EQ(actual.special_world, expected.special_world);
}

// Configure only disposable maps. Existing Oracle maps $86/$87/$8E are
// independent small screens; this fixture does not prescribe island layout.
void AuthoringSaveWorkflowTest::QualifySpecialArea(zelda3::AreaSizeEnum size,
                                                   int child, int x, int y) {
  constexpr int kParent = 0x86;
  const auto fixture = CopyProject("yaze_oracle_special_area_save");
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldProperties);
  auto* editor = LoadedOverworld(manager_.get());
  ASSERT_NE(editor, nullptr);
  auto capture_layout = [](OverworldEditor& current) {
    std::vector<std::pair<int, int>> layout;
    for (int id = 0; id < zelda3::kNumOverworldMaps; ++id) {
      const auto* map = current.overworld().overworld_map(id);
      layout.emplace_back(map->parent(), static_cast<int>(map->area_size()));
    }
    return layout;
  };
  const auto original = *editor->overworld().mutable_map_tiles();
  const auto original_layout = capture_layout(*editor);
  const auto small = static_cast<int>(zelda3::AreaSizeEnum::SmallArea);
  ASSERT_EQ(original_layout[kParent], std::make_pair(kParent, small));
  ASSERT_EQ(original_layout[child], std::make_pair(child, small));
  auto expected_layout = original_layout;
  expected_layout[kParent] = {kParent, static_cast<int>(size)};
  expected_layout[child] = {kParent, static_cast<int>(size)};

  ASSERT_OK(
      AuthoringSaveWorkflowTestPeer::EditAreaSize(*editor, kParent, size));
  EXPECT_EQ(capture_layout(*editor), expected_layout);
  ASSERT_OK(editor->Undo());
  EXPECT_EQ(capture_layout(*editor), original_layout);
  ASSERT_OK(editor->Redo());
  EXPECT_EQ(capture_layout(*editor), expected_layout);

  EXPECT_EQ(editor->overworld().mutable_map_tiles()->light_world,
            original.light_world);
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->dark_world,
            original.dark_world);
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->special_world,
            original.special_world);
  auto expected = original;
  TileBrush brush{2, 2, {}};
  for (int dy = 0; dy < 2; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      const auto old = original.special_world[x + dx][y + dy];
      const uint16_t replacement = old == 0 ? 1 : 0;
      brush.tile_ids.push_back(replacement);
      expected.special_world[x + dx][y + dy] = replacement;
    }
  }
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::PastePattern(*editor, x, y, brush,
                                                        kParent));
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->special_world,
            expected.special_world);
  ASSERT_OK(editor->Undo());
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->special_world,
            original.special_world);
  EXPECT_EQ(capture_layout(*editor), expected_layout);
  ASSERT_OK(editor->Redo());
  EXPECT_EQ(editor->overworld().mutable_map_tiles()->special_world,
            expected.special_world);

  const auto source = ReadFile(fixture.rom);
  const auto destination = fixture.root / "oracle-special-area.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      source, ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);

  manager_.reset();
  manager_ = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager_, &renderer_);
  manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(manager_->OpenRomOrProject(destination.string()));
  auto* reopened = LoadedOverworld(manager_.get());
  ASSERT_NE(reopened, nullptr);
  EXPECT_EQ(capture_layout(*reopened), expected_layout);
  const auto& actual = *reopened->overworld().mutable_map_tiles();
  EXPECT_EQ(actual.light_world, expected.light_world);
  EXPECT_EQ(actual.dark_world, expected.dark_world);
  EXPECT_EQ(actual.special_world, expected.special_world);
}

TEST_F(AuthoringSaveWorkflowTest, OracleWideSpecialAreaCanvasSaveReopens) {
  // $86 -> $87, crossing the east edge at local Tile16 x=31.
  QualifySpecialArea(zelda3::AreaSizeEnum::WideArea, 0x87, 6 * 32 + 31, 4);
}

TEST_F(AuthoringSaveWorkflowTest, OracleTallSpecialAreaCanvasSaveReopens) {
  // $86 -> $8E, crossing the south edge at local Tile16 y=31.
  QualifySpecialArea(zelda3::AreaSizeEnum::TallArea, 0x8E, 6 * 32 + 4, 31);
}

TEST_F(AuthoringSaveWorkflowTest, OracleTileEditRetryKeepsHooksOrStaysRefused) {
  const auto rom = ResolveOracleEditRom();
  ASSERT_FALSE(rom.empty()) << "Oracle edit ROM was not resolved. Set "
                               "YAZE_TEST_ROM_OOS to the base ROM.";
  const auto rom_dir = rom.parent_path();
  const auto project_root = rom_dir.parent_path();
  const auto project = project_root / "Oracle-of-Secrets.yaze";
  const auto manifest = rom_dir / "hack_manifest.json";
  const auto collision = project_root / "Data/dungeons/custom_collision.json";
  ASSERT_TRUE(std::filesystem::exists(project)) << project;
  ASSERT_TRUE(std::filesystem::exists(manifest)) << manifest;
  ASSERT_TRUE(std::filesystem::exists(collision)) << collision;

  const auto fixture = CopyProjectFrom("yaze_oracle_tile_retry", rom, project,
                                       manifest, collision);
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  ASSERT_EQ(manager_->GetProjectRomWritePolicy(),
            project::RomWritePolicy::kBlock);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);
  const auto painted =
      AuthoringSaveWorkflowTestPeer::PaintTile(*overworld, 4, 4);
  ASSERT_NE(painted.edited, painted.original);
  const auto source_bytes = ReadFile(fixture.rom);

  const auto destination = fixture.root / "oracle-tile-retry.sfc";
  {
    test::ScopedRomStagingFailure fault;
    const auto failed = manager_->SaveRomAs(destination.string());
    EXPECT_FALSE(failed.ok()) << failed;
    EXPECT_FALSE(IsProtectionRefusal(failed)) << failed;
  }
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(rom.string()), fixture.shared_hash);
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
      painted.edited);

  const auto saved = manager_->SaveRomAs(destination.string());
  if (IsProtectionRefusal(saved)) {
    manager_->CancelRomWriteConfirm();
    const auto retried = manager_->SaveRomAs(destination.string());
    EXPECT_NE(std::string(retried.message()), "Save pending confirmation")
        << retried;
    EXPECT_TRUE(IsProtectionRefusal(retried)) << retried;
    EXPECT_FALSE(std::filesystem::exists(destination));
    EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
              fixture.source_hash);
    EXPECT_EQ(
        AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
        painted.edited);
    return;
  }
  ASSERT_OK(saved);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(rom.string()), fixture.shared_hash);
  const auto saved_bytes = ReadFile(destination);
  ExpectProtectedRegionsMatch(
      source_bytes, saved_bytes,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());

  {
    auto reopened = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*reopened, &renderer_);
    reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
    auto* reopened_overworld = LoadedOverworld(reopened.get());
    ASSERT_NE(reopened_overworld, nullptr);
    EXPECT_EQ(AuthoringSaveWorkflowTestPeer::TileAt(*reopened_overworld,
                                                    painted.x, painted.y),
              painted.edited);
  }

  ASSERT_OK(overworld->Undo());
  EXPECT_EQ(
      AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
      painted.original);
  const auto undone = manager_->SaveRom();
  if (IsProtectionRefusal(undone)) {
    manager_->CancelRomWriteConfirm();
    const auto retried = manager_->SaveRom();
    EXPECT_TRUE(IsProtectionRefusal(retried)) << retried;
    EXPECT_EQ(
        AuthoringSaveWorkflowTestPeer::TileAt(*overworld, painted.x, painted.y),
        painted.original);
    return;
  }
  ASSERT_OK(undone);
  const auto undone_bytes = ReadFile(destination);
  ExpectProtectedRegionsMatch(
      source_bytes, undone_bytes,
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  auto* reopened_overworld = LoadedOverworld(reopened.get());
  ASSERT_NE(reopened_overworld, nullptr);
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::TileAt(*reopened_overworld,
                                                  painted.x, painted.y),
            painted.original);
}

// Beginning-music slots 0x13..0x33 are PC 0x14316..0x14337, the Overworld hook
// at SNES 0x02C316. Map 0x15 is a self-parent, so the editor writes that slot.
// A light-world map cell does not. OverworldEditor::Save also does not write
// the tile-type table.
constexpr int kProtectedMusicPc = 0x14316;
constexpr int kProtectedMusicEnd = 0x14337;
constexpr int kProtectedSelfParent = 0x15;

bool ConflictCoversPc(const std::vector<core::WriteConflict>& conflicts,
                      uint32_t pc_start, uint32_t pc_end) {
  for (const auto& conflict : conflicts) {
    const uint32_t pc = SnesToPc(conflict.address);
    if (pc >= pc_start && pc < pc_end) {
      return true;
    }
  }
  return false;
}

TEST_F(AuthoringSaveWorkflowTest,
       ProtectedSelfParentMusicWriteStaysRefusedAfterRetry) {
  const auto fixture = CopyProject("yaze_oracle_protected_music_slot");
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);
  const auto* map = overworld->overworld().overworld_map(kProtectedSelfParent);
  ASSERT_NE(map, nullptr);
  ASSERT_EQ(map->parent(), kProtectedSelfParent);
  const int slot = zelda3::kOverworldMusicBeginning + kProtectedSelfParent;
  ASSERT_GE(slot, kProtectedMusicPc);
  ASSERT_LT(slot, kProtectedMusicEnd);

  auto* rom = manager_->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  const uint8_t original = (*rom)[slot];
  const uint8_t edited =
      static_cast<uint8_t>(original == 0xFF ? 0x00 : original + 1);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::EditBeginningMusic(
      *overworld, kProtectedSelfParent, edited));
  EXPECT_EQ(map->area_music(0), edited);
  EXPECT_EQ((*rom)[slot], edited);

  const auto destination = fixture.root / "protected-music-slot.sfc";
  const auto refused = manager_->SaveRomAs(destination.string());
  EXPECT_TRUE(IsProtectionRefusal(refused)) << refused;
  EXPECT_TRUE(ConflictCoversPc(manager_->pending_write_conflicts(),
                               kProtectedMusicPc, kProtectedMusicEnd))
      << "pending conflicts=" << manager_->pending_write_conflicts().size();
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_EQ((*rom)[slot], edited);
  EXPECT_EQ(
      overworld->overworld().overworld_map(kProtectedSelfParent)->area_music(0),
      edited);

  const auto pending = manager_->SaveRomAs(destination.string());
  EXPECT_EQ(std::string(pending.message()), "Save pending confirmation")
      << pending;

  manager_->CancelRomWriteConfirm();
  const auto retried = manager_->SaveRomAs(destination.string());
  EXPECT_NE(std::string(retried.message()), "Save pending confirmation")
      << retried;
  EXPECT_TRUE(IsProtectionRefusal(retried)) << retried;
  EXPECT_TRUE(ConflictCoversPc(manager_->pending_write_conflicts(),
                               kProtectedMusicPc, kProtectedMusicEnd));
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_EQ((*rom)[slot], edited);
}

TEST_F(AuthoringSaveWorkflowTest,
       ParentMusicSaveReopenPreservesChildHookBytes) {
  const auto fixture = CopyProject("yaze_oracle_parent_music");
  Open(fixture);
  ASSERT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldMaps);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);

  constexpr int kMusicBase = zelda3::kOverworldMusicBeginning;
  constexpr int kHookStart = 0x14316;
  constexpr int kHookEnd = 0x14337;
  int parent_map = -1;
  int child_map = -1;
  for (int map_id = 0; map_id < zelda3::kDarkWorldMapIdStart; ++map_id) {
    const auto* map = overworld->overworld().overworld_map(map_id);
    ASSERT_NE(map, nullptr);
    const int slot = kMusicBase + map_id;
    const int parent = map->parent();
    if (parent == map_id || parent < 0 ||
        parent >= zelda3::kDarkWorldMapIdStart || slot < kHookStart ||
        slot >= kHookEnd) {
      continue;
    }
    const int parent_slot = kMusicBase + parent;
    if (parent_slot < kHookStart || parent_slot >= kHookEnd) {
      parent_map = parent;
      child_map = map_id;
      break;
    }
  }
  ASSERT_GE(parent_map, 0);
  ASSERT_GE(child_map, 0);
  ASSERT_EQ(overworld->overworld().overworld_map(child_map)->parent(),
            parent_map);
  ASSERT_EQ(overworld->overworld().overworld_map(parent_map)->parent(),
            parent_map);
  RecordProperty("parent_map", parent_map);
  RecordProperty("child_map", child_map);
  const auto before = ReadFile(fixture.rom);
  const uint8_t child_hook = before[kMusicBase + child_map];
  const uint8_t parent_music = before[kMusicBase + parent_map];
  const uint8_t edited_music =
      static_cast<uint8_t>(parent_music == 0x03 ? 0x02 : 0x03);
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::EditBeginningMusic(
      *overworld, parent_map, edited_music));
  EXPECT_EQ(overworld->overworld().overworld_map(parent_map)->area_music(0),
            edited_music);
  EXPECT_EQ((*manager_->GetCurrentRom())[kMusicBase + child_map], child_hook);

  ASSERT_OK(manager_->SaveRom());
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  const auto saved = ReadFile(fixture.rom);
  ASSERT_EQ(saved.size(), before.size());
  EXPECT_EQ(saved[kMusicBase + parent_map], edited_music);
  EXPECT_TRUE(std::equal(before.begin() + kHookStart, before.begin() + kHookEnd,
                         saved.begin() + kHookStart));
  EXPECT_EQ(saved[kMusicBase + child_map], child_hook);

  auto reopened = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(fixture.rom.string()));
  auto* reopened_overworld = LoadedOverworld(reopened.get());
  ASSERT_NE(reopened_overworld, nullptr);
  ASSERT_EQ(reopened_overworld->overworld().overworld_map(child_map)->parent(),
            parent_map);
  EXPECT_EQ(
      reopened_overworld->overworld().overworld_map(child_map)->area_music(0),
      edited_music);
  EXPECT_EQ(
      reopened_overworld->overworld().overworld_map(parent_map)->area_music(0),
      edited_music);
  EXPECT_EQ((*reopened->GetCurrentRom())[kMusicBase + child_map], child_hook);
  const auto reopened_bytes = ReadFile(fixture.rom);
  EXPECT_TRUE(std::equal(before.begin() + kHookStart, before.begin() + kHookEnd,
                         reopened_bytes.begin() + kHookStart));
}

TEST_F(AuthoringSaveWorkflowTest,
       RevertRefusalKeepsPendingDungeonAndGraphicsEdits) {
  const auto fixture = CopyProject("yaze_failed_authoring_revert");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
  auto* editors = manager_->GetCurrentEditorSet();
  auto* dungeon = editors->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  auto* graphics = editors->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  constexpr int kRoom = 5;
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  AuthoringSaveWorkflowTestPeer::ActivateDungeonRoom(*dungeon, kRoom);
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  auto& room = dungeon->rooms()[kRoom];
  ASSERT_FALSE(room.GetTileObjects().empty());
  const auto object = room.GetTileObjects().front();
  const int dx = object.x_ >= 63 ? -1 : 1;
  viewer->object_interaction().SetCurrentRoom(&dungeon->rooms(), kRoom);
  viewer->object_interaction().entity_coordinator().tile_handler().MoveObjects(
      kRoom, {0}, dx, 0);
  ASSERT_TRUE(dungeon->CanUndo());
  ASSERT_EQ(room.GetTileObjects().front().x_, object.x_ + dx);

  auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  for (const auto& candidate : room.GetTileObjects()) {
    AuthoringSaveWorkflowTestPeer::OpenObjectGraphics(*dungeon, kRoom,
                                                      candidate);
    if (state.SourceTarget())
      break;
  }
  ASSERT_TRUE(state.SourceTarget());
  const auto target = *state.SourceTarget();
  const int x = (target.tile_index % 16) * 8;
  const int y = (target.tile_index / 16) * 8;
  const int offset = y * 128 + x;
  const auto original =
      state.game_data->sheet_store.Sheet(target.sheet_id)[offset];
  const uint8_t changed = original == 1 ? 2 : 1;
  AuthoringSaveWorkflowTestPeer::PaintGraphicsPixel(*graphics, x, y, changed);
  ASSERT_TRUE(graphics->HasPendingGraphicsChanges());
  ASSERT_TRUE(graphics->CanUndo());
  const auto raw = manager_->GetCurrentRom()->vector();
  const auto identity = manager_->GetCurrentRomHash();
  const auto project_path = manager_->GetCurrentProject()->filepath;

  // A loadable ROM header whose missing palette data fails asset loading.
  auto invalid = raw;
  invalid.resize(512 * 1024);
  {
    std::ofstream out(fixture.rom, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(invalid.data()), invalid.size());
    ASSERT_TRUE(out.good());
  }
  const auto disk_hash = util::ComputeFileSha1Hex(fixture.rom.string());
  ASSERT_OK(manager_->RevertRomToSaved());
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_FALSE(manager_->HasPendingUnsavedSessionAction());
  const auto* toast = manager_->toast_manager()->GetPendingToast(
      manager_->toast_manager()->pending_count() - 1);
  ASSERT_NE(toast, nullptr);
  EXPECT_EQ(toast->type, ToastType::kError);
  EXPECT_NE(toast->message.find("Revert failed:"), std::string::npos);
  // The existing graphics-draft guard refuses before attempting asset reload.
  EXPECT_NE(toast->message.find("unsaved sheet edits"), std::string::npos)
      << toast->message;
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), raw);
  EXPECT_EQ(manager_->GetCurrentRomHash(), identity);
  EXPECT_EQ(manager_->GetCurrentProject()->filepath, project_path);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()), disk_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_ + dx);
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id)[offset],
            changed);
  EXPECT_TRUE(graphics->HasPendingGraphicsChanges());
  EXPECT_TRUE(dungeon->CanUndo());
  EXPECT_TRUE(graphics->CanUndo());
  // Exercise both surviving histories, rather than checking only stack size.
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_ + dx);
  ASSERT_OK(graphics->Undo());
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id)[offset],
            original);
  ASSERT_OK(graphics->Redo());
  EXPECT_EQ(state.game_data->sheet_store.Sheet(target.sheet_id)[offset],
            changed);
  ASSERT_TRUE(state.source_context);
  ASSERT_TRUE(state.source_context->return_to_source);
  state.source_context->return_to_source();
}

TEST_F(AuthoringSaveWorkflowTest, FailedRevertKeepsDungeonObjectAndUndo) {
  const auto fixture = CopyProject("yaze_failed_dungeon_revert");
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
      EditorType::kDungeon);
  constexpr int kRoom = 5;
  ASSERT_OK(AuthoringSaveWorkflowTestPeer::LoadDungeonRoom(*dungeon, kRoom));
  AuthoringSaveWorkflowTestPeer::ActivateDungeonRoom(*dungeon, kRoom);
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  auto& room = dungeon->rooms()[kRoom];
  ASSERT_FALSE(room.GetTileObjects().empty());
  const auto object = room.GetTileObjects().front();
  const int dx = object.x_ >= 63 ? -1 : 1;
  viewer->object_interaction().SetCurrentRoom(&dungeon->rooms(), kRoom);
  viewer->object_interaction().entity_coordinator().tile_handler().MoveObjects(
      kRoom, {0}, dx, 0);
  ASSERT_EQ(room.GetTileObjects().front().x_, object.x_ + dx);
  ASSERT_TRUE(dungeon->CanUndo());
  const auto raw = manager_->GetCurrentRom()->vector();
  const auto identity = manager_->GetCurrentRomHash();
  const auto project_path = manager_->GetCurrentProject()->filepath;
  auto invalid = raw;
  invalid.resize(512 * 1024);
  {
    std::ofstream out(fixture.rom, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(invalid.data()), invalid.size());
    ASSERT_TRUE(out.good());
  }
  const auto disk_hash = util::ComputeFileSha1Hex(fixture.rom.string());
  ASSERT_OK(manager_->RevertRomToSaved());
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_FALSE(manager_->HasPendingUnsavedSessionAction());
  const auto* toast = manager_->toast_manager()->GetPendingToast(
      manager_->toast_manager()->pending_count() - 1);
  ASSERT_NE(toast, nullptr);
  EXPECT_EQ(toast->type, ToastType::kError);
  EXPECT_NE(toast->message.find("palette"), std::string::npos)
      << toast->message;
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), raw);
  EXPECT_EQ(manager_->GetCurrentRomHash(), identity);
  EXPECT_EQ(manager_->GetCurrentProject()->filepath, project_path);
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()), disk_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(kSharedRom), fixture.shared_hash);
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_ + dx);
  ASSERT_TRUE(dungeon->CanUndo());
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(room.GetTileObjects().front().x_, object.x_ + dx);
}

// Opt-in rehearsal on a complete, disposable copy of the real authoring project.
// This leaves artifacts for a separate Build Project and emulator acceptance.
TEST_F(AuthoringSaveWorkflowTest, GoronRouteRedesignSaveAsReopen) {
  const char* input = std::getenv("YAZE_GORON_ROUTE_PROJECT");
  if (!input || !*input)
    GTEST_SKIP()
        << "Set YAZE_GORON_ROUTE_PROJECT to a disposable route project";
  const auto project_path = std::filesystem::weakly_canonical(input);
  ASSERT_NE(project_path.string().find("goron_route_redesign_"),
            std::string::npos);
  constexpr int kRoom = 0x89;
  constexpr int kTrack = 10;
  const auto root = project_path.parent_path();
  DisposableOracleProject fixture;
  fixture.root = root;
  fixture.project = project_path;
  fixture.rom = root / "Roms/oos168.sfc";
  Open(fixture);
  const auto rom_before = ReadFile(fixture.rom);
  const auto collision_path = root / "Data/dungeons/custom_collision.json";
  const auto source_path = root / "Sprites/Objects/data/minecart_tracks.asm";
  const auto source_before = ReadFile(source_path);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  auto* dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  dungeon->add_room(kRoom);
  auto& room = dungeon->rooms()[kRoom];
  room.EnsureObjectsLoaded();
  room.EnsureSpritesLoaded();
  const auto collision_before = room.custom_collision();
  const auto objects_before = room.GetTileObjects();
  const auto sprites_before = room.GetSprites();
  auto panel_or = AuthoringSaveWorkflowTestPeer::MinecartPanel(*dungeon);
  ASSERT_OK(panel_or.status());
  auto* panel = *panel_or;
  ASSERT_OK(panel->ReloadTracks());
  const auto tracks_before = panel->GetTracks();
  ASSERT_EQ(tracks_before.at(kTrack).start_x, 0x1270);
  ASSERT_EQ(tracks_before.at(kTrack).start_y, 0x1110);
  ASSERT_EQ(collision_before.tiles[34 * 64 + 14], 0xB8);
  AuthoringSaveWorkflowTestPeer::ActivateDungeonRoom(*dungeon, kRoom);
  auto* viewer = AuthoringSaveWorkflowTestPeer::Viewer(*dungeon, kRoom);
  ASSERT_NE(viewer, nullptr);
  viewer->SetHeaderVisible(false);
  viewer->SetHeaderHiddenMetadataHudVisible(false);
  viewer->canvas().set_global_scale(1.0f);
  viewer->canvas().set_scrolling(ImVec2(0, 0));
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280, 900);
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  ImGuiID stamp_id = 0, behavior_id = 0, tracks_id = 0;
  std::array<ImGuiID, 16> art_ids{};
  auto frame = [&](bool focus_tools = false) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 870), ImGuiCond_Always);
    if (focus_tools)
      ImGui::SetNextWindowFocus();
    ImGui::Begin("RouteTools", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushID("MinecartRailPalette");
    for (int i = 0; i < 16; ++i)
      art_ids[i] = ImGui::GetID(absl::StrFormat("##Rail%d", i).c_str());
    ImGui::PopID();
    stamp_id = ImGui::GetID("Stamp rail behavior");
    behavior_id = ImGui::GetID("##RailBehavior");
    panel->DrawRailTools();
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(320, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(940, 870), ImGuiCond_Always);
    ImGui::Begin("RouteCanvas", nullptr, ImGuiWindowFlags_NoSavedSettings);
    tracks_id = ImGui::GetID("Tracks##canvas_tracks");
    viewer->DrawDungeonCanvas(kRoom);
    ImGui::End();
    ImGui::Render();
  };
  auto key = [&](ImGuiKey k) {
    io.AddKeyEvent(k, true);
    frame();
    io.AddKeyEvent(k, false);
    frame();
  };
  auto activate = [&](ImGuiID id, bool focus_tools = true) {
    frame(focus_tools);
    for (int step = 0; step < 80 && ImGui::GetCurrentContext()->NavId != id;
         ++step)
      key(ImGuiKey_Tab);
    ASSERT_EQ(ImGui::GetCurrentContext()->NavId, id);
    key(ImGuiKey_Space);
  };
  auto point = [&](int x, int y, int offset = 1) {
    const auto& canvas = viewer->canvas();
    io.AddMousePosEvent(canvas.zero_point().x + canvas.scrolling().x +
                            (x * 8 + offset) * canvas.global_scale(),
                        canvas.zero_point().y + canvas.scrolling().y +
                            (y * 8 + offset) * canvas.global_scale());
    frame();
  };
  auto click = [&] {
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
  };
  frame();
  frame(true);
  auto click_widget = [&](const char* label) {
    frame();
    std::optional<gui::WidgetIdRegistry::WidgetInfo> widget;
    for (const auto& [path, info] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets())
      if (info.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        widget = info;
    ASSERT_TRUE(widget.has_value()) << label;
    io.AddMousePosEvent((widget->bounds.min_x + widget->bounds.max_x) / 2,
                        (widget->bounds.min_y + widget->bounds.max_y) / 2);
    frame();
    click();
    frame();
  };
  auto choose_behavior = [&](const char* label, uint8_t value) {
    ASSERT_NO_FATAL_FAILURE(click_widget("RailBehavior"));
    auto* popup = ImGui::FindWindowByName("##Combo_00");
    ASSERT_NE(popup, nullptr);
    ASSERT_TRUE(popup->Active);
    const auto target = popup->GetID(label);
    for (int step = 0; step < 26 && ImGui::GetCurrentContext()->NavId != target;
         ++step)
      key(ImGuiKey_DownArrow);
    ASSERT_EQ(ImGui::GetCurrentContext()->NavId, target) << label;
    frame();
    ASSERT_NO_FATAL_FAILURE(click_widget(label));
    ASSERT_EQ(panel->collision_stamp_value(), value);
    ASSERT_TRUE(panel->collision_stamping());
    std::cout << "behavior=" << label
              << " value=" << static_cast<int>(panel->collision_stamp_value())
              << " active=" << panel->collision_stamping() << std::endl;
  };

  // Turn the original south stop into a curved spur, retaining the north route.
  ASSERT_NO_FATAL_FAILURE(click_widget("Stamp rail behavior"));
  ASSERT_TRUE(panel->collision_stamping());
  ASSERT_NO_FATAL_FAILURE(choose_behavior("Erase rail behavior", 0));
  point(14, 34);
  click();
  EXPECT_EQ(room.custom_collision().tiles[34 * 64 + 14], 0);
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(room.custom_collision().tiles, collision_before.tiles);
  ASSERT_OK(dungeon->Redo());
  ASSERT_NO_FATAL_FAILURE(choose_behavior("Corner: right / up", 0xB3));
  point(14, 34);
  click();
  ASSERT_NO_FATAL_FAILURE(choose_behavior("Straight: horizontal", 0xB0));
  point(16, 34);
  click();
  point(18, 34);
  click();
  ASSERT_NO_FATAL_FAILURE(choose_behavior("Stop: depart west", 0xBA));
  point(20, 34);
  click();
  auto expected_collision = collision_before;
  for (int y = 34; y < 36; ++y)
    for (int x = 14; x < 22; ++x)
      expected_collision.tiles[y * 64 + x] = x < 16   ? 0xB3
                                             : x < 20 ? 0xB0
                                                      : 0xBA;
  for (size_t i = 0; i < expected_collision.tiles.size(); ++i)
    if (room.custom_collision().tiles[i] != expected_collision.tiles[i])
      std::cout << "collision mismatch " << i % 64 << "," << i / 64
                << " actual="
                << static_cast<int>(room.custom_collision().tiles[i])
                << " expected=" << static_cast<int>(expected_collision.tiles[i])
                << std::endl;
  ASSERT_EQ(room.custom_collision().tiles, expected_collision.tiles);
  key(ImGuiKey_Escape);
  ASSERT_FALSE(panel->collision_stamping());

  // Existing object-delete action, then real palette clicks and canvas stamps.
  auto& handler =
      viewer->object_interaction().entity_coordinator().tile_handler();
  size_t old_art = objects_before.size();
  for (size_t i = 0; i < objects_before.size(); ++i)
    if (objects_before[i].id_ == 0x31 && objects_before[i].x_ == 14 &&
        objects_before[i].y_ == 34)
      old_art = i;
  ASSERT_LT(old_art, objects_before.size());
  ASSERT_TRUE(handler.DeleteObjects(kRoom, {old_art}));
  ASSERT_OK(dungeon->Undo());
  ASSERT_EQ(room.GetTileObjects().size(), objects_before.size());
  ASSERT_OK(dungeon->Redo());
  for (const auto& [subtype, x] : std::array<std::pair<int, int>, 4>{
           {{4, 14}, {0, 16}, {0, 18}, {0, 20}}}) {
    ASSERT_NO_FATAL_FAILURE(
        click_widget(absl::StrFormat("Rail%d", subtype).c_str()));
    const auto count = room.GetTileObjects().size();
    point(x, 34);
    click();
    ASSERT_EQ(room.GetTileObjects().size(), count + 1);
    const auto& placed = room.GetTileObjects().back();
    EXPECT_EQ(placed.id_, 0x31);
    EXPECT_EQ(placed.size_, subtype);
    EXPECT_EQ(placed.x_, x);
    EXPECT_EQ(placed.y_, 34);
    ASSERT_OK(dungeon->Undo());
    EXPECT_EQ(room.GetTileObjects().size(), count);
    ASSERT_OK(dungeon->Redo());
  }

  // Enable Tracks through the same control a designer uses, then drag the pair.
  point(14, 34, 8);
  click();  // Focus the canvas after leaving one-shot artwork placement.
  ASSERT_NO_FATAL_FAILURE(activate(tracks_id, false));
  ASSERT_TRUE(panel->canvas_editing());
  point(14, 34, 8);
  io.AddMouseButtonEvent(0, true);
  frame();
  point(20, 34, 8);
  io.AddMouseButtonEvent(0, false);
  frame();
  const auto revised = panel->GetTracks().at(kTrack);
  EXPECT_EQ(revised.start_x, 0x12A0);
  EXPECT_EQ(revised.start_y, 0x1110);
  ASSERT_OK(dungeon->Undo());
  EXPECT_EQ(panel->GetTracks(), tracks_before);
  ASSERT_OK(dungeon->Redo());
  EXPECT_EQ(panel->GetTracks().at(kTrack), revised);
  ASSERT_OK(panel->SaveTracks());
  EXPECT_NE(ReadFile(source_path), source_before);
  const auto published = ReadFile(source_path);
  const auto parsed = MinecartTrackSourceDocument::Parse(
      std::string(published.begin(), published.end()));
  ASSERT_OK(parsed.status());
  EXPECT_EQ(parsed->tracks(), panel->GetTracks());
  auto expected_tracks = tracks_before;
  expected_tracks.at(kTrack) = revised;
  EXPECT_EQ(panel->GetTracks(), expected_tracks);
  auto original_cart = sprites_before.size();
  for (size_t i = 0; i < sprites_before.size(); ++i)
    if (sprites_before[i].id() == 0xA3 &&
        sprites_before[i].subtype() == kTrack && sprites_before[i].x() == 7 &&
        sprites_before[i].y() == 17) {
      ASSERT_EQ(original_cart, sprites_before.size());
      original_cart = i;
    }
  ASSERT_LT(original_cart, sprites_before.size());
  EXPECT_EQ(sprites_before[original_cart].x(), 7);
  EXPECT_EQ(sprites_before[original_cart].y(), 17);
  ASSERT_EQ(room.GetSprites().size(), sprites_before.size());
  for (size_t i = 0; i < sprites_before.size(); ++i) {
    const auto& actual = room.GetSprites()[i];
    const auto& before = sprites_before[i];
    EXPECT_EQ(actual.id(), before.id());
    EXPECT_EQ(actual.subtype(), before.subtype());
    EXPECT_EQ(actual.layer(), before.layer());
    EXPECT_EQ(actual.key_drop(), before.key_drop());
    EXPECT_EQ(actual.deleted(), before.deleted());
    EXPECT_EQ(actual.x(), i == original_cart ? 10 : before.x());
    EXPECT_EQ(actual.y(), before.y());
  }
  ASSERT_EQ(room.GetTileObjects().size(), objects_before.size() + 3);
  for (size_t i = 0, j = 0; i < objects_before.size(); ++i) {
    if (i == old_art)
      continue;
    const auto& before = objects_before[i];
    const auto& actual = room.GetTileObjects()[j++];
    EXPECT_EQ(std::make_tuple(actual.id_, actual.x_, actual.y_, actual.size_,
                              actual.layer_),
              std::make_tuple(before.id_, before.x_, before.y_, before.size_,
                              before.layer_));
  }
  const auto expected_objects = room.GetTileObjects();
  const auto expected_sprites = room.GetSprites();

  // Commit collision source together with its bound disposable ROM. Save As
  // then rebinds the project to the copy that Build Project will consume.
  ASSERT_OK(manager_->SaveRom());
  const auto canonical =
      zelda3::DumpCustomCollisionSourceFromRom(manager_->GetCurrentRom());
  ASSERT_OK(canonical.status());
  const auto published_collision = ReadFile(collision_path);
  EXPECT_EQ(std::string(published_collision.begin(), published_collision.end()),
            *canonical);
  const auto destination = root / "Roms/goron-route.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ExpectProtectedRegionsMatch(
      rom_before, ReadFile(destination),
      manager_->GetCurrentProject()->hack_manifest.protected_regions());
  ASSERT_OK(manager_->SwapProjectRom(destination.string()));
  manager_.reset();
  Open(fixture);
  ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  dungeon = manager_->GetCurrentEditorSet()->GetDungeonEditor();
  dungeon->add_room(kRoom);
  auto& fresh = dungeon->rooms()[kRoom];
  fresh.EnsureObjectsLoaded();
  fresh.EnsureSpritesLoaded();
  EXPECT_EQ(fresh.custom_collision().tiles, expected_collision.tiles);
  ASSERT_EQ(fresh.GetTileObjects().size(), expected_objects.size());
  for (size_t i = 0; i < expected_objects.size(); ++i) {
    const auto& expected = expected_objects[i];
    const auto& actual = fresh.GetTileObjects()[i];
    EXPECT_EQ(std::make_tuple(actual.id_, actual.x_, actual.y_, actual.size_,
                              actual.layer_),
              std::make_tuple(expected.id_, expected.x_, expected.y_,
                              expected.size_, expected.layer_));
  }
  ASSERT_EQ(fresh.GetSprites().size(), expected_sprites.size());
  for (size_t i = 0; i < expected_sprites.size(); ++i) {
    const auto& expected = expected_sprites[i];
    const auto& actual = fresh.GetSprites()[i];
    EXPECT_EQ(std::make_tuple(actual.id(), actual.x(), actual.y(),
                              actual.subtype(), actual.layer()),
              std::make_tuple(expected.id(), expected.x(), expected.y(),
                              expected.subtype(), expected.layer()));
  }
  panel_or = AuthoringSaveWorkflowTestPeer::MinecartPanel(*dungeon);
  ASSERT_OK(panel_or.status());
  ASSERT_OK((*panel_or)->ReloadTracks());
  EXPECT_EQ((*panel_or)->GetTracks(), parsed->tracks());
  EXPECT_EQ(ReadFile(collision_path), published_collision);
  EXPECT_EQ(ReadFile(source_path), published);
  std::ofstream(root / "route-authoring-passed.txt")
      << "room=089 track=10 start=12A0,1110 corner=14,34/B3 stop=20,34/BA\n";
}

// Invoked separately so the product build runner can acquire the heavy-job lock.
TEST_F(AuthoringSaveWorkflowTest, GoronRouteSavedProjectBuild) {
  const char* input = std::getenv("YAZE_GORON_ROUTE_PROJECT");
  if (!input || !*input)
    GTEST_SKIP()
        << "Set YAZE_GORON_ROUTE_PROJECT to the authored disposable project";
  const auto project_path = std::filesystem::weakly_canonical(input);
  ASSERT_NE(project_path.string().find("goron_route_redesign_"),
            std::string::npos);
  const auto root = project_path.parent_path();
  ASSERT_TRUE(std::filesystem::exists(root / "route-authoring-passed.txt"));
  DisposableOracleProject fixture;
  fixture.root = root;
  fixture.project = project_path;
  Open(fixture);
  const auto* project = manager_->GetCurrentProject();
  const auto rom = project->GetAbsolutePath(project->rom_filename);
  const auto before = ReadFile(rom);
  ASSERT_OK(manager_->BuildCurrentProject());
  EXPECT_EQ(ReadFile(rom), before);
  const auto output = project->GetAbsolutePath(project->build_target);
  ASSERT_TRUE(std::filesystem::exists(output));
  Rom compiled;
  ASSERT_OK(compiled.LoadFromFile(output));
  auto room = zelda3::LoadRoomFromRom(&compiled, 0x89);
  room.EnsureObjectsLoaded();
  room.EnsureSpritesLoaded();
  EXPECT_EQ(room.custom_collision().tiles[34 * 64 + 14], 0xB3);
  EXPECT_EQ(room.custom_collision().tiles[34 * 64 + 16], 0xB0);
  EXPECT_EQ(room.custom_collision().tiles[34 * 64 + 20], 0xBA);
  EXPECT_EQ(std::count_if(room.GetSprites().begin(), room.GetSprites().end(),
                          [](const auto& sprite) {
                            return sprite.id() == 0xA3 &&
                                   sprite.subtype() == 10 && sprite.x() == 10 &&
                                   sprite.y() == 17;
                          }),
            1);
  std::cout << "Authored runtime ROM: " << output << std::endl;
}

}  // namespace
}  // namespace yaze::editor
