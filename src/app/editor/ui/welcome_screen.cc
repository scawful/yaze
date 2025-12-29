#include "app/editor/ui/welcome_screen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>

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

// Get Zelda-inspired colors from theme or use fallback
ImVec4 GetThemedColor(const char* color_name, const ImVec4& fallback) {
  auto& theme_mgr = gui::ThemeManager::Get();
  const auto& theme = theme_mgr.GetCurrentTheme();

  if (!color_name) {
    return fallback;
  }

  const std::string_view name(color_name);
  if (name == "triforce_gold") {
    return gui::ConvertColorToImVec4(theme.accent);
  }
  if (name == "hyrule_green") {
    return gui::ConvertColorToImVec4(theme.success);
  }
  if (name == "master_sword_blue") {
    return gui::ConvertColorToImVec4(theme.info);
  }
  if (name == "ganon_purple") {
    return gui::ConvertColorToImVec4(theme.secondary);
  }
  if (name == "heart_red") {
    return gui::ConvertColorToImVec4(theme.error);
  }
  if (name == "spirit_orange") {
    return gui::ConvertColorToImVec4(theme.warning);
  }
  if (name == "shadow_purple") {
    return ImLerp(gui::ConvertColorToImVec4(theme.secondary),
                  gui::GetSurfaceVec4(), 0.4f);
  }

  return fallback;
}

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

  layout.columns =
      std::max(1, static_cast<int>((avail_width + spacing) /
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

bool WelcomeScreen::Show(bool* p_open) {
  // Update theme colors each frame
  kTriforceGold = GetThemedColor("triforce_gold", kTriforceGoldFallback);
  kHyruleGreen = GetThemedColor("hyrule_green", kHyruleGreenFallback);
  kMasterSwordBlue =
      GetThemedColor("master_sword_blue", kMasterSwordBlueFallback);
  kGanonPurple = GetThemedColor("ganon_purple", kGanonPurpleFallback);
  kHeartRed = GetThemedColor("heart_red", kHeartRedFallback);
  kSpiritOrange = GetThemedColor("spirit_orange", kSpiritOrangeFallback);

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
  float dockspace_center_x = dockspace_x + dockspace_width / 2.0f;
  float dockspace_center_y = viewport->WorkPos.y + viewport_size.y / 2.0f;
  ImVec2 center(dockspace_center_x, dockspace_center_y);

  // Size based on dockspace region, not full viewport
  float width = std::min(dockspace_width * 0.85f, 1400.0f);
  float height = std::min(viewport_size.y * 0.85f, 900.0f);

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
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoTitleBar;

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
          ImU32 particle_color =
              ImGui::GetColorU32(ImVec4(1.0f, 0.843f, 0.0f, alpha));
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

    if (narrow_layout) {
      float left_height = std::clamp(content_height * 0.45f, 260.0f, content_height);
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
      ImGui::BeginChild("LeftPanel",
                        ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0), true,
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

  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);  // Large font

  // Simple centered title
  const char* title = ICON_MD_CASTLE " yaze";
  auto windowWidth = ImGui::GetWindowSize().x;
  auto textWidth = ImGui::CalcTextSize(title).x;
  float xPos = (windowWidth - textWidth) * 0.5f;

  ImGui::SetCursorPosX(xPos);
  ImVec2 text_pos = ImGui::GetCursorScreenPos();

  // Subtle static glow behind text
  float glow_size = 30.0f;
  ImU32 glow_color = ImGui::GetColorU32(
      ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f));
  draw_list->AddCircleFilled(
      ImVec2(text_pos.x + textWidth / 2, text_pos.y + 15), glow_size,
      glow_color, 32);

  // Simple gold color for title
  ImGui::TextColored(kTriforceGold, "%s", title);
  ImGui::PopFont();

  // Static subtitle
  const char* subtitle = "Yet Another Zelda3 Editor";
  textWidth = ImGui::CalcTextSize(subtitle).x;
  ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);

  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", subtitle);

  // Small decorative triforces flanking the title (static, transparent)
  // Positioned well away from text to avoid crowding
  ImVec2 left_tri_pos(xPos - 80, text_pos.y + 20);
  ImVec2 right_tri_pos(xPos + textWidth + 50, text_pos.y + 20);
  DrawTriforceBackground(draw_list, left_tri_pos, 20, 0.12f, 0.0f);
  DrawTriforceBackground(draw_list, right_tri_pos, 20, 0.12f, 0.0f);

  ImGui::Spacing();
}

