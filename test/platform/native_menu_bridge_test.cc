#include "app/platform/native_menu_bridge.h"

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace platform {
namespace native_menu {
namespace {

KeyChord Chord(char key, bool command, bool shift = false, bool option = false,
               bool control = false) {
  KeyChord chord;
  chord.key = key;
  chord.command = command;
  chord.shift = shift;
  chord.option = option;
  chord.control = control;
  return chord;
}

// The name shortcut_configurator.cc registers the Settings editor switch
// under (default Ctrl/Cmd+,), computed the same way.
std::string SettingsShortcutName() {
  return absl::StrFormat("switch.%d",
                         static_cast<int>(editor::EditorType::kSettings));
}

class FakeHost : public NativeMenuHost {
 public:
  explicit FakeHost(editor::ShortcutManager* shortcuts)
      : shortcuts_(shortcuts) {}

  editor::ShortcutManager* shortcut_manager() override { return shortcuts_; }
  bool HasLoadedRom() const override { return rom_loaded; }
  bool HasCurrentEditor() const override { return has_editor; }
  bool CanUndo() const override { return has_editor; }
  bool CanRedo() const override { return has_editor; }
  bool WantsTextInput() const override { return text_input; }
  void Defer(std::function<void()> action) override {
    deferred.push_back(std::move(action));
  }
  void InjectCommandChord(ImGuiKey key) override { injected.push_back(key); }
  void RunHostAction(MenuAction action) override {
    host_actions.push_back(action);
  }
  std::vector<std::string> RecentFiles() const override { return {}; }
  void OpenRecentFile(const std::string&) override {}
  void ClearRecentFiles() override {}

  // Simulates EditorManager draining its deferred queue on the next frame.
  void RunDeferred() {
    auto pending = std::move(deferred);
    deferred.clear();
    for (auto& action : pending) {
      action();
    }
  }

  bool rom_loaded = false;
  bool has_editor = false;
  bool text_input = false;
  std::vector<std::function<void()>> deferred;
  std::vector<ImGuiKey> injected;
  std::vector<MenuAction> host_actions;

 private:
  editor::ShortcutManager* shortcuts_;
};

class NativeMenuBridgeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Mirror the production registrations the native menu depends on
    // (shortcut_configurator.cc), with counters instead of EditorManager.
    auto count = [](int* counter) {
      return [counter]() {
        ++*counter;
      };
    };
    shortcuts_.RegisterShortcut("Save", {ImGuiMod_Ctrl, ImGuiKey_S},
                                count(&save_calls_));
    shortcuts_.RegisterShortcut("Save As",
                                {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_S},
                                count(&save_as_calls_));
    shortcuts_.RegisterShortcut("Close ROM", {ImGuiMod_Ctrl, ImGuiKey_W},
                                count(&close_calls_));
    shortcuts_.RegisterShortcut("Quit", {ImGuiMod_Ctrl, ImGuiKey_Q},
                                count(&quit_calls_));
    shortcuts_.RegisterShortcut("Undo", {ImGuiMod_Ctrl, ImGuiKey_Z},
                                count(&undo_calls_),
                                editor::Shortcut::Scope::kEditor);
    shortcuts_.RegisterShortcut("Copy", {ImGuiMod_Ctrl, ImGuiKey_C},
                                count(&copy_calls_),
                                editor::Shortcut::Scope::kEditor);
    shortcuts_.RegisterShortcut("Find", {ImGuiMod_Ctrl, ImGuiKey_F},
                                count(&find_calls_),
                                editor::Shortcut::Scope::kEditor);
    shortcuts_.RegisterShortcut("Agent Sidebar", {ImGuiMod_Ctrl, ImGuiKey_H},
                                count(&agent_calls_));
  }

  void TearDown() override { ImGui::DestroyContext(context_); }

  void RunFrame(const std::function<void(ImGuiIO&)>& inject) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject) {
      inject(io);
    }
    io.DisplaySize = ImVec2(640, 480);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(100, 100));
    ImGui::SetNextWindowFocus();
    ImGui::Begin(
        "##NativeMenuHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    editor::ExecuteShortcuts(shortcuts_);
    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
  }

  ImGuiContext* context_ = nullptr;
  editor::ShortcutManager shortcuts_;
  int save_calls_ = 0;
  int save_as_calls_ = 0;
  int close_calls_ = 0;
  int quit_calls_ = 0;
  int undo_calls_ = 0;
  int copy_calls_ = 0;
  int find_calls_ = 0;
  int agent_calls_ = 0;
};

