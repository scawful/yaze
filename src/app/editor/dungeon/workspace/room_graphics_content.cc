#include "app/editor/dungeon/workspace/room_graphics_content.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/window/dungeon_panel_access.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_config.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze {
namespace editor {

namespace {

void DrawRoomGraphicsScaleControl(gui::AdaptiveSheetScaleMode* mode) {
  constexpr const char* kLabels[] = {"Fit", "1x", "2x", "4x"};
  int selected = static_cast<int>(*mode);
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", tr("Scale"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::ScaledSize(80.0f, 0.0f).x);
  if (ImGui::Combo("##RoomGraphicsScale", &selected, kLabels,
                   IM_ARRAYSIZE(kLabels))) {
    *mode = static_cast<gui::AdaptiveSheetScaleMode>(selected);
  }
}

}  // namespace

RoomGraphicsContent::~RoomGraphicsContent() {
  ClearPickerPreviews();
  for (auto& preview : sheet_previews_) {
    gfx::Arena::Get().RetireBitmap(preview);
  }
}

void RoomGraphicsContent::ClearPickerPreviews() {
  for (auto& [id, bitmap] : picker_previews_) {
    gfx::Arena::Get().RetireBitmap(bitmap);
  }
  picker_previews_.clear();
}

void RoomGraphicsContent::DetachRuntimeContext() {
  runtime_detached_ = true;
  current_room_id_ = nullptr;
  rooms_ = nullptr;
  renderer_ = nullptr;
  assignment_callback_ = {};
  usage_callback_ = {};
}

void RoomGraphicsContent::DrawSharedUsage(int roomset) {
  if (!usage_callback_) {
    ImGui::TextDisabled("Shared usage unavailable.");
    return;
  }
  const auto usage = usage_callback_(roomset);
  if (!usage.ok()) {
    ImGui::TextWrapped("Shared usage unavailable: %s",
                       std::string(usage.status().message()).c_str());
    return;
  }
  const auto summary = absl::StrFormat(
      "Shared by %d room%s and %d map screen%s", usage->rooms.size(),
      usage->rooms.size() == 1 ? "" : "s", usage->ow_areas.size(),
      usage->ow_areas.size() == 1 ? "" : "s");
  ImGui::TextWrapped("%s", summary.c_str());
  if (usage->unscanned_rooms || usage->unscanned_ow_areas) {
    ImGui::TextWrapped(
        "Incomplete scan: %d rooms / %d map screens unavailable.",
        usage->unscanned_rooms, usage->unscanned_ow_areas);
  }
  const bool expanded = ImGui::TreeNode("Shared assignment details");
  gui::AutoRegisterLastItem("tree", "Shared assignment details");
  if (!expanded)
    return;
  ImGui::TextWrapped("%s", usage->uses_live_overworld
                               ? "Includes unsaved room and map selections."
                               : "Includes unsaved room selections; maps read "
                                 "from ROM until the overworld is loaded.");
  ImGui::TextWrapped(
      "Map screens include large-map children. Zero inherits "
      "each room or map's main group.");
  if (ImGui::BeginChild("##SharedRoomGraphicsUsage", ImVec2(0, 150), true)) {
    const auto draw_ids = [](const char* label, const std::vector<int>& ids,
                             int digits) {
      const auto title = absl::StrFormat("%s (%d)", label, ids.size());
      ImGui::TextWrapped("%s", title.c_str());
      if (ids.empty()) {
        ImGui::TextDisabled("None");
        return;
      }
      std::string text;
      for (const int id : ids) {
        if (!text.empty())
          text += ", ";
        text += absl::StrFormat("%0*X", digits, id);
      }
      ImGui::TextWrapped("%s", text.c_str());
    };
    draw_ids("Rooms using this assignment", usage->rooms, 3);
    draw_ids("Map screens using this assignment", usage->ow_areas, 2);
    draw_ids("Map references overridden by custom sheets",
             usage->overridden_ow_areas, 2);
    draw_ids("Map references using a separate ROM table",
             usage->independent_ow_areas, 2);
  }
  ImGui::EndChild();
  ImGui::TreePop();
}

void RoomGraphicsContent::DrawAssignmentControls(const zelda3::Room& room) {
  auto* data = rooms_->game_data();
  const auto blocks = room.blocks();
  if (!data || selected_slot_ < 0 || selected_slot_ >= 16)
    return;
  const bool room_slot = selected_slot_ >= 3 && selected_slot_ <= 6;
  const bool editable =
      room_slot && room.blockset() < data->room_blockset_ids.size();
  ImGui::Separator();
  ImGui::Text("Slot %d / Sheet %02X", selected_slot_, blocks[selected_slot_]);
  if (editable) {
    const auto assigned =
        data->room_blockset_ids[room.blockset()][selected_slot_ - 3];
    ImGui::TextDisabled(
        "%s / Room group %02X",
        assigned == 0 ? "Inherited from main group" : "Room override",
        room.blockset());
    DrawSharedUsage(room.blockset());
    ImGui::BeginDisabled(!assignment_callback_);
    if (ImGui::Button("Choose replacement sheet")) {
      show_picker_ = !show_picker_;
      pending_sheet_ = -1;
      ClearPickerPreviews();
    }
    gui::AutoRegisterLastItem("button", "Choose replacement sheet");
    if (assigned != 0) {
      ImGui::SameLine();
      if (ImGui::Button("Use main group")) {
        const auto status = assignment_callback_(room.id(), selected_slot_, 0);
        assignment_error_ = status.ok() ? "" : std::string(status.message());
        show_picker_ = false;
      }
      gui::AutoRegisterLastItem("button", "Use main group");
    }
    ImGui::EndDisabled();
  } else {
    ImGui::TextDisabled("%s", selected_slot_ < 8
                                  ? "Main group / animation source"
                              : selected_slot_ < 12 ? "Fixed sprite source"
                                                    : "Spriteset source");
    ImGui::TextWrapped(
        "Assignment editing is available for room override slots 3-6.");
  }
  if (!assignment_error_.empty())
    ImGui::TextWrapped("%s", assignment_error_.c_str());
  if (editable && show_picker_)
    DrawAssignmentPicker(room);
}

void RoomGraphicsContent::DrawAssignmentPicker(const zelda3::Room& room) {
  const auto* data = rooms_->game_data();
  if (!data || !assignment_callback_)
    return;
  // Use the loaded room's actual palette half. Thumbnails are local display
  // copies; the session store and its palettes are never changed by browsing.
  std::vector<gfx::SnesColor> colors;
  const auto& rendered = room.rendered_palette();
  if (rendered.size() < 16) {
    ImGui::TextWrapped("Load the room palette to preview replacement sheets.");
    return;
  }
  const int half = room.BackgroundPaletteOffset(selected_slot_);
  for (int index = 0; index < 8; ++index) {
    const auto color = rendered[index == 0 ? 0 : half + index];
    colors.emplace_back(color.r, color.g, color.b);
  }
  ImGui::TextDisabled(
      "Preview: room palette row 0. Select a sheet, then apply.");
  if (pending_sheet_ > 0) {
    if (ImGui::Button("Apply to shared group")) {
      const auto status =
          assignment_callback_(room.id(), selected_slot_, pending_sheet_);
      assignment_error_ = status.ok() ? "" : std::string(status.message());
      if (status.ok())
        show_picker_ = false;
    }
    gui::AutoRegisterLastItem("button", "Apply to shared group");
    ImGui::SameLine();
    ImGui::Text("Sheet %02X", pending_sheet_);
  }
  if (ImGui::BeginChild("##RoomSheetPicker", ImVec2(0, 220), true)) {
    std::vector<int> candidates;
    for (int id = 1; id < zelda3::GraphicsSheetStore::kSheetCount; ++id) {
      if (data->sheet_store.HasSheet(id) &&
          zelda3::GetGfxSheetStorageKind(id) !=
              zelda3::GfxSheetStorageKind::kCompressed2bpp)
        candidates.push_back(id);
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(candidates.size()), 54.0f);
    while (clipper.Step()) {
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const int id = candidates[row];
        auto& preview = picker_previews_[id];
        const auto pixels = data->sheet_store.Sheet(id);
        const std::vector<uint8_t> current(pixels.begin(), pixels.end());
        if (!preview.is_active() || preview.vector() != current) {
          preview.Create(128, 32, 8, current);
        }
        preview.SetPalette(gfx::SnesPalette(colors));
        if (preview.texture())
          preview.UpdateTexture();
        else
          preview.CreateTexture();
        const auto position = ImGui::GetCursorScreenPos();
        const auto label = absl::StrFormat("Sheet %02X##replacement", id);
        ImGui::PushID(id);
        if (ImGui::Selectable(label.c_str(), pending_sheet_ == id, 0,
                              ImVec2(0, 50)))
          pending_sheet_ = id;
        gui::AutoRegisterLastItem("selectable",
                                  absl::StrFormat("Sheet %02X", id));
        if (preview.texture()) {
          ImGui::GetWindowDrawList()->AddImage(
              (ImTextureID)preview.texture(),
              ImVec2(position.x + 84, position.y + 8),
              ImVec2(position.x + 212, position.y + 40), ImVec2(0, 0),
              ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
        }
        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
}

void RoomGraphicsContent::RefreshSheetPreviews(const zelda3::Room& room) {
  const auto blocks = room.blocks();
  if (blocks.size() < sheet_previews_.size()) {
    preview_cache_valid_ = false;
    return;
  }

  bool needs_refresh = palette_dirty_ || !preview_cache_valid_ ||
                       preview_room_id_ != room.id() ||
                       preview_graphics_revision_ != room.graphics_revision();
  if (!needs_refresh) {
    for (size_t i = 0; i < preview_block_ids_.size(); ++i) {
      if (preview_block_ids_[i] != blocks[i]) {
        needs_refresh = true;
        break;
      }
    }
  }
  if (!needs_refresh) {
    return;
  }

  std::vector<SDL_Color> palette_colors;
  if (SDL_Surface* bg_surface = room.bg1_buffer().bitmap().surface()) {
    if (SDL_Palette* palette = platform::GetSurfacePalette(bg_surface)) {
      palette_colors.assign(palette->colors,
                            palette->colors + palette->ncolors);
    }
  }

  const auto& gfx_buffer = room.get_gfx_buffer();
  constexpr size_t kSheetBytes = 128 * 32;
  for (size_t i = 0; i < sheet_previews_.size(); ++i) {
    preview_block_ids_[i] = blocks[i];
    const size_t offset = i * kSheetBytes;
    auto& meta = sheet_preview_metadata_[i];
    meta = {};
    meta.block_id = blocks[i];
    meta.source_offset = offset;
    if (offset + kSheetBytes > gfx_buffer.size()) {
      gfx::Arena::Get().RetireBitmap(sheet_previews_[i]);
      sheet_previews_[i] = gfx::Bitmap();
      continue;
    }

    std::vector<uint8_t> sheet_pixels(
        gfx_buffer.begin() + offset, gfx_buffer.begin() + offset + kSheetBytes);
    meta.nonzero_pixels = static_cast<int>(
        std::count_if(sheet_pixels.begin(), sheet_pixels.end(),
                      [](uint8_t value) { return value != 0; }));
    sheet_previews_[i].Create(128, 32, 8,
                              static_cast<int>(gfx::BitmapFormat::kIndexed),
                              sheet_pixels);
    if (!palette_colors.empty()) {
      sheet_previews_[i].SetPalette(palette_colors);
    }
    // Bitmap::Create keeps an existing texture and advances its generation,
    // superseding pending commands. Updating preserves handles that may already
    // appear in this frame's draw list instead of destroying them during Draw.
    if (sheet_previews_[i].texture() != nullptr) {
      sheet_previews_[i].UpdateTexture();
    } else {
      sheet_previews_[i].CreateTexture();
    }
    meta.width = sheet_previews_[i].width();
    meta.height = sheet_previews_[i].height();
    meta.bitmap_active = sheet_previews_[i].is_active();
    meta.has_surface = sheet_previews_[i].surface() != nullptr;
    meta.palette_colors = static_cast<int>(palette_colors.size());
    meta.has_texture = sheet_previews_[i].texture() != nullptr;
  }

  preview_room_id_ = room.id();
  preview_graphics_revision_ = room.graphics_revision();
  preview_cache_valid_ = true;
  palette_dirty_ = false;
}

void RoomGraphicsContent::Draw(bool* p_open) {
  gui::AutoWidgetScope scope("Dungeon/RoomGraphics");
  (void)p_open;
  if (runtime_detached_) {
    ImGui::TextDisabled("Room graphics session closed.");
    return;
  }
  // Resolve the active dungeon editor lazily when this panel is hosted through
  // the workspace window system instead of the legacy direct constructor.
  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    const auto ctx = CurrentDungeonWindowContext();
    if (ctx) {
      current_room_id_ = ctx.editor->mutable_current_room_id();
      rooms_ = &ctx.editor->rooms();
      renderer_ = ctx.editor->renderer();
      assignment_callback_ = [editor = ctx.editor](int room, int slot,
                                                   int sheet) {
        return editor->EditRoomGraphicsSheet(room, slot, sheet);
      };
      usage_callback_ = [editor = ctx.editor](int roomset) {
        return editor->GetRoomGraphicsUsage(roomset);
      };
    }
  }

  if (current_room_id_ == nullptr || rooms_ == nullptr) {
    ImGui::TextDisabled(tr("No room data available"));
    return;
  }

  const int active_room_id = *current_room_id_;
  if (active_room_id < 0 ||
      active_room_id >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled(tr("Invalid room ID: %d"), active_room_id);
    return;
  }

  auto& room = (*rooms_)[active_room_id];
  if (selection_room_ != active_room_id ||
      selection_group_ != room.blockset()) {
    selection_room_ = active_room_id;
    selection_group_ = room.blockset();
    show_picker_ = false;
    pending_sheet_ = -1;
    assignment_error_.clear();
    ClearPickerPreviews();
  }
  // Rebuild pixels and palettes together when entrance/header context changes.
  // Loading only the sheet IDs can label old pixels with the new assignment.
  room.PrepareForRender();
  RefreshSheetPreviews(room);

  if (renderer_ != nullptr) {
    gfx::Arena::Get().ProcessTextureQueue(renderer_);
  }
  auto blocks = room.blocks();

  constexpr int kBlockWidth = 128;
  constexpr int kBlockHeight = 32;
  constexpr int kPreferredBlocksPerRow = 2;
  constexpr float kPadding = 4.0f;

  const int block_count = static_cast<int>(blocks.size());
  ImGui::Text(tr("Room %03X Graphics Blocks"), active_room_id);
  ImGui::TextDisabled(tr("Main %02X | Room %02X | Sprites %02X | Palette %02X"),
                      room.resolved_main_blockset(), room.blockset(),
                      room.spriteset(), room.palette());
  auto& asset_manager = zelda3::CustomObjectManager::Get();
  const auto patterns = asset_manager.GetTilePatterns();
  if (!patterns.ok()) {
    ImGui::TextWrapped("%s", patterns.status().ToString().c_str());
  } else if (*patterns) {
    ImGui::TextWrapped("%s", tr("Standard object patterns: source file"));
    ImGui::TextWrapped("%s", (*patterns)->source_path.c_str());
    ImGui::TextDisabled(tr("%zu tile words; ROM bytes unchanged"),
                        (*patterns)->words.size());
  }
  if (!patterns.ok() || *patterns) {
    if (gui::AutoSmallButton(tr("Reload Pattern Source"))) {
      asset_manager.ReloadAll();
    }
  }
  ImGui::Checkbox(tr("Source Trace"), &show_source_trace_);
  ImGui::SameLine();
  DrawRoomGraphicsScaleControl(&sheet_scale_mode_);
  ImGui::Separator();
  const float available_width = ImGui::GetContentRegionAvail().x;

  const gui::AdaptiveSheetGridLayout sheet_layout =
      gui::ResolveAdaptiveSheetGridLayout(
          available_width, kBlockWidth, kBlockHeight, block_count,
          kPreferredBlocksPerRow, sheet_scale_mode_, 0.35f, 4.0f, kPadding,
          kPadding * 2.0f);
  const ImVec2 canvas_size(
      std::max(available_width, sheet_layout.content_width + kPadding * 2.0f),
      sheet_layout.content_height + kPadding * 2.0f +
          ((block_count + sheet_layout.columns - 1) / sheet_layout.columns) *
              ImGui::GetTextLineHeightWithSpacing());

  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = canvas_size;
  frame_opts.draw_grid = false;
  frame_opts.draw_overlay = false;
  frame_opts.draw_context_menu = false;
  frame_opts.render_popups = false;

  // The canvas backdrop must yield hit testing to the slot buttons below.
  ImGui::SetNextItemAllowOverlap();
  auto rt = gui::BeginCanvas(room_gfx_canvas_, frame_opts);

  const float grid_width = sheet_layout.content_width + kPadding * 2.0f;
  const float x_offset = std::max(0.0f, (canvas_size.x - grid_width) * 0.5f);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (int i = 0; i < block_count; ++i) {
    const uint8_t block_id = blocks[static_cast<size_t>(i)];
    const int row = i / sheet_layout.columns;
    const int col = i % sheet_layout.columns;
    const ImVec2 local_pos(
        x_offset + kPadding + col * (sheet_layout.item_width + kPadding),
        kPadding + row * (sheet_layout.item_height + kPadding +
                          ImGui::GetTextLineHeightWithSpacing()));

    if (i < static_cast<int>(sheet_previews_.size()) &&
        sheet_previews_[static_cast<size_t>(i)].texture() != 0) {
      gui::BitmapPreviewOptions preview_opts;
      preview_opts.dest_pos = local_pos;
      preview_opts.dest_size =
          ImVec2(sheet_layout.item_width, sheet_layout.item_height);
      preview_opts.draw_context_menu = false;
      preview_opts.draw_grid = false;
      preview_opts.draw_overlay = false;
      preview_opts.render_popups = false;
      preview_opts.ensure_texture = true;
      gui::DrawBitmapPreview(rt, sheet_previews_[static_cast<size_t>(i)],
                             preview_opts);
    } else {
      const ImVec2 zero = room_gfx_canvas_.zero_point();
      const float scale = room_gfx_canvas_.global_scale();
      const ImVec2 screen_pos(zero.x + local_pos.x, zero.y + local_pos.y);
      const ImVec2 screen_end(screen_pos.x + sheet_layout.item_width * scale,
                              screen_pos.y + sheet_layout.item_height * scale);
      draw_list->AddRect(screen_pos, screen_end,
                         ImGui::GetColorU32(gui::GetOutlineVec4()));
      draw_list->AddText(ImVec2(screen_pos.x + 6.0f, screen_pos.y + 6.0f),
                         ImGui::GetColorU32(gui::GetTextSecondaryVec4()),
                         block_id == 0 ? "Empty" : "Missing");
    }
    const auto zero = room_gfx_canvas_.zero_point();
    const ImVec2 start(zero.x + local_pos.x, zero.y + local_pos.y);
    const ImVec2 end(start.x + sheet_layout.item_width,
                     start.y + sheet_layout.item_height);
    const auto saved_cursor = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(start);
    ImGui::PushID(i);
    if (ImGui::InvisibleButton(
            "##RoomSheetSlot",
            ImVec2(sheet_layout.item_width, sheet_layout.item_height),
            ImGuiButtonFlags_EnableNav)) {
      selected_slot_ = i;
      show_picker_ = false;
      assignment_error_.clear();
    }
    gui::AutoRegisterLastItem("button", absl::StrFormat("Slot %02d", i));
    if (i == selected_slot_)
      draw_list->AddRect(start, end, ImGui::GetColorU32(ImGuiCol_NavHighlight),
                         0, 0, 2);
    const char* role = i < 3    ? "Main"
                       : i < 7  ? "Room"
                       : i == 7 ? "Main/anim"
                       : i < 12 ? "Fixed sprite"
                                : "Spriteset";
    const auto label = absl::StrFormat("%02d %s / %02X", i, role, block_id);
    draw_list->AddText(ImVec2(start.x, end.y),
                       ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    ImGui::PopID();
    ImGui::SetCursorScreenPos(saved_cursor);
  }

  gui::EndCanvas(room_gfx_canvas_, rt, frame_opts);
  DrawAssignmentControls(room);

  if (show_source_trace_ &&
      ImGui::CollapsingHeader(ICON_MD_BUG_REPORT " Source Trace",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled(tr(
        "Source: Room::get_gfx_buffer() -> preview bitmap -> queued texture"));
    ImGui::TextDisabled(tr("Room buffer bytes: %zu"),
                        room.get_gfx_buffer().size());
    ImGui::Separator();
    for (int i = 0;
         i < std::min(block_count,
                      static_cast<int>(sheet_preview_metadata_.size()));
         ++i) {
      const auto& meta = sheet_preview_metadata_[static_cast<size_t>(i)];
      const uintptr_t texture_value = reinterpret_cast<uintptr_t>(
          sheet_previews_[static_cast<size_t>(i)].texture());
      ImGui::PushID(i);
      ImGui::Text(
          tr("Slot %02d | Block %02X | Buf +0x%04zX | %dx%d | nz=%d | pal=%d | "
             "active=%d surf=%d tex=%d (0x%zx)"),
          i, meta.block_id, meta.source_offset, meta.width, meta.height,
          meta.nonzero_pixels, meta.palette_colors, meta.bitmap_active,
          meta.has_surface, meta.has_texture, texture_value);
      ImGui::PopID();
    }
  }
}

}  // namespace editor
}  // namespace yaze
