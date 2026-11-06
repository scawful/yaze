#include <gtest/gtest.h>

#include "absl/strings/str_format.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/cleanup/cleanup.h"
#include "app/editor/editor_manager.h"
#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/entity/entity_operations.h"
#include "app/editor/overworld/maps/map_properties.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "app/editor/system/session/collision_source_pairing.h"
#include "app/editor/system/session/rom_file_manager.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/automation/widget_id_registry.h"
#include "cli/handlers/game/overworld_render_commands.h"
#include "cli/handlers/rom/rom_commands.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "fixtures/lyra/synthetic_fixture_data.h"
#include "framework/headless_editor_test.h"
#include "framework/rom_save_fault.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz/miniz.h"
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_geometry.h"
#include "zelda3/overworld/overworld_sprite_io.h"
#include "zelda3/overworld/overworld_terrain_io.h"

namespace yaze::editor {
class NativePropertySaveTestPeer {
 public:
  static absl::Status Apply(OverworldEditor& editor,
                            const OverworldPropertyEdit& edit) {
    return editor.map_properties_system_->ApplyPropertyEdit(edit);
  }
};
}  // namespace yaze::editor

namespace yaze {
namespace {

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
  // Only bare fixture names use the built-in synthetic corpus. An explicitly
  // configured fixture directory must still exist and contain its own data.
  if (!path.has_parent_path()) {
    const auto fixture = test::lyra::FindFixture(path.string());
    if (fixture.data != nullptr) {
      std::vector<uint8_t> bytes(fixture.image_size);
      mz_ulong size = static_cast<mz_ulong>(bytes.size());
      if (mz_uncompress(bytes.data(), &size, fixture.data,
                        static_cast<mz_ulong>(fixture.size)) != MZ_OK ||
          size != bytes.size()) {
        return {};
      }
      return bytes;
    }
  }
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::filesystem::path FixtureDir() {
  const char* path = std::getenv("YAZE_LYRA_FIXTURE_DIR");
  return path == nullptr ? std::filesystem::path()
                         : std::filesystem::path(path);
}

void WriteBytes(const std::filesystem::path& path,
                const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

void Put32(std::vector<uint8_t>* bytes, size_t pc, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    (*bytes)[pc + i] = static_cast<uint8_t>(value >> (8 * i));
  }
}

// Reseal each directory entry and the descriptor after a semantic mutation.
// This prevents a CRC failure from masking the deeper contract being tested.
void Reseal(std::vector<uint8_t>* bytes,
            const rom::LyraLayoutInspection& original) {
  constexpr size_t kDescriptorPc = 0x147000;
  for (size_t i = 0; i < original.entries.size(); ++i) {
    const auto& e = original.entries[i];
    Put32(bytes, kDescriptorPc + 32 + 16 * i + 12,
          mz_crc32(MZ_CRC32_INIT, bytes->data() + e.pc_address,
                   e.count * e.stride));
  }
  Put32(bytes, kDescriptorPc + 28, 0);
  Put32(bytes, kDescriptorPc + 28,
        mz_crc32(MZ_CRC32_INIT, bytes->data() + kDescriptorPc, 1024));
}

// Hermetic directory/codec fixture. These synthetic helper bytes exercise
// binding encoding only; they do not implement or qualify game readers.
struct NativeDirectoryFixture {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(0x300000, 0);
  rom::LyraLayoutInspection rows;

  static uint32_t Snes(uint32_t pc) {
    uint32_t bank = pc / 0x8000;
    if (bank >= 0x7E)
      bank |= 0x80;
    return (bank << 16) | 0x8000 | (pc & 0x7FFF);
  }
  void Word(size_t pc, uint16_t value) {
    bytes[pc] = static_cast<uint8_t>(value);
    bytes[pc + 1] = static_cast<uint8_t>(value >> 8);
  }
  void Long(size_t pc, uint32_t value) {
    Word(pc, static_cast<uint16_t>(value));
    bytes[pc + 2] = static_cast<uint8_t>(value >> 16);
  }
  rom::LyraLayoutEntry& Entry(uint16_t id) {
    return *std::find_if(rows.entries.begin(), rows.entries.end(),
                         [id](const auto& e) { return e.id == id; });
  }
  size_t EntryPc(uint16_t id) {
    return 0x147020 + (&Entry(id) - rows.entries.data()) * 16;
  }
  void Add(uint16_t id, uint16_t kind, uint32_t pc, uint16_t count,
           uint16_t stride) {
    rows.entries.push_back({id, kind, Snes(pc), pc, count, stride, 0});
  }
  void Seal() {
    for (uint16_t id : {20, 21}) {
      const size_t prefix = id == 20 ? 0x1794D : 0x17B2D;
      std::copy_n(bytes.begin() + Entry(id).pc_address, 160 * 3,
                  bytes.begin() + prefix);
    }
    for (const auto& e : rows.entries) {
      const size_t at = EntryPc(e.id);
      Word(at, e.id);
      Word(at + 2, e.kind);
      Put32(&bytes, at + 4, e.snes_address);
      Word(at + 8, e.count);
      Word(at + 10, e.stride);
    }
    Put32(&bytes, 0x147018, static_cast<uint32_t>(bytes.size()));
    Reseal(&bytes, rows);
  }
  // A model fixture, not a migration/ROM producer: preserve the test ROM's
  // existing pages/resources and explicitly supply synthetic tail content.
  void SeedLoadedLegacyForModelTest(const Rom& source,
                                    const zelda3::Overworld& world) {
    ASSERT_LE(source.size(), 0x200000);
    std::array<uint8_t, 32> header;
    std::copy_n(bytes.begin() + 0x147000, header.size(), header.begin());
    std::copy(source.vector().begin(), source.vector().end(), bytes.begin());
    std::fill_n(bytes.begin() + 0x147000, 1024, 0);
    std::copy(header.begin(), header.end(), bytes.begin() + 0x147000);
    const auto& original = world.resolved_layout();
    std::copy_n(source.vector().begin() + original.tile16_pc.value, 4096 * 8,
                bytes.begin() + Entry(400).pc_address);
    for (int i = 0; i < 4; ++i)
      std::copy_n(source.vector().begin() + original.tile32_pcs[i].value,
                  4432 * 6, bytes.begin() + Entry(401 + i).pc_address);
    // A stale legacy TL address must not accidentally satisfy the new reader.
    bytes[original.tile32_pcs[0].value] ^= 0x10;
    const auto sprites = zelda3::GetOverworldSpriteLayout(source);
    const auto item_root = source.ReadLong(zelda3::overworldItemsAddress);
    ASSERT_TRUE(item_root.ok());
    const size_t item_table = SnesToPc(*item_root);
    std::optional<uint16_t> empty_items;
    const auto item_bank = *source.ReadByte(zelda3::overworldItemsAddressBank);
    for (size_t map = 0; map < 160; ++map) {
      for (uint16_t id : {20, 21}) {
        const auto table = id == 20 ? original.terrain_high_table_pc.value
                                    : original.terrain_low_table_pc.value;
        std::copy_n(source.vector().begin() + table + map * 3, 3,
                    bytes.begin() + Entry(id).pc_address + map * 3);
      }
      for (int phase = 0; phase < 3; ++phase)
        std::copy_n(source.vector().begin() + sprites.tables[phase] + map * 2,
                    2, bytes.begin() + Entry(16 + phase).pc_address + map * 2);
      const uint16_t word = *source.ReadWord(item_table + map * 2);
      Word(Entry(23).pc_address + map * 2, word);
      if (word >= 0x8000) {
        const auto terminator =
            source.ReadWord(SnesToPc((item_bank << 16) | word));
        if (terminator.ok() && *terminator == 0xFFFF)
          empty_items = word;
      }
    }
    ASSERT_TRUE(empty_items.has_value());
    bytes[0x240010] = 0xE4;
    bytes[0x240011] = 0xFF;
    bytes[0x240012] = 0;
    bytes[0x240013] = 0xFF;
    for (size_t map = 0; map < Entry(7).count; ++map) {
      const auto& m = *world.overworld_map(map < 160 ? map : 0x80);
      Word(Entry(1).pc_address + map * 2, m.area_specific_bg_color());
      bytes[Entry(2).pc_address + map] = m.main_palette();
      bytes[Entry(3).pc_address + map] = 0;
      for (int quadrant = 0; quadrant < 4; ++quadrant)
        if (m.mosaic_expanded()[quadrant])
          bytes[Entry(3).pc_address + map] |= 0x08 >> quadrant;
      bytes[Entry(4).pc_address + map] = m.animated_gfx();
      Word(Entry(5).pc_address + map * 2, m.subscreen_overlay());
      for (int slot = 0; slot < 8; ++slot)
        bytes[Entry(6).pc_address + map * 8 + slot] = m.custom_tileset(slot);
      bytes[Entry(7).pc_address + map] = map < 160 ? m.parent() : map;
      bytes[Entry(30).pc_address + map] =
          map < 160 ? static_cast<uint8_t>(m.area_size()) : 0;
      Word(Entry(19).pc_address + map * 2, m.message_id());
      bytes[Entry(26).pc_address + map] = m.area_graphics();
      bytes[Entry(27).pc_address + map] = m.area_graphics();
      bytes[Entry(31).pc_address + map] = m.area_palette();
      for (int phase = 0; phase < 4; ++phase)
        bytes[Entry(32 + phase).pc_address + map] = m.area_music(phase);
      for (int phase = 0; phase < 3; ++phase) {
        constexpr std::array<uint16_t, 3> gfx = {24, 36, 39};
        constexpr std::array<uint16_t, 3> palettes = {25, 37, 38};
        bytes[Entry(gfx[phase]).pc_address + map] = m.sprite_graphics(phase);
        bytes[Entry(palettes[phase]).pc_address + map] =
            m.sprite_palette(phase);
      }
      if (map >= 160) {
        if (map != 0xA0)
          Long(Entry(20).pc_address + map * 3, Snes(0x240010));
        Word(Entry(23).pc_address + map * 2, *empty_items);
      }
    }
    // Restore only the five synthetic binding instructions after copying the
    // real resource data. No fixture claims runtime helper correctness.
    for (size_t i = 0; i < 5; ++i) {
      const size_t at = Entry(301).pc_address + i * 16;
      const uint32_t site =
          bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16);
      std::copy_n(bytes.begin() + at + 12, 4,
                  bytes.begin() + (site >> 16) * 0x8000 + (site & 0x7FFF));
    }
    Seal();
  }
  void SeedNativeSpritePrefix() {
    bytes[0x4C881] = 0xFF;
    for (int phase = 0; phase < 3; ++phase) {
      constexpr std::array<size_t, 3> mirrors = {0x141438, 0x141578, 0x1416B8};
      for (size_t map = 0; map < 160; ++map) {
        Word(Entry(16 + phase).pc_address + map * 2, 0xC881);
        Word(mirrors[phase] + map * 2, 0xC881);
      }
    }
    Seal();
  }
  // In-place revision-3 → revision-4 upgrade for tests (synthetic allocation).
  void UpgradeToRevision4() {
    const uint16_t capacity = rows.entries.empty() ? 192 : Entry(1).count;
    Add(410, 8, 0x2B0000, capacity, 1);
    Add(411, 8, 0x2B1000, 64, 256);
    std::fill_n(bytes.begin() + Entry(410).pc_address, capacity, 0);
    std::fill_n(bytes.begin() + Entry(411).pc_address, 64 * 256, 0);
    Word(0x147008, 4);
    Word(0x14700C, 928);
    Word(0x147012, 56);
    bytes[0x147015] = 0x1C;
    Seal();
  }
  explicit NativeDirectoryFixture(uint16_t capacity = 192) {
    std::memcpy(bytes.data() + 0x147000, "OOSOW192", 8);
    Word(0x147008, 3);
    Word(0x14700A, 32);
    Word(0x14700C, 896);
    Word(0x14700E, capacity);
    Word(0x147010, capacity);
    Word(0x147012, 54);
    bytes[0x147014] = 0x4C;
    bytes[0x147015] = 0x0C;
    Word(0x147016, 2 * capacity);
    bytes[0x140145] = 3;
    uint32_t table_pc = 0x230000;
    for (uint16_t id = 1; id <= 39; ++id) {
      const uint16_t stride = id == 6                  ? 8
                              : (id == 20 || id == 21) ? 3
                              : (id == 1 || id == 5 || (id >= 8 && id <= 19) ||
                                 id == 22 || id == 23)
                                  ? 2
                                  : 1;
      Add(id, 1, table_pc, capacity, stride);
      table_pc += capacity * stride;
    }
    Add(100, 2, 0x260000, 16, 1);
    Add(200, 3, 0x58000, 0x7E70, 1);
    Add(201, 3, 0x60000, 0x411F, 1);
    Add(202, 3, 0x130000, 0x8000, 1);
    for (uint16_t id = 210; id <= 213; ++id)
      Add(id, 3, 0x240000 + (id - 210) * 0x8000, 0x8000, 1);
    Add(300, 5, 0x234000, 64, 1);
    Add(301, 6, 0x233800, 5, 16);
    Add(400, 7, 0x280000, 4096, 8);
    for (uint16_t id = 401; id <= 404; ++id)
      Add(id, 7, 0x288000 + (id - 401) * 0x8000, 4432, 6);
    auto fill_stream = [&](size_t pc, uint8_t value) {
      bytes[pc] = 0xE4;
      bytes[pc + 1] = 0xFF;
      bytes[pc + 2] = value;
      bytes[pc + 3] = 0xFF;
    };
    fill_stream(0x58000, 0);
    fill_stream(0x240000, 0x11);
    fill_stream(0x240004, 0x34);
    fill_stream(0x240008, 0x12);
    fill_stream(0x24000C, 0x56);
    bytes[0x260000] = 0xFF;
    for (size_t map = 0; map < capacity; ++map) {
      bytes[Entry(7).pc_address + map] = static_cast<uint8_t>(map);
      bytes[Entry(29).pc_address + map] = 0xFF;
      Long(Entry(20).pc_address + map * 3,
           Snes(map == 0xA0 ? 0x240008 : 0x58000));
      Long(Entry(21).pc_address + map * 3, Snes(map == 0xA0   ? 0x240004
                                                : map == 0xBF ? 0x24000C
                                                              : 0x240000));
      for (uint16_t id : {16, 17, 18})
        Word(Entry(id).pc_address + map * 2, 0x8000);
    }
    constexpr std::array<uint32_t, 5> sites = {0x02F59D, 0x02F5C8, 0x02F7E3,
                                               0x02F80E, 0x09C500};
    for (size_t i = 0; i < sites.size(); ++i) {
      const size_t pc = Entry(301).pc_address + i * 16;
      const uint32_t target = 0x46C000 + static_cast<uint32_t>(i) * 4;
      Put32(&bytes, pc, sites[i]);
      Put32(&bytes, pc + 4, target);
      Word(pc + 8, i < 4 ? 20 + i % 2 : 16);
      bytes[pc + 10] = i < 2 ? 1 : i < 4 ? 2 : 4;
      const uint32_t instruction = (target << 8) | (i < 4 ? 0x22 : 0x5C);
      Put32(&bytes, pc + 12, instruction);
      Put32(&bytes, (sites[i] >> 16) * 0x8000 + (sites[i] & 0x7FFF),
            instruction);
      bytes[Entry(300).pc_address + i * 4] = 0x6B;
    }
    Seal();
  }
};

TEST(LyraLayoutDescriptorTest, NativeDirectoryDecodesBothCapacitiesById) {
  for (uint16_t capacity : {160, 192}) {
    NativeDirectoryFixture fixture(capacity);
    // Directory order is independent of table identity and physical order.
    std::reverse(fixture.rows.entries.begin(), fixture.rows.entries.end());
    fixture.Seal();
    const auto layout =
        rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
    ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
    EXPECT_EQ(layout.capacity, capacity);
    EXPECT_EQ(layout.active_count, capacity);
    EXPECT_EQ(layout.tail_actor_bank, 0x4C);
    EXPECT_FALSE(layout.offline_only);
    EXPECT_FALSE(layout.write_supported);
    EXPECT_FALSE(layout.a0_terrain_write_supported);
    for (size_t map = 0; map < capacity; ++map) {
      const auto page = rom::LyraMapTile32(layout, fixture.bytes.data(),
                                           fixture.bytes.size(), map);
      ASSERT_TRUE(page.has_value());
      const uint16_t expected = map == 0xA0   ? 0x1234
                                : map == 0xBF ? 0x56
                                              : 0x11;
      EXPECT_TRUE(
          std::all_of(page->begin(), page->end(),
                      [expected](uint16_t tile) { return tile == expected; }));
      const auto role = rom::LyraLayoutRecord(layout, fixture.bytes.data(),
                                              fixture.bytes.size(), 29, map);
      ASSERT_TRUE(role.has_value());
      EXPECT_EQ((*role)[0], 0xFF);
    }
    EXPECT_FALSE(rom::LyraMapTile32(layout, fixture.bytes.data(),
                                    fixture.bytes.size(), capacity));
    Rom loaded;
    ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
    auto options = zelda3::OverworldLayoutOptions::Defaults();
    options.map16_expanded_pc = 0x10000;
    const auto resolved = zelda3::InspectOverworldLayout(loaded, options);
    EXPECT_EQ(resolved.layout_kind,
              zelda3::OverworldLayout::LayoutKind::kNativeSpecialOverworld);
    EXPECT_EQ(resolved.special_storage_slots, capacity - 128);
    EXPECT_EQ(resolved.terrain_table_entries, capacity);
    EXPECT_EQ(resolved.tile16_pc.value, fixture.Entry(400).pc_address);
    for (size_t i = 0; i < 4; ++i)
      EXPECT_EQ(resolved.tile32_pcs[i].value,
                fixture.Entry(401 + i).pc_address);
    EXPECT_EQ(resolved.terrain_high_table_pc.value,
              fixture.Entry(20).pc_address);
    EXPECT_FALSE(loaded.CheckLayoutWritable().ok());
    auto world = std::make_unique<zelda3::Overworld>(&loaded);
    EXPECT_FALSE(world->LoadLyraTerrainPreview(&loaded).ok());
    EXPECT_EQ(world->materialized_map_count(), 0);
  }
}

TEST(LyraLayoutDescriptorTest,
     NativeDirectoryRejectsResealedShapeAndOwnershipDamage) {
  NativeDirectoryFixture fixture;
  auto rejects = [&](const char* label, auto mutate, const char* error) {
    SCOPED_TRACE(label);
    auto changed = fixture.bytes;
    mutate(changed);
    Reseal(&changed, fixture.rows);
    const auto layout = rom::InspectLyraLayout(changed.data(), changed.size());
    EXPECT_EQ(layout.presence, rom::LyraLayoutPresence::kInvalid);
    EXPECT_NE(layout.error.find(error), std::string::npos) << layout.error;
  };
  rejects("unsupported flags", [](auto& b) { b[0x147015] = 0x0F; }, "flags");
  rejects("materialized count", [](auto& b) { b[0x147010] = 160; }, "capacity");
  rejects(
      "duplicate ID", [&](auto& b) { b[fixture.EntryPc(39)] = 38; },
      "duplicate");
  rejects(
      "old compact family", [&](auto& b) { b[fixture.EntryPc(24) + 8] = 64; },
      "entry");
  rejects(
      "dictionary width", [&](auto& b) { b[fixture.EntryPc(401) + 10] = 8; },
      "entry");
  rejects(
      "physical alias",
      [&](auto& b) {
        Put32(&b, fixture.EntryPc(401) + 4, fixture.Entry(400).snes_address);
      },
      "overlap");
  rejects(
      "descriptor alias",
      [&](auto& b) { Put32(&b, fixture.EntryPc(1) + 4, 0x28F000); },
      "descriptor");
  rejects(
      "WRAM entry",
      [&](auto& b) { Put32(&b, fixture.EntryPc(210) + 4, 0x7E8000); },
      "outside bank");
  rejects(
      "short pool", [&](auto& b) { b[fixture.EntryPc(210) + 9] = 0x40; },
      "entry");
}

TEST(LyraLayoutDescriptorTest,
     NativeDirectoryValidatesTailActorsAndAreaFootprints) {
  NativeDirectoryFixture fixture;
  auto rejects = [&](auto mutate, const char* error) {
    auto changed = fixture.bytes;
    mutate(changed);
    Reseal(&changed, fixture.rows);
    const auto layout = rom::InspectLyraLayout(changed.data(), changed.size());
    EXPECT_EQ(layout.presence, rom::LyraLayoutPresence::kInvalid);
    EXPECT_NE(layout.error.find(error), std::string::npos) << layout.error;
  };
  rejects([&](auto& b) { b[fixture.Entry(7).pc_address + 0xBF] = 0xC0; },
          "parent");
  rejects([&](auto& b) { b[fixture.Entry(29).pc_address + 0xBF] = 4; }, "role");
  rejects([&](auto& b) { b[fixture.Entry(30).pc_address + 0xBF] = 2; },
          "footprint");
  rejects([](auto& b) { b[0x147014] = 0x4D; }, "actor bank");
  rejects(
      [&](auto& b) { b[fixture.Entry(16).pc_address + 0xA0 * 2 + 1] = 0x7F; },
      "actor pointer");
  rejects(
      [&](auto& b) {
        std::fill_n(b.begin() + fixture.Entry(100).pc_address, 16, 0);
      },
      "actor record");
  for (size_t map : {0xB6, 0xB7, 0xBE, 0xBF}) {
    fixture.bytes[fixture.Entry(7).pc_address + map] = 0xB6;
    fixture.bytes[fixture.Entry(30).pc_address + map] = 1;
  }
  fixture.Seal();
  const auto large =
      rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
  EXPECT_EQ(large.presence, rom::LyraLayoutPresence::kValid) << large.error;
}

TEST(LyraLayoutDescriptorTest,
     NativeDirectoryRejectsCorruptBindingsAndBoundedStreams) {
  NativeDirectoryFixture fixture;
  auto rejects = [&](const char* label, auto mutate, const char* error) {
    SCOPED_TRACE(label);
    auto changed = fixture.bytes;
    mutate(changed);
    Reseal(&changed, fixture.rows);
    const auto layout = rom::InspectLyraLayout(changed.data(), changed.size());
    EXPECT_EQ(layout.presence, rom::LyraLayoutPresence::kInvalid);
    EXPECT_NE(layout.error.find(error), std::string::npos) << layout.error;
  };
  const size_t binding = fixture.Entry(301).pc_address;
  rejects(
      "installed hook mismatch", [](auto& b) { b[0x1759D] = 0xEA; }, "binding");
  rejects("wrong table", [&](auto& b) { b[binding + 8] = 21; }, "ABI");
  rejects(
      "missing full reader", [&](auto& b) { b[binding + 10] = 3; },
      "required reader");
  rejects("nonzero reserved", [&](auto& b) { b[binding + 11] = 1; }, "binding");
  rejects(
      "WRAM stream",
      [&](auto& b) { b[fixture.Entry(20).pc_address + 2] = 0x7E; },
      "terrain stream");
  rejects(
      "bank-end stream",
      [&](auto& b) {
        const size_t pc = fixture.Entry(20).pc_address;
        b[pc] = 0xFE;
        b[pc + 1] = 0xFF;
        b[pc + 2] = 0x48;
        b[0x247FFE] = 0xE4;
        b[0x247FFF] = 0xFF;
      },
      "terrain stream");
  rejects(
      "Tile32 exceeds dictionary", [&](auto& b) { b[0x24000A] = 0xFF; },
      "Tile32");
}

std::vector<rom::NativeOverworldPropertyEdit> GeometryEdits(
    std::span<const zelda3::OverworldGeometryRow> rows,
    std::span<const int> map_ids) {
  std::vector<rom::NativeOverworldPropertyEdit> edits;
  for (int map : map_ids) {
    edits.push_back({7, static_cast<uint16_t>(map), {rows[map].parent}});
    edits.push_back({30,
                     static_cast<uint16_t>(map),
                     {static_cast<uint8_t>(rows[map].size)}});
    for (size_t i = 0; i < rows[map].words.size(); ++i) {
      const uint16_t value = rows[map].words[i];
      edits.push_back(
          {static_cast<uint16_t>(8 + i),
           static_cast<uint16_t>(map),
           {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)}});
    }
  }
  return edits;
}

