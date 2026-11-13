#include "app/editor/dungeon/inspectors/dungeon_room_swap_dialog.h"

#include <cfloat>
#include <optional>
#include <string>
#include <utility>

#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

constexpr const char* kSwapPopup = "Swap Room Data##DungeonRoomSwap";

}  // namespace

void DungeonRoomSwapDialog::Open(int first_room_id, int second_room_id) {
  if (first_room_id != first_room_id_ || second_room_id != second_room_id_) {
    InvalidatePreview();
  }
  first_room_id_ = first_room_id;
  second_room_id_ = second_room_id;
  request_open_ = true;
}

void DungeonRoomSwapDialog::Reset() {
  InvalidatePreview();
  request_open_ = false;
}

void DungeonRoomSwapDialog::InvalidatePreview() {
  preview_.reset();
  error_.clear();
  status_.clear();
  can_retry_without_properties_ = false;
}

bool DungeonRoomSwapDialog::HasValidRooms() const {
  auto valid = [](int id) {
    return id >= 0 && id < zelda3::kNumberOfRooms;
  };
  return valid(first_room_id_) && valid(second_room_id_) &&
         first_room_id_ != second_room_id_;
}

void DungeonRoomSwapDialog::RunPreview() {
  InvalidatePreview();
  if (!preview_callback_) {
    error_ = "Room swap is unavailable";
    return;
  }
  auto plan = preview_callback_(first_room_id_, second_room_id_,
                                {domains_, copy_destinations_});
  if (plan.ok()) {
    preview_ = std::make_shared<const DungeonRoomSwapPlan>(*std::move(plan));
    return;
  }
  error_ = std::string(plan.status().message());
  can_retry_without_properties_ =
      (domains_ & kTransferMetadata) != 0 &&
      (domains_ & ~kTransferMetadata) != 0 &&
      plan.status().GetPayload(kRoomTransferSharedHeaderPayload).has_value();
}

void DungeonRoomSwapDialog::Draw() {
  if (request_open_) {
    request_open_ = false;
    ImGui::OpenPopup(kSwapPopup);
  }
  // Fixed width for wrapped text; height follows the preview, and the modal
  // stays centered so Apply remains on screen once counts appear.
  ImGui::SetNextWindowSizeConstraints(ImVec2(420, 0), ImVec2(420, FLT_MAX));
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  bool keep_open = true;
  if (!ImGui::BeginPopupModal(kSwapPopup, &keep_open,
                              ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
    open_ = false;
    return;
  }
  open_ = true;
  // The scope pushes an ImGui ID, so it must end before EndPopup().
  std::optional<gui::AutoWidgetScope> scope(std::in_place, "Dungeon/RoomSwap");

  ImGui::TextWrapped("Exchange the selected contents of two rooms.");
  const bool editable = !read_only_ && preview_callback_ && apply_callback_;
  ImGui::BeginDisabled(!editable);
  constexpr ImGuiTableFlags kRoomTableFlags = ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("##SwapRooms", 2, kRoomTableFlags)) {
    auto room_field = [&](const char* label, const char* id, int* room_id,
                          uint64_t slot) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(label);
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(ImGui::CalcTextSize("0000").x +
                              ImGui::GetStyle().FramePadding.x * 2.0f);
      if (gui::InputScalarDeferred(id, ImGuiDataType_S32, room_id, "%03X",
                                   ImGuiInputTextFlags_CharsHexadecimal,
                                   {reinterpret_cast<uintptr_t>(this), slot})) {
        InvalidatePreview();
      }
      gui::AutoRegisterLastItem("input_int", id + 2);
      ImGui::SameLine();
      if (*room_id >= 0 && *room_id < zelda3::kNumberOfRooms) {
        ImGui::TextDisabled("%s", zelda3::GetRoomLabel(*room_id).c_str());
      } else {
        ImGui::TextDisabled("Invalid room");
      }
    };
    room_field("Room A", "##SwapFirstRoom", &first_room_id_, 0);
    room_field("Room B", "##SwapSecondRoom", &second_room_id_, 1);
    ImGui::EndTable();
  }
  if (ImGui::SmallButton(ICON_MD_SWAP_VERT " Exchange A and B")) {
    std::swap(first_room_id_, second_room_id_);
    InvalidatePreview();
  }
  gui::AutoRegisterLastItem("button", "SwapExchangeRooms");

  ImGui::Spacing();
  if (DrawDungeonRoomTransferDomainChoices(&domains_, &copy_destinations_)) {
    InvalidatePreview();
  }
  ImGui::TextWrapped("Unchecked domains stay in their room.");
  const bool valid_rooms = HasValidRooms();
  ImGui::BeginDisabled(!valid_rooms || domains_ == 0);
  if (ImGui::Button("Preview Swap", ImVec2(-1, 0))) {
    RunPreview();
  }
  gui::AutoRegisterLastItem("button", "SwapPreview");
  ImGui::EndDisabled();
  ImGui::EndDisabled();
  if (!valid_rooms) {
    ImGui::TextWrapped("Choose two different rooms between 000 and 127.");
  }

  if (preview_) {
    ImGui::Separator();
    ImGui::TextWrapped("Room %03X receives room %03X's contents:",
                       preview_->first.target_room_id,
                       preview_->second.target_room_id);
    ImGui::Indent();
    DrawDungeonRoomTransferCounts(preview_->first);
    ImGui::Unindent();
    ImGui::TextWrapped("Room %03X receives room %03X's contents:",
                       preview_->second.target_room_id,
                       preview_->first.target_room_id);
    ImGui::Indent();
    DrawDungeonRoomTransferCounts(preview_->second);
    ImGui::Unindent();
    if (!preview_->changed()) {
      ImGui::TextWrapped("The selected contents already match.");
    }
    ImGui::BeginDisabled(!editable || !preview_->changed());
    const bool apply =
        ImGui::Button(ICON_MD_SWAP_HORIZ " Apply Swap", ImVec2(-1, 0));
    gui::AutoRegisterLastItem("button", "SwapApply");
    ImGui::EndDisabled();
    if (apply) {
      // Publication refreshes room views; keep the reviewed plan alive for
      // the callback rather than borrowing it from dialog state.
      const auto plan = preview_;
      const auto status = apply_callback_(*plan);
      preview_.reset();
      error_ = status.ok() ? "" : std::string(status.message());
      status_ = status.ok() ? "Rooms swapped. Undo restores both rooms." : "";
    }
  }

  ImGui::TextWrapped(
      "Entrances, incoming links, pit damage, sprite sort mode and reserved "
      "header data stay with each room ID.");
  if (!error_.empty()) {
    ImGui::TextWrapped("Room swap: %s", error_.c_str());
  }
  if (can_retry_without_properties_) {
    ImGui::TextWrapped(
        "Keep each room's shared properties and preview the other selected "
        "contents instead.");
    ImGui::BeginDisabled(!editable || !valid_rooms);
    if (ImGui::Button("Preview without room properties", ImVec2(-1, 0))) {
      domains_ &= ~kTransferMetadata;
      copy_destinations_ = false;
      RunPreview();
    }
    gui::AutoRegisterLastItem("button", "SwapPreviewWithoutProperties");
    ImGui::EndDisabled();
  }
  if (!status_.empty()) {
    ImGui::TextWrapped("%s", status_.c_str());
  }
  if (ImGui::Button("Close", ImVec2(-1, 0))) {
    ImGui::CloseCurrentPopup();
  }
  gui::AutoRegisterLastItem("button", "SwapClose");
  scope.reset();
  ImGui::EndPopup();
}

}  // namespace yaze::editor
