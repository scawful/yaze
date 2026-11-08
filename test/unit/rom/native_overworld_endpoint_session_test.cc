#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <type_traits>

#include "app/editor/editor_manager.h"
#include "app/editor/overworld/overworld_editor.h"
#include "editor_test_support.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "framework/headless_editor_test.h"
#include "framework/rom_save_fault.h"
#include "unique_temp_path.h"

namespace yaze {
namespace {
using namespace test::native_overworld;

class NativeEndpointSessionTest : public test::HeadlessEditorTest {
 protected:
  void SetUp() override {
    HeadlessEditorTest::SetUp();
    flags_ = core::FeatureFlags::get();
    const char* input = std::getenv("YAZE_TEST_ROM_OOS");
    if (!input || !std::filesystem::exists(input))
      GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to the legacy Oracle test fixture";
    Rom legacy;
    ASSERT_TRUE(legacy.LoadFromFile(input).ok());
    zelda3::Overworld reference(&legacy);
    ASSERT_TRUE(reference.Load(&legacy).ok());
    NativeDirectoryFixture fixture;
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    fixture.UpgradeToRevision4();
    auto metadata = fixture.SeedCanonicalPlacementRows();
    CustomizeMetadata(metadata);
    const auto encoded =
        rom::EncodeNativeOverworldAuthoringMetadata(metadata, 192);
    ASSERT_TRUE(encoded.ok()) << encoded.status();
    std::copy(encoded->begin(), encoded->end(),
              fixture.bytes.begin() + fixture.Entry(412).pc_address);
    fixture.Entry(100).count = 512;
    fixture.Seal();
    before_ = fixture.bytes;
    metadata_pc_ = fixture.Entry(412).pc_address;
    actor_pc_ = fixture.Entry(100).pc_address;
    root_ = test::UniqueTempPath("endpoint_session");
    std::filesystem::create_directories(root_);
    source_ = root_ / "in.sfc";
    WriteBytes(source_, before_);
    manager_ = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager_->user_settings().prefs().backup_before_save = false;
    ASSERT_TRUE(manager_->OpenRomOrProject(source_.string()).ok());
    ASSERT_TRUE(
        manager_->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld)
            .ok());
    editor_ = manager_->GetCurrentEditorSet()->GetOverworldEditor();
    ASSERT_NE(editor_, nullptr);
    // Initializing the manager applies settings defaults to FeatureFlags.
    // Select the test's Save policy afterward so these switches are real.
    auto& flags = core::FeatureFlags::get().overworld;
    flags.kSaveOverworldMaps = false;
    flags.kSaveOverworldSprites = false;
    flags.kSaveOverworldProperties = true;
    flags.kSaveOverworldEntrances = true;
    flags.kSaveOverworldExits = true;
    ASSERT_GT(editor_->overworld().entrances().size(), kEntranceRow);
    ASSERT_GT(editor_->overworld().exits()->size(), kReturnRow);
  }

  void TearDown() override {
    manager_.reset();
    if (!root_.empty())
      std::filesystem::remove_all(root_);
    if (flags_)
      core::FeatureFlags::get() = *flags_;
    HeadlessEditorTest::TearDown();
  }

  virtual void CustomizeMetadata(rom::NativeOverworldAuthoringMetadata&) {}

  void PrepareBindingRows() {
    auto& world = editor_->overworld();
    auto& entrance = world.entrances()[kEntranceRow];
    auto door = entrance;
    door.map_id_ = 0xB0;
    door.x_ = 22 * 16;
    door.y_ = 0xC00 + 8 * 16;
    door.entrance_id_ = 0x0B;
    door.is_hole_ = false;
    door.deleted = false;
    ASSERT_TRUE(editor_->ApplyEntranceEdit(&entrance, door).ok());
    auto& exit = (*world.mutable_exits())[kReturnRow];
    auto back = exit;
    back.map_id_ = 0xB0;
    back.x_ = 22 * 16;
    back.y_ = 0xC00 + 11 * 16;
    back.room_id_ = 0x83;
    back.is_hole_ = false;
    back.deleted_ = false;
    ASSERT_TRUE(editor_->ApplyExitEdit(&exit, back).ok());
  }

