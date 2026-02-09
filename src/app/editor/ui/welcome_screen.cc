#include "app/editor/ui/welcome_screen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/timing.h"
#include "core/project.h"
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
const ImVec4 kGanonPurpleFallback = ImVec4(0.502f, 0.0f, 0.502f, 1.0f);
const ImVec4 kHeartRedFallback = ImVec4(0.863f, 0.078f, 0.235f, 1.0f);
const ImVec4 kSpiritOrangeFallback = ImVec4(1.0f, 0.647f, 0.0f, 1.0f);
const ImVec4 kShadowPurpleFallback = ImVec4(0.416f, 0.353f, 0.804f, 1.0f);

constexpr float kRecentCardBaseWidth = 220.0f;
constexpr float kRecentCardBaseHeight = 95.0f;
constexpr float kRecentCardWidthMaxFactor = 1.25f;
constexpr float kRecentCardHeightMaxFactor = 1.25f;

// Active colors (updated each frame from theme)
ImVec4 kTriforceGold = kTriforceGoldFallback;
ImVec4 kHyruleGreen = kHyruleGreenFallback;
ImVec4 kMasterSwordBlue = kMasterSwordBlueFallback;
ImVec4 kGanonPurple = kGanonPurpleFallback;
ImVec4 kHeartRed = kHeartRedFallback;
ImVec4 kSpiritOrange = kSpiritOrangeFallback;
ImVec4 kShadowPurple = kShadowPurpleFallback;

void UpdateWelcomeAccentPalette() {
  auto& theme_mgr = gui::ThemeManager::Get();
  const auto& theme = theme_mgr.GetCurrentTheme();

  const ImVec4 secondary = gui::ConvertColorToImVec4(theme.secondary);
  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  const ImVec4 warning = gui::ConvertColorToImVec4(theme.warning);
  const ImVec4 success = gui::ConvertColorToImVec4(theme.success);
  const ImVec4 info = gui::ConvertColorToImVec4(theme.info);
  const ImVec4 error = gui::ConvertColorToImVec4(theme.error);
  const ImVec4 surface = gui::GetSurfaceVec4();

  // Welcome accent palette: themed, but with distinct flavor per role.
  kTriforceGold = ImLerp(accent, warning, 0.55f);
  kHyruleGreen = success;
  kMasterSwordBlue = info;
  kGanonPurple = secondary;
  kHeartRed = error;
  kSpiritOrange = ImLerp(warning, accent, 0.35f);
  kShadowPurple = ImLerp(secondary, surface, 0.45f);
}

std::string GetRelativeTimeString(
    const std::filesystem::file_time_type& ftime) {
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  auto now = std::chrono::system_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::hours>(now - sctp);

  int hours = diff.count();
  if (hours < 24) {
    return "Today";
  } else if (hours < 48) {
    return "Yesterday";
  } else if (hours < 168) {
    int days = hours / 24;
    return absl::StrFormat("%d days ago", days);
  } else if (hours < 720) {
    int weeks = hours / 168;
    return absl::StrFormat("%d week%s ago", weeks, weeks > 1 ? "s" : "");
  } else {
    int months = hours / 720;
    return absl::StrFormat("%d month%s ago", months, months > 1 ? "s" : "");
  }
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

struct GridLayout {
  int columns = 1;
  float item_width = 0.0f;
  float item_height = 0.0f;
  float spacing = 0.0f;
  float row_start_x = 0.0f;
};

GridLayout ComputeGridLayout(float avail_width, float min_width,
                             float max_width, float min_height,
                             float max_height, float preferred_width,
                             float aspect_ratio, float spacing) {
  GridLayout layout;
  layout.spacing = spacing;
  const auto width_for_columns = [avail_width, spacing](int columns) {
    return (avail_width - spacing * static_cast<float>(columns - 1)) /
           static_cast<float>(columns);
  };

  layout.columns = std::max(1, static_cast<int>((avail_width + spacing) /
                                                (preferred_width + spacing)));

  layout.item_width = width_for_columns(layout.columns);
  while (layout.columns > 1 && layout.item_width < min_width) {
    layout.columns -= 1;
    layout.item_width = width_for_columns(layout.columns);
  }

  layout.item_width = std::min(layout.item_width, max_width);
  layout.item_width = std::min(layout.item_width, avail_width);
  layout.item_height =
      std::clamp(layout.item_width * aspect_ratio, min_height, max_height);

  const float row_width =
      layout.item_width * static_cast<float>(layout.columns) +
      spacing * static_cast<float>(layout.columns - 1);
  layout.row_start_x = ImGui::GetCursorPosX();
  if (row_width < avail_width) {
    layout.row_start_x += (avail_width - row_width) * 0.5f;
  }

  return layout;
}

}  // namespace

