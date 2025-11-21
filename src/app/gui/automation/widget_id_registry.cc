#include "app/gui/automation/widget_id_registry.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "imgui/imgui_internal.h"  // For ImGuiContext internals

namespace yaze {
namespace gui {

// Thread-local storage for ID stack
thread_local std::vector<std::string> WidgetIdScope::id_stack_;

WidgetIdScope::WidgetIdScope(const std::string& name) : name_(name) {
  // Only push ID if we're in an active ImGui frame with a valid window
  // This prevents crashes during editor initialization before ImGui begins its
  // frame
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx && ctx->CurrentWindow && !ctx->Windows.empty()) {
    ImGui::PushID(name.c_str());
    id_stack_.push_back(name);
  }
}

WidgetIdScope::~WidgetIdScope() {
  // Only pop if we successfully pushed
  if (!id_stack_.empty() && id_stack_.back() == name_) {
    ImGui::PopID();
    id_stack_.pop_back();
  }
}

std::string WidgetIdScope::GetFullPath() const {
  return absl::StrJoin(id_stack_, "/");
}

std::string WidgetIdScope::GetWidgetPath(const std::string& widget_type,
                                         const std::string& widget_name) const {
  std::string path = GetFullPath();
  if (!path.empty()) {
    path += "/";
  }
  return absl::StrCat(path, widget_type, ":", widget_name);
}

// WidgetIdRegistry implementation

WidgetIdRegistry& WidgetIdRegistry::Instance() {
  static WidgetIdRegistry instance;
  return instance;
}

void WidgetIdRegistry::BeginFrame() {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx) {
    current_frame_ = ctx->FrameCount;
  } else if (current_frame_ >= 0) {
    ++current_frame_;
  } else {
    current_frame_ = 0;
  }
  frame_time_ = absl::Now();
  for (auto& [_, info] : widgets_) {
    info.seen_in_current_frame = false;
  }
}

void WidgetIdRegistry::EndFrame() {
  for (auto& [_, info] : widgets_) {
    if (!info.seen_in_current_frame) {
      info.visible = false;
      info.enabled = false;
      info.bounds.valid = false;
      info.stale_frame_count += 1;
    } else {
      info.seen_in_current_frame = false;
      info.stale_frame_count = 0;
    }
  }
  TrimStaleEntries();
}

namespace {

std::string ExtractWindowFromPath(absl::string_view path) {
  size_t slash = path.find('/');
  if (slash == absl::string_view::npos) {
    return std::string(path);
  }
  return std::string(path.substr(0, slash));
}

std::string ExtractLabelFromPath(absl::string_view path) {
  size_t colon = path.rfind(':');
  if (colon == absl::string_view::npos) {
    size_t slash = path.rfind('/');
    if (slash == absl::string_view::npos) {
      return std::string(path);
    }
    return std::string(path.substr(slash + 1));
  }
  return std::string(path.substr(colon + 1));
}

std::string FormatTimestampUTC(const absl::Time& timestamp) {
  if (timestamp == absl::Time()) {
    return "";
  }

  std::chrono::system_clock::time_point chrono_time =
      absl::ToChronoTime(timestamp);
  std::time_t time_value = std::chrono::system_clock::to_time_t(chrono_time);

  std::tm tm_buffer;
#if defined(_WIN32)
  if (gmtime_s(&tm_buffer, &time_value) != 0) {
    return "";
  }
#else
  if (gmtime_r(&time_value, &tm_buffer) == nullptr) {
    return "";
  }
#endif

  char buffer[32];
  if (std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                    tm_buffer.tm_year + 1900, tm_buffer.tm_mon + 1,
                    tm_buffer.tm_mday, tm_buffer.tm_hour, tm_buffer.tm_min,
                    tm_buffer.tm_sec) <= 0) {
    return "";
  }

  return std::string(buffer);
}

WidgetIdRegistry::WidgetBounds BoundsFromImGui(const ImRect& rect) {
  WidgetIdRegistry::WidgetBounds bounds;
  bounds.min_x = rect.Min.x;
  bounds.min_y = rect.Min.y;
  bounds.max_x = rect.Max.x;
  bounds.max_y = rect.Max.y;
  bounds.valid = true;
  return bounds;
}

}  // namespace

