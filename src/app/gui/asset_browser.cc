#include "asset_browser.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace gui {

using namespace ImGui;

const ImGuiTableSortSpecs* AssetObject::s_current_sort_specs = NULL;

void GfxSheetAssetBrowser::Draw(
    const std::array<gfx::Bitmap, kNumGfxSheets>& bmp_manager) {
  PushItemWidth(GetFontSize() * 10);
  SeparatorText("Contents");
  Checkbox("Show Type Overlay", &ShowTypeOverlay);
  SameLine();
  Checkbox("Allow Sorting", &AllowSorting);
  SameLine();
  Checkbox("Stretch Spacing", &StretchSpacing);
  SameLine();
  Checkbox("Allow dragging unselected item", &AllowDragUnselected);
  SameLine();
  Checkbox("Allow box-selection", &AllowBoxSelect);
  SameLine();
  SliderFloat("Icon Size", &IconSize, 16.0f, 128.0f, "%.0f");
  SameLine();
  SliderInt("Icon Spacing", &IconSpacing, 0, 32);
  SameLine();
  SliderInt("Icon Hit Spacing", &IconHitSpacing, 0, 32);
  PopItemWidth();

  // Filter by types
  static bool filter_type[4] = {true, true, true, true};
  Text("Filter by type:");
  SameLine();
  Checkbox("Unsorted", &filter_type[0]);
  SameLine();
  Checkbox("Dungeon", &filter_type[1]);
  SameLine();
  Checkbox("Overworld", &filter_type[2]);
  SameLine();
  Checkbox("Sprite", &filter_type[3]);

  // Show a table with ONLY one header row to showcase the idea/possibility of
  // using this to provide a sorting UI
  if (AllowSorting) {
    PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGuiTableFlags table_flags_for_sort_specs =
        ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders;
    if (BeginTable("for_sort_specs_only", 2, table_flags_for_sort_specs,
                   ImVec2(0.0f, GetFrameHeight()))) {
      TableSetupColumn("Index");
      TableSetupColumn("Type");
      TableHeadersRow();
      if (ImGuiTableSortSpecs* sort_specs = TableGetSortSpecs())
        if (sort_specs->SpecsDirty || RequestSort) {
          AssetObject::SortWithSortSpecs(sort_specs, Items.Data, Items.Size);
          sort_specs->SpecsDirty = RequestSort = false;
        }
      EndTable();
    }
    PopStyleVar();
  }

  ImGuiIO& io = GetIO();
  SetNextWindowContentSize(ImVec2(
      0.0f, LayoutOuterPadding +
                LayoutLineCount * (LayoutItemSize.x + LayoutItemSpacing)));
  if (BeginChild("Assets", ImVec2(0.0f, -GetTextLineHeightWithSpacing()),
                 ImGuiChildFlags_Border, ImGuiWindowFlags_NoMove)) {
    ImDrawList* draw_list = GetWindowDrawList();

    const float avail_width = GetContentRegionAvail().x;
    UpdateLayoutSizes(avail_width);

    // Calculate and store start position.
    ImVec2 start_pos = GetCursorScreenPos();
    start_pos = ImVec2(start_pos.x + LayoutOuterPadding,
                       start_pos.y + LayoutOuterPadding);
    SetCursorScreenPos(start_pos);

    // Multi-select
    ImGuiMultiSelectFlags ms_flags = ImGuiMultiSelectFlags_ClearOnEscape |
                                     ImGuiMultiSelectFlags_ClearOnClickVoid;

    // - Enable box-select (in 2D mode, so that changing box-select rectangle
    // X1/X2 boundaries will affect clipped items)
    if (AllowBoxSelect) ms_flags |= ImGuiMultiSelectFlags_BoxSelect2d;

    // - This feature allows dragging an unselected item without selecting it
    // (rarely used)
    if (AllowDragUnselected)
      ms_flags |= ImGuiMultiSelectFlags_SelectOnClickRelease;

    // - Enable keyboard wrapping on X axis
    // (FIXME-MULTISELECT: We haven't designed/exposed a general nav wrapping
    // api yet, so this flag is provided as a courtesy to avoid doing:
    //    NavMoveRequestTryWrapping(GetCurrentWindow(),
    //    ImGuiNavMoveFlags_WrapX);
    // When we finish implementing a more general API for this, we will
    // obsolete this flag in favor of the new system)
    ms_flags |= ImGuiMultiSelectFlags_NavWrapX;

    ImGuiMultiSelectIO* ms_io =
        BeginMultiSelect(ms_flags, Selection.Size, Items.Size);

    // Use custom selection adapter: store ID in selection (recommended)
    Selection.UserData = this;
    Selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self_,
                                           int idx) {
      GfxSheetAssetBrowser* self = (GfxSheetAssetBrowser*)self_->UserData;
      return self->Items[idx].ID;
    };
    Selection.ApplyRequests(ms_io);

    const bool want_delete =
        (Shortcut(ImGuiKey_Delete, ImGuiInputFlags_Repeat) &&
         (Selection.Size > 0)) ||
        RequestDelete;
    const int item_curr_idx_to_focus =
        want_delete ? Selection.ApplyDeletionPreLoop(ms_io, Items.Size) : -1;
    RequestDelete = false;

    // Push LayoutSelectableSpacing (which is LayoutItemSpacing minus
    // hit-spacing, if we decide to have hit gaps between items) Altering
    // style ItemSpacing may seem unnecessary as we position every items using
    // SetCursorScreenPos()... But it is necessary for two reasons:
    // - Selectables uses it by default to visually fill the space between two
    // items.
    // - The vertical spacing would be measured by Clipper to calculate line
    // height if we didn't provide it explicitly (here we do).
    PushStyleVar(ImGuiStyleVar_ItemSpacing,
                 ImVec2(LayoutSelectableSpacing, LayoutSelectableSpacing));

    // Rendering parameters
    const ImU32 icon_type_overlay_colors[5] = {
        0, IM_COL32(200, 70, 70, 255), IM_COL32(70, 170, 70, 255),
        IM_COL32(70, 70, 200, 255), IM_COL32(200, 200, 200, 255)};
    const ImU32 icon_bg_color = GetColorU32(ImGuiCol_MenuBarBg);
    const ImVec2 icon_type_overlay_size = ImVec2(5.0f, 5.0f);
    const bool display_label = (LayoutItemSize.x >= CalcTextSize("999").x);

    const int column_count = LayoutColumnCount;
    ImGuiListClipper clipper;
    clipper.Begin(LayoutLineCount, LayoutItemStep.y);

    // Ensure focused item line is not clipped.
    if (item_curr_idx_to_focus != -1)
      clipper.IncludeItemByIndex(item_curr_idx_to_focus / column_count);

    // Ensure RangeSrc item line is not clipped.
    if (ms_io->RangeSrcItem != -1)
      clipper.IncludeItemByIndex((int)ms_io->RangeSrcItem / column_count);

    while (clipper.Step()) {
      for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd;
           line_idx++) {
        const int item_min_idx_for_current_line = line_idx * column_count;
        const int item_max_idx_for_current_line =
            IM_MIN((line_idx + 1) * column_count, Items.Size);
        for (int item_idx = item_min_idx_for_current_line;
             item_idx < item_max_idx_for_current_line; ++item_idx) {
          AssetObject* item_data = &Items[item_idx];
          PushID((int)item_data->ID);

          // Position item
          ImVec2 pos =
              ImVec2(start_pos.x + (item_idx % column_count) * LayoutItemStep.x,
                     start_pos.y + line_idx * LayoutItemStep.y);
          SetCursorScreenPos(pos);

          SetNextItemSelectionUserData(item_idx);
          bool item_is_selected = Selection.Contains((ImGuiID)item_data->ID);
          bool item_is_visible = IsRectVisible(LayoutItemSize);
          Selectable("", item_is_selected, ImGuiSelectableFlags_None,
                     LayoutItemSize);

          // Update our selection state immediately (without waiting for
          // EndMultiSelect() requests) because we use this to alter the color
          // of our text/icon.
          if (IsItemToggledSelection()) item_is_selected = !item_is_selected;

          // Focus (for after deletion)
          if (item_curr_idx_to_focus == item_idx) SetKeyboardFocusHere(-1);

          // Drag and drop
          if (BeginDragDropSource()) {
            // Create payload with full selection OR single unselected item.
            // (the later is only possible when using
            // ImGuiMultiSelectFlags_SelectOnClickRelease)
            if (GetDragDropPayload() == NULL) {
              ImVector<ImGuiID> payload_items;
              void* it = NULL;
              ImGuiID id = 0;
              if (!item_is_selected)
                payload_items.push_back(item_data->ID);
              else
                while (Selection.GetNextSelectedItem(&it, &id))
                  payload_items.push_back(id);
              SetDragDropPayload("ASSETS_BROWSER_ITEMS", payload_items.Data,
                                 (size_t)payload_items.size_in_bytes());
            }

            // Display payload content in tooltip, by extracting it from the
            // payload data (we could read from selection, but it is more
            // correct and reusable to read from payload)
            const ImGuiPayload* payload = GetDragDropPayload();
            const int payload_count =
                (int)payload->DataSize / (int)sizeof(ImGuiID);
            Text("%d assets", payload_count);

            EndDragDropSource();
          }

          // Render icon (a real app would likely display an image/thumbnail
          // here) Because we use ImGuiMultiSelectFlags_BoxSelect2d, clipping
          // vertical may occasionally be larger, so we coarse-clip our
          // rendering as well.
          if (item_is_visible) {
            ImVec2 box_min(pos.x - 1, pos.y - 1);
            ImVec2 box_max(box_min.x + LayoutItemSize.x + 2,
                           box_min.y + LayoutItemSize.y + 2);  // Dubious
            draw_list->AddRectFilled(box_min, box_max,
                                     icon_bg_color);  // Background color

            if (display_label) {
              ImU32 label_col = GetColorU32(
                  item_is_selected ? ImGuiCol_Text : ImGuiCol_TextDisabled);
              draw_list->AddImage((void*)bmp_manager[item_data->ID].texture(),
                                  box_min, box_max, ImVec2(0, 0), ImVec2(1, 1),
                                  GetColorU32(ImVec4(1, 1, 1, 1)));
              draw_list->AddText(ImVec2(box_min.x, box_max.y - GetFontSize()),
                                 label_col,
                                 absl::StrFormat("%X", item_data->ID).c_str());
            }
            if (ShowTypeOverlay && item_data->Type != 0) {
              ImU32 type_col = icon_type_overlay_colors
                  [item_data->Type % IM_ARRAYSIZE(icon_type_overlay_colors)];
              draw_list->AddRectFilled(
                  ImVec2(box_max.x - 2 - icon_type_overlay_size.x,
                         box_min.y + 2),
                  ImVec2(box_max.x - 2,
                         box_min.y + 2 + icon_type_overlay_size.y),
                  type_col);
            }
          }

          PopID();
        }
      }
    }
    clipper.End();
    PopStyleVar();  // ImGuiStyleVar_ItemSpacing

    // Context menu
    if (BeginPopupContextWindow()) {
      Text("Selection: %d items", Selection.Size);
      Separator();
      if (BeginMenu("Set Type")) {
        if (MenuItem("Unsorted")) {
          void* it = NULL;
          ImGuiID id = 0;
          while (Selection.GetNextSelectedItem(&it, &id)) Items[id].Type = 0;
        }
        if (MenuItem("Dungeon")) {
          void* it = NULL;
          ImGuiID id = 0;
          while (Selection.GetNextSelectedItem(&it, &id)) Items[id].Type = 1;
        }
        if (MenuItem("Overworld")) {
          void* it = NULL;
          ImGuiID id = 0;
          while (Selection.GetNextSelectedItem(&it, &id)) Items[id].Type = 2;
        }
        if (MenuItem("Sprite")) {
          void* it = NULL;
          ImGuiID id = 0;
          while (Selection.GetNextSelectedItem(&it, &id)) Items[id].Type = 3;
        }
        EndMenu();
      }
      Separator();
      if (MenuItem("Delete", "Del", false, Selection.Size > 0))
        RequestDelete = true;
      EndPopup();
    }

    ms_io = EndMultiSelect();
    Selection.ApplyRequests(ms_io);
    if (want_delete)
      Selection.ApplyDeletionPostLoop(ms_io, Items, item_curr_idx_to_focus);

    // Zooming with CTRL+Wheel
    if (IsWindowAppearing()) ZoomWheelAccum = 0.0f;
    if (IsWindowHovered() && io.MouseWheel != 0.0f &&
        IsKeyDown(ImGuiMod_Ctrl) && IsAnyItemActive() == false) {
      ZoomWheelAccum += io.MouseWheel;
      if (fabsf(ZoomWheelAccum) >= 1.0f) {
        // Calculate hovered item index from mouse location
        // FIXME: Locking aiming on 'hovered_item_idx' (with a cool-down
        // timer) would ensure zoom keeps on it.
        const float hovered_item_nx =
            (io.MousePos.x - start_pos.x + LayoutItemSpacing * 0.5f) /
            LayoutItemStep.x;
        const float hovered_item_ny =
            (io.MousePos.y - start_pos.y + LayoutItemSpacing * 0.5f) /
            LayoutItemStep.y;
        const int hovered_item_idx =
            ((int)hovered_item_ny * LayoutColumnCount) + (int)hovered_item_nx;
        // SetTooltip("%f,%f -> item %d", hovered_item_nx,
        // hovered_item_ny, hovered_item_idx); // Move those 4 lines in block
        // above for easy debugging

        // Zoom
        IconSize *= powf(1.1f, (float)(int)ZoomWheelAccum);
        IconSize = IM_CLAMP(IconSize, 16.0f, 128.0f);
        ZoomWheelAccum -= (int)ZoomWheelAccum;
        UpdateLayoutSizes(avail_width);

        // Manipulate scroll to that we will land at the same Y location of
        // currently hovered item.
        // - Calculate next frame position of item under mouse
        // - Set new scroll position to be used in next BeginChild()
        // call.
        float hovered_item_rel_pos_y =
            ((float)(hovered_item_idx / LayoutColumnCount) +
             fmodf(hovered_item_ny, 1.0f)) *
            LayoutItemStep.y;
        hovered_item_rel_pos_y += GetStyle().WindowPadding.y;
        float mouse_local_y = io.MousePos.y - GetWindowPos().y;
        SetScrollY(hovered_item_rel_pos_y - mouse_local_y);
      }
    }
  }
  EndChild();

  Text("Selected: %d/%d items", Selection.Size, Items.Size);
}

}  // namespace gui
}  // namespace app
}  // namespace yaze
