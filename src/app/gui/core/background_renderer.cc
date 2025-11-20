#include "app/gui/core/background_renderer.h"

#include <algorithm>
#include <cmath>

#include "app/gui/core/theme_manager.h"
#include "app/platform/timing.h"
#include "imgui/imgui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yaze {
namespace gui {

// BackgroundRenderer Implementation
BackgroundRenderer& BackgroundRenderer::Get() {
  static BackgroundRenderer instance;
  return instance;
}

void BackgroundRenderer::RenderDockingBackground(ImDrawList* draw_list,
                                                 const ImVec2& window_pos,
                                                 const ImVec2& window_size,
                                                 const Color& theme_color) {
  if (!draw_list) return;

  UpdateAnimation(TimingManager::Get().GetDeltaTime());

  // Get current theme colors
  auto& theme_manager = ThemeManager::Get();
  auto current_theme = theme_manager.GetCurrentTheme();

  // Create a subtle tinted background
  Color bg_tint = {current_theme.background.red * 1.1f,
                   current_theme.background.green * 1.1f,
                   current_theme.background.blue * 1.1f, 0.3f};

  ImU32 bg_color =
      ImGui::ColorConvertFloat4ToU32(ConvertColorToImVec4(bg_tint));
  draw_list->AddRectFilled(
      window_pos,
      ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
      bg_color);

  // Render the grid if enabled
  if (grid_settings_.grid_size > 0) {
    RenderGridBackground(draw_list, window_pos, window_size, theme_color);
  }

  // Add subtle corner accents
  if (current_theme.enable_glow_effects) {
    float corner_size = 60.0f;
    Color accent_faded = current_theme.accent;
    accent_faded.alpha = 0.1f + 0.05f * sinf(animation_time_ * 2.0f);

    ImU32 corner_color =
        ImGui::ColorConvertFloat4ToU32(ConvertColorToImVec4(accent_faded));

    // Top-left corner
    draw_list->AddRectFilledMultiColor(
        window_pos,
        ImVec2(window_pos.x + corner_size, window_pos.y + corner_size),
        corner_color, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), corner_color);

    // Bottom-right corner
    draw_list->AddRectFilledMultiColor(
        ImVec2(window_pos.x + window_size.x - corner_size,
               window_pos.y + window_size.y - corner_size),
        ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
        IM_COL32(0, 0, 0, 0), corner_color, corner_color, IM_COL32(0, 0, 0, 0));
  }
}

