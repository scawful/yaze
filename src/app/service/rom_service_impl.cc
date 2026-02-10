#include "app/service/rom_service_impl.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>

#include "absl/strings/str_format.h"
#include "app/net/rom_version_manager.h"
#include "rom/rom.h"
#include "util/json.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/sprite/sprite.h"

// Proto namespace alias for convenience
namespace rom_svc = ::yaze::proto;

namespace yaze {

namespace net {

namespace {

std::string GenerateProposalId() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  return absl::StrFormat("proposal_%lld", ms);
}

std::string BuildRoomRawJson(const zelda3::Room& room) {
  nlohmann::json payload = nlohmann::json::object();
  payload["room_id"] = room.id();
  payload["layout"] = room.layout;
  payload["floor1"] = room.floor1();
  payload["floor2"] = room.floor2();
  payload["blockset"] = room.blockset;
  payload["spriteset"] = room.spriteset;
  payload["palette"] = room.palette;
  payload["effect"] = static_cast<int>(room.effect());
  payload["tag1"] = static_cast<int>(room.tag1());
  payload["tag2"] = static_cast<int>(room.tag2());

  auto objects = nlohmann::json::array();
  for (const auto& obj : room.GetTileObjects()) {
    nlohmann::json entry = {
        {"id", static_cast<int>(obj.id_)},
        {"x", obj.x()},
        {"y", obj.y()},
        {"size", obj.size()},
        {"layer", obj.GetLayerValue()},
        {"size_x_bits", obj.size_x_bits_},
        {"size_y_bits", obj.size_y_bits_},
    };
    objects.push_back(entry);
  }
  payload["objects"] = objects;

  auto doors = nlohmann::json::array();
  for (const auto& door : room.GetDoors()) {
    nlohmann::json entry = {
        {"byte1", door.byte1},
        {"byte2", door.byte2},
        {"position", door.position},
        {"direction", static_cast<int>(door.direction)},
        {"type", static_cast<int>(door.type)},
    };
    doors.push_back(entry);
  }
  payload["doors"] = doors;

  auto sprites = nlohmann::json::array();
  for (const auto& sprite : room.GetSprites()) {
    nlohmann::json entry = {
        {"id", sprite.id()},
        {"x", sprite.x()},
        {"y", sprite.y()},
        {"subtype", sprite.subtype()},
        {"layer", sprite.layer()},
    };
    sprites.push_back(entry);
  }
  payload["sprites"] = sprites;

  payload["encoded_objects"] = room.EncodeObjects();
  payload["encoded_sprites"] = room.EncodeSprites();

  return payload.dump();
}

absl::Status LoadGameDataForRom(Rom* rom, zelda3::GameData* data,
                                const zelda3::LoadOptions& options) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!data) {
    return absl::InvalidArgumentError("GameData is null");
  }
  data->set_rom(rom);
  return zelda3::LoadGameData(*rom, *data, options);
}

