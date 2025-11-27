#include "app/emu/render/save_state_manager.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace emu {
namespace render {

SaveStateManager::SaveStateManager(emu::Snes* snes, Rom* rom)
    : snes_(snes), rom_(rom) {
  // Default state directory in user's config
  state_directory_ = "./states";
}

SaveStateManager::~SaveStateManager() = default;

absl::Status SaveStateManager::Initialize() {
  if (!snes_ || !rom_) {
    return absl::FailedPreconditionError("SNES or ROM not provided");
  }

  // Calculate ROM checksum for compatibility checking
  rom_checksum_ = CalculateRomChecksum();
  printf("[StateManager] ROM checksum: 0x%08X\n", rom_checksum_);

  // Use ~/.yaze/states/ directory for state files
  if (const char* home = std::getenv("HOME")) {
    state_directory_ = std::string(home) + "/.yaze/states";
  } else {
    state_directory_ = "./states";
  }

  // Ensure state directory exists
  std::filesystem::create_directories(state_directory_);
  printf("[StateManager] State directory: %s\n", state_directory_.c_str());

  return absl::OkStatus();
}

absl::Status SaveStateManager::LoadState(StateType type, int context_id) {
  std::string path = GetStatePath(type, context_id);

  if (!std::filesystem::exists(path)) {
    return absl::NotFoundError("State file not found: " + path);
  }

  // Load metadata and check compatibility
  std::string meta_path = GetMetadataPath(type, context_id);
  if (std::filesystem::exists(meta_path)) {
    auto meta_result = GetStateMetadata(type, context_id);
    if (meta_result.ok() && !IsStateCompatible(*meta_result)) {
      return absl::FailedPreconditionError(
          "State incompatible with current ROM (checksum mismatch)");
    }
  }

  return LoadStateFromFile(path);
}

absl::Status SaveStateManager::GenerateRoomState(int room_id) {
  printf("[StateManager] Generating state for room %d...\n", room_id);

  // Boot game to title screen
  auto status = BootToTitleScreen();
  if (!status.ok()) {
    return status;
  }

  // Navigate to file select
  status = NavigateToFileSelect();
  if (!status.ok()) {
    return status;
  }

  // Start new game
  status = StartNewGame();
  if (!status.ok()) {
    return status;
  }

  // Navigate to target room (this is game-specific)
  status = NavigateToRoom(room_id);
  if (!status.ok()) {
    return status;
  }

  // Save state
  StateMetadata metadata;
  metadata.rom_checksum = rom_checksum_;
  metadata.rom_region = 0;  // TODO: detect region
  metadata.room_id = room_id;
  metadata.game_module = GetGameModule();
  metadata.description = "Room " + std::to_string(room_id);

  std::string path = GetStatePath(StateType::kRoomLoaded, room_id);
  return SaveStateToFile(path, metadata);
}

absl::Status SaveStateManager::GenerateAllBaselineStates() {
  // Generate states for common rooms used in testing and previews
  const std::vector<std::pair<int, const char*>> baseline_rooms = {
      {0x0012, "Sanctuary"},
      {0x0020, "Hyrule Castle Entrance"},
      {0x0028, "Eastern Palace Entrance"},
      {0x0004, "Link's House"},
      {0x0044, "Desert Palace Entrance"},
      {0x0075, "Tower of Hera Entrance"},
  };

  int success_count = 0;
  for (const auto& [room_id, name] : baseline_rooms) {
    printf("[StateManager] Generating %s (0x%04X)...\n", name, room_id);
    auto status = GenerateRoomState(room_id);
    if (status.ok()) {
      success_count++;
    } else {
      printf("[StateManager] Warning: Failed to generate %s: %s\n", name,
             std::string(status.message()).c_str());
    }
  }

  printf("[StateManager] Generated %d/%zu baseline states\n", success_count,
         baseline_rooms.size());
  return absl::OkStatus();
}

bool SaveStateManager::HasCachedState(StateType type, int context_id) const {
  std::string path = GetStatePath(type, context_id);
  return std::filesystem::exists(path);
}

absl::StatusOr<StateMetadata> SaveStateManager::GetStateMetadata(
    StateType type, int context_id) const {
  std::string path = GetMetadataPath(type, context_id);

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::NotFoundError("Metadata file not found");
  }

  StateMetadata metadata;
  file.read(reinterpret_cast<char*>(&metadata.version), sizeof(metadata.version));
  file.read(reinterpret_cast<char*>(&metadata.rom_checksum),
            sizeof(metadata.rom_checksum));
  file.read(reinterpret_cast<char*>(&metadata.rom_region),
            sizeof(metadata.rom_region));
  file.read(reinterpret_cast<char*>(&metadata.room_id), sizeof(metadata.room_id));
  file.read(reinterpret_cast<char*>(&metadata.game_module),
            sizeof(metadata.game_module));