void WidgetIdRegistry::RegisterWidget(const std::string& full_path,
                                      const std::string& type, ImGuiID imgui_id,
                                      const std::string& description,
                                      const WidgetMetadata& metadata) {
  WidgetInfo& info = widgets_[full_path];
  info.full_path = full_path;
  info.type = type;
  info.imgui_id = imgui_id;
  info.description = description;

  if (metadata.label.has_value()) {
    info.label = NormalizeLabel(*metadata.label);
  } else {
    info.label = NormalizeLabel(ExtractLabelFromPath(full_path));
  }
  if (info.label.empty()) {
    info.label = ExtractLabelFromPath(full_path);
  }

  if (metadata.window_name.has_value()) {
    info.window_name = NormalizeLabel(*metadata.window_name);
  } else {
    info.window_name = NormalizePathSegment(ExtractWindowFromPath(full_path));
  }
  if (info.window_name.empty()) {
    info.window_name = ExtractWindowFromPath(full_path);
  }

  ImGuiContext* ctx = ImGui::GetCurrentContext();
  absl::Time observed_at = absl::Now();

  if (ctx) {
    const ImGuiLastItemData& last = ctx->LastItemData;
    if (metadata.visible.has_value()) {
      info.visible = *metadata.visible;
    } else {
      info.visible = (last.StatusFlags & ImGuiItemStatusFlags_Visible) != 0;
    }

    if (metadata.enabled.has_value()) {
      info.enabled = *metadata.enabled;
    } else {
      info.enabled = (last.ItemFlags & ImGuiItemFlags_Disabled) == 0;
    }

    if (metadata.bounds.has_value()) {
      info.bounds = *metadata.bounds;
    } else {
      info.bounds = BoundsFromImGui(last.Rect);
    }

    info.last_seen_frame = ctx->FrameCount;
  } else {
    info.visible = metadata.visible.value_or(true);
    info.enabled = metadata.enabled.value_or(true);
    if (metadata.bounds.has_value()) {
      info.bounds = *metadata.bounds;
    } else {
      info.bounds.valid = false;
    }
    if (current_frame_ >= 0) {
      info.last_seen_frame = current_frame_;
    }
  }

  info.last_seen_time = observed_at;
  info.seen_in_current_frame = true;
  info.stale_frame_count = 0;
}

std::vector<std::string> WidgetIdRegistry::FindWidgets(
    const std::string& pattern) const {
  std::vector<std::string> matches;

  // Simple glob-style pattern matching
  // Supports: "*" (any), "?" (single char), exact matches
  for (const auto& [path, info] : widgets_) {
    bool match = false;

    if (pattern == "*") {
      match = true;
    } else if (pattern.find('*') != std::string::npos) {
      // Wildcard pattern - convert to simple substring match for now
      std::string search = pattern;
      search.erase(std::remove(search.begin(), search.end(), '*'),
                   search.end());
      if (!search.empty() && path.find(search) != std::string::npos) {
        match = true;
      }
    } else {
      // Exact match
      if (path == pattern) {
        match = true;
      }
    }

    if (match) {
      matches.push_back(path);
    }
  }

  // Sort for consistent ordering
  std::sort(matches.begin(), matches.end());
  return matches;
}

ImGuiID WidgetIdRegistry::GetWidgetId(const std::string& full_path) const {
  auto it = widgets_.find(full_path);
  if (it != widgets_.end()) {
    return it->second.imgui_id;
  }
  return 0;
}

const WidgetIdRegistry::WidgetInfo* WidgetIdRegistry::GetWidgetInfo(
    const std::string& full_path) const {
  auto it = widgets_.find(full_path);
  if (it != widgets_.end()) {
    return &it->second;
  }
  return nullptr;
}

void WidgetIdRegistry::Clear() {
  widgets_.clear();
  current_frame_ = -1;
}

