#include <gtest/gtest.h>

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
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/system/session/collision_source_pairing.h"
#include "app/editor/system/session/rom_file_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "cli/handlers/rom/rom_commands.h"
#include "editor_test_support.h"
#include "fixtures/lyra/synthetic_fixture_data.h"
#include "framework/headless_editor_test.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz/miniz.h"
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

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

class NativeSpecialOverworldEditorTest : public test::HeadlessEditorTest {};

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
