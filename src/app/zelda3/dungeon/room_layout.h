#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Represents a room layout object (wall, floor, etc.)
 * 
 * Room layout objects are the basic building blocks of dungeon rooms.
 * They include walls, floors, ceilings, and other structural elements.
 * Unlike regular room objects, these are loaded from the room layout data
 * and represent the fundamental geometry of the room.
 */
class RoomLayoutObject {
 public:
  enum class Type {
    kWall = 0,
    kFloor = 1,
    kCeiling = 2,
    kPit = 3,
    kWater = 4,
    kStairs = 5,
    kDoor = 6,
    kUnknown = 7
  };

  RoomLayoutObject(int16_t id, uint8_t x, uint8_t y, Type type, uint8_t layer = 0)
      : id_(id), x_(x), y_(y), type_(type), layer_(layer) {}

  // Getters
  int16_t id() const { return id_; }
  uint8_t x() const { return x_; }
  uint8_t y() const { return y_; }
  Type type() const { return type_; }
  uint8_t layer() const { return layer_; }
  
  // Setters
  void set_id(int16_t id) { id_ = id; }
  void set_x(uint8_t x) { x_ = x; }
  void set_y(uint8_t y) { y_ = y; }
  void set_type(Type type) { type_ = type; }
  void set_layer(uint8_t layer) { layer_ = layer; }

  // Get tile data for this layout object
  // NOTE: Layout codes need to be mapped to actual VRAM tile IDs
  // The room_gfx_buffer provides the assembled graphics for this specific room
  absl::StatusOr<gfx::Tile16> GetTile(const uint8_t* room_gfx_buffer = nullptr) const;
  
  // Get the name/description of this layout object type
  std::string GetTypeName() const;

 private:
  int16_t id_;
  uint8_t x_;
  uint8_t y_;
  Type type_;
  uint8_t layer_;
};

/**
 * @brief Manages room layout data and objects
 * 
 * This class handles loading and managing room layout objects from ROM data.
 * It provides efficient access to wall, floor, and other layout elements
 * without copying large amounts of data.
 */
class RoomLayout {
 public:
  RoomLayout() = default;
  explicit RoomLayout(Rom* rom) : rom_(rom) {}

  // Load layout data from ROM for a specific room
  absl::Status LoadLayout(int room_id);
  
  // Load layout data from a specific address
  absl::Status LoadLayoutData(int layout_data_address);
  
  // Get all layout objects of a specific type
  std::vector<RoomLayoutObject> GetObjectsByType(RoomLayoutObject::Type type) const;
  
  // Get layout object at specific coordinates
  absl::StatusOr<RoomLayoutObject> GetObjectAt(uint8_t x, uint8_t y, uint8_t layer = 0) const;
  
  // Get all layout objects
  const std::vector<RoomLayoutObject>& GetObjects() const { return objects_; }
  
  // Check if a position has a wall
  bool HasWall(uint8_t x, uint8_t y, uint8_t layer = 0) const;
  
  // Check if a position has a floor
  bool HasFloor(uint8_t x, uint8_t y, uint8_t layer = 0) const;
  
  // Get room dimensions
  std::pair<uint8_t, uint8_t> GetDimensions() const { return {width_, height_}; }

 private:
  Rom* rom_ = nullptr;
  std::vector<RoomLayoutObject> objects_;
  uint8_t width_ = 16;  // Default room width in tiles
  uint8_t height_ = 11; // Default room height in tiles
  
  // Parse layout data from ROM
  absl::Status ParseLayoutData(const std::vector<uint8_t>& data);
  
  // Create layout object from tile data
  RoomLayoutObject CreateLayoutObject(int16_t tile_id, uint8_t x, uint8_t y, uint8_t layer);
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H
