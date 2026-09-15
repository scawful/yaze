#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/shell/coordinator/welcome_screen.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/system/session/user_settings.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "app/platform/timing.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/file_util.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yaze {
namespace editor {

namespace {

// Zelda-inspired color palette (fallbacks)
const ImVec4 kTriforceGoldFallback = ImVec4(1.0f, 0.843f, 0.0f, 1.0f);
const ImVec4 kHyruleGreenFallback = ImVec4(0.133f, 0.545f, 0.133f, 1.0f);
const ImVec4 kMasterSwordBlueFallback = ImVec4(0.196f, 0.6f, 0.8f, 1.0f);
const ImVec4 kHeartRedFallback = ImVec4(0.863f, 0.078f, 0.235f, 1.0f);
const ImVec4 kSpiritOrangeFallback = ImVec4(1.0f, 0.647f, 0.0f, 1.0f);

// Compact recent rows with a fixed comfortable height. The welcome card itself
// stays a GIMP-style start dialog — it does not stretch to fill the dockspace.
constexpr float kRecentRowBaseHeight = 52.0f;
constexpr float kWelcomeSplitMinWidth = 800.0f;
constexpr float kWelcomeSplitMinHeight = 420.0f;
constexpr float kWelcomeCardMaxWidth = 960.0f;
constexpr float kWelcomeCardMaxHeight = 580.0f;

// Active colors (updated each frame from theme)
ImVec4 kTriforceGold = kTriforceGoldFallback;
ImVec4 kHyruleGreen = kHyruleGreenFallback;
ImVec4 kMasterSwordBlue = kMasterSwordBlueFallback;
ImVec4 kHeartRed = kHeartRedFallback;
ImVec4 kSpiritOrange = kSpiritOrangeFallback;

void UpdateWelcomeAccentPalette() {
  auto& theme_mgr = gui::ThemeManager::Get();
  // Skip the palette recompute when the active theme hasn't changed. The
  // The welcome screen previously recomputed its accent palette every frame;
  // skip that work while the active theme is unchanged.
  static std::string s_cached_theme_name;
  static bool s_cached_once = false;
  const std::string& current_name = theme_mgr.GetCurrentThemeName();
  if (s_cached_once && current_name == s_cached_theme_name) {
    return;
  }
  s_cached_theme_name = current_name;
  s_cached_once = true;

  const auto& theme = theme_mgr.GetCurrentTheme();

  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  const ImVec4 warning = gui::ConvertColorToImVec4(theme.warning);
  const ImVec4 success = gui::ConvertColorToImVec4(theme.success);
  const ImVec4 info = gui::ConvertColorToImVec4(theme.info);
  const ImVec4 error = gui::ConvertColorToImVec4(theme.error);

  // Welcome accent palette: themed, but with distinct flavor per role.
  kTriforceGold = ImLerp(accent, warning, 0.55f);
  kHyruleGreen = success;
  kMasterSwordBlue = info;
  kHeartRed = error;
  kSpiritOrange = ImLerp(warning, accent, 0.35f);
}

// Truncate `text` to fit within `max_width` pixels, appending "..." if clipped.
// Uses binary search over byte positions; CalcTextSize is invoked at most
// log2(N) times per call instead of the previous O(N) pop_back loop that
// re-measured the string after every character removal.
std::string EllipsizeText(const std::string& text, float max_width) {
  if (text.empty())
    return std::string();
  if (ImGui::CalcTextSize(text.c_str()).x <= max_width)
    return text;

  static constexpr const char* kEllipsis = "...";
  const float ellipsis_w = ImGui::CalcTextSize(kEllipsis).x;
  if (ellipsis_w > max_width)
    return std::string(kEllipsis);

  const float budget = max_width - ellipsis_w;

  // Binary search the longest byte-prefix whose width is <= budget.
  // Note: this splits by bytes, not code points; for ASCII-only titles (the
  // common case here) that is exact. For UTF-8 multi-byte sequences we nudge
  // the split point back to a code-point boundary after the search.
  size_t lo = 0;
  size_t hi = text.size();
  std::string buffer;
  buffer.reserve(text.size());
  while (lo < hi) {
    size_t mid = lo + (hi - lo + 1) / 2;
    buffer.assign(text, 0, mid);
    if (ImGui::CalcTextSize(buffer.c_str()).x <= budget) {
      lo = mid;
    } else {
      hi = mid - 1;
    }
  }

  // Pull back off any UTF-8 continuation bytes so we don't split a codepoint.
  while (lo > 0 && (static_cast<unsigned char>(text[lo]) & 0xC0) == 0x80) {
    --lo;
  }

  if (lo == 0)
    return std::string(kEllipsis);
  buffer.assign(text, 0, lo);
  buffer.append(kEllipsis);
  return buffer;
}

// Draw a pixelated triforce in the background (ALTTP style)
void DrawTriforceBackground(ImDrawList* draw_list, ImVec2 pos, float size,
                            float alpha, float glow) {
  // Make it pixelated - round size to nearest 4 pixels
  size = std::round(size / 4.0f) * 4.0f;

  // Calculate triangle points with pixel-perfect positioning
  auto triangle = [&](ImVec2 center, float s, ImU32 color) {
    // Round to pixel boundaries for crisp edges
    float half_s = s / 2.0f;
    float tri_h = s * 0.866f;  // Height of equilateral triangle

    // Fixed: Proper equilateral triangle with apex at top
    ImVec2 p1(std::round(center.x),
              std::round(center.y - tri_h / 2.0f));  // Top apex
    ImVec2 p2(std::round(center.x - half_s),
              std::round(center.y + tri_h / 2.0f));  // Bottom left
    ImVec2 p3(std::round(center.x + half_s),
              std::round(center.y + tri_h / 2.0f));  // Bottom right

    draw_list->AddTriangleFilled(p1, p2, p3, color);
  };

  ImVec4 gold_color = kTriforceGold;
  gold_color.w = alpha;
  ImU32 gold = ImGui::GetColorU32(gold_color);

  // Proper triforce layout with three triangles
  float small_size = size / 2.0f;
  float small_height = small_size * 0.866f;

  // Top triangle (centered above)
  triangle(ImVec2(pos.x, pos.y), small_size, gold);

  // Bottom left triangle
  triangle(ImVec2(pos.x - small_size / 2.0f, pos.y + small_height), small_size,
           gold);

  // Bottom right triangle
  triangle(ImVec2(pos.x + small_size / 2.0f, pos.y + small_height), small_size,
           gold);
}

}  // namespace

WelcomeScreen::WelcomeScreen() {
  RefreshRecentProjects();
}

void WelcomeScreen::SetUserSettings(UserSettings* settings) {
  if (!settings)
    return;
  const auto& prefs = settings->prefs();
  triforce_alpha_multiplier_ = prefs.welcome_triforce_alpha;
  triforce_speed_multiplier_ = prefs.welcome_triforce_speed;
  triforce_size_multiplier_ = prefs.welcome_triforce_size;
  particles_enabled_ = prefs.welcome_particles_enabled;
  triforce_mouse_repel_enabled_ = prefs.welcome_mouse_repel_enabled;
}

bool WelcomeScreen::ShouldUseStackedLayout(float content_width,
                                           float content_height,
                                           float layout_scale) {
  // Card and pane minimums scale with the active font. Scale the breakpoint
  // from the same source so accessibility-sized text cannot force the wide
  // layout into a space that only fits it at the default font size.
  const float safe_scale = std::max(layout_scale, 0.01f);
  return content_width < kWelcomeSplitMinWidth * safe_scale ||
         content_height < kWelcomeSplitMinHeight * safe_scale;
}

// Helper function to calculate staggered animation progress
float GetStaggeredEntryProgress(float entry_time, int section_index,
                                float duration, float stagger_delay) {
  float section_start = section_index * stagger_delay;
  float section_time = entry_time - section_start;
  if (section_time < 0.0f) {
    return 0.0f;
  }
  float progress = std::min(section_time / duration, 1.0f);
  // Use EaseOutCubic for smooth deceleration
  float inv = 1.0f - progress;
  return 1.0f - (inv * inv * inv);
}

bool WelcomeScreen::Show(bool* p_open) {
  // Update theme colors each frame
  UpdateWelcomeAccentPalette();

  // Update entry animation time
  if (!entry_animations_started_) {
    entry_time_ = 0.0f;
    entry_animations_started_ = true;
  }
  entry_time_ += ImGui::GetIO().DeltaTime;

  UpdateAnimations();

  // Get mouse position for interactive triforce movement
  ImVec2 mouse_pos = ImGui::GetMousePos();

  bool action_taken = false;

  // Center the window within the dockspace region (accounting for sidebars)
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 viewport_size = viewport->WorkSize;

  // Calculate the dockspace region (excluding sidebars)
  float dockspace_x = viewport->WorkPos.x + left_offset_;
  float dockspace_width = viewport_size.x - left_offset_ - right_offset_;
  if (dockspace_width < 200.0f) {
    dockspace_x = viewport->WorkPos.x;
    dockspace_width = viewport_size.x;
  }
  float dockspace_center_x = dockspace_x + dockspace_width / 2.0f;
  float dockspace_center_y = viewport->WorkPos.y + viewport_size.y / 2.0f;
  ImVec2 center(dockspace_center_x, dockspace_center_y);

  // Compact start card centered in the dockspace. Leave the surrounding canvas
  // visible so this reads like a launcher, not a stretched full-bleed pane.
  const float font_scale = ImGui::GetFontSize() / 16.0f;
  float width = std::clamp(dockspace_width * 0.70f, 560.0f * font_scale,
                           kWelcomeCardMaxWidth * font_scale);
  float height = std::clamp(viewport_size.y * 0.68f, 400.0f * font_scale,
                            kWelcomeCardMaxHeight * font_scale);
  // Treat the viewport as a hard ceiling. The preferred card minimums above
  // yield to a cramped browser/WASM surface instead of pushing the window
  // off-screen.
  width = std::min(width, std::max(1.0f, dockspace_width - 24.0f));
  height = std::min(height, std::max(1.0f, viewport_size.y - 24.0f));

  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

  // Window flags: allow menu bar to be clickable by not bringing to front
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

  gui::StyleVarGuard window_padding_guard(ImGuiStyleVar_WindowPadding,
                                          ImVec2(16, 14));

  if (ImGui::Begin("##WelcomeScreen", p_open, window_flags)) {
    // Esc dismisses the welcome screen when it (or one of its children) has
    // focus. Avoids stealing Esc globally, which would conflict with other
    // editors that use it for their own "cancel current interaction" flow.
    if (p_open != nullptr &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) {
      *p_open = false;
    }

    ImDrawList* bg_draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Interactive scattered triforces (react to mouse position)
    struct TriforceConfig {
      float x_pct, y_pct;  // Base position (percentage of window)
      float size;
      float alpha;
      float repel_distance;  // How far they move away from mouse
    };

    TriforceConfig triforce_configs[] = {
        {0.08f, 0.12f, 36.0f, 0.025f, 50.0f},  // Top left corner
        {0.92f, 0.15f, 34.0f, 0.022f, 50.0f},  // Top right corner
        {0.06f, 0.88f, 32.0f, 0.020f, 45.0f},  // Bottom left
        {0.94f, 0.85f, 34.0f, 0.023f, 50.0f},  // Bottom right
        {0.50f, 0.08f, 38.0f, 0.028f, 55.0f},  // Top center
        {0.50f, 0.92f, 32.0f, 0.020f, 45.0f},  // Bottom center
    };

    // Initialize base positions on first frame
    if (!triforce_positions_initialized_) {
      for (int i = 0; i < kNumTriforces; ++i) {
        float x = window_pos.x + window_size.x * triforce_configs[i].x_pct;
        float y = window_pos.y + window_size.y * triforce_configs[i].y_pct;
        triforce_base_positions_[i] = ImVec2(x, y);
        triforce_positions_[i] = triforce_base_positions_[i];
      }
      triforce_positions_initialized_ = true;
    }

    // Skip the triforce background entirely when the user has faded it out.
    // The alpha_multiplier slider at 0 should mean "no work at all", not
    // "compute positions and draw transparent triangles".
    const bool triforces_visible = triforce_alpha_multiplier_ > 0.001f;

    // Update triforce positions based on mouse interaction + floating animation
    for (int i = 0; triforces_visible && i < kNumTriforces; ++i) {
      // Update base position in case window moved/resized
      float base_x = window_pos.x + window_size.x * triforce_configs[i].x_pct;
      float base_y = window_pos.y + window_size.y * triforce_configs[i].y_pct;
      triforce_base_positions_[i] = ImVec2(base_x, base_y);

      // Slow, subtle floating animation
      float time_offset = i * 1.2f;  // Offset each triforce's animation
      float float_speed_x =
          (0.15f + (i % 2) * 0.1f) * triforce_speed_multiplier_;  // Very slow
      float float_speed_y =
          (0.12f + ((i + 1) % 2) * 0.08f) * triforce_speed_multiplier_;
      float float_amount_x = (20.0f + (i % 2) * 10.0f) *
                             triforce_size_multiplier_;  // Smaller amplitude
      float float_amount_y =
          (25.0f + ((i + 1) % 2) * 15.0f) * triforce_size_multiplier_;

      // Create gentle orbital motion
      float float_x = std::sin(animation_time_ * float_speed_x + time_offset) *
                      float_amount_x;
      float float_y =
          std::cos(animation_time_ * float_speed_y + time_offset * 1.2f) *
          float_amount_y;

      // Calculate distance from mouse
      float dx = triforce_base_positions_[i].x - mouse_pos.x;
      float dy = triforce_base_positions_[i].y - mouse_pos.y;
      float dist = std::sqrt(dx * dx + dy * dy);

      // Calculate repulsion offset with stronger effect
      ImVec2 target_pos = triforce_base_positions_[i];
      float repel_radius =
          200.0f;  // Larger radius for more visible interaction

      // Add floating motion to base position
      target_pos.x += float_x;
      target_pos.y += float_y;

      // Apply mouse repulsion if enabled
      if (triforce_mouse_repel_enabled_ && dist < repel_radius && dist > 0.1f) {
        // Normalize direction away from mouse
        float dir_x = dx / dist;
        float dir_y = dy / dist;

        // Much stronger repulsion when closer with exponential falloff
        float normalized_dist = dist / repel_radius;
        float repel_strength = (1.0f - normalized_dist * normalized_dist) *
                               triforce_configs[i].repel_distance;

        target_pos.x += dir_x * repel_strength;
        target_pos.y += dir_y * repel_strength;
      }

      // Smooth interpolation to target position (faster response)
      // Use TimingManager for accurate delta time
      float lerp_speed = 8.0f * yaze::TimingManager::Get().GetDeltaTime();
      triforce_positions_[i].x +=
          (target_pos.x - triforce_positions_[i].x) * lerp_speed;
      triforce_positions_[i].y +=
          (target_pos.y - triforce_positions_[i].y) * lerp_speed;

      // Draw at current position with alpha multiplier. Skip issuing draw
      // commands for alphas that would quantize to 0 in an 8-bit color.
      float adjusted_alpha =
          triforce_configs[i].alpha * triforce_alpha_multiplier_;
      if (adjusted_alpha < (1.0f / 255.0f)) {
        continue;
      }
      float adjusted_size =
          triforce_configs[i].size * triforce_size_multiplier_;
      DrawTriforceBackground(bg_draw_list, triforce_positions_[i],
                             adjusted_size, adjusted_alpha, 0.0f);
    }

    // Update and draw particle system. Also skipped when the triforce alpha
    // multiplier is 0, because particles inherit that alpha — drawing them
    // invisibly is pure overhead.
    if (particles_enabled_ && triforces_visible) {
      // Spawn new particles
      particle_spawn_accumulator_ +=
          ImGui::GetIO().DeltaTime * particle_spawn_rate_;
      while (particle_spawn_accumulator_ >= 1.0f &&
             active_particle_count_ < kMaxParticles) {
        // Find inactive particle slot
        for (int i = 0; i < kMaxParticles; ++i) {
          if (particles_[i].lifetime <= 0.0f) {
            // Spawn from random triforce
            int source_triforce = rand() % kNumTriforces;
            particles_[i].position = triforce_positions_[source_triforce];

            // Random direction and speed
            float angle = (rand() % 360) * (M_PI / 180.0f);
            float speed = 20.0f + (rand() % 40);
            particles_[i].velocity =
                ImVec2(std::cos(angle) * speed, std::sin(angle) * speed);

            particles_[i].size = 2.0f + (rand() % 4);
            particles_[i].alpha = 0.4f + (rand() % 40) / 100.0f;
            particles_[i].max_lifetime = 2.0f + (rand() % 30) / 10.0f;
            particles_[i].lifetime = particles_[i].max_lifetime;
            active_particle_count_++;
            break;
          }
        }
        particle_spawn_accumulator_ -= 1.0f;
      }

      // Update and draw particles
      float dt = ImGui::GetIO().DeltaTime;
      for (int i = 0; i < kMaxParticles; ++i) {
        if (particles_[i].lifetime > 0.0f) {
          // Update lifetime
          particles_[i].lifetime -= dt;
          if (particles_[i].lifetime <= 0.0f) {
            active_particle_count_--;
            continue;
          }

          // Update position
          particles_[i].position.x += particles_[i].velocity.x * dt;
          particles_[i].position.y += particles_[i].velocity.y * dt;

          // Fade out near end of life
          float life_ratio =
              particles_[i].lifetime / particles_[i].max_lifetime;
          float alpha =
              particles_[i].alpha * life_ratio * triforce_alpha_multiplier_;

          // Draw particle as small golden circle
          ImU32 particle_color = ImGui::GetColorU32(
              ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, alpha));
          bg_draw_list->AddCircleFilled(particles_[i].position,
                                        particles_[i].size, particle_color, 8);
        }
      }
    }

    DrawHeader();

    ImGui::Spacing();

    // Thin accent rule under the brand.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 separator_start = ImGui::GetCursorScreenPos();
    ImVec2 separator_end(separator_start.x + ImGui::GetContentRegionAvail().x,
                         separator_start.y + 1);
    ImVec4 gold_faded = kTriforceGold;
    gold_faded.w = 0.16f;
    ImVec4 blue_faded = kMasterSwordBlue;
    blue_faded.w = 0.16f;
    draw_list->AddRectFilledMultiColor(
        separator_start, separator_end, ImGui::GetColorU32(gold_faded),
        ImGui::GetColorU32(blue_faded), ImGui::GetColorU32(blue_faded),
        ImGui::GetColorU32(gold_faded));

    ImGui::Dummy(ImVec2(0, 8));

    // Reserve the footer from the active font and theme, not a fixed pixel
    // height. The content pane itself never scrolls — recents are capped to
    // what fits so the whole welcome surface stays readable at a glance.
    const float footer_gap = ImGui::GetStyle().ItemSpacing.y;
    const float footer_height = ImGui::GetFrameHeight() + 3.0f * footer_gap;
    ImGui::BeginChild(
        "WelcomeContent", ImVec2(0, -footer_height), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const float content_width = ImGui::GetContentRegionAvail().x;
    const float content_height = ImGui::GetContentRegionAvail().y;
    const float layout_scale = ImGui::GetFontSize() / 16.0f;
    const bool stacked_layout =
        ShouldUseStackedLayout(content_width, content_height, layout_scale);

    if (stacked_layout) {
      DrawFirstRunGuide();
      DrawQuickActions();
      // Prefer Start + Recents when the window is short; What's new is optional.
      if (ImGui::GetContentRegionAvail().y > 220.0f) {
        ImGui::Spacing();
        DrawWhatsNew();
      }
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      DrawRecentProjects();
    } else {
      // Fixed action rail — comfortable width, not a percentage of a huge pane.
      float left_width =
          std::clamp(280.0f * layout_scale, 260.0f * layout_scale,
                     std::min(320.0f * layout_scale, content_width * 0.42f));
      ImGui::BeginChild(
          "LeftPanel", ImVec2(left_width, 0), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
      DrawFirstRunGuide();
      DrawQuickActions();
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      DrawWhatsNew();
      ImGui::EndChild();

      ImGui::SameLine(0.0f, 16.0f);

      ImGui::BeginChild(
          "RightPanel", ImVec2(0, 0), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
      DrawRecentProjects();
      ImGui::EndChild();
    }

    ImGui::EndChild();

    // Footer with subtle gradient
    ImVec2 footer_start = ImGui::GetCursorScreenPos();
    ImVec2 footer_end(footer_start.x + ImGui::GetContentRegionAvail().x,
                      footer_start.y + 1);
    ImVec4 red_faded = kHeartRed;
    red_faded.w = 0.3f;
    ImVec4 green_faded = kHyruleGreen;
    green_faded.w = 0.3f;
    draw_list->AddRectFilledMultiColor(
        footer_start, footer_end, ImGui::GetColorU32(red_faded),
        ImGui::GetColorU32(green_faded), ImGui::GetColorU32(green_faded),
        ImGui::GetColorU32(red_faded));

    ImGui::Dummy(ImVec2(0, footer_gap));
    DrawTipsSection();
  }
  ImGui::End();

  return action_taken;
}

void WelcomeScreen::UpdateAnimations() {
  animation_time_ += ImGui::GetIO().DeltaTime;

  // Note: Triforce positions and particles are updated in Show() based on mouse
  // position
}

void WelcomeScreen::RefreshRecentProjects(bool force) {
  recent_projects_model_.Refresh(force);
}

void WelcomeScreen::DrawHeader() {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Entry animation for header (section 0)
  float header_progress = GetStaggeredEntryProgress(
      entry_time_, 0, kEntryAnimDuration, kEntryStaggerDelay);
  float header_alpha = header_progress;
  float header_offset_y = (1.0f - header_progress) * 20.0f;

  if (header_progress < 0.001f) {
    ImGui::Dummy(ImVec2(0, 48));  // Reserve space
    return;
  }

  ImFont* header_font = nullptr;
  const auto& font_list = ImGui::GetIO().Fonts->Fonts;
  if (font_list.Size > 2) {
    header_font = font_list[2];
  } else if (font_list.Size > 0) {
    header_font = font_list[0];
  }
  if (header_font) {
    ImGui::PushFont(header_font);  // Large font (fallback to default)
  }

  // Simple centered title
  const char* title = ICON_MD_CASTLE " yaze";
  const float window_width = ImGui::GetWindowSize().x;
  const float title_width = ImGui::CalcTextSize(title).x;
  const float xPos = (window_width - title_width) * 0.5f;

  // Apply entry offset
  ImVec2 cursor_pos = ImGui::GetCursorPos();
  ImGui::SetCursorPos(ImVec2(xPos, cursor_pos.y - header_offset_y));
  ImVec2 text_pos = ImGui::GetCursorScreenPos();

  // Subtle static glow behind text (faded by entry alpha)
  float glow_size = 30.0f;
  ImU32 glow_color = ImGui::GetColorU32(ImVec4(
      kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f * header_alpha));
  draw_list->AddCircleFilled(
      ImVec2(text_pos.x + title_width / 2, text_pos.y + 15), glow_size,
      glow_color, 32);

  // Simple gold color for title with entry alpha
  ImVec4 title_color = kTriforceGold;
  title_color.w *= header_alpha;
  ImGui::TextColored(title_color, "%s", title);
  if (header_font) {
    ImGui::PopFont();
  }

  // Static subtitle (entry animation section 1)
  float subtitle_progress = GetStaggeredEntryProgress(
      entry_time_, 1, kEntryAnimDuration, kEntryStaggerDelay);
  float subtitle_alpha = subtitle_progress;
  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();

  const char* subtitle = "Yet Another Zelda3 Editor";
  const float subtitle_width = ImGui::CalcTextSize(subtitle).x;
  ImGui::SetCursorPosX((window_width - subtitle_width) * 0.5f);

  ImGui::TextColored(
      ImVec4(text_secondary.x, text_secondary.y, text_secondary.z,
             text_secondary.w * subtitle_alpha),
      "%s", subtitle);
}

void WelcomeScreen::DrawFirstRunGuide() {
  // Keep the empty-state guidance compact so the primary action stays visible.
  if (!recent_projects_model_.entries().empty() || has_rom_)
    return;

  const float layout_scale = ImGui::GetFontSize() / 16.0f;
  if (ImGui::GetContentRegionAvail().y < 300.0f * layout_scale) {
    return;
  }

  // Entry animation piggybacks on the quick actions section.
  float progress = GetStaggeredEntryProgress(entry_time_, 1, kEntryAnimDuration,
                                             kEntryStaggerDelay);
  if (progress < 0.001f)
    return;
  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, progress);

  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  ImGui::TextColored(kTriforceGold, ICON_MD_AUTO_AWESOME " New here?");
  ImGui::SameLine();
  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped(
        tr("Open a clean .sfc or .smc ROM. Changes are not written until you "
           "choose Save."));
  }
}

void WelcomeScreen::DrawQuickActions() {
  // Entry animation for quick actions (section 2)
  float actions_progress = GetStaggeredEntryProgress(
      entry_time_, 2, kEntryAnimDuration, kEntryStaggerDelay);
  float actions_alpha = actions_progress;
  float actions_offset_x =
      (1.0f - actions_progress) * -30.0f;  // Slide from left

  if (actions_progress < 0.001f) {
    return;  // Don't draw yet
  }

  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, actions_alpha);

