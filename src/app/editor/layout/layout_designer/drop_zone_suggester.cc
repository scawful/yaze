#include "app/editor/layout/layout_designer/drop_zone_suggester.h"

#include <algorithm>
#include <utility>

namespace yaze {
namespace editor {
namespace layout_designer {

DropSuggestion SuggestDrop(const ImRect& leaf_rect, ImVec2 mouse) {
  DropSuggestion s;
  if (!leaf_rect.Contains(mouse))
    return s;

  const float width = leaf_rect.GetWidth();
  const float height = leaf_rect.GetHeight();
  if (width <= 0.0f || height <= 0.0f)
    return s;

  const float u = (mouse.x - leaf_rect.Min.x) / width;
  const float v = (mouse.y - leaf_rect.Min.y) / height;
  // Distance (in normalized units) to the nearest horizontal / vertical
  // edge. Smaller == closer.
  const float du = std::min(u, 1.0f - u);
  const float dv = std::min(v, 1.0f - v);

  if (du >= kDropEdgeFraction && dv >= kDropEdgeFraction) {
    s.kind = DropSuggestion::Kind::kTab;
    return s;
  }

  // Whichever axis is closer to its edge wins — ties break to horizontal
  // which matches most left-right dominant editor layouts.
  if (du <= dv) {
    s.kind = u < 0.5f ? DropSuggestion::Kind::kSplitLeft
                      : DropSuggestion::Kind::kSplitRight;
  } else {
    s.kind = v < 0.5f ? DropSuggestion::Kind::kSplitTop
                      : DropSuggestion::Kind::kSplitBottom;
  }
  return s;
}

ImRect ComputeDropPreviewRect(const ImRect& leaf_rect,
                              const DropSuggestion& suggestion) {
  switch (suggestion.kind) {
    case DropSuggestion::Kind::kNone:
      return ImRect(leaf_rect.Min, leaf_rect.Min);
    case DropSuggestion::Kind::kTab:
      return leaf_rect;
    case DropSuggestion::Kind::kSplitLeft: {
      const float w = leaf_rect.GetWidth() * kDropSplitRatio;
      return ImRect(leaf_rect.Min,
                    ImVec2(leaf_rect.Min.x + w, leaf_rect.Max.y));
    }
    case DropSuggestion::Kind::kSplitRight: {
      const float w = leaf_rect.GetWidth() * kDropSplitRatio;
      return ImRect(ImVec2(leaf_rect.Max.x - w, leaf_rect.Min.y),
                    leaf_rect.Max);
    }
    case DropSuggestion::Kind::kSplitTop: {
      const float h = leaf_rect.GetHeight() * kDropSplitRatio;
      return ImRect(leaf_rect.Min,
                    ImVec2(leaf_rect.Max.x, leaf_rect.Min.y + h));
    }
    case DropSuggestion::Kind::kSplitBottom: {
      const float h = leaf_rect.GetHeight() * kDropSplitRatio;
      return ImRect(ImVec2(leaf_rect.Min.x, leaf_rect.Max.y - h),
                    leaf_rect.Max);
    }
  }
  return ImRect(leaf_rect.Min, leaf_rect.Min);
}

bool ApplyDropSuggestion(DockTree* tree, DockNode* leaf,
                         const DropSuggestion& suggestion, PanelEntry panel) {
  if (tree == nullptr || leaf == nullptr)
    return false;
  if (suggestion.kind == DropSuggestion::Kind::kNone)
    return false;
  if (leaf->type != DockNode::Type::kLeaf)
    return false;
  if (panel.panel_id.empty())
    return false;
  if (tree->root != nullptr &&
      tree->root->FindPanel(panel.panel_id) != nullptr) {
    return false;
  }

  switch (suggestion.kind) {
    case DropSuggestion::Kind::kNone:
      return false;
    case DropSuggestion::Kind::kTab:
      leaf->panels.push_back(std::move(panel));
      leaf->active_tab_index = static_cast<int>(leaf->panels.size()) - 1;
      return true;
    case DropSuggestion::Kind::kSplitLeft:
      leaf->SplitInPlace(SplitDirection::kLeft, kDropSplitRatio,
                         DockNode::MakeLeaf({std::move(panel)}),
                         /*new_child_first=*/true);
      return true;
    case DropSuggestion::Kind::kSplitRight:
      leaf->SplitInPlace(SplitDirection::kRight, 1.0f - kDropSplitRatio,
                         DockNode::MakeLeaf({std::move(panel)}),
                         /*new_child_first=*/false);
      return true;
    case DropSuggestion::Kind::kSplitTop:
      leaf->SplitInPlace(SplitDirection::kUp, kDropSplitRatio,
                         DockNode::MakeLeaf({std::move(panel)}),
                         /*new_child_first=*/true);
      return true;
    case DropSuggestion::Kind::kSplitBottom:
      leaf->SplitInPlace(SplitDirection::kDown, 1.0f - kDropSplitRatio,
                         DockNode::MakeLeaf({std::move(panel)}),
                         /*new_child_first=*/false);
      return true;
  }
  return false;
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
