#include "app/editor/agent/panels/oracle_state_library_panel.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace editor {

namespace {

// Status colors
ImVec4 GetStatusColor(const std::string& status) {
  if (status == "canon") {
    return ImVec4(0.2f, 0.8f, 0.2f, 1.0f);  // Green
  } else if (status == "draft") {
    return ImVec4(0.9f, 0.7f, 0.1f, 1.0f);  // Yellow
  } else if (status == "deprecated") {
    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
  }
  return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

std::string GetStatusBadge(const std::string& status) {
  if (status == "canon") return "[CANON]";
  if (status == "draft") return "[draft]";
  if (status == "deprecated") return "[DEPR]";
  return "[???]";
}

}  // namespace

OracleStateLibraryPanel::OracleStateLibraryPanel() {
  // Default Oracle manifest path
  const char* home = std::getenv("HOME");
  if (home) {
    manifest_path_ = absl::StrFormat(
        "%s/src/hobby/oracle-of-secrets/Docs/Testing/save_state_library.json",
        home);
  }
  LoadManifest();
}

OracleStateLibraryPanel::~OracleStateLibraryPanel() = default;

void OracleStateLibraryPanel::SetClient(
    std::shared_ptr<emu::mesen::MesenSocketClient> client) {
  client_ = std::move(client);
}

void OracleStateLibraryPanel::RefreshLibrary() {
  LoadManifest();
  status_message_ = absl::StrFormat("Loaded %d states", entries_.size());
  status_is_error_ = false;
}

void OracleStateLibraryPanel::LoadManifest() {
  entries_.clear();

  std::ifstream file(manifest_path_);
  if (!file.is_open()) {
    status_message_ = "Failed to open manifest: " + manifest_path_;
    status_is_error_ = true;
    return;
  }

  try {
    nlohmann::json data = nlohmann::json::parse(file);

    if (data.contains("library_root")) {
      library_root_ = data["library_root"].get<std::string>();
    }

    if (data.contains("entries") && data["entries"].is_array()) {
      for (const auto& entry_json : data["entries"]) {
        StateEntry entry;
        entry.id = entry_json.value("id", "");
        entry.label = entry_json.value("label",
                                        entry_json.value("description", ""));
        entry.path = entry_json.value("path", "");
        entry.status = entry_json.value("status", "draft");
        entry.md5 = entry_json.value("md5", "");
        entry.captured_by = entry_json.value("captured_by", "");
        entry.verified_by = entry_json.value("verified_by", "");
        entry.verified_at = entry_json.value("verified_at", "");
        entry.deprecated_reason = entry_json.value("deprecated_reason", "");

        // Tags
        if (entry_json.contains("tags") && entry_json["tags"].is_array()) {
          for (const auto& tag : entry_json["tags"]) {
            entry.tags.push_back(tag.get<std::string>());
          }
        }

        // Metadata
        if (entry_json.contains("metadata")) {
          const auto& meta = entry_json["metadata"];
          entry.module = meta.value("module", 0);
          entry.room = meta.value("room", 0);
          entry.area = meta.value("area", 0);
          entry.indoors = meta.value("indoors", false);
          entry.link_x = meta.value("link_x", 0);
          entry.link_y = meta.value("link_y", 0);
          entry.health = meta.value("health", 0);
          entry.max_health = meta.value("max_health", 0);
          entry.rupees = meta.value("rupees", 0);
          entry.location = meta.value("location", "");
          entry.summary = meta.value("summary", "");
        } else if (entry_json.contains("gameState")) {
          // Legacy format
          const auto& gs = entry_json["gameState"];
          entry.indoors = gs.value("indoors", false);
        }

        entries_.push_back(entry);
      }
    }

    status_message_ = absl::StrFormat("Loaded %d states", entries_.size());
    status_is_error_ = false;
  } catch (const std::exception& e) {
    status_message_ = absl::StrFormat("JSON parse error: %s", e.what());
    status_is_error_ = true;
  }
}

void OracleStateLibraryPanel::SaveManifest() {
  // Re-read the full manifest, update entries, and write back
  std::ifstream file_in(manifest_path_);
  if (!file_in.is_open()) {
    status_message_ = "Failed to open manifest for writing";
    status_is_error_ = true;
    return;
  }

  nlohmann::json data;
  try {
    data = nlohmann::json::parse(file_in);
  } catch (...) {
    status_message_ = "Failed to parse manifest for update";
    status_is_error_ = true;
    return;
  }
  file_in.close();

  // Update entries in the JSON
  if (data.contains("entries") && data["entries"].is_array()) {
    for (auto& entry_json : data["entries"]) {
      std::string id = entry_json.value("id", "");
      for (const auto& entry : entries_) {
        if (entry.id == id) {
          entry_json["status"] = entry.status;
          if (!entry.verified_by.empty()) {
            entry_json["verified_by"] = entry.verified_by;
            entry_json["verified_at"] = entry.verified_at;
          }
          if (!entry.deprecated_reason.empty()) {
            entry_json["deprecated_reason"] = entry.deprecated_reason;
          }
          break;
        }
      }
    }
  }

  std::ofstream file_out(manifest_path_);
  if (!file_out.is_open()) {
    status_message_ = "Failed to write manifest";
    status_is_error_ = true;
    return;
  }
  file_out << data.dump(2);
  status_message_ = "Manifest saved";
  status_is_error_ = false;
}

absl::Status OracleStateLibraryPanel::LoadState(const std::string& state_id) {
  // Find the entry
  const StateEntry* entry = nullptr;
  for (const auto& ent : entries_) {
    if (ent.id == state_id) {
      entry = &ent;
      break;
    }
  }
  if (!entry) {
    return absl::NotFoundError("State not found: " + state_id);
  }

  // Use the Python CLI to load the state (supports path-based loading)
  const char* home = std::getenv("HOME");
  std::string cmd = absl::StrFormat(
      "python3 %s/src/hobby/oracle-of-secrets/scripts/mesen2_client.py "
      "lib-load %s 2>&1",
      home ? home : "", state_id.c_str());

  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return absl::InternalError("Failed to execute lib-load command");
  }

