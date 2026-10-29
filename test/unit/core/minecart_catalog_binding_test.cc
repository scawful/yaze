#include "core/minecart_catalog_binding.h"

#include "gtest/gtest.h"

#if defined(YAZE_WITH_JSON)

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unique_temp_path.h"

namespace yaze::core {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using zelda3::MinecartRailProfile;

std::string Hash(const std::vector<uint8_t>& bytes) {
  return ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

std::string Hex(const std::array<uint8_t, 3>& bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : bytes) {
    result += digits[byte >> 4];
    result += digits[byte & 15];
  }
  return result;
}

void Write(const fs::path& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open()) << path;
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  ASSERT_TRUE(output.good()) << path;
}

std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open()) << path;
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

int SetAppData(const std::optional<std::string>& path) {
#if defined(_WIN32)
  return _putenv_s("YAZE_APP_DATA_DIR", path.value_or("").c_str());
#else
  return path.has_value() ? setenv("YAZE_APP_DATA_DIR", path->c_str(), 1)
                          : unsetenv("YAZE_APP_DATA_DIR");
#endif
}

class MinecartCatalogBindingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (const char* old = std::getenv("YAZE_APP_DATA_DIR"))
      old_app_data_ = old;
    directory_ = ::yaze::test::UniqueTempPath("yaze_minecart_catalog_binding");
    ASSERT_TRUE(fs::create_directories(directory_));
    directory_ = fs::canonical(directory_);
    ASSERT_EQ(SetAppData(directory_.string()), 0);
    project_.name = "Minecart catalog";
    project_.filepath = (directory_ / "catalog.yaze").string();
    project_.rom_filename = (directory_ / "base.sfc").string();
    project_.dungeon_overlay.minecart_catalog_version = 2;
    project_.minecart_catalog_migration_receipt =
        (directory_ / "migration.json").string();
    original_.assign(0x200000, 0);
    const auto store_pointer = [&](size_t pos, size_t pc) {
      const uint32_t snes = ((pc & 0x3F8000) << 1) | (pc & 0x7FFF) | 0x8000;
      for (int byte = 0; byte < 3; ++byte)
        original_[pos + byte] = snes >> (byte * 8);
    };
    store_pointer(0x874C, 0xF8000);
    Json records = Json::array();
    size_t ordinal = 0;
    for (int room = 0; room < 296; ++room) {
      size_t cursor = 0x50000 + room * 0x100;
      store_pointer(0xF8000 + room * 3, cursor);
      original_[cursor++] = 1;
      original_[cursor++] = 12;
      for (int layer = 0; layer < 3; ++layer) {
        // Spread the 37 placements over two rooms and all three stream lists.
        const int count = room == 0 ? 6 : room == 1 ? (layer == 0 ? 7 : 6) : 0;
        for (int index = 0; index < count; ++index) {
          const int subtype = 12 + ordinal % 4;
          const int target = subtype - 7;
          const int x = 2 + ordinal % 20;
          const int y = 8 + layer * 4;
          const std::array<uint8_t, 3> old = {
              static_cast<uint8_t>((x << 2) | (subtype >> 2)),
              static_cast<uint8_t>((y << 2) | (subtype & 3)), 0x31};
          const std::array<uint8_t, 3> next = {
              static_cast<uint8_t>((x << 2) | (target >> 2)),
              static_cast<uint8_t>((y << 2) | (target & 3)), 0x32};
          records.push_back({{"room", room},
                             {"layer", layer},
                             {"index", index},
                             {"pc", cursor},
                             {"raw", Hex(old)},
                             {"aliased_rooms", Json::array({room})},
                             {"old", Hex(old)},
                             {"new", Hex(next)},
                             {"x", x},
                             {"y", y},
                             {"old_object", 0x31},
                             {"old_subtype", subtype},
                             {"new_object", 0x32},
                             {"new_subtype", target}});
          for (const auto byte : old)
            original_[cursor++] = byte;
          ++ordinal;
        }
        if (room >= 2 && layer == 0) {
          for (const uint8_t byte : {0x20, 0x20, 0x05})
            original_[cursor++] = byte;
        }
        if (layer == 2) {
          for (const uint8_t byte : {0xF0, 0xFF, 0x81, 0x00})
            original_[cursor++] = byte;
        }
        original_[cursor++] = 0xFF;
        original_[cursor++] = 0xFF;
      }
    }
    ASSERT_EQ(ordinal, 37);
    migrated_ = original_;
    for (const auto& record : records) {
      const auto pos = record["pc"].get<size_t>();
      const auto old = record["old_subtype"].get<int>();
      const int target = old - 7;
      migrated_[pos] = (migrated_[pos] & 0xFC) | (target >> 2);
      migrated_[pos + 1] = (migrated_[pos + 1] & 0xFC) | (target & 3);
      migrated_[pos + 2] = 0x32;
    }
    migration_ = {{"schema", "oracle-minecart-object-migration/v1"},
                  {"mode", "published"},
                  {"state", "intermediate-old-runtime"},
                  {"editor_ready", false},
                  {"runtime_ready", false},
                  {"bytes", migrated_.size()},
                  {"rooms_parsed", 296},
                  {"record_count", 37},
                  {"changed_bytes", 111},
                  {"input_sha256", Hash(original_)},
                  {"output_sha256", Hash(migrated_)},
                  {"records", records}};
    WriteMigration();
    ASSERT_TRUE(rom_.LoadFromData(migrated_).ok());
    rom_.set_filename(project_.rom_filename);
  }

  void TearDown() override {
    EXPECT_EQ(SetAppData(old_app_data_), 0);
    std::error_code error;
    fs::remove_all(directory_, error);
  }

  void WriteMigration() {
    Write(project_.minecart_catalog_migration_receipt,
          migration_.dump(2) + "\n");
  }

  void PublishEditedSave(bool include_migration = true) {
    auto edited = migrated_;
    edited[0x100000] = 0x5A;  // Later authoring, outside the initial migration.
    ASSERT_TRUE(rom_.LoadFromData(edited).ok());
    rom_.set_filename(project_.rom_filename);
    Write(project_.rom_filename,
          std::string(reinterpret_cast<const char*>(edited.data()),
                      edited.size()));
    const auto descriptor = project_.SerializeToString();
    ASSERT_TRUE(descriptor.ok()) << descriptor.status();
    Write(project_.filepath, *descriptor);
    const auto producer_path = directory_ / "producer.bin";
    Write(producer_path, "qualified-test-producer");
    const auto context =
        GetNativeSaveContext(project_.rom_filename, project_.filepath);
    ASSERT_TRUE(context.ok()) << context.status();
    context_ = *context;
    NativeSaveReceiptRequest request;
    request.receipt_path = project_.filepath + ".native-save.json";
    request.operation_id = "catalog-save-1";
    request.operation = "save";
    request.context = context_;
    request.artifacts = {{"rom", project_.rom_filename, Hash(edited)},
                         {"project", project_.filepath,
                          ComputeSourceArtifactSha256(*descriptor)},
                         {"producer", producer_path,
                          ComputeSourceArtifactSha256(Read(producer_path))}};
    if (include_migration) {
      request.artifacts.push_back(
          {"dependency.minecart_catalog_migration",
           project_.minecart_catalog_migration_receipt,
           ComputeSourceArtifactSha256(
               Read(project_.minecart_catalog_migration_receipt))});
    }
    const auto receipt = PublishNativeSaveReceipt(request);
    ASSERT_TRUE(receipt.ok()) << receipt.status();
  }

  fs::path directory_;
  std::optional<std::string> old_app_data_;
  project::YazeProject project_;
  Rom rom_;
  std::vector<uint8_t> original_, migrated_;
  Json migration_;
  std::optional<NativeSaveContext> context_;
};

