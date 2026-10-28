#include <gtest/gtest.h>

#include "app/gui/core/icons.h"
#include "app/platform/font_loader.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace {

// These icons have vertically symmetric ink in the Material 24dp grid, so
// their ink center is the center of the em box the font loader positions.
constexpr const char* kSymmetricIcons[] = {ICON_MD_SETTINGS, ICON_MD_FOLDER,
                                           ICON_MD_SEARCH, ICON_MD_ADD,
                                           ICON_MD_CLOSE};

// Body sizes reachable through FontGlobalScale plus heading sizes.
constexpr float kFontSizes[] = {12.0f, 13.0f, 14.0f, 15.0f, 16.0f,
                                17.0f, 18.0f, 19.0f, 20.0f, 22.0f,
                                24.0f, 26.0f, 28.0f, 32.0f};

// ImGui snaps the base ascent, the scaled offset and the ink bounds to whole
// pixels, so exact centering is not representable.
constexpr float kMaxCenterErrorPx = 1.0f;

ImWchar Codepoint(const char* utf8) {
  unsigned int codepoint = 0;
  ImTextCharFromUtf8(&codepoint, utf8, nullptr);
  return static_cast<ImWchar>(codepoint);
}

// Vertical center of the first glyph quad drawn after `vtx_begin`.
bool FirstGlyphCenterY(const ImDrawList& draw_list, int vtx_begin,
                       float* center_y) {
  const ImVec2 white = ImGui::GetIO().Fonts->TexUvWhitePixel;
  for (int i = vtx_begin; i + 3 < draw_list.VtxBuffer.Size; ++i) {
    const ImDrawVert* quad = &draw_list.VtxBuffer[i];
    const bool textured = quad[0].uv.x != white.x || quad[0].uv.y != white.y;
    const bool axis_aligned = quad[0].pos.y == quad[1].pos.y &&
                              quad[2].pos.y == quad[3].pos.y &&
                              quad[2].pos.y > quad[0].pos.y;
    const bool glyph =
        quad[0].uv.x != quad[2].uv.x && quad[0].uv.y != quad[2].uv.y;
    if (textured && axis_aligned && glyph) {
      *center_y = (quad[0].pos.y + quad[2].pos.y) * 0.5f;
      return true;
    }
  }
  return false;
}

class IconFontAlignmentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    // Bake glyphs on demand, as the SDL renderer backends do, instead of
    // pre-rasterizing every merged Japanese range for each font.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.IniFilename = nullptr;
    // Tall enough that no row of the frame test is clipped and culled.
    io.DisplaySize = ImVec2(1280.0f, 2048.0f);
    io.DeltaTime = 1.0f / 60.0f;
    const absl::Status status = LoadPackageFonts();
    ASSERT_TRUE(status.ok()) << status.message();
    ASSERT_FALSE(io.Fonts->Fonts.empty());
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(IconFontAlignmentTest, IconGlyphsAreCenteredOnTheLineAtEverySize) {
  for (ImFont* font : ImGui::GetIO().Fonts->Fonts) {
    for (const float size : kFontSizes) {
      ImFontBaked* baked = font->GetFontBaked(size);
      ASSERT_NE(baked, nullptr);
      for (const char* icon : kSymmetricIcons) {
        const ImWchar codepoint = Codepoint(icon);
        const ImFontGlyph* glyph = baked->FindGlyph(codepoint);
        ASSERT_NE(glyph, nullptr);
        ASSERT_EQ(glyph->Codepoint, codepoint) << font->GetDebugName();
        ASSERT_TRUE(glyph->Visible);
        EXPECT_NEAR((glyph->Y0 + glyph->Y1) * 0.5f, size * 0.5f,
                    kMaxCenterErrorPx)
            << font->GetDebugName() << " at " << size << "px, U+" << std::hex
            << codepoint;
      }
    }
  }
}

TEST_F(IconFontAlignmentTest, IconIsCenteredInButtonAndSelectableFrames) {
  ImGui::NewFrame();
  // An auto-sized window has an empty clip rect on its first frame, which
  // culls all text.
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ASSERT_TRUE(ImGui::Begin("icon_alignment"));
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  auto expect_centered = [&](int vtx_begin, const char* what, ImFont* font,
                             float size) {
    float icon_center = 0.0f;
    ASSERT_TRUE(FirstGlyphCenterY(*draw_list, vtx_begin, &icon_center)) << what;
    const float frame_center =
        (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    EXPECT_NEAR(icon_center, frame_center, kMaxCenterErrorPx)
        << what << " " << font->GetDebugName() << " at " << size << "px";
  };
  for (ImFont* font : ImGui::GetIO().Fonts->Fonts) {
    // The editor's own size and the 1.4x heading scale used by dashboards.
    for (const float size : {font->LegacySize, font->LegacySize * 1.4f}) {
      ImGui::PushFont(font, size);
      int vtx_begin = draw_list->VtxBuffer.Size;
      ImGui::Button(ICON_MD_SETTINGS);
      expect_centered(vtx_begin, "icon-only Button", font, size);

      vtx_begin = draw_list->VtxBuffer.Size;
      ImGui::Button(ICON_MD_FOLDER " Open");
      expect_centered(vtx_begin, "icon + label Button", font, size);

      vtx_begin = draw_list->VtxBuffer.Size;
      ImGui::Selectable(ICON_MD_SEARCH " Row", true);
      expect_centered(vtx_begin, "Selectable", font, size);
      ImGui::PopFont();
    }
  }
  ImGui::End();
  ImGui::EndFrame();
}

}  // namespace
}  // namespace yaze
