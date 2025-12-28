#ifndef YAZE_APP_NET_ROM_SERVICE_IMPL_H_
#define YAZE_APP_NET_ROM_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#ifdef YAZE_WITH_GRPC
#ifdef _WIN32
#pragma push_macro("DWORD")
#pragma push_macro("ERROR")
#undef DWORD
#undef ERROR
#endif  // _WIN32
#include <grpcpp/grpcpp.h>

#include "protos/rom_service.grpc.pb.h"
#ifdef _WIN32
#pragma pop_macro("DWORD")
#pragma pop_macro("ERROR")
#endif  // _WIN32
// Note: Proto files will be generated to build directory
#endif

#include <functional>
#include "app/net/rom_version_manager.h"
#include "rom/rom.h"

namespace yaze {

namespace net {

#ifdef YAZE_WITH_GRPC

/**
 * @brief gRPC service implementation for remote ROM manipulation
 */
class RomServiceImpl final : public proto::RomService::Service {
 public:
  using RomGetter = std::function<Rom*()>;

  struct Config {
    bool require_approval_for_writes = true;
    bool enable_version_management = true;
    int max_read_size_bytes = 1024 * 1024;
    bool allow_raw_rom_access = true;
  };

  /**
   * @brief Construct ROM service
   * @param rom_getter Function to retrieve the active ROM instance
   * @param version_mgr Pointer to version manager (not owned, optional)
   * @param approval_mgr Pointer to approval manager (not owned, optional)
   */
  RomServiceImpl(RomGetter rom_getter, RomVersionManager* version_mgr = nullptr,
                 ProposalApprovalManager* approval_mgr = nullptr);

  ~RomServiceImpl() override = default;

  // Initialize with configuration
  void SetConfig(const Config& config);

  // =========================================================================
  // Basic ROM Operations
  // =========================================================================

  grpc::Status ReadBytes(grpc::ServerContext* context,
                         const proto::ReadBytesRequest* request,
                         proto::ReadBytesResponse* response) override;

  grpc::Status WriteBytes(grpc::ServerContext* context,
                          const proto::WriteBytesRequest* request,
                          proto::WriteBytesResponse* response) override;

  grpc::Status GetRomInfo(grpc::ServerContext* context,
                          const proto::GetRomInfoRequest* request,
                          proto::GetRomInfoResponse* response) override;

  // =========================================================================
  // Overworld Operations
  // =========================================================================

  grpc::Status ReadOverworldMap(
      grpc::ServerContext* context,
      const proto::ReadOverworldMapRequest* request,
      proto::ReadOverworldMapResponse* response) override;

  grpc::Status WriteOverworldTile(
      grpc::ServerContext* context,
      const proto::WriteOverworldTileRequest* request,
      proto::WriteOverworldTileResponse* response) override;

  // =========================================================================
  // Dungeon Operations
  // =========================================================================

  grpc::Status ReadDungeonRoom(
      grpc::ServerContext* context,
      const proto::ReadDungeonRoomRequest* request,
      proto::ReadDungeonRoomResponse* response) override;

  grpc::Status WriteDungeonTile(
      grpc::ServerContext* context,
      const proto::WriteDungeonTileRequest* request,
      proto::WriteDungeonTileResponse* response) override;

  // =========================================================================
  // Sprite Operations
  // =========================================================================

  grpc::Status ReadSprite(grpc::ServerContext* context,
                          const proto::ReadSpriteRequest* request,
                          proto::ReadSpriteResponse* response) override;

  // =========================================================================
  // Proposal System
  // =========================================================================

  grpc::Status SubmitRomProposal(
      grpc::ServerContext* context,
      const proto::SubmitRomProposalRequest* request,
      proto::SubmitRomProposalResponse* response) override;

  grpc::Status GetProposalStatus(
      grpc::ServerContext* context,
      const proto::GetProposalStatusRequest* request,
      proto::GetProposalStatusResponse* response) override;

  // =========================================================================
  // Version Management
  // =========================================================================

  grpc::Status CreateSnapshot(grpc::ServerContext* context,
                              const proto::CreateSnapshotRequest* request,
                              proto::CreateSnapshotResponse* response) override;

  grpc::Status RestoreSnapshot(
      grpc::ServerContext* context,
      const proto::RestoreSnapshotRequest* request,
      proto::RestoreSnapshotResponse* response) override;

  grpc::Status ListSnapshots(grpc::ServerContext* context,
                             const proto::ListSnapshotsRequest* request,
                             proto::ListSnapshotsResponse* response) override;

 private:
  Config config_;
  RomGetter rom_getter_;
  RomVersionManager* version_mgr_;         // Not owned, may be null
  ProposalApprovalManager* approval_mgr_;  // Not owned, may be null

  // Helper to check if ROM is loaded
  grpc::Status ValidateRomLoaded();

  // Helper to create snapshot before write operations
  absl::Status MaybeCreateSnapshot(const std::string& description);
};

#endif  // YAZE_WITH_GRPC

}  // namespace net

}  // namespace yaze

#endif  // YAZE_APP_NET_ROM_SERVICE_IMPL_H_