std::string WidgetIdRegistry::ExportCatalog(const std::string& format) const {
  std::ostringstream ss;

  if (format == "json") {
    ss << "{\n";
    ss << "  \"widgets\": [\n";

    bool first = true;
    for (const auto& [path, info] : widgets_) {
      if (!first)
        ss << ",\n";
      first = false;

      ss << "    {\n";
      ss << absl::StrFormat("      \"path\": \"%s\",\n", path);
      ss << absl::StrFormat("      \"type\": \"%s\",\n", info.type);
      ss << absl::StrFormat("      \"imgui_id\": %u,\n", info.imgui_id);
      ss << absl::StrFormat("      \"label\": \"%s\",\n", info.label);
      ss << absl::StrFormat("      \"window\": \"%s\",\n", info.window_name);
      ss << absl::StrFormat("      \"visible\": %s,\n",
                            info.visible ? "true" : "false");
      ss << absl::StrFormat("      \"enabled\": %s,\n",
                            info.enabled ? "true" : "false");
      if (info.bounds.valid) {
        ss << absl::StrFormat(
            "      \"bounds\": {\"min\": [%0.1f, %0.1f], \"max\": [%0.1f, "
            "%0.1f]},\n",
            info.bounds.min_x, info.bounds.min_y, info.bounds.max_x,
            info.bounds.max_y);
      } else {
        ss << "      \"bounds\": null,\n";
      }
      ss << absl::StrFormat("      \"last_seen_frame\": %d,\n",
                            info.last_seen_frame);
      std::string iso_timestamp = FormatTimestampUTC(info.last_seen_time);
      ss << absl::StrFormat("      \"last_seen_at\": \"%s\",\n", iso_timestamp);
      ss << absl::StrFormat("      \"stale\": %s",
                            info.stale_frame_count > 0 ? "true" : "false");
      if (!info.description.empty()) {
        ss << ",\n";
        ss << absl::StrFormat("      \"description\": \"%s\"\n",
                              info.description);
      } else {
        ss << "\n";
      }
      ss << "    }";
    }

    ss << "\n  ]\n";
    ss << "}\n";
  } else {
    // YAML format (default)
    ss << "widgets:\n";

    for (const auto& [path, info] : widgets_) {
      ss << absl::StrFormat("  - path: \"%s\"\n", path);
      ss << absl::StrFormat("    type: %s\n", info.type);
      ss << absl::StrFormat("    imgui_id: %u\n", info.imgui_id);
      ss << absl::StrFormat("    label: \"%s\"\n", info.label);
      ss << absl::StrFormat("    window: \"%s\"\n", info.window_name);
      ss << absl::StrFormat("    visible: %s\n",
                            info.visible ? "true" : "false");
      ss << absl::StrFormat("    enabled: %s\n",
                            info.enabled ? "true" : "false");
      if (info.bounds.valid) {
        ss << "    bounds:\n";
        ss << absl::StrFormat("      min: [%0.1f, %0.1f]\n", info.bounds.min_x,
                              info.bounds.min_y);
        ss << absl::StrFormat("      max: [%0.1f, %0.1f]\n", info.bounds.max_x,
                              info.bounds.max_y);
      }
      ss << absl::StrFormat("    last_seen_frame: %d\n", info.last_seen_frame);
      std::string iso_timestamp = FormatTimestampUTC(info.last_seen_time);
      ss << absl::StrFormat("    last_seen_at: %s\n", iso_timestamp);
      ss << absl::StrFormat("    stale: %s\n",
                            info.stale_frame_count > 0 ? "true" : "false");

      // Parse hierarchical context from path
      std::vector<std::string> segments = absl::StrSplit(path, '/');
      if (!segments.empty()) {
        ss << "    context:\n";
        if (segments.size() > 0) {
          ss << absl::StrFormat("      editor: %s\n", segments[0]);
        }
        if (segments.size() > 1) {
          ss << absl::StrFormat("      tab: %s\n", segments[1]);
        }
        if (segments.size() > 2) {
          ss << absl::StrFormat("      section: %s\n", segments[2]);
        }
      }

      if (!info.description.empty()) {
        ss << absl::StrFormat("    description: %s\n", info.description);
      }

      // Add suggested actions based on widget type
      ss << "    actions: [";
      if (info.type == "button") {
        ss << "click";
      } else if (info.type == "input") {
        ss << "type, clear";
      } else if (info.type == "canvas") {
        ss << "click, drag, scroll";
      } else if (info.type == "checkbox") {
        ss << "toggle";
      } else if (info.type == "slider") {
        ss << "drag, set";
      } else {
        ss << "interact";
      }
      ss << "]\n";
    }
  }

  return ss.str();
}

void WidgetIdRegistry::ExportCatalogToFile(const std::string& output_file,
                                           const std::string& format) const {
  std::string content = ExportCatalog(format);
  std::ofstream file(output_file);
  if (file.is_open()) {
    file << content;
    file.close();
  }
}

std::string WidgetIdRegistry::NormalizeLabel(absl::string_view label) {
  size_t pos = label.find("##");
  if (pos != absl::string_view::npos) {
    label = label.substr(0, pos);
  }
  std::string sanitized = std::string(absl::StripAsciiWhitespace(label));
  return sanitized;
}

std::string WidgetIdRegistry::NormalizePathSegment(absl::string_view segment) {
  return NormalizeLabel(segment);
}

void WidgetIdRegistry::TrimStaleEntries() {
  auto it = widgets_.begin();
  while (it != widgets_.end()) {
    if (ShouldPrune(it->second)) {
      it = widgets_.erase(it);
    } else {
      ++it;
    }
  }
}

bool WidgetIdRegistry::ShouldPrune(const WidgetInfo& info) const {
  if (info.last_seen_frame < 0 || stale_frame_limit_ <= 0) {
    return false;
  }
  if (current_frame_ < 0) {
    return false;
  }
  return (current_frame_ - info.last_seen_frame) > stale_frame_limit_;
}

}  // namespace gui
}  // namespace yaze
