#include "e2e/rom_dependent/editor_save_test_base.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/overworld/tile16/tile16_edit_session.h"
#include "app/gfx/resource/arena.h"
#include "core/source_artifact_publisher.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::test {
namespace {

enum class Tile16RomClass { kVanilla, kExpanded, kOracle };

std::string RomClassName(Tile16RomClass rom_class) {
  switch (rom_class) {
    case Tile16RomClass::kVanilla:
      return "Vanilla";
    case Tile16RomClass::kExpanded:
      return "Expanded";
    case Tile16RomClass::kOracle:
      return "Oracle";
  }
  return "Unknown";
}

absl::StatusOr<std::string> ReadFileBytes(const std::filesystem::path& path) {
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

std::array<uint16_t, 4> DefinitionWords(const gfx::Tile16& tile) {
  std::array<uint16_t, 4> words{};
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    words[quadrant] =
        gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(tile, quadrant));
  }
  return words;
}

gfx::Tile16 AlteredDefinition(const gfx::Tile16& tile) {
  gfx::Tile16 edited = tile;
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    auto& info = zelda3::MutableTile16QuadrantInfo(edited, quadrant);
    info.palette_ = (info.palette_ + quadrant + 1) % 8;
    info.horizontal_mirror_ = !info.horizontal_mirror_;
    info.vertical_mirror_ = !info.vertical_mirror_;
  }
  zelda3::SyncTile16TilesInfo(&edited);
  return edited;
}

// Role availability is a precondition. A present ROM that cannot load or whose
// layout does not match its selected role is a failure, never a skip.
class SaveTrustTile16Test
    : public EditorSaveTestBase,
      public ::testing::WithParamInterface<Tile16RomClass> {
 protected:
  void SetUp() override {
    if (GetParam() == Tile16RomClass::kOracle) {
      const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
      if (oracle) {
        source_path_ = oracle;
      }
    } else {
      source_path_ = TestRomManager::GetRomPath(
          GetParam() == Tile16RomClass::kVanilla ? RomRole::kVanilla
                                                 : RomRole::kExpanded);
    }
    if (source_path_.empty() || !std::filesystem::exists(source_path_)) {
      GTEST_SKIP() << RomClassName(GetParam())
                   << " fixture unavailable; Oracle requires YAZE_TEST_ROM_OOS";
    }
    source_path_ = std::filesystem::weakly_canonical(source_path_);
    if (GetParam() == Tile16RomClass::kExpanded) {
      const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
      if (oracle && std::filesystem::exists(oracle) &&
          source_path_ == std::filesystem::weakly_canonical(oracle)) {
        GTEST_SKIP() << "Expanded role aliases the Oracle fixture; a distinct "
                        "ZSCustomOverworld v3 fixture is unavailable";
      }
    }
    // Oracle fixtures must be released/captured ROMs, never the daily bundle.
    ASSERT_EQ(source_path_.string().find("Oracle-of-Secrets.yazeproj"),
              std::string::npos);
    const auto before_hash =
        core::ComputeSourceArtifactFileSha256(source_path_);
    ASSERT_TRUE(before_hash.ok()) << before_hash.status();
    source_hash_ = *before_hash;
    RecordProperty("rom_class", RomClassName(GetParam()));
    RecordProperty("source_rom_sha256", source_hash_);

    test_id_ = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_save_trust_tile16_" +
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

    const auto raw = ReadFileBytes(test_rom_path_);
    ASSERT_TRUE(raw.ok()) << raw.status();
    original_file_ = *raw;
    Rom::LoadOptions options;
    options.load_resource_labels = false;
    ASSERT_TRUE(rom_.LoadFromFile(test_rom_path_, options).ok());
    ASSERT_GE(original_file_.size(), rom_.size());
    copier_header_size_ = original_file_.size() - rom_.size();
    ASSERT_TRUE(copier_header_size_ == 0 || copier_header_size_ == 512);

    bool expanded = false;
    auto definitions = zelda3::ReadMap16Tiles(rom_, &expanded);
    ASSERT_TRUE(definitions.ok()) << definitions.status();
    expanded_ = expanded;
    if (GetParam() == Tile16RomClass::kVanilla) {
      ASSERT_FALSE(expanded_);
    } else {
      const auto marker = rom_.ReadByte(0x140145);
      ASSERT_TRUE(marker.ok()) << marker.status();
      ASSERT_EQ(*marker, 0x03) << "This matrix requires ZSCustomOverworld v3";
      ASSERT_TRUE(expanded_);
    }
    ASSERT_FALSE(definitions->empty());
    *overworld_.mutable_tiles16() = std::move(*definitions);
    ASSERT_EQ(overworld_.tiles16().size(),
              static_cast<size_t>(zelda3::kNumTile16Individual));
    initial_ = overworld_.tiles16()[0];
    edited_ = AlteredDefinition(initial_);
    ASSERT_NE(DefinitionWords(initial_), DefinitionWords(edited_));

    // Rendering fixtures support the production editor session's edit/Undo.
    // ROM definitions, serialization and reopen all use the real input ROM.
    std::vector<gfx::SnesColor> colors(256);
    gfx::SnesPalette palette(colors);
    atlas_.Create(128, 8192, 8, std::vector<uint8_t>(128 * 8192, 0));
    atlas_.SetPalette(palette);
    tilemap_.atlas.Create(128, 8192, 8, atlas_.vector());
    tilemap_.atlas.SetPalette(palette);
    graphics_.Create(128, 512, 8, std::vector<uint8_t>(128 * 512, 0));
    graphics_.SetPalette(palette);
    session_.BindDocument(overworld_.mutable_tiles16(), &history_, [] {});
    ASSERT_TRUE(session_.InitializeBitmaps(atlas_, graphics_, types_).ok());
    session_.set_palette(palette);
    rom_.set_dirty(false);
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
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

  absl::Status SerializeDefinitions() {
    return expanded_ ? overworld_.SaveMap16Expanded()
                     : overworld_.SaveMap16Tiles();
  }

  absl::Status SaveDefinitions() {
    RETURN_IF_ERROR(SerializeDefinitions());
    return SaveRomToFile(&rom_, test_rom_path_);
  }

  absl::StatusOr<gfx::Tile16> ReopenDefinition(int tile_id = 0) {
    Rom fresh;
    Rom::LoadOptions options;
    options.load_resource_labels = false;
    RETURN_IF_ERROR(fresh.LoadFromFile(test_rom_path_, options));
    bool expanded = false;
    ASSIGN_OR_RETURN(auto definitions,
                     zelda3::ReadMap16Tiles(fresh, &expanded));
    if (expanded != expanded_) {
      return absl::DataLossError("Fresh reader selected a different layout");
    }
    if (tile_id < 0 || static_cast<size_t>(tile_id) >= definitions.size()) {
      return absl::DataLossError("Fresh reader did not return the Tile16 id");
    }
    return definitions[tile_id];
  }

  int SavableCount() const {
    return expanded_ ? zelda3::NumberOfMap16Ex : zelda3::NumberOfMap16;
  }

  // Every changed byte must be in a declared writer region.
  ::testing::AssertionResult ChangedBytesInDeclaredRegions() {
    const auto after = ReadFileBytes(test_rom_path_);
    if (!after.ok()) {
      return ::testing::AssertionFailure() << after.status();
    }
    if (after->size() != original_file_.size()) {
      return ::testing::AssertionFailure() << "File size changed";
    }
    size_t changed_bytes = 0;
    for (size_t offset = 0; offset < after->size(); ++offset) {
      if ((*after)[offset] == original_file_[offset]) {
        continue;
      }
      ++changed_bytes;
      if (offset < copier_header_size_) {
        return ::testing::AssertionFailure() << "Copier header changed";
      }
      if (!InDeclaredWriterRegion(offset - copier_header_size_)) {
        return ::testing::AssertionFailure()
               << "Unexpected changed PC byte: " << std::hex
               << (offset - copier_header_size_);
      }
    }
    if (changed_bytes == 0) {
      return ::testing::AssertionFailure() << "Save changed no bytes";
    }
    return ::testing::AssertionSuccess();
  }

  bool InDeclaredWriterRegion(size_t pc) const {
    const size_t start =
        expanded_ ? zelda3::GetMap16TilesExpanded() : zelda3::kMap16Tiles;
    const size_t count =
        expanded_ ? zelda3::NumberOfMap16Ex : zelda3::NumberOfMap16;
    if (pc >= start && pc < start + count * 8) {
      return true;
    }
    if (!expanded_) {
      return false;
    }
    // SaveMap16Expanded's declared pointer operands, excluding opcodes.
    constexpr uint32_t kLongOperands[] = {
        0x008865, 0x0EDE4F, 0x0EDEE9, 0x1BBC2D, 0x1BBC4C, 0x1BBCC2, 0x1BBCCB,
        0x1BBEF6, 0x1BBF23, 0x1BC041, 0x1BC9B3, 0x1BC9BA, 0x1BC9C1, 0x1BC9C8,
        0x1BCA40, 0x1BCA47, 0x1BCA4E, 0x1BCA55, 0x02F457, 0x02F45E, 0x02F467,
        0x02F46E, 0x02F51F, 0x02F526, 0x02F52F, 0x02F536};
    for (const auto operand : kLongOperands) {
      const size_t address = SnesToPc(operand);
      if (pc >= address && pc < address + 3) {
        return true;
      }
    }
    constexpr uint32_t kShortOperands[] = {0x02FE1C, 0x02FE23, 0x02FE2C,
                                           0x02FE33};
    for (const auto operand : kShortOperands) {
      const size_t address = SnesToPc(operand);
      if (pc >= address && pc < address + 2) {
        return true;
      }
    }
    return pc == SnesToPc(0x02FD28) || pc == SnesToPc(0x02FD39);
  }

  std::filesystem::path source_path_, directory_;
  std::string source_hash_, original_file_;
  size_t copier_header_size_ = 0;
  bool expanded_ = false;
  Rom rom_;
  zelda3::Overworld overworld_{&rom_};
  editor::UndoManager history_;
  gfx::Tilemap tilemap_;
  gfx::Bitmap atlas_, graphics_;
  std::array<uint8_t, 0x200> types_{};
  editor::Tile16EditSession session_{&rom_, &tilemap_};
  gfx::Tile16 initial_, edited_;
};

TEST_P(SaveTrustTile16Test, I1NoOpSavePreservesFileBytes) {
  ASSERT_TRUE(SaveDefinitions().ok());
  const auto after = ReadFileBytes(test_rom_path_);
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_TRUE(*after == original_file_);
}

TEST_P(SaveTrustTile16Test, I2EditSaveFreshProductionReader) {
  ASSERT_TRUE(session_.ReplaceCurrentTile(edited_).ok());
  ASSERT_TRUE(SaveDefinitions().ok());
  const auto reopened = ReopenDefinition();
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(edited_));
}