  char buffer[256];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe)) {
    output += buffer;
  }
  int ret = pclose(pipe);

  if (ret != 0) {
    return absl::InternalError("lib-load failed: " + output);
  }

  status_message_ = absl::StrFormat("Loaded: %s", entry->label.c_str());
  status_is_error_ = false;
  return absl::OkStatus();
}

absl::Status OracleStateLibraryPanel::VerifyState(const std::string& state_id) {
  for (auto& entry : entries_) {
    if (entry.id == state_id) {
      entry.status = "canon";
      entry.verified_by = "scawful";  // TODO(scawful): Make configurable
      entry.verified_at = absl::FormatTime(absl::RFC3339_full, absl::Now(),
                                           absl::UTCTimeZone());
      SaveManifest();
      status_message_ = absl::StrFormat("Verified: %s", entry.label.c_str());
      status_is_error_ = false;
      return absl::OkStatus();
    }
  }
  return absl::NotFoundError("State not found: " + state_id);
}

absl::Status OracleStateLibraryPanel::DeprecateState(
    const std::string& state_id, const std::string& reason) {
  for (auto& entry : entries_) {
    if (entry.id == state_id) {
      entry.status = "deprecated";
      entry.deprecated_reason = reason;
      SaveManifest();
      status_message_ = absl::StrFormat("Deprecated: %s", entry.label.c_str());
      status_is_error_ = false;
      return absl::OkStatus();
    }
  }
  return absl::NotFoundError("State not found: " + state_id);
}

void OracleStateLibraryPanel::Draw() {
  ImGui::PushID("OracleStateLibraryPanel");

  DrawToolbar();
  ImGui::Separator();

  // Main content area
  float details_width = 300.0f;
  ImVec2 avail = ImGui::GetContentRegionAvail();

  // Left side: state list
  ImGui::BeginChild("StateList", ImVec2(avail.x - details_width - 10, 0), true);
  DrawStateList();
  ImGui::EndChild();

  ImGui::SameLine();

  // Right side: details
  ImGui::BeginChild("StateDetails", ImVec2(details_width, 0), true);
  DrawStateDetails();
  ImGui::EndChild();

  // Dialogs
  DrawVerificationDialog();

  ImGui::PopID();
}

