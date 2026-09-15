#include "zelda3/dungeon/object_tile_editor.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/features.h"
#include "rom/rom.h"
#include "rom/write_fence.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {
namespace {

gfx::PaletteGroup MakeTestPaletteGroup() {
  gfx::PaletteGroup group("test");

  gfx::SnesPalette pal0;
  pal0.AddColor(gfx::SnesColor(1, 2, 3));
  pal0.AddColor(gfx::SnesColor(4, 5, 6));
  group.AddPalette(pal0);

  gfx::SnesPalette pal1;
  pal1.AddColor(gfx::SnesColor(7, 8, 9));
  pal1.AddColor(gfx::SnesColor(10, 11, 12));
  group.AddPalette(pal1);

  gfx::SnesPalette pal2;
  pal2.AddColor(gfx::SnesColor(13, 14, 15));
  pal2.AddColor(gfx::SnesColor(16, 17, 18));
  group.AddPalette(pal2);

  return group;
}

struct EditableObjectFixture {
  int16_t object_id;
  uint32_t descriptor_pc_address;
  uint16_t descriptor_word;
  uint32_t source_pc_address;
  std::array<uint16_t, 4> source_words;
};

constexpr std::array<EditableObjectFixture, 2> kEditableObjectFixtures = {{
    {/*object_id=*/0x11F,
     /*descriptor_pc_address=*/0x842E,
     /*descriptor_word=*/0x0E9A,
     /*source_pc_address=*/0x29EC,
     /*source_words=*/{0x0DEE, 0x8DEE, 0x4DEE, 0xCDEE}},
    {/*object_id=*/0x120,
     /*descriptor_pc_address=*/0x8430,
     /*descriptor_word=*/0x0ECA,
     /*source_pc_address=*/0x2A1C,
     /*source_words=*/{0x0DC0, 0x0DC1, 0x4DC0, 0x4DC1}},
}};

void StoreWord(std::vector<uint8_t>& data, uint32_t address, uint16_t word) {
  data[address] = static_cast<uint8_t>(word & 0xFF);
  data[address + 1] = static_cast<uint8_t>(word >> 8);
}

std::vector<uint8_t> MakeEditableObjectRomData() {
  std::vector<uint8_t> data(0x200000, 0);
  for (const auto& fixture : kEditableObjectFixtures) {
    StoreWord(data, fixture.descriptor_pc_address, fixture.descriptor_word);
    for (size_t index = 0; index < fixture.source_words.size(); ++index) {
      StoreWord(data, fixture.source_pc_address + index * 2,
                fixture.source_words[index]);
    }
  }
  return data;
}

void StoreWordWithoutDirtying(Rom& rom, uint32_t address, uint16_t word) {
  StoreWord(rom.mutable_vector(), address, word);
}

class ScopedCustomObjectsDisabled {
 public:
  ScopedCustomObjectsDisabled()
      : previous_(core::FeatureFlags::get().kEnableCustomObjects) {
    core::FeatureFlags::get().kEnableCustomObjects = false;
  }

  ~ScopedCustomObjectsDisabled() {
    core::FeatureFlags::get().kEnableCustomObjects = previous_;
  }

 private:
  bool previous_;
};

class ScopedCustomObjectDirectory {
 public:
  explicit ScopedCustomObjectDirectory(const std::string& prefix)
      : previous_state_(CustomObjectManager::Get().SnapshotState()) {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            (prefix + "_" + std::to_string(nonce));
    std::error_code cleanup_error;
    std::filesystem::remove_all(path_, cleanup_error);
    ready_ = std::filesystem::create_directories(path_);
    CustomObjectManager::Get().Initialize(path_.string());
    CustomObjectManager::Get().ClearObjectFileMap();
  }

  ~ScopedCustomObjectDirectory() {
    CustomObjectManager::Get().RestoreState(previous_state_);
    std::error_code cleanup_error;
    std::filesystem::remove_all(path_, cleanup_error);
  }

  const std::filesystem::path& path() const { return path_; }
  bool ready() const { return ready_; }

 private:
  CustomObjectManager::State previous_state_;
  std::filesystem::path path_;
  bool ready_ = false;
};

std::vector<uint8_t> ReadTestBinary(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void WriteTestBinary(const std::filesystem::path& path,
                     const std::vector<uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary);
  ASSERT_TRUE(output.is_open());
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(output.good());
}

void WriteTestCustomObject(const std::filesystem::path& path,
                           const CustomObject& object) {
  auto bytes_or = EncodeCustomObjectBinary(object);
  ASSERT_TRUE(bytes_or.ok()) << bytes_or.status();
  WriteTestBinary(path, *bytes_or);
}

// Pins ObjectTileEditor::CaptureObjectLayout against the canonical
// ObjectGeometry bounds for routines that draw upward or leftward. The
// preview pipeline previously anchored at hardcoded (2, 2); routines
// like acute diagonals (0x09-0x14 / 0x15-0x20) and diagonal ceilings
// (0xA0-0xAC) wrote tiles at negative tile coordinates, which
// DrawRoutineUtils::WriteTile8 drops via IsValidTilePosition before the trace
// hook fires. The selector
// preview, tooltip cell grid, and ObjectTileEditor panel all consume
// CaptureObjectLayout output, so previews of those object families
// were silently clipped (e.g. 0xA3 BottomRight diagonal ceiling
// rendered at half its real extent).
//
// The fix routes CaptureObjectLayout's anchor through
// ObjectGeometry::ResolveAnchor, which uses the same logic that
// MeasureRoutine uses internally. This test pins parity in both
// directions: a future regression that reverts the anchor or breaks
// the dispatch will surface as a bounds mismatch here.
TEST(ObjectTileEditorTest, CaptureLayoutBoundsMatchObjectGeometry) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette = MakeTestPaletteGroup();
  ObjectTileEditor editor(&rom);

  // Mix anchor-sensitive object families with one anchor-insensitive
  // baseline to confirm the parity holds for both:
  //   0x09: acute diagonal (Diagonal category, routine 5) -> upward.
  //   0x12: diagonal grave BothBG (routine 6) -> downward, baseline.
  //   0xA3: diagonal ceiling BottomRight (routine 78) -> up + left.
  //   0xF86: single-tile somaria path piece -> subtype-3 baseline.
  //   0x33: 4x4 block rightward (routine 16) -> baseline, anchor (0,0).
  for (int16_t object_id : {int16_t{0x09}, int16_t{0x12}, int16_t{0x33},
                            int16_t{0xA3}, int16_t{0xF86}}) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);

    // The compatibility overload uses the canonical placement default for
    // each object family. ObjectGeometry must measure the same persisted size
    // that CaptureObjectLayout uses for its trace.
    RoomObject geom_obj(object_id, 0, 0,
                        DefaultRoomObjectSizeForPlacement(object_id), 0);
    auto geom_or = ObjectGeometry::Get().MeasureByObjectId(geom_obj);
    ASSERT_TRUE(geom_or.ok());

    auto layout_or = editor.CaptureObjectLayout(object_id, room, palette);
    ASSERT_TRUE(layout_or.ok());
    EXPECT_EQ(layout_or->bounds_width, geom_or->width_tiles)
        << "CaptureObjectLayout bounds width must match ObjectGeometry";
    EXPECT_EQ(layout_or->bounds_height, geom_or->height_tiles)
        << "CaptureObjectLayout bounds height must match ObjectGeometry";
  }
}

