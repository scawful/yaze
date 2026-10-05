#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "app/editor/system/session/collision_source_pairing.h"
#include "app/editor/system/session/rom_file_manager.h"
#include "cli/handlers/rom/rom_commands.h"
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
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
  std::ifstream in(dir / "editor_cases.json");
  ASSERT_TRUE(in.good());
  const auto cases = nlohmann::json::parse(in)["cases"];
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
      // The Oracle fixture detector rejects the older $EA format. Yaze's
      // established marker detector still owns and accepts that format.
      EXPECT_EQ(result.presence, rom::LyraLayoutPresence::kAbsent);
      if (name == "old_tail_marker_without_descriptor") {
        Rom older;
        ASSERT_TRUE(older.LoadFromData(bytes).ok());
        const auto profile = zelda3::DetectOverworldRomProfile(older);
        EXPECT_EQ(profile.layout_kind,
                  zelda3::OverworldRomProfile::LayoutKind::kOlderTailEa);
        EXPECT_EQ(profile.editable_map_count, 192);
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
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
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

TEST(LyraLayoutDescriptorTest, ResealedSemanticCorruptionAndIdentityFail) {
  const auto dir = FixtureDir();
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
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
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
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
  if (dir.empty())
    GTEST_SKIP() << "Set YAZE_LYRA_FIXTURE_DIR";
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
  // Room $25 has a single-tile collision stream in the frozen fixture. Change
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
