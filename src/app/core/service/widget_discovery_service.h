#ifndef YAZE_APP_CORE_SERVICE_WIDGET_DISCOVERY_SERVICE_H_
#define YAZE_APP_CORE_SERVICE_WIDGET_DISCOVERY_SERVICE_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#ifdef _WIN32
#pragma push_macro("DWORD")
#pragma push_macro("ERROR")
#undef DWORD
#undef ERROR
#endif  // _WIN32
#include "protos/imgui_test_harness.pb.h"
#ifdef _WIN32
#pragma pop_macro("DWORD")
#pragma pop_macro("ERROR")
#endif  // _WIN32
#include "app/gui/automation/widget_id_registry.h"

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE
#include "imgui_test_engine/imgui_te_context.h"
#else
struct ImGuiTestContext;
#endif

namespace yaze {
namespace test {

// Service responsible for transforming widget registry data into
// DiscoverWidgetsResponse payloads.
class WidgetDiscoveryService {
 public:
  WidgetDiscoveryService() = default;

  void CollectWidgets(ImGuiTestContext* ctx,
                      const DiscoverWidgetsRequest& request,
                      DiscoverWidgetsResponse* response) const;

 private:
  bool MatchesWindow(absl::string_view window_name,
                     absl::string_view filter) const;
  bool MatchesPathPrefix(absl::string_view path,
                         absl::string_view prefix) const;
  bool MatchesType(absl::string_view type,
                   WidgetType filter) const;

  std::string ExtractWindowName(absl::string_view path) const;
  std::string ExtractLabel(absl::string_view path) const;
  std::string SuggestedAction(absl::string_view type,
                              absl::string_view label) const;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_CORE_SERVICE_WIDGET_DISCOVERY_SERVICE_H_