TEST(ObjectTileEditorTest,
     StaticWaterIcePreviewsMatchDirectObjectPixelsAndSourceMotif) {
  ScopedCustomObjectsDisabled custom_objects_disabled;
  constexpr std::array<int16_t, 11> kObjectIds = {
      0xC8, 0xC9, 0xCA, 0xD1, 0xD2, 0xD9, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7};
  constexpr std::array<uint16_t, 8> kSourceWords = {
      0x09B0, 0x4DB1, 0x91B2, 0xD5B3, 0x39B4, 0x7DB5, 0xA9B6, 0xEDB7};
  constexpr uint16_t kSourceOffset = 0x0500;

  std::vector<uint8_t> rom_data(0x200000, 0);
  for (const int16_t object_id : kObjectIds) {
    StoreWord(rom_data, kRoomObjectSubtype1 + object_id * 2, kSourceOffset);
  }
  for (size_t i = 0; i < kSourceWords.size(); ++i) {
    StoreWord(rom_data, kRoomObjectTileAddress + kSourceOffset + i * 2,
              kSourceWords[i]);
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::move(rom_data)).ok());
  Room room(/*room_id=*/0xCE, &rom, /*game_data=*/nullptr);
  auto& room_gfx =
      const_cast<std::array<uint8_t, 0x10000>&>(room.get_gfx_buffer());
  room_gfx.fill(0);
  for (int slot = 0; slot < 8; ++slot) {
    const int tile_id = 0x1B0 + slot;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        // Asymmetric pixels expose either swapped flip bit. Zero pixels must
        // remain transparent, including in the final palette bank.
        room_gfx[(tile_id / 16) * 1024 + (tile_id % 16) * 8 + y * 128 + x] =
            static_cast<uint8_t>((slot * 3 + x + y * 2) % 16);
      }
    }
  }
  gfx::PaletteGroup palette("static_water_ice");
  for (int bank = 0; bank < 8; ++bank) {
    gfx::SnesPalette colors;
    for (int color = 0; color < 16; ++color) {
      colors.AddColor(
          gfx::SnesColor(static_cast<uint16_t>(0x0100 + bank * 16 + color)));
    }
    palette.AddPalette(colors);
  }

  ObjectTileEditor editor(&rom);
  // This is static preview consistency, not independent emulator parity.
  // USDASM $018FA5 expands each two-bit dimension by one 4x4 block;
  // $018A44 repeats its eight source words as rows [0..3; 4..7; 0..3; 4..7].
  // Include minimum/maximum sizes and the sizes of Oracle's ice witnesses.
  for (const int16_t object_id : kObjectIds) {
    for (const uint8_t size : {0, 5, 10, 15}) {
      SCOPED_TRACE(::testing::Message()
                   << "object=0x" << std::hex << object_id
                   << " size=" << std::dec << static_cast<int>(size));
      const int width_tiles = (((size >> 2) & 3) + 1) * 4;
      const int height_tiles = ((size & 3) + 1) * 4;
      auto layout_or =
          editor.CaptureObjectLayout(object_id, room, palette, size);
      ASSERT_TRUE(layout_or.ok()) << layout_or.status();
      const auto& layout = *layout_or;
      ASSERT_EQ(layout.bounds_width, width_tiles);
      ASSERT_EQ(layout.bounds_height, height_tiles);
      ASSERT_EQ(layout.cells.size(), width_tiles * height_tiles);
      for (const auto& cell : layout.cells) {
        EXPECT_EQ(gfx::TileInfoToWord(cell.tile_info),
                  kSourceWords[(cell.rel_y % 2) * 4 + cell.rel_x % 4]);
      }

      gfx::Bitmap preview;
      ASSERT_TRUE(
          editor.RenderLayoutToBitmap(layout, preview, room_gfx.data(), palette)
              .ok());
      const int width = width_tiles * 8;
      const int height = height_tiles * 8;
      ASSERT_EQ(preview.width(), width);
      ASSERT_EQ(preview.height(), height);
      ASSERT_EQ(preview.palette().size(), 128u);
      for (int bank = 0; bank < 8; ++bank) {
        for (int color = 0; color < 16; ++color) {
          EXPECT_EQ(preview.palette()[bank * 16 + color].snes(),
                    palette.palette_ref(bank)[color].snes());
        }
      }
      std::vector<uint8_t> expected(width * height);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const int slot = ((y / 8) % 2) * 4 + (x / 8) % 4;
          const uint16_t word = kSourceWords[slot];
          const int source_x = (word & 0x4000) ? 7 - x % 8 : x % 8;
          const int source_y = (word & 0x8000) ? 7 - y % 8 : y % 8;
          const int pixel = (slot * 3 + source_x + source_y * 2) % 16;
          expected[y * width + x] =
              pixel == 0 ? 255 : pixel + ((word >> 10) & 7) * 16;
        }
      }
      EXPECT_EQ(preview.vector(), expected);

      for (const uint8_t layer : {0, 1, 2}) {
        SCOPED_TRACE(::testing::Message()
                     << "stream=" << static_cast<int>(layer));
        // Off-grid placement must only translate this object-relative motif.
        RoomObject object(object_id, /*x=*/29, /*y=*/23, size, layer);
        gfx::BackgroundBuffer bg1(512, 512);
        gfx::BackgroundBuffer bg2(512, 512);
        bg1.EnsureBitmapInitialized();
        bg2.EnsureBitmapInitialized();
        ObjectDrawer drawer(&rom, room.id(), room_gfx.data());
        ASSERT_TRUE(drawer.DrawObject(object, bg1, bg2, palette).ok());
        const auto& direct = (layer == 1 ? bg2 : bg1).bitmap().vector();
        std::vector<uint8_t> cropped(width * height);
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            cropped[y * width + x] = direct[(23 * 8 + y) * 512 + 29 * 8 + x];
          }
        }
        EXPECT_EQ(cropped, expected);
        EXPECT_EQ(cropped, preview.vector());
      }
    }
  }
}