void WelcomeScreen::DrawQuickActions() {
  ImGui::TextColored(kSpiritOrange, ICON_MD_BOLT " Quick Actions");
  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  ImGui::PushStyleColor(ImGuiCol_Text, text_secondary);
  ImGui::TextWrapped(
      "New here? Start with Open ROM or New Project. Use a clean, legally "
      "obtained ALttP (USA) ROM to get going.");
  ImGui::PopStyleColor();
  ImGui::Spacing();

  float button_width = ImGui::GetContentRegionAvail().x;

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

    bool clicked =
        ImGui::Button(absl::StrFormat("%s %s", icon, text).c_str(),
                      ImVec2(button_width, 38));  // Reduced from 45 to 38

    if (!enabled)
      ImGui::EndDisabled();

    ImGui::PopStyleColor(3);

    if (clicked && enabled && callback) {
      callback();
    }

    return clicked;
  };

  // Open ROM button - Green like finding an item
  if (draw_action_button(ICON_MD_FOLDER_OPEN, "Open ROM", kHyruleGreen, true,
                         open_rom_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        ICON_MD_INFO " Open a clean, legally obtained ALttP (USA) ROM file");
  }

  ImGui::Spacing();

  // New Project button - Gold like getting a treasure
  if (draw_action_button(ICON_MD_ADD_CIRCLE, "New Project", kTriforceGold, true,
                         new_project_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(ICON_MD_INFO " Create a new project from a ROM and template");
  }

  ImGui::Spacing();

  // AI Agent button - Purple like magic
  if (draw_action_button(ICON_MD_SMART_TOY, "AI Agent", kGanonPurple, true,
                         open_agent_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        ICON_MD_INFO " Ask the AI agent to guide edits in natural language");
  }
}

void WelcomeScreen::DrawRecentProjects() {
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_HISTORY " Recent Projects");
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
    return;
  }

  const float scale = ImGui::GetFontSize() / 16.0f;
  const float min_width = kRecentCardBaseWidth * scale;
  const float max_width = kRecentCardBaseWidth * kRecentCardWidthMaxFactor * scale;
  const float min_height = kRecentCardBaseHeight * scale;
  const float max_height = kRecentCardBaseHeight * kRecentCardHeightMaxFactor * scale;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float aspect_ratio = min_height / std::max(min_width, 1.0f);

  GridLayout layout = ComputeGridLayout(ImGui::GetContentRegionAvail().x,
                                        min_width, max_width, min_height,
                                        max_height, min_width, aspect_ratio,
                                        spacing);

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

  if (column != 0) {
    ImGui::NewLine();
  }
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
      color_top_u32,
      color_top_u32, color_bottom_u32, color_bottom_u32);

  // Static themed border
  ImVec4 border_color_base = (index % 3 == 0)   ? kHyruleGreen
                             : (index % 3 == 1) ? kMasterSwordBlue
                                                : kTriforceGold;
  ImU32 border_color = ImGui::GetColorU32(ImVec4(
      border_color_base.x, border_color_base.y, border_color_base.z, 0.5f));

  draw_list->AddRect(
      cursor_pos,
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

  // Subtle hover glow (no particles)
  if (is_hovered) {
    ImU32 hover_color = ImGui::GetColorU32(
        ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f));
    draw_list->AddRectFilled(
        cursor_pos,
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
    ImVec4 color;
  };

  Template templates[] = {
      {ICON_MD_COTTAGE, "Vanilla ROM Hack",
       "Standard editing without custom ASM", "Vanilla ROM Hack", kHyruleGreen},
      {ICON_MD_MAP, "ZSCustomOverworld v3",
       "Full overworld expansion features", "ZSCustomOverworld v3 (Recommended)",
       kMasterSwordBlue},
      {ICON_MD_LAYERS, "ZSCustomOverworld v2",
       "Basic overworld expansion", "ZSCustomOverworld v2", kShadowPurple},
      {ICON_MD_SHUFFLE, "Randomizer Compatible",
       "Minimal custom features for rando", "Randomizer Compatible",
       kSpiritOrange},
  };

  for (int i = 0; i < 4; ++i) {
    bool is_selected = (selected_template_ == i);

    // Subtle selection highlight (no animation)
    if (is_selected) {
      ImGui::PushStyleColor(
          ImGuiCol_Header,
          ImVec4(templates[i].color.x * 0.6f, templates[i].color.y * 0.6f,
                 templates[i].color.z * 0.6f, 0.6f));
    }

    if (ImGui::Selectable(
            absl::StrFormat("%s %s", templates[i].icon, templates[i].name)
                .c_str(),
            is_selected)) {
      selected_template_ = i;
    }

    if (is_selected) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s %s\n%s", ICON_MD_INFO, templates[i].name,
                        templates[i].description);
    }
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
      new_project_with_template_callback_(templates[selected_template_].template_id);
    } else if (new_project_callback_) {
      // Fallback to regular new project if template callback not set
      new_project_callback_();
    }
  }

  ImGui::PopStyleColor(3);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s Create new project with '%s' template\nThis will "
                      "open a ROM and apply the template settings.",
                      ICON_MD_INFO, templates[selected_template_].name);
  }
}

