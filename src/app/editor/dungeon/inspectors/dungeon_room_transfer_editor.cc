#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_document_file.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"

namespace yaze::editor {
namespace {

struct TransferDomain {
  uint16_t bit;
  const char* label;
  const char* widget;
};

constexpr std::array<TransferDomain, 7> kDomains{{
    {kTransferObjects, "Objects and chest rewards", "TransferObjects"},
    {kTransferDoors, "Doors", "TransferDoors"},
    {kTransferSprites, "Sprites", "TransferSprites"},
    {kTransferItems, "Pot items", "TransferItems"},
    {kTransferMetadata, "Room properties", "TransferMetadata"},
    {kTransferCollision, "Custom collision", "TransferCollision"},
    {kTransferWater, "Water fill zones", "TransferWater"},
}};

void InvalidatePreview(DungeonRoomTransferEditorState& state) {
  state.preview.reset();
  state.error.clear();
  state.status.clear();
  state.can_retry_without_properties = false;
}

void PreviewReplacement(DungeonCanvasViewer& viewer,
                        DungeonRoomTransferEditorState& state) {
  InvalidatePreview(state);
  const auto preview = viewer.PreviewRoomTransfer(
      state.import_json ? -1 : state.source_room_id, state.json,
      {state.domains, state.copy_destinations});
  if (preview.ok()) {
    state.preview = std::make_shared<DungeonRoomTransferPlan>(*preview);
  } else {
    state.error = std::string(preview.status().message());
    state.can_retry_without_properties =
        (state.domains & kTransferMetadata) != 0 &&
        (state.domains & ~kTransferMetadata) != 0 &&
        preview.status()
            .GetPayload(kRoomTransferSharedHeaderPayload)
            .has_value();
  }
}

void DrawCounts(const DungeonRoomTransferPlan& plan) {
  const auto& before = plan.before.contents;
  const auto& after = plan.after.contents;
  auto count = [&](const char* label, uint16_t bit, size_t old_count,
                   size_t new_count) {
    if ((plan.options.domains & bit) != 0) {
      ImGui::TextWrapped("%s: %zu → %zu (replace)", label, old_count,
                         new_count);
    } else {
      ImGui::TextWrapped("%s: %zu (preserve)", label, old_count);
    }
  };
  count("Objects", kTransferObjects, before.objects.size(),
        after.objects.size());
  count("Chest rewards", kTransferObjects, before.chests.size(),
        after.chests.size());
  count("Doors", kTransferDoors, before.doors.size(), after.doors.size());
  count("Sprites", kTransferSprites, before.sprites.size(),
        after.sprites.size());
  count("Pot items", kTransferItems, before.items.size(), after.items.size());
  for (const auto& domain : kDomains) {
    if (domain.bit >= kTransferMetadata) {
      ImGui::TextWrapped(
          "%s: %s", domain.label,
          (plan.options.domains & domain.bit) != 0 ? "replace" : "preserve");
    }
  }
  ImGui::TextWrapped("Stair and warp destinations: %s",
                     plan.options.copy_destinations ? "replace" : "preserve");
  if ((plan.options.domains & kTransferWater) && plan.after.water.has_data) {
    ImGui::TextWrapped("Water-fill save bit: %02X (other rooms retain theirs)",
                       plan.after.water.sram_bit_mask);
  }
}

}  // namespace

void LoadDungeonRoomTransferFile(DungeonRoomTransferEditorState& state,
                                 const std::string& path) {
  if (path.empty())
    return;  // Native dialog cancellation leaves the form alone.
  const auto document = ReadDungeonRoomDocumentFile(path);
  if (!document.ok()) {
    state.error = std::string(document.status().message());
    state.status.clear();
    return;
  }
  InvalidatePreview(state);
  state.json = *document;
  state.import_json = true;
  state.status = "Room file loaded. Preview Replacement before applying.";
}

void DrawDungeonRoomTransferEditor(DungeonCanvasViewer& viewer) {
  auto& state = viewer.room_transfer_state();
  const int target = viewer.current_room_id();
  if (state.room_id != target || state.rom != viewer.rom()) {
    const int popup_room = state.popup_room_id;
    const bool popup_open = state.popup_open;
    state = {};
    state.room_id = target;
    state.rom = viewer.rom();
    state.source_room_id = target == 0 ? 1 : 0;
    state.popup_room_id = popup_room;
    state.popup_open = popup_open;
  }
  gui::AutoWidgetScope scope("Dungeon/RoomTransfer");
  ImGui::PushID("DungeonRoomTransfer");
  ImGui::PushID(&viewer);
  ImGui::PushID(target);
  const bool valid_target =
      viewer.rooms() && viewer.rooms()->GetIfLoaded(target);
  if (!valid_target) {
    ImGui::TextWrapped(
        "Open a room in the canvas to clone or import its contents.");
    ImGui::PopID();
    ImGui::PopID();
    ImGui::PopID();
    return;
  }
  ImGui::TextWrapped("Replace contents of room %03X", target);
  ImGui::TextWrapped(
      "Selected domains are replaced. Unchecked domains stay in this room.");
  if (ImGui::Button("Copy Room JSON", ImVec2(-1, 0))) {
    const auto json = viewer.ExportRoomDocument(target);
    state.error = json.ok() ? "" : std::string(json.status().message());
    state.status.clear();
    if (json.ok()) {
      ImGui::SetClipboardText(json->c_str());
      state.status = "Room JSON copied to clipboard.";
    }
  }
  gui::AutoRegisterLastItem("button", "TransferExport");

#ifndef __EMSCRIPTEN__
  if (ImGui::Button("Save Room File...", ImVec2(-1, 0))) {
    const auto json = viewer.ExportRoomDocument(target);
    if (!json.ok()) {
      state.error = std::string(json.status().message());
      state.status.clear();
    } else {
      const auto path = util::FileDialogWrapper::ShowSaveFileDialog(
          absl::StrFormat("room_%03X.yaze-room.json", target), "json");
      if (!path.empty()) {
        const auto result = WriteDungeonRoomDocumentFile(path, *json);
        state.error = result.ok() ? "" : std::string(result.message());
        state.status = result.ok() ? "Room template saved to " + path : "";
      }
    }
  }
  gui::AutoRegisterLastItem("button", "TransferSaveFile");
#endif

  const bool editable =
      !viewer.header_read_only() && viewer.IsObjectInteractionEnabled();
  ImGui::BeginDisabled(!editable);
  if (ImGui::RadioButton("Clone", !state.import_json)) {
    state.import_json = false;
    InvalidatePreview(state);
  }
  gui::AutoRegisterLastItem("radio", "TransferCloneMode");
  ImGui::SameLine();
  if (ImGui::RadioButton("Import JSON", state.import_json)) {
    state.import_json = true;
    InvalidatePreview(state);
  }
  gui::AutoRegisterLastItem("radio", "TransferImportMode");
  if (state.import_json) {
#ifndef __EMSCRIPTEN__
    if (ImGui::Button("Open Room File...", ImVec2(-1, 0))) {
      util::FileDialogOptions options;
      options.filters.push_back({"Yaze room template", "json"});
      LoadDungeonRoomTransferFile(
          state, util::FileDialogWrapper::ShowOpenFileDialog(options));
    }
    gui::AutoRegisterLastItem("button", "TransferOpenFile");
#endif
    if (ImGui::Button("Paste JSON", ImVec2(-1, 0))) {
      const char* clipboard = ImGui::GetClipboardText();
      InvalidatePreview(state);
      if (!clipboard) {
        state.error = "The clipboard contains no text.";
      } else if (strnlen(clipboard, kMaxDungeonRoomDocumentBytes + 1) >
                 kMaxDungeonRoomDocumentBytes) {
        state.error = "Room JSON exceeds the 1 MiB document limit.";
      } else {
        state.json = clipboard;
      }
    }
    gui::AutoRegisterLastItem("button", "TransferPaste");
    if (ImGui::InputTextMultiline("##RoomJSON", &state.json, ImVec2(-1, 130))) {
      InvalidatePreview(state);
    }
    gui::AutoRegisterLastItem("input_text", "TransferJSON");
  } else {
    ImGui::TextUnformatted("Source room (hex)");
    ImGui::SetNextItemWidth(-1);
    if (gui::InputScalarDeferred("##TransferSource", ImGuiDataType_S32,
                                 &state.source_room_id, "%03X",
                                 ImGuiInputTextFlags_CharsHexadecimal,
                                 {reinterpret_cast<uintptr_t>(&viewer),
                                  static_cast<uint64_t>(target)})) {
      InvalidatePreview(state);
    }
    gui::AutoRegisterLastItem("input_int", "TransferSource");
  }

  ImGui::TextUnformatted("Replace these domains");
  for (const auto& domain : kDomains) {
    bool enabled = (state.domains & domain.bit) != 0;
    if (ImGui::Checkbox(domain.label, &enabled)) {
      if (enabled) {
        state.domains |= domain.bit;
      } else {
        state.domains &= ~domain.bit;
      }
      if ((state.domains & kTransferMetadata) == 0) {
        state.copy_destinations = false;
      }
      InvalidatePreview(state);
    }
    gui::AutoRegisterLastItem("checkbox", domain.widget);
  }
  ImGui::BeginDisabled((state.domains & kTransferMetadata) == 0);
  if (ImGui::Checkbox("Copy destination links", &state.copy_destinations)) {
    InvalidatePreview(state);
  }
  gui::AutoRegisterLastItem("checkbox", "TransferDestinations");
  ImGui::EndDisabled();
  ImGui::TextWrapped(
      "Destinations are preserved by default. Copied links may need changes "
      "for the new room.");
  const bool valid_source =
      state.import_json ? !state.json.empty() &&
                              state.json.size() <= kMaxDungeonRoomDocumentBytes
                        : state.source_room_id >= 0 &&
                              state.source_room_id < zelda3::kNumberOfRooms &&
                              state.source_room_id != target;
  ImGui::BeginDisabled(!valid_source || state.domains == 0);
  if (ImGui::Button("Preview Replacement", ImVec2(-1, 0))) {
    PreviewReplacement(viewer, state);
  }
  gui::AutoRegisterLastItem("button", "TransferPreview");
  ImGui::EndDisabled();
  ImGui::EndDisabled();

  if (state.preview) {
    ImGui::Separator();
    ImGui::TextWrapped("Preview for room %03X", state.preview->target_room_id);
    DrawCounts(*state.preview);
    if (!state.preview->changed()) {
      ImGui::TextWrapped("Selected contents already match.");
    }
    ImGui::BeginDisabled(!editable || !state.preview->changed());
    const bool apply = ImGui::Button("Apply Replacement", ImVec2(-1, 0));
    gui::AutoRegisterLastItem("button", "TransferApply");
    ImGui::EndDisabled();
    if (apply) {
      // Publication can refresh this viewer; retain the reviewed plan during
      // its callback rather than borrowing it from presentation state.
      const auto plan = state.preview;
      const auto status = viewer.ApplyRoomTransfer(*plan);
      state.preview.reset();
      state.error = status.ok() ? "" : std::string(status.message());
      state.status =
          status.ok() ? "Replacement applied. Undo restores this room." : "";
    }
  }
  if (!editable) {
    ImGui::TextWrapped(
        "This view is read-only. Room JSON can still be copied or exported.");
  }
  if (!valid_source && !state.import_json) {
    ImGui::TextWrapped("Choose a different source room between 000 and 127.");
  }
  if (state.import_json && state.json.size() > kMaxDungeonRoomDocumentBytes) {
    ImGui::TextWrapped("Room JSON exceeds the 1 MiB document limit.");
  }
  ImGui::TextWrapped(
      "Room JSON reuses numeric asset IDs. Graphics, palettes, layouts and "
      "messages must already exist in the target project.");
  ImGui::TextWrapped(
      "Entrances, incoming links, pit damage, sprite sort mode and reserved "
      "header data stay in the target room.");
  if (!state.error.empty()) {
    ImGui::TextWrapped("Room transfer: %s", state.error.c_str());
  }
  if (state.can_retry_without_properties) {
    ImGui::TextWrapped(
        "Keep this room's shared properties and preview the other selected "
        "contents instead.");
    ImGui::BeginDisabled(!editable || !valid_source);
    if (ImGui::Button("Preview without room properties", ImVec2(-1, 0))) {
      state.domains &= ~kTransferMetadata;
      state.copy_destinations = false;
      PreviewReplacement(viewer, state);
    }
    gui::AutoRegisterLastItem("button", "TransferPreviewWithoutProperties");
    ImGui::EndDisabled();
  }
  if (!state.status.empty()) {
    ImGui::TextWrapped("%s", state.status.c_str());
  }
  ImGui::PopID();
  ImGui::PopID();
  ImGui::PopID();
}

void DrawDungeonRoomTransferPopup(DungeonCanvasViewer& viewer) {
  auto& state = viewer.room_transfer_state();
  ImGui::PushID(&viewer);
  constexpr const char* kPopup = "Clone / Import Room##RoomTransfer";
  if (state.request_popup) {
    state.request_popup = false;
    ImGui::OpenPopup(kPopup);
  }
  ImGui::SetNextWindowSize(ImVec2(440, 680), ImGuiCond_FirstUseEver);
  bool open = true;
  if (ImGui::BeginPopupModal(kPopup, &open, ImGuiWindowFlags_NoSavedSettings)) {
    state.popup_open = true;
    if (state.popup_room_id != viewer.current_room_id()) {
      ImGui::CloseCurrentPopup();
    } else {
      DrawDungeonRoomTransferEditor(viewer);
      if (ImGui::Button("Close", ImVec2(-1, 0))) {
        ImGui::CloseCurrentPopup();
      }
      gui::AutoRegisterLastItem("button", "TransferClose");
    }
    ImGui::EndPopup();
  } else {
    state.popup_open = false;
  }
  ImGui::PopID();
}

}  // namespace yaze::editor
