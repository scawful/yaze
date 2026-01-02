#include "app/service/rom_service_impl.h"

#ifdef YAZE_WITH_GRPC

#include "absl/strings/str_format.h"
#include "app/net/rom_version_manager.h"
#include "rom/rom.h"

// Proto namespace alias for convenience
namespace rom_svc = ::yaze::proto;

namespace yaze {

namespace net {

RomServiceImpl::RomServiceImpl(RomGetter rom_getter,
                               RomVersionManager* version_manager,
                               ProposalApprovalManager* approval_manager)
    : rom_getter_(rom_getter),
      version_mgr_(version_manager),
      approval_mgr_(approval_manager) {}

void RomServiceImpl::SetConfig(const Config& config) {
  config_ = config;
}

grpc::Status RomServiceImpl::ReadBytes(grpc::ServerContext* context,
                                       const rom_svc::ReadBytesRequest* request,
                                       rom_svc::ReadBytesResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  uint32_t offset = request->offset();
  uint32_t length = request->length();

  // Validate range
  if (offset + length > rom->size()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        absl::StrFormat("Read beyond ROM: 0x%X+%d > %d", offset,
                                        length, rom->size()));
  }

  // Read data
  const auto* data = rom->data() + offset;
  response->set_data(data, length);

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteBytes(
    grpc::ServerContext* context, const rom_svc::WriteBytesRequest* request,
    rom_svc::WriteBytesResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  uint32_t offset = request->offset();
  const std::string& data = request->data();

  // Validate range
  if (offset + data.size() > rom->size()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        absl::StrFormat("Write beyond ROM: 0x%X+%zu > %d",
                                        offset, data.size(), rom->size()));
  }

  if (config_.require_approval_for_writes) {
    return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                        "Direct ROM writes disabled; use proposal system");
  }

  // Create auto-snapshot if enabled
  auto status = MaybeCreateSnapshot(absl::StrFormat(
      "Auto-snapshot before write at 0x%X (%zu bytes)", offset, data.size()));
  if (!status.ok()) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        "Failed to create safety snapshot: " + std::string(status.message()));
  }

  // Perform the write
  std::memcpy(rom->mutable_data() + offset, data.data(), data.size());
  response->set_success(true);

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetRomInfo(
    grpc::ServerContext* context, const rom_svc::GetRomInfoRequest* request,
    rom_svc::GetRomInfoResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    response->set_title("ROM not loaded");
    return grpc::Status::OK;
  }

  response->set_title(rom->title());
  response->set_size(rom->size());
  // Removed checksum, is_expanded, version as they don't exist in yaze::Rom

  return grpc::Status::OK;
}

// ... Stubs for other methods to keep it building ...
grpc::Status RomServiceImpl::ReadOverworldMap(
    grpc::ServerContext* context,
    const rom_svc::ReadOverworldMapRequest* request,
    rom_svc::ReadOverworldMapResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::WriteOverworldTile(
    grpc::ServerContext* context,
    const rom_svc::WriteOverworldTileRequest* request,
    rom_svc::WriteOverworldTileResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::ReadDungeonRoom(
    grpc::ServerContext* context,
    const rom_svc::ReadDungeonRoomRequest* request,
    rom_svc::ReadDungeonRoomResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::WriteDungeonTile(
    grpc::ServerContext* context,
    const rom_svc::WriteDungeonTileRequest* request,
    rom_svc::WriteDungeonTileResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::ReadSprite(
    grpc::ServerContext* context, const rom_svc::ReadSpriteRequest* request,
    rom_svc::ReadSpriteResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::SubmitRomProposal(
    grpc::ServerContext* context,
    const rom_svc::SubmitRomProposalRequest* request,
    rom_svc::SubmitRomProposalResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::GetProposalStatus(
    grpc::ServerContext* context,
    const rom_svc::GetProposalStatusRequest* request,
    rom_svc::GetProposalStatusResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::CreateSnapshot(
    grpc::ServerContext* context, const rom_svc::CreateSnapshotRequest* request,
    rom_svc::CreateSnapshotResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::RestoreSnapshot(
    grpc::ServerContext* context,
    const rom_svc::RestoreSnapshotRequest* request,
    rom_svc::RestoreSnapshotResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}
grpc::Status RomServiceImpl::ListSnapshots(
    grpc::ServerContext* context, const rom_svc::ListSnapshotsRequest* request,
    rom_svc::ListSnapshotsResponse* response) {
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not implemented");
}

grpc::Status RomServiceImpl::ValidateRomLoaded() {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }
  return grpc::Status::OK;
}

absl::Status RomServiceImpl::MaybeCreateSnapshot(
    const std::string& description) {
  if (!config_.enable_version_management || !version_mgr_) {
    return absl::OkStatus();
  }
  return version_mgr_->CreateSnapshot(description, "gRPC", false).status();
}

}  // namespace net

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