TEST(NativeMenuChordTest, ConvertsShortcutManagerKeys) {
  EXPECT_EQ(ChordFromImGuiKeys({ImGuiMod_Ctrl, ImGuiKey_S}), Chord('s', true));
  EXPECT_EQ(ChordFromImGuiKeys({ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z}),
            Chord('z', true, /*shift=*/true));
  EXPECT_EQ(ChordFromImGuiKeys({ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Slash}),
            Chord('/', true, /*shift=*/true));
  EXPECT_EQ(ChordFromImGuiKeys({ImGuiMod_Ctrl, ImGuiKey_Comma}),
            Chord(',', true));
  // Keys a menu key equivalent cannot express stay ImGui-only.
  EXPECT_FALSE(ChordFromImGuiKeys({ImGuiKey_F1}).valid());
  EXPECT_FALSE(ChordFromImGuiKeys({ImGuiKey_S}).valid());
  EXPECT_FALSE(ChordFromImGuiKeys({ImGuiMod_Ctrl, ImGuiKey_LeftArrow}).valid());
}

TEST_F(NativeMenuBridgeTest, ShortcutBackedItemsAreImGuiOwned) {
  const ResolvedItem save = ResolveItem(&shortcuts_, MenuAction::kSave);
  EXPECT_EQ(save.owner, ChordOwner::kImGui);
  EXPECT_EQ(save.chord, Chord('s', true));
  EXPECT_EQ(save.dispatch, Dispatch::kShortcut);

  const ResolvedItem close = ResolveItem(&shortcuts_, MenuAction::kCloseRom);
  EXPECT_EQ(close.owner, ChordOwner::kImGui);
  EXPECT_EQ(close.chord, Chord('w', true));

  // Text-edit chords belong to ImGui even without a ShortcutManager entry.
  EXPECT_EQ(ResolveItem(&shortcuts_, MenuAction::kSelectAll).owner,
            ChordOwner::kImGui);

  for (const KeyChord& chord :
       {Chord('s', true), Chord('s', true, true), Chord('w', true),
        Chord('q', true), Chord('z', true), Chord('c', true),
        Chord('a', true)}) {
    EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, chord)) << chord.key;
  }
}

TEST_F(NativeMenuBridgeTest, NativeItemsOwnUnboundChordsOnly) {
  EXPECT_EQ(ResolveItem(&shortcuts_, MenuAction::kSettings).owner,
            ChordOwner::kNative);
  EXPECT_TRUE(IsNativeOwnedChord(&shortcuts_, Chord(',', true)));
  EXPECT_TRUE(IsNativeOwnedChord(&shortcuts_, Chord('m', true)));
  EXPECT_TRUE(IsNativeOwnedChord(
      &shortcuts_, Chord('f', true, false, false, /*control=*/true)));
  EXPECT_TRUE(IsNativeOwnedChord(&shortcuts_,
                                 Chord('h', true, false, /*option=*/true)));

  // Cmd+H is the agent sidebar: "Hide yaze" keeps its item but drops ⌘H.
  const ResolvedItem hide = ResolveItem(&shortcuts_, MenuAction::kHide);
  EXPECT_EQ(hide.owner, ChordOwner::kNone);
  EXPECT_FALSE(hide.chord.valid());
  EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord('h', true)));

  // Without the agent UI, ⌘H goes back to the native Hide item.
  editor::ShortcutManager no_agent;
  EXPECT_EQ(ResolveItem(&no_agent, MenuAction::kHide).owner,
            ChordOwner::kNative);
}

TEST_F(NativeMenuBridgeTest, RootMenuGateDeclinesImGuiChords) {
  // ImGui-owned: bindings and ImGui-owned items.
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('s', true)));
  EXPECT_FALSE(
      ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('s', true, true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('w', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('q', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('h', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('a', true)));
  // Native-owned and AppKit-inserted (Emoji & Symbols is Ctrl+Cmd+Space).
  EXPECT_TRUE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord(',', true)));
  EXPECT_TRUE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('m', true)));
  EXPECT_TRUE(ShouldMenuPerformKeyEquivalent(
      &shortcuts_, Chord(' ', true, false, false, /*control=*/true)));
}

// claude/ui-shortcuts binds Cmd+, to the Settings editor in ShortcutManager.
// One ⌘, must then run exactly once, through ImGui; the native item keeps
// showing ⌘, and a click still opens Settings through the host.
TEST_F(NativeMenuBridgeTest, CommandCommaHasOneOwnerWhenShortcutBindsIt) {
  int settings_calls = 0;
  shortcuts_.RegisterShortcut(SettingsShortcutName(),
                              {ImGuiMod_Ctrl, ImGuiKey_Comma},
                              [&settings_calls]() { ++settings_calls; });

  const ResolvedItem settings = ResolveItem(&shortcuts_, MenuAction::kSettings);
  EXPECT_EQ(settings.owner, ChordOwner::kImGui);
  EXPECT_EQ(settings.chord, Chord(',', true));
  EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord(',', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord(',', true)));

  RunFrame([](ImGuiIO& io) {
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_Comma, true);
  });
  RunFrame([](ImGuiIO& io) {
    io.AddKeyEvent(ImGuiKey_Comma, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
  });
  EXPECT_EQ(settings_calls, 1);

  FakeHost host(&shortcuts_);
  EXPECT_TRUE(PerformAction(host, MenuAction::kSettings));
  ASSERT_EQ(host.host_actions.size(), 1u);
  EXPECT_EQ(host.host_actions[0], MenuAction::kSettings);
  EXPECT_EQ(settings_calls, 1);
}