  void BindBoth() {
    ASSERT_TRUE(
        editor_->BindNativeEndpointPlacement(0x6001, kEntranceRow).ok());
    ASSERT_TRUE(editor_->BindNativeEndpointPlacement(0x6002, kReturnRow).ok());
  }

  rom::NativeOverworldAuthoringMetadata DiskMetadata() {
    Rom reopened;
    const auto loaded = reopened.LoadFromFile(source_.string());
    EXPECT_TRUE(loaded.ok()) << loaded;
    if (!loaded.ok())
      return {};
    if (metadata_pc_ > reopened.size() ||
        rom::kNativeAuthoringMetadataSize > reopened.size() - metadata_pc_) {
      ADD_FAILURE() << "Saved metadata extent is truncated";
      return {};
    }
    const auto decoded = rom::DecodeNativeOverworldAuthoringMetadata(
        {reopened.data() + metadata_pc_, rom::kNativeAuthoringMetadataSize},
        192);
    EXPECT_TRUE(decoded.ok()) << decoded.status();
    return decoded.ok() ? *decoded : rom::NativeOverworldAuthoringMetadata{};
  }

  void ExpectSpritePayloadUnchanged() {
    const auto saved = ReadBytes(source_);
    ASSERT_EQ(saved.size(), before_.size());
    EXPECT_TRUE(std::equal(before_.begin() + actor_pc_,
                           before_.begin() + actor_pc_ + 512,
                           saved.begin() + actor_pc_));
  }

  static const rom::NativeAuthoringPlacement* FindPlacement(
      const rom::NativeOverworldAuthoringMetadata& metadata, uint32_t id) {
    const auto found =
        std::find_if(metadata.placements.begin(), metadata.placements.end(),
                     [id](const auto& p) { return p.id == id; });
    return found == metadata.placements.end() ? nullptr : &*found;
  }

  void ReopenSession() {
    const auto save_policy = core::FeatureFlags::get();
    manager_.reset();
    manager_ = std::make_unique<editor::EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager_->user_settings().prefs().backup_before_save = false;
    ASSERT_TRUE(manager_->OpenRomOrProject(source_.string()).ok());
    ASSERT_TRUE(
        manager_->EnsureEditorAssetsLoaded(editor::EditorType::kOverworld)
            .ok());
    editor_ = manager_->GetCurrentEditorSet()->GetOverworldEditor();
    ASSERT_NE(editor_, nullptr);
    core::FeatureFlags::get() = save_policy;
  }

