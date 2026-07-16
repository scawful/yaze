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
#include "rom/snes.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/dungeon_validator.h"
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

void ConfigureObjectSpritePotSave() {
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = true;
  flags.kSaveSprites = true;
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

bool IsInsideDataRange(const DungeonStreamLayout& layout,
                       const DungeonStreamRecord& record) {
  return std::any_of(layout.data_ranges.begin(), layout.data_ranges.end(),
                     [&record](const DungeonStreamPcRange& range) {
                       return range.begin <= record.data_pc &&
                              record.logical_end_pc <= range.end;
                     });
}

bool IsCanonicalEmptyObjectStream(const std::vector<uint8_t>& stream) {
  if (stream.size() < 8) {
    return false;
  }

  size_t cursor = 2;  // Preserve the floor/layout header.
  for (int list = 0; list < 2; ++list) {
    if (cursor + 2 > stream.size() || stream[cursor] != 0xFF ||
        stream[cursor + 1] != 0xFF) {
      return false;
    }
    cursor += 2;
  }

  // Empty Oracle rooms either terminate list 2 directly or include an
  // explicit empty door list.
  if (cursor + 2 == stream.size() && stream[cursor] == 0xFF &&
      stream[cursor + 1] == 0xFF) {
    return true;
  }
  return cursor + 4 == stream.size() && stream[cursor] == 0xF0 &&
         stream[cursor + 1] == 0xFF && stream[cursor + 2] == 0xFF &&
         stream[cursor + 3] == 0xFF;
}

bool IsCanonicalEmptySpriteStream(const std::vector<uint8_t>& stream) {
  return stream.size() == 2 && stream[1] == 0xFF;
}

bool IsCanonicalEmptyPotStream(const std::vector<uint8_t>& stream) {
  return stream == std::vector<uint8_t>({0xFF, 0xFF});
}

template <typename Predicate>
const DungeonStreamAliasGroup* FindCanonicalEmptyAlias(
    const DungeonStreamInventory& inventory, Predicate&& is_empty) {
  for (const DungeonStreamAliasGroup& alias : inventory.aliases) {
    const DungeonStreamRecord* record =
        alias.room_ids.empty()
            ? nullptr
            : FindRoomRecord(inventory, alias.room_ids.front());
    if (record != nullptr && is_empty(record->encoded_stream)) {
      return &alias;
    }
  }
  return nullptr;
}

std::vector<uint32_t> IntersectRoomIds(const std::vector<uint32_t>& first,
                                       const std::vector<uint32_t>& second,
                                       const std::vector<uint32_t>& third) {
  std::vector<uint32_t> common;
  for (uint32_t room_id : first) {
    if (std::find(second.begin(), second.end(), room_id) != second.end() &&
        std::find(third.begin(), third.end(), room_id) != third.end()) {
      common.push_back(room_id);
    }
  }
  std::sort(common.begin(), common.end());
  return common;
}

bool CoordinateIsOccupied(const Room& room, int x, int y) {
  if (std::any_of(room.GetTileObjects().begin(), room.GetTileObjects().end(),
                  [x, y](const RoomObject& object) {
                    return object.x() == x && object.y() == y;
                  })) {
    return true;
  }
  if (std::any_of(room.GetSprites().begin(), room.GetSprites().end(),
                  [x, y](const Sprite& sprite) {
                    return sprite.x() == x && sprite.y() == y;
                  })) {
    return true;
  }
  return std::any_of(room.GetPotItems().begin(), room.GetPotItems().end(),
                     [x, y](const PotItem& item) {
                       return item.GetTileX() == x && item.GetTileY() == y;
                     });
}

absl::StatusOr<DungeonStreamLayout> LoadManifestLayout(
    const project::YazeProject& project, core::DungeonStreamType stream_type,
    core::DungeonWriteStrategy expected_strategy) {
  const core::DungeonStreamLayout* manifest_layout =
      project.hack_manifest.GetDungeonStreamLayout(stream_type);
  if (manifest_layout == nullptr) {
    return absl::FailedPreconditionError(
        "Oracle manifest is missing a required dungeon stream layout");
  }
  if (manifest_layout->strategy != expected_strategy) {
    return absl::FailedPreconditionError(
        "Oracle dungeon stream layout uses the wrong write strategy");
  }
  return core::ToDungeonStreamAllocatorLayout(stream_type, *manifest_layout);
}

::testing::AssertionResult InventoryPreservesUntouchedRooms(
    const DungeonStreamInventory& before, const DungeonStreamInventory& after,
    const DungeonStreamLayout& layout, uint32_t selected_room,
    const std::vector<uint8_t>& selected_stream,
    bool preserve_untouched_addresses) {
  if (!after.ok()) {
    return ::testing::AssertionFailure() << "Reopened inventory contains "
                                         << after.issues.size() << " issue(s)";
  }
  if (after.streams.size() != before.streams.size()) {
    return ::testing::AssertionFailure()
           << "Stream slot count changed from " << before.streams.size()
           << " to " << after.streams.size();
  }

  for (const DungeonStreamRecord& record_before : before.streams) {
    const DungeonStreamRecord* record_after =
        FindRoomRecord(after, record_before.room_id);
    if (record_after == nullptr) {
      return ::testing::AssertionFailure()
             << "Missing reopened room " << record_before.room_id;
    }
    if (!record_after->valid || !IsInsideDataRange(layout, *record_after)) {
      return ::testing::AssertionFailure()
             << "Invalid reopened stream for room " << record_before.room_id;
    }
    if (record_before.room_id == selected_room) {
      if (record_after->encoded_stream != selected_stream) {
        return ::testing::AssertionFailure()
               << "Selected room stream did not round trip";
      }
      if (!IsInsideAllocationRange(layout, *record_after)) {
        return ::testing::AssertionFailure()
               << "Selected room escaped its allocation range";
      }
      continue;
    }
    if (record_after->encoded_stream != record_before.encoded_stream) {
      return ::testing::AssertionFailure()
             << "Untouched room " << record_before.room_id
             << " changed semantically";
    }
    if (preserve_untouched_addresses &&
        record_after->data_pc != record_before.data_pc) {
      return ::testing::AssertionFailure()
             << "Untouched room " << record_before.room_id
             << " moved during copy-on-write";
    }
  }
  return ::testing::AssertionSuccess();
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

TEST_F(DungeonPotRepackOosIntegrationTest,
       ObjectSpritePotSaveRoomSurvivesFiftyReopenCycles) {
  auto object_layout =
      LoadManifestLayout(project_, core::DungeonStreamType::kObjects,
                         core::DungeonWriteStrategy::kCopyOnWrite);
  ASSERT_TRUE(object_layout.ok()) << object_layout.status().message();
  auto sprite_layout =
      LoadManifestLayout(project_, core::DungeonStreamType::kSprites,
                         core::DungeonWriteStrategy::kCopyOnWrite);
  ASSERT_TRUE(sprite_layout.ok()) << sprite_layout.status().message();
  ASSERT_EQ(object_layout->pointer_count, kOracleRoomCount);
  ASSERT_EQ(sprite_layout->pointer_count, kOracleRoomCount);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(temp_rom_path_.string()).ok());

  auto object_before = InventoryDungeonStreams(rom, *object_layout);
  ASSERT_TRUE(object_before.ok()) << object_before.status().message();
  ASSERT_TRUE(object_before->ok())
      << "Oracle object inventory contains stream issues";
  auto sprite_before = InventoryDungeonStreams(rom, *sprite_layout);
  ASSERT_TRUE(sprite_before.ok()) << sprite_before.status().message();
  ASSERT_TRUE(sprite_before->ok())
      << "Oracle sprite inventory contains stream issues";
  auto pot_before = InventoryDungeonStreams(rom, layout_);
  ASSERT_TRUE(pot_before.ok()) << pot_before.status().message();
  ASSERT_TRUE(pot_before->ok())
      << "Oracle pot inventory contains stream issues";
  ASSERT_EQ(object_before->streams.size(), kOracleRoomCount);
  ASSERT_EQ(sprite_before->streams.size(), kOracleRoomCount);
  ASSERT_EQ(pot_before->streams.size(), kOracleRoomCount);

  const DungeonStreamAliasGroup* object_empty =
      FindCanonicalEmptyAlias(*object_before, IsCanonicalEmptyObjectStream);
  const DungeonStreamAliasGroup* sprite_empty =
      FindCanonicalEmptyAlias(*sprite_before, IsCanonicalEmptySpriteStream);
  const DungeonStreamAliasGroup* pot_empty =
      FindCanonicalEmptyAlias(*pot_before, IsCanonicalEmptyPotStream);
  ASSERT_NE(object_empty, nullptr)
      << "Oracle ROM has no shared canonical-empty object stream";
  ASSERT_NE(sprite_empty, nullptr)
      << "Oracle ROM has no shared canonical-empty sprite stream";
  ASSERT_NE(pot_empty, nullptr)
      << "Oracle ROM has no shared canonical-empty pot stream";

  const std::vector<uint32_t> common_empty_rooms = IntersectRoomIds(
      object_empty->room_ids, sprite_empty->room_ids, pot_empty->room_ids);
  ASSERT_FALSE(common_empty_rooms.empty())
      << "Oracle ROM has no room shared by all three canonical-empty aliases";

  int selected_room = -1;
  DungeonValidator validator;
  for (uint32_t room_id : common_empty_rooms) {
    Room probe = LoadRoomFromRom(&rom, static_cast<int>(room_id));
    probe.LoadSprites();
    if (!probe.IsLoaded() || !probe.AreObjectsLoaded() ||
        !probe.AreSpritesLoaded() || !probe.ArePotItemsLoaded() ||
        !probe.GetSprites().empty() || !probe.GetPotItems().empty() ||
        !validator.ValidateRoom(probe).is_valid) {
      continue;
    }
    selected_room = static_cast<int>(room_id);
    break;
  }
  ASSERT_GE(selected_room, 0)
      << "No canonical-empty intersection room passed editor validation";
  RecordProperty("multidomain_room", absl::StrFormat("0x%03X", selected_room));

  DungeonSaveFlagsGuard flags_guard;
  ConfigureObjectSpritePotSave();

  auto dungeon_editor = std::make_unique<editor::DungeonEditorV2>(&rom);
  editor::EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.project = &project_;
  dungeon_editor->SetDependencies(dependencies);

  Room& edited_room = dungeon_editor->rooms()[selected_room];
  edited_room = LoadRoomFromRom(&rom, selected_room);
  edited_room.LoadSprites();
  ASSERT_TRUE(edited_room.IsLoaded());
  ASSERT_TRUE(edited_room.AreObjectsLoaded());
  ASSERT_TRUE(edited_room.AreSpritesLoaded());
  ASSERT_TRUE(edited_room.ArePotItemsLoaded());
  ASSERT_TRUE(edited_room.GetSprites().empty());
  ASSERT_TRUE(edited_room.GetPotItems().empty());

  std::vector<std::pair<uint8_t, uint8_t>> free_positions;
  for (int y = 8; y <= 24 && free_positions.size() < 2; y += 4) {
    for (int x = 8; x <= 24 && free_positions.size() < 2; x += 4) {
      if (!CoordinateIsOccupied(edited_room, x, y)) {
        free_positions.emplace_back(static_cast<uint8_t>(x),
                                    static_cast<uint8_t>(y));
      }
    }
  }
  ASSERT_EQ(free_positions.size(), 2u)
      << "Could not find two free editor coordinates in the selected room";

  RoomObject replacement_object(0x10, free_positions[0].first,
                                free_positions[0].second, 0, 0);
  replacement_object.SetRom(&rom);
  ASSERT_TRUE(edited_room.AddObject(replacement_object).ok());
  edited_room.GetSprites().emplace_back(0x10, free_positions[1].first,
                                        free_positions[1].second, 0, 0);
  edited_room.MarkSpritesDirty();

  std::vector<PotItem> replacement_items;
  std::vector<uint8_t> replacement_pot_stream;
  for (uint16_t x = 0x10; x < 0x80; ++x) {
    std::vector<PotItem> candidate{{static_cast<uint16_t>(0x1000 | x), 0x01}};
    if (CoordinateIsOccupied(edited_room, candidate[0].GetTileX(),
                             candidate[0].GetTileY())) {
      continue;
    }
    std::vector<uint8_t> encoded = EncodePotItems(candidate);
    const bool already_present =
        std::any_of(pot_before->streams.begin(), pot_before->streams.end(),
                    [&encoded](const DungeonStreamRecord& record) {
                      return record.encoded_stream == encoded;
                    });
    if (!already_present) {
      replacement_items = std::move(candidate);
      replacement_pot_stream = std::move(encoded);
      break;
    }
  }
  ASSERT_EQ(replacement_items.size(), 1u)
      << "Could not construct a unique valid one-item pot stream";
  edited_room.GetPotItems() = replacement_items;
  edited_room.MarkPotItemsDirty();
  ASSERT_TRUE(validator.ValidateRoom(edited_room).is_valid);

  const DungeonStreamRecord* object_before_selected =
      FindRoomRecord(*object_before, selected_room);
  const DungeonStreamRecord* sprite_before_selected =
      FindRoomRecord(*sprite_before, selected_room);
  const DungeonStreamRecord* pot_before_selected =
      FindRoomRecord(*pot_before, selected_room);
  ASSERT_NE(object_before_selected, nullptr);
  ASSERT_NE(sprite_before_selected, nullptr);
  ASSERT_NE(pot_before_selected, nullptr);
  ASSERT_GE(object_before_selected->encoded_stream.size(), 2u);
  ASSERT_GE(sprite_before_selected->encoded_stream.size(), 1u);

  const std::vector<uint8_t> replacement_object_payload =
      edited_room.EncodeObjects();
  std::vector<uint8_t> replacement_object_stream{
      object_before_selected->encoded_stream[0],
      object_before_selected->encoded_stream[1]};
  replacement_object_stream.insert(replacement_object_stream.end(),
                                   replacement_object_payload.begin(),
                                   replacement_object_payload.end());
  const std::vector<uint8_t> replacement_sprite_payload =
      edited_room.EncodeSprites();
  std::vector<uint8_t> replacement_sprite_stream{
      sprite_before_selected->encoded_stream[0]};
  replacement_sprite_stream.insert(replacement_sprite_stream.end(),
                                   replacement_sprite_payload.begin(),
                                   replacement_sprite_payload.end());

  const auto predicted_ranges = dungeon_editor->CollectWriteRanges();
  auto expect_range = [&predicted_ranges](uint32_t begin, uint32_t end,
                                          const char* label) {
    EXPECT_NE(std::find(predicted_ranges.begin(), predicted_ranges.end(),
                        std::pair<uint32_t, uint32_t>{begin, end}),
              predicted_ranges.end())
        << label;
  };
  for (const DungeonStreamPcRange& range : object_layout->allocation_ranges) {
    expect_range(range.begin, range.end,
                 "Missing object allocation range prediction");
  }
  expect_range(object_layout->pointer_table_pc + selected_room * 3u,
               object_layout->pointer_table_pc + selected_room * 3u + 3u,
               "Missing selected object pointer prediction");
  expect_range(kDoorPointers + selected_room * 3u,
               kDoorPointers + selected_room * 3u + 3u,
               "Missing selected door pointer prediction");
  for (const DungeonStreamPcRange& range : sprite_layout->allocation_ranges) {
    expect_range(range.begin, range.end,
                 "Missing sprite allocation range prediction");
  }
  expect_range(sprite_layout->pointer_table_pc + selected_room * 2u,
               sprite_layout->pointer_table_pc + selected_room * 2u + 2u,
               "Missing selected sprite pointer prediction");
  for (const DungeonStreamPcRange& range : layout_.allocation_ranges) {
    expect_range(range.begin, range.end,
                 "Missing pot allocation range prediction");
  }
  expect_range(layout_.pointer_table_pc,
               layout_.pointer_table_pc + layout_.pointer_count * 2u,
               "Missing complete pot pointer table prediction");

  const std::vector<uint8_t> before_save_bytes = rom.vector();
  const absl::Status save_status = dungeon_editor->SaveRoom(selected_room);
  ASSERT_TRUE(save_status.ok()) << save_status.message();
  EXPECT_FALSE(edited_room.object_stream_dirty());
  EXPECT_FALSE(edited_room.sprites_dirty());
  EXPECT_FALSE(edited_room.pot_items_dirty());
  ASSERT_EQ(rom.vector().size(), before_save_bytes.size());
  size_t uncovered_write_count = 0;
  size_t first_uncovered_write = 0;
  for (size_t offset = 0; offset < before_save_bytes.size(); ++offset) {
    if (before_save_bytes[offset] == rom.vector()[offset]) {
      continue;
    }
    const bool covered =
        std::any_of(predicted_ranges.begin(), predicted_ranges.end(),
                    [offset](const std::pair<uint32_t, uint32_t>& range) {
                      return range.first <= offset && offset < range.second;
                    });
    if (!covered) {
      if (uncovered_write_count == 0) {
        first_uncovered_write = offset;
      }
      ++uncovered_write_count;
    }
  }
  EXPECT_EQ(uncovered_write_count, 0u)
      << "First write outside CollectWriteRanges at PC 0x" << std::hex
      << first_uncovered_write;

  Rom::SaveSettings save_settings;
  save_settings.filename = temp_rom_path_.string();
  ASSERT_TRUE(rom.SaveToFile(save_settings).ok());
  const std::vector<uint8_t> first_save_bytes = ReadFile(temp_rom_path_);
  ASSERT_FALSE(first_save_bytes.empty());
  dungeon_editor.reset();

  {
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(temp_rom_path_.string()).ok());
    auto object_after = InventoryDungeonStreams(reopened, *object_layout);
    ASSERT_TRUE(object_after.ok()) << object_after.status().message();
    auto sprite_after = InventoryDungeonStreams(reopened, *sprite_layout);
    ASSERT_TRUE(sprite_after.ok()) << sprite_after.status().message();
    auto pot_after = InventoryDungeonStreams(reopened, layout_);
    ASSERT_TRUE(pot_after.ok()) << pot_after.status().message();

    EXPECT_TRUE(InventoryPreservesUntouchedRooms(
        *object_before, *object_after, *object_layout, selected_room,
        replacement_object_stream, /*preserve_untouched_addresses=*/true));
    EXPECT_TRUE(InventoryPreservesUntouchedRooms(
        *sprite_before, *sprite_after, *sprite_layout, selected_room,
        replacement_sprite_stream, /*preserve_untouched_addresses=*/true));
    EXPECT_TRUE(InventoryPreservesUntouchedRooms(
        *pot_before, *pot_after, layout_, selected_room, replacement_pot_stream,
        /*preserve_untouched_addresses=*/false));

    const DungeonStreamRecord* object_after_selected =
        FindRoomRecord(*object_after, selected_room);
    const DungeonStreamRecord* sprite_after_selected =
        FindRoomRecord(*sprite_after, selected_room);
    const DungeonStreamRecord* pot_after_selected =
        FindRoomRecord(*pot_after, selected_room);
    ASSERT_NE(object_after_selected, nullptr);
    ASSERT_NE(sprite_after_selected, nullptr);
    ASSERT_NE(pot_after_selected, nullptr);
    EXPECT_NE(object_after_selected->data_pc, object_before_selected->data_pc);
    EXPECT_NE(sprite_after_selected->data_pc, sprite_before_selected->data_pc);
    EXPECT_NE(pot_after_selected->data_pc, pot_before_selected->data_pc);

    for (const auto* alias : {object_empty, sprite_empty, pot_empty}) {
      const DungeonStreamInventory* after_inventory =
          alias == object_empty
              ? &*object_after
              : (alias == sprite_empty ? &*sprite_after : &*pot_after);
      const DungeonStreamRecord* selected_after =
          FindRoomRecord(*after_inventory, selected_room);
      ASSERT_NE(selected_after, nullptr);
      for (uint32_t alias_room : alias->room_ids) {
        if (alias_room == static_cast<uint32_t>(selected_room)) {
          continue;
        }
        const DungeonStreamRecord* alias_after =
            FindRoomRecord(*after_inventory, alias_room);
        ASSERT_NE(alias_after, nullptr);
        EXPECT_NE(alias_after->data_pc, selected_after->data_pc)
            << "Selected room remained attached to room " << alias_room;
      }
    }

    auto door_pointer = reopened.ReadLong(kDoorPointers + selected_room * 3u);
    ASSERT_TRUE(door_pointer.ok()) << door_pointer.status().message();
    EXPECT_EQ(
        SnesToPc(*door_pointer),
        object_after_selected->data_pc + replacement_object_stream.size() - 2u);

    Room reopened_room = LoadRoomFromRom(&reopened, selected_room);
    reopened_room.LoadSprites();
    std::vector<const RoomObject*> ordinary_objects;
    for (const RoomObject& object : reopened_room.GetTileObjects()) {
      if ((object.options() & ObjectOption::Torch) == ObjectOption::Nothing &&
          (object.options() & ObjectOption::Block) == ObjectOption::Nothing) {
        ordinary_objects.push_back(&object);
      }
    }
    ASSERT_EQ(ordinary_objects.size(), 1u);
    EXPECT_EQ(ordinary_objects[0]->id_, replacement_object.id_);
    EXPECT_EQ(ordinary_objects[0]->x(), replacement_object.x());
    EXPECT_EQ(ordinary_objects[0]->y(), replacement_object.y());
    EXPECT_EQ(ordinary_objects[0]->size(), replacement_object.size());
    EXPECT_EQ(ordinary_objects[0]->GetLayerValue(),
              replacement_object.GetLayerValue());
    EXPECT_TRUE(reopened_room.GetDoors().empty());

    ASSERT_EQ(reopened_room.GetSprites().size(), 1u);
    EXPECT_EQ(reopened_room.GetSprites()[0].id(), 0x10);
    EXPECT_EQ(reopened_room.GetSprites()[0].x(), free_positions[1].first);
    EXPECT_EQ(reopened_room.GetSprites()[0].y(), free_positions[1].second);
    EXPECT_EQ(reopened_room.GetSprites()[0].subtype(), 0);
    EXPECT_EQ(reopened_room.GetSprites()[0].layer(), 0);
    EXPECT_EQ(EncodePotItems(reopened_room.GetPotItems()),
              replacement_pot_stream);
  }

  for (int cycle = 1; cycle <= kSaveReopenCycles; ++cycle) {
    SCOPED_TRACE(absl::StrFormat("multi-domain save/reopen cycle %d", cycle));
    {
      Rom cycle_rom;
      ASSERT_TRUE(cycle_rom.LoadFromFile(temp_rom_path_.string()).ok());
      auto cycle_editor = std::make_unique<editor::DungeonEditorV2>(&cycle_rom);
      editor::EditorDependencies cycle_dependencies = dependencies;
      cycle_dependencies.rom = &cycle_rom;
      cycle_editor->SetDependencies(cycle_dependencies);

      Room& cycle_room = cycle_editor->rooms()[selected_room];
      cycle_room = LoadRoomFromRom(&cycle_rom, selected_room);
      cycle_room.LoadSprites();
      ASSERT_EQ(cycle_room.EncodeObjects(), replacement_object_payload);
      ASSERT_EQ(cycle_room.EncodeSprites(), replacement_sprite_payload);
      ASSERT_EQ(EncodePotItems(cycle_room.GetPotItems()),
                replacement_pot_stream);
      cycle_room.MarkObjectStreamDirty();
      cycle_room.MarkSpritesDirty();
      cycle_room.MarkPotItemsDirty();

      const absl::Status cycle_status = cycle_editor->SaveRoom(selected_room);
      ASSERT_TRUE(cycle_status.ok()) << cycle_status.message();
      EXPECT_FALSE(cycle_room.object_stream_dirty());
      EXPECT_FALSE(cycle_room.sprites_dirty());
      EXPECT_FALSE(cycle_room.pot_items_dirty());
      ASSERT_TRUE(cycle_rom.SaveToFile(save_settings).ok());
    }

    ASSERT_EQ(ReadFile(temp_rom_path_), first_save_bytes)
        << "Multi-domain save changed ROM bytes during cycle " << cycle;
  }
  RecordProperty("multidomain_save_reopen_cycles", kSaveReopenCycles);
}

}  // namespace
}  // namespace yaze::zelda3::test