  // Apply horizontal offset for slide effect
  float indent = std::max(0.0f, -actions_offset_x);
  if (indent > 0.0f) {
    ImGui::Indent(indent);
  }

  ImGui::TextColored(kSpiritOrange, ICON_MD_BOLT " Start");
  ImGui::Spacing();

  const float scale = ImGui::GetFontSize() / 16.0f;
  const float button_height = std::max(34.0f, 36.0f * scale);
  const float secondary_height = std::max(28.0f, 30.0f * scale);
  const float action_width = ImGui::GetContentRegionAvail().x;
  float button_width = action_width;

  // The browser upload path accepts ROMs only; desktop uses the combined picker.
#ifdef __EMSCRIPTEN__
  constexpr const char* open_label = ICON_MD_FOLDER_OPEN " Open ROM";
  constexpr const char* open_tooltip = ICON_MD_INFO " Open .sfc/.smc ROMs";
#else
  constexpr const char* open_label = ICON_MD_FOLDER_OPEN " Open ROM / Project";
  constexpr const char* open_tooltip =
      ICON_MD_INFO " Open .sfc/.smc ROMs and .yaze/.yazeproj project files";
#endif
  if (gui::PrimaryButton(open_label, ImVec2(button_width, button_height),
                         "welcome_screen", "open_rom_or_project") &&
      open_rom_callback_) {
    open_rom_callback_();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", open_tooltip);
  }

