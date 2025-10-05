#include "app/editor/ui/welcome_screen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/core/project.h"
#include "app/gui/icons.h"
#include "app/gui/theme_manager.h"
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
  
  // TODO: Fix this
  // Map color names to theme colors
  // if (strcmp(color_name, "triforce_gold") == 0) {
  //   return theme.accent.to_im_vec4();
  // } else if (strcmp(color_name, "hyrule_green") == 0) {
  //   return theme.success.to_im_vec4();
  // } else if (strcmp(color_name, "master_sword_blue") == 0) {
  //   return theme.info.to_im_vec4();
  // } else if (strcmp(color_name, "ganon_purple") == 0) {
  //   return theme.secondary.to_im_vec4();
  // } else if (strcmp(color_name, "heart_red") == 0) {
  //   return theme.error.to_im_vec4();
  // } else if (strcmp(color_name, "spirit_orange") == 0) {
  //   return theme.warning.to_im_vec4();
  // }
  
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

// Active colors (updated each frame from theme)
ImVec4 kTriforceGold = kTriforceGoldFallback;
ImVec4 kHyruleGreen = kHyruleGreenFallback;
ImVec4 kMasterSwordBlue = kMasterSwordBlueFallback;
ImVec4 kGanonPurple = kGanonPurpleFallback;
ImVec4 kHeartRed = kHeartRedFallback;
ImVec4 kSpiritOrange = kSpiritOrangeFallback;
ImVec4 kShadowPurple = kShadowPurpleFallback;

std::string GetRelativeTimeString(const std::filesystem::file_time_type& ftime) {
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
void DrawTriforceBackground(ImDrawList* draw_list, ImVec2 pos, float size, float alpha, float glow) {
  // Make it pixelated - round size to nearest 4 pixels
  size = std::round(size / 4.0f) * 4.0f;
  
  // Calculate triangle points with pixel-perfect positioning
  auto triangle = [&](ImVec2 center, float s, ImU32 color) {
    // Round to pixel boundaries for crisp edges
    float half_s = s / 2.0f;
    float tri_h = s * 0.866f;  // Height of equilateral triangle
    
    // Fixed: Proper equilateral triangle with apex at top
    ImVec2 p1(std::round(center.x), std::round(center.y - tri_h / 2.0f));  // Top apex
    ImVec2 p2(std::round(center.x - half_s), std::round(center.y + tri_h / 2.0f));  // Bottom left
    ImVec2 p3(std::round(center.x + half_s), std::round(center.y + tri_h / 2.0f));  // Bottom right
    
    draw_list->AddTriangleFilled(p1, p2, p3, color);
  };
  
  ImU32 gold = ImGui::GetColorU32(ImVec4(1.0f, 0.843f, 0.0f, alpha));
  
  // Proper triforce layout with three triangles
  float small_size = size / 2.0f;
  float small_height = small_size * 0.866f;
  
  // Top triangle (centered above)
  triangle(ImVec2(pos.x, pos.y), small_size, gold);
  
  // Bottom left triangle
  triangle(ImVec2(pos.x - small_size / 2.0f, pos.y + small_height), small_size, gold);
  
  // Bottom right triangle
  triangle(ImVec2(pos.x + small_size / 2.0f, pos.y + small_height), small_size, gold);
}

}  // namespace

WelcomeScreen::WelcomeScreen() {
  RefreshRecentProjects();
}