TEST_F(MinecartCatalogBindingTest,
       LegacyAndExactMarkerKeepTheirExplicitProfiles) {
  Rom neutral;
  project_.dungeon_overlay.minecart_catalog_version = 1;
  const auto empty = ResolveMinecartCatalogBinding(neutral, project_);
  ASSERT_TRUE(empty.ok()) << empty.status();
  EXPECT_TRUE(empty->validated);
  EXPECT_EQ(empty->profile, MinecartRailProfile::kLegacy);
  project_.minecart_catalog_migration_receipt.clear();
  auto legacy = ResolveMinecartCatalogBinding(rom_, project_);
  ASSERT_TRUE(legacy.ok()) << legacy.status();
  EXPECT_EQ(legacy->profile, MinecartRailProfile::kLegacy);
  auto bytes = migrated_;
  constexpr std::array<uint8_t, 8> marker = {'M', 'C', 'R', '2', 16, 13, 2, 0};
  std::copy(marker.begin(), marker.end(), bytes.begin() + 0x160017);
  ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
  auto marked = ResolveMinecartCatalogBinding(rom_, project_);
  ASSERT_TRUE(marked.ok()) << marked.status();
  EXPECT_EQ(marked->profile, MinecartRailProfile::kExpanded);
  bytes[0x16001C] = 5;
  ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       MarkedVersionTwoRetainsConfiguredImmutableDependency) {
  constexpr std::array<uint8_t, 8> marker = {'M', 'C', 'R', '2', 16, 13, 2, 0};
  std::copy(marker.begin(), marker.end(), rom_.mutable_data() + 0x160017);
  const auto binding = ResolveMinecartCatalogBinding(rom_, project_);
  ASSERT_TRUE(binding.ok()) << binding.status();
  EXPECT_TRUE(binding->validated);
  EXPECT_EQ(binding->profile, MinecartRailProfile::kExpanded);
  EXPECT_FALSE(binding->migration_receipt_path.empty());
  EXPECT_EQ(binding->migration_receipt_sha256,
            ComputeSourceArtifactSha256(
                Read(project_.minecart_catalog_migration_receipt)));
}

