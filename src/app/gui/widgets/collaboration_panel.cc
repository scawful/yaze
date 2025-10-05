#include "app/gui/widgets/collaboration_panel.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace gui {

CollaborationPanel::CollaborationPanel()
    : selected_tab_(0),
      selected_rom_sync_(-1),
      selected_snapshot_(-1),
      selected_proposal_(-1),
      show_sync_details_(false),
      show_snapshot_preview_(true),
      auto_scroll_(true),
      filter_pending_only_(false) {
  
  // Initialize search filter
  search_filter_[0] = '\0';
  
  // Initialize colors
  colors_.sync_applied = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
  colors_.sync_pending = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
  colors_.sync_error = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
  colors_.proposal_pending = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
  colors_.proposal_approved = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
  colors_.proposal_rejected = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
  colors_.proposal_applied = ImVec4(0.2f, 0.6f, 0.8f, 1.0f);
}

CollaborationPanel::~CollaborationPanel() {
  // Cleanup any OpenGL textures
  for (auto& snapshot : snapshots_) {
    if (snapshot.texture_id) {
      // Note: Actual texture cleanup would depend on your renderer
      // This is a placeholder
      snapshot.texture_id = nullptr;
    }
  }
}

void CollaborationPanel::Render(bool* p_open) {
  if (!ImGui::Begin("Collaboration", p_open, ImGuiWindowFlags_None)) {
    ImGui::End();
    return;
  }
  
  // Tabs for different collaboration features
  if (ImGui::BeginTabBar("CollaborationTabs")) {
    if (ImGui::BeginTabItem("ROM Sync")) {
      selected_tab_ = 0;
      RenderRomSyncTab();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Snapshots")) {
      selected_tab_ = 1;
      RenderSnapshotsTab();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Proposals")) {
      selected_tab_ = 2;
      RenderProposalsTab();
      ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
  }
  
  ImGui::End();
}

void CollaborationPanel::RenderRomSyncTab() {
  ImGui::TextWrapped("ROM Synchronization History");
  ImGui::Separator();
  
  // Toolbar
  if (ImGui::Button("Clear History")) {
    rom_syncs_.clear();
  }
  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &auto_scroll_);
  ImGui::SameLine();
  ImGui::Checkbox("Show Details", &show_sync_details_);
  
  ImGui::Separator();
  
  // Stats
  int applied_count = 0;
  int pending_count = 0;
  int error_count = 0;
  
  for (const auto& sync : rom_syncs_) {
    if (sync.applied) applied_count++;
    else if (!sync.error_message.empty()) error_count++;
    else pending_count++;
  }
  
  ImGui::Text("Total: %zu | ", rom_syncs_.size());
  ImGui::SameLine();
  ImGui::TextColored(colors_.sync_applied, "Applied: %d", applied_count);
  ImGui::SameLine();
  ImGui::TextColored(colors_.sync_pending, "Pending: %d", pending_count);
  ImGui::SameLine();
  ImGui::TextColored(colors_.sync_error, "Errors: %d", error_count);
  
  ImGui::Separator();
  
  // Sync list
  if (ImGui::BeginChild("SyncList", ImVec2(0, 0), true)) {
    for (size_t i = 0; i < rom_syncs_.size(); ++i) {
      RenderRomSyncEntry(rom_syncs_[i], i);
    }
    
    if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  ImGui::EndChild();
}

void CollaborationPanel::RenderSnapshotsTab() {
  ImGui::TextWrapped("Shared Snapshots Gallery");
  ImGui::Separator();
  
  // Toolbar
  if (ImGui::Button("Clear Gallery")) {
    snapshots_.clear();
  }
  ImGui::SameLine();
  ImGui::Checkbox("Show Preview", &show_snapshot_preview_);
  ImGui::SameLine();
  ImGui::InputText("Search", search_filter_, sizeof(search_filter_));
  
  ImGui::Separator();
  
  // Snapshot grid
  if (ImGui::BeginChild("SnapshotGrid", ImVec2(0, 0), true)) {
    float thumbnail_size = 150.0f;
    float padding = 10.0f;
    float cell_size = thumbnail_size + padding;
    
    int columns = std::max(1, (int)((ImGui::GetContentRegionAvail().x) / cell_size));
    
    for (size_t i = 0; i < snapshots_.size(); ++i) {
      // Filter by search
      if (search_filter_[0] != '\0') {
        std::string search_lower = search_filter_;
        std::string sender_lower = snapshots_[i].sender;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
        std::transform(sender_lower.begin(), sender_lower.end(), sender_lower.begin(), ::tolower);
        
        if (sender_lower.find(search_lower) == std::string::npos &&
            snapshots_[i].snapshot_type.find(search_lower) == std::string::npos) {
          continue;
        }
      }
      
      RenderSnapshotEntry(snapshots_[i], i);
      
      // Grid layout
      if ((i + 1) % columns != 0 && i < snapshots_.size() - 1) {
        ImGui::SameLine();
      }
    }
  }
  ImGui::EndChild();
}

void CollaborationPanel::RenderProposalsTab() {
  ImGui::TextWrapped("AI Proposals & Suggestions");
  ImGui::Separator();
  
  // Toolbar
  if (ImGui::Button("Clear All")) {
    proposals_.clear();
  }
  ImGui::SameLine();
  ImGui::Checkbox("Pending Only", &filter_pending_only_);
  ImGui::SameLine();
  ImGui::InputText("Search", search_filter_, sizeof(search_filter_));
  
  ImGui::Separator();
  
  // Stats
  int pending = 0, approved = 0, rejected = 0, applied = 0;
  for (const auto& proposal : proposals_) {
    if (proposal.status == "pending") pending++;
    else if (proposal.status == "approved") approved++;
    else if (proposal.status == "rejected") rejected++;
    else if (proposal.status == "applied") applied++;
  }
  
  ImGui::Text("Total: %zu", proposals_.size());
  ImGui::SameLine();
  ImGui::TextColored(colors_.proposal_pending, " | Pending: %d", pending);
  ImGui::SameLine();
  ImGui::TextColored(colors_.proposal_approved, " | Approved: %d", approved);
  ImGui::SameLine();
  ImGui::TextColored(colors_.proposal_rejected, " | Rejected: %d", rejected);
  ImGui::SameLine();
  ImGui::TextColored(colors_.proposal_applied, " | Applied: %d", applied);
  
  ImGui::Separator();
  
  // Proposal list
  if (ImGui::BeginChild("ProposalList", ImVec2(0, 0), true)) {
    for (size_t i = 0; i < proposals_.size(); ++i) {
      // Filter
      if (filter_pending_only_ && proposals_[i].status != "pending") {
        continue;
      }
      
      if (search_filter_[0] != '\0') {
        std::string search_lower = search_filter_;
        std::string sender_lower = proposals_[i].sender;
        std::string desc_lower = proposals_[i].description;
        std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
        std::transform(sender_lower.begin(), sender_lower.end(), sender_lower.begin(), ::tolower);
        std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(), ::tolower);
        
        if (sender_lower.find(search_lower) == std::string::npos &&
            desc_lower.find(search_lower) == std::string::npos) {
          continue;
        }
      }
      
      RenderProposalEntry(proposals_[i], i);
    }
  }
  ImGui::EndChild();
}

