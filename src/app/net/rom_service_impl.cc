#include "app/net/rom_service_impl.h"

#ifdef YAZE_WITH_GRPC

#include "absl/strings/str_format.h"
#include "app/rom.h"
#include "app/net/rom_version_manager.h"

// Proto namespace alias for convenience
namespace rom_svc = ::yaze::proto;

namespace yaze {

namespace net {

RomServiceImpl::RomServiceImpl(
    Rom* rom,
    RomVersionManager* version_manager,
    ProposalApprovalManager* approval_manager)
    : rom_(rom),
      version_mgr_(version_manager),
      approval_mgr_(approval_manager) {
}

void RomServiceImpl::SetConfig(const Config& config) {
  config_ = config;
}

grpc::Status RomServiceImpl::ReadBytes(
    grpc::ServerContext* context,
    const rom_svc::ReadBytesRequest* request,
    rom_svc::ReadBytesResponse* response) {
  
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
    const rom_svc::WriteBytesRequest* request,
    rom_svc::WriteBytesResponse* response) {
  
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
  if (config_.require_approval_for_writes && approval_mgr_) {
    // Create a proposal for this write
    std::string proposal_id = absl::StrFormat(
        "write_0x%X_%zu_bytes", address, data.size());
    
    if (request->has_proposal_id()) {
      proposal_id = request->proposal_id();
    }
    
    // Check if proposal is approved
    auto status = approval_mgr_->GetProposalStatus(proposal_id);
    if (status != ProposalApprovalManager::ApprovalStatus::kApproved) {
      response->set_success(false);
      response->set_message("Write requires approval");
      response->set_proposal_id(proposal_id);
      return grpc::Status::OK;  // Not an error, just needs approval
    }
  }
  
  // Create snapshot before write
  if (version_mgr_) {
    std::string snapshot_desc = absl::StrFormat(
        "Before write to 0x%X (%zu bytes)", address, data.size());
    auto snapshot_result = version_mgr_->CreateSnapshot(snapshot_desc);
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
    const rom_svc::GetRomInfoRequest* request,
    rom_svc::GetRomInfoResponse* response) {
  
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
    const rom_svc::GetTileDataRequest* request,
    rom_svc::GetTileDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, 
                     "GetTileData not yet implemented");
}

grpc::Status RomServiceImpl::SetTileData(
    grpc::ServerContext* context,
    const rom_svc::SetTileDataRequest* request,
    rom_svc::SetTileDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetTileData not yet implemented");
}

grpc::Status RomServiceImpl::GetMapData(
    grpc::ServerContext* context,
    const rom_svc::GetMapDataRequest* request,
    rom_svc::GetMapDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "GetMapData not yet implemented");
}

grpc::Status RomServiceImpl::SetMapData(
    grpc::ServerContext* context,
    const rom_svc::SetMapDataRequest* request,
    rom_svc::SetMapDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetMapData not yet implemented");
}

grpc::Status RomServiceImpl::GetSpriteData(
    grpc::ServerContext* context,
    const rom_svc::GetSpriteDataRequest* request,
    rom_svc::GetSpriteDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "GetSpriteData not yet implemented");
}

grpc::Status RomServiceImpl::SetSpriteData(
    grpc::ServerContext* context,
    const rom_svc::SetSpriteDataRequest* request,
    rom_svc::SetSpriteDataResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetSpriteData not yet implemented");
}

grpc::Status RomServiceImpl::GetDialogue(
    grpc::ServerContext* context,
    const rom_svc::GetDialogueRequest* request,
    rom_svc::GetDialogueResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "GetDialogue not yet implemented");
}

grpc::Status RomServiceImpl::SetDialogue(
    grpc::ServerContext* context,
    const rom_svc::SetDialogueRequest* request,
    rom_svc::SetDialogueResponse* response) {
  
  return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
                     "SetDialogue not yet implemented");
}

}  // namespace net

}  // namespace yaze

#endif  // YAZE_WITH_GRPC