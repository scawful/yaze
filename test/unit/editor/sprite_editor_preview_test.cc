#include "app/editor/sprite/sprite_editor_internal.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/sprite/sprite_authoring.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "core/project.h"
#include "framework/mock_renderer.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

struct SpriteEditorPreviewTestPeer {
  static void Select(SpriteEditor& editor, int id) {
    editor.SelectVanillaSprite(id);
  }
  static int CurrentId(const SpriteEditor& editor) {
    return editor.current_sprite_id_;
  }
  static gfx::Bitmap& VanillaBitmap(SpriteEditor& editor) {
    return editor.vanilla_preview_bitmap_;
  }
  static gfx::Bitmap& CustomBitmap(SpriteEditor& editor) {
    return editor.sprite_preview_bitmap_;
  }
  static void SetSheet(SpriteEditor& editor, int slot, uint8_t id) {
    editor.current_sheets_[slot] = id;
  }
  static uint8_t Sheet(const SpriteEditor& editor, int slot) {
    return editor.current_sheets_[slot];
  }
  static void CreateOracleFrame(SpriteEditor& editor) {
    editor.CreateNewZSprite();
    editor.custom_sprites_.back().editor.Frames[0] =
        sprite_authoring::CopyVanillaLayout(
            *zelda3::SpriteOamRegistry::GetPreviewOverride(
                0xB1, "Oracle of Secrets"));
  }
};

namespace {

constexpr int kVanillaWidth = 128;
constexpr int kVanillaHeight = 128;
constexpr int kSpriteDepth = 8;

class SpriteEditorPreviewTest : public ::testing::Test {
 protected:
  void SetUp() override { gfx::Arena::Get().ClearTextureQueue(); }
  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }
};

TEST_F(SpriteEditorPreviewTest, FirstCallCreatesActiveBitmapAndQueuesCreate) {
  gfx::Bitmap bmp;  // Default: surface=nullptr, texture=nullptr, !is_active().
  std::vector<uint8_t> buffer(kVanillaWidth * kVanillaHeight, 0);

  const size_t before = gfx::Arena::Get().texture_command_queue_size();
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, buffer);
  const size_t after = gfx::Arena::Get().texture_command_queue_size();

  EXPECT_TRUE(bmp.is_active());
  EXPECT_NE(bmp.surface(), nullptr);
  EXPECT_EQ(after, before + 1)
      << "First call must queue exactly one CREATE for the texture; "
         "without this, canvas_rendering silently skips the draw.";
  EXPECT_EQ(bmp.metadata().purpose,
            gfx::Bitmap::BitmapPurpose::kCompositeOutput)
      << "Sprite preview bitmaps are post-render composites (pixels come "
         "from SpriteDrawer, not direct user paint). Stamping the purpose "
         "lets consumers tell composite outputs apart from editable "
         "scratchpads without consulting caller convention.";
}

TEST_F(SpriteEditorPreviewTest, CompositeStartsBlankAndPublishesChangedPixels) {
  gfx::Bitmap bmp;
  std::vector<uint8_t> graphics(kVanillaWidth * kVanillaHeight, 7);
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, graphics);
  ASSERT_TRUE(bmp.is_active());
  EXPECT_EQ(bmp.data()[0], 0);
  EXPECT_EQ(static_cast<uint8_t*>(bmp.surface()->pixels)[0], 0);
  bmp.mutable_data()[0] = 3;
  internal::PublishSpritePreviewPixels(bmp);
  EXPECT_EQ(static_cast<uint8_t*>(bmp.surface()->pixels)[0], 3);
  bmp.mutable_data()[0] = 0;
  internal::PublishSpritePreviewPixels(bmp);
  EXPECT_EQ(static_cast<uint8_t*>(bmp.surface()->pixels)[0], 0);
}

