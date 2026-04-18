#include "app/editor/overworld/core/interaction_coordinator.h"

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
  if (ImGui::IsAnyItemActive()) {
    return;
  }

  // Modifier states
  const bool ctrl_held = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                         ImGui::IsKeyDown(ImGuiKey_RightCtrl);
  const bool shift_held = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                          ImGui::IsKeyDown(ImGuiKey_RightShift);
  const bool alt_held =
      ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);

  // 1. Tool shortcuts (1-2 for mode selection)
  if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
    if (sink_.on_set_editor_mode)
      sink_.on_set_editor_mode(EditingMode::MOUSE);
  } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
    if (sink_.on_set_editor_mode)
      sink_.on_set_editor_mode(EditingMode::DRAW_TILE);
  }

  // 2. Entity editing modes (3-8)
  // These use IsKeyDown in the original to allow rapid mode switching/status update
  if (ImGui::IsKeyDown(ImGuiKey_3)) {
    if (sink_.on_set_entity_mode)
      sink_.on_set_entity_mode(EntityEditMode::ENTRANCES);
  } else if (ImGui::IsKeyDown(ImGuiKey_4)) {
    if (sink_.on_set_entity_mode)
      sink_.on_set_entity_mode(EntityEditMode::EXITS);
  } else if (ImGui::IsKeyDown(ImGuiKey_5)) {
    if (sink_.on_set_entity_mode)
      sink_.on_set_entity_mode(EntityEditMode::ITEMS);
  } else if (ImGui::IsKeyDown(ImGuiKey_6)) {
    if (sink_.on_set_entity_mode)
      sink_.on_set_entity_mode(EntityEditMode::SPRITES);
  } else if (ImGui::IsKeyDown(ImGuiKey_7)) {
    if (sink_.on_set_entity_mode)
      sink_.on_set_entity_mode(EntityEditMode::TRANSPORTS);
  } else if (ImGui::IsKeyDown(ImGuiKey_8)) {
    if (sink_.on_set_entity_mode)
      sink_.on_set_entity_mode(EntityEditMode::MUSIC);
  }

  // 3. Brush/Fill/Pick shortcuts (avoid clobbering Ctrl/Alt based shortcuts).
  if (!ctrl_held && !alt_held) {
    if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
      if (sink_.on_toggle_brush)
        sink_.on_toggle_brush();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
      if (sink_.on_activate_fill)
        sink_.on_activate_fill();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
      if (sink_.on_pick_tile_from_hover)
        sink_.on_pick_tile_from_hover();
    }
  }

  // 4. View / Map shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
    if (sink_.on_toggle_fullscreen)
      sink_.on_toggle_fullscreen();
  }

  // Toggle map lock with Ctrl+L
  if (ctrl_held && ImGui::IsKeyPressed(ImGuiKey_L, false)) {
    if (sink_.on_toggle_lock)
      sink_.on_toggle_lock();
  }

  // Toggle Tile16 editor with Ctrl+T
  if (ctrl_held && ImGui::IsKeyPressed(ImGuiKey_T, false)) {
    if (sink_.on_toggle_tile16_editor)
      sink_.on_toggle_tile16_editor();
  }

  // Toggle Overworld Item List with Ctrl+Shift+I
  if (ctrl_held && shift_held && ImGui::IsKeyPressed(ImGuiKey_I, false)) {
    if (sink_.on_toggle_item_list)
      sink_.on_toggle_item_list();
  }

  // 5. Item workflow shortcuts (duplicate + nudge)
  if (sink_.can_edit_items && sink_.can_edit_items()) {
    if (ctrl_held && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
      if (sink_.on_duplicate_selected)
        sink_.on_duplicate_selected();
    } else if (!ctrl_held) {
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

  // 6. Undo/Redo (supports Ctrl+Z, Ctrl+Shift+Z, and Ctrl+Y)
  if (ctrl_held) {
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      if (shift_held) {
        if (sink_.on_redo)
          sink_.on_redo();
      } else {
        if (sink_.on_undo)
          sink_.on_undo();
      }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      if (sink_.on_redo)
        sink_.on_redo();
    }
  }
}

}  // namespace editor
}  // namespace yaze
