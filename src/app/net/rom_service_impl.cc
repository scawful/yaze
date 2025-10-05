#include "app/net/rom_service_impl.h"

#ifdef YAZE_WITH_GRPC

#include "absl/strings/str_format.h"
#include "app/rom.h"
#include "app/net/rom_version_manager.h"

namespace yaze {
namespace app {
namespace net {

RomServiceImpl::RomServiceImpl(
    Rom* rom,
    RomVersionManager* version_manager,
    ProposalApprovalManager* approval_manager)
    : rom_(rom),
      version_manager_(version_manager),
      approval_manager_(approval_manager) {
}

void RomServiceImpl::SetConfig(const Config& config) {
  config_ = config;
}

grpc::Status RomServiceImpl::ReadBytes(
    grpc::ServerContext* context,
    const rom_service::ReadBytesRequest* request,
    rom_service::ReadBytesResponse* response) {
  
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "ROM not loaded");
  }
  
  uint32_t address = request->address();
  uint32_t length = request->length();
  
  // Validate range
  if (address + length > rom_->size()) {
    return grpc::Status(
        grpc::StatusCode::OUT_OF_RANGE,
        absl::StrFormat("Read beyond ROM: 0x%X+%d > %d", 
                       address, length, rom_->size()));
  }
  
  // Read data
  const auto* data = rom_->data() + address;
  response->set_data(data, length);
  response->set_success(true);
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteBytes(
    grpc::ServerContext* context,
    const rom_service::WriteBytesRequest* request,
    rom_service::WriteBytesResponse* response) {
  
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "ROM not loaded");
  }
  
  uint32_t address = request->address();
  const std::string& data = request->data();
  
  // Validate range
  if (address + data.size() > rom_->size()) {
    return grpc::Status(
        grpc::StatusCode::OUT_OF_RANGE,
        absl::StrFormat("Write beyond ROM: 0x%X+%zu > %d",
                       address, data.size(), rom_->size()));
  }
  
  // Check if approval required
  if (config_.require_approval_for_writes && approval_manager_) {
    // Create a proposal for this write
    std::string proposal_id = absl::StrFormat(
        "write_0x%X_%zu_bytes", address, data.size());
    
    if (request->has_proposal_id()) {
      proposal_id = request->proposal_id();
    }
    
    // Check if proposal is approved
    auto status = approval_manager_->GetProposalStatus(proposal_id);
    if (status != ProposalApprovalManager::ApprovalStatus::kApproved) {
      response->set_success(false);
      response->set_message("Write requires approval");
      response->set_proposal_id(proposal_id);
      return grpc::Status::OK;  // Not an error, just needs approval
    }
  }
  
  // Create snapshot before write
  if (version_manager_) {
    std::string snapshot_desc = absl::StrFormat(
        "Before write to 0x%X (%zu bytes)", address, data.size());
    auto snapshot_result = version_manager_->CreateSnapshot(snapshot_desc);
    if (snapshot_result.ok()) {
      response->set_snapshot_id(std::to_string(snapshot_result.value()));
    }
  }
  
  // Perform write
  std::memcpy(rom_->mutable_data() + address, data.data(), data.size());
  
  response->set_success(true);
  response->set_message("Write successful");
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetRomInfo(
    grpc::ServerContext* context,
    const rom_service::GetRomInfoRequest* request,
    rom_service::GetRomInfoResponse* response) {
  
  if (!rom_ || !rom_->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "ROM not loaded");
  }
  
  auto* info = response->mutable_info();
  info->set_title(rom_->title());
  info->set_size(rom_->size());
  info->set_is_loaded(rom_->is_loaded());
  info->set_filename(rom_->filename());
  
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetTileData(
    grpc::ServerContext* context,
    const rom_service::GetTileDataRequest* request,
    rom_service::GetTileDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, 
                     "GetTileData not yet implemented");
}

grpc::Status RomServiceImpl::SetTileData(
    grpc::ServerContext* context,
    const rom_service::SetTileDataRequest* request,
    rom_service::SetTileDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetTileData not yet implemented");
}

grpc::Status RomServiceImpl::GetMapData(
    grpc::ServerContext* context,
    const rom_service::GetMapDataRequest* request,
    rom_service::GetMapDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "GetMapData not yet implemented");
}

grpc::Status RomServiceImpl::SetMapData(
    grpc::ServerContext* context,
    const rom_service::SetMapDataRequest* request,
    rom_service::SetMapDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetMapData not yet implemented");
}

grpc::Status RomServiceImpl::GetSpriteData(
    grpc::ServerContext* context,
    const rom_service::GetSpriteDataRequest* request,
    rom_service::GetSpriteDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "GetSpriteData not yet implemented");
}

grpc::Status RomServiceImpl::SetSpriteData(
    grpc::ServerContext* context,
    const rom_service::SetSpriteDataRequest* request,
    rom_service::SetSpriteDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetSpriteData not yet implemented");
}

grpc::Status RomServiceImpl::GetDialogue(
    grpc::ServerContext* context,
    const rom_service::GetDialogueRequest* request,
    rom_service::GetDialogueResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "GetDialogue not yet implemented");
}

grpc::Status RomServiceImpl::SetDialogue(
    grpc::ServerContext* context,
    const rom_service::SetDialogueRequest* request,
    rom_service::SetDialogueResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetDialogue not yet implemented");
}

}  // namespace net
}  // namespace app
}  // namespace yaze

#endif  // YAZE_WITH_GRPC