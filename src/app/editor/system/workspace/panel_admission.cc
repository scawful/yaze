#include "app/editor/system/workspace/panel_admission.h"

#include <algorithm>
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
     "Palette group picker; selects what the palette editor shows."},
    {"palette.ow_main", "Primary palette grid in the Palette Default preset."},

    // Sprite
    {"sprite.vanilla_editor", "Primary sprite editing surface."},

    // Screen
    {"screen.dungeon_maps",
     "Primary surface of the Screen editor's default preset."},

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

    // Emulator and memory
    {"emulator.ppu_viewer", "Primary emulator output surface."},
    {"memory.hex_editor", "Primary surface of the Memory editor."},

    // Agent
    {"agent.chat", "Primary agent conversation surface."},
    {"agent.configuration",
     "Current default: agent provider setup in the Agent preset."},
    {"agent.status", "Current default: agent status in the Agent preset."},
};

std::string AuthorFix(const std::string& window_id) {
  return "Override GetPresentationPolicy() in the panel class (or pass a "
         "policy to RegisterWindowContent) and return one of: "
         "WindowPresentationPolicy::EmbeddedTool().WithHost(\"<surface that "
         "embeds it>\") for a tool that acts on another surface's selection "
         "(inspector section, tool tab, canvas context-menu popup); "
         "OptionalPopOut() for a standalone window users choose to open; "
         "Diagnostic().WithHost(\"Command palette\") for developer or "
         "validation tooling. CoreWorkspace() is only for an editor's primary "
         "work surface and needs a FirstClassPanelAllowlist() entry for '" +
         window_id +
         "'. See \"Panel admission\" in "
         "docs/internal/architecture/editor-ui-module-pattern.md.";
}

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

    if (!policy.declared) {
      add("declares no presentation policy.", AuthorFix(window_id));
      continue;
    }

    if (policy.role == WindowPresentationRole::kCoreWorkspace && !admitted) {
      add("declares CoreWorkspace but is not in FirstClassPanelAllowlist().",
          "Core is reserved for an editor's primary work surface. Either add "
          "'" +
              window_id +
              "' to FirstClassPanelAllowlist() in "
              "src/app/editor/system/workspace/panel_admission.cc with the "
              "workflow it serves and why no inspector, tool tab, or "
              "context-menu popup can host it, or pick "
              "EmbeddedTool().WithHost(...) or OptionalPopOut().");
    }

    if (default_visible_set.count(window_id) != 0 && !admitted) {
      add(std::string("is visible by default (IsVisibleByDefault(), "
                      "visible_by_default, or a LayoutPresets default) with "
                      "role ") +
              WindowPresentationRoleName(policy.role) +
              " but is not in FirstClassPanelAllowlist().",
          "Hide it by default: remove IsVisibleByDefault()/visible_by_default "
          "and its LayoutPresets default_visible_panels entry so it opens "
          "from its host or the command palette. If it really is a primary "
          "surface, add '" +
              window_id +
              "' to FirstClassPanelAllowlist() in "
              "src/app/editor/system/workspace/panel_admission.cc with a "
              "reason.");
    }

    const bool needs_host =
        policy.role == WindowPresentationRole::kEmbeddedTool ||
        policy.role == WindowPresentationRole::kDiagnostic;
    if (needs_host && policy.host.empty()) {
      add(std::string(WindowPresentationRoleName(policy.role)) +
              " names no host surface.",
          "Add .WithHost(\"<surface>\") to the policy, for example "
          "EmbeddedTool().WithHost(\"Dungeon Workbench inspector\") or "
          "Diagnostic().WithHost(\"Command palette\"). The host is where "
          "users find the window now that it is not in the window browser.");
    }
  }
  return findings;
}

std::string FormatPanelAdmissionFindings(
    const std::vector<PanelAdmissionFinding>& findings) {
  std::string out;
  for (const auto& finding : findings) {
    out += finding.ToString();
    out += "\n";
  }
  return out;
}

}  // namespace editor
}  // namespace yaze