  ImGui::Spacing();

  if (gui::ThemedButton(ICON_MD_ADD_CIRCLE " New Project",
                        ImVec2(button_width, button_height), "welcome_screen",
                        "new_project") &&
      new_project_callback_) {
    new_project_callback_();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        ICON_MD_INFO
        " Create a new project for metadata, labels, and workflow settings");
  }

  // Secondary starts live in the open — no nested "More ways" menu.
  const RecentProject* last_recent = nullptr;
  for (const auto& recent : recent_projects_model_.entries()) {
    if (!recent.unavailable && !recent.is_missing) {
      last_recent = &recent;
      break;
    }
  }
  if (last_recent && open_project_callback_) {
    ImGui::Spacing();
    const std::string resume_label = absl::StrFormat(
        "%s Resume %s", ICON_MD_PLAY_ARROW, last_recent->name.c_str());
    const std::string resume_path = last_recent->filepath;
    if (gui::ThemedButton(resume_label.c_str(),
                          ImVec2(button_width, secondary_height),
                          "welcome_screen", "resume_recent")) {
      open_project_callback_(resume_path);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", resume_path.c_str());
    }
  }

  if (open_prototype_research_callback_ ||
      open_assembly_editor_no_rom_callback_) {
    ImGui::Spacing();
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "%s", tr("Without a ROM"));
  }

  const bool has_both_secondary = open_prototype_research_callback_ &&
                                  open_assembly_editor_no_rom_callback_;
  const bool inline_secondary =
      has_both_secondary && action_width >= 420.0f * scale;
  const float secondary_width =
      inline_secondary ? (button_width - ImGui::GetStyle().ItemSpacing.x) * 0.5f
                       : button_width;

  if (open_prototype_research_callback_) {
    ImGui::Spacing();
    if (gui::ThemedButton(ICON_MD_CONSTRUCTION " Prototype Research",
                          ImVec2(secondary_width, secondary_height),
                          "welcome_screen", "prototype_research")) {
      open_prototype_research_callback_();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          ICON_MD_INFO
          " Open the Graphics editor for CGX, SCR, COL, BIN, and clipboard "
          "work without loading a ROM");
    }
    if (inline_secondary) {
      ImGui::SameLine();
    }
  }

  if (open_assembly_editor_no_rom_callback_) {
    if (!inline_secondary) {
      ImGui::Spacing();
    }
    if (gui::ThemedButton(ICON_MD_CODE " Assembly Editor",
                          ImVec2(secondary_width, secondary_height),
                          "welcome_screen", "assembly_editor")) {
      open_assembly_editor_no_rom_callback_();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          ICON_MD_INFO
          " Open files or a folder for assembly work without loading a ROM");
    }
  }

  // Clean up entry animation styles
  if (indent > 0.0f) {
    ImGui::Unindent(indent);
  }
}