WelcomeScreen::WelcomeScreen() {
  RefreshRecentProjects();
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

  // Size based on dockspace region, not full viewport
  float width = std::clamp(dockspace_width * 0.85f, 480.0f, 1400.0f);
  float height = std::clamp(viewport_size.y * 0.85f, 360.0f, 900.0f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

  // CRITICAL: Override ImGui's saved window state from imgui.ini
  // Without this, ImGui will restore the last saved state (hidden/collapsed)
  // even when our logic says the window should be visible
  if (first_show_attempt_) {
    ImGui::SetNextWindowCollapsed(false);  // Force window to be expanded
    // Don't steal focus - allow menu bar to remain clickable
    first_show_attempt_ = false;
  }

  // Window flags: allow menu bar to be clickable by not bringing to front
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));

  if (ImGui::Begin("##WelcomeScreen", p_open, window_flags)) {
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

    // Update triforce positions based on mouse interaction + floating animation
    for (int i = 0; i < kNumTriforces; ++i) {
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

      // Draw at current position with alpha multiplier
      float adjusted_alpha =
          triforce_configs[i].alpha * triforce_alpha_multiplier_;
      float adjusted_size =
          triforce_configs[i].size * triforce_size_multiplier_;
      DrawTriforceBackground(bg_draw_list, triforce_positions_[i],
                             adjusted_size, adjusted_alpha, 0.0f);
    }

    // Update and draw particle system
    if (particles_enabled_) {
      // Spawn new particles
      static float spawn_accumulator = 0.0f;
      spawn_accumulator += ImGui::GetIO().DeltaTime * particle_spawn_rate_;
      while (spawn_accumulator >= 1.0f &&
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
        spawn_accumulator -= 1.0f;
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
    ImGui::Spacing();

    // Main content area with subtle gradient separator
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 separator_start = ImGui::GetCursorScreenPos();
    ImVec2 separator_end(separator_start.x + ImGui::GetContentRegionAvail().x,
                         separator_start.y + 1);
    ImVec4 gold_faded = kTriforceGold;
    gold_faded.w = 0.3f;
    ImVec4 blue_faded = kMasterSwordBlue;
    blue_faded.w = 0.3f;
    draw_list->AddRectFilledMultiColor(
        separator_start, separator_end, ImGui::GetColorU32(gold_faded),
        ImGui::GetColorU32(blue_faded), ImGui::GetColorU32(blue_faded),
        ImGui::GetColorU32(gold_faded));

    ImGui::Dummy(ImVec2(0, 10));

    ImGui::BeginChild("WelcomeContent", ImVec2(0, -60), false);
    const float content_width = ImGui::GetContentRegionAvail().x;
    const float content_height = ImGui::GetContentRegionAvail().y;
    const bool narrow_layout = content_width < 900.0f;
    const float layout_scale = ImGui::GetFontSize() / 16.0f;

    if (narrow_layout) {
      float min_left_height = 240.0f * layout_scale;
      float left_height =
          std::clamp(content_height * 0.55f, min_left_height, content_height);
      ImGui::BeginChild("LeftPanel", ImVec2(0, left_height), true,
                        ImGuiWindowFlags_NoScrollbar);
      DrawQuickActions();
      ImGui::Spacing();

      ImVec2 sep_start = ImGui::GetCursorScreenPos();
      draw_list->AddLine(
          sep_start,
          ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
          ImGui::GetColorU32(
              ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.2f)),
          1.0f);

      ImGui::Dummy(ImVec2(0, 5));
      DrawTemplatesSection();
      ImGui::EndChild();

      ImGui::Spacing();

      ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
      DrawRecentProjects();
      ImGui::Spacing();

      sep_start = ImGui::GetCursorScreenPos();
      draw_list->AddLine(
          sep_start,
          ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
          ImGui::GetColorU32(ImVec4(kMasterSwordBlue.x, kMasterSwordBlue.y,
                                    kMasterSwordBlue.z, 0.2f)),
          1.0f);

      ImGui::Dummy(ImVec2(0, 5));
      DrawWhatsNew();
      ImGui::EndChild();
    } else {
      float left_width =
          std::clamp(ImGui::GetContentRegionAvail().x * 0.38f,
                     320.0f * layout_scale, 520.0f * layout_scale);
      ImGui::BeginChild("LeftPanel", ImVec2(left_width, 0), true,
                        ImGuiWindowFlags_NoScrollbar);
      DrawQuickActions();
      ImGui::Spacing();

      ImVec2 sep_start = ImGui::GetCursorScreenPos();
      draw_list->AddLine(
          sep_start,
          ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
          ImGui::GetColorU32(
              ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.2f)),
          1.0f);

      ImGui::Dummy(ImVec2(0, 5));
      DrawTemplatesSection();
      ImGui::EndChild();

      ImGui::SameLine();

      ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
      DrawRecentProjects();
      ImGui::Spacing();

      sep_start = ImGui::GetCursorScreenPos();
      draw_list->AddLine(
          sep_start,
          ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
          ImGui::GetColorU32(ImVec4(kMasterSwordBlue.x, kMasterSwordBlue.y,
                                    kMasterSwordBlue.z, 0.2f)),
          1.0f);

      ImGui::Dummy(ImVec2(0, 5));
      DrawWhatsNew();
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

    ImGui::Dummy(ImVec2(0, 5));
    DrawTipsSection();
  }
  ImGui::End();

  ImGui::PopStyleVar();

  return action_taken;
}

void WelcomeScreen::UpdateAnimations() {
  animation_time_ += ImGui::GetIO().DeltaTime;

  // Update hover scale for cards (smooth interpolation)
  for (int i = 0; i < 6; ++i) {
    float target = (hovered_card_ == i) ? 1.03f : 1.0f;
    card_hover_scale_[i] +=
        (target - card_hover_scale_[i]) * ImGui::GetIO().DeltaTime * 10.0f;
  }

  // Note: Triforce positions and particles are updated in Show() based on mouse
  // position
}

void WelcomeScreen::RefreshRecentProjects() {
  recent_projects_.clear();

  // Use the ProjectManager singleton to get recent files
  auto& manager = project::RecentFilesManager::GetInstance();
  auto recent_files = manager.GetRecentFiles();  // Copy to allow modification

  std::vector<std::string> files_to_remove;

  for (const auto& filepath : recent_files) {
    if (recent_projects_.size() >= kMaxRecentProjects)
      break;

    std::filesystem::path path(filepath);

    // Skip and mark for removal if file doesn't exist
    if (!std::filesystem::exists(path)) {
      files_to_remove.push_back(filepath);
      continue;
    }

    RecentProject project;
    project.filepath = filepath;
    project.name = path.filename().string();

    // Get file modification time
    try {
      auto ftime = std::filesystem::last_write_time(path);
      project.last_modified = GetRelativeTimeString(ftime);
      project.rom_title = "ALTTP ROM";
    } catch (const std::filesystem::filesystem_error&) {
      // File became inaccessible between exists() check and last_write_time()
      files_to_remove.push_back(filepath);
      continue;
    }

    recent_projects_.push_back(project);
  }

  // Remove missing files from the recent files manager
  for (const auto& missing_file : files_to_remove) {
    manager.RemoveFile(missing_file);
  }

  // Save updated list if we removed any files
  if (!files_to_remove.empty()) {
    manager.Save();
  }
}

void WelcomeScreen::DrawHeader() {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Entry animation for header (section 0)
  float header_progress = GetStaggeredEntryProgress(
      entry_time_, 0, kEntryAnimDuration, kEntryStaggerDelay);
  float header_alpha = header_progress;
  float header_offset_y = (1.0f - header_progress) * 20.0f;

  if (header_progress < 0.001f) {
    ImGui::Dummy(ImVec2(0, 80));  // Reserve space
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
  auto windowWidth = ImGui::GetWindowSize().x;
  auto textWidth = ImGui::CalcTextSize(title).x;
  float xPos = (windowWidth - textWidth) * 0.5f;

  // Apply entry offset
  ImVec2 cursor_pos = ImGui::GetCursorPos();
  ImGui::SetCursorPos(ImVec2(xPos, cursor_pos.y - header_offset_y));
  ImVec2 text_pos = ImGui::GetCursorScreenPos();

  // Subtle static glow behind text (faded by entry alpha)
  float glow_size = 30.0f;
  ImU32 glow_color = ImGui::GetColorU32(ImVec4(
      kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f * header_alpha));
  draw_list->AddCircleFilled(
      ImVec2(text_pos.x + textWidth / 2, text_pos.y + 15), glow_size,
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

  const char* subtitle = "Yet Another Zelda3 Editor";
  textWidth = ImGui::CalcTextSize(subtitle).x;
  ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);

  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, subtitle_alpha), "%s", subtitle);

  const std::string version_line = absl::StrFormat(
      "v%s - projects, templates, and editor workflows", YAZE_VERSION_STRING);
  textWidth = ImGui::CalcTextSize(version_line.c_str()).x;
  ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
  ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, subtitle_alpha), "%s",
                     version_line.c_str());

  // Small decorative triforces flanking the title (static, transparent)
  // Positioned well away from text to avoid crowding
  float tri_alpha = 0.12f * header_alpha;
  ImVec2 left_tri_pos(xPos - 80, text_pos.y + 20);
  ImVec2 right_tri_pos(xPos + textWidth + 50, text_pos.y + 20);
  DrawTriforceBackground(draw_list, left_tri_pos, 20, tri_alpha, 0.0f);
  DrawTriforceBackground(draw_list, right_tri_pos, 20, tri_alpha, 0.0f);

  ImGui::Spacing();
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

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, actions_alpha);

  // Apply horizontal offset for slide effect
  float indent = std::max(0.0f, -actions_offset_x);
  if (indent > 0.0f) {
    ImGui::Indent(indent);
  }

  ImGui::TextColored(kSpiritOrange, ICON_MD_BOLT " Quick Actions");
  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);
  ImGui::TextWrapped(
      "Open a ROM or project, or start a new project from a template.");
  ImGui::PopStyleColor();
  ImGui::Spacing();

  const float scale = ImGui::GetFontSize() / 16.0f;
  const float button_height = std::max(38.0f, 40.0f * scale);
  const float action_width = ImGui::GetContentRegionAvail().x;
  const float action_spacing = ImGui::GetStyle().ItemSpacing.x;
  const bool wide_actions = action_width > 360.0f;
  float button_width = action_width;
  const bool has_project = has_project_;

  // Animated button colors (compact height)
  auto draw_action_button = [&](const char* icon, const char* text,
                                const ImVec4& color, bool enabled,
                                std::function<void()> callback) {
    ImGui::PushStyleColor(
        ImGuiCol_Button,
        ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(color.x, color.y, color.z, 1.0f));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));

    if (!enabled)
      ImGui::BeginDisabled();

    bool clicked = ImGui::Button(absl::StrFormat("%s %s", icon, text).c_str(),
                                 ImVec2(button_width, button_height));

    if (!enabled)
      ImGui::EndDisabled();

    ImGui::PopStyleColor(3);

    if (clicked && enabled && callback) {
      callback();
    }

    return clicked;
  };

  if (wide_actions) {
    const float half_width = (action_width - action_spacing) * 0.5f;
    button_width = half_width;
    if (draw_action_button(ICON_MD_FOLDER_OPEN, "Open ROM", kHyruleGreen, true,
                           open_rom_callback_)) {
      // Handled by callback
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_INFO
                        " Open a clean, legally obtained ALttP (USA) ROM file");
    }
    ImGui::SameLine(0.0f, action_spacing);
    if (draw_action_button(ICON_MD_FOLDER_SPECIAL, "Open Project",
                           kMasterSwordBlue,
                           open_project_dialog_callback_ != nullptr,
                           open_project_dialog_callback_)) {
      // Handled by callback
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_INFO
                        " Open an existing .yazeproj bundle or .yaze project file");
    }
    ImGui::Spacing();
  } else {
    // Open ROM button - Green like finding an item
    if (draw_action_button(ICON_MD_FOLDER_OPEN, "Open ROM", kHyruleGreen, true,
                           open_rom_callback_)) {
      // Handled by callback
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_INFO
                        " Open a clean, legally obtained ALttP (USA) ROM file");
    }

    ImGui::Spacing();

    // Open Project button - Blue for project workflows
    if (draw_action_button(ICON_MD_FOLDER_SPECIAL, "Open Project",
                           kMasterSwordBlue,
                           open_project_dialog_callback_ != nullptr,
                           open_project_dialog_callback_)) {
      // Handled by callback
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_INFO
                        " Open an existing .yazeproj bundle or .yaze project file");
    }

    ImGui::Spacing();
  }

  // New Project button - Gold like getting a treasure
  if (draw_action_button(ICON_MD_ADD_CIRCLE, "New Project", kTriforceGold, true,
                         new_project_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(ICON_MD_INFO
                      " Create a new project from a ROM and template");
  }

  ImGui::Spacing();

  // Project tools row
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_TUNE " Project Tools");
  ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);
  ImGui::TextWrapped("Manage snapshots and metadata for the active project.");
  ImGui::PopStyleColor();
  ImGui::Spacing();

  float tool_spacing = ImGui::GetStyle().ItemSpacing.x;
  float tool_width = (ImGui::GetContentRegionAvail().x - tool_spacing) * 0.5f;

  const bool can_manage_project =
      has_project && open_project_management_callback_ != nullptr;
  const bool can_edit_project_file =
      has_project && open_project_file_editor_callback_ != nullptr;

  if (!has_project) {
    ImGui::BeginDisabled();
  }
  button_width = tool_width;
  if (draw_action_button(ICON_MD_FOLDER_SPECIAL, "Project Manager",
                         kMasterSwordBlue, can_manage_project,
                         open_project_management_callback_)) {
    // Handled by callback
  }
  if (!has_project && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Load or create a project to use Project Manager");
  }
  ImGui::SameLine(0.0f, tool_spacing);
  button_width = tool_width;
  if (draw_action_button(ICON_MD_DESCRIPTION, "Project File", kShadowPurple,
                         can_edit_project_file,
                         open_project_file_editor_callback_)) {
    // Handled by callback
  }
  if (!has_project && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Load a project to edit its .yaze file");
  }
  if (!has_project) {
    ImGui::EndDisabled();
  }

  // Clean up entry animation styles
  if (indent > 0.0f) {
    ImGui::Unindent(indent);
  }
  ImGui::PopStyleVar();  // Alpha
}

