#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "app/editor/overworld/core/interaction_coordinator.h"
#include "app/editor/system/commands/shortcut_configurator.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "app/editor/system/session/user_settings.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

// Arbitration tests for ExecuteShortcuts(): editor ownership, scope priority,
// the text-input guard, and persisted rebinds.
class ShortcutArbitrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    // Prime one empty frame so the next frame's key presses register.
    RunFrame(nullptr, nullptr);
  }

  void TearDown() override {
    if (imgui_context_) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  static ImGuiKey Primary() {
    return ImGui::GetIO().ConfigMacOSXBehaviors ? ImGuiMod_Super
                                                : ImGuiMod_Ctrl;
  }

  void RunFrame(std::function<void(ImGuiIO&)> inject_events,
                std::function<void()> frame_body) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject_events) {
      inject_events(io);
    }
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(200, 200));
    ImGui::SetNextWindowFocus();
    ImGui::Begin("##ArbitrationHost", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings);
    if (frame_body) {
      frame_body();
    }
    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
  }

  // Press the chord for one frame, run ExecuteShortcuts, then release it.
  void Press(const ShortcutManager& shortcuts, std::vector<ImGuiKey> keys,
             bool want_text_input = false) {
    RunFrame(
        [&](ImGuiIO& io) {
          for (ImGuiKey key : keys) {
            io.AddKeyEvent(key, true);
          }
        },
        [&]() {
          ImGui::GetIO().WantTextInput = want_text_input;
          ExecuteShortcuts(shortcuts);
        });
    RunFrame(
        [&](ImGuiIO& io) {
          for (ImGuiKey key : keys) {
            io.AddKeyEvent(key, false);
          }
        },
        nullptr);
  }

 private:
  ImGuiContext* imgui_context_ = nullptr;
};

TEST_F(ShortcutArbitrationTest, EditorOwnedShortcutOnlyFiresInItsEditor) {
  ShortcutManager shortcuts;
  std::optional<EditorType> active = EditorType::kOverworld;
  shortcuts.SetActiveEditorProvider([&]() { return active; });

  int overworld_next = 0;
  int dungeon_next = 0;
  shortcuts.RegisterEditorShortcut(
      "overworld.next_tile", {ImGuiKey_RightBracket},
      [&]() { ++overworld_next; }, EditorType::kOverworld);
  // Sorts before "overworld.*": the old alphabetical tie-break let it win.
  shortcuts.RegisterEditorShortcut(
      "dungeon.object.next_object", {ImGuiKey_RightBracket},
      [&]() { ++dungeon_next; }, EditorType::kDungeon);

  Press(shortcuts, {ImGuiKey_RightBracket});
  EXPECT_EQ(overworld_next, 1);
  EXPECT_EQ(dungeon_next, 0);

  active = EditorType::kDungeon;
  Press(shortcuts, {ImGuiKey_RightBracket});
  EXPECT_EQ(overworld_next, 1);
  EXPECT_EQ(dungeon_next, 1);

  active = std::nullopt;
  Press(shortcuts, {ImGuiKey_RightBracket});
  EXPECT_EQ(overworld_next, 1);
  EXPECT_EQ(dungeon_next, 1);
}

TEST_F(ShortcutArbitrationTest, InapplicableShortcutDoesNotBlockGlobal) {
  ShortcutManager shortcuts;
  shortcuts.SetActiveEditorProvider(
      []() -> std::optional<EditorType> { return EditorType::kDungeon; });

  int global_called = 0;
  int overworld_called = 0;
  shortcuts.RegisterShortcut(
      "Maximize Window", {ImGuiKey_F11}, [&]() { ++global_called; },
      Shortcut::Scope::kGlobal);
  shortcuts.RegisterEditorShortcut(
      "overworld.toggle_fullscreen", {ImGuiKey_F11},
      [&]() { ++overworld_called; }, EditorType::kOverworld);

  Press(shortcuts, {ImGuiKey_F11});
  EXPECT_EQ(global_called, 1);
  EXPECT_EQ(overworld_called, 0);
}