TEST_F(SpriteEditorPreviewTest, RepeatedCallIsNoOpForActiveBitmap) {
  gfx::Bitmap bmp;
  std::vector<uint8_t> buffer(kVanillaWidth * kVanillaHeight, 0);
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, buffer);
  const size_t after_first = gfx::Arena::Get().texture_command_queue_size();

  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, buffer);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), after_first)
      << "Subsequent calls on an already-active bitmap must short-circuit "
         "and not re-queue CREATE.";
  EXPECT_TRUE(bmp.is_active());
}

TEST_F(SpriteEditorPreviewTest, EmptyBufferLeavesBitmapInactive) {
  // Bitmap::Create with empty data sets active_=false and bails out.
  // The helper must not stamp purpose or queue CREATE when init failed.
  gfx::Bitmap bmp;
  std::vector<uint8_t> empty;

  const size_t before = gfx::Arena::Get().texture_command_queue_size();
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, empty);

  EXPECT_FALSE(bmp.is_active());
  EXPECT_EQ(bmp.texture(), nullptr);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), before)
      << "An inactive bitmap (failed Create) must not queue CREATE; the "
         "Arena would otherwise process a CREATE for a bitmap with no "
         "surface and silently fail again later.";
}

// Exercise the WindowContent callbacks SpriteEditor::Initialize actually
// registers, without calling Update first or invoking render helpers directly.
class RegisteredSpritePreviewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1600, 1000);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000, -1000);
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    data_.set_rom(&rom_);
    FillGraphics(data_, 2);
    data_.spriteset_ids[0] = {0x1F, 0x20, 0x21, 0x22};
    deps_.rom = &rom_;
    deps_.game_data = &data_;
    deps_.window_manager = &windows_;
    editor_ = std::make_unique<SpriteEditor>(&rom_);
    editor_->SetDependencies(deps_);
    editor_->Initialize();
    ASSERT_TRUE(editor_->Load().ok());
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    gui::WidgetIdRegistry::Instance().Clear();
    editor_.reset();
    gfx::PaletteManager::Get().ReleaseSession(&data_);
    ImGui::DestroyContext();
  }

  static void FillGraphics(zelda3::GameData& data, uint8_t index) {
    data.graphics_buffer.assign(223 * 4096, index);
    data.sheet_store.MarkAllSheetsChanged();
    for (int row = 0; row < 8; ++row) {
      gfx::SnesPalette palette;
      for (int color = 0; color < 16; ++color)
        palette.AddColor(gfx::SnesColor(color * 8, 128, row * 16));
      data.palette_groups.global_sprites.AddPalette(palette);
    }
  }

  void Draw(const char* panel_id = "sprite.vanilla_editor") {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1500, 950), ImGuiCond_Always);
    ImGui::Begin("RegisteredSpriteHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    auto* panel = windows_.GetWindowContent(panel_id);
    EXPECT_NE(panel, nullptr);
    if (panel)
      panel->Draw(nullptr);
    log_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
  }

  void Select(int id) {
    SpriteEditorPreviewTestPeer::Select(*editor_, id);
    Draw();
    Draw();
  }

  static bool HasPixel(const gfx::Bitmap& bitmap, uint8_t color) {
    return std::any_of(
        bitmap.vector().begin(), bitmap.vector().end(),
        [color](uint8_t pixel) { return (pixel & 15) == color; });
  }

  void ClickTab(const char* label) {
    const auto normalized = gui::WidgetIdRegistry::NormalizeLabel(label);
    std::optional<gui::WidgetIdRegistry::WidgetInfo> item;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == normalized)
        item = widget;
    }
    ASSERT_TRUE(item.has_value());
    ASSERT_TRUE(item->visible);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((item->bounds.min_x + item->bounds.max_x) / 2,
                        (item->bounds.min_y + item->bounds.max_y) / 2);
    Draw();
    io.AddMouseButtonEvent(0, true);
    Draw();
    io.AddMouseButtonEvent(0, false);
    Draw();
    Draw();
  }

  Rom rom_;
  zelda3::GameData data_;
  project::YazeProject project_;
  WorkspaceWindowManager windows_;
  EditorDependencies deps_;
  std::unique_ptr<SpriteEditor> editor_;
  std::string log_;
};