void WelcomeScreen::DrawRecentProjects() {
  // Entry animation for recent projects (section 4)
  float recent_progress = GetStaggeredEntryProgress(
      entry_time_, 4, kEntryAnimDuration, kEntryStaggerDelay);

  if (recent_progress < 0.001f) {
    return;  // Don't draw yet
  }

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, recent_progress);

  ImGui::TextColored(kMasterSwordBlue, ICON_MD_HISTORY " Recent Projects");

  const float header_spacing = ImGui::GetStyle().ItemSpacing.x;
  const float manage_width = ImGui::CalcTextSize(" Manage").x +
                             ImGui::CalcTextSize(ICON_MD_FOLDER_SPECIAL).x +
                             ImGui::GetStyle().FramePadding.x * 2.0f;
  const float clear_width = ImGui::CalcTextSize(" Clear").x +
                            ImGui::CalcTextSize(ICON_MD_DELETE_SWEEP).x +
                            ImGui::GetStyle().FramePadding.x * 2.0f;
  const float total_width = manage_width + clear_width + header_spacing;

  ImGui::SameLine();
  float start_x = ImGui::GetCursorPosX();
  float right_edge = start_x + ImGui::GetContentRegionAvail().x;
  float button_start = std::max(start_x, right_edge - total_width);
  ImGui::SetCursorPosX(button_start);

  bool can_manage = open_project_management_callback_ != nullptr;
  if (!can_manage) {
    ImGui::BeginDisabled();
  }
  if (ImGui::SmallButton(
          absl::StrFormat("%s Manage", ICON_MD_FOLDER_SPECIAL).c_str())) {
    if (open_project_management_callback_) {
      open_project_management_callback_();
    }
  }
  if (!can_manage) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine(0.0f, header_spacing);
  if (ImGui::SmallButton(
          absl::StrFormat("%s Clear", ICON_MD_DELETE_SWEEP).c_str())) {
    auto& manager = project::RecentFilesManager::GetInstance();
    manager.Clear();
    manager.Save();
    RefreshRecentProjects();
  }

  ImGui::Spacing();

  if (recent_projects_.empty()) {
    // Simple empty state
    const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
    ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);

    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(cursor.x + ImGui::GetContentRegionAvail().x * 0.3f);
    ImGui::TextColored(
        ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.8f),
        ICON_MD_EXPLORE);
    ImGui::SetCursorPosX(cursor.x);

    ImGui::TextWrapped(
        "No recent projects yet.\nOpen a ROM or start a new project to begin "
        "your adventure!");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();  // Alpha (entry animation)
    return;
  }

  const float scale = ImGui::GetFontSize() / 16.0f;
  const float min_width = kRecentCardBaseWidth * scale;
  const float max_width =
      kRecentCardBaseWidth * kRecentCardWidthMaxFactor * scale;
  const float min_height = kRecentCardBaseHeight * scale;
  const float max_height =
      kRecentCardBaseHeight * kRecentCardHeightMaxFactor * scale;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float aspect_ratio = min_height / std::max(min_width, 1.0f);

  GridLayout layout = ComputeGridLayout(
      ImGui::GetContentRegionAvail().x, min_width, max_width, min_height,
      max_height, min_width, aspect_ratio, spacing);

  int column = 0;
  for (size_t i = 0; i < recent_projects_.size(); ++i) {
    if (column == 0) {
      ImGui::SetCursorPosX(layout.row_start_x);
    }

    DrawProjectPanel(recent_projects_[i], static_cast<int>(i),
                     ImVec2(layout.item_width, layout.item_height));

    column += 1;
    if (column < layout.columns) {
      ImGui::SameLine(0.0f, layout.spacing);
    } else {
      column = 0;
      ImGui::Spacing();
    }
  }

  if (pending_recent_refresh_) {
    RefreshRecentProjects();
    pending_recent_refresh_ = false;
  }

  if (column != 0) {
    ImGui::NewLine();
  }

  ImGui::PopStyleVar();  // Alpha (entry animation)
}