void CollaborationPanel::RenderRomSyncEntry(const RomSyncEntry& entry, int index) {
  ImGui::PushID(index);
  
  // Status indicator
  ImVec4 status_color;
  const char* status_icon;
  
  if (entry.applied) {
    status_color = colors_.sync_applied;
    status_icon = "[✓]";
  } else if (!entry.error_message.empty()) {
    status_color = colors_.sync_error;
    status_icon = "[✗]";
  } else {
    status_color = colors_.sync_pending;
    status_icon = "[◷]";
  }
  
  ImGui::TextColored(status_color, "%s", status_icon);
  ImGui::SameLine();
  
  // Entry info
  ImGui::Text("%s - %s (%s)",
              FormatTimestamp(entry.timestamp).c_str(),
              entry.sender.c_str(),
              FormatFileSize(entry.diff_size).c_str());
  
  // Details on hover or if enabled
  if (show_sync_details_ || ImGui::IsItemHovered()) {
    ImGui::Indent();
    ImGui::TextWrapped("ROM Hash: %s", entry.rom_hash.substr(0, 16).c_str());
    if (!entry.error_message.empty()) {
      ImGui::TextColored(colors_.sync_error, "Error: %s", entry.error_message.c_str());
    }
    ImGui::Unindent();
  }
  
  ImGui::Separator();
  ImGui::PopID();
}

