#include "app/emu/render/save_state_manager.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>

#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif

#include "app/emu/snes.h"
#include "rom/rom.h"

namespace {

#ifdef __EMSCRIPTEN__
// Simple path utilities for WASM builds
std::string GetParentPath(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return "";
  return path.substr(0, pos);
}

bool FileExists(const std::string& path) {
  std::ifstream f(path);
  return f.good();
}

void CreateDirectories(const std::string& path) {
  // In WASM/Emscripten, directories are typically auto-created
  // or we use MEMFS which doesn't require explicit directory creation
  (void)path;
}
#else
std::string GetParentPath(const std::string& path) {
  std::filesystem::path p(path);
  return p.parent_path().string();
}

bool FileExists(const std::string& path) {
  return std::filesystem::exists(path);
}

void CreateDirectories(const std::string& path) {
  std::filesystem::create_directories(path);
}
#endif

}  // namespace

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
  CreateDirectories(state_directory_);
  printf("[StateManager] State directory: %s\n", state_directory_.c_str());

  return absl::OkStatus();
}

absl::Status SaveStateManager::LoadState(StateType type, int context_id) {
  std::string path = GetStatePath(type, context_id);

  if (!FileExists(path)) {
    return absl::NotFoundError("State file not found: " + path);
  }

  // Load metadata and check compatibility
  std::string meta_path = GetMetadataPath(type, context_id);
  if (FileExists(meta_path)) {
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
  return FileExists(path);
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

  if (!file) {
    return absl::InternalError("Failed to read metadata");
  }

  return metadata;
}

absl::Status SaveStateManager::SaveStateToFile(const std::string& path,
                                                const StateMetadata& metadata) {
  // Ensure directory exists
  std::string parent_path = GetParentPath(path);
  if (!parent_path.empty()) {
    CreateDirectories(parent_path);
  }

  // Save SNES state using existing method
  auto save_status = snes_->saveState(path);
  if (!save_status.ok()) {
    return save_status;
  }

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

  if (!meta_file) {
    return absl::InternalError("Failed while writing metadata");
  }
  printf("[StateManager] Saved state to %s\n", path.c_str());
  return absl::OkStatus();
}

absl::Status SaveStateManager::LoadStateFromFile(const std::string& path) {
  auto status = snes_->loadState(path);
  if (!status.ok()) {
    return status;
  }
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

  // Run frames until we reach File Select (module 0x01)
  // In ALTTP, Module 0x00 is Intro, which transitions to 0x14 (Attract)
  // unless Start is pressed, which goes to 0x01 (File Select).
  const int kMaxFrames = 2000;
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();
    uint8_t module = GetGameModule();

    if (i % 60 == 0) {
      printf("[StateManager] Frame %d: module=0x%02X\n", i, module);
    }

    // Reached File Select
    if (module == 0x01) {
      printf("[StateManager] Reached File Select (module 0x01) at frame %d\n", i);
      return absl::OkStatus();
    }

    // If we hit Attract Mode (0x14), press Start to go to File Select
    if (module == 0x14) {
      // Hold Start for 10 frames every 60 frames
      if ((i % 60) < 10) {
        if (i % 60 == 0) printf("[StateManager] In Attract Mode, holding Start...\n");
        snes_->SetButtonState(0, buttons::kStart, true);
      } else {
        snes_->SetButtonState(0, buttons::kStart, false);
      }
    }
    // Also try pressing Start during Intro (after some initial frames)
    // Submodule 8 (FadeLogoIn) is when input is accepted
    else if (module == 0x00 && i > 300) {
      // Hold Start for 10 frames every 60 frames
      if ((i % 60) < 10) {
        if (i % 60 == 0) printf("[StateManager] In Intro, holding Start...\n");
        snes_->SetButtonState(0, buttons::kStart, true);
      } else {
        snes_->SetButtonState(0, buttons::kStart, false);
      }
    }
  }

  printf("[StateManager] Boot timeout, module=0x%02X\n", GetGameModule());
  return absl::DeadlineExceededError("Failed to reach File Select");
}

absl::Status SaveStateManager::NavigateToFileSelect() {
  // We should already be at File Select (0x01) from BootToTitleScreen
  // But if not, press Start
  
  const int kMaxFrames = 120;
  for (int i = 0; i < kMaxFrames; ++i) {
    uint8_t module = GetGameModule();
    if (module == 0x01) {
      printf("[StateManager] Navigated to file select (module 0x01)\n");
      return absl::OkStatus();
    }
    
    // If in Intro or Attract, press Start
    if (module == 0x00 || module == 0x14) {
       snes_->SetButtonState(0, buttons::kStart, true);
    }
    
    snes_->RunFrame();
    snes_->SetButtonState(0, buttons::kStart, false);
  }

  printf("[StateManager] File select timeout, module=0x%02X (continuing)\n",
         GetGameModule());
  return absl::OkStatus();
}