bool WelcomeScreen::Show(bool* p_open) {
  // Update theme colors each frame
  kTriforceGold = GetThemedColor("triforce_gold", kTriforceGoldFallback);
  kHyruleGreen = GetThemedColor("hyrule_green", kHyruleGreenFallback);
  kMasterSwordBlue = GetThemedColor("master_sword_blue", kMasterSwordBlueFallback);
  kGanonPurple = GetThemedColor("ganon_purple", kGanonPurpleFallback);
  kHeartRed = GetThemedColor("heart_red", kHeartRedFallback);
  kSpiritOrange = GetThemedColor("spirit_orange", kSpiritOrangeFallback);
  
  UpdateAnimations();
  
  // Get mouse position for interactive triforce movement
  ImVec2 mouse_pos = ImGui::GetMousePos();
  
  bool action_taken = false;
  
  // Center the window with responsive size (80% of viewport, max 1400x900)
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 center = viewport->GetCenter();
  ImVec2 viewport_size = viewport->Size;
  
  float width = std::min(viewport_size.x * 0.8f, 1400.0f);
  float height = std::min(viewport_size.y * 0.85f, 900.0f);
  
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | 
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove;
  
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
      {0.08f, 0.12f, 32.0f, 0.065f, 80.0f},   // Top left corner
      {0.92f, 0.15f, 30.0f, 0.055f, 75.0f},   // Top right corner
      {0.06f, 0.85f, 28.0f, 0.050f, 70.0f},   // Bottom left
      {0.94f, 0.82f, 30.0f, 0.058f, 75.0f},   // Bottom right
      {0.15f, 0.48f, 26.0f, 0.048f, 65.0f},   // Mid left
      {0.85f, 0.52f, 26.0f, 0.048f, 65.0f},   // Mid right
      {0.50f, 0.92f, 24.0f, 0.040f, 60.0f},   // Bottom center
      {0.28f, 0.22f, 22.0f, 0.038f, 55.0f},   // Upper mid-left
      {0.72f, 0.25f, 22.0f, 0.038f, 55.0f},   // Upper mid-right
      {0.50f, 0.08f, 28.0f, 0.052f, 70.0f},   // Top center
      {0.22f, 0.65f, 20.0f, 0.035f, 50.0f},   // Mid-lower left
      {0.78f, 0.68f, 20.0f, 0.035f, 50.0f},   // Mid-lower right
      {0.12f, 0.35f, 18.0f, 0.030f, 45.0f},   // Upper-mid left
      {0.88f, 0.38f, 18.0f, 0.030f, 45.0f},   // Upper-mid right
      {0.38f, 0.75f, 16.0f, 0.028f, 40.0f},   // Lower left-center
      {0.62f, 0.77f, 16.0f, 0.028f, 40.0f},   // Lower right-center
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
      
      // Add floating animation using sine waves with unique frequencies per triforce
      float time_offset = i * 0.5f;  // Offset each triforce's animation
      float float_speed_x = (0.6f + (i % 4) * 0.25f) * triforce_speed_multiplier_;  // Apply speed multiplier
      float float_speed_y = (0.5f + ((i + 1) % 4) * 0.2f) * triforce_speed_multiplier_;
      float float_amount_x = (35.0f + (i % 3) * 20.0f) * triforce_size_multiplier_;  // Apply size multiplier
      float float_amount_y = (40.0f + ((i + 1) % 3) * 25.0f) * triforce_size_multiplier_;
      
      // Create complex orbital/figure-8 motion with more variation
      float float_x = std::sin(animation_time_ * float_speed_x + time_offset) * float_amount_x;
      float float_y = std::cos(animation_time_ * float_speed_y + time_offset * 1.5f) * float_amount_y;
      
      // Add secondary wave for more complex movement
      float_x += std::cos(animation_time_ * float_speed_x * 0.7f + time_offset * 2.0f) * (float_amount_x * 0.3f);
      float_y += std::sin(animation_time_ * float_speed_y * 0.6f + time_offset * 1.8f) * (float_amount_y * 0.3f);
      
      // Calculate distance from mouse
      float dx = triforce_base_positions_[i].x - mouse_pos.x;
      float dy = triforce_base_positions_[i].y - mouse_pos.y;
      float dist = std::sqrt(dx * dx + dy * dy);
      
      // Calculate repulsion offset with stronger effect
      ImVec2 target_pos = triforce_base_positions_[i];
      float repel_radius = 200.0f;  // Larger radius for more visible interaction
      
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
        float repel_strength = (1.0f - normalized_dist * normalized_dist) * triforce_configs[i].repel_distance;
        
        target_pos.x += dir_x * repel_strength;
        target_pos.y += dir_y * repel_strength;
      }
      
      // Smooth interpolation to target position (faster response)
      float lerp_speed = 8.0f * ImGui::GetIO().DeltaTime;
      triforce_positions_[i].x += (target_pos.x - triforce_positions_[i].x) * lerp_speed;
      triforce_positions_[i].y += (target_pos.y - triforce_positions_[i].y) * lerp_speed;
      
      // Draw at current position with alpha multiplier
      float adjusted_alpha = triforce_configs[i].alpha * triforce_alpha_multiplier_;
      float adjusted_size = triforce_configs[i].size * triforce_size_multiplier_;
      DrawTriforceBackground(bg_draw_list, triforce_positions_[i], 
                           adjusted_size, adjusted_alpha, 0.0f);
    }
    
    DrawHeader();
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Main content area with subtle gradient separator
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 separator_start = ImGui::GetCursorScreenPos();
    ImVec2 separator_end(separator_start.x + ImGui::GetContentRegionAvail().x, separator_start.y + 1);
    ImVec4 gold_faded = kTriforceGold;
    gold_faded.w = 0.3f;
    ImVec4 blue_faded = kMasterSwordBlue;
    blue_faded.w = 0.3f;
    draw_list->AddRectFilledMultiColor(
        separator_start, separator_end,
        ImGui::GetColorU32(gold_faded),
        ImGui::GetColorU32(blue_faded),
        ImGui::GetColorU32(blue_faded),
        ImGui::GetColorU32(gold_faded));
    
    ImGui::Dummy(ImVec2(0, 10));
    
    ImGui::BeginChild("WelcomeContent", ImVec2(0, -60), false);
    
    // Left side - Quick Actions & Templates
    ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0), true,
                     ImGuiWindowFlags_NoScrollbar);
    DrawQuickActions();
    ImGui::Spacing();
    
    // Subtle separator
    ImVec2 sep_start = ImGui::GetCursorScreenPos();
    draw_list->AddLine(
        sep_start,
        ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
        ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.2f)),
        1.0f);
    
    ImGui::Dummy(ImVec2(0, 5));
    DrawTemplatesSection();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right side - Recent Projects & What's New
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    DrawRecentProjects();
    ImGui::Spacing();
    
    // Subtle separator
    sep_start = ImGui::GetCursorScreenPos();
    draw_list->AddLine(
        sep_start,
        ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
        ImGui::GetColorU32(ImVec4(kMasterSwordBlue.x, kMasterSwordBlue.y, kMasterSwordBlue.z, 0.2f)),
        1.0f);
    
    ImGui::Dummy(ImVec2(0, 5));
    DrawWhatsNew();
    ImGui::EndChild();
    
    ImGui::EndChild();
    
    // Footer with subtle gradient
    ImVec2 footer_start = ImGui::GetCursorScreenPos();
    ImVec2 footer_end(footer_start.x + ImGui::GetContentRegionAvail().x, footer_start.y + 1);
    ImVec4 red_faded = kHeartRed;
    red_faded.w = 0.3f;
    ImVec4 green_faded = kHyruleGreen;
    green_faded.w = 0.3f;
    draw_list->AddRectFilledMultiColor(
        footer_start, footer_end,
        ImGui::GetColorU32(red_faded),
        ImGui::GetColorU32(green_faded),
        ImGui::GetColorU32(green_faded),
        ImGui::GetColorU32(red_faded));
    
    ImGui::Dummy(ImVec2(0, 5));
    DrawTipsSection();
    
    // Triforce animation settings panel
    ImGui::SameLine(ImGui::GetWindowWidth() - 50);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 5);
    if (ImGui::SmallButton(show_triforce_settings_ ? ICON_MD_CLOSE : ICON_MD_TUNE)) {
      show_triforce_settings_ = !show_triforce_settings_;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Triforce Animation Settings");
    }
    
    if (show_triforce_settings_) {
      ImGui::Separator();
      ImGui::Spacing();
      ImGui::BeginChild("TriforceSettings", ImVec2(0, 120), true, ImGuiWindowFlags_NoScrollbar);
      {
        ImGui::TextColored(kTriforceGold, ICON_MD_AUTO_AWESOME " Triforce Animation");
        ImGui::Spacing();
        
        ImGui::Columns(2, nullptr, false);
        
        // Left column
        ImGui::Text(ICON_MD_OPACITY " Visibility");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##visibility", &triforce_alpha_multiplier_, 0.0f, 3.0f, "%.1fx");
        
        ImGui::Text(ICON_MD_SPEED " Speed");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##speed", &triforce_speed_multiplier_, 0.1f, 3.0f, "%.1fx");
        
        ImGui::NextColumn();
        
        // Right column
        ImGui::Text(ICON_MD_ASPECT_RATIO " Size");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##size", &triforce_size_multiplier_, 0.5f, 2.0f, "%.1fx");
        
        ImGui::Checkbox(ICON_MD_MOUSE " Mouse Interaction", &triforce_mouse_repel_enabled_);
        
        ImGui::Columns(1);
        
        ImGui::Spacing();
        if (ImGui::SmallButton(ICON_MD_REFRESH " Reset to Defaults")) {
          triforce_alpha_multiplier_ = 1.0f;
          triforce_speed_multiplier_ = 1.0f;
          triforce_size_multiplier_ = 1.0f;
          triforce_mouse_repel_enabled_ = true;
        }
      }
      ImGui::EndChild();
    }
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
    card_hover_scale_[i] += (target - card_hover_scale_[i]) * ImGui::GetIO().DeltaTime * 10.0f;
  }
  
  // Note: Triforce positions are updated in Show() based on mouse position
}