void BackgroundRenderer::RenderGridBackground(ImDrawList* draw_list,
                                              const ImVec2& window_pos,
                                              const ImVec2& window_size,
                                              const Color& grid_color) {
  if (!draw_list || grid_settings_.grid_size <= 0) return;

  // Grid parameters with optional animation
  float grid_size = grid_settings_.grid_size;
  float offset_x = 0.0f;
  float offset_y = 0.0f;

  // Apply animation if enabled
  if (grid_settings_.enable_animation) {
    float animation_offset =
        animation_time_ * grid_settings_.animation_speed * 10.0f;
    offset_x = fmodf(animation_offset, grid_size);
    offset_y = fmodf(animation_offset * 0.7f,
                     grid_size);  // Different speed for interesting effect
  }

  // Window center for radial calculations
  ImVec2 center = ImVec2(window_pos.x + window_size.x * 0.5f,
                         window_pos.y + window_size.y * 0.5f);
  float max_distance =
      sqrtf(window_size.x * window_size.x + window_size.y * window_size.y) *
      0.5f;

  // Apply breathing effect to color if enabled
  Color themed_grid_color = grid_color;
  themed_grid_color.alpha = grid_settings_.opacity;

  if (grid_settings_.enable_breathing) {
    float breathing_factor =
        1.0f + grid_settings_.breathing_intensity *
                   sinf(animation_time_ * grid_settings_.breathing_speed);
    themed_grid_color.red =
        std::min(1.0f, themed_grid_color.red * breathing_factor);
    themed_grid_color.green =
        std::min(1.0f, themed_grid_color.green * breathing_factor);
    themed_grid_color.blue =
        std::min(1.0f, themed_grid_color.blue * breathing_factor);
  }

  if (grid_settings_.enable_dots) {
    // Render grid as dots
    for (float x = window_pos.x - offset_x;
         x < window_pos.x + window_size.x + grid_size; x += grid_size) {
      for (float y = window_pos.y - offset_y;
           y < window_pos.y + window_size.y + grid_size; y += grid_size) {
        ImVec2 dot_pos(x, y);

        // Calculate radial fade
        float fade_factor = 1.0f;
        if (grid_settings_.radial_fade) {
          float distance =
              sqrtf((dot_pos.x - center.x) * (dot_pos.x - center.x) +
                    (dot_pos.y - center.y) * (dot_pos.y - center.y));
          fade_factor =
              1.0f - std::min(distance / grid_settings_.fade_distance, 1.0f);
          fade_factor =
              fade_factor * fade_factor;  // Square for smoother falloff
        }

        if (fade_factor > 0.01f) {
          ImU32 dot_color = BlendColorWithFade(themed_grid_color, fade_factor);
          DrawGridDot(draw_list, dot_pos, dot_color, grid_settings_.dot_size);
        }
      }
    }
  } else {
    // Render grid as lines
    // Vertical lines
    for (float x = window_pos.x - offset_x;
         x < window_pos.x + window_size.x + grid_size; x += grid_size) {
      ImVec2 line_start(x, window_pos.y);
      ImVec2 line_end(x, window_pos.y + window_size.y);

      // Calculate average fade for this line
      float avg_fade = 0.0f;
      if (grid_settings_.radial_fade) {
        for (float y = window_pos.y; y < window_pos.y + window_size.y;
             y += grid_size * 0.5f) {
          float distance = sqrtf((x - center.x) * (x - center.x) +
                                 (y - center.y) * (y - center.y));
          float fade =
              1.0f - std::min(distance / grid_settings_.fade_distance, 1.0f);
          avg_fade += fade * fade;
        }
        avg_fade /= (window_size.y / (grid_size * 0.5f));
      } else {
        avg_fade = 1.0f;
      }

      if (avg_fade > 0.01f) {
        ImU32 line_color = BlendColorWithFade(themed_grid_color, avg_fade);
        DrawGridLine(draw_list, line_start, line_end, line_color,
                     grid_settings_.line_thickness);
      }
    }

    // Horizontal lines
    for (float y = window_pos.y - offset_y;
         y < window_pos.y + window_size.y + grid_size; y += grid_size) {
      ImVec2 line_start(window_pos.x, y);
      ImVec2 line_end(window_pos.x + window_size.x, y);

      // Calculate average fade for this line
      float avg_fade = 0.0f;
      if (grid_settings_.radial_fade) {
        for (float x = window_pos.x; x < window_pos.x + window_size.x;
             x += grid_size * 0.5f) {
          float distance = sqrtf((x - center.x) * (x - center.x) +
                                 (y - center.y) * (y - center.y));
          float fade =
              1.0f - std::min(distance / grid_settings_.fade_distance, 1.0f);
          avg_fade += fade * fade;
        }
        avg_fade /= (window_size.x / (grid_size * 0.5f));
      } else {
        avg_fade = 1.0f;
      }

      if (avg_fade > 0.01f) {
        ImU32 line_color = BlendColorWithFade(themed_grid_color, avg_fade);
        DrawGridLine(draw_list, line_start, line_end, line_color,
                     grid_settings_.line_thickness);
      }
    }
  }
}

