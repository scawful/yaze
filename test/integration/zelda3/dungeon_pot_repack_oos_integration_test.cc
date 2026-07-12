// ROM-gated integration coverage for manifest-backed Oracle pot-item repacks.
//
// Fixtures:
//   YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_EXPANDED
//   YAZE_TEST_HACK_MANIFEST_OOS or YAZE_TEST_HACK_MANIFEST
//
// A Yaze project may be supplied instead of a manifest with
// YAZE_TEST_PROJECT_OOS or YAZE_TEST_PROJECT. The source ROM is never opened
// for writing; every mutation is performed on a temporary copy.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/oracle_rom_safety_preflight.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"

namespace yaze::zelda3::test {
namespace {

constexpr uint32_t kOracleRoomCount = 296;
constexpr std::pair<const char*, const char*> kManifestEnvVars[] = {
    {"YAZE_TEST_HACK_MANIFEST_OOS", "Oracle manifest"},
    {"YAZE_TEST_HACK_MANIFEST", "hack manifest"},
};
constexpr std::pair<const char*, const char*> kProjectEnvVars[] = {
    {"YAZE_TEST_PROJECT_OOS", "Oracle project"},
    {"YAZE_TEST_PROJECT", "Yaze project"},
};

std::filesystem::path MakeUniqueTempDir() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_oos_pot_repack_" + std::to_string(nonce));
}

std::string FirstConfiguredPath(const std::pair<const char*, const char*>* vars,
                                size_t count, std::string* configured_var) {
  for (size_t i = 0; i < count; ++i) {
    if (const char* value = std::getenv(vars[i].first); value != nullptr) {
      if (configured_var != nullptr) {
        *configured_var = vars[i].first;
      }
      return value;
    }
  }
  return {};
}

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>());
}

std::vector<uint8_t> EncodePotItems(const std::vector<PotItem>& items) {
  std::vector<uint8_t> encoded;
  encoded.reserve(items.size() * 3 + 2);
  for (const PotItem& item : items) {
    encoded.push_back(item.position & 0xFF);
    encoded.push_back((item.position >> 8) & 0xFF);
    encoded.push_back(item.item);
  }
  encoded.push_back(0xFF);
  encoded.push_back(0xFF);
  return encoded;
}

std::vector<PotItem> DecodePotItems(const std::vector<uint8_t>& encoded) {
  std::vector<PotItem> items;
  if (encoded.size() < 2 || encoded[encoded.size() - 2] != 0xFF ||
      encoded.back() != 0xFF || (encoded.size() - 2) % 3 != 0) {
    return items;
  }
  for (size_t offset = 0; offset + 2 < encoded.size() - 1; offset += 3) {
    const uint16_t position = static_cast<uint16_t>(encoded[offset]) |
                              (static_cast<uint16_t>(encoded[offset + 1]) << 8);
    items.push_back({position, encoded[offset + 2]});
  }
  return items;
}

const DungeonStreamRecord* FindRoomRecord(
    const DungeonStreamInventory& inventory, uint32_t room_id) {
  const auto iter =
      std::find_if(inventory.streams.begin(), inventory.streams.end(),
                   [room_id](const DungeonStreamRecord& record) {
                     return record.room_id == room_id;
                   });
  return iter == inventory.streams.end() ? nullptr : &*iter;
}

bool IsInsideAllocationRange(const DungeonStreamLayout& layout,
                             const DungeonStreamRecord& record) {
  return std::any_of(layout.allocation_ranges.begin(),
                     layout.allocation_ranges.end(),
                     [&record](const DungeonStreamPcRange& range) {
                       return range.begin <= record.data_pc &&
                              record.logical_end_pc <= range.end;
                     });
}

class DungeonPotRepackOosIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    YAZE_SKIP_IF_ROM_MISSING(::yaze::test::RomRole::kExpanded,
                             "DungeonPotRepackOosIntegrationTest");
    source_rom_path_ = ::yaze::test::TestRomManager::GetRomPath(
        ::yaze::test::RomRole::kExpanded);

    std::string configured_manifest_var;
    std::string configured_project_var;
    const std::string manifest_path =
        FirstConfiguredPath(kManifestEnvVars, std::size(kManifestEnvVars),
                            &configured_manifest_var);
    const std::string project_path = FirstConfiguredPath(
        kProjectEnvVars, std::size(kProjectEnvVars), &configured_project_var);
    if (manifest_path.empty() && project_path.empty()) {
      GTEST_SKIP() << "Oracle pot repack manifest not configured. Set "
                      "YAZE_TEST_HACK_MANIFEST_OOS or "
                      "YAZE_TEST_PROJECT_OOS.";
    }

    if (!manifest_path.empty()) {
      ASSERT_TRUE(std::filesystem::exists(manifest_path))
          << configured_manifest_var << " does not exist: " << manifest_path;
      const absl::Status status = manifest_.LoadFromFile(manifest_path);
      ASSERT_TRUE(status.ok()) << status.message();
      manifest_in_use_ = &manifest_;
    } else {
      ASSERT_TRUE(std::filesystem::exists(project_path))
          << configured_project_var << " does not exist: " << project_path;
      const absl::Status status = project_.Open(project_path);
      ASSERT_TRUE(status.ok()) << status.message();
      ASSERT_TRUE(project_.hack_manifest.loaded())
          << "Project did not load its configured hack manifest: "
          << project_path;
      manifest_in_use_ = &project_.hack_manifest;
    }

    const core::DungeonStreamLayout* manifest_layout =
        manifest_in_use_->GetDungeonStreamLayout(
            core::DungeonStreamType::kPotItems);
    ASSERT_NE(manifest_layout, nullptr)
        << "Oracle manifest has no pot_items dungeon stream layout";
    ASSERT_EQ(manifest_layout->strategy,
              core::DungeonWriteStrategy::kRepackAll);
    auto layout = core::ToDungeonStreamAllocatorLayout(
        core::DungeonStreamType::kPotItems, *manifest_layout);
    ASSERT_TRUE(layout.ok()) << layout.status().message();
    layout_ = std::move(*layout);
    ASSERT_EQ(layout_.pointer_count, kOracleRoomCount);

    temp_dir_ = MakeUniqueTempDir();
    ASSERT_TRUE(std::filesystem::create_directories(temp_dir_));
    temp_rom_path_ = temp_dir_ / "oos-pot-repack-test.sfc";
    std::error_code copy_error;
    ASSERT_TRUE(std::filesystem::copy_file(
        source_rom_path_, temp_rom_path_,
        std::filesystem::copy_options::overwrite_existing, copy_error))
        << copy_error.message();

    auto source_sha = ComputeSha256(source_rom_path_);
    ASSERT_TRUE(source_sha.ok()) << source_sha.status().message();
    source_sha_before_ = *source_sha;
  }

  void TearDown() override {
    // YazeProject registers its manifest with the process-global resource
    // labels. Clear that non-owning pointer before the fixture is destroyed.
    GetResourceLabels().SetHackManifest(nullptr);
    std::error_code cleanup_error;
    std::filesystem::remove_all(temp_dir_, cleanup_error);
  }

  std::string source_rom_path_;
  std::string source_sha_before_;
  std::filesystem::path temp_dir_;
  std::filesystem::path temp_rom_path_;
  core::HackManifest manifest_;
  project::YazeProject project_;
  const core::HackManifest* manifest_in_use_ = nullptr;
  DungeonStreamLayout layout_;
};