TEST(ObjectTileEditorTest, CaptureLayoutUsesRequestedOracleCustomSubtype) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  const auto unique_suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path temp_base =
      std::filesystem::temp_directory_path() /
      ("yaze_test_custom_preview_subtypes_" + std::to_string(unique_suffix));
  std::filesystem::create_directories(temp_base);
  struct Cleanup {
    bool old_custom_objects_flag;
    CustomObjectManager::State old_custom_object_state;
    std::filesystem::path temp_base;
    ~Cleanup() {
      core::FeatureFlags::get().kEnableCustomObjects = old_custom_objects_flag;
      CustomObjectManager::Get().RestoreState(old_custom_object_state);
      std::filesystem::remove_all(temp_base);
    }
  } cleanup{old_custom_objects_flag, old_custom_object_state, temp_base};

  const auto write_object = [&](const std::string& filename,
                                const std::vector<uint16_t>& tile_words) {
    std::ofstream file(temp_base / filename, std::ios::binary);
    const uint16_t header = static_cast<uint16_t>(tile_words.size());
    file.put(static_cast<char>(header & 0xFF));
    file.put(static_cast<char>((header >> 8) & 0xFF));
    for (uint16_t tile_word : tile_words) {
      file.put(static_cast<char>(tile_word & 0xFF));
      file.put(static_cast<char>((tile_word >> 8) & 0xFF));
    }
    file.put(0);
    file.put(0);
  };

  write_object("subtype_zero.bin", {0x0011});
  write_object("subtype_one.bin", {0x0022, 0x0033});

  auto& manager = CustomObjectManager::Get();
  manager.Initialize(temp_base.string());
  manager.SetObjectFileMap({{0x31, {"subtype_zero.bin", "subtype_one.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  auto subtype_zero = editor.CaptureObjectLayout(
      /*object_id=*/0x31, room, palette, /*object_size=*/0);
  auto subtype_one = editor.CaptureObjectLayout(
      /*object_id=*/0x31, room, palette, /*object_size=*/1);

  ASSERT_TRUE(subtype_zero.ok()) << subtype_zero.status();
  ASSERT_TRUE(subtype_one.ok()) << subtype_one.status();
  EXPECT_TRUE(subtype_zero->is_custom);
  EXPECT_TRUE(subtype_one->is_custom);
  EXPECT_EQ(subtype_zero->custom_filename, "subtype_zero.bin");
  EXPECT_EQ(subtype_one->custom_filename, "subtype_one.bin");
  EXPECT_EQ(subtype_zero->cells.size(), 1u);
  EXPECT_EQ(subtype_one->cells.size(), 2u);
  EXPECT_EQ(subtype_zero->bounds_width, 1);
  EXPECT_EQ(subtype_one->bounds_width, 2);
}

TEST(ObjectTileLayoutTest, FromTracesEmptyInput) {
  std::vector<ObjectDrawer::TileTrace> traces;
  auto layout = ObjectTileLayout::FromTraces(traces);
  EXPECT_EQ(layout.cells.size(), 0);
  EXPECT_EQ(layout.bounds_width, 0);
  EXPECT_EQ(layout.bounds_height, 0);
}

TEST(ObjectTileLayoutTest, FromTracesKnownTraces) {
  std::vector<ObjectDrawer::TileTrace> traces;
  // 4 traces at (10, 10), (11, 10), (10, 11), (11, 11)
  for (int y = 10; y <= 11; ++y) {
    for (int x = 10; x <= 11; ++x) {
      ObjectDrawer::TileTrace t;
      t.object_id = 0x12;
      t.x_tile = static_cast<int16_t>(x);
      t.y_tile = static_cast<int16_t>(y);
      t.tile_id = static_cast<uint16_t>((y - 10) * 16 + (x - 10));
      t.flags = 0;
      traces.push_back(t);
    }
  }

  auto layout = ObjectTileLayout::FromTraces(traces);
  EXPECT_EQ(layout.cells.size(), 4);
  EXPECT_EQ(layout.bounds_width, 2);
  EXPECT_EQ(layout.bounds_height, 2);
  EXPECT_EQ(layout.origin_tile_x, 10);
  EXPECT_EQ(layout.origin_tile_y, 10);

  // Verify normalization
  auto* c00 = layout.FindCell(0, 0);
  ASSERT_NE(c00, nullptr);
  EXPECT_EQ(c00->tile_info.id_, 0);

  auto* c11 = layout.FindCell(1, 1);
  ASSERT_NE(c11, nullptr);
  EXPECT_EQ(c11->tile_info.id_, 17);
}

TEST(ObjectTileLayoutTest, FromTracesDuplicateCellKeepsLastVisibleTile) {
  std::vector<ObjectDrawer::TileTrace> traces;

  ObjectDrawer::TileTrace first;
  first.object_id = 0x12;
  first.x_tile = 10;
  first.y_tile = 10;
  first.tile_id = 0x11;
  first.flags = 0;
  traces.push_back(first);

  ObjectDrawer::TileTrace overwrite = first;
  overwrite.tile_id = 0x22;
  overwrite.flags = static_cast<uint8_t>(3 << 3);
  traces.push_back(overwrite);

  ObjectDrawer::TileTrace second_cell = first;
  second_cell.x_tile = 11;
  second_cell.tile_id = 0x33;
  traces.push_back(second_cell);

  auto layout = ObjectTileLayout::FromTraces(traces);

  ASSERT_EQ(layout.cells.size(), 2u);
  const auto* overwritten = layout.FindCell(0, 0);
  ASSERT_NE(overwritten, nullptr);
  EXPECT_EQ(overwritten->tile_info.id_, 0x22);
  EXPECT_EQ(overwritten->tile_info.palette_, 3);
  EXPECT_EQ(overwritten->write_index, 1);

  const auto* neighbor = layout.FindCell(1, 0);
  ASSERT_NE(neighbor, nullptr);
  EXPECT_EQ(neighbor->tile_info.id_, 0x33);
  EXPECT_EQ(neighbor->write_index, 2);
}

TEST(ObjectTileLayoutTest, FindCell) {
  ObjectTileLayout layout;
  ObjectTileLayout::Cell cell;
  cell.rel_x = 2;
  cell.rel_y = 3;
  layout.cells.push_back(cell);

  EXPECT_NE(layout.FindCell(2, 3), nullptr);
  EXPECT_EQ(layout.FindCell(0, 0), nullptr);
}

TEST(ObjectTileLayoutTest, ModificationsAndRevert) {
  ObjectTileLayout layout;
  ObjectTileLayout::Cell cell;
  cell.rel_x = 0;
  cell.rel_y = 0;
  cell.tile_info = gfx::TileInfo(0x100, 2, false, false, false);
  cell.original_word = gfx::TileInfoToWord(cell.tile_info);
  cell.modified = false;
  layout.cells.push_back(cell);

  ASSERT_FALSE(layout.HasModifications());

  layout.cells[0].tile_info.id_ = 0x200;
  layout.cells[0].modified = true;
  EXPECT_TRUE(layout.HasModifications());

  layout.RevertAll();
  EXPECT_FALSE(layout.HasModifications());
  EXPECT_EQ(layout.cells[0].tile_info.id_, 0x100);
}

TEST(ObjectTileEditorTest,
     GenericCaptureIsPreviewOnlyAndCannotAuthorizeStandardWrites) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or =
      editor.CaptureObjectLayout(/*object_id=*/0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  EXPECT_EQ(layout_or->tile_data_address, -1);
  EXPECT_FALSE(layout_or->source_provenance.has_value());
  for (const auto& cell : layout_or->cells) {
    EXPECT_FALSE(cell.source_ref.has_value());
  }

  auto* cell = layout_or->FindCell(0, 0);
  ASSERT_NE(cell, nullptr);
  cell->tile_info.id_ ^= 1;
  cell->modified = true;

  const auto original = rom.vector();
  const bool original_dirty = rom.dirty();
  const auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  EXPECT_TRUE(absl::IsFailedPrecondition(plan_or.status()));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);
}

TEST(ObjectTileEditorTest, EditableCaptureRejectsUnsupportedStandardObject) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  EXPECT_TRUE(ObjectTileEditor::IsEditableStandardObject(0x11F));
  EXPECT_TRUE(ObjectTileEditor::IsEditableStandardObject(0x120));
  EXPECT_FALSE(ObjectTileEditor::IsEditableStandardObject(0x11E));
  const auto layout_or = editor.CaptureEditableObjectLayout(
      /*object_id=*/0x11E, room, palette);
  EXPECT_TRUE(absl::IsUnimplemented(layout_or.status()));
}

TEST(ObjectTileEditorTest,
     EditableCaptureRejectsSourcesOverlappingObjectMetadata) {
  ScopedCustomObjectsDisabled disable_custom_objects;

  for (const uint32_t source_address :
       {static_cast<uint32_t>(0x7FF9), static_cast<uint32_t>(0x8000),
        static_cast<uint32_t>(0x842F), static_cast<uint32_t>(0x8432),
        static_cast<uint32_t>(0x8470), static_cast<uint32_t>(0x84F0),
        static_cast<uint32_t>(0x86EF)}) {
    SCOPED_TRACE(::testing::Message()
                 << "source_address=0x" << std::hex << source_address);
    Rom rom;
    auto data = MakeEditableObjectRomData();
    StoreWord(data, /*address=*/0x842E,
              static_cast<uint16_t>(source_address - 0x1B52));
    ASSERT_TRUE(rom.LoadFromData(data).ok());

    Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
    gfx::PaletteGroup palette;
    ObjectTileEditor editor(&rom);
    const auto original = rom.vector();
    const bool original_dirty = rom.dirty();

    const auto layout_or =
        editor.CaptureEditableObjectLayout(0x11F, room, palette);
    EXPECT_TRUE(absl::IsFailedPrecondition(layout_or.status()));
    EXPECT_NE(std::string(layout_or.status().message()).find("overlaps"),
              std::string::npos);
    EXPECT_EQ(rom.vector(), original);
    EXPECT_EQ(rom.dirty(), original_dirty);
  }
}

TEST(ObjectTileEditorTest,
     EditableCapturePinsDescriptorsSpansAndColumnMajorSourceMap) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  struct VisualCell {
    int rel_x;
    int rel_y;
    size_t source_word_index;
  };
  // Explicit visual/source map: [0 2; 1 3].
  constexpr std::array<VisualCell, 4> kVisualCells = {{
      {0, 0, 0},
      {1, 0, 2},
      {0, 1, 1},
      {1, 1, 3},
  }};

  for (const auto& fixture : kEditableObjectFixtures) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << fixture.object_id);
    auto layout_or =
        editor.CaptureEditableObjectLayout(fixture.object_id, room, palette);
    ASSERT_TRUE(layout_or.ok()) << layout_or.status();
    const auto& layout = *layout_or;

    EXPECT_EQ(layout.object_id, fixture.object_id);
    EXPECT_EQ(layout.bounds_width, 2);
    EXPECT_EQ(layout.bounds_height, 2);
    EXPECT_EQ(layout.tile_data_address,
              static_cast<int>(fixture.source_pc_address));
    ASSERT_TRUE(layout.source_provenance.has_value());
    const auto& provenance = *layout.source_provenance;
    EXPECT_EQ(provenance.object_id, fixture.object_id);
    EXPECT_EQ(provenance.descriptor_pc_address, fixture.descriptor_pc_address);
    EXPECT_EQ(provenance.expected_descriptor_word, fixture.descriptor_word);
    ASSERT_EQ(provenance.spans.size(), 1u);
    EXPECT_EQ(provenance.spans[0].pc_address, fixture.source_pc_address);
    EXPECT_EQ(provenance.spans[0].expected_words,
              std::vector<uint16_t>(fixture.source_words.begin(),
                                    fixture.source_words.end()));

    for (const auto& visual_cell : kVisualCells) {
      const auto* cell = layout.FindCell(visual_cell.rel_x, visual_cell.rel_y);
      ASSERT_NE(cell, nullptr);
      ASSERT_TRUE(cell->source_ref.has_value());
      EXPECT_EQ(cell->source_ref->span_index, 0u);
      EXPECT_EQ(cell->source_ref->word_index, visual_cell.source_word_index);
      EXPECT_EQ(cell->original_word,
                fixture.source_words[visual_cell.source_word_index]);
      EXPECT_EQ(gfx::TileInfoToWord(cell->tile_info), cell->original_word);
    }
  }
}