void WelcomeScreen::DrawProjectPanel(const RecentProject& project, int index,
                                     const ImVec2& card_size) {
  ImGui::BeginGroup();

  const ImVec4 surface = gui::GetSurfaceVec4();
  const ImVec4 surface_variant = gui::GetSurfaceVariantVec4();
  const ImVec4 text_primary = gui::GetOnSurfaceVec4();
  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  const ImVec4 text_disabled = gui::GetTextDisabledVec4();

  ImVec2 resolved_card_size = card_size;
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

  // Subtle hover scale (only on actual hover, no animation)
  float hover_scale = card_hover_scale_[index];
  if (hover_scale != 1.0f) {
    ImVec2 center(cursor_pos.x + resolved_card_size.x / 2,
                  cursor_pos.y + resolved_card_size.y / 2);
    cursor_pos.x = center.x - (resolved_card_size.x * hover_scale) / 2;
    cursor_pos.y = center.y - (resolved_card_size.y * hover_scale) / 2;
    resolved_card_size.x *= hover_scale;
    resolved_card_size.y *= hover_scale;
  }

  const float layout_scale = resolved_card_size.y / kRecentCardBaseHeight;
  const float padding = 8.0f * layout_scale;
  const float icon_radius = 15.0f * layout_scale;
  const float icon_offset = 13.0f * layout_scale;
  const float text_offset = 32.0f * layout_scale;
  const float name_offset_y = 8.0f * layout_scale;
  const float rom_offset_y = 35.0f * layout_scale;
  const float path_offset_y = 58.0f * layout_scale;

  // Draw card background with subtle gradient
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Gradient background
  ImVec4 color_top = ImLerp(surface_variant, surface, 0.7f);
  ImVec4 color_bottom = ImLerp(surface_variant, surface, 0.3f);
  ImU32 color_top_u32 = ImGui::GetColorU32(color_top);
  ImU32 color_bottom_u32 = ImGui::GetColorU32(color_bottom);
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + resolved_card_size.x,
             cursor_pos.y + resolved_card_size.y),
      color_top_u32, color_top_u32, color_bottom_u32, color_bottom_u32);

  // Static themed border
  ImVec4 border_color_base = (index % 3 == 0)   ? kHyruleGreen
                             : (index % 3 == 1) ? kMasterSwordBlue
                                                : kTriforceGold;
  ImU32 border_color = ImGui::GetColorU32(ImVec4(
      border_color_base.x, border_color_base.y, border_color_base.z, 0.5f));

  draw_list->AddRect(cursor_pos,
                     ImVec2(cursor_pos.x + resolved_card_size.x,
                            cursor_pos.y + resolved_card_size.y),
                     border_color, 6.0f, 0, 2.0f);

  // Make the card clickable
  ImGui::SetCursorScreenPos(cursor_pos);
  ImGui::InvisibleButton(absl::StrFormat("ProjectPanel_%d", index).c_str(),
                         resolved_card_size);
  bool is_hovered = ImGui::IsItemHovered();
  bool is_clicked = ImGui::IsItemClicked();

  hovered_card_ =
      is_hovered ? index : (hovered_card_ == index ? -1 : hovered_card_);

  if (ImGui::BeginPopupContextItem(
          absl::StrFormat("ProjectPanelMenu_%d", index).c_str())) {
    if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open")) {
      if (open_project_callback_) {
        open_project_callback_(project.filepath);
      }
    }
    if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy Path")) {
      ImGui::SetClipboardText(project.filepath.c_str());
    }
    if (ImGui::MenuItem(ICON_MD_DELETE_SWEEP " Remove from Recents")) {
      auto& manager = project::RecentFilesManager::GetInstance();
      manager.RemoveFile(project.filepath);
      manager.Save();
      pending_recent_refresh_ = true;
    }
    ImGui::EndPopup();
  }

  // Subtle hover glow (no particles)
  if (is_hovered) {
    ImU32 hover_color = ImGui::GetColorU32(
        ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f));
    draw_list->AddRectFilled(cursor_pos,
                             ImVec2(cursor_pos.x + resolved_card_size.x,
                                    cursor_pos.y + resolved_card_size.y),
                             hover_color, 6.0f);
  }

  // Draw content (tighter layout)
  ImVec2 content_pos(cursor_pos.x + padding, cursor_pos.y + padding);

  // Icon with colored background circle (compact)
  ImVec2 icon_center(content_pos.x + icon_offset, content_pos.y + icon_offset);
  ImU32 icon_bg = ImGui::GetColorU32(border_color_base);
  draw_list->AddCircleFilled(icon_center, icon_radius, icon_bg, 24);

  // Center the icon properly
  ImVec2 icon_size = ImGui::CalcTextSize(ICON_MD_VIDEOGAME_ASSET);
  ImGui::SetCursorScreenPos(
      ImVec2(icon_center.x - icon_size.x / 2, icon_center.y - icon_size.y / 2));
  ImGui::PushStyleColor(ImGuiCol_Text, text_primary);
  ImGui::Text(ICON_MD_VIDEOGAME_ASSET);
  ImGui::PopStyleColor();

  // Project name (compact, shorten if too long)
  ImGui::SetCursorScreenPos(
      ImVec2(content_pos.x + text_offset, content_pos.y + name_offset_y));
  ImGui::PushTextWrapPos(cursor_pos.x + resolved_card_size.x - padding);
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Default font
  std::string short_name = project.name;
  const float approx_char_width = ImGui::CalcTextSize("A").x;
  const float name_max_width =
      std::max(120.0f, resolved_card_size.x - text_offset - padding);
  size_t max_chars = static_cast<size_t>(
      std::max(12.0f, name_max_width / std::max(approx_char_width, 1.0f)));
  if (short_name.length() > max_chars) {
    short_name = short_name.substr(0, max_chars - 3) + "...";
  }
  ImGui::TextColored(kTriforceGold, "%s", short_name.c_str());
  ImGui::PopFont();
  ImGui::PopTextWrapPos();

  // ROM title (compact)
  ImGui::SetCursorScreenPos(
      ImVec2(content_pos.x + padding * 0.5f, content_pos.y + rom_offset_y));
  ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);
  ImGui::Text(ICON_MD_GAMEPAD " %s", project.rom_title.c_str());
  ImGui::PopStyleColor();

  // Path in card (compact)
  ImGui::SetCursorScreenPos(
      ImVec2(content_pos.x + padding * 0.5f, content_pos.y + path_offset_y));
  ImGui::PushStyleColor(ImGuiCol_Text, text_disabled);
  std::string short_path = project.filepath;
  const float path_max_width =
      std::max(120.0f, resolved_card_size.x - padding * 2.0f);
  size_t max_path_chars = static_cast<size_t>(
      std::max(18.0f, path_max_width / std::max(approx_char_width, 1.0f)));
  if (short_path.length() > max_path_chars) {
    short_path =
        "..." + short_path.substr(short_path.length() - (max_path_chars - 3));
  }
  ImGui::Text(ICON_MD_FOLDER " %s", short_path.c_str());
  ImGui::PopStyleColor();

  // Tooltip
  if (is_hovered) {
    ImGui::BeginTooltip();
    ImGui::TextColored(kMasterSwordBlue, ICON_MD_INFO " Project Details");
    ImGui::Separator();
    ImGui::Text("Name: %s", project.name.c_str());
    ImGui::Text("ROM: %s", project.rom_title.c_str());
    ImGui::Text("Last opened: %s", project.last_modified.c_str());
    ImGui::Text("Path: %s", project.filepath.c_str());
    ImGui::Separator();
    ImGui::TextColored(kTriforceGold, ICON_MD_TOUCH_APP " Click to open");
    ImGui::EndTooltip();
  }

  // Handle click
  if (is_clicked && open_project_callback_) {
    open_project_callback_(project.filepath);
  }

  ImGui::EndGroup();
}

