#include "zelda3/overworld/overworld_sprite_io.h"
#include <algorithm>
#include <memory>
#include "app/editor/overworld/entity/entity.h"
#include "app/editor/overworld/overworld_editor.h"
#include "core/features.h"
#include "gtest/gtest.h"
#include "rom/write_fence.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::zelda3 {
namespace {
class OverworldSpriteIoTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    std::vector<uint8_t> data(0x200000, 0xA5);
    data[0x140145] = GetParam() ? 3 : 0xFF;
    ASSERT_TRUE(rom_.LoadFromData(data).ok());
    layout_ = GetOverworldSpriteLayout(rom_);
    ASSERT_TRUE(rom_.WriteByte(layout_.data_start, 0xFF).ok());
    for (int s = 0; s < 3; ++s) {
      for (int m = 0; m < layout_.counts[s]; ++m) {
        ASSERT_TRUE(rom_.WriteWord(layout_.tables[s] + m * 2,
                                   layout_.data_start - 0x40000)
                        .ok());
      }
    }
    rom_.set_dirty(false);
  }
  void Save(const OverworldSpriteEdits& edits) {
    auto plan = PlanOverworldSpriteSave(rom_, edits);
    ASSERT_TRUE(plan.ok()) << plan.status();
    ASSERT_TRUE(ApplyOverworldSpriteSave(rom_, *plan).ok());
  }
  OverworldSpriteBytes Read(int s, int m) {
    auto bytes = ReadOverworldSpriteList(rom_, layout_.tables[s] + m * 2);
    EXPECT_TRUE(bytes.ok()) << bytes.status();
    return bytes.ok() ? *bytes : OverworldSpriteBytes{};
  }
  Rom rom_;
  OverworldSpriteLayout layout_;
};
TEST_P(OverworldSpriteIoTest, InsertMoveChangeDeleteRoundTripAllStates) {
  OverworldSpriteEdits edits;
  for (int s = 0; s < 3; ++s)
    edits[s][s] = {1, 2, static_cast<uint8_t>(s), 0xFF};
  Save(edits);
  for (int s = 0; s < 3; ++s)
    EXPECT_EQ(Read(s, s), *edits[s][s]);
  edits[1][1] = {33, 35, 0xAC, 0xFF};
  edits[2][2] = {0xFF};
  Save(edits);
  EXPECT_EQ(Read(1, 1), *edits[1][1]);
  EXPECT_EQ(Read(2, 2), OverworldSpriteBytes{0xFF});
}
TEST_P(OverworldSpriteIoTest, SharedPointersDetachAndPreserveOtherMaps) {
  OverworldSpriteEdits edits;
  edits[0][0] = {3, 4, 5, 0xFF};
  edits[1][3] = edits[0][0];
  Save(edits);
  EXPECT_EQ(*rom_.ReadWord(layout_.tables[0]),
            *rom_.ReadWord(layout_.tables[1] + 6));
  edits = {};
  edits[0][0] = {6, 7, 8, 0xFF};
  Save(edits);
  EXPECT_EQ(Read(1, 3), (OverworldSpriteBytes{3, 4, 5, 0xFF}));
  EXPECT_NE(*rom_.ReadWord(layout_.tables[0]),
            *rom_.ReadWord(layout_.tables[1] + 6));
}
TEST_P(OverworldSpriteIoTest, PreservesOrderDuplicatesFlagsAndUneditedTail) {
  OverworldSpriteEdits edits;
  edits[2][layout_.counts[2] - 1] = {0x81, 0xC2, 9, 0x81, 0xC2,
                                     9,    4,    3, 2,    0xFF};
  Save(edits);
  const auto tail = Read(2, layout_.counts[2] - 1);
  edits = {};
  edits[0][0] = {0, 0, 1, 0xFF};
  Save(edits);
  EXPECT_EQ(Read(2, layout_.counts[2] - 1), tail);
}
TEST_P(OverworldSpriteIoTest, NoOpDoesNotRepackOrDirtyRom) {
  const auto before = rom_.vector();
  OverworldSpriteEdits edits;
  edits[0][0] = {0xFF};
  auto plan = PlanOverworldSpriteSave(rom_, edits);
  ASSERT_TRUE(plan.ok());
  EXPECT_TRUE(plan->empty());
  ASSERT_TRUE(ApplyOverworldSpriteSave(rom_, *plan).ok());
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_FALSE(rom_.dirty());
}
TEST_P(OverworldSpriteIoTest, OverflowFailsBeforeAnyWrite) {
  OverworldSpriteEdits edits;
  edits[0][0] = OverworldSpriteBytes(12000, 1);
  edits[0][0]->push_back(0xFF);
  const auto before = rom_.vector();
  auto plan = PlanOverworldSpriteSave(rom_, edits);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_FALSE(rom_.dirty());
}
TEST_P(OverworldSpriteIoTest, UnrelatedBytesAndDungeonPointersRemainExact) {
  const auto before = rom_.vector();
  OverworldSpriteEdits edits;
  edits[0][0] = {1, 2, 3, 0xFF};
  auto plan = PlanOverworldSpriteSave(rom_, edits);
  ASSERT_TRUE(plan.ok());
  ASSERT_TRUE(ApplyOverworldSpriteSave(rom_, *plan).ok());
  for (int i = 0; i < rom_.size(); ++i) {
    bool written = false;
    for (const auto& write : *plan) {
      written |= i >= write.address &&
                 i < write.address + static_cast<int>(write.bytes.size());
    }
    if (!written && before[i] != rom_.vector()[i])
      FAIL() << "Unexpected write at " << i;
  }
}
TEST_P(OverworldSpriteIoTest, OuterFenceFailureRollsBackPriorPointerWrites) {
  OverworldSpriteEdits edits;
  edits[0][0] = {1, 2, 3, 0xFF};
  auto plan = PlanOverworldSpriteSave(rom_, edits);
  ASSERT_TRUE(plan.ok());
  const auto before = rom_.vector();
  rom::WriteFence fence;
  ASSERT_TRUE(fence
                  .Allow(layout_.tables[0],
                         layout_.tables[0] + layout_.counts[0] * 2,
                         "first table only")
                  .ok());
  rom::ScopedWriteFence scope(&rom_, &fence);
  EXPECT_EQ(ApplyOverworldSpriteSave(rom_, *plan).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_FALSE(rom_.dirty());
}
TEST_P(OverworldSpriteIoTest, RejectsPointersIntoDungeonAndLowBank) {
  for (uint16_t ptr : {uint16_t{0xD62E}, uint16_t{0x1234}}) {
    ASSERT_TRUE(rom_.WriteWord(layout_.tables[0], ptr).ok());
    EXPECT_EQ(ReadOverworldSpriteList(rom_, layout_.tables[0]).status().code(),
              absl::StatusCode::kFailedPrecondition);
  }
}
TEST_P(OverworldSpriteIoTest, TerminatorAtFinalByteDoesNotReadDungeonData) {
  ASSERT_TRUE(
      rom_.WriteWord(layout_.tables[0], kOverworldSpriteDataEnd - 1 - 0x40000)
          .ok());
  ASSERT_TRUE(rom_.WriteByte(kOverworldSpriteDataEnd - 1, 0xFF).ok());
  EXPECT_EQ(Read(0, 0), OverworldSpriteBytes{0xFF});
  ASSERT_TRUE(rom_.WriteByte(kOverworldSpriteDataEnd - 1, 0).ok());
  EXPECT_FALSE(ReadOverworldSpriteList(rom_, layout_.tables[0]).ok());
}
TEST_P(OverworldSpriteIoTest, TruncatedRegionFailsClosed) {
  Rom short_rom;
  ASSERT_TRUE(short_rom.LoadFromData(std::vector<uint8_t>(0x10000, 0)).ok());
  EXPECT_EQ(
      ReadOverworldSpriteList(short_rom, layout_.tables[0]).status().code(),
      absl::StatusCode::kFailedPrecondition);
}
TEST_P(OverworldSpriteIoTest, RejectsEmbeddedTerminator) {
  OverworldSpriteEdits edits;
  edits[0][0] = {0xFF, 1, 2, 0xFF};
  EXPECT_EQ(PlanOverworldSpriteSave(rom_, edits).status().code(),
            absl::StatusCode::kInvalidArgument);
}
TEST_P(OverworldSpriteIoTest, ModelSaveUsesMovedPositionAndPreservesFlags) {
  OverworldSpriteEdits edits;
  edits[0][0] = {0x81, 0xC2, 3, 0xFF};
  Save(edits);
  auto world = std::make_unique<Overworld>(&rom_);
  // Seed only the map needed by this headless model test; no graphics/ROM-file load.
  auto& maps = const_cast<std::vector<OverworldMap>&>(world->overworld_maps());
  maps.emplace_back(0, &rom_);
  ASSERT_TRUE(world->LoadSpritesFromMap(layout_.tables[0], 1, 0).ok());
  auto& sprite = world->mutable_sprites(0)->at(0);
  sprite.set_x(48);
  sprite.set_y(64);
  sprite.set_id(0xAC);
  ASSERT_TRUE(world->SaveSprites().ok());
  EXPECT_EQ(Read(0, 0), (OverworldSpriteBytes{0x84, 0xC3, 0xAC, 0xFF}));
  // Reload replaces the list instead of appending duplicates.
  ASSERT_TRUE(world->LoadSpritesFromMap(layout_.tables[0], 1, 0).ok());
  ASSERT_EQ(world->sprites(0).size(), 1);
  EXPECT_EQ(world->sprites(0)[0].x(), 48);
  world->mutable_sprites(0)->clear();
  ASSERT_TRUE(world->SaveSprites().ok());
  EXPECT_EQ(Read(0, 0), OverworldSpriteBytes{0xFF});
}
TEST_P(OverworldSpriteIoTest, EditorSavePersistsSpritesWithMapSavingDisabled) {
  const auto saved_flags = core::FeatureFlags::get().overworld;
  struct RestoreFlags {
    decltype(saved_flags) flags;
    ~RestoreFlags() { core::FeatureFlags::get().overworld = flags; }
  } restore{saved_flags};
  auto& flags = core::FeatureFlags::get().overworld;
  flags.kSaveOverworldMaps = false;
  flags.kSaveOverworldEntrances = false;
  flags.kSaveOverworldExits = false;
  flags.kSaveOverworldItems = false;
  flags.kSaveOverworldProperties = false;
  auto editor = std::make_unique<editor::OverworldEditor>(&rom_);
  auto& world = editor->overworld();
  auto& maps = const_cast<std::vector<OverworldMap>&>(world.overworld_maps());
  maps.emplace_back(0, &rom_);
  ASSERT_TRUE(world.LoadSpritesFromMap(layout_.tables[0], 1, 0).ok());
  world.mutable_sprites(0)->emplace_back(std::vector<uint8_t>{}, 0, 0x42, 1, 2,
                                         16, 32);
  ASSERT_TRUE(editor->Save().ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom_.vector()).ok());
  const auto pointer = *reopened.ReadWord(layout_.tables[0]);
  EXPECT_EQ(*reopened.ReadByte(0x40000 + pointer), 2);
  EXPECT_EQ(*reopened.ReadByte(0x40001 + pointer), 1);
  EXPECT_EQ(*reopened.ReadByte(0x40002 + pointer), 0x42);
  EXPECT_EQ(*reopened.ReadByte(0x40003 + pointer), 0xFF);
  // Undoing an already saved insertion must write the original empty list.
  world.mutable_sprites(0)->clear();
  ASSERT_TRUE(editor->Save().ok());
  EXPECT_EQ(Read(0, 0), OverworldSpriteBytes{0xFF});
}
TEST_P(OverworldSpriteIoTest,
       ModelRejectsOffGridAndUnsupportedStateWithoutWriting) {
  auto world = std::make_unique<Overworld>(&rom_);
  auto& maps = const_cast<std::vector<OverworldMap>&>(world->overworld_maps());
  maps.emplace_back(0, &rom_);
  ASSERT_TRUE(world->LoadSpritesFromMap(layout_.tables[0], 1, 0).ok());
  world->mutable_sprites(0)->emplace_back(std::vector<uint8_t>{}, 0, 1, 0, 0, 8,
                                          0);
  const auto before = rom_.vector();
  EXPECT_EQ(world->SaveSprites().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom_.vector(), before);
  world->mutable_sprites(0)->clear();
  world->mutable_sprites(0)->emplace_back(std::vector<uint8_t>{}, 0x40, 1, 0, 0,
                                          0, 0);
  EXPECT_EQ(world->SaveSprites().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom_.vector(), before);
}
TEST_P(OverworldSpriteIoTest, InternalFenceRejectsWritesIntoDungeonRegion) {
  const auto before = rom_.vector();
  OverworldSpriteSavePlan plan{{layout_.data_start, {1}},
                               {kOverworldSpriteDataEnd, {0}}};
  EXPECT_EQ(ApplyOverworldSpriteSave(rom_, plan).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom_.vector(), before);
}
TEST_P(OverworldSpriteIoTest, CapacityBoundaryFitsThenRejectsOneMoreRecord) {
  const int capacity = kOverworldSpriteDataEnd - layout_.data_start;
  const int records =
      (capacity - 2) / 3;  // edited list plus shared empty terminator
  OverworldSpriteEdits edits;
  edits[0][0] = OverworldSpriteBytes(records * 3, 1);
  edits[0][0]->push_back(0xFF);
  ASSERT_TRUE(PlanOverworldSpriteSave(rom_, edits).ok());
  edits[0][0]->insert(edits[0][0]->begin(), 3, 1);
  EXPECT_EQ(PlanOverworldSpriteSave(rom_, edits).status().code(),
            absl::StatusCode::kResourceExhausted);
}
TEST(OverworldSpritePlacementTest, FreeMovementStillUsesRepresentableGrid) {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  auto* context = ImGui::CreateContext();
  ImGui::GetIO().MousePos = ImVec2(24, 40);
  Sprite sprite(std::vector<uint8_t>{}, 0, 1, 0, 0, 0, 0);
  editor::MoveEntityOnGrid(&sprite, ImVec2(0, 0), ImVec2(0, 0), true, 1.0f);
  EXPECT_EQ(sprite.x(), 16);
  EXPECT_EQ(sprite.y(), 32);
  ImGui::DestroyContext(context);
  ImGui::SetCurrentContext(previous);
}
INSTANTIATE_TEST_SUITE_P(VanillaAndExpanded, OverworldSpriteIoTest,
                         ::testing::Bool());
}  // namespace
}  // namespace yaze::zelda3
