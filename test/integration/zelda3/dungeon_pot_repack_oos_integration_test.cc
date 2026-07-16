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
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/features.h"
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
constexpr int kSaveReopenCycles = 50;
constexpr std::pair<const char*, const char*> kManifestEnvVars[] = {
    {"YAZE_TEST_HACK_MANIFEST_OOS", "Oracle manifest"},
    {"YAZE_TEST_HACK_MANIFEST", "hack manifest"},
};
constexpr std::pair<const char*, const char*> kProjectEnvVars[] = {
    {"YAZE_TEST_PROJECT_OOS", "Oracle project"},
    {"YAZE_TEST_PROJECT", "Yaze project"},
};

struct DungeonSaveFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) previous =
      core::FeatureFlags::get().dungeon;
  ~DungeonSaveFlagsGuard() { core::FeatureFlags::get().dungeon = previous; }
};

void ConfigurePotOnlySave() {
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveSprites = false;
  flags.kSaveRoomHeaders = false;
  flags.kSaveTorches = false;
  flags.kSavePits = false;
  flags.kSaveBlocks = false;
  flags.kSaveCollision = false;
  flags.kSaveWaterFillZones = false;
  flags.kSaveChests = false;
  flags.kSavePotItems = true;
  flags.kSaveEntrances = false;
  flags.kSavePalettes = false;
}

