#include "app/editor/agent/panels/manifest_panel.h"

#include <chrono>
#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "imgui/imgui.h"

namespace yaze::editor {

namespace {

// Format a file_time_type to a human-readable string.
std::string FormatFileTime(std::filesystem::file_time_type ftime) {
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&cftime));
  return std::string(buf);
}

}  // namespace

void ManifestPanel::Draw() {
  DrawManifestStatus();
  ImGui::Separator();
  DrawProtectedRegions();
}

// ---------------------------------------------------------------------------
// Card B3: Manifest Freshness UX
// ---------------------------------------------------------------------------

void ManifestPanel::DrawManifestStatus() {
  ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
                     ICON_MD_DESCRIPTION " Hack Manifest");
  ImGui::Spacing();

  if (!project_) {
    ImGui::TextDisabled("No project loaded.");
    return;
  }

  const auto& manifest = project_->hack_manifest;
  std::string manifest_path = ResolveManifestPath();

  // Status indicator
  if (manifest.loaded()) {
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), ICON_MD_CHECK_CIRCLE);
    ImGui::SameLine();
    ImGui::Text("Loaded: %s", manifest.hack_name().c_str());
  } else {
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), ICON_MD_ERROR);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                       "No manifest loaded — write conflict detection disabled");
  }

  // Metadata table
  if (manifest.loaded()) {
    if (ImGui::BeginTable("ManifestInfo", 2,
                          ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableSetupColumn("Value");

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Version");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", manifest.manifest_version());

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Total Hooks");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", manifest.total_hooks());

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Protected Regions");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%zu", manifest.protected_regions().size());

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("Feature Flags");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%zu", manifest.feature_flags().size());

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("SRAM Variables");
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%zu", manifest.sram_variables().size());

      ImGui::EndTable();
    }
  }

  // File path and mtime
  ImGui::Spacing();
  if (!manifest_path.empty()) {
    ImGui::TextDisabled("Path:");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", manifest_path.c_str());

    // Show file modification time
    std::error_code ec;
    if (std::filesystem::exists(manifest_path, ec)) {
      auto mtime = std::filesystem::last_write_time(manifest_path, ec);
      if (!ec) {
        ImGui::TextDisabled("Modified:");
        ImGui::SameLine();
        ImGui::Text("%s", FormatFileTime(mtime).c_str());
      }
    } else {
      ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                         ICON_MD_WARNING " File not found on disk");
    }
  } else {
    ImGui::TextDisabled("Path: (not configured)");
  }

  // Reload button
  ImGui::Spacing();
  if (ImGui::Button(ICON_MD_REFRESH " Reload Manifest")) {
    project_->ReloadHackManifest();
    if (project_->hack_manifest.loaded()) {
      status_message_ = "Manifest reloaded successfully.";
      status_is_error_ = false;
    } else {
      status_message_ = "Reload failed — check path and file format.";
      status_is_error_ = true;
    }
  }

  // Status feedback
  if (!status_message_.empty()) {
    ImGui::SameLine();
    if (status_is_error_) {
      ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s",
                         status_message_.c_str());
    } else {
      ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s",
                         status_message_.c_str());
    }
  }
}

// ---------------------------------------------------------------------------
// Card B4: Protected Regions Inspector
// ---------------------------------------------------------------------------

void ManifestPanel::DrawProtectedRegions() {
  ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f),
                     ICON_MD_SHIELD " Protected Regions");
  ImGui::Spacing();

  if (!project_ || !project_->hack_manifest.loaded()) {
    ImGui::TextDisabled("Load a manifest to view protected regions.");
    return;
  }

  const auto& regions = project_->hack_manifest.protected_regions();

  // Filter
  ImGui::SetNextItemWidth(200);
  ImGui::InputTextWithHint("##RegionFilter", "Filter by module...",
                           filter_text_, sizeof(filter_text_));
  ImGui::SameLine();
  ImGui::Text("%zu regions", regions.size());

  ImGui::Spacing();

  // Regions table
  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
      ImGuiTableFlags_SizingFixedFit;

  float table_height = ImGui::GetContentRegionAvail().y - 4;
  if (table_height < 100) table_height = 200;

  if (ImGui::BeginTable("ProtectedRegions", 5, kTableFlags,
                        ImVec2(0, table_height))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Start", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn("Hooks", ImGuiTableColumnFlags_WidthFixed, 45);
    ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    std::string filter_lower;
    if (filter_text_[0] != '\0') {
      filter_lower = filter_text_;
      for (auto& c : filter_lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      }
    }

    for (const auto& region : regions) {
      // Filter by module name
      if (!filter_lower.empty()) {
        std::string module_lower = region.module;
        for (auto& c : module_lower) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (module_lower.find(filter_lower) == std::string::npos) {
          continue;
        }
      }

      ImGui::TableNextRow();

      // Start address
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("$%06X", region.start);

      // End address
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("$%06X", region.end);

      // Size in bytes
      ImGui::TableSetColumnIndex(2);
      uint32_t size = (region.end > region.start) ? region.end - region.start : 0;
      ImGui::Text("%u", size);

      // Hook count
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", region.hook_count);

      // Module
      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(region.module.c_str());

      // Copy address range on right-click
      if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        std::string range_str =
            absl::StrFormat("$%06X-$%06X", region.start, region.end);
        ImGui::SetClipboardText(range_str.c_str());
      }
    }

    ImGui::EndTable();
  }

  ImGui::TextDisabled("Right-click a module to copy the address range.");
}

std::string ManifestPanel::ResolveManifestPath() const {
  if (!project_) return "";

  // Priority 1: Explicit hack_manifest_file setting
  if (!project_->hack_manifest_file.empty()) {
    return project_->GetAbsolutePath(project_->hack_manifest_file);
  }

  // Priority 2: Auto-discover in code_folder
  if (!project_->code_folder.empty()) {
    std::string auto_path =
        project_->GetAbsolutePath(project_->code_folder + "/hack_manifest.json");
    std::error_code ec;
    if (std::filesystem::exists(auto_path, ec)) {
      return auto_path;
    }
  }

  return "";
}

}  // namespace yaze::editor
