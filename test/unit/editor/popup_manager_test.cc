#include "app/editor/shell/feedback/popup_manager.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

#include "app/editor/editor_manager.h"
#include "app/editor/shell/windows/settings_panel.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gui/automation/widget_id_registry.h"
#include "core/features.h"
#include "editor_test_support.h"
#include "unique_temp_path.h"
#include "util/rom_hash.h"

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;
    io.ConfigMacOSXBehaviors = false;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() { ImGui::DestroyContext(ctx); }
};

void RunFrames(PopupManager& popups, int frames) {
  for (int i = 0; i < frames; ++i) {
    ImGui::NewFrame();
    popups.DrawPopups();
    ImGui::Render();
  }
}

// Save As starts a save that needs confirmation, and the confirmation popup is
// shown from inside the Save As popup. Both stay visible in the manager. If
// each frame reopens both modals, they close each other, stay hidden while
// auto-sizing, and the open modal blocks all input: the app looks frozen.
TEST(PopupManagerTest, PopupShownFromAnotherPopupStaysOpen) {
  ScopedImGuiContext imgui;
  PopupManager popups(nullptr);
  int confirm_appearances = 0;
  int save_as_appearances = 0;
  popups.RegisterPopup("Save As", PopupType::kFileOperation, [&] {
    if (ImGui::IsWindowAppearing()) {
      ++save_as_appearances;
    }
    if (!popups.IsVisible("Confirm")) {
      popups.Show("Confirm");
    }
  });
  popups.RegisterPopup("Confirm", PopupType::kConfirmation, [&] {
    if (ImGui::IsWindowAppearing()) {
      ++confirm_appearances;
    }
  });

  popups.Show("Save As");
  RunFrames(popups, 12);
  EXPECT_EQ(confirm_appearances, 1)
      << "the confirmation popup is reopened every frame";
  EXPECT_EQ(save_as_appearances, 1);
  EXPECT_TRUE(popups.IsVisible("Save As"));
  EXPECT_TRUE(popups.IsVisible("Confirm"));
}

// Closing the top popup returns to the one underneath.
TEST(PopupManagerTest, HidingTopPopupRedrawsTheOneBelow) {
  ScopedImGuiContext imgui;
  PopupManager popups(nullptr);
  int lower_draws = 0;
  popups.RegisterPopup("Lower", PopupType::kFileOperation,
                       [&] { ++lower_draws; });
  popups.RegisterPopup("Upper", PopupType::kConfirmation, [] {});

  popups.Show("Lower");
  RunFrames(popups, 3);
  popups.Show("Upper");
  RunFrames(popups, 3);
  popups.Hide("Upper");
  const int lower_before = lower_draws;
  RunFrames(popups, 4);
  EXPECT_GT(lower_draws, lower_before);
}

