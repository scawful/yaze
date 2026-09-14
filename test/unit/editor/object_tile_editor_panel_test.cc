#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

struct ObjectTileEditorPanelTestAccess {
  static bool HasLayout(const ObjectTileEditorPanel& panel) {
    return !panel.current_layout_.cells.empty();
  }

  static int SelectedCellIndex(const ObjectTileEditorPanel& panel) {
    return panel.selected_cell_index_;
  }

  static int SelectedSourceTile(const ObjectTileEditorPanel& panel) {
    return panel.selected_source_tile_;
  }

  static int SourcePalette(const ObjectTileEditorPanel& panel) {
    return panel.source_palette_;
  }

  static bool AtlasDirty(const ObjectTileEditorPanel& panel) {
    return panel.atlas_dirty_;
  }

  static bool PreviewDirty(const ObjectTileEditorPanel& panel) {
    return panel.preview_dirty_;
  }

  static bool ShowSharedConfirm(const ObjectTileEditorPanel& panel) {
    return panel.show_shared_confirm_;
  }

  static int SharedObjectCount(const ObjectTileEditorPanel& panel) {
    return panel.shared_object_count_;
  }

  static bool HasActionStatus(const ObjectTileEditorPanel& panel) {
    return !panel.action_status_message_.empty();
  }

  static bool ActionStatusIsNone(const ObjectTileEditorPanel& panel) {
    return panel.action_status_tone_ ==
           ObjectTileEditorPanel::ActionStatusTone::kNone;
  }

  static bool ActionStatusIsWarning(const ObjectTileEditorPanel& panel) {
    return panel.action_status_tone_ ==
           ObjectTileEditorPanel::ActionStatusTone::kWarning;
  }

  static bool ActionStatusIsSuccess(const ObjectTileEditorPanel& panel) {
    return panel.action_status_tone_ ==
           ObjectTileEditorPanel::ActionStatusTone::kSuccess;
  }

  static bool ActionStatusIsError(const ObjectTileEditorPanel& panel) {
    return panel.action_status_tone_ ==
           ObjectTileEditorPanel::ActionStatusTone::kError;
  }

  static const std::string& ActionStatusMessage(
      const ObjectTileEditorPanel& panel) {
    return panel.action_status_message_;
  }

  static int CurrentRoomId(const ObjectTileEditorPanel& panel) {
    return panel.current_room_id_;
  }

  static int CurrentObjectId(const ObjectTileEditorPanel& panel) {
    return panel.current_object_id_;
  }

  static uint16_t CurrentPaletteColor(const ObjectTileEditorPanel& panel,
                                      int palette, int color) {
    return panel.current_palette_group_.GetColor(palette, color).snes();
  }

  static void SetCurrentObjectId(ObjectTileEditorPanel& panel,
                                 int16_t object_id) {
    panel.current_object_id_ = object_id;
  }

  static void SetOpen(ObjectTileEditorPanel& panel, bool open) {
    panel.is_open_ = open;
  }

  static void SetSharedTileDataUsageOverride(ObjectTileEditorPanel& panel,
                                             int shared_count) {
    panel.shared_tile_data_usage_override_ = shared_count;
  }

  static int SharedTileDataUsageOverride(const ObjectTileEditorPanel& panel) {
    return panel.shared_tile_data_usage_override_;
  }

  static const DungeonRoomStore* Rooms(const ObjectTileEditorPanel& panel) {
    return panel.rooms_;
  }

  static bool HasActivePreview(const ObjectTileEditorPanel& panel) {
    return panel.object_preview_bmp_.is_active();
  }

  static bool HasActiveAtlas(const ObjectTileEditorPanel& panel) {
    return panel.tile8_atlas_bmp_.is_active();
  }

  static std::array<gfx::Bitmap*, 2> PreviewOwners(
      ObjectTileEditorPanel& panel) {
    return {&panel.object_preview_bmp_, &panel.tile8_atlas_bmp_};
  }

  static int PreviewWidth(const ObjectTileEditorPanel& panel) {
    return panel.object_preview_bmp_.width();
  }

  static int PreviewHeight(const ObjectTileEditorPanel& panel) {
    return panel.object_preview_bmp_.height();
  }

  static int AtlasWidth(const ObjectTileEditorPanel& panel) {
    return panel.tile8_atlas_bmp_.width();
  }

  static int AtlasHeight(const ObjectTileEditorPanel& panel) {
    return panel.tile8_atlas_bmp_.height();
  }

  static uint16_t PreviewPaletteColor(const ObjectTileEditorPanel& panel,
                                      size_t index) {
    return panel.object_preview_bmp_.palette()[index].snes();
  }

  static uint16_t AtlasPaletteColor(const ObjectTileEditorPanel& panel,
                                    size_t index) {
    return panel.tile8_atlas_bmp_.palette()[index].snes();
  }

  static uint8_t PreviewPixel(ObjectTileEditorPanel& panel, int x, int y) {
    return panel.object_preview_bmp_
        .mutable_data()[y * panel.object_preview_bmp_.width() + x];
  }

  static uint8_t AtlasPixel(ObjectTileEditorPanel& panel, int tile_id, int x,
                            int y) {
    const int column = tile_id % zelda3::ObjectTileEditor::kAtlasTilesPerRow;
    const int row = tile_id / zelda3::ObjectTileEditor::kAtlasTilesPerRow;
    return panel.tile8_atlas_bmp_
        .mutable_data()[(row * 8 + y) * panel.tile8_atlas_bmp_.width() +
                        column * 8 + x];
  }

  static void SeedTransientState(ObjectTileEditorPanel& panel) {
    panel.selected_cell_index_ = 4;
    panel.selected_source_tile_ = 0x2A;
    panel.show_shared_confirm_ = true;
    panel.shared_object_count_ = 7;
    panel.shared_tile_data_usage_override_ = 7;
    panel.action_status_tone_ =
        ObjectTileEditorPanel::ActionStatusTone::kWarning;
    panel.action_status_message_ = "stale shared status";
  }

  static void SeedRenderedBitmaps(ObjectTileEditorPanel& panel) {
    std::vector<uint8_t> pixels(64, 0);
    panel.object_preview_bmp_.Create(/*width=*/8, /*height=*/8, /*depth=*/8,
                                     pixels);
    panel.tile8_atlas_bmp_.Create(/*width=*/8, /*height=*/8, /*depth=*/8,
                                  pixels);
  }

  static void RenderObjectPreview(ObjectTileEditorPanel& panel) {
    panel.RenderObjectPreview();
  }

  static void RenderTile8Atlas(ObjectTileEditorPanel& panel) {
    panel.RenderTile8Atlas();
  }

  static void SetLayout(ObjectTileEditorPanel& panel,
                        zelda3::ObjectTileLayout layout) {
    panel.current_layout_ = std::move(layout);
  }

  static void SetSelectedCellIndex(ObjectTileEditorPanel& panel, int index) {
    panel.selected_cell_index_ = index;
  }

  static void SetSourcePalette(ObjectTileEditorPanel& panel, int palette) {
    panel.source_palette_ = palette;
  }

  static void SetAtlasDirty(ObjectTileEditorPanel& panel, bool dirty) {
    panel.atlas_dirty_ = dirty;
  }

  static void SyncSourceSelectionFromSelectedCell(
      ObjectTileEditorPanel& panel) {
    panel.SyncSourceSelectionFromSelectedCell();
  }

  static void HandleKeyboardShortcuts(ObjectTileEditorPanel& panel) {
    bool open = true;
    panel.HandleKeyboardShortcuts(&open);
  }

  static void DrawTileProperties(ObjectTileEditorPanel& panel) {
    panel.DrawTileProperties();
  }

  static zelda3::ObjectTileLayout MakeCustomLayout(int width, int height,
                                                   int16_t object_id,
                                                   std::string filename,
                                                   bool modified = true) {
    zelda3::ObjectTileLayout layout;
    layout.object_id = object_id;
    layout.bounds_width = width;
    layout.bounds_height = height;
    layout.tile_data_address = -1;
    layout.is_custom = true;
    layout.custom_subtype = 0;
    layout.custom_filename = std::move(filename);
    layout.custom_source_bytes = {0x01, 0x00, 0x00, 0x08, 0x00, 0x00};
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        zelda3::ObjectTileLayout::Cell cell;
        cell.rel_x = x;
        cell.rel_y = y;
        cell.tile_info = gfx::TileInfo(0, 2, false, false, false);
        cell.original_word = gfx::TileInfoToWord(cell.tile_info);
        cell.write_index = static_cast<int>(layout.cells.size());
        cell.modified = modified;
        layout.cells.push_back(cell);
      }
    }
    return layout;
  }

  static void OpenCustomLayoutForTest(ObjectTileEditorPanel& panel, int width,
                                      int height, int16_t object_id,
                                      int room_id, DungeonRoomStore* rooms,
                                      bool modified = true) {
    panel.current_object_id_ = object_id;
    panel.current_room_id_ = room_id;
    panel.rooms_ = rooms;
    panel.current_layout_ =
        MakeCustomLayout(width, height, object_id, "test_custom.bin", modified);
    panel.ResetTransientState();
    panel.is_open_ = true;
    panel.SelectFirstCellIfAvailable();
  }

  static void ApplyChanges(ObjectTileEditorPanel& panel) {
    panel.ApplyChanges();
  }

  static void ApplyChanges(ObjectTileEditorPanel& panel, bool confirm_shared) {
    panel.ApplyChanges(confirm_shared);
  }

  static absl::Status AddFirstTileToEmptyCustomLayout(
      ObjectTileEditorPanel& panel) {
    return panel.AddFirstTileToEmptyCustomLayout();
  }

  static void RevertCurrentLayout(ObjectTileEditorPanel& panel) {
    panel.RevertCurrentLayout();
  }

  static std::string BuildWindowTitle(const ObjectTileEditorPanel& panel) {
    return panel.BuildWindowTitle();
  }

  static void MarkFirstCellModified(ObjectTileEditorPanel& panel) {
    ASSERT_FALSE(panel.current_layout_.cells.empty());
    panel.current_layout_.cells[0].modified = true;
  }

  static void ClearModifications(ObjectTileEditorPanel& panel) {
    for (auto& cell : panel.current_layout_.cells) {
      cell.modified = false;
    }
  }

  static void RequestSafeWindowClose(ObjectTileEditorPanel& panel,
                                     bool* p_open) {
    panel.RequestSafeWindowClose(p_open);
  }

  static void SetFirstCellTileAndPalette(ObjectTileEditorPanel& panel,
                                         uint16_t tile_id, uint8_t palette) {
    ASSERT_FALSE(panel.current_layout_.cells.empty());
    panel.current_layout_.cells[0].tile_info.id_ = tile_id;
    panel.current_layout_.cells[0].tile_info.palette_ = palette;
    panel.current_layout_.cells[0].modified = true;
  }

  static bool HasModifications(const ObjectTileEditorPanel& panel) {
    return panel.current_layout_.HasModifications();
  }

  static const zelda3::ObjectTileLayout& Layout(
      const ObjectTileEditorPanel& panel) {
    return panel.current_layout_;
  }

  static uint16_t FirstCellTileId(const ObjectTileEditorPanel& panel) {
    EXPECT_FALSE(panel.current_layout_.cells.empty());
    return panel.current_layout_.cells.front().tile_info.id_;
  }

  static uint8_t FirstCellPalette(const ObjectTileEditorPanel& panel) {
    EXPECT_FALSE(panel.current_layout_.cells.empty());
    return panel.current_layout_.cells.front().tile_info.palette_;
  }

  static absl::StatusOr<int> SharedTileDataUsageCount(
      const ObjectTileEditorPanel& panel) {
    return panel.GetSharedTileDataUsageCount();
  }

  static absl::StatusOr<int> DisplayedSharedTileDataUsageCount(
      ObjectTileEditorPanel& panel) {
    return panel.GetDisplayedSharedTileDataUsageCount();
  }

  static absl::StatusOr<bool> HasSharedTileDataConflict(
      const ObjectTileEditorPanel& panel) {
    return panel.HasSharedTileDataConflict();
  }

  static bool HasStandardWritePreflightCallback(
      const ObjectTileEditorPanel& panel) {
    return static_cast<bool>(panel.standard_write_preflight_);
  }

  static bool HasTilesAppliedCallback(const ObjectTileEditorPanel& panel) {
    return static_cast<bool>(panel.on_tiles_applied_);
  }
};

class DungeonEditorV2ObjectTileEditorTestPeer {
 public:
  static absl::Status OpenObjectTileEditorForObject(
      DungeonEditorV2& editor, int room_id, const zelda3::RoomObject& object) {
    return editor.OpenObjectTileEditorForObject(room_id, object);
  }

  static void SetObjectTileEditorPanel(DungeonEditorV2& editor,
                                       ObjectTileEditorPanel* panel) {
    editor.object_tile_editor_panel_ = panel;
  }

  static void SetObjectSelectorPanel(DungeonEditorV2& editor,
                                     ObjectSelectorContent* panel) {
    editor.object_selector_panel_ = panel;
  }

  static void SynchronizeCustomObjectAssets(DungeonEditorV2& editor) {
    editor.SynchronizeCustomObjectAssets();
  }
};

namespace {

gfx::PaletteGroup MakeTestPaletteGroup(int base) {
  gfx::PaletteGroup group("test");

  gfx::SnesPalette pal0;
  pal0.AddColor(gfx::SnesColor(base + 1, base + 2, base + 3));
  pal0.AddColor(gfx::SnesColor(base + 4, base + 5, base + 6));
  group.AddPalette(pal0);

  gfx::SnesPalette pal1;
  pal1.AddColor(gfx::SnesColor(base + 7, base + 8, base + 9));
  pal1.AddColor(gfx::SnesColor(base + 10, base + 11, base + 12));
  group.AddPalette(pal1);

  gfx::SnesPalette pal2;
  pal2.AddColor(gfx::SnesColor(base + 13, base + 14, base + 15));
  pal2.AddColor(gfx::SnesColor(base + 16, base + 17, base + 18));
  group.AddPalette(pal2);

  return group;
}

std::filesystem::path MakeTempDir(const std::string& stem) {
  auto now = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() / (stem + "_" + now);
}

struct ScopedCustomObjectState {
  explicit ScopedCustomObjectState(std::filesystem::path temp_dir)
      : old_state(zelda3::CustomObjectManager::Get().SnapshotState()),
        old_enabled(core::FeatureFlags::get().kEnableCustomObjects),
        dir(std::move(temp_dir)) {
    std::filesystem::create_directories(dir);
    zelda3::CustomObjectManager::Get().Initialize(dir.string());
    zelda3::CustomObjectManager::Get().ClearObjectFileMap();
    core::FeatureFlags::get().kEnableCustomObjects = true;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

  ~ScopedCustomObjectState() {
    core::FeatureFlags::get().kEnableCustomObjects = old_enabled;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    zelda3::CustomObjectManager::Get().RestoreState(old_state);
    std::filesystem::remove_all(dir);
  }

  zelda3::CustomObjectManager::State old_state;
  bool old_enabled;
  std::filesystem::path dir;
};

class ScopedImGuiContext {
 public:
  ScopedImGuiContext() : previous_(ImGui::GetCurrentContext()) {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1024.0f, 768.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  ~ScopedImGuiContext() {
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_);
  }

 private:
  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

void WriteCustomObjectAsset(const std::filesystem::path& path,
                            const zelda3::CustomObject& object) {
  auto bytes_or = zelda3::EncodeCustomObjectBinary(object);
  ASSERT_TRUE(bytes_or.ok()) << bytes_or.status();
  std::ofstream output(path, std::ios::binary);
  ASSERT_TRUE(output.is_open());
  output.write(reinterpret_cast<const char*>(bytes_or->data()),
               static_cast<std::streamsize>(bytes_or->size()));
  ASSERT_TRUE(output.good());
}

std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open());
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input),
                              std::istreambuf_iterator<char>());
}

int ReadWordAt(const Rom& rom, int addr) {
  const uint8_t low = rom.data()[addr];
  const uint8_t high = rom.data()[addr + 1];
  return static_cast<int>(low | (high << 8));
}

constexpr int16_t kEditableStandardObjectId = 0x11F;
constexpr uint32_t kEditableDescriptorPcAddress = 0x842E;
constexpr uint16_t kEditableDescriptorWord = 0x0E9A;
constexpr uint32_t kEditableSourcePcAddress = 0x29EC;
constexpr uint16_t kEditableSourceWords[] = {0x0DEE, 0x8DEE, 0x4DEE, 0xCDEE};
constexpr int16_t kTorchAliasObjectId = 0x120;
constexpr uint32_t kTorchAliasDescriptorPcAddress = 0x8430;
constexpr uint16_t kTorchAliasDescriptorWord = 0x0ECA;
constexpr uint32_t kTorchAliasSourcePcAddress = 0x2A1C;
constexpr uint16_t kTorchAliasSourceWords[] = {0x0DC0, 0x0DC1, 0x4DC0, 0x4DC1};

void StoreWord(std::vector<uint8_t>& data, uint32_t address, uint16_t word) {
  data[address] = static_cast<uint8_t>(word & 0xFF);
  data[address + 1] = static_cast<uint8_t>(word >> 8);
}

std::vector<uint8_t> MakeEditableStandardObjectRomData() {
  std::vector<uint8_t> data(0x200000, 0);
  StoreWord(data, kEditableDescriptorPcAddress, kEditableDescriptorWord);
  for (size_t index = 0; index < std::size(kEditableSourceWords); ++index) {
    StoreWord(data, kEditableSourcePcAddress + index * 2,
              kEditableSourceWords[index]);
  }
  StoreWord(data, kTorchAliasDescriptorPcAddress, kTorchAliasDescriptorWord);
  for (size_t index = 0; index < std::size(kTorchAliasSourceWords); ++index) {
    StoreWord(data, kTorchAliasSourcePcAddress + index * 2,
              kTorchAliasSourceWords[index]);
  }
  return data;
}

void SeedCoordinatorPaletteGroup(gfx::PaletteGroup* group, int palette_count,
                                 int color_count, uint16_t seed) {
  ASSERT_NE(group, nullptr);
  group->clear();
  for (int palette_index = 0; palette_index < palette_count; ++palette_index) {
    gfx::SnesPalette palette;
    for (int color_index = 0; color_index < color_count; ++color_index) {
      palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(
          (seed + palette_index * color_count + color_index) & 0x7FFF)));
    }
    group->AddPalette(palette);
  }
}

void ConfigureCoordinatorGameData(zelda3::GameData* game_data) {
  ASSERT_NE(game_data, nullptr);
  game_data->graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
  for (auto& ids : game_data->main_blockset_ids) {
    ids.fill(0);
  }
  for (auto& ids : game_data->room_blockset_ids) {
    ids.fill(0);
  }
  for (auto& ids : game_data->spriteset_ids) {
    ids.fill(0);
  }
  for (auto& ids : game_data->paletteset_ids) {
    ids.fill(0);
  }

  SeedCoordinatorPaletteGroup(&game_data->palette_groups.hud,
                              /*palette_count=*/1, /*color_count=*/32,
                              /*seed=*/0x0100);
  SeedCoordinatorPaletteGroup(&game_data->palette_groups.dungeon_main,
                              /*palette_count=*/4, /*color_count=*/90,
                              /*seed=*/0x0200);
  game_data->paletteset_ids[5][0] = 2;
}

void PrepareCoordinatorRoom(Rom* rom, zelda3::GameData* game_data,
                            DungeonEditorV2* editor, int room_id) {
  ASSERT_NE(rom, nullptr);
  ASSERT_NE(game_data, nullptr);
  ASSERT_NE(editor, nullptr);

  const int layout_address = SnesToPc(zelda3::kRoomLayoutPointers.front());
  ASSERT_GE(layout_address, 0);
  ASSERT_LT(layout_address + 1, static_cast<int>(rom->size()));
  rom->mutable_data()[layout_address] = 0xFF;
  rom->mutable_data()[layout_address + 1] = 0xFF;
  ASSERT_TRUE(rom->WriteWord(zelda3::kDungeonPalettePointerTable + 2,
                             3 * zelda3::kDungeonPaletteBytes)
                  .ok());

  auto& room = editor->rooms()[room_id];
  room = zelda3::Room(room_id, rom, game_data);
  room.SetLoaded(true);
  room.SetTileObjects({});
  room.SetPalette(5);  // Resolves through the pointer table to palette 3.
}

absl::StatusOr<zelda3::ObjectTileLayout> CaptureEditableStandardLayout(
    Rom& rom, int16_t object_id = kEditableStandardObjectId) {
  zelda3::Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  zelda3::ObjectTileEditor editor(&rom);
  return editor.CaptureEditableObjectLayout(object_id, room, palette);
}

int FirstCellSourceAddress(const zelda3::ObjectTileLayout& layout) {
  if (!layout.source_provenance.has_value() || layout.cells.empty() ||
      !layout.cells.front().source_ref.has_value()) {
    return -1;
  }
  const auto& provenance = *layout.source_provenance;
  const auto& source_ref = *layout.cells.front().source_ref;
  if (source_ref.span_index >= provenance.spans.size() ||
      source_ref.word_index >=
          provenance.spans[source_ref.span_index].expected_words.size()) {
    return -1;
  }
  return static_cast<int>(provenance.spans[source_ref.span_index].pc_address +
                          source_ref.word_index * 2);
}

std::string ManifestProtectingPcRange(uint32_t begin, uint32_t end) {
  return absl::StrFormat(
      R"json(
{
  "manifest_version": 3,
  "hack_name": "Synthetic object tile save guard",
  "protected_regions": {
    "total_hooks": 1,
    "regions": [
      {
        "start": "0x%06X",
        "end": "0x%06X",
        "hook_count": 1,
        "module": "SyntheticObjectTileGuard"
      }
    ]
  }
}
)json",
      PcToSnes(begin), PcToSnes(end));
}

void OpenInjectedSharedStandardObjectSession(ObjectTileEditorPanel& panel,
                                             Rom& rom) {
  auto layout_or = CaptureEditableStandardLayout(rom);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(
      panel, kEditableStandardObjectId);
  ObjectTileEditorPanelTestAccess::SetOpen(panel, true);
  ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 0);
  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);
}

TEST(DungeonEditorV2ObjectTileEditorTest,
     RegisteredPanelOpensWithResolvedRoomPalette) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  zelda3::GameData game_data(&rom);
  ConfigureCoordinatorGameData(&game_data);

  WorkspaceWindowManager window_manager;
  window_manager.RegisterSession(0);
  window_manager.SetActiveSession(0);

  DungeonEditorV2 editor(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.game_data = &game_data;
  dependencies.window_manager = &window_manager;
  editor.SetDependencies(dependencies);
  editor.SetGameData(&game_data);
  PrepareCoordinatorRoom(&rom, &game_data, &editor, /*room_id=*/0);

  auto panel = std::make_unique<ObjectTileEditorPanel>(nullptr, &rom);
  ObjectTileEditorPanel* panel_ptr = panel.get();
  window_manager.RegisterWindowContent(std::move(panel));
  DungeonEditorV2ObjectTileEditorTestPeer::SetObjectTileEditorPanel(editor,
                                                                    panel_ptr);

  ASSERT_FALSE(window_manager.IsWindowOpen(0, panel_ptr->GetId()));
  const zelda3::RoomObject selected_object(kEditableStandardObjectId, /*x=*/0,
                                           /*y=*/0, /*size=*/0, /*layer=*/0);
  const absl::Status status =
      DungeonEditorV2ObjectTileEditorTestPeer::OpenObjectTileEditorForObject(
          editor, /*room_id=*/0, selected_object);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_TRUE(window_manager.IsWindowOpen(0, panel_ptr->GetId()));
  EXPECT_TRUE(panel_ptr->IsOpen());
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(*panel_ptr), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(*panel_ptr),
            kEditableStandardObjectId);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Rooms(*panel_ptr),
            &editor.rooms());
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::Layout(*panel_ptr)
                  .source_provenance.has_value());

  const auto& hud_palette = game_data.palette_groups.hud.palette_ref(0);
  const auto expected_group = zelda3::BuildDungeonRenderPaletteGroup(
      game_data.palette_groups.dungeon_main.palette_ref(3), &hud_palette);
  const auto default_group = zelda3::BuildDungeonRenderPaletteGroup(
      game_data.palette_groups.dungeon_main.palette_ref(0), &hud_palette);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentPaletteColor(
                *panel_ptr, /*palette=*/2, /*color=*/1),
            expected_group.GetColor(/*palette=*/2, /*color=*/1).snes());
  EXPECT_NE(ObjectTileEditorPanelTestAccess::CurrentPaletteColor(
                *panel_ptr, /*palette=*/2, /*color=*/1),
            default_group.GetColor(/*palette=*/2, /*color=*/1).snes());
}

TEST(DungeonEditorV2ObjectTileEditorTest,
     UnregisteredPanelFailsAndClosesCapturedSession) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  zelda3::GameData game_data(&rom);
  ConfigureCoordinatorGameData(&game_data);

  WorkspaceWindowManager window_manager;
  window_manager.RegisterSession(0);
  window_manager.SetActiveSession(0);
  ObjectTileEditorPanel unregistered_panel(nullptr, &rom);

  DungeonEditorV2 editor(&rom);
  EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.game_data = &game_data;
  dependencies.window_manager = &window_manager;
  editor.SetDependencies(dependencies);
  editor.SetGameData(&game_data);
  PrepareCoordinatorRoom(&rom, &game_data, &editor, /*room_id=*/0);
  DungeonEditorV2ObjectTileEditorTestPeer::SetObjectTileEditorPanel(
      editor, &unregistered_panel);

  const zelda3::RoomObject selected_object(kEditableStandardObjectId, /*x=*/0,
                                           /*y=*/0, /*size=*/0, /*layer=*/0);
  const absl::Status status =
      DungeonEditorV2ObjectTileEditorTestPeer::OpenObjectTileEditorForObject(
          editor, /*room_id=*/0, selected_object);

  EXPECT_TRUE(absl::IsNotFound(status)) << status;
  EXPECT_FALSE(window_manager.IsWindowOpen(0, unregistered_panel.GetId()));
  EXPECT_FALSE(unregistered_panel.IsOpen());
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(unregistered_panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(unregistered_panel),
            -1);
  EXPECT_EQ(
      ObjectTileEditorPanelTestAccess::CurrentObjectId(unregistered_panel), -1);
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectInvalidRoomPreservesPreviousLayout) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/2, /*height=*/2, /*object_id=*/0x31,
      /*room_id=*/0, /*rooms=*/nullptr, /*modified=*/false);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ObjectTileEditorPanelTestAccess::ClearModifications(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  DungeonRoomStore rooms(&rom);
  const absl::Status status =
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/-1, &rooms);

  EXPECT_TRUE(absl::IsOutOfRange(status));
  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x31);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectCaptureFailurePreservesPreviousLayout) {
  Rom rom;

  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/2, /*height=*/2, /*object_id=*/0x31,
      /*room_id=*/0, /*rooms=*/nullptr, /*modified=*/false);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ObjectTileEditorPanelTestAccess::ClearModifications(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  const absl::Status status =
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms);

  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x31);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectRejectsNonAllowlistedObjectWithoutChangingSession) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/1, /*height=*/1, /*object_id=*/0x31,
      /*room_id=*/0, &rooms, /*modified=*/false);

  const absl::Status status =
      panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/0, &rooms);

  EXPECT_TRUE(absl::IsUnimplemented(status));
  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::Layout(panel).is_custom);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x31);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::Layout(panel)
                   .source_provenance.has_value());
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectRejectsReplacingSessionWithUnappliedChanges) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  const auto original_layout = ObjectTileEditorPanelTestAccess::Layout(panel);

  const absl::Status status =
      panel.OpenForObject(kTorchAliasObjectId, /*room_id=*/1, &rooms);

  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_NE(std::string(status.message()).find("Apply, revert, or close"),
            std::string::npos);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel),
            kEditableStandardObjectId);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Layout(panel)
                .cells.front()
                .tile_info.id_,
            original_layout.cells.front().tile_info.id_);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
}

TEST(ObjectTileEditorPanelTest, PaletteUpdatesStayBoundToTheOpenSessionRoom) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);
  const gfx::PaletteGroup room_zero_palette = MakeTestPaletteGroup(1);
  const gfx::PaletteGroup room_one_palette = MakeTestPaletteGroup(10);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForObject(kEditableStandardObjectId, /*room_id=*/0,
                                 &rooms, room_zero_palette)
                  .ok());
  const uint16_t original_color =
      ObjectTileEditorPanelTestAccess::CurrentPaletteColor(panel, 0, 0);

  panel.SetCurrentPaletteGroupForRoom(/*room_id=*/1, room_one_palette);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentPaletteColor(panel, 0, 0),
            original_color);

  panel.SetCurrentPaletteGroupForRoom(/*room_id=*/0, room_one_palette);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentPaletteColor(panel, 0, 0),
            room_one_palette.GetColor(0, 0).snes());
}

TEST(ObjectTileEditorPanelTest, ExplicitCloseClearsTransientStateAndContext) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonRoomStore rooms(&rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/2, /*height=*/2, /*object_id=*/0x31,
      /*room_id=*/5, &rooms);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);

  panel.Close();

  EXPECT_FALSE(panel.IsOpen());
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedTileDataUsageOverride(panel),
            -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActionStatus(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsNone(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Rooms(panel), nullptr);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

class ObjectTileEditorPreviewLifetimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    arena_.ClearTextureQueue();
    arena_.DrainRetiredBitmaps(&renderer_);
  }

  void TearDown() override {
    // A failed RED assertion must not leave commands pointing at dead owners.
    arena_.ClearTextureQueue();
    arena_.DrainRetiredBitmaps(&renderer_);
  }

  size_t ActiveSurfaces() const {
    return arena_.GetSurfaceCount() - arena_.GetPooledSurfaceCount();
  }

  void QueuePreviewWork(ObjectTileEditorPanel& panel) {
    ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
    const auto owners = ObjectTileEditorPanelTestAccess::PreviewOwners(panel);
    for (size_t index = 0; index < owners.size(); ++index) {
      owners[index]->set_texture(&texture_storage_[index]);
      arena_.QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                 owners[index]);
      arena_.QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                 owners[index]);
    }
    ASSERT_EQ(arena_.texture_command_queue_size(), 4u);
  }

  void ExpectDeferredDestruction() {
    EXPECT_EQ(arena_.retired_texture_handle_count(), 2u);
    EXPECT_CALL(renderer_, DestroyTexture(&texture_storage_[0])).Times(1);
    EXPECT_CALL(renderer_, DestroyTexture(&texture_storage_[1])).Times(1);
    EXPECT_EQ(arena_.DrainRetiredBitmaps(&renderer_), 2u);
    EXPECT_EQ(arena_.DrainRetiredBitmaps(&renderer_), 0u);
  }

  gfx::Arena& arena_ = gfx::Arena::Get();
  ::testing::NiceMock<test::MockRenderer> renderer_;
  std::array<int, 2> texture_storage_{};
};

TEST_F(ObjectTileEditorPreviewLifetimeTest,
       CloseCancelsPendingCommandsAndRetiresBothPreviewOwners) {
  Rom rom;
  const size_t initial_surfaces = ActiveSurfaces();
  {
    ObjectTileEditorPanel panel(nullptr, &rom);
    QueuePreviewWork(panel);
    EXPECT_EQ(ActiveSurfaces(), initial_surfaces + 2);

    // Close can happen after ImGui recorded either texture in this frame.
    // Neither handle may be destroyed until the arena's explicit drain.
    EXPECT_CALL(renderer_, DestroyTexture).Times(0);
    panel.Close();
    EXPECT_EQ(arena_.texture_command_queue_size(), 0u);
    EXPECT_EQ(ActiveSurfaces(), initial_surfaces);
    for (const auto* owner :
         ObjectTileEditorPanelTestAccess::PreviewOwners(panel)) {
      EXPECT_EQ(owner->surface(), nullptr);
      EXPECT_EQ(owner->texture(), nullptr);
    }
    panel.Close();  // Retirement is idempotent, including the later destructor.
  }
  ::testing::Mock::VerifyAndClearExpectations(&renderer_);
  ExpectDeferredDestruction();
}

TEST_F(ObjectTileEditorPreviewLifetimeTest,
       DestructionCancelsPendingCommandsBeforePreviewOwnersDisappear) {
  Rom rom;
  const size_t initial_surfaces = ActiveSurfaces();
  {
    ObjectTileEditorPanel panel(nullptr, &rom);
    QueuePreviewWork(panel);
    EXPECT_EQ(ActiveSurfaces(), initial_surfaces + 2);
    EXPECT_CALL(renderer_, DestroyTexture).Times(0);
  }
  EXPECT_EQ(arena_.texture_command_queue_size(), 0u);
  EXPECT_EQ(ActiveSurfaces(), initial_surfaces);
  ::testing::Mock::VerifyAndClearExpectations(&renderer_);
  ExpectDeferredDestruction();
}

TEST(ObjectTileEditorPanelTest, OnClosePreservesModifiedSessionForReopen) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);

  panel.OnClose();

  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel),
            kEditableStandardObjectId);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
}

TEST(DungeonEditorV2ObjectTileEditorTest,
     UnappliedLayoutMarksSessionPendingAndBlocksSaveUntilDiscarded) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);
  ObjectTileEditorPanel panel(nullptr, &rom);
  DungeonEditorV2 editor(&rom);
  DungeonEditorV2ObjectTileEditorTestPeer::SetObjectTileEditorPanel(editor,
                                                                    &panel);

  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  EXPECT_FALSE(panel.HasUnappliedChanges());
  EXPECT_FALSE(editor.HasPendingDungeonChanges());

  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  ASSERT_TRUE(panel.HasUnappliedChanges());
  EXPECT_TRUE(editor.HasPendingDungeonChanges());
  const auto rom_before_save = rom.vector();
  const bool dirty_before_save = rom.dirty();

  panel.OnClose();
  EXPECT_TRUE(panel.HasUnappliedChanges());
  EXPECT_TRUE(editor.HasPendingDungeonChanges());

  const absl::Status save_status = editor.Save();
  EXPECT_TRUE(absl::IsFailedPrecondition(save_status)) << save_status;
  EXPECT_NE(std::string(save_status.message()).find("Object Tile Editor"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), rom_before_save);
  EXPECT_EQ(rom.dirty(), dirty_before_save);
  EXPECT_TRUE(panel.HasUnappliedChanges());

  const absl::Status save_room_status = editor.SaveRoom(/*room_id=*/0);
  EXPECT_TRUE(absl::IsFailedPrecondition(save_room_status)) << save_room_status;
  EXPECT_NE(std::string(save_room_status.message()).find("Object Tile Editor"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), rom_before_save);
  EXPECT_EQ(rom.dirty(), dirty_before_save);
  EXPECT_TRUE(panel.HasUnappliedChanges());

  panel.Close();
  EXPECT_FALSE(panel.HasUnappliedChanges());
  EXPECT_FALSE(editor.HasPendingDungeonChanges());
}

TEST(ObjectTileEditorPanelTest,
     SafeWindowCloseHidesAndPreservesModifiedSession) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  bool visible = true;

  ObjectTileEditorPanelTestAccess::RequestSafeWindowClose(panel, &visible);

  EXPECT_FALSE(visible);
  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Unapplied tile changes were kept"),
            std::string::npos);
}

TEST(ObjectTileEditorPanelTest, OpeningCustomLayoutClearsPendingSharedConfirm) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/2, /*height=*/2, /*object_id=*/0x31,
      /*room_id=*/0, /*rooms=*/nullptr, /*modified=*/false);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);

  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/1, /*height=*/1, /*object_id=*/0x32,
      /*room_id=*/1, /*rooms=*/nullptr, /*modified=*/false);

  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedTileDataUsageOverride(panel),
            -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActionStatus(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsNone(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x32);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForCustomObjectRejectsReplacingModifiedSession) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_modified_session"));
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[1].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/2, /*height=*/2, /*object_id=*/0x31,
      /*room_id=*/0, &rooms);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));

  const absl::Status status = panel.OpenForCustomObject(
      /*object_id=*/0x32, /*subtype=*/0, /*room_id=*/1, &rooms);

  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x31);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Layout(panel).custom_filename,
            "test_custom.bin");
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
}

TEST(ObjectTileEditorPanelTest,
     CustomLayoutSelectsFirstCellAndKeepsDefaultPaletteInSync) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/2, /*height=*/2, /*object_id=*/0x32,
      /*room_id=*/1, /*rooms=*/nullptr, /*modified=*/false);

  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 2);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::Layout(panel).is_custom);
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectCapturedLayoutSelectsFirstCellAndSyncsSourceSelection) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  ObjectTileEditorPanel panel(nullptr, &rom);
  const absl::Status status =
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms);

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_TRUE(panel.IsOpen());
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::Layout(panel)
                  .source_provenance.has_value());
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Layout(panel)
                .source_provenance->object_id,
            kEditableStandardObjectId);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel),
            ObjectTileEditorPanelTestAccess::FirstCellTileId(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel),
            ObjectTileEditorPanelTestAccess::FirstCellPalette(panel));
}

TEST(ObjectTileEditorPanelTest,
     CustomSourceImpactCountsSingleFixedSlotByResolvedPath) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_single_consumer"));
  WriteCustomObjectAsset(custom_state.dir / "track_LR.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 1);
  auto conflict_or =
      ObjectTileEditorPanelTestAccess::HasSharedTileDataConflict(panel);
  ASSERT_TRUE(conflict_or.ok()) << conflict_or.status();
  EXPECT_FALSE(*conflict_or);
}

TEST(ObjectTileEditorPanelTest,
     SharedCustomAssetRequiresConfirmationBeforePublishingSameFamilySlots) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_shared_family"));
  const auto asset_path = custom_state.dir / "shared.bin";
  WriteCustomObjectAsset(asset_path,
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"shared.bin", "shared.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 2);
  const std::vector<uint8_t> original_bytes = ReadBinaryFile(asset_path);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x24, /*palette=*/2);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Confirm shared asset publish"),
            std::string::npos);
  EXPECT_EQ(ReadBinaryFile(asset_path), original_bytes);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_NE(ReadBinaryFile(asset_path), original_bytes);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsSuccess(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Published shared custom asset"),
            std::string::npos);
}

TEST(ObjectTileEditorPanelTest,
     CustomSourceImpactCountsAllFamiliesSharingOneAsset) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_cross_family"));
  WriteCustomObjectAsset(custom_state.dir / "shared.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"shared.bin"}}, {0x32, {"shared.bin"}}, {0x54, {"shared.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x54, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 3);
}

TEST(ObjectTileEditorPanelTest,
     CustomSourceImpactRecognizesCaseAliasOnInsensitiveFilesystem) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_case_alias"));
  const auto mixed_case_path = custom_state.dir / "Shared.bin";
  const auto lower_case_path = custom_state.dir / "shared.bin";
  WriteCustomObjectAsset(mixed_case_path,
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  std::error_code equivalent_error;
  if (!std::filesystem::equivalent(mixed_case_path, lower_case_path,
                                   equivalent_error) ||
      equivalent_error) {
    GTEST_SKIP() << "Filesystem treats case variants as distinct paths";
  }
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"Shared.bin", "shared.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 2);
}

TEST(ObjectTileEditorPanelTest,
     CustomSourceImpactDoesNotCountVanillaWallCornerObjects) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_wall_identity"));
  WriteCustomObjectAsset(custom_state.dir / "corner.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"", "", "corner.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/2,
                                       /*room_id=*/0, &rooms)
                  .ok());

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 1);
}

TEST(ObjectTileEditorPanelTest,
     SharedCustomAssetCountsExplicitWallOverrideAndRequiresConfirmation) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_explicit_wall_override"));
  const auto asset_path = custom_state.dir / "corner.bin";
  WriteCustomObjectAsset(asset_path,
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"", "", "corner.bin"}}, {0x100, {"corner.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/2,
                                       /*room_id=*/0, &rooms)
                  .ok());

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 2);

  const std::vector<uint8_t> original_bytes = ReadBinaryFile(asset_path);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x24, /*palette=*/2);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);
  EXPECT_EQ(ReadBinaryFile(asset_path), original_bytes);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForCustomObjectRejectsDisabledFeatureAndUnloadedRoom) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_readiness"));
  WriteCustomObjectAsset(custom_state.dir / "track_LR.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  (void)rooms[0];
  ObjectTileEditorPanel panel(nullptr, &rom);

  const absl::Status unloaded_status = panel.OpenForCustomObject(
      /*object_id=*/0x31, /*subtype=*/0, /*room_id=*/0, &rooms);
  EXPECT_TRUE(absl::IsFailedPrecondition(unloaded_status));
  EXPECT_FALSE(panel.IsOpen());

  rooms[0].SetLoaded(true);
  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  const absl::Status disabled_status = panel.OpenForCustomObject(
      /*object_id=*/0x31, /*subtype=*/0, /*room_id=*/0, &rooms);
  EXPECT_TRUE(absl::IsFailedPrecondition(disabled_status));
  EXPECT_FALSE(panel.IsOpen());
}

TEST(ObjectTileEditorPanelTest,
     SharedTileDataUsageCountRejectsStandardLayoutsWithoutProvenance) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = ObjectTileEditorPanelTestAccess::MakeCustomLayout(
      /*width=*/1, /*height=*/1, /*object_id=*/0x40, "custom.bin");
  layout.is_custom = false;
  layout.custom_filename.clear();
  layout.tile_data_address = -1;
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));

  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(panel, 0x40);
  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  EXPECT_TRUE(absl::IsFailedPrecondition(usage_count_or.status()));
  auto conflict_or =
      ObjectTileEditorPanelTestAccess::HasSharedTileDataConflict(panel);
  EXPECT_TRUE(absl::IsFailedPrecondition(conflict_or.status()));
}

TEST(ObjectTileEditorPanelTest,
     DisplayImpactCacheUsesLayoutAndObjectTileRevision) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kTorchAliasObjectId, /*room_id=*/0, &rooms).ok());

  auto first_display_or =
      ObjectTileEditorPanelTestAccess::DisplayedSharedTileDataUsageCount(panel);
  ASSERT_TRUE(first_display_or.ok()) << first_display_or.status();
  ASSERT_EQ(*first_display_or, 3);

  // A raw mutation deliberately bypasses the revision contract. Display-only
  // state may keep its cached result even while the user edits tile values,
  // while every apply still analyzes fresh.
  StoreWord(rom.mutable_vector(), /*Type 1 object 1 descriptor=*/0x8002,
            0x8000);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  auto cached_display_or =
      ObjectTileEditorPanelTestAccess::DisplayedSharedTileDataUsageCount(panel);
  ASSERT_TRUE(cached_display_or.ok()) << cached_display_or.status();
  EXPECT_EQ(*cached_display_or, 3);
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel)
          .status()));

  rom.AdvanceObjectTileRevision();
  auto refreshed_display_or =
      ObjectTileEditorPanelTestAccess::DisplayedSharedTileDataUsageCount(panel);
  EXPECT_TRUE(absl::IsFailedPrecondition(refreshed_display_or.status()));
}

TEST(ObjectTileEditorPanelTest, RenderWithoutRoomContextClearsStaleBitmaps) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      panel, /*width=*/1, /*height=*/1, /*object_id=*/0x31,
      /*room_id=*/0, /*rooms=*/nullptr, /*modified=*/false);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  ObjectTileEditorPanelTestAccess::RenderObjectPreview(panel);
  ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     SyncSourceSelectionFromSelectedCellUsesSelectedCellTileAndPalette) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = ObjectTileEditorPanelTestAccess::MakeCustomLayout(
      /*width=*/2, /*height=*/1, /*object_id=*/0x123, "custom.bin");
  layout.cells[1].tile_info =
      gfx::TileInfo(/*id=*/0x56, /*palette=*/5, false, false, false);
  layout.cells[1].modified = true;

  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
  ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 1);
  ObjectTileEditorPanelTestAccess::SetSourcePalette(panel, 2);
  ObjectTileEditorPanelTestAccess::SetAtlasDirty(panel, false);

  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0x56);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 5);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
}

TEST(ObjectTileEditorPanelTest,
     SyncSourceSelectionFromSelectedCellKeepsAtlasCleanWhenPaletteMatches) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = ObjectTileEditorPanelTestAccess::MakeCustomLayout(
      /*width=*/1, /*height=*/1, /*object_id=*/0x123, "custom.bin");
  layout.cells[0].tile_info =
      gfx::TileInfo(/*id=*/0x2A, /*palette=*/2, false, false, false);
  layout.cells[0].modified = true;

  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
  ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 0);
  ObjectTileEditorPanelTestAccess::SetSourcePalette(panel, 2);
  ObjectTileEditorPanelTestAccess::SetAtlasDirty(panel, false);

  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0x2A);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 2);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesForSharedStandardObjectShowsConfirmationBeforeWriteback) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout_or = CaptureEditableStandardLayout(rom);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(
      panel, kEditableStandardObjectId);
  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/3);

  auto shared_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(shared_count_or.ok()) << shared_count_or.status();
  ASSERT_GT(*shared_count_or, 1);

  const int write_addr =
      FirstCellSourceAddress(ObjectTileEditorPanelTestAccess::Layout(panel));
  ASSERT_GE(write_addr, 0);
  const int original_word = ReadWordAt(rom, write_addr);

  const uint16_t original_tile_id =
      ObjectTileEditorPanelTestAccess::FirstCellTileId(panel);
  const uint8_t original_palette =
      ObjectTileEditorPanelTestAccess::FirstCellPalette(panel);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, static_cast<uint16_t>(original_tile_id ^ 0x1),
      static_cast<uint8_t>((original_palette + 1) & 0x7));

  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel, /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_GT(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 1);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActionStatus(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Confirm global apply"),
            std::string::npos);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, write_addr), original_word);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesUsesRealPartialOverlapImpactBeforeWriteback) {
  auto data = MakeEditableStandardObjectRomData();
  // Type 1 object 0 consumes [0x29E8, 0x29F0), partially overlapping the
  // editable object's [0x29EC, 0x29F4) source.
  StoreWord(data, /*Type 1 object 0 descriptor=*/0x8000, 0x0E96);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  OpenInjectedSharedStandardObjectSession(panel, rom);
  auto shared_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(shared_count_or.ok()) << shared_count_or.status();
  ASSERT_EQ(*shared_count_or, 2);

  const int write_addr =
      FirstCellSourceAddress(ObjectTileEditorPanelTestAccess::Layout(panel));
  ASSERT_GE(write_addr, 0);
  const int original_word = ReadWordAt(rom, write_addr);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, write_addr), original_word);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);
  ASSERT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  ASSERT_NE(ReadWordAt(rom, write_addr), original_word);

  // The successful apply must refresh provenance.expected_words. A second
  // edit in the same session should pass the real analyzer and warn again,
  // not fail its write-plan CAS check against the first applied value.
  const int first_applied_word = ReadWordAt(rom, write_addr);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x234, /*palette=*/4);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ActionStatusIsError(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, write_addr), first_applied_word);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesWarnsForGlobalTorchConsumersOfObject120) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout_or = CaptureEditableStandardLayout(rom, kTorchAliasObjectId);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(panel,
                                                      kTorchAliasObjectId);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);

  auto shared_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(shared_count_or.ok()) << shared_count_or.status();
  // Object 0x120 plus initial torch drawing and live lighting-change paths.
  ASSERT_EQ(*shared_count_or, 3);

  const int original_word = ReadWordAt(rom, kTorchAliasSourcePcAddress);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 3);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "3 consumers"),
            std::string::npos);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, kTorchAliasSourcePcAddress), original_word);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesFailsClosedWhenAnySourceImpactIsMalformed) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  OpenInjectedSharedStandardObjectSession(panel, rom);
  StoreWord(rom.mutable_vector(), /*Type 1 object 1 descriptor=*/0x8002,
            0x8000);

  const int write_addr =
      FirstCellSourceAddress(ObjectTileEditorPanelTestAccess::Layout(panel));
  ASSERT_GE(write_addr, 0);
  const int original_word = ReadWordAt(rom, write_addr);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 0);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsError(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "source impact could not be resolved"),
            std::string::npos);
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "object 0x001"),
            std::string::npos);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, write_addr), original_word);
}

TEST(ObjectTileEditorPanelTest,
     FirstApplyIgnoresValidDisplayCacheAndReanalyzesFresh) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  auto displayed_or =
      ObjectTileEditorPanelTestAccess::DisplayedSharedTileDataUsageCount(panel);
  ASSERT_TRUE(displayed_or.ok()) << displayed_or.status();

  const int original_word = ReadWordAt(rom, kEditableSourcePcAddress);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  StoreWord(rom.mutable_vector(), /*Type 1 object 1 descriptor=*/0x8002,
            0x8000);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsError(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, kEditableSourcePcAddress), original_word);
}

TEST(ObjectTileEditorPanelTest, ConfirmedApplyReanalyzesFreshBeforeWriting) {
  auto data = MakeEditableStandardObjectRomData();
  StoreWord(data, /*Type 1 object 0 descriptor=*/0x8000, 0x0E96);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  const int original_word = ReadWordAt(rom, kEditableSourcePcAddress);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));

  StoreWord(rom.mutable_vector(), /*Type 1 object 1 descriptor=*/0x8002,
            0x8000);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsError(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, kEditableSourcePcAddress), original_word);
}

TEST(ObjectTileEditorPanelTest,
     ConfirmedApplyRepromptsWhenConsumerCountChanges) {
  auto data = MakeEditableStandardObjectRomData();
  StoreWord(data, /*Type 1 object 0 descriptor=*/0x8000, 0x0E96);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  const int original_word = ReadWordAt(rom, kEditableSourcePcAddress);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  ASSERT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);

  StoreWord(rom.mutable_vector(), /*Type 1 object 7 descriptor=*/0x800E,
            kEditableDescriptorWord);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 3);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Source impact changed"),
            std::string::npos);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, kEditableSourcePcAddress), original_word);
}

TEST(ObjectTileEditorPanelTest,
     ConfirmedApplyRepromptsWhenConsumerIdentityChangesAtSameCount) {
  auto data = MakeEditableStandardObjectRomData();
  StoreWord(data, /*Type 1 object 0 descriptor=*/0x8000, 0x0E96);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  DungeonRoomStore rooms(&rom);

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  const int original_word = ReadWordAt(rom, kEditableSourcePcAddress);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  ASSERT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);

  StoreWord(rom.mutable_vector(), /*Type 1 object 0 descriptor=*/0x8000,
            0x0E00);
  StoreWord(rom.mutable_vector(), /*Type 1 object 7 descriptor=*/0x800E,
            0x0E96);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 2);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Source impact changed"),
            std::string::npos);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, kEditableSourcePcAddress), original_word);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesWithoutConfirmationWritesSharedStandardObjectAndClearsModal) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout_or = CaptureEditableStandardLayout(rom);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(
      panel, kEditableStandardObjectId);
  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/3);

  auto shared_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(shared_count_or.ok()) << shared_count_or.status();
  ASSERT_GT(*shared_count_or, 1);

  const int write_addr =
      FirstCellSourceAddress(ObjectTileEditorPanelTestAccess::Layout(panel));
  ASSERT_GE(write_addr, 0);
  const int original_word = ReadWordAt(rom, write_addr);

  const uint16_t original_tile_id =
      ObjectTileEditorPanelTestAccess::FirstCellTileId(panel);
  const uint8_t original_palette =
      ObjectTileEditorPanelTestAccess::FirstCellPalette(panel);
  const auto original_cell =
      ObjectTileEditorPanelTestAccess::Layout(panel).cells.front();
  const uint16_t updated_tile_id =
      static_cast<uint16_t>(original_tile_id ^ 0x1);
  const uint8_t updated_palette =
      static_cast<uint8_t>((original_palette + 1) & 0x7);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, updated_tile_id, updated_palette);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel, /*confirm_shared=*/true);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 0);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActionStatus(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsSuccess(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Applied changes to shared tile data"),
            std::string::npos);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_NE(ReadWordAt(rom, write_addr), original_word);
  EXPECT_EQ(ReadWordAt(rom, write_addr),
            static_cast<int>(gfx::TileInfoToWord(
                gfx::TileInfo(updated_tile_id, updated_palette,
                              original_cell.tile_info.horizontal_mirror_,
                              original_cell.tile_info.vertical_mirror_,
                              original_cell.tile_info.over_))));
}

TEST(ObjectTileEditorPanelTest,
     SuccessfulStandardApplyInvalidatesAllRoomsAndExternalViews) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());
  DungeonRoomStore rooms(&rom);
  auto& current_room = rooms[0];
  auto& other_room = rooms[1];
  current_room.SetLoaded(true);
  zelda3::RoomLayerManager layer_manager;
  (void)current_room.GetCompositeBitmap(layer_manager);
  (void)other_room.GetCompositeBitmap(layer_manager);
  ASSERT_FALSE(current_room.IsCompositeDirty());
  ASSERT_FALSE(other_room.IsCompositeDirty());

  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(
      panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/0, &rooms)
          .ok());
  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/1);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);
  int callback_count = 0;
  panel.SetTilesAppliedCallback([&callback_count]() { ++callback_count; });

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_EQ(callback_count, 1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(current_room.IsCompositeDirty());
  EXPECT_TRUE(other_room.IsCompositeDirty());
}

TEST(ObjectTileEditorPanelTest,
     SuccessfulCustomApplyInvalidatesAllRoomsAndSelectorPreviews) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_custom_global_refresh"));
  WriteCustomObjectAsset(custom_state.dir / "track_LR.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  auto& current_room = rooms[0];
  auto& other_room = rooms[1];
  current_room.SetLoaded(true);
  zelda3::RoomLayerManager layer_manager;
  (void)current_room.GetCompositeBitmap(layer_manager);
  (void)other_room.GetCompositeBitmap(layer_manager);
  ASSERT_FALSE(current_room.IsCompositeDirty());
  ASSERT_FALSE(other_room.IsCompositeDirty());

  DungeonObjectSelector selector(&rom);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x24, /*palette=*/2);
  panel.SetTilesAppliedCallback(
      [&selector]() { selector.InvalidatePreviewCache(); });

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(current_room.IsCompositeDirty());
  EXPECT_TRUE(other_room.IsCompositeDirty());
  EXPECT_EQ(selector.preview_cache_invalidations_for_testing(), 1u);
}

TEST(ObjectTileEditorPanelTest,
     EditorSynchronizesCustomAssetGenerationWithoutVisibleSelector) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_dungeon_editor_custom_generation"));
  WriteCustomObjectAsset(custom_state.dir / "track_LR.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonEditorV2 editor(&rom);
  auto& room = editor.rooms()[0];
  room.SetLoaded(true);
  DungeonEditorV2ObjectTileEditorTestPeer::SynchronizeCustomObjectAssets(
      editor);
  zelda3::RoomLayerManager layer_manager;
  (void)room.GetCompositeBitmap(layer_manager);
  ASSERT_FALSE(room.IsCompositeDirty());
  ASSERT_EQ(editor.object_selector_panel(), nullptr);

  zelda3::CustomObjectManager::Get().ReloadAll();
  DungeonEditorV2ObjectTileEditorTestPeer::SynchronizeCustomObjectAssets(
      editor);

  EXPECT_TRUE(room.IsCompositeDirty());
}

TEST(ObjectTileEditorPanelTest,
     EditorDestructionDetachesWorkspaceOwnedCustomObjectPanels) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  ObjectTileEditorPanel tile_editor(nullptr, &rom);
  ObjectSelectorContent object_selector(&rom, nullptr);
  auto editor = std::make_unique<DungeonEditorV2>(&rom);
  editor->rooms()[0].SetLoaded(true);
  object_selector.SetRooms(&editor->rooms());
  object_selector.object_selector().SetTileEditorPanel(&tile_editor);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
      tile_editor, /*width=*/1, /*height=*/1, /*object_id=*/0x31,
      /*room_id=*/0, &editor->rooms(), /*modified=*/false);
  tile_editor.SetStandardWritePreflightCallback(
      [](const std::vector<std::pair<uint32_t, uint32_t>>&) {
        return absl::OkStatus();
      });
  tile_editor.SetTilesAppliedCallback([]() {});
  DungeonEditorV2ObjectTileEditorTestPeer::SetObjectTileEditorPanel(
      *editor, &tile_editor);
  DungeonEditorV2ObjectTileEditorTestPeer::SetObjectSelectorPanel(
      *editor, &object_selector);

  editor.reset();

  EXPECT_FALSE(tile_editor.IsOpen());
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Rooms(tile_editor), nullptr);
  EXPECT_FALSE(
      ObjectTileEditorPanelTestAccess::HasStandardWritePreflightCallback(
          tile_editor));
  EXPECT_FALSE(
      ObjectTileEditorPanelTestAccess::HasTilesAppliedCallback(tile_editor));
  EXPECT_EQ(object_selector.object_selector().get_rooms(), nullptr);
}

TEST(ObjectTileEditorPanelTest,
     ManifestBlockedApplyPreservesRomAndModifiedRetryState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout_or = CaptureEditableStandardLayout(rom);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(
      panel, kEditableStandardObjectId);
  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/1);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x123, /*palette=*/3);

  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(ManifestProtectingPcRange(
                      kEditableSourcePcAddress, kEditableSourcePcAddress + 2))
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;

  int preflight_count = 0;
  int applied_callback_count = 0;
  std::vector<std::pair<uint32_t, uint32_t>> observed_ranges;
  panel.SetTilesAppliedCallback(
      [&applied_callback_count]() { ++applied_callback_count; });
  panel.SetStandardWritePreflightCallback(
      [&](const std::vector<std::pair<uint32_t, uint32_t>>& ranges) {
        ++preflight_count;
        observed_ranges = ranges;
        return ValidateHackManifestSaveConflicts(
            project.hack_manifest, project.rom_metadata.write_policy, ranges,
            "dungeon object tile data", "ObjectTileEditorPanelTest",
            /*toast_manager=*/nullptr);
      });

  const auto original = rom.vector();
  const bool original_dirty = rom.dirty();
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_EQ(preflight_count, 1);
  EXPECT_EQ(applied_callback_count, 0);
  EXPECT_EQ(observed_ranges,
            (std::vector<std::pair<uint32_t, uint32_t>>{
                {kEditableSourcePcAddress, kEditableSourcePcAddress + 2}}));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsError(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Write conflict with Hack Manifest"),
            std::string::npos);

  panel.SetStandardWritePreflightCallback(
      [](const std::vector<std::pair<uint32_t, uint32_t>>&) {
        return absl::OkStatus();
      });
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_NE(rom.vector(), original);
  EXPECT_EQ(applied_callback_count, 1);
}

TEST(ObjectTileEditorPanelTest, UnrelatedManifestRangeAllowsStandardApply) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout_or = CaptureEditableStandardLayout(rom);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetCurrentObjectId(
      panel, kEditableStandardObjectId);
  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/1);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x234, /*palette=*/4);

  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(ManifestProtectingPcRange(0x2000, 0x2002))
                  .ok());
  project.rom_metadata.write_policy = project::RomWritePolicy::kBlock;

  int preflight_count = 0;
  panel.SetStandardWritePreflightCallback(
      [&](const std::vector<std::pair<uint32_t, uint32_t>>& ranges) {
        ++preflight_count;
        return ValidateHackManifestSaveConflicts(
            project.hack_manifest, project.rom_metadata.write_policy, ranges,
            "dungeon object tile data", "ObjectTileEditorPanelTest",
            /*toast_manager=*/nullptr);
      });

  const int original_word = ReadWordAt(rom, kEditableSourcePcAddress);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/false);

  EXPECT_EQ(preflight_count, 1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_NE(ReadWordAt(rom, kEditableSourcePcAddress), original_word);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ActionStatusIsError(panel));
}

TEST(ObjectTileEditorPanelTest,
     SharedGuardedApplyCanWarnAgainAfterCloseAndReopen) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableStandardObjectRomData()).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  OpenInjectedSharedStandardObjectSession(panel, rom);
  ASSERT_TRUE(panel.IsOpen());
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));

  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/3);
  const auto first_layout = ObjectTileEditorPanelTestAccess::Layout(panel);
  const int first_write_addr = FirstCellSourceAddress(first_layout);
  ASSERT_GE(first_write_addr, 0);
  const int first_original_word = ReadWordAt(rom, first_write_addr);
  const uint16_t first_tile_id =
      ObjectTileEditorPanelTestAccess::FirstCellTileId(panel);
  const uint8_t first_palette =
      ObjectTileEditorPanelTestAccess::FirstCellPalette(panel);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, static_cast<uint16_t>(first_tile_id ^ 0x1),
      static_cast<uint8_t>((first_palette + 1) & 0x7));

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel, /*confirm_shared=*/true);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_EQ(ReadWordAt(rom, first_write_addr), first_original_word);

  panel.Close();

  EXPECT_FALSE(panel.IsOpen());
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedTileDataUsageOverride(panel),
            -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActionStatus(panel));

  OpenInjectedSharedStandardObjectSession(panel, rom);
  ASSERT_TRUE(panel.IsOpen());
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel),
            ObjectTileEditorPanelTestAccess::FirstCellTileId(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel),
            ObjectTileEditorPanelTestAccess::FirstCellPalette(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedTileDataUsageOverride(panel),
            -1);

  ObjectTileEditorPanelTestAccess::SetSharedTileDataUsageOverride(
      panel, /*shared_count=*/3);
  const auto reopened_layout = ObjectTileEditorPanelTestAccess::Layout(panel);
  const int reopened_write_addr = FirstCellSourceAddress(reopened_layout);
  ASSERT_GE(reopened_write_addr, 0);
  const int reopened_original_word = ReadWordAt(rom, reopened_write_addr);
  const uint16_t reopened_tile_id =
      ObjectTileEditorPanelTestAccess::FirstCellTileId(panel);
  const uint8_t reopened_palette =
      ObjectTileEditorPanelTestAccess::FirstCellPalette(panel);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, static_cast<uint16_t>(reopened_tile_id ^ 0x2),
      static_cast<uint8_t>((reopened_palette + 2) & 0x7));

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel, /*confirm_shared=*/true);

  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ShowSharedConfirm(panel));
  EXPECT_GT(ObjectTileEditorPanelTestAccess::SharedObjectCount(panel), 1);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActionStatus(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::ActionStatusIsWarning(panel));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::ActionStatusMessage(panel).find(
                "Confirm global apply"),
            std::string::npos);
  EXPECT_EQ(ReadWordAt(rom, reopened_write_addr), reopened_original_word);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesReplacesExistingSlotAndShowsExactCustomTitle) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_custom_save"));
  WriteCustomObjectAsset(custom_state.dir / "track_LR.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x24, /*palette=*/2);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(std::filesystem::exists(custom_state.dir / "track_LR.bin"));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::BuildWindowTitle(panel).find(
                "Custom 0x031:00 - track_LR.bin"),
            std::string::npos);
}

TEST(ObjectTileEditorPanelTest,
     CustomAtlasRetainsZeroTileAttributesWithoutFlippingSourceImages) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data;
  game_data.graphics_buffer.assign(2 * 4096, 0);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      game_data.graphics_buffer[y * 128 + x] = 9 + (x + 2 * y) % 6;
      game_data.graphics_buffer[4096 + y * 128 + x] = 1 + (x + 2 * y) % 6;
    }
  }
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0];
  room.SetLoaded(true);
  room.SetGameData(&game_data);
  room.mutable_blocks().fill(0);
  room.mutable_blocks()[12] = 1;
  room.CopyRoomGraphicsToBuffer();
  gfx::PaletteGroup palette;
  SeedCoordinatorPaletteGroup(&palette, 8, 16, 0);

  // Oracle skips only the entire zero source word. With ID/palette zero,
  // each retained attribute alone makes a real tile: $54 forces page $300,
  // while ordinary custom $31 still uses tile $000.
  for (const int object_id : {0x54, 0x31}) {
    for (const uint16_t attributes : {0x4000, 0x8000, 0x2000}) {
      SCOPED_TRACE(object_id);
      SCOPED_TRACE(attributes);
      ObjectTileEditorPanel panel(nullptr, &rom);
      ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(
          panel, 2, 1, object_id, 0, &rooms, false);
      auto layout = ObjectTileEditorPanelTestAccess::Layout(panel);
      layout.cells[0].tile_info = gfx::WordToTileInfo(0);
      layout.cells[0].original_word = 0;
      layout.cells[1].tile_info = gfx::WordToTileInfo(attributes);
      layout.cells[1].original_word = 0;
      layout.cells[1].modified = true;
      const auto source_bytes = layout.custom_source_bytes;
      ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
      panel.SetCurrentPaletteGroup(palette);
      ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(
          panel);
      ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);
      ASSERT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0, 0, 0),
                255);
      ASSERT_FALSE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));

      // Same tile ID and palette, different retained attributes.
      ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 1);
      ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(
          panel);
      EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
      ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);
      ObjectTileEditorPanelTestAccess::RenderObjectPreview(panel);
      const int base_color = object_id == 0x54 ? 1 : 9;
      for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
          EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0, x, y),
                    base_color + (x + 2 * y) % 6);
          const int preview_x = attributes == 0x4000 ? 7 - x : x;
          const int preview_y = attributes == 0x8000 ? 7 - y : y;
          EXPECT_EQ(
              ObjectTileEditorPanelTestAccess::PreviewPixel(panel, 8 + x, y),
              base_color + (preview_x + 2 * preview_y) % 6);
          EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewPixel(panel, x, y),
                    255);
        }
      }

      ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 0);
      ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(
          panel);
      EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
      ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);
      EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0, 0, 0),
                255);
      const auto& unchanged = ObjectTileEditorPanelTestAccess::Layout(panel);
      EXPECT_EQ(gfx::TileInfoToWord(unchanged.cells[1].tile_info), attributes);
      EXPECT_EQ(unchanged.cells[1].original_word, 0);
      EXPECT_TRUE(unchanged.cells[1].modified);
      EXPECT_EQ(unchanged.custom_source_bytes, source_bytes);
    }
  }
}

TEST(ObjectTileEditorPanelTest,
     AttributeShortcutsInvalidateCustomAtlasAndKeepSourceSelection) {
  Rom rom;
  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(panel, 1, 1, 0x54, 0,
                                                           nullptr, false);
  auto layout = ObjectTileEditorPanelTestAccess::Layout(panel);
  layout.cells[0].tile_info = gfx::WordToTileInfo(0);
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);
  ScopedImGuiContext imgui_context;
  const auto frame = [&] {
    ImGui::NewFrame();
    ImGui::SetNextWindowFocus();
    ImGui::Begin("ObjectTileEditorAttributeShortcutHost");
    ObjectTileEditorPanelTestAccess::HandleKeyboardShortcuts(panel);
    ImGui::End();
    ImGui::Render();
  };
  frame();
  for (const auto [key, attributes] :
       {std::pair{ImGuiKey_H, 0x4000}, std::pair{ImGuiKey_V, 0x8000},
        std::pair{ImGuiKey_P, 0x2000}}) {
    SCOPED_TRACE(key);
    for (const int expected_word : {attributes, 0}) {
      ObjectTileEditorPanelTestAccess::SetAtlasDirty(panel, false);
      ImGui::GetIO().AddKeyEvent(key, true);
      frame();
      EXPECT_EQ(
          gfx::TileInfoToWord(ObjectTileEditorPanelTestAccess::Layout(panel)
                                  .cells[0]
                                  .tile_info),
          expected_word);
      EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
      EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0);
      EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
      ImGui::GetIO().AddKeyEvent(key, false);
      frame();
    }
  }
}

TEST(ObjectTileEditorPanelTest,
     PriorityCheckboxInvalidatesCustomAtlasWithZeroTileAndPalette) {
  Rom rom;
  ObjectTileEditorPanel panel(nullptr, &rom);
  ObjectTileEditorPanelTestAccess::OpenCustomLayoutForTest(panel, 1, 1, 0x54, 0,
                                                           nullptr, false);
  auto layout = ObjectTileEditorPanelTestAccess::Layout(panel);
  layout.cells[0].tile_info = gfx::WordToTileInfo(0);
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);
  ScopedImGuiContext imgui_context;
  ImVec2 checkbox_center;
  const auto frame = [&] {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(800, 200));
    ImGui::Begin("ObjectTileEditorAttributeCheckboxHost");
    ObjectTileEditorPanelTestAccess::DrawTileProperties(panel);
    // Priority is the final property control; capture its real hit rectangle.
    const ImVec2 minimum = ImGui::GetItemRectMin();
    const ImVec2 maximum = ImGui::GetItemRectMax();
    checkbox_center =
        ImVec2((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
    ImGui::End();
    ImGui::Render();
  };
  frame();
  for (const int expected_word : {0x2000, 0}) {
    ObjectTileEditorPanelTestAccess::SetAtlasDirty(panel, false);
    ImGui::GetIO().AddMousePosEvent(checkbox_center.x, checkbox_center.y);
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
    EXPECT_EQ(
        gfx::TileInfoToWord(
            ObjectTileEditorPanelTestAccess::Layout(panel).cells[0].tile_info),
        expected_word);
    EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
    EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0);
    EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  }
}

TEST(ObjectTileEditorPanelTest,
     CustomSpriteBodyAtlasMatchesRuntimePreviewWithoutChangingSourceWords) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_runtime_atlas"));
  // First source word of Oracle's manhandla_body_1a.bin. SpriteObjectsDraw
  // ORs nonzero words with $0300: raw tile $10D becomes runtime tile $30D.
  constexpr uint16_t kSourceWord = 0x1D0D;
  WriteCustomObjectAsset(custom_state.dir / "manhandla_body_1a.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, kSourceWord}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data;
  game_data.graphics_buffer.assign(2 * 4096, 3);
  std::fill(game_data.graphics_buffer.begin() + 4096,
            game_data.graphics_buffer.end(), 7);
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0];
  room.SetLoaded(true);
  room.SetGameData(&game_data);
  room.mutable_blocks().fill(0);
  room.mutable_blocks()[12] = 1;
  room.CopyRoomGraphicsToBuffer();

  gfx::PaletteGroup palette;
  SeedCoordinatorPaletteGroup(&palette, 8, 16, 0);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel.OpenForCustomObject(0x54, 1, 0, &rooms, palette).ok());
  const auto original_layout = ObjectTileEditorPanelTestAccess::Layout(panel);

  // Switching away from a body object must restore the ordinary atlas. Keep
  // the same raw tile/palette to isolate runtime-page handling from color.
  for (const int object_id : {0x54, 0x31, 0x32, 0x11F, 0x54}) {
    SCOPED_TRACE(object_id);
    auto layout = original_layout;
    layout.object_id = object_id;
    layout.is_custom = object_id != 0x11F;
    ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));
    ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);
    ObjectTileEditorPanelTestAccess::RenderObjectPreview(panel);
    ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);
    ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
    ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
    const int expected_color = object_id == 0x54 ? 7 : 3;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        EXPECT_EQ(
            ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0x10D, x, y),
            expected_color);
        EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewPixel(panel, x, y),
                  7 * 16 + expected_color);
      }
    }
    EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel),
              0x10D);
    EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 7);
    const auto& unchanged = ObjectTileEditorPanelTestAccess::Layout(panel);
    EXPECT_EQ(gfx::TileInfoToWord(unchanged.cells[0].tile_info), kSourceWord);
    EXPECT_EQ(unchanged.cells[0].original_word, kSourceWord);
    EXPECT_EQ(unchanged.custom_source_bytes,
              original_layout.custom_source_bytes);
    EXPECT_FALSE(unchanged.HasModifications());
  }

  // Source tile zero still uses runtime tile $300 when its palette makes the
  // word nonzero. The all-zero source word remains a no-op, not tile $300.
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0, 0, 0), 7);
  ObjectTileEditorPanelTestAccess::SetSourcePalette(panel, 0);
  ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0, 0, 0), 255);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::FirstCellTileId(panel), 0x10D);
}

TEST(ObjectTileEditorPanelTest,
     DrawRefreshesRoomGraphicsWithoutDiscardingUnsavedCustomTiles) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_graphics_refresh"));
  WriteCustomObjectAsset(custom_state.dir / "ice_chair.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x0986}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data;
  game_data.graphics_buffer.assign(4096, 3);
  DungeonRoomStore rooms(&rom);
  auto& room = rooms[0];
  room.SetLoaded(true);
  room.SetGameData(&game_data);
  room.mutable_blocks().fill(0);
  room.CopyRoomGraphicsToBuffer();

  gfx::PaletteGroup palette;
  SeedCoordinatorPaletteGroup(&palette, 8, 16, 0);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel.OpenForCustomObject(0x32, 2, 0, &rooms, palette).ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(panel, 0x196, 2);
  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);
  const auto original_layout = ObjectTileEditorPanelTestAccess::Layout(panel);

  ScopedImGuiContext imgui_context;
  const auto draw = [&] {
    bool open = true;
    ImGui::NewFrame();
    ImGui::Begin("ObjectTileEditorGraphicsRefreshHost");
    panel.Draw(&open);
    ImGui::End();
    ImGui::Render();
    EXPECT_TRUE(open);
  };
  draw();
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
  ASSERT_EQ(ObjectTileEditorPanelTestAccess::PreviewPixel(panel, 0, 0), 35);
  ASSERT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0x196, 0, 0), 3);

  // The room ID, headers and block IDs are unchanged; only decoded sheet
  // contents change, as when the graphics editor reloads a source sheet.
  game_data.graphics_buffer.assign(4096, 5);
  room.CopyRoomGraphicsToBuffer();
  draw();

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewPixel(panel, 0, 0), 37);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPixel(panel, 0x196, 0, 0), 5);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::PreviewDirty(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0x196);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  const auto& unchanged = ObjectTileEditorPanelTestAccess::Layout(panel);
  EXPECT_EQ(gfx::TileInfoToWord(unchanged.cells[0].tile_info), 0x0996);
  EXPECT_EQ(unchanged.cells[0].original_word, 0x0986);
  EXPECT_EQ(unchanged.custom_source_bytes, original_layout.custom_source_bytes);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesWithRoomContextRefreshesPreviewAndAtlasImmediately) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_apply_refresh"));
  WriteCustomObjectAsset(
      custom_state.dir / "track_LR.bin",
      zelda3::CustomObject{
          .tiles = {
              {0, 0, 0x2810}, {1, 0, 0x2811}, {0, 1, 0x2820}, {1, 1, 0x2821}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.SetCurrentPaletteGroup(MakeTestPaletteGroup(/*base=*/0));
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x24, /*palette=*/2);

  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::PreviewDirty(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewWidth(panel), 16);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewHeight(panel), 16);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasWidth(panel),
            zelda3::ObjectTileEditor::kAtlasWidthPx);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasHeight(panel),
            zelda3::ObjectTileEditor::kAtlasHeightPx);
}

TEST(ObjectTileEditorPanelTest,
     PaletteChangeAfterApplyRefreshesPreviewAndAtlasWithoutStalePaletteData) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_palette_refresh"));
  WriteCustomObjectAsset(custom_state.dir / "track_LR.bin",
                         zelda3::CustomObject{.tiles = {{0, 0, 0x2810}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);

  ObjectTileEditorPanel panel(nullptr, &rom);
  const auto first_palette_group = MakeTestPaletteGroup(/*base=*/0);
  const auto second_palette_group = MakeTestPaletteGroup(/*base=*/20);
  panel.SetCurrentPaletteGroup(first_palette_group);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());
  ObjectTileEditorPanelTestAccess::SetFirstCellTileAndPalette(
      panel, /*tile_id=*/0x18, /*palette=*/2);

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewPaletteColor(panel, 0),
            first_palette_group.palette_ref(0)[0].snes());
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPaletteColor(panel, 0),
            first_palette_group.palette_ref(2)[0].snes());

  panel.SetCurrentPaletteGroup(second_palette_group);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::PreviewDirty(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));

  ObjectTileEditorPanelTestAccess::RenderObjectPreview(panel);
  ObjectTileEditorPanelTestAccess::RenderTile8Atlas(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::PreviewDirty(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::PreviewPaletteColor(panel, 0),
            second_palette_group.palette_ref(0)[0].snes());
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::AtlasPaletteColor(panel, 0),
            second_palette_group.palette_ref(2)[0].snes());
}

TEST(ObjectTileEditorPanelTest,
     TerminatorOnlyCustomAssetStaysOpenAndSupportsAddRevertApply) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_empty_slot"));
  const auto asset_path = custom_state.dir / "track_LR.bin";
  WriteCustomObjectAsset(asset_path, zelda3::CustomObject{});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomStore rooms(&rom);
  rooms[0].SetLoaded(true);
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForCustomObject(/*object_id=*/0x31, /*subtype=*/0,
                                       /*room_id=*/0, &rooms)
                  .ok());
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));

  {
    ScopedImGuiContext imgui_context;
    bool open = true;
    ImGui::NewFrame();
    ImGui::Begin("ObjectTileEditorEmptySlotHost");
    panel.Draw(&open);
    ImGui::End();
    ImGui::Render();
    EXPECT_TRUE(open);
    EXPECT_TRUE(panel.IsOpen());
  }

  ASSERT_TRUE(
      ObjectTileEditorPanelTestAccess::AddFirstTileToEmptyCustomLayout(panel)
          .ok());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));

  ObjectTileEditorPanelTestAccess::RevertCurrentLayout(panel);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));

  ASSERT_TRUE(
      ObjectTileEditorPanelTestAccess::AddFirstTileToEmptyCustomLayout(panel)
          .ok());
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));

  auto decoded_or =
      zelda3::DecodeCustomObjectBinary(ReadBinaryFile(asset_path));
  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();
  ASSERT_EQ(decoded_or->tiles.size(), 1u);
  EXPECT_EQ(decoded_or->tiles.front().rel_x, 0);
  EXPECT_EQ(decoded_or->tiles.front().rel_y, 0);
  EXPECT_NE(decoded_or->tiles.front().tile_data, 0);
}

}  // namespace
}  // namespace yaze::editor
