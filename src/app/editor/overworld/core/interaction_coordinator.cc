#include "app/editor/overworld/core/interaction_coordinator.h"

#include "app/gui/core/platform_keys.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

OverworldInteractionCoordinator::OverworldInteractionCoordinator(
    OverworldCommandSink sink)
    : sink_(std::move(sink)) {}

void OverworldInteractionCoordinator::Update(
    zelda3::GameEntity* hovered_entity) {
  // 1. Mouse Interactions (Higher priority than shortcuts in some cases)
  if (hovered_entity) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      if (sink_.on_entity_context_menu)
        sink_.on_entity_context_menu(hovered_entity);
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      if (sink_.on_entity_double_click)
        sink_.on_entity_double_click(hovered_entity);
    }
  }

  // Skip processing if any ImGui item is active (e.g., text input)
  if (ImGui::IsAnyItemActive() || ImGui::GetIO().WantTextInput) {
    return;
  }

  // Modifier states. Use the same primary-modifier rule as ShortcutManager:
  // Cmd (Super) counts as Ctrl on macOS, and ImGui's macOS behaviors may
  // already have swapped it into io.KeyCtrl.
  const ImGuiIO& io = ImGui::GetIO();
  const bool ctrl_held = io.KeyCtrl || (gui::IsMacPlatform() && io.KeySuper);
  const bool shift_held = io.KeyShift;
  const bool alt_held = io.KeyAlt;

  // Digits are plain keys only: Cmd/Ctrl+1..9 switch editors and Alt+1..3
  // switch worlds (both through ShortcutManager).
  const bool plain_key = !ctrl_held && !alt_held && !io.KeySuper;

  // 1. Tool shortcuts. 1 = select, 2 = brush; both leave any entity mode.
  if (plain_key) {
    if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
      if (sink_.on_set_editor_mode)
        sink_.on_set_editor_mode(EditingMode::MOUSE);
    } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
      if (sink_.on_set_editor_mode)
        sink_.on_set_editor_mode(EditingMode::DRAW_TILE);
    }
  }

  // 2. Entity modes (3-8): only that entity type responds to the mouse.
  static constexpr struct {
    ImGuiKey key;
    EntityEditMode mode;
  } kEntityKeys[] = {
      {ImGuiKey_3, EntityEditMode::ENTRANCES},
      {ImGuiKey_4, EntityEditMode::EXITS},
      {ImGuiKey_5, EntityEditMode::ITEMS},
      {ImGuiKey_6, EntityEditMode::SPRITES},
      {ImGuiKey_7, EntityEditMode::TRANSPORTS},
      {ImGuiKey_8, EntityEditMode::MUSIC},
  };
  if (plain_key && sink_.on_set_entity_mode) {
    for (const auto& entry : kEntityKeys) {
      if (ImGui::IsKeyPressed(entry.key, false)) {
        sink_.on_set_entity_mode(entry.mode);
        break;
      }
    }
  }

  // 3. Pick shortcut (avoid clobbering Ctrl/Alt based shortcuts). Brush (B),
  // fill (F), tile cycling ([ ]), F11, Ctrl+L, Ctrl+T and Ctrl+Shift+I are
  // dispatched once by ShortcutManager as overworld editor shortcuts.
  if (!ctrl_held && !alt_held) {
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
      if (sink_.on_pick_tile_from_hover)
        sink_.on_pick_tile_from_hover();
    }
  }

  // 5. Item workflow shortcuts (duplicate + nudge). Available whenever an
  // item is selected with the select tool, not only in Items mode.
  if (sink_.can_edit_items && sink_.can_edit_items()) {
    // Ctrl+Shift+D is Duplicate Session; only plain Ctrl+D duplicates items.
    if (ctrl_held && !shift_held && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
      if (sink_.on_duplicate_selected)
        sink_.on_duplicate_selected();
    } else if (!ctrl_held && !alt_held) {  // Alt+arrows: adjacent map
      int dx = 0, dy = 0;
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
        dx = -1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
        dx = 1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
        dy = -1;
      } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
        dy = 1;
      }
      if (dx != 0 || dy != 0) {
        if (sink_.on_nudge_selected)
          sink_.on_nudge_selected(dx, dy, shift_held);
      }
    }
  }

  // Undo/Redo is dispatched once by the application shortcut manager.
}

}  // namespace editor
}  // namespace yaze