TEST(LyraLayoutDescriptorTest, NativeGeometryPlansAcceptedAreasAndAllSizes) {
  NativeDirectoryFixture fixture;
  Rom image;
  ASSERT_TRUE(image.LoadFromData(fixture.bytes).ok());
  const auto layout = image.InspectLyraLayout();
  std::vector<zelda3::OverworldMap> maps;
  for (int i = 0; i < layout.capacity; ++i) {
    auto map =
        zelda3::OverworldMap::CreateFromLayout(i, &image, nullptr, &layout);
    ASSERT_TRUE(map.ok()) << map.status();
    maps.push_back(std::move(*map));
  }
  for (int parent : {0xA0, 0xBE}) {
    maps[parent].SetAsWideMap(parent, 0);
    maps[parent + 1].SetAsWideMap(parent, 1);
  }
  for (int parent : {0xA6, 0xB0, 0xB3}) {
    for (int q = 0; q < 4; ++q)
      maps[parent + q / 2 * 8 + q % 2].SetAsLargeMap(parent, q);
  }
  auto plan = zelda3::PlanExpandedOverworldGeometry(maps);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->size(), 192u);
  int tail_areas = 0;
  for (int map = 0xA0; map < 0xC0; ++map) {
    const auto& row = (*plan)[map];
    tail_areas += row.parent == map;
    const int x = row.parent % 8;
    const int y = row.parent % 64 / 8;
    EXPECT_EQ(row.words[4], y * 0x200);
    EXPECT_EQ(row.words[5], x * 0x200);
    EXPECT_EQ(row.words[6], static_cast<uint16_t>(y * 0x200 - 0xE0));
    EXPECT_EQ(row.words[7], static_cast<uint16_t>(x * 0x200 - 0x100));
  }
  EXPECT_EQ(tail_areas, 21);
  EXPECT_EQ((*plan)[0xBF].parent, 0xBE);
  EXPECT_EQ((*plan)[0xBF].size, zelda3::AreaSizeEnum::WideArea);
  EXPECT_EQ((*plan)[0xB9].parent, 0xB0);
  EXPECT_EQ((*plan)[0xB9].size, zelda3::AreaSizeEnum::LargeArea);
  // A8 is small below A0/A1 wide; B8/B9 are the lower half of B0 large.
  EXPECT_EQ((*plan)[0xA8].words[0], 0x0060);
  EXPECT_EQ((*plan)[0xA9].words[2], 0x17C0);
  EXPECT_EQ((*plan)[0xB9].words[0], 0x1060);
  EXPECT_EQ((*plan)[0xB9].words[2], 0x1840);
  // A tall area uses 2x2 quadrant numbering (bottom=2), including the BF edge.
  maps[0xBE].SetAsSmallMap(0xBE);
  maps[0xBF].SetAsSmallMap(0xBF);
  maps[0xB7].SetAsTallMap(0xB7, 0);
  maps[0xBF].SetAsTallMap(0xB7, 2);
  plan = zelda3::PlanExpandedOverworldGeometry(maps);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ((*plan)[0xBF].parent, 0xB7);
  EXPECT_EQ((*plan)[0xBF].words[0], 0x1060);
  EXPECT_EQ((*plan)[0xBF].words[3], 0x2000);
  EXPECT_EQ(image.vector(), fixture.bytes);
  // A partial old group is refused before planning any writes.
  maps[0xB9].SetAsSmallMap(0xB9);
  EXPECT_FALSE(zelda3::PlanExpandedOverworldGeometry(maps).ok());
  // A horizontal area cannot wrap into the next physical row.
  for (int i = 0; i < static_cast<int>(maps.size()); ++i)
    maps[i].SetAsSmallMap(i);
  maps[0xA7].SetAsWideMap(0xA7, 0);
  maps[0xA8].SetAsWideMap(0xA7, 1);
  EXPECT_FALSE(zelda3::PlanExpandedOverworldGeometry(maps).ok());
}

TEST(LyraLayoutDescriptorTest, NativeGeometryBatchMirrorsAndReopensAtomically) {
  for (int capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    NativeDirectoryFixture fixture(capacity);
    const std::array<size_t, 10> mirrors = {
        0x140998, 0x1788D,  0x140A38, 0x140B78, 0x140CB8,
        0x140DF8, 0x140F38, 0x141078, 0x1411B8, 0x1412F8};
    constexpr std::array<uint16_t, 10> ids = {7,  30, 8,  9,  10,
                                              11, 12, 13, 14, 15};
    // Start with coherent original-prefix mirrors. An unchanged row must not
    // repair unrelated baseline bytes as a side effect of another map's edit.
    for (size_t i = 0; i < ids.size(); ++i) {
      const auto& table = fixture.Entry(ids[i]);
      std::copy_n(fixture.bytes.begin() + table.pc_address, 160 * table.stride,
                  fixture.bytes.begin() + mirrors[i]);
    }
    Rom image;
    ASSERT_TRUE(image.LoadFromData(fixture.bytes).ok());
    const auto layout = image.InspectLyraLayout();
    std::vector<zelda3::OverworldMap> maps;
    for (int i = 0; i < capacity; ++i) {
      auto map =
          zelda3::OverworldMap::CreateFromLayout(i, &image, nullptr, &layout);
      ASSERT_TRUE(map.ok()) << map.status();
      maps.push_back(std::move(*map));
    }
    const int parent = capacity - 10;
    const std::array<int, 4> members = {parent, parent + 1, parent + 8,
                                        parent + 9};
    for (int q = 0; q < 4; ++q)
      maps[members[q]].SetAsLargeMap(parent, q);
    const auto plan = zelda3::PlanExpandedOverworldGeometry(maps);
    ASSERT_TRUE(plan.ok()) << plan.status();
    auto edits = GeometryEdits(*plan, members);
    const auto original = image.vector();
    auto saved = rom::SealNativeOverworldTerrain(original, original, edits);
    ASSERT_TRUE(saved.ok()) << saved.status();
    const auto checked = rom::InspectLyraLayout(saved->data(), saved->size());
    ASSERT_EQ(checked.presence, rom::LyraLayoutPresence::kValid)
        << checked.error;
    for (size_t n = 0; n < edits.size(); ++n) {
      const auto& edit = edits[n];
      SCOPED_TRACE(::testing::Message() << edit.table_id << ":" << edit.map_id);
      const auto& table = fixture.Entry(edit.table_id);
      const size_t at = table.pc_address + edit.map_id * table.stride;
      EXPECT_TRUE(std::equal(edit.value.begin(), edit.value.end(),
                             saved->begin() + at));
      if (edit.map_id < 160) {
        const size_t mirror = mirrors[n % 10] + edit.map_id * table.stride;
        EXPECT_TRUE(std::equal(edit.value.begin(), edit.value.end(),
                               saved->begin() + mirror));
      }
    }
    if (capacity == 192) {
      for (size_t i = 0; i < ids.size(); ++i) {
        const size_t end = mirrors[i] + 160 * fixture.Entry(ids[i]).stride;
        EXPECT_TRUE(std::equal(original.begin() + mirrors[i],
                               original.begin() + end,
                               saved->begin() + mirrors[i]));
      }
    }
    auto repeated = rom::SealNativeOverworldTerrain(*saved, *saved, edits);
    ASSERT_TRUE(repeated.ok()) << repeated.status();
    EXPECT_EQ(*repeated, *saved);
    // The ordinary atomic publisher retains read-only, fence and stale guards.
    {
      ScopedRomReadOnly readonly(image);
      EXPECT_FALSE(
          image.CommitNativeTerrainImage(original, original, edits).ok());
    }
    {
      rom::WriteFence fence;
      rom::ScopedWriteFence scope(&image, &fence);
      EXPECT_FALSE(
          image.CommitNativeTerrainImage(original, original, edits).ok());
    }
    EXPECT_EQ(image.vector(), original);
    auto partial = edits;
    partial.pop_back();
    EXPECT_FALSE(
        rom::SealNativeOverworldTerrain(original, original, partial).ok());
    auto inconsistent = edits;
    inconsistent[10].value[0] = members[1];
    EXPECT_FALSE(
        rom::SealNativeOverworldTerrain(original, original, inconsistent).ok());
    auto duplicate = edits;
    duplicate.push_back(edits.front());
    EXPECT_FALSE(
        rom::SealNativeOverworldTerrain(original, original, duplicate).ok());
    ASSERT_TRUE(image.CommitNativeTerrainImage(original, original, edits).ok());
    EXPECT_EQ(image.vector(), *saved);
    EXPECT_FALSE(
        image.CommitNativeTerrainImage(original, original, edits).ok());
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromData(image.vector()).ok());
    const auto reopened_layout = reopened.InspectLyraLayout();
    auto child = zelda3::OverworldMap::CreateFromLayout(
        members[3], &reopened, nullptr, &reopened_layout);
    ASSERT_TRUE(child.ok()) << child.status();
    EXPECT_EQ(child->parent(), parent);
    EXPECT_EQ(child->area_size(), zelda3::AreaSizeEnum::LargeArea);
  }
}

TEST(LyraLayoutDescriptorTest, NativePropertiesMirrorOnlyEditedSlots) {
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    NativeDirectoryFixture fixture(capacity);
    const uint16_t map = capacity - 1;
    const std::vector<rom::NativeOverworldPropertyEdit> edits = {
        {2, 0x9F, {5}},
        {19, map, {0x34, 0x12}},
        {6, map, {1, 2, 3, 4, 5, 6, 7, 8}}};
    const auto original = fixture.bytes;
    auto saved = rom::SealNativeOverworldTerrain(original, original, edits);
    ASSERT_TRUE(saved.ok()) << saved.status();
    EXPECT_EQ(rom::InspectLyraLayout(saved->data(), saved->size()).presence,
              rom::LyraLayoutPresence::kValid);
    EXPECT_EQ((*saved)[fixture.Entry(2).pc_address + 0x9F], 5);
    EXPECT_EQ((*saved)[0x140160 + 0x9F], 5);
    EXPECT_EQ((*saved)[fixture.Entry(19).pc_address + map * 2], 0x34);
    if (capacity == 160) {
      EXPECT_EQ((*saved)[0x1417F8 + map * 2], 0x34);
      EXPECT_EQ((*saved)[0x140480 + map * 8], 1);
    } else {
      EXPECT_EQ((*saved)[0x1417F8 + map * 2], original[0x1417F8 + map * 2]);
      EXPECT_EQ((*saved)[0x140480 + map * 8], original[0x140480 + map * 8]);
    }
    for (size_t pc = 0; pc < original.size(); ++pc) {
      if (original[pc] == (*saved)[pc])
        continue;
      bool allowed = pc >= 0x14701C && pc < 0x147020;
      for (uint16_t id : {2, 6, 19}) {
        const auto& e = fixture.Entry(id);
        allowed |= pc >= e.pc_address && pc < e.pc_address + e.count * e.stride;
        allowed |=
            pc >= fixture.EntryPc(id) + 12 && pc < fixture.EntryPc(id) + 16;
      }
      allowed |= pc == 0x140160 + 0x9F;
      if (capacity == 160) {
        allowed |= pc >= 0x1417F8 + map * 2 && pc < 0x1417F8 + map * 2 + 2;
        allowed |= pc >= 0x140480 + map * 8 && pc < 0x140480 + map * 8 + 8;
      }
      ASSERT_TRUE(allowed) << std::hex << pc;
    }
    const auto repeated =
        rom::SealNativeOverworldTerrain(*saved, *saved, edits);
    ASSERT_TRUE(repeated.ok()) << repeated.status();
    EXPECT_EQ(*repeated, *saved);
    EXPECT_EQ(fixture.bytes, original);
  }
}

TEST(LyraLayoutDescriptorTest, NativePropertiesPreserveLegacyPhaseAliases) {
  NativeDirectoryFixture fixture;
  for (const auto& edits :
       std::vector<std::vector<rom::NativeOverworldPropertyEdit>>{
           {{32, 0x90, {7}}}, {{36, 0x90, {7}}}, {{38, 0x90, {7}}}}) {
    EXPECT_FALSE(
        rom::SealNativeOverworldTerrain(fixture.bytes, fixture.bytes, edits)
            .ok());
  }
  const std::vector<rom::NativeOverworldPropertyEdit> edits = {
      {28, 0x90, {7}}, {32, 0x90, {7}},  {33, 0x90, {7}}, {34, 0x90, {7}},
      {35, 0x90, {7}}, {24, 0x90, {8}},  {36, 0x90, {8}}, {39, 0x90, {8}},
      {32, 0xBF, {9}}, {33, 0xBF, {10}}, {36, 0xBF, {11}}};
  auto saved =
      rom::SealNativeOverworldTerrain(fixture.bytes, fixture.bytes, edits);
  ASSERT_TRUE(saved.ok()) << saved.status();
  EXPECT_EQ((*saved)[0x14403 + 0x90 - 64], 7);
  EXPECT_EQ((*saved)[fixture.Entry(28).pc_address + 0x90], 7);
  EXPECT_EQ((*saved)[0x166E1 + 0x90 - 128], 8);
  EXPECT_EQ((*saved)[fixture.Entry(32).pc_address + 0xBF], 9);
  EXPECT_EQ((*saved)[fixture.Entry(33).pc_address + 0xBF], 10);
  EXPECT_EQ((*saved)[fixture.Entry(36).pc_address + 0xBF], 11);
  EXPECT_EQ(rom::InspectLyraLayout(saved->data(), saved->size()).presence,
            rom::LyraLayoutPresence::kValid);
}

TEST(LyraLayoutDescriptorTest,
     Revision4LayoutValidatesAndRefusesInvalidAppearance) {
  NativeDirectoryFixture fixture;
  fixture.UpgradeToRevision4();
  const auto ok =
      rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
  ASSERT_EQ(ok.presence, rom::LyraLayoutPresence::kValid) << ok.error;
  EXPECT_EQ(ok.revision, 4);

  auto rejects = [&](const char* label, auto mutate) {
    SCOPED_TRACE(label);
    auto changed = fixture.bytes;
    mutate(changed);
    Reseal(&changed, fixture.rows);
    const auto bad = rom::InspectLyraLayout(changed.data(), changed.size());
    EXPECT_EQ(bad.presence, rom::LyraLayoutPresence::kInvalid);
  };
  rejects("invalid flags byte",
          [&](auto& b) { b[fixture.Entry(410).pc_address + 0x50] = 4; });
  rejects("fullBG without bit0",
          [&](auto& b) { b[fixture.Entry(410).pc_address + 0x90] = 2; });
  rejects("rough21 nonzero flags",
          [&](auto& b) { b[fixture.Entry(410).pc_address + 0x3F] = 1; });
  rejects("palette word range", [&](auto& b) {
    const size_t row = (0xBF - 0x80) * 256;
    b[fixture.Entry(411).pc_address + row + 1] = 0x80;
  });
}

TEST(LyraLayoutDescriptorTest,
     Revision4PropertySealRejectsAppearanceEditOnRev3) {
  NativeDirectoryFixture fixture;
  std::vector<uint8_t> pal(256, 0);
  const std::vector<rom::NativeOverworldPropertyEdit> edits = {
      {410, 0xA0, {3}}, {411, 0xBF, pal}};
  EXPECT_FALSE(
      rom::SealNativeOverworldTerrain(fixture.bytes, fixture.bytes, edits)
          .ok());
}

TEST(LyraLayoutDescriptorTest,
     Revision4SourceChangesRequireExplicitPropertyEdits) {
  NativeDirectoryFixture fixture;
  fixture.UpgradeToRevision4();
  auto candidate = fixture.bytes;
  candidate[fixture.Entry(411).pc_address + 2] = 1;
  EXPECT_FALSE(rom::SealNativeOverworldTerrain(fixture.bytes, candidate).ok());
  candidate = fixture.bytes;
  candidate[fixture.Entry(410).pc_address + 0xA0] = 1;
  EXPECT_FALSE(rom::SealNativeOverworldTerrain(fixture.bytes, candidate).ok());
}

TEST(LyraLayoutDescriptorTest, Revision4AppearanceRecordSealAndPaletteWords) {
  NativeDirectoryFixture fixture;
  fixture.UpgradeToRevision4();
  const auto original = fixture.bytes;
  std::vector<uint8_t> pal(256);
  pal[2] = 0x34;
  pal[3] = 0x12;
  const std::vector<rom::NativeOverworldPropertyEdit> edits = {
      {410, 0xA0, {3}},
      {411, 0xBF, std::move(pal)},
  };
  auto saved = rom::SealNativeOverworldTerrain(original, original, edits);
  ASSERT_TRUE(saved.ok()) << saved.status();
  const size_t f = fixture.Entry(410).pc_address + 0xA0;
  EXPECT_EQ((*saved)[f], 3);
  EXPECT_EQ((*saved)[fixture.Entry(411).pc_address + (0xBF - 0x80) * 256 + 2],
            0x34);
  gfx::SnesPalette palette(std::vector<uint16_t>(256, 0));
  std::array<uint16_t, 128> words{};
  const uint8_t* row =
      saved->data() + fixture.Entry(411).pc_address + (0xBF - 0x80) * 256;
  for (size_t i = 0; i < 128; ++i)
    words[i] = static_cast<uint16_t>(row[i * 2] | (row[i * 2 + 1] << 8));
  ASSERT_TRUE(zelda3::ApplyLyraBackgroundPaletteWords128(palette, words).ok());
}

TEST(LyraLayoutDescriptorTest, NativePropertyCapabilityKeepsPublicationGuards) {
  NativeDirectoryFixture fixture;
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
  const std::vector<rom::NativeOverworldPropertyEdit> edits = {{2, 0xBF, {5}}};
  const auto original = loaded.vector();
  {
    ScopedRomReadOnly read_only(loaded);
    EXPECT_FALSE(
        loaded.CommitNativeTerrainImage(original, original, edits).ok());
  }
  {
    rom::WriteFence fence;
    const auto& table = fixture.Entry(2);
    ASSERT_TRUE(fence
                    .Allow(table.pc_address, table.pc_address + table.count,
                           "property bytes without CRC grant")
                    .ok());
    rom::ScopedWriteFence scope(&loaded, &fence);
    EXPECT_FALSE(
        loaded.CommitNativeTerrainImage(original, original, edits).ok());
  }
  EXPECT_EQ(loaded.vector(), original);
  EXPECT_FALSE(loaded.dirty());
  for (auto edit :
       std::vector<rom::NativeOverworldPropertyEdit>{{7, 0xBF, {0xBE}},
                                                     {29, 0xBF, {0}},
                                                     {16, 0xBF, {0, 0}},
                                                     {24, 0x7F, {1}},
                                                     {2, 192, {1}},
                                                     {2, 0xBF, {1, 2}}}) {
    EXPECT_FALSE(
        rom::SealNativeOverworldTerrain(original, original, std::span(&edit, 1))
            .ok());
  }
  const std::vector<rom::NativeOverworldPropertyEdit> duplicates = {
      {2, 0xBF, {1}}, {2, 0xBF, {2}}};
  EXPECT_FALSE(
      rom::SealNativeOverworldTerrain(original, original, duplicates).ok());
  auto unowned = original;
  unowned[fixture.Entry(2).pc_address + 0xBF] = 5;
  EXPECT_FALSE(rom::SealNativeOverworldTerrain(original, unowned, edits).ok());
  ASSERT_TRUE(loaded.CommitNativeTerrainImage(original, original, edits).ok());
  EXPECT_TRUE(loaded.dirty());
  const auto committed = loaded.vector();
  EXPECT_FALSE(loaded.CommitNativeTerrainImage(original, original, edits).ok());
  EXPECT_EQ(loaded.vector(), committed);
}

TEST(LyraLayoutDescriptorTest, NativePropertyTailCannotAliasNativePrefix) {
  NativeDirectoryFixture fixture;
  auto& palette = fixture.Entry(2);
  palette.pc_address = 0x140160;
  palette.snes_address = NativeDirectoryFixture::Snes(palette.pc_address);
  fixture.Seal();
  const auto layout =
      rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
  const auto original = loaded.vector();
  const std::vector<rom::NativeOverworldPropertyEdit> edits = {{2, 0xA0, {5}}};
  EXPECT_FALSE(loaded.CommitNativeTerrainImage(original, original, edits).ok());
  EXPECT_EQ(loaded.vector(), original);
  EXPECT_FALSE(loaded.dirty());
  EXPECT_EQ(loaded.vector()[0x140200], original[0x140200]);
}

TEST(LyraLayoutDescriptorTest, NativePropertyPhaseAliasesCannotHideWrites) {
  NativeDirectoryFixture fixture;
  auto& packed = fixture.Entry(28);
  packed.pc_address = 0x140170;
  packed.snes_address = NativeDirectoryFixture::Snes(packed.pc_address);
  fixture.Seal();
  ASSERT_EQ(rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size())
                .presence,
            rom::LyraLayoutPresence::kValid);
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
  const auto original = loaded.vector();
  std::vector<rom::NativeOverworldPropertyEdit> edits = {
      {32, 0x90, {7}}, {33, 0x90, {7}}, {34, 0x90, {7}}, {35, 0x90, {7}}};
  EXPECT_FALSE(loaded.CommitNativeTerrainImage(original, original, edits).ok());
  // The packed row is explicit, so it cannot bypass the same ownership check.
  edits.push_back({28, 0x90, {7}});
  EXPECT_FALSE(loaded.CommitNativeTerrainImage(original, original, edits).ok());
  EXPECT_EQ(loaded.vector(), original);
  EXPECT_FALSE(loaded.dirty());
}

