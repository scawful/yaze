#include "app/editor/ui/welcome_screen.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "app/platform/timing.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/file_util.h"
#include "util/rom_hash.h"

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
constexpr float kRecentCardBaseHeight = 112.0f;
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

std::string ToLowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string TrimAscii(const std::string& value) {
  const auto not_space = [](unsigned char c) {
    return !std::isspace(c);
  };
  auto begin = std::find_if(value.begin(), value.end(), not_space);
  if (begin == value.end()) {
    return "";
  }
  auto end = std::find_if(value.rbegin(), value.rend(), not_space).base();
  return std::string(begin, end);
}

std::string FormatFileSize(uintmax_t bytes) {
  static constexpr std::array<const char*, 4> kUnits = {"B", "KB", "MB", "GB"};
  double value = static_cast<double>(bytes);
  size_t unit = 0;
  while (value >= 1024.0 && unit + 1 < kUnits.size()) {
    value /= 1024.0;
    ++unit;
  }
  if (unit == 0) {
    return absl::StrFormat("%llu %s", static_cast<unsigned long long>(bytes),
                           kUnits[unit]);
  }
  return absl::StrFormat("%.1f %s", value, kUnits[unit]);
}

bool IsRomPath(const std::filesystem::path& path) {
  const std::string ext = ToLowerAscii(path.extension().string());
  return ext == ".sfc" || ext == ".smc";
}

bool IsProjectPath(const std::filesystem::path& path) {
  const std::string ext = ToLowerAscii(path.extension().string());
  return ext == ".yaze" || ext == ".yazeproj" || ext == ".zsproj";
}

std::string DecodeSnesRegion(uint8_t code) {
  switch (code) {
    case 0x00:
      return "Japan";
    case 0x01:
      return "USA";
    case 0x02:
      return "Europe";
    case 0x03:
      return "Sweden";
    case 0x06:
      return "France";
    case 0x07:
      return "Netherlands";
    case 0x08:
      return "Spain";
    case 0x09:
      return "Germany";
    case 0x0A:
      return "Italy";
    case 0x0B:
      return "China";
    case 0x0D:
      return "Korea";
    default:
      return "Unknown region";
  }
}

std::string DecodeSnesMapMode(uint8_t code) {
  switch (code & 0x3F) {
    case 0x20:
      return "LoROM";
    case 0x21:
      return "HiROM";
    case 0x22:
      return "ExLoROM";
    case 0x25:
      return "ExHiROM";
    case 0x30:
      return "Fast LoROM";
    case 0x31:
      return "Fast HiROM";
    default:
      return absl::StrFormat("Mode %02X", code);
  }
}

struct SnesHeaderMetadata {
  std::string title;
  std::string region;
  std::string map_mode;
  bool valid = false;
};

bool ReadFileBlock(std::ifstream* file, std::streamoff offset, char* out,
                   size_t size) {
  if (!file) {
    return false;
  }
  file->clear();
  file->seekg(offset, std::ios::beg);
  if (!file->good()) {
    return false;
  }
  file->read(out, static_cast<std::streamsize>(size));
  return file->good() && file->gcount() == static_cast<std::streamsize>(size);
}

bool LooksLikeSnesTitle(const std::string& title) {
  if (title.empty()) {
    return false;
  }
  int printable = 0;
  for (unsigned char c : title) {
    if (c >= 32 && c <= 126) {
      ++printable;
    }
  }
  return printable >= std::max(6, static_cast<int>(title.size()) / 2);
}

SnesHeaderMetadata ReadSnesHeaderMetadata(const std::filesystem::path& path) {
  std::error_code size_ec;
  const uintmax_t file_size = std::filesystem::file_size(path, size_ec);
  if (size_ec || file_size < 0x8020) {
    return {};
  }

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return {};
  }

  static constexpr std::array<std::streamoff, 2> kHeaderBases = {0x7FC0,
                                                                 0xFFC0};
  static constexpr std::array<std::streamoff, 2> kHeaderBiases = {0, 512};

  for (std::streamoff bias : kHeaderBiases) {
    for (std::streamoff base : kHeaderBases) {
      const std::streamoff offset = base + bias;
      if (static_cast<uintmax_t>(offset + 0x20) > file_size) {
        continue;
      }

      char header[0x20] = {};
      if (!ReadFileBlock(&input, offset, header, sizeof(header))) {
        continue;
      }

      std::string raw_title(header, header + 21);
      for (char& c : raw_title) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 32 || uc > 126) {
          c = ' ';
        }
      }
      const std::string title = TrimAscii(raw_title);
      if (!LooksLikeSnesTitle(title)) {
        continue;
      }

      const uint8_t map_mode_code = static_cast<uint8_t>(header[0x15]);
      const uint8_t region_code = static_cast<uint8_t>(header[0x19]);
      return {title, DecodeSnesRegion(region_code),
              DecodeSnesMapMode(map_mode_code), true};
    }
  }

  return {};
}