void WelcomeScreen::DrawTemplatesSection() {
  // Entry animation for templates (section 3)
  float templates_progress = GetStaggeredEntryProgress(
      entry_time_, 3, kEntryAnimDuration, kEntryStaggerDelay);

  if (templates_progress < 0.001f) {
    return;  // Don't draw yet
  }

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, templates_progress);

  // Header with visual settings button
  float content_width = ImGui::GetContentRegionAvail().x;
  ImGui::TextColored(kGanonPurple, ICON_MD_LAYERS " Project Templates");
  ImGui::SameLine(content_width - 25);
  if (ImGui::SmallButton(show_triforce_settings_ ? ICON_MD_CLOSE
                                                 : ICON_MD_TUNE)) {
    show_triforce_settings_ = !show_triforce_settings_;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(ICON_MD_AUTO_AWESOME " Visual Effects Settings");
  }

  ImGui::Spacing();

  // Visual effects settings panel (when opened)
  if (show_triforce_settings_) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.15f, 0.22f, 0.4f));
    ImGui::BeginChild("VisualSettingsCompact", ImVec2(0, 115), true,
                      ImGuiWindowFlags_NoScrollbar);
    {
      ImGui::TextColored(kGanonPurple, ICON_MD_AUTO_AWESOME " Visual Effects");
      ImGui::Spacing();

      ImGui::Text(ICON_MD_OPACITY " Visibility");
      ImGui::SetNextItemWidth(-1);
      ImGui::SliderFloat("##visibility", &triforce_alpha_multiplier_, 0.0f,
                         3.0f, "%.1fx");

      ImGui::Text(ICON_MD_SPEED " Speed");
      ImGui::SetNextItemWidth(-1);
      ImGui::SliderFloat("##speed", &triforce_speed_multiplier_, 0.05f, 1.0f,
                         "%.2fx");

      ImGui::Checkbox(ICON_MD_MOUSE " Mouse Interaction",
                      &triforce_mouse_repel_enabled_);
      ImGui::SameLine();
      ImGui::Checkbox(ICON_MD_AUTO_FIX_HIGH " Particles", &particles_enabled_);

      if (ImGui::SmallButton(ICON_MD_REFRESH " Reset")) {
        triforce_alpha_multiplier_ = 1.0f;
        triforce_speed_multiplier_ = 0.3f;
        triforce_size_multiplier_ = 1.0f;
        triforce_mouse_repel_enabled_ = true;
        particles_enabled_ = true;
        particle_spawn_rate_ = 2.0f;
      }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }

  ImGui::Spacing();

  struct Template {
    const char* icon;
    const char* name;
    const char* description;
    const char* template_id;
    const char** details;
    int detail_count;
    ImVec4 color;
  };

  const char* vanilla_details[] = {"No custom ASM required",
                                   "Best for vanilla-compatible edits",
                                   "Overworld data stays vanilla"};
  const char* zso3_details[] = {"Expanded overworld (wide/tall areas)",
                                "Entrances, exits, items, and properties",
                                "Palettes, GFX groups, dungeon maps"};
  const char* zso2_details[] = {"Custom overworld maps + parent system",
                                "Lightweight expansion for legacy hacks",
                                "Palette + BG color support"};
  const char* rando_details[] = {"Avoids overworld remap + ASM features",
                                 "Safe for rando patch pipelines",
                                 "Minimal save surface"};

  Template templates[] = {
      {ICON_MD_COTTAGE, "Vanilla ROM Hack",
       "Standard editing without custom ASM patches. Ideal for vanilla edits.",
       "Vanilla ROM Hack", vanilla_details,
       static_cast<int>(sizeof(vanilla_details) / sizeof(vanilla_details[0])),
       kHyruleGreen},
      {ICON_MD_TERRAIN, "ZSCustomOverworld v3",
       "Full overworld expansion with modern ZSO feature coverage.",
       "ZSCustomOverworld v3", zso3_details,
       static_cast<int>(sizeof(zso3_details) / sizeof(zso3_details[0])),
       kMasterSwordBlue},
      {ICON_MD_MAP, "ZSCustomOverworld v2",
       "Legacy overworld expansion for older ZSO projects.",
       "ZSCustomOverworld v2", zso2_details,
       static_cast<int>(sizeof(zso2_details) / sizeof(zso2_details[0])),
       kShadowPurple},
      {ICON_MD_SHUFFLE, "Randomizer Compatible",
       "Minimal changes that stay friendly to randomizer patches.",
       "Randomizer Compatible", rando_details,
       static_cast<int>(sizeof(rando_details) / sizeof(rando_details[0])),
       kSpiritOrange},
  };

  const int template_count =
      static_cast<int>(sizeof(templates) / sizeof(templates[0]));
  if (selected_template_ < 0 || selected_template_ >= template_count) {
    selected_template_ = 0;
  }

  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  const float template_width = ImGui::GetContentRegionAvail().x;
  const float scale = ImGui::GetFontSize() / 16.0f;
  const bool stack_templates = template_width < 520.0f;

  auto draw_template_list = [&]() {
    for (int i = 0; i < template_count; ++i) {
      bool is_selected = (selected_template_ == i);

      if (is_selected) {
        ImGui::PushStyleColor(
            ImGuiCol_Header,
            ImVec4(templates[i].color.x * 0.6f, templates[i].color.y * 0.6f,
                   templates[i].color.z * 0.6f, 0.6f));
      }

      ImGui::PushID(i);
      ImGui::PushStyleColor(ImGuiCol_Text, templates[i].color);
      if (ImGui::Selectable(
              absl::StrFormat("%s %s", templates[i].icon, templates[i].name)
                  .c_str(),
              is_selected)) {
        selected_template_ = i;
      }
      ImGui::PopStyleColor();
      ImGui::PopID();

      if (is_selected) {
        ImGui::PopStyleColor();
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s %s\n%s", ICON_MD_INFO, templates[i].name,
                          templates[i].description);
      }
    }
  };

  auto draw_template_details = [&]() {
    const Template& active = templates[selected_template_];
    ImGui::TextColored(active.color, "%s %s", active.icon, active.name);
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped("%s", active.description);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextColored(kTriforceGold, ICON_MD_CHECK_CIRCLE " Includes");
    for (int i = 0; i < active.detail_count; ++i) {
      ImGui::Bullet();
      ImGui::SameLine();
      ImGui::TextColored(text_secondary, "%s", active.details[i]);
    }
  };

  if (stack_templates) {
    const float row_height = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float list_height = std::clamp(row_height * (template_count + 1),
                                         120.0f * scale, 200.0f * scale);
    ImGui::BeginChild("TemplateList", ImVec2(0, list_height), false,
                      ImGuiWindowFlags_NoScrollbar);
    draw_template_list();
    ImGui::EndChild();
    ImGui::Spacing();
    ImGui::BeginChild("TemplateDetails", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar);
    draw_template_details();
    ImGui::EndChild();
  } else if (ImGui::BeginTable("TemplateGrid", 2,
                               ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("TemplateList", ImGuiTableColumnFlags_WidthStretch,
                            0.42f);
    ImGui::TableSetupColumn("TemplateDetails",
                            ImGuiTableColumnFlags_WidthStretch, 0.58f);

    ImGui::TableNextColumn();
    ImGui::BeginChild("TemplateList", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar);
    draw_template_list();
    ImGui::EndChild();

    ImGui::TableNextColumn();
    ImGui::BeginChild("TemplateDetails", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar);
    draw_template_details();
    ImGui::EndChild();

    ImGui::EndTable();
  }

  ImGui::Spacing();

  // Use Template button - enabled and functional
  ImGui::PushStyleColor(ImGuiCol_Button,
                        ImVec4(kSpiritOrange.x * 0.6f, kSpiritOrange.y * 0.6f,
                               kSpiritOrange.z * 0.6f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSpiritOrange);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(kSpiritOrange.x * 1.2f, kSpiritOrange.y * 1.2f,
                               kSpiritOrange.z * 1.2f, 1.0f));

  if (ImGui::Button(
          absl::StrFormat("%s Use Template", ICON_MD_ROCKET_LAUNCH).c_str(),
          ImVec2(-1, 30))) {
    // Trigger template-based project creation
    if (new_project_with_template_callback_) {
      new_project_with_template_callback_(
          templates[selected_template_].template_id);
    } else if (new_project_callback_) {
      // Fallback to regular new project if template callback not set
      new_project_callback_();
    }
  }

  ImGui::PopStyleColor(3);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s Create new project with '%s' template\nThis will "
        "open a ROM and apply the template settings.",
        ICON_MD_INFO, templates[selected_template_].name);
  }

  ImGui::PopStyleVar();  // Alpha (entry animation)
}