TEST(LyraLayoutDescriptorTest,
     NativeDirectoryKeepsAttributionAndCrcFailuresDistinct) {
  NativeDirectoryFixture fixture;
  const auto baseline = fixture.bytes;
  fixture.bytes[fixture.Entry(300).pc_address] ^= 1;
  EXPECT_EQ(
      rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size()).error,
      "Native special-overworld data CRC mismatch");
  fixture.bytes = baseline;
  fixture.bytes[0x147000] = 'X';
  EXPECT_EQ(rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size())
                .presence,
            rom::LyraLayoutPresence::kInvalid);
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(baseline).ok());
  std::fill_n(loaded.mutable_vector().begin() + 0x147000, 1024, 0);
  EXPECT_EQ(loaded.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kInvalid);
  EXPECT_FALSE(loaded.CheckLayoutWritable().ok());
}

TEST(LyraLayoutDescriptorTest,
     NativeDirectoryReorderedDamagedMagicRefusesFreshLoadWrites) {
  for (uint16_t capacity : {160, 192}) {
    NativeDirectoryFixture fixture(capacity);
    std::reverse(fixture.rows.entries.begin(), fixture.rows.entries.end());
    fixture.Seal();
    fixture.bytes[0x147000] = 'X';
    EXPECT_EQ(rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size())
                  .presence,
              rom::LyraLayoutPresence::kInvalid);
    Rom loaded;
    ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
    const auto before = loaded.vector();
    EXPECT_EQ(loaded.InspectLyraLayout().presence,
              rom::LyraLayoutPresence::kInvalid);
    EXPECT_FALSE(loaded.CheckLayoutWritable().ok());
    EXPECT_FALSE(loaded.WriteByte(0x100, 1).ok());
    EXPECT_FALSE(loaded.WriteWord(0x100, 1).ok());
    EXPECT_EQ(loaded.vector(), before);
    EXPECT_FALSE(loaded.dirty());
  }
}

TEST(LyraLayoutDescriptorTest, NativeDirectoryReaderInstructionFitsTheCpuBank) {
  for (uint32_t site : {0x03FFFC, 0x03FFFE, 0x03FFFF}) {
    SCOPED_TRACE(site);
    NativeDirectoryFixture fixture;
    const size_t actor = fixture.Entry(301).pc_address + 4 * 16;
    Put32(&fixture.bytes, actor, site);
    const size_t pc = (site >> 16) * 0x8000 + (site & 0x7FFF);
    std::copy_n(fixture.bytes.begin() + actor + 12, 4,
                fixture.bytes.begin() + pc);
    fixture.Seal();
    const auto layout =
        rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
    if (site == 0x03FFFC) {
      EXPECT_EQ(layout.presence, rom::LyraLayoutPresence::kValid)
          << layout.error;
    } else {
      EXPECT_EQ(layout.presence, rom::LyraLayoutPresence::kInvalid);
      EXPECT_EQ(layout.error,
                "Native special-overworld reader binding invalid");
    }
  }
}

TEST(LyraLayoutDescriptorTest,
     NativeDirectoryAcceptsFeFfPoolsWithoutAcceptingWram) {
  NativeDirectoryFixture fixture;
  fixture.bytes.resize(0x400000);
  std::copy_n(fixture.bytes.begin() + 0x240000, 0x8000,
              fixture.bytes.begin() + 0x3F0000);
  for (uint16_t id : {210, 211}) {
    auto& entry = fixture.Entry(id);
    entry.pc_address = id == 210 ? 0x3F0000 : 0x3F8000;
    entry.snes_address = NativeDirectoryFixture::Snes(entry.pc_address);
  }
  for (uint16_t id : {20, 21}) {
    for (size_t map = 0; map < 192; ++map) {
      const size_t at = fixture.Entry(id).pc_address + map * 3;
      if (fixture.bytes[at + 2] == 0x48)
        fixture.bytes[at + 2] = 0xFE;
    }
  }
  fixture.bytes[0x3F8000] = 0xE4;
  fixture.bytes[0x3F8001] = 0xFF;
  fixture.bytes[0x3F8002] = 0;
  fixture.bytes[0x3F8003] = 0xFF;
  const size_t bf_high = fixture.Entry(20).pc_address + 0xBF * 3;
  fixture.Long(bf_high, 0xFF8000);
  fixture.Seal();
  const auto layout =
      rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size());
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
  const auto bf = rom::LyraMapTile32(layout, fixture.bytes.data(),
                                     fixture.bytes.size(), 0xBF);
  ASSERT_TRUE(bf.has_value());
  EXPECT_EQ((*bf)[0], 0x56);
  const auto a0 = rom::LyraMapTile32(layout, fixture.bytes.data(),
                                     fixture.bytes.size(), 0xA0);
  ASSERT_TRUE(a0.has_value());
  EXPECT_EQ((*a0)[0], 0x1234);
  fixture.bytes[bf_high + 2] = 0x7F;
  Reseal(&fixture.bytes, fixture.rows);
  EXPECT_EQ(rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size())
                .presence,
            rom::LyraLayoutPresence::kInvalid);
}

TEST(LyraLayoutDescriptorTest, NativeMapOwnsLayoutAndUsesAbsolutePhaseRecords) {
  NativeDirectoryFixture fixture;
  constexpr size_t parent = 0xB6;
  for (size_t map : {0xB6, 0xB7, 0xBE, 0xBF}) {
    fixture.bytes[fixture.Entry(7).pc_address + map] = parent;
    fixture.bytes[fixture.Entry(30).pc_address + map] = 1;
  }
  fixture.bytes[fixture.Entry(26).pc_address + parent] = 3;
  fixture.bytes[fixture.Entry(27).pc_address + parent] = 9;
  fixture.Word(fixture.Entry(19).pc_address + parent * 2, 0x1234);
  fixture.Word(fixture.Entry(19).pc_address + 0xBF * 2, 0x5678);
  fixture.Word(fixture.Entry(1).pc_address + parent * 2, 0x1111);
  fixture.Word(fixture.Entry(1).pc_address + 0xBF * 2, 0x2222);
  fixture.bytes[fixture.Entry(2).pc_address + parent] = 1;
  fixture.bytes[fixture.Entry(2).pc_address + 0xBF] = 2;
  fixture.bytes[fixture.Entry(3).pc_address + parent] = 0;
  fixture.bytes[fixture.Entry(3).pc_address + 0xBF] = 0x0F;
  fixture.bytes[fixture.Entry(4).pc_address + parent] = 3;
  fixture.bytes[fixture.Entry(4).pc_address + 0xBF] = 4;
  fixture.Word(fixture.Entry(5).pc_address + parent * 2, 0x95);
  fixture.Word(fixture.Entry(5).pc_address + 0xBF * 2, 0x9D);
  fixture.bytes[fixture.Entry(6).pc_address + parent * 8] = 5;
  fixture.bytes[fixture.Entry(6).pc_address + 0xBF * 8] = 6;
  for (int phase = 0; phase < 3; ++phase) {
    constexpr std::array<uint16_t, 3> gfx = {24, 36, 39};
    constexpr std::array<uint16_t, 3> palettes = {25, 37, 38};
    fixture.bytes[fixture.Entry(gfx[phase]).pc_address + parent] = 11 + phase;
    fixture.bytes[fixture.Entry(palettes[phase]).pc_address + parent] =
        21 + phase;
  }
  for (int phase = 0; phase < 4; ++phase)
    fixture.bytes[fixture.Entry(32 + phase).pc_address + parent] = 31 + phase;
  fixture.Seal();
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
  auto owner = std::make_shared<const rom::LyraLayoutInspection>(
      loaded.InspectLyraLayout());
  ASSERT_EQ(owner->presence, rom::LyraLayoutPresence::kValid) << owner->error;
  std::weak_ptr<const rom::LyraLayoutInspection> weak = owner;
  {
    auto map =
        zelda3::OverworldMap::CreateFromLayout(0xBF, &loaded, nullptr, owner);
    ASSERT_TRUE(map.ok()) << map.status();
    owner.reset();
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(map->parent(), parent);
    EXPECT_EQ(map->area_graphics(), 9);
    EXPECT_EQ(map->message_id(), 0x5678);
    EXPECT_EQ(map->area_specific_bg_color(), 0x2222);
    EXPECT_EQ(map->main_palette(), 2);
    EXPECT_EQ(map->animated_gfx(), 4);
    EXPECT_EQ(map->subscreen_overlay(), 0x9D);
    EXPECT_EQ(map->custom_tileset(0), 6);
    EXPECT_TRUE(std::all_of(map->mosaic_expanded().begin(),
                            map->mosaic_expanded().end(),
                            [](bool set) { return set; }));
    EXPECT_EQ(map->area_render_properties().area_specific_bg_color, 0x1111);
    EXPECT_EQ(map->render_main_palette(), 1);
    EXPECT_EQ(map->area_render_properties().animated_gfx, 3);
    EXPECT_EQ(map->render_subscreen_overlay(), 0x95);
    EXPECT_EQ(map->area_render_properties().custom_gfx_ids[0], 5);
    for (int phase = 0; phase < 3; ++phase) {
      EXPECT_EQ(map->sprite_graphics(phase), 11 + phase);
      EXPECT_EQ(map->sprite_palette(phase), 21 + phase);
    }
    for (int phase = 0; phase < 4; ++phase)
      EXPECT_EQ(map->area_music(phase), 31 + phase);
  }
  EXPECT_TRUE(weak.expired());
}

TEST(LyraLayoutDescriptorTest,
     NativeTailActorPhasesUseDeclaredBankAndKeepFlags) {
  NativeDirectoryFixture fixture;
  for (int phase = 0; phase < 3; ++phase) {
    const size_t at = fixture.Entry(100).pc_address + 4 + phase * 4;
    fixture.bytes[at] = 0x81 + phase;
    fixture.bytes[at + 1] = 0xC2 + phase;
    fixture.bytes[at + 2] = 0xA0 + phase;
    fixture.bytes[at + 3] = 0xFF;
    fixture.Word(fixture.Entry(16 + phase).pc_address + 0xBF * 2,
                 0x8004 + phase * 4);
  }
  fixture.Seal();
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
  const auto layout = loaded.InspectLyraLayout();
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
  for (int phase = 0; phase < 3; ++phase) {
    auto list = zelda3::ReadOverworldSpriteList(loaded, layout, phase, 0xBF);
    ASSERT_TRUE(list.ok()) << list.status();
    EXPECT_EQ(*list,
              (std::vector<uint8_t>{static_cast<uint8_t>(0x81 + phase),
                                    static_cast<uint8_t>(0xC2 + phase),
                                    static_cast<uint8_t>(0xA0 + phase), 0xFF}));
  }
  EXPECT_FALSE(zelda3::ReadOverworldSpriteList(loaded, layout, 3, 0xBF).ok());
  EXPECT_FALSE(zelda3::ReadOverworldSpriteList(loaded, layout, 0, 0xC0).ok());
  EXPECT_EQ(loaded.vector(), fixture.bytes);
  loaded.mutable_vector().resize(0x250000);
  EXPECT_FALSE(zelda3::ReadOverworldSpriteList(loaded, layout, 0, 0xBF).ok());
}

TEST(LyraLayoutDescriptorTest,
     NativeItemListsFollowAbsolutePointersAndBankLimits) {
  NativeDirectoryFixture fixture;
  fixture.bytes[zelda3::overworldItemsAddressBank] = 0x1B;
  for (size_t map = 0; map < 192; ++map)
    fixture.Word(fixture.Entry(23).pc_address + map * 2, 0xC2F9);
  fixture.Word(0xDC2F9, 0xFFFF);
  fixture.Word(fixture.Entry(23).pc_address + 0xBF * 2, 0xC300);
  fixture.Word(0xDC300, 2 * (3 + 2 * 64));
  fixture.bytes[0xDC302] = 0x42;
  fixture.Word(0xDC303, 0xFFFF);
  fixture.Seal();
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
  const auto layout = std::make_shared<const rom::LyraLayoutInspection>(
      loaded.InspectLyraLayout());
  ASSERT_EQ(layout->presence, rom::LyraLayoutPresence::kValid) << layout->error;
  std::vector<zelda3::OverworldMap> maps;
  for (int map = 0; map < 192; ++map) {
    auto m =
        zelda3::OverworldMap::CreateFromLayout(map, &loaded, nullptr, layout);
    ASSERT_TRUE(m.ok()) << m.status();
    maps.push_back(std::move(*m));
  }
  auto items = zelda3::LoadItems(&loaded, maps, layout.get());
  ASSERT_TRUE(items.ok()) << items.status();
  ASSERT_EQ(items->size(), 1);
  EXPECT_EQ(items->at(0).room_map_id_, 0xBF);
  EXPECT_EQ(items->at(0).id_, 0x42);
  EXPECT_EQ(items->at(0).game_x_, 3);
  EXPECT_EQ(items->at(0).game_y_, 2);
  EXPECT_EQ(loaded.vector(), fixture.bytes);
  const size_t pointer = fixture.Entry(23).pc_address + 0xBF * 2;
  loaded.mutable_vector()[pointer] = 0xFF;
  loaded.mutable_vector()[pointer + 1] = 0xFF;
  EXPECT_FALSE(zelda3::LoadItems(&loaded, maps, layout.get()).ok());
}

TEST(LyraLayoutDescriptorTest,
     NativeNormalLoadUsesDeclaredDictionariesAndAllPages) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for normal-model native fixture";
  Rom original;
  ASSERT_TRUE(original.LoadFromData(ReadBytes(path)).ok());
  zelda3::GameData original_data;
  ASSERT_TRUE(zelda3::LoadGameData(original, original_data).ok());
  auto reference =
      std::make_unique<zelda3::Overworld>(&original, &original_data);
  ASSERT_TRUE(reference->Load(&original).ok());
  const auto source_before = original.vector();
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    NativeDirectoryFixture fixture(capacity);
    fixture.SeedLoadedLegacyForModelTest(original, *reference);
    if (capacity == 192) {
      fixture.bytes[fixture.Entry(7).pc_address + 0xBF] = 0xB7;
      fixture.bytes[fixture.Entry(30).pc_address + 0xB7] = 3;
      fixture.bytes[fixture.Entry(30).pc_address + 0xBF] = 3;
      fixture.Word(fixture.Entry(1).pc_address + 0xB7 * 2, 0x5678);
      fixture.Word(fixture.Entry(1).pc_address + 0xBF * 2, 0x1234);
      fixture.Word(fixture.Entry(19).pc_address + 0xB7 * 2, 0xFEED);
      fixture.Word(fixture.Entry(19).pc_address + 0xBF * 2, 0xBEEF);
      fixture.Seal();
    }
    Rom loaded;
    ASSERT_TRUE(loaded.LoadFromData(fixture.bytes).ok());
    const auto layout = loaded.InspectLyraLayout();
    ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
    zelda3::GameData data;
    ASSERT_TRUE(zelda3::LoadGameData(loaded, data).ok());
    auto world = std::make_unique<zelda3::Overworld>(&loaded, &data);
    const auto status = world->Load(&loaded);
    ASSERT_TRUE(status.ok()) << status;
    EXPECT_FALSE(world->is_lyra_preview());
    EXPECT_EQ(world->materialized_map_count(), capacity);
    EXPECT_EQ(world->inspected_map_count(), capacity);
    ASSERT_EQ(world->tiles32_unique().size(),
              reference->tiles32_unique().size());
    EXPECT_EQ(world->tiles32_unique()[0].tile0_,
              reference->tiles32_unique()[0].tile0_);
    EXPECT_EQ(world->tiles16(), reference->tiles16());
    for (int map = 0; map < 160; ++map) {
      EXPECT_EQ(world->overworld_map(map)->message_id(),
                reference->overworld_map(map)->message_id());
      EXPECT_EQ(world->overworld_map(map)->area_specific_bg_color(),
                reference->overworld_map(map)->area_specific_bg_color());
      const int kind = map < 0x40 ? 0 : map < 0x80 ? 1 : 2;
      const auto& actual = world->GetMapTiles(kind);
      const auto& expected = reference->GetMapTiles(kind);
      const int x = map % 8 * 32, y = (map % 64) / 8 * 32;
      for (int dx = 0; dx < 32; ++dx)
        for (int dy = 0; dy < 32; ++dy)
          ASSERT_EQ(actual[x + dx][y + dy], expected[x + dx][y + dy]);
    }
    if (capacity == 192) {
      const auto special_before = world->GetMapTiles(2);
      ASSERT_TRUE(world->EnsureMapBuilt(0xBF).ok());
      EXPECT_EQ(world->GetMapTiles(2), special_before);
      EXPECT_FALSE(world->overworld_map(0xBF)->bitmap_data().empty());
      EXPECT_EQ(world->overworld_map(0xBF)->message_id(), 0xBEEF);
      EXPECT_EQ(world->overworld_map(0xBF)->area_specific_bg_color(), 0x1234);
      EXPECT_EQ(world->overworld_map(0xBF)
                    ->area_render_properties()
                    .area_specific_bg_color,
                0x5678);
      const auto& tile = reference->tiles32_unique()[0x56];
      EXPECT_EQ(world->GetMapTiles(2)[7 * 32][7 * 32], tile.tile0_);
      EXPECT_FALSE(world->overworld_map(0xC0));
    } else {
      EXPECT_FALSE(world->EnsureMapBuilt(0xA0).ok());
    }
    EXPECT_EQ(loaded.vector(), fixture.bytes);
    ASSERT_TRUE(world->Load(&loaded).ok());
    EXPECT_EQ(world->materialized_map_count(), capacity);
    if (capacity == 192) {
      EXPECT_EQ(world->overworld_map(0xBF)->message_id(), 0xBEEF);
      EXPECT_EQ(world->overworld_map(0xBF)->area_specific_bg_color(), 0x1234);
    }
    EXPECT_EQ(loaded.vector(), fixture.bytes);
  }
  EXPECT_EQ(original.vector(), source_before);
}

std::array<uint16_t, 256> DecodePilotLiterals(
    const std::vector<uint8_t>& bytes) {
  std::array<uint16_t, 256> tiles{};
  // Independent byte-level oracle for this writer's literal-only encoding.
  // Do not call the production descriptor decoder for persisted terrain proof.
  for (size_t pc : {size_t{0x132000}, size_t{0x132200}}) {
    EXPECT_EQ(bytes[pc], 0xE0);
    EXPECT_EQ(bytes[pc + 1], 0xFF);
    EXPECT_EQ(bytes[pc + 258], 0xFF);
  }
  for (size_t i = 0; i < tiles.size(); ++i) {
    tiles[i] = (bytes[0x132002 + i] << 8) | bytes[0x132202 + i];
  }
  return tiles;
}

bool PilotWriteOwns(size_t pc) {
  return (pc >= 0x132000 && pc < 0x132103) ||
         (pc >= 0x132200 && pc < 0x132303) ||
         (pc >= 0x14701C && pc < 0x147020) ||
         (pc >= 0x14724C && pc < 0x147250) || (pc >= 0x14725C && pc < 0x147260);
}

TEST(LyraA0TerrainTest, Revision2CapabilityDoesNotEnableGenericWrites) {
  auto bytes = ReadBytes("LYRA_WRITABLE_A0.bin");
  const auto offline = rom::InspectLyraLayout(bytes.data(), bytes.size());
  ASSERT_EQ(offline.presence, rom::LyraLayoutPresence::kValid) << offline.error;
  EXPECT_TRUE(offline.a0_terrain_write_supported);
  EXPECT_TRUE(offline.offline_only);
  EXPECT_FALSE(offline.write_supported);
  ASSERT_EQ(offline.entries.size(), 36u);
  EXPECT_EQ(offline.entries.back().pc_address, 0x132000u);
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(bytes).ok());
  EXPECT_FALSE(loaded.CheckLayoutWritable().ok());
  EXPECT_FALSE(loaded.WriteByte(0x100, 1).ok());
  EXPECT_FALSE(loaded.WriteWord(0x132002, 1).ok());
  EXPECT_FALSE(loaded.SaveToFile({}).ok());

  bytes[0x147015] = 2;
  bytes[0x147010] = 161;
  bytes[offline.entries[28].pc_address + 0xA0] = 0;
  Reseal(&bytes, offline);
  const auto runtime = rom::InspectLyraLayout(bytes.data(), bytes.size());
  ASSERT_EQ(runtime.presence, rom::LyraLayoutPresence::kValid) << runtime.error;
  EXPECT_FALSE(runtime.offline_only);
  EXPECT_FALSE(runtime.a0_terrain_write_supported);
  EXPECT_FALSE(runtime.write_supported);
  EXPECT_FALSE(
      rom::SerializeLyraA0Terrain(bytes, DecodePilotLiterals(bytes)).ok());
  for (const auto [flags, active] : {std::pair{2, 160}, std::pair{3, 161},
                                     std::pair{7, 161}, std::pair{3, 192}}) {
    auto invalid = bytes;
    invalid[0x147015] = flags;
    invalid[0x147010] = active;
    Reseal(&invalid, offline);
    EXPECT_EQ(rom::InspectLyraLayout(invalid.data(), invalid.size()).presence,
              rom::LyraLayoutPresence::kInvalid);
  }
}

