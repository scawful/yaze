#include "e2e/rom_dependent/editor_save_test_base.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "app/editor/overworld/overworld_editor.h"
#include "core/source_artifact_publisher.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"

namespace yaze::test {
namespace {

enum class EntranceRomClass { kVanilla, kExpanded, kOracle };
enum class EntranceDomain { kEntrances, kExits, kHoles };
using EntranceCase = std::tuple<EntranceRomClass, EntranceDomain>;

std::string EntranceRoleName(EntranceRomClass role) {
  switch (role) {
    case EntranceRomClass::kVanilla:
      return "Vanilla";
    case EntranceRomClass::kExpanded:
      return "Expanded";
    case EntranceRomClass::kOracle:
      return "Oracle";
  }
  return "Unknown";
}

std::string EntranceDomainName(EntranceDomain domain) {
  switch (domain) {
    case EntranceDomain::kEntrances:
      return "Entrances";
    case EntranceDomain::kExits:
      return "Exits";
    case EntranceDomain::kHoles:
      return "Holes";
  }
  return "Unknown";
}

absl::StatusOr<std::string> ReadEntranceFile(
    const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return absl::NotFoundError("Cannot read test ROM: " + path.string());
  }
  std::string bytes((std::istreambuf_iterator<char>(stream)),
                    std::istreambuf_iterator<char>());
  if (stream.bad()) {
    return absl::DataLossError("Failed reading test ROM: " + path.string());
  }
  return bytes;
}

// Uses loaded source records and the production delete/Undo commands. No map
// geometry is invented: deletion does not require moving a record to a new map.
// Full map movement, camera derivation and gameplay are separate coverage.
class SaveTrustEntrancesTest
    : public EditorSaveTestBase,
      public ::testing::WithParamInterface<EntranceCase> {
 protected:
  EntranceRomClass Role() const { return std::get<0>(GetParam()); }
  EntranceDomain Domain() const { return std::get<1>(GetParam()); }

  void SetUp() override {
    if (Role() == EntranceRomClass::kOracle) {
      const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
      if (oracle) {
        source_path_ = oracle;
      }
    } else {
      source_path_ = TestRomManager::GetRomPath(
          Role() == EntranceRomClass::kVanilla ? RomRole::kVanilla
                                               : RomRole::kExpanded);
    }
    if (source_path_.empty() || !std::filesystem::exists(source_path_)) {
      GTEST_SKIP() << EntranceRoleName(Role())
                   << " fixture unavailable; Oracle requires YAZE_TEST_ROM_OOS";
    }
    source_path_ = std::filesystem::weakly_canonical(source_path_);
    if (Role() == EntranceRomClass::kExpanded) {
      const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
      if (oracle && std::filesystem::exists(oracle) &&
          source_path_ == std::filesystem::weakly_canonical(oracle)) {
        GTEST_SKIP() << "Expanded role aliases the Oracle fixture; a distinct "
                        "ZSCustomOverworld v3 fixture is unavailable";
      }
    }
    ASSERT_EQ(source_path_.string().find("Oracle-of-Secrets.yazeproj"),
              std::string::npos);
    const auto before_hash =
        core::ComputeSourceArtifactFileSha256(source_path_);
    ASSERT_TRUE(before_hash.ok()) << before_hash.status();
    source_hash_ = *before_hash;
    RecordProperty("rom_class", EntranceRoleName(Role()));
    RecordProperty("writer_domain", EntranceDomainName(Domain()));
    RecordProperty("source_rom_sha256", source_hash_);

    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_save_trust_entrances_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    test_rom_path_ = (directory_ / "edit.sfc").string();
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::copy_file(source_path_, test_rom_path_, ec))
        << ec.message();
    const auto after_hash = core::ComputeSourceArtifactFileSha256(source_path_);
    ASSERT_TRUE(after_hash.ok()) << after_hash.status();
    ASSERT_EQ(*after_hash, source_hash_);

    const auto raw = ReadEntranceFile(test_rom_path_);
    ASSERT_TRUE(raw.ok()) << raw.status();
    original_file_ = *raw;
    Rom::LoadOptions options;
    options.load_resource_labels = false;
    ASSERT_TRUE(rom_.LoadFromFile(test_rom_path_, options).ok());
    ASSERT_GE(original_file_.size(), rom_.size());
    copier_header_size_ = original_file_.size() - rom_.size();
    ASSERT_TRUE(copier_header_size_ == 0 || copier_header_size_ == 512);
    const auto profile = zelda3::DetectOverworldRomProfile(rom_);
    expanded_ = profile.has_expanded_entrances;
    if (Role() == EntranceRomClass::kVanilla) {
      ASSERT_TRUE(profile.is_vanilla);
      ASSERT_FALSE(expanded_);
    } else {
      const auto marker = rom_.ReadByte(0x140145);
      ASSERT_TRUE(marker.ok()) << marker.status();
      ASSERT_EQ(*marker, 0x03);
    }

    editor_ = std::make_unique<editor::OverworldEditor>(&rom_);
    auto entrances = zelda3::LoadEntrances(&rom_);
    auto holes = zelda3::LoadHoles(&rom_);
    auto exits = zelda3::LoadExits(&rom_);
    ASSERT_TRUE(entrances.ok()) << entrances.status();
    ASSERT_TRUE(holes.ok()) << holes.status();
    ASSERT_TRUE(exits.ok()) << exits.status();
    *editor_->overworld().mutable_entrances() = std::move(*entrances);
    *editor_->overworld().mutable_holes() = std::move(*holes);
    *editor_->overworld().mutable_exits() = std::move(*exits);
    rom_.set_dirty(false);
  }

