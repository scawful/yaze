#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/menu/menu_shortcut_labels.h"
#include "app/editor/menu/recent_files_menu_model.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "app/gui/core/platform_keys.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

// --- ShortcutLabelForAction --------------------------------------------------

TEST(MenuShortcutLabelsTest, LabelComesFromLiveBinding) {
  ShortcutManager shortcuts;
  shortcuts.RegisterShortcut("Redo",
                             {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z});
  shortcuts.RegisterShortcut("Global Search",
                             {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_K});
  shortcuts.RegisterShortcut("Save Layout",
                             {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_L});

  // These were previously hard-coded wrong in the menu (Ctrl+Y, Ctrl+Shift+F,
  // Ctrl+Shift+S). The label must match the binding that actually fires.
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Redo"),
            gui::FormatCtrlShiftShortcut(ImGuiKey_Z));
  EXPECT_NE(ShortcutLabelForAction(&shortcuts, "Redo"),
            gui::FormatCtrlShortcut(ImGuiKey_Y));
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Global Search"),
            gui::FormatCtrlShiftShortcut(ImGuiKey_K));
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Save Layout"),
            gui::FormatCtrlShiftShortcut(ImGuiKey_L));
}

TEST(MenuShortcutLabelsTest, RebindingChangesLabel) {
  ShortcutManager shortcuts;
  shortcuts.RegisterShortcut("Save As",
                             {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_S});
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Save As"),
            gui::FormatCtrlShiftShortcut(ImGuiKey_S));

  ASSERT_TRUE(
      shortcuts.UpdateShortcutKeys("Save As", {ImGuiMod_Ctrl, ImGuiKey_F12}));
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Save As"),
            gui::FormatCtrlShortcut(ImGuiKey_F12));
}

TEST(MenuShortcutLabelsTest, UnknownUnboundOrMissingManagerIsEmpty) {
  ShortcutManager shortcuts;
  shortcuts.RegisterCommand("Palette Only", []() {});

  EXPECT_EQ(ShortcutLabelForAction(nullptr, "Redo"), "");
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Not Registered"), "");
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, "Palette Only"), "");
  EXPECT_EQ(ShortcutLabelForAction(&shortcuts, ""), "");
}

// --- BuildRecentFileMenuEntries ----------------------------------------------

TEST(RecentFilesMenuModelTest, UsesFileNamesInMruOrder) {
  const auto entries =
      BuildRecentFileMenuEntries({"/roms/a.sfc", "/projects/hack/hack.yaze"},
                                 10, [](const std::string&) { return true; });
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].label, "a.sfc");
  EXPECT_EQ(entries[0].path, "/roms/a.sfc");
  EXPECT_EQ(entries[1].label, "hack.yaze");
  EXPECT_TRUE(entries[0].exists);
}

TEST(RecentFilesMenuModelTest, DisambiguatesDuplicateFileNames) {
  const auto entries = BuildRecentFileMenuEntries(
      {"/work/oracle/Roms/oos.sfc", "/backup/Old/oos.sfc", "/x/other.sfc"}, 10,
      [](const std::string&) { return true; });
  ASSERT_EQ(entries.size(), 3u);
  EXPECT_EQ(entries[0].label, "oos.sfc (Roms)");
  EXPECT_EQ(entries[1].label, "oos.sfc (Old)");
  EXPECT_EQ(entries[2].label, "other.sfc");
}

TEST(RecentFilesMenuModelTest, SameNameAndParentFallsBackToFullPath) {
  const auto entries =
      BuildRecentFileMenuEntries({"/a/Roms/oos.sfc", "/b/Roms/oos.sfc"}, 10,
                                 [](const std::string&) { return true; });
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].label, "/a/Roms/oos.sfc");
  EXPECT_EQ(entries[1].label, "/b/Roms/oos.sfc");
}

TEST(RecentFilesMenuModelTest, LabelsAreUniqueForImGuiIds) {
  const auto entries = BuildRecentFileMenuEntries(
      {"/a/x.sfc", "/b/x.sfc", "/a/y.sfc", "/c/b/x.sfc"}, 10,
      [](const std::string&) { return true; });
  std::set<std::string> labels;
  for (const auto& entry : entries) {
    EXPECT_TRUE(labels.insert(entry.label).second) << entry.label;
  }
}

TEST(RecentFilesMenuModelTest, CapsSkipsEmptyAndDuplicatePaths) {
  const auto entries = BuildRecentFileMenuEntries(
      {"", "/r/1.sfc", "/r/1.sfc", "/r/2.sfc", "/r/3.sfc"}, 2,
      [](const std::string&) { return true; });
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].path, "/r/1.sfc");
  EXPECT_EQ(entries[1].path, "/r/2.sfc");
}

TEST(RecentFilesMenuModelTest, MarksMissingFiles) {
  const auto entries = BuildRecentFileMenuEntries(
      {"/r/here.sfc", "/r/gone.sfc"}, 10,
      [](const std::string& path) { return path == "/r/here.sfc"; });
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_TRUE(entries[0].exists);
  EXPECT_FALSE(entries[1].exists);
}

TEST(RecentFilesMenuModelTest, EmptyListYieldsNoEntries) {
  EXPECT_TRUE(BuildRecentFileMenuEntries({}).empty());
}

// --- Editor::Can* defaults ---------------------------------------------------

class MinimalEditor : public Editor {
 public:
  void Initialize() override {}
  absl::Status Load() override { return absl::OkStatus(); }
  absl::Status Save() override { return absl::OkStatus(); }
  absl::Status Update() override { return absl::OkStatus(); }
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Undo() override { return undo_manager_.Undo(); }
  absl::Status Redo() override { return undo_manager_.Redo(); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  UndoManager& mutable_undo_manager() { return undo_manager_; }
};

class NoopAction : public UndoAction {
 public:
  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  std::string Description() const override { return "noop"; }
};

TEST(EditorCanDefaultsTest, FindIsOptInAndClipboardStaysEnabled) {
  MinimalEditor editor;
  EXPECT_FALSE(editor.CanFind());
  EXPECT_TRUE(editor.CanCut());
  EXPECT_TRUE(editor.CanCopy());
  EXPECT_TRUE(editor.CanPaste());
}

TEST(EditorCanDefaultsTest, UndoRedoFollowUndoManager) {
  MinimalEditor editor;
  EXPECT_FALSE(editor.CanUndo());
  EXPECT_FALSE(editor.CanRedo());

  editor.mutable_undo_manager().Push(std::make_unique<NoopAction>());
  EXPECT_TRUE(editor.CanUndo());
  EXPECT_FALSE(editor.CanRedo());

  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_FALSE(editor.CanUndo());
  EXPECT_TRUE(editor.CanRedo());
}

}  // namespace
}  // namespace yaze::editor