TEST(LyraA0TerrainTest, RejectsSlotAliasesConsumedOverlapsAndOverflow) {
  const auto bytes = ReadBytes("LYRA_WRITABLE_A0.bin");
  const auto layout = rom::InspectLyraLayout(bytes.data(), bytes.size());
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid);
  auto invalid = bytes;
  // A legacy stream starts before the reservation but consumes its first bytes.
  const size_t start = 0x131F00;
  invalid[start] = 0xE0;
  invalid[start + 1] = 0xFF;
  std::fill_n(invalid.begin() + start + 2, 256, 0);
  invalid[start + 258] = 0xFF;
  const size_t pointer = layout.entries[19].pc_address;
  const uint32_t snes = 0x269F00;
  for (int i = 0; i < 3; ++i)
    invalid[pointer + i] = static_cast<uint8_t>(snes >> (8 * i));
  Reseal(&invalid, layout);
  auto checked = rom::InspectLyraLayout(invalid.data(), invalid.size());
  EXPECT_EQ(checked.presence, rom::LyraLayoutPresence::kInvalid);
  EXPECT_NE(checked.error.find("overlaps another map"), std::string::npos);

  invalid = bytes;
  // Both A0 halves must point to their own exact slot, without aliasing.
  const size_t low_pointer = layout.entries[20].pc_address + 0xA0 * 3;
  std::copy_n(bytes.begin() + layout.entries[19].pc_address + 0xA0 * 3, 3,
              invalid.begin() + low_pointer);
  Reseal(&invalid, layout);
  EXPECT_EQ(rom::InspectLyraLayout(invalid.data(), invalid.size()).presence,
            rom::LyraLayoutPresence::kInvalid);

  invalid = bytes;
  for (size_t i = 0; i < 256; ++i) {
    invalid[0x132000 + i * 2] = 0;  // 256 one-byte literal commands.
    invalid[0x132001 + i * 2] = 0x21;
  }
  invalid[0x132200] =
      0xFF;  // Consumed extent is 513, beyond the 512-byte slot.
  Reseal(&invalid, layout);
  checked = rom::InspectLyraLayout(invalid.data(), invalid.size());
  EXPECT_EQ(checked.presence, rom::LyraLayoutPresence::kInvalid);
  EXPECT_NE(checked.error.find("dedicated slot"), std::string::npos);

  invalid = bytes;
  invalid[0x147020 + 35 * 16 + 10] = 1;  // 513-byte stride.
  Reseal(&invalid, layout);
  EXPECT_EQ(rom::InspectLyraLayout(invalid.data(), invalid.size()).presence,
            rom::LyraLayoutPresence::kInvalid);
}

TEST(LyraA0TerrainTest, AtomicSaveAsGuardsSourceSnapshotAndEntryAnchors) {
  const auto bytes = ReadBytes("LYRA_WRITABLE_A0.bin");
  const auto source = test::UniqueTempPath("lyra_a0_source", ".sfc");
  const auto target = test::UniqueTempPath("lyra_a0_target", ".sfc");
  WriteBytes(source, bytes);
  WriteBytes(target, bytes);
  const auto cleanup = absl::MakeCleanup([&] {
    std::filesystem::remove(source);
    std::filesystem::remove(target);
    for (const auto& entry :
         std::filesystem::directory_iterator(target.parent_path())) {
      if (entry.path().string().starts_with(target.string() + "_backup_"))
        std::filesystem::remove(entry.path());
    }
  });
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromFile(source.string()).ok());
  auto tiles = DecodePilotLiterals(bytes);
  tiles[5 * 16 + 5] = 0x20B2;
  loaded.set_dirty(true);
  {
    ScopedRomReadOnly read_only(loaded);
    const auto refused = loaded.SaveLyraA0TerrainAs(tiles, target.string());
    EXPECT_EQ(refused.code(), absl::StatusCode::kPermissionDenied);
    EXPECT_EQ(loaded.vector(), bytes);
    EXPECT_EQ(loaded.filename(), source.string());
    EXPECT_EQ(ReadBytes(source), bytes);
    EXPECT_EQ(ReadBytes(target), bytes);
  }
  EXPECT_FALSE(loaded.SaveLyraA0TerrainAs(tiles, source.string()).ok());
  loaded.mutable_vector()[0x100] ^= 1;
  EXPECT_FALSE(loaded.SaveLyraA0TerrainAs(tiles, target.string()).ok());
  EXPECT_EQ(ReadBytes(target), bytes);
  loaded.mutable_vector()[0x100] ^= 1;
  auto blocked = tiles;
  blocked[7 * 16 + 7] = 0x20B2;
  EXPECT_FALSE(loaded.SaveLyraA0TerrainAs(blocked, target.string()).ok());
  EXPECT_EQ(ReadBytes(target), bytes);
  Rom::SetStagingFailureForTesting(true);
  const auto reset_staging =
      absl::MakeCleanup([] { Rom::SetStagingFailureForTesting(false); });
  EXPECT_FALSE(loaded.SaveLyraA0TerrainAs(tiles, target.string()).ok());
  EXPECT_EQ(loaded.vector(), bytes);
  EXPECT_EQ(loaded.filename(), source.string());
  EXPECT_TRUE(loaded.dirty());
  EXPECT_EQ(ReadBytes(source), bytes);
  EXPECT_EQ(ReadBytes(target), bytes);
}

TEST(LyraLayoutDescriptorTest, NativeTerrainPublicationKeepsGuardsAndMirrors) {
  NativeDirectoryFixture fixture;
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  const auto original = document.vector();
  ASSERT_TRUE(document.CheckLayoutSaveable().ok());
  EXPECT_FALSE(document.CheckLayoutWritable().ok());
  EXPECT_FALSE(document.WriteByte(fixture.Entry(400).pc_address, 1).ok());
  EXPECT_FALSE(document.WriteWord(fixture.Entry(400).pc_address, 1).ok());
  EXPECT_EQ(document.vector(), original);
  for (size_t pc : {size_t{0x1234}, size_t{fixture.Entry(200).pc_address},
                    size_t{fixture.Entry(300).pc_address}, size_t{0x14701C},
                    fixture.EntryPc(20) + 4}) {
    auto candidate = original;
    candidate[pc] ^= 1;
    EXPECT_FALSE(document.CommitNativeTerrainImage(original, candidate).ok())
        << std::hex << pc;
    EXPECT_EQ(document.vector(), original);
    EXPECT_FALSE(document.dirty());
  }
  auto candidate = original;
  candidate[fixture.Entry(400).pc_address] ^= 1;
  {
    ScopedRomReadOnly read_only(document);
    EXPECT_FALSE(document.CommitNativeTerrainImage(original, candidate).ok());
    EXPECT_FALSE(document.CheckLayoutSaveable().ok());
  }
  {
    rom::WriteFence fence;
    ASSERT_TRUE(fence
                    .Allow(fixture.Entry(400).pc_address,
                           fixture.Entry(400).pc_address + 32768,
                           "dictionary only, no CRC permission")
                    .ok());
    rom::ScopedWriteFence scope(&document, &fence);
    EXPECT_FALSE(document.CommitNativeTerrainImage(original, candidate).ok());
    EXPECT_TRUE(fence.written_ranges().empty());
  }
  EXPECT_EQ(document.vector(), original);
  auto stale = original;
  stale[0x1234] ^= 1;
  EXPECT_FALSE(document.CommitNativeTerrainImage(stale, candidate).ok());
  auto divergent = original;
  divergent[fixture.Entry(20).pc_address] ^= 1;
  EXPECT_FALSE(document.CommitNativeTerrainImage(original, divergent).ok());
  auto broken = original;
  // Both tables agree but the stream is outside all declared pools.
  for (size_t pc : {size_t{0x1794D}, size_t{fixture.Entry(20).pc_address}}) {
    broken[pc] = 0;
    broken[pc + 1] = 0x80;
    broken[pc + 2] = 0x7E;
  }
  EXPECT_FALSE(document.CommitNativeTerrainImage(original, broken).ok());
  EXPECT_EQ(document.vector(), original);
  ASSERT_TRUE(document.CommitNativeTerrainImage(original, candidate).ok());
  EXPECT_TRUE(document.dirty());
  EXPECT_EQ(document.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kValid);
  const auto saved = document.vector();
  auto damaged_crc = saved;
  damaged_crc[0x14701C] ^= 1;
  Rom invalid;
  ASSERT_TRUE(invalid.LoadFromData(damaged_crc).ok());
  EXPECT_FALSE(invalid.CheckLayoutSaveable().ok());
}

TEST(LyraLayoutDescriptorTest, NativeTerrainPlannerUsesCompleteDirectory) {
  for (int capacity : {160, 192}) {
    NativeDirectoryFixture fixture(capacity);
    Rom document;
    ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
    auto storage = zelda3::ResolveNativeOverworldTerrainStorage(document);
    ASSERT_TRUE(storage.ok()) << storage.status();
    std::vector<zelda3::OverworldTerrainPage> pages(capacity);
    const auto layout = document.InspectLyraLayout();
    for (int map = 0; map < capacity; ++map) {
      const auto decoded =
          rom::LyraMapTile32(layout, document.data(), document.size(), map);
      ASSERT_TRUE(decoded.has_value());
      pages[map] = *decoded;
    }
    pages[0][5] = 0x1234;
    pages[0x9F][7] = 0x1234;
    if (capacity == 192) {
      pages[0xA0][9] = 0x0056;
      pages[0xBF][11] = 0x1234;
      auto partial = *storage;
      partial.map_count = 160;
      EXPECT_FALSE(
          zelda3::PlanOverworldTerrainSave(document, partial, pages).ok());
    }
    auto wrong_pool = *storage;
    wrong_pool.stream_regions[0].start += 4;
    EXPECT_FALSE(
        zelda3::PlanOverworldTerrainSave(document, wrong_pool, pages).ok());
    auto plan = zelda3::PlanOverworldTerrainSave(document, *storage, pages);
    ASSERT_TRUE(plan.ok()) << plan.status();
    const auto applied = zelda3::ApplyOverworldTerrainSave(document, *plan);
    ASSERT_TRUE(applied.ok()) << applied;
    const auto saved_layout = document.InspectLyraLayout();
    ASSERT_EQ(saved_layout.presence, rom::LyraLayoutPresence::kValid);
    for (int map = 0; map < capacity; ++map) {
      const auto decoded = rom::LyraMapTile32(saved_layout, document.data(),
                                              document.size(), map);
      ASSERT_TRUE(decoded.has_value());
      EXPECT_EQ(*decoded, pages[map]) << map;
    }
    for (int id : {200, 201, 202}) {
      const auto& e = fixture.Entry(id);
      EXPECT_TRUE(std::equal(fixture.bytes.begin() + e.pc_address,
                             fixture.bytes.begin() + e.pc_address + e.count,
                             document.vector().begin() + e.pc_address));
    }
    auto noop = zelda3::PlanOverworldTerrainSave(document, *storage, pages);
    ASSERT_TRUE(noop.ok()) << noop.status();
    EXPECT_TRUE(noop->writes.empty());
  }
}

TEST(LyraLayoutDescriptorTest,
     NativeSpriteSavePartitionsMirrorsAndPreservesUnloadedLists) {
  for (uint16_t capacity : {160, 192}) {
    NativeDirectoryFixture fixture(capacity);
    fixture.SeedNativeSpritePrefix();
    const std::vector<uint8_t> untouched = {0xC1, 0x82, 0x52, 2, 3, 0x53, 0xFF};
    std::copy(untouched.begin(), untouched.end(),
              fixture.bytes.begin() + 0x4C882);
    fixture.Word(fixture.Entry(17).pc_address + 10, 0xC882);
    fixture.Word(0x141578 + 10, 0xC882);
    fixture.Seal();
    const auto before = fixture.bytes;
    std::vector<rom::NativeOverworldSpriteEdit> edits = {
        {0, 0, {0x81, 0xC2, 0x54, 0xFF}}, {2, 0x9F, {2, 3, 0x55, 0xFF}}};
    if (capacity == 192) {
      edits.push_back({1, 0xA0, {0x81, 0xC2, 0x54, 0xFF}});
      edits.push_back({2, 0xBF, {3, 4, 0x56, 0xFF}});
    }
    Rom document;
    ASSERT_TRUE(document.LoadFromData(before).ok());
    ASSERT_TRUE(
        document.CommitNativeTerrainImage(before, before, {}, edits).ok());
    const auto layout = document.InspectLyraLayout();
    ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
    for (const auto& edit : edits) {
      const auto bytes = zelda3::ReadOverworldSpriteList(
          document, layout, edit.phase, edit.map_id);
      ASSERT_TRUE(bytes.ok()) << bytes.status();
      EXPECT_EQ(*bytes, edit.value);
    }
    EXPECT_EQ(*zelda3::ReadOverworldSpriteList(document, layout, 1, 5),
              untouched);
    for (int phase = 0; phase < 3; ++phase) {
      constexpr std::array<size_t, 3> mirrors = {0x141438, 0x141578, 0x1416B8};
      EXPECT_TRUE(std::equal(
          document.vector().begin() + fixture.Entry(16 + phase).pc_address,
          document.vector().begin() + fixture.Entry(16 + phase).pc_address +
              320,
          document.vector().begin() + mirrors[phase]));
    }
    const auto saved = document.vector();
    document.set_dirty(false);
    ASSERT_TRUE(
        document.CommitNativeTerrainImage(saved, saved, {}, edits).ok());
    EXPECT_EQ(document.vector(), saved);
    EXPECT_FALSE(document.dirty());
    EXPECT_EQ(fixture.bytes, before);
  }
}

TEST(LyraLayoutDescriptorTest,
     NativeSpriteSaveRefusesOverflowAliasesAndReadOnlyWithoutMutation) {
  NativeDirectoryFixture fixture;
  fixture.SeedNativeSpritePrefix();
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  const auto before = document.vector();
  std::vector<rom::NativeOverworldSpriteEdit> edits = {
      {0, 0xA0, {1, 2, 3, 4, 5, 6, 0xFF}},
      {1, 0xA1, {1, 2, 4, 4, 5, 7, 0xFF}},
      {2, 0xBF, {1, 2, 5, 4, 5, 8, 0xFF}}};
  EXPECT_EQ(document.CommitNativeTerrainImage(before, before, {}, edits).code(),
            absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(document.vector(), before);
  edits.resize(1);
  {
    ScopedRomReadOnly read_only(document);
    EXPECT_FALSE(
        document.CommitNativeTerrainImage(before, before, {}, edits).ok());
  }
  {
    rom::WriteFence fence;
    ASSERT_TRUE(fence.Allow(0x147000, 0x147400, "directory only").ok());
    rom::ScopedWriteFence scoped(&document, &fence);
    EXPECT_FALSE(
        document.CommitNativeTerrainImage(before, before, {}, edits).ok());
  }
  EXPECT_EQ(document.vector(), before);
  EXPECT_FALSE(document.dirty());
  // A resealed actor allocation must not borrow the original bank09 pool.
  fixture.rows.tail_actor_bank = fixture.bytes[0x147014] = 9;
  auto& actor = fixture.Entry(100);
  actor.pc_address = 0x4C881;
  actor.snes_address = NativeDirectoryFixture::Snes(actor.pc_address);
  for (int phase = 0; phase < 3; ++phase)
    for (int map = 160; map < 192; ++map)
      fixture.Word(fixture.Entry(16 + phase).pc_address + map * 2, 0xC881);
  fixture.Seal();
  ASSERT_EQ(rom::InspectLyraLayout(fixture.bytes.data(), fixture.bytes.size())
                .presence,
            rom::LyraLayoutPresence::kValid);
  const auto refused = rom::PlanNativeOverworldSpriteSave(fixture.bytes, edits);
  EXPECT_EQ(refused.status().code(), absl::StatusCode::kPermissionDenied);
}

class NativeSpecialOverworldEditorTest : public test::HeadlessEditorTest {};

TEST_F(NativeSpecialOverworldEditorTest, SavedNativeSourceExport) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for saved native source export";
  Rom legacy;
  ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
  zelda3::Overworld reference(&legacy);
  ASSERT_TRUE(reference.Load(&legacy).ok());
  const auto old_addresses = core::RomSettings::Get().address_overrides();
  auto restore = absl::MakeCleanup(
      [&] { core::RomSettings::Get().SetAddressOverrides(old_addresses); });
  core::RomSettings::Get().ClearOverrides();
  for (int capacity : {160, 192}) {
    for (int revision : {3, 4}) {
      SCOPED_TRACE(
          absl::StrFormat("capacity%d revision%d", capacity, revision));
      NativeDirectoryFixture fixture(capacity);
      fixture.SeedLoadedLegacyForModelTest(legacy, reference);
      for (int map = 128; map < capacity; ++map) {
        fixture.bytes[fixture.Entry(7).pc_address + map] = map;
        fixture.bytes[fixture.Entry(30).pc_address + map] = 0;
      }
      const int selected = capacity == 192 ? 0xB1 : 0x91;
      const int parent = selected - 1;
      for (int member : {parent, selected}) {
        fixture.bytes[fixture.Entry(7).pc_address + member] = parent;
        fixture.bytes[fixture.Entry(30).pc_address + member] = 2;
        fixture.bytes[fixture.Entry(2).pc_address + member] = 2;
        fixture.bytes[fixture.Entry(31).pc_address + member] = 2;
      }
      for (const auto [id, graphics] :
           {std::pair{24, 1}, std::pair{36, 2}, std::pair{39, 3}})
        fixture.bytes[fixture.Entry(id).pc_address + parent] = graphics;
      if (revision == 4) {
        fixture.UpgradeToRevision4();
        fixture.bytes[fixture.Entry(410).pc_address + selected] = 3;
        fixture.Word(
            fixture.Entry(411).pc_address + (selected - 128) * 256 + 254,
            0x3210);
      }
      fixture.Seal();
      Rom document;
      ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
      const auto before = document.vector();
      cli::handlers::OverworldExportNativeSourceCommandHandler command;
      cli::resources::ArgumentParser args({});
      auto run = [&]() -> absl::StatusOr<nlohmann::json> {
        cli::resources::OutputFormatter out(
            cli::resources::OutputFormatter::Format::kJson);
        out.BeginObject();
        const auto result = command.Execute(&document, args, out);
        if (!result.ok())
          return result;
        out.EndObject();
        return nlohmann::json::parse(out.GetOutput());
      };
      const auto exported = run();
      ASSERT_TRUE(exported.ok()) << exported.status();
      const auto& source = exported->at("native_source");
      EXPECT_EQ(source["schema"], "yaze.special-overworld-native-source.v1");
      EXPECT_EQ(source["directory_revision"], revision);
      EXPECT_EQ(source["materialized_map_count"], capacity);
      EXPECT_EQ(source["pages"].size(), capacity - 128);
      EXPECT_EQ(
          source["source_rom_sha256"],
          core::ComputeSourceArtifactSha256(
              {reinterpret_cast<const char*>(before.data()), before.size()}));
      EXPECT_EQ(exported->at("native_source_sha256"),
                core::ComputeSourceArtifactSha256(source.dump()));
      const auto& page = source["pages"][selected - 128];
      EXPECT_EQ(page["parent_map_id"], parent);
      EXPECT_EQ(page["tiles"].size(), 1024);
      EXPECT_EQ(page["resolved_states"].size(), revision == 4 ? 3 : 0);
      zelda3::GameData data;
      zelda3::LoadOptions options;
      options.expand_rom = false;
      ASSERT_TRUE(zelda3::LoadGameData(document, data, options).ok());
      zelda3::Overworld expected(&document, &data);
      ASSERT_TRUE(expected.Load(&document).ok());
      EXPECT_EQ(
          page["tiles"][19 * 32 + 7],
          expected.GetMapTiles(
              2)[(selected % 8) * 32 + 7][((selected % 64) / 8) * 32 + 19]);
      const auto area = std::find_if(source["gameplay_areas"].begin(),
                                     source["gameplay_areas"].end(),
                                     [parent](const auto& item) {
                                       return item["parent_map_id"] == parent;
                                     });
      ASSERT_NE(area, source["gameplay_areas"].end());
      EXPECT_EQ((*area)["size"], "wide");
      EXPECT_EQ((*area)["maps"], nlohmann::json::array({parent, selected}));
      for (int phase = 0; revision == 4 && phase < 3; ++phase) {
        const auto& state = page["resolved_states"][phase];
        EXPECT_EQ(state["sprite_graphics"], phase + 1);
        std::vector<uint8_t> pixels(0x10000, 0);
        for (int slot = 0; slot < 16; ++slot) {
          const auto& description = state["slots"][slot];
          if (description["empty"].get<bool>())
            continue;
          const size_t sheet = description["sheet_id"].get<size_t>();
          for (int pixel = 0; pixel < 0x1000; ++pixel)
            pixels[slot * 0x1000 + pixel] = static_cast<uint8_t>(
                data.graphics_buffer[sheet * 0x1000 + pixel] +
                ((slot == 0 || slot == 3 || slot == 4 || slot == 5) ? 0x88
                                                                    : 0));
        }
        const size_t animated = state["animation"]["sheet_id"].get<size_t>();
        if (animated != 0)
          std::copy_n(data.graphics_buffer.begin() + animated * 0x1000, 0x800,
                      pixels.begin() + 7 * 0x1000);
        EXPECT_EQ(
            state["graphics_sha256"],
            core::ComputeSourceArtifactSha256(
                {reinterpret_cast<const char*>(pixels.data()), pixels.size()}));
      }
      if (revision == 4)
        EXPECT_NE(page["resolved_states"][0]["graphics_sha256"],
                  page["resolved_states"][1]["graphics_sha256"]);
      if (revision == 4) {
        EXPECT_EQ(page["source_profile"]["appearance_flags"], 3);
        EXPECT_EQ(page["source_profile"]["background_palette_words"][127],
                  0x3210);
        for (const auto& state : page["resolved_states"])
          EXPECT_EQ(state["palette"]["words"][127], 0x3210);
      } else {
        EXPECT_TRUE(
            page["source_profile"]["background_palette_words"].is_null());
      }
      const auto repeated = run();
      ASSERT_TRUE(repeated.ok()) << repeated.status();
      EXPECT_EQ(*repeated, *exported);
      EXPECT_EQ(document.vector(), before);
      EXPECT_FALSE(document.dirty());
      document.set_dirty(true);
      const auto refused = run();
      EXPECT_TRUE(absl::IsFailedPrecondition(refused.status()));
    }
  }
}