TEST_F(ShortcutArbitrationTest, MoreSpecificScopeWins) {
  ShortcutManager shortcuts;
  int global_called = 0;
  int editor_called = 0;
  int panel_called = 0;
  const std::vector<ImGuiKey> chord = {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_K};
  shortcuts.RegisterShortcut(
      "a.global", chord, [&]() { ++global_called; }, Shortcut::Scope::kGlobal);
  shortcuts.RegisterShortcut(
      "b.editor", chord, [&]() { ++editor_called; }, Shortcut::Scope::kEditor);
  shortcuts.RegisterShortcut(
      "c.panel", chord, [&]() { ++panel_called; }, Shortcut::Scope::kPanel);

  Press(shortcuts, {Primary(), ImGuiMod_Alt, ImGuiKey_K});
  EXPECT_EQ(panel_called, 1);
  EXPECT_EQ(editor_called, 0);
  EXPECT_EQ(global_called, 0);

  shortcuts.UpdateShortcutKeys("c.panel", {});
  Press(shortcuts, {Primary(), ImGuiMod_Alt, ImGuiKey_K});
  EXPECT_EQ(editor_called, 1);
  EXPECT_EQ(global_called, 0);
}

TEST_F(ShortcutArbitrationTest, EditorOwnedBeatsUnownedInSameScope) {
  ShortcutManager shortcuts;
  shortcuts.SetActiveEditorProvider(
      []() -> std::optional<EditorType> { return EditorType::kMusic; });
  int generic = 0;
  int music = 0;
  shortcuts.RegisterShortcut(
      "A Generic", {ImGuiKey_Equal}, [&]() { ++generic; },
      Shortcut::Scope::kEditor);
  shortcuts.RegisterEditorShortcut(
      "music.speed_up", {ImGuiKey_Equal}, [&]() { ++music; },
      EditorType::kMusic);

  Press(shortcuts, {ImGuiKey_Equal});
  EXPECT_EQ(music, 1);
  EXPECT_EQ(generic, 0);
}

TEST_F(ShortcutArbitrationTest, EnabledPredicateFiltersBeforeArbitration) {
  ShortcutManager shortcuts;
  bool panel_enabled = false;
  int panel_called = 0;
  int global_called = 0;
  shortcuts.RegisterShortcut(
      "panel", {ImGuiKey_F7}, [&]() { ++panel_called; },
      Shortcut::Scope::kPanel);
  shortcuts.SetShortcutEnabled("panel", [&]() { return panel_enabled; });
  shortcuts.RegisterShortcut(
      "global", {ImGuiKey_F7}, [&]() { ++global_called; },
      Shortcut::Scope::kGlobal);

  Press(shortcuts, {ImGuiKey_F7});
  EXPECT_EQ(panel_called, 0);
  EXPECT_EQ(global_called, 1);

  panel_enabled = true;
  Press(shortcuts, {ImGuiKey_F7});
  EXPECT_EQ(panel_called, 1);
  EXPECT_EQ(global_called, 1);
}

TEST_F(ShortcutArbitrationTest, TextInputSuppressesTextEditingChords) {
  ShortcutManager shortcuts;
  int undo = 0;
  int redo_alt = 0;
  int paste = 0;
  int palette = 0;
  shortcuts.RegisterShortcut(
      "Undo", {ImGuiMod_Ctrl, ImGuiKey_Z}, [&]() { ++undo; },
      Shortcut::Scope::kEditor);
  shortcuts.RegisterShortcut(
      "Redo (Alt)", {ImGuiMod_Ctrl, ImGuiKey_Y}, [&]() { ++redo_alt; },
      Shortcut::Scope::kEditor);
  shortcuts.RegisterShortcut(
      "Paste", {ImGuiMod_Ctrl, ImGuiKey_V}, [&]() { ++paste; },
      Shortcut::Scope::kEditor);
  shortcuts.RegisterShortcut(
      "Command Palette", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P},
      [&]() { ++palette; }, Shortcut::Scope::kGlobal);

  Press(shortcuts, {Primary(), ImGuiKey_Z}, /*want_text_input=*/true);
  Press(shortcuts, {Primary(), ImGuiKey_Y}, /*want_text_input=*/true);
  Press(shortcuts, {Primary(), ImGuiKey_V}, /*want_text_input=*/true);
  Press(shortcuts, {Primary(), ImGuiMod_Shift, ImGuiKey_P},
        /*want_text_input=*/true);
  EXPECT_EQ(undo, 0);
  EXPECT_EQ(redo_alt, 0);
  EXPECT_EQ(paste, 0);
  EXPECT_EQ(palette, 1);

  // Without a focused text field the same chords reach the editor.
  Press(shortcuts, {Primary(), ImGuiKey_Z});
  Press(shortcuts, {Primary(), ImGuiKey_Y});
  EXPECT_EQ(undo, 1);
  EXPECT_EQ(redo_alt, 1);
}

