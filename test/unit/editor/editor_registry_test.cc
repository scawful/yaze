#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "app/editor/editor.h"
#include "app/editor/system/editor_registry.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {

class FakeEditor final : public Editor {
 public:
  explicit FakeEditor(EditorType type) { type_ = type; }

  void Initialize() override {}
  absl::Status Load() override { return absl::OkStatus(); }
  absl::Status Save() override { return absl::OkStatus(); }
  absl::Status Update() override { return absl::OkStatus(); }

  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Find() override { return absl::OkStatus(); }
};

class UnitTest_EditorRegistry : public ::testing::Test {
 protected:
  EditorRegistry registry_;
};

TEST_F(UnitTest_EditorRegistry, RegisterGetUnregister) {
  FakeEditor overworld(EditorType::kOverworld);

  registry_.RegisterEditor(EditorType::kOverworld, &overworld);
  EXPECT_EQ(registry_.GetEditor(EditorType::kOverworld), &overworld);

  registry_.UnregisterEditor(EditorType::kOverworld);
  EXPECT_EQ(registry_.GetEditor(EditorType::kOverworld), nullptr);
}

TEST_F(UnitTest_EditorRegistry, ActiveStateReflectsEditor) {
  FakeEditor dungeon(EditorType::kDungeon);
  registry_.RegisterEditor(EditorType::kDungeon, &dungeon);

  EXPECT_FALSE(registry_.IsEditorActive(EditorType::kDungeon));

  registry_.SetEditorActive(EditorType::kDungeon, true);
  EXPECT_TRUE(registry_.IsEditorActive(EditorType::kDungeon));

  registry_.SetEditorActive(EditorType::kDungeon, false);
  EXPECT_FALSE(registry_.IsEditorActive(EditorType::kDungeon));
}

TEST_F(UnitTest_EditorRegistry, DefaultContextSnapshotUsesEditorIdentity) {
  FakeEditor dungeon(EditorType::kDungeon);
  const EditorContextSnapshot snapshot = dungeon.BuildContextSnapshot();
  EXPECT_EQ(snapshot.category, "Dungeon");
  EXPECT_EQ(snapshot.title, "Dungeon");
  EXPECT_EQ(snapshot.semantic_owner, "Dungeon");
}

TEST_F(UnitTest_EditorRegistry, InvalidTypesThrow) {
  FakeEditor unknown(EditorType::kUnknown);

  EXPECT_THROW(registry_.RegisterEditor(EditorType::kUnknown, &unknown),
               std::invalid_argument);
  EXPECT_THROW(registry_.GetEditor(EditorType::kUnknown),
               std::invalid_argument);
  EXPECT_THROW(registry_.IsEditorActive(EditorType::kUnknown),
               std::invalid_argument);
}

TEST_F(UnitTest_EditorRegistry, NullEditorPointerThrows) {
  EXPECT_THROW(registry_.RegisterEditor(EditorType::kOverworld, nullptr),
               std::invalid_argument);
}

TEST_F(UnitTest_EditorRegistry, CreateEditorWithoutFactoryReturnsNullptr) {
  Rom rom;
  EXPECT_EQ(registry_.CreateEditor(EditorType::kOverworld, &rom), nullptr);
}

TEST_F(UnitTest_EditorRegistry, FactoryRegistrationAndExecution) {
  Rom rom;
  Rom* seen_rom = nullptr;
  registry_.RegisterFactory(EditorType::kOverworld, [&seen_rom](Rom* arg_rom) {
    seen_rom = arg_rom;
    return std::make_unique<FakeEditor>(EditorType::kOverworld);
  });

  auto editor = registry_.CreateEditor(EditorType::kOverworld, &rom);
  ASSERT_NE(editor, nullptr);
  EXPECT_EQ(seen_rom, &rom);
  EXPECT_EQ(editor->type(), EditorType::kOverworld);
}

TEST_F(UnitTest_EditorRegistry, NullFactoryThrows) {
  EditorRegistry::EditorFactory factory;
  EXPECT_THROW(registry_.RegisterFactory(EditorType::kOverworld, factory),
               std::invalid_argument);
}

TEST_F(UnitTest_EditorRegistry,
       ExperimentalEditorsWarnWithoutBecomingVisibilityGates) {
  const auto stable = EditorRegistry::GetExperimentPolicy(EditorType::kDungeon);
  EXPECT_FALSE(stable.experimental);
  EXPECT_FALSE(EditorRegistry::ShouldWarnAboutExperimentalEditor(
      EditorType::kDungeon, false));

  const auto screen = EditorRegistry::GetExperimentPolicy(EditorType::kScreen);
  EXPECT_TRUE(screen.experimental);
  EXPECT_EQ(screen.disabled_save_posture, ExperimentalSavePosture::kReadOnly);

  const auto music = EditorRegistry::GetExperimentPolicy(EditorType::kMusic);
  EXPECT_TRUE(music.experimental);
  EXPECT_EQ(music.disabled_save_posture,
            ExperimentalSavePosture::kDefensiveBackup);

  const auto agent = EditorRegistry::GetExperimentPolicy(EditorType::kAgent);
  EXPECT_TRUE(agent.experimental);
  EXPECT_EQ(agent.disabled_save_posture, ExperimentalSavePosture::kNormal);

  EXPECT_TRUE(EditorRegistry::ShouldWarnAboutExperimentalEditor(
      EditorType::kMusic, false));
  EXPECT_FALSE(EditorRegistry::ShouldWarnAboutExperimentalEditor(
      EditorType::kMusic, true));
}

}  // namespace
}  // namespace yaze::editor