  void TearDown() override {
    editor_.reset();
    if (!source_hash_.empty()) {
      const auto after = core::ComputeSourceArtifactFileSha256(source_path_);
      EXPECT_TRUE(after.ok()) << after.status();
      if (after.ok()) {
        EXPECT_EQ(*after, source_hash_) << "Read-only source ROM changed";
      }
    }
    if (!directory_.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(directory_, ec);
      EXPECT_FALSE(ec) << ec.message();
    }
  }

  std::vector<zelda3::OverworldEntrance>& EntranceRecords() {
    if (Domain() == EntranceDomain::kHoles && !expanded_) {
      return *editor_->overworld().mutable_holes();
    }
    return *editor_->overworld().mutable_entrances();
  }

  bool SelectLiveRow() {
    if (Domain() == EntranceDomain::kExits) {
      const auto& records = *editor_->overworld().mutable_exits();
      for (size_t row = 0; row < records.size(); ++row) {
        if (!records[row].deleted_) {
          row_ = row;
          return true;
        }
      }
    } else {
      const auto& records = EntranceRecords();
      const bool want_hole = Domain() == EntranceDomain::kHoles;
      for (size_t row = 0; row < records.size(); ++row) {
        if (!records[row].deleted && records[row].is_hole_ == want_hole) {
          row_ = row;
          return true;
        }
      }
    }
    return false;
  }

  absl::Status DeleteSelectedRow() {
    if (Domain() == EntranceDomain::kExits) {
      auto& record = editor_->overworld().mutable_exits()->at(row_);
      auto draft = record;
      draft.deleted_ = true;
      return editor_->ApplyExitEdit(&record, draft);
    }
    auto& record = EntranceRecords().at(row_);
    auto draft = record;
    draft.deleted = true;
    return editor_->ApplyEntranceEdit(&record, draft);
  }

  absl::Status Serialize() {
    return Domain() == EntranceDomain::kExits
               ? editor_->overworld().SaveExits()
               : editor_->overworld().SaveEntrances();
  }

  absl::Status Save() {
    RETURN_IF_ERROR(Serialize());
    return SaveRomToFile(&rom_, test_rom_path_);
  }

  absl::StatusOr<bool> ReopenedRowIsDeleted() {
    Rom fresh;
    Rom::LoadOptions options;
    options.load_resource_labels = false;
    RETURN_IF_ERROR(fresh.LoadFromFile(test_rom_path_, options));
    if (Domain() == EntranceDomain::kExits) {
      ASSIGN_OR_RETURN(auto records, zelda3::LoadExits(&fresh));
      if (row_ >= records.size()) {
        return absl::DataLossError("Exit row disappeared");
      }
      return records[row_].deleted_ && records[row_].room_id_ == 0xFFFF &&
             records[row_].x_player_ == 0xFFFF &&
             records[row_].y_player_ == 0xFFFF;
    }
    ASSIGN_OR_RETURN(auto records,
                     Domain() == EntranceDomain::kHoles && !expanded_
                         ? zelda3::LoadHoles(&fresh)
                         : zelda3::LoadEntrances(&fresh));
    if (row_ >= records.size()) {
      return absl::DataLossError("Entrance row disappeared");
    }
    return records[row_].deleted;
  }

