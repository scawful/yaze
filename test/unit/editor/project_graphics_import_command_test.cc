#include "app/editor/overworld/project_graphics_import_command.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/registry/undo_manager.h"
#include "app/gfx/util/palette_manager.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

struct DefinitionParticipant {
  int value = 0;
  int redos = 0;
  int undos = 0;
  bool fail_redo = false;
  bool fail_undo = false;
};

class DefinitionAction final : public UndoAction {
 public:
  DefinitionAction(DefinitionParticipant* state,
                   std::vector<std::string>* events)
      : state_(state), events_(events) {}
  absl::Status Redo() override {
    ++state_->redos;
    events_->push_back("definitions redo");
    if (std::exchange(state_->fail_redo, false))
      return absl::InternalError("definition failure");
    state_->value = 1;
    return absl::OkStatus();
  }
  absl::Status Undo() override {
    ++state_->undos;
    events_->push_back("definitions undo");
    if (std::exchange(state_->fail_undo, false))
      return absl::InternalError("definition rollback failure");
    state_->value = 0;
    return absl::OkStatus();
  }
  std::string Description() const override { return "Prepared definitions"; }

 private:
  DefinitionParticipant* state_;
  std::vector<std::string>* events_;
};

class ProjectGraphicsImportCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto& palettes = gfx::PaletteManager::Get();
    palettes.ResetForTesting();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
    auto* group = data_.palette_groups.get_group("dungeon_main");
    ASSERT_NE(group, nullptr);
    group->clear();
    gfx::SnesPalette palette;
    for (int i = 0; i < 16; ++i)
      palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(0x1000 + i)));
    group->AddPalette(palette);
    palettes.Initialize(&data_);
    plan_.resource_before.assign(64, 0);
    plan_.resource_after.assign(64, 1);
    bytes_ = plan_.resource_before;
    plan_.palette_changes.push_back({"dungeon_main", 0, 0,
                                     gfx::SnesColor(0x1000),
                                     gfx::SnesColor(0x2222), 0});
    listener_ = palettes.RegisterChangeListener([&](const auto&) {
      events_.push_back("palette notification");
      notification_resources_.push_back(bytes_);
      notification_definitions_.push_back(definitions_.value);
    });
  }

  void TearDown() override {
    gfx::PaletteManager::Get().UnregisterChangeListener(listener_);
    gfx::PaletteManager::Get().ResetForTesting();
  }

  ProjectGraphicsImportResourceCallbacks ResourceCallbacks() {
    ProjectGraphicsImportResourceCallbacks callbacks;
    callbacks.read = [&]() -> absl::StatusOr<std::vector<uint8_t>> {
      read_paths_.push_back(current_path_);
      return bytes_;
    };
    callbacks.replace = [&](const std::vector<uint8_t>& bytes) {
      write_paths_.push_back(current_path_);
      events_.push_back("resource replacement");
      if (failed_writes_.contains(++writes_))
        return absl::InternalError("resource failure");
      bytes_ = bytes;
      return absl::OkStatus();
    };
    return callbacks;
  }

  absl::Status Refresh() {
    events_.push_back("refresh");
    if (failed_refreshes_.contains(++refreshes_))
      return absl::InternalError("refresh failure");
    visible_color_ =
        gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes();
    visible_resource_ = bytes_;
    visible_definitions_ = definitions_.value;
    return absl::OkStatus();
  }

  absl::StatusOr<std::unique_ptr<UndoAction>> Prepare() {
    return CreateProjectGraphicsImportAction(
        ResourceCallbacks(), plan_,
        std::make_unique<DefinitionAction>(&definitions_, &events_), &data_,
        [&] { return Refresh(); }, "Import boat graphics");
  }

  void ExpectOriginal() {
    EXPECT_EQ(bytes_, plan_.resource_before);
    EXPECT_EQ(definitions_.value, 0);
    EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes(),
              0x1000);
    EXPECT_EQ(gfx::PaletteManager::Get().GetModifiedColorCount(), 0);
    EXPECT_EQ(gfx::PaletteManager::Get().GetUndoStackSize(), 0u);
  }

  Rom rom_;
  zelda3::GameData data_{&rom_};
  ProjectGraphicsImportPlan plan_;
  DefinitionParticipant definitions_;
  std::vector<uint8_t> bytes_, visible_resource_;
  std::vector<std::string> events_, read_paths_, write_paths_;
  std::vector<std::vector<uint8_t>> notification_resources_;
  std::vector<int> notification_definitions_;
  std::set<int> failed_writes_, failed_refreshes_;
  std::string current_path_ = "original-hash.4bpp";
  uint16_t visible_color_ = 0x1000;
  int visible_definitions_ = 0;
  int writes_ = 0;
  int refreshes_ = 0;
  int listener_ = 0;
};

TEST_F(ProjectGraphicsImportCommandTest,
       PreparedActionUsesOnlyParentHistoryAndNotifiesAfterAllParticipants) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  ASSERT_NE(*action, nullptr);
  ExpectOriginal();
  EXPECT_TRUE(events_.empty());
  EXPECT_EQ(refreshes_, 0);
  EXPECT_EQ(definitions_.redos, 0);
  EXPECT_EQ((*action)->Description(), "Import boat graphics");
  ASSERT_TRUE((*action)->Redo().ok());
  EXPECT_EQ(bytes_, plan_.resource_after);
  EXPECT_EQ(definitions_.value, 1);
  EXPECT_EQ(visible_color_, 0x2222);
  ASSERT_EQ(notification_resources_.size(), 1u);
  EXPECT_EQ(notification_resources_[0], plan_.resource_after);
  EXPECT_EQ(notification_definitions_[0], 1);
  EXPECT_EQ(events_.back(), "palette notification");
  EXPECT_EQ(gfx::PaletteManager::Get().GetUndoStackSize(), 0u);
  UndoManager parent;
  parent.Push(std::move(*action));
  ASSERT_EQ(parent.UndoStackSize(), 1u);
  ASSERT_TRUE(parent.Undo().ok());
  EXPECT_EQ(bytes_, plan_.resource_before);
  EXPECT_EQ(definitions_.value, 0);
  EXPECT_EQ(visible_color_, 0x1000);
  ASSERT_TRUE(parent.Redo().ok());
  EXPECT_EQ(bytes_, plan_.resource_after);
  EXPECT_EQ(definitions_.value, 1);
  EXPECT_EQ(parent.UndoStackSize(), 1u);
}

