#ifndef YAZE_APP_EDITOR_SYSTEM_WORKSPACE_PANEL_ADMISSION_H_
#define YAZE_APP_EDITOR_SYSTEM_WORKSPACE_PANEL_ADMISSION_H_

#include <cstddef>
#include <string>
#include <vector>

namespace yaze {
namespace editor {

class WorkspaceWindowManager;

/**
 * @file panel_admission.h
 * @brief Admission rules for registered workspace windows.
 *
 * Registering a WindowContent does not make it a first-class window. Every
 * registered window must declare a WindowPresentationPolicy (see
 * editor_panel.h) so the workspace knows where users meet it:
 *
 * - A window may be kCoreWorkspace, or visible by default, only when its id is
 *   in FirstClassPanelAllowlist() below, with a reason a reviewer accepted.
 * - kEmbeddedTool and kDiagnostic windows must name their host surface with
 *   WindowPresentationPolicy::WithHost().
 * - A window that declares nothing is reported, even though it still behaves
 *   as a listed optional pop-out until it chooses.
 *
 * AuditPanelAdmission() applies these rules to the windows registered in one
 * session. test/unit/editor/panel_admission_test.cc runs it against a booted
 * EditorManager, and scripts/audit_panel_policies.py checks the source tree
 * for panel classes that never declare a policy.
 */

/// One reviewed exception: a window that may be CoreWorkspace or visible by
/// default. `reason` states why no existing surface can host it.
struct PanelAdmissionEntry {
  const char* window_id;
  const char* reason;
};

/// Windows admitted as first-class default surfaces. Keep this list short.
/// Adding an entry is a product decision: name the primary workflow, and the
/// reason an inspector, tool tab, or context-menu popup cannot host it.
const std::vector<PanelAdmissionEntry>& FirstClassPanelAllowlist();

/// Returns the allowlist entry for a base window id, or nullptr.
const PanelAdmissionEntry* FindFirstClassPanelEntry(
    const std::string& window_id);

/// Windows that have not declared a policy yet because their source directory
/// is frozen (its owner must approve the edit). Each entry names the blocker.
/// The audit tolerates an undeclared policy for these ids only; remove the
/// entry in the same change that declares the policy.
const std::vector<PanelAdmissionEntry>& DeferredPolicyDeclarations();
bool IsPolicyDeclarationDeferred(const std::string& window_id);

/// One audit failure. `fix` tells the panel author what to change.
struct PanelAdmissionFinding {
  std::string window_id;
  std::string category;
  std::string problem;
  std::string fix;

  std::string ToString() const;
};

/// Base ids of the windows in `session_id` that are open right after
/// registration or listed in a default layout preset (LayoutPresets).
std::vector<std::string> CollectDefaultVisibleWindowIds(
    const WorkspaceWindowManager& window_manager, size_t session_id);

/// Audits every window registered for `session_id`. Empty means admitted.
std::vector<PanelAdmissionFinding> AuditPanelAdmission(
    const WorkspaceWindowManager& window_manager, size_t session_id);

/// Joins findings into one message for test output.
std::string FormatPanelAdmissionFindings(
    const std::vector<PanelAdmissionFinding>& findings);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_WORKSPACE_PANEL_ADMISSION_H_
