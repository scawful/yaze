#include "e2e/rom_dependent/editor_save_test_base.h"

#include <array>
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

#include "app/editor/message/message_data.h"
#include "app/gfx/util/palette_manager.h"
#include "core/source_artifact_publisher.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_sprite_io.h"
#include "zelda3/palette_constants.h"

namespace yaze::test {
namespace {

enum class ResourceRomClass { kVanilla, kExpanded, kOracle };
enum class ResourceDomain {
  kVanillaMessages,
  kExpandedMessages,
  kOverworldSprites,
  kOverworldItems,
  kOverworldMainPalette
};
using ResourceCase = std::tuple<ResourceRomClass, ResourceDomain>;

std::string ResourceRoleName(ResourceRomClass role) {
  switch (role) {
    case ResourceRomClass::kVanilla:
      return "Vanilla";
    case ResourceRomClass::kExpanded:
      return "Expanded";
    case ResourceRomClass::kOracle:
      return "Oracle";
  }
  return "Unknown";
}

std::string ResourceDomainName(ResourceDomain domain) {
  switch (domain) {
    case ResourceDomain::kVanillaMessages:
      return "VanillaMessageCodec";
    case ResourceDomain::kExpandedMessages:
      return "ExpandedMessageCodec";
    case ResourceDomain::kOverworldSprites:
      return "OverworldSpriteBatch";
    case ResourceDomain::kOverworldItems:
      return "OverworldItems";
    case ResourceDomain::kOverworldMainPalette:
      return "OverworldMainPalette";
  }
  return "Unknown";
}

absl::StatusOr<std::string> ReadResourceFile(
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

using ItemState = std::vector<std::array<int, 4>>;

ItemState SerializedItems(const std::vector<zelda3::OverworldItem>& items) {
  ItemState state;
  for (const auto& item : items) {
    if (!item.deleted) {
      state.push_back(
          {item.room_map_id_, item.game_x_, item.game_y_, item.id_});
    }
  }
  return state;
}

using MessageState = std::vector<std::vector<uint8_t>>;

MessageState EncodedMessages(const std::vector<editor::MessageData>& messages) {
  MessageState state;
  for (const auto& message : messages) {
    state.push_back(message.Data);
  }
  return state;
}

// Coverage is deliberately named by its actual writer. Message cases call the
// codecs, not MessageEditor project publication. Sprite cases call the batch
// planner/application used by Overworld::SaveSprites. Palette coverage is one
// ow_main color, not every palette group. All readback uses a fresh disk Rom.
class SaveTrustResourcesTest
    : public EditorSaveTestBase,
      public ::testing::WithParamInterface<ResourceCase> {
 protected:
  ResourceRomClass Role() const { return std::get<0>(GetParam()); }
  ResourceDomain Domain() const { return std::get<1>(GetParam()); }

  void SetUp() override {
    if (Domain() == ResourceDomain::kExpandedMessages &&
        Role() != ResourceRomClass::kOracle) {
      GTEST_SKIP() << "No expanded message bank is bound for this ROM class";
    }
    if (Role() == ResourceRomClass::kOracle) {
      const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
      if (oracle) {
        source_path_ = oracle;
      }
    } else {
      source_path_ = TestRomManager::GetRomPath(
          Role() == ResourceRomClass::kVanilla ? RomRole::kVanilla
                                               : RomRole::kExpanded);
    }
    if (source_path_.empty() || !std::filesystem::exists(source_path_)) {
      GTEST_SKIP() << ResourceRoleName(Role())
                   << " fixture unavailable; Oracle requires YAZE_TEST_ROM_OOS";
    }
    source_path_ = std::filesystem::weakly_canonical(source_path_);
    if (Role() == ResourceRomClass::kExpanded) {
      const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
      if (oracle && std::filesystem::exists(oracle) &&
          source_path_ == std::filesystem::weakly_canonical(oracle)) {
        GTEST_SKIP() << "Expanded role aliases the Oracle fixture";
      }
    }
    ASSERT_EQ(source_path_.string().find("Oracle-of-Secrets.yazeproj"),
              std::string::npos);
    const auto source_hash =
        core::ComputeSourceArtifactFileSha256(source_path_);
    ASSERT_TRUE(source_hash.ok()) << source_hash.status();
    source_hash_ = *source_hash;
    RecordProperty("rom_class", ResourceRoleName(Role()));
    RecordProperty("writer_domain", ResourceDomainName(Domain()));
    RecordProperty("source_rom_sha256", source_hash_);

    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_save_trust_resources_" +
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
    const auto bytes = ReadResourceFile(test_rom_path_);
    ASSERT_TRUE(bytes.ok()) << bytes.status();
    original_file_ = *bytes;

    Rom::LoadOptions options;
    options.load_resource_labels = false;
    ASSERT_TRUE(rom_.LoadFromFile(test_rom_path_, options).ok());
    ASSERT_GE(original_file_.size(), rom_.size());
    copier_header_size_ = original_file_.size() - rom_.size();
    ASSERT_TRUE(copier_header_size_ == 0 || copier_header_size_ == 512);
    const auto version = zelda3::OverworldVersionHelper::GetVersion(rom_);
    if (Role() == ResourceRomClass::kVanilla) {
      ASSERT_EQ(version, zelda3::OverworldVersion::kVanilla);
    } else {
      const auto marker = rom_.ReadByte(0x140145);
      ASSERT_TRUE(marker.ok()) << marker.status();
      ASSERT_EQ(*marker, 0x03);
    }

    const auto initialized = InitializeDomain();
    ASSERT_TRUE(initialized.ok()) << initialized;
    rom_.set_dirty(false);
  }

  void TearDown() override {
    if (palette_initialized_) {
      gfx::PaletteManager::Get().ReleaseSession(&game_data_);
    }
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

  absl::Status InitializeDomain() {
    if (Domain() == ResourceDomain::kVanillaMessages) {
      messages_ =
          editor::ReadAllTextData(rom_.mutable_data(), editor::kTextData,
                                  static_cast<int>(rom_.size()));
      return absl::OkStatus();
    }
    if (Domain() == ResourceDomain::kExpandedMessages) {
      expanded_start_ = editor::GetExpandedTextDataStart();
      expanded_end_ = editor::GetExpandedTextDataEnd();
      if (expanded_start_ < 0 || expanded_end_ < expanded_start_ ||
          static_cast<size_t>(expanded_end_) >= rom_.size()) {
        return absl::FailedPreconditionError(
            "Oracle expanded message region is not present");
      }
      messages_ = editor::ReadExpandedTextData(rom_.mutable_data(),
                                               expanded_start_, expanded_end_);
      for (const auto& message : messages_) {
        expanded_texts_.push_back(message.RawString);
      }
      return absl::OkStatus();
    }
    if (Domain() == ResourceDomain::kOverworldSprites) {
      const auto layout = zelda3::GetOverworldSpriteLayout(rom_);
      for (int state = 0; state < 3; ++state) {
        for (int map = 0; map < layout.counts[state]; ++map) {
          ASSIGN_OR_RETURN(auto bytes,
                           zelda3::ReadOverworldSpriteList(
                               rom_, layout.tables[state] + map * 2));
          sprite_edits_[state][map] = std::move(bytes);
        }
      }
      return absl::OkStatus();
    }
    if (Domain() == ResourceDomain::kOverworldItems) {
      overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
      auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
          overworld_->overworld_maps());
      // Constructors read actual parent/area metadata; no map geometry is
      // synthesized and no terrain, graphics or renderer is initialized.
      for (int map = 0; map < zelda3::kNumOverworldMaps; ++map) {
        maps.emplace_back(map, &rom_);
      }
      ASSIGN_OR_RETURN(auto items, zelda3::LoadItems(&rom_, maps));
      *overworld_->mutable_all_items() = std::move(items);
      return absl::OkStatus();
    }
    game_data_.set_rom(&rom_);
    RETURN_IF_ERROR(zelda3::LoadPalettes(rom_, game_data_));
    auto& manager = gfx::PaletteManager::Get();
    manager.Initialize(&game_data_);
    palette_initialized_ = true;
    if (!manager.IsManaging(&game_data_)) {
      return absl::FailedPreconditionError(
          "Palette session did not initialize");
    }
    initial_color_ = manager.GetColor("ow_main", 0, 0).snes();
    expected_color_ = (initial_color_ ^ 1) & 0x7FFF;
    return absl::OkStatus();
  }

  bool HasEditableRecord() {
    if (Domain() == ResourceDomain::kVanillaMessages) {
      for (size_t index = 0; index < messages_.size(); ++index) {
        if (!messages_[index].Data.empty() && messages_[index].Data[0] < 0x67) {
          selected_message_ = index;
          return true;
        }
      }
      return false;
    }
    if (Domain() == ResourceDomain::kExpandedMessages) {
      return !expanded_texts_.empty();
    }
    if (Domain() == ResourceDomain::kOverworldSprites) {
      const auto layout = zelda3::GetOverworldSpriteLayout(rom_);
      for (int state = 0; state < 3; ++state) {
        for (int map = 0; map < layout.counts[state]; ++map) {
          if (sprite_edits_[state][map]->size() >= 4) {
            sprite_state_ = state;
            sprite_map_ = map;
            return true;
          }
        }
      }
      return false;
    }
    if (Domain() == ResourceDomain::kOverworldItems) {
      return !overworld_->mutable_all_items()->empty();
    }
    return true;
  }

  absl::Status Edit() {
    if (Domain() == ResourceDomain::kVanillaMessages) {
      auto& first = messages_[selected_message_].Data[0];
      first = first == 0x66 ? 0x65 : first ^ 1;
    } else if (Domain() == ResourceDomain::kExpandedMessages) {
      expanded_texts_[0] = expanded_texts_[0] == "A" ? "B" : "A";
    } else if (Domain() == ResourceDomain::kOverworldSprites) {
      (*sprite_edits_[sprite_state_][sprite_map_])[2] ^= 1;
    } else if (Domain() == ResourceDomain::kOverworldItems) {
      overworld_->mutable_all_items()->at(0).id_ ^= 1;
    } else {
      return gfx::PaletteManager::Get().SetColor(
          "ow_main", 0, 0, gfx::SnesColor(expected_color_));
    }
    return absl::OkStatus();
  }

  absl::Status Serialize() {
    if (Domain() == ResourceDomain::kVanillaMessages) {
      return editor::WriteAllTextData(&rom_, messages_);
    }
    if (Domain() == ResourceDomain::kExpandedMessages) {
      return editor::WriteExpandedTextData(&rom_, expanded_start_,
                                           expanded_end_, expanded_texts_);
    }
    if (Domain() == ResourceDomain::kOverworldSprites) {
      ASSIGN_OR_RETURN(auto plan,
                       zelda3::PlanOverworldSpriteSave(rom_, sprite_edits_));
      return zelda3::ApplyOverworldSpriteSave(rom_, plan);
    }
    if (Domain() == ResourceDomain::kOverworldItems) {
      return overworld_->SaveItems();
    }
    return gfx::PaletteManager::Get().SaveAllToRom();
  }

  absl::Status Save() {
    RETURN_IF_ERROR(Serialize());
    return SaveRomToFile(&rom_, test_rom_path_);
  }

  absl::StatusOr<bool> FreshFileMatchesEditedDocument() {
    Rom fresh;
    Rom::LoadOptions options;
    options.load_resource_labels = false;
    RETURN_IF_ERROR(fresh.LoadFromFile(test_rom_path_, options));
    if (Domain() == ResourceDomain::kVanillaMessages) {
      const auto loaded =
          editor::ReadAllTextData(fresh.mutable_data(), editor::kTextData,
                                  static_cast<int>(fresh.size()));
      return EncodedMessages(loaded) == EncodedMessages(messages_);
    }
    if (Domain() == ResourceDomain::kExpandedMessages) {
      const auto loaded = editor::ReadExpandedTextData(
          fresh.mutable_data(), expanded_start_, expanded_end_);
      MessageState expected;
      for (const auto& text : expanded_texts_) {
        expected.push_back(editor::ParseMessageToData(text));
      }
      return EncodedMessages(loaded) == expected;
    }
    if (Domain() == ResourceDomain::kOverworldSprites) {
      const auto layout = zelda3::GetOverworldSpriteLayout(fresh);
      for (int state = 0; state < 3; ++state) {
        for (int map = 0; map < layout.counts[state]; ++map) {
          ASSIGN_OR_RETURN(auto loaded,
                           zelda3::ReadOverworldSpriteList(
                               fresh, layout.tables[state] + map * 2));
          if (loaded != *sprite_edits_[state][map]) {
            return false;
          }
        }
      }
      return true;
    }
    if (Domain() == ResourceDomain::kOverworldItems) {
      std::vector<zelda3::OverworldMap> maps;
      for (int map = 0; map < zelda3::kNumOverworldMaps; ++map) {
        maps.emplace_back(map, &fresh);
      }
      ASSIGN_OR_RETURN(auto loaded, zelda3::LoadItems(&fresh, maps));
      return SerializedItems(loaded) ==
             SerializedItems(*overworld_->mutable_all_items());
    }
    zelda3::GameData data(&fresh);
    RETURN_IF_ERROR(zelda3::LoadPalettes(fresh, data));
    const auto& palette = data.palette_groups.overworld_main.palette_ref(0);
    if (palette.size() == 0) {
      return absl::DataLossError("Fresh ow_main palette is empty");
    }
    return palette[0].snes() == expected_color_;
  }

  bool InDeclaredWriterRegion(size_t pc) const {
    const auto in_range = [pc](size_t start, size_t size) {
      return pc >= start && pc < start + size;
    };
    if (Domain() == ResourceDomain::kVanillaMessages) {
      return in_range(editor::kTextData,
                      editor::kTextDataEnd - editor::kTextData + 1) ||
             in_range(editor::kTextData2,
                      editor::kTextData2End - editor::kTextData2 + 1);
    }
    if (Domain() == ResourceDomain::kExpandedMessages) {
      return in_range(expanded_start_, expanded_end_ - expanded_start_ + 1);
    }
    if (Domain() == ResourceDomain::kOverworldSprites) {
      const auto layout = zelda3::GetOverworldSpriteLayout(rom_);
      for (int state = 0; state < 3; ++state) {
        if (in_range(layout.tables[state], layout.counts[state] * 2)) {
          return true;
        }
      }
      return in_range(
          layout.data_start,
          zelda3::GetOverworldSpriteRegionEnd(rom_) - layout.data_start);
    }
    if (Domain() == ResourceDomain::kOverworldItems) {
      const int count = zelda3::OverworldVersionHelper::SupportsAreaEnum(
                            zelda3::OverworldVersionHelper::GetVersion(rom_))
                            ? zelda3::kNumOverworldMaps
                            : zelda3::kNumOverworldMapItemPointers;
      return in_range(zelda3::kOverworldBombDoorItemLocationsNew, count * 2) ||
             in_range(zelda3::kOverworldItemsPointersNew, count * 2) ||
             in_range(zelda3::kOverworldItemsStartDataNew,
                      zelda3::kOverworldItemsEndData -
                          zelda3::kOverworldItemsStartDataNew) ||
             in_range(zelda3::overworldItemsAddress, 3) ||
             pc == zelda3::overworldItemsAddressBank;
    }
    // Only this one color was edited. No other palette row may change.
    return in_range(zelda3::PaletteAddress::kOverworldMain, 2);
  }

  std::filesystem::path source_path_, directory_;
  std::string source_hash_, original_file_;
  size_t copier_header_size_ = 0;
  Rom rom_;
  std::vector<editor::MessageData> messages_;
  std::vector<std::string> expanded_texts_;
  int expanded_start_ = 0;
  int expanded_end_ = 0;
  size_t selected_message_ = 0;
  zelda3::OverworldSpriteEdits sprite_edits_;
  int sprite_state_ = 0;
  int sprite_map_ = 0;
  std::unique_ptr<zelda3::Overworld> overworld_;
  zelda3::GameData game_data_;
  bool palette_initialized_ = false;
  uint16_t initial_color_ = 0;
  uint16_t expected_color_ = 0;
};

TEST_P(SaveTrustResourcesTest, I1NoOpSavePreservesFileBytes) {
  ASSERT_TRUE(Save().ok());
  const auto after = ReadResourceFile(test_rom_path_);
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_TRUE(*after == original_file_)
      << "No-op " << ResourceDomainName(Domain()) << " changed file bytes";
}

TEST_P(SaveTrustResourcesTest, I2EditSaveFreshProductionReader) {
  if (!HasEditableRecord()) {
    GTEST_SKIP() << "Fixture has no suitable record for this writer";
  }
  ASSERT_TRUE(Edit().ok());
  ASSERT_TRUE(Save().ok());
  const auto matches = FreshFileMatchesEditedDocument();
  ASSERT_TRUE(matches.ok()) << matches.status();
  EXPECT_TRUE(*matches);
}

TEST_P(SaveTrustResourcesTest, I3EditUndoSavePreservesFileBytes) {
  if (Domain() != ResourceDomain::kOverworldMainPalette) {
    GTEST_SKIP() << "NOT-RUN: no qualified editor Undo seam in this "
                 << ResourceDomainName(Domain()) << " codec fixture";
  }
  auto& manager = gfx::PaletteManager::Get();
  ASSERT_TRUE(Edit().ok());
  ASSERT_TRUE(manager.CanUndo());
  manager.Undo();
  ASSERT_TRUE(Save().ok());
  const auto after = ReadResourceFile(test_rom_path_);
  ASSERT_TRUE(after.ok()) << after.status();
  EXPECT_TRUE(*after == original_file_)
      << "Palette edit/Undo/Save changed file bytes";
}

TEST_P(SaveTrustResourcesTest, I4EditSaveContainsWritesToDeclaredRegions) {
  if (!HasEditableRecord()) {
    GTEST_SKIP() << "Fixture has no suitable record for this writer";
  }
  ASSERT_TRUE(Edit().ok());
  ASSERT_TRUE(Save().ok());
  const auto after = ReadResourceFile(test_rom_path_);
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
  const auto matches = FreshFileMatchesEditedDocument();
  ASSERT_TRUE(matches.ok()) << matches.status();
  EXPECT_TRUE(*matches);
}

TEST_P(SaveTrustResourcesTest, ControlSkippingDiskSaveCannotObserveEdit) {
  if (!HasEditableRecord()) {
    GTEST_SKIP() << "Fixture has no suitable record for this writer";
  }
  ASSERT_TRUE(Edit().ok());
  ASSERT_TRUE(Serialize().ok());
  // Omit only SaveRomToFile from I2. The exact same fresh-reader comparison
  // against the edited document must now reject the expected result.
  const auto matches = FreshFileMatchesEditedDocument();
  ASSERT_TRUE(matches.ok()) << matches.status();
  EXPECT_FALSE(*matches);
}

INSTANTIATE_TEST_SUITE_P(
    SaveTrustRomClasses, SaveTrustResourcesTest,
    ::testing::Combine(
        ::testing::Values(ResourceRomClass::kVanilla,
                          ResourceRomClass::kExpanded,
                          ResourceRomClass::kOracle),
        ::testing::Values(ResourceDomain::kVanillaMessages,
                          ResourceDomain::kExpandedMessages,
                          ResourceDomain::kOverworldSprites,
                          ResourceDomain::kOverworldItems,
                          ResourceDomain::kOverworldMainPalette)),
    [](const ::testing::TestParamInfo<ResourceCase>& info) {
      return ResourceRoleName(std::get<0>(info.param)) +
             ResourceDomainName(std::get<1>(info.param));
    });

}  // namespace
}  // namespace yaze::test
