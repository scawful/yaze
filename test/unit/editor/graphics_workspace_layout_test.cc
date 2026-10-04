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
#include "zelda3/game_data.h"

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

  std::optional<gui::WidgetIdRegistry::WidgetInfo> FindWidget(
      const char* label) {
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        return widget;
    }
    return std::nullopt;
  }

  void Click(const char* label, const std::function<void()>& frame) {
    const auto target = FindWidget(label);
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

  // Inspect the actual swatch draw geometry, not a helper that could return the
  // right color while the production widget still renders a stale value.
  std::optional<ImU32> SwatchColor(const char* label) {
    const auto widget = FindWidget(label);
    auto* picker = ActiveChild("##ColorPickerSide");
    if (!widget || !picker)
      return std::nullopt;
    const ImVec2 point(widget->bounds.min_x + 6.5f,
                       widget->bounds.min_y + 6.5f);
    const auto& draw = *picker->DrawList;
    std::optional<ImU32> result;
    for (const auto& command : draw.CmdBuffer) {
      for (unsigned int i = 0; i + 2 < command.ElemCount; i += 3) {
        const auto& a = draw.VtxBuffer[command.VtxOffset +
                                       draw.IdxBuffer[command.IdxOffset + i]];
        const auto& b =
            draw.VtxBuffer[command.VtxOffset +
                           draw.IdxBuffer[command.IdxOffset + i + 1]];
        const auto& c =
            draw.VtxBuffer[command.VtxOffset +
                           draw.IdxBuffer[command.IdxOffset + i + 2]];
        if (a.col == b.col && b.col == c.col &&
            ImTriangleContainsPoint(a.pos, b.pos, c.pos, point)) {
          result = a.col;
        }
      }
    }
    return result;
  }

  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
  GraphicsEditorState state_;
  gfx::Bitmap* bitmap_ = nullptr;
  gfx::Bitmap saved_bitmap_;
};

TEST_F(GraphicsWorkspaceLayoutTest,
       CurrentSwatchFollowsSelectionAndPaletteRow) {
  PixelEditorPanel pixel(&state_, nullptr);
  state_.open_sheets.insert(0);
  std::vector<uint16_t> colors(8, 0x001f);
  colors[2] = 0x7c00;
  bitmap_->SetPalette(gfx::SnesPalette(colors));
  const auto pixels_before = bitmap_->vector();
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  EXPECT_EQ(state_.current_color_index, 1);
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  state_.apply_palette_to_sheet = [&](uint16_t sheet) {
    EXPECT_EQ(sheet, 0);
    colors[1] = 0x03e0;
    bitmap_->SetPalette(gfx::SnesPalette(colors));
  };
  Click("Next palette row", frame);
  EXPECT_EQ(state_.sub_palette_index, 1);
  EXPECT_EQ(state_.current_color_index, 1);
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 255, 0, 255));
  Click("Palette index 2", frame);
  EXPECT_EQ(state_.current_color_index, 2);
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 0, 255, 255));
  EXPECT_EQ(bitmap_->vector(), pixels_before);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       TransparentAndUnavailableSlotsAreNotColors) {
  PixelEditorPanel pixel(&state_, nullptr);
  state_.open_sheets.insert(0);
  bitmap_->SetPalette(gfx::SnesPalette(std::vector<uint16_t>(8, 0x001f)));
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  Click("Palette index 0", frame);
  ASSERT_EQ(state_.current_color_index, 0);
  const auto color = SwatchColor("Current color");
  ASSERT_TRUE(color);
  // A transparent index displays the neutral checkerboard, not palette0 red.
  EXPECT_EQ((*color >> IM_COL32_R_SHIFT) & 255,
            (*color >> IM_COL32_G_SHIFT) & 255);
  EXPECT_EQ((*color >> IM_COL32_G_SHIFT) & 255,
            (*color >> IM_COL32_B_SHIFT) & 255);
  state_.current_color_index = 7;
  bitmap_->SetPalette(gfx::SnesPalette(std::vector<uint16_t>(2, 0x001f)));
  frame();
  EXPECT_FALSE(FindWidget("Current color"));
  EXPECT_EQ(state_.current_color_index, 7);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

TEST_F(GraphicsWorkspaceLayoutTest,
       CurrentSwatchUsesOnlyTheOwningSourcePalette) {
  zelda3::GameData owner;
  zelda3::GameData other;
  PixelEditorPanel pixel(&state_, nullptr);
  state_.open_sheets.insert(0);
  state_.game_data = &owner;
  GraphicsEditContext context;
  context.owner = &owner;
  context.targets.push_back(
      {.sheet_id = 0,
       .label = "Source tile",
       .palette = gfx::SnesPalette(std::vector<uint16_t>(8, 0x001f))});
  state_.source_context = std::move(context);
  const auto frame = [&] {
    Frame([&] { EXPECT_TRUE(pixel.Update().ok()); });
  };
  frame();
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 0, 0, 255));
  state_.source_context->targets[0].palette =
      gfx::SnesPalette(std::vector<uint16_t>(8, 0x03e0));
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(0, 255, 0, 255));
  state_.game_data = &other;
  frame();
  EXPECT_EQ(SwatchColor("Current color"), IM_COL32(255, 255, 255, 255));
  EXPECT_EQ(state_.current_color_index, 1);
  EXPECT_TRUE(state_.modified_sheets.empty());
}

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