TEST_F(ShortcutArbitrationTest, ParseShortcutRoundTripsPrintedKeys) {
  const std::vector<std::vector<ImGuiKey>> cases = {
      {ImGuiMod_Ctrl, ImGuiKey_0},
      {ImGuiMod_Ctrl, ImGuiKey_Equal},
      {ImGuiMod_Ctrl, ImGuiKey_Comma},
      {ImGuiKey_LeftBracket},
      {ImGuiKey_Space},
      {ImGuiKey_F11},
      {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z},
      {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_RightBracket},
  };
  for (const auto& keys : cases) {
    const std::string printed = PrintShortcut(keys);
    EXPECT_TRUE(SameChord(ParseShortcut(printed), keys)) << printed;
  }
  EXPECT_TRUE(SameChord(ParseShortcut("ctrl+s"), {ImGuiMod_Ctrl, ImGuiKey_S}));
  EXPECT_TRUE(ParseShortcut("Ctrl+NotAKey").size() == 1);
}

TEST_F(ShortcutArbitrationTest, DisplayStringConflictsAndReset) {
  ShortcutManager shortcuts;
  shortcuts.RegisterShortcut(
      "Redo", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z}, []() {},
      Shortcut::Scope::kEditor);
  shortcuts.RegisterCommand("Palette Only", []() {});

  EXPECT_EQ(shortcuts.GetDisplayString("Redo"),
            PrintShortcut({ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z}));
  EXPECT_EQ(shortcuts.GetDisplayString("Palette Only"), "");
  EXPECT_EQ(shortcuts.GetDisplayString("Missing"), "");

  shortcuts.RegisterShortcut(
      "Other", {ImGuiMod_Ctrl, ImGuiKey_R}, []() {}, Shortcut::Scope::kGlobal);
  ASSERT_TRUE(
      shortcuts.UpdateShortcutKeys("Redo", {ImGuiMod_Ctrl, ImGuiKey_R}));
  EXPECT_EQ(shortcuts.GetDisplayString("Redo"),
            PrintShortcut({ImGuiMod_Ctrl, ImGuiKey_R}));
  EXPECT_EQ(shortcuts.FindConflicts("Redo"), std::vector<std::string>{"Other"});

  // Different editors never conflict with each other.
  shortcuts.RegisterEditorShortcut(
      "dungeon.x", {ImGuiKey_X}, []() {}, EditorType::kDungeon);
  shortcuts.RegisterEditorShortcut(
      "overworld.x", {ImGuiKey_X}, []() {}, EditorType::kOverworld);
  EXPECT_TRUE(shortcuts.FindConflicts("dungeon.x").empty());

  ASSERT_TRUE(shortcuts.ResetShortcutKeys("Redo"));
  EXPECT_EQ(shortcuts.GetShortcut("Redo").keys,
            (std::vector<ImGuiKey>{ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z}));
  EXPECT_TRUE(shortcuts.FindConflicts("Redo").empty());
}