absl::StatusOr<std::filesystem::path> CreateUniqueTempDir() {
  static std::atomic<uint64_t> sequence = 0;
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path base = std::filesystem::temp_directory_path();
  for (uint32_t attempt = 0; attempt < 100; ++attempt) {
    const std::filesystem::path candidate =
        base / absl::StrFormat("yaze_oos_pot_repack_%lld_%llu_%u", nonce,
                               sequence.fetch_add(1), attempt);
    std::error_code create_error;
    if (std::filesystem::create_directory(candidate, create_error)) {
      std::error_code permission_error;
      std::filesystem::permissions(candidate, std::filesystem::perms::owner_all,
                                   std::filesystem::perm_options::replace,
                                   permission_error);
      if (permission_error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(candidate, cleanup_error);
        return absl::InternalError(absl::StrFormat(
            "Could not secure private pot-repack temp directory: %s%s",
            permission_error.message(),
            cleanup_error ? absl::StrFormat("; cleanup also failed: %s",
                                            cleanup_error.message())
                          : ""));
      }
      return candidate;
    }
    if (create_error) {
      return absl::InternalError(absl::StrFormat(
          "Could not create private pot-repack temp directory: %s",
          create_error.message()));
    }
  }
  return absl::AlreadyExistsError(
      "Could not allocate a collision-free pot-repack temp directory");
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

std::string ResolveOracleRomPath() {
  // The Oracle-specific fixture must win even when a generic expanded ROM is
  // also configured in the developer's environment.
  for (const char* env_var : {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_EXPANDED",
                              "YAZE_TEST_ROM_EXPANDED_PATH"}) {
    if (const char* value = std::getenv(env_var);
        value != nullptr && std::filesystem::exists(value)) {
      return value;
    }
  }

  // Retain the test harness's compile-time expanded-ROM fallback.
  return ::yaze::test::TestRomManager::GetRomPath(
      ::yaze::test::RomRole::kExpanded);
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
    if (std::getenv("YAZE_SKIP_ROM_TESTS") != nullptr) {
      GTEST_SKIP() << "ROM testing disabled via YAZE_SKIP_ROM_TESTS. Test: "
                      "DungeonPotRepackOosIntegrationTest";
    }
    source_rom_path_ = ResolveOracleRomPath();
    if (source_rom_path_.empty()) {
      GTEST_SKIP() << "Oracle ROM fixture not found. Set YAZE_TEST_ROM_OOS "
                      "or YAZE_TEST_ROM_EXPANDED.";
    }

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
      const absl::Status status =
          project_.hack_manifest.LoadFromFile(manifest_path);
      ASSERT_TRUE(status.ok()) << status.message();
      project_.hack_manifest_file = manifest_path;
      project_.rom_filename = source_rom_path_;
      project_.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
    } else {
      ASSERT_TRUE(std::filesystem::exists(project_path))
          << configured_project_var << " does not exist: " << project_path;
      const absl::Status status = project_.Open(project_path);
      ASSERT_TRUE(status.ok()) << status.message();
      ASSERT_TRUE(project_.hack_manifest.loaded())
          << "Project did not load its configured hack manifest: "
          << project_path;
    }

    const core::DungeonStreamLayout* manifest_layout =
        project_.hack_manifest.GetDungeonStreamLayout(
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

    auto temp_dir = CreateUniqueTempDir();
    ASSERT_TRUE(temp_dir.ok()) << temp_dir.status().message();
    temp_dir_ = std::move(*temp_dir);
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
    if (!source_rom_path_.empty() && !source_sha_before_.empty()) {
      auto source_sha_after = ComputeSha256(source_rom_path_);
      EXPECT_TRUE(source_sha_after.ok()) << source_sha_after.status().message();
      if (source_sha_after.ok()) {
        EXPECT_EQ(*source_sha_after, source_sha_before_)
            << "The source Oracle ROM changed during the integration test";
      }
    }

    // YazeProject registers its manifest with the process-global resource
    // labels. Clear that non-owning pointer before the fixture is destroyed.
    GetResourceLabels().SetHackManifest(nullptr);
    if (!temp_dir_.empty()) {
      std::error_code cleanup_error;
      std::filesystem::remove_all(temp_dir_, cleanup_error);
      EXPECT_FALSE(cleanup_error)
          << "Failed to remove private ROM temp directory " << temp_dir_ << ": "
          << cleanup_error.message();
      std::error_code exists_error;
      EXPECT_FALSE(std::filesystem::exists(temp_dir_, exists_error))
          << "Private ROM temp directory leaked: " << temp_dir_;
      EXPECT_FALSE(exists_error)
          << "Could not verify private ROM temp cleanup: "
          << exists_error.message();
    }
  }

  std::string source_rom_path_;
  std::string source_sha_before_;
  std::filesystem::path temp_dir_;
  std::filesystem::path temp_rom_path_;
  project::YazeProject project_;
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

  DungeonSaveFlagsGuard flags_guard;
  ConfigurePotOnlySave();

  auto dungeon_editor = std::make_unique<editor::DungeonEditorV2>(&rom);
  editor::EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project_;
  dungeon_editor->SetDependencies(dependencies);

  Room& edited_room = dungeon_editor->rooms()[selected_room];
  edited_room = Room(static_cast<int>(selected_room), &rom);
  edited_room.LoadPotItems();
  ASSERT_TRUE(edited_room.GetPotItems().empty());
  edited_room.GetPotItems() = replacement_items;
  edited_room.MarkPotItemsDirty();

  const auto predicted_ranges = dungeon_editor->CollectWriteRanges();
  for (const DungeonStreamPcRange& range : layout_.allocation_ranges) {
    EXPECT_NE(std::find(predicted_ranges.begin(), predicted_ranges.end(),
                        std::pair<uint32_t, uint32_t>{range.begin, range.end}),
              predicted_ranges.end())
        << "Editor persistence omitted a declared pot allocation range";
  }
  const uint32_t pointer_width =
      layout_.pointer_encoding == DungeonPointerEncoding::kLong24 ? 3u : 2u;
  EXPECT_NE(std::find(predicted_ranges.begin(), predicted_ranges.end(),
                      std::pair<uint32_t, uint32_t>{
                          layout_.pointer_table_pc,
                          layout_.pointer_table_pc +
                              layout_.pointer_count * pointer_width}),
            predicted_ranges.end())
      << "Editor persistence omitted the complete pot pointer table";

  const absl::Status save_status =
      dungeon_editor->SaveRoom(static_cast<int>(selected_room));
  ASSERT_TRUE(save_status.ok()) << save_status.message();
  EXPECT_FALSE(edited_room.pot_items_dirty());
  RecordProperty("repack_edit_mode", "unique-one-item");

  Rom::SaveSettings save_settings;
  save_settings.filename = temp_rom_path_.string();
  ASSERT_TRUE(rom.SaveToFile(save_settings).ok());
  const std::vector<uint8_t> first_save_bytes = ReadFile(temp_rom_path_);
  ASSERT_FALSE(first_save_bytes.empty());
  dungeon_editor.reset();

  {
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
      const DungeonStreamRecord* alias_after =
          FindRoomRecord(*after, alias_room);
      ASSERT_NE(alias_after, nullptr);
      EXPECT_EQ(alias_after->encoded_stream,
                std::vector<uint8_t>({0xFF, 0xFF}));
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
  }

  for (int cycle = 1; cycle <= kSaveReopenCycles; ++cycle) {
    SCOPED_TRACE(absl::StrFormat("save/close/reopen cycle %d", cycle));
    {
      Rom cycle_rom;
      ASSERT_TRUE(cycle_rom.LoadFromFile(temp_rom_path_.string()).ok());

      auto cycle_editor = std::make_unique<editor::DungeonEditorV2>(&cycle_rom);
      dependencies.rom = &cycle_rom;
      cycle_editor->SetDependencies(dependencies);
      Room& cycle_room = cycle_editor->rooms()[selected_room];
      cycle_room = Room(static_cast<int>(selected_room), &cycle_rom);
      cycle_room.LoadPotItems();
      ASSERT_EQ(EncodePotItems(cycle_room.GetPotItems()), replacement_stream);
      cycle_room.MarkPotItemsDirty();

      const absl::Status cycle_status =
          cycle_editor->SaveRoom(static_cast<int>(selected_room));
      ASSERT_TRUE(cycle_status.ok()) << cycle_status.message();
      EXPECT_FALSE(cycle_room.pot_items_dirty());
      ASSERT_TRUE(cycle_rom.SaveToFile(save_settings).ok());
    }

    ASSERT_EQ(ReadFile(temp_rom_path_), first_save_bytes)
        << "Semantic save changed ROM bytes during cycle " << cycle;
  }
  RecordProperty("save_reopen_cycles", kSaveReopenCycles);
}

}  // namespace
}  // namespace yaze::zelda3::test
