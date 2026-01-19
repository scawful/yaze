#include "app/editor/ui/about_panel.h"
#include "app/editor/core/content_registry.h"

namespace yaze::editor {

namespace {
// Auto-register AboutPanel using the ContentRegistry
// This demonstrates the Unified Panel Registry pattern (Proposal A)
struct AboutPanelRegistration {
  AboutPanelRegistration() {
    ContentRegistry::Panels::add<AboutPanel>();
  }
};
static AboutPanelRegistration s_registration;
}

} // namespace yaze::editor