TEST_F(NativeSpecialOverworldEditorTest,
       SpriteSaveEditorUndoFailureRetryAndFreshReopen) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native sprite Save/reopen";
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    const auto flags = core::FeatureFlags::get();
    auto restore_flags =
        absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
    core::FeatureFlags::get().overworld.kSaveOverworldMaps = false;
    core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
    Rom legacy;
    ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
    zelda3::Overworld reference(&legacy);
    ASSERT_TRUE(reference.Load(&legacy).ok());
    NativeDirectoryFixture fixture(capacity);
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    const auto root = test::UniqueTempPath("ows");
    std::filesystem::create_directories(root);
    auto cleanup =
        absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
    const auto source = root / "in.sfc", target = root / "out.sfc";
    WriteBytes(source, fixture.bytes);
    auto manager = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager, renderer_.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
    ASSERT_TRUE(
        manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
    auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
    auto& world = editor->overworld();
    auto* document = manager->GetCurrentRom();
    editor->SetPreviewStoryState(1);
    const int owner =
        capacity == 192 ? world.overworld_map(capacity - 1)->parent() : 2;
    const ImVec2 position((owner % 8) * 512 + 16,
                          ((owner % 64) / 8) * 512 + 16);
    const auto original_count = world.sprites(1).size();
    {
      ScopedRomReadOnly read_only(*document);
      EXPECT_FALSE(editor::InsertSprite(&world, position, owner, 1, 0x52).ok());
      EXPECT_EQ(world.sprites(1).size(), original_count);
    }
    if (capacity == 160) {
      const auto original = zelda3::ReadOverworldSpriteList(
          *document, document->InspectLyraLayout(), 1, owner);
      ASSERT_TRUE(original.ok()) << original.status();
      auto more = *original;
      more.insert(more.end() - 1, {1, 1, 0x52});
      const std::vector<rom::NativeOverworldSpriteEdit> overflow_edits = {
          {1, static_cast<uint16_t>(owner), more}};
      const auto overflow = rom::PlanNativeOverworldSpriteSave(
          document->vector(), overflow_edits);
      EXPECT_EQ(overflow.status().code(), absl::StatusCode::kResourceExhausted)
          << overflow.status();
      EXPECT_EQ(document->vector(), fixture.bytes);
    }
    auto existing = std::find_if(
        world.mutable_sprites(1)->begin(), world.mutable_sprites(1)->end(),
        [owner](const auto& sprite) { return sprite.map_id() == owner; });
    if (capacity == 160)
      ASSERT_NE(existing, world.mutable_sprites(1)->end());
    // The real bank09 fixture is full (2609/2609 bytes); edit its existing
    // unique map2 stream. Added maps use the separately declared tail pool.
    const auto added =
        capacity == 192 ? editor::InsertSprite(&world, position, owner, 1, 0x52)
                        : absl::StatusOr<zelda3::Sprite*>(&*existing);
    ASSERT_TRUE(added.ok()) << added.status();
    const auto old_id = (*added)->id();
    const size_t row = *added - world.mutable_sprites(1)->data();
    const auto& model_sprites = *world.mutable_sprites(1);
    const size_t owner_row =
        std::count_if(model_sprites.begin(), model_sprites.begin() + row,
                      [owner](const auto& sprite) {
                        return sprite.map_id() == owner && !sprite.deleted();
                      });
    auto draft = **added;
    if (capacity == 192) {
      auto outside = draft;
      outside.set_x((owner % 8) * 512 + 512);
      EXPECT_FALSE(editor->ValidateSpriteEdit(*added, outside).ok());
      const int old_x = (*added)->x();
      (*added)->set_x(outside.x());
      EXPECT_FALSE(world.PrepareNativeSpriteSave().ok());
      (*added)->set_x(old_x);
    }
    draft.set_id(0x53);
    draft.set_x(draft.x() + 16);
    {
      ScopedRomReadOnly read_only(*document);
      EXPECT_FALSE(editor->ApplySpriteEdit(*added, draft).ok());
      EXPECT_EQ((*added)->id(), old_id);
    }
    ASSERT_TRUE(editor->ApplySpriteEdit(*added, draft).ok());
    ASSERT_TRUE(editor->Undo().ok());
    EXPECT_EQ((*added)->id(), old_id);
    ASSERT_TRUE(editor->Redo().ok());
    EXPECT_EQ((*added)->id(), 0x53);
    EXPECT_EQ(document->vector(), fixture.bytes);
    const auto planned = world.PrepareNativeSpriteSave();
    ASSERT_TRUE(planned.ok()) << planned.status();
    const auto expected = std::find_if(
        planned->begin(), planned->end(), [owner](const auto& edit) {
          return edit.phase == 1 && edit.map_id == owner;
        });
    ASSERT_NE(expected, planned->end());
    {
      test::ScopedRomStagingFailure fault;
      EXPECT_FALSE(manager->SaveRomAs(target.string()).ok());
    }
    EXPECT_EQ(document->vector(), fixture.bytes);
    EXPECT_FALSE(std::filesystem::exists(target));
    const auto saved_status = manager->SaveRomAs(target.string());
    ASSERT_TRUE(saved_status.ok()) << saved_status;
    const auto saved = ReadBytes(target);
    ASSERT_NE(saved, fixture.bytes);
    ASSERT_TRUE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), saved);
    // Disabling a domain leaves its edits pending for a later enabled Save.
    auto later = world.mutable_sprites(1)->at(row);
    later.set_id(0x54);
    ASSERT_TRUE(
        editor->ApplySpriteEdit(&world.mutable_sprites(1)->at(row), later)
            .ok());
    core::FeatureFlags::get().overworld.kSaveOverworldSprites = false;
    ASSERT_TRUE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), saved);
    core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
    ASSERT_TRUE(manager->SaveRom().ok());
    const auto final = ReadBytes(target);
    EXPECT_NE(final, saved);
    manager.reset();
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
    const auto layout = reopened.InspectLyraLayout();
    ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid);
    const auto bytes =
        zelda3::ReadOverworldSpriteList(reopened, layout, 1, owner);
    ASSERT_TRUE(bytes.ok()) << bytes.status();
    auto final_expected = expected->value;
    final_expected[owner_row * 3 + 2] = 0x54;
    EXPECT_EQ(*bytes, final_expected);
    zelda3::Overworld fresh(&reopened);
    ASSERT_TRUE(fresh.Load(&reopened).ok());
    EXPECT_EQ(fresh.materialized_map_count(), capacity);
    EXPECT_EQ(reopened.vector(), final);
  }
}

TEST_F(NativeSpecialOverworldEditorTest, SidecarLifecycle) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native editor lifecycle";
  // Keep sibling atomic-save names below the Windows legacy path limit.
  const auto root = test::UniqueTempPath("ow3");
  std::filesystem::create_directories(root);
  auto cleanup = absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
  project::YazeProject project;
  project.name = "Native editor lifecycle fixture";
  project.filepath = (root / "fixture.yaze").string();
  project.rom_filename = path;
  const auto created = project.SaveNew();
  ASSERT_TRUE(created.ok()) << created;
  Rom document;
  ASSERT_TRUE(document.LoadFromData(ReadBytes(path)).ok());
  zelda3::GameData data;
  ASSERT_TRUE(zelda3::LoadGameData(document, data).ok());
  editor::EditorDependencies deps;
  deps.rom = &document;
  deps.project = &project;
  deps.renderer = renderer_.get();
  deps.window_manager = window_manager_.get();
  auto editor = std::make_unique<editor::OverworldEditor>(&document, deps);
  editor->SetGameData(&data);
  editor->Initialize();
  ASSERT_TRUE(editor->Load().ok());
  NativeDirectoryFixture fixture;
  fixture.SeedLoadedLegacyForModelTest(document, editor->overworld());
  Rom replacement;
  ASSERT_TRUE(replacement.LoadFromData(fixture.bytes).ok());
  ASSERT_TRUE(editor->CreateLyraTerrain().ok());
  ASSERT_TRUE(editor->lyra_canvas_.dirty());
  const auto* draft = editor->lyra_canvas_.document();
  const auto before = document.vector();
  EXPECT_FALSE(editor->CheckRomReplacement(replacement).ok());
  EXPECT_FALSE(editor->Load().ok());
  EXPECT_EQ(editor->lyra_canvas_.document(), draft);
  EXPECT_TRUE(editor->lyra_canvas_.dirty());
  EXPECT_EQ(document.vector(), before);

  const auto saved = editor->lyra_canvas_.Save(&project, editor->overworld());
  ASSERT_TRUE(saved.ok()) << saved;
  const auto project_saved = project.Save();
  ASSERT_TRUE(project_saved.ok()) << project_saved;
  editor->ProjectSaveCommitted();
  ASSERT_FALSE(editor->lyra_canvas_.dirty());
  const auto sidecar = project.lyra_terrain_file;
  const auto sidecar_before = ReadBytes(sidecar);
  ASSERT_FALSE(sidecar_before.empty());
  ASSERT_TRUE(editor->CheckRomReplacement(replacement).ok());
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  ASSERT_TRUE(zelda3::LoadGameData(document, data).ok());
  const auto loaded = editor->Load();
  ASSERT_TRUE(loaded.ok()) << loaded;
  EXPECT_FALSE(editor->is_lyra_preview());
  EXPECT_FALSE(editor->lyra_canvas_.loaded());
  EXPECT_FALSE(editor->IsProjectTerrainMap(0xBF));
  EXPECT_EQ(editor->DisplayedMapCount(), 192);
  editor->SelectMapForEditing(0xBF, false);
  EXPECT_TRUE(editor->map_bitmap(0xBF).is_active());
  editor->SetPreviewStoryState(1);
  EXPECT_EQ(editor->overworld().overworld_map(0xBF)->game_state(), 1);
  editor->SetPreviewStoryState(2);
  EXPECT_EQ(editor->overworld().overworld_map(0xBF)->game_state(), 2);
  EXPECT_FALSE(editor->CreateLyraTerrain().ok());
  EXPECT_FALSE(editor->OpenLyraTerrain(sidecar).ok());
  EXPECT_FALSE(editor->lyra_canvas_.loaded());
  EXPECT_EQ(ReadBytes(sidecar), sidecar_before);
  EXPECT_EQ(project.lyra_terrain_file, sidecar);
  EXPECT_EQ(document.vector(), fixture.bytes);
}

TEST_F(NativeSpecialOverworldEditorTest, ScreenAppearanceSaveUndoAndReopen) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native appearance Save/reopen";
  const auto flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
  for (int capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    Rom legacy;
    ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
    zelda3::Overworld reference(&legacy);
    ASSERT_TRUE(reference.Load(&legacy).ok());
    NativeDirectoryFixture fixture(capacity);
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    fixture.UpgradeToRevision4();
    const int page = capacity == 192 ? 0xB1 : 0x9F;
    if (capacity == 192) {
      for (int member : {0xB0, 0xB1, 0xB8, 0xB9}) {
        fixture.bytes[fixture.Entry(7).pc_address + member] = 0xB0;
        fixture.bytes[fixture.Entry(30).pc_address + member] = 1;
        fixture.bytes[fixture.Entry(410).pc_address + member] = 1;
        fixture.bytes[fixture.Entry(2).pc_address + member] = member & 1;
        fixture.bytes[fixture.Entry(27).pc_address + member] = member & 1;
        fixture.bytes[fixture.Entry(31).pc_address + member] = member & 1;
      }
    }
    fixture.bytes[fixture.Entry(410).pc_address + page] = 1;
    fixture.bytes[fixture.Entry(2).pc_address + page] = 1;
    fixture.bytes[fixture.Entry(27).pc_address + page] = 1;
    fixture.bytes[fixture.Entry(31).pc_address + page] = 1;
    fixture.Word(fixture.Entry(411).pc_address + (page - 0x80) * 256 + 10,
                 0x1234);
    fixture.Seal();
    const auto root = test::UniqueTempPath("ap");
    std::filesystem::create_directories(root);
    auto cleanup =
        absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
    const auto source = root / "in.sfc";
    WriteBytes(source, fixture.bytes);
    auto manager = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager, renderer_.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
    ASSERT_TRUE(
        manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
    auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
    auto& world = editor->overworld();
    auto* document = manager->GetCurrentRom();
    auto* screen = world.mutable_overworld_map(page);
    ASSERT_EQ(screen->native_appearance_bg_palette()[5], 0x1234);
    const auto before = world.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(before.ok()) << before.status();
    using Field = editor::OverworldPropertyField;
    auto apply = [&](Field field, int index, int value) {
      return editor::NativePropertySaveTestPeer::Apply(
          *editor, {page, field, index, value});
    };
    {
      ScopedRomReadOnly readonly(*document);
      EXPECT_FALSE(apply(Field::kScreenColor, 5, 0x3210).ok());
    }
    ASSERT_TRUE(apply(Field::kMainPalette, 0, 2).ok());
    EXPECT_EQ(screen->main_palette(), 2);
    ASSERT_TRUE(apply(Field::kAreaPalette, 0, 2).ok());
    ASSERT_TRUE(apply(Field::kAreaGraphics, 0, 2).ok());
    EXPECT_EQ(screen->area_palette(), 2);
    EXPECT_EQ(screen->area_graphics(), 2);
    if (capacity == 192) {
      EXPECT_EQ(world.overworld_map(0xB0)->main_palette(), 0);
      EXPECT_EQ(world.overworld_map(0xB8)->main_palette(), 0);
      EXPECT_EQ(world.overworld_map(0xB9)->main_palette(), 1);
      for (int other : {0xB0, 0xB8, 0xB9}) {
        EXPECT_EQ(world.overworld_map(other)->area_palette(), other & 1);
        EXPECT_EQ(world.overworld_map(other)->area_graphics(), other & 1);
      }
    }
    ASSERT_TRUE(apply(Field::kScreenColor, 5, 0x3210).ok());
    const auto changed = world.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(changed.ok()) << changed.status();
    EXPECT_EQ(changed->pages[page].appearance_bg_palette[5], 0x3210);
    ASSERT_TRUE(editor->Undo().ok());
    EXPECT_EQ(screen->native_appearance_bg_palette()[5], 0x1234);
    ASSERT_TRUE(editor->Redo().ok());
    EXPECT_EQ(screen->native_appearance_bg_palette()[5], 0x3210);
    ASSERT_TRUE(screen->LoadPalette().ok());
    const auto source_palette = screen->current_palette();
    ASSERT_TRUE(apply(Field::kScreenAppearance, 0, 3).ok());
    ASSERT_TRUE(screen->LoadPalette().ok());
    EXPECT_EQ(screen->current_palette()[5].snes(), 0x3210);
    for (int color = 128; color < 256; ++color)
      EXPECT_EQ(screen->current_palette()[color].snes(),
                source_palette[color].snes());
    ASSERT_TRUE(apply(Field::kScreenAppearance, 0, 1).ok());
    EXPECT_EQ(screen->native_appearance_bg_palette()[5], 0x3210);
    ASSERT_TRUE(screen->LoadPalette().ok());
    EXPECT_EQ(screen->current_palette()[5].snes(), source_palette[5].snes());
    ASSERT_TRUE(editor->Undo().ok());
    EXPECT_EQ(screen->native_appearance_flags(), 3);
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = false;
    EXPECT_FALSE(manager->SaveCurrentSession(false).ok());
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
    {
      test::ScopedRomStagingFailure fault;
      EXPECT_FALSE(manager->SaveCurrentSession(false).ok());
    }
    EXPECT_EQ(document->vector(), fixture.bytes);
    EXPECT_EQ(screen->native_appearance_bg_palette()[5], 0x3210);
    const auto saved = manager->SaveCurrentSession(false);
    ASSERT_TRUE(saved.ok()) << saved;
    const auto persisted = ReadBytes(source);
    ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
    EXPECT_EQ(ReadBytes(source), persisted);
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(source.string()).ok());
    zelda3::Overworld fresh(&reopened);
    const auto loaded = fresh.Load(&reopened);
    ASSERT_TRUE(loaded.ok()) << loaded;
    const auto restored = fresh.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(restored.ok()) << restored.status();
    EXPECT_EQ(restored->pages[page].appearance_flags, 3);
    EXPECT_EQ(restored->pages[page].appearance_bg_palette[5], 0x3210);
    EXPECT_EQ(restored->pages[page].main_palette, 2);
    EXPECT_EQ(restored->profiles[page].palette, 2);
    EXPECT_EQ(restored->profiles[page].graphics, 2);
    EXPECT_EQ(restored->geometry, before->geometry);
    for (int other = 0; other < capacity; ++other)
      if (other != page) {
        EXPECT_EQ(restored->pages[other], before->pages[other]);
        EXPECT_EQ(restored->profiles[other], before->profiles[other]);
      }
    ASSERT_TRUE(editor->Redo().ok());
    const auto cleared = manager->SaveCurrentSession(false);
    ASSERT_TRUE(cleared.ok()) << cleared;
    Rom clear_reopen;
    ASSERT_TRUE(clear_reopen.LoadFromFile(source.string()).ok());
    zelda3::Overworld clear_world(&clear_reopen);
    ASSERT_TRUE(clear_world.Load(&clear_reopen).ok());
    EXPECT_EQ(clear_world.overworld_map(page)->native_appearance_flags(), 1);
    EXPECT_EQ(
        clear_world.overworld_map(page)->native_appearance_bg_palette()[5],
        0x3210);
  }
}

TEST_F(NativeSpecialOverworldEditorTest,
       ScreenColorsSidebarWidgetsRevision4Headless) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native screen color UI harness";
  Rom legacy;
  ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
  zelda3::Overworld reference(&legacy);
  ASSERT_TRUE(reference.Load(&legacy).ok());
  NativeDirectoryFixture fixture(192);
  fixture.SeedLoadedLegacyForModelTest(legacy, reference);
  fixture.UpgradeToRevision4();
  const int page = 0xB1;
  for (int member : {0xB0, 0xB1, 0xB8, 0xB9}) {
    fixture.bytes[fixture.Entry(7).pc_address + member] = 0xB0;
    fixture.bytes[fixture.Entry(30).pc_address + member] = 1;
    fixture.bytes[fixture.Entry(410).pc_address + member] = 1;
    fixture.bytes[fixture.Entry(2).pc_address + member] = member & 1;
    fixture.bytes[fixture.Entry(27).pc_address + member] = member & 1;
    fixture.bytes[fixture.Entry(31).pc_address + member] = member & 1;
  }
  fixture.bytes[fixture.Entry(410).pc_address + page] = 3;
  fixture.bytes[fixture.Entry(2).pc_address + page] = 1;
  fixture.bytes[fixture.Entry(27).pc_address + page] = 1;
  fixture.bytes[fixture.Entry(31).pc_address + page] = 1;
  fixture.Word(fixture.Entry(411).pc_address + (page - 0x80) * 256 + 10,
               0x1234);
  fixture.Seal();
  const auto root = test::UniqueTempPath("ow_sc");
  std::filesystem::create_directories(root);
  auto cleanup = absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
  const auto source = root / "in.sfc";
  WriteBytes(source, fixture.bytes);
  auto manager = std::make_unique<editor::EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer_.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;
  ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
  ASSERT_TRUE(
      manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
  auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
  auto* document = manager->GetCurrentRom();
  auto& world = editor->overworld();
  project::YazeProject project;
  project.name = "Native screen colors";
  project.lyra_terrain_file = "legacy-import-only.lyra-terrain.json";
  editor->SetProjectContextProvider([&] { return &project; });
  editor->SelectMapForEditing(page, true);
  auto* main_screen = world.mutable_overworld_map(page);
  ASSERT_TRUE(main_screen->LoadPalette().ok());
  const uint16_t obj_sample_before = main_screen->current_palette()[200].snes();
  const uint16_t neighbor_color5 =
      world.overworld_map(0xB0)->native_appearance_bg_palette()[5];

  auto frame = [&] {
    ImGui::EndFrame();
    ImGui::GetIO().DisplaySize = ImVec2(1920, 2400);
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1400, 2200), ImGuiCond_Always);
    ImGui::Begin("OwNativeScreenUi", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    editor->DrawMapProperties();
    ImGui::End();
  };

  auto click_widget = [&](const std::string& path) {
    auto info = gui::WidgetIdRegistry::Instance().GetWidgetInfo(path);
    ASSERT_TRUE(info.has_value()) << path;
    ASSERT_TRUE(info->bounds.valid) << path;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((info->bounds.min_x + info->bounds.max_x) * 0.5f,
                        (info->bounds.min_y + info->bounds.max_y) * 0.5f);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame();
  };

  for (int i = 0; i < 3; ++i) {
    frame();
  }

  click_widget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/swatch7F", page));
  ASSERT_FALSE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
  auto key = [&](ImGuiKey code) {
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(code, true);
    frame();
    io.AddKeyEvent(code, false);
    frame();
  };
  auto enter_color = [&](const char* text, bool commit) {
    click_widget(absl::StrFormat(
        "OverworldSidebar/native_screen/m%02X/color_value", page));
    auto& io = ImGui::GetIO();
    const auto modifier =
        io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
    io.AddKeyEvent(modifier, true);
    key(ImGuiKey_A);
    io.AddKeyEvent(modifier, false);
    frame();
    io.AddInputCharactersUTF8(text);
    frame();
    if (commit)
      key(ImGuiKey_Enter);
  };
  enter_color("3210", true);
  EXPECT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
  const uint16_t after_edit = main_screen->native_appearance_bg_palette()[127];
  EXPECT_EQ(after_edit, 0x3210);
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(main_screen->native_appearance_bg_palette()[127], 0x0000);
  ASSERT_TRUE(editor->Redo().ok());
  EXPECT_EQ(main_screen->native_appearance_bg_palette()[127], 0x3210);
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  const auto persisted = ReadBytes(source);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(source.string()).ok());
  zelda3::Overworld saved_world(&reopened);
  ASSERT_TRUE(saved_world.Load(&reopened).ok());
  EXPECT_EQ(
      saved_world.overworld_map(page)->native_appearance_bg_palette()[127],
      0x3210);
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source), persisted);

  click_widget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/swatch05", page));
  ASSERT_FALSE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
  {
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Escape, false);
    frame();
  }
  EXPECT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
  EXPECT_EQ(main_screen->native_appearance_bg_palette()[5], 0x1234);

  using Field = editor::OverworldPropertyField;
  {
    ScopedRomReadOnly read_only(*document);
    EXPECT_FALSE(editor::NativePropertySaveTestPeer::Apply(
                     *editor, {page, Field::kScreenColor, 10, 0x3210})
                     .ok());
  }

  click_widget(absl::StrFormat(
      "OverworldSidebar/native_screen/m%02X/reset_source", page));
  EXPECT_EQ(main_screen->native_appearance_flags() & 3, 1);
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(main_screen->native_appearance_flags(), 3);

  // A draft cannot follow a changed physical selection or a ROM reload.
  click_widget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/swatch05", page));
  enter_color("1111", false);
  editor->SelectMapForEditing(0xB0, true);
  frame();
  key(ImGuiKey_Enter);
  EXPECT_EQ(main_screen->native_appearance_bg_palette()[5], 0x1234);
  EXPECT_EQ(world.overworld_map(0xB0)->native_appearance_bg_palette()[5],
            neighbor_color5);
  editor->SelectMapForEditing(page, true);
  frame();
  click_widget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/swatch05", page));
  enter_color("1111", false);
  // Both changed words cancelled the former XOR generation fingerprint.
  auto external = main_screen->native_appearance_bg_palette();
  external[0] ^= 0x0100;
  external[1] ^= 0x0001;
  main_screen->set_native_appearance_bg_palette(external);
  frame();
  key(ImGuiKey_Enter);
  EXPECT_EQ(main_screen->native_appearance_bg_palette()[5], 0x1234);
  click_widget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/swatch05", page));
  enter_color("1111", false);
  ASSERT_TRUE(editor->Load().ok());
  frame();
  key(ImGuiKey_Enter);
  EXPECT_EQ(world.overworld_map(page)->native_appearance_bg_palette()[5],
            0x1234);
  main_screen = world.mutable_overworld_map(page);
  {
    ScopedRomReadOnly read_only(*document);
    frame();
    click_widget(
        absl::StrFormat("OverworldSidebar/native_screen/m%02X/swatch05", page));
    EXPECT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
    EXPECT_EQ(main_screen->native_appearance_bg_palette()[5], 0x1234);
  }
  frame();

  // Enabling screen appearance on an inherited child must keep its parent exact.
  ASSERT_TRUE(editor::NativePropertySaveTestPeer::Apply(
                  *editor, {page, Field::kScreenAppearance, 0, 0})
                  .ok());
  frame();
  click_widget(
      absl::StrFormat("OverworldSidebar/native_screen/m%02X/mode", page));
  // ImGui sizes and positions a newly opened combo over its first frames.
  frame();
  frame();
  click_widget(absl::StrFormat(
      "OverworldSidebar/native_screen/m%02X/mode_option1", page));
  EXPECT_EQ(main_screen->native_appearance_flags(), 1);
  EXPECT_EQ(world.overworld_map(0xB0)->native_appearance_flags(), 1);
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(main_screen->native_appearance_flags(), 0);
  ASSERT_TRUE(editor->Redo().ok());
  EXPECT_EQ(main_screen->native_appearance_flags(), 1);

  ASSERT_TRUE(main_screen->LoadPalette().ok());
  EXPECT_EQ(main_screen->current_palette()[200].snes(), obj_sample_before);
  EXPECT_EQ(world.overworld_map(0xB0)->native_appearance_bg_palette()[5],
            neighbor_color5);
}

