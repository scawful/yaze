#ifndef YAZE_APP_EMU_RENDER_EMULATOR_RENDER_SERVICE_H_
#define YAZE_APP_EMU_RENDER_EMULATOR_RENDER_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/emu/render/render_context.h"

namespace yaze {

class Rom;
namespace zelda3 {
struct GameData;
}  // namespace zelda3

namespace emu {
class Snes;
}  // namespace emu


namespace emu {
namespace render {

class SaveStateManager;

// Rendering mode for the service
enum class RenderMode {
  kEmulated,  // Use SNES emulator with state injection
  kStatic,    // Use ObjectDrawer (fast, reliable)
  kHybrid,    // Use ObjectDrawer for objects, emulator for sprites
};

// Shared emulator-based rendering service for ALTTP entities.
//
// This service provides a unified interface for rendering dungeon objects,
// sprites, and full rooms using either SNES emulation with save state
// injection or static rendering via ObjectDrawer.
//
// The save state injection approach solves the "cold start" problem where
// ALTTP's object handlers cannot run in isolation because they expect a
// fully initialized game context.
//
// Usage:
//   EmulatorRenderService service(rom);
//   service.Initialize();
//
//   RenderRequest req;
//   req.type = RenderTargetType::kDungeonObject;
//   req.entity_id = 0x01;  // Wall object
//   req.room_id = 0x12;    // Sanctuary
//
//   auto result = service.Render(req);
//   if (result.ok()) {
//     // Use result->rgba_pixels
//   }
class EmulatorRenderService {
 public:
  explicit EmulatorRenderService(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~EmulatorRenderService();

  // Non-copyable
  EmulatorRenderService(const EmulatorRenderService&) = delete;
  EmulatorRenderService& operator=(const EmulatorRenderService&) = delete;

  // Initialize the service (creates SNES instance, loads baseline states)
  absl::Status Initialize();

  // Generate baseline save states for different game contexts
  // This may take several seconds as it boots the game via TAS input
  absl::Status GenerateBaselineStates();

  // Render a single entity
  absl::StatusOr<RenderResult> Render(const RenderRequest& request);

  // Render multiple entities (batch operation)
  absl::StatusOr<std::vector<RenderResult>> RenderBatch(
      const std::vector<RenderRequest>& requests);

  // Check if service is ready to render
  bool IsReady() const { return initialized_; }

  // Set the rendering mode
  void SetRenderMode(RenderMode mode) { render_mode_ = mode; }
  RenderMode GetRenderMode() const { return render_mode_; }

  // Access to underlying SNES instance (for advanced use)
  emu::Snes* snes() { return snes_.get(); }

  // Access to save state manager
  SaveStateManager* state_manager() { return state_manager_.get(); }

 private:
  // Render implementations for each target type
  absl::StatusOr<RenderResult> RenderDungeonObject(const RenderRequest& req);
  absl::StatusOr<RenderResult> RenderDungeonObjectStatic(
      const RenderRequest& req);
  absl::StatusOr<RenderResult> RenderSprite(const RenderRequest& req);
  absl::StatusOr<RenderResult> RenderFullRoom(const RenderRequest& req);

  // State injection helpers
  void InjectRoomContext(int room_id, uint8_t blockset, uint8_t palette);
  void LoadPaletteIntoCgram(int palette_id);
  void LoadGraphicsIntoVram(uint8_t blockset);
  void InitializeTilemapPointers();
  void ClearTilemapBuffers();
  void MockApuPorts();

  // Object handler execution
  absl::StatusOr<int> LookupHandlerAddress(int object_id, int* data_offset);
  absl::Status ExecuteHandler(int handler_addr, int data_offset,
                              int tilemap_pos);

  // PPU rendering
  void RenderPpuFrame();
  std::vector<uint8_t> ExtractPixelsFromPpu();

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  std::unique_ptr<zelda3::GameData> owned_game_data_;
  std::unique_ptr<emu::Snes> snes_;
  std::unique_ptr<SaveStateManager> state_manager_;

  RenderMode render_mode_ = RenderMode::kHybrid;
  bool initialized_ = false;
};

}  // namespace render
}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_EMU_RENDER_EMULATOR_RENDER_SERVICE_H_
