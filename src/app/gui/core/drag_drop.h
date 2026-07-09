#ifndef YAZE_APP_GUI_CORE_DRAG_DROP_H_
#define YAZE_APP_GUI_CORE_DRAG_DROP_H_

#include <cstdint>
#include <cstring>
#include "util/i18n/tr.h"

#include "imgui/imgui.h"

namespace yaze {

namespace gui {

// ============================================================================
// Payload type identifiers for ImGui drag-drop
// ============================================================================

constexpr const char* kDragPayloadTile16 = "YAZE_TILE16";
constexpr const char* kDragPayloadSprite = "YAZE_SPRITE";
constexpr const char* kDragPayloadPalette = "YAZE_PALETTE";
constexpr const char* kDragPayloadRoomObject = "YAZE_ROOM_OBJ";
constexpr const char* kDragPayloadPanel = "YAZE_PANEL";

// ============================================================================
// Payload structs
// ============================================================================

struct TileDragPayload {
  int tile_id;
  int source_map_id;
};

struct SpriteDragPayload {
  int sprite_id;
  int source_room_id;
};

struct PaletteDragPayload {
  int group_index;
  int palette_index;
  int color_index;
};

struct RoomObjectDragPayload {
  uint16_t object_id;
  int source_room_id;
  int x;
  int y;
  uint8_t size;
};

// Drag payload for the Layout Designer. Panel IDs are bounded strings
// ("dungeon.room_selector" style), so the payload stays POD-copyable
// through ImGui's raw-memory drag-drop.
struct PanelDragPayload {
  char panel_id[128] = {};
};

// ============================================================================
// Drag source helpers (call inside an ImGui widget loop)
// ============================================================================

/// Begin a tile16 drag source. Call after rendering a tile widget.
/// Returns true while dragging is active.
inline bool BeginTileDragSource(int tile_id, int map_id) {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    TileDragPayload payload{tile_id, map_id};
    ImGui::SetDragDropPayload(kDragPayloadTile16, &payload, sizeof(payload));
    ImGui::Text(tr("Tile #%d"), tile_id);
    ImGui::EndDragDropSource();
    return true;
  }
  return false;
}

inline bool BeginSpriteDragSource(int sprite_id, int room_id) {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    SpriteDragPayload payload{sprite_id, room_id};
    ImGui::SetDragDropPayload(kDragPayloadSprite, &payload, sizeof(payload));
    ImGui::Text(tr("Sprite #%d"), sprite_id);
    ImGui::EndDragDropSource();
    return true;
  }
  return false;
}

inline bool BeginPaletteDragSource(int group_idx, int palette_idx,
                                   int color_idx) {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    PaletteDragPayload payload{group_idx, palette_idx, color_idx};
    ImGui::SetDragDropPayload(kDragPayloadPalette, &payload, sizeof(payload));
    ImGui::Text(tr("Color [%d:%d:%d]"), group_idx, palette_idx, color_idx);
    ImGui::EndDragDropSource();
    return true;
  }
  return false;
}

inline bool BeginRoomObjectDragSource(uint16_t object_id, int room_id,
                                      int pos_x, int pos_y, uint8_t size = 0) {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    RoomObjectDragPayload payload{object_id, room_id, pos_x, pos_y, size};
    ImGui::SetDragDropPayload(kDragPayloadRoomObject, &payload,
                              sizeof(payload));
    ImGui::Text(tr("Object 0x%04X"), object_id);
    ImGui::EndDragDropSource();
    return true;
  }
  return false;
}

inline bool BeginPanelDragSource(const char* panel_id,
                                 const char* preview_label) {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    PanelDragPayload payload;
    if (panel_id != nullptr) {
      std::strncpy(payload.panel_id, panel_id, sizeof(payload.panel_id) - 1);
    }
    ImGui::SetDragDropPayload(kDragPayloadPanel, &payload, sizeof(payload));
    ImGui::TextUnformatted(preview_label != nullptr ? preview_label : panel_id);
    ImGui::EndDragDropSource();
    return true;
  }
  return false;
}

// ============================================================================
// Drop target helpers (call inside a widget that should accept drops)
// ============================================================================

/// Accept a tile16 drop. Returns true if a payload was accepted.
inline bool AcceptTileDrop(TileDragPayload* out) {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kDragPayloadTile16)) {
      *out = *static_cast<const TileDragPayload*>(payload->Data);
      ImGui::EndDragDropTarget();
      return true;
    }
    ImGui::EndDragDropTarget();
  }
  return false;
}

inline bool AcceptSpriteDrop(SpriteDragPayload* out) {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kDragPayloadSprite)) {
      *out = *static_cast<const SpriteDragPayload*>(payload->Data);
      ImGui::EndDragDropTarget();
      return true;
    }
    ImGui::EndDragDropTarget();
  }
  return false;
}

inline bool AcceptPaletteDrop(PaletteDragPayload* out) {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kDragPayloadPalette)) {
      *out = *static_cast<const PaletteDragPayload*>(payload->Data);
      ImGui::EndDragDropTarget();
      return true;
    }
    ImGui::EndDragDropTarget();
  }
  return false;
}

inline bool AcceptRoomObjectDrop(RoomObjectDragPayload* out) {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kDragPayloadRoomObject)) {
      *out = *static_cast<const RoomObjectDragPayload*>(payload->Data);
      ImGui::EndDragDropTarget();
      return true;
    }
    ImGui::EndDragDropTarget();
  }
  return false;
}

// Accepts a panel drag payload assuming BeginDragDropTarget has already
// been entered by the caller (so the target can draw drop-preview hints
// before committing to accept). Returns true when the user releases over
// the target with a YAZE_PANEL payload in flight.
inline bool AcceptPanelDropWithinTarget(PanelDragPayload* out) {
  if (const ImGuiPayload* payload =
          ImGui::AcceptDragDropPayload(kDragPayloadPanel)) {
    *out = *static_cast<const PanelDragPayload*>(payload->Data);
    return true;
  }
  return false;
}

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_DRAG_DROP_H_
