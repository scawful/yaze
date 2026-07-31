#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"

#include <chrono>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_object.h"

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

  static void SetCurrentObjectId(ObjectTileEditorPanel& panel,
                                 int16_t object_id) {
    panel.current_object_id_ = object_id;
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

  static int FirstCellWriteIndex(const ObjectTileEditorPanel& panel) {
    EXPECT_FALSE(panel.current_layout_.cells.empty());
    return panel.current_layout_.cells.front().write_index;
  }

  static int SharedTileDataUsageCount(const ObjectTileEditorPanel& panel) {
    return panel.GetSharedTileDataUsageCount();
  }

  static bool HasSharedTileDataConflict(const ObjectTileEditorPanel& panel) {
    return panel.HasSharedTileDataConflict();
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
  return data;
}

absl::StatusOr<zelda3::ObjectTileLayout> CaptureEditableStandardLayout(
    Rom& rom) {
  zelda3::Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  zelda3::ObjectTileEditor editor(&rom);
  return editor.CaptureEditableObjectLayout(kEditableStandardObjectId, room,
                                            palette);
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

std::optional<int16_t> FindCapturableObjectId(Rom* rom, DungeonRoomStore* rooms,
                                              int start_id = 0,
                                              int end_id = 0x1FF) {
  ObjectTileEditorPanel probe(nullptr, rom);
  for (int id = start_id; id <= end_id; ++id) {
    probe.OpenForObject(static_cast<int16_t>(id), /*room_id=*/0, rooms);
    if (ObjectTileEditorPanelTestAccess::HasLayout(probe)) {
      return static_cast<int16_t>(id);
    }
  }
  return std::nullopt;
}

void OpenInjectedSharedStandardObjectSession(ObjectTileEditorPanel& panel,
                                             Rom& rom) {
  panel.OpenForObject(kEditableStandardObjectId, /*room_id=*/-1, nullptr);

  auto layout_or = CaptureEditableStandardLayout(rom);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(*layout_or));
  ObjectTileEditorPanelTestAccess::SetSelectedCellIndex(panel, 0);
  ObjectTileEditorPanelTestAccess::SyncSourceSelectionFromSelectedCell(panel);
}

TEST(ObjectTileEditorPanelTest, OpenForObjectInvalidRoomClearsPreviousLayout) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  DungeonRoomStore rooms(&rom);
  panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/-1, &rooms);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest,
     OpenForObjectCaptureFailureClearsPreviousLayout) {
  Rom rom;

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "custom.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  panel.OpenForObject(/*object_id=*/0x40, /*room_id=*/0, &rooms);

  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedCellIndex(panel), -1);
  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SelectedSourceTile(panel), -1);
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActivePreview(panel));
  EXPECT_FALSE(ObjectTileEditorPanelTestAccess::HasActiveAtlas(panel));
}

TEST(ObjectTileEditorPanelTest, CloseClearsTransientStateAndContext) {
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

TEST(ObjectTileEditorPanelTest, OpenForNewObjectClearsPendingSharedConfirm) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForNewObject(/*width=*/2, /*height=*/2, "first.bin",
                         /*object_id=*/0x123, /*room_id=*/0, nullptr);
  ObjectTileEditorPanelTestAccess::SeedTransientState(panel);
  ObjectTileEditorPanelTestAccess::SeedRenderedBitmaps(panel);

  panel.OpenForNewObject(/*width=*/1, /*height=*/1, "second.bin",
                         /*object_id=*/0x124, /*room_id=*/1, nullptr);

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
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonRoomStore rooms(&rom);
  (void)rooms[0];

  auto object_id = FindCapturableObjectId(&rom, &rooms);
  ASSERT_TRUE(object_id.has_value());

  ObjectTileEditorPanel panel(nullptr, &rom);
  panel.OpenForObject(*object_id, /*room_id=*/0, &rooms);

  ASSERT_TRUE(ObjectTileEditorPanelTestAccess::HasLayout(panel));
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

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel),
            0);
  EXPECT_FALSE(
      ObjectTileEditorPanelTestAccess::HasSharedTileDataConflict(panel));
}

TEST(ObjectTileEditorPanelTest,
     SharedTileDataUsageCountIgnoresStandardLayoutsWithoutTileDataAddress) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditorPanel panel(nullptr, &rom);
  auto layout = zelda3::ObjectTileLayout::CreateEmpty(
      /*width=*/1, /*height=*/1, /*object_id=*/0x40, "custom.bin");
  layout.is_custom = false;
  layout.custom_filename.clear();
  layout.tile_data_address = -1;
  ObjectTileEditorPanelTestAccess::SetLayout(panel, std::move(layout));

  EXPECT_EQ(ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel),
            0);
  EXPECT_FALSE(
      ObjectTileEditorPanelTestAccess::HasSharedTileDataConflict(panel));
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

  ASSERT_GT(ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel),
            1);

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
                "Confirm shared apply"),
            std::string::npos);
  EXPECT_TRUE(ObjectTileEditorPanelTestAccess::HasModifications(panel));
  EXPECT_EQ(ReadWordAt(rom, write_addr), original_word);
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

  ASSERT_GT(ObjectTileEditorPanelTestAccess::SharedTileDataUsageCount(panel),
            1);

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
  std::vector<std::pair<uint32_t, uint32_t>> observed_ranges;
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
                "Confirm shared apply"),
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
