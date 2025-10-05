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

// Draw a pulsing triforce in the background
void DrawTriforceBackground(ImDrawList* draw_list, ImVec2 pos, float size, float alpha, float glow) {
  float height = size * 0.866f; // sqrt(3)/2 for equilateral triangle
  
  // Calculate triangle points
  auto triangle = [&](ImVec2 center, float s, ImU32 color) {
    ImVec2 p1(center.x, center.y - height * s / size);
    ImVec2 p2(center.x - s / 2, center.y + height * s / (2 * size));
    ImVec2 p3(center.x + s / 2, center.y + height * s / (2 * size));
    
    draw_list->AddTriangleFilled(p1, p2, p3, color);
    
    // Glow effect
    if (glow > 0.0f) {
      ImU32 glow_color = ImGui::GetColorU32(ImVec4(1.0f, 0.843f, 0.0f, glow * 0.3f));
      draw_list->AddTriangle(p1, p2, p3, glow_color, 2.0f + glow * 3.0f);
    }
  };
  
  ImU32 gold = ImGui::GetColorU32(ImVec4(1.0f, 0.843f, 0.0f, alpha));
  
  // Top triangle
  triangle(ImVec2(pos.x, pos.y), size, gold);
  
  // Bottom left triangle
  triangle(ImVec2(pos.x - size / 4, pos.y + height / 2), size / 2, gold);
  
  // Bottom right triangle
  triangle(ImVec2(pos.x + size / 4, pos.y + height / 2), size / 2, gold);
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
  bool action_taken = false;
  
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
                                   ImGuiWindowFlags_NoMove | 
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus;
  
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20));
  
  if (ImGui::Begin("##WelcomeScreen", p_open, window_flags)) {
    ImDrawList* bg_draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    
    // Dreamlike animated background with floating triforces
    for (int i = 0; i < 5; ++i) {
      float offset = animation_time_ * 0.5f + i * 2.0f;
      float x = window_pos.x + window_size.x * (0.2f + 0.15f * i + sin(offset) * 0.1f);
      float y = window_pos.y + window_size.y * (0.3f + cos(offset * 0.7f) * 0.3f);
      float size = 40.0f + sin(offset * 1.3f) * 20.0f;
      float alpha = 0.05f + sin(offset) * 0.05f;
      
      DrawTriforceBackground(bg_draw_list, ImVec2(x, y), size, alpha, 0.0f);
    }
    
    DrawHeader();
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Main content area with gradient separator
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 separator_start = ImGui::GetCursorScreenPos();
    ImVec2 separator_end(separator_start.x + ImGui::GetContentRegionAvail().x, separator_start.y + 2);
    draw_list->AddRectFilledMultiColor(
        separator_start, separator_end,
        ImGui::GetColorU32(kTriforceGold),
        ImGui::GetColorU32(kMasterSwordBlue),
        ImGui::GetColorU32(kMasterSwordBlue),
        ImGui::GetColorU32(kTriforceGold));
    
    ImGui::Dummy(ImVec2(0, 10));
    
    ImGui::BeginChild("WelcomeContent", ImVec2(0, -60), false);
    
    // Left side - Quick Actions & Templates
    ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0), true,
                     ImGuiWindowFlags_NoScrollbar);
    DrawQuickActions();
    ImGui::Spacing();
    
    // Animated separator
    ImVec2 sep_start = ImGui::GetCursorScreenPos();
    float pulse = sin(animation_time_ * 2.0f) * 0.5f + 0.5f;
    draw_list->AddLine(
        sep_start,
        ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
        ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.3f + pulse * 0.3f)),
        2.0f);
    
    ImGui::Dummy(ImVec2(0, 5));
    DrawTemplatesSection();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right side - Recent Projects & What's New
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    DrawRecentProjects();
    ImGui::Spacing();
    
    // Another animated separator
    sep_start = ImGui::GetCursorScreenPos();
    draw_list->AddLine(
        sep_start,
        ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
        ImGui::GetColorU32(ImVec4(kMasterSwordBlue.x, kMasterSwordBlue.y, kMasterSwordBlue.z, 0.3f + pulse * 0.3f)),
        2.0f);
    
    ImGui::Dummy(ImVec2(0, 5));
    DrawWhatsNew();
    ImGui::EndChild();
    
    ImGui::EndChild();
    
    // Footer with gradient
    ImVec2 footer_start = ImGui::GetCursorScreenPos();
    ImVec2 footer_end(footer_start.x + ImGui::GetContentRegionAvail().x, footer_start.y + 2);
    draw_list->AddRectFilledMultiColor(
        footer_start, footer_end,
        ImGui::GetColorU32(kHeartRed),
        ImGui::GetColorU32(kHyruleGreen),
        ImGui::GetColorU32(kHyruleGreen),
        ImGui::GetColorU32(kHeartRed));
    
    ImGui::Dummy(ImVec2(0, 5));
    DrawTipsSection();
  }
  ImGui::End();
  
  ImGui::PopStyleVar();
  
  return action_taken;
}

