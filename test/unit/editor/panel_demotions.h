#ifndef YAZE_TEST_UNIT_EDITOR_PANEL_DEMOTIONS_H_
#define YAZE_TEST_UNIT_EDITOR_PANEL_DEMOTIONS_H_

#include "app/editor/system/workspace/editor_panel.h"

namespace yaze::editor::admission {

// Windows the panel admission policy moved out of the window browser. Every
// row pins three things together: the declared role and host, the persisted
// visibility that the layout-defaults revision closes (category is the
// window's category), and that the window stays openable from the command
// palette. Add a row in the same change that demotes a window.
struct DemotedWindowExpectation {
  const char* category;
  const char* window_id;
  WindowPresentationRole role;
  const char* host_contains;
};

inline constexpr DemotedWindowExpectation kDemotedWindows[] = {
    {"Agent", "agent.mesen_debug", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "agent.mesen_screenshot", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "agent.oracle_states", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "agent.feature_flags", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "agent.manifest", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "agent.sram_viewer", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "oracle.validation", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
    {"Agent", "oracle.menu_inspector", WindowPresentationRole::kDiagnostic,
     "Hack Workflows"},
};

}  // namespace yaze::editor::admission

#endif  // YAZE_TEST_UNIT_EDITOR_PANEL_DEMOTIONS_H_