TEST(ObjectTileEditorTest,
     StandardTileSourceImpactIncludesAliasesAcrossObjectFamilies) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  auto data = MakeEditableObjectRomData();
  // Type 1 object 0 and Type 3 object F96 both consume the same exact four
  // words as editable Type 2 object 11F.
  StoreWord(data, /*Type 1 object 0 descriptor=*/0x8000, 0x0E9A);
  StoreWord(data, /*Type 3 object F96 descriptor=*/0x851C, 0x0E9A);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  auto impact_or = editor.AnalyzeStandardTileSourceImpact(*layout_or);
  ASSERT_TRUE(impact_or.ok()) << impact_or.status();
  ASSERT_EQ(impact_or->consumer_count(), 3u);
  EXPECT_TRUE(impact_or->runtime_consumers.empty());
  EXPECT_EQ(impact_or->affected_objects[0].object_id, 0x000);
  EXPECT_EQ(impact_or->affected_objects[1].object_id, 0x11F);
  EXPECT_EQ(impact_or->affected_objects[2].object_id, 0xF96);
  for (const auto& entry : impact_or->affected_objects) {
    EXPECT_THAT(entry.overlapping_ranges,
                ::testing::ElementsAre(ObjectTileReadRange{0x29EC, 0x29F4}));
  }
}

TEST(ObjectTileEditorTest,
     StandardTileSourceImpactIncludesGlobalLightableTorchConsumer) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());
  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x120, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  auto impact_or = editor.AnalyzeStandardTileSourceImpact(*layout_or);
  ASSERT_TRUE(impact_or.ok()) << impact_or.status();
  ASSERT_EQ(impact_or->affected_objects.size(), 1u);
  EXPECT_EQ(impact_or->affected_objects[0].object_id, 0x120);
  ASSERT_EQ(impact_or->runtime_consumers.size(), 2u);
  EXPECT_EQ(impact_or->runtime_consumers[0].consumer,
            ObjectTileRuntimeConsumer::kLightableTorchDraw);
  EXPECT_THAT(impact_or->runtime_consumers[0].overlapping_ranges,
              ::testing::ElementsAre(ObjectTileReadRange{0x2A1C, 0x2A24}));
  EXPECT_EQ(impact_or->runtime_consumers[1].consumer,
            ObjectTileRuntimeConsumer::kTorchLightingChange);
  EXPECT_THAT(impact_or->runtime_consumers[1].overlapping_ranges,
              ::testing::ElementsAre(ObjectTileReadRange{0x2A1C, 0x2A24}));
  EXPECT_EQ(impact_or->consumer_count(), 3u);
}

TEST(ObjectTileEditorTest,
     StandardTileSourceImpactDetectsPartialButNotAdjacentRanges) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  auto data = MakeEditableObjectRomData();
  // Editable source: [0x29EC, 0x29F4).
  StoreWord(data, /*Type 1 object 0 descriptor=*/0x8000,
            /*source 0x29E8=*/0x0E96);
  StoreWord(data, /*Type 1 object 7 descriptor=*/0x800E,
            /*source 0x29F0=*/0x0E9E);
  StoreWord(data, /*Type 1 object 8 descriptor=*/0x8010,
            /*source 0x29F4=*/0x0EA2);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  auto impact_or = editor.AnalyzeStandardTileSourceImpact(*layout_or);
  ASSERT_TRUE(impact_or.ok()) << impact_or.status();
  ASSERT_EQ(impact_or->consumer_count(), 3u);
  EXPECT_EQ(impact_or->affected_objects[0].object_id, 0x000);
  EXPECT_THAT(impact_or->affected_objects[0].overlapping_ranges,
              ::testing::ElementsAre(ObjectTileReadRange{0x29E8, 0x29F0}));
  EXPECT_EQ(impact_or->affected_objects[1].object_id, 0x007);
  EXPECT_THAT(impact_or->affected_objects[1].overlapping_ranges,
              ::testing::ElementsAre(ObjectTileReadRange{0x29F0, 0x29F8}));
  EXPECT_EQ(impact_or->affected_objects[2].object_id, 0x11F);
}

TEST(ObjectTileEditorTest,
     StandardTileSourceImpactFailsClosedOnMalformedUnrelatedObject) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());
  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  // Type 1 offsets are signed. 0x8000 resolves before the start of this ROM
  // and must invalidate the complete impact inventory instead of being
  // silently ignored as a non-overlap.
  StoreWordWithoutDirtying(rom, /*Type 1 object 1 descriptor=*/0x8002, 0x8000);
  const auto original = rom.vector();
  const bool original_dirty = rom.dirty();

  auto impact_or = editor.AnalyzeStandardTileSourceImpact(*layout_or);
  EXPECT_TRUE(absl::IsFailedPrecondition(impact_or.status()));
  EXPECT_NE(std::string(impact_or.status().message()).find("object 0x001"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);
}

TEST(ObjectTileEditorTest, BuildStandardWritePlanRejectsObjectMismatch) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  layout_or->object_id = 0x120;

  const auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  EXPECT_TRUE(absl::IsFailedPrecondition(plan_or.status()));
}

TEST(ObjectTileEditorTest,
     BuildStandardWritePlanRejectsOutOfBoundsSourceReferences) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  ObjectTileLayout bad_span = *layout_or;
  ASSERT_TRUE(bad_span.cells[0].source_ref.has_value());
  bad_span.cells[0].source_ref->span_index = 1;
  EXPECT_TRUE(absl::IsFailedPrecondition(
      editor.BuildStandardWritePlan(bad_span).status()));

  ObjectTileLayout bad_word = *layout_or;
  ASSERT_TRUE(bad_word.cells[0].source_ref.has_value());
  bad_word.cells[0].source_ref->word_index = 4;
  EXPECT_TRUE(absl::IsFailedPrecondition(
      editor.BuildStandardWritePlan(bad_word).status()));
}

TEST(ObjectTileEditorTest,
     BuildStandardWritePlanRejectsDuplicateResolvedAddresses) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  auto* duplicate = layout_or->FindCell(0, 1);
  ASSERT_NE(duplicate, nullptr);
  duplicate->rel_y = 0;
  duplicate->source_ref = ObjectTileSourceRef{/*span_index=*/0,
                                              /*word_index=*/0};
  const auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  ASSERT_TRUE(absl::IsFailedPrecondition(plan_or.status()));
  EXPECT_NE(std::string(plan_or.status().message()).find("duplicate"),
            std::string::npos);
}

TEST(ObjectTileEditorTest,
     BuildStandardWritePlanRejectsDescriptorSourcePreconditionAlias) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ASSERT_TRUE(layout_or->source_provenance.has_value());

  auto& provenance = *layout_or->source_provenance;
  provenance.expected_descriptor_word = 0x68DC;
  ASSERT_EQ(provenance.spans.size(), 1u);
  provenance.spans[0].pc_address = provenance.descriptor_pc_address;
  StoreWordWithoutDirtying(rom, provenance.descriptor_pc_address,
                           provenance.expected_descriptor_word);

  const auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  ASSERT_TRUE(absl::IsFailedPrecondition(plan_or.status()));
  EXPECT_NE(std::string(plan_or.status().message()).find("overlap"),
            std::string::npos);
}

