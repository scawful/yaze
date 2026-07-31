#include "zelda3/dungeon/object_tile_editor.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

TEST(ObjectTileLayoutTest, CreateEmptyBuildsCustomModifiedGrid) {
  auto layout =
      ObjectTileLayout::CreateEmpty(2, 3, /*object_id=*/0x123, "custom.bin");

  EXPECT_EQ(layout.object_id, 0x123);
  EXPECT_EQ(layout.bounds_width, 2);
  EXPECT_EQ(layout.bounds_height, 3);
  EXPECT_TRUE(layout.is_custom);
  EXPECT_EQ(layout.custom_filename, "custom.bin");
  EXPECT_EQ(layout.tile_data_address, -1);
  ASSERT_EQ(layout.cells.size(), 6u);

  for (const auto& cell : layout.cells) {
    EXPECT_TRUE(cell.modified);
    EXPECT_EQ(cell.tile_info.palette_, 2);
  }

  ASSERT_NE(layout.FindCell(1, 2), nullptr);
  EXPECT_EQ(layout.FindCell(1, 2)->rel_x, 1);
  EXPECT_EQ(layout.FindCell(1, 2)->rel_y, 2);
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
  EXPECT_NE(std::string(plan_or.status().message()).find("alias"),
            std::string::npos);
}

TEST(ObjectTileEditorTest,
     ApplyStandardWritePlanRejectsForgedRangesAndCASBeforeMutation) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  ObjectTileEditor editor(&rom);

  ObjectTileWritePlan plan;
  plan.writes.push_back(
      {/*address=*/0x1000, /*expected_word=*/0, /*word=*/0x1234});
  plan.write_ranges.push_back({0x1002, 0x1004});
  plan.preconditions.push_back({/*address=*/0x1000, /*expected_word=*/0});
  const auto original = rom.vector();
  const bool original_dirty = rom.dirty();

  EXPECT_TRUE(absl::IsFailedPrecondition(editor.ApplyStandardWritePlan(plan)));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);

  plan.write_ranges[0] = {0x1000, 0x1002};
  plan.preconditions.clear();
  EXPECT_TRUE(absl::IsFailedPrecondition(editor.ApplyStandardWritePlan(plan)));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);

  plan.preconditions.push_back({/*address=*/0x1002, /*expected_word=*/0});
  EXPECT_TRUE(absl::IsFailedPrecondition(editor.ApplyStandardWritePlan(plan)));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);

  plan.preconditions = {{/*address=*/0x1000, /*expected_word=*/0},
                        {/*address=*/0x1000, /*expected_word=*/0}};
  EXPECT_TRUE(absl::IsFailedPrecondition(editor.ApplyStandardWritePlan(plan)));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);

  plan.preconditions = {{/*address=*/0x1000, /*expected_word=*/1}};
  EXPECT_TRUE(absl::IsFailedPrecondition(editor.ApplyStandardWritePlan(plan)));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);
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
  ASSERT_EQ(plan_or->preconditions.size(), 5u);
  EXPECT_EQ(plan_or->preconditions[0].address, 0x842E);
  EXPECT_EQ(plan_or->preconditions[0].expected_word, 0x0E9A);
  ASSERT_EQ(plan_or->writes.size(), 2u);
  ASSERT_EQ(plan_or->write_ranges.size(), 2u);
  EXPECT_EQ(plan_or->writes[0].address, 0x29EC);
  EXPECT_EQ(plan_or->writes[0].expected_word, 0x0DEE);
  EXPECT_EQ(plan_or->writes[0].word, top_left_word);
  EXPECT_EQ(plan_or->write_ranges[0],
            (std::pair<uint32_t, uint32_t>{0x29EC, 0x29EE}));
  EXPECT_EQ(plan_or->writes[1].address, 0x29F2);
  EXPECT_EQ(plan_or->writes[1].expected_word, 0xCDEE);
  EXPECT_EQ(plan_or->writes[1].word, bottom_right_word);
  EXPECT_EQ(plan_or->write_ranges[1],
            (std::pair<uint32_t, uint32_t>{0x29F2, 0x29F4}));

  auto expected_rom = rom.vector();
  StoreWord(expected_rom, 0x29EC, top_left_word);
  StoreWord(expected_rom, 0x29F2, bottom_right_word);
  ASSERT_TRUE(editor.ApplyStandardWritePlan(*plan_or).ok());
  EXPECT_EQ(rom.vector(), expected_rom);
  EXPECT_TRUE(rom.dirty());

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

    const absl::Status status = editor.ApplyStandardWritePlan(*plan_or);
    EXPECT_TRUE(absl::IsFailedPrecondition(status));
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.dirty(), was_dirty);
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
  ASSERT_EQ(plan_or->writes.size(), 2u);

  rom::WriteFence fence;
  ASSERT_TRUE(fence.Allow(0x29EC, 0x29EE, "first object tile word").ok());
  rom::ScopedWriteFence fence_scope(&rom, &fence);

  const auto original = rom.vector();
  const bool original_dirty = rom.dirty();
  const absl::Status status = editor.ApplyStandardWritePlan(*plan_or);

  EXPECT_TRUE(absl::IsPermissionDenied(status));
  EXPECT_EQ(rom.vector(), original);
  EXPECT_EQ(rom.dirty(), original_dirty);
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
  // Setup temp directory for custom objects
  std::string temp_base = "/tmp/yaze_test_custom_objects";
  std::filesystem::create_directories(temp_base);

  auto& mgr = CustomObjectManager::Get();
  mgr.Initialize(temp_base);

  Rom rom;
  ObjectTileEditor editor(&rom);
  ObjectTileLayout layout;
  layout.is_custom = true;
  layout.custom_filename = "test_object.bin";

  // Create a 1x2 vertical object
  ObjectTileLayout::Cell c1, c2;
  c1.rel_x = 0;
  c1.rel_y = 0;
  c1.tile_info = gfx::TileInfo(0x10, 2, false, false, false);
  c1.modified = true;

  c2.rel_x = 0;
  c2.rel_y = 1;
  c2.tile_info = gfx::TileInfo(0x20, 2, false, false, false);
  c2.modified = true;

  layout.cells.push_back(c1);
  layout.cells.push_back(c2);

  ASSERT_TRUE(editor.WriteBack(layout).ok());

  // Verify file existence
  std::filesystem::path full_path =
      std::filesystem::path(temp_base) / "test_object.bin";
  ASSERT_TRUE(std::filesystem::exists(full_path));

  // Read back via CustomObjectManager
  auto custom_obj_result = mgr.LoadObject("test_object.bin");
  ASSERT_TRUE(custom_obj_result.ok());
  auto custom_obj = custom_obj_result.value();

  ASSERT_EQ(custom_obj->tiles.size(), 2);
  EXPECT_EQ(custom_obj->tiles[0].rel_x, 0);
  EXPECT_EQ(custom_obj->tiles[0].rel_y, 0);
  EXPECT_EQ(custom_obj->tiles[0].tile_data, gfx::TileInfoToWord(c1.tile_info));

  EXPECT_EQ(custom_obj->tiles[1].rel_x, 0);
  EXPECT_EQ(custom_obj->tiles[1].rel_y, 1);
  EXPECT_EQ(custom_obj->tiles[1].tile_data, gfx::TileInfoToWord(c2.tile_info));

  // Cleanup
  std::filesystem::remove_all(temp_base);
}

TEST(ObjectTileEditorTest,
     CaptureLayoutForCornerAliasResolvesCustomFilenameWithExplicitTrackMap) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  std::string temp_base = "/tmp/yaze_test_corner_alias_capture";
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

  auto layout_or =
      editor.CaptureObjectLayout(/*object_id=*/0x100, room, palette);
  ASSERT_TRUE(layout_or.ok());
  EXPECT_TRUE(layout_or->is_custom);
  EXPECT_EQ(layout_or->custom_filename, "track_corner_TL.bin");
}

TEST(ObjectTileEditorTest,
     CaptureLayoutForCornerAliasWithoutTrackMapStaysVanilla) {
  const bool old_custom_objects_flag =
      core::FeatureFlags::get().kEnableCustomObjects;
  const auto old_custom_object_state =
      CustomObjectManager::Get().SnapshotState();
  core::FeatureFlags::get().kEnableCustomObjects = true;

  std::string temp_base = "/tmp/yaze_test_corner_alias_capture_no_map";
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
