#include "app/service/widget_discovery_service.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace yaze {
namespace test {
namespace {

struct WindowEntry {
  int index = -1;
  bool visible = false;
};

}  // namespace

void WidgetDiscoveryService::CollectWidgets(
    ImGuiTestContext* ctx, const DiscoverWidgetsRequest& request,
    DiscoverWidgetsResponse* response) const {
  if (!response) {
    return;
  }

  response->clear_windows();
  response->set_total_widgets(0);
  response->set_generated_at_ms(absl::ToUnixMillis(absl::Now()));

  const auto& registry = gui::WidgetIdRegistry::Instance().GetAllWidgets();
  if (registry.empty()) {
    return;
  }

  const std::string window_filter_lower =
      absl::AsciiStrToLower(std::string(request.window_filter()));
  const std::string path_prefix_lower =
      absl::AsciiStrToLower(std::string(request.path_prefix()));
  const bool include_invisible = request.include_invisible();
  const bool include_disabled = request.include_disabled();

  std::map<std::string, WindowEntry> window_lookup;
  int total_widgets = 0;

  for (const auto& [path, info] : registry) {
    if (!MatchesType(info.type, request.type_filter())) {
      continue;
    }

    if (!MatchesPathPrefix(path, path_prefix_lower)) {
      continue;
    }

    const std::string window_name =
        info.window_name.empty() ? ExtractWindowName(path) : info.window_name;
    if (!MatchesWindow(window_name, window_filter_lower)) {
      continue;
    }

    const std::string label =
        info.label.empty() ? ExtractLabel(path) : info.label;

    bool widget_enabled = info.enabled;
    bool widget_visible = info.visible;

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
    bool has_item_info = false;
    ImGuiTestItemInfo item_info;
    if (ctx) {
      item_info = ctx->ItemInfo(label.c_str(), ImGuiTestOpFlags_NoError);
      if (item_info.ID != 0) {
        has_item_info = true;
        widget_visible = item_info.RectClipped.GetWidth() > 0.0f &&
                         item_info.RectClipped.GetHeight() > 0.0f;
        widget_enabled = (item_info.ItemFlags & ImGuiItemFlags_Disabled) == 0;
      }
    }
#else
    (void)ctx;
#endif

    auto [it, inserted] = window_lookup.emplace(window_name, WindowEntry{});
    WindowEntry& entry = it->second;
    if (inserted) {
      entry.visible = widget_visible;
    } else {
      entry.visible = entry.visible || widget_visible;
    }

    if (!include_invisible && !widget_visible) {
      continue;
    }
    if (!include_disabled && !widget_enabled) {
      continue;
    }

    if (entry.index == -1) {
      DiscoveredWindow* window_proto = response->add_windows();
      entry.index = response->windows_size() - 1;
      window_proto->set_name(window_name);
      window_proto->set_visible(entry.visible);
    }

    auto* window_proto = response->mutable_windows(entry.index);
    window_proto->set_visible(entry.visible);

    auto* widget_proto = window_proto->add_widgets();
    widget_proto->set_path(path);
    widget_proto->set_widget_key(path);
    widget_proto->set_legacy_path(path);
    widget_proto->set_label(label);
    widget_proto->set_type(info.type);
    widget_proto->set_suggested_action(SuggestedAction(info.type, label));
    widget_proto->set_visible(widget_visible);
    widget_proto->set_enabled(widget_enabled);
    widget_proto->set_widget_id(info.imgui_id);

    if (!info.description.empty()) {
      widget_proto->set_description(info.description);
    }

    if (info.bounds.valid) {
      WidgetBounds* bounds = widget_proto->mutable_bounds();
      bounds->set_min_x(info.bounds.min_x);
      bounds->set_min_y(info.bounds.min_y);
      bounds->set_max_x(info.bounds.max_x);
      bounds->set_max_y(info.bounds.max_y);
#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
    } else if (ctx && has_item_info) {
      WidgetBounds* bounds = widget_proto->mutable_bounds();
      bounds->set_min_x(item_info.RectFull.Min.x);
      bounds->set_min_y(item_info.RectFull.Min.y);
      bounds->set_max_x(item_info.RectFull.Max.x);
      bounds->set_max_y(item_info.RectFull.Max.y);
    } else {
      (void)ctx;
#else
    } else {
      (void)ctx;
#endif
    }

    widget_proto->set_last_seen_frame(info.last_seen_frame);
    int64_t last_seen_ms = 0;
    if (info.last_seen_time != absl::Time()) {
      last_seen_ms = absl::ToUnixMillis(info.last_seen_time);
    }
    widget_proto->set_last_seen_at_ms(last_seen_ms);
    widget_proto->set_stale(info.stale_frame_count > 0);

    ++total_widgets;
  }

  response->set_total_widgets(total_widgets);
}

bool WidgetDiscoveryService::MatchesWindow(
    absl::string_view window_name, absl::string_view filter_lower) const {
  if (filter_lower.empty()) {
    return true;
  }
  std::string name_lower = absl::AsciiStrToLower(std::string(window_name));
  return absl::StrContains(name_lower, filter_lower);
}

bool WidgetDiscoveryService::MatchesPathPrefix(
    absl::string_view path, absl::string_view prefix_lower) const {
  if (prefix_lower.empty()) {
    return true;
  }
  std::string path_lower = absl::AsciiStrToLower(std::string(path));
  return absl::StartsWith(path_lower, prefix_lower);
}

bool WidgetDiscoveryService::MatchesType(absl::string_view type,
                                         WidgetType filter) const {
  if (filter == WIDGET_TYPE_UNSPECIFIED || filter == WIDGET_TYPE_ALL) {
    return true;
  }

  std::string type_lower = absl::AsciiStrToLower(std::string(type));
  switch (filter) {
    case WIDGET_TYPE_BUTTON:
      return type_lower == "button" || type_lower == "menuitem";
    case WIDGET_TYPE_INPUT:
      return type_lower == "input" || type_lower == "textbox" ||
             type_lower == "inputtext";
    case WIDGET_TYPE_MENU:
      return type_lower == "menu" || type_lower == "menuitem";
    case WIDGET_TYPE_TAB:
      return type_lower == "tab";
    case WIDGET_TYPE_CHECKBOX:
      return type_lower == "checkbox";
    case WIDGET_TYPE_SLIDER:
      return type_lower == "slider" || type_lower == "drag";
    case WIDGET_TYPE_CANVAS:
      return type_lower == "canvas";
    case WIDGET_TYPE_SELECTABLE:
      return type_lower == "selectable";
    case WIDGET_TYPE_OTHER:
      return true;
    default:
      return true;
  }
}

std::string WidgetDiscoveryService::ExtractWindowName(
    absl::string_view path) const {
  size_t slash = path.find('/');
  if (slash == absl::string_view::npos) {
    return std::string(path);
  }
  return std::string(path.substr(0, slash));
}

std::string WidgetDiscoveryService::ExtractLabel(absl::string_view path) const {
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

std::string WidgetDiscoveryService::SuggestedAction(
    absl::string_view type, absl::string_view label) const {
  std::string type_lower = absl::AsciiStrToLower(std::string(type));
  if (type_lower == "button" || type_lower == "menuitem") {
    return absl::StrCat("Click button:", label);
  }
  if (type_lower == "checkbox") {
    return absl::StrCat("Toggle checkbox:", label);
  }
  if (type_lower == "slider" || type_lower == "drag") {
    return absl::StrCat("Adjust slider:", label);
  }
  if (type_lower == "input" || type_lower == "textbox" ||
      type_lower == "inputtext") {
    return absl::StrCat("Type text into:", label);
  }
  if (type_lower == "canvas") {
    return absl::StrCat("Interact with canvas:", label);
  }
  if (type_lower == "tab") {
    return absl::StrCat("Switch to tab:", label);
  }
  return absl::StrCat("Interact with:", label);
}

}  // namespace test
}  // namespace yaze