TEST_F(RegisteredSpritePreviewTest, DefaultPanelExplainsMissingLayout) {
  Draw();
  Draw();
  EXPECT_NE(log_.find("Experimental static preview"), std::string::npos);
  EXPECT_NE(log_.find("No static OAM layout for sprite $00"),
            std::string::npos);
  EXPECT_EQ(log_.find("Graphics sheet"), std::string::npos);
}

TEST_F(RegisteredSpritePreviewTest,
       VanillaPanelPublishesPixelsWithoutUpdateOrdering) {
  Select(0xA7);
  auto& bitmap = SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_);
  ASSERT_TRUE(bitmap.is_active());
  EXPECT_TRUE(HasPixel(bitmap, 2));
  EXPECT_TRUE(std::equal(bitmap.vector().begin(), bitmap.vector().end(),
                         static_cast<uint8_t*>(bitmap.surface()->pixels)));
  EXPECT_NE(log_.find("Stalfos"), std::string::npos);
  testing::NiceMock<test::MockRenderer> renderer;
  EXPECT_CALL(renderer, CreateTexture(128, 128)).Times(1);
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  ASSERT_NE(bitmap.texture(), nullptr);
  Draw();
  bool submitted = false;
  const auto* draw_data = ImGui::GetDrawData();
  for (int list = 0; list < draw_data->CmdListsCount; ++list) {
    for (const auto& command : draw_data->CmdLists[list]->CmdBuffer) {
      // Font atlas commands may not have a backend texture in headless ImGui.
      if (command.TexRef._TexData &&
          command.TexRef._TexData->GetTexID() == ImTextureID_Invalid)
        continue;
      if (command.ElemCount &&
          command.GetTexID() == (ImTextureID)(intptr_t)bitmap.texture())
        submitted = true;
    }
  }
  EXPECT_TRUE(submitted);
}

TEST_F(RegisteredSpritePreviewTest,
       TabClickRendersItsOwnSpriteAndReselectsOpenTab) {
  Select(0xA7);
  Select(0x0B);
  EXPECT_EQ(SpriteEditorPreviewTestPeer::CurrentId(*editor_), 0x0B);
  ClickTab("SpriteTabA7");
  EXPECT_EQ(SpriteEditorPreviewTestPeer::CurrentId(*editor_), 0xA7);
  EXPECT_NE(log_.find("Sprite: Stalfos (0xA7)"), std::string::npos);
  Select(0x0B);
  EXPECT_EQ(SpriteEditorPreviewTestPeer::CurrentId(*editor_), 0x0B);
  EXPECT_NE(log_.find("Sprite: Chicken (0x0B)"), std::string::npos);
}

TEST_F(RegisteredSpritePreviewTest,
       SelectedSheetsAndLiveSessionEditsRefreshPreview) {
  Select(0xA7);
  ASSERT_TRUE(
      data_.sheet_store.WriteSheet(0x95, std::vector<uint8_t>(4096, 4)).ok());
  SpriteEditorPreviewTestPeer::SetSheet(*editor_, 4, 0x95);
  Draw();
  EXPECT_EQ(SpriteEditorPreviewTestPeer::Sheet(*editor_, 4), 0x95);
  EXPECT_TRUE(
      HasPixel(SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_), 4));
  ASSERT_TRUE(
      data_.sheet_store.WriteSheet(0x95, std::vector<uint8_t>(4096, 5)).ok());
  Draw();
  EXPECT_TRUE(
      HasPixel(SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_), 5));
  EXPECT_FALSE(
      HasPixel(SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_), 4));
  zelda3::GameData second;
  FillGraphics(second, 6);
  editor_->SetGameData(&second);
  Draw();
  EXPECT_TRUE(
      HasPixel(SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_), 6));
  editor_->SetGameData(&data_);
}

