#include "app/service/rom_service_impl.h"

#ifdef YAZE_WITH_GRPC

#include "absl/strings/str_format.h"
#include "app/net/rom_version_manager.h"
#include "rom/rom.h"

// Proto namespace alias for convenience
namespace rom_svc = ::yaze::proto;

namespace yaze {

namespace net {

RomServiceImpl::RomServiceImpl(Rom* rom, RomVersionManager* version_manager,
                               ProposalApprovalManager* approval_manager)
    : rom_(rom),
      version_mgr_(version_manager),
      approval_mgr_(approval_manager) {}

void RomServiceImpl::SetConfig(const Config& config) {
  config_ = config;
}

grpc::Status RomServiceImpl::ReadBytes(grpc::ServerContext* context,
                                       const rom_svc::ReadBytesRequest* request,
                                       rom_svc::ReadBytesResponse* response) {
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  uint32_t offset = request->offset();
  uint32_t length = request->length();

  // Validate range
  if (offset + length > rom_->size()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        absl::StrFormat("Read beyond ROM: 0x%X+%d > %d",
                                        offset, length, rom_->size()));
  }

  // Read data
  const auto* data = rom_->data() + offset;
  response->set_data(data, length);

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteBytes(
    grpc::ServerContext* context, const rom_svc::WriteBytesRequest* request,
    rom_svc::WriteBytesResponse* response) {
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  uint32_t offset = request->offset();
  const std::string& data = request->data();

  // Validate range
  if (offset + data.size() > rom_->size()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        absl::StrFormat("Write beyond ROM: 0x%X+%zu > %d",
                                        offset, data.size(), rom_->size()));
  }

  // Check if approval required
  if (config_.require_approval_for_writes && approval_mgr_) {
    // Create a proposal for this write
    std::string proposal_id =
        absl::StrFormat("write_0x%X_%zu_bytes", offset, data.size());

    // Check if proposal is approved
    if (!approval_mgr_->IsProposalApproved(proposal_id)) {
      response->set_success(false);
      response->set_error("Write requires approval");
      response->set_proposal_id(proposal_id);
      return grpc::Status::OK;  // Not an error, just needs approval
    }
  }

  // Create snapshot before write
  if (version_mgr_) {
    std::string snapshot_desc = absl::StrFormat(
        "Before write to 0x%X (%zu bytes)", offset, data.size());
    // Creator is "system" for now, could be passed in context
    version_mgr_->CreateSnapshot(snapshot_desc, "system");
  }

  // Perform write
  std::memcpy(rom_->mutable_data() + offset, data.data(), data.size());

  response->set_success(true);

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetRomInfo(
    grpc::ServerContext* context, const rom_svc::GetRomInfoRequest* request,
    rom_svc::GetRomInfoResponse* response) {
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  response->set_title(rom_->title());
  response->set_size(rom_->size());
  // response->set_is_loaded(rom_->is_loaded()); // Not in proto
  // response->set_filename(rom_->filename()); // Not in proto
  // Proto has: title, size, checksum, is_expanded, version

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::ReadOverworldMap(
    grpc::ServerContext* context, const rom_svc::ReadOverworldMapRequest* request,
    rom_svc::ReadOverworldMapResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::ReadDungeonRoom(
    grpc::ServerContext* context, const rom_svc::ReadDungeonRoomRequest* request,
    rom_svc::ReadDungeonRoomResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::ReadSprite(
    grpc::ServerContext* context, const rom_svc::ReadSpriteRequest* request,
    rom_svc::ReadSpriteResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::WriteOverworldTile(
    grpc::ServerContext* context, const rom_svc::WriteOverworldTileRequest* request,
    rom_svc::WriteOverworldTileResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::WriteDungeonTile(
    grpc::ServerContext* context, const rom_svc::WriteDungeonTileRequest* request,
    rom_svc::WriteDungeonTileResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::SubmitRomProposal(
    grpc::ServerContext* context, const rom_svc::SubmitRomProposalRequest* request,
    rom_svc::SubmitRomProposalResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::GetProposalStatus(
    grpc::ServerContext* context, const rom_svc::GetProposalStatusRequest* request,
    rom_svc::GetProposalStatusResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::CreateSnapshot(
    grpc::ServerContext* context, const rom_svc::CreateSnapshotRequest* request,
    rom_svc::CreateSnapshotResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::RestoreSnapshot(
    grpc::ServerContext* context, const rom_svc::RestoreSnapshotRequest* request,
    rom_svc::RestoreSnapshotResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::ListSnapshots(
    grpc::ServerContext* context, const rom_svc::ListSnapshotsRequest* request,
    rom_svc::ListSnapshotsResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

}  // namespace net

}  // namespace yaze

#endif  // YAZE_WITH_GRPC