void WelcomeScreen::DrawRecentProjects() {
  // Entry animation for recent projects (section 4)
  float recent_progress = GetStaggeredEntryProgress(
      entry_time_, 4, kEntryAnimDuration, kEntryStaggerDelay);

  if (recent_progress < 0.001f) {
    return;  // Don't draw yet
  }

  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, recent_progress);

  ImGui::TextColored(kMasterSwordBlue, ICON_MD_HISTORY " Recent");

  const float header_spacing = ImGui::GetStyle().ItemSpacing.x;
  const float manage_width = ImGui::CalcTextSize(ICON_MD_FOLDER_SPECIAL).x +
                             ImGui::GetStyle().FramePadding.x * 2.0f;
  const float clear_width = ImGui::CalcTextSize(ICON_MD_DELETE_SWEEP).x +
                            ImGui::GetStyle().FramePadding.x * 2.0f;
  const float total_width = manage_width + clear_width + header_spacing;

  ImGui::SameLine();
  const float start_x = ImGui::GetCursorPosX();
  const float right_edge = start_x + ImGui::GetContentRegionAvail().x;
  const float button_start = std::max(start_x, right_edge - total_width);
  ImGui::SetCursorPosX(button_start);

  bool can_manage = open_project_management_callback_ != nullptr;
  if (!can_manage) {
    ImGui::BeginDisabled();
  }
  if (ImGui::SmallButton(ICON_MD_FOLDER_SPECIAL "##manage_recents")) {
    if (open_project_management_callback_) {
      open_project_management_callback_();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Manage projects"));
  }
  if (!can_manage) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine(0.0f, header_spacing);
  if (ImGui::SmallButton(ICON_MD_DELETE_SWEEP "##clear_recents")) {
    recent_projects_model_.ClearAll();
    RefreshRecentProjects();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Clear recent list"));
  }

  DrawUndoRemovalBanner();

  ImGui::Spacing();

  if (recent_projects_model_.entries().empty()) {
    const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped(tr("No recent files yet. Open a ROM to begin."));
    return;
  }

  const float scale = ImGui::GetFontSize() / 16.0f;
  const float row_height = std::max(44.0f, kRecentRowBaseHeight * scale);
  const float row_gap = ImGui::GetStyle().ItemSpacing.y;
  const float avail_h = ImGui::GetContentRegionAvail().y;
  const float more_line = ImGui::GetTextLineHeightWithSpacing();
  const auto& entries = recent_projects_model_.entries();
  const size_t visible = static_cast<size_t>(
      CalculateVisibleRecentCount(static_cast<int>(entries.size()), avail_h,
                                  row_height, row_gap, more_line));
  for (size_t i = 0; i < visible; ++i) {
    DrawProjectPanel(entries[i], static_cast<int>(i),
                     ImVec2(ImGui::GetContentRegionAvail().x, row_height));
  }
  if (entries.size() > visible) {
    const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::Text(tr("+%zu more in Manage"), entries.size() - visible);
  }

  DrawRecentAnnotationPopup();
}