void WelcomeScreen::DrawTipsSection() {
  // Entry animation for tips (section 6, appears last)
  float tips_progress = GetStaggeredEntryProgress(
      entry_time_, 6, kEntryAnimDuration, kEntryStaggerDelay);

  if (tips_progress < 0.001f) {
    return;  // Don't draw yet
  }

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tips_progress);

  // Static tip (or could rotate based on session start time rather than
  // animation)
  const char* tips[] = {
      "Open a ROM first, then save a copy before editing",
      "Projects track ROM versions, templates, and settings",
      "Use Project Management to swap ROMs and manage snapshots",
      "Press Ctrl+Shift+P for the command palette and F1 for help",
      "Shortcuts are configurable in Settings > Keyboard Shortcuts",
      "Project + settings data live under ~/.yaze (user profile on Windows)",
      "Use the panel browser to find any tool quickly"};
  int tip_index = 0;  // Show first tip, or could be random on screen open

  ImGui::Text(ICON_MD_LIGHTBULB);
  ImGui::SameLine();
  ImGui::TextColored(kTriforceGold, "Tip:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", tips[tip_index]);

  ImGui::SameLine(ImGui::GetWindowWidth() - 220);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
  if (ImGui::SmallButton(
          absl::StrFormat("%s Don't show again", ICON_MD_CLOSE).c_str())) {
    manually_closed_ = true;
  }
  ImGui::PopStyleColor();

  ImGui::PopStyleVar();  // Alpha (entry animation)
}

