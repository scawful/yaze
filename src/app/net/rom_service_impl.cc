#include "app/net/rom_service_impl.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace net {

#ifdef YAZE_WITH_GRPC

RomServiceImpl::RomServiceImpl(
    Rom* rom,
    RomVersionManager* version_mgr,
    ProposalApprovalManager* approval_mgr)
    : rom_(rom),
      version_mgr_(version_mgr),
      approval_mgr_(approval_mgr) {
  
  // Set default config
  config_.require_approval_for_writes = (approval_mgr != nullptr);
  config_.enable_version_management = (version_mgr != nullptr);
}

// ============================================================================
// Basic ROM Operations
// ============================================================================

grpc::Status RomServiceImpl::ReadBytes(
    grpc::ServerContext* context,
    const proto::ReadBytesRequest* request,
    proto::ReadBytesResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  uint32_t offset = request->offset();
  uint32_t length = request->length();
  
  // Validate bounds
  if (length > config_.max_read_size_bytes) {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT,
        absl::StrFormat("Read size %d exceeds maximum %d",
                        length, config_.max_read_size_bytes));
  }
  
  if (offset + length > rom_->size()) {
    return grpc::Status(
        grpc::StatusCode::OUT_OF_RANGE,
        "Read would exceed ROM bounds");
  }
  
  // Read data
  const uint8_t* rom_data = rom_->data();
  response->set_data(reinterpret_cast<const char*>(rom_data + offset), length);
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteBytes(
    grpc::ServerContext* context,
    const proto::WriteBytesRequest* request,
    proto::WriteBytesResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  uint32_t offset = request->offset();
  const std::string& data = request->data();
  
  // Validate bounds
  if (offset + data.size() > rom_->size()) {
    response->set_success(false);
    response->set_error("Write would exceed ROM bounds");
    return grpc::Status::OK;
  }
  
  // Check if approval required
  if (config_.require_approval_for_writes || request->require_approval()) {
    // TODO: Submit as proposal
    response->set_success(false);
    response->set_error("Proposal submission not yet implemented");
    return grpc::Status::OK;
  }
  
  // Create snapshot before write
  if (config_.enable_version_management && version_mgr_) {
    auto snapshot_status = MaybeCreateSnapshot(
        absl::StrFormat("gRPC write at 0x%X (%d bytes)", offset, data.size()));
    
    if (!snapshot_status.ok()) {
      response->set_success(false);
      response->set_error("Failed to create backup snapshot");
      return grpc::Status::OK;
    }
  }
  
  // Perform write
  uint8_t* rom_data = rom_->mutable_data();
  std::memcpy(rom_data + offset, data.data(), data.size());
  
  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetRomInfo(
    grpc::ServerContext* context,
    const proto::GetRomInfoRequest* request,
    proto::GetRomInfoResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  response->set_title(rom_->title());
  response->set_size(rom_->size());
  response->set_is_expanded(rom_->is_expanded());
  
  // Calculate checksum if available
  if (version_mgr_) {
    response->set_checksum(version_mgr_->GetCurrentHash());
  }
  
  return grpc::Status::OK;
}

// ============================================================================
// Overworld Operations
// ============================================================================

grpc::Status RomServiceImpl::ReadOverworldMap(
    grpc::ServerContext* context,
    const proto::ReadOverworldMapRequest* request,
    proto::ReadOverworldMapResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  uint32_t map_id = request->map_id();
  
  if (map_id >= 160) {
    response->set_error("Invalid map ID (must be 0-159)");
    return grpc::Status::OK;
  }
  
  // TODO: Read actual overworld map data
  // For now, return placeholder
  response->set_map_id(map_id);
  response->set_error("Not yet implemented");
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteOverworldTile(
    grpc::ServerContext* context,
    const proto::WriteOverworldTileRequest* request,
    proto::WriteOverworldTileResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  // Validate coordinates
  if (request->x() >= 32 || request->y() >= 32) {
    response->set_success(false);
    response->set_error("Invalid tile coordinates (must be 0-31)");
    return grpc::Status::OK;
  }
  
  if (request->map_id() >= 160) {
    response->set_success(false);
    response->set_error("Invalid map ID (must be 0-159)");
    return grpc::Status::OK;
  }
  
  // TODO: Implement actual overworld tile writing
  response->set_success(false);
  response->set_error("Not yet implemented");
  
  return grpc::Status::OK;
}

// ============================================================================
// Dungeon Operations
// ============================================================================

grpc::Status RomServiceImpl::ReadDungeonRoom(
    grpc::ServerContext* context,
    const proto::ReadDungeonRoomRequest* request,
    proto::ReadDungeonRoomResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  uint32_t room_id = request->room_id();
  
  if (room_id >= 296) {
    response->set_error("Invalid room ID (must be 0-295)");
    return grpc::Status::OK;
  }
  
  // TODO: Read actual dungeon room data
  response->set_room_id(room_id);
  response->set_error("Not yet implemented");
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteDungeonTile(
    grpc::ServerContext* context,
    const proto::WriteDungeonTileRequest* request,
    proto::WriteDungeonTileResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  // TODO: Implement dungeon tile writing
  response->set_success(false);
  response->set_error("Not yet implemented");
  
  return grpc::Status::OK;
}

// ============================================================================
// Sprite Operations
// ============================================================================

grpc::Status RomServiceImpl::ReadSprite(
    grpc::ServerContext* context,
    const proto::ReadSpriteRequest* request,
    proto::ReadSpriteResponse* response) {
  
  auto status = ValidateRomLoaded();
  if (!status.ok()) {
    return status;
  }
  
  // TODO: Implement sprite reading
  response->set_error("Not yet implemented");
  
  return grpc::Status::OK;
}

// ============================================================================
// Proposal System
// ============================================================================

grpc::Status RomServiceImpl::SubmitRomProposal(
    grpc::ServerContext* context,
    const proto::SubmitRomProposalRequest* request,
    proto::SubmitRomProposalResponse* response) {
  
  if (!approval_mgr_) {
    response->set_success(false);
    response->set_error("Proposal system not enabled");
    return grpc::Status::OK;
  }
  
  // TODO: Implement proposal submission
  response->set_success(false);
  response->set_error("Not yet implemented");
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetProposalStatus(
    grpc::ServerContext* context,
    const proto::GetProposalStatusRequest* request,
    proto::GetProposalStatusResponse* response) {
  
  if (!approval_mgr_) {
    return grpc::Status(
        grpc::StatusCode::FAILED_PRECONDITION,
        "Proposal system not enabled");
  }
  
  std::string proposal_id = request->proposal_id();
  
  auto status_result = approval_mgr_->GetProposalStatus(proposal_id);
  if (!status_result.ok()) {
    return grpc::Status(
        grpc::StatusCode::NOT_FOUND,
        "Proposal not found");
  }
  
  const auto& status_info = *status_result;
  response->set_proposal_id(proposal_id);
  response->set_status(status_info.status);
  
  // TODO: Add vote information
  
  return grpc::Status::OK;
}

// ============================================================================
// Version Management
// ============================================================================

grpc::Status RomServiceImpl::CreateSnapshot(
    grpc::ServerContext* context,
    const proto::CreateSnapshotRequest* request,
    proto::CreateSnapshotResponse* response) {
  
  if (!version_mgr_) {
    response->set_success(false);
    response->set_error("Version management not enabled");
    return grpc::Status::OK;
  }
  
  auto snapshot_result = version_mgr_->CreateSnapshot(
      request->description(),
      request->username(),
      request->is_checkpoint()
  );
  
  if (snapshot_result.ok()) {
    response->set_success(true);
    response->set_snapshot_id(*snapshot_result);
  } else {
    response->set_success(false);
    response->set_error(std::string(snapshot_result.status().message()));
  }
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::RestoreSnapshot(
    grpc::ServerContext* context,
    const proto::RestoreSnapshotRequest* request,
    proto::RestoreSnapshotResponse* response) {
  
  if (!version_mgr_) {
    response->set_success(false);
    response->set_error("Version management not enabled");
    return grpc::Status::OK;
  }
  
  auto status = version_mgr_->RestoreSnapshot(request->snapshot_id());
  
  if (status.ok()) {
    response->set_success(true);
  } else {
    response->set_success(false);
    response->set_error(std::string(status.message()));
  }
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::ListSnapshots(
    grpc::ServerContext* context,
    const proto::ListSnapshotsRequest* request,
    proto::ListSnapshotsResponse* response) {
  
  if (!version_mgr_) {
    response->set_error("Version management not enabled");
    return grpc::Status::OK;
  }
  
  auto snapshots = version_mgr_->GetSnapshots();
  
  uint32_t max_results = request->max_results();
  if (max_results == 0) {
    max_results = snapshots.size();
  }
  
  for (size_t i = 0; i < std::min(max_results, static_cast<uint32_t>(snapshots.size())); ++i) {
    const auto& snapshot = snapshots[i];
    
    auto* info = response->add_snapshots();
    info->set_snapshot_id(snapshot.snapshot_id);
    info->set_description(snapshot.description);
    info->set_username(snapshot.username);
    info->set_timestamp(snapshot.timestamp);
    info->set_is_checkpoint(snapshot.is_checkpoint);
    info->set_is_safe_point(snapshot.is_safe_point);
    info->set_size_bytes(snapshot.compressed_size);
  }
  
  return grpc::Status::OK;
}

// ============================================================================
// Private Helpers
// ============================================================================

grpc::Status RomServiceImpl::ValidateRomLoaded() {
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(
        grpc::StatusCode::FAILED_PRECONDITION,
        "ROM not loaded");
  }
  return grpc::Status::OK;
}

absl::Status RomServiceImpl::MaybeCreateSnapshot(
    const std::string& description) {
  
  if (!version_mgr_) {
    return absl::OkStatus();
  }
  
  auto snapshot_result = version_mgr_->CreateSnapshot(
      description,
      "grpc_service",
      false  // not a checkpoint
  );
  
  return snapshot_result.status();
}

#endif  // YAZE_WITH_GRPC

}  // namespace net
}  // namespace app
}  // namespace yaze
