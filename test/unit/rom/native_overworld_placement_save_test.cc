#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>

#include "absl/cleanup/cleanup.h"
#include "app/editor/editor_manager.h"
#include "app/editor/overworld/entity/entity_mutation_service.h"
#include "app/editor/overworld/overworld_editor.h"
#include "editor_test_support.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "framework/headless_editor_test.h"
#include "framework/rom_save_fault.h"
#include "rom/write_fence.h"
#include "unique_temp_path.h"

namespace yaze {
namespace {
using namespace test::native_overworld;

rom::NativeOverworldAuthoringMetadata Metadata(const Rom& document) {
  const auto layout = document.InspectLyraLayout();
  const auto it =
      std::find_if(layout.entries.begin(), layout.entries.end(),
                   [](const auto& entry) { return entry.id == 412; });
  if (it == layout.entries.end())
    return {};
  const auto value = rom::DecodeNativeOverworldAuthoringMetadata(
      {document.data() + it->pc_address, rom::kNativeAuthoringMetadataSize},
      layout.capacity);
  EXPECT_TRUE(value.ok()) << value.status();
  return value.ok() ? *value : rom::NativeOverworldAuthoringMetadata{};
}

zelda3::Sprite* FindId(zelda3::Overworld& world, uint32_t id) {
  auto& rows = *world.mutable_sprites(1);
  const auto it = std::find_if(
      rows.begin(), rows.end(),
      [id](const auto& sprite) { return sprite.placement_id() == id; });
  return it == rows.end() ? nullptr : &*it;
}

void GrowActorFixture(NativeDirectoryFixture& fixture) {
  fixture.Entry(100).count = 512;
  fixture.Seal();
}

TEST(NativePlacementCommitTest, AtomicMetadataSpriteBatchAndOwnershipRefusals) {
  NativeDirectoryFixture fixture;
  fixture.SeedNativeSpritePrefix();
  fixture.UpgradeToRevision4();
  const auto original = fixture.SeedCanonicalPlacementRows();
  GrowActorFixture(fixture);
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  auto proposed = original;
  std::get<rom::NativeAuthoringSpritePlacement>(proposed.placements[1].value)
      .packed[1]++;
  auto stream = std::vector<uint8_t>{14, 23, 0x52, 21, 16, 0x52, 12,  40, 0x52,
                                     43, 13, 0x52, 45, 37, 0x52, 0xFF};
  const std::vector<rom::NativeOverworldSpriteEdit> edits = {{1, 0xB0, stream}};
  const auto before = document.vector();
  EXPECT_FALSE(
      document.CommitNativeTerrainImage(before, before, {}, edits).ok());
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(before, before, {}, edits, nullptr,
                                             &proposed)
                   .ok());
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(before, before, {}, edits,
                                             &original, nullptr)
                   .ok());
  auto bad = original;
  bad.placements.clear();
  EXPECT_FALSE(
      document.CommitNativeTerrainImage(before, before, {}, {}, &original, &bad)
          .ok());
  bad = proposed;
  bad.next_placement_id = 0;
  EXPECT_FALSE(rom::PlanNativeOverworldSpriteSave(before, edits, &bad).ok());
  auto raw = before;
  raw[fixture.Entry(412).pc_address + 16] ^= 1;
  EXPECT_FALSE(document
                   .CommitNativeTerrainImage(before, raw, {}, edits, &original,
                                             &proposed)
                   .ok());
  {
    ScopedRomReadOnly read_only(document);
    EXPECT_FALSE(document
                     .CommitNativeTerrainImage(before, before, {}, edits,
                                               &original, &proposed)
                     .ok());
  }
  {
    rom::WriteFence fence;
    const auto at = fixture.Entry(412).pc_address;
    ASSERT_TRUE(fence.Allow(0, at, "before metadata").ok());
    ASSERT_TRUE(fence
                    .Allow(at + rom::kNativeAuthoringMetadataSize,
                           before.size(), "after metadata")
                    .ok());
    rom::ScopedWriteFence scope(&document, &fence);
    EXPECT_FALSE(document
                     .CommitNativeTerrainImage(before, before, {}, edits,
                                               &original, &proposed)
                     .ok());
  }
  EXPECT_EQ(document.vector(), before);
  EXPECT_FALSE(document.dirty());
  ASSERT_TRUE(document
                  .CommitNativeTerrainImage(before, before, {}, edits,
                                            &original, &proposed)
                  .ok());
  EXPECT_EQ(Metadata(document), proposed);
  const auto saved = document.vector();
  ASSERT_TRUE(document
                  .CommitNativeTerrainImage(saved, saved, {}, edits, &proposed,
                                            &proposed)
                  .ok());
  EXPECT_EQ(document.vector(), saved);
}