TEST_F(ProjectGraphicsImportCommandTest,
       ResourceFailureRestoresPaletteTrackingWithoutApplyingDefinitions) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  const auto revision = gfx::PaletteManager::Get().ColorRevision(&data_);
  failed_writes_.insert(1);
  EXPECT_FALSE((*action)->Redo().ok());
  ExpectOriginal();
  EXPECT_EQ(gfx::PaletteManager::Get().ColorRevision(&data_), revision);
  EXPECT_EQ(definitions_.redos, 0);
  EXPECT_TRUE(notification_resources_.empty());
  EXPECT_EQ(visible_color_, 0x1000);
}

TEST_F(ProjectGraphicsImportCommandTest,
       DefinitionFailureRestoresResourcePaletteAndView) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  definitions_.fail_redo = true;
  EXPECT_FALSE((*action)->Redo().ok());
  ExpectOriginal();
  EXPECT_EQ(definitions_.undos, 0);
  EXPECT_EQ(visible_resource_, plan_.resource_before);
  EXPECT_EQ(visible_color_, 0x1000);
  EXPECT_TRUE(notification_resources_.empty());
}

TEST_F(ProjectGraphicsImportCommandTest,
       LateRefreshFailureReversesEveryParticipantBeforeReturning) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  failed_refreshes_.insert(2);
  EXPECT_FALSE((*action)->Redo().ok());
  ExpectOriginal();
  EXPECT_EQ(definitions_.redos, 1);
  EXPECT_EQ(definitions_.undos, 1);
  EXPECT_EQ(refreshes_, 3);
  EXPECT_EQ(visible_resource_, plan_.resource_before);
  EXPECT_EQ(visible_definitions_, 0);
  EXPECT_EQ(visible_color_, 0x1000);
  EXPECT_TRUE(notification_resources_.empty());
}

TEST_F(ProjectGraphicsImportCommandTest,
       FailedUndoKeepsAppliedStateAndParentHistory) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  ASSERT_TRUE((*action)->Redo().ok());
  UndoManager parent;
  parent.Push(std::move(*action));
  notification_resources_.clear();
  failed_refreshes_.insert(4);
  EXPECT_FALSE(parent.Undo().ok());
  EXPECT_EQ(bytes_, plan_.resource_after);
  EXPECT_EQ(definitions_.value, 1);
  EXPECT_EQ(visible_color_, 0x2222);
  EXPECT_EQ(visible_resource_, plan_.resource_after);
  EXPECT_TRUE(notification_resources_.empty());
  EXPECT_EQ(parent.UndoStackSize(), 1u);
  EXPECT_EQ(parent.RedoStackSize(), 0u);
}

TEST_F(ProjectGraphicsImportCommandTest,
       CurrentBindingCallbacksSurvivePublishedResourcePathChange) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  ASSERT_TRUE((*action)->Redo().ok());
  current_path_ = "published-hash.4bpp";
  ASSERT_TRUE((*action)->Undo().ok());
  EXPECT_EQ(read_paths_.back(), current_path_);
  EXPECT_EQ(write_paths_.back(), current_path_);
  EXPECT_EQ(bytes_, plan_.resource_before);
  ASSERT_TRUE((*action)->Redo().ok());
  EXPECT_EQ(write_paths_.back(), current_path_);
  EXPECT_EQ(bytes_, plan_.resource_after);
}

TEST_F(ProjectGraphicsImportCommandTest,
       StaleResourceAndPaletteRefuseBeforeAnyMutation) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  bytes_[0] = 7;
  EXPECT_FALSE((*action)->Redo().ok());
  EXPECT_EQ(writes_, 0);
  EXPECT_EQ(refreshes_, 0);
  EXPECT_EQ(definitions_.redos, 0);
  bytes_ = plan_.resource_before;
  ASSERT_TRUE(gfx::PaletteManager::Get()
                  .SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x3333))
                  .ok());
  events_.clear();
  EXPECT_FALSE((*action)->Redo().ok());
  EXPECT_TRUE(events_.empty());
  EXPECT_EQ(writes_, 0);
  EXPECT_EQ(definitions_.redos, 0);
}

TEST_F(ProjectGraphicsImportCommandTest,
       RollbackFailureIsReportedAndDisablesFurtherReplay) {
  auto action = Prepare();
  ASSERT_TRUE(action.ok());
  failed_refreshes_.insert(2);
  failed_writes_.insert(2);
  const auto failed = (*action)->Redo();
  EXPECT_FALSE(failed.ok());
  EXPECT_NE(std::string(failed.message()).find("resource rollback"),
            std::string::npos);
  const int writes = writes_;
  EXPECT_FALSE((*action)->Redo().ok());
  EXPECT_FALSE((*action)->Undo().ok());
  EXPECT_EQ(writes_, writes);
  EXPECT_TRUE(notification_resources_.empty());
}

}  // namespace
}  // namespace yaze::editor