TEST_F(DungeonPotRepackOosIntegrationTest,
       SharedEmptyOwnerDetachesAndRoundTripsDeterministically) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(temp_rom_path_.string()).ok());

  auto before = InventoryDungeonStreams(rom, layout_);
  ASSERT_TRUE(before.ok()) << before.status().message();
  ASSERT_TRUE(before->ok()) << "Oracle pot inventory contains stream issues";
  ASSERT_EQ(before->streams.size(), kOracleRoomCount);

  const DungeonStreamAliasGroup* empty_alias = nullptr;
  for (const DungeonStreamAliasGroup& alias : before->aliases) {
    const DungeonStreamRecord* record =
        alias.room_ids.empty()
            ? nullptr
            : FindRoomRecord(*before, alias.room_ids.front());
    if (record != nullptr &&
        record->encoded_stream == std::vector<uint8_t>({0xFF, 0xFF})) {
      empty_alias = &alias;
      break;
    }
  }
  ASSERT_NE(empty_alias, nullptr)
      << "Oracle ROM has no shared canonical-empty pot stream";
  ASSERT_GT(empty_alias->room_ids.size(), 1u);

  // The deterministic repacker defines a payload's owner as its lowest room
  // ID. Editing that actual shared-empty owner exercises ownership transfer as
  // well as alias detachment.
  const uint32_t selected_room = *std::min_element(
      empty_alias->room_ids.begin(), empty_alias->room_ids.end());

  // Pick a valid, in-room green-rupee item whose five-byte stream is new to
  // this ROM. A unique payload exercises the real 9-byte-capacity edge rather
  // than accidentally deduplicating with an existing room.
  std::vector<PotItem> replacement_items;
  std::vector<uint8_t> replacement_stream;
  for (uint16_t x = 0x10; x < 0x80; ++x) {
    std::vector<PotItem> candidate{{static_cast<uint16_t>(0x1000 | x), 0x01}};
    std::vector<uint8_t> encoded = EncodePotItems(candidate);
    const bool already_present =
        std::any_of(before->streams.begin(), before->streams.end(),
                    [&encoded](const DungeonStreamRecord& record) {
                      return record.encoded_stream == encoded;
                    });
    if (!already_present) {
      replacement_items = std::move(candidate);
      replacement_stream = std::move(encoded);
      break;
    }
  }
  ASSERT_EQ(replacement_items.size(), 1u)
      << "Could not construct a unique valid one-item pot stream";

  Room edited_room(static_cast<int>(selected_room), &rom);
  edited_room.LoadPotItems();
  ASSERT_TRUE(edited_room.GetPotItems().empty());
  edited_room.GetPotItems() = replacement_items;
  edited_room.MarkPotItemsDirty();

  auto room_lookup = [&edited_room, selected_room](int room_id) -> const Room* {
    return room_id == static_cast<int>(selected_room) ? &edited_room : nullptr;
  };
  const std::vector<uint8_t> before_attempt = rom.vector();
  absl::Status save_status = SaveAllPotItems(
      &rom, static_cast<int>(kOracleRoomCount), room_lookup, &layout_);

  if (absl::IsResourceExhausted(save_status)) {
    // A future Oracle layout may have less than today's nine free bytes. The
    // failed unique insertion must be fully transactional. Reusing the
    // smallest existing non-empty payload is the smaller semantic edit: it
    // still detaches this room from the shared-empty owner while adding no new
    // unique bytes to the repack.
    ASSERT_EQ(rom.vector(), before_attempt);
    ASSERT_TRUE(edited_room.pot_items_dirty());

    const DungeonStreamRecord* smallest_nonempty = nullptr;
    for (const DungeonStreamRecord& record : before->streams) {
      if (record.encoded_stream.size() <= 2) {
        continue;
      }
      if (smallest_nonempty == nullptr ||
          record.encoded_stream.size() <
              smallest_nonempty->encoded_stream.size()) {
        smallest_nonempty = &record;
      }
    }
    ASSERT_NE(smallest_nonempty, nullptr)
        << "No smaller existing Oracle pot payload is available";
    replacement_items = DecodePotItems(smallest_nonempty->encoded_stream);
    ASSERT_FALSE(replacement_items.empty());
    replacement_stream = smallest_nonempty->encoded_stream;
    edited_room.GetPotItems() = replacement_items;
    edited_room.MarkPotItemsDirty();
    save_status = SaveAllPotItems(&rom, static_cast<int>(kOracleRoomCount),
                                  room_lookup, &layout_);
    RecordProperty("repack_edit_mode", "existing-payload-capacity-fallback");
  } else {
    RecordProperty("repack_edit_mode", "unique-one-item");
  }
  ASSERT_TRUE(save_status.ok()) << save_status.message();
  EXPECT_FALSE(edited_room.pot_items_dirty());

  Rom::SaveSettings save_settings;
  save_settings.filename = temp_rom_path_.string();
  ASSERT_TRUE(rom.SaveToFile(save_settings).ok());
  const std::vector<uint8_t> first_save_bytes = ReadFile(temp_rom_path_);
  ASSERT_FALSE(first_save_bytes.empty());

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(temp_rom_path_.string()).ok());
  auto after = InventoryDungeonStreams(reopened, layout_);
  ASSERT_TRUE(after.ok()) << after.status().message();
  ASSERT_TRUE(after->ok()) << "Repacked Oracle pot inventory has issues";
  ASSERT_EQ(after->streams.size(), kOracleRoomCount);
  for (const DungeonStreamRecord& record : after->streams) {
    EXPECT_TRUE(record.valid) << "Invalid room pointer: " << record.room_id;
    EXPECT_TRUE(IsInsideAllocationRange(layout_, record))
        << "Room pointer escaped the manifest allocation range: "
        << record.room_id;
  }

  const DungeonStreamRecord* selected_after =
      FindRoomRecord(*after, selected_room);
  ASSERT_NE(selected_after, nullptr);
  EXPECT_EQ(selected_after->encoded_stream, replacement_stream);
  for (uint32_t alias_room : empty_alias->room_ids) {
    if (alias_room == selected_room) {
      continue;
    }
    const DungeonStreamRecord* alias_after = FindRoomRecord(*after, alias_room);
    ASSERT_NE(alias_after, nullptr);
    EXPECT_EQ(alias_after->encoded_stream, std::vector<uint8_t>({0xFF, 0xFF}));
    EXPECT_NE(alias_after->data_pc, selected_after->data_pc)
        << "Selected room remained attached to shared-empty room "
        << alias_room;
  }

  for (const DungeonStreamRecord& record_before : before->streams) {
    if (record_before.room_id == selected_room) {
      continue;
    }
    const DungeonStreamRecord* record_after =
        FindRoomRecord(*after, record_before.room_id);
    ASSERT_NE(record_after, nullptr);
    EXPECT_EQ(record_after->encoded_stream, record_before.encoded_stream)
        << "Semantic pot stream changed for untouched room "
        << record_before.room_id;
  }

  Room repeated_room(static_cast<int>(selected_room), &reopened);
  repeated_room.LoadPotItems();
  ASSERT_EQ(EncodePotItems(repeated_room.GetPotItems()), replacement_stream);
  repeated_room.MarkPotItemsDirty();
  auto repeated_lookup = [&repeated_room,
                          selected_room](int room_id) -> const Room* {
    return room_id == static_cast<int>(selected_room) ? &repeated_room
                                                      : nullptr;
  };
  ASSERT_TRUE(SaveAllPotItems(&reopened, static_cast<int>(kOracleRoomCount),
                              repeated_lookup, &layout_)
                  .ok());
  ASSERT_TRUE(reopened.SaveToFile(save_settings).ok());
  EXPECT_EQ(ReadFile(temp_rom_path_), first_save_bytes)
      << "Repeating the same semantic save changed the ROM bytes";

  auto source_sha_after = ComputeSha256(source_rom_path_);
  ASSERT_TRUE(source_sha_after.ok()) << source_sha_after.status().message();
  EXPECT_EQ(*source_sha_after, source_sha_before_)
      << "The source Oracle ROM changed during the integration test";
}

}  // namespace
}  // namespace yaze::zelda3::test
