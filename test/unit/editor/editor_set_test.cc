#include <gtest/gtest.h>

#include <memory>

#include "app/editor/code/memory_editor.h"
#include "app/editor/editor.h"
#include "app/editor/session_types.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/system/editor_registry.h"
#include "rom/rom.h"

namespace yaze::editor {
namespace {

class UnitTest_EditorSet : public ::testing::Test {
 protected:
  void SetUp() override {
    // Basic setup if needed
  }
};

TEST_F(UnitTest_EditorSet, GenericContainerOperations) {
  EditorSet editor_set;

  // Verify core editors are present
  EXPECT_NE(editor_set.GetEditor(EditorType::kAssembly), nullptr);
  EXPECT_NE(editor_set.GetEditor(EditorType::kDungeon), nullptr);
  EXPECT_NE(editor_set.GetEditor(EditorType::kOverworld), nullptr);

  // Verify type-safe access
  EXPECT_NE(editor_set.GetEditorAs<AssemblyEditor>(EditorType::kAssembly),
            nullptr);

  // Verify unknown type returns nullptr
  EXPECT_EQ(editor_set.GetEditor(EditorType::kUnknown), nullptr);
}

TEST_F(UnitTest_EditorSet, ActiveEditorsList) {
  EditorSet editor_set;
  EXPECT_FALSE(editor_set.active_editors_.empty());

  // Verify some expected editors are in the active list
  bool found_dungeon = false;
  for (Editor* editor : editor_set.active_editors_) {
    ASSERT_NE(editor, nullptr);
    if (editor->type() == EditorType::kDungeon) {
      found_dungeon = true;
      break;
    }
  }
  EXPECT_TRUE(found_dungeon);
}

TEST_F(UnitTest_EditorSet, ApplyDependenciesSetsRomAndWiresMemoryEditor) {
  EditorSet editor_set;

  Rom rom;
  EditorDependencies deps;
  deps.rom = &rom;

  editor_set.ApplyDependencies(deps);

  // Base Editor dependency should be set for all editors.
  EXPECT_EQ(editor_set.GetEditor(EditorType::kAssembly)->rom(), &rom);
  EXPECT_EQ(editor_set.GetEditor(EditorType::kDungeon)->rom(), &rom);
  EXPECT_EQ(editor_set.GetEditor(EditorType::kOverworld)->rom(), &rom);

  // MemoryEditor also has an internal ROM pointer that must be updated.
  ASSERT_NE(editor_set.GetMemoryEditor(), nullptr);
  EXPECT_EQ(editor_set.GetMemoryEditor()->rom(), &rom);
}

TEST_F(UnitTest_EditorSet, UsesEditorRegistryFactoriesWhenProvided) {
  Rom rom;
  EditorRegistry registry;
  bool factory_called = false;

  registry.RegisterFactory(EditorType::kAssembly,
                           [&factory_called](Rom* arg_rom) {
                             factory_called = true;
                             return std::make_unique<AssemblyEditor>(arg_rom);
                           });

  EditorSet editor_set(&rom, /*game_data=*/nullptr, /*user_settings=*/nullptr,
                       /*session_id=*/0, &registry);

  EXPECT_TRUE(factory_called);
  EXPECT_NE(editor_set.GetEditor(EditorType::kAssembly), nullptr);
}

}  // namespace
}  // namespace yaze::editor