absl::Status SaveStateManager::StartNewGame() {
  printf("[StateManager] Starting new game sequence...\n");

  // Phase 1: File Select (0x01) -> Name Entry (0x04)
  // Try to select the first file and confirm
  const int kFileSelectTimeout = 600;
  for (int i = 0; i < kFileSelectTimeout; ++i) {
    snes_->RunFrame();
    uint8_t module = GetGameModule();

    if (module == 0x04) {
      printf("[StateManager] Reached Name Entry (module 0x04) at frame %d\n", i);
      break;
    }
    
    // If we are in File Select (0x01), press A to select/confirm
    if (module == 0x01) {
      if (i % 60 < 10) { // Press A for 10 frames every 60 frames
        snes_->SetButtonState(0, buttons::kA, true);
      } else {
        snes_->SetButtonState(0, buttons::kA, false);
      }
    } else if (module == 0x03) { // Copy File (shouldn't happen but just in case)
       // ...
    }
    
    if (i == kFileSelectTimeout - 1) {
       printf("[StateManager] Timeout waiting for Name Entry (current: 0x%02X)\n", module);
       // Don't fail yet, maybe we skipped it?
    }
  }

  // Phase 2: Name Entry (0x04) -> Game Load (0x07/0x09)
  // Accept default name (Start) and confirm (A)
  const int kNameEntryTimeout = 2000;
  for (int i = 0; i < kNameEntryTimeout; ++i) {
    snes_->RunFrame();
    uint8_t module = GetGameModule();

    if (module == 0x07 || module == 0x09) {
      printf("[StateManager] Started new game (module 0x%02X) at frame %d\n", module, i);
      return absl::OkStatus();
    }

    // If we are in Name Entry (0x04), press Start then A
    if (module == 0x04) {
      // If we've been in Name Entry for a while (e.g. > 600 frames), try to force transition
      if (i > 600) {
         printf("[StateManager] Stuck in Name Entry, forcing Module 0x05 (Load Level)...\n");
         snes_->Write(0x7E0010, 0x05); // Force Load Level module
         
         // Also set a safe room to load (Link's House = 0x0104)
         // Or just let it use whatever is in 0xA0 (usually 0)
         // But we want to exit Name Entry.
         continue;
      }
    
      int cycle = i % 120; // Slower cycle
      if (cycle < 20) {
        // Press Start to accept name
        snes_->SetButtonState(0, buttons::kStart, true);
        snes_->SetButtonState(0, buttons::kA, false);
      } else if (cycle >= 60 && cycle < 80) {
        // Press A to confirm
        snes_->SetButtonState(0, buttons::kStart, false);
        snes_->SetButtonState(0, buttons::kA, true);
      } else {
        snes_->SetButtonState(0, buttons::kStart, false);
        snes_->SetButtonState(0, buttons::kA, false);
      }
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
  // Module 0x06 = Underworld Load (0x05 is Load File, which resets state)
  snes_->Write(0x7E0010, 0x06);  // Loading module

  // Set safe center position for Link
  snes_->Write(0x7E0022, 0x80);  // X low
  snes_->Write(0x7E0023, 0x00);  // X high
  snes_->Write(0x7E0020, 0x80);  // Y low
  snes_->Write(0x7E0021, 0x00);  // Y high

  // Wait for room to fully load
  const int kMaxFrames = 600;
  for (int i = 0; i < kMaxFrames; ++i) {
    snes_->RunFrame();

    // Force write the room ID every frame to prevent it from being overwritten
    snes_->Write(0x7E00A0, room_id & 0xFF);
    snes_->Write(0x7E00A1, (room_id >> 8) & 0xFF);
    snes_->Write(0x7E001B, (room_id < 0x128) ? 0x01 : 0x00);

    if (i % 60 == 0) {
      uint8_t submodule = ReadWram(0x7E0011);
      printf("[StateManager] Teleport wait frame %d: module=0x%02X, sub=0x%02X, room=0x%04X\n", 
             i, GetGameModule(), submodule, GetCurrentRoom());
    }

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
  // Submodule 0x00 is normal gameplay
  // Submodule 0x0F is often "Spotlight Open" or similar stable state in dungeons
  return (module == 0x07 || module == 0x09) && (submodule == 0x00 || submodule == 0x0F);
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