void WelcomeScreen::DrawTipsSection() {
  // Static tip (or could rotate based on session start time rather than
  // animation)
  const char* tips[] = {
      "New here? Open a ROM first, then save a copy before editing",
      "Press Ctrl+Shift+P to open the command palette",
      "Use z3ed agent for AI-powered ROM editing (Ctrl+Shift+A)",
      "Enable ZSCustomOverworld in Debug menu for expanded features",
      "Check the Performance Dashboard for optimization insights",
      "Collaborate in real-time with yaze-server"};
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
}

void WelcomeScreen::DrawWhatsNew() {
  ImGui::TextColored(kHeartRed, ICON_MD_NEW_RELEASES " What's New");
  ImGui::Spacing();

  // Version badge (no animation)
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_VERIFIED "yaze v%s",
                     YAZE_VERSION_STRING);
  ImGui::Spacing();

  // Feature list with icons and colors
  struct Feature {
    const char* icon;
    const char* title;
    const char* desc;
    ImVec4 color;
  };

  Feature features[] = {
      {ICON_MD_MUSIC_NOTE, "Music Editor",
       "Complete SPC music editing with piano roll and tracker views", kTriforceGold},
      {ICON_MD_PIANO, "Piano Roll & Playback",
       "Visual note editing with authentic N-SPC audio preview", kMasterSwordBlue},
      {ICON_MD_SPEAKER, "Instrument Editor",
       "Edit ADSR envelopes, samples, and instrument banks", kHyruleGreen},
      {ICON_MD_PSYCHOLOGY, "AI Agent Integration",
       "Natural language ROM editing with z3ed agent", kGanonPurple},
      {ICON_MD_SPEED, "Performance Improvements",
       "Improved graphics arena and faster loading", kSpiritOrange},
  };

  for (const auto& feature : features) {
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextColored(feature.color, "%s ", feature.icon);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", feature.title);

    ImGui::Indent(25);
    ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "%s", feature.desc);
    ImGui::Unindent(25);
    ImGui::Spacing();
  }

  ImGui::Spacing();
  ImGui::PushStyleColor(
      ImGuiCol_Button,
      ImVec4(kMasterSwordBlue.x * 0.6f, kMasterSwordBlue.y * 0.6f,
             kMasterSwordBlue.z * 0.6f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kMasterSwordBlue);
  if (ImGui::Button(
          absl::StrFormat("%s View Full Changelog", ICON_MD_OPEN_IN_NEW)
              .c_str())) {
    // Open changelog or GitHub releases
  }
  ImGui::PopStyleColor(2);
}

}  // namespace editor
}  // namespace yaze
