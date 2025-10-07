#include "app/net/collaboration_service.h"

#include <chrono>
#include <thread>

#include "absl/strings/str_format.h"

namespace yaze {

namespace net {

CollaborationService::CollaborationService(Rom* rom)
    : rom_(rom),
      version_mgr_(nullptr),
      approval_mgr_(nullptr),
      client_(std::make_unique<WebSocketClient>()),
      sync_in_progress_(false) {
}

CollaborationService::~CollaborationService() {
  Disconnect();
}

absl::Status CollaborationService::Initialize(
    const Config& config,
    RomVersionManager* version_mgr,
    ProposalApprovalManager* approval_mgr) {
  
  config_ = config;
  version_mgr_ = version_mgr;
  approval_mgr_ = approval_mgr;
  
  if (!version_mgr_) {
    return absl::InvalidArgumentError("version_mgr cannot be null");
  }
  
  if (!approval_mgr_) {
    return absl::InvalidArgumentError("approval_mgr cannot be null");
  }
  
  // Set up network event callbacks
  client_->OnMessage("rom_sync", [this](const nlohmann::json& payload) {
    OnRomSyncReceived(payload);
  });
  
  client_->OnMessage("proposal_shared", [this](const nlohmann::json& payload) {
    OnProposalReceived(payload);
  });
  
  client_->OnMessage("proposal_vote_received", [this](const nlohmann::json& payload) {
    OnProposalUpdated(payload);
  });
  
  client_->OnMessage("proposal_updated", [this](const nlohmann::json& payload) {
    OnProposalUpdated(payload);
  });
  
  client_->OnMessage("participant_joined", [this](const nlohmann::json& payload) {
    OnParticipantJoined(payload);
  });
  
  client_->OnMessage("participant_left", [this](const nlohmann::json& payload) {
    OnParticipantLeft(payload);
  });
  
  // Store initial ROM hash
  if (rom_ && rom_->is_loaded()) {
    last_sync_hash_ = version_mgr_->GetCurrentHash();
  }
  
  return absl::OkStatus();
}

absl::Status CollaborationService::Connect(const std::string& host, int port) {
  return client_->Connect(host, port);
}

void CollaborationService::Disconnect() {
  if (client_->IsConnected()) {
    client_->Disconnect();
  }
}

absl::Status CollaborationService::HostSession(
    const std::string& session_name,
    const std::string& username,
    bool ai_enabled) {
  
  if (!client_->IsConnected()) {
    return absl::FailedPreconditionError("Not connected to server");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Get current ROM hash
  std::string rom_hash = version_mgr_->GetCurrentHash();
  
  // Create initial safe point
  auto snapshot_result = version_mgr_->CreateSnapshot(
      "Session start",
      username,
      true  // is_checkpoint
  );
  
  if (snapshot_result.ok()) {
    version_mgr_->MarkAsSafePoint(*snapshot_result);
  }
  
  // Host session on server
  auto session_result = client_->HostSession(
      session_name,
      username,
      rom_hash,
      ai_enabled
  );
  
  if (!session_result.ok()) {
    return session_result.status();
  }
  
  last_sync_hash_ = rom_hash;
  
  return absl::OkStatus();
}

absl::Status CollaborationService::JoinSession(
    const std::string& session_code,
    const std::string& username) {
  
  if (!client_->IsConnected()) {
    return absl::FailedPreconditionError("Not connected to server");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Create backup before joining
  auto snapshot_result = version_mgr_->CreateSnapshot(
      "Before joining session",
      username,
      true
  );
  
  if (snapshot_result.ok()) {
    version_mgr_->MarkAsSafePoint(*snapshot_result);
  }
  
  // Join session
  auto session_result = client_->JoinSession(session_code, username);
  
  if (!session_result.ok()) {
    return session_result.status();
  }
  
  last_sync_hash_ = version_mgr_->GetCurrentHash();
  
  return absl::OkStatus();
}

absl::Status CollaborationService::LeaveSession() {
  if (!client_->InSession()) {
    return absl::FailedPreconditionError("Not in a session");
  }
  
  return client_->LeaveSession();
}

absl::Status CollaborationService::SubmitChangesAsProposal(
    const std::string& description,
    const std::string& username) {
  
  if (!client_->InSession()) {
    return absl::FailedPreconditionError("Not in a session");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Generate diff from last sync
  std::string current_hash = version_mgr_->GetCurrentHash();
  if (current_hash == last_sync_hash_) {
    return absl::OkStatus();  // No changes to submit
  }
  
  std::string diff = GenerateDiff(last_sync_hash_, current_hash);
  
  // Create proposal data
  nlohmann::json proposal_data = {
    {"description", description},
    {"type", "rom_modification"},
    {"diff_data", diff},
    {"from_hash", last_sync_hash_},
    {"to_hash", current_hash}
  };
  
  // Submit to server
  auto status = client_->ShareProposal(proposal_data, username);
  
  if (status.ok() && config_.require_approval_for_sync) {
    // Proposal submitted, waiting for approval
    // The actual application will happen when approved
  }
  
  return status;
}

absl::Status CollaborationService::ApplyRomSync(
    const std::string& diff_data,
    const std::string& rom_hash,
    const std::string& sender) {
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  if (sync_in_progress_) {
    return absl::UnavailableError("Sync already in progress");
  }
  
  sync_in_progress_ = true;
  
  // Create snapshot before applying
  if (config_.create_snapshot_before_sync) {
    auto snapshot_result = version_mgr_->CreateSnapshot(
        absl::StrFormat("Before sync from %s", sender),
        "system",
        false
    );
    
    if (!snapshot_result.ok()) {
      sync_in_progress_ = false;
      return absl::InternalError("Failed to create backup snapshot");
    }
  }
  
  // Apply the diff
  auto status = ApplyDiff(diff_data);
  
  if (status.ok()) {
    last_sync_hash_ = rom_hash;
  } else {
    // Rollback on error
    if (config_.create_snapshot_before_sync) {
      auto snapshots = version_mgr_->GetSnapshots();
      if (!snapshots.empty()) {
        version_mgr_->RestoreSnapshot(snapshots[0].snapshot_id);
      }
    }
  }
  
  sync_in_progress_ = false;
  return status;
}

absl::Status CollaborationService::HandleIncomingProposal(
    const std::string& proposal_id,
    const nlohmann::json& proposal_data,
    const std::string& sender) {
  
  if (!approval_mgr_) {
    return absl::FailedPreconditionError("Approval manager not initialized");
  }
  
  // Submit to approval manager
  return approval_mgr_->SubmitProposal(
      proposal_id,
      sender,
      proposal_data["description"],
      proposal_data
  );
}

absl::Status CollaborationService::VoteOnProposal(
    const std::string& proposal_id,
    bool approved,
    const std::string& username) {
  
  if (!client_->InSession()) {
    return absl::FailedPreconditionError("Not in a session");
  }
  
  // Vote locally
  auto status = approval_mgr_->VoteOnProposal(proposal_id, username, approved);
  
  if (!status.ok()) {
    return status;
  }
  
  // Send vote to server
  return client_->VoteOnProposal(proposal_id, approved, username);
}

absl::Status CollaborationService::ApplyApprovedProposal(
    const std::string& proposal_id) {
  
  if (!approval_mgr_->IsProposalApproved(proposal_id)) {
    return absl::FailedPreconditionError("Proposal not approved");
  }
  
  auto proposal_result = approval_mgr_->GetProposalStatus(proposal_id);
  if (!proposal_result.ok()) {
    return proposal_result.status();
  }
  
  // Apply the proposal (implementation depends on proposal type)
  // For now, just update status
  auto status = client_->UpdateProposalStatus(proposal_id, "applied");
  
  if (status.ok()) {
    // Create snapshot after applying
    version_mgr_->CreateSnapshot(
        absl::StrFormat("Applied proposal %s", proposal_id.substr(0, 8)),
        "system",
        false
    );
  }
  
  return status;
}

bool CollaborationService::IsConnected() const {
  return client_->IsConnected();
}

absl::StatusOr<SessionInfo> CollaborationService::GetSessionInfo() const {
  return client_->GetSessionInfo();
}

void CollaborationService::SetAutoSync(bool enabled) {
  config_.auto_sync_enabled = enabled;
}

// Private callback handlers

void CollaborationService::OnRomSyncReceived(const nlohmann::json& payload) {
  std::string diff_data = payload["diff_data"];
  std::string rom_hash = payload["rom_hash"];
  std::string sender = payload["sender"];
  
  auto status = ApplyRomSync(diff_data, rom_hash, sender);
  
  if (!status.ok()) {
    // Log error or notify user
  }
}

void CollaborationService::OnProposalReceived(const nlohmann::json& payload) {
  std::string proposal_id = payload["proposal_id"];
  nlohmann::json proposal_data = payload["proposal_data"];
  std::string sender = payload["sender"];
  
  HandleIncomingProposal(proposal_id, proposal_data, sender);
}

void CollaborationService::OnProposalUpdated(const nlohmann::json& payload) {
  std::string proposal_id = payload["proposal_id"];
  
  if (payload.contains("status")) {
    std::string status = payload["status"];
    
    if (status == "approved" && approval_mgr_) {
      // Proposal was approved, consider applying it
      // This would be triggered by the host or based on voting results
    }
  }
  
  if (payload.contains("votes")) {
    // Vote update received
    nlohmann::json votes = payload["votes"];
    // Update local approval manager state
  }
}

void CollaborationService::OnParticipantJoined(const nlohmann::json& payload) {
  std::string username = payload["username"];
  // Update participant list or notify user
}

void CollaborationService::OnParticipantLeft(const nlohmann::json& payload) {
  std::string username = payload["username"];
  // Update participant list or notify user
}

// Helper functions

std::string CollaborationService::GenerateDiff(
    const std::string& from_hash,
    const std::string& to_hash) {
  
  // Simplified diff generation
  // In production, this would generate a binary diff
  // For now, just return placeholder
  
  if (!rom_ || !rom_->is_loaded()) {
    return "";
  }
  
  // TODO: Implement proper binary diff generation
  // This could use algorithms like bsdiff or a custom format
  
  return "diff_placeholder";
}

absl::Status CollaborationService::ApplyDiff(const std::string& diff_data) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // TODO: Implement proper diff application
  // For now, just return success
  
  return absl::OkStatus();
}

bool CollaborationService::ShouldAutoSync() {
  if (!config_.auto_sync_enabled) {
    return false;
  }
  
  if (!client_->IsConnected() || !client_->InSession()) {
    return false;
  }
  
  if (sync_in_progress_) {
    return false;
  }
  
  // Check if enough time has passed since last sync
  // (Implementation would track last sync time)
  
  return true;
}

}  // namespace net

}  // namespace yaze
