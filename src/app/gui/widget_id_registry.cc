#include "widget_id_registry.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "imgui/imgui_internal.h"  // For ImGuiContext internals

namespace yaze {
namespace gui {

// Thread-local storage for ID stack
thread_local std::vector<std::string> WidgetIdScope::id_stack_;

WidgetIdScope::WidgetIdScope(const std::string& name) : name_(name) {
  // Only push ID if we're in an active ImGui frame with a valid window
  // This prevents crashes during editor initialization before ImGui begins its frame
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

void WidgetIdRegistry::RegisterWidget(const std::string& full_path,
                                      const std::string& type, ImGuiID imgui_id,
                                      const std::string& description) {
  WidgetInfo info{full_path, type, imgui_id, description};
  widgets_[full_path] = info;
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
      search.erase(std::remove(search.begin(), search.end(), '*'), search.end());
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

void WidgetIdRegistry::Clear() { widgets_.clear(); }

std::string WidgetIdRegistry::ExportCatalog(const std::string& format) const {
  std::ostringstream ss;

  if (format == "json") {
    ss << "{\n";
    ss << "  \"widgets\": [\n";

    bool first = true;
    for (const auto& [path, info] : widgets_) {
      if (!first) ss << ",\n";
      first = false;

      ss << "    {\n";
      ss << absl::StrFormat("      \"path\": \"%s\",\n", path);
      ss << absl::StrFormat("      \"type\": \"%s\",\n", info.type);
      ss << absl::StrFormat("      \"imgui_id\": %u", info.imgui_id);
      if (!info.description.empty()) {
        ss << ",\n";
        ss << absl::StrFormat("      \"description\": \"%s\"", info.description);
      }
      ss << "\n    }";
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

}  // namespace gui
}  // namespace yaze