void OracleStateLibraryPanel::DrawToolbar() {
  // Refresh button
  if (ImGui::Button(ICON_MD_REFRESH " Refresh")) {
    RefreshLibrary();
  }
  ImGui::SameLine();

  // Filter toggles
  ImGui::Checkbox("Canon", &show_canon_);
  ImGui::SameLine();
  ImGui::Checkbox("Draft", &show_draft_);
  ImGui::SameLine();
  ImGui::Checkbox("Deprecated", &show_deprecated_);
  ImGui::SameLine();

  // Text filter
  ImGui::SetNextItemWidth(150);
  ImGui::InputTextWithHint("##filter", "Filter...", filter_text_,
                           sizeof(filter_text_));

  // Status message
  if (!status_message_.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(status_is_error_ ? ImVec4(1, 0.3f, 0.3f, 1)
                                        : ImVec4(0.3f, 1, 0.3f, 1),
                       "%s", status_message_.c_str());
  }

  // Stats
  int canon_count = 0, draft_count = 0, depr_count = 0;
  for (const auto& e : entries_) {
    if (e.status == "canon") canon_count++;
    else if (e.status == "draft") draft_count++;
    else if (e.status == "deprecated") depr_count++;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("| %d canon, %d draft, %d deprecated",
                      canon_count, draft_count, depr_count);
}

void OracleStateLibraryPanel::DrawStateList() {
  std::string filter_lower(filter_text_);
  std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(),
                 ::tolower);

  int visible_index = 0;
  for (size_t i = 0; i < entries_.size(); ++i) {
    const auto& entry = entries_[i];

    // Filter by status
    if (entry.status == "canon" && !show_canon_) continue;
    if (entry.status == "draft" && !show_draft_) continue;
    if (entry.status == "deprecated" && !show_deprecated_) continue;

    // Filter by text
    if (!filter_lower.empty()) {
      std::string label_lower = entry.label;
      std::transform(label_lower.begin(), label_lower.end(),
                     label_lower.begin(), ::tolower);
      std::string id_lower = entry.id;
      std::transform(id_lower.begin(), id_lower.end(),
                     id_lower.begin(), ::tolower);
      if (label_lower.find(filter_lower) == std::string::npos &&
          id_lower.find(filter_lower) == std::string::npos) {
        continue;
      }
    }

    // Status badge
    ImGui::TextColored(GetStatusColor(entry.status), "%s",
                       GetStatusBadge(entry.status).c_str());
    ImGui::SameLine();

    // Selectable
    bool is_selected = (selected_index_ == static_cast<int>(i));
    if (ImGui::Selectable(entry.label.c_str(), is_selected,
                          ImGuiSelectableFlags_SpanAllColumns)) {
      selected_index_ = static_cast<int>(i);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem(ICON_MD_PLAY_ARROW " Load in Mesen2")) {
        auto status = LoadState(entry.id);
        if (!status.ok()) {
          status_message_ = std::string(status.message());
          status_is_error_ = true;
        }
      }
      if (entry.status == "draft") {
        if (ImGui::MenuItem(ICON_MD_CHECK " Verify as Canon")) {
          verify_target_id_ = entry.id;
          show_verify_dialog_ = true;
        }
      }
      if (entry.status != "deprecated") {
        if (ImGui::MenuItem(ICON_MD_DELETE " Deprecate")) {
          deprecate_target_id_ = entry.id;
          show_deprecate_dialog_ = true;
        }
      }
      ImGui::EndPopup();
    }

    // Tooltip with tags
    if (ImGui::IsItemHovered() && !entry.tags.empty()) {
      ImGui::BeginTooltip();
      ImGui::Text("Tags: ");
      for (size_t t = 0; t < entry.tags.size(); ++t) {
        if (t > 0) ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "[%s]",
                          entry.tags[t].c_str());
      }
      ImGui::EndTooltip();
    }

    ++visible_index;
  }
}