TEST_F(NativeSpecialOverworldEditorTest, NormalSaveUndoFailureAndDiskReopen) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native Save/reopen";
  for (int capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    const auto flags = core::FeatureFlags::get();
    auto restore_flags =
        absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
    Rom legacy;
    ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
    zelda3::Overworld reference(&legacy);
    ASSERT_TRUE(reference.Load(&legacy).ok());
    NativeDirectoryFixture fixture(capacity);
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    const auto root = test::UniqueTempPath("ow3");
    std::filesystem::create_directories(root);
    auto cleanup =
        absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
    const auto source = root / "in.sfc";
    const auto target = root / "out.sfc";
    WriteBytes(source, fixture.bytes);
    auto manager = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager, renderer_.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    const auto opened = manager->OpenRomOrProject(source.string());
    ASSERT_TRUE(opened.ok()) << opened;
    const auto assets =
        manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld);
    ASSERT_TRUE(assets.ok()) << assets;
    auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
    ASSERT_NE(editor, nullptr);
    ASSERT_TRUE(editor->overworld().is_loaded());
    auto* document = manager->GetCurrentRom();
    auto& world = editor->overworld();
    editor->SetPreviewStoryState(1);
    editor->SetPreviewStoryState(2);
    const auto noop = manager->SaveRom();
    ASSERT_TRUE(noop.ok()) << noop;
    EXPECT_EQ(ReadBytes(source), fixture.bytes);
    const auto initial_definitions = world.tiles32_unique();
    auto cell = [&](int map) -> uint16_t& {
      return world.GetMapTiles(
          map / 64)[(map % 8) * 32 + 5][((map % 64) / 8) * 32 + 5];
    };
    const std::vector<int> edited_maps =
        capacity == 192 ? std::vector<int>{0x00, 0x9F, 0xA0, 0xBF}
                        : std::vector<int>{0x00, 0x80, 0x9F};
    for (int map : edited_maps) {
      const uint16_t old = cell(map);
      const uint16_t next = old == 1 ? 2 : 1;
      const auto painted = editor->ApplyTile16Stencil(map, {{5, 5, old, next}});
      ASSERT_TRUE(painted.ok()) << std::hex << map << ": " << painted.status();
      ASSERT_EQ(*painted, 1);
      EXPECT_EQ(cell(map), next);
      ASSERT_TRUE(editor->Undo().ok());
      EXPECT_EQ(cell(map), old);
      ASSERT_TRUE(editor->Redo().ok());
      EXPECT_EQ(cell(map), next);
    }
    auto& definition = world.mutable_tiles16()->at(1);
    definition.tile0_.horizontal_mirror_ =
        !definition.tile0_.horizontal_mirror_;
    const int property_map = world.overworld_map(capacity - 1)->parent();
    const int message = world.overworld_map(property_map)->message_id() ^ 1;
    ASSERT_TRUE(
        editor::NativePropertySaveTestPeer::Apply(
            *editor, {capacity - 1, editor::OverworldPropertyField::kMessageId,
                      0, message})
            .ok());
    editor->SetPreviewStoryState(1);
    if (capacity == 192) {
      const int actor_map = world.overworld_map(0xBF)->parent();
      const auto actor = editor::InsertSprite(
          &world,
          ImVec2((actor_map % 8) * 512 + 16, ((actor_map % 64) / 8) * 512 + 16),
          actor_map, 1, 0x52);
      ASSERT_TRUE(actor.ok()) << actor.status();
    } else {
      auto& sprites = *world.mutable_sprites(1);
      const auto actor =
          std::find_if(sprites.begin(), sprites.end(),
                       [](const auto& value) { return value.map_id() == 2; });
      ASSERT_NE(actor, sprites.end());
      auto draft = *actor;
      draft.set_id(draft.id() ^ 1);
      ASSERT_TRUE(editor->ApplySpriteEdit(&*actor, draft).ok());
    }
    const auto expected_tile16 = world.tiles16();
    const std::array<zelda3::OverworldBlockset, 3> expected = {
        world.GetMapTiles(0), world.GetMapTiles(1), world.GetMapTiles(2)};
    const auto pending = document->vector();
    const auto filename = document->filename();
    {
      test::ScopedRomStagingFailure fault;
      const auto failed = manager->SaveRomAs(target.string());
      EXPECT_FALSE(failed.ok());
      EXPECT_NE(failed.message().find("staging failure injected for testing"),
                absl::string_view::npos)
          << failed;
    }
    EXPECT_EQ(document->vector(), pending);
    EXPECT_EQ(document->filename(), filename);
    EXPECT_TRUE(document->dirty());
    EXPECT_FALSE(std::filesystem::exists(target));
    EXPECT_EQ(world.tiles32_unique(), initial_definitions);
    EXPECT_EQ(world.tiles16(), expected_tile16);
    for (int i = 0; i < 3; ++i)
      EXPECT_EQ(world.GetMapTiles(i), expected[i]);
    const auto saved = manager->SaveRomAs(target.string());
    ASSERT_TRUE(saved.ok()) << saved;
    EXPECT_FALSE(document->dirty());
    const auto persisted = ReadBytes(target);
    EXPECT_EQ(ReadBytes(source), fixture.bytes);
    EXPECT_NE(persisted, fixture.bytes);
    auto mutable_byte = [&](size_t pc) {
      if ((pc >= 0x1794D && pc < 0x1794D + 480) ||
          (pc >= 0x17B2D && pc < 0x17B2D + 480) ||
          (pc >= 0x14701C && pc < 0x147020))
        return true;
      for (size_t i = 0; i < fixture.rows.entries.size(); ++i) {
        const auto& e = fixture.rows.entries[i];
        if ((e.id >= 16 && e.id <= 21) || e.id == 100 ||
            (e.id >= 210 && e.id <= 213) || (e.id >= 400 && e.id <= 404)) {
          if (pc >= e.pc_address && pc < e.pc_address + e.count * e.stride)
            return true;
          if (pc >= 0x147020 + i * 16 + 12 && pc < 0x147020 + i * 16 + 16)
            return true;
        }
      }
      if ((pc >= 0x4C881 && pc < 0x4D62E) || (pc >= 0x141438 && pc < 0x1417F8))
        return true;
      if (property_map < 160 && pc >= 0x1417F8 + property_map * 2 &&
          pc < 0x1417F8 + property_map * 2 + 2)
        return true;
      return false;
    };
    for (size_t pc = 0; pc < persisted.size(); ++pc)
      if (persisted[pc] != fixture.bytes[pc])
        ASSERT_TRUE(mutable_byte(pc)) << std::hex << pc;
    const auto second = manager->SaveRom();
    ASSERT_TRUE(second.ok()) << second;
    EXPECT_EQ(ReadBytes(target), persisted);
    // Geometry cannot be masked by disabling its property writer. It must
    // stay pending and refuse publication until the coupled edit is valid.
    auto* map = world.mutable_overworld_map(capacity - 1);
    const auto old_size = map->area_size();
    map->SetAreaSize(old_size == zelda3::AreaSizeEnum::SmallArea
                         ? zelda3::AreaSizeEnum::LargeArea
                         : zelda3::AreaSizeEnum::SmallArea);
    EXPECT_FALSE(manager->SaveRom().ok());
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = false;
    const auto disabled = manager->SaveRom();
    EXPECT_FALSE(disabled.ok()) << disabled;
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
    EXPECT_FALSE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), persisted);
    manager.reset();
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
    ASSERT_EQ(reopened.InspectLyraLayout().presence,
              rom::LyraLayoutPresence::kValid);
    zelda3::Overworld fresh(&reopened);
    const auto loaded = fresh.Load(&reopened);
    ASSERT_TRUE(loaded.ok()) << loaded;
    EXPECT_EQ(fresh.materialized_map_count(), capacity);
    EXPECT_EQ(fresh.overworld_map(property_map)->message_id(), message);
    EXPECT_EQ(fresh.tiles16(), expected_tile16);
    for (int i = 0; i < 3; ++i)
      EXPECT_EQ(fresh.GetMapTiles(i), expected[i]);
    EXPECT_EQ(reopened.vector(), persisted);
  }
}

TEST_F(NativeSpecialOverworldEditorTest,
       AreaResizePreservesRawProfilesSpritesUndoAndDiskReopen) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native area editing";
  const auto flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
  core::FeatureFlags::get().overworld.kSaveOverworldMaps = false;
  core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
  core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
  Rom legacy;
  ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
  zelda3::Overworld reference(&legacy);
  ASSERT_TRUE(reference.Load(&legacy).ok());
  for (bool dormant_conflict : {false, true}) {
    SCOPED_TRACE(dormant_conflict);
    NativeDirectoryFixture fixture;
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    for (int parent : {0xB0, 0xB8}) {
      fixture.bytes[fixture.Entry(7).pc_address + parent] = parent;
      fixture.bytes[fixture.Entry(7).pc_address + parent + 1] = parent;
      fixture.bytes[fixture.Entry(30).pc_address + parent] = 2;
      fixture.bytes[fixture.Entry(30).pc_address + parent + 1] = 2;
    }
    fixture.bytes[fixture.Entry(27).pc_address + 0xB1] ^= 1;
    fixture.bytes[fixture.Entry(31).pc_address + 0xB1] = 1;
    fixture.bytes[fixture.Entry(32).pc_address + 0xB1] = 7;
    fixture.bytes[fixture.Entry(33).pc_address + 0xB1] = 8;
    fixture.Entry(100).count = 64;
    const size_t pool = fixture.Entry(100).pc_address;
    const std::vector<uint8_t> top = {14,   0x80 | 23, 0x52, 21,   15,
                                      0x52, 12,        40,   0x52, 0xFF};
    const std::vector<uint8_t> bottom = {0x40 | 11, 13,   0x52, 13,
                                         37,        0x52, 0xFF};
    std::copy(top.begin(), top.end(), fixture.bytes.begin() + pool + 1);
    std::copy(bottom.begin(), bottom.end(), fixture.bytes.begin() + pool + 11);
    fixture.Word(fixture.Entry(17).pc_address + 0xB0 * 2, 0x8001);
    fixture.Word(fixture.Entry(17).pc_address + 0xB8 * 2, 0x800B);
    if (dormant_conflict) {
      const std::array<uint8_t, 4> dormant = {1, 1, 0x53, 0xFF};
      std::copy(dormant.begin(), dormant.end(),
                fixture.bytes.begin() + pool + 18);
      fixture.Word(fixture.Entry(17).pc_address + 0xB1 * 2, 0x8012);
      // Same world pixels/code/flags, but owned by an unrelated area. Its
      // offscreen placement is not evidence that B1's dormant actor survived.
      const std::array<uint8_t, 4> unrelated = {33, 1, 0x53, 0xFF};
      std::copy(unrelated.begin(), unrelated.end(),
                fixture.bytes.begin() + pool + 22);
      fixture.Word(fixture.Entry(17).pc_address + 0xA9 * 2, 0x8016);
    }
    fixture.Seal();
    const auto root = test::UniqueTempPath("owarea");
    std::filesystem::create_directories(root);
    auto cleanup =
        absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
    const auto source = root / "in.sfc", target = root / "out.sfc";
    WriteBytes(source, fixture.bytes);
    auto manager = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager, renderer_.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
    ASSERT_TRUE(
        manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
    auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
    auto& world = editor->overworld();
    auto* document = manager->GetCurrentRom();
    const auto before = world.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(before.ok()) << before.status();
    using Field = editor::OverworldPropertyField;
    auto resize = [&](zelda3::AreaSizeEnum size) {
      return editor::NativePropertySaveTestPeer::Apply(
          *editor, {0xB0, Field::kAreaSize, 0, static_cast<int>(size)});
    };
    if (dormant_conflict) {
      const auto refused = resize(zelda3::AreaSizeEnum::SmallArea);
      EXPECT_FALSE(refused.ok());
      EXPECT_NE(refused.message().find("distinct dormant sprites"),
                absl::string_view::npos)
          << refused;
      const auto unchanged = world.CaptureNativeAreaSnapshot();
      ASSERT_TRUE(unchanged.ok());
      EXPECT_EQ(*unchanged, *before);
      EXPECT_EQ(document->vector(), fixture.bytes);
      EXPECT_FALSE(document->dirty());
      continue;
    }
    {
      ScopedRomReadOnly readonly(*document);
      EXPECT_FALSE(resize(zelda3::AreaSizeEnum::LargeArea).ok());
    }
    ASSERT_TRUE(resize(zelda3::AreaSizeEnum::LargeArea).ok());
    const auto large = world.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(large.ok()) << large.status();
    EXPECT_EQ(large->profiles, before->profiles);
    EXPECT_EQ(document->vector(), fixture.bytes);
    for (int member : {0xB0, 0xB1, 0xB8, 0xB9})
      EXPECT_EQ(world.overworld_map(member)->parent(), 0xB0);
    ASSERT_TRUE(editor->Undo().ok());
    auto undone = world.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(undone.ok());
    EXPECT_EQ(*undone, *before);
    EXPECT_EQ(world.overworld_map(0xB8)->area_size(),
              zelda3::AreaSizeEnum::WideArea);
    ASSERT_TRUE(editor->Redo().ok());
    auto redone = world.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(redone.ok());
    EXPECT_EQ(*redone, *large);
    core::FeatureFlags::get().overworld.kSaveOverworldSprites = false;
    EXPECT_FALSE(manager->SaveRomAs(target.string()).ok());
    EXPECT_EQ(document->vector(), fixture.bytes);
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = false;
    EXPECT_FALSE(manager->SaveRomAs(target.string()).ok());
    EXPECT_TRUE(document->dirty());
    EXPECT_EQ(document->vector(), fixture.bytes);
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
    core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
    {
      test::ScopedRomStagingFailure fault;
      EXPECT_FALSE(manager->SaveRomAs(target.string()).ok());
    }
    EXPECT_EQ(document->vector(), fixture.bytes);
    const auto saved = manager->SaveRomAs(target.string());
    ASSERT_TRUE(saved.ok()) << saved;
    const auto persisted = ReadBytes(target);
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
    zelda3::Overworld fresh(&reopened);
    ASSERT_TRUE(fresh.Load(&reopened).ok());
    const auto fresh_snapshot = fresh.CaptureNativeAreaSnapshot();
    ASSERT_TRUE(fresh_snapshot.ok()) << fresh_snapshot.status();
    EXPECT_EQ(fresh_snapshot->geometry, large->geometry);
    EXPECT_EQ(fresh_snapshot->profiles, before->profiles);
    const auto packed = zelda3::ReadOverworldSpriteList(
        reopened, reopened.InspectLyraLayout(), 1, 0xB0);
    ASSERT_TRUE(packed.ok()) << packed.status();
    EXPECT_EQ(*packed, (std::vector<uint8_t>{14, 0x80 | 23, 0x52, 21, 15, 0x52,
                                             12, 40, 0x52, 0x40 | 43, 13, 0x52,
                                             45, 37, 0x52, 0xFF}));
    const auto cold_split =
        fresh.ConfigureMultiAreaMap(0xB0, zelda3::AreaSizeEnum::SmallArea);
    ASSERT_TRUE(cold_split.ok()) << cold_split;
    EXPECT_EQ(fresh.overworld_map(0xB1)->area_music(0), 7);
    const auto second = manager->SaveRom();
    ASSERT_TRUE(second.ok()) << second;
    EXPECT_EQ(ReadBytes(target), persisted);
    ASSERT_TRUE(editor->Undo().ok());
    EXPECT_TRUE(document->dirty());
    EXPECT_EQ(nlohmann::json::parse(manager->ExecuteEditorControl(
                  R"({"command":"status"})"))["state"]["unsaved"],
              true);
    ASSERT_TRUE(manager->SaveRom().ok());
    ASSERT_TRUE(editor->Redo().ok());
    EXPECT_TRUE(document->dirty());
    ASSERT_TRUE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), persisted);
    // A split restores the raw child selector/music values rather than copying
    // the former parent's effective settings into the new gameplay area.
    const auto split = resize(zelda3::AreaSizeEnum::SmallArea);
    ASSERT_TRUE(split.ok()) << split;
    EXPECT_EQ(world.overworld_map(0xB1)->area_graphics(),
              before->profiles[0xB1].graphics);
    EXPECT_EQ(world.overworld_map(0xB1)->area_music(0), 7);
    EXPECT_EQ(world.overworld_map(0xB1)->area_music(1), 8);
    ASSERT_TRUE(editor->Undo().ok());
    ASSERT_TRUE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), persisted);
  }
}

TEST_F(NativeSpecialOverworldEditorTest,
       PropertyOnlySaveFailureRetryUndoAndFreshReopen) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS for native property Save/reopen";
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    const auto flags = core::FeatureFlags::get();
    auto restore_flags =
        absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
    core::FeatureFlags::get().overworld.kSaveOverworldMaps = false;
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
    Rom legacy;
    ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
    zelda3::Overworld reference(&legacy);
    ASSERT_TRUE(reference.Load(&legacy).ok());
    NativeDirectoryFixture fixture(capacity);
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    const int last = capacity - 1;
    const int owner = fixture.bytes[fixture.Entry(7).pc_address + last];
    fixture.bytes[fixture.Entry(3).pc_address + owner] |= 0xA0;
    fixture.Seal();
    const auto root = test::UniqueTempPath("owp");
    std::filesystem::create_directories(root);
    auto cleanup =
        absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
    const auto source = root / "in.sfc", target = root / "out.sfc";
    WriteBytes(source, fixture.bytes);
    auto manager = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager, renderer_.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
    ASSERT_TRUE(
        manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
    auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
    auto& world = editor->overworld();
    auto* document = manager->GetCurrentRom();
    auto* map = world.mutable_overworld_map(owner);
    const uint16_t old_message = world.overworld_map(owner)->message_id();
    auto apply = [&](editor::OverworldPropertyField field, int value,
                     int index = 0) {
      return editor::NativePropertySaveTestPeer::Apply(
          *editor, {last, field, index, value});
    };
    using Field = editor::OverworldPropertyField;
    {
      ScopedRomReadOnly read_only(*document);
      EXPECT_FALSE(apply(Field::kMessageId, old_message ^ 1).ok());
      EXPECT_EQ(map->message_id(), old_message);
      EXPECT_EQ(document->vector(), fixture.bytes);
    }
    EXPECT_FALSE(apply(Field::kMosaic, 1).ok());
    ASSERT_TRUE(apply(Field::kMessageId, old_message ^ 1).ok());
    const uint8_t palette = map->main_palette() ^ 1;
    ASSERT_TRUE(apply(Field::kMainPalette, palette).ok());
    const uint16_t background = map->area_specific_bg_color() ^ 1;
    ASSERT_TRUE(apply(Field::kAreaSpecificBgColor, background).ok());
    const uint8_t graphic = map->custom_tileset(0) == 0x3A ? 0x3B : 0x3A;
    ASSERT_TRUE(apply(Field::kCustomTileset, graphic).ok());
    ASSERT_TRUE(apply(Field::kMosaicExpanded, !map->mosaic_expanded()[0]).ok());
    const int music = map->area_music(0) ^ 1;
    ASSERT_TRUE(apply(Field::kMusic, music).ok());
    if (capacity == 192)
      ASSERT_TRUE(apply(Field::kMusic, music ^ 2, 1).ok());
    EXPECT_EQ(document->vector(), fixture.bytes);
    const auto mosaic = map->mosaic_expanded();
    const auto projected = world.GetProjectedWriteRanges();
    const auto pending = document->vector();
    const auto filename = document->filename();
    {
      ScopedRomReadOnly read_only(*document);
      EXPECT_FALSE(manager->SaveRomAs(target.string()).ok());
    }
    {
      test::ScopedRomStagingFailure fault;
      const auto failed = manager->SaveRomAs(target.string());
      EXPECT_FALSE(failed.ok());
      EXPECT_NE(failed.message().find("staging failure injected for testing"),
                absl::string_view::npos)
          << failed;
    }
    EXPECT_EQ(document->vector(), pending);
    EXPECT_EQ(document->filename(), filename);
    EXPECT_TRUE(document->dirty());
    EXPECT_FALSE(std::filesystem::exists(target));
    EXPECT_EQ(map->main_palette(), palette);
    EXPECT_EQ(world.overworld_map(owner)->message_id(), old_message ^ 1);
    const auto saved = manager->SaveRomAs(target.string());
    ASSERT_TRUE(saved.ok()) << saved;
    auto persisted = ReadBytes(target);
    EXPECT_FALSE(document->dirty());
    EXPECT_NE(persisted, fixture.bytes);
    EXPECT_EQ(persisted[fixture.Entry(3).pc_address + owner] & 0xF0, 0xA0);
    for (size_t pc = 0; pc < persisted.size(); ++pc) {
      if (persisted[pc] != fixture.bytes[pc])
        ASSERT_TRUE(std::any_of(
            projected.begin(), projected.end(),
            [pc](const auto& r) { return pc >= r.first && pc < r.second; }))
            << std::hex << pc;
    }
    const auto& definitions = fixture.Entry(400);
    EXPECT_TRUE(std::equal(fixture.bytes.begin() + definitions.pc_address,
                           fixture.bytes.begin() + definitions.pc_address +
                               definitions.count * definitions.stride,
                           persisted.begin() + definitions.pc_address));
    const size_t message_at = owner < 160
                                  ? 0x1417F8 + owner * 2
                                  : fixture.Entry(19).pc_address + owner * 2;
    EXPECT_EQ(persisted[message_at] | (persisted[message_at + 1] << 8),
              old_message ^ 1);
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = false;
    ASSERT_TRUE(apply(Field::kMessageId, old_message ^ 2).ok());
    ASSERT_TRUE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), persisted);
    core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
    ASSERT_TRUE(manager->SaveRom().ok());
    persisted = ReadBytes(target);
    EXPECT_EQ(persisted[message_at] | (persisted[message_at + 1] << 8),
              old_message ^ 2);
    ASSERT_TRUE(editor->Undo().ok());
    ASSERT_TRUE(manager->SaveRom().ok());
    persisted = ReadBytes(target);
    EXPECT_EQ(persisted[message_at] | (persisted[message_at + 1] << 8),
              old_message ^ 1);
    ASSERT_TRUE(editor->Redo().ok());
    ASSERT_TRUE(manager->SaveRom().ok());
    persisted = ReadBytes(target);
    ASSERT_TRUE(manager->SaveRom().ok());
    EXPECT_EQ(ReadBytes(target), persisted);
    EXPECT_EQ(ReadBytes(source), fixture.bytes);
    manager.reset();
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
    zelda3::Overworld fresh(&reopened);
    const auto loaded = fresh.Load(&reopened);
    ASSERT_TRUE(loaded.ok()) << loaded;
    EXPECT_EQ(fresh.overworld_map(owner)->message_id(), old_message ^ 2);
    EXPECT_EQ(fresh.overworld_map(owner)->main_palette(), palette);
    EXPECT_EQ(fresh.overworld_map(owner)->area_specific_bg_color(), background);
    EXPECT_EQ(fresh.overworld_map(owner)->custom_tileset(0), graphic);
    EXPECT_EQ(fresh.overworld_map(owner)->mosaic_expanded(), mosaic);
    EXPECT_EQ(fresh.overworld_map(owner)->area_music(0), music);
    if (capacity == 192)
      EXPECT_EQ(fresh.overworld_map(owner)->area_music(1), music ^ 2);
    EXPECT_EQ(reopened.vector(), persisted);
  }
}