  void CheckBindingDomain(bool save_entrance) {
    PrepareBindingRows();
    ASSERT_FALSE(HasFatalFailure());
    BindBoth();
    ASSERT_FALSE(HasFatalFailure());
    auto& world = editor_->overworld();
    const auto draft = world.native_authoring_metadata();
    ASSERT_TRUE(draft.has_value());
    EXPECT_FALSE(world.HasPendingNativeSpriteChanges());
    auto& flags = core::FeatureFlags::get().overworld;
    ASSERT_FALSE(flags.kSaveOverworldSprites);
    flags.kSaveOverworldEntrances = save_entrance;
    flags.kSaveOverworldExits = !save_entrance;
    const auto partial = manager_->SaveCurrentSession(false);
    EXPECT_EQ(partial.code(), absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(partial.message(), "Save left unsaved project or ROM work");
    const auto saved = DiskMetadata();
    ASSERT_NE(FindPlacement(saved, 0x6001), nullptr);
    ASSERT_NE(FindPlacement(saved, 0x6002), nullptr);
    EXPECT_EQ(FindPlacement(saved, save_entrance ? 0x6001 : 0x6002)->state,
              rom::NativePlacementState::kBound);
    EXPECT_EQ(FindPlacement(saved, save_entrance ? 0x6002 : 0x6001)->state,
              rom::NativePlacementState::kPending);
    EXPECT_EQ(world.native_authoring_metadata(), draft);
    EXPECT_TRUE(world.HasPendingNativeEndpointChanges());
    EXPECT_FALSE(world.HasPendingNativeSpriteChanges());
    manager_->CloseRom();
    ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
    manager_->CancelPendingUnsavedSessionAction();
    flags.kSaveOverworldEntrances = flags.kSaveOverworldExits = true;
    ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
    EXPECT_EQ(DiskMetadata(), *draft);
    ExpectSpritePayloadUnchanged();
    EXPECT_FALSE(world.HasPendingNativeEndpointChanges());
  }

  void MoveEndpoints() {
    auto& world = editor_->overworld();
    auto* entrance = &world.entrances()[kEntranceRow];
    auto door = *entrance;
    door.map_id_ = 0xA0;
    door.x_ = 21 * 16;
    door.y_ = 0x800 + 9 * 16;
    ASSERT_TRUE(editor_->ApplyEntranceEdit(entrance, door).ok());
    auto* exit = &(*world.mutable_exits())[kReturnRow];
    auto destination = *exit;
    destination.map_id_ = 0xA0;
    destination.x_ = 22 * 16;
    destination.y_ = 0x800 + 11 * 16;
    ASSERT_TRUE(editor_->ApplyExitEdit(exit, destination).ok());
  }

  void ExpectDiskMaps(uint16_t entrance_map, uint16_t return_map) {
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(source_.string()).ok());
    const auto doors = zelda3::LoadEntrances(&reopened, {});
    const auto returns = zelda3::LoadExits(&reopened);
    ASSERT_TRUE(doors.ok()) << doors.status();
    ASSERT_TRUE(returns.ok()) << returns.status();
    EXPECT_EQ((*doors)[kEntranceRow].map_id_, entrance_map);
    EXPECT_EQ((*returns)[kReturnRow].map_id_, return_map);
    EXPECT_TRUE(std::equal(
        before_.begin() + metadata_pc_,
        before_.begin() + metadata_pc_ + rom::kNativeAuthoringMetadataSize,
        reopened.vector().begin() + metadata_pc_));
  }

  static constexpr size_t kEntranceRow = 53;
  static constexpr size_t kReturnRow = 41;
  using Flags = std::remove_cvref_t<decltype(core::FeatureFlags::get())>;
  std::optional<Flags> flags_;
  std::filesystem::path root_;
  std::filesystem::path source_;
  std::vector<uint8_t> before_;
  uint32_t metadata_pc_ = 0;
  uint32_t actor_pc_ = 0;
  std::unique_ptr<editor::EditorManager> manager_;
  editor::OverworldEditor* editor_ = nullptr;
};

TEST_F(NativeEndpointSessionTest,
       BoundIdentityFailureRetryAndFreshModelReopen) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  // First save the authored rows while placements are still pending. Binding
  // them afterward must work as a metadata-only selected-row transaction.
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  const auto before_bind_disk = ReadBytes(source_);
  auto& world = editor_->overworld();
  const auto old_door = world.entrances()[kEntranceRow];
  const auto old_return = (*world.exits())[kReturnRow];
  BindBoth();
  ASSERT_FALSE(HasFatalFailure());
  EXPECT_EQ(world.entrances()[kEntranceRow].x_, old_door.x_);
  EXPECT_EQ(world.entrances()[kEntranceRow].y_, old_door.y_);
  EXPECT_EQ((*world.exits())[kReturnRow].x_, old_return.x_);
  EXPECT_EQ((*world.exits())[kReturnRow].y_, old_return.y_);
  EXPECT_EQ(world.entrances()[kEntranceRow].authoring_placement_id,
            std::optional<uint32_t>(0x6001));
  EXPECT_EQ((*world.exits())[kReturnRow].authoring_placement_id,
            std::optional<uint32_t>(0x6002));
  const auto bound = world.native_authoring_metadata();
  ASSERT_TRUE(bound.has_value());
  EXPECT_EQ(bound->schema_version, 2);
  ASSERT_NE(FindPlacement(*bound, 0x6001), nullptr);
  const auto& door = std::get<rom::NativeAuthoringDoorPlacement>(
      FindPlacement(*bound, 0x6001)->value);
  EXPECT_EQ(door.x, 22);
  EXPECT_EQ(door.y, 8);
  EXPECT_EQ(door.trigger_row, kEntranceRow);
  const auto memory_before_failure = manager_->GetCurrentRom()->vector();
  {
    test::ScopedRomStagingFailure fault;
    EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  }
  EXPECT_EQ(ReadBytes(source_), before_bind_disk);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before_failure);
  EXPECT_EQ(world.native_authoring_metadata(), bound);
  EXPECT_TRUE(world.HasPendingNativeEndpointChanges());
  manager_->CloseRom();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->CancelPendingUnsavedSessionAction();
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), *bound);
  ExpectSpritePayloadUnchanged();
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(source_.string()).ok());
  zelda3::Overworld fresh(&reopened);
  ASSERT_TRUE(fresh.Load(&reopened).ok());
  EXPECT_EQ(fresh.native_authoring_metadata(), bound);
  EXPECT_EQ(fresh.entrances()[kEntranceRow].authoring_placement_id,
            std::optional<uint32_t>(0x6001));
  EXPECT_EQ((*fresh.exits())[kReturnRow].authoring_placement_id,
            std::optional<uint32_t>(0x6002));
  EXPECT_FALSE(fresh.HasPendingNativeEndpointChanges());
}