void BackgroundRenderer::RenderRadialGradient(ImDrawList* draw_list,
                                              const ImVec2& center,
                                              float radius,
                                              const Color& inner_color,
                                              const Color& outer_color) {
  if (!draw_list) return;

  const int segments = 32;
  const int rings = 8;

  for (int ring = 0; ring < rings; ++ring) {
    float ring_radius = radius * (ring + 1) / rings;
    float inner_ring_radius = radius * ring / rings;

    // Interpolate colors for this ring
    float t = static_cast<float>(ring) / rings;
    Color ring_color = {inner_color.red * (1.0f - t) + outer_color.red * t,
                        inner_color.green * (1.0f - t) + outer_color.green * t,
                        inner_color.blue * (1.0f - t) + outer_color.blue * t,
                        inner_color.alpha * (1.0f - t) + outer_color.alpha * t};

    ImU32 color =
        ImGui::ColorConvertFloat4ToU32(ConvertColorToImVec4(ring_color));

    if (ring == 0) {
      // Center circle
      draw_list->AddCircleFilled(center, ring_radius, color, segments);
    } else {
      // Ring
      for (int i = 0; i < segments; ++i) {
        float angle1 = (2.0f * M_PI * i) / segments;
        float angle2 = (2.0f * M_PI * (i + 1)) / segments;

        ImVec2 p1_inner = ImVec2(center.x + cosf(angle1) * inner_ring_radius,
                                 center.y + sinf(angle1) * inner_ring_radius);
        ImVec2 p2_inner = ImVec2(center.x + cosf(angle2) * inner_ring_radius,
                                 center.y + sinf(angle2) * inner_ring_radius);
        ImVec2 p1_outer = ImVec2(center.x + cosf(angle1) * ring_radius,
                                 center.y + sinf(angle1) * ring_radius);
        ImVec2 p2_outer = ImVec2(center.x + cosf(angle2) * ring_radius,
                                 center.y + sinf(angle2) * ring_radius);

        draw_list->AddQuadFilled(p1_inner, p2_inner, p2_outer, p1_outer, color);
      }
    }
  }
}

void BackgroundRenderer::UpdateAnimation(float delta_time) {
  if (grid_settings_.enable_animation) {
    animation_time_ += delta_time;
  }
}

void BackgroundRenderer::UpdateForTheme(const Color& primary_color,
                                        const Color& background_color) {
  // Create a grid color that's a subtle blend of the theme's primary and
  // background
  cached_grid_color_ = {
      (primary_color.red * 0.3f + background_color.red * 0.7f),
      (primary_color.green * 0.3f + background_color.green * 0.7f),
      (primary_color.blue * 0.3f + background_color.blue * 0.7f),
      grid_settings_.opacity};
}

void BackgroundRenderer::DrawSettingsUI() {
  if (ImGui::CollapsingHeader("Background Grid Settings")) {
    ImGui::Indent();

    ImGui::SliderFloat("Grid Size", &grid_settings_.grid_size, 8.0f, 128.0f,
                       "%.0f px");
    ImGui::SliderFloat("Line Thickness", &grid_settings_.line_thickness, 0.5f,
                       3.0f, "%.1f px");
    ImGui::SliderFloat("Opacity", &grid_settings_.opacity, 0.01f, 0.3f, "%.3f");
    ImGui::SliderFloat("Fade Distance", &grid_settings_.fade_distance, 50.0f,
                       500.0f, "%.0f px");

    ImGui::Separator();
    ImGui::Text("Visual Effects:");
    ImGui::Checkbox("Enable Animation", &grid_settings_.enable_animation);
    ImGui::SameLine();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Makes the grid move slowly across the screen");
    }

    ImGui::Checkbox("Color Breathing", &grid_settings_.enable_breathing);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Grid color pulses with a breathing effect");
    }

    ImGui::Checkbox("Radial Fade", &grid_settings_.radial_fade);
    ImGui::Checkbox("Use Dots Instead of Lines", &grid_settings_.enable_dots);

    // Animation settings (only show if animation is enabled)
    if (grid_settings_.enable_animation) {
      ImGui::Indent();
      ImGui::SliderFloat("Animation Speed", &grid_settings_.animation_speed,
                         0.1f, 3.0f, "%.1fx");
      ImGui::Unindent();
    }

    // Breathing settings (only show if breathing is enabled)
    if (grid_settings_.enable_breathing) {
      ImGui::Indent();
      ImGui::SliderFloat("Breathing Speed", &grid_settings_.breathing_speed,
                         0.5f, 3.0f, "%.1fx");
      ImGui::SliderFloat("Breathing Intensity",
                         &grid_settings_.breathing_intensity, 0.1f, 0.8f,
                         "%.1f");
      ImGui::Unindent();
    }

    if (grid_settings_.enable_dots) {
      ImGui::SliderFloat("Dot Size", &grid_settings_.dot_size, 1.0f, 8.0f,
                         "%.1f px");
    }

    // Preview
    ImGui::Spacing();
    ImGui::Text("Preview:");
    ImVec2 preview_size(200, 100);
    ImVec2 preview_pos = ImGui::GetCursorScreenPos();

    ImDrawList* preview_draw_list = ImGui::GetWindowDrawList();
    auto& theme_manager = ThemeManager::Get();
    auto theme_color = theme_manager.GetCurrentTheme().primary;

    // Draw preview background
    preview_draw_list->AddRectFilled(
        preview_pos,
        ImVec2(preview_pos.x + preview_size.x, preview_pos.y + preview_size.y),
        IM_COL32(30, 30, 30, 255));

    // Draw preview grid
    RenderGridBackground(preview_draw_list, preview_pos, preview_size,
                         theme_color);

    // Advance cursor
    ImGui::Dummy(preview_size);

    ImGui::Unindent();
  }
}

