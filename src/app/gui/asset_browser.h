#ifndef YAZE_APP_GUI_ASSET_BROWSER_H
#define YAZE_APP_GUI_ASSET_BROWSER_H

#include "imgui/imgui.h"

#include <string>

#include "app/gfx/bitmap.h"

#define IM_MIN(A, B) (((A) < (B)) ? (A) : (B))
#define IM_MAX(A, B) (((A) >= (B)) ? (A) : (B))
#define IM_CLAMP(V, MN, MX) ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))

namespace yaze {
namespace app {
namespace gui {

// Extra functions to add deletion support to ImGuiSelectionBasicStorage
struct ExampleSelectionWithDeletion : ImGuiSelectionBasicStorage {
  // Find which item should be Focused after deletion.
  // Call _before_ item submission. Retunr an index in the before-deletion item
  // list, your item loop should call SetKeyboardFocusHere() on it. The
  // subsequent ApplyDeletionPostLoop() code will use it to apply Selection.
  // - We cannot provide this logic in core Dear ImGui because we don't have
  // access to selection data.
  // - We don't actually manipulate the ImVector<> here, only in
  // ApplyDeletionPostLoop(), but using similar API for consistency and
  // flexibility.
  // - Important: Deletion only works if the underlying ImGuiID for your items
  // are stable: aka not depend on their index, but on e.g. item id/ptr.
  // FIXME-MULTISELECT: Doesn't take account of the possibility focus target
  // will be moved during deletion. Need refocus or scroll offset.
  int ApplyDeletionPreLoop(ImGuiMultiSelectIO* ms_io, int items_count) {
    if (Size == 0) return -1;

    // If focused item is not selected...
    const int focused_idx =
        (int)ms_io->NavIdItem;  // Index of currently focused item
    if (ms_io->NavIdSelected ==
        false)  // This is merely a shortcut, ==
                // Contains(adapter->IndexToStorage(items, focused_idx))
    {
      ms_io->RangeSrcReset =
          true;  // Request to recover RangeSrc from NavId next frame. Would be
                 // ok to reset even when NavIdSelected==true, but it would take
                 // an extra frame to recover RangeSrc when deleting a selected
                 // item.
      return focused_idx;  // Request to focus same item after deletion.
    }

    // If focused item is selected: land on first unselected item after focused
    // item.
    for (int idx = focused_idx + 1; idx < items_count; idx++)
      if (!Contains(GetStorageIdFromIndex(idx))) return idx;

    // If focused item is selected: otherwise return last unselected item before
    // focused item.
    for (int idx = IM_MIN(focused_idx, items_count) - 1; idx >= 0; idx--)
      if (!Contains(GetStorageIdFromIndex(idx))) return idx;

    return -1;
  }

  // Rewrite item list (delete items) + update selection.
  // - Call after EndMultiSelect()
  // - We cannot provide this logic in core Dear ImGui because we don't have
  // access to your items, nor to selection data.
  template <typename ITEM_TYPE>
  void ApplyDeletionPostLoop(ImGuiMultiSelectIO* ms_io,
                             ImVector<ITEM_TYPE>& items,
                             int item_curr_idx_to_select) {
    // Rewrite item list (delete items) + convert old selection index (before
    // deletion) to new selection index (after selection). If NavId was not part
    // of selection, we will stay on same item.
    ImVector<ITEM_TYPE> new_items;
    new_items.reserve(items.Size - Size);
    int item_next_idx_to_select = -1;
    for (int idx = 0; idx < items.Size; idx++) {
      if (!Contains(GetStorageIdFromIndex(idx)))
        new_items.push_back(items[idx]);
      if (item_curr_idx_to_select == idx)
        item_next_idx_to_select = new_items.Size - 1;
    }
    items.swap(new_items);

    // Update selection
    Clear();
    if (item_next_idx_to_select != -1 && ms_io->NavIdSelected)
      SetItemSelected(GetStorageIdFromIndex(item_next_idx_to_select), true);
  }
};

struct AssetObject {
  ImGuiID ID;
  int Type;

  AssetObject(ImGuiID id, int type) {
    ID = id;
    Type = type;
  }

  static const ImGuiTableSortSpecs* s_current_sort_specs;

  static void SortWithSortSpecs(ImGuiTableSortSpecs* sort_specs,
                                AssetObject* items, int items_count) {
    // Store in variable accessible by the sort function.
    s_current_sort_specs = sort_specs;
    if (items_count > 1)
      qsort(items, (size_t)items_count, sizeof(items[0]),
            AssetObject::CompareWithSortSpecs);
    s_current_sort_specs = NULL;
  }

