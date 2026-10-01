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

class MainGraphicsSheetAction final : public UndoAction {
 public:
  using Apply = std::function<absl::Status(uint8_t, uint8_t)>;
  MainGraphicsSheetAction(int group, int slot, uint8_t before, uint8_t after,
                          Apply apply)
      : description_(
            absl::StrFormat("Main graphics group %02X slot %d", group, slot)),
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
  ResetMainPicker();
  for (auto& preview : blockset_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
  for (auto& preview : roomset_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
  for (auto& preview : spriteset_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
}

void GfxGroupEditor::ResetMainPicker() {
  show_main_picker_ = false;
  pending_sheet_ = -1;
  picker_group_ = -1;
  for (auto& [id, preview] : main_picker_previews_)
    gfx::Arena::Get().RetireBitmap(preview);
  main_picker_previews_.clear();
  assignment_error_.clear();
}

void GfxGroupEditor::SetRom(Rom* rom) {
  if (rom_ != rom) {
    ++assignment_session_->generation;
    ResetMainPicker();
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
  ResetMainPicker();
  current_palette_ = nullptr;
  game_data_ = data;
  assignment_session_->data = data;
}

void GfxGroupEditor::DetachRuntimeContext() {
  SetRom(nullptr);
  SetGameData(nullptr);
  undo_ = nullptr;
  main_usage_ = {};
  workspace_ = nullptr;
}

absl::Status GfxGroupEditor::EditMainGraphicsSheet(int group, int slot,
                                                   int sheet) {
  if (!rom_ || !rom_->is_loaded() || !game_data_ ||
      !game_data_->gfx_groups_loaded || !undo_) {
    return absl::FailedPreconditionError("No editable graphics session");
  }
  if (group < 0 || group >= game_data_->main_blockset_ids.size() || slot < 0 ||
      slot >= 8 || sheet < 0 ||
      sheet >= zelda3::GraphicsSheetStore::kSheetCount) {
    return absl::InvalidArgumentError(
        "Choose a main group, slot 0-7 and sheet");
  }
  if (!game_data_->sheet_store.HasSheet(sheet) ||
      zelda3::GetGfxSheetStorageKind(sheet) ==
          zelda3::GfxSheetStorageKind::kCompressed2bpp) {
    return absl::InvalidArgumentError(
        "Main backgrounds require a loaded 3bpp sheet");
  }
  const uint8_t before = game_data_->main_blockset_ids[group][slot];
  const auto after = static_cast<uint8_t>(sheet);
  if (before == after)
    return absl::OkStatus();
  const std::weak_ptr<AssignmentSession> weak = assignment_session_;
  auto apply = [weak, generation = assignment_session_->generation, group,
                slot](uint8_t expected, uint8_t value) -> absl::Status {
    const auto session = weak.lock();
    if (!session || session->generation != generation || !session->rom ||
        !session->rom->is_loaded() || !session->data ||
        !session->data->gfx_groups_loaded) {
      return absl::FailedPreconditionError(
          "Graphics assignment belongs to another session");
    }
    auto& assigned = session->data->main_blockset_ids[group][slot];
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
  undo_->Push(std::make_unique<MainGraphicsSheetAction>(
      group, slot, before, after, std::move(apply)));
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

void GfxGroupEditor::DrawMainAssignmentControls() {
  const int group = Ws().selected_blockset;
  if (picker_group_ != -1 && picker_group_ != group)
    ResetMainPicker();
  for (int slot = 0; slot < 8; ++slot) {
    const auto label =
        absl::StrFormat("Slot %d / Sheet %02X", slot,
                        game_data_->main_blockset_ids[group][slot]);
    if (ImGui::Selectable(label.c_str(), selected_main_slot_ == slot)) {
      if (selected_main_slot_ != slot)
        ResetMainPicker();
      selected_main_slot_ = slot;
    }
    gui::AutoRegisterLastItem("selectable",
                              absl::StrFormat("Main slot %d", slot));
  }
  DrawMainUsage();
  ImGui::BeginDisabled(!undo_ || !main_usage_ ||
                       !game_data_->gfx_groups_loaded);
  if (ImGui::Button("Choose replacement sheet")) {
    const bool open = !show_main_picker_;
    ResetMainPicker();
    show_main_picker_ = open;
    picker_group_ = group;
    picker_expected_sheet_ =
        game_data_->main_blockset_ids[group][selected_main_slot_];
  }
  gui::AutoRegisterLastItem("button", "Choose replacement sheet");
  ImGui::EndDisabled();
  if (show_main_picker_)
    DrawMainSheetPicker();
  if (!assignment_error_.empty())
    ImGui::TextWrapped("%s", assignment_error_.c_str());
}

void GfxGroupEditor::DrawMainSheetPicker() {
  ImGui::TextWrapped(
      "Preview uses the selected palette or slot default. Sheet 00 is a "
      "physical sheet, not inheritance.");
  if (pending_sheet_ >= 0) {
    if (ImGui::Button("Apply to shared main group")) {
      if (game_data_->main_blockset_ids[picker_group_][selected_main_slot_] !=
          picker_expected_sheet_) {
        assignment_error_ =
            "Assignment changed while choosing. Reopen the picker.";
      } else {
        const auto status = EditMainGraphicsSheet(
            picker_group_, selected_main_slot_, pending_sheet_);
        assignment_error_ = status.ok() ? "" : std::string(status.message());
        if (status.ok())
          show_main_picker_ = false;
      }
    }
    gui::AutoRegisterLastItem("button", "Apply to shared main group");
    ImGui::Text("Selected sheet %02X", pending_sheet_);
  }
  if (ImGui::BeginChild("##MainSheetPicker", ImVec2(0, 220), true)) {
    std::vector<int> candidates;
    for (int id = 0; id < zelda3::GraphicsSheetStore::kSheetCount; ++id) {
      if (game_data_->sheet_store.HasSheet(id) &&
          zelda3::GetGfxSheetStorageKind(id) !=
              zelda3::GfxSheetStorageKind::kCompressed2bpp) {
        candidates.push_back(id);
      }
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(candidates.size()), 54.0f);
    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const int id = candidates[row];
        auto* preview = PrepareSheetPreview(
            id, gfx::RoleForBlocksetSlot(selected_main_slot_),
            main_picker_previews_[id]);
        const auto position = ImGui::GetCursorScreenPos();
        const auto label = absl::StrFormat("Sheet %02X##main_replacement", id);
        ImGui::PushID(id);
        if (ImGui::Selectable(label.c_str(), pending_sheet_ == id, 0,
                              ImVec2(0, 50)))
          pending_sheet_ = id;
        gui::AutoRegisterLastItem("selectable",
                                  absl::StrFormat("Main sheet %02X", id));
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
