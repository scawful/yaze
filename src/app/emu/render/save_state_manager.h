#ifndef YAZE_APP_EMU_RENDER_SAVE_STATE_MANAGER_H_
#define YAZE_APP_EMU_RENDER_SAVE_STATE_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/emu/render/render_context.h"

namespace yaze {

class Rom;

namespace emu {
class Snes;
}  // namespace emu

namespace emu {
namespace render {

// Manages save states for the emulator render service.
//
// This class handles:
// 1. Loading pre-generated baseline states (shipped with yaze)
// 2. Generating new states via TAS-style game boot
// 3. Verifying ROM compatibility via checksum
// 4. Caching states for reuse
//
// The save state approach allows rendering of objects/sprites by loading
// a known "game ready" state and injecting the specific entity data,
// avoiding the "cold start" problem where handlers expect full game context.
class SaveStateManager {
 public:
  SaveStateManager(emu::Snes* snes, Rom* rom);
  ~SaveStateManager();

  // Initialize the manager (checks for existing states, calculates ROM checksum)
  absl::Status Initialize();

  // Load a baseline state of the given type
  // If context_id is provided, loads state for specific room/area
  absl::Status LoadState(StateType type, int context_id = 0);

  // Generate a new state by booting the game to the specified room
  // This is slow (~5-30 seconds) as it runs the game via TAS input
  absl::Status GenerateRoomState(int room_id);

  // Generate all baseline states
  absl::Status GenerateAllBaselineStates();

  // Check if a cached state exists for the given type/context
  bool HasCachedState(StateType type, int context_id = 0) const;

  // Get metadata for a cached state
  absl::StatusOr<StateMetadata> GetStateMetadata(StateType type,
                                                  int context_id = 0) const;

  // Save current SNES state to a file
  absl::Status SaveStateToFile(const std::string& path,
                               const StateMetadata& metadata);

  // Load SNES state from a file
  absl::Status LoadStateFromFile(const std::string& path);

  // Get/set the base directory for state files
  void SetStateDirectory(const std::string& path) { state_directory_ = path; }
  const std::string& GetStateDirectory() const { return state_directory_; }

  // Calculate CRC32 checksum of ROM
  uint32_t CalculateRomChecksum() const;

 private:
  // TAS-style game boot helpers
  absl::Status BootToTitleScreen();
  absl::Status NavigateToFileSelect();
  absl::Status StartNewGame();
  absl::Status NavigateToRoom(int room_id);

  // Input injection
  void PressButton(int button, int frames = 1);
  void ReleaseAllButtons();
  void WaitFrames(int frames);

  // Module waiting helpers
  bool WaitForModule(uint8_t target, int max_frames);
  absl::Status TeleportToRoomViaWram(int room_id);
  absl::Status NavigateToRoomViaTas(int room_id);

  // WRAM monitoring
  uint8_t ReadWram(uint32_t addr);
  uint16_t ReadWram16(uint32_t addr);
  int GetCurrentRoom();
  uint8_t GetGameModule();
  bool IsRoomFullyLoaded();

  // State file path generation
  std::string GetStatePath(StateType type, int context_id) const;
  std::string GetMetadataPath(StateType type, int context_id) const;

  // Verify state compatibility with current ROM
  bool IsStateCompatible(const StateMetadata& metadata) const;

  emu::Snes* snes_ = nullptr;
  Rom* rom_ = nullptr;

  std::string state_directory_;
  uint32_t rom_checksum_ = 0;

  // Cache of loaded state metadata
  struct CacheKey {
    StateType type;
    int context_id;
    bool operator==(const CacheKey& other) const {
      return type == other.type && context_id == other.context_id;
    }
  };
  struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
      return std::hash<int>()(static_cast<int>(k.type)) ^
             (std::hash<int>()(k.context_id) << 1);
    }
  };
  std::unordered_map<CacheKey, StateMetadata, CacheKeyHash> state_cache_;
};

// Button constants for input injection (SNES controller bit indices)
// These map to Snes::SetButtonState() which expects bit indices 0-11
namespace buttons {
constexpr int kB = 0;       // Bit 0
constexpr int kY = 1;       // Bit 1
constexpr int kSelect = 2;  // Bit 2
constexpr int kStart = 3;   // Bit 3
constexpr int kUp = 4;      // Bit 4
constexpr int kDown = 5;    // Bit 5
constexpr int kLeft = 6;    // Bit 6
constexpr int kRight = 7;   // Bit 7
constexpr int kA = 8;       // Bit 8
constexpr int kX = 9;       // Bit 9
constexpr int kL = 10;      // Bit 10
constexpr int kR = 11;      // Bit 11
}  // namespace buttons

}  // namespace render
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_RENDER_SAVE_STATE_MANAGER_H_