  bool InDeclaredWriterRegion(size_t pc) const {
    const auto in_range = [pc](size_t start, size_t size) {
      return pc >= start && pc < start + size;
    };
    if (Domain() == EntranceDomain::kExits) {
      constexpr size_t kWordTables[] = {
          zelda3::OWExitRoomId,    zelda3::OWExitVram,
          zelda3::OWExitYScroll,   zelda3::OWExitXScroll,
          zelda3::OWExitYPlayer,   zelda3::OWExitXPlayer,
          zelda3::OWExitYCamera,   zelda3::OWExitXCamera,
          zelda3::OWExitDoorType1, zelda3::OWExitDoorType2};
      for (const auto table : kWordTables) {
        if (in_range(table, zelda3::kNumOverworldExits * 2)) {
          return true;
        }
      }
      return in_range(zelda3::OWExitMapId, zelda3::kNumOverworldExits) ||
             in_range(zelda3::OWExitUnk1, zelda3::kNumOverworldExits) ||
             in_range(zelda3::OWExitUnk2, zelda3::kNumOverworldExits) ||
             pc == zelda3::OWExitDoorPosition ||
             pc == zelda3::OWExitDoorPosition + 2 ||
             pc == zelda3::OWExitDoorPosition + 4;
    }
    if (expanded_) {
      return in_range(zelda3::GetOverworldEntranceMapExpanded(), 512) ||
             in_range(zelda3::GetOverworldEntrancePosExpanded(), 512) ||
             in_range(zelda3::GetOverworldEntranceIdExpanded(), 256);
    }
    return in_range(zelda3::kOverworldEntranceMap,
                    zelda3::kNumOverworldEntrances * 2) ||
           in_range(zelda3::kOverworldEntrancePos,
                    zelda3::kNumOverworldEntrances * 2) ||
           in_range(zelda3::kOverworldEntranceEntranceId,
                    zelda3::kNumOverworldEntrances) ||
           in_range(zelda3::kOverworldHolePos,
                    zelda3::kNumOverworldHoles * 2) ||
           in_range(zelda3::kOverworldHoleArea,
                    zelda3::kNumOverworldHoles * 2) ||
           in_range(zelda3::kOverworldHoleEntrance, zelda3::kNumOverworldHoles);
  }

  void ExpectContainedFileChanges() {
    const auto after = ReadEntranceFile(test_rom_path_);
    ASSERT_TRUE(after.ok()) << after.status();
    ASSERT_EQ(after->size(), original_file_.size());
    size_t changed_bytes = 0;
    for (size_t offset = 0; offset < after->size(); ++offset) {
      if ((*after)[offset] == original_file_[offset]) {
        continue;
      }
      ++changed_bytes;
      ASSERT_GE(offset, copier_header_size_) << "Copier header changed";
      ASSERT_TRUE(InDeclaredWriterRegion(offset - copier_header_size_))
          << "Unexpected changed PC byte: " << std::hex
          << (offset - copier_header_size_);
    }
    EXPECT_GT(changed_bytes, 0u);
  }

  std::filesystem::path source_path_, directory_;
  std::string source_hash_, original_file_;
  size_t copier_header_size_ = 0;
  size_t row_ = 0;
  bool expanded_ = false;
  Rom rom_;
  std::unique_ptr<editor::OverworldEditor> editor_;
};

TEST_P(SaveTrustEntrancesTest, I1NoOpSavePreservesFileBytes) {
  ASSERT_TRUE(Save().ok());
  const auto after = ReadEntranceFile(test_rom_path_);
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_TRUE(*after == original_file_)
      << "No-op " << EntranceDomainName(Domain()) << " Save changed file bytes";
}

TEST_P(SaveTrustEntrancesTest, I2DeleteSaveFreshProductionReader) {
  if (!SelectLiveRow()) {
    GTEST_SKIP() << "Fixture has no active " << EntranceDomainName(Domain())
                 << " record for this lifecycle";
  }
  ASSERT_TRUE(DeleteSelectedRow().ok());
  ASSERT_TRUE(Save().ok());
  const auto deleted = ReopenedRowIsDeleted();
  ASSERT_TRUE(deleted.ok()) << deleted.status();
  EXPECT_TRUE(*deleted);
}