std::string ReadFileCrc32(const std::filesystem::path& path) {
  std::error_code size_ec;
  const uintmax_t file_size = std::filesystem::file_size(path, size_ec);
  if (size_ec || file_size == 0 ||
      file_size > static_cast<uintmax_t>(std::numeric_limits<size_t>::max())) {
    return "";
  }

  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return "";
  }

  std::vector<uint8_t> data(static_cast<size_t>(file_size));
  input.read(reinterpret_cast<char*>(data.data()),
             static_cast<std::streamsize>(data.size()));
  if (input.gcount() != static_cast<std::streamsize>(data.size())) {
    return "";
  }

  return absl::StrFormat("%08X",
                         util::CalculateCrc32(data.data(), data.size()));
}

std::string ParseConfigValue(const std::string& line) {
  if (line.empty()) {
    return "";
  }
  size_t sep = line.find('=');
  if (sep == std::string::npos) {
    sep = line.find(':');
  }
  if (sep == std::string::npos || sep + 1 >= line.size()) {
    return "";
  }
  std::string value = line.substr(sep + 1);
  const size_t comment_pos = value.find('#');
  if (comment_pos != std::string::npos) {
    value = value.substr(0, comment_pos);
  }
  value = TrimAscii(value);
  if (value.empty()) {
    return "";
  }
  if (!value.empty() && value.back() == ',') {
    value.pop_back();
  }
  value = TrimAscii(value);
  if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                            (value.front() == '\'' && value.back() == '\''))) {
    value = value.substr(1, value.size() - 2);
  }
  return TrimAscii(value);
}

std::string ExtractLinkedProjectRomName(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    return "";
  }

  static constexpr std::array<const char*, 3> kKeys = {"rom_filename",
                                                       "rom_file", "rom_path"};
  std::string line;
  while (std::getline(input, line)) {
    const std::string lowered = ToLowerAscii(line);
    for (const char* key : kKeys) {
      const size_t key_pos = lowered.find(key);
      if (key_pos == std::string::npos) {
        continue;
      }
      const std::string value = ParseConfigValue(line);
      if (!value.empty()) {
        return std::filesystem::path(value).filename().string();
      }
    }
  }
  return "";
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