class LyraA0EditorTest : public test::HeadlessEditorTest {};

TEST_F(LyraA0EditorTest, CanvasBrushTargetsA0AndRefusesA1) {
  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromData(ReadBytes("LYRA_WRITABLE_A0.bin")).ok());
  editor::EditorDependencies deps;
  deps.rom = &loaded;
  deps.renderer = renderer_.get();
  deps.window_manager = window_manager_.get();
  auto editor = std::make_unique<editor::OverworldEditor>(&loaded, deps);
  editor->SetGameData(nullptr);
  editor->Initialize();
  ASSERT_TRUE(editor->Load().ok());
  editor->ow_map_canvas().set_global_scale(1.0f);
  auto frame = [&] {
    ImGui::EndFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1200, 900), ImGuiCond_Always);
    ImGui::Begin("Lyra pilot canvas", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    gui::WidgetIdRegistry::Instance().Clear();
    editor->DrawOverworldCanvas();
    ImGui::End();
  };
  auto click = [&](ImVec2 point) {
    ImGui::GetIO().AddMousePosEvent(point.x, point.y);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    frame();
  };
  frame();
  frame();
  editor->JumpToMap(0xA0);
  frame();
  frame();
  const auto radio = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
      "OverworldLyra/radio:BlockingBorder");
  ASSERT_TRUE(radio.has_value());
  ASSERT_TRUE(radio->bounds.valid);
  click(ImVec2((radio->bounds.min_x + radio->bounds.max_x) / 2,
               (radio->bounds.min_y + radio->bounds.max_y) / 2));
  const auto initial = editor->overworld().lyra_a0_tiles();
  const auto canvas_point = [&](int map, int x, int y) {
    const auto& canvas = editor->ow_map_canvas();
    const auto origin = canvas.zero_point();
    const auto scroll = canvas.scrolling();
    const float scale = canvas.global_scale();
    return ImVec2(
        origin.x + scroll.x + ((map & 7) * 512 + x * 32 + 16) * scale,
        origin.y + scroll.y + (((map & 63) / 8) * 512 + y * 32 + 16) * scale);
  };
  click(canvas_point(0xA0, 5, 5));
  auto expected = initial;
  expected[5 * 16 + 5] = 0x20B2;
  ASSERT_EQ(editor->overworld().lyra_a0_tiles(), expected);
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(editor->overworld().lyra_a0_tiles(), initial);
  ASSERT_TRUE(editor->Redo().ok());
  EXPECT_EQ(editor->overworld().lyra_a0_tiles(), expected);
  const auto world_before_a1 = editor->overworld().GetMapTiles(2);
  click(canvas_point(0xA1, 5, 5));
  EXPECT_EQ(editor->overworld().GetMapTiles(2), world_before_a1);
  EXPECT_EQ(editor->overworld().lyra_a0_tiles(), expected);
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(editor->overworld().lyra_a0_tiles(), initial);
  EXPECT_FALSE(editor->Undo().ok());  // A1 click did not add history.
}

TEST_F(LyraA0EditorTest, EditorMutationUndoSaveAsAndFreshReopenPreserveRom) {
  const auto original = ReadBytes("LYRA_WRITABLE_A0.bin");
  const auto source = test::UniqueTempPath("lyra_a0_editor_source", ".sfc");
  const auto target = test::UniqueTempPath("lyra_a0_editor_saved", ".sfc");
  WriteBytes(source, original);
  const auto cleanup = absl::MakeCleanup([&] {
    std::error_code error;
    std::filesystem::remove(source, error);
    EXPECT_FALSE(error) << source.string() << ": " << error.message();
    error.clear();
    std::filesystem::remove(target, error);
    EXPECT_FALSE(error) << target.string() << ": " << error.message();
  });
  auto manager = std::make_unique<editor::EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer_.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
  auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
  ASSERT_NE(editor, nullptr);
  // Synthetic graphics intentionally stay blank; this tests real editor/model
  // mutations and coordinated Save As, not art or runtime acceptance.
  editor->SetGameData(nullptr);
  editor->Initialize();
  ASSERT_TRUE(editor->Load().ok());
  ASSERT_TRUE(editor->lyra_a0_terrain_writable());
  const auto initial_tiles = editor->overworld().lyra_a0_tiles();
  ASSERT_EQ(initial_tiles[5 * 16 + 5], 0x2103);
  EXPECT_EQ(editor->overworld().GetMapTiles(2)[10][138], 1);
  EXPECT_FALSE(editor->PaintLyraA0Terrain(0xA1, 5, 5, 0x20B2).ok());
  EXPECT_FALSE(editor->PaintLyraA0Terrain(0xC0, 5, 5, 0x20B2).ok());
  EXPECT_FALSE(editor->PaintLyraA0Terrain(0xA0, 7, 7, 0x20B2).ok());
  EXPECT_FALSE(editor->PaintLyraA0Terrain(0xA0, 5, 5, 0x7FFF).ok());
  EXPECT_TRUE(editor->GetUndoDescription().empty());
  ASSERT_TRUE(editor->PaintLyraA0Terrain(0xA0, 5, 5, 0x2103).ok());
  EXPECT_TRUE(editor->GetUndoDescription().empty());
  ASSERT_TRUE(editor->PaintLyraA0Terrain(0xA0, 5, 5, 0x20B2).ok());
  EXPECT_EQ(editor->overworld().GetMapTiles(2)[10][138], 2);
  EXPECT_TRUE(manager->GetCurrentRom()->dirty());
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(editor->overworld().lyra_a0_tiles(), initial_tiles);
  EXPECT_EQ(editor->overworld().GetMapTiles(2)[10][138], 1);
  ASSERT_TRUE(editor->Redo().ok());
  EXPECT_EQ(editor->overworld().GetMapTiles(2)[10][138], 2);
  const auto expected_tiles = editor->overworld().lyra_a0_tiles();
  EXPECT_FALSE(editor->Save().ok());
  EXPECT_FALSE(manager->SaveRom().ok());
  const auto saved = manager->SaveRomAs(target.string());
  ASSERT_TRUE(saved.ok()) << saved;
  EXPECT_EQ(ReadBytes(source), original);
  const auto persisted = ReadBytes(target);
  ASSERT_EQ(persisted.size(), original.size());
  EXPECT_NE(persisted, original);
  EXPECT_EQ(DecodePilotLiterals(persisted), expected_tiles);
  for (size_t pc = 0; pc < original.size(); ++pc) {
    if (!PilotWriteOwns(pc))
      ASSERT_EQ(persisted[pc], original[pc]) << "Unowned PC " << std::hex << pc;
  }
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  manager.reset();  // Destroy the entire saving editor session before reopen.
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
  auto fresh = std::make_unique<editor::OverworldEditor>(&reopened);
  fresh->Initialize();
  ASSERT_TRUE(fresh->Load().ok());
  EXPECT_EQ(fresh->overworld().lyra_a0_tiles(), expected_tiles);
  EXPECT_EQ(fresh->overworld().GetMapTiles(2)[10][138], 2);
  EXPECT_FALSE(fresh->Undo().ok());
  EXPECT_FALSE(reopened.CheckLayoutWritable().ok());
}

TEST_F(LyraA0EditorTest, OwnedPilotEditorSaveAsArtifact) {
  const char* input_path = std::getenv("YAZE_LYRA_PILOT_INPUT");
  const char* output_path = std::getenv("YAZE_LYRA_PILOT_OUTPUT");
  if (input_path == nullptr || output_path == nullptr) {
    GTEST_SKIP()
        << "Requires explicit owned offline pilot input and new output paths";
  }
  const std::filesystem::path source(input_path);
  const std::filesystem::path target(output_path);
  ASSERT_FALSE(std::filesystem::exists(target));
  ASSERT_FALSE(std::filesystem::exists(target.string() + ".json"));
  const auto original = ReadBytes(source);
  ASSERT_FALSE(original.empty());
  auto manager = std::make_unique<editor::EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer_.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
  auto* loaded_rom = manager->GetCurrentRom();
  ASSERT_FALSE(loaded_rom->has_copier_header());
  ASSERT_TRUE(loaded_rom->InspectLyraLayout().a0_terrain_write_supported);
  auto* game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  const auto assets = zelda3::LoadGameData(*loaded_rom, *game_data);
  ASSERT_TRUE(assets.ok()) << assets;
  auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
  ASSERT_NE(editor, nullptr);
  editor->SetGameData(game_data);
  editor->Initialize();
  ASSERT_TRUE(editor->Load().ok());
  editor->SelectMapForEditing(0xA0, false);
  ASSERT_TRUE(editor->map_bitmap(0xA0).is_active());
  const auto initial_tiles = editor->overworld().lyra_a0_tiles();
  ASSERT_EQ(initial_tiles[5 * 16 + 5], 0x2103);
  std::vector<uint8_t> before_pixels(
      editor->map_bitmap(0xA0).data(),
      editor->map_bitmap(0xA0).data() + 512 * 512);
  ASSERT_TRUE(editor->PaintLyraA0Terrain(0xA0, 5, 5, 0x20B2).ok());
  std::vector<uint8_t> authored_pixels(
      editor->map_bitmap(0xA0).data(),
      editor->map_bitmap(0xA0).data() + 512 * 512);
  EXPECT_NE(authored_pixels, before_pixels);
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(editor->overworld().lyra_a0_tiles(), initial_tiles);
  EXPECT_EQ(std::memcmp(editor->map_bitmap(0xA0).data(), before_pixels.data(),
                        before_pixels.size()),
            0);
  ASSERT_TRUE(editor->Redo().ok());
  const auto expected_tiles = editor->overworld().lyra_a0_tiles();
  EXPECT_EQ(expected_tiles[5 * 16 + 5], 0x20B2);
  EXPECT_EQ(std::memcmp(editor->map_bitmap(0xA0).data(), authored_pixels.data(),
                        authored_pixels.size()),
            0);
  const auto save = manager->SaveRomAs(target.string());
  ASSERT_TRUE(save.ok()) << save;
  EXPECT_EQ(ReadBytes(source), original);
  const auto persisted = ReadBytes(target);
  ASSERT_EQ(persisted.size(), original.size());
  EXPECT_EQ(DecodePilotLiterals(persisted), expected_tiles);
  size_t changed_bytes = 0;
  for (size_t pc = 0; pc < original.size(); ++pc) {
    if (!PilotWriteOwns(pc))
      ASSERT_EQ(persisted[pc], original[pc]) << "Unowned PC " << std::hex << pc;
    changed_bytes += original[pc] != persisted[pc];
  }
  ASSERT_GT(changed_bytes, 0u);
  manager.reset();
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(target.string()).ok());
  auto fresh_data = std::make_unique<zelda3::GameData>(&reopened);
  ASSERT_TRUE(zelda3::LoadGameData(reopened, *fresh_data).ok());
  auto fresh = std::make_unique<editor::OverworldEditor>(&reopened);
  fresh->SetGameData(fresh_data.get());
  fresh->Initialize();
  ASSERT_TRUE(fresh->Load().ok());
  fresh->SelectMapForEditing(0xA0, false);
  EXPECT_EQ(fresh->overworld().lyra_a0_tiles(), expected_tiles);
  EXPECT_EQ(std::memcmp(fresh->map_bitmap(0xA0).data(), authored_pixels.data(),
                        authored_pixels.size()),
            0);
  EXPECT_FALSE(fresh->Undo().ok());
  const nlohmann::json receipt = {{"input_path", source.string()},
                                  {"output_path", target.string()},
                                  {"map", 0xA0},
                                  {"tile32_x", 5},
                                  {"tile32_y", 5},
                                  {"before_tile32", 0x2103},
                                  {"after_tile32", 0x20B2},
                                  {"changed_bytes", changed_bytes},
                                  {"unowned_bytes_preserved", true},
                                  {"runtime_verified", false}};
  std::ofstream out(target.string() + ".json");
  ASSERT_TRUE(out.good());
  out << receipt.dump(2) << '\n';
  ASSERT_TRUE(out.good());
}

TEST(LyraLayoutDescriptorTest, ShortAndUnrelatedImagesAreNotAttributed) {
  std::vector<uint8_t> short_rom(0x100000, 0x77);
  EXPECT_EQ(rom::InspectLyraLayout(short_rom.data(), short_rom.size()).presence,
            rom::LyraLayoutPresence::kAbsent);
  std::vector<uint8_t> other_hack(0x200000, 0x55);
  EXPECT_EQ(
      rom::InspectLyraLayout(other_hack.data(), other_hack.size()).presence,
      rom::LyraLayoutPresence::kAbsent);
}

TEST(LyraLayoutDescriptorTest, PortableVectorsPreserveProductCompatibility) {
  const auto dir = FixtureDir();
  nlohmann::json cases;
  if (dir.empty()) {
    cases = nlohmann::json::parse(test::lyra::kCases)["cases"];
  } else {
    std::ifstream in(dir / "editor_cases.json");
    ASSERT_TRUE(in.good());
    cases = nlohmann::json::parse(in)["cases"];
  }
  ASSERT_EQ(cases.size(), 16u);
  for (const auto& item : cases) {
    const std::string name = item["name"];
    SCOPED_TRACE(name);
    auto bytes = ReadBytes(dir / item["base"].get<std::string>());
    ASSERT_FALSE(bytes.empty());
    for (const auto& patch : item["patches"]) {
      const size_t pc = patch["pc"];
      const std::string hex = patch["hex"];
      ASSERT_EQ(hex.size() % 2, 0u);
      ASSERT_LE(pc + hex.size() / 2, bytes.size());
      for (size_t i = 0; i < hex.size(); i += 2) {
        bytes[pc + i / 2] =
            static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16));
      }
    }
    if (!item["truncate_to"].is_null()) {
      bytes.resize(item["truncate_to"].get<size_t>());
    }
    const auto result = rom::InspectLyraLayout(bytes.data(), bytes.size());
    const std::string expected = item["expected"];
    if (expected == "supported_offline_inspection_only") {
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kValid);
      EXPECT_EQ(result.revision, 1);
      EXPECT_EQ(result.capacity, 192);
      EXPECT_EQ(result.active_count, 160);
      EXPECT_FALSE(result.write_supported);
      EXPECT_EQ(result.entries.size(), 35u);
    } else if (expected == "legacy_160" ||
               name == "old_tail_marker_without_descriptor") {
      // The descriptor parser leaves the older EA family to Yaze. A marker
      // alone identifies that family but cannot validate its runtime tables.
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kAbsent);
      if (name == "old_tail_marker_without_descriptor") {
        Rom older;
        ASSERT_TRUE(older.LoadFromData(bytes).ok());
        const auto profile = zelda3::DetectOverworldRomProfile(older);
        EXPECT_EQ(profile.layout_kind,
                  zelda3::OverworldRomProfile::LayoutKind::kOlderTailEa);
        EXPECT_EQ(profile.editable_map_count, 160);
        EXPECT_FALSE(profile.has_tail_map_expansion);
        const auto layout = zelda3::InspectOverworldLayout(
            older, zelda3::OverworldLayoutOptions::Defaults());
        EXPECT_FALSE(
            layout
                .CheckOperation(zelda3::OverworldLayoutDomain::kTerrainPointers,
                                zelda3::OverworldLayoutOperation::kSerialize,
                                -1, 0)
                .ok());
      }
    } else {
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kInvalid)
          << result.error;
      EXPECT_FALSE(result.write_supported);
      Rom malformed;
      ASSERT_TRUE(malformed.LoadFromData(bytes).ok());
      EXPECT_FALSE(malformed.CheckLayoutWritable().ok());
      const auto profile = zelda3::DetectOverworldRomProfile(malformed);
      EXPECT_EQ(profile.layout_kind,
                zelda3::OverworldRomProfile::LayoutKind::kLyraOffline);
      zelda3::Overworld overworld(&malformed);
      EXPECT_FALSE(overworld.Load(&malformed).ok());
    }
  }
}

TEST(LyraLayoutDescriptorTest, HeaderedImageAndTwoSessionsKeepSavePolicy) {
  const auto dir = FixtureDir();
  const auto offline = ReadBytes(dir / "OFFLINE_ONLY.bin");
  const auto legacy = ReadBytes(dir / "legacy_160_input.bin");
  ASSERT_FALSE(offline.empty());
  ASSERT_FALSE(legacy.empty());
  Rom lyra;
  std::vector<uint8_t> headered(0x200, 0);
  headered.insert(headered.end(), offline.begin(), offline.end());
  ASSERT_TRUE(lyra.LoadFromData(headered).ok());
  EXPECT_TRUE(lyra.has_copier_header());
  ASSERT_EQ(lyra.InspectLyraLayout().presence, rom::LyraLayoutPresence::kValid);
  Rom copied = lyra;
  EXPECT_FALSE(copied.CheckLayoutWritable().ok());
  copied.Close();
  EXPECT_TRUE(copied.CheckLayoutWritable().ok());
  zelda3::Overworld overworld(&lyra);
  EXPECT_FALSE(overworld.Load(&lyra).ok());
  EXPECT_TRUE(overworld.LoadLyraTerrainPreview(&lyra).ok());
  EXPECT_TRUE(overworld.is_lyra_preview());
  EXPECT_FALSE(overworld.descriptor_sprite_display_available());
  EXPECT_FALSE(overworld.Save(&lyra).ok());
  Rom ordinary;
  ASSERT_TRUE(ordinary.LoadFromData(legacy).ok());
  EXPECT_EQ(ordinary.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kAbsent);
  EXPECT_TRUE(ordinary.CheckLayoutWritable().ok());

  const auto before = lyra.vector();
  const bool dirty = lyra.dirty();
  const auto destination = test::UniqueTempPath("lyra_save_refusal");
  const auto refused = lyra.SaveToFile({.filename = destination.string()});
  EXPECT_FALSE(refused.ok());
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(lyra.vector(), before);
  EXPECT_EQ(lyra.dirty(), dirty);
  EXPECT_FALSE(lyra.WriteByte(0x100, 0x42).ok());
  EXPECT_EQ(lyra.vector(), before);

  // A damaged signature must not downgrade this loaded session to legacy.
  lyra.mutable_vector()[0x147000] = 0;
  EXPECT_EQ(lyra.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kInvalid);
  EXPECT_FALSE(lyra.CheckLayoutWritable().ok());
  std::fill_n(lyra.mutable_vector().begin() + 0x147000, 1024, 0);
  EXPECT_EQ(lyra.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kInvalid);
  EXPECT_FALSE(lyra.CheckLayoutWritable().ok());
  EXPECT_TRUE(ordinary.CheckLayoutWritable().ok());
}

TEST(LyraLayoutDescriptorTest, SyntheticTerrainVectorsNeedNoGameGraphics) {
  const auto bytes = ReadBytes("LYRA_PREVIEW_A0_A1.bin");
  ASSERT_FALSE(bytes.empty());
  const auto layout = rom::InspectLyraLayout(bytes.data(), bytes.size());
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
  EXPECT_EQ(layout.capacity, 192);
  EXPECT_EQ(layout.active_count, 160);
  EXPECT_FALSE(layout.write_supported);
  const auto a0 = rom::LyraMapTile32(layout, bytes.data(), bytes.size(), 0xA0);
  const auto a1 = rom::LyraMapTile32(layout, bytes.data(), bytes.size(), 0xA1);
  ASSERT_TRUE(a0.has_value());
  ASSERT_TRUE(a1.has_value());
  EXPECT_NE(*a0, *a1);
  EXPECT_EQ((*a0)[7 * 16 + 15], 0x2103);
  EXPECT_EQ((*a1)[7 * 16], 0x2103);
  EXPECT_EQ((*a1)[7 * 16 + 8], 0x20B2);
  EXPECT_FALSE(rom::LyraMapTile32(layout, bytes.data(), bytes.size(), 0xC0));
  Rom lyra;
  ASSERT_TRUE(lyra.LoadFromData(bytes).ok());
  EXPECT_FALSE(lyra.CheckLayoutWritable().ok());
}