void WelcomeScreen::RefreshRecentProjects() {
  recent_projects_.clear();
  
  // Use the ProjectManager singleton to get recent files
  auto& recent_files = core::RecentFilesManager::GetInstance().GetRecentFiles();
  
  for (const auto& filepath : recent_files) {
    if (recent_projects_.size() >= kMaxRecentProjects) break;
    
    RecentProject project;
    project.filepath = filepath;
    
    // Extract filename
    std::filesystem::path path(filepath);
    project.name = path.filename().string();
    
    // Get file modification time if it exists
    if (std::filesystem::exists(path)) {
      auto ftime = std::filesystem::last_write_time(path);
      project.last_modified = GetRelativeTimeString(ftime);
      project.rom_title = "ALTTP ROM";
    } else {
      project.last_modified = "File not found";
      project.rom_title = "Missing";
    }
    
    recent_projects_.push_back(project);
  }
}

void WelcomeScreen::DrawHeader() {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]); // Large font
  
  // Simple centered title
  const char* title = ICON_MD_CASTLE " yaze";
  auto windowWidth = ImGui::GetWindowSize().x;
  auto textWidth = ImGui::CalcTextSize(title).x;
  float xPos = (windowWidth - textWidth) * 0.5f;
  
  ImGui::SetCursorPosX(xPos);
  ImVec2 text_pos = ImGui::GetCursorScreenPos();
  
  // Subtle static glow behind text
  float glow_size = 30.0f;
  ImU32 glow_color = ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f));
  draw_list->AddCircleFilled(
      ImVec2(text_pos.x + textWidth / 2, text_pos.y + 15),
      glow_size,
      glow_color,
      32);
  
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
  ImGui::Spacing();
  
  float button_width = ImGui::GetContentRegionAvail().x;
  
  // Animated button colors
  auto draw_action_button = [&](const char* icon, const char* text, 
                               const ImVec4& color, bool enabled,
                               std::function<void()> callback) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));
    
    if (!enabled) ImGui::BeginDisabled();
    
    bool clicked = ImGui::Button(absl::StrFormat("%s %s", icon, text).c_str(), 
                                 ImVec2(button_width, 45));
    
    if (!enabled) ImGui::EndDisabled();
    
    ImGui::PopStyleColor(3);
    
    if (clicked && enabled && callback) {
      callback();
    }
    
    return clicked;
  };
  
  // Open ROM button - Green like finding an item
  if (draw_action_button(ICON_MD_FOLDER_OPEN, "Open ROM", kHyruleGreen, true, open_rom_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(ICON_MD_INFO " Open an existing ALTTP ROM file");
  }
  
  ImGui::Spacing();
  
  // New Project button - Gold like getting a treasure
  if (draw_action_button(ICON_MD_ADD_CIRCLE, "New Project", kTriforceGold, true, new_project_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(ICON_MD_INFO " Create a new ROM hacking project");
  }
  
  ImGui::Spacing();
  
  // Clone Project button - Blue like water/ice dungeon
  draw_action_button(ICON_MD_CLOUD_DOWNLOAD, "Clone Project", kMasterSwordBlue, false, nullptr);
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(ICON_MD_CONSTRUCTION " Clone a project from git (Coming soon)");
  }
}

