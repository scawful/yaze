#include "app/editor/system/workspace/panel_admission.h"

#include <algorithm>
#include <map>
#include <set>

#include "app/editor/editor.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/editor/system/workspace/workspace_window_manager.h"

namespace yaze {
namespace editor {

namespace {

// Keep entries grouped by editor, in the order of the editor's default preset.
// A reason names the workflow the window serves and why no existing surface
// hosts it. "Current default" entries record windows that were already visible
// on first open when the admission policy landed; the audit document
// (docs/internal/plans/panel-admission-audit-2026-09-29.md) tracks which of
// them are candidates for demotion.
const std::vector<PanelAdmissionEntry> kFirstClassAllowlist = {
    // Overworld
    {"overworld.canvas", "Primary map canvas; the editor's work surface."},
    {"overworld.tile16_selector",
     "Tile16 palette the canvas paints from; docked beside the canvas in the "
     "Overworld Default preset."},
    {"overworld.properties",
     "Current default: selected-map properties in the Overworld Default "
     "preset."},

    // Dungeon
    {"dungeon.workbench",
     "Single default dungeon surface; hosts the room browser, canvas, and "
     "inspector tools."},

    // Graphics
    {"graphics.sheet_browser_v2",
     "Sheet list that selects what the pixel editor edits."},
    {"graphics.pixel_editor", "Primary graphics sheet editing surface."},

    // Palette
    {"palette.control_panel",
     "Single Palette workspace; hosts group navigation and color editing."},

    // Sprite
    {"sprite.vanilla_editor", "Primary sprite editing surface."},

    // Screen
    {"screen.dungeon_maps",
     "Primary surface of the Screen editor's default preset."},
    {"screen.title_screen",
     "Opened by ScreenEditor::Initialize, so it is what actually shows on "
     "first open (the preset lists dungeon_maps); resolve in the audit doc."},

    // Music
    {"music.song_browser", "Song list that selects what the tracker edits."},
    {"music.tracker", "Playback transport for the Music Default preset."},
    {"music.piano_roll", "Primary music editing surface."},

    // Message
    {"message.message_list",
     "Message list that selects what the editor edits."},
    {"message.message_editor", "Primary message text editing surface."},

    // Assembly
    {"assembly.code_editor", "Primary assembly source editing surface."},
    {"assembly.file_browser", "Project file tree that opens sources."},
    {"assembly.disassembly",
     "Current default: disassembly view in the Assembly Default preset."},
    {"assembly.toolbar",
     "Current default: registered visible. Its Open Folder and Save buttons "
     "are the only quick actions when no project code folder is set."},

    // Shell
    {"dashboard.main", "Editor picker shown at startup and on Ctrl+E."},

    // Emulator and memory
    {"emulator.ppu_viewer", "Primary emulator output surface."},
    {"memory.hex_editor", "Primary surface of the Memory editor."},

    // Agent
    {"agent.chat", "Primary agent conversation surface."},
    {"agent.configuration",
     "Current default: agent provider setup in the Agent preset."},
    {"agent.status", "Current default: agent status in the Agent preset."},
};

// panel-admission: deferred-begin
// The Overworld panels are Codex-coordinated: src/app/editor/overworld/* is
// frozen for edits until scawful approves, and Codex has in-flight branches
// there (codex/overworld-large-selection-preview, codex/terrain-catalog).
// Proposals for each window are in the audit document.
constexpr char kOverworldFrozen[] =
    "Overworld panels are Codex-coordinated; src/app/editor/overworld/* needs "
    "scawful's approval before edits.";
const std::vector<PanelAdmissionEntry> kDeferredDeclarations = {
    {"overworld.canvas", kOverworldFrozen},
    {"overworld.tile16_selector", kOverworldFrozen},
    {"overworld.properties", kOverworldFrozen},
    {"overworld.tile16_editor", kOverworldFrozen},
    {"overworld.tile8_selector", kOverworldFrozen},
    {"overworld.area_graphics", kOverworldFrozen},
    {"overworld.scratch", kOverworldFrozen},
    {"overworld.gfx_groups", kOverworldFrozen},
    {"overworld.usage_stats", kOverworldFrozen},
    {"overworld.item_list", kOverworldFrozen},
    {"overworld.v3_settings", kOverworldFrozen},
    {"overworld.debug", kOverworldFrozen},
    {"overworld.entity_workbench", kOverworldFrozen},
};
// panel-admission: deferred-end

// Fix texts carry no window id so identical failures group into one block.
constexpr char kUndeclaredFix[] =
    "Override GetPresentationPolicy() in the panel class (or pass a policy to "
    "RegisterWindowContent) and return one of: "
    "WindowPresentationPolicy::EmbeddedTool().WithHost(\"<surface that embeds "
    "it>\") for a tool that acts on another surface's selection (inspector "
    "section, tool tab, canvas context-menu popup); OptionalPopOut() for a "
    "standalone window users choose to open; "
    "Diagnostic().WithHost(\"Command palette\") for developer or validation "
    "tooling. CoreWorkspace() is only for an editor's primary work surface and "
    "needs a FirstClassPanelAllowlist() entry. See \"Panel admission\" in "
    "docs/internal/architecture/editor-ui-module-pattern.md.";

constexpr char kCoreFix[] =
    "Core is reserved for an editor's primary work surface. Either add the "
    "window id to FirstClassPanelAllowlist() in "
    "src/app/editor/system/workspace/panel_admission.cc with the workflow it "
    "serves and why no inspector, tool tab, or context-menu popup can host it, "
    "or pick EmbeddedTool().WithHost(...) or OptionalPopOut().";

constexpr char kDefaultVisibleFix[] =
    "Hide it by default: remove IsVisibleByDefault()/visible_by_default and "
    "its LayoutPresets default_visible_panels entry so it opens from its host "
    "or the command palette. If it really is a primary surface, add the window "
    "id to FirstClassPanelAllowlist() in "
    "src/app/editor/system/workspace/panel_admission.cc with a reason.";

constexpr char kHostFix[] =
    "Add .WithHost(\"<surface>\") to the policy, for example "
    "EmbeddedTool().WithHost(\"Dungeon Workbench inspector\") or "
    "Diagnostic().WithHost(\"Command palette\"). The host is where users find "
    "the window now that it is not in the window browser.";

}  // namespace

const std::vector<PanelAdmissionEntry>& FirstClassPanelAllowlist() {
  return kFirstClassAllowlist;
}

const PanelAdmissionEntry* FindFirstClassPanelEntry(
    const std::string& window_id) {
  for (const auto& entry : kFirstClassAllowlist) {
    if (window_id == entry.window_id) {
      return &entry;
    }
  }
  return nullptr;
}

const std::vector<PanelAdmissionEntry>& DeferredPolicyDeclarations() {
  return kDeferredDeclarations;
}

bool IsPolicyDeclarationDeferred(const std::string& window_id) {
  for (const auto& entry : kDeferredDeclarations) {
    if (window_id == entry.window_id) {
      return true;
    }
  }
  return false;
}

std::string PanelAdmissionFinding::ToString() const {
  return "[" + category + "] " + window_id + ": " + problem +
         "\n    Fix: " + fix;
}

std::vector<std::string> CollectDefaultVisibleWindowIds(
    const WorkspaceWindowManager& window_manager, size_t session_id) {
  const std::vector<std::string> registered =
      window_manager.GetWindowsInSession(session_id);
  std::set<std::string> registered_ids(registered.begin(), registered.end());
  std::set<std::string> visible;

  for (const auto& window_id : registered) {
    const WindowDescriptor* descriptor =
        window_manager.GetWindowDescriptor(session_id, window_id);
    if (descriptor != nullptr && descriptor->visibility_flag != nullptr &&
        *descriptor->visibility_flag) {
      visible.insert(window_id);
    }
  }

  constexpr EditorType kPresetEditors[] = {
      EditorType::kOverworld, EditorType::kDungeon, EditorType::kGraphics,
      EditorType::kPalette,   EditorType::kSprite,  EditorType::kScreen,
      EditorType::kMusic,     EditorType::kMessage, EditorType::kAssembly,
      EditorType::kEmulator,  EditorType::kHex,     EditorType::kAgent,
  };
  for (EditorType type : kPresetEditors) {
    for (const auto& window_id : LayoutPresets::GetDefaultPanels(type)) {
      if (registered_ids.count(window_id) != 0) {
        visible.insert(window_id);
      }
    }
  }
  return std::vector<std::string>(visible.begin(), visible.end());
}

std::vector<PanelAdmissionFinding> AuditPanelAdmission(
    const WorkspaceWindowManager& window_manager, size_t session_id) {
  std::vector<PanelAdmissionFinding> findings;

  const std::vector<std::string> default_visible =
      CollectDefaultVisibleWindowIds(window_manager, session_id);
  const std::set<std::string> default_visible_set(default_visible.begin(),
                                                  default_visible.end());

  std::vector<std::string> window_ids =
      window_manager.GetWindowsInSession(session_id);
  std::sort(window_ids.begin(), window_ids.end());
  window_ids.erase(std::unique(window_ids.begin(), window_ids.end()),
                   window_ids.end());

  for (const auto& window_id : window_ids) {
    const WindowDescriptor* descriptor =
        window_manager.GetWindowDescriptor(session_id, window_id);
    if (descriptor == nullptr) {
      continue;
    }
    const WindowPresentationPolicy& policy = descriptor->presentation;
    const bool admitted = FindFirstClassPanelEntry(window_id) != nullptr;
    auto add = [&](std::string problem, std::string fix) {
      findings.push_back({window_id, descriptor->category, std::move(problem),
                          std::move(fix)});
    };

    if (!policy.declared && !IsPolicyDeclarationDeferred(window_id)) {
      add("declares no presentation policy.", kUndeclaredFix);
      continue;
    }

    if (policy.role == WindowPresentationRole::kCoreWorkspace && !admitted) {
      add("declares CoreWorkspace but is not in FirstClassPanelAllowlist().",
          kCoreFix);
    }

    if (default_visible_set.count(window_id) != 0 && !admitted) {
      add(std::string("is visible by default (IsVisibleByDefault(), "
                      "visible_by_default, or a LayoutPresets default) with "
                      "role ") +
              WindowPresentationRoleName(policy.role) +
              " but is not in FirstClassPanelAllowlist().",
          kDefaultVisibleFix);
    }

    const bool needs_host =
        policy.role == WindowPresentationRole::kEmbeddedTool ||
        policy.role == WindowPresentationRole::kDiagnostic;
    if (needs_host && policy.host.empty()) {
      add(std::string(WindowPresentationRoleName(policy.role)) +
              " names no host surface.",
          kHostFix);
    }
  }
  return findings;
}

std::string FormatPanelAdmissionFindings(
    const std::vector<PanelAdmissionFinding>& findings) {
  // One block per distinct fix so 60 identical failures do not repeat the
  // instructions 60 times.
  std::vector<std::string> fix_order;
  std::map<std::string, std::vector<const PanelAdmissionFinding*>> groups;
  for (const auto& finding : findings) {
    auto [it, inserted] = groups.try_emplace(finding.fix);
    if (inserted) {
      fix_order.push_back(finding.fix);
    }
    it->second.push_back(&finding);
  }

  std::string out;
  for (const auto& fix : fix_order) {
    for (const PanelAdmissionFinding* finding : groups[fix]) {
      out += "  [" + finding->category + "] " + finding->window_id + ": " +
             finding->problem + "\n";
    }
    out += "  Fix: " + fix + "\n\n";
  }
  return out;
}

}  // namespace editor
}  // namespace yaze