TEST(ObjectTileEditorTest,
     BuildStandardWritePlanRejectsSourcesOverlappingObjectMetadata) {
  ScopedCustomObjectsDisabled disable_custom_objects;

  for (const uint32_t source_address :
       {static_cast<uint32_t>(0x7FF9), static_cast<uint32_t>(0x8000),
        static_cast<uint32_t>(0x842F), static_cast<uint32_t>(0x8432),
        static_cast<uint32_t>(0x8470), static_cast<uint32_t>(0x84F0),
        static_cast<uint32_t>(0x86EF)}) {
    SCOPED_TRACE(::testing::Message()
                 << "source_address=0x" << std::hex << source_address);
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());
    Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
    gfx::PaletteGroup palette;
    ObjectTileEditor editor(&rom);
    auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
    ASSERT_TRUE(layout_or.ok()) << layout_or.status();
    ASSERT_TRUE(layout_or->source_provenance.has_value());

    const uint16_t descriptor_word =
        static_cast<uint16_t>(source_address - 0x1B52);
    auto& provenance = *layout_or->source_provenance;
    provenance.expected_descriptor_word = descriptor_word;
    ASSERT_EQ(provenance.spans.size(), 1u);
    provenance.spans[0].pc_address = source_address;
    StoreWordWithoutDirtying(rom, provenance.descriptor_pc_address,
                             descriptor_word);
    const auto original = rom.vector();
    const bool original_dirty = rom.dirty();

    const auto plan_or = editor.BuildStandardWritePlan(*layout_or);
    EXPECT_TRUE(absl::IsFailedPrecondition(plan_or.status()));
    EXPECT_NE(std::string(plan_or.status().message()).find("overlaps"),
              std::string::npos);
    EXPECT_EQ(rom.vector(), original);
    EXPECT_EQ(rom.dirty(), original_dirty);
  }
}

TEST(ObjectTileEditorTest, StandardWritePlansAreOpaqueAndBuilderOwned) {
  static_assert(!std::is_aggregate_v<ObjectTileWritePlan>);
  static_assert(!std::is_default_constructible_v<ObjectTileWritePlan>);
  SUCCEED();
}

TEST(ObjectTileEditorTest,
     BuildStandardWritePlanRejectsStaleDescriptorAndSource) {
  ScopedCustomObjectsDisabled disable_custom_objects;

  for (bool stale_descriptor : {true, false}) {
    SCOPED_TRACE(stale_descriptor ? "stale descriptor" : "stale source");
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());
    Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
    gfx::PaletteGroup palette;
    ObjectTileEditor editor(&rom);
    auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
    ASSERT_TRUE(layout_or.ok()) << layout_or.status();

    if (stale_descriptor) {
      StoreWordWithoutDirtying(rom,
                               kEditableObjectFixtures[0].descriptor_pc_address,
                               kEditableObjectFixtures[0].descriptor_word ^ 1);
    } else {
      StoreWordWithoutDirtying(rom,
                               kEditableObjectFixtures[0].source_pc_address + 4,
                               kEditableObjectFixtures[0].source_words[2] ^ 1);
    }
    rom.set_dirty(stale_descriptor);
    const auto before = rom.vector();
    const bool was_dirty = rom.dirty();

    const auto plan_or = editor.BuildStandardWritePlan(*layout_or);
    EXPECT_TRUE(absl::IsFailedPrecondition(plan_or.status()));
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.dirty(), was_dirty);
  }
}

TEST(ObjectTileEditorTest, StandardWritePlanUsesExactWritesAndReadback) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();

  RoomObject cached_object(/*id=*/0x11F, /*x=*/0, /*y=*/0, /*size=*/0,
                           /*layer=*/0);
  cached_object.SetRom(&rom);
  auto cached_tile_or = cached_object.GetTile(/*index=*/0);
  ASSERT_TRUE(cached_tile_or.ok()) << cached_tile_or.status();
  const int original_cached_tile_id = (*cached_tile_or)->id_;
  RoomObject copied_cached_object = cached_object;

  auto* top_left = layout_or->FindCell(0, 0);
  auto* bottom_right = layout_or->FindCell(1, 1);
  ASSERT_NE(top_left, nullptr);
  ASSERT_NE(bottom_right, nullptr);
  top_left->tile_info.id_ ^= 1;
  top_left->modified = true;
  bottom_right->tile_info.id_ ^= 1;
  bottom_right->modified = true;
  const uint16_t top_left_word = gfx::TileInfoToWord(top_left->tile_info);
  const uint16_t bottom_right_word =
      gfx::TileInfoToWord(bottom_right->tile_info);

  auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();
  ASSERT_EQ(plan_or->preconditions().size(), 5u);
  EXPECT_EQ(plan_or->preconditions()[0].address, 0x842E);
  EXPECT_EQ(plan_or->preconditions()[0].expected_word, 0x0E9A);
  ASSERT_EQ(plan_or->writes().size(), 2u);
  ASSERT_EQ(plan_or->write_ranges().size(), 2u);
  EXPECT_EQ(plan_or->writes()[0].address, 0x29EC);
  EXPECT_EQ(plan_or->writes()[0].expected_word, 0x0DEE);
  EXPECT_EQ(plan_or->writes()[0].word, top_left_word);
  EXPECT_EQ(plan_or->write_ranges()[0],
            (std::pair<uint32_t, uint32_t>{0x29EC, 0x29EE}));
  EXPECT_EQ(plan_or->writes()[1].address, 0x29F2);
  EXPECT_EQ(plan_or->writes()[1].expected_word, 0xCDEE);
  EXPECT_EQ(plan_or->writes()[1].word, bottom_right_word);
  EXPECT_EQ(plan_or->write_ranges()[1],
            (std::pair<uint32_t, uint32_t>{0x29F2, 0x29F4}));

  auto expected_rom = rom.vector();
  StoreWord(expected_rom, 0x29EC, top_left_word);
  StoreWord(expected_rom, 0x29F2, bottom_right_word);
  const uint64_t revision_before_apply = rom.object_tile_revision();
  ASSERT_TRUE(editor.ApplyStandardWritePlan(*plan_or).ok());
  EXPECT_EQ(rom.vector(), expected_rom);
  EXPECT_TRUE(rom.dirty());
  EXPECT_EQ(rom.object_tile_revision(), revision_before_apply + 1);
  auto refreshed_cached_tile_or = copied_cached_object.GetTile(/*index=*/0);
  ASSERT_TRUE(refreshed_cached_tile_or.ok())
      << refreshed_cached_tile_or.status();
  EXPECT_EQ((*refreshed_cached_tile_or)->id_, top_left->tile_info.id_);
  EXPECT_NE((*refreshed_cached_tile_or)->id_, original_cached_tile_id);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom.vector()).ok());
  Room reopened_room(/*room_id=*/0, &reopened, /*game_data=*/nullptr);
  ObjectTileEditor reopened_editor(&reopened);
  auto readback_or = reopened_editor.CaptureEditableObjectLayout(
      0x11F, reopened_room, palette);
  ASSERT_TRUE(readback_or.ok()) << readback_or.status();
  EXPECT_EQ(readback_or->FindCell(0, 0)->original_word, top_left_word);
  EXPECT_EQ(readback_or->FindCell(1, 1)->original_word, bottom_right_word);
}

TEST(ObjectTileEditorTest,
     EmptyStandardWritePlanDoesNotAdvanceObjectTileRevision) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();
  ASSERT_TRUE(plan_or->writes().empty());

  const uint64_t revision_before_apply = rom.object_tile_revision();
  EXPECT_TRUE(editor.ApplyStandardWritePlan(*plan_or).ok());
  EXPECT_EQ(rom.object_tile_revision(), revision_before_apply);
}

TEST(ObjectTileEditorTest,
     ApplyStandardWritePlanRejectsDescriptorAndSourceCASStaleness) {
  ScopedCustomObjectsDisabled disable_custom_objects;

  for (bool stale_descriptor : {true, false}) {
    SCOPED_TRACE(stale_descriptor ? "stale descriptor" : "stale source");
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());
    Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
    gfx::PaletteGroup palette;
    ObjectTileEditor editor(&rom);
    auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
    ASSERT_TRUE(layout_or.ok()) << layout_or.status();
    auto* cell = layout_or->FindCell(0, 0);
    ASSERT_NE(cell, nullptr);
    cell->tile_info.id_ ^= 1;
    cell->modified = true;
    auto plan_or = editor.BuildStandardWritePlan(*layout_or);
    ASSERT_TRUE(plan_or.ok()) << plan_or.status();

    if (stale_descriptor) {
      StoreWordWithoutDirtying(rom,
                               kEditableObjectFixtures[0].descriptor_pc_address,
                               kEditableObjectFixtures[0].descriptor_word ^ 1);
    } else {
      // Change an unmodified source word to prove Apply rechecks the complete
      // captured source, not only the target write address.
      StoreWordWithoutDirtying(rom,
                               kEditableObjectFixtures[0].source_pc_address + 4,
                               kEditableObjectFixtures[0].source_words[2] ^ 1);
    }
    rom.set_dirty(stale_descriptor);
    const auto before = rom.vector();
    const bool was_dirty = rom.dirty();
    const uint64_t revision_before_apply = rom.object_tile_revision();

    const absl::Status status = editor.ApplyStandardWritePlan(*plan_or);
    EXPECT_TRUE(absl::IsFailedPrecondition(status));
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.dirty(), was_dirty);
    EXPECT_EQ(rom.object_tile_revision(), revision_before_apply);
  }
}

