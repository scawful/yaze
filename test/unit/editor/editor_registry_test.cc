#include <gtest/gtest.h>

#include <stdexcept>

#include "app/editor/editor.h"
#include "app/editor/system/editor_registry.h"

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

TEST_F(UnitTest_EditorRegistry, SwitchToEditorDeactivatesOtherEditors) {
  FakeEditor overworld(EditorType::kOverworld);
  FakeEditor dungeon(EditorType::kDungeon);
  registry_.RegisterEditor(EditorType::kOverworld, &overworld);
  registry_.RegisterEditor(EditorType::kDungeon, &dungeon);

  overworld.set_active(true);
  dungeon.set_active(true);

  registry_.SwitchToEditor(EditorType::kDungeon);
  EXPECT_TRUE(*dungeon.active());
  EXPECT_FALSE(*overworld.active());
}

TEST_F(UnitTest_EditorRegistry, InvalidTypesThrow) {
  FakeEditor unknown(EditorType::kUnknown);

  EXPECT_THROW(registry_.RegisterEditor(EditorType::kUnknown, &unknown),
               std::invalid_argument);
  EXPECT_THROW(registry_.GetEditor(EditorType::kUnknown), std::invalid_argument);
  EXPECT_THROW(registry_.IsEditorActive(EditorType::kUnknown),
               std::invalid_argument);
  EXPECT_THROW(registry_.SwitchToEditor(EditorType::kUnknown),
               std::invalid_argument);
}

TEST_F(UnitTest_EditorRegistry, NullEditorPointerThrows) {
  EXPECT_THROW(registry_.RegisterEditor(EditorType::kOverworld, nullptr),
               std::invalid_argument);
}

}  // namespace
}  // namespace yaze::editor

