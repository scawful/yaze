#include "app/gui/core/draw_sampling.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas_rendering.h"
#include "app/gui/canvas/canvas_state.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace {

// Headless ImGui has no renderer, so these callbacks never run. Each one
// writes a different value so the linker cannot fold them into one function,
// which would make their pointers compare equal.
int last_fake_callback = 0;

void FakeSetSamplerNearest(const ImDrawList*, const ImDrawCmd*) {
  last_fake_callback = 1;
}

void FakeSetSamplerLinear(const ImDrawList*, const ImDrawCmd*) {
  last_fake_callback = 2;
}

void FakeResetRenderState(const ImDrawList*, const ImDrawCmd*) {
  last_fake_callback = 3;
}

const ImTextureID kPixelArtTexture = (ImTextureID)(intptr_t)0x1234;

class DrawSamplingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    saved_reset_ = platform_io.DrawCallback_ResetRenderState;
    saved_linear_ = platform_io.DrawCallback_SetSamplerLinear;
    saved_nearest_ = platform_io.DrawCallback_SetSamplerNearest;
    saved_from_tex_ = platform_io.DrawCallback_SetSamplerFromTex;
  }

  void TearDown() override {
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.DrawCallback_ResetRenderState = saved_reset_;
    platform_io.DrawCallback_SetSamplerLinear = saved_linear_;
    platform_io.DrawCallback_SetSamplerNearest = saved_nearest_;
    platform_io.DrawCallback_SetSamplerFromTex = saved_from_tex_;
    ImGui::DestroyContext(context_);
    context_ = nullptr;
    ImGui::SetCurrentContext(previous_context_);
  }

  void SetCallbacks(ImDrawCallback nearest, ImDrawCallback linear,
                    ImDrawCallback reset) {
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.DrawCallback_SetSamplerNearest = nearest;
    platform_io.DrawCallback_SetSamplerLinear = linear;
    platform_io.DrawCallback_ResetRenderState = reset;
  }

  // Runs one frame, calls draw with the host window's draw list, and returns
  // that draw list's commands after ImGui::Render() has finalized them.
  std::vector<ImDrawCmd> DrawFrame(
      const std::function<void(ImDrawList*)>& draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 240.0f), ImGuiCond_Always);
    ImGui::Begin(
        "DrawSamplingHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw(draw_list);
    ImGui::End();
    ImGui::Render();
    return std::vector<ImDrawCmd>(draw_list->CmdBuffer.begin(),
                                  draw_list->CmdBuffer.end());
  }

  // Encodes the commands as one letter each: N nearest callback, L linear
  // callback, R reset callback, I image of `texture`. Other commands, such as
  // the window background, are skipped.
  static std::string Sequence(const std::vector<ImDrawCmd>& commands,
                              ImTextureID texture = kPixelArtTexture) {
    std::string sequence;
    for (const ImDrawCmd& command : commands) {
      if (command.UserCallback == &FakeSetSamplerNearest) {
        sequence += 'N';
      } else if (command.UserCallback == &FakeSetSamplerLinear) {
        sequence += 'L';
      } else if (command.UserCallback == &FakeResetRenderState) {
        sequence += 'R';
      } else if (command.UserCallback == nullptr && command.ElemCount > 0 &&
                 command.TexRef._TexData == nullptr &&
                 command.GetTexID() == texture) {
        // Font atlas commands have no backend texture in headless ImGui, so
        // only user textures (no _TexData) are read with GetTexID().
        sequence += 'I';
      }
    }
    return sequence;
  }

  static int CallbackCount(const std::vector<ImDrawCmd>& commands) {
    int count = 0;
    for (const ImDrawCmd& command : commands) {
      if (command.UserCallback != nullptr) {
        ++count;
      }
    }
    return count;
  }

  static void AddPixelArtImage(ImDrawList* draw_list) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    draw_list->AddImage(kPixelArtTexture, origin,
                        ImVec2(origin.x + 64.0f, origin.y + 64.0f));
  }

  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  ImDrawCallback saved_reset_ = nullptr;
  ImDrawCallback saved_linear_ = nullptr;
  ImDrawCallback saved_nearest_ = nullptr;
  ImDrawCallback saved_from_tex_ = nullptr;
};

TEST_F(DrawSamplingTest, NearestBeforeImageAndLinearAfterIt) {
  SetCallbacks(&FakeSetSamplerNearest, &FakeSetSamplerLinear,
               &FakeResetRenderState);

  const auto commands = DrawFrame([](ImDrawList* draw_list) {
    ScopedNearestSampling nearest_sampling(draw_list);
    EXPECT_TRUE(nearest_sampling.active());
    AddPixelArtImage(draw_list);
  });

  // The reset callback is not used while a linear callback exists.
  EXPECT_EQ(Sequence(commands), "NIL");
}

TEST_F(DrawSamplingTest, FallsBackToResetWhenBackendHasNoLinearCallback) {
  SetCallbacks(&FakeSetSamplerNearest, nullptr, &FakeResetRenderState);

  const auto commands = DrawFrame([](ImDrawList* draw_list) {
    ScopedNearestSampling nearest_sampling(draw_list);
    AddPixelArtImage(draw_list);
  });

  EXPECT_EQ(Sequence(commands), "NIR");
}

TEST_F(DrawSamplingTest, NoCallbacksWhenSamplerCallbacksAreNull) {
  SetCallbacks(nullptr, nullptr, nullptr);

  const auto commands = DrawFrame([](ImDrawList* draw_list) {
    ScopedNearestSampling nearest_sampling(draw_list);
    EXPECT_FALSE(nearest_sampling.active());
    AddPixelArtImage(draw_list);
  });

  EXPECT_EQ(CallbackCount(commands), 0);
  EXPECT_EQ(Sequence(commands), "I");
}

