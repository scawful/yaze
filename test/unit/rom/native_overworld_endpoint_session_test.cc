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
    auto& flags = core::FeatureFlags::get().overworld;
    flags.kSaveOverworldMaps = false;
    flags.kSaveOverworldSprites = false;
    flags.kSaveOverworldProperties = true;
    flags.kSaveOverworldEntrances = true;
    flags.kSaveOverworldExits = true;
    Rom legacy;
    ASSERT_TRUE(legacy.LoadFromFile(input).ok());
    zelda3::Overworld reference(&legacy);
    ASSERT_TRUE(reference.Load(&legacy).ok());
    NativeDirectoryFixture fixture;
    fixture.SeedLoadedLegacyForModelTest(legacy, reference);
    fixture.UpgradeToRevision4();
    fixture.SeedCanonicalPlacementRows();
    fixture.Entry(100).count = 512;
    fixture.Seal();
    before_ = fixture.bytes;
    metadata_pc_ = fixture.Entry(412).pc_address;
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
  std::unique_ptr<editor::EditorManager> manager_;
  editor::OverworldEditor* editor_ = nullptr;
};

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