TEST_F(NativeEndpointSessionTest,
       BindingUndoSaveRedoRetainsSchemaAndRowIdentity) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  BindBoth();
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  const auto bound = editor_->overworld().native_authoring_metadata();
  ASSERT_TRUE(bound.has_value());
  ASSERT_EQ(bound->schema_version, 2);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  auto& world = editor_->overworld();
  const auto pending = world.native_authoring_metadata();
  ASSERT_TRUE(pending.has_value());
  EXPECT_EQ(pending->schema_version, 2);
  for (uint32_t id : {0x6001u, 0x6002u}) {
    ASSERT_NE(FindPlacement(*pending, id), nullptr);
    EXPECT_EQ(FindPlacement(*pending, id)->state,
              rom::NativePlacementState::kPending);
  }
  EXPECT_FALSE(
      world.entrances()[kEntranceRow].authoring_placement_id.has_value());
  EXPECT_FALSE((*world.exits())[kReturnRow].authoring_placement_id.has_value());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), *pending);
  EXPECT_EQ(DiskMetadata().schema_version, 2);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), *bound);
  ReopenSession();
  ASSERT_FALSE(HasFatalFailure());
  EXPECT_EQ(editor_->overworld().native_authoring_metadata(), bound);
  EXPECT_EQ(
      editor_->overworld().entrances()[kEntranceRow].authoring_placement_id,
      std::optional<uint32_t>(0x6001));
  EXPECT_EQ((*editor_->overworld().exits())[kReturnRow].authoring_placement_id,
            std::optional<uint32_t>(0x6002));
}