void WelcomeScreen::UpdateAnimations() {
  animation_time_ += ImGui::GetIO().DeltaTime;
  header_glow_ = sin(animation_time_ * 2.0f) * 0.5f + 0.5f;
  
  // Smooth card hover animations
  for (int i = 0; i < 6; ++i) {
    float target = (hovered_card_ == i) ? 1.05f : 1.0f;
    card_hover_scale_[i] += (target - card_hover_scale_[i]) * ImGui::GetIO().DeltaTime * 8.0f;
  }
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
  
  // Animated title with glow effect
  const char* title = ICON_MD_CASTLE " yaze";
  auto windowWidth = ImGui::GetWindowSize().x;
  auto textWidth = ImGui::CalcTextSize(title).x;
  float xPos = (windowWidth - textWidth) * 0.5f;
  
  ImGui::SetCursorPosX(xPos);
  ImVec2 text_pos = ImGui::GetCursorScreenPos();
  
  // Glow effect behind text
  float glow_size = 40.0f * header_glow_;
  ImU32 glow_color = ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.3f * header_glow_));
  draw_list->AddCircleFilled(
      ImVec2(text_pos.x + textWidth / 2, text_pos.y + 15),
      glow_size,
      glow_color,
      32);
  
  // Rainbow gradient on title
  ImVec4 color1 = kTriforceGold;
  ImVec4 color2 = kMasterSwordBlue;
  float t = sin(animation_time_ * 1.5f) * 0.5f + 0.5f;
  ImVec4 title_color(
      color1.x * (1 - t) + color2.x * t,
      color1.y * (1 - t) + color2.y * t,
      color1.z * (1 - t) + color2.z * t,
      1.0f);
  
  ImGui::TextColored(title_color, "%s", title);
  ImGui::PopFont();
  
  // Animated subtitle
  const char* subtitle = "Yet Another Zelda3 Editor";
  textWidth = ImGui::CalcTextSize(subtitle).x;
  ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
  
  float subtitle_alpha = 0.6f + sin(animation_time_ * 3.0f) * 0.2f;
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, subtitle_alpha), "%s", subtitle);
  
  // Draw small decorative triforces on either side of title
  ImVec2 left_tri_pos(xPos - 50, text_pos.y + 10);
  ImVec2 right_tri_pos(xPos + textWidth + 20, text_pos.y + 10);
  DrawTriforceBackground(draw_list, left_tri_pos, 30, 0.5f, header_glow_);
  DrawTriforceBackground(draw_list, right_tri_pos, 30, 0.5f, header_glow_);
  
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
    // Draw a cute "empty state" with animated icons
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    float pulse = sin(animation_time_ * 2.0f) * 0.3f + 0.7f;
    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(cursor.x + ImGui::GetContentRegionAvail().x * 0.3f);
    ImGui::TextColored(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, pulse),
                      ICON_MD_EXPLORE);
    ImGui::SetCursorPosX(cursor.x);
    
    ImGui::TextWrapped("No recent projects yet.\nOpen a ROM to begin your adventure!");
    ImGui::PopStyleColor();
    return;
  }
  
  // Grid layout for project cards
  float card_width = 220.0f;
  float card_height = 140.0f;
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
  
  ImVec2 card_size(220, 140);
  ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  
  // Apply hover scale
  float scale = card_hover_scale_[index];
  if (scale != 1.0f) {
    ImVec2 center(cursor_pos.x + card_size.x / 2, cursor_pos.y + card_size.y / 2);
    cursor_pos.x = center.x - (card_size.x * scale) / 2;
    cursor_pos.y = center.y - (card_size.y * scale) / 2;
    card_size.x *= scale;
    card_size.y *= scale;
  }
  
  // Draw card background with Zelda-themed gradient
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  
  // Gradient background
  ImU32 color_top = ImGui::GetColorU32(ImVec4(0.15f, 0.20f, 0.25f, 1.0f));
  ImU32 color_bottom = ImGui::GetColorU32(ImVec4(0.10f, 0.15f, 0.20f, 1.0f));
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
      color_top, color_top, color_bottom, color_bottom);
  
  // Border with animated color
  float border_t = (sin(animation_time_ + index) * 0.5f + 0.5f);
  ImVec4 border_color_base = (index % 3 == 0) ? kHyruleGreen : 
                             (index % 3 == 1) ? kMasterSwordBlue : kTriforceGold;
  ImU32 border_color = ImGui::GetColorU32(
      ImVec4(border_color_base.x, border_color_base.y, border_color_base.z, 0.4f + border_t * 0.3f));
  
  draw_list->AddRect(cursor_pos, 
                    ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
                    border_color, 6.0f, 0, 2.0f);
  
  // Make the card clickable
  ImGui::SetCursorScreenPos(cursor_pos);
  ImGui::InvisibleButton(absl::StrFormat("ProjectCard_%d", index).c_str(), card_size);
  bool is_hovered = ImGui::IsItemHovered();
  bool is_clicked = ImGui::IsItemClicked();
  
  hovered_card_ = is_hovered ? index : (hovered_card_ == index ? -1 : hovered_card_);
  
  // Hover glow effect
  if (is_hovered) {
    ImU32 hover_color = ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.2f));
    draw_list->AddRectFilled(cursor_pos, 
                            ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
                            hover_color, 6.0f);
                            
    // Draw small particles around the card
    for (int p = 0; p < 5; ++p) {
      float angle = animation_time_ * 2.0f + p * M_PI * 0.4f;
      float radius = 10.0f + sin(animation_time_ * 3.0f + p) * 5.0f;
      ImVec2 particle_pos(
          cursor_pos.x + card_size.x / 2 + cos(angle) * (card_size.x / 2 + radius),
          cursor_pos.y + card_size.y / 2 + sin(angle) * (card_size.y / 2 + radius));
      float particle_alpha = 0.3f + sin(animation_time_ * 4.0f + p) * 0.2f;
      draw_list->AddCircleFilled(particle_pos, 2.0f, 
          ImGui::GetColorU32(ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, particle_alpha)));
    }
  }
  
  // Draw content
  ImVec2 content_pos(cursor_pos.x + 15, cursor_pos.y + 15);
  
  // Icon with colored background circle
  ImVec2 icon_center(content_pos.x + 20, content_pos.y + 20);
  ImU32 icon_bg = ImGui::GetColorU32(border_color_base);
  draw_list->AddCircleFilled(icon_center, 25, icon_bg, 32);
  
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 5, content_pos.y + 8));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  ImGui::Text(ICON_MD_VIDEOGAME_ASSET);
  ImGui::PopStyleColor();
  
  // Project name
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 50, content_pos.y + 15));
  ImGui::PushTextWrapPos(cursor_pos.x + card_size.x - 15);
  ImGui::TextColored(kTriforceGold, "%s", project.name.c_str());
  ImGui::PopTextWrapPos();
  
  // ROM title
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 5, content_pos.y + 55));
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), ICON_MD_GAMEPAD " %s", project.rom_title.c_str());
  
  // Last modified with clock icon
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 5, content_pos.y + 80));
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
                    ICON_MD_ACCESS_TIME " %s", project.last_modified.c_str());
  
  // Path in card
  ImGui::SetCursorScreenPos(ImVec2(content_pos.x + 5, content_pos.y + 105));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
  std::string short_path = project.filepath;
  if (short_path.length() > 35) {
    short_path = "..." + short_path.substr(short_path.length() - 32);
  }
  ImGui::Text(ICON_MD_FOLDER " %s", short_path.c_str());
  ImGui::PopStyleColor();
  
  // Tooltip
  if (is_hovered) {
    ImGui::BeginTooltip();
    ImGui::TextColored(kMasterSwordBlue, ICON_MD_INFO " Project Details");
    ImGui::Separator();
    ImGui::Text("Name: %s", project.name.c_str());
    ImGui::Text("Path: %s", project.filepath.c_str());
    ImGui::Text("Modified: %s", project.last_modified.c_str());
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
    
    // Animated selection glow
    if (is_selected) {
      float glow = sin(animation_time_ * 3.0f) * 0.3f + 0.5f;
      ImGui::PushStyleColor(ImGuiCol_Header, 
          ImVec4(templates[i].color.x * glow, templates[i].color.y * glow, 
                 templates[i].color.z * glow, 0.8f));
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
  // Rotating tips
  const char* tips[] = {
    "Press Ctrl+P to open the command palette",
    "Use z3ed agent for AI-powered ROM editing",
    "Enable ZSCustomOverworld in Debug menu for expanded features",
    "Check the Performance Dashboard for optimization insights",
    "Collaborate in real-time with yaze-server"
  };
  int tip_index = ((int)(animation_time_ / 5.0f)) % 5;
  
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
  
  // Version badge
  float pulse = sin(animation_time_ * 2.0f) * 0.2f + 0.8f;
  ImGui::TextColored(ImVec4(kMasterSwordBlue.x * pulse, kMasterSwordBlue.y * pulse, 
                            kMasterSwordBlue.z * pulse, 1.0f), 
                    ICON_MD_VERIFIED " yaze v0.2.0-alpha");
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