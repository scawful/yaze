#include "app/editor/code/diagnostics_panel.h"

#include <string>

#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

struct SeverityStyle {
  const char* icon;
  ImVec4 color;
  const char* label;
};

SeverityStyle StyleFor(core::AssemblyDiagnosticSeverity severity) {
  switch (severity) {
    case core::AssemblyDiagnosticSeverity::kWarning:
      return {ICON_MD_WARNING, ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "warning"};
    case core::AssemblyDiagnosticSeverity::kNote:
      return {ICON_MD_INFO, ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "note"};
    case core::AssemblyDiagnosticSeverity::kError:
    default:
      return {ICON_MD_ERROR, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "error"};
  }
}

}  // namespace

void DrawDiagnosticsPanel(std::span<const core::AssemblyDiagnostic> diagnostics,
                          const DiagnosticsPanelCallbacks& callbacks) {
  if (diagnostics.empty()) {
    ImGui::TextDisabled("No diagnostics");
    return;
  }

  // Counts strip — quick read on the overall health.
  int errors = 0, warnings = 0, notes = 0;
  for (const auto& d : diagnostics) {
    switch (d.severity) {
      case core::AssemblyDiagnosticSeverity::kWarning:
        ++warnings;
        break;
      case core::AssemblyDiagnosticSeverity::kNote:
        ++notes;
        break;
      case core::AssemblyDiagnosticSeverity::kError:
      default:
        ++errors;
        break;
    }
  }
  if (notes > 0) {
    ImGui::Text("%d error(s), %d warning(s), %d note(s)", errors, warnings,
                notes);
  } else {
    ImGui::Text("%d error(s), %d warning(s)", errors, warnings);
  }
  ImGui::Separator();

  if (ImGui::BeginTable("##diagnostics", 3,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed,
                            90.0f);
    ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed,
                            220.0f);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
      const auto& d = diagnostics[i];
      const auto style = StyleFor(d.severity);

      ImGui::PushID(static_cast<int>(i));
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      {
        gui::StyleColorGuard guard(ImGuiCol_Text, style.color);
        ImGui::Text("%s %s", style.icon, style.label);
      }

      ImGui::TableSetColumnIndex(1);
      if (!d.file.empty()) {
        std::string label = d.file + ":" + std::to_string(d.line);
        if (d.column > 0)
          label += ":" + std::to_string(d.column);
        if (ImGui::Selectable(label.c_str(), false,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          if (callbacks.on_diagnostic_activated) {
            callbacks.on_diagnostic_activated(d.file, d.line, d.column);
          }
        }
      } else {
        ImGui::TextDisabled("<unknown>");
      }

      ImGui::TableSetColumnIndex(2);
      ImGui::TextWrapped("%s", d.message.c_str());

      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

}  // namespace editor
}  // namespace yaze