TEST_P(SaveTrustTile16Test, I3EditUndoSavePreservesFileBytes) {
  ASSERT_TRUE(session_.ReplaceCurrentTile(edited_).ok());
  ASSERT_TRUE(history_.Undo().ok());
  ASSERT_TRUE(SaveDefinitions().ok());
  const auto after = ReadFileBytes(test_rom_path_);
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_TRUE(*after == original_file_);
  const auto reopened = ReopenDefinition();
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(initial_));
}

TEST_P(SaveTrustTile16Test, I4EditSaveContainsWritesToDeclaredRegions) {
  ASSERT_TRUE(session_.ReplaceCurrentTile(edited_).ok());
  ASSERT_TRUE(SaveDefinitions().ok());
  ASSERT_TRUE(ChangedBytesInDeclaredRegions());
  const auto reopened = ReopenDefinition();
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(edited_));
}

TEST_P(SaveTrustTile16Test, I5LastSavableDefinitionRoundTrips) {
  const int last = SavableCount() - 1;
  const gfx::Tile16 neighbor = overworld_.tiles16()[last - 1];
  const gfx::Tile16 edited = AlteredDefinition(overworld_.tiles16()[last]);
  ASSERT_TRUE(session_.SetCurrentTile(last).ok());
  ASSERT_TRUE(session_.ReplaceCurrentTile(edited).ok());
  ASSERT_TRUE(SaveDefinitions().ok());
  ASSERT_TRUE(ChangedBytesInDeclaredRegions());
  const auto reopened = ReopenDefinition(last);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(edited));
  const auto reopened_neighbor = ReopenDefinition(last - 1);
  ASSERT_TRUE(reopened_neighbor.ok()) << reopened_neighbor.status();
  EXPECT_EQ(DefinitionWords(*reopened_neighbor), DefinitionWords(neighbor));
}

// A vanilla save writes only NumberOfMap16 definitions, so an edit to the next
// id is dropped; expanded storage keeps it.
TEST_P(SaveTrustTile16Test, I6FirstIdPastVanillaTableFollowsLayout) {
  const int id = zelda3::NumberOfMap16;
  const gfx::Tile16 initial = overworld_.tiles16()[id];
  const gfx::Tile16 edited = AlteredDefinition(initial);
  (*overworld_.mutable_tiles16())[id] = edited;
  ASSERT_TRUE(SaveDefinitions().ok());
  const auto reopened = ReopenDefinition(id);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  if (expanded_) {
    ASSERT_TRUE(ChangedBytesInDeclaredRegions());
    EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(edited));
  } else {
    const auto after = ReadFileBytes(test_rom_path_);
    ASSERT_TRUE(after.ok()) << after.status();
    EXPECT_TRUE(*after == original_file_);
    EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(initial));
  }
}

TEST_P(SaveTrustTile16Test, ControlSkippingDiskSaveCannotObserveEdit) {
  ASSERT_TRUE(session_.ReplaceCurrentTile(edited_).ok());
  ASSERT_TRUE(SerializeDefinitions().ok());
  // This differs from I2 only by omitting SaveRomToFile. A fresh reader must
  // reject the edited-value expectation even though ROM serialization ran.
  const auto reopened = ReopenDefinition();
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_NE(DefinitionWords(*reopened), DefinitionWords(edited_));
  EXPECT_EQ(DefinitionWords(*reopened), DefinitionWords(initial_));
}

INSTANTIATE_TEST_SUITE_P(
    SaveTrustRomClasses, SaveTrustTile16Test,
    ::testing::Values(Tile16RomClass::kVanilla, Tile16RomClass::kExpanded,
                      Tile16RomClass::kOracle),
    [](const ::testing::TestParamInfo<Tile16RomClass>& info) {
      return RomClassName(info.param);
    });

}  // namespace
}  // namespace yaze::test