void CollaborationPanel::RenderSnapshotEntry(const SnapshotEntry& entry, int index) {
  ImGui::PushID(index);
  
  ImGui::BeginGroup();
  
  // Thumbnail placeholder or actual image
  if (show_snapshot_preview_ && entry.is_image && entry.texture_id) {
    ImGui::Image(entry.texture_id, ImVec2(150, 150));
  } else {
    // Placeholder
    ImGui::BeginChild("SnapshotPlaceholder", ImVec2(150, 150), true);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50);
    ImGui::TextWrapped("%s", entry.snapshot_type.c_str());
    ImGui::EndChild();
  }
  
  // Info
  ImGui::TextWrapped("%s", entry.sender.c_str());
  ImGui::Text("%s", FormatTimestamp(entry.timestamp).c_str());
  ImGui::Text("%s", FormatFileSize(entry.data_size).c_str());
  
  // Actions
  if (ImGui::SmallButton("View")) {
    selected_snapshot_ = index;
    // TODO: Open snapshot viewer
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Export")) {
    // TODO: Export snapshot to file
  }
  
  ImGui::EndGroup();
  
  ImGui::PopID();
}

void CollaborationPanel::RenderProposalEntry(const ProposalEntry& entry, int index) {
  ImGui::PushID(index);
  
  // Status icon and color
  const char* icon = GetProposalStatusIcon(entry.status);
  ImVec4 color = GetProposalStatusColor(entry.status);
  
  ImGui::TextColored(color, "%s", icon);
  ImGui::SameLine();
  
  // Collapsible header
  bool is_open = ImGui::TreeNode(entry.description.c_str());
  
  if (is_open) {
    ImGui::Indent();
    
    ImGui::Text("From: %s", entry.sender.c_str());
    ImGui::Text("Time: %s", FormatTimestamp(entry.timestamp).c_str());
    ImGui::Text("Status: %s", entry.status.c_str());
    
    ImGui::Separator();
    
    // Proposal data
    ImGui::TextWrapped("%s", entry.proposal_data.c_str());
    
    // Actions for pending proposals
    if (entry.status == "pending") {
      ImGui::Separator();
      if (ImGui::Button("✓ Approve")) {
        // TODO: Send approval to server
      }
      ImGui::SameLine();
      if (ImGui::Button("✗ Reject")) {
        // TODO: Send rejection to server
      }
      ImGui::SameLine();
      if (ImGui::Button("▶ Apply Now")) {
        // TODO: Execute proposal
      }
    }
    
    ImGui::Unindent();
    ImGui::TreePop();
  }
  
  ImGui::Separator();
  ImGui::PopID();
}

void CollaborationPanel::AddRomSync(const RomSyncEntry& entry) {
  rom_syncs_.push_back(entry);
}

void CollaborationPanel::AddSnapshot(const SnapshotEntry& entry) {
  snapshots_.push_back(entry);
}

void CollaborationPanel::AddProposal(const ProposalEntry& entry) {
  proposals_.push_back(entry);
}

void CollaborationPanel::UpdateProposalStatus(const std::string& proposal_id, const std::string& status) {
  for (auto& proposal : proposals_) {
    if (proposal.proposal_id == proposal_id) {
      proposal.status = status;
      break;
    }
  }
}

void CollaborationPanel::Clear() {
  rom_syncs_.clear();
  snapshots_.clear();
  proposals_.clear();
}

ProposalEntry* CollaborationPanel::GetProposal(const std::string& proposal_id) {
  for (auto& proposal : proposals_) {
    if (proposal.proposal_id == proposal_id) {
      return &proposal;
    }
  }
  return nullptr;
}

std::string CollaborationPanel::FormatTimestamp(int64_t timestamp) {
  std::time_t time = timestamp / 1000;  // Convert ms to seconds
  std::tm* tm = std::localtime(&time);
  
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%H:%M:%S", tm);
  return std::string(buffer);
}

std::string CollaborationPanel::FormatFileSize(size_t bytes) {
  const char* units[] = {"B", "KB", "MB", "GB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);
  
  while (size >= 1024.0 && unit_index < 3) {
    size /= 1024.0;
    unit_index++;
  }
  
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.1f %s", size, units[unit_index]);
  return std::string(buffer);
}

const char* CollaborationPanel::GetProposalStatusIcon(const std::string& status) {
  if (status == "pending") return "[◷]";
  if (status == "approved") return "[✓]";
  if (status == "rejected") return "[✗]";
  if (status == "applied") return "[✦]";
  return "[?]";
}

ImVec4 CollaborationPanel::GetProposalStatusColor(const std::string& status) {
  if (status == "pending") return colors_.proposal_pending;
  if (status == "approved") return colors_.proposal_approved;
  if (status == "rejected") return colors_.proposal_rejected;
  if (status == "applied") return colors_.proposal_applied;
  return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
}

}  // namespace gui
}  // namespace app
}  // namespace yaze