TEST(ObjectTileEditorTest, ApplyStandardWritePlanRollsBackAndPreservesDirty) {
  ScopedCustomObjectsDisabled disable_custom_objects;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeEditableObjectRomData()).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.CaptureEditableObjectLayout(0x11F, room, palette);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  for (const auto& coordinate :
       std::array<std::pair<int, int>, 2>{{{0, 0}, {0, 1}}}) {
    auto* cell = layout_or->FindCell(coordinate.first, coordinate.second);
    ASSERT_NE(cell, nullptr);
    cell->tile_info.id_ ^= 1;
    cell->modified = true;
  }

  auto plan_or = editor.BuildStandardWritePlan(*layout_or);
  ASSERT_TRUE(plan_or.ok()) << plan_or.status();
  ASSERT_EQ(plan_or->writes().size(), 2u);

  RoomObject cached_object(/*id=*/0x11F, /*x=*/0, /*y=*/0, /*size=*/0,
                           /*layer=*/0);
  cached_object.SetRom(&rom);
  auto cached_tile_or = cached_object.GetTile(/*index=*/0);
  ASSERT_TRUE(cached_tile_or.ok()) << cached_tile_or.status();
  const int original_cached_tile_id = (*cached_tile_or)->id_;
  RoomObject copied_cached_object = cached_object;

  rom::WriteFence fence;
  ASSERT_TRUE(fence.Allow(0x29EC, 0x29EE, "first object tile word").ok());
  rom::ScopedWriteFence fence_scope(&rom, &fence);

  const auto original = rom.vector();
  const bool original_dirty = rom.dirty();
  const uint64_t revision_before_apply = rom.object_tile_revision();
  const absl::Status status = editor.ApplyStandardWritePlan(*plan_or);

  EXPECT_TRUE(absl::IsPermissionDenied(status));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);
  EXPECT_EQ(rom.object_tile_revision(), revision_before_apply);
  auto preserved_cached_tile_or = copied_cached_object.GetTile(/*index=*/0);
  ASSERT_TRUE(preserved_cached_tile_or.ok())
      << preserved_cached_tile_or.status();
  EXPECT_EQ((*preserved_cached_tile_or)->id_, original_cached_tile_id);
}

TEST(ObjectTileEditorTest, RenderLayoutToBitmapUsesThirdPaletteWhenAvailable) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  ObjectTileLayout layout;
  layout.bounds_width = 2;
  layout.bounds_height = 1;

  ObjectTileLayout::Cell left;
  left.rel_x = 0;
  left.rel_y = 0;
  left.tile_info = gfx::TileInfo(/*id=*/0, /*palette=*/0, false, false, false);
  layout.cells.push_back(left);

  ObjectTileLayout::Cell right;
  right.rel_x = 1;
  right.rel_y = 0;
  right.tile_info = gfx::TileInfo(/*id=*/1, /*palette=*/1, false, false, false);
  layout.cells.push_back(right);

  const auto palette_group = MakeTestPaletteGroup();
  std::vector<uint8_t> gfx_buffer(0x8000, 0x00);
  gfx_buffer[0] = 1;
  gfx_buffer[8] = 1;
  gfx::Bitmap bitmap;

  auto status = editor.RenderLayoutToBitmap(layout, bitmap, gfx_buffer.data(),
                                            palette_group);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(bitmap.is_active());
  EXPECT_EQ(bitmap.width(), 16);
  EXPECT_EQ(bitmap.height(), 8);
  EXPECT_EQ(bitmap.palette().size(), 48u);
  EXPECT_EQ(bitmap.palette()[0].snes(), palette_group.palette_ref(0)[0].snes());
  EXPECT_EQ(bitmap.palette()[1].snes(), palette_group.palette_ref(0)[1].snes());
  EXPECT_EQ(bitmap.palette()[16].snes(),
            palette_group.palette_ref(1)[0].snes());
  EXPECT_EQ(bitmap.palette()[17].snes(),
            palette_group.palette_ref(1)[1].snes());
  EXPECT_EQ(bitmap.mutable_data()[0], 1);
  EXPECT_EQ(bitmap.mutable_data()[8], 17);
}

TEST(ObjectTileEditorTest, RenderLayoutToBitmapUsesCanonicalDungeonCgramRows) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  gfx::SnesPalette hud_palette;
  for (int i = 0; i < 32; ++i) {
    hud_palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(0x0100 + i)));
  }
  gfx::SnesPalette dungeon_palette;
  for (int i = 0; i < 90; ++i) {
    dungeon_palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(0x0200 + i)));
  }
  const auto palette_group =
      BuildDungeonRenderPaletteGroup(dungeon_palette, &hud_palette);

  ObjectTileLayout layout;
  layout.bounds_width = 2;
  layout.bounds_height = 1;
  ObjectTileLayout::Cell first_bank;
  first_bank.rel_x = 0;
  first_bank.rel_y = 0;
  first_bank.tile_info =
      gfx::TileInfo(/*id=*/0, /*palette=*/2, false, false, false);
  layout.cells.push_back(first_bank);
  ObjectTileLayout::Cell last_bank;
  last_bank.rel_x = 1;
  last_bank.rel_y = 0;
  last_bank.tile_info =
      gfx::TileInfo(/*id=*/1, /*palette=*/7, false, false, false);
  layout.cells.push_back(last_bank);

  std::vector<uint8_t> gfx_buffer(0x10000, 0);
  gfx_buffer[0] = 1;
  gfx_buffer[8] = 15;
  gfx::Bitmap bitmap;
  ObjectTileEditor editor(&rom);

  const auto status = editor.RenderLayoutToBitmap(
      layout, bitmap, gfx_buffer.data(), palette_group);

  ASSERT_TRUE(status.ok()) << status.message();
  ASSERT_EQ(bitmap.palette().size(), 128u);
  EXPECT_EQ(bitmap.palette()[33].snes(), dungeon_palette[0].snes());
  EXPECT_EQ(bitmap.palette()[127].snes(), dungeon_palette[89].snes());
  EXPECT_EQ(bitmap.mutable_data()[0], 33);
  EXPECT_EQ(bitmap.mutable_data()[8], 127);
  ASSERT_NE(bitmap.surface(), nullptr);
  EXPECT_EQ(SDL_HasColorKey(bitmap.surface()), SDL_TRUE);
  Uint32 transparent_key = 0;
  ASSERT_EQ(SDL_GetColorKey(bitmap.surface(), &transparent_key), 0);
  EXPECT_EQ(transparent_key, 255u);
}

TEST(ObjectTileEditorTest,
     CustomSpriteBodyPreviewAppliesRuntimePageMaskAndPreservesZero) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileLayout layout;
  layout.object_id = 0x54;
  layout.is_custom = true;
  layout.bounds_width = 2;
  layout.bounds_height = 1;
  layout.cells.push_back(
      {.rel_x = 0, .rel_y = 0, .tile_info = gfx::WordToTileInfo(0x0132)});
  layout.cells.push_back(
      {.rel_x = 1, .rel_y = 0, .tile_info = gfx::WordToTileInfo(0x0000)});

  std::vector<uint8_t> gfx_buffer(0x10000, 0);
  constexpr int kRawTileOffset = 0x13 * 1024 + 2 * 8;
  constexpr int kRuntimeTileOffset = 0x33 * 1024 + 2 * 8;
  gfx_buffer[kRawTileOffset] = 3;
  gfx_buffer[kRuntimeTileOffset] = 7;
  gfx::Bitmap bitmap;
  ObjectTileEditor editor(&rom);

  const absl::Status status = editor.RenderLayoutToBitmap(
      layout, bitmap, gfx_buffer.data(), MakeTestPaletteGroup());

  ASSERT_TRUE(status.ok()) << status;
  ASSERT_TRUE(bitmap.is_active());
  EXPECT_EQ(bitmap.mutable_data()[0], 7);
  EXPECT_EQ(bitmap.mutable_data()[8], 255);
}

TEST(ObjectTileEditorTest, BuildTile8AtlasUsesRequestedPaletteIndex) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  const auto palette_group = MakeTestPaletteGroup();
  // BuildTile8Atlas renders all 1024 tiles (kAtlasTileCount), which indexes the
  // full 0x10000-byte SNES graphics sheet; a smaller buffer overruns.
  std::vector<uint8_t> gfx_buffer(0x10000, 0x00);
  gfx_buffer[0] = 1;
  gfx::Bitmap atlas;

  auto status = editor.BuildTile8Atlas(atlas, gfx_buffer.data(), palette_group,
                                       /*display_palette=*/1);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(atlas.is_active());
  EXPECT_EQ(atlas.width(), ObjectTileEditor::kAtlasWidthPx);
  EXPECT_EQ(atlas.height(), ObjectTileEditor::kAtlasHeightPx);
  EXPECT_EQ(atlas.palette().size(), 16u);
  EXPECT_EQ(atlas.palette()[0].snes(), palette_group.palette_ref(1)[0].snes());
  EXPECT_EQ(atlas.palette()[1].snes(), palette_group.palette_ref(1)[1].snes());
  EXPECT_EQ(atlas.mutable_data()[0], 1);
}

TEST(ObjectTileEditorTest,
     BuildTile8AtlasFallsBackToFirstPaletteWhenRequestedPaletteMissing) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  ObjectTileEditor editor(&rom);
  const auto palette_group = MakeTestPaletteGroup();
  // Full-atlas render needs the complete 0x10000-byte graphics sheet.
  std::vector<uint8_t> gfx_buffer(0x10000, 0x00);
  gfx::Bitmap atlas;

  auto status = editor.BuildTile8Atlas(atlas, gfx_buffer.data(), palette_group,
                                       /*display_palette=*/7);
  ASSERT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(atlas.palette().size(), 16u);
  EXPECT_EQ(atlas.palette()[0].snes(), palette_group.palette_ref(0)[0].snes());
  EXPECT_EQ(atlas.palette()[1].snes(), palette_group.palette_ref(0)[1].snes());
}

TEST(ObjectTileEditorTest, CustomObjectRoundtrip) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_objects");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original_object{
      .tiles = {{0, 0, 0x2810}, {0, 1, 0x2820}},
  };
  WriteTestCustomObject(custom_dir.path() / "track_LR.bin", original_object);

  auto& mgr = CustomObjectManager::Get();

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(/*object_id=*/0x31,
                                                 /*subtype=*/0);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileLayout layout = std::move(*layout_or);
  ASSERT_EQ(layout.cells.size(), 2u);
  layout.cells[0].tile_info = gfx::TileInfo(0x30, 3, false, false, false);
  layout.cells[0].modified = true;
  layout.cells[1].tile_info = gfx::TileInfo(0x40, 4, false, false, false);
  layout.cells[1].modified = true;

  ASSERT_TRUE(editor.WriteBack(layout).ok());

  // Read back via CustomObjectManager
  auto custom_obj_result = mgr.LoadObject("track_LR.bin");
  ASSERT_TRUE(custom_obj_result.ok());
  auto custom_obj = custom_obj_result.value();

  ASSERT_EQ(custom_obj->tiles.size(), 2);
  EXPECT_EQ(custom_obj->tiles[0].rel_x, 0);
  EXPECT_EQ(custom_obj->tiles[0].rel_y, 0);
  EXPECT_EQ(custom_obj->tiles[0].tile_data,
            gfx::TileInfoToWord(layout.cells[0].tile_info));

  EXPECT_EQ(custom_obj->tiles[1].rel_x, 0);
  EXPECT_EQ(custom_obj->tiles[1].rel_y, 1);
  EXPECT_EQ(custom_obj->tiles[1].tile_data,
            gfx::TileInfoToWord(layout.cells[1].tile_info));
  EXPECT_FALSE(layout.custom_source_bytes.empty());
}

TEST(ObjectTileEditorTest, CustomSpriteBodyRoundtripKeepsRawSourceWords) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_sprite_body");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original_object{
      .tiles = {{0, 0, 0x1D32}, {1, 0, 0x0000}, {2, 0, 0x1D33}},
  };
  WriteTestCustomObject(custom_dir.path() / "manhandla_body_1a.bin",
                        original_object);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(/*object_id=*/0x54,
                                                 /*subtype=*/1);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileLayout layout = std::move(*layout_or);
  ASSERT_EQ(layout.cells.size(), 3u);
  EXPECT_EQ(layout.cells[0].original_word, 0x1D32);
  EXPECT_EQ(gfx::TileInfoToWord(layout.cells[0].tile_info), 0x1D32);
  EXPECT_EQ(layout.cells[1].original_word, 0x0000);

  layout.cells[2].tile_info = gfx::WordToTileInfo(0x1D34);
  layout.cells[2].modified = true;
  ASSERT_TRUE(editor.WriteBack(layout).ok());

  auto loaded_or =
      CustomObjectManager::Get().LoadObject("manhandla_body_1a.bin");
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  ASSERT_EQ((*loaded_or)->tiles.size(), 3u);
  EXPECT_EQ((*loaded_or)->tiles[0].tile_data, 0x1D32);
  EXPECT_EQ((*loaded_or)->tiles[1].tile_data, 0x0000);
  EXPECT_EQ((*loaded_or)->tiles[2].tile_data, 0x1D34);
}

TEST(ObjectTileEditorTest, CustomObjectWritePreservesSparseCoordinates) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_sparse");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original{
      .tiles = {{0, 0, 0x2800}, {3, 0, 0x2803}, {4, 0, 0x2804}, {0, 1, 0x2840}},
  };
  WriteTestCustomObject(custom_dir.path() / "track_LR.bin", original);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(0x31, 0);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileLayout layout = std::move(*layout_or);
  layout.cells[1].tile_info = gfx::WordToTileInfo(0x2A03);
  layout.cells[1].modified = true;

  const absl::Status status = editor.WriteBack(layout);

  ASSERT_TRUE(status.ok()) << status;
  auto loaded_or = CustomObjectManager::Get().LoadObject("track_LR.bin");
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  const std::vector<CustomObject::TileMapEntry> expected = {
      {0, 0, 0x2800}, {3, 0, 0x2A03}, {4, 0, 0x2804}, {0, 1, 0x2840}};
  EXPECT_EQ((*loaded_or)->tiles, expected);
}

TEST(ObjectTileEditorTest, CustomObjectWriteBridgesLeadingAndLongGaps) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_gaps");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original{.tiles = {{0, 0, 0x2AAA}}};
  WriteTestCustomObject(custom_dir.path() / "track_LR.bin", original);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(0x31, 0);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileLayout layout = std::move(*layout_or);
  layout.cells.clear();
  for (const auto& tile : std::vector<CustomObject::TileMapEntry>{
           {5, 0, 0x2805}, {0, 2, 0x2880}, {63, 63, 0x2FFF}}) {
    ObjectTileLayout::Cell cell;
    cell.rel_x = tile.rel_x;
    cell.rel_y = tile.rel_y;
    cell.tile_info = gfx::WordToTileInfo(tile.tile_data);
    cell.modified = true;
    layout.cells.push_back(cell);
  }

  const absl::Status status = editor.WriteBack(layout);

  ASSERT_TRUE(status.ok()) << status;
  auto loaded_or = CustomObjectManager::Get().LoadObject("track_LR.bin");
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  std::vector<CustomObject::TileMapEntry> visible;
  for (const auto& tile : (*loaded_or)->tiles) {
    if (tile.tile_data != 0) {
      visible.push_back(tile);
    }
  }
  const std::vector<CustomObject::TileMapEntry> expected = {
      {5, 0, 0x2805}, {0, 2, 0x2880}, {63, 63, 0x2FFF}};
  EXPECT_EQ(visible, expected);
}