TEST_F(NativeEndpointSessionTest, BoundMoveDeleteSaveUndoRedoKeepIdentity) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  BindBoth();
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  auto& world = editor_->overworld();
  const auto bound = world.native_authoring_metadata();
  ASSERT_TRUE(bound.has_value());
  auto& door = world.entrances()[kEntranceRow];
  auto moved_door = door;
  moved_door.x_ += 16;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&door, moved_door).ok());
  auto& back = (*world.mutable_exits())[kReturnRow];
  auto moved_back = back;
  moved_back.y_ += 16;
  ASSERT_TRUE(editor_->ApplyExitEdit(&back, moved_back).ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  const auto moved = world.native_authoring_metadata();
  ASSERT_TRUE(moved.has_value());
  ASSERT_NE(FindPlacement(*moved, 0x6001), nullptr);
  ASSERT_NE(FindPlacement(*moved, 0x6002), nullptr);
  EXPECT_EQ(std::get<rom::NativeAuthoringDoorPlacement>(
                FindPlacement(*moved, 0x6001)->value)
                .x,
            23);
  EXPECT_EQ(std::get<rom::NativeAuthoringReturnPlacement>(
                FindPlacement(*moved, 0x6002)->value)
                .y,
            12);
  auto deleted_door = world.entrances()[kEntranceRow];
  deleted_door.deleted = true;
  ASSERT_TRUE(
      editor_->ApplyEntranceEdit(&world.entrances()[kEntranceRow], deleted_door)
          .ok());
  auto deleted_back = (*world.exits())[kReturnRow];
  deleted_back.deleted_ = true;
  ASSERT_TRUE(
      editor_
          ->ApplyExitEdit(&(*world.mutable_exits())[kReturnRow], deleted_back)
          .ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  const auto deleted = DiskMetadata();
  for (uint32_t id : {0x6001u, 0x6002u}) {
    ASSERT_NE(FindPlacement(deleted, id), nullptr);
    EXPECT_EQ(FindPlacement(deleted, id)->state,
              rom::NativePlacementState::kDeleted);
    EXPECT_EQ(FindPlacement(deleted, id)->value,
              FindPlacement(*moved, id)->value);
  }
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), *moved);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), deleted);
  for (int i = 0; i < 4; ++i)
    ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), *bound);
  EXPECT_EQ(world.entrances()[kEntranceRow].authoring_placement_id,
            std::optional<uint32_t>(0x6001));
  EXPECT_EQ((*world.exits())[kReturnRow].authoring_placement_id,
            std::optional<uint32_t>(0x6002));
}

TEST_F(NativeEndpointSessionTest,
       EntranceBindingDomainIndependentWithSpritesOff) {
  CheckBindingDomain(true);
}

TEST_F(NativeEndpointSessionTest,
       ReturnBindingDomainIndependentWithSpritesOff) {
  CheckBindingDomain(false);
}

TEST_F(NativeEndpointSessionTest,
       EndpointSavePreservesDisabledSpriteReservationAndAllocator) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  BindBoth();
  ASSERT_FALSE(HasFatalFailure());
  auto& world = editor_->overworld();
  auto* service = editor_->entity_mutation_service();
  ASSERT_NE(service, nullptr);
  const auto inserted =
      service->InsertEntity("sprite", ImVec2(32, 3104), 0xB0, 1);
  ASSERT_TRUE(inserted.ok()) << inserted.status;
  const auto identity =
      static_cast<zelda3::Sprite*>(inserted.entity)->placement_id();
  ASSERT_TRUE(identity.has_value());
  const auto reserved_next =
      world.native_authoring_metadata()->next_placement_id;
  const auto partial = manager_->SaveCurrentSession(false);
  EXPECT_EQ(partial.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(partial.message(), "Save left unsaved project or ROM work");
  const auto endpoints_only = DiskMetadata();
  EXPECT_EQ(endpoints_only.schema_version, 2);
  EXPECT_EQ(FindPlacement(endpoints_only, *identity), nullptr);
  EXPECT_NE(FindPlacement(*world.native_authoring_metadata(), *identity),
            nullptr);
  EXPECT_EQ(world.native_authoring_metadata()->next_placement_id,
            reserved_next);
  EXPECT_TRUE(world.HasPendingNativeSpriteChanges());
  EXPECT_FALSE(world.HasPendingNativeEndpointChanges());
  ExpectSpritePayloadUnchanged();
  core::FeatureFlags::get().overworld.kSaveOverworldSprites = true;
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_NE(FindPlacement(DiskMetadata(), *identity), nullptr);
  EXPECT_FALSE(world.HasPendingNativeSpriteChanges());
  ReopenSession();
  ASSERT_FALSE(HasFatalFailure());
  EXPECT_EQ(editor_->overworld().native_authoring_metadata()->next_placement_id,
            reserved_next);
}

