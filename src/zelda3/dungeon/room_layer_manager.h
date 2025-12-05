#ifndef YAZE_ZELDA3_DUNGEON_ROOM_LAYER_MANAGER_H
#define YAZE_ZELDA3_DUNGEON_ROOM_LAYER_MANAGER_H

#include <array>
#include <cstdint>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/background_buffer.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Layer types for the 4-way visibility system
 *
 * The SNES dungeon rendering uses two background layers (BG1, BG2).
 * Each layer has two components:
 * - Layout: Base tiles from the room layout (floor, walls)
 * - Objects: Tiles drawn by room objects (pots, blocks, doors)
 */
enum class LayerType {
  BG1_Layout,   // Base BG1 tiles from room layout
  BG1_Objects,  // Objects drawn to BG1 (Layer 0, 2)
  BG2_Layout,   // Base BG2 tiles from room layout
  BG2_Objects   // Objects drawn to BG2 (Layer 1)
};

/**
 * @brief Layer blend modes for compositing
 */
enum class LayerBlendMode {
  Normal,       // Standard alpha blending
  Translucent,  // 50% alpha
  Addition,     // Additive blending
  Dark,         // Darkened blend
  Off           // Layer hidden
};

/**
 * @brief Per-object translucency settings
 */
struct ObjectTranslucency {
  size_t object_index = 0;
  bool translucent = false;
  uint8_t alpha = 255;  // 0-255 alpha value
};

/**
 * @brief Object priority for rendering order within merged layers
 */
struct ObjectPriority {
  size_t object_index = 0;
  int layer = 0;          // Object's layer (0, 1, 2)
  int priority = 0;       // Render priority (higher = on top)
  bool is_bg2_object = false;  // True if object renders to BG2
};

/**
 * @brief RoomLayerManager - Manages layer visibility and compositing
 *
 * This class provides:
 * - 4-way layer visibility (BG1_Layout, BG1_Objects, BG2_Layout, BG2_Objects)
 * - Layer blend mode control
 * - Per-object translucency settings
 * - Compositing layers to a final output bitmap
 *
 * Usage:
 *   RoomLayerManager manager;
 *   manager.SetLayerVisible(LayerType::BG1_Objects, false);  // Hide objects
 *   manager.SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Translucent);
 *   manager.CompositeToOutput(room, output_bitmap);
 */
class RoomLayerManager {
 public:
  RoomLayerManager() { Reset(); }

  // Reset to default state (all layers visible, normal blend)
  void Reset() {
    for (int i = 0; i < 4; ++i) {
      layer_visible_[i] = true;
      layer_blend_mode_[i] = LayerBlendMode::Normal;
      layer_alpha_[i] = 255;
    }
    object_translucency_.clear();
    bg2_on_top_ = false;
    layers_merged_ = false;
    current_merge_type_id_ = 0;
  }

  // Layer visibility
  void SetLayerVisible(LayerType layer, bool visible) {
    layer_visible_[static_cast<int>(layer)] = visible;
  }

  bool IsLayerVisible(LayerType layer) const {
    return layer_visible_[static_cast<int>(layer)];
  }

  // Layer blend mode
  void SetLayerBlendMode(LayerType layer, LayerBlendMode mode) {
    layer_blend_mode_[static_cast<int>(layer)] = mode;
    // Update alpha based on blend mode
    switch (mode) {
      case LayerBlendMode::Normal:
        layer_alpha_[static_cast<int>(layer)] = 255;
        break;
      case LayerBlendMode::Translucent:
        layer_alpha_[static_cast<int>(layer)] = 180;
        break;
      case LayerBlendMode::Addition:
        layer_alpha_[static_cast<int>(layer)] = 220;
        break;
      case LayerBlendMode::Dark:
        layer_alpha_[static_cast<int>(layer)] = 120;
        break;
      case LayerBlendMode::Off:
        layer_alpha_[static_cast<int>(layer)] = 0;
        break;
    }
  }

  LayerBlendMode GetLayerBlendMode(LayerType layer) const {
    return layer_blend_mode_[static_cast<int>(layer)];
  }

  uint8_t GetLayerAlpha(LayerType layer) const {
    return layer_alpha_[static_cast<int>(layer)];
  }

  // Per-object translucency
  void SetObjectTranslucency(size_t object_index, bool translucent,
                             uint8_t alpha = 128) {
    // Find existing entry or add new one
    for (auto& entry : object_translucency_) {
      if (entry.object_index == object_index) {
        entry.translucent = translucent;
        entry.alpha = alpha;
        return;
      }
    }
    object_translucency_.push_back({object_index, translucent, alpha});
  }

  bool IsObjectTranslucent(size_t object_index) const {
    for (const auto& entry : object_translucency_) {
      if (entry.object_index == object_index) {
        return entry.translucent;
      }
    }
    return false;
  }

  uint8_t GetObjectAlpha(size_t object_index) const {
    for (const auto& entry : object_translucency_) {
      if (entry.object_index == object_index && entry.translucent) {
        return entry.alpha;
      }
    }
    return 255;
  }

  void ClearObjectTranslucency() { object_translucency_.clear(); }

  // BG2 ordering (from LayerMergeType)
  void SetBG2OnTop(bool on_top) { bg2_on_top_ = on_top; }
  bool IsBG2OnTop() const { return bg2_on_top_; }

  // Apply layer settings to room from LayerMergeType
  // NOTE: This only affects blend modes and ordering, NOT visibility.
  // Layer visibility checkboxes remain independent of merge type.
  void ApplyLayerMerging(const LayerMergeType& merge_type) {
    // Store the current merge type for queries
    current_merge_type_id_ = merge_type.ID;
    layers_merged_ = (merge_type.ID != 0);  // ID 0 = "Off" = not merged

    // Set BG2 ordering (on top or below BG1)
    SetBG2OnTop(merge_type.Layer2OnTop);

    // Apply blend mode based on merge type
    // Layer2Visible = false means BG2 should not be composited (hidden by ROM)
    // Layer2Translucent = true means BG2 should use translucent blend
    if (!merge_type.Layer2Visible) {
      // ROM says BG2 is disabled for this merge type
      SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Off);
      SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Off);
    } else if (merge_type.Layer2Translucent) {
      SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Translucent);
      SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Translucent);
    } else {
      SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Normal);
      SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Normal);
    }

    // BG1 always uses normal blend mode
    SetLayerBlendMode(LayerType::BG1_Layout, LayerBlendMode::Normal);
    SetLayerBlendMode(LayerType::BG1_Objects, LayerBlendMode::Normal);
  }

  /**
   * @brief Apply layer merge settings without changing visibility
   * 
   * Use this when you want to update blend/ordering from ROM data
   * but preserve the user's manual visibility overrides.
   */
  void ApplyLayerMergingPreserveVisibility(const LayerMergeType& merge_type) {
    // Store visibility before applying
    bool bg1_layout_vis = IsLayerVisible(LayerType::BG1_Layout);
    bool bg1_objects_vis = IsLayerVisible(LayerType::BG1_Objects);
    bool bg2_layout_vis = IsLayerVisible(LayerType::BG2_Layout);
    bool bg2_objects_vis = IsLayerVisible(LayerType::BG2_Objects);

    // Apply merge settings
    ApplyLayerMerging(merge_type);

    // Restore visibility
    SetLayerVisible(LayerType::BG1_Layout, bg1_layout_vis);
    SetLayerVisible(LayerType::BG1_Objects, bg1_objects_vis);
    SetLayerVisible(LayerType::BG2_Layout, bg2_layout_vis);
    SetLayerVisible(LayerType::BG2_Objects, bg2_objects_vis);
  }

  /**
   * @brief Check if layers are currently merged
   * @return true if any layer merging is active
   */
  bool AreLayersMerged() const { return layers_merged_; }

  /**
   * @brief Get the current merge type ID
   * @return The LayerMergeType ID (0-8)
   */
  uint8_t GetMergeTypeId() const { return current_merge_type_id_; }

  // ============================================================================
  // Object Priority for Merged Layers
  // ============================================================================

  /**
   * @brief Calculate object priority for render ordering
   *
   * When layers are merged, objects need priority ordering to ensure
   * proper visibility. BG2 objects should render above BG1 objects
   * when BG2 is on top.
   *
   * @param object_layer The object's layer value (0, 1, 2)
   * @return Priority value (higher = renders on top)
   */
  int CalculateObjectPriority(int object_layer) const {
    // Base priority from object layer
    int priority = object_layer * 10;

    // If BG2 is on top and this is a layer 1 (BG2) object, boost priority
    if (bg2_on_top_ && object_layer == 1) {
      priority += 100;
    }

    // If layers are merged, layer 0 and 2 objects (BG1) get lower priority
    // when BG2 is on top
    if (layers_merged_ && bg2_on_top_ && (object_layer == 0 || object_layer == 2)) {
      priority -= 50;
    }

    return priority;
  }

  /**
   * @brief Check if an object on a given layer should render to BG2
   * @param object_layer The object's layer value
   * @return true if object renders to BG2 buffer
   */
  static bool IsObjectOnBG2(int object_layer) {
    // Layer 1 objects render to BG2, layers 0 and 2 render to BG1
    return object_layer == 1;
  }

  /**
   * @brief Get the appropriate background layer type for an object
   * @param object_layer The object's layer value (0, 1, 2)
   * @return The LayerType for this object's background
   */
  static LayerType GetObjectLayerType(int object_layer) {
    return IsObjectOnBG2(object_layer) ? LayerType::BG2_Objects
                                       : LayerType::BG1_Objects;
  }

  /**
   * @brief Get the draw order for layers
   *
   * Returns layers in the order they should be drawn (back to front).
   * BG2 layers are drawn either before or after BG1 based on bg2_on_top_.
   * Within each BG, layout is drawn before objects.
   */
  std::array<LayerType, 4> GetDrawOrder() const {
    if (bg2_on_top_) {
      return {LayerType::BG1_Layout, LayerType::BG1_Objects,
              LayerType::BG2_Layout, LayerType::BG2_Objects};
    } else {
      return {LayerType::BG2_Layout, LayerType::BG2_Objects,
              LayerType::BG1_Layout, LayerType::BG1_Objects};
    }
  }

  /**
   * @brief Get the bitmap buffer for a layer type
   */
  static gfx::BackgroundBuffer& GetLayerBuffer(Room& room, LayerType layer) {
    switch (layer) {
      case LayerType::BG1_Layout:
        return room.bg1_buffer();
      case LayerType::BG1_Objects:
        return room.object_bg1_buffer();
      case LayerType::BG2_Layout:
        return room.bg2_buffer();
      case LayerType::BG2_Objects:
        return room.object_bg2_buffer();
    }
    // Fallback (should never reach)
    return room.bg1_buffer();
  }

  static const gfx::BackgroundBuffer& GetLayerBuffer(const Room& room,
                                                     LayerType layer) {
    switch (layer) {
      case LayerType::BG1_Layout:
        return room.bg1_buffer();
      case LayerType::BG1_Objects:
        return room.object_bg1_buffer();
      case LayerType::BG2_Layout:
        return room.bg2_buffer();
      case LayerType::BG2_Objects:
        return room.object_bg2_buffer();
    }
    // Fallback (should never reach)
    return room.bg1_buffer();
  }

  /**
   * @brief Get human-readable name for layer type
   */
  static const char* GetLayerName(LayerType layer) {
    switch (layer) {
      case LayerType::BG1_Layout:
        return "BG1 Layout";
      case LayerType::BG1_Objects:
        return "BG1 Objects";
      case LayerType::BG2_Layout:
        return "BG2 Layout";
      case LayerType::BG2_Objects:
        return "BG2 Objects";
    }
    return "Unknown";
  }

  /**
   * @brief Get blend mode name
   */
  static const char* GetBlendModeName(LayerBlendMode mode) {
    switch (mode) {
      case LayerBlendMode::Normal:
        return "Normal";
      case LayerBlendMode::Translucent:
        return "Translucent";
      case LayerBlendMode::Addition:
        return "Addition";
      case LayerBlendMode::Dark:
        return "Dark";
      case LayerBlendMode::Off:
        return "Off";
    }
    return "Unknown";
  }

  // ============================================================================
  // Layer Compositing
  // ============================================================================

  /**
   * @brief Composite all visible layers into a single output bitmap
   *
   * Merges layers in correct draw order based on GetDrawOrder().
   * Applies per-layer visibility and blend modes at pixel level.
   * Transparency is detected via palette index 0 (SNES standard) or 255
   * (object buffer fill color).
   *
   * @note PALETTE HANDLING: Room layer buffers have their palettes applied
   * via SetPalette(vector<SDL_Color>), which means their internal SnesPalette
   * (accessible via bitmap.palette()) is EMPTY. This method extracts the
   * palette directly from the first visible layer's SDL surface and applies
   * it to the output bitmap. See the "Bitmap Dual Palette System" section in
   * docs/public/developer/palette-system-overview.md for details.
   *
   * @param room The room containing the layer buffers
   * @param output Output bitmap to receive composited result (512x512)
   *
   * @see ApplySDLPaletteToBitmap() for the palette extraction implementation
   */
  void CompositeToOutput(Room& room, gfx::Bitmap& output) const;

 private:
  /**
   * @brief Check if a pixel index represents transparency
   */
  static bool IsTransparent(uint8_t pixel) {
    return pixel == 0 || pixel == 255;
  }

  /**
   * @brief Composite a single layer onto the output bitmap
   */
  void CompositeLayer(const gfx::Bitmap& src, gfx::Bitmap& dst,
                      LayerBlendMode mode) const;
  std::array<bool, 4> layer_visible_;
  std::array<LayerBlendMode, 4> layer_blend_mode_;
  std::array<uint8_t, 4> layer_alpha_;
  std::vector<ObjectTranslucency> object_translucency_;
  bool bg2_on_top_ = false;

  // Merge state tracking
  bool layers_merged_ = false;
  uint8_t current_merge_type_id_ = 0;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_LAYER_MANAGER_H