// These tests drive the production popup with ImGui input events. NullRenderer
// has no platform window, and every save targets a disposable fixture folder.
class SaveAsPopupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = test::UniqueTempPath("yaze_save_as_popup");
    std::filesystem::create_directories(root_);
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager_->user_settings().prefs().backup_before_save = false;
    Open("oos168.sfc");
  }

  void TearDown() override {
    manager_.reset();
    core::FeatureFlags::get() = original_flags_;
    gui::WidgetIdRegistry::Instance().Clear();
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }

  void Open(const char* name) {
    source_ = root_ / name;
    std::vector<uint8_t> bytes(512 * 1024, 0);
    const std::string title = "THE LEGEND OF ZELDA";
    std::copy(title.begin(), title.end(), bytes.begin() + 0x7FC0);
    {
      std::ofstream out(source_, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    ASSERT_TRUE(manager_->OpenRomOrProject(source_.string()).ok());
    // Exercise the actual manager save and disk commit without asking a
    // synthetic ROM to serialize unloaded dungeon/graphics assets.
    auto& flags = core::FeatureFlags::get();
    flags.kSaveDungeonMaps = false;
    flags.kSaveGraphicsSheet = false;
    flags.kSaveMessages = false;
    flags.kSaveAllPalettes = false;
    flags.kSaveGfxGroups = false;
    flags.dungeon.kSaveObjects = false;
    flags.dungeon.kSaveSprites = false;
    flags.dungeon.kSaveRoomHeaders = false;
    flags.dungeon.kSaveTorches = false;
    flags.dungeon.kSavePits = false;
    flags.dungeon.kSaveBlocks = false;
    flags.dungeon.kSaveCollision = false;
    flags.dungeon.kSaveWaterFillZones = false;
    flags.dungeon.kSaveChests = false;
    flags.dungeon.kSavePotItems = false;
    flags.dungeon.kSaveEntrances = false;
    flags.dungeon.kSavePalettes = false;
    flags.overworld.kSaveOverworldMaps = false;
    flags.overworld.kSaveOverworldEntrances = false;
    flags.overworld.kSaveOverworldExits = false;
    flags.overworld.kSaveOverworldItems = false;
    flags.overworld.kSaveOverworldProperties = false;
    manager_->user_settings().prefs().backup_before_save = false;
    auto* project = manager_->GetCurrentProject();
    project->workspace_settings.backup_on_save = false;
    project->rom_metadata.expected_hash.clear();
  }

  void Frame(std::function<void(ImGuiIO&)> events = {}) {
    if (events)
      events(ImGui::GetIO());
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    if (draw_settings_) {
      ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_Always);
      ImGui::Begin("Settings test", nullptr, ImGuiWindowFlags_NoSavedSettings);
      manager_->GetCurrentEditorSet()->GetSettingsPanel()->Draw();
      ImGui::End();
    } else {
      manager_->popup_manager()->DrawPopups();
    }
    ImGui::Render();
  }

  void Show() {
    manager_->popup_manager()->Show(PopupID::kSaveAs);
    Frame();
    Frame();
    Frame();
  }

  void Click(const char* path) {
    const auto widget = gui::WidgetIdRegistry::Instance().GetWidgetInfo(path);
    ASSERT_TRUE(widget.has_value()) << path;
    ASSERT_TRUE(widget->bounds.valid);
    const auto b = widget->bounds;
    Frame([&](ImGuiIO& io) {
      io.AddMousePosEvent((b.min_x + b.max_x) / 2, (b.min_y + b.max_y) / 2);
    });
    Frame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
    Frame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
  }

  void Filename(const char* value) {
    Click("SaveAs/input:Filename");
    Frame([](ImGuiIO& io) {
      io.AddKeyEvent(ImGuiMod_Ctrl, true);
      io.AddKeyEvent(ImGuiKey_A, true);
    });
    Frame([](ImGuiIO& io) {
      io.AddKeyEvent(ImGuiKey_A, false);
      io.AddKeyEvent(ImGuiMod_Ctrl, false);
    });
    Frame([&](ImGuiIO& io) { io.AddInputCharactersUTF8(value); });
  }

  void ExpectSaved(const std::filesystem::path& path, uint8_t expected = 0) {
    ASSERT_TRUE(std::filesystem::exists(path)) << path;
    EXPECT_EQ(manager_->GetCurrentRom()->filename(), path.string());
    EXPECT_FALSE(manager_->popup_manager()->IsVisible(PopupID::kSaveAs));
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
    ASSERT_TRUE(reopened.ReadByte(0x1234).ok());
    EXPECT_EQ(*reopened.ReadByte(0x1234), expected);
  }

  bool draw_settings_ = false;
  ScopedImGuiContext imgui_;
  core::FeatureFlags::Flags original_flags_ = core::FeatureFlags::get();
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  std::filesystem::path root_;
  std::filesystem::path source_;
};

TEST_F(SaveAsPopupTest, SuggestsAvailableSiblingAndReadsBackSavedBytes) {
  std::ofstream(root_ / "oos168-copy.sfc").put('a');
  std::ofstream(root_ / "oos168-copy-2.sfc").put('b');
  ASSERT_TRUE(manager_->GetCurrentRom()->WriteByte(0x1234, 0x5A).ok());
  Show();
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "oos168-copy-3.sfc", 0x5A);
  Rom source;
  ASSERT_TRUE(source.LoadFromFile(source_.string()).ok());
  EXPECT_EQ(*source.ReadByte(0x1234), 0);
  EXPECT_EQ(std::filesystem::file_size(root_ / "oos168-copy.sfc"), 1u);
}