void WelcomeScreen::DrawWhatsNew() {
  // Entry animation for what's new (section 5)
  float whatsnew_progress = GetStaggeredEntryProgress(
      entry_time_, 5, kEntryAnimDuration, kEntryStaggerDelay);

  if (whatsnew_progress < 0.001f) {
    return;  // Don't draw yet
  }

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, whatsnew_progress);

  ImGui::TextColored(kHeartRed, ICON_MD_NEW_RELEASES " Release History");
  ImGui::Spacing();

  // Version badge (no animation)
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_VERIFIED " Current: v%s",
                     YAZE_VERSION_STRING);
  ImGui::Spacing();

  struct ReleaseHighlight {
    const char* icon;
    const char* text;
  };

  struct ReleaseEntry {
    const char* icon;
    const char* version;
    const char* title;
    const char* date;
    ImVec4 color;
    const ReleaseHighlight* highlights;
    int highlight_count;
  };

  const ReleaseHighlight highlights_054[] = {
      {ICON_MD_BUG_REPORT, "Mesen2 debug panel + socket controls"},
      {ICON_MD_SYNC, "Model registry + API refresh stability"},
      {ICON_MD_TERMINAL, "ROM/debug CLI workflows"},
  };
  const ReleaseHighlight highlights_053[] = {
      {ICON_MD_BUILD, "DMG validation + build polish"},
      {ICON_MD_PUBLIC, "WASM storage + service worker fixes"},
      {ICON_MD_TERMINAL, "Local model support (LM Studio)"},
  };
  const ReleaseHighlight highlights_052[] = {
      {ICON_MD_SHIELD, "AI runtime guard fixes"},
      {ICON_MD_BUILD, "Build presets stabilized"},
  };
  const ReleaseHighlight highlights_051[] = {
      {ICON_MD_PALETTE, "ImHex-style UI modernization"},
      {ICON_MD_TUNE, "Theme system + layout polish"},
      {ICON_MD_DASHBOARD, "Panel registry improvements"},
  };
  const ReleaseHighlight highlights_050[] = {
      {ICON_MD_TABLET, "Platform expansion + iOS scaffolding"},
      {ICON_MD_VISIBILITY, "Editor UX + stability"},
      {ICON_MD_PUBLIC, "WASM preview hardening"},
  };

  const ReleaseEntry releases[] = {
      {ICON_MD_BUG_REPORT, "0.5.4", "Stability + Mesen2 debugging",
       "Jan 25, 2026", kMasterSwordBlue, highlights_054,
       static_cast<int>(sizeof(highlights_054) / sizeof(highlights_054[0]))},
      {ICON_MD_BUILD, "0.5.3", "Build + WASM improvements", "Jan 20, 2026",
       kMasterSwordBlue, highlights_053,
       static_cast<int>(sizeof(highlights_053) / sizeof(highlights_053[0]))},
      {ICON_MD_TUNE, "0.5.2", "Runtime guards", "Jan 20, 2026", kSpiritOrange,
       highlights_052,
       static_cast<int>(sizeof(highlights_052) / sizeof(highlights_052[0]))},
      {ICON_MD_AUTO_AWESOME, "0.5.1", "UI polish + templates", "Jan 20, 2026",
       kTriforceGold, highlights_051,
       static_cast<int>(sizeof(highlights_051) / sizeof(highlights_051[0]))},
      {ICON_MD_ROCKET_LAUNCH, "0.5.0", "Platform expansion", "Jan 10, 2026",
       kHyruleGreen, highlights_050,
       static_cast<int>(sizeof(highlights_050) / sizeof(highlights_050[0]))},
  };

  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  for (int i = 0; i < static_cast<int>(sizeof(releases) / sizeof(releases[0]));
       ++i) {
    const auto& release = releases[i];
    ImGui::PushID(release.version);
    if (i > 0) {
      ImGui::Separator();
    }
    ImGui::TextColored(release.color, "%s v%s", release.icon, release.version);
    ImGui::SameLine();
    ImGui::TextColored(text_secondary, "%s", release.date);
    ImGui::TextColored(text_secondary, "%s", release.title);
    for (int j = 0; j < release.highlight_count; ++j) {
      ImGui::Bullet();
      ImGui::SameLine();
      ImGui::TextColored(release.color, "%s", release.highlights[j].icon);
      ImGui::SameLine();
      ImGui::TextColored(text_secondary, "%s", release.highlights[j].text);
    }
    ImGui::Spacing();
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(
      ImGuiCol_Button,
      ImVec4(kMasterSwordBlue.x * 0.6f, kMasterSwordBlue.y * 0.6f,
             kMasterSwordBlue.z * 0.6f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kMasterSwordBlue);
  if (ImGui::Button(
          absl::StrFormat("%s View Full Changelog", ICON_MD_OPEN_IN_NEW)
              .c_str(),
          ImVec2(-1, 0))) {
    // Open changelog or GitHub releases
  }
  ImGui::PopStyleColor(2);

  ImGui::PopStyleVar();  // Alpha (entry animation)
}

}  // namespace editor
}  // namespace yaze