void WelcomeScreen::DrawRecentProjects() {
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_HISTORY " Recent Projects");
  ImGui::Spacing();
  
  if (recent_projects_.empty()) {
    // Simple empty state
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(cursor.x + ImGui::GetContentRegionAvail().x * 0.3f);
    ImGui::TextColored(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.8f),
                      ICON_MD_EXPLORE);
    ImGui::SetCursorPosX(cursor.x);
    
    ImGui::TextWrapped("No recent projects yet.\nOpen a ROM to begin your adventure!");
    ImGui::PopStyleColor();
    return;
  }
  
  // Grid layout for project cards
  float card_width = 260.0f;  // Increased from 220.0f
  float card_height = 120.0f;  // Reduced from 140.0f
  int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / (card_width + 15)));
  
  for (size_t i = 0; i < recent_projects_.size(); ++i) {
    if (i % columns != 0) {
      ImGui::SameLine();
    }
    DrawProjectCard(recent_projects_[i], i);
  }
}

void WelcomeScreen::DrawProjectCard(const RecentProject& project, int index) {
  ImGui::BeginGroup();
  
  ImVec2 card_size(260, 120);  // Reduced height from 140 to 120
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  
  // Subtle hover scale (only on actual hover, no animation)
  float scale = card_hover_scale_[index];
  if (scale != 1.0f) {
    ImVec2 center(cursor_pos.x + card_size.x / 2, cursor_pos.y + card_size.y / 2);
    cursor_pos.x = center.x - (card_size.x * scale) / 2;
    cursor_pos.y = center.y - (card_size.y * scale) / 2;
    card_size.x *= scale;
    card_size.y *= scale;
  }
  
  // Draw card background with subtle gradient
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  
  // Gradient background
  ImU32 color_top = ImGui::GetColorU32(ImVec4(0.15f, 0.20f, 0.25f, 1.0f));
  ImU32 color_bottom = ImGui::GetColorU32(ImVec4(0.10f, 0.15f, 0.20f, 1.0f));
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
      color_top, color_top, color_bottom, color_bottom);
  
  // Static themed border
  ImVec4 border_color_base = (index % 3 == 0) ? kHyruleGreen : 
                             (index % 3 == 1) ? kMasterSwordBlue : kTriforceGold;
  ImU32 border_color = ImGui::GetColorU32(
      ImVec4(border_color_base.x, border_color_base.y, border_color_base.z, 0.5f));
  
  draw_list->AddRect(cursor_pos, 
                    ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
                    border_color, 6.0f, 0, 2.0f);
  
  // Make the card clickable
  ImGui::SetCursorScreenPos(cursor_pos);
  ImGui::InvisibleButton(absl::StrFormat("ProjectCard_%d", index).c_str(), card_size);
  bool is_hovered = ImGui::IsItemHovered();
  bool is_clicked = ImGui::IsItemClicked();
  
  hovered_card_ = is_hovered ? index : (hovered_card_ == index ? -1 : hovered_card_);
  
  // Subtle hover glow (no particles)
  if (is_hovered) {
    ImU32 hover_color = ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.15f));
    draw_list->AddRectFilled(cursor_pos, 
                            ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
                            hover_color, 6.0f);
  }
  
  // Draw content
  ImVec2 content_pos(cursor_pos.x + 12, cursor_pos.y + 12);
  
  // Icon with colored background circle (smaller and centered)
  ImVec2 icon_center(content_pos.x + 16, content_pos.y + 16);
  ImU32 icon_bg = ImGui::GetColorU32(border_color_base);
  draw_list->AddCircleFilled(icon_center, 18, icon_bg, 32);  // Reduced from 25 to 18
  
  // Center the icon properly
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]); // Medium font for icon
  ImVec2 icon_size = ImGui::CalcTextSize(ICON_MD_VIDEOGAME_ASSET);
  ImGui::SetCursorScreenPos(ImVec2(icon_center.x - icon_size.x / 2, icon_center.y - icon_size.y / 2));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  ImGui::Text(ICON_MD_VIDEOGAME_ASSET);
  ImGui::PopStyleColor();
  ImGui::PopFont();
  
  // Project name
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 40, content_pos.y + 12));
  ImGui::PushTextWrapPos(cursor_pos.x + card_size.x - 12);
  ImGui::TextColored(kTriforceGold, "%s", project.name.c_str());
  ImGui::PopTextWrapPos();
  
  // ROM title
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 5, content_pos.y + 45));
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), ICON_MD_GAMEPAD " %s", project.rom_title.c_str());
  
  // Path in card (condensed)
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 5, content_pos.y + 70));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
  std::string short_path = project.filepath;
  if (short_path.length() > 38) {
    short_path = "..." + short_path.substr(short_path.length() - 35);
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
  ImGui::TextColored(kGanonPurple, ICON_MD_LAYERS " Templates");
  ImGui::Spacing();
  
  struct Template {
    const char* icon;
    const char* name;
    ImVec4 color;
  };
  
  Template templates[] = {
    {ICON_MD_COTTAGE, "Vanilla ALTTP", kHyruleGreen},
    {ICON_MD_MAP, "ZSCustomOverworld v3", kMasterSwordBlue},
    {ICON_MD_PUBLIC, "Parallel Worlds Base", kGanonPurple},
  };
  
  for (int i = 0; i < 3; ++i) {
    bool is_selected = (selected_template_ == i);
    
    // Subtle selection highlight (no animation)
    if (is_selected) {
      ImGui::PushStyleColor(ImGuiCol_Header, 
          ImVec4(templates[i].color.x * 0.6f, templates[i].color.y * 0.6f, 
                 templates[i].color.z * 0.6f, 0.6f));
    }
    
    if (ImGui::Selectable(absl::StrFormat("%s %s", templates[i].icon, templates[i].name).c_str(), 
                         is_selected)) {
      selected_template_ = i;
    }
    
    if (is_selected) {
      ImGui::PopStyleColor();
    }
    
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(ICON_MD_STAR " Start with a %s template", templates[i].name);
    }
  }
  
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kSpiritOrange.x * 0.6f, kSpiritOrange.y * 0.6f, kSpiritOrange.z * 0.6f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSpiritOrange);
  ImGui::BeginDisabled(true);
  ImGui::Button(absl::StrFormat("%s Use Template", ICON_MD_ROCKET_LAUNCH).c_str(), 
               ImVec2(-1, 35));
  ImGui::EndDisabled();
  ImGui::PopStyleColor(2);
}