TEST_F(NativeEndpointSessionTest,
       BindingRejectsWrongTargetAreaAndTamperedIdentity) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  auto& world = editor_->overworld();
  const auto pending = world.native_authoring_metadata();
  auto& door = world.entrances()[kEntranceRow];
  const auto correct = door;
  auto wrong = door;
  wrong.entrance_id_ = 0x0C;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&door, wrong).ok());
  EXPECT_FALSE(editor_->BindNativeEndpointPlacement(0x6001, kEntranceRow).ok());
  EXPECT_EQ(world.native_authoring_metadata(), pending);
  wrong = correct;
  wrong.map_id_ = 0xA0;
  wrong.y_ = 0x800 + 8 * 16;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&door, wrong).ok());
  EXPECT_FALSE(editor_->BindNativeEndpointPlacement(0x6001, kEntranceRow).ok());
  EXPECT_EQ(world.native_authoring_metadata(), pending);
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&door, correct).ok());
  auto& back = (*world.mutable_exits())[kReturnRow];
  const auto correct_back = back;
  auto wrong_back = back;
  wrong_back.room_id_ = 0x84;
  ASSERT_TRUE(editor_->ApplyExitEdit(&back, wrong_back).ok());
  EXPECT_FALSE(editor_->BindNativeEndpointPlacement(0x6002, kReturnRow).ok());
  EXPECT_EQ(world.native_authoring_metadata(), pending);
  wrong_back = correct_back;
  wrong_back.map_id_ = 0xA0;
  wrong_back.y_ = 0x800 + 11 * 16;
  ASSERT_TRUE(editor_->ApplyExitEdit(&back, wrong_back).ok());
  EXPECT_FALSE(editor_->BindNativeEndpointPlacement(0x6002, kReturnRow).ok());
  EXPECT_EQ(world.native_authoring_metadata(), pending);
  ASSERT_TRUE(editor_->ApplyExitEdit(&back, correct_back).ok());
  BindBoth();
  ASSERT_FALSE(HasFatalFailure());
  auto tampered = door;
  tampered.authoring_placement_id = 0x6002;
  EXPECT_FALSE(editor_->ApplyEntranceEdit(&door, tampered).ok());
  auto tampered_back = back;
  tampered_back.authoring_placement_id = 0x6001;
  EXPECT_FALSE(editor_->ApplyExitEdit(&back, tampered_back).ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  const auto saved = ReadBytes(source_);
  door.authoring_placement_id = 0xDEAD;
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), saved);
  door.authoring_placement_id = 0x6001;
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
}

TEST_F(NativeEndpointSessionTest,
       ReusedDeletedRowsDoNotAcquireTombstoneIdentities) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  BindBoth();
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  auto& world = editor_->overworld();
  auto& door = world.entrances()[kEntranceRow];
  auto deleted = door;
  deleted.deleted = true;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&door, deleted).ok());
  auto& back = (*world.mutable_exits())[kReturnRow];
  auto deleted_back = back;
  deleted_back.deleted_ = true;
  ASSERT_TRUE(editor_->ApplyExitEdit(&back, deleted_back).ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  const auto tombstones = DiskMetadata();
  ReopenSession();
  ASSERT_FALSE(HasFatalFailure());
  auto& reopened = editor_->overworld();
  auto& free_door = reopened.entrances()[kEntranceRow];
  auto& free_back = (*reopened.mutable_exits())[kReturnRow];
  EXPECT_FALSE(free_door.authoring_placement_id.has_value());
  EXPECT_FALSE(free_back.authoring_placement_id.has_value());
  auto replacement = free_door;
  replacement.deleted = false;
  replacement.is_hole_ = false;
  replacement.map_id_ = 0xB0;
  replacement.x_ = 24 * 16;
  replacement.y_ = 0xC00 + 8 * 16;
  replacement.entrance_id_ = 0x0B;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(&free_door, replacement).ok());
  auto return_replacement = free_back;
  return_replacement.deleted_ = false;
  return_replacement.is_hole_ = false;
  return_replacement.map_id_ = 0xB0;
  return_replacement.x_ = 25 * 16;
  return_replacement.y_ = 0xC00 + 11 * 16;
  return_replacement.room_id_ = 0x83;
  ASSERT_TRUE(editor_->ApplyExitEdit(&free_back, return_replacement).ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(DiskMetadata(), tombstones);
  ReopenSession();
  ASSERT_FALSE(HasFatalFailure());
  EXPECT_FALSE(editor_->overworld()
                   .entrances()[kEntranceRow]
                   .authoring_placement_id.has_value());
  EXPECT_FALSE((*editor_->overworld().exits())[kReturnRow]
                   .authoring_placement_id.has_value());
}