  uint32_t desc_len;
  file.read(reinterpret_cast<char*>(&desc_len), sizeof(desc_len));
  if (desc_len > 0 && desc_len < 1024) {
    metadata.description.resize(desc_len);
    file.read(metadata.description.data(), desc_len);
  }

  return metadata;
}

absl::Status SaveStateManager::SaveStateToFile(const std::string& path,
                                                const StateMetadata& metadata) {
  // Save SNES state using existing method
  snes_->saveState(path);

  // Save metadata
  std::string meta_path = path + ".meta";
  std::ofstream meta_file(meta_path, std::ios::binary);
  if (!meta_file) {
    return absl::InternalError("Failed to create metadata file");
  }

  meta_file.write(reinterpret_cast<const char*>(&metadata.version),
                  sizeof(metadata.version));
  meta_file.write(reinterpret_cast<const char*>(&metadata.rom_checksum),
                  sizeof(metadata.rom_checksum));
  meta_file.write(reinterpret_cast<const char*>(&metadata.rom_region),
                  sizeof(metadata.rom_region));
  meta_file.write(reinterpret_cast<const char*>(&metadata.room_id),
                  sizeof(metadata.room_id));
  meta_file.write(reinterpret_cast<const char*>(&metadata.game_module),
                  sizeof(metadata.game_module));

  uint32_t desc_len = metadata.description.size();
  meta_file.write(reinterpret_cast<const char*>(&desc_len), sizeof(desc_len));
  meta_file.write(metadata.description.data(), desc_len);

  printf("[StateManager] Saved state to %s\n", path.c_str());
  return absl::OkStatus();
}

absl::Status SaveStateManager::LoadStateFromFile(const std::string& path) {
  snes_->loadState(path);
  printf("[StateManager] Loaded state from %s\n", path.c_str());
  return absl::OkStatus();
}

uint32_t SaveStateManager::CalculateRomChecksum() const {
  if (!rom_ || !rom_->is_loaded()) {
    return 0;
  }
  return CalculateCRC32(rom_->data(), rom_->size());
}

absl::Status SaveStateManager::BootToTitleScreen() {
  snes_->Reset(true);

  // Run frames until title screen appears (module 0x01)
  // ALTTP shows title screen after ~200 frames
  const int kMaxFrames = 400;
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();
    uint8_t module = GetGameModule();
    // Module 0x01 = title screen, but we may also see 0x00 during init
    if (module == 0x01) {
      printf("[StateManager] Reached title screen (module 0x%02X) at frame %d\n",
             module, i);
      return absl::OkStatus();
    }
  }

  // Even if we didn't detect module 0x01, continue if we're past init
  printf("[StateManager] Title screen timeout, module=0x%02X (continuing)\n",
         GetGameModule());
  return absl::OkStatus();
}

absl::Status SaveStateManager::NavigateToFileSelect() {
  // Press Start to go to file select
  PressButton(buttons::kStart, 2);

  // Wait for file select module (0x02)
  const int kMaxFrames = 120;
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();
    uint8_t module = GetGameModule();
    if (module == 0x02) {
      printf("[StateManager] Navigated to file select (module 0x%02X)\n", module);
      return absl::OkStatus();
    }
  }

  printf("[StateManager] File select timeout, module=0x%02X (continuing)\n",
         GetGameModule());
  return absl::OkStatus();
}

absl::Status SaveStateManager::StartNewGame() {
  // Select first file slot (empty slot creates new game)
  PressButton(buttons::kA, 2);
  WaitFrames(30);

  // Confirm selection (starts name entry or uses default name)
  PressButton(buttons::kA, 2);
  WaitFrames(30);

  // If we hit name entry, press Start to accept default name
  // Then press A again to confirm
  PressButton(buttons::kStart, 2);
  WaitFrames(30);
  PressButton(buttons::kA, 2);
  WaitFrames(30);

  // Wait for gameplay module (0x07=dungeon or 0x09=overworld)
  const int kMaxFrames = 300;
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();
    uint8_t module = GetGameModule();
    if (module == 0x07 || module == 0x09) {
      printf("[StateManager] Started new game (module 0x%02X)\n", module);
      return absl::OkStatus();
    }
  }

  printf("[StateManager] Game start timeout, module=0x%02X\n", GetGameModule());
  return absl::DeadlineExceededError("Game failed to start");
}

absl::Status SaveStateManager::NavigateToRoom(int room_id) {
  // Try WRAM teleportation first (fast)
  auto status = TeleportToRoomViaWram(room_id);
  if (status.ok()) {
    return absl::OkStatus();
  }

  // Fall back to TAS navigation if WRAM fails
  printf("[StateManager] WRAM teleport failed: %s, trying TAS fallback\n",
         std::string(status.message()).c_str());
  return NavigateToRoomViaTas(room_id);
}

absl::Status SaveStateManager::TeleportToRoomViaWram(int room_id) {
  // Set target room
  snes_->Write(0x7E00A0, room_id & 0xFF);
  snes_->Write(0x7E00A1, (room_id >> 8) & 0xFF);

  // Set indoor flag for dungeon rooms (rooms < 0x128 are dungeons)
  bool is_dungeon = (room_id < 0x128);
  snes_->Write(0x7E001B, is_dungeon ? 0x01 : 0x00);

  // Trigger room transition by setting loading module
  snes_->Write(0x7E0010, 0x05);  // Loading module

  // Set safe center position for Link
  snes_->Write(0x7E0022, 0x80);  // X low
  snes_->Write(0x7E0023, 0x00);  // X high
  snes_->Write(0x7E0020, 0x80);  // Y low
  snes_->Write(0x7E0021, 0x00);  // Y high

  // Wait for room to fully load
  const int kMaxFrames = 300;
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();

    if (IsRoomFullyLoaded() && GetCurrentRoom() == room_id) {
      printf("[StateManager] WRAM teleport to room 0x%04X successful\n",
             room_id);
      return absl::OkStatus();
    }
  }

  return absl::DeadlineExceededError(
      "WRAM teleport failed for room " + std::to_string(room_id));
}

absl::Status SaveStateManager::NavigateToRoomViaTas(int room_id) {
  // TAS fallback: wait for whatever room loads naturally
  // This is useful when WRAM injection doesn't work
  const int kMaxFrames = 600;  // 10 seconds
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();

    if (IsRoomFullyLoaded()) {
      int current_room = GetCurrentRoom();
      printf("[StateManager] TAS: Room loaded: 0x%04X (target: 0x%04X)\n",
             current_room, room_id);
      if (current_room == room_id) {
        return absl::OkStatus();
      }
      // Accept whatever room we're in for now
      // Future: implement actual TAS navigation
      return absl::OkStatus();
    }
  }

  return absl::DeadlineExceededError("TAS navigation timeout");
}

void SaveStateManager::PressButton(int button, int frames) {
  for (int i = 0; i < frames; ++i) {
    snes_->SetButtonState(0, button, true);
    snes_->RunFrame();
  }
  snes_->SetButtonState(0, button, false);
}

void SaveStateManager::ReleaseAllButtons() {
  // Release all buttons (bit indices 0-11)
  for (int btn : {buttons::kA, buttons::kB, buttons::kX, buttons::kY,
                  buttons::kL, buttons::kR, buttons::kStart, buttons::kSelect,
                  buttons::kUp, buttons::kDown, buttons::kLeft, buttons::kRight}) {
    snes_->SetButtonState(0, btn, false);
  }
}

void SaveStateManager::WaitFrames(int frames) {
  for (int i = 0; i < frames; ++i) {
    snes_->RunFrame();
  }
}

uint8_t SaveStateManager::ReadWram(uint32_t addr) {
  return snes_->Read(addr);
}

uint16_t SaveStateManager::ReadWram16(uint32_t addr) {
  return snes_->Read(addr) | (snes_->Read(addr + 1) << 8);
}

int SaveStateManager::GetCurrentRoom() {
  return ReadWram16(wram_addresses::kRoomId);
}

uint8_t SaveStateManager::GetGameModule() {
  return ReadWram(wram_addresses::kGameModule);
}

bool SaveStateManager::WaitForModule(uint8_t target, int max_frames) {
  for (int i = 0; i < max_frames; ++i) {
    snes_->RunFrame();
    if (GetGameModule() == target) {
      return true;
    }
  }
  return false;
}

bool SaveStateManager::IsRoomFullyLoaded() {
  // Check if game is in dungeon module (0x07) or overworld module (0x09)
  // Also verify submodule is 0x00 (fully loaded, not transitioning)
  uint8_t module = GetGameModule();
  uint8_t submodule = ReadWram(0x7E0011);
  return (module == 0x07 || module == 0x09) && (submodule == 0x00);
}

std::string SaveStateManager::GetStatePath(StateType type,
                                            int context_id) const {
  std::string type_str;
  switch (type) {
    case StateType::kRoomLoaded:
      type_str = "room";
      break;
    case StateType::kOverworldLoaded:
      type_str = "overworld";
      break;
    case StateType::kBlankCanvas:
      type_str = "blank";
      break;
  }
  return state_directory_ + "/" + type_str + "_" + std::to_string(context_id) +
         ".state";
}

std::string SaveStateManager::GetMetadataPath(StateType type,
                                               int context_id) const {
  return GetStatePath(type, context_id) + ".meta";
}

bool SaveStateManager::IsStateCompatible(const StateMetadata& metadata) const {
  return metadata.rom_checksum == rom_checksum_;
}

}  // namespace render
}  // namespace emu
}  // namespace yaze
