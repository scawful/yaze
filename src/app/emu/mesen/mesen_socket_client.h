#ifndef YAZE_APP_EMU_MESEN_MESEN_SOCKET_CLIENT_H_
#define YAZE_APP_EMU_MESEN_MESEN_SOCKET_CLIENT_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace emu {
namespace mesen {

/**
 * @brief CPU register state from Mesen2
 */
struct CpuState {
  uint16_t A;
  uint16_t X;
  uint16_t Y;
  uint16_t SP;
  uint16_t D;   // Direct page
  uint32_t PC;
  uint8_t K;    // Program bank
  uint8_t DBR;  // Data bank
  uint8_t P;    // Processor status
  bool emulation_mode;
};

/**
 * @brief Emulation state from Mesen2
 */
struct MesenState {
  bool running;
  bool paused;
  bool debugging;
  uint64_t frame;
  double fps;
  int console_type;
};

/**
 * @brief ALTTP Link state
 */
struct LinkState {
  uint16_t x;
  uint16_t y;
  uint8_t layer;
  uint8_t direction;  // 0=up, 2=down, 4=left, 6=right
  uint8_t state;
  uint8_t pose;
};

/**
 * @brief ALTTP health/items state
 */
struct GameItems {
  uint8_t current_health;
  uint8_t max_health;
  uint8_t magic;
  uint16_t rupees;
  uint8_t bombs;
  uint8_t arrows;
};

/**
 * @brief ALTTP game mode state
 */
struct GameMode {
  uint8_t mode;
  uint8_t submode;
  bool indoors;
  uint16_t room_id;       // Valid when indoors
  uint8_t overworld_area; // Valid when !indoors
};

/**
 * @brief Complete ALTTP game state from GAMESTATE command
 */
struct GameState {
  LinkState link;
  GameItems items;
  GameMode game;
};

/**
 * @brief Sprite information from SPRITES command
 */
struct SpriteInfo {
  int slot;
  uint8_t type;
  uint8_t state;
  uint16_t x;
  uint16_t y;
  uint8_t health;
  uint8_t subtype;
};

/**
 * @brief Breakpoint types
 */
enum class BreakpointType {
  kExecute,
  kRead,
  kWrite,
  kReadWrite
};

/**
 * @brief Event from Mesen2 subscription
 */
struct MesenEvent {
  std::string type;      // "breakpoint_hit", "frame_complete", etc.
  std::string raw_json;  // Full event payload
  uint32_t address;      // For breakpoint events
  uint64_t frame;        // Frame number when event occurred
};

using EventCallback = std::function<void(const MesenEvent&)>;
using EventListenerId = uint64_t;

/**
 * @brief Unix socket client for Mesen2-OoS fork
 *
 * Connects to Mesen2's socket API at /tmp/mesen2-<pid>.sock
 * and provides type-safe wrapper methods for all commands.
 */
class MesenSocketClient {
 public:
  MesenSocketClient();
  ~MesenSocketClient();

  // Disable copy
  MesenSocketClient(const MesenSocketClient&) = delete;
  MesenSocketClient& operator=(const MesenSocketClient&) = delete;

  // ──────────────────────────────────────────────────────────────────────────
  // Connection Management
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Auto-discover and connect to first available Mesen2 socket
   * @return Status indicating success or error
   */
  absl::Status Connect();

  /**
   * @brief Connect to a specific socket path
   * @param socket_path Full path to Unix socket (e.g., /tmp/mesen2-12345.sock)
   */
  absl::Status Connect(const std::string& socket_path);

  /**
   * @brief Disconnect from Mesen2
   */
  void Disconnect();

  /**
   * @brief Check if connected to Mesen2
   */
  bool IsConnected() const;

  /**
   * @brief Get the current socket path
   */
  const std::string& GetSocketPath() const { return socket_path_; }

  /**
   * @brief List available Mesen2 sockets on the system
   */
  static std::vector<std::string> ListAvailableSockets();

  // ──────────────────────────────────────────────────────────────────────────
  // Control Commands
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Ping Mesen2 to check connectivity
   */
  absl::Status Ping();

  /**
   * @brief Get current emulation state
   */
  absl::StatusOr<MesenState> GetState();

  /**
   * @brief Pause emulation
   */
  absl::Status Pause();

  /**
   * @brief Resume emulation
   */
  absl::Status Resume();

  /**
   * @brief Reset the console
   */
  absl::Status Reset();

  /**
   * @brief Run exactly one frame
   */
  absl::Status Frame();

  /**
   * @brief Step N CPU instructions (default 1)
   */
  absl::Status Step(int count = 1);

  // ──────────────────────────────────────────────────────────────────────────
  // Memory Access
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Read a single byte from memory
   */
  absl::StatusOr<uint8_t> ReadByte(uint32_t addr);

  /**
   * @brief Read a 16-bit word from memory
   */
  absl::StatusOr<uint16_t> ReadWord(uint32_t addr);

  /**
   * @brief Read a block of bytes from memory
   */
  absl::StatusOr<std::vector<uint8_t>> ReadBlock(uint32_t addr, size_t len);

  /**
   * @brief Write a single byte to memory
   */
  absl::Status WriteByte(uint32_t addr, uint8_t value);

  /**
   * @brief Write a 16-bit word to memory
   */
  absl::Status WriteWord(uint32_t addr, uint16_t value);

  /**
   * @brief Write a block of bytes to memory
   */
  absl::Status WriteBlock(uint32_t addr, const std::vector<uint8_t>& data);

  // ──────────────────────────────────────────────────────────────────────────
  // Debugging Commands
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Get CPU register state
   */
  absl::StatusOr<CpuState> GetCpuState();

  /**
   * @brief Disassemble instructions at address
   */
  absl::StatusOr<std::string> Disassemble(uint32_t addr, int count = 10);

  /**
   * @brief Add a breakpoint
   * @return Breakpoint ID
   */
  absl::StatusOr<int> AddBreakpoint(uint32_t addr, BreakpointType type,
                                     const std::string& condition = "");

  /**
   * @brief Remove a breakpoint by ID
   */
  absl::Status RemoveBreakpoint(int id);

  /**
   * @brief Clear all breakpoints
   */
  absl::Status ClearBreakpoints();

  /**
   * @brief Get execution trace log
   */
  absl::StatusOr<std::string> GetTrace(int count = 20);

  // ──────────────────────────────────────────────────────────────────────────
  // ALTTP-Specific Commands
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Get comprehensive ALTTP game state
   */
  absl::StatusOr<GameState> GetGameState();

  /**
   * @brief Get active sprites
   * @param all If true, include inactive sprites
   */
  absl::StatusOr<std::vector<SpriteInfo>> GetSprites(bool all = false);

  /**
   * @brief Enable/disable collision overlay
   */
  absl::Status SetCollisionOverlay(bool enable, const std::string& colmap = "A");

  // ──────────────────────────────────────────────────────────────────────────
  // Save State Commands
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Save state to slot
   */
  absl::Status SaveState(int slot);

  /**
   * @brief Load state from slot
   */
  absl::Status LoadState(int slot);

  /**
   * @brief Take a screenshot
   * @return Base64-encoded PNG data
   */
  absl::StatusOr<std::string> Screenshot();

  // ──────────────────────────────────────────────────────────────────────────
  // Event Subscription
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Subscribe to events
   * @param events Event types: "breakpoint_hit", "frame_complete", "all", etc.
   */
  absl::Status Subscribe(const std::vector<std::string>& events);

  /**
   * @brief Unsubscribe from events
   */
  absl::Status Unsubscribe();

  /**
   * @brief Set callback for received events
   */
  void SetEventCallback(EventCallback callback);

  /**
   * @brief Add an event listener without replacing existing listeners.
   * @return Listener ID used for removal.
   */
  EventListenerId AddEventListener(EventCallback callback);

  /**
   * @brief Remove a previously added event listener.
   */
  void RemoveEventListener(EventListenerId id);

  // ──────────────────────────────────────────────────────────────────────────
  // Low-Level Commands
  // ──────────────────────────────────────────────────────────────────────────

  /**
   * @brief Send a raw JSON command and get raw response
   */
  absl::StatusOr<std::string> SendCommand(const std::string& json);

 private:
  /**
   * @brief Find available Mesen2 socket paths
   */
  static std::vector<std::string> FindSocketPaths();

  /**
   * @brief Parse JSON response for success/error
   */
  absl::StatusOr<std::string> ParseResponse(const std::string& response);

  /**
   * @brief Send a command using a specific socket descriptor
   */
  absl::StatusOr<std::string> SendCommandOnSocket(int fd,
                                                  const std::string& json,
                                                  bool update_connection_state);

  /**
   * @brief Event listening thread function
   */
  void EventLoop();

  int socket_fd_ = -1;
  int event_socket_fd_ = -1;
  std::string socket_path_;
  std::atomic<bool> connected_{false};
  std::mutex command_mutex_;
  std::mutex event_callback_mutex_;

  // Event handling
  EventCallback event_callback_;
  std::unordered_map<EventListenerId, EventCallback> event_listeners_;
  EventListenerId next_event_listener_id_ = 1;
  std::thread event_thread_;
  std::atomic<bool> event_thread_running_{false};
  std::string pending_event_payload_;
};

}  // namespace mesen
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_MESEN_MESEN_SOCKET_CLIENT_H_