class NativeEndpointDuplicateBindingSessionTest
    : public NativeEndpointSessionTest {
 protected:
  void CustomizeMetadata(
      rom::NativeOverworldAuthoringMetadata& metadata) override {
    metadata.next_placement_id = 0x6004;
    metadata.placements.push_back(
        {0x6003, rom::NativePlacementState::kPending,
         rom::NativeAuthoringDoorPlacement{0xB0, 22, 8, 0x0B, std::nullopt}});
  }
};

TEST_F(NativeEndpointDuplicateBindingSessionTest,
       DifferentIdentityCannotClaimBoundRow) {
  PrepareBindingRows();
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(editor_->BindNativeEndpointPlacement(0x6001, kEntranceRow).ok());
  const auto bound = editor_->overworld().native_authoring_metadata();
  EXPECT_FALSE(editor_->BindNativeEndpointPlacement(0x6003, kEntranceRow).ok());
  EXPECT_EQ(editor_->overworld().native_authoring_metadata(), bound);
  EXPECT_EQ(
      editor_->overworld().entrances()[kEntranceRow].authoring_placement_id,
      std::optional<uint32_t>(0x6001));
}

TEST_F(NativeEndpointSessionTest, HistoryFailureRetryAndFreshReopen) {
  auto& world = editor_->overworld();
  const auto old_door = world.entrances()[kEntranceRow];
  const auto old_return = (*world.exits())[kReturnRow];
  MoveEndpoints();
  ASSERT_FALSE(HasFatalFailure());
  ASSERT_TRUE(world.HasPendingNativeEndpointChanges());
  const auto ranges = world.GetProjectedWriteRanges();
  auto covered = [&](uint32_t pc) {
    return std::any_of(ranges.begin(), ranges.end(), [pc](const auto& range) {
      return range.first <= pc && pc < range.second;
    });
  };
  EXPECT_TRUE(covered(zelda3::kOverworldEntranceMap + kEntranceRow * 2));
  EXPECT_TRUE(covered(zelda3::OWExitMapId + kReturnRow));
  const auto memory_before_failure = manager_->GetCurrentRom()->vector();
  {
    test::ScopedRomStagingFailure fault;
    EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  }
  EXPECT_EQ(ReadBytes(source_), before_);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before_failure);
  EXPECT_TRUE(world.HasPendingNativeEndpointChanges());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  ExpectDiskMaps(0xA0, 0xA0);
  auto* entrance = &world.entrances()[kEntranceRow];
  auto deleted_door = *entrance;
  deleted_door.deleted = true;
  ASSERT_TRUE(editor_->ApplyEntranceEdit(entrance, deleted_door).ok());
  auto* exit = &(*world.mutable_exits())[kReturnRow];
  auto deleted_return = *exit;
  deleted_return.deleted_ = true;
  ASSERT_TRUE(editor_->ApplyExitEdit(exit, deleted_return).ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  Rom deleted;
  ASSERT_TRUE(deleted.LoadFromFile(source_.string()).ok());
  const auto deleted_doors = zelda3::LoadEntrances(&deleted, {});
  const auto deleted_returns = zelda3::LoadExits(&deleted);
  ASSERT_TRUE(deleted_doors.ok());
  ASSERT_TRUE(deleted_returns.ok());
  EXPECT_TRUE((*deleted_doors)[kEntranceRow].deleted);
  EXPECT_TRUE((*deleted_returns)[kReturnRow].deleted_);
  EXPECT_EQ((*deleted_returns)[kReturnRow].room_id_, 0xFFFF);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  ExpectDiskMaps(0xA0, 0xA0);
  EXPECT_FALSE(world.HasPendingNativeEndpointChanges());
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(world.HasPendingNativeEndpointChanges());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  ExpectDiskMaps(old_door.map_id_, old_return.map_id_);
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  ExpectDiskMaps(0xA0, 0xA0);
}