TEST(LyraLayoutDescriptorTest, LayoutMapConstructionAndEdgeAreasCover192Slots) {
  const auto bytes = ReadBytes("OFFLINE_ONLY.bin");
  ASSERT_FALSE(bytes.empty());
  Rom image;
  ASSERT_TRUE(image.LoadFromData(bytes).ok());
  const auto layout = image.InspectLyraLayout();
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid) << layout.error;
  std::vector<zelda3::OverworldMap> maps;
  for (int map_id = 0; map_id < layout.capacity; ++map_id) {
    auto map = zelda3::OverworldMap::CreateFromLayout(map_id, &image, nullptr,
                                                      &layout);
    ASSERT_TRUE(map.ok()) << map.status();
    maps.push_back(std::move(*map));
  }
  ASSERT_EQ(maps.size(), 192u);
  EXPECT_FALSE(
      zelda3::OverworldMap::CreateFromLayout(0xC0, &image, nullptr, &layout)
          .ok());
  zelda3::Overworld world(&image);
  const auto reset_models = [&maps] {
    for (int map_id = 0; map_id < static_cast<int>(maps.size()); ++map_id)
      maps[map_id].SetAsSmallMap(map_id);
  };
  reset_models();
  maps[0xBE].SetAreaSize(zelda3::AreaSizeEnum::WideArea);
  world.AssignMapSizes(maps);
  EXPECT_EQ(maps[0xBF].parent(), 0xBE);
  EXPECT_EQ(maps[0xBF].large_index(), 1);
  reset_models();
  maps[0xB7].SetAreaSize(zelda3::AreaSizeEnum::TallArea);
  world.AssignMapSizes(maps);
  EXPECT_EQ(maps[0xBF].parent(), 0xB7);
  EXPECT_EQ(maps[0xBF].large_index(), 2);
  reset_models();
  maps[0xB6].SetAreaSize(zelda3::AreaSizeEnum::LargeArea);
  world.AssignMapSizes(maps);
  EXPECT_EQ(maps[0xBF].parent(), 0xB6);
  EXPECT_EQ(maps[0xBF].large_index(), 3);
  EXPECT_EQ(image.vector(), bytes);
}

TEST(LyraLayoutDescriptorTest,
     MaterializedMapCountAndUnloadBoundsFollowTheModel) {
  Rom image;
  ASSERT_TRUE(image.LoadFromData(ReadBytes("OFFLINE_ONLY.bin")).ok());
  zelda3::Overworld world(&image);
  EXPECT_EQ(world.materialized_map_count(), 0);
  ASSERT_TRUE(world.LoadLyraTerrainPreview(&image).ok());
  EXPECT_EQ(world.materialized_map_count(), 192);
  EXPECT_NE(world.overworld_map(0xBF), nullptr);
  EXPECT_EQ(world.overworld_map(0xC0), nullptr);
  const auto before = image.vector();
  const auto parent = world.overworld_map(0xA1)->parent();
  const auto size = world.overworld_map(0xA0)->area_size();
  EXPECT_FALSE(
      world.ConfigureMultiAreaMap(0xA0, zelda3::AreaSizeEnum::WideArea).ok());
  EXPECT_EQ(world.overworld_map(0xA1)->parent(), parent);
  EXPECT_EQ(world.overworld_map(0xA0)->area_size(), size);
  EXPECT_EQ(image.vector(), before);
  world.Destroy();
  EXPECT_EQ(world.materialized_map_count(), 0);
  EXPECT_FALSE(world.EnsureMapBuilt(0).ok());
}

TEST(LyraLayoutDescriptorTest, InactiveA0A1TerrainUsesDescriptorRenderPath) {
  const auto fixture = FixtureDir() / "LYRA_PREVIEW_A0_A1.bin";
  if (!std::filesystem::exists(fixture)) {
    GTEST_SKIP()
        << "Set YAZE_LYRA_FIXTURE_DIR to the disposable preview packet";
  }
  Rom lyra;
  ASSERT_TRUE(lyra.LoadFromData(ReadBytes(fixture)).ok());
  const auto layout = lyra.InspectLyraLayout();
  ASSERT_EQ(layout.presence, rom::LyraLayoutPresence::kValid);
  EXPECT_EQ(layout.capacity, 192);
  EXPECT_EQ(layout.active_count, 160);
  EXPECT_FALSE(layout.write_supported);
  const auto a0_tiles =
      rom::LyraMapTile32(layout, lyra.data(), lyra.size(), 0xA0);
  const auto a1_tiles =
      rom::LyraMapTile32(layout, lyra.data(), lyra.size(), 0xA1);
  ASSERT_TRUE(a0_tiles.has_value());
  ASSERT_TRUE(a1_tiles.has_value());
  EXPECT_NE(*a0_tiles, *a1_tiles);
  EXPECT_EQ((*a0_tiles)[7 * 16 + 15], 0x2103);
  EXPECT_EQ((*a1_tiles)[7 * 16], 0x2103);
  EXPECT_EQ((*a1_tiles)[7 * 16 + 8], 0x20B2);
  EXPECT_FALSE(rom::LyraMapTile32(layout, lyra.data(), lyra.size(), 0xC0));

  zelda3::GameData game_data(&lyra);
  ASSERT_TRUE(zelda3::LoadGameData(lyra, game_data).ok());
  zelda3::Overworld world(&lyra, &game_data);
  EXPECT_FALSE(world.Load(&lyra).ok());
  ASSERT_TRUE(world.LoadLyraTerrainPreview(&lyra).ok());
  ASSERT_EQ(world.overworld_maps().size(), 192u);
  ASSERT_TRUE(world.EnsureMapBuilt(0xA0).ok());
  ASSERT_TRUE(world.EnsureMapBuilt(0xA1).ok());
  const auto* a0 = world.overworld_map(0xA0);
  const auto* a1 = world.overworld_map(0xA1);
  ASSERT_NE(a0, nullptr);
  ASSERT_NE(a1, nullptr);
  EXPECT_EQ(a0->parent(), 0xA0);
  EXPECT_EQ(a1->parent(), 0xA1);
  EXPECT_EQ(a0->area_specific_bg_color(), 0x19C6);
  EXPECT_EQ(a0->main_palette(), 0);
  EXPECT_EQ(a0->subscreen_overlay(), 0x00FF);
  EXPECT_EQ(a0->animated_gfx(), 0x5B);
  EXPECT_EQ(a0->custom_tileset(0), 0x3A);
  EXPECT_EQ(a0->area_palette(), 0x0A);
  EXPECT_EQ(a0->bitmap_data().size(), 512u * 512u);
  EXPECT_EQ(a1->bitmap_data().size(), 512u * 512u);
  EXPECT_NE(a0->bitmap_data(), a1->bitmap_data());
  EXPECT_FALSE(lyra.CheckLayoutWritable().ok());

  Rom sealed;
  ASSERT_TRUE(
      sealed.LoadFromData(ReadBytes(FixtureDir() / "OFFLINE_ONLY.bin")).ok());
  zelda3::Overworld sealed_world(&sealed);
  ASSERT_TRUE(sealed_world.LoadLyraTerrainPreview(&sealed).ok());
  const auto sealed_tiles = rom::LyraMapTile32(
      sealed.InspectLyraLayout(), sealed.data(), sealed.size(), 0xA0);
  ASSERT_TRUE(sealed_tiles.has_value());
  EXPECT_NE(*sealed_tiles, *a0_tiles);
  EXPECT_EQ((*sealed_tiles)[7 * 16 + 15], 0);

  // Keep the failure semantic: reseal the CRCs so stream decoding, rather
  // than the descriptor checksum, rejects the broken source.
  auto malformed_bytes = ReadBytes(fixture);
  malformed_bytes[0x131010] = 0xFF;
  Reseal(&malformed_bytes, layout);
  Rom malformed;
  ASSERT_TRUE(malformed.LoadFromData(malformed_bytes).ok());
  EXPECT_EQ(malformed.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kInvalid);
  zelda3::Overworld bad_world(&malformed);
  EXPECT_FALSE(bad_world.Load(&malformed).ok());
  EXPECT_FALSE(bad_world.LoadLyraTerrainPreview(&malformed).ok());
}

class LyraCanvasPreviewTest : public test::HeadlessEditorTest {
 protected:
  void SetUp() override {
    HeadlessEditorTest::SetUp();
    const auto fixture = FixtureDir() / "LYRA_PREVIEW_A0_A1.bin";
    if (!std::filesystem::exists(fixture)) {
      GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR to the preview packet";
    }
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(ReadBytes(fixture)).ok());
    game_data_ = std::make_unique<zelda3::GameData>(rom_.get());
    ASSERT_TRUE(zelda3::LoadGameData(*rom_, *game_data_).ok());
    editor::EditorDependencies deps;
    deps.rom = rom_.get();
    deps.game_data = game_data_.get();
    deps.window_manager = window_manager_.get();
    deps.renderer = renderer_.get();
    editor_ = std::make_unique<editor::OverworldEditor>(rom_.get(), deps);
    editor_->SetGameData(game_data_.get());
    editor_->Initialize();
  }

  void TearDown() override {
    editor_.reset();
    game_data_.reset();
    HeadlessEditorTest::TearDown();
  }

  std::unique_ptr<editor::OverworldEditor> editor_;
};

TEST_F(LyraCanvasPreviewTest, FreshCanvasSelectsA0A1AndRejectsC0) {
  ASSERT_TRUE(editor_->Load().ok());
  ASSERT_TRUE(editor_->is_lyra_preview());
  EXPECT_FALSE(editor_->overlay_preview_enabled());
  EXPECT_EQ(editor_->current_map_id(), 0xA0);
  EXPECT_EQ(editor_->current_world_id(), 2);
  EXPECT_FALSE(editor_->overworld().descriptor_sprite_display_available());

  editor_->SelectMapForEditing(0xA0, false);
  ASSERT_TRUE(editor_->map_bitmap(0xA0).is_active());
  editor_->SelectMapForEditing(0xA1, false);
  ASSERT_TRUE(editor_->map_bitmap(0xA1).is_active());
  EXPECT_EQ(editor_->current_map_id(), 0xA1);
  EXPECT_NE(std::memcmp(editor_->map_bitmap(0xA0).data(),
                        editor_->map_bitmap(0xA1).data(), 512 * 512),
            0);

  const auto& palette = editor_->map_bitmap(0xA0).palette();
  ASSERT_FALSE(palette.empty());
  EXPECT_EQ(palette[0].snes(), 0x19C6);
  const auto expected_rgb = gfx::SnesColor(palette[0].snes()).rgb();
  EXPECT_EQ(palette[0].rgb().x, expected_rgb.x);
  EXPECT_EQ(palette[0].rgb().y, expected_rgb.y);
  EXPECT_EQ(palette[0].rgb().z, expected_rgb.z);
  EXPECT_EQ(
      palette[0].snes(),
      editor_->overworld().overworld_map(0xA0)->current_palette()[0].snes());

  const auto a1_target = editor::ResolveOverworldContextTarget(
      2, 0, ImVec2(512.0f, 2048.0f), ImVec2(0, 0), ImVec2(0, 0), 1.0f,
      zelda3::kExpandedMapCount);
  ASSERT_TRUE(a1_target.has_value());
  EXPECT_EQ(a1_target->map_id, 0xA1);
  EXPECT_FALSE(
      editor::ResolveOverworldContextTarget(2, 0, ImVec2(512.0f, 2048.0f),
                                            ImVec2(0, 0), ImVec2(0, 0), 1.0f)
          .has_value());
  editor_->JumpToMap(0xC0);
  EXPECT_EQ(editor_->current_map_id(), 0xA1);

  ASSERT_TRUE(editor_->Update().ok());
  ImGui::Begin("Lyra canvas headless");
  editor_->DrawOverworldCanvas();
  ImGui::End();
  EXPECT_FALSE(rom_->CheckLayoutWritable().ok());
  EXPECT_FALSE(editor_->Save().ok());
  EXPECT_FALSE(editor_->Undo().ok());
  EXPECT_FALSE(editor_->Redo().ok());
  EXPECT_FALSE(editor_->Paste().ok());
}

TEST_F(LyraCanvasPreviewTest, ExistingSpecialOverlayStillBuilds) {
  ASSERT_TRUE(editor_->Load().ok());
  editor_->SelectMapForEditing(0x80, false);
  const auto* map = editor_->overworld().overworld_map(0x80);
  ASSERT_NE(map, nullptr);
  const int overlay_screen =
      zelda3::SubscreenOverlayScreen(map->render_subscreen_overlay());
  ASSERT_GE(overlay_screen, 0);
  ASSERT_LT(overlay_screen, 0xA0);
  auto layer = editor_->overworld().BuildSubscreenOverlayLayer(0x80);
  ASSERT_TRUE(layer.ok()) << layer.status();
  EXPECT_EQ(layer->overlay_screen, overlay_screen);
  EXPECT_EQ(layer->pixels.size(), 512u * 512u);
}

TEST_F(LyraCanvasPreviewTest, IdenticalRollbackReloadKeepsBoundPreview) {
  ASSERT_TRUE(editor_->Load().ok());
  editor_->SelectMapForEditing(0xA1, false);
  const bool dirty = rom_->dirty();
  const auto before = rom_->vector();
  EXPECT_TRUE(editor_->Load().ok());
  EXPECT_EQ(editor_->current_map_id(), 0xA1);
  rom_->mutable_vector()[0x100] ^= 1;
  const auto refused = editor_->Load();
  EXPECT_FALSE(refused.ok());
  EXPECT_EQ(editor_->current_map_id(), 0xA1);
  EXPECT_TRUE(editor_->is_lyra_preview());
  EXPECT_EQ(rom_->dirty(), dirty);
  rom_->mutable_vector()[0x100] ^= 1;
  EXPECT_EQ(rom_->vector(), before);
  EXPECT_TRUE(editor_->Load().ok());
  EXPECT_EQ(editor_->current_map_id(), 0xA1);
}

TEST_F(LyraCanvasPreviewTest, LegacyCanvasRequiresFreshSessionForLyra) {
  Rom legacy;
  ASSERT_TRUE(
      legacy.LoadFromData(ReadBytes(FixtureDir() / "legacy_160_input.bin"))
          .ok());
  auto legacy_data = std::make_unique<zelda3::GameData>(&legacy);
  ASSERT_TRUE(zelda3::LoadGameData(legacy, *legacy_data).ok());
  auto legacy_editor = std::make_unique<editor::OverworldEditor>(&legacy);
  legacy_editor->SetGameData(legacy_data.get());
  legacy_editor->Initialize();
  ASSERT_TRUE(legacy_editor->Load().ok());
  ASSERT_FALSE(legacy_editor->is_lyra_preview());
  EXPECT_TRUE(legacy_editor->CheckRomReplacement(legacy).ok());
  EXPECT_EQ(legacy_editor->CheckRomReplacement(*rom_).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(legacy_editor->is_lyra_preview());
}

TEST(LyraLayoutDescriptorTest, ResealedSemanticCorruptionAndIdentityFail) {
  const auto dir = FixtureDir();
  const auto original_bytes = ReadBytes(dir / "OFFLINE_ONLY.bin");
  const auto original =
      rom::InspectLyraLayout(original_bytes.data(), original_bytes.size());
  ASSERT_EQ(original.presence, rom::LyraLayoutPresence::kValid);
  ASSERT_EQ(original.entries.size(), 35u);

  for (int mutation = 0; mutation < 7; ++mutation) {
    SCOPED_TRACE(mutation);
    auto bytes = original_bytes;
    std::string expected;
    switch (mutation) {
      case 0:
        bytes[0x147000 + 900] = 1;
        expected = "padding";
        break;
      case 1:
        std::fill_n(bytes.begin() + 0x1E4000, 0x4000, 0);
        expected = "sprite pointer";
        break;
      case 2:
        bytes[original.entries[19].pc_address + 3 * 160] = 0xFF;
        bytes[original.entries[19].pc_address + 3 * 160 + 1] = 0xFF;
        bytes[original.entries[19].pc_address + 3 * 160 + 2] = 0x26;
        bytes[0x137FFF] = 0xE4;
        expected = "compressed map stream";
        break;
      case 3:
        bytes[0x131002] = 0xFF;
        expected = "Tile32";
        break;
      case 4:
        bytes[original.entries[6].pc_address + 160] = 0xC0;
        expected = "inactive tail";
        break;
      case 5:
        bytes[0x1423FF] = 0xEA;
        expected = "Conflicting";
        break;
      case 6:
        bytes[0x140145] = 2;
        expected = "Conflicting";
        break;
    }
    Reseal(&bytes, original);
    const auto result = rom::InspectLyraLayout(bytes.data(), bytes.size());
    EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kInvalid);
    EXPECT_NE(result.error.find(expected), std::string::npos) << result.error;
    Rom malformed;
    ASSERT_TRUE(malformed.LoadFromData(bytes).ok());
    EXPECT_EQ(zelda3::DetectOverworldRomProfile(malformed).layout_kind,
              zelda3::OverworldRomProfile::LayoutKind::kLyraOffline);
    EXPECT_FALSE(malformed.CheckLayoutWritable().ok());
  }
}

TEST(LyraLayoutDescriptorTest, InvalidDescriptorFailsCliValidation) {
  const auto dir = FixtureDir();
  cli::handlers::RomValidateCommandHandler validate;
  std::string output;
  for (const char* filename : {"OFFLINE_ONLY.bin", "legacy_160_input.bin"}) {
    Rom control;
    ASSERT_TRUE(control.LoadFromData(ReadBytes(dir / filename)).ok());
    output.clear();
    ASSERT_TRUE(validate.Run({"--format=json"}, &control, &output).ok());
    EXPECT_TRUE(
        nlohmann::json::parse(output).at("validation_passed").get<bool>());
  }
  auto bytes = ReadBytes(dir / "OFFLINE_ONLY.bin");
  bytes[0x14701C] ^= 1;
  Rom malformed;
  ASSERT_TRUE(malformed.LoadFromData(bytes).ok());
  cli::handlers::RomInfoCommandHandler info;
  output.clear();
  ASSERT_TRUE(info.Run({"--format=json"}, &malformed, &output).ok());
  EXPECT_FALSE(
      nlohmann::json::parse(output).at("overworld_format_valid").get<bool>());
  output.clear();
  const auto status = validate.Run({"--format=json"}, &malformed, &output);
  EXPECT_FALSE(status.ok());
  EXPECT_NE(std::string(status.message()).find("CRC"), std::string::npos);
  const auto result = nlohmann::json::parse(output);
  EXPECT_FALSE(result.at("validation_passed").get<bool>());
  EXPECT_NE(
      result.at("results").get<std::string>().find("lyra_descriptor: FAILED"),
      std::string::npos);
}

TEST(LyraLayoutDescriptorTest, PairedSourceAndBackupsRefuseBeforeWrites) {
  const auto dir = FixtureDir();
  const auto root = test::UniqueTempPath("lyra_paired_refusal");
  std::filesystem::create_directories(root);
  struct Cleanup {
    std::filesystem::path root;
    ~Cleanup() { std::filesystem::remove_all(root); }
  } cleanup{root};
  const auto source = root / "source.sfc";
  const auto destination = root / "destination.sfc";
  const auto missing_json = root / "missing.json";
  const auto changed_json = root / "changed.json";
  const auto backups = root / "backups";
  const auto source_bytes = ReadBytes(dir / "OFFLINE_ONLY.bin");
  const std::vector<uint8_t> destination_bytes(64, 0xA5);
  WriteBytes(source, source_bytes);
  WriteBytes(destination, destination_bytes);
  std::filesystem::create_directory(backups);

  Rom loaded;
  ASSERT_TRUE(loaded.LoadFromFile(source.string()).ok());
  const auto disk_source = zelda3::DumpCustomCollisionSourceFromRom(&loaded);
  ASSERT_TRUE(disk_source.ok()) << disk_source.status();
  const std::vector<uint8_t> changed_bytes(disk_source->begin(),
                                           disk_source->end());
  WriteBytes(changed_json, changed_bytes);
  // Room $25 has a single-tile collision stream in both fixture corpora. Change
  // one tile in memory while the sidecar remains coherent with the disk ROM.
  constexpr int kRoom = 0x25;
  const int pointer_offset = zelda3::kCustomCollisionRoomPointers + kRoom * 3;
  auto& memory = loaded.mutable_vector();
  const uint32_t snes = memory[pointer_offset] |
                        (memory[pointer_offset + 1] << 8) |
                        (memory[pointer_offset + 2] << 16);
  const int stream_pc = SnesToPc(snes);
  ASSERT_GE(stream_pc, 0);
  ASSERT_LT(static_cast<size_t>(stream_pc + 4), memory.size());
  ASSERT_EQ(memory[stream_pc], 0xF0);
  ASSERT_EQ(memory[stream_pc + 1], 0xF0);
  memory[stream_pc + 4] ^= 1;
  const auto pending_source = zelda3::DumpCustomCollisionSourceFromRom(&loaded);
  ASSERT_TRUE(pending_source.ok()) << pending_source.status();
  ASSERT_NE(*pending_source, *disk_source);
  loaded.set_dirty(true);
  const auto memory_before = loaded.vector();
  const auto filename_before = loaded.filename();
  auto expect_unchanged = [&] {
    EXPECT_EQ(loaded.vector(), memory_before);
    EXPECT_TRUE(loaded.dirty());
    EXPECT_EQ(loaded.filename(), filename_before);
    EXPECT_EQ(ReadBytes(source), source_bytes);
    EXPECT_EQ(ReadBytes(destination), destination_bytes);
    EXPECT_FALSE(std::filesystem::exists(missing_json));
    EXPECT_EQ(ReadBytes(changed_json), changed_bytes);
    EXPECT_TRUE(std::filesystem::is_empty(backups));
  };
  for (const auto& json : {missing_json, changed_json}) {
    const auto direct = editor::SaveRomWithCollisionSource(&loaded, json);
    EXPECT_EQ(direct.code(), absl::StatusCode::kFailedPrecondition) << direct;
    expect_unchanged();
  }
  editor::RomFileManager manager(nullptr);
  manager.SetBackupFolder(backups.string());
  manager.SetBackupBeforeSave(true);
  for (const auto& json : {missing_json, changed_json}) {
    const auto paired = manager.SaveRom(&loaded, json);
    EXPECT_EQ(paired.code(), absl::StatusCode::kFailedPrecondition) << paired;
    expect_unchanged();
  }
  EXPECT_FALSE(manager.SaveRom(&loaded).ok());
  expect_unchanged();
  EXPECT_FALSE(manager.SaveRomAs(&loaded, destination.string()).ok());
  expect_unchanged();
}

}  // namespace
}  // namespace yaze