int WelcomeScreen::CalculateVisibleRecentCount(int entry_count,
                                               float available_height,
                                               float row_height, float row_gap,
                                               float more_line_height) {
  if (entry_count <= 0 || available_height <= 0.0f || row_height <= 0.0f) {
    return 0;
  }

  row_gap = std::max(0.0f, row_gap);
  more_line_height = std::max(0.0f, more_line_height);
  const float row_stride = row_height + row_gap;
  auto rows_that_fit = [&](float height) {
    return std::max(
        0, static_cast<int>((std::max(0.0f, height) + row_gap) / row_stride));
  };

  // First determine whether every entry fits without a hint. If rows must be
  // truncated, reserve the hint before calculating the final row count. This
  // may intentionally leave zero rows in extremely short layouts so the
  // management path remains visible without introducing a hidden scrollbar.
  int max_visible = rows_that_fit(available_height);
  if (entry_count > max_visible) {
    max_visible = rows_that_fit(available_height - more_line_height);
  }

  return std::min(entry_count, max_visible);
}

void WelcomeScreen::DrawRecentAnnotationPopup() {
  if (pending_annotation_kind_ == RecentAnnotationKind::None)
    return;

  // Open once per transition, then keep the modal visible until the user
  // hits Save or Cancel. IsPopupOpen gates the OpenPopup call so we don't
  // re-open every frame.
  const char* kPopupId = "##RecentAnnotationPopup";
  if (!ImGui::IsPopupOpen(kPopupId)) {
    ImGui::OpenPopup(kPopupId);
  }

  ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(kPopupId, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings)) {
    const bool renaming =
        pending_annotation_kind_ == RecentAnnotationKind::Rename;
    ImGui::TextUnformatted(renaming ? ICON_MD_EDIT " Rename"
                                    : ICON_MD_NOTE " Edit Notes");
    const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
    {
      gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
      ImGui::TextWrapped("%s", pending_annotation_path_.c_str());
    }
    ImGui::Spacing();

    bool committed = false;
    if (renaming) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::InputText("##rename_input", rename_buffer_,
                           sizeof(rename_buffer_),
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        committed = true;
      }
      {
        gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
        ImGui::TextWrapped(tr(
            "Leave blank to restore the filename. Affects only how this entry "
            "is displayed on the welcome screen."));
      }
    } else {
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextMultiline("##notes_input", notes_buffer_,
                                sizeof(notes_buffer_), ImVec2(-1, 120));
      {
        gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
        ImGui::TextWrapped(
            tr("Short free-form note shown on hover. Useful for tagging "
               "works-in-progress (\"WIP: palette swap\")."));
      }
    }

    ImGui::Spacing();
    if (ImGui::Button(ICON_MD_CHECK " Save") || committed) {
      if (renaming) {
        recent_projects_model_.SetDisplayName(pending_annotation_path_,
                                              std::string(rename_buffer_));
      } else {
        recent_projects_model_.SetNotes(pending_annotation_path_,
                                        std::string(notes_buffer_));
      }
      pending_annotation_kind_ = RecentAnnotationKind::None;
      pending_annotation_path_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLOSE " Cancel") ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) {
      pending_annotation_kind_ = RecentAnnotationKind::None;
      pending_annotation_path_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void WelcomeScreen::DrawUndoRemovalBanner() {
  if (!recent_projects_model_.HasUndoableRemoval())
    return;

  const auto pending = recent_projects_model_.PeekLastRemoval();
  if (pending.path.empty())
    return;

  // A single-row inline banner reads as ephemeral feedback, not a dialog.
  // We colour it with the theme's warning surface so it visually pairs with
  // destructive-action affordances elsewhere.
  const ImVec4 warning_bg = gui::ConvertColorToImVec4(
      gui::ThemeManager::Get().GetCurrentTheme().warning);
  ImVec4 bg = warning_bg;
  bg.w = 0.18f;

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float row_height = ImGui::GetFrameHeight() + 6.0f;
  const ImVec2 cursor = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(cursor,
                           ImVec2(cursor.x + avail.x, cursor.y + row_height),
                           ImGui::GetColorU32(bg), 4.0f);

  ImGui::Dummy(ImVec2(0, 3.0f));
  ImGui::SameLine(8.0f);
  ImGui::TextColored(warning_bg, ICON_MD_INFO);
  ImGui::SameLine();
  ImGui::Text(tr("Removed \"%s\""), pending.display_name.c_str());
  ImGui::SameLine();

  // Right-align the action buttons inside the banner.
  const float undo_width = ImGui::CalcTextSize(ICON_MD_UNDO " Undo").x +
                           ImGui::GetStyle().FramePadding.x * 2.0f;
  const float dismiss_width = ImGui::CalcTextSize(ICON_MD_CLOSE).x +
                              ImGui::GetStyle().FramePadding.x * 2.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float button_row = undo_width + dismiss_width + spacing;
  const float right_edge =
      ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), right_edge - button_row - 4.0f));

  if (ImGui::SmallButton(ICON_MD_UNDO " Undo")) {
    recent_projects_model_.UndoLastRemoval();
    RefreshRecentProjects(/*force=*/true);
  }
  ImGui::SameLine(0.0f, spacing);
  if (ImGui::SmallButton(ICON_MD_CLOSE "##dismiss_undo")) {
    recent_projects_model_.DismissLastRemoval();
  }
  ImGui::Dummy(ImVec2(0, 2.0f));
}

