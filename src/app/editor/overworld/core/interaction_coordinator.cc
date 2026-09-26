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
  if (ImGui::IsAnyItemActive()) {
    return;
  }

  // Modifier states. Use the same primary-modifier rule as ShortcutManager:
  // Cmd (Super) counts as Ctrl on macOS, and ImGui's macOS behaviors may
  // already have swapped it into io.KeyCtrl.
  const ImGuiIO& io = ImGui::GetIO();
  const bool ctrl_held = io.KeyCtrl || (gui::IsMacPlatform() && io.KeySuper);
  const bool shift_held = io.KeyShift;
  const bool alt_held = io.KeyAlt;

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

  // 3. Pick shortcut (avoid clobbering Ctrl/Alt based shortcuts). Brush (B),
  // fill (F), tile cycling ([ ]), F11, Ctrl+L, Ctrl+T and Ctrl+Shift+I are
  // dispatched once by ShortcutManager as overworld editor shortcuts.
  if (!ctrl_held && !alt_held) {
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
      if (sink_.on_pick_tile_from_hover)
        sink_.on_pick_tile_from_hover();
    }
  }

  // 5. Item workflow shortcuts (duplicate + nudge)
  if (sink_.can_edit_items && sink_.can_edit_items()) {
    // Ctrl+Shift+D is Duplicate Session; only plain Ctrl+D duplicates items.
    if (ctrl_held && !shift_held && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
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

  // Undo/Redo is dispatched once by the application shortcut manager.
}

}  // namespace editor
}  // namespace yaze
