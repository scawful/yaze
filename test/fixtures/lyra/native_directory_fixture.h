#ifndef YAZE_TEST_FIXTURES_LYRA_NATIVE_DIRECTORY_FIXTURE_H_
#define YAZE_TEST_FIXTURES_LYRA_NATIVE_DIRECTORY_FIXTURE_H_

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include "fixtures/lyra/synthetic_fixture_data.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/native_overworld_authoring_metadata.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_sprite_io.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz/miniz.h"
#undef MINIZ_NO_ZLIB_COMPATIBLE_NAMES

namespace yaze::test::native_overworld {
inline std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
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

inline std::filesystem::path FixtureDir() {
  const char* path = std::getenv("YAZE_LYRA_FIXTURE_DIR");
  return path == nullptr ? std::filesystem::path()
                         : std::filesystem::path(path);
}

inline void WriteBytes(const std::filesystem::path& path,
                       const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

inline void Put32(std::vector<uint8_t>* bytes, size_t pc, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    (*bytes)[pc + i] = static_cast<uint8_t>(value >> (8 * i));
  }
}

// Reseal each directory entry and the descriptor after a semantic mutation.
// This prevents a CRC failure from masking the deeper contract being tested.
inline void Reseal(std::vector<uint8_t>* bytes,
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
  void AddAuthoringMetadata(
      const rom::NativeOverworldAuthoringMetadata& metadata = {}) {
    const auto encoded =
        rom::EncodeNativeOverworldAuthoringMetadata(metadata, Entry(1).count);
    ASSERT_TRUE(encoded.ok()) << encoded.status();
    // Caller-supplied synthetic storage, not a production allocation grant.
    Add(412, 9, 0x2B6000, 256, 32);
    std::copy(encoded->begin(), encoded->end(),
              bytes.begin() + Entry(412).pc_address);
    Word(0x14700C, 0x03B0);
    Word(0x147012, 57);
    bytes[0x147015] = 0x3C;
    Seal();
  }
  // Synthetic bank allocation only; no production migration is implied.
  void AddPageTile16(uint16_t row_capacity = 16) {
    Add(rom::kNativePageTile16TableId, 10, 0x2C0000, row_capacity,
        rom::kNativePageTile16Stride);
    std::fill_n(bytes.begin() + Entry(413).pc_address,
                row_capacity * rom::kNativePageTile16Stride, 0xFF);
    Word(0x147008, 5);
    Word(0x14700C, 32 + rows.entries.size() * 16);
    Word(0x147012, rows.entries.size());
    bytes[0x147015] |= 0x40;
    Seal();
  }
  rom::NativeOverworldAuthoringMetadata SeedCanonicalPlacementRows() {
    // Canonical identities/order on a synthetic model allocation; this is
    // not the one-time project importer or runtime qualification.
    rom::NativeOverworldAuthoringMetadata metadata;
    metadata.next_placement_id = 0x6003;
    metadata.placements = {
        {0x5001, rom::NativePlacementState::kBound,
         rom::NativeAuthoringSpritePlacement{1, 0xB0, 0, {14, 23, 0x52}}},
        {0x5002, rom::NativePlacementState::kBound,
         rom::NativeAuthoringSpritePlacement{1, 0xB0, 1, {21, 15, 0x52}}},
        {0x5005, rom::NativePlacementState::kBound,
         rom::NativeAuthoringSpritePlacement{1, 0xB0, 2, {12, 40, 0x52}}},
        {0x5003, rom::NativePlacementState::kBound,
         rom::NativeAuthoringSpritePlacement{1, 0xB0, 3, {43, 13, 0x52}}},
        {0x5004, rom::NativePlacementState::kBound,
         rom::NativeAuthoringSpritePlacement{1, 0xB0, 4, {45, 37, 0x52}}},
        {0x6001, rom::NativePlacementState::kPending,
         rom::NativeAuthoringDoorPlacement{0xB0, 21, 9, 0x0B, std::nullopt}},
        {0x6002, rom::NativePlacementState::kPending,
         rom::NativeAuthoringReturnPlacement{0xB0, 22, 11, 0x83,
                                             std::nullopt}}};
    const auto actors = Entry(100).pc_address;
    EXPECT_EQ(Entry(100).count, 16);
    const std::array<uint8_t, 16> bytes = {14, 23, 0x52, 21,  15, 0x52,
                                           12, 40, 0x52, 43,  13, 0x52,
                                           45, 37, 0x52, 0xFF};
    std::copy(bytes.begin(), bytes.end(), this->bytes.begin() + actors);
    for (int phase = 0; phase < 3; ++phase)
      for (int map = 160; map < 192; ++map)
        Word(Entry(16 + phase).pc_address + map * 2, 0x800F);
    Word(Entry(17).pc_address + 0xB0 * 2, 0x8000);
    for (int map : {0xB0, 0xB1, 0xB8, 0xB9}) {
      this->bytes[Entry(7).pc_address + map] = 0xB0;
      this->bytes[Entry(30).pc_address + map] = 1;
    }
    AddAuthoringMetadata(metadata);
    return metadata;
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

}  // namespace yaze::test::native_overworld
#endif
