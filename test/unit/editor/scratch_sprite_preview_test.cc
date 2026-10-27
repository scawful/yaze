#include "app/editor/graphics/ui/scratch/graphics_scratch_view.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

#include "app/editor/sprite/sprite_graphics_context.h"
#include "app/editor/sprite/zsprite.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_id_registry.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

struct ScratchSpritePreviewTestPeer {
  static void DrawPixels(GraphicsScratchView& view) {
    view.DrawPixels(*view.Selected());
  }
  static void DrawPreview(GraphicsScratchView& view) {
    view.DrawAssociatedPreview();
  }
  static void SetColor(GraphicsScratchView& view, int color) {
    view.color_ = color;
  }
  static const gfx::Bitmap& Bitmap(const GraphicsScratchView& view) {
    return view.preview_bitmap_;
  }
};

namespace {

gfx::PaletteGroup CapturedPalettes() {
  gfx::PaletteGroup colors;
  for (int row = 0; row < 8; ++row) {
    gfx::SnesPalette palette;
    for (int color = 0; color < 16; ++color)
      palette.AddColor(gfx::SnesColor(color * 8, row * 16, 0));
    colors.AddPalette(palette);
  }
  return colors;
}

zsprite::Frame CapturedFrame() {
  zsprite::Frame frame;
  frame.Tiles.emplace_back(128, 112, false, false, 0x100, 2, false, 3);
  frame.Tiles.emplace_back(144, 112, false, false, 0x140, 4, false, 3);
  return frame;
}

TEST(ScratchSpritePreviewRenderTest,
     CommonRendererPreservesOtherSlotsAndInput) {
  std::vector<uint8_t> captured(0x10000, 0);
  captured[4 * 4096] = 1;
  captured[5 * 4096] = 6;
  const auto original = captured;
  auto initial = sprite_graphics_context::RenderFrameBuffer(
      CapturedFrame(), captured, CapturedPalettes());
  ASSERT_TRUE(initial.ok()) << initial.status();
  captured[4 * 4096] = 5;
  auto edited = sprite_graphics_context::RenderFrameBuffer(
      CapturedFrame(), captured, CapturedPalettes());
  ASSERT_TRUE(edited.ok()) << edited.status();
  const size_t center = 128 * 256 + 128;
  EXPECT_EQ(initial->pixels[center], 33);
  EXPECT_EQ(edited->pixels[center], 37);
  EXPECT_EQ(initial->pixels[center + 16], 70);
  EXPECT_EQ(edited->pixels[center + 16], 70);
  EXPECT_EQ(captured[5 * 4096], original[5 * 4096]);
  EXPECT_FALSE(
      sprite_graphics_context::RenderFrameBuffer(
          CapturedFrame(), std::vector<uint8_t>(4096), CapturedPalettes())
          .ok());
}

class ScratchSpritePreviewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze-scratch-preview-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    bin_ = directory_ / "pose.bin";
    original_.assign(1536, '\0');
    original_[0] = '\x80';
    std::ofstream output(bin_, std::ios::binary);
    output.write(original_.data(), original_.size());
    output.close();
    view_ = std::make_unique<GraphicsScratchView>(
        &state_, &history_, (directory_ / "library.json").string());
    ASSERT_TRUE(view_->ImportBin(bin_.string(), 3, 0).ok());
    previous_context_ = ImGui::GetCurrentContext();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1400, 1100);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.AddMousePosEvent(-1000, -1000);
  }
  void TearDown() override {
    view_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    gui::WidgetIdRegistry::Instance().Clear();
    if (ImGui::GetCurrentContext())
      ImGui::DestroyContext();
    ImGui::SetCurrentContext(previous_context_);
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }
  void BindPreview() {
    std::vector<uint8_t> captured(0x10000, 0);
    captured[4 * 4096] = 1;
    captured[5 * 4096] = 6;
    view_->SetAssociatedPreview(
        "Captured sprite pose / slot 4",
        [this, captured, frame = CapturedFrame(),
         palettes = CapturedPalettes()](const GraphicsScratchAsset& asset)
            -> absl::StatusOr<usage_preview::PreviewImage> {
          ++renders_;
          auto updated = captured;
          std::copy(asset.pixels.begin(), asset.pixels.end(),
                    updated.begin() + 4 * 4096);
          return sprite_graphics_context::RenderFrameBuffer(frame, updated,
                                                            palettes);
        });
  }
  void Draw() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1000, 1000), ImGuiCond_Always);
    ImGui::Begin("ScratchSpriteHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    view_->FinishGesture();
    ScratchSpritePreviewTestPeer::DrawPixels(*view_);
    ScratchSpritePreviewTestPeer::DrawPreview(*view_);
    log_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
  }
  std::string ReadBin() const {
    std::ifstream input(bin_, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  }
  std::filesystem::path directory_, bin_;
  std::string original_, log_;
  GraphicsEditorState state_;
  UndoManager history_;
  std::unique_ptr<GraphicsScratchView> view_;
  int renders_ = 0;
  ImGuiContext* previous_context_ = nullptr;
};

TEST_F(ScratchSpritePreviewTest,
       PaintingUpdatesPoseBeforeExportAndReusesUnchangedImage) {
  BindPreview();
  Draw();
  Draw();
  ASSERT_EQ(renders_, 1);
  const size_t center = 128 * 256 + 128;
  EXPECT_EQ(ScratchSpritePreviewTestPeer::Bitmap(*view_).vector()[center], 33);
  std::optional<gui::WidgetIdRegistry::WidgetInfo> canvas;
  for (const auto& [path, widget] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets())
    if (widget.label == gui::WidgetIdRegistry::NormalizeLabel("ScratchPixels"))
      canvas = widget;
  ASSERT_TRUE(canvas.has_value());
  ScratchSpritePreviewTestPeer::SetColor(*view_, 5);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(canvas->bounds.min_x + 2, canvas->bounds.min_y + 2);
  Draw();
  io.AddMouseButtonEvent(0, true);
  Draw();
  EXPECT_EQ(view_->library().assets[0].pixels[0], 5);
  EXPECT_EQ(ScratchSpritePreviewTestPeer::Bitmap(*view_).vector()[center], 37);
  EXPECT_EQ(ScratchSpritePreviewTestPeer::Bitmap(*view_).vector()[center + 16],
            70);
  EXPECT_EQ(renders_, 2);
  EXPECT_EQ(ReadBin(), original_);
  io.AddMouseButtonEvent(0, false);
  Draw();
  EXPECT_EQ(renders_, 2);
  ASSERT_TRUE(history_.Undo().ok());
  Draw();
  EXPECT_EQ(ScratchSpritePreviewTestPeer::Bitmap(*view_).vector()[center], 33);
  EXPECT_EQ(ReadBin(), original_);
}

TEST_F(ScratchSpritePreviewTest,
       CapturedPoseDoesNotFollowAnotherImportedAsset) {
  BindPreview();
  Draw();
  EXPECT_NE(log_.find("Captured sprite pose / slot 4"), std::string::npos);
  ASSERT_TRUE(view_->ImportBin(bin_.string(), 3, 0).ok());
  Draw();
  EXPECT_EQ(renders_, 1);
  EXPECT_EQ(log_.find("Associated sprite preview"), std::string::npos);
  EXPECT_EQ(ReadBin(), original_);
}

}  // namespace
}  // namespace yaze::editor