float BackgroundRenderer::CalculateRadialFade(const ImVec2& pos,
                                              const ImVec2& center,
                                              float max_distance) const {
  float distance = sqrtf((pos.x - center.x) * (pos.x - center.x) +
                         (pos.y - center.y) * (pos.y - center.y));
  float fade = 1.0f - std::min(distance / max_distance, 1.0f);
  return fade * fade;  // Square for smoother falloff
}

ImU32 BackgroundRenderer::BlendColorWithFade(const Color& base_color,
                                             float fade_factor) const {
  Color faded_color = {base_color.red, base_color.green, base_color.blue,
                       base_color.alpha * fade_factor};
  return ImGui::ColorConvertFloat4ToU32(ConvertColorToImVec4(faded_color));
}

void BackgroundRenderer::DrawGridLine(ImDrawList* draw_list,
                                      const ImVec2& start, const ImVec2& end,
                                      ImU32 color, float thickness) const {
  draw_list->AddLine(start, end, color, thickness);
}

void BackgroundRenderer::DrawGridDot(ImDrawList* draw_list, const ImVec2& pos,
                                     ImU32 color, float size) const {
  draw_list->AddCircleFilled(pos, size, color);
}

// DockSpaceRenderer Implementation
bool DockSpaceRenderer::background_enabled_ = true;
bool DockSpaceRenderer::grid_enabled_ = true;
bool DockSpaceRenderer::effects_enabled_ = true;
ImVec2 DockSpaceRenderer::last_dockspace_pos_{};
ImVec2 DockSpaceRenderer::last_dockspace_size_{};

void DockSpaceRenderer::BeginEnhancedDockSpace(ImGuiID dockspace_id,
                                               const ImVec2& size,
                                               ImGuiDockNodeFlags flags) {
  // Store window info
  last_dockspace_pos_ = ImGui::GetWindowPos();
  last_dockspace_size_ = ImGui::GetWindowSize();

  // Create the actual dockspace first
  ImGui::DockSpace(dockspace_id, size, flags);

  // NOW draw the background effects on the foreground draw list so they're
  // visible
  if (background_enabled_) {
    ImDrawList* fg_draw_list = ImGui::GetForegroundDrawList();
    auto& theme_manager = ThemeManager::Get();
    auto current_theme = theme_manager.GetCurrentTheme();

    if (grid_enabled_) {
      auto& bg_renderer = BackgroundRenderer::Get();
      // Use the main viewport for full-screen grid
      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      ImVec2 grid_pos = viewport->WorkPos;
      ImVec2 grid_size = viewport->WorkSize;

      // Use subtle grid color that doesn't distract
      Color subtle_grid_color = current_theme.primary;
      // Use the grid settings opacity for consistency
      subtle_grid_color.alpha = bg_renderer.GetGridSettings().opacity;

      bg_renderer.RenderGridBackground(fg_draw_list, grid_pos, grid_size,
                                       subtle_grid_color);
    }
  }
}

void DockSpaceRenderer::EndEnhancedDockSpace() {
  // Additional post-processing effects could go here
  // For now, this is just for API consistency
}

}  // namespace gui
}  // namespace yaze
