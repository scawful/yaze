#include "app/editor/dungeon/workspace/room_matrix_census.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "core/hack_manifest.h"
#include "rom/rom.h"

namespace yaze::editor {

namespace {

// Categorical owner colors. Index 7 is purple so the eighth dungeon (Oracle
// D8, Fortress of Secrets) matches the census PNG. Bright green and yellow
// are left out: they mark free and reclaimable outlines.
constexpr std::array<ImVec4, 16> kOwnerPalette = {{
    {0.25f, 0.66f, 0.63f, 1.0f},  // teal
    {0.88f, 0.53f, 0.23f, 1.0f},  // orange
    {0.82f, 0.34f, 0.30f, 1.0f},  // red
    {0.29f, 0.50f, 0.83f, 1.0f},  // blue
    {0.42f, 0.76f, 0.88f, 1.0f},  // ice
    {0.65f, 0.47f, 0.25f, 1.0f},  // brown
    {0.36f, 0.42f, 0.75f, 1.0f},  // indigo
    {0.60f, 0.36f, 0.78f, 1.0f},  // purple
    {0.83f, 0.42f, 0.62f, 1.0f},  // pink
    {0.69f, 0.55f, 0.42f, 1.0f},  // tan
    {0.48f, 0.56f, 0.65f, 1.0f},  // slate
    {0.56f, 0.23f, 0.27f, 1.0f},  // maroon
    {0.30f, 0.52f, 0.45f, 1.0f},  // pine
    {0.55f, 0.30f, 0.50f, 1.0f},  // plum
    {0.20f, 0.40f, 0.55f, 1.0f},  // deep blue
    {0.75f, 0.40f, 0.20f, 1.0f},  // rust
}};
constexpr ImVec4 kInteriorColor = {0.50f, 0.50f, 0.52f, 1.0f};
constexpr ImVec4 kUnownedColor = {0.16f, 0.16f, 0.18f, 1.0f};
constexpr float kTooltipWrapWidth = 380.0f;

// Theme status colors can be muted; outlines on small cells need a clearly
// green / yellow / red hue, so keep the theme's hue and lift saturation and
// value.
ImVec4 Vivid(ImVec4 color) {
  float h = 0.0f;
  float s = 0.0f;
  float v = 0.0f;
  ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
  s = std::max(s, 0.65f);
  v = std::max(v, 0.85f);
  ImVec4 out(0, 0, 0, color.w);
  ImGui::ColorConvertHSVtoRGB(h, s, v, out.x, out.y, out.z);
  return out;
}
ImVec4 FreeColor() {
  return Vivid(gui::GetSuccessColor());
}
ImVec4 ReclaimableColor() {
  return Vivid(gui::GetWarningColor());
}
ImVec4 OrphanColor() {
  return Vivid(gui::GetErrorColor());
}

}  // namespace

RoomMatrixCensusOverlay::RoomMatrixCensusOverlay() = default;
RoomMatrixCensusOverlay::~RoomMatrixCensusOverlay() = default;

ImVec4 RoomMatrixCensusOverlay::OwnerColor(int owner_index, bool interior) {
  if (interior) {
    return kInteriorColor;
  }
  if (owner_index < 0) {
    return kUnownedColor;
  }
  return kOwnerPalette[static_cast<size_t>(owner_index) % kOwnerPalette.size()];
}

void RoomMatrixCensusOverlay::StartComputation(Rom* rom) {
  // Work on a copy so edits and saves on the live ROM never race the
  // background read. Project data is copied too.
  auto rom_copy = std::make_shared<Rom>(*rom);
  std::vector<zelda3::RoomCensusOwnerGroup> owners;
  std::set<uint8_t> warp_tags;
  if (manifest_provider_) {
    if (const core::HackManifest* manifest = manifest_provider_()) {
      owners =
          zelda3::RoomCensusOwnersFromProject(manifest->project_registry());
      warp_tags = zelda3::RoomCensusWarpTagsFromManifest(*manifest);
    }
  }
  pending_ = std::async(
      std::launch::async,
      [rom_copy, owners = std::move(owners), warp_tags = std::move(warp_tags)]()
          -> absl::StatusOr<zelda3::RoomCensus> {
        auto input = zelda3::CollectRoomCensusInput(rom_copy.get());
        if (!input.ok()) {
          return input.status();
        }
        input->project_owners = owners;
        input->warp_tag_ids = warp_tags;
        return zelda3::BuildRoomCensus(*input);
      });
}

void RoomMatrixCensusOverlay::Update(Rom* rom) {
  if (pending_.valid() &&
      pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto result = pending_.get();
    if (result.ok()) {
      census_ = std::move(result).value();
      error_.clear();
      largest_block_.clear();
      if (!census_->free_clusters.empty()) {
        const auto& rooms = census_->free_clusters.front().rooms;
        largest_block_.insert(rooms.begin(), rooms.end());
      }
      // Owner indices may have changed.
      filter_owners_.clear();
    } else {
      census_.reset();
      error_ = std::string(result.status().message());
    }
  }
  if (!enabled_) {
    return;
  }
  if (rom == nullptr || !rom->is_loaded()) {
    census_.reset();
    computed_for_ = nullptr;
    return;
  }
  if (!pending_.valid() && (stale_ || computed_for_ != rom)) {
    stale_ = false;
    computed_for_ = rom;
    StartComputation(rom);
  }
}

void RoomMatrixCensusOverlay::WrapBeforeItem(float item_width, bool first) {
  if (first) {
    return;
  }
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float right =
      ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
  if (ImGui::GetItemRectMax().x + spacing + item_width <= right) {
    ImGui::SameLine();
  }
}

bool RoomMatrixCensusOverlay::DrawChip(const char* id,
                                       const std::string& label_in,
                                       ImVec4 swatch, bool outline_only,
                                       bool active, bool first) {
  const ImGuiStyle& style = ImGui::GetStyle();
  const float swatch_size = ImGui::GetTextLineHeight() - 2.0f;
  const float pad_x = style.FramePadding.x;
  const float pad_y = 2.0f;
  const float line_width = std::max(40.0f, ImGui::GetContentRegionAvail().x);

  // Shorten the label until the chip fits on an empty line: never clip.
  std::string label = label_in;
  auto chip_width = [&](const std::string& text) {
    return pad_x * 2 + swatch_size + style.ItemInnerSpacing.x +
           ImGui::CalcTextSize(text.c_str()).x;
  };
  while (label.size() > 4 && chip_width(label) > line_width) {
    label = label.substr(0, label.size() - 4) + "...";
  }
  const ImVec2 size(chip_width(label), ImGui::GetTextLineHeight() + pad_y * 2);

  WrapBeforeItem(size.x, first);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(id, size);
  gui::AutoRegisterLastItem("button", absl::StrCat("Census chip ", id + 2));
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 max(pos.x + size.x, pos.y + size.y);
  ImVec4 bg = style.Colors[active    ? ImGuiCol_ButtonActive
                           : hovered ? ImGuiCol_ButtonHovered
                                     : ImGuiCol_FrameBg];
  draw_list->AddRectFilled(pos, max, ImGui::ColorConvertFloat4ToU32(bg), 4.0f);
  if (active) {
    draw_list->AddRect(pos, max, ImGui::GetColorU32(ImGuiCol_Text), 4.0f, 0,
                       1.5f);
  }
  const ImVec2 swatch_min(pos.x + pad_x, pos.y + (size.y - swatch_size) * 0.5f);
  const ImVec2 swatch_max(swatch_min.x + swatch_size,
                          swatch_min.y + swatch_size);
  if (outline_only) {
    draw_list->AddRectFilled(swatch_min, swatch_max,
                             ImGui::ColorConvertFloat4ToU32(kUnownedColor));
    draw_list->AddRect(swatch_min, swatch_max,
                       ImGui::ColorConvertFloat4ToU32(swatch), 0.0f, 0, 2.0f);
  } else {
    draw_list->AddRectFilled(swatch_min, swatch_max,
                             ImGui::ColorConvertFloat4ToU32(swatch), 2.0f);
  }
  draw_list->AddText(
      ImVec2(swatch_max.x + style.ItemInnerSpacing.x, pos.y + pad_y),
      ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
  if (hovered && label != label_in) {
    ImGui::SetTooltip("%s", label_in.c_str());
  }
  return clicked;
}

void RoomMatrixCensusOverlay::ToggleOwnerFilter(int owner_index) {
  if (!filter_owners_.erase(owner_index)) {
    filter_owners_.insert(owner_index);
  }
}

void RoomMatrixCensusOverlay::DrawControls(Rom* rom) {
  ImGui::PushID("RoomMatrixCensus");
  bool on = enabled_;
  if (ImGui::Checkbox("Census overlay", &on)) {
    enabled_ = on;
    Update(rom);
  }
  gui::AutoRegisterLastItem("checkbox", "Census overlay");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Color rooms by dungeon/area; outline free (green) and reclaimable "
        "(yellow) rooms. Same model as z3ed dungeon-room-census.");
  }
  if (!enabled_) {
    ImGui::PopID();
    return;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(computing() || rom == nullptr);
  if (ImGui::SmallButton(ICON_MD_REFRESH " Refresh")) {
    Invalidate();
    Update(rom);
  }
  gui::AutoRegisterLastItem("button", "Census Refresh");
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "Recompute from the ROM (unsaved edits are not "
        "included until saved).");
  }

  if (computing() && !census_.has_value()) {
    ImGui::TextDisabled("Computing room census...");
    ImGui::PopID();
    return;
  }
  if (!error_.empty()) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(gui::GetErrorColor(), "Census failed: %s",
                       error_.c_str());
    ImGui::PopTextWrapPos();
  }
  if (!census_.has_value()) {
    ImGui::PopID();
    return;
  }
  const auto& census = *census_;
  ImGui::PushTextWrapPos(0.0f);
  // Info icon first: after wrapped text it could land past the right edge.
  ImGui::TextDisabled(ICON_MD_INFO_OUTLINE);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Owners: %s\nVanilla baseline: %s\nEntrance table: %s",
                      census.owners_from_project ? "project dungeons.json"
                                                 : "derived from entrances",
                      census.has_vanilla_baseline
                          ? census.vanilla_baseline_source.c_str()
                          : "none",
                      census.expanded_entrance_tables ? "ZScream bank $0F"
                                                      : "vanilla $02:C813");
  }
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextWrapped("%d free, %d reclaimable, %d in use (%d orphan)%s",
                     census.free_count, census.reclaimable_count,
                     census.in_use_count, census.orphan_count,
                     computing() ? " - refreshing..." : "");
  if (!census.free_clusters.empty()) {
    ImGui::TextColored(FreeColor(), ICON_MD_CROP_FREE);
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextWrapped("Largest free block: %s",
                       census.free_clusters.front().summary.c_str());
  }
  ImGui::PopTextWrapPos();

  ImGui::SeparatorText("Census legend (click to filter)");
  bool first = true;
  if (DrawChip("##chip_free", absl::StrFormat("Free (%d)", census.free_count),
               FreeColor(), true, filter_free_, first)) {
    filter_free_ = !filter_free_;
  }
  first = false;
  if (DrawChip("##chip_reclaimable",
               absl::StrFormat("Reclaimable (%d)", census.reclaimable_count),
               ReclaimableColor(), true, filter_reclaimable_, first)) {
    filter_reclaimable_ = !filter_reclaimable_;
  }
  if (census.orphan_count > 0 &&
      DrawChip("##chip_orphan",
               absl::StrFormat("Orphan (%d)", census.orphan_count),
               OrphanColor(), true, filter_orphan_, first)) {
    filter_orphan_ = !filter_orphan_;
  }
  for (size_t i = 0; i < census.owners.size(); ++i) {
    const auto& owner = census.owners[i];
    if (owner.room_count == 0) {
      continue;
    }
    // Short labels keep the legend to a few lines; the chip tooltip and the
    // cell tooltip give the full name.
    std::string label;
    if (owner.from_project) {
      label = absl::StrFormat("%s (%d)", owner.id, owner.room_count);
    } else if (owner.interior || census.vanilla_owner_names) {
      label = absl::StrFormat("%s (%d)", owner.name, owner.room_count);
    } else {
      label =
          absl::StrFormat("ID 0x%02X (%d)", owner.dungeon_id, owner.room_count);
    }
    const std::string id = absl::StrFormat("##chip_owner_%d", i);
    const int index = static_cast<int>(i);
    if (DrawChip(id.c_str(), label, OwnerColor(index, owner.interior), false,
                 filter_owners_.contains(index), first)) {
      ToggleOwnerFilter(index);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s %s: %d rooms. Click to filter.", owner.id.c_str(),
                        owner.name.c_str(), owner.room_count);
    }
  }
  if (HasActiveChips()) {
    WrapBeforeItem(ImGui::CalcTextSize("Clear filters").x +
                       ImGui::GetStyle().FramePadding.x * 2,
                   false);
    if (ImGui::SmallButton("Clear filters")) {
      filter_free_ = filter_reclaimable_ = filter_orphan_ = false;
      filter_owners_.clear();
    }
  }
  ImGui::PopID();
}

ImU32 RoomMatrixCensusOverlay::FillColor(int room_id) const {
  if (!census_.has_value() || room_id < 0 ||
      room_id >= static_cast<int>(census_->rooms.size())) {
    return ImGui::ColorConvertFloat4ToU32(kUnownedColor);
  }
  const auto& entry = census_->rooms[room_id];
  ImVec4 color = OwnerColor(entry.owner_index, entry.interior);
  if (entry.owner_index < 0) {
    color = kUnownedColor;
  }
  // Keep the hex label readable on light fills.
  color.x *= 0.78f;
  color.y *= 0.78f;
  color.z *= 0.78f;
  return ImGui::ColorConvertFloat4ToU32(color);
}

std::optional<ImU32> RoomMatrixCensusOverlay::StatusOutline(int room_id) const {
  if (!census_.has_value() || room_id < 0 ||
      room_id >= static_cast<int>(census_->rooms.size())) {
    return std::nullopt;
  }
  switch (census_->rooms[room_id].status) {
    case zelda3::RoomCensusStatus::kFree:
      return ImGui::ColorConvertFloat4ToU32(FreeColor());
    case zelda3::RoomCensusStatus::kReclaimable:
      return ImGui::ColorConvertFloat4ToU32(ReclaimableColor());
    case zelda3::RoomCensusStatus::kInUse:
      break;
  }
  return std::nullopt;
}

ImU32 RoomMatrixCensusOverlay::OrphanMarkColor() {
  return ImGui::ColorConvertFloat4ToU32(OrphanColor());
}

ImU32 RoomMatrixCensusOverlay::BlockHaloColor(float alpha) {
  ImVec4 color = FreeColor();
  color.w = alpha;
  return ImGui::ColorConvertFloat4ToU32(color);
}

bool RoomMatrixCensusOverlay::InLargestFreeBlock(int room_id) const {
  return largest_block_.contains(room_id);
}

bool RoomMatrixCensusOverlay::IsOrphan(int room_id) const {
  return census_.has_value() && room_id >= 0 &&
         room_id < static_cast<int>(census_->rooms.size()) &&
         census_->rooms[room_id].orphan;
}

bool RoomMatrixCensusOverlay::MatchesChips(int room_id) const {
  if (!HasActiveChips() || !census_.has_value()) {
    return true;
  }
  if (room_id < 0 || room_id >= static_cast<int>(census_->rooms.size())) {
    return false;
  }
  const auto& entry = census_->rooms[room_id];
  return (filter_free_ && entry.status == zelda3::RoomCensusStatus::kFree) ||
         (filter_reclaimable_ &&
          entry.status == zelda3::RoomCensusStatus::kReclaimable) ||
         (filter_orphan_ && entry.orphan) ||
         filter_owners_.contains(entry.owner_index);
}

void RoomMatrixCensusOverlay::DrawTooltip(int room_id) const {
  if (!census_.has_value() || room_id < 0 ||
      room_id >= static_cast<int>(census_->rooms.size())) {
    return;
  }
  const auto& census = *census_;
  const auto& entry = census.rooms[room_id];
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipWrapWidth);
  ImVec4 status_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
  const char* status_label = "In use";
  switch (entry.status) {
    case zelda3::RoomCensusStatus::kFree:
      status_color = FreeColor();
      status_label = "Free";
      break;
    case zelda3::RoomCensusStatus::kReclaimable:
      status_color = ReclaimableColor();
      status_label = "Reclaimable";
      break;
    case zelda3::RoomCensusStatus::kInUse:
      if (entry.orphan) {
        status_color = OrphanColor();
        status_label = "In use (orphan)";
      }
      break;
  }
  ImGui::TextColored(status_color, "Census: %s", status_label);
  if (entry.owner_index >= 0) {
    const auto& owner = census.owners[entry.owner_index];
    ImGui::Text("Owner: %s%s%s", owner.interior ? "" : owner.id.c_str(),
                owner.interior ? "" : " ", owner.name.c_str());
  } else {
    ImGui::TextDisabled("Owner: none");
  }
  if (InLargestFreeBlock(room_id)) {
    ImGui::TextColored(FreeColor(), "In the largest free block");
  }
  for (const auto& reason : entry.reasons) {
    ImGui::BulletText("%s", reason.c_str());
  }
  ImGui::PopTextWrapPos();
}

}  // namespace yaze::editor
