#include "app/gui/app/collaboration_panel.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "imgui/imgui.h"

namespace yaze {

namespace gui {

CollaborationPanel::CollaborationPanel()
    : rom_(nullptr),
      version_mgr_(nullptr),
      approval_mgr_(nullptr),
      selected_tab_(0),
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

void CollaborationPanel::Initialize(
    Rom* rom, net::RomVersionManager* version_mgr,
    net::ProposalApprovalManager* approval_mgr) {
  rom_ = rom;
  version_mgr_ = version_mgr;
  approval_mgr_ = approval_mgr;
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

    if (ImGui::BeginTabItem("Version History")) {
      selected_tab_ = 1;
      RenderVersionHistoryTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Snapshots")) {
      selected_tab_ = 2;
      RenderSnapshotsTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Proposals")) {
      selected_tab_ = 3;
      RenderProposalsTab();
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("🔒 Approvals")) {
      selected_tab_ = 4;
      RenderApprovalTab();
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
    if (sync.applied)
      applied_count++;
    else if (!sync.error_message.empty())
      error_count++;
    else
      pending_count++;
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

    int columns =
        std::max(1, (int)((ImGui::GetContentRegionAvail().x) / cell_size));

    for (size_t i = 0; i < snapshots_.size(); ++i) {
      // Filter by search
      if (search_filter_[0] != '\0') {
        std::string search_lower = search_filter_;
        std::string sender_lower = snapshots_[i].sender;
        std::transform(search_lower.begin(), search_lower.end(),
                       search_lower.begin(), ::tolower);
        std::transform(sender_lower.begin(), sender_lower.end(),
                       sender_lower.begin(), ::tolower);

        if (sender_lower.find(search_lower) == std::string::npos &&
            snapshots_[i].snapshot_type.find(search_lower) ==
                std::string::npos) {
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
    if (proposal.status == "pending")
      pending++;
    else if (proposal.status == "approved")
      approved++;
    else if (proposal.status == "rejected")
      rejected++;
    else if (proposal.status == "applied")
      applied++;
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
        std::transform(search_lower.begin(), search_lower.end(),
                       search_lower.begin(), ::tolower);
        std::transform(sender_lower.begin(), sender_lower.end(),
                       sender_lower.begin(), ::tolower);
        std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(),
                       ::tolower);

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

void CollaborationPanel::RenderRomSyncEntry(const RomSyncEntry& entry,
                                            int index) {
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
  ImGui::Text("%s - %s (%s)", FormatTimestamp(entry.timestamp).c_str(),
              entry.sender.c_str(), FormatFileSize(entry.diff_size).c_str());

  // Details on hover or if enabled
  if (show_sync_details_ || ImGui::IsItemHovered()) {
    ImGui::Indent();
    ImGui::TextWrapped("ROM Hash: %s", entry.rom_hash.substr(0, 16).c_str());
    if (!entry.error_message.empty()) {
      ImGui::TextColored(colors_.sync_error, "Error: %s",
                         entry.error_message.c_str());
    }
    ImGui::Unindent();
  }

  ImGui::Separator();
  ImGui::PopID();
}

void CollaborationPanel::RenderSnapshotEntry(const SnapshotEntry& entry,
                                             int index) {
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

void CollaborationPanel::RenderProposalEntry(const ProposalEntry& entry,
                                             int index) {
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

void CollaborationPanel::UpdateProposalStatus(const std::string& proposal_id,
                                              const std::string& status) {
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

const char* CollaborationPanel::GetProposalStatusIcon(
    const std::string& status) {
  if (status == "pending")
    return "[◷]";
  if (status == "approved")
    return "[✓]";
  if (status == "rejected")
    return "[✗]";
  if (status == "applied")
    return "[✦]";
  return "[?]";
}

ImVec4 CollaborationPanel::GetProposalStatusColor(const std::string& status) {
  if (status == "pending")
    return colors_.proposal_pending;
  if (status == "approved")
    return colors_.proposal_approved;
  if (status == "rejected")
    return colors_.proposal_rejected;
  if (status == "applied")
    return colors_.proposal_applied;
  return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
}

void CollaborationPanel::RenderVersionHistoryTab() {
  if (!version_mgr_) {
    ImGui::TextWrapped("Version management not initialized");
    return;
  }

  ImGui::TextWrapped("ROM Version History & Protection");
  ImGui::Separator();

  // Stats
  auto stats = version_mgr_->GetStats();
  ImGui::Text("Total Snapshots: %zu", stats.total_snapshots);
  ImGui::SameLine();
  ImGui::TextColored(colors_.sync_applied, "Safe Points: %zu",
                     stats.safe_points);
  ImGui::SameLine();
  ImGui::TextColored(colors_.sync_pending, "Auto-Backups: %zu",
                     stats.auto_backups);

  ImGui::Text("Storage Used: %s",
              FormatFileSize(stats.total_storage_bytes).c_str());

  ImGui::Separator();

  // Toolbar
  if (ImGui::Button("💾 Create Checkpoint")) {
    auto result =
        version_mgr_->CreateSnapshot("Manual checkpoint", "user", true);
    // TODO: Show result in UI
  }
  ImGui::SameLine();
  if (ImGui::Button("🛡️ Mark Current as Safe Point")) {
    std::string current_hash = version_mgr_->GetCurrentHash();
    // TODO: Find snapshot with this hash and mark as safe
  }
  ImGui::SameLine();
  if (ImGui::Button("🔍 Check for Corruption")) {
    auto result = version_mgr_->DetectCorruption();
    // TODO: Show result
  }

  ImGui::Separator();

  // Version list
  if (ImGui::BeginChild("VersionList", ImVec2(0, 0), true)) {
    auto snapshots = version_mgr_->GetSnapshots();

    for (size_t i = 0; i < snapshots.size(); ++i) {
      RenderVersionSnapshot(snapshots[i], i);
    }
  }
  ImGui::EndChild();
}

void CollaborationPanel::RenderApprovalTab() {
  if (!approval_mgr_) {
    ImGui::TextWrapped("Approval management not initialized");
    return;
  }

  ImGui::TextWrapped("Proposal Approval System");
  ImGui::Separator();

  // Pending proposals that need votes
  auto pending = approval_mgr_->GetPendingProposals();

  if (pending.empty()) {
    ImGui::TextWrapped("No proposals pending approval.");
    return;
  }

  ImGui::Text("Pending Proposals: %zu", pending.size());
  ImGui::Separator();

  if (ImGui::BeginChild("ApprovalList", ImVec2(0, 0), true)) {
    for (size_t i = 0; i < pending.size(); ++i) {
      RenderApprovalProposal(pending[i], i);
    }
  }
  ImGui::EndChild();
}

void CollaborationPanel::RenderVersionSnapshot(const net::RomSnapshot& snapshot,
                                               int index) {
  ImGui::PushID(index);

  // Icon based on type
  const char* icon;
  ImVec4 color;

  if (snapshot.is_safe_point) {
    icon = "🛡️";
    color = colors_.sync_applied;
  } else if (snapshot.is_checkpoint) {
    icon = "💾";
    color = colors_.proposal_approved;
  } else {
    icon = "📝";
    color = colors_.sync_pending;
  }

  ImGui::TextColored(color, "%s", icon);
  ImGui::SameLine();

  // Collapsible header
  bool is_open = ImGui::TreeNode(snapshot.description.c_str());

  if (is_open) {
    ImGui::Indent();

    ImGui::Text("Creator: %s", snapshot.creator.c_str());
    ImGui::Text("Time: %s", FormatTimestamp(snapshot.timestamp).c_str());
    ImGui::Text("Hash: %s", snapshot.rom_hash.substr(0, 16).c_str());
    ImGui::Text("Size: %s", FormatFileSize(snapshot.compressed_size).c_str());

    if (snapshot.is_safe_point) {
      ImGui::TextColored(colors_.sync_applied, "✓ Safe Point (Host Verified)");
    }

    ImGui::Separator();

    // Actions
    if (ImGui::Button("↩️ Restore This Version")) {
      auto result = version_mgr_->RestoreSnapshot(snapshot.snapshot_id);
      // TODO: Show result
    }
    ImGui::SameLine();
    if (!snapshot.is_safe_point && ImGui::Button("🛡️ Mark as Safe")) {
      version_mgr_->MarkAsSafePoint(snapshot.snapshot_id);
    }
    ImGui::SameLine();
    if (!snapshot.is_safe_point && ImGui::Button("🗑️ Delete")) {
      version_mgr_->DeleteSnapshot(snapshot.snapshot_id);
    }

    ImGui::Unindent();
    ImGui::TreePop();
  }

  ImGui::Separator();
  ImGui::PopID();
}

void CollaborationPanel::RenderApprovalProposal(
    const net::ProposalApprovalManager::ApprovalStatus& status, int index) {
  ImGui::PushID(index);

  // Status indicator
  ImGui::TextColored(colors_.proposal_pending, "[⏳]");
  ImGui::SameLine();

  // Proposal ID (shortened)
  std::string short_id = status.proposal_id.substr(0, 8);
  bool is_open =
      ImGui::TreeNode(absl::StrFormat("Proposal %s", short_id.c_str()).c_str());

  if (is_open) {
    ImGui::Indent();

    ImGui::Text("Created: %s", FormatTimestamp(status.created_at).c_str());
    ImGui::Text("Snapshot Before: %s",
                status.snapshot_before.substr(0, 8).c_str());

    ImGui::Separator();
    ImGui::TextWrapped("Votes:");

    for (const auto& [username, approved] : status.votes) {
      ImVec4 vote_color =
          approved ? colors_.proposal_approved : colors_.proposal_rejected;
      const char* vote_icon = approved ? "✓" : "✗";
      ImGui::TextColored(vote_color, "  %s %s", vote_icon, username.c_str());
    }

    ImGui::Separator();

    // Voting actions
    if (ImGui::Button("✓ Approve")) {
      // TODO: Send approval vote
      // approval_mgr_->VoteOnProposal(status.proposal_id, "current_user", true);
    }
    ImGui::SameLine();
    if (ImGui::Button("✗ Reject")) {
      // TODO: Send rejection vote
      // approval_mgr_->VoteOnProposal(status.proposal_id, "current_user", false);
    }
    ImGui::SameLine();
    if (ImGui::Button("↩️ Rollback")) {
      // Restore snapshot from before this proposal
      version_mgr_->RestoreSnapshot(status.snapshot_before);
    }

    ImGui::Unindent();
    ImGui::TreePop();
  }

  ImGui::Separator();
  ImGui::PopID();
}

}  // namespace gui

}  // namespace yaze