TEST_F(DrawSamplingTest, NoCallbacksWhenBackendOnlyResetsRenderState) {
  // SDLRenderer2 provides only the reset callback. Adding a reset after the
  // image without a nearest callback before it would change nothing.
  SetCallbacks(nullptr, nullptr, &FakeResetRenderState);

  const auto commands = DrawFrame([](ImDrawList* draw_list) {
    ScopedNearestSampling nearest_sampling(draw_list);
    EXPECT_FALSE(nearest_sampling.active());
    AddPixelArtImage(draw_list);
  });

  EXPECT_EQ(CallbackCount(commands), 0);
  EXPECT_EQ(Sequence(commands), "I");
}

TEST_F(DrawSamplingTest, NestedScopeOnSameDrawListAddsNothing) {
  SetCallbacks(&FakeSetSamplerNearest, &FakeSetSamplerLinear,
               &FakeResetRenderState);

  const auto commands = DrawFrame([](ImDrawList* draw_list) {
    ScopedNearestSampling outer(draw_list);
    {
      ScopedNearestSampling inner(draw_list);
      EXPECT_FALSE(inner.active());
      AddPixelArtImage(draw_list);
    }
    // Still inside the outer scope, so this image must stay nearest.
    AddPixelArtImage(draw_list);
  });

  // ImGui may merge the two same-texture images into one command, so accept
  // one or more image commands. Both must sit between the only nearest and
  // linear callbacks.
  const std::string sequence = Sequence(commands);
  ASSERT_GE(sequence.size(), 3u) << sequence;
  EXPECT_EQ(sequence.front(), 'N') << sequence;
  EXPECT_EQ(sequence.back(), 'L') << sequence;
  EXPECT_EQ(sequence.substr(1, sequence.size() - 2).find_first_not_of('I'),
            std::string::npos)
      << sequence;
}

TEST_F(DrawSamplingTest, NestedScopeOnOtherDrawListIsActive) {
  SetCallbacks(&FakeSetSamplerNearest, &FakeSetSamplerLinear,
               &FakeResetRenderState);

  DrawFrame([](ImDrawList* draw_list) {
    ScopedNearestSampling outer(draw_list);
    ScopedNearestSampling inner(ImGui::GetForegroundDrawList());
    EXPECT_TRUE(outer.active());
    EXPECT_TRUE(inner.active());
  });
}

TEST_F(DrawSamplingTest, NullDrawListIsNoOp) {
  SetCallbacks(&FakeSetSamplerNearest, &FakeSetSamplerLinear,
               &FakeResetRenderState);

  ScopedNearestSampling nearest_sampling(nullptr);
  EXPECT_FALSE(nearest_sampling.active());
}

TEST_F(DrawSamplingTest, RenderBitmapOnCanvasSamplesNearest) {
  SetCallbacks(&FakeSetSamplerNearest, &FakeSetSamplerLinear,
               &FakeResetRenderState);
  gfx::Bitmap bitmap(16, 16, 8, std::vector<uint8_t>(16 * 16, 0));
  // The headless draw list never sends this handle to a renderer.
  bitmap.set_texture(&bitmap);
  const ImTextureID texture = (ImTextureID)(intptr_t)bitmap.texture();

  const auto commands = DrawFrame([&bitmap](ImDrawList* draw_list) {
    CanvasGeometry geometry;
    geometry.canvas_p0 = ImGui::GetCursorScreenPos();
    RenderBitmapOnCanvas(draw_list, geometry, bitmap, 0, 2.0f);
  });
  bitmap.set_texture(nullptr);

  EXPECT_EQ(Sequence(commands, texture), "NIL");
}

TEST_F(DrawSamplingTest, ReleasedSelectionOutlineIsIndependentOfDragDirection) {
  const ImVec2 starts[] = {{16, 32}, {64, 32}, {16, 80}, {64, 80}};
  const ImVec2 ends[] = {{64, 80}, {16, 80}, {64, 32}, {16, 32}};
  std::vector<ImVec2> forward_vertices;
  for (int direction = 0; direction < 4; ++direction) {
    std::vector<ImVec2> vertices;
    DrawFrame([&](ImDrawList* draw_list) {
      CanvasGeometry geometry;
      geometry.canvas_p0 = ImVec2(25.25f, 30.5f);
      geometry.scrolling = ImVec2(2.5f, -1.25f);
      CanvasConfig config;
      config.global_scale = 1.5f;
      ImVector<ImVec2> selected_points;
      selected_points.push_back(starts[direction]);
      selected_points.push_back(ends[direction]);
      const int first_vertex = draw_list->VtxBuffer.Size;
      RenderCanvasOverlay(draw_list, geometry, config, {}, selected_points);
      for (int i = first_vertex; i < draw_list->VtxBuffer.Size; ++i) {
        vertices.push_back(draw_list->VtxBuffer[i].pos);
      }
    });
    ASSERT_FALSE(vertices.empty()) << direction;
    if (direction == 0) {
      forward_vertices = vertices;
      continue;
    }
    ASSERT_EQ(vertices.size(), forward_vertices.size()) << direction;
    for (size_t i = 0; i < vertices.size(); ++i) {
      EXPECT_FLOAT_EQ(vertices[i].x, forward_vertices[i].x) << direction;
      EXPECT_FLOAT_EQ(vertices[i].y, forward_vertices[i].y) << direction;
    }
  }
}

}  // namespace
}  // namespace gui
}  // namespace yaze