TEST(ObjectTileEditorTest, ThirtyTwoWideCustomLayoutPublishesAllTiles) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_width_32");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original{.tiles = {{0, 0, 0x2AAA}}};
  WriteTestCustomObject(custom_dir.path() / "track_LR.bin", original);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(0x31, 0);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  ObjectTileLayout layout = std::move(*layout_or);
  layout.cells.clear();
  for (int x = 0; x < 32; ++x) {
    ObjectTileLayout::Cell cell;
    cell.rel_x = x;
    cell.rel_y = 0;
    cell.tile_info = gfx::WordToTileInfo(static_cast<uint16_t>(0x2800 + x));
    cell.modified = true;
    layout.cells.push_back(cell);
  }

  const absl::Status status = editor.WriteBack(layout);

  ASSERT_TRUE(status.ok()) << status;
  auto loaded_or = CustomObjectManager::Get().LoadObject("track_LR.bin");
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  ASSERT_EQ((*loaded_or)->tiles.size(), 32u);
  EXPECT_EQ((*loaded_or)->tiles.front().rel_x, 0);
  EXPECT_EQ((*loaded_or)->tiles.back().rel_x, 31);
  EXPECT_EQ((*loaded_or)->tiles.back().rel_y, 0);
}

TEST(ObjectTileEditorTest, LoadCustomObjectPreservesNoOpsAndSourceSnapshot) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_noop_load");
  ASSERT_TRUE(custom_dir.ready());
  const std::vector<uint8_t> bytes = {
      0x03, 0x00,  // Three positions in the first segment.
      0x11, 0x28, 0x00, 0x00, 0x22, 0x28, 0x00, 0x00,
  };
  WriteTestBinary(custom_dir.path() / "track_LR.bin", bytes);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(0x31, 0);

  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  EXPECT_EQ(layout_or->custom_subtype, 0);
  EXPECT_EQ(layout_or->custom_filename, "track_LR.bin");
  EXPECT_FALSE(layout_or->custom_resolved_path.empty());
  EXPECT_EQ(layout_or->custom_source_bytes, bytes);
  ASSERT_EQ(layout_or->cells.size(), 3u);
  EXPECT_EQ(layout_or->cells[1].rel_x, 1);
  EXPECT_EQ(gfx::TileInfoToWord(layout_or->cells[1].tile_info), 0);
}

TEST(ObjectTileEditorTest, StaleCustomObjectWriterKeepsSecondDraftAndFile) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_stale_writer");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original{.tiles = {{0, 0, 0x2810}}};
  WriteTestCustomObject(custom_dir.path() / "track_LR.bin", original);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto first_or = editor.LoadCustomObjectLayout(0x31, 0);
  auto second_or = editor.LoadCustomObjectLayout(0x31, 0);
  ASSERT_TRUE(first_or.ok()) << first_or.status();
  ASSERT_TRUE(second_or.ok()) << second_or.status();
  first_or->cells[0].tile_info = gfx::WordToTileInfo(0x2820);
  first_or->cells[0].modified = true;
  second_or->cells[0].tile_info = gfx::WordToTileInfo(0x2830);
  second_or->cells[0].modified = true;

  ASSERT_TRUE(editor.WriteBack(*first_or).ok());
  const std::vector<uint8_t> first_bytes =
      ReadTestBinary(custom_dir.path() / "track_LR.bin");
  const absl::Status second_status = editor.WriteBack(*second_or);

  EXPECT_TRUE(absl::IsAborted(second_status));
  EXPECT_TRUE(second_or->HasModifications());
  EXPECT_EQ(ReadTestBinary(custom_dir.path() / "track_LR.bin"), first_bytes);
}

TEST(ObjectTileEditorTest, ProjectFolderSwitchCannotRedirectCustomApply) {
  ScopedCustomObjectDirectory custom_dir("yaze_test_custom_project_switch");
  ASSERT_TRUE(custom_dir.ready());
  const CustomObject original{.tiles = {{0, 0, 0x2810}}};
  WriteTestCustomObject(custom_dir.path() / "track_LR.bin", original);
  const std::filesystem::path other_dir = custom_dir.path() / "other_project";
  ASSERT_TRUE(std::filesystem::create_directories(other_dir));
  WriteTestCustomObject(other_dir / "track_LR.bin", original);

  Rom rom;
  ObjectTileEditor editor(&rom);
  auto layout_or = editor.LoadCustomObjectLayout(0x31, 0);
  ASSERT_TRUE(layout_or.ok()) << layout_or.status();
  layout_or->cells[0].tile_info = gfx::WordToTileInfo(0x2820);
  layout_or->cells[0].modified = true;
  CustomObjectManager::Get().Initialize(other_dir.string());
  CustomObjectManager::Get().ClearObjectFileMap();
  const std::vector<uint8_t> original_other =
      ReadTestBinary(other_dir / "track_LR.bin");

  const absl::Status status = editor.WriteBack(*layout_or);

  EXPECT_TRUE(absl::IsAborted(status));
  EXPECT_TRUE(layout_or->HasModifications());
  EXPECT_EQ(ReadTestBinary(other_dir / "track_LR.bin"), original_other);
}

TEST(ObjectTileEditorTest, CaptureVanillaWallCornersIgnoresConfiguredTrackMap) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  std::string temp_base = "/tmp/yaze_test_wall_corner_capture";
  std::filesystem::create_directories(temp_base);
  struct Cleanup {
    bool old_custom_objects_flag;
    CustomObjectManager::State old_custom_object_state;
    std::string temp_base;
    ~Cleanup() {
      core::FeatureFlags::get().kEnableCustomObjects = old_custom_objects_flag;
      CustomObjectManager::Get().RestoreState(old_custom_object_state);
      std::filesystem::remove_all(temp_base);
    }
  } cleanup{old_custom_objects_flag, old_custom_object_state, temp_base};

  auto write_one_tile_object = [&](const std::string& filename) {
    std::ofstream file(std::filesystem::path(temp_base) / filename,
                       std::ios::binary);
    const std::vector<uint8_t> data = {
        0x01, 0x00,  // count=1, jump=0
        0x11, 0x11,  // tile word
        0x00, 0x00   // terminator
    };
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
  };

  write_one_tile_object("track_corner_TL.bin");

  CustomObjectManager::Get().Initialize(temp_base);
  CustomObjectManager::Get().SetObjectFileMap(
      {{0x31,
        {"track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
         "track_corner_TR.bin", "track_corner_BL.bin",
         "track_corner_BR.bin"}}});

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  for (const int object_id : {0x100, 0x101, 0x102, 0x103}) {
    SCOPED_TRACE(object_id);
    auto layout_or = editor.CaptureObjectLayout(object_id, room, palette);
    ASSERT_TRUE(layout_or.ok());
    EXPECT_FALSE(layout_or->is_custom);
    EXPECT_TRUE(layout_or->custom_filename.empty());
    EXPECT_GT(layout_or->cells.size(), 1u);
  }
}

TEST(ObjectTileEditorTest,
     CaptureWallCornerWithCustomAssetFolderStaysRomBacked) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  std::string temp_base = "/tmp/yaze_test_wall_corner_capture_no_map";
  std::filesystem::create_directories(temp_base);
  struct Cleanup {
    bool old_custom_objects_flag;
    CustomObjectManager::State old_custom_object_state;
    std::string temp_base;
    ~Cleanup() {
      core::FeatureFlags::get().kEnableCustomObjects = old_custom_objects_flag;
      CustomObjectManager::Get().RestoreState(old_custom_object_state);
      std::filesystem::remove_all(temp_base);
    }
  } cleanup{old_custom_objects_flag, old_custom_object_state, temp_base};

  std::ofstream file(std::filesystem::path(temp_base) / "track_corner_TL.bin",
                     std::ios::binary);
  const std::vector<uint8_t> data = {
      0x01, 0x00,  // count=1, jump=0
      0x11, 0x11,  // tile word
      0x00, 0x00   // terminator
  };
  file.write(reinterpret_cast<const char*>(data.data()), data.size());

  CustomObjectManager::Get().Initialize(temp_base);
  CustomObjectManager::Get().ClearObjectFileMap();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  Room room(/*room_id=*/0, &rom, /*game_data=*/nullptr);
  gfx::PaletteGroup palette;
  ObjectTileEditor editor(&rom);

  auto layout_or =
      editor.CaptureObjectLayout(/*object_id=*/0x100, room, palette);
  ASSERT_TRUE(layout_or.ok());
  EXPECT_FALSE(layout_or->is_custom);
  EXPECT_TRUE(layout_or->custom_filename.empty());
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
