#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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
    std::filesystem::remove(source);
    std::filesystem::remove(target);
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
