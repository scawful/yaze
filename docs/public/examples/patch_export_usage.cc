// Example: How to use WasmPatchExport in the yaze editor
// This code would typically be integrated into the ROM file manager or editor menu

#include "app/platform/wasm/wasm_patch_export.h"
#include "app/rom.h"
#include "imgui.h"

namespace yaze {
namespace editor {

// Example function that could be added to RomFileManager or MenuOrchestrator
void ShowPatchExportDialog(Rom* rom) {
  static bool show_export_dialog = false;
  static int patch_format = 0;  // 0 = BPS, 1 = IPS
  static char filename[256] = "my_hack";

  // Menu item to trigger export
  if (ImGui::MenuItem("Export Patch...", nullptr, nullptr, rom->is_loaded())) {
    show_export_dialog = true;
  }

  // Export dialog window
  if (show_export_dialog) {
    ImGui::OpenPopup("Export Patch");
  }

  if (ImGui::BeginPopupModal("Export Patch", &show_export_dialog)) {
    // Get the original ROM data (assuming rom stores both original and modified)
    const auto& original_data = rom->original_data();  // Would need to add this
    const auto& modified_data = rom->data();

    // Show patch preview information
    auto patch_info = platform::WasmPatchExport::GetPatchPreview(
        original_data, modified_data);

    ImGui::Text("Patch Summary:");
    ImGui::Separator();
    ImGui::Text("Total changed bytes: %zu", patch_info.changed_bytes);
    ImGui::Text("Number of regions: %zu", patch_info.num_regions);

    if (patch_info.changed_bytes == 0) {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "No changes detected!");
    }

    // Show changed regions (limit to first 10 for UI)
    if (!patch_info.changed_regions.empty()) {
      ImGui::Separator();
      ImGui::Text("Changed Regions:");
      int region_count = 0;
      for (const auto& region : patch_info.changed_regions) {
        if (region_count >= 10) {
          ImGui::Text("... and %zu more regions",
                     patch_info.changed_regions.size() - 10);
          break;
        }
        ImGui::Text("  Offset: 0x%06X, Size: %zu bytes",
                   static_cast<unsigned>(region.first), region.second);
        region_count++;
      }
    }

    ImGui::Separator();

    // Format selection
    ImGui::Text("Patch Format:");
    ImGui::RadioButton("BPS (Beat)", &patch_format, 0);
    ImGui::SameLine();
    ImGui::RadioButton("IPS", &patch_format, 1);

    // Filename input
    ImGui::Text("Filename:");
    ImGui::InputText("##filename", filename, sizeof(filename));

    // Export buttons
    ImGui::Separator();
    if (ImGui::Button("Export", ImVec2(120, 0))) {
      if (patch_info.changed_bytes > 0) {
        absl::Status status;
        std::string full_filename = std::string(filename);

        if (patch_format == 0) {
          // BPS format
          if (full_filename.find(".bps") == std::string::npos) {
            full_filename += ".bps";
          }
          status = platform::WasmPatchExport::ExportBPS(
              original_data, modified_data, full_filename);
        } else {
          // IPS format
          if (full_filename.find(".ips") == std::string::npos) {
            full_filename += ".ips";
          }
          status = platform::WasmPatchExport::ExportIPS(
              original_data, modified_data, full_filename);
        }

        if (status.ok()) {
          ImGui::CloseCurrentPopup();
          show_export_dialog = false;
          // Could show success toast here
        } else {
          // Show error message
          ImGui::OpenPopup("Export Error");
        }
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
      show_export_dialog = false;
    }

    // Error popup
    if (ImGui::BeginPopupModal("Export Error")) {
      ImGui::Text("Failed to export patch!");
      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::EndPopup();
  }
}

// Alternative: Quick export functions for toolbar/menu
void QuickExportBPS(Rom* rom) {
#ifdef __EMSCRIPTEN__
  if (!rom || !rom->is_loaded()) return;

  const auto& original = rom->original_data();  // Would need to add this
  const auto& modified = rom->data();

  // Generate default filename based on ROM name
  std::string filename = rom->filename();
  size_t dot_pos = filename.find_last_of('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos);
  }
  filename += ".bps";

  auto status = platform::WasmPatchExport::ExportBPS(
      original, modified, filename);

  if (!status.ok()) {
    // Show error toast or log
  }
#endif
}

void QuickExportIPS(Rom* rom) {
#ifdef __EMSCRIPTEN__
  if (!rom || !rom->is_loaded()) return;

  const auto& original = rom->original_data();
  const auto& modified = rom->data();

  // Check IPS size limit
  if (modified.size() > 0xFFFFFF) {
    // Show error: ROM too large for IPS format
    return;
  }

  std::string filename = rom->filename();
  size_t dot_pos = filename.find_last_of('.');
  if (dot_pos != std::string::npos) {
    filename = filename.substr(0, dot_pos);
  }
  filename += ".ips";

  auto status = platform::WasmPatchExport::ExportIPS(
      original, modified, filename);

  if (!status.ok()) {
    // Show error toast or log
  }
#endif
}

}  // namespace editor
}  // namespace yaze