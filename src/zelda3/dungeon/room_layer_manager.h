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
 * @brief Object metadata for tracking layer assignment
 * 
 * Note: In SNES Mode 1, BG1 is always above BG2 by default.
 * The "priority" field here refers to object layer (0, 1, 2),
 * not visual Z-order, which is fixed by the SNES PPU.
 */
struct ObjectPriority {
  size_t object_index = 0;
  int layer = 0;          // Object's layer (0=BG1, 1=BG2, 2=BG1 priority)
  int priority = 0;       // Object layer value (not visual Z-order)
  bool is_bg2_object = false;  // True if object renders to BG2 buffer
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
    use_priority_compositing_ = true;  // Default to accurate SNES behavior
  }

  // Priority compositing control
  // When enabled (default): Uses SNES Mode 1 per-tile priority for Z-ordering
  // When disabled: Simple back-to-front layer order (BG2 behind, BG1 in front)
  void SetPriorityCompositing(bool enabled) { use_priority_compositing_ = enabled; }
  bool IsPriorityCompositingEnabled() const { return use_priority_compositing_; }

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

  // Color math participation flag (from LayerMergeType.Layer2OnTop)
  // NOTE: This does NOT affect draw order - BG1 is always above BG2.
  // This flag controls whether BG2 participates in sub-screen color math
  // effects like transparency and additive blending.
  void SetBG2ColorMathEnabled(bool enabled) { bg2_on_top_ = enabled; }
  bool IsBG2ColorMathEnabled() const { return bg2_on_top_; }
  
  // Legacy aliases for compatibility
  void SetBG2OnTop(bool on_top) { bg2_on_top_ = on_top; }
  bool IsBG2OnTop() const { return bg2_on_top_; }

  // Apply layer settings to room from LayerMergeType
  // NOTE: This affects BLEND MODES and COLOR MATH, not draw order.
  // SNES Mode 1 always renders BG1 above BG2 - this is hardware behavior.
  // Layer visibility checkboxes remain independent of merge type.
  void ApplyLayerMerging(const LayerMergeType& merge_type) {
    // Store the current merge type for queries
    current_merge_type_id_ = merge_type.ID;
    layers_merged_ = (merge_type.ID != 0);  // ID 0 = "Off" = not merged

    // Set BG2 color math participation (does NOT change draw order)
    SetBG2ColorMathEnabled(merge_type.Layer2OnTop);

    // Apply blend mode based on merge type
    // NOTE: Layer2Visible from ROM is informational only - user can still
    // enable/disable layers via checkboxes. We only set blend modes here.
    // Layer2Translucent = true means BG2 should use translucent blend
    if (merge_type.Layer2Translucent) {
      SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Translucent);
      SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Translucent);
    } else {
      SetLayerBlendMode(LayerType::BG2_Layout, LayerBlendMode::Normal);
      SetLayerBlendMode(LayerType::BG2_Objects, LayerBlendMode::Normal);
    }

    // BG1 blend mode depends on merge type
    // DarkRoom (ID 0x08) should darken BG1 to simulate unlit room
    if (merge_type.ID == 0x08) {
      // Dark room - BG1 is dimmed (reduced brightness)
      SetLayerBlendMode(LayerType::BG1_Layout, LayerBlendMode::Dark);
      SetLayerBlendMode(LayerType::BG1_Objects, LayerBlendMode::Dark);
    } else {
      SetLayerBlendMode(LayerType::BG1_Layout, LayerBlendMode::Normal);
      SetLayerBlendMode(LayerType::BG1_Objects, LayerBlendMode::Normal);
    }
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
  // Object Layer Assignment
  // ============================================================================

  /**
   * @brief Get object layer value for buffer assignment
   *
   * Objects are assigned to buffers based on their layer value:
   * - Layer 0: BG1 buffer (main floor/walls)
   * - Layer 1: BG2 buffer (background details)
   * - Layer 2: BG1 buffer with priority (overlays on BG1)
   *
   * NOTE: Visual Z-order is fixed by SNES Mode 1 hardware (BG1 > BG2).
   * This layer value only determines which buffer the object draws to,
   * not the visual stacking order.
   *
   * @param object_layer The object's layer value (0, 1, 2)
   * @return Layer value (same as input - used for buffer routing)
   */
  int GetObjectLayerValue(int object_layer) const {
    return object_layer;
  }
  
  // Legacy function - kept for API compatibility
  // No longer affects visual order since SNES Mode 1 is fixed (BG1 > BG2)
  int CalculateObjectPriority(int object_layer) const {
    return object_layer * 10;
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
   * 
   * SNES Mode 1 default priority: BG1 > BG2 (BG1 on top)
   * The "Layer2OnTop" flag from ROM controls COLOR MATH effects
   * (sub-screen participation for transparency/additive blend),
   * NOT the actual Z-order of opaque pixels.
   * 
   * Draw order is ALWAYS: BG2 (back) -> BG1 (front)
   * Blend modes handle the visual effects separately.
   */
  std::array<LayerType, 4> GetDrawOrder() const {
    // Standard SNES Mode 1 order: BG2 behind BG1
    // bg2_on_top_ affects blend modes, not draw order
    return {LayerType::BG2_Layout, LayerType::BG2_Objects,
            LayerType::BG1_Layout, LayerType::BG1_Objects};
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

  /**
   * @brief Apply surface color modulation for DarkRoom effect
   *
   * In DarkRoom mode (merge type 0x08), surfaces should be darkened to 50%.
   * This should be called before drawing each layer in separate mode to
   * match the composite mode appearance.
   *
   * @param surface The SDL surface to apply modulation to
   */
  void ApplySurfaceColorMod(SDL_Surface* surface) const {
    if (!surface) return;

    if (current_merge_type_id_ == 0x08) {
      // DarkRoom: 50% brightness
      SDL_SetSurfaceColorMod(surface, 128, 128, 128);
    } else {
      // Normal: Full brightness
      SDL_SetSurfaceColorMod(surface, 255, 255, 255);
    }
  }

  // ============================================================================
  // Layer Compositing
  // ============================================================================

  /**
   * @brief Composite all visible layers into a single output bitmap
   *
   * Implements SNES Mode 1 per-tile priority compositing. Each tile's priority
   * bit affects its effective Z-order:
   *
   * | Layer | Priority | Effective Order |
   * |-------|----------|-----------------|
   * | BG1   | 0        | 0 (back)        |
   * | BG2   | 0        | 1               |
   * | BG2   | 1        | 2               |
   * | BG1   | 1        | 3 (front)       |
   *
   * This allows BG2 tiles with priority 1 to appear above BG1 tiles with
   * priority 0, matching authentic SNES hardware behavior.
   *
   * Applies per-layer visibility and blend modes at pixel level.
   * Transparency is detected via palette index 255 (object buffer fill color).
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
   * @see docs/internal/agents/composite-layer-system.md for full documentation
   */
  void CompositeToOutput(Room& room, gfx::Bitmap& output) const;

 private:
  /**
   * @brief Check if a pixel index represents transparency
   *
   * IMPORTANT: Only treat 255 as transparent (fill color for undrawn areas).
   * Do NOT treat 0 as transparent! In dungeon rendering with 16-color banks:
   * - Source pixel 1 with bank 0 writes final_color = 1 to the buffer
   * - Each bank's index 0 is transparent in SNES CGRAM but never written
   * - Actual colors are at indices 1-15 within each 16-color bank
   * - Buffers should be initialized to 255, not 0
   */
  static bool IsTransparent(uint8_t pixel) {
    return pixel == 255;
  }

  std::array<bool, 4> layer_visible_;
  std::array<LayerBlendMode, 4> layer_blend_mode_;
  std::array<uint8_t, 4> layer_alpha_;
  std::vector<ObjectTranslucency> object_translucency_;
  
  // Color math participation flag (from ROM's Layer2OnTop)
  // NOTE: Does NOT affect draw order - BG1 is always above BG2 per SNES Mode 1.
  // This controls whether BG2 participates in sub-screen color math effects.
  bool bg2_on_top_ = false;

  // Merge state tracking
  bool layers_merged_ = false;
  uint8_t current_merge_type_id_ = 0;
  
  // DEPRECATED: Priority compositing is no longer used.
  // The correct SNES behavior uses simple back-to-front layer ordering:
  // BG2 (background) first, then BG1 (foreground) on top.
  // Per-tile priority is encoded in the tile data itself.
  // Kept for API compatibility.
  bool use_priority_compositing_ = false;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_LAYER_MANAGER_H