TEST_F(RegisteredSpritePreviewTest,
       PaletteOnlyEditsRefreshPreviewWithoutSheetChanges) {
  auto& palettes = gfx::PaletteManager::Get();
  palettes.Initialize(&data_);
  Select(0xA7);
  auto& bitmap = SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_);
  const auto sheet_revision = data_.sheet_store.Revision(0x92);
  const auto pixels = bitmap.vector();
  ASSERT_GT(bitmap.palette().size(), 18u);
  const auto before = bitmap.palette()[18].snes();
  ASSERT_TRUE(
      palettes
          .SetColor("global_sprites", 1, 2, gfx::SnesColor(uint16_t{0x001F}))
          .ok());
  Draw();
  EXPECT_EQ(bitmap.vector(), pixels);
  EXPECT_EQ(data_.sheet_store.Revision(0x92), sheet_revision);
  EXPECT_NE(bitmap.palette()[18].snes(), before);
  EXPECT_EQ(bitmap.palette()[18].snes(), 0x001F);
  SpriteEditorPreviewTestPeer::CreateOracleFrame(*editor_);
  Draw("sprite.custom_editor");
  ASSERT_TRUE(
      palettes
          .SetColor("global_sprites", 1, 2, gfx::SnesColor(uint16_t{0x03E0}))
          .ok());
  Draw("sprite.custom_editor");
  EXPECT_EQ(
      SpriteEditorPreviewTestPeer::CustomBitmap(*editor_).palette()[18].snes(),
      0x03E0);
}

TEST_F(RegisteredSpritePreviewTest,
       UnavailableGraphicsAndBlankArtHaveDistinctStatus) {
  Select(0xA7);
  data_.sheet_store.Clear();
  Draw();
  EXPECT_NE(log_.find("Preview unavailable: Graphics sheet"),
            std::string::npos);
  EXPECT_EQ(log_.find("No static OAM layout"), std::string::npos);
  EXPECT_FALSE(
      HasPixel(SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_), 2));
  data_.graphics_buffer.assign(223 * 4096, 0);
  data_.sheet_store.MarkAllSheetsChanged();
  Draw();
  EXPECT_NE(log_.find("no visible pixels for this layout"), std::string::npos);
  EXPECT_EQ(log_.find("Preview unavailable"), std::string::npos);
}

TEST_F(RegisteredSpritePreviewTest,
       OracleOverridesStayProjectLocalAndExternalArtIsExplicit) {
  ASSERT_TRUE(
      project_.hack_manifest
          .LoadFromString(
              R"({"manifest_version":2,"hack_name":"Oracle of Secrets"})")
          .ok());
  deps_.project = &project_;
  editor_->SetDependencies(deps_);
  Select(0xB1);
  EXPECT_NE(log_.find("Sprite: Puffstool (0xB1)"), std::string::npos);
  EXPECT_TRUE(
      HasPixel(SpriteEditorPreviewTestPeer::VanillaBitmap(*editor_), 2));
  Select(0x88);
  EXPECT_NE(log_.find("requires external graphics (Bosses/manhandla.bin)"),
            std::string::npos);
  EXPECT_EQ(log_.find("Sprite: Uncle"), std::string::npos);
  deps_.project = nullptr;
  editor_->SetDependencies(deps_);
  Select(0xB1);
  EXPECT_NE(log_.find("No static OAM layout for sprite $B1"),
            std::string::npos);
  EXPECT_EQ(log_.find("Puffstool"), std::string::npos);
}

TEST_F(RegisteredSpritePreviewTest,
       CustomOracleFrameUsesRegisteredCanvasWithoutBlanketWarning) {
  SpriteEditorPreviewTestPeer::CreateOracleFrame(*editor_);
  Draw("sprite.custom_editor");
  Draw("sprite.custom_editor");
  EXPECT_TRUE(HasPixel(SpriteEditorPreviewTestPeer::CustomBitmap(*editor_), 2));
  EXPECT_EQ(log_.find("Experimental static preview"), std::string::npos);
  EXPECT_EQ(log_.find("Preview unavailable"), std::string::npos);
}

}  // namespace
}  // namespace yaze::editor