TEST_F(MinecartCatalogBindingTest,
       InitialMigrationReconstructsAllRoomsAndLayers) {
  const auto before = Read(project_.minecart_catalog_migration_receipt);
  const auto binding = ResolveMinecartCatalogBinding(rom_, project_);
  ASSERT_TRUE(binding.ok()) << binding.status();
  EXPECT_TRUE(binding->validated);
  EXPECT_EQ(binding->profile, MinecartRailProfile::kExpanded);
  EXPECT_EQ(binding->rom_sha256, Hash(migrated_));
  EXPECT_EQ(binding->native_save_generation, 0);
  EXPECT_EQ(binding->migration_receipt_sha256,
            ComputeSourceArtifactSha256(before));
  EXPECT_EQ(Read(project_.minecart_catalog_migration_receipt), before);
  EXPECT_EQ(rom_.vector(), migrated_);
}

TEST_F(MinecartCatalogBindingTest,
       VersionTwoNeverFallsBackWhenReceiptIsMissing) {
  project_.minecart_catalog_migration_receipt.clear();
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
  project_.dungeon_overlay.minecart_catalog_version = 3;
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       ChangedRomCannotReuseInitialReceiptWithoutNativeSave) {
  rom_.mutable_data()[0x100000] ^= 1;
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       InitialReceiptRejectsStaleAndPartialRecordProof) {
  const auto valid = migration_;
  migration_["input_sha256"] = std::string(64, '0');
  WriteMigration();
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
  migration_ = valid;
  migration_["records"].erase(0);
  WriteMigration();
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
  migration_ = valid;
  migration_["records"][1] = migration_["records"][0];
  WriteMigration();
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       ExhaustiveCensusRejectsUnreportedLegacyPlacement) {
  auto extra_original = original_;
  auto extra_migrated = migrated_;
  for (auto* bytes : {&extra_original, &extra_migrated}) {
    (*bytes)[0x50202] = 0x23;
    (*bytes)[0x50203] = 0x20;
    (*bytes)[0x50204] = 0x31;
  }
  migration_["input_sha256"] = Hash(extra_original);
  migration_["output_sha256"] = Hash(extra_migrated);
  WriteMigration();
  ASSERT_TRUE(rom_.LoadFromData(extra_migrated).ok());
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

#if !defined(__EMSCRIPTEN__)
TEST_F(MinecartCatalogBindingTest,
       LaterNativeSaveBindsCurrentEditedBaseAndImmutableReceipt) {
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave());
  const auto binding = ResolveMinecartCatalogBinding(rom_, project_, context_);
  ASSERT_TRUE(binding.ok()) << binding.status();
  EXPECT_EQ(binding->profile, MinecartRailProfile::kExpanded);
  EXPECT_EQ(binding->native_save_generation, 1);
}

TEST_F(MinecartCatalogBindingTest,
       FreshReopenAdmitsSavedDataAfterProcessContextRetirement) {
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave());
  ASSERT_TRUE(RetireNativeSaveContext(*context_, "session_changed").ok());
  const auto reopened = ResolveMinecartCatalogBinding(rom_, project_);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->native_save_generation, 1);
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_, context_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       NativeSaveWithoutMigrationDependencyCannotAdmitVersionTwo) {
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave(false));
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       NativeSaveRejectsChangedProducerProjectRomAndDependency) {
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave());
  for (const fs::path path :
       {directory_ / "producer.bin", fs::path(project_.filepath),
        fs::path(project_.rom_filename),
        fs::path(project_.minecart_catalog_migration_receipt)}) {
    SCOPED_TRACE(path.string());
    const auto before = Read(path);
    Write(path, before + " ");
    if (path == directory_ / "producer.bin") {
      EXPECT_TRUE(ResolveMinecartCatalogBinding(rom_, project_).ok());
    } else {
      EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
    }
    EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_, context_).ok());
    Write(path, before);
  }
}

TEST_F(
    MinecartCatalogBindingTest,
    RetiredProducerDoesNotPreventEditorReopenButCannotAuthorizeCurrentBuild) {
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave());
  ASSERT_TRUE(fs::remove(directory_ / "producer.bin"));
  const auto reopened = ResolveMinecartCatalogBinding(rom_, project_);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_TRUE(reopened->validated);
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_, context_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       DraftVersionChangeCannotLaunderSavedLegacyDescriptor) {
  project_.dungeon_overlay.minecart_catalog_version = 1;
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave());
  project_.dungeon_overlay.minecart_catalog_version = 2;
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_).ok());
}

TEST_F(MinecartCatalogBindingTest,
       ExplicitCurrentContextRejectsStaleGeneration) {
  ASSERT_NO_FATAL_FAILURE(PublishEditedSave());
  auto pointer = Json::parse(Read(context_->path));
  pointer["native_commit"]["generation"] = uint64_t{2};
  Write(context_->path, pointer.dump(2) + "\n");
  EXPECT_FALSE(ResolveMinecartCatalogBinding(rom_, project_, context_).ok());
}
#endif

}  // namespace
}  // namespace yaze::core
#endif