void DrawThemeQuickSwitcher(const char* popup_id, const ImVec2& button_size) {
  auto& theme_mgr = gui::ThemeManager::Get();
  const std::string button_label = absl::StrFormat(
      "%s Theme: %s", ICON_MD_PALETTE, theme_mgr.GetCurrentThemeName());

  if (gui::ThemedButton(button_label.c_str(), button_size, "welcome_screen",
                        "theme_quick_switch")) {
    ImGui::OpenPopup(popup_id);
  }

  if (ImGui::BeginPopup(popup_id)) {
    auto themes = theme_mgr.GetAvailableThemes();
    std::sort(themes.begin(), themes.end());

    const bool classic_selected =
        theme_mgr.GetCurrentThemeName() == "Classic YAZE";
    if (ImGui::Selectable("Classic YAZE", classic_selected)) {
      if (theme_mgr.IsPreviewActive()) {
        theme_mgr.EndPreview();
      }
      theme_mgr.ApplyClassicYazeTheme();
    }
    ImGui::Separator();

    for (const auto& name : themes) {
      if (ImGui::Selectable(name.c_str(),
                            theme_mgr.GetCurrentThemeName() == name)) {
        if (theme_mgr.IsPreviewActive()) {
          theme_mgr.EndPreview();
        }
        theme_mgr.ApplyTheme(name);
      }
      if (ImGui::IsItemHovered() && (!theme_mgr.IsPreviewActive() ||
                                     theme_mgr.GetCurrentThemeName() != name)) {
        theme_mgr.StartPreview(name);
      }
    }

    ImGui::EndPopup();
  } else if (theme_mgr.IsPreviewActive()) {
    theme_mgr.EndPreview();
  }
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

  gui::StyleVarGuard window_padding_guard(ImGuiStyleVar_WindowPadding,
                                          ImVec2(20, 20));

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
    gold_faded.w = 0.18f;
    ImVec4 blue_faded = kMasterSwordBlue;
    blue_faded.w = 0.18f;
    draw_list->AddRectFilledMultiColor(
        separator_start, separator_end, ImGui::GetColorU32(gold_faded),
        ImGui::GetColorU32(blue_faded), ImGui::GetColorU32(blue_faded),
        ImGui::GetColorU32(gold_faded));

    ImGui::Dummy(ImVec2(0, 14));

    ImGui::BeginChild("WelcomeContent", ImVec2(0, -60), false);
    const float content_width = ImGui::GetContentRegionAvail().x;
    const float content_height = ImGui::GetContentRegionAvail().y;
    const bool narrow_layout = content_width < 900.0f;
    const float layout_scale = ImGui::GetFontSize() / 16.0f;

    if (narrow_layout) {
      const float quick_actions_h = std::clamp(
          content_height * 0.28f, 150.0f * layout_scale, 240.0f * layout_scale);
      const float release_h = std::clamp(
          content_height * 0.32f, 160.0f * layout_scale, 320.0f * layout_scale);

      ImGui::BeginChild("QuickActionsNarrow", ImVec2(0, quick_actions_h), true,
                        ImGuiWindowFlags_NoScrollbar);
      DrawQuickActions();
      ImGui::EndChild();

      ImGui::Spacing();

      ImGui::BeginChild("ReleaseHistoryNarrow", ImVec2(0, release_h), true);
      DrawWhatsNew();
      ImGui::EndChild();

      ImGui::Spacing();

      ImGui::BeginChild("RecentPanelNarrow", ImVec2(0, 0), true);
      DrawRecentProjects();
      ImGui::EndChild();
    } else {
      float left_width =
          std::clamp(ImGui::GetContentRegionAvail().x * 0.38f,
                     320.0f * layout_scale, 520.0f * layout_scale);
      ImGui::BeginChild("LeftPanel", ImVec2(left_width, 0), true,
                        ImGuiWindowFlags_NoScrollbar);
      const float left_height = ImGui::GetContentRegionAvail().y;
      const float quick_actions_h = std::clamp(
          left_height * 0.27f, 150.0f * layout_scale, 220.0f * layout_scale);

      ImGui::BeginChild("QuickActionsWide", ImVec2(0, quick_actions_h), false,
                        ImGuiWindowFlags_NoScrollbar);
      DrawQuickActions();
      ImGui::EndChild();

      ImGui::Spacing();
      ImVec2 sep_start = ImGui::GetCursorScreenPos();
      draw_list->AddLine(
          sep_start,
          ImVec2(sep_start.x + ImGui::GetContentRegionAvail().x, sep_start.y),
          ImGui::GetColorU32(ImVec4(kMasterSwordBlue.x, kMasterSwordBlue.y,
                                    kMasterSwordBlue.z, 0.2f)),
          1.0f);
      ImGui::Dummy(ImVec2(0, 5));

      ImGui::BeginChild("ReleaseHistoryWide", ImVec2(0, 0), true);
      DrawWhatsNew();
      ImGui::EndChild();
      ImGui::EndChild();

      ImGui::SameLine();

      ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
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

    ImGui::Dummy(ImVec2(0, 5));
    DrawTipsSection();
  }
  ImGui::End();

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
    if (recent_projects_.size() >= kMaxRecentProjects) {
      break;
    }

    std::filesystem::path path(filepath);

    RecentProject project;
    project.filepath = filepath;
    project.name = path.filename().string();
    if (project.name.empty()) {
      project.name = filepath;
    }
    project.item_type = "File";
    project.item_icon = ICON_MD_INSERT_DRIVE_FILE;
    project.rom_title = "Local file";
    project.metadata_summary = "";

    // Skip and mark for removal if file doesn't exist.
    //
    // IMPORTANT (iOS): `std::filesystem::exists(path)` may throw if the app
    // doesn't have permission to access the path (e.g. iCloud Drive open-in-
    // place URLs without an active security scope). Use the error_code
    // overload to avoid crashing at startup.
    std::error_code exists_ec;
    const bool exists = std::filesystem::exists(path, exists_ec);
    if (exists_ec) {
      // Keep the entry but mark it as unavailable; the user can re-open via the
      // iOS document picker to re-grant access.
      project.unavailable = true;
      project.last_modified = "Unavailable";
      project.item_type = "Unavailable";
      project.item_icon = ICON_MD_WARNING;
      project.rom_title = "Re-open required";
      project.metadata_summary = "Permission expired for this location";
      recent_projects_.push_back(project);
      continue;
    }
    if (!exists) {
      files_to_remove.push_back(filepath);
      continue;
    }

    // Get file modification time
    std::error_code time_ec;
    auto ftime = std::filesystem::last_write_time(path, time_ec);
    if (!time_ec) {
      project.last_modified = GetRelativeTimeString(ftime);
    } else {
      project.last_modified = "Unknown";
    }

    std::error_code size_ec;
    const uintmax_t size_bytes = std::filesystem::file_size(path, size_ec);
    const std::string size_text =
        size_ec ? "Unknown size" : FormatFileSize(size_bytes);

    if (IsRomPath(path)) {
      project.item_type = "ROM";
      project.item_icon = ICON_MD_MEMORY;

      const SnesHeaderMetadata metadata = ReadSnesHeaderMetadata(path);
      const std::string crc32 = ReadFileCrc32(path);
      const std::string crc32_summary =
          crc32.empty() ? "CRC unknown"
                        : absl::StrFormat("CRC %s", crc32.c_str());
      if (metadata.valid && !metadata.title.empty()) {
        project.rom_title = metadata.title;
        project.metadata_summary =
            absl::StrFormat("%s • %s • %s • %s", metadata.region.c_str(),
                            metadata.map_mode.c_str(), size_text.c_str(),
                            crc32_summary.c_str());
      } else {
        project.rom_title = "SNES ROM";
        project.metadata_summary = absl::StrFormat("%s • %s", size_text.c_str(),
                                                   crc32_summary.c_str());
      }
    } else if (IsProjectPath(path)) {
      project.item_type = "Project";
      project.item_icon = ICON_MD_FOLDER_SPECIAL;

      const std::string linked_rom = ExtractLinkedProjectRomName(path);
      if (!linked_rom.empty()) {
        project.rom_title = absl::StrFormat("ROM: %s", linked_rom.c_str());
      } else {
        project.rom_title = "Project metadata + settings";
      }
      project.metadata_summary = absl::StrFormat("%s • %s", size_text.c_str(),
                                                 project.last_modified.c_str());
    } else {
      project.item_type = "File";
      project.item_icon = ICON_MD_INSERT_DRIVE_FILE;
      project.rom_title = "Imported file";
      project.metadata_summary = absl::StrFormat("%s • %s", size_text.c_str(),
                                                 project.last_modified.c_str());
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
  const ImVec4 text_disabled = gui::GetTextDisabledVec4();

  const char* subtitle = "Yet Another Zelda3 Editor";
  const float subtitle_width = ImGui::CalcTextSize(subtitle).x;
  ImGui::SetCursorPosX((window_width - subtitle_width) * 0.5f);

  ImGui::TextColored(
      ImVec4(text_secondary.x, text_secondary.y, text_secondary.z,
             text_secondary.w * subtitle_alpha),
      "%s", subtitle);

  const std::string version_line =
      absl::StrFormat("Version %s", YAZE_VERSION_STRING);
  const float version_width = ImGui::CalcTextSize(version_line.c_str()).x;
  ImGui::SetCursorPosX((window_width - version_width) * 0.5f);
  ImGui::TextColored(ImVec4(text_disabled.x, text_disabled.y, text_disabled.z,
                            text_disabled.w * subtitle_alpha),
                     "%s", version_line.c_str());

  // Small decorative triforces flanking the title (static, transparent)
  // Positioned well away from text to avoid crowding
  float tri_alpha = 0.12f * header_alpha;
  ImVec2 left_tri_pos(xPos - 80, text_pos.y + 20);
  ImVec2 right_tri_pos(xPos + title_width + 50, text_pos.y + 20);
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

  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, actions_alpha);

  // Apply horizontal offset for slide effect
  float indent = std::max(0.0f, -actions_offset_x);
  if (indent > 0.0f) {
    ImGui::Indent(indent);
  }

  ImGui::TextColored(kSpiritOrange, ICON_MD_BOLT " Quick Actions");
  const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped(
        "Open a ROM or project, then create a project when you need metadata.");
  }
  size_t rom_count = 0;
  size_t project_count = 0;
  size_t unavailable_count = 0;
  for (const auto& recent : recent_projects_) {
    if (recent.unavailable) {
      ++unavailable_count;
      continue;
    }
    if (recent.item_type == "ROM") {
      ++rom_count;
    } else if (recent.item_type == "Project") {
      ++project_count;
    }
  }
  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::TextWrapped(
        "%zu recent entries • %zu ROMs • %zu projects%s",
        recent_projects_.size(), rom_count, project_count,
        unavailable_count > 0 ? " • some entries need re-open permission" : "");
  }
  ImGui::Spacing();

  const float scale = ImGui::GetFontSize() / 16.0f;
  const float button_height = std::max(38.0f, 40.0f * scale);
  const float action_width = ImGui::GetContentRegionAvail().x;
  float button_width = action_width;

  // Animated button colors (compact height)
  auto draw_action_button = [&](const char* icon, const char* text,
                                const ImVec4& color, bool enabled,
                                std::function<void()> callback) {
    gui::StyleColorGuard button_colors({
        {ImGuiCol_Button,
         ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, 0.8f)},
        {ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 1.0f)},
        {ImGuiCol_ButtonActive,
         ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f)},
    });

    if (!enabled)
      ImGui::BeginDisabled();

    bool clicked = ImGui::Button(absl::StrFormat("%s %s", icon, text).c_str(),
                                 ImVec2(button_width, button_height));

    if (!enabled)
      ImGui::EndDisabled();

    if (clicked && enabled && callback) {
      callback();
    }

    return clicked;
  };

  // Unified startup open path.
  if (draw_action_button(ICON_MD_FOLDER_OPEN, "Open ROM / Project",
                         kHyruleGreen, true, open_rom_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(ICON_MD_INFO
                      " Open .sfc/.smc ROMs and .yaze/.yazeproj project files");
  }

  ImGui::Spacing();

  const RecentProject* last_recent = nullptr;
  for (const auto& recent : recent_projects_) {
    if (!recent.unavailable) {
      last_recent = &recent;
      break;
    }
  }
  if (last_recent && open_project_callback_) {
    const std::string resume_label = absl::StrFormat(
        "Resume Last (%s)", last_recent->item_type.empty()
                                ? "File"
                                : last_recent->item_type.c_str());
    const std::string resume_path = last_recent->filepath;
    if (draw_action_button(ICON_MD_PLAY_ARROW, resume_label.c_str(),
                           kMasterSwordBlue, true, [this, resume_path]() {
                             if (open_project_callback_) {
                               open_project_callback_(resume_path);
                             }
                           })) {
      // Handled by callback
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s\n%s", last_recent->name.c_str(),
                        last_recent->filepath.c_str());
    }
    ImGui::Spacing();
  }

  // New Project button - Gold like getting a treasure
  if (draw_action_button(ICON_MD_ADD_CIRCLE, "New Project", kTriforceGold, true,
                         new_project_callback_)) {
    // Handled by callback
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        ICON_MD_INFO
        " Create a new project for metadata, labels, and workflow settings");
  }

  {
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Release highlights and migration notes are now in the panel below.");
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

  int rom_count = 0;
  int project_count = 0;
  for (const auto& item : recent_projects_) {
    if (item.item_type == "ROM") {
      ++rom_count;
    } else if (item.item_type == "Project") {
      ++project_count;
    }
  }

  ImGui::TextColored(kMasterSwordBlue,
                     ICON_MD_HISTORY " Recent ROMs & Projects");

  const float header_spacing = ImGui::GetStyle().ItemSpacing.x;
  const float manage_width = ImGui::CalcTextSize(" Manage").x +
                             ImGui::CalcTextSize(ICON_MD_FOLDER_SPECIAL).x +
                             ImGui::GetStyle().FramePadding.x * 2.0f;
  const float clear_width = ImGui::CalcTextSize(" Clear").x +
                            ImGui::CalcTextSize(ICON_MD_DELETE_SWEEP).x +
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

  {
    const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
    ImGui::Text("%d ROMs • %d projects", rom_count, project_count);
  }

  ImGui::Spacing();

  if (recent_projects_.empty()) {
    // Simple empty state
    const ImVec4 text_secondary = gui::GetTextSecondaryVec4();
    gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);

    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPosX(cursor.x + ImGui::GetContentRegionAvail().x * 0.3f);
    ImGui::TextColored(
        ImVec4(kTriforceGold.x, kTriforceGold.y, kTriforceGold.z, 0.8f),
        ICON_MD_EXPLORE);
    ImGui::SetCursorPosX(cursor.x);

    ImGui::TextWrapped("No recent files yet.\nOpen a ROM or project to begin.");
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

  // Subtle hover scale.
  float hover_scale = card_hover_scale_[index];
  if (hover_scale != 1.0f) {
    ImVec2 center(cursor_pos.x + resolved_card_size.x / 2,
                  cursor_pos.y + resolved_card_size.y / 2);
    cursor_pos.x = center.x - (resolved_card_size.x * hover_scale) / 2;
    cursor_pos.y = center.y - (resolved_card_size.y * hover_scale) / 2;
    resolved_card_size.x *= hover_scale;
    resolved_card_size.y *= hover_scale;
  }

  ImVec4 accent = kTriforceGold;
  if (project.unavailable) {
    accent = kHeartRed;
  } else if (project.item_type == "ROM") {
    accent = kHyruleGreen;
  } else if (project.item_type == "Project") {
    accent = kMasterSwordBlue;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec4 color_top = ImLerp(surface_variant, surface, 0.7f);
  ImVec4 color_bottom = ImLerp(surface_variant, surface, 0.3f);
  ImU32 color_top_u32 = ImGui::GetColorU32(color_top);
  ImU32 color_bottom_u32 = ImGui::GetColorU32(color_bottom);
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + resolved_card_size.x,
             cursor_pos.y + resolved_card_size.y),
      color_top_u32, color_top_u32, color_bottom_u32, color_bottom_u32);

  ImU32 border_color =
      ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.6f));

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

  if (is_hovered) {
    ImU32 hover_color =
        ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.16f));
    draw_list->AddRectFilled(cursor_pos,
                             ImVec2(cursor_pos.x + resolved_card_size.x,
                                    cursor_pos.y + resolved_card_size.y),
                             hover_color, 6.0f);
  }

  const float layout_scale = resolved_card_size.y / kRecentCardBaseHeight;
  const float padding = 10.0f * layout_scale;
  const float icon_radius = 14.0f * layout_scale;
  const float icon_spacing = 10.0f * layout_scale;
  const float line_spacing = 2.0f * layout_scale;

  const ImVec2 icon_center(cursor_pos.x + padding + icon_radius,
                           cursor_pos.y + padding + icon_radius);
  draw_list->AddCircleFilled(icon_center, icon_radius,
                             ImGui::GetColorU32(accent), 24);

  const char* item_icon = project.item_icon.empty() ? ICON_MD_INSERT_DRIVE_FILE
                                                    : project.item_icon.c_str();
  const ImVec2 icon_size = ImGui::CalcTextSize(item_icon);
  ImGui::SetCursorScreenPos(ImVec2(icon_center.x - icon_size.x * 0.5f,
                                   icon_center.y - icon_size.y * 0.5f));
  gui::ColoredText(item_icon, text_primary);

  const std::string badge_text =
      project.item_type.empty() ? "File" : project.item_type;
  const ImVec2 badge_text_size = ImGui::CalcTextSize(badge_text.c_str());
  const float badge_pad_x = 6.0f * layout_scale;
  const float badge_pad_y = 2.0f * layout_scale;
  const ImVec2 badge_min(cursor_pos.x + resolved_card_size.x - padding -
                             badge_text_size.x - (badge_pad_x * 2.0f),
                         cursor_pos.y + padding);
  const ImVec2 badge_max(
      badge_min.x + badge_text_size.x + (badge_pad_x * 2.0f),
      badge_min.y + badge_text_size.y + (badge_pad_y * 2.0f));
  draw_list->AddRectFilled(
      badge_min, badge_max,
      ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.24f)), 4.0f);
  draw_list->AddRect(
      badge_min, badge_max,
      ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.50f)), 4.0f);
  draw_list->AddText(
      ImVec2(badge_min.x + badge_pad_x, badge_min.y + badge_pad_y),
      ImGui::GetColorU32(text_primary), badge_text.c_str());

  const float content_x = icon_center.x + icon_radius + icon_spacing;
  const float content_right = badge_min.x - (6.0f * layout_scale);
  const float text_max_w = std::max(80.0f, content_right - content_x);

  auto ellipsize = [text_max_w](const std::string& text) {
    if (text.empty()) {
      return std::string();
    }
    if (ImGui::CalcTextSize(text.c_str()).x <= text_max_w) {
      return text;
    }
    std::string clipped = text;
    while (!clipped.empty() &&
           ImGui::CalcTextSize((clipped + "...").c_str()).x > text_max_w) {
      clipped.pop_back();
    }
    return clipped.empty() ? std::string("...") : clipped + "...";
  };

  float text_y = cursor_pos.y + padding;
  const std::string display_name = ellipsize(project.name);
  ImGui::SetCursorScreenPos(ImVec2(content_x, text_y));
  gui::ColoredText(display_name.c_str(), text_primary);

  text_y += ImGui::GetTextLineHeight() + line_spacing;
  ImGui::SetCursorScreenPos(ImVec2(content_x, text_y));
  gui::ColoredTextF(text_secondary, "%s", ellipsize(project.rom_title).c_str());

  const std::string summary = project.metadata_summary.empty()
                                  ? project.last_modified
                                  : project.metadata_summary;
  text_y += ImGui::GetTextLineHeight() + line_spacing;
  ImGui::SetCursorScreenPos(ImVec2(content_x, text_y));
  gui::ColoredTextF(text_secondary, "%s", ellipsize(summary).c_str());

  text_y += ImGui::GetTextLineHeight() + line_spacing;
  const std::string opened_line =
      project.last_modified.empty()
          ? ""
          : absl::StrFormat("Last opened: %s", project.last_modified.c_str());
  ImGui::SetCursorScreenPos(ImVec2(content_x, text_y));
  gui::ColoredTextF(text_disabled, "%s", ellipsize(opened_line).c_str());

  if (is_hovered) {
    ImGui::BeginTooltip();
    ImGui::TextColored(kMasterSwordBlue, ICON_MD_INFO " Recent Item");
    ImGui::Separator();
    ImGui::Text("Type: %s", badge_text.c_str());
    ImGui::Text("Name: %s", project.name.c_str());
    ImGui::Text("Details: %s", project.rom_title.c_str());
    if (!project.metadata_summary.empty()) {
      ImGui::Text("Metadata: %s", project.metadata_summary.c_str());
    }
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

  gui::StyleVarGuard alpha_guard(ImGuiStyleVar_Alpha, templates_progress);

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
    {
      gui::StyledChild visual_settings(
          "VisualSettingsCompact", ImVec2(0, 115),
          {.bg = ImVec4(0.18f, 0.15f, 0.22f, 0.4f)}, true,
          ImGuiWindowFlags_NoScrollbar);
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

      std::optional<gui::StyleColorGuard> header_guard;
      if (is_selected) {
        header_guard.emplace(std::initializer_list<gui::StyleColorGuard::Entry>{
            {ImGuiCol_Header,
             ImVec4(templates[i].color.x * 0.6f, templates[i].color.y * 0.6f,
                    templates[i].color.z * 0.6f, 0.6f)}});
      }

      ImGui::PushID(i);
      {
        gui::StyleColorGuard text_guard(ImGuiCol_Text, templates[i].color);
        if (ImGui::Selectable(
                absl::StrFormat("%s %s", templates[i].icon, templates[i].name)
                    .c_str(),
                is_selected)) {
          selected_template_ = i;
        }
      }
      ImGui::PopID();

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
    {
      gui::StyleColorGuard text_guard(ImGuiCol_Text, text_secondary);
      ImGui::TextWrapped("%s", active.description);
    }
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
  {
    gui::StyleColorGuard button_colors({
        {ImGuiCol_Button, ImVec4(kSpiritOrange.x * 0.6f, kSpiritOrange.y * 0.6f,
                                 kSpiritOrange.z * 0.6f, 0.8f)},
        {ImGuiCol_ButtonHovered, kSpiritOrange},
        {ImGuiCol_ButtonActive,
         ImVec4(kSpiritOrange.x * 1.2f, kSpiritOrange.y * 1.2f,
                kSpiritOrange.z * 1.2f, 1.0f)},
    });

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
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s Create new project with '%s' template\nThis will "
        "open a ROM and apply the template settings.",
        ICON_MD_INFO, templates[selected_template_].name);
  }
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

  ImGui::Text(ICON_MD_LIGHTBULB);
  ImGui::SameLine();
  ImGui::TextColored(kTriforceGold, "Tip:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", tips[tip_index]);

  ImGui::SameLine(ImGui::GetWindowWidth() - 220);
  {
    gui::StyleColorGuard button_guard(ImGuiCol_Button,
                                      ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    if (ImGui::SmallButton(
            absl::StrFormat("%s Don't show again", ICON_MD_CLOSE).c_str())) {
      manually_closed_ = true;
    }
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

  ImGui::TextColored(kHeartRed, ICON_MD_NEW_RELEASES " Release History");
  ImGui::Spacing();

  // Version badge (no animation)
  ImGui::TextColored(kMasterSwordBlue, ICON_MD_VERIFIED " Current: v%s",
                     YAZE_VERSION_STRING);
  ImGui::Spacing();
  DrawThemeQuickSwitcher("WelcomeThemeQuickSwitch", ImVec2(-1, 0));
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

  const ReleaseHighlight highlights_060[] = {
      {ICON_MD_PALETTE, "GUI modernization with unified themed widgets"},
      {ICON_MD_COLOR_LENS, "Semantic theming and smooth editor transitions"},
      {ICON_MD_GRID_VIEW, "Visual Object Tile Editor for dungeon rooms"},
      {ICON_MD_UNDO, "Unified cross-editor Undo/Redo system"},
  };
  const ReleaseHighlight highlights_056[] = {
      {ICON_MD_TRAM, "Minecart overlays and collision tile validation"},
      {ICON_MD_RULE, "Track audit tooling with filler/missing-start checks"},
      {ICON_MD_TUNE, "Object preview stability and layer-aware hover"},
  };
  const ReleaseHighlight highlights_055[] = {
      {ICON_MD_ACCOUNT_TREE, "EditorManager architecture refactor"},
      {ICON_MD_FACT_CHECK, "Expanded tests for editor and ASAR workflows"},
      {ICON_MD_BUILD, "Build cleanup with shared yaze_core_lib target"},
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
      {ICON_MD_AUTO_AWESOME, "0.6.0", "GUI Modernization + Tile Editor",
       "Feb 13, 2026", kTriforceGold, highlights_060,
       static_cast<int>(sizeof(highlights_060) / sizeof(highlights_060[0]))},
      {ICON_MD_TRAM, "0.5.6", "Minecart workflow + editor stability",
       "Feb 5, 2026", kSpiritOrange, highlights_056,
       static_cast<int>(sizeof(highlights_056) / sizeof(highlights_056[0]))},
      {ICON_MD_ACCOUNT_TREE, "0.5.5", "Editor architecture + testability",
       "Jan 28, 2026", kShadowPurple, highlights_055,
       static_cast<int>(sizeof(highlights_055) / sizeof(highlights_055[0]))},
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
  {
    gui::StyleColorGuard button_colors({
        {ImGuiCol_Button,
         ImVec4(kMasterSwordBlue.x * 0.6f, kMasterSwordBlue.y * 0.6f,
                kMasterSwordBlue.z * 0.6f, 0.8f)},
        {ImGuiCol_ButtonHovered, kMasterSwordBlue},
    });
    if (ImGui::Button(
            absl::StrFormat("%s View Full Changelog", ICON_MD_OPEN_IN_NEW)
                .c_str(),
            ImVec2(-1, 0))) {
      // Open changelog or GitHub releases
    }
  }
}

}  // namespace editor
}  // namespace yaze