// The Settings item follows the live Settings binding, not a hard-coded ⌘,:
// rebound, it shows the new chord (ImGui-owned) and ⌘, stops being a
// Settings chord; unbound, it shows none and nothing performs ⌘, natively.
TEST_F(NativeMenuBridgeTest, SettingsChordFollowsRebindAndUnbind) {
  int settings_calls = 0;
  shortcuts_.RegisterShortcut(SettingsShortcutName(),
                              {ImGuiMod_Ctrl, ImGuiKey_Comma},
                              [&settings_calls]() { ++settings_calls; });

  // Rebind Settings to Cmd+P.
  ASSERT_TRUE(shortcuts_.UpdateShortcutKeys(SettingsShortcutName(),
                                            {ImGuiMod_Ctrl, ImGuiKey_P}));
  ResolvedItem settings = ResolveItem(&shortcuts_, MenuAction::kSettings);
  EXPECT_EQ(settings.owner, ChordOwner::kImGui);
  EXPECT_EQ(settings.chord, Chord('p', true));
  EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord(',', true)));
  EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord('p', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('p', true)));
  RunFrame([](ImGuiIO& io) {
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_P, true);
  });
  RunFrame([](ImGuiIO& io) {
    io.AddKeyEvent(ImGuiKey_P, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
  });
  EXPECT_EQ(settings_calls, 1);

  // Another action takes Cmd+,: the Settings item must not claim it.
  int other_calls = 0;
  shortcuts_.RegisterShortcut("Other Action", {ImGuiMod_Ctrl, ImGuiKey_Comma},
                              [&other_calls]() { ++other_calls; });
  settings = ResolveItem(&shortcuts_, MenuAction::kSettings);
  EXPECT_EQ(settings.chord, Chord('p', true));
  EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord(',', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord(',', true)));

  // Unbind Settings: no chord on the item, and ⌘, is not native-owned.
  editor::ShortcutManager unbound;
  unbound.RegisterShortcut(SettingsShortcutName(),
                           {ImGuiMod_Ctrl, ImGuiKey_Comma},
                           [&settings_calls]() { ++settings_calls; });
  ASSERT_TRUE(unbound.UpdateShortcutKeys(SettingsShortcutName(), {}));
  settings = ResolveItem(&unbound, MenuAction::kSettings);
  EXPECT_EQ(settings.owner, ChordOwner::kNone);
  EXPECT_FALSE(settings.chord.valid());
  EXPECT_FALSE(IsNativeOwnedChord(&unbound, Chord(',', true)));

  // A click still opens Settings through the host in every case.
  FakeHost host(&unbound);
  EXPECT_TRUE(PerformAction(host, MenuAction::kSettings));
  ASSERT_EQ(host.host_actions.size(), 1u);
  EXPECT_EQ(host.host_actions[0], MenuAction::kSettings);
}

// Without a registered Settings shortcut, a foreign exact binding on ⌘, is
// never shown on the Settings item (it would run the other action).
TEST_F(NativeMenuBridgeTest, SettingsDropsCommandCommaBoundToAnotherAction) {
  shortcuts_.RegisterShortcut("Other Action", {ImGuiMod_Ctrl, ImGuiKey_Comma},
                              []() {});
  const ResolvedItem settings = ResolveItem(&shortcuts_, MenuAction::kSettings);
  EXPECT_EQ(settings.owner, ChordOwner::kNone);
  EXPECT_FALSE(settings.chord.valid());
  EXPECT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord(',', true)));
  EXPECT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord(',', true)));
}

TEST_F(NativeMenuBridgeTest, MissingShortcutFallsBackToHostAction) {
  editor::ShortcutManager empty;
  const ResolvedItem quit = ResolveItem(&empty, MenuAction::kQuit);
  EXPECT_EQ(quit.dispatch, Dispatch::kHost);
  EXPECT_EQ(quit.owner, ChordOwner::kNative);
  EXPECT_EQ(quit.chord, Chord('q', true));

  FakeHost host(&empty);
  EXPECT_TRUE(PerformAction(host, MenuAction::kQuit));
  ASSERT_EQ(host.host_actions.size(), 1u);
  EXPECT_EQ(host.host_actions[0], MenuAction::kQuit);
}

// One physical ⌘S must produce exactly one save: the native menu declines
// the chord (not native-owned), so only SDL -> ImGui -> ExecuteShortcuts runs
// it. A menu click is a separate, single dispatch through the same callback.
TEST_F(NativeMenuBridgeTest, OneCommandSProducesOneSave) {
  FakeHost host(&shortcuts_);
  host.rom_loaded = true;

  // AppKit side: YazeMainMenu::performKeyEquivalent: declines the chord, and
  // the SDL filter (IsNativeOwnedChord) keeps the SDL key event.
  ASSERT_FALSE(ShouldMenuPerformKeyEquivalent(&shortcuts_, Chord('s', true)));
  ASSERT_FALSE(IsNativeOwnedChord(&shortcuts_, Chord('s', true)));

  // ImGui side: the key event SDL delivered.
  RunFrame([](ImGuiIO& io) {
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_S, true);
  });
  RunFrame([](ImGuiIO& io) {
    io.AddKeyEvent(ImGuiKey_S, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
  });
  RunFrame(nullptr);
  host.RunDeferred();
  EXPECT_EQ(save_calls_, 1);
  EXPECT_EQ(save_as_calls_, 0);

  // Menu click: one deferred ShortcutManager callback, no host fallback.
  EXPECT_TRUE(PerformAction(host, MenuAction::kSave));
  EXPECT_EQ(save_calls_, 1) << "click must wait for the next frame";
  host.RunDeferred();
  EXPECT_EQ(save_calls_, 2);
  EXPECT_TRUE(host.host_actions.empty());
  EXPECT_TRUE(host.injected.empty());
}

TEST_F(NativeMenuBridgeTest, ClickRoutesThroughShortcutCallbacks) {
  FakeHost host(&shortcuts_);
  PerformAction(host, MenuAction::kCloseRom);
  PerformAction(host, MenuAction::kQuit);
  PerformAction(host, MenuAction::kUndo);
  host.RunDeferred();
  EXPECT_EQ(close_calls_, 1);
  EXPECT_EQ(quit_calls_, 1);
  EXPECT_EQ(undo_calls_, 1);
  EXPECT_TRUE(host.host_actions.empty());

  // Cocoa-selector items are left to the responder chain.
  EXPECT_FALSE(PerformAction(host, MenuAction::kMinimize));
  EXPECT_FALSE(PerformAction(host, MenuAction::kToggleFullScreen));
}

TEST_F(NativeMenuBridgeTest, EditItemsTargetTextFieldsWhenTyping) {
  FakeHost host(&shortcuts_);
  host.has_editor = true;
  host.text_input = true;
  PerformAction(host, MenuAction::kCopy);
  PerformAction(host, MenuAction::kSelectAll);
  host.RunDeferred();
  ASSERT_EQ(host.injected.size(), 2u);
  EXPECT_EQ(host.injected[0], ImGuiKey_C);
  EXPECT_EQ(host.injected[1], ImGuiKey_A);
  EXPECT_EQ(copy_calls_, 0);

  host.text_input = false;
  PerformAction(host, MenuAction::kCopy);
  host.RunDeferred();
  EXPECT_EQ(copy_calls_, 1);
  EXPECT_EQ(host.injected.size(), 2u);
}

TEST_F(NativeMenuBridgeTest, ValidationTracksRomAndEditorState) {
  FakeHost host(&shortcuts_);
  EXPECT_FALSE(IsActionEnabled(host, MenuAction::kSave));
  EXPECT_FALSE(IsActionEnabled(host, MenuAction::kSaveAs));
  EXPECT_FALSE(IsActionEnabled(host, MenuAction::kCloseRom));
  EXPECT_FALSE(IsActionEnabled(host, MenuAction::kUndo));
  EXPECT_FALSE(IsActionEnabled(host, MenuAction::kSelectAll));
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kOpenRom));
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kSettings));

  host.rom_loaded = true;
  host.has_editor = true;
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kSave));
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kCloseRom));
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kUndo));
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kCopy));
  EXPECT_FALSE(IsActionEnabled(host, MenuAction::kSelectAll));
  host.text_input = true;
  EXPECT_TRUE(IsActionEnabled(host, MenuAction::kSelectAll));
}

TEST(NativeMenuRecentTest, LabelsUseFileName) {
  EXPECT_EQ(RecentFileLabel("/a/b/oos168.sfc"), "oos168.sfc");
  EXPECT_EQ(RecentFileLabel("plain.sfc"), "plain.sfc");
  EXPECT_EQ(RecentFileLabel("/a/b/"), "/a/b/");
}

}  // namespace
}  // namespace native_menu
}  // namespace platform
}  // namespace yaze