TEST_P(SaveTrustEntrancesTest, I3DeleteUndoSavePreservesFileBytes) {
  if (!SelectLiveRow()) {
    GTEST_SKIP() << "Fixture has no active " << EntranceDomainName(Domain())
                 << " record for this lifecycle";
  }
  ASSERT_TRUE(DeleteSelectedRow().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(Save().ok());
  const auto after = ReadEntranceFile(test_rom_path_);
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_TRUE(*after == original_file_) << "Edit/Undo/Save changed file bytes";
  const auto deleted = ReopenedRowIsDeleted();
  ASSERT_TRUE(deleted.ok()) << deleted.status();
  EXPECT_FALSE(*deleted);
}

TEST_P(SaveTrustEntrancesTest, I4DeleteSaveContainsWritesToDeclaredRegions) {
  if (!SelectLiveRow()) {
    GTEST_SKIP() << "Fixture has no active " << EntranceDomainName(Domain())
                 << " record for this lifecycle";
  }
  ASSERT_TRUE(DeleteSelectedRow().ok());
  ASSERT_TRUE(Save().ok());
  ExpectContainedFileChanges();
  const auto deleted = ReopenedRowIsDeleted();
  ASSERT_TRUE(deleted.ok()) << deleted.status();
  EXPECT_TRUE(*deleted);
}

TEST_P(SaveTrustEntrancesTest, ControlSkippingDiskSaveCannotObserveDeletion) {
  if (!SelectLiveRow()) {
    GTEST_SKIP() << "Fixture has no active " << EntranceDomainName(Domain())
                 << " record for this lifecycle";
  }
  ASSERT_TRUE(DeleteSelectedRow().ok());
  ASSERT_TRUE(Serialize().ok());
  // Same edited document and production serialization as I2, but omit only
  // SaveRomToFile. The fresh-file deleted-value expectation must fail.
  const auto deleted = ReopenedRowIsDeleted();
  ASSERT_TRUE(deleted.ok()) << deleted.status();
  EXPECT_FALSE(*deleted);
}

TEST_P(SaveTrustEntrancesTest, ExpandedIdsSurviveLegacyHoleOverlapOnDisk) {
  if (Domain() != EntranceDomain::kEntrances || !expanded_) {
    GTEST_SKIP() << "Expanded combined entrance table regression";
  }
  auto& records = *editor_->overworld().mutable_entrances();
  ASSERT_EQ(records.size(), 256u);
  // PC $DB800..$DB85E is entrance-ID rows161..255. These bytes are
  // overlapping legacy hole storage, not an independent writable table.
  ASSERT_EQ(zelda3::GetOverworldEntranceIdExpanded(), 0xDB75F);
  std::vector<uint8_t> expected;
  for (size_t row = 161; row < 256; ++row) {
    records[row].entrance_id_ ^= 0x5A;
    expected.push_back(records[row].entrance_id_);
  }
  ASSERT_TRUE(Save().ok());
  ASSERT_TRUE(Save().ok()) << "Repeated Save must retain overlap protection";
  Rom fresh;
  Rom::LoadOptions options;
  options.load_resource_labels = false;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_, options).ok());
  const auto loaded = zelda3::LoadEntrances(&fresh);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->size(), 256u);
  for (size_t row = 161; row < 256; ++row) {
    EXPECT_EQ(loaded->at(row).entrance_id_, expected[row - 161]) << row;
  }
  ExpectContainedFileChanges();
}

TEST_P(SaveTrustEntrancesTest,
       ExpandedEditedLegacyHolesRejectBeforeAnyRomOrDiskWrite) {
  if (Domain() != EntranceDomain::kEntrances || !expanded_) {
    GTEST_SKIP() << "Expanded combined entrance table regression";
  }
  auto& records = *editor_->overworld().mutable_entrances();
  auto& holes = *editor_->overworld().mutable_holes();
  ASSERT_FALSE(records.empty());
  ASSERT_FALSE(holes.empty());
  records[0].entrance_id_ ^= 1;
  holes[0].entrance_id_ ^= 1;
  const auto before_memory = rom_.vector();
  const auto status = editor_->overworld().SaveEntrances();
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(status.message()).find("Legacy holes were edited"),
            std::string::npos);
  EXPECT_TRUE(rom_.vector() == before_memory);
  const auto disk = ReadEntranceFile(test_rom_path_);
  ASSERT_TRUE(disk.ok()) << disk.status();
  EXPECT_TRUE(*disk == original_file_);
}

INSTANTIATE_TEST_SUITE_P(
    SaveTrustRomClasses, SaveTrustEntrancesTest,
    ::testing::Combine(::testing::Values(EntranceRomClass::kVanilla,
                                         EntranceRomClass::kExpanded,
                                         EntranceRomClass::kOracle),
                       ::testing::Values(EntranceDomain::kEntrances,
                                         EntranceDomain::kExits,
                                         EntranceDomain::kHoles)),
    [](const ::testing::TestParamInfo<EntranceCase>& info) {
      return EntranceRoleName(std::get<0>(info.param)) +
             EntranceDomainName(std::get<1>(info.param));
    });

}  // namespace
}  // namespace yaze::test
