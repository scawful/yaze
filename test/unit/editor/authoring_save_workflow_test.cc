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
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/minecart_track_source.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gui/automation/widget_id_registry.h"
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

  static absl::Status EditAreaGraphics(OverworldEditor& editor, int map_id,
                                       int group) {
    return editor.ApplyOverworldPropertyEdit(
        {map_id, OverworldPropertyField::kAreaGraphics, 0, group});
  }

  static EditingMode EditingModeOf(OverworldEditor& editor) {
    return editor.current_mode;
  }

  static void DrawToolbar(
      OverworldEditor& editor, const std::function<void()>& appearance = []() {
        ImGui::TextUnformatted("Appearance");
      }) {
    editor.toolbar_->Draw(
        editor.current_world_, editor.current_map_, editor.current_map_lock_,
        editor.current_mode, editor.entity_edit_mode_,
        editor.dependencies_.window_manager, editor.rom_, &editor.overworld_,
        editor.dependencies_.project, editor.game_state_, appearance);
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
    if (!std::filesystem::exists(kSharedRom) ||
        !std::filesystem::exists(kSharedProject) ||
        !std::filesystem::exists(kSharedManifest) ||
        !std::filesystem::exists(kSharedCollision)) {
      GTEST_SKIP() << "Oracle base ROM/project fixture is not available";
    }
    original_flags_ = core::FeatureFlags::get();
  }

  void TearDown() override {
    manager_.reset();
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
  ScopedImGuiContext imgui_;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
};

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
  for (int y = 0; y < 32 && !opened.ok(); ++y) {
    for (int x = 0; x < 32 && !opened.ok(); ++x) {
      const auto tile = world.GetMapTiles(0)[x][y];
      for (int quadrant = 0; quadrant < 4 && !opened.ok(); ++quadrant) {
        opened = overworld->OpenGraphicsForTile16Quadrant(tile, quadrant);
      }
    }
  }
  ASSERT_OK(opened);
  auto* graphics = manager_->GetCurrentEditorSet()->GetGraphicsEditor();
  const auto& state = AuthoringSaveWorkflowTestPeer::GraphicsState(*graphics);
  ASSERT_TRUE(state.source_context);
  EXPECT_EQ(state.source_context->resolved.id, 0);
  EXPECT_EQ(state.source_context->resolved.slots.size(), 16u);
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
    EXPECT_NE(text.find("Drawing: " + context.drawing_description),
              std::string::npos);
    EXPECT_NE(text.find("Pattern orientations: " +
                        state.SourceTarget()->SourceOrientationLabel()),
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
  // These two entrances agree in Oracle's active and legacy table copies.
  // This regression isolates preview persistence from table-format decoding.
  // Their slot-2 sheets are architectural walls (13) and rock/cave walls (12).
  for (const auto [entrance, main, wall_sheet] :
       {std::tuple{0x00, 0x03, 0x13}, std::tuple{0x06, 0x06, 0x12},
        std::tuple{0x00, 0x03, 0x13}}) {
    AuthoringSaveWorkflowTestPeer::SelectDungeonEntrance(
        *dungeon, zelda3::kNumDungeonSpawnPoints + entrance);
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
  EXPECT_EQ(ReadFile(patterns_path), ReadFile(source));
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
    ImGui::End();
    ImGui::Render();
  }
  EXPECT_EQ(util::ComputeFileSha1Hex(fixture.rom.string()),
            fixture.source_hash);
}

TEST_F(AuthoringSaveWorkflowTest,
       NarrowToolbarKeepsPaintAndAppearanceReachable) {
  const auto fixture = CopyProject("yaze-toolbar-overflow");
  Open(fixture);
  auto* overworld = LoadedOverworld(manager_.get());
  ASSERT_NE(overworld, nullptr);
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  overworld->SetEditingMode(EditingMode::MOUSE);
  int appearance_frames = 0;
  const auto frame = [&]() {
    gui::WidgetIdRegistry::Instance().BeginFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(480, 600));
    ImGui::Begin("ToolbarInputHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    AuthoringSaveWorkflowTestPeer::DrawToolbar(*overworld, [&]() {
      ++appearance_frames;
      ImGui::TextUnformatted("Appearance controls");
    });
    ImGui::End();
    ImGui::Render();
    gui::WidgetIdRegistry::Instance().EndFrame();
  };
  const auto click = [&](const char* name) {
    const auto info = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        std::string("OverworldToolbar/button:") + name);
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
  EXPECT_EQ(appearance_frames, 0);
  click("Tools");
  click("Paint");
  EXPECT_EQ(AuthoringSaveWorkflowTestPeer::EditingModeOf(*overworld),
            EditingMode::DRAW_TILE);
  click("Appearance");
  EXPECT_GT(appearance_frames, 0);
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

}  // namespace
}  // namespace yaze::editor
