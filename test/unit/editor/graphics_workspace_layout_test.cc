#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/graphics/pixel_editor_panel.h"
#include "app/editor/graphics/sheet_browser_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/canvas/canvas_rendering.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {

class GraphicsWorkspaceLayoutTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1440, 900);
    io.DeltaTime = 1.0f / 60;
    io.ConfigInputTrickleEventQueue = false;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID((ImTextureID)0x4321);
    bitmap_ = &gfx::Arena::Get().mutable_gfx_sheets()->at(0);
    saved_bitmap_ = std::move(*bitmap_);
    bitmap_->Create(128, 32, 8, std::vector<uint8_t>(4096, 1));
    bitmap_->SetPalette(gfx::SnesPalette(std::vector<uint16_t>(8, 0x7fff)));
    // The headless draw-list test never sends this sentinel to a renderer.
    bitmap_->set_texture(reinterpret_cast<gfx::TextureHandle>(0x1234));
  }

  void TearDown() override {
    bitmap_->set_texture(nullptr);
    *bitmap_ = std::move(saved_bitmap_);
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_);
  }

  void Frame(const std::function<void()>& draw, float width = 640,
             float alpha = 1) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 700), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::Begin("GraphicsHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    draw();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
  }

  ImGuiWindow* ActiveChild(const char* part) {
    for (ImGuiWindow* w : context_->Windows) {
      if (w->Active && std::string(w->Name).find(part) != std::string::npos)
        return w;
    }
    return nullptr;
  }

  void Click(const char* label, const std::function<void()>& frame) {
    std::optional<gui::WidgetIdRegistry::WidgetInfo> target;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        target = widget;
    }
    ASSERT_TRUE(target) << label;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((target->bounds.min_x + target->bounds.max_x) / 2,
                        (target->bounds.min_y + target->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    frame();
  }

  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
  GraphicsEditorState state_;
  gfx::Bitmap* bitmap_ = nullptr;
  gfx::Bitmap saved_bitmap_;
};

TEST_F(GraphicsWorkspaceLayoutTest,
       BrowserStartsWithSheetsAndDetailsRemainReachable) {
  SheetBrowserPanel browser(&state_);
  browser.Initialize();
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(browser.Update().ok()); }, 280);
  };
  frame();
  frame();
  auto* grid = ActiveChild("##SheetGridChild");
  ASSERT_NE(grid, nullptr);
  EXPECT_GT(grid->Size.y, 560);
  EXPECT_EQ(ActiveChild("##SheetDetails"), nullptr);
  auto* thumbnail = ActiveChild("/##Sheet00");
  ASSERT_NE(thumbnail, nullptr);
  EXPECT_LE(thumbnail->Pos.x + thumbnail->Size.x, grid->InnerRect.Max.x);
  Click("Details", frame);
  EXPECT_NE(ActiveChild("##SheetDetails"), nullptr);
  EXPECT_EQ(ActiveChild("##SheetGridChild"), nullptr);
  Click("PNG", frame);
  EXPECT_NE(ActiveChild("##SheetTransfer"), nullptr);
  Click("Sheets", frame);
  Click("Sheet filters", frame);
  EXPECT_TRUE(ImGui::IsPopupOpen("Sheet filters", ImGuiPopupFlags_AnyPopupId));
}

TEST_F(GraphicsWorkspaceLayoutTest,
       PixelToolbarFitsAndViewOptionsAreReachable) {
  PixelEditorPanel pixel(&state_, nullptr);
  for (float width : {440.0f, 640.0f, 980.0f}) {
    const auto frame = [&] {
      Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, width);
    };
    frame();
    frame();
    auto* host = ImGui::FindWindowByName("GraphicsHost");
    ASSERT_NE(host, nullptr);
    EXPECT_LE(host->ContentSize.x, host->InnerRect.GetWidth() + 1) << width;
    auto* content = ActiveChild("##PixelEditorContent");
    ASSERT_NE(content, nullptr);
    EXPECT_GT(content->Size.y, 580) << width;
    auto* canvas = ActiveChild("##CanvasArea");
    ASSERT_NE(canvas, nullptr);
    EXPECT_GT(canvas->Size.x, width - 190) << width;
  }
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  Click("View options", frame);
  EXPECT_TRUE(ImGui::IsPopupOpen("View options", ImGuiPopupFlags_AnyPopupId));
}

TEST_F(GraphicsWorkspaceLayoutTest, PixelTextureUsesTheSameFadeAsItsWindow) {
  PixelEditorPanel pixel(&state_, nullptr);
  for (float alpha : {0.25f, 0.5f, 1.0f}) {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, 640, alpha);
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); }, 640, alpha);
    auto* canvas = ActiveChild("##CanvasArea");
    ASSERT_NE(canvas, nullptr);
    // Checkerboard, grid, and selection marks must not flash opaque either.
    for (const auto& vertex : canvas->DrawList->VtxBuffer) {
      EXPECT_LE((vertex.col >> IM_COL32_A_SHIFT) & 0xff, alpha * 255 + 1);
    }
    int samples = 0;
    for (const auto& command : canvas->DrawList->CmdBuffer) {
      if (command.GetTexID() != (ImTextureID)0x1234)
        continue;
      for (unsigned int i = 0; i < command.ElemCount; ++i) {
        const auto index = canvas->DrawList->IdxBuffer[command.IdxOffset + i];
        const auto color =
            canvas->DrawList->VtxBuffer[command.VtxOffset + index].col;
        EXPECT_NEAR((color >> IM_COL32_A_SHIFT) & 0xff, alpha * 255, 1);
        ++samples;
      }
    }
    EXPECT_GE(samples, 6);
  }
}

TEST_F(GraphicsWorkspaceLayoutTest,
       SharedCanvasPreservesLocalAlphaAndUvDuringFade) {
  Frame(
      [&] {
        auto* draw = ImGui::GetWindowDrawList();
        gui::CanvasGeometry geometry;
        geometry.canvas_p0 = ImVec2(50, 50);
        const int first = draw->VtxBuffer.Size;
        gui::RenderBitmapOnCanvas(draw, geometry, *bitmap_, 0, 1.0f);
        gui::RenderBitmapOnCanvas(draw, geometry, *bitmap_, 0, 0, 1.0f, 128);
        gui::RenderBitmapOnCanvas(draw, geometry, *bitmap_, ImVec2(0, 0),
                                  ImVec2(16, 16), ImVec2(8, 8), ImVec2(16, 16));
        ASSERT_EQ(draw->VtxBuffer.Size - first, 12);
        for (int i = 0; i < 12; ++i) {
          const int expected = i >= 4 && i < 8 ? 64 : 128;
          EXPECT_NEAR(
              (draw->VtxBuffer[first + i].col >> IM_COL32_A_SHIFT) & 255,
              expected, 1);
        }
        EXPECT_FLOAT_EQ(draw->VtxBuffer[first + 8].uv.x, 8.0f / 128);
        EXPECT_FLOAT_EQ(draw->VtxBuffer[first + 8].uv.y, 8.0f / 32);
      },
      640, 0.5f);
}

}  // namespace
}  // namespace yaze::editor