void WelcomeScreen::DrawProjectPanel(const RecentProject& project, int index,
                                     const ImVec2& card_size) {
  // Disambiguate ImGui IDs without allocating a new std::string every frame
  // (the old code called absl::StrFormat("ProjectPanel_%d", ...) per card).
  ImGui::PushID(index);

  const ImVec4 text_primary = gui::GetOnSurfaceVec4();
  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();

  const ImVec2 resolved_card_size = card_size;
  const bool can_open = !project.is_missing && !project.unavailable;

  ImVec4 accent = kTriforceGold;
  if (project.unavailable) {
    accent = kHeartRed;
  } else if (project.item_type == "ROM") {
    accent = kHyruleGreen;
  } else if (project.item_type == "Project") {
    accent = kMasterSwordBlue;
  }

  // Selectable provides theme Header/HeaderHovered/HeaderActive so hover inherits
  // from the active theme instead of a hand-tinted overlay.
  {
    ImVec4 idle = gui::GetSurfaceVariantVec4();
    idle.w = 0.35f;
    gui::StyleColorGuard header_guard(ImGuiCol_Header, idle);
    const bool is_activated = ImGui::Selectable(
        "##ProjectPanel", false, ImGuiSelectableFlags_AllowDoubleClick,
        resolved_card_size);
    const bool is_hovered = ImGui::IsItemHovered();
    const ImVec2 cursor_pos = ImGui::GetItemRectMin();
    const ImVec2 item_max = ImGui::GetItemRectMax();

    if (ImGui::BeginPopupContextItem("ProjectPanelMenu")) {
      if (project.is_missing) {
        // Missing file: offer relink + forget instead of open. Destructive "Open"
        // is hidden because it would just fail.
        if (ImGui::MenuItem(ICON_MD_SEARCH " Locate...")) {
          const std::string new_path =
              util::FileDialogWrapper::ShowOpenFileDialog();
          if (!new_path.empty() && new_path != project.filepath) {
            recent_projects_model_.RelinkRecent(project.filepath, new_path);
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(tr(
              "Point at the new location for this file. Pin/rename/notes are "
              "preserved."));
        }
      } else if (can_open) {
        if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open")) {
          if (open_project_callback_) {
            open_project_callback_(project.filepath);
          }
        }
      } else {
        ImGui::BeginDisabled();
        ImGui::MenuItem(ICON_MD_WARNING " Re-open required");
        ImGui::EndDisabled();
      }
      ImGui::Separator();
      if (ImGui::MenuItem(project.pinned ? ICON_MD_PUSH_PIN " Unpin"
                                         : ICON_MD_PUSH_PIN " Pin")) {
        recent_projects_model_.SetPinned(project.filepath, !project.pinned);
      }
      if (ImGui::MenuItem(ICON_MD_EDIT " Rename...")) {
        pending_annotation_kind_ = RecentAnnotationKind::Rename;
        pending_annotation_path_ = project.filepath;
        // Seed the buffer with the current display name (empty override falls
        // back to the filename so the user can start from what they see).
        std::snprintf(rename_buffer_, sizeof(rename_buffer_), "%s",
                      project.display_name_override.empty()
                          ? project.name.c_str()
                          : project.display_name_override.c_str());
      }
      if (ImGui::MenuItem(ICON_MD_NOTE " Edit Notes...")) {
        pending_annotation_kind_ = RecentAnnotationKind::EditNotes;
        pending_annotation_path_ = project.filepath;
        std::snprintf(notes_buffer_, sizeof(notes_buffer_), "%s",
                      project.notes.c_str());
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy Path")) {
        ImGui::SetClipboardText(project.filepath.c_str());
      }
      if (ImGui::MenuItem(project.is_missing ? ICON_MD_DELETE_SWEEP " Forget"
                                             : ICON_MD_DELETE_SWEEP
                              " Remove from Recents")) {
        recent_projects_model_.RemoveRecent(project.filepath);
      }
      ImGui::EndPopup();
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec4 border = project.unavailable
                        ? ImVec4(kHeartRed.x, kHeartRed.y, kHeartRed.z, 0.7f)
                        : ImGui::GetStyleColorVec4(ImGuiCol_Border);
    draw_list->AddRect(cursor_pos, item_max, ImGui::GetColorU32(border), 6.0f,
                       0, 1.0f);
    // Accent rail on the left edge for type identity without covering hover.
    draw_list->AddRectFilled(cursor_pos,
                             ImVec2(cursor_pos.x + 3.0f, item_max.y),
                             ImGui::GetColorU32(accent), 2.0f);

    // Compact two-line row: name + secondary meta. Details stay on hover.
    const float padding_x = 10.0f;
    const float padding_y = 6.0f;
    const float icon_radius = 11.0f;
    const float row_h = item_max.y - cursor_pos.y;
    const ImVec2 icon_center(cursor_pos.x + padding_x + icon_radius,
                             cursor_pos.y + row_h * 0.5f);
    draw_list->AddCircleFilled(icon_center, icon_radius,
                               ImGui::GetColorU32(accent), 20);

    const char* item_icon = project.item_icon.empty()
                                ? ICON_MD_INSERT_DRIVE_FILE
                                : project.item_icon.c_str();
    const ImVec2 icon_size = ImGui::CalcTextSize(item_icon);
    draw_list->AddText(ImVec2(icon_center.x - icon_size.x * 0.5f,
                              icon_center.y - icon_size.y * 0.5f),
                       ImGui::GetColorU32(text_primary), item_icon);

    const std::string badge_text =
        project.item_type.empty() ? "File" : project.item_type;
    const ImVec2 badge_text_size = ImGui::CalcTextSize(badge_text.c_str());
    const float badge_pad_x = 5.0f;
    const float badge_pad_y = 1.0f;
    const ImVec2 badge_min(
        item_max.x - padding_x - badge_text_size.x - (badge_pad_x * 2.0f),
        cursor_pos.y + (row_h - badge_text_size.y - badge_pad_y * 2.0f) * 0.5f);
    const ImVec2 badge_max(
        badge_min.x + badge_text_size.x + (badge_pad_x * 2.0f),
        badge_min.y + badge_text_size.y + (badge_pad_y * 2.0f));
    draw_list->AddRectFilled(
        badge_min, badge_max,
        ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.22f)), 3.0f);

    const float content_x = icon_center.x + icon_radius + 10.0f;
    const float content_right = badge_min.x - 8.0f;
    const float text_max_w = std::max(60.0f, content_right - content_x);
    const float line_h = ImGui::GetTextLineHeight();
    const float text_block_h = line_h * 2.0f + 2.0f;
    float text_y =
        cursor_pos.y + std::max(padding_y, (row_h - text_block_h) * 0.5f);

    const std::string raw_name =
        project.pinned
            ? absl::StrFormat("%s %s", ICON_MD_PUSH_PIN, project.name.c_str())
            : project.name;
    const std::string visible_name = EllipsizeText(raw_name, text_max_w);
    draw_list->AddText(ImVec2(content_x, text_y),
                       ImGui::GetColorU32(text_primary), visible_name.c_str());

    text_y += line_h + 2.0f;
    const std::string secondary =
        !project.last_modified.empty()
            ? project.last_modified
            : (!project.rom_title.empty() ? project.rom_title
                                          : project.filepath);
    draw_list->AddText(ImVec2(content_x, text_y),
                       ImGui::GetColorU32(text_secondary),
                       EllipsizeText(secondary, text_max_w).c_str());
    draw_list->AddText(
        ImVec2(badge_min.x + badge_pad_x, badge_min.y + badge_pad_y),
        ImGui::GetColorU32(text_primary), badge_text.c_str());

    if (is_hovered) {
      ImGui::BeginTooltip();
      ImGui::TextColored(kMasterSwordBlue, ICON_MD_INFO " Recent Item");
      ImGui::Separator();
      ImGui::Text(tr("Type: %s"), badge_text.c_str());
      ImGui::Text(tr("Name: %s"), project.name.c_str());
      ImGui::Text(tr("Details: %s"), project.rom_title.c_str());
      if (!project.metadata_summary.empty()) {
        ImGui::Text(tr("Metadata: %s"), project.metadata_summary.c_str());
      }
      ImGui::Text(tr("Last opened: %s"), project.last_modified.c_str());
      ImGui::Text(tr("Path: %s"), project.filepath.c_str());
      ImGui::Separator();
      if (project.is_missing) {
        ImGui::TextColored(kTriforceGold,
                           ICON_MD_SEARCH " Right-click to locate");
      } else if (project.unavailable) {
        ImGui::TextColored(kHeartRed,
                           ICON_MD_WARNING " Re-open from the start actions");
      } else {
        ImGui::TextColored(kTriforceGold, ICON_MD_TOUCH_APP " Click to open");
      }
      ImGui::EndTooltip();
    }

    if (is_activated && can_open && open_project_callback_) {
      open_project_callback_(project.filepath);
    }
  }

  ImGui::PopID();
}

