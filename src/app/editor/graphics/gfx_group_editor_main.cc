#include "app/editor/graphics/gfx_group_editor.h"

#include <utility>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/sheet_role_palette_table.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/automation/widget_id_registry.h"
#include "imgui/imgui.h"
#include "util/macro.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::editor {
namespace {

class GraphicsSheetAssignmentAction final : public UndoAction {
 public:
  using Apply = std::function<absl::Status(uint8_t, uint8_t)>;
  GraphicsSheetAssignmentAction(const char* kind, int group, int slot,
                                uint8_t before, uint8_t after, Apply apply)
      : description_(absl::StrFormat("%s graphics group %02X slot %d", kind,
                                     group, slot)),
        before_(before),
        after_(after),
        apply_(std::move(apply)) {}
  absl::Status Undo() override { return apply_(after_, before_); }
  absl::Status Redo() override { return apply_(before_, after_); }
  std::string Description() const override { return description_; }
  size_t MemoryUsage() const override { return sizeof(*this); }

 private:
  std::string description_;
  uint8_t before_;
  uint8_t after_;
  Apply apply_;
};

void DrawIds(const char* label, const std::vector<int>& ids, int digits) {
  ImGui::TextWrapped("%s (%zu)", label, ids.size());
  std::string text;
  for (const int id : ids) {
    if (!text.empty())
      text += ", ";
    text += absl::StrFormat("%0*X", digits, id);
  }
  ImGui::TextWrapped("%s", text.empty() ? "None" : text.c_str());
}

}  // namespace

GfxGroupEditor::~GfxGroupEditor() {
  ResetSheetPicker();
  for (auto& preview : blockset_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
  for (auto& preview : roomset_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
  for (auto& preview : spriteset_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
}

void GfxGroupEditor::ResetSheetPicker() {
  show_sheet_picker_ = false;
  pending_sheet_ = -1;
  picker_group_ = -1;
  for (auto& [id, preview] : picker_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
  picker_previews_.clear();
  assignment_error_.clear();
}

void GfxGroupEditor::SetRom(Rom* rom) {
  if (rom_ != rom) {
    ++assignment_session_->generation;
    ResetSheetPicker();
    usage_areas_.reset();
    usage_rooms_.reset();
    current_palette_ = nullptr;
  }
  rom_ = rom;
  assignment_session_->rom = rom;
}

void GfxGroupEditor::SetGameData(zelda3::GameData* data) {
  // EditorSet rebinds after ROM replacement/load, including in-place reloads
  // and rollback where both pointer addresses are unchanged. Every binding
  // notification starts a new assignment generation.
  ++assignment_session_->generation;
  ResetSheetPicker();
  current_palette_ = nullptr;
  game_data_ = data;
  assignment_session_->data = data;
}

void GfxGroupEditor::DetachRuntimeContext() {
  SetRom(nullptr);
  SetGameData(nullptr);
  undo_ = nullptr;
  main_usage_ = {};
  roomset_usage_ = {};
  workspace_ = nullptr;
}

absl::Status GfxGroupEditor::EditMainGraphicsSheet(int group, int slot,
                                                   int sheet) {
  return EditGraphicsSheet(GroupKind::kMain, group, slot, sheet);
}

absl::Status GfxGroupEditor::EditRoomsetGraphicsSheet(int group, int entry,
                                                      int sheet) {
  return EditGraphicsSheet(GroupKind::kRoomset, group, entry, sheet);
}

absl::Status GfxGroupEditor::EditGraphicsSheet(GroupKind kind, int group,
                                               int slot, int sheet) {
  if (!rom_ || !rom_->is_loaded() || !game_data_ ||
      !game_data_->gfx_groups_loaded || !undo_) {
    return absl::FailedPreconditionError("No editable graphics session");
  }
  const bool roomset = kind == GroupKind::kRoomset;
  const int groups = roomset ? game_data_->room_blockset_ids.size()
                             : game_data_->main_blockset_ids.size();
  if (group < 0 || group >= groups || slot < 0 || slot >= (roomset ? 4 : 8) ||
      sheet < 0 || sheet >= zelda3::GraphicsSheetStore::kSheetCount) {
    return absl::InvalidArgumentError(
        "Choose a valid graphics group, slot and sheet");
  }
  if (!(roomset && sheet == 0) &&
      (!game_data_->sheet_store.HasSheet(sheet) ||
       zelda3::GetGfxSheetStorageKind(sheet) ==
           zelda3::GfxSheetStorageKind::kCompressed2bpp)) {
    return absl::InvalidArgumentError(
        "Backgrounds require a loaded 3bpp sheet");
  }
  const uint8_t before = roomset ? game_data_->room_blockset_ids[group][slot]
                                 : game_data_->main_blockset_ids[group][slot];
  const auto after = static_cast<uint8_t>(sheet);
  if (before == after)
    return absl::OkStatus();
  const std::weak_ptr<AssignmentSession> weak = assignment_session_;
  auto apply = [weak, generation = assignment_session_->generation, group, slot,
                roomset](uint8_t expected, uint8_t value) -> absl::Status {
    const auto session = weak.lock();
    if (!session || session->generation != generation || !session->rom ||
        !session->rom->is_loaded() || !session->data ||
        !session->data->gfx_groups_loaded) {
      return absl::FailedPreconditionError(
          "Graphics assignment belongs to another session");
    }
    auto& assigned = roomset ? session->data->room_blockset_ids[group][slot]
                             : session->data->main_blockset_ids[group][slot];
    if (assigned != expected) {
      return absl::FailedPreconditionError(
          "Graphics assignment changed elsewhere; Undo was not applied");
    }
    assigned = value;
    // Rooms and maps compare resolved source IDs before rendering. Leave their
    // selection and entrance context alone; they refresh on their next draw.
    session->rom->set_dirty(true);
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(apply(before, after));
  undo_->Push(std::make_unique<GraphicsSheetAssignmentAction>(
      roomset ? "Roomset" : "Main", group, roomset ? slot + 3 : slot, before,
      after, std::move(apply)));
  return absl::OkStatus();
}

void GfxGroupEditor::DrawMainUsage() {
  if (!main_usage_) {
    ImGui::TextDisabled("Main graphics usage unavailable.");
    return;
  }
  const auto usage = main_usage_(Ws().selected_blockset, selected_main_slot_);
  if (!usage.ok()) {
    ImGui::TextWrapped("Usage unavailable: %s",
                       std::string(usage.status().message()).c_str());
    return;
  }
  const auto room_count = usage->explicit_preview_rooms.size() +
                          usage->direct_default_rooms.size() +
                          usage->inferred_default_rooms.size() +
                          usage->fallback_rooms.size();
  ImGui::TextWrapped("Affects %zu room previews and %zu map screens",
                     room_count, usage->ow_areas.size());
  ImGui::TextWrapped(
      "Rooms use current/default entrance previews. This is not a census of "
      "all in-game entrance routes.");
  if (usage->slot7_bottom_half_only) {
    ImGui::TextWrapped(
        "Map slot 7: bottom half only. The animated top half has a separate "
        "source.");
  }
  if (usage->unscanned_rooms || usage->unscanned_ow_areas) {
    ImGui::TextWrapped("Incomplete scan: %d rooms / %d maps unavailable.",
                       usage->unscanned_rooms, usage->unscanned_ow_areas);
  }
  if (ImGui::TreeNode("Main assignment details")) {
    ImGui::TextWrapped(
        "%s", usage->uses_live_overworld
                  ? "Includes unsaved room and map selections."
                  : "Includes unsaved rooms; maps read from ROM until loaded.");
    if (ImGui::BeginChild("##MainUsage", ImVec2(0, 150), true)) {
      DrawIds("Explicit room previews", usage->explicit_preview_rooms, 3);
      DrawIds("Direct entrance defaults", usage->direct_default_rooms, 3);
      DrawIds("Inferred entrance defaults", usage->inferred_default_rooms, 3);
      DrawIds("Fallback previews", usage->fallback_rooms, 3);
      DrawIds("Room previews overridden in this slot", usage->overridden_rooms,
              3);
      DrawIds("Affected map screens", usage->ow_areas, 2);
      DrawIds("Overridden map slots", usage->overridden_ow_areas, 2);
      DrawIds("Maps using an independent table", usage->independent_ow_areas,
              2);
    }
    ImGui::EndChild();
    ImGui::TreePop();
  }
}

void GfxGroupEditor::DrawRoomsetUsage() {
  if (!roomset_usage_) {
    ImGui::TextDisabled("Roomset usage unavailable.");
    return;
  }
  const auto usage = roomset_usage_(Ws().selected_roomset);
  if (!usage.ok()) {
    ImGui::TextWrapped("Usage unavailable: %s",
                       std::string(usage.status().message()).c_str());
    return;
  }
  ImGui::TextWrapped("Shared by %zu rooms and %zu map screens",
                     usage->rooms.size(), usage->ow_areas.size());
  if (usage->unscanned_rooms || usage->unscanned_ow_areas) {
    ImGui::TextWrapped("Incomplete scan: %d rooms / %d maps unavailable.",
                       usage->unscanned_rooms, usage->unscanned_ow_areas);
  }
  if (ImGui::TreeNode("Roomset assignment details")) {
    ImGui::TextWrapped(
        "%s", usage->uses_live_overworld
                  ? "Includes unsaved room and map selections."
                  : "Includes unsaved rooms; maps read from ROM until loaded.");
    if (ImGui::BeginChild("##RoomsetUsage", ImVec2(0, 150), true)) {
      DrawIds("Rooms", usage->rooms, 3);
      DrawIds("Affected map screens", usage->ow_areas, 2);
      DrawIds("Maps using custom sheets", usage->overridden_ow_areas, 2);
      DrawIds("Maps using an independent table", usage->independent_ow_areas,
              2);
    }
    ImGui::EndChild();
    ImGui::TreePop();
  }
}

void GfxGroupEditor::DrawAssignmentControls(GroupKind kind) {
  const bool roomset = kind == GroupKind::kRoomset;
  const int group = roomset ? Ws().selected_roomset : Ws().selected_blockset;
  int& selected = roomset ? selected_roomset_entry_ : selected_main_slot_;
  if (picker_kind_ != kind || (picker_group_ != -1 && picker_group_ != group))
    ResetSheetPicker();
  picker_kind_ = kind;
  const auto* entries = roomset ? game_data_->room_blockset_ids[group].data()
                                : game_data_->main_blockset_ids[group].data();
  for (int slot = 0; slot < (roomset ? 4 : 8); ++slot) {
    const int display_slot = roomset ? slot + 3 : slot;
    const auto label = roomset && entries[slot] == 0
                           ? absl::StrFormat("Slot %d / Inherit", display_slot)
                           : absl::StrFormat("Slot %d / Sheet %02X",
                                             display_slot, entries[slot]);
    if (ImGui::Selectable(label.c_str(), selected == slot)) {
      if (selected != slot)
        ResetSheetPicker();
      selected = slot;
    }
    gui::AutoRegisterLastItem(
        "selectable",
        absl::StrFormat("%s slot %d", roomset ? "Roomset" : "Main",
                        display_slot));
  }
  if (roomset)
    DrawRoomsetUsage();
  else
    DrawMainUsage();
  ImGui::BeginDisabled(!undo_ || (roomset ? !roomset_usage_ : !main_usage_) ||
                       !game_data_->gfx_groups_loaded);
  if (ImGui::Button("Choose replacement sheet")) {
    const bool open = !show_sheet_picker_;
    ResetSheetPicker();
    show_sheet_picker_ = open;
    picker_group_ = group;
    picker_expected_sheet_ = entries[selected];
  }
  gui::AutoRegisterLastItem("button", "Choose replacement sheet");
  ImGui::EndDisabled();
  if (show_sheet_picker_)
    DrawSheetPicker();
  if (!assignment_error_.empty())
    ImGui::TextWrapped("%s", assignment_error_.c_str());
}

void GfxGroupEditor::DrawSheetPicker() {
  const bool roomset = picker_kind_ == GroupKind::kRoomset;
  const int selected = roomset ? selected_roomset_entry_ : selected_main_slot_;
  ImGui::TextWrapped("Preview uses the selected palette or slot default.");
  if (roomset) {
    ImGui::TextWrapped(
        "Inherit uses each room or map's main group. Its appearance can differ "
        "between areas.");
    if (ImGui::Selectable("Inherit from main group (00)", pending_sheet_ == 0))
      pending_sheet_ = 0;
    gui::AutoRegisterLastItem("selectable", "Inherit from main group");
  } else {
    ImGui::TextWrapped("Sheet 00 is a physical sheet, not inheritance.");
  }
  if (pending_sheet_ >= 0) {
    const char* apply_label =
        roomset ? "Apply to shared roomset" : "Apply to shared main group";
    if (ImGui::Button(apply_label)) {
      const auto current =
          roomset ? game_data_->room_blockset_ids[picker_group_][selected]
                  : game_data_->main_blockset_ids[picker_group_][selected];
      if (current != picker_expected_sheet_) {
        assignment_error_ =
            "Assignment changed while choosing. Reopen the picker.";
      } else {
        const auto status = EditGraphicsSheet(picker_kind_, picker_group_,
                                              selected, pending_sheet_);
        assignment_error_ = status.ok() ? "" : std::string(status.message());
        if (status.ok())
          show_sheet_picker_ = false;
      }
    }
    gui::AutoRegisterLastItem("button", apply_label);
    if (roomset && pending_sheet_ == 0)
      ImGui::TextUnformatted("Selected: Inherit");
    else
      ImGui::Text("Selected sheet %02X", pending_sheet_);
  }
  if (ImGui::BeginChild("##SheetPicker", ImVec2(0, 220), true)) {
    std::vector<int> candidates;
    for (int id = roomset ? 1 : 0; id < zelda3::GraphicsSheetStore::kSheetCount;
         ++id) {
      if (game_data_->sheet_store.HasSheet(id) &&
          zelda3::GetGfxSheetStorageKind(id) !=
              zelda3::GfxSheetStorageKind::kCompressed2bpp)
        candidates.push_back(id);
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(candidates.size()), 54.0f);
    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const int id = candidates[row];
        auto* preview =
            PrepareSheetPreview(id,
                                roomset ? gfx::RoleForRoomsetSlot(selected)
                                        : gfx::RoleForBlocksetSlot(selected),
                                picker_previews_[id]);
        const auto position = ImGui::GetCursorScreenPos();
        const auto label = absl::StrFormat("Sheet %02X##replacement", id);
        ImGui::PushID(id);
        if (ImGui::Selectable(label.c_str(), pending_sheet_ == id, 0,
                              ImVec2(0, 50)))
          pending_sheet_ = id;
        gui::AutoRegisterLastItem(
            "selectable",
            absl::StrFormat("%s sheet %02X", roomset ? "Roomset" : "Main", id));
        if (preview && preview->texture()) {
          ImGui::GetWindowDrawList()->AddImage(
              (ImTextureID)preview->texture(),
              ImVec2(position.x + 84, position.y + 8),
              ImVec2(position.x + 212, position.y + 40));
        }
        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
}

}  // namespace yaze::editor