TEST_F(SaveAsPopupTest, FilenameWithoutExtensionUsesSourceFolder) {
  Show();
  Filename("overworld-save");
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "overworld-save.sfc");
}

TEST_F(SaveAsPopupTest, RecognizesUppercaseExtension) {
  Show();
  Filename("dungeon-save.SMC");
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "dungeon-save.SMC");
}

TEST_F(SaveAsPopupTest, ExtensionMustBeTheSuffix) {
  Show();
  Filename("dungeon.sfc.copy");
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "dungeon.sfc.copy.sfc");
}

TEST_F(SaveAsPopupTest, CancelResetsEditedFilename) {
  Show();
  Filename("cancelled");
  Click("SaveAs/button:Cancel");
  EXPECT_FALSE(manager_->popup_manager()->IsVisible(PopupID::kSaveAs));
  Show();
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "oos168-copy.sfc");
  EXPECT_FALSE(std::filesystem::exists(root_ / "cancelled.sfc"));
}

TEST_F(SaveAsPopupTest, NewSourceDoesNotReusePreviousFilename) {
  Show();
  Filename("stale");
  Click("SaveAs/button:Cancel");
  Open("second.SMC");
  Show();
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "second-copy.SMC");
}

TEST_F(SaveAsPopupTest, RejectsPathInFilenameWithoutSaving) {
  Show();
  Filename("../outside");
  Click("SaveAs/button:Save");
  EXPECT_TRUE(manager_->popup_manager()->IsVisible(PopupID::kSaveAs));
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), source_.string());
  Filename("valid");
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "valid.sfc");
}

TEST_F(SaveAsPopupTest, ConfirmationPreservesTypedDestination) {
  auto* project = manager_->GetCurrentProject();
  project->name = "Save confirmation fixture";
  project->filepath = (root_ / "fixture.yaze").string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(manager_->GetCurrentRom()->WriteByte(0x1234, 0x42).ok());
  Show();
  Filename("confirmed");
  Click("SaveAs/button:Save");
  EXPECT_TRUE(manager_->IsRomWriteConfirmPending());
  EXPECT_FALSE(std::filesystem::exists(root_ / "confirmed.sfc"));
  Frame();
  Frame();
  Click("RomWriteConfirm/button:Save anyway");
  ExpectSaved(root_ / "confirmed.sfc", 0x42);
}

TEST_F(SaveAsPopupTest, SettingsHashUsesSaveIdentityAndSurvivesProjectCapture) {
  auto* project = manager_->GetCurrentProject();
  project->name = "Settings identity fixture";
  project->filepath = (root_ / "fixture.yaze").string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(project->Save().ok());
  const auto identity_hash = manager_->GetCurrentRomHash();
  ASSERT_FALSE(identity_hash.empty());
  ASSERT_TRUE(manager_->GetCurrentRom()->WriteByte(0x1234, 0x72).ok());
  ASSERT_NE(identity_hash,
            util::ComputeRomHash(manager_->GetCurrentRom()->data(),
                                 manager_->GetCurrentRom()->size()));
  ASSERT_TRUE(manager_->IsRomHashMismatch());

  draw_settings_ = true;
  Frame();
  Frame();
  Click("Settings/tab:Project");
  Frame();
  Click("Settings/button:Use Current ROM Hash");
  Frame();
  EXPECT_EQ(project->rom_metadata.expected_hash, identity_hash);
  EXPECT_FALSE(manager_->IsRomHashMismatch());
  project::YazeProject on_disk;
  ASSERT_TRUE(on_disk.Open((root_ / "fixture.yaze").string()).ok());
  EXPECT_EQ(on_disk.rom_metadata.expected_hash, identity_hash);

  draw_settings_ = false;
  Show();
  Filename("settings-confirmed");
  Click("SaveAs/button:Save");
  ExpectSaved(root_ / "settings-confirmed.sfc", 0x72);
  EXPECT_EQ(project->rom_metadata.expected_hash, identity_hash);
  auto* session = manager_->session_coordinator()->GetActiveRomSession();
  ASSERT_TRUE(session->project_context.has_value());
  EXPECT_EQ(session->project_context->rom_metadata.expected_hash,
            identity_hash);
}

}  // namespace
}  // namespace yaze::editor