TEST_F(ShortcutArbitrationTest, SavedGlobalAndEditorRebindsAreApplied) {
  ShortcutManager shortcuts;
  int save = 0;
  shortcuts.RegisterShortcut(
      "Save", {ImGuiMod_Ctrl, ImGuiKey_S}, [&]() { ++save; },
      Shortcut::Scope::kGlobal);
  shortcuts.RegisterEditorShortcut(
      "music.play_pause", {ImGuiKey_Space}, []() {}, EditorType::kMusic);
  shortcuts.RegisterShortcut(
      "Find", {ImGuiMod_Ctrl, ImGuiKey_F}, []() {}, Shortcut::Scope::kEditor);

  UserSettings settings;
  settings.prefs().global_shortcuts["Save"] = "Ctrl+Alt+S";
  settings.prefs().global_shortcuts["Removed Command"] = "Ctrl+Alt+9";
  settings.prefs().editor_shortcuts["music.play_pause"] = "P";
  settings.prefs().editor_shortcuts["Find"] = "";  // explicit unbind

  ApplyUserShortcutOverrides(settings, &shortcuts);

  EXPECT_TRUE(SameChord(shortcuts.GetShortcut("Save").keys,
                        {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_S}));
  EXPECT_EQ(shortcuts.GetShortcut("music.play_pause").keys,
            std::vector<ImGuiKey>{ImGuiKey_P});
  EXPECT_TRUE(shortcuts.GetShortcut("Find").keys.empty());
  EXPECT_EQ(shortcuts.FindShortcut("Removed Command"), nullptr);
  // Defaults are kept for reset.
  EXPECT_EQ(shortcuts.GetShortcut("Save").default_keys,
            (std::vector<ImGuiKey>{ImGuiMod_Ctrl, ImGuiKey_S}));

  Press(shortcuts, {Primary(), ImGuiKey_S});
  EXPECT_EQ(save, 0);
  Press(shortcuts, {Primary(), ImGuiMod_Alt, ImGuiKey_S});
  EXPECT_EQ(save, 1);
}

TEST_F(ShortcutArbitrationTest, OverworldCoordinatorUsesPlatformPrimaryMod) {
  int duplicated = 0;
  OverworldCommandSink sink;
  sink.can_edit_items = []() {
    return true;
  };
  sink.on_duplicate_selected = [&]() {
    ++duplicated;
  };
  OverworldInteractionCoordinator coordinator(std::move(sink));

  auto press = [&](std::vector<ImGuiKey> keys) {
    RunFrame(
        [&](ImGuiIO& io) {
          for (ImGuiKey key : keys) {
            io.AddKeyEvent(key, true);
          }
        },
        [&]() { coordinator.Update(); });
    RunFrame(
        [&](ImGuiIO& io) {
          for (ImGuiKey key : keys) {
            io.AddKeyEvent(key, false);
          }
        },
        nullptr);
  };

  // The modifier arrives as a mod flag only (no physical LeftCtrl key), as
  // Cmd does on macOS.
  press({Primary(), ImGuiKey_D});
  EXPECT_EQ(duplicated, 1);

  // Cmd/Ctrl+Shift+D is Duplicate Session, not item duplicate.
  press({Primary(), ImGuiMod_Shift, ImGuiKey_D});
  EXPECT_EQ(duplicated, 1);
}

TEST_F(ShortcutArbitrationTest, OverworldDigitsIgnoreModifiedPresses) {
  std::vector<EditingMode> modes;
  std::vector<EntityEditMode> entity_modes;
  OverworldCommandSink sink;
  sink.on_set_editor_mode = [&](EditingMode mode) {
    modes.push_back(mode);
  };
  sink.on_set_entity_mode = [&](EntityEditMode mode) {
    entity_modes.push_back(mode);
  };
  OverworldInteractionCoordinator coordinator(std::move(sink));

  auto press = [&](std::vector<ImGuiKey> keys) {
    RunFrame(
        [&](ImGuiIO& io) {
          for (ImGuiKey key : keys) {
            io.AddKeyEvent(key, true);
          }
        },
        [&]() { coordinator.Update(); });
    RunFrame(
        [&](ImGuiIO& io) {
          for (ImGuiKey key : keys) {
            io.AddKeyEvent(key, false);
          }
        },
        [&]() { coordinator.Update(); });
  };

  // Cmd/Ctrl+digit switches editors; Alt+digit switches worlds.
  press({Primary(), ImGuiKey_1});
  press({Primary(), ImGuiKey_3});
  press({ImGuiMod_Alt, ImGuiKey_2});
  press({ImGuiMod_Alt, ImGuiKey_5});
  EXPECT_TRUE(modes.empty());
  EXPECT_TRUE(entity_modes.empty());

  // Plain digits act once per press (not every frame the key is held).
  press({ImGuiKey_3});
  press({ImGuiKey_2});
  ASSERT_EQ(entity_modes.size(), 1u);
  EXPECT_EQ(entity_modes[0], EntityEditMode::ENTRANCES);
  ASSERT_EQ(modes.size(), 1u);
  EXPECT_EQ(modes[0], EditingMode::DRAW_TILE);
}

}  // namespace
}  // namespace yaze::editor