void WelcomeScreen::DrawTipsSection() {
  // Entry animation for tips (section 6, appears last)
  float tips_progress = GetStaggeredEntryProgress(
      entry_time_, 6, kEntryAnimDuration, kEntryStaggerDelay);

  if (tips_progress < 0.001f) {
    return;  // Don't draw yet
  }

  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, tips_progress);

  // Static tip (or could rotate based on session start time rather than
  // animation)
  const char* tips[] = {
      "Open a ROM first, then save a copy before editing",
      "Projects track ROM versions and editor settings",
      "Use Project Management to swap ROMs and manage snapshots",
      "Press Ctrl+Shift+P for the command palette and F1 for help",
      "Shortcuts are configurable in Settings > Keyboard Shortcuts",
      "Project + settings data live under ~/.yaze (user profile on Windows)",
      "Use the panel browser to find any tool quickly"};
  int tip_index = 0;  // Show first tip, or could be random on screen open

  const ImGuiStyle& style = ImGui::GetStyle();
  const std::string close_label =
      absl::StrFormat("%s Don't show again", ICON_MD_CLOSE);
  const float close_width =
      ImGui::CalcTextSize(close_label.c_str()).x + 2.0f * style.FramePadding.x;
  const float close_x =
      ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - close_width;
  const std::string tip = absl::StrFormat("%s %s %s", ICON_MD_LIGHTBULB,
                                          tr("Tip:"), tips[tip_index]);
  const float tip_width =
      close_x - ImGui::GetCursorPosX() - style.ItemSpacing.x;
  if (tip_width > 0.0f) {
    const std::string visible_tip = EllipsizeText(tip, tip_width);
    ImGui::TextColored(gui::GetTextSecondaryVec4(), "%s", visible_tip.c_str());
    if (visible_tip != tip && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tip.c_str());
    }
    ImGui::SameLine(close_x);
  }
  if (ImGui::SmallButton(close_label.c_str())) {
    manually_closed_ = true;
  }
}

