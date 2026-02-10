#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_GROUP_CARD_H
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_GROUP_CARD_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

/**
 * @brief Represents a single color change for undo/redo
 */
struct ColorChange {
  int palette_index;
  int color_index;
  gfx::SnesColor original_color;
  gfx::SnesColor new_color;
  uint64_t timestamp;  // For history ordering
};

/**
 * @brief Metadata for a single palette in a group
 */
struct PaletteMetadata {
  int palette_id;           // Palette ID in ROM
  std::string name;         // Display name (e.g., "Light World Main")
  std::string description;  // Usage description
  uint32_t rom_address;     // Base ROM address for this palette
  uint32_t vram_address;    // VRAM address (for sprite palettes, 0 if N/A)
  std::string usage_notes;  // Additional usage information
};

/**
 * @brief Metadata for an entire palette group
 */
struct PaletteGroupMetadata {
  std::string group_name;                 // Internal group name
  std::string display_name;               // Display name for UI
  std::vector<PaletteMetadata> palettes;  // Metadata for each palette
  int colors_per_palette;  // Number of colors per palette (usually 8 or 16)
  int colors_per_row;      // Colors per row for grid layout
};

/**
 * @brief Base class for palette group editing cards
 *
 * Provides common functionality for all palette group editors:
 * - ROM persistence with transaction-based writes
 * - Undo/redo history management
 * - Modified state tracking with visual indicators
 * - Save/discard workflow
 * - Common toolbar and color picker UI
 * - PanelManager integration
 *
 * Derived classes implement specific grid layouts and palette access.
 */
class PaletteGroupPanel : public EditorPanel {
 public:
  /**
   * @brief Construct a new Palette Group Panel
   * @param group_name Internal palette group name (e.g., "ow_main",
   * "dungeon_main")
   * @param display_name Human-readable name for UI
   * @param rom ROM instance for reading/writing palettes
   * @param game_data GameData instance for palette access
   */
  PaletteGroupPanel(const std::string& group_name,
                   const std::string& display_name, Rom* rom,
                   zelda3::GameData* game_data = nullptr);
  
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }

  virtual ~PaletteGroupPanel() = default;

  // ========== Main Rendering ==========

  /**
   * @brief Draw the card's ImGui UI
   */
  /**
   * @brief Draw the card's ImGui UI
   */
  void Draw(bool* p_open) override;

  // EditorPanel Implementation
  std::string GetId() const override { return "palette." + group_name_; }
  std::string GetDisplayName() const override { return display_name_; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; } // Default, override in derived
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 50; } // Default, override in derived

  // ========== Panel Control ==========

  void Show() { show_ = true; }
  void Hide() { show_ = false; }
  bool IsVisible() const { return show_; }
  bool* visibility_flag() { return &show_; }

  // ========== Palette Operations ==========

  /**
   * @brief Save all modified palettes to ROM
   */
  absl::Status SaveToRom();

  /**
   * @brief Discard all unsaved changes
   */
  void DiscardChanges();

  /**
   * @brief Reset a specific palette to original ROM values
   */
  void ResetPalette(int palette_index);

  /**
   * @brief Reset a specific color to original ROM value
   */
  void ResetColor(int palette_index, int color_index);

  /**
   * @brief Set a color value (records change for undo)
   */
  void SetColor(int palette_index, int color_index,
                const gfx::SnesColor& new_color);

  // ========== History Management ==========

  void Undo();
  void Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  void ClearHistory();

  // ========== State Queries ==========

  bool HasUnsavedChanges() const;
  bool IsPaletteModified(int palette_index) const;
  bool IsColorModified(int palette_index, int color_index) const;

  int GetSelectedPaletteIndex() const { return selected_palette_; }
  void SetSelectedPaletteIndex(int index) { selected_palette_ = index; }

  int GetSelectedColorIndex() const { return selected_color_; }
  void SetSelectedColorIndex(int index) { selected_color_ = index; }

  // ========== Export/Import ==========

  std::string ExportToJson() const;
  absl::Status ImportFromJson(const std::string& json);

  std::string ExportToClipboard() const;
  absl::Status ImportFromClipboard();

 protected:
  // ========== Pure Virtual Methods (Implemented by Derived Classes) ==========

  /**
   * @brief Get the palette group for this card
   */
  virtual gfx::PaletteGroup* GetPaletteGroup() = 0;
  virtual const gfx::PaletteGroup* GetPaletteGroup() const = 0;

  /**
   * @brief Get metadata for this palette group
   */
  virtual const PaletteGroupMetadata& GetMetadata() const = 0;

  /**
   * @brief Draw the palette grid specific to this palette type
   */
  virtual void DrawPaletteGrid() = 0;

  /**
   * @brief Get the number of colors per row for grid layout
   */
  virtual int GetColorsPerRow() const = 0;

  // ========== Optional Overrides ==========

  /**
   * @brief Draw additional toolbar buttons (called after standard buttons)
   */
  virtual void DrawCustomToolbarButtons() {}

  /**
   * @brief Draw additional panels (called after main content)
   */
  virtual void DrawCustomPanels() {}

  // ========== Common UI Components ==========

  /**
   * @brief Draw standard toolbar with save/discard/undo/redo
   */
  void DrawToolbar();

  /**
   * @brief Draw palette selector dropdown
   */
  void DrawPaletteSelector();

  /**
   * @brief Draw color picker for selected color
   */
  void DrawColorPicker();

  /**
   * @brief Draw color info panel with RGB/SNES/Hex values
   */
  void DrawColorInfo();

  /**
   * @brief Draw palette metadata info panel
   */
  void DrawMetadataInfo();

  /**
   * @brief Draw batch operations popup
   */
  void DrawBatchOperationsPopup();

  // ========== Helper Methods ==========

  /**
   * @brief Get mutable palette by index
   */
  gfx::SnesPalette* GetMutablePalette(int index);

  /**
   * @brief Get original color from ROM (for reset/comparison)
   */
  gfx::SnesColor GetOriginalColor(int palette_index, int color_index) const;

  /**
   * @brief Write a single color to ROM
   */
  absl::Status WriteColorToRom(int palette_index, int color_index,
                               const gfx::SnesColor& color);

  /**
   * @brief Mark palette as modified
   */
  void MarkModified(int palette_index, int color_index);

  /**
   * @brief Clear modified flags for palette
   */
  void ClearModified(int palette_index);

  // ========== Member Variables ==========

  std::string group_name_;    // Internal name (e.g., "ow_main")
  std::string display_name_;  // Display name (e.g., "Overworld Main")
  Rom* rom_;                  // ROM instance
  zelda3::GameData* game_data_ = nullptr;  // GameData instance
  bool show_ = false;         // Visibility flag

  // Selection state
  int selected_palette_ = 0;      // Currently selected palette index
  int selected_color_ = -1;       // Currently selected color (-1 = none)
  gfx::SnesColor editing_color_;  // Color being edited in picker

  // Settings
  bool auto_save_enabled_ = false;  // Auto-save to ROM on every change
  bool show_snes_format_ = true;    // Show SNES $xxxx format in info
  bool show_hex_format_ = true;     // Show #xxxxxx hex in info
};