void OracleStateLibraryPanel::DrawStateDetails() {
  if (selected_index_ < 0 ||
      selected_index_ >= static_cast<int>(entries_.size())) {
    ImGui::TextDisabled("Select a state to view details");
    return;
  }

  const auto& entry = entries_[selected_index_];

  // Header
  ImGui::TextColored(GetStatusColor(entry.status), "%s",
                     GetStatusBadge(entry.status).c_str());
  ImGui::SameLine();
  ImGui::Text("%s", entry.label.c_str());
  ImGui::Separator();

  // Actions
  bool connected = client_ && client_->IsConnected();
  if (!connected) {
    ImGui::TextDisabled("Connect to Mesen2 to load states");
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Load", ImVec2(-1, 0))) {
      auto status = LoadState(entry.id);
      if (!status.ok()) {
        status_message_ = std::string(status.message());
        status_is_error_ = true;
      }
    }
  }

  if (entry.status == "draft") {
    if (ImGui::Button(ICON_MD_CHECK " Verify as Canon", ImVec2(-1, 0))) {
      verify_target_id_ = entry.id;
      show_verify_dialog_ = true;
    }
  }

  if (entry.status != "deprecated") {
    if (ImGui::Button(ICON_MD_DELETE " Deprecate", ImVec2(-1, 0))) {
      deprecate_target_id_ = entry.id;
      std::memset(deprecate_reason_, 0, sizeof(deprecate_reason_));
      show_deprecate_dialog_ = true;
    }
  }

  ImGui::Separator();

  // Info
  ImGui::Text("ID: %s", entry.id.c_str());
  ImGui::Text("Path: %s", entry.path.c_str());

  if (!entry.md5.empty()) {
    ImGui::Text("MD5: %s", entry.md5.substr(0, 16).c_str());
  }

  if (!entry.location.empty()) {
    ImGui::Text("Location: %s", entry.location.c_str());
  }

  if (entry.area > 0 || entry.link_x > 0) {
    ImGui::Text("Area: 0x%02X  Pos: (%d, %d)", entry.area, entry.link_x,
                entry.link_y);
  }

  if (entry.health > 0) {
    float ratio = static_cast<float>(entry.health) /
                  std::max(1, entry.max_health);
    ImGui::Text("Health: %d/%d", entry.health, entry.max_health);
    ImGui::ProgressBar(ratio, ImVec2(-1, 0));
  }

  if (!entry.captured_by.empty()) {
    ImGui::Text("Captured by: %s", entry.captured_by.c_str());
  }

  if (!entry.verified_by.empty()) {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                       "Verified by: %s", entry.verified_by.c_str());
    if (!entry.verified_at.empty()) {
      ImGui::TextDisabled("at %s", entry.verified_at.c_str());
    }
  }

  if (!entry.deprecated_reason.empty()) {
    ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.3f, 1.0f),
                       "Deprecated: %s", entry.deprecated_reason.c_str());
  }

  // Tags
  if (!entry.tags.empty()) {
    ImGui::Separator();
    ImGui::Text("Tags:");
    for (const auto& tag : entry.tags) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "[%s]", tag.c_str());
    }
  }
}

void OracleStateLibraryPanel::DrawVerificationDialog() {
  // Verify dialog
  if (show_verify_dialog_) {
    ImGui::OpenPopup("Verify State");
    show_verify_dialog_ = false;
  }
  if (ImGui::BeginPopupModal("Verify State", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Promote '%s' to CANON status?", verify_target_id_.c_str());
    ImGui::TextDisabled("This marks the state as verified and trusted.");

    ImGui::InputText("Notes (optional)", verify_notes_, sizeof(verify_notes_));

    if (ImGui::Button("Verify", ImVec2(120, 0))) {
      auto status = VerifyState(verify_target_id_);
      if (!status.ok()) {
        status_message_ = std::string(status.message());
        status_is_error_ = true;
      }
      std::memset(verify_notes_, 0, sizeof(verify_notes_));
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Deprecate dialog
  if (show_deprecate_dialog_) {
    ImGui::OpenPopup("Deprecate State");
    show_deprecate_dialog_ = false;
  }
  if (ImGui::BeginPopupModal("Deprecate State", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Mark '%s' as DEPRECATED?", deprecate_target_id_.c_str());
    ImGui::TextDisabled("This excludes the state from testing.");

    ImGui::InputText("Reason", deprecate_reason_, sizeof(deprecate_reason_));

    if (ImGui::Button("Deprecate", ImVec2(120, 0))) {
      auto status = DeprecateState(deprecate_target_id_, deprecate_reason_);
      if (!status.ok()) {
        status_message_ = std::string(status.message());
        status_is_error_ = true;
      }
      std::memset(deprecate_reason_, 0, sizeof(deprecate_reason_));
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace yaze