TEST(NativePlacementCommitTest, MetadataCannotAliasOriginalEngineTables) {
  NativeDirectoryFixture fixture;
  fixture.SeedNativeSpritePrefix();
  fixture.UpgradeToRevision4();
  const auto metadata = fixture.SeedCanonicalPlacementRows();
  auto& entry = fixture.Entry(412);
  entry.pc_address = 0x140480;
  entry.snes_address = NativeDirectoryFixture::Snes(entry.pc_address);
  const auto encoded =
      rom::EncodeNativeOverworldAuthoringMetadata(metadata, 192);
  ASSERT_TRUE(encoded.ok());
  std::copy(encoded->begin(), encoded->end(),
            fixture.bytes.begin() + entry.pc_address);
  fixture.Seal();
  Rom document;
  ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
  ASSERT_EQ(document.InspectLyraLayout().presence,
            rom::LyraLayoutPresence::kValid);
  const auto status = document.CommitNativeTerrainImage(
      fixture.bytes, fixture.bytes, {}, {}, &metadata, &metadata);
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(document.vector(), fixture.bytes);
}

class NativePlacementSaveTest : public test::HeadlessEditorTest {};

TEST_F(NativePlacementSaveTest, HistoryFailureRetryAndColdReopen) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS";
  const auto flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
  core::FeatureFlags::get().overworld.kSaveOverworldMaps = false;
  core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
  core::FeatureFlags::get().overworld.kSaveOverworldProperties = true;
  Rom legacy;
  ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
  zelda3::Overworld reference(&legacy);
  ASSERT_TRUE(reference.Load(&legacy).ok());
  NativeDirectoryFixture fixture;
  fixture.SeedLoadedLegacyForModelTest(legacy, reference);
  fixture.UpgradeToRevision4();
  fixture.SeedCanonicalPlacementRows();
  GrowActorFixture(fixture);
  const auto root = test::UniqueTempPath("pls");
  std::filesystem::create_directories(root);
  auto cleanup = absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
  const auto source = root / "in.sfc";
  WriteBytes(source, fixture.bytes);
  auto manager = std::make_unique<editor::EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer_.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;
  ASSERT_TRUE(manager->OpenRomOrProject(source.string()).ok());
  ASSERT_TRUE(
      manager->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld).ok());
  auto* editor = manager->GetCurrentEditorSet()->GetOverworldEditor();
  auto& world = editor->overworld();
  auto* document = manager->GetCurrentRom();
  editor->SetPreviewStoryState(1);
  ASSERT_TRUE(world.LoadSprites().ok());
  auto* selected = FindId(world, 0x5002);
  ASSERT_NE(selected, nullptr);
  const auto old = *selected;
  auto moved = old;
  moved.set_x(old.x() + 16);
  moved.set_id(0x53);
  ASSERT_TRUE(editor->ApplySpriteEdit(selected, moved).ok());
  {
    ScopedRomReadOnly read_only(*document);
    EXPECT_FALSE(editor->Undo().ok());
  }
  EXPECT_EQ(FindId(world, 0x5002)->x(), moved.x());
  ASSERT_TRUE(editor->Undo().ok());
  EXPECT_EQ(FindId(world, 0x5002)->x(), old.x());
  ASSERT_TRUE(editor->Redo().ok());
  const auto before_failure = document->vector();
  {
    test::ScopedRomStagingFailure fault;
    EXPECT_FALSE(manager->SaveCurrentSession(false).ok());
  }
  EXPECT_EQ(document->vector(), before_failure);
  EXPECT_EQ(ReadBytes(source), fixture.bytes);
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  auto cold = [&] {
    Rom reopened;
    EXPECT_TRUE(reopened.LoadFromFile(source.string()).ok());
    return Metadata(reopened);
  };
  auto saved = cold();
  EXPECT_EQ(
      std::get<rom::NativeAuthoringSpritePlacement>(saved.placements[1].value)
          .packed,
      (std::array<uint8_t, 3>{21, 16, 0x53}));
  auto* service = editor->entity_mutation_service();
  ASSERT_NE(service, nullptr);
  ASSERT_TRUE(service->DeleteSprite(FindId(world, 0x5002), 1).ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  auto deleted = cold();
  EXPECT_EQ(deleted.placements[1].state, rom::NativePlacementState::kDeleted);
  EXPECT_EQ(deleted.placements[1].value, saved.placements[1].value);
  EXPECT_EQ(
      std::get<rom::NativeAuthoringSpritePlacement>(deleted.placements[2].value)
          .ordinal,
      1);
  ASSERT_TRUE(editor->Undo().ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(cold().placements[1].state, rom::NativePlacementState::kBound);
  ASSERT_TRUE(editor->Redo().ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  ASSERT_TRUE(editor->Undo().ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  auto inserted = service->InsertEntity("sprite", ImVec2(32, 3104), 0xB0, 1);
  ASSERT_TRUE(inserted.ok()) << inserted.status;
  const auto new_id =
      static_cast<zelda3::Sprite*>(inserted.entity)->placement_id();
  ASSERT_EQ(new_id, 0x6003);
  ASSERT_TRUE(editor->Undo().ok());
  ASSERT_TRUE(FindId(world, *new_id)->deleted());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(cold().placements.back().state,
            rom::NativePlacementState::kDeleted);
  EXPECT_EQ(cold().next_placement_id, 0x6004);
  ASSERT_TRUE(editor->Redo().ok());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(cold().placements.back().state, rom::NativePlacementState::kBound);
  // Untracked legacy rows precede new identities in the actual packed list.
  auto prefix = service->InsertEntity("sprite", ImVec2(1040, 16), 2, 1);
  ASSERT_TRUE(prefix.ok()) << prefix.status;
  const auto prefix_id =
      static_cast<zelda3::Sprite*>(prefix.entity)->placement_id();
  auto proposal = world.ProposeNativeAuthoringMetadataForSave();
  ASSERT_TRUE(proposal.ok()) << proposal.status();
  const auto& last = std::get<rom::NativeAuthoringSpritePlacement>(
      proposal->placements.back().value);
  EXPECT_GT(last.ordinal, 0);
  EXPECT_NE(prefix_id, new_id);
  ASSERT_TRUE(
      editor->Undo().ok());  // Bank09 is full; preserve only its tombstone.
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  // Disabled sprite Save retains pending edits and the saved metadata exactly.
  auto draft = *FindId(world, 0x5001);
  draft.set_id(0x54);
  ASSERT_TRUE(editor->ApplySpriteEdit(FindId(world, 0x5001), draft).ok());
  auto persisted = Metadata(*document);
  core::FeatureFlags::get().overworld.kSaveOverworldSprites = false;
  const auto message = world.overworld_map(0x80)->message_id() ^ 1;
  world.mutable_overworld_map(0x80)->set_message_id(message);
  document->set_dirty(true);
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(cold(), persisted);
  core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(
      std::get<rom::NativeAuthoringSpritePlacement>(cold().placements[0].value)
          .packed[2],
      0x54);
  // Resize history can predate a later insertion that has been undone. Its
  // extra deleted ID must remain reserved while the old geometry is restored.
  const auto area = world.CaptureNativeAreaSnapshot();
  ASSERT_TRUE(area.ok()) << area.status();
  ASSERT_TRUE(
      world.ConfigureMultiAreaMap(0xB0, zelda3::AreaSizeEnum::SmallArea).ok());
  auto later = service->InsertEntity("sprite", ImVec2(48, 3120), 0xB0, 1);
  ASSERT_TRUE(later.ok()) << later.status;
  const auto later_id =
      static_cast<zelda3::Sprite*>(later.entity)->placement_id();
  EXPECT_FALSE(world.RestoreNativeAreaSnapshot(*area).ok());
  ASSERT_TRUE(editor->Undo().ok());
  ASSERT_TRUE(world.RestoreNativeAreaSnapshot(*area).ok());
  EXPECT_TRUE(FindId(world, *later_id)->deleted());
  ASSERT_TRUE(manager->SaveCurrentSession(false).ok());
  EXPECT_EQ(cold().placements.back().id, later_id);
  EXPECT_EQ(cold().placements.back().state,
            rom::NativePlacementState::kDeleted);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(source.string()).ok());
  zelda3::Overworld fresh(&reopened);
  ASSERT_TRUE(fresh.Load(&reopened).ok());
  ASSERT_NE(FindId(fresh, 0x6003), nullptr);
  EXPECT_EQ(FindId(fresh, 0x6003)->placement_id(), new_id);
  EXPECT_EQ(fresh.overworld_map(0x80)->message_id(), message);
  // Another valid typed writer advanced the ROM after this model loaded.
  // Preserve that change and refuse to publish the stale editor draft.
  const auto expected = Metadata(*document);
  auto external = expected;
  ++external.next_placement_id;
  const auto ram_before = document->vector();
  ASSERT_TRUE(document
                  ->CommitNativeTerrainImage(ram_before, ram_before, {}, {},
                                             &expected, &external)
                  .ok());
  const auto external_bytes = document->vector();
  const auto disk_before = ReadBytes(source);
  const auto stale = manager->SaveCurrentSession(false);
  EXPECT_FALSE(stale.ok());
  EXPECT_NE(stale.message().find("baseline"), absl::string_view::npos) << stale;
  EXPECT_EQ(document->vector(), external_bytes);
  EXPECT_EQ(ReadBytes(source), disk_before);
}

TEST_F(NativePlacementSaveTest, CapacityAndAllocatorRefuseBeforeInsertion) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS";
  Rom legacy;
  ASSERT_TRUE(legacy.LoadFromData(ReadBytes(path)).ok());
  zelda3::Overworld reference(&legacy);
  ASSERT_TRUE(reference.Load(&legacy).ok());
  for (bool full : {false, true}) {
    SCOPED_TRACE(full);
    NativeDirectoryFixture fixture;
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    fixture.UpgradeToRevision4();
    auto metadata = fixture.SeedCanonicalPlacementRows();
    if (full) {
      while (metadata.placements.size() < rom::kNativeAuthoringMetadataSlots) {
        metadata.placements.push_back(
            {metadata.next_placement_id++, rom::NativePlacementState::kPending,
             rom::NativeAuthoringDoorPlacement{0xB0, 0, 0, std::nullopt,
                                               std::nullopt}});
      }
    } else {
      metadata.next_placement_id = std::numeric_limits<uint32_t>::max();
    }
    const auto encoded =
        rom::EncodeNativeOverworldAuthoringMetadata(metadata, 192);
    ASSERT_TRUE(encoded.ok());
    std::copy(encoded->begin(), encoded->end(),
              fixture.bytes.begin() + fixture.Entry(412).pc_address);
    fixture.Seal();
    Rom document;
    ASSERT_TRUE(document.LoadFromData(fixture.bytes).ok());
    zelda3::Overworld world(&document);
    ASSERT_TRUE(world.Load(&document).ok());
    const auto count = world.sprites(1).size();
    editor::EntityMutationService service(world);
    const auto inserted =
        service.InsertEntity("sprite", ImVec2(32, 3104), 0xB0, 1);
    EXPECT_EQ(inserted.status.code(), absl::StatusCode::kResourceExhausted)
        << inserted.status;
    EXPECT_EQ(world.sprites(1).size(), count);
    const auto after = world.ProposeNativeAuthoringMetadataForSave();
    ASSERT_TRUE(after.ok()) << after.status();
    EXPECT_EQ(*after, metadata);
    EXPECT_EQ(document.vector(), fixture.bytes);
    EXPECT_FALSE(document.dirty());
  }
}

}  // namespace
}  // namespace yaze