TEST_F(NativeEndpointSessionTest, DisabledSaveDomainsRemainPending) {
  const auto old_door = editor_->overworld().entrances()[kEntranceRow];
  MoveEndpoints();
  ASSERT_FALSE(HasFatalFailure());
  auto& flags = core::FeatureFlags::get().overworld;
  flags.kSaveOverworldEntrances = false;
  const auto partial_return = manager_->SaveCurrentSession(false);
  EXPECT_EQ(partial_return.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(partial_return.message(), "Save left unsaved project or ROM work");
  ExpectDiskMaps(old_door.map_id_, 0xA0);
  EXPECT_FALSE(manager_->GetCurrentRom()->dirty());
  manager_->CloseRom();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->CancelPendingUnsavedSessionAction();
  flags.kSaveOverworldEntrances = true;
  flags.kSaveOverworldExits = false;
  auto& row = (*editor_->overworld().mutable_exits())[kReturnRow];
  auto draft = row;
  draft.x_ += 16;
  ASSERT_TRUE(editor_->ApplyExitEdit(&row, draft).ok());
  const auto partial_entrance = manager_->SaveCurrentSession(false);
  EXPECT_EQ(partial_entrance.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(partial_entrance.message(),
            "Save left unsaved project or ROM work");
  ExpectDiskMaps(0xA0, 0xA0);
  manager_->CloseRom();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->CancelPendingUnsavedSessionAction();
  flags.kSaveOverworldExits = true;
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  EXPECT_FALSE(editor_->overworld().HasPendingNativeEndpointChanges());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(source_.string()).ok());
  const auto returns = zelda3::LoadExits(&reopened);
  ASSERT_TRUE(returns.ok()) << returns.status();
  EXPECT_EQ((*returns)[kReturnRow].x_player_, draft.x_);
}

TEST_F(NativeEndpointSessionTest, UnsupportedEditsRefuseWithoutLosingWork) {
  auto& world = editor_->overworld();
  auto& hole = (*world.mutable_holes())[0];
  const auto old_hole = hole;
  hole.entrance_id_++;
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), before_);
  manager_->CloseRom();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->CancelPendingUnsavedSessionAction();
  hole = old_hole;
  auto& door = world.entrances()[kEntranceRow];
  const auto old_door = door;
  door.x_ += 16;  // An unencoded drag must not be acknowledged as saved.
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), before_);
  door = old_door;
  auto& return_row = (*world.mutable_exits())[kReturnRow];
  const auto old_return = return_row;
  return_row.room_id_ ^= 1;
  return_row.game_x_++;
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), before_);
  return_row = old_return;
  return_row.room_id_ ^= 1;
  return_row.is_hole_ = !return_row.is_hole_;
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), before_);
  return_row = old_return;
  return_row.map_id_ = 0x1A0;
  return_row.room_id_ ^= 1;
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), before_);
  return_row = old_return;
  MoveEndpoints();
  ASSERT_FALSE(HasFatalFailure());
  const auto old_options = world.layout_inspect_options();
  auto moved = old_options;
  moved.entrance_expanded_flag_pc = 0x200000;
  world.set_layout_inspect_options(moved);
  EXPECT_FALSE(manager_->SaveCurrentSession(false).ok());
  EXPECT_EQ(ReadBytes(source_), before_);
  world.set_layout_inspect_options(old_options);
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  ExpectDiskMaps(0xA0, 0xA0);
}

}  // namespace
}  // namespace yaze