void WelcomeScreen::DrawWhatsNew() {
  // Entry animation for what's new (section 5)
  float whatsnew_progress = GetStaggeredEntryProgress(
      entry_time_, 5, kEntryAnimDuration, kEntryStaggerDelay);

  if (whatsnew_progress < 0.001f) {
    return;  // Don't draw yet
  }

  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, whatsnew_progress);

  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_NEW_RELEASES " v%s",
                     YAZE_VERSION_STRING);
  ImGui::SameLine();
  ImGui::TextColored(text_secondary, "%s", tr("highlights"));

  // Drop bullets when the Start column is short so the pane never scrolls.
  const float remaining = ImGui::GetContentRegionAvail().y;
  if (remaining > 110.0f) {
    const char* highlights[] = {
        "Clearer dungeon object layers",
        "Calmer Dungeon Workbench",
        "Cleaner start screen",
    };
    for (const char* highlight : highlights) {
      ImGui::BulletText("%s", highlight);
    }
  }

  if (gui::ThemedButton(ICON_MD_OPEN_IN_NEW " Release notes", ImVec2(-1, 0),
                        "welcome_screen", "view_release_notes")) {
    constexpr char kReleaseNotesUrl[] =
        "https://github.com/scawful/yaze/blob/master/docs/public/"
        "release-notes.md";
    release_notes_open_failed_ = !gui::OpenUrl(kReleaseNotesUrl);
  }

  if (release_notes_open_failed_) {
    const ImVec4 warning = gui::ConvertColorToImVec4(
        gui::ThemeManager::Get().GetCurrentTheme().warning);
    ImGui::TextColored(warning, ICON_MD_INFO);
    ImGui::SameLine();
    ImGui::TextWrapped(tr("Could not open the browser. Release notes: %s"),
                       "https://github.com/scawful/yaze/blob/master/docs/"
                       "public/release-notes.md");
  }
}

}  // namespace editor
}  // namespace yaze