absl::Status LoadMetadataForRom(Rom* rom, zelda3::GameData* data) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!data) {
    return absl::InvalidArgumentError("GameData is null");
  }
  data->set_rom(rom);
  return zelda3::LoadMetadata(*rom, *data);
}

}  // namespace

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

  if (config_.require_approval_for_writes || request->require_approval()) {
    if (!approval_mgr_) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                          "Approval manager not initialized");
    }

    std::string description = request->require_approval()
                                  ? "WriteBytes (proposal)"
                                  : "WriteBytes (approval required)";
    if (!request->data().empty()) {
      description = absl::StrFormat("WriteBytes at 0x%X (%zu bytes)", offset,
                                    request->data().size());
    }

    nlohmann::json proposal_data = {{"description", description},
                                    {"type", "write_bytes"},
                                    {"offset", offset},
                                    {"size", request->data().size()}};
    std::vector<uint8_t> bytes(request->data().begin(), request->data().end());
    proposal_data["data"] = bytes;

    std::string proposal_id = GenerateProposalId();
    auto status = approval_mgr_->SubmitProposal(proposal_id, "grpc",
                                                description, proposal_data);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }

    response->set_success(true);
    response->set_proposal_id(proposal_id);
    return grpc::Status::OK;
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
  std::vector<uint8_t> bytes(data.begin(), data.end());
  auto write_status =
      rom->WriteVector(static_cast<int>(offset), std::move(bytes));
  if (!write_status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string(write_status.message()));
  }
  response->set_success(true);

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::GetRomInfo(
    grpc::ServerContext* context, const rom_svc::GetRomInfoRequest* request,
    rom_svc::GetRomInfoResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  response->set_title(rom->title());
  response->set_size(rom->size());

  auto metadata = std::make_unique<zelda3::GameData>(rom);
  if (LoadMetadataForRom(rom, metadata.get()).ok()) {
    response->set_version(metadata->version == zelda3_version::JP ? "JP" : "US");
  }
  response->set_is_expanded(rom->size() > 0x200000);

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::ReadOverworldMap(
    grpc::ServerContext* context,
    const rom_svc::ReadOverworldMapRequest* request,
    rom_svc::ReadOverworldMapResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  uint32_t map_id = request->map_id();
  if (map_id >= 160) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        "Invalid map_id (0-159)");
  }

  auto metadata = std::make_unique<zelda3::GameData>(rom);
  auto metadata_status = LoadMetadataForRom(rom, metadata.get());
  if (!metadata_status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string(metadata_status.message()));
  }

  // Instantiate Overworld parser (heavy: loads/decompresses all maps).
  zelda3::Overworld overworld(rom, metadata.get());
  auto status = overworld.Load(rom);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed to load Overworld: " +
                            std::string(status.message()));
  }

  int world = 0;
  int local_id = map_id;
  if (map_id >= 128) {
    world = 2;
    local_id -= 128;
  } else if (map_id >= 64) {
    world = 1;
    local_id -= 64;
  }

  // Access tile data (map tiles are stored as [x][y])
  auto& blockset = overworld.GetMapTiles(world);
  
  // Calculate global coordinates
  // All worlds (Light, Dark, Special) are laid out in 8-map columns
  int maps_per_row = 8;
  int map_col = local_id % maps_per_row;
  int map_row = local_id / maps_per_row;
  int global_start_x = map_col * 32;
  int global_start_y = map_row * 32;

  response->set_map_id(map_id);
  
  if (blockset.empty() || blockset[0].empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Overworld tiles not initialized");
  }

  // Validate bounds (outer = x, inner = y)
  if (global_start_x + 32 > static_cast<int>(blockset.size()) ||
      global_start_y + 32 > static_cast<int>(blockset[0].size())) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        absl::StrFormat("Map bounds error: %d (local %d) -> (%d, %d)", map_id,
                        local_id, global_start_x, global_start_y));
  }

  // Flatten 32x32 area from global grid
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      response->add_tile16_data(
          blockset[global_start_x + x][global_start_y + y]);
    }
  }

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteOverworldTile(
    grpc::ServerContext* context,
    const rom_svc::WriteOverworldTileRequest* request,
    rom_svc::WriteOverworldTileResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "ROM not loaded");
  }

  uint32_t map_id = request->map_id();
  uint32_t x = request->x();
  uint32_t y = request->y();
  uint32_t tile_id = request->tile16_id();

  if (map_id >= 160 || x >= 32 || y >= 32) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Invalid coordinates");
  }

  if (config_.require_approval_for_writes || request->require_approval()) {
    if (!approval_mgr_) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                          "Approval manager not initialized");
    }

    std::string description = request->description().empty()
                                  ? absl::StrFormat(
                                        "Update OW tile: Map %d (%d,%d) -> %d",
                                        map_id, x, y, tile_id)
                                  : request->description();
    nlohmann::json proposal_data = {{"description", description},
                                    {"type", "overworld_tile_write"},
                                    {"map_id", map_id},
                                    {"x", x},
                                    {"y", y},
                                    {"tile16_id", tile_id}};

    std::string proposal_id = GenerateProposalId();
    auto status = approval_mgr_->SubmitProposal(proposal_id, "grpc",
                                                description, proposal_data);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          std::string(status.message()));
    }

    response->set_success(true);
    response->set_proposal_id(proposal_id);
    return grpc::Status::OK;
  }

  auto metadata = std::make_unique<zelda3::GameData>(rom);
  auto metadata_status = LoadMetadataForRom(rom, metadata.get());
  if (!metadata_status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string(metadata_status.message()));
  }

  // Load Overworld
  zelda3::Overworld overworld(rom, metadata.get());
  auto status = overworld.Load(rom);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed to load Overworld");
  }

  int world = 0;
  int local_id = map_id;
  if (map_id >= 128) {
    world = 2;
    local_id -= 128;
  } else if (map_id >= 64) {
    world = 1;
    local_id -= 64;
  }

  // Calculate global coordinates for SetTile
  int maps_per_row = 8;
  int map_col = local_id % maps_per_row;
  int map_row = local_id / maps_per_row;
  int global_x = (map_col * 32) + x;
  int global_y = (map_row * 32) + y;

  // Update tile in the map tiles (map tiles are stored as [x][y])
  auto& blockset = overworld.GetMapTiles(world);
  if (blockset.empty() || blockset[0].empty()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Overworld tiles not initialized");
  }
  if (global_x >= static_cast<int>(blockset.size()) ||
      global_y >= static_cast<int>(blockset[0].size())) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        "Global coordinates out of range");
  }
  blockset[global_x][global_y] = static_cast<uint16_t>(tile_id);

  // Create snapshot
  auto snap_status = MaybeCreateSnapshot(
      absl::StrFormat("Update OW tile: Map %d (%d,%d) -> %d", map_id, x, y, tile_id));
  if (!snap_status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, "Snapshot failed");
  }

  // Save back to ROM
  status = overworld.Save(rom);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed to save Overworld: " + std::string(status.message()));
  }

  response->set_success(true);
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::ReadDungeonRoom(
    grpc::ServerContext* context,
    const rom_svc::ReadDungeonRoomRequest* request,
    rom_svc::ReadDungeonRoomResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "ROM not loaded");
  }

  uint32_t room_id = request->room_id();
  if (room_id >= 296) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Invalid room_id");
  }

  auto game_data = std::make_unique<zelda3::GameData>(rom);
  zelda3::LoadOptions options;
  options.load_graphics = true;
  options.load_palettes = true;
  options.load_gfx_groups = true;
  options.populate_metadata = true;
  options.expand_rom = false;
  auto game_status = LoadGameDataForRom(rom, game_data.get(), options);
  if (!game_status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string(game_status.message()));
  }

  // Load Room
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);
  room.SetGameData(game_data.get());
  room.LoadObjects();
  room.LoadSprites();
  room.RenderRoomGraphics();

  response->set_room_id(room_id);

  // Get tile indices from BG1 buffer
  // Note: Dungeon rooms can be large, but BG buffer is fixed size?
  // BackgroundBuffer default is 512x512 pixels (64x64 tiles)
  // Check room dimensions? For now assuming standard buffer
  const auto& buffer = room.bg1_buffer().buffer();
  
  for (uint16_t tile : buffer) {
    response->add_tile16_data(tile);
  }

  const std::string raw_json = BuildRoomRawJson(room);
  if (!raw_json.empty()) {
    response->set_raw_data(raw_json);
  }

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::WriteDungeonTile(
    grpc::ServerContext* context,
    const rom_svc::WriteDungeonTileRequest* request,
    rom_svc::WriteDungeonTileResponse* response) {
  if (!approval_mgr_) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "Approval manager not initialized");
  }

  std::string description = request->description().empty()
                                ? absl::StrFormat(
                                      "Dungeon tile write: Room %d (%d,%d) -> %d",
                                      request->room_id(), request->x(),
                                      request->y(), request->tile16_id())
                                : request->description();
  nlohmann::json proposal_data = {{"description", description},
                                  {"type", "dungeon_tile_write"},
                                  {"room_id", request->room_id()},
                                  {"x", request->x()},
                                  {"y", request->y()},
                                  {"tile16_id", request->tile16_id()}};

  std::string proposal_id = GenerateProposalId();
  auto status = approval_mgr_->SubmitProposal(proposal_id, "grpc",
                                              description, proposal_data);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string(status.message()));
  }

  response->set_success(true);
  response->set_proposal_id(proposal_id);
  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::ReadSprite(
    grpc::ServerContext* context, const rom_svc::ReadSpriteRequest* request,
    rom_svc::ReadSpriteResponse* response) {
  Rom* rom = rom_getter_();
  if (!rom || !rom->is_loaded()) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "ROM not loaded");
  }

  uint32_t sprite_id = request->sprite_id();
  if (sprite_id >= 256) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Invalid sprite_id (0-255)");
  }

  // ALttP Sprite Property Tables (PC Offsets)
  const uint32_t kSpriteHPTable = 0x6B173;
  const uint32_t kSpriteDamageTable = 0x6B266;
  const uint32_t kSpritePaletteTable = 0x6B35B;
  const uint32_t kSpritePropertiesTable = 0x6B450; // 4 bytes per sprite

  response->set_sprite_id(sprite_id);
  
  auto read_byte = [&](uint32_t offset, uint8_t* out) -> absl::Status {
    auto val = rom->ReadByte(static_cast<int>(offset));
    if (!val.ok()) {
      return val.status();
    }
    *out = *val;
    return absl::OkStatus();
  };

  std::vector<uint8_t> data;
  data.reserve(7);
  uint8_t value = 0;
  auto status = read_byte(kSpriteHPTable + sprite_id, &value);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        std::string(status.message()));
  }
  data.push_back(value);
  status = read_byte(kSpriteDamageTable + sprite_id, &value);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        std::string(status.message()));
  }
  data.push_back(value);
  status = read_byte(kSpritePaletteTable + sprite_id, &value);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                        std::string(status.message()));
  }
  data.push_back(value);
  for (int i = 0; i < 4; ++i) {
    status =
        read_byte(kSpritePropertiesTable + (sprite_id * 4) + i, &value);
    if (!status.ok()) {
      return grpc::Status(grpc::StatusCode::OUT_OF_RANGE,
                          std::string(status.message()));
    }
    data.push_back(value);
  }

  response->set_sprite_data(data.data(), data.size());

  return grpc::Status::OK;
}