// ============================================================================
// Concrete Palette Panel Implementations
// ============================================================================

/**
 * @brief Overworld Main palette group panel
 *
 * Manages palettes used for overworld rendering:
 * - Light World palettes (0-19)
 * - Dark World palettes (20-39)
 * - Special World palettes (40-59)
 */
class OverworldMainPalettePanel : public PaletteGroupPanel {
 public:
  explicit OverworldMainPalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~OverworldMainPalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 7; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_LANDSCAPE; }
  int GetPriority() const override { return 20; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Overworld Animated palette group panel
 *
 * Manages animated palettes for water, lava, and other effects
 */
class OverworldAnimatedPalettePanel : public PaletteGroupPanel {
 public:
  explicit OverworldAnimatedPalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~OverworldAnimatedPalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 7; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_WATER; }
  int GetPriority() const override { return 30; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Dungeon Main palette group panel
 *
 * Manages palettes for dungeon rooms (0-19)
 */
class DungeonMainPalettePanel : public PaletteGroupPanel {
 public:
  explicit DungeonMainPalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~DungeonMainPalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 15; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_CASTLE; }
  int GetPriority() const override { return 40; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Global Sprite palette group panel
 *
 * Manages global sprite palettes for Light World and Dark World
 * - 2 palettes (LW and DW)
 * - Each has 60 colors organized as 4 rows of 16 colors
 * - Transparent colors at indices 0, 16, 32, 48
 */
class SpritePalettePanel : public PaletteGroupPanel {
 public:
  explicit SpritePalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~SpritePalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 15; }
  void DrawCustomPanels() override;  // Show VRAM info

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_PETS; }
  int GetPriority() const override { return 50; }



 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Sprites Aux1 palette group panel
 *
 * Manages auxiliary sprite palettes 1
 * - 12 palettes of 8 colors (7 colors + transparent)
 */
class SpritesAux1PalettePanel : public PaletteGroupPanel {
 public:
  explicit SpritesAux1PalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~SpritesAux1PalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 7; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_FILTER_1; }
  int GetPriority() const override { return 51; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Sprites Aux2 palette group panel
 *
 * Manages auxiliary sprite palettes 2
 * - 11 palettes of 8 colors (7 colors + transparent)
 */
class SpritesAux2PalettePanel : public PaletteGroupPanel {
 public:
  explicit SpritesAux2PalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~SpritesAux2PalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 7; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_FILTER_2; }
  int GetPriority() const override { return 52; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Sprites Aux3 palette group panel
 *
 * Manages auxiliary sprite palettes 3
 * - 24 palettes of 8 colors (7 colors + transparent)
 */
class SpritesAux3PalettePanel : public PaletteGroupPanel {
 public:
  explicit SpritesAux3PalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~SpritesAux3PalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 7; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_FILTER_3; }
  int GetPriority() const override { return 53; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

/**
 * @brief Equipment/Armor palette group panel
 *
 * Manages Link's equipment color palettes (green, blue, red tunics)
 */
class EquipmentPalettePanel : public PaletteGroupPanel {
 public:
  explicit EquipmentPalettePanel(Rom* rom, zelda3::GameData* game_data = nullptr);
  ~EquipmentPalettePanel() override = default;

 protected:
  gfx::PaletteGroup* GetPaletteGroup() override;
  const gfx::PaletteGroup* GetPaletteGroup() const override;
  const PaletteGroupMetadata& GetMetadata() const override { return metadata_; }
  void DrawPaletteGrid() override;
  int GetColorsPerRow() const override { return 15; }

  // EditorPanel Overrides
  std::string GetIcon() const override { return ICON_MD_SHIELD; }
  int GetPriority() const override { return 60; }

 private:
  static PaletteGroupMetadata InitializeMetadata();
  static const PaletteGroupMetadata metadata_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTE_GROUP_CARD_H
