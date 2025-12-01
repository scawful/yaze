#ifndef YAZE_APP_EDITOR_PALETTE_PANELS_PALETTE_CARD_PANELS_H_
#define YAZE_APP_EDITOR_PALETTE_PANELS_PALETTE_CARD_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/palette/palette_group_card.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// EditorPanel wrappers for PaletteGroupCard classes
// =============================================================================

/**
 * @brief EditorPanel wrapper for OverworldMainPaletteCard
 */
class OverworldMainPalettePanel : public EditorPanel {
 public:
  explicit OverworldMainPalettePanel(OverworldMainPaletteCard* card)
      : card_(card) {}

  std::string GetId() const override { return "palette.ow_main"; }
  std::string GetDisplayName() const override { return "Overworld Main"; }
  std::string GetIcon() const override { return ICON_MD_LANDSCAPE; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 20; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  OverworldMainPaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for OverworldAnimatedPaletteCard
 */
class OverworldAnimatedPalettePanel : public EditorPanel {
 public:
  explicit OverworldAnimatedPalettePanel(OverworldAnimatedPaletteCard* card)
      : card_(card) {}

  std::string GetId() const override { return "palette.ow_animated"; }
  std::string GetDisplayName() const override { return "Overworld Animated"; }
  std::string GetIcon() const override { return ICON_MD_WATER; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 30; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  OverworldAnimatedPaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for DungeonMainPaletteCard
 */
class DungeonMainPalettePanel : public EditorPanel {
 public:
  explicit DungeonMainPalettePanel(DungeonMainPaletteCard* card)
      : card_(card) {}

  std::string GetId() const override { return "palette.dungeon_main"; }
  std::string GetDisplayName() const override { return "Dungeon Main"; }
  std::string GetIcon() const override { return ICON_MD_CASTLE; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 40; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  DungeonMainPaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for SpritePaletteCard
 */
class SpritePalettePanel : public EditorPanel {
 public:
  explicit SpritePalettePanel(SpritePaletteCard* card) : card_(card) {}

  std::string GetId() const override { return "palette.sprites"; }
  std::string GetDisplayName() const override {
    return "Global Sprite Palettes";
  }
  std::string GetIcon() const override { return ICON_MD_PETS; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 50; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  SpritePaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for SpritesAux1PaletteCard
 */
class SpritesAux1PalettePanel : public EditorPanel {
 public:
  explicit SpritesAux1PalettePanel(SpritesAux1PaletteCard* card)
      : card_(card) {}

  std::string GetId() const override { return "palette.sprites_aux1"; }
  std::string GetDisplayName() const override { return "Sprites Aux 1"; }
  std::string GetIcon() const override { return ICON_MD_FILTER_1; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 51; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  SpritesAux1PaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for SpritesAux2PaletteCard
 */
class SpritesAux2PalettePanel : public EditorPanel {
 public:
  explicit SpritesAux2PalettePanel(SpritesAux2PaletteCard* card)
      : card_(card) {}

  std::string GetId() const override { return "palette.sprites_aux2"; }
  std::string GetDisplayName() const override { return "Sprites Aux 2"; }
  std::string GetIcon() const override { return ICON_MD_FILTER_2; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 52; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  SpritesAux2PaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for SpritesAux3PaletteCard
 */
class SpritesAux3PalettePanel : public EditorPanel {
 public:
  explicit SpritesAux3PalettePanel(SpritesAux3PaletteCard* card)
      : card_(card) {}

  std::string GetId() const override { return "palette.sprites_aux3"; }
  std::string GetDisplayName() const override { return "Sprites Aux 3"; }
  std::string GetIcon() const override { return ICON_MD_FILTER_3; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 53; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  SpritesAux3PaletteCard* card_;
};

/**
 * @brief EditorPanel wrapper for EquipmentPaletteCard
 */
class EquipmentPalettePanel : public EditorPanel {
 public:
  explicit EquipmentPalettePanel(EquipmentPaletteCard* card) : card_(card) {}

  std::string GetId() const override { return "palette.equipment"; }
  std::string GetDisplayName() const override { return "Equipment Palettes"; }
  std::string GetIcon() const override { return ICON_MD_SHIELD; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 60; }

  void Draw(bool* p_open) override {
    if (card_) {
      card_->Draw();
    }
  }

 private:
  EquipmentPaletteCard* card_;
};

// =============================================================================
// EditorPanel for Control Panel (delegates to PaletteEditor methods)
// =============================================================================

/**
 * @brief EditorPanel for the Palette Control Panel
 *
 * Uses a callback to delegate drawing to PaletteEditor::DrawControlPanel()
 */
class PaletteControlPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit PaletteControlPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "palette.control_panel"; }
  std::string GetDisplayName() const override { return "Palette Controls"; }
  std::string GetIcon() const override { return ICON_MD_PALETTE; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 10; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Quick Access palette panel
 */
class QuickAccessPalettePanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit QuickAccessPalettePanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "palette.quick_access"; }
  std::string GetDisplayName() const override { return "Quick Access"; }
  std::string GetIcon() const override { return ICON_MD_COLOR_LENS; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 70; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Custom Palette panel
 */
class CustomPalettePanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit CustomPalettePanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "palette.custom"; }
  std::string GetDisplayName() const override { return "Custom Palette"; }
  std::string GetIcon() const override { return ICON_MD_BRUSH; }
  std::string GetEditorCategory() const override { return "Palette"; }
  int GetPriority() const override { return 80; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_PALETTE_PANELS_PALETTE_CARD_PANELS_H_