grpc::Status RomServiceImpl::SubmitRomProposal(
    grpc::ServerContext* context,
    const rom_svc::SubmitRomProposalRequest* request,
    rom_svc::SubmitRomProposalResponse* response) {
  if (!approval_mgr_) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "Approval manager not initialized");
  }

  std::string description = request->description();
  if (description.empty()) {
    description = "ROM proposal";
  }
  std::string username = request->username().empty() ? "grpc" : request->username();

  nlohmann::json proposal_data = {{"description", description}};

  if (request->has_write_bytes()) {
    const auto& write = request->write_bytes();
    proposal_data["type"] = "write_bytes";
    proposal_data["offset"] = write.offset();
    proposal_data["size"] = write.data().size();
    std::vector<uint8_t> bytes(write.data().begin(), write.data().end());
    proposal_data["data"] = bytes;
  } else if (request->has_overworld_tile()) {
    const auto& write = request->overworld_tile();
    proposal_data["type"] = "overworld_tile_write";
    proposal_data["map_id"] = write.map_id();
    proposal_data["x"] = write.x();
    proposal_data["y"] = write.y();
    proposal_data["tile16_id"] = write.tile16_id();
  } else if (request->has_dungeon_tile()) {
    const auto& write = request->dungeon_tile();
    proposal_data["type"] = "dungeon_tile_write";
    proposal_data["room_id"] = write.room_id();
    proposal_data["x"] = write.x();
    proposal_data["y"] = write.y();
    proposal_data["tile16_id"] = write.tile16_id();
  } else {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "No proposal payload provided");
  }

  std::string proposal_id = GenerateProposalId();
  auto status = approval_mgr_->SubmitProposal(proposal_id, username, description,
                                              proposal_data);
  if (!status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        std::string(status.message()));
  }

  response->set_success(true);
  response->set_proposal_id(proposal_id);
  return grpc::Status::OK;
}
grpc::Status RomServiceImpl::GetProposalStatus(
    grpc::ServerContext* context,
    const rom_svc::GetProposalStatusRequest* request,
    rom_svc::GetProposalStatusResponse* response) {
  if (!approval_mgr_) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "Approval manager not initialized");
  }

  auto status_or = approval_mgr_->GetProposalStatus(request->proposal_id());
  if (!status_or.ok()) {
    return grpc::Status(grpc::StatusCode::NOT_FOUND,
                        std::string(status_or.status().message()));
  }

  const auto& status = *status_or;
  response->set_proposal_id(status.proposal_id);
  response->set_status(status.status);

  int approvals = 0;
  int rejections = 0;
  for (const auto& [user, approved] : status.votes) {
    response->add_voters(user);
    if (approved) {
      approvals++;
    } else {
      rejections++;
    }
  }
  response->set_approval_count(approvals);
  response->set_rejection_count(rejections);
  return grpc::Status::OK;
}
grpc::Status RomServiceImpl::CreateSnapshot(
    grpc::ServerContext* context, const rom_svc::CreateSnapshotRequest* request,
    rom_svc::CreateSnapshotResponse* response) {
  if (!version_mgr_) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "Version manager not initialized");
  }

  auto result = version_mgr_->CreateSnapshot(
      request->description(),
      request->username().empty() ? "grpc" : request->username(),
      request->is_checkpoint());
  if (!result.ok()) {
    response->set_success(false);
    response->set_error(std::string(result.status().message()));
    return grpc::Status::OK;
  }

  response->set_success(true);
  response->set_snapshot_id(*result);
  return grpc::Status::OK;
}
grpc::Status RomServiceImpl::RestoreSnapshot(
    grpc::ServerContext* context,
    const rom_svc::RestoreSnapshotRequest* request,
    rom_svc::RestoreSnapshotResponse* response) {
  if (!version_mgr_) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "Version manager not initialized");
  }

  auto status = version_mgr_->RestoreSnapshot(request->snapshot_id());
  if (!status.ok()) {
    response->set_success(false);
    response->set_error(std::string(status.message()));
    return grpc::Status::OK;
  }

  response->set_success(true);
  return grpc::Status::OK;
}
grpc::Status RomServiceImpl::ListSnapshots(
    grpc::ServerContext* context, const rom_svc::ListSnapshotsRequest* request,
    rom_svc::ListSnapshotsResponse* response) {
  if (!version_mgr_) {
    return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION,
                        "Version manager not initialized");
  }

  auto snapshots = version_mgr_->GetSnapshots(false);
  uint32_t max_results = request->max_results();
  if (max_results > 0 && snapshots.size() > max_results) {
    snapshots.resize(max_results);
  }

  for (const auto& snapshot : snapshots) {
    auto* info = response->add_snapshots();
    info->set_snapshot_id(snapshot.snapshot_id);
    info->set_description(snapshot.description);
    info->set_username(snapshot.creator);
    info->set_timestamp(snapshot.timestamp);
    info->set_is_checkpoint(snapshot.is_checkpoint);
    info->set_is_safe_point(snapshot.is_safe_point);
    info->set_size_bytes(snapshot.rom_data.size());
  }

  return grpc::Status::OK;
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