  // Compare function to be used by qsort()
  static int __cdecl CompareWithSortSpecs(const void* lhs, const void* rhs) {
    const AssetObject* a = (const AssetObject*)lhs;
    const AssetObject* b = (const AssetObject*)rhs;
    for (int n = 0; n < s_current_sort_specs->SpecsCount; n++) {
      const ImGuiTableColumnSortSpecs* sort_spec =
          &s_current_sort_specs->Specs[n];
      int delta = 0;
      if (sort_spec->ColumnIndex == 0)
        delta = ((int)a->ID - (int)b->ID);
      else if (sort_spec->ColumnIndex == 1)
        delta = (a->Type - b->Type);
      if (delta > 0)
        return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1
                                                                          : -1;
      if (delta < 0)
        return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1
                                                                          : +1;
    }
    return ((int)a->ID - (int)b->ID);
  }
};

struct UnsortedAsset : public AssetObject {
  UnsortedAsset(ImGuiID id) : AssetObject(id, 0) {}
};

struct DungeonAsset : public AssetObject {
  DungeonAsset(ImGuiID id) : AssetObject(id, 1) {}
};

struct OverworldAsset : public AssetObject {
  OverworldAsset(ImGuiID id) : AssetObject(id, 2) {}
};

struct SpriteAsset : public AssetObject {
  SpriteAsset(ImGuiID id) : AssetObject(id, 3) {}
};

struct GfxSheetAssetBrowser {
  // Options
  bool ShowTypeOverlay = true;
  bool AllowSorting = true;
  bool AllowDragUnselected = false;
  bool AllowBoxSelect = true;
  float IconSize = 32.0f;
  int IconSpacing = 10;
  // Increase hit-spacing if you want to make it possible to clear or
  // box-select from gaps. Some spacing is required to able to amend
  // with Shift+box-select. Value is small in Explorer.
  int IconHitSpacing = 4;
  bool StretchSpacing = true;

  // State
  ImVector<AssetObject> Items;

  // (ImGuiSelectionBasicStorage + helper funcs to handle deletion)
  ExampleSelectionWithDeletion Selection;

  ImGuiID NextItemId = 0;      // Unique identifier when creating new items
  bool RequestDelete = false;  // Deferred deletion request
  bool RequestSort = false;    // Deferred sort request
  // Mouse wheel accumulator to handle smooth wheels better
  float ZoomWheelAccum = 0.0f;

  // Calculated sizes for layout, output of UpdateLayoutSizes(). Could be locals
  // but our code is simpler this way.
  ImVec2 LayoutItemSize;
  ImVec2 LayoutItemStep;  // == LayoutItemSize + LayoutItemSpacing
  float LayoutItemSpacing = 0.0f;
  float LayoutSelectableSpacing = 0.0f;
  float LayoutOuterPadding = 0.0f;
  int LayoutColumnCount = 0;
  int LayoutLineCount = 0;
  bool Initialized = false;

  void Initialize(gfx::BitmapManager* bmp_manager) {
    // Load the assets
    for (int i = 0; i < bmp_manager->size(); i++) {
      Items.push_back(UnsortedAsset(i));
    }
    Initialized = true;
  }

  void AddItems(int count) {
    if (Items.Size == 0) NextItemId = 0;
    Items.reserve(Items.Size + count);
    for (int n = 0; n < count; n++, NextItemId++)
      Items.push_back(AssetObject(NextItemId, (NextItemId % 20) < 15   ? 0
                                              : (NextItemId % 20) < 18 ? 1
                                                                       : 2));
    RequestSort = true;
  }
  void ClearItems() {
    Items.clear();
    Selection.Clear();
  }

  // Logic would be written in the main code BeginChild() and outputing to local
  // variables. We extracted it into a function so we can call it easily from
  // multiple places.
  void UpdateLayoutSizes(float avail_width) {
    // Layout: when not stretching: allow extending into right-most spacing.
    LayoutItemSpacing = (float)IconSpacing;
    if (StretchSpacing == false)
      avail_width += floorf(LayoutItemSpacing * 0.5f);

    // Layout: calculate number of icon per line and number of lines
    LayoutItemSize = ImVec2(floorf(IconSize * 4), floorf(IconSize));
    LayoutColumnCount =
        IM_MAX((int)(avail_width / (LayoutItemSize.x + LayoutItemSpacing)), 1);
    LayoutLineCount = (Items.Size + LayoutColumnCount - 1) / LayoutColumnCount;

    // Layout: when stretching: allocate remaining space to more spacing. Round
    // before division, so item_spacing may be non-integer.
    if (StretchSpacing && LayoutColumnCount > 1)
      LayoutItemSpacing =
          floorf(avail_width - LayoutItemSize.x * LayoutColumnCount) /
          LayoutColumnCount;

    LayoutItemStep = ImVec2(LayoutItemSize.x + LayoutItemSpacing,
                            LayoutItemSize.y + LayoutItemSpacing);
    LayoutSelectableSpacing =
        IM_MAX(floorf(LayoutItemSpacing) - IconHitSpacing, 0.0f);
    LayoutOuterPadding = floorf(LayoutItemSpacing * 0.5f);
  }

  void Draw(gfx::BitmapManager* bmp_manager);
};

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GUI_ASSET_BROWSER_H