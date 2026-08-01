#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"

#include <chrono>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_object.h"
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

  static bool IsNewObject(const ObjectTileEditorPanel& panel) {
    return panel.is_new_object_;
  }

  static void ApplyChanges(ObjectTileEditorPanel& panel) {
    panel.ApplyChanges();
  }

  static void ApplyChanges(ObjectTileEditorPanel& panel, bool confirm_shared) {
    panel.ApplyChanges(confirm_shared);
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
        dir(std::move(temp_dir)) {
    std::filesystem::create_directories(dir);
    zelda3::CustomObjectManager::Get().Initialize(dir.string());
  }

  ~ScopedCustomObjectState() {
    zelda3::CustomObjectManager::Get().RestoreState(old_state);
    std::filesystem::remove_all(dir);
  }

  zelda3::CustomObjectManager::State old_state;
  std::filesystem::path dir;
};

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

  auto expected_palette = game_data.palette_groups.dungeon_main.palette_ref(3);
  auto expected_group =
      gfx::CreatePaletteGroupFromLargePalette(expected_palette);
  ASSERT_TRUE(expected_group.ok()) << expected_group.status();
  auto default_palette = game_data.palette_groups.dungeon_main.palette_ref(0);
  auto default_group = gfx::CreatePaletteGroupFromLargePalette(default_palette);
  ASSERT_TRUE(default_group.ok()) << default_group.status();
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentPaletteColor(
                *panel_ptr, /*palette=*/0, /*color=*/0),
            expected_group->GetColor(/*palette=*/0, /*color=*/0).snes());
  EXPECT_NE(ObjectTileEditorPanelTestAccess::CurrentPaletteColor(
                *panel_ptr, /*palette=*/0, /*color=*/0),
            default_group->GetColor(/*palette=*/0, /*color=*/0).snes());
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
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
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
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x123);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectCaptureFailurePreservesPreviousLayout) {
  Rom rom;

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
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
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x123);
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
  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "custom.bin",
                         /*object_id=*/0x31, /*room_id=*/0, &rooms);

  const absl::Status status =
      panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/0, &rooms);

  EXPECT_TRUE(absl::IsUnimplemented(status));
  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::IsNewObject(panel));
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
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/5, &rooms);
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

TEST(ObjectTileEditorPanelTest, OpenForNewObjectClearsPendingSharedConfirm) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "first.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ObjectTileEditorPanelTestAccess::ClearModifications(panel);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);

  const absl::Status status = panel.OpenForNewObject(
      /*width=*/1, /*height=*/1, "second.bin", /*object_id=*/0x124,
      /*room_id=*/1, nullptr);

  ASSERT_TRUE(status.ok()) << status;
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
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x124);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForNewObjectRejectsReplacingModifiedSession) {
  Rom rom;
  ObjectTileEditorPanel panel(nullptr, &rom);
  ASSERT_TRUE(panel
                  .OpenForNewObject(/*width=*/2, /*height=*/2, "first.bin",
                                    /*object_id=*/0x123, /*room_id=*/0, nullptr)
                  .ok());
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));

  const absl::Status status = panel.OpenForNewObject(
      /*width=*/1, /*height=*/1, "second.bin", /*object_id=*/0x124,
      /*room_id=*/1, nullptr);

  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentObjectId(panel), 0x123);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::CurrentRoomId(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::Layout(panel).custom_filename,
            "first.bin");
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForNewObjectSelectsFirstCellAndKeepsDefaultPaletteInSync) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "selected.bin",
                         /*object_id=*/0x124, /*room_id=*/1, nullptr);

  EXPECT_TRUE(panel.IsOpen());
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), 0);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SourcePalette(panel), 2);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::AtlasDirty(panel));
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
     SharedTileDataUsageCountIgnoresCustomLayoutsEvenWithTileDataAddress) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
      /*width=*/1, /*height=*/1, /*object_id=*/0x40, "custom.bin");
  layout.tile_data_address = 0x1234;
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));

  auto usage_count_or =
      ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel);
  ASSERT_TRUE(usage_count_or.ok()) << usage_count_or.status();
  EXPECT_EQ(*usage_count_or, 0);
  auto conflict_or =
      ObjectTileEditorPanelTestAccess::HasSharedTileDataConflict(panel);
  ASSERT_TRUE(conflict_or.ok()) << conflict_or.status();
  EXPECT_FALSE(*conflict_or);
}

TEST(ObjectTileEditorPanelTest,
     SharedTileDataUsageCountRejectsStandardLayoutsWithoutProvenance) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
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
  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
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
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
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
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
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
  panel.SetStandardTilesAppliedCallback(
      [&callback_count]() { ++callback_count; });

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel,
                                                /*confirm_shared=*/true);

  EXPECT_EQ(callback_count, 1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_TRUE(current_room.IsCompositeDirty());
  EXPECT_TRUE(other_room.IsCompositeDirty());
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
  panel.SetStandardTilesAppliedCallback(
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
     ApplyChangesWithoutCallbackLeavesNewObjectModeAndShowsCustomTitle) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_custom_save"));

  Rom rom;
  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "fresh_custom.bin",
                         /*object_id=*/0x31, /*room_id=*/0, nullptr);

  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::IsNewObject(panel));
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::IsNewObject(panel));
  EXPECT_TRUE(std::filesystem::exists(custom_state.dir / "fresh_custom.bin"));
  EXPECT_NE(ObjectTileEditorPanelTestAccess::BuildWindowTitle(panel).find(
                "Custom Object 0x031 - fresh_custom.bin"),
            std::string::npos);
}

TEST(ObjectTileEditorPanelTest, ApplyChangesForNewObjectFiresCallbackOnce) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_custom_callback"));

  Rom rom;
  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "callback_custom.bin",
                         /*object_id=*/0x31, /*room_id=*/0, nullptr);

  int callback_count = 0;
  int callback_object_id = -1;
  std::string callback_filename;
  panel.SetObjectCreatedCallback(
      [&](int object_id, const std::string& filename) {
        ++callback_count;
        callback_object_id = object_id;
        callback_filename = filename;
      });

  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);
  ASSERT_EQ(callback_count, 1);
  EXPECT_EQ(callback_object_id, 0x31);
  EXPECT_EQ(callback_filename, "callback_custom.bin");
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::IsNewObject(panel));

  ObjectTileEditorPanelTestAccess::MarkFirstCellModified(panel);
  ObjectTileEditorPanelTestAccess::ApplyChanges(panel);
  EXPECT_EQ(callback_count, 1);
}

TEST(ObjectTileEditorPanelTest,
     ApplyChangesWithRoomContextRefreshesPreviewAndAtlasImmediately) {
  ScopedCustomObjectState custom_state(
      MakeTempDir("yaze_obj_tile_panel_apply_refresh"));

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.SetCurrentPaletteGroup(MakeTestPaletteGroup(/*base=*/0));
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "apply_refresh.bin",
                         /*object_id=*/0x31, /*room_id=*/0, &rooms);
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

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  ObjectTileEditorPanel panel(nullptr, &rom);
  const auto first_palette_group = MakeTestPaletteGroup(/*base=*/0);
  const auto second_palette_group = MakeTestPaletteGroup(/*base=*/20);
  panel.SetCurrentPaletteGroup(first_palette_group);
  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "palette_refresh.bin",
                         /*object_id=*/0x31, /*room_id=*/0, &rooms);
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

}  // namespace
}  // namespace yaze::editor