void WelcomeScreen::DrawTipsSection() {
  // Static tip (or could rotate based on session start time rather than animation)
  const char* tips[] = {
    "Press Ctrl+Shift+P to open the command palette",
    "Use z3ed agent for AI-powered ROM editing (Ctrl+Shift+A)",
    "Enable ZSCustomOverworld in Debug menu for expanded features",
    "Check the Performance Dashboard for optimization insights",
    "Collaborate in real-time with yaze-server"
  };
  int tip_index = 0;  // Show first tip, or could be random on screen open
  
  ImGui::Text(ICON_MD_LIGHTBULB);
  ImGui::SameLine();
  ImGui::TextColored(kTriforceGold, "Tip:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", tips[tip_index]);
  
  ImGui::SameLine(ImGui::GetWindowWidth() - 220);
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
  if (ImGui::SmallButton(absl::StrFormat("%s Don't show again", ICON_MD_CLOSE).c_str())) {
    manually_closed_ = true;
  }
  ImGui::PopStyleColor();
}

void WelcomeScreen::DrawWhatsNew() {
  ImGui::TextColored(kHeartRed, ICON_MD_NEW_RELEASES " What's New");
  ImGui::Spacing();
  
  // Version badge (no animation)
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_VERIFIED "yaze v%s", YAZE_VERSION_STRING);
  ImGui::Spacing();
  
  // Feature list with icons and colors
  struct Feature {
    const char* icon;
    const char* title;
    const char* desc;
    ImVec4 color;
  };
  
  Feature features[] = {
    {ICON_MD_PSYCHOLOGY, "AI Agent Integration", 
     "Natural language ROM editing with z3ed agent", kGanonPurple},
    {ICON_MD_CLOUD_SYNC, "Collaboration Features", 
     "Real-time ROM collaboration via yaze-server", kMasterSwordBlue},
    {ICON_MD_HISTORY, "Version Management", 
     "ROM snapshots, rollback, corruption detection", kHyruleGreen},
    {ICON_MD_PALETTE, "Enhanced Palette Editor", 
     "Advanced color tools with ROM palette browser", kSpiritOrange},
    {ICON_MD_SPEED, "Performance Improvements", 
     "Faster dungeon loading with parallel processing", kTriforceGold},
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
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(kMasterSwordBlue.x * 0.6f, kMasterSwordBlue.y * 0.6f, kMasterSwordBlue.z * 0.6f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kMasterSwordBlue);
  if (ImGui::Button(absl::StrFormat("%s View Full Changelog", ICON_MD_OPEN_IN_NEW).c_str())) {
    // Open changelog or GitHub releases
  }
  ImGui::PopStyleColor(2);
}

}  // namespace editor
}  // namespace yaze