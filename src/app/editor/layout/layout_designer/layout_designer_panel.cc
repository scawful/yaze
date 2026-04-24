#include "app/editor/layout/layout_designer/layout_designer_panel.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "app/editor/layout/layout_designer/dock_tree_hit_test.h"
#include "app/editor/layout/layout_designer/dock_tree_json.h"
#include "app/editor/layout/layout_designer/dock_tree_renderer.h"
#include "app/editor/layout/layout_designer/drop_zone_suggester.h"
#include "app/editor/layout/layout_designer/panel_palette.h"
#include "app/editor/layout/layout_designer/split_boundary_drag.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/registry/panel_registration.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/drag_drop.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace editor {
namespace layout_designer {
namespace {

constexpr float kPaletteInitialWidth = 240.0f;
constexpr float kPropertiesInitialWidth = 280.0f;
constexpr const char* kOpenPopupId = "LayoutDesigner_OpenPopup";
constexpr const char* kSaveAsPopupId = "LayoutDesigner_SaveAsPopup";
// Default ImGui main dockspace id used throughout the editor (see
// editor_activator.cc / layout_coordinator.cc). Panels that drive the
// live dockspace all hash the same string.
ImGuiID MainDockspaceId() {
  return ImGui::GetID("MainDockSpace");
}

bool IsSplitHorizontal(SplitDirection d) {
  return d == SplitDirection::kLeft || d == SplitDirection::kRight;
}

const char* SplitDirectionLabel(SplitDirection d) {
  switch (d) {
    case SplitDirection::kLeft:
      return "Left";
    case SplitDirection::kRight:
      return "Right";
    case SplitDirection::kUp:
      return "Up";
    case SplitDirection::kDown:
      return "Down";
  }
  return "Left";
}

}  // namespace

LayoutDesignerPanel::LayoutDesignerPanel() : tree_(MakeEmptyTree("Untitled")) {}

void LayoutDesignerPanel::PushUndoSnapshot() {
  undo_.Push(tree_);
}

void LayoutDesignerPanel::ReplaceTree(DockTree new_tree) {
  tree_ = std::move(new_tree);
  selected_ = nullptr;
  drag_ = ActiveDrag{};
  undo_.Clear();
}

void LayoutDesignerPanel::SaveCurrentTreeToNamedLayouts() {
  UserSettings* settings = ContentRegistry::Context::user_settings();
  if (settings == nullptr) {
    status_message_ = "Save failed: UserSettings not available.";
    status_is_error_ = true;
    return;
  }
  if (tree_.name.empty()) {
    status_message_ = "Save failed: tree has no name (use Save As…).";
    status_is_error_ = true;
    return;
  }
  // DockTreeToJson is infallible — any tree serializes. Validation is a
  // separate concern; a partially-built tree is still persistable.
  const nlohmann::json body = DockTreeToJson(tree_);
  settings->prefs().named_layouts[tree_.name] = body.dump();
  const absl::Status save_status = settings->Save();
  if (save_status.ok()) {
    status_message_ = absl::StrCat("Saved \"", tree_.name, "\".");
    status_is_error_ = false;
  } else {
    status_message_ = absl::StrCat("Saved to memory, but disk write failed: ",
                                   save_status.message());
    status_is_error_ = true;
  }
}

void LayoutDesignerPanel::LoadNamedLayoutIntoTree(const std::string& name) {
  UserSettings* settings = ContentRegistry::Context::user_settings();
  if (settings == nullptr) {
    status_message_ = "Open failed: UserSettings not available.";
    status_is_error_ = true;
    return;
  }
  const auto it = settings->prefs().named_layouts.find(name);
  if (it == settings->prefs().named_layouts.end()) {
    status_message_ =
        absl::StrCat("Open failed: no layout named \"", name, "\".");
    status_is_error_ = true;
    return;
  }
  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(it->second);
  } catch (const nlohmann::json::parse_error& e) {
    status_message_ =
        absl::StrCat("Open failed: invalid JSON (", e.what(), ").");
    status_is_error_ = true;
    return;
  }
  auto tree_or = DockTreeFromJson(parsed);
  if (!tree_or.ok()) {
    status_message_ = absl::StrCat("Open failed: ", tree_or.status().message());
    status_is_error_ = true;
    return;
  }
  // DockTreeFromJson preserves the name from the JSON body; fall back to
  // the lookup key if the body lacks one (legacy migrations).
  if (tree_or->name.empty()) {
    tree_or->name = name;
  }
  ReplaceTree(*std::move(tree_or));
  status_message_ = absl::StrCat("Loaded \"", name, "\".");
  status_is_error_ = false;
}

void LayoutDesignerPanel::ApplyCurrentTreeToLiveDockspace() {
  LayoutManager* manager = ContentRegistry::Context::layout_manager();
  UserSettings* settings = ContentRegistry::Context::user_settings();
  if (manager == nullptr) {
    status_message_ = "Apply failed: LayoutManager not available.";
    status_is_error_ = true;
    return;
  }
  const absl::Status apply_status =
      manager->ApplyDockTree(tree_, MainDockspaceId());
  if (!apply_status.ok()) {
    status_message_ = absl::StrCat("Apply failed: ", apply_status.message());
    status_is_error_ = true;
    return;
  }
  // Persist the last-applied name so EditorManager can reapply on the
  // next startup (hook lands in a follow-up commit; the field itself
  // already exists in UserSettings).
  if (settings != nullptr && !tree_.name.empty()) {
    settings->prefs().last_applied_layout_name = tree_.name;
    (void)settings->Save();  // best-effort; apply succeeded regardless.
  }
  status_message_ = absl::StrCat(
      "Applied \"", tree_.name.empty() ? "(unnamed)" : tree_.name.c_str(),
      "\" to main dockspace.");
  status_is_error_ = false;
}

void LayoutDesignerPanel::DrawFileRow() {
  UserSettings* settings = ContentRegistry::Context::user_settings();
  LayoutManager* manager = ContentRegistry::Context::layout_manager();
  const bool has_named_layouts =
      settings != nullptr && !settings->prefs().named_layouts.empty();
  const bool can_save = settings != nullptr && !tree_.name.empty();
  const bool can_apply = manager != nullptr;

  ImGui::TextUnformatted("File:");
  ImGui::SameLine();
  if (ImGui::SmallButton("New")) {
    ReplaceTree(MakeEmptyTree("Untitled"));
    status_message_ = "New layout.";
    status_is_error_ = false;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!has_named_layouts);
  if (ImGui::SmallButton("Open...")) {
    open_popup_requested_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_save);
  if (ImGui::SmallButton("Save")) {
    SaveCurrentTreeToNamedLayouts();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(settings == nullptr);
  if (ImGui::SmallButton("Save As...")) {
    save_as_buffer_ = tree_.name.empty() ? "Untitled" : tree_.name;
    save_as_popup_requested_ = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_apply);
  if (ImGui::SmallButton("Apply")) {
    ApplyCurrentTreeToLiveDockspace();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::BeginDisabled(!undo_.CanUndo());
  if (ImGui::SmallButton("Undo")) {
    if (undo_.Undo(&tree_))
      selected_ = nullptr;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!undo_.CanRedo());
  if (ImGui::SmallButton("Redo")) {
    if (undo_.Redo(&tree_))
      selected_ = nullptr;
  }
  ImGui::EndDisabled();
}

void LayoutDesignerPanel::DrawOpenPopup() {
  if (!ImGui::BeginPopupModal(kOpenPopupId, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  UserSettings* settings = ContentRegistry::Context::user_settings();
  if (settings == nullptr) {
    ImGui::TextUnformatted("UserSettings unavailable.");
    if (ImGui::Button("Close"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  // Gather + sort so the list is stable across frames (named_layouts is
  // an unordered_map).
  std::vector<std::string> names;
  names.reserve(settings->prefs().named_layouts.size());
  for (const auto& entry : settings->prefs().named_layouts) {
    names.push_back(entry.first);
  }
  std::sort(names.begin(), names.end());

  ImGui::TextUnformatted("Choose a saved layout:");
  ImGui::BeginChild("layout_designer_open_list", ImVec2(320.0f, 200.0f),
                    ImGuiChildFlags_Borders);
  for (const auto& name : names) {
    const bool is_selected = (name == open_selection_);
    if (ImGui::Selectable(name.c_str(), is_selected,
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      open_selection_ = name;
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        LoadNamedLayoutIntoTree(open_selection_);
        ImGui::CloseCurrentPopup();
      }
    }
  }
  ImGui::EndChild();

  const bool can_open = !open_selection_.empty() &&
                        settings->prefs().named_layouts.count(open_selection_);
  ImGui::BeginDisabled(!can_open);
  if (ImGui::Button("Open")) {
    LoadNamedLayoutIntoTree(open_selection_);
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Cancel"))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void LayoutDesignerPanel::DrawSaveAsPopup() {
  if (!ImGui::BeginPopupModal(kSaveAsPopupId, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  ImGui::TextUnformatted("Save layout as:");
  ImGui::SetNextItemWidth(320.0f);
  const bool submitted_via_enter =
      ImGui::InputText("##layout_designer_save_as", &save_as_buffer_,
                       ImGuiInputTextFlags_EnterReturnsTrue);
  const bool name_empty =
      save_as_buffer_.find_first_not_of(" \t") == std::string::npos;
  ImGui::BeginDisabled(name_empty);
  const bool save_clicked = ImGui::Button("Save");
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
  }
  if ((save_clicked || submitted_via_enter) && !name_empty) {
    tree_.name = save_as_buffer_;
    SaveCurrentTreeToNamedLayouts();
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void LayoutDesignerPanel::Draw(bool* p_open) {
  (void)p_open;  // Closing the window is handled by the workspace shell.

  // Keyboard shortcuts: Ctrl/Cmd+Z (undo), Ctrl/Cmd+Shift+Z (redo),
  // Ctrl/Cmd+S (save — or Save As if the tree has no name yet).
  // Gated on focus so typing in palette/property text fields doesn't
  // trip them.
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    const ImGuiIO& io = ImGui::GetIO();
    const bool chord = io.KeyCtrl || io.KeySuper;
    if (chord && ImGui::IsKeyPressed(ImGuiKey_Z, /*repeat=*/false)) {
      if (io.KeyShift) {
        if (undo_.Redo(&tree_))
          selected_ = nullptr;
      } else {
        if (undo_.Undo(&tree_))
          selected_ = nullptr;
      }
    }
    if (chord && ImGui::IsKeyPressed(ImGuiKey_S, /*repeat=*/false)) {
      if (tree_.name.empty() || tree_.name == "Untitled") {
        save_as_buffer_ = tree_.name.empty() ? "Untitled" : tree_.name;
        save_as_popup_requested_ = true;
      } else {
        SaveCurrentTreeToNamedLayouts();
      }
    }
  }

  // WindowContent::Draw contract: no ImGui::Begin/End and no BeginMenuBar
  // (the outer PanelWindow does not opt into ImGuiWindowFlags_MenuBar). A
  // button row stands in for the File menu.
  DrawFileRow();
  ImGui::Separator();

  constexpr ImGuiTableFlags kBodyFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;

  if (ImGui::BeginTable("layout_designer_body", 3, kBodyFlags)) {
    ImGui::TableSetupColumn("Palette", ImGuiTableColumnFlags_WidthFixed,
                            kPaletteInitialWidth);
    ImGui::TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthFixed,
                            kPropertiesInitialWidth);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    if (ImGui::BeginChild("layout_designer_palette", ImVec2(0, 0),
                          ImGuiChildFlags_None)) {
      // Exclude self so the designer can't be dropped inside its own
      // canvas.
      const auto entries = CollectPaletteEntries(GetId());
      DrawPanelPalette(entries, &palette_query_);
    }
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    if (ImGui::BeginChild("layout_designer_canvas", ImVec2(0, 0),
                          ImGuiChildFlags_None)) {
      const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
      const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
      if (canvas_size.x >= kMinCellSize && canvas_size.y >= kMinCellSize) {
        const ImRect viewport(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x,
                                                 canvas_pos.y + canvas_size.y));
        const DockTreeLayout layout = ComputeLayout(tree_, viewport);
        RenderDockTree(tree_, layout, selected_, ImGui::GetWindowDrawList());
        ImGui::Dummy(canvas_size);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool hovered = ImGui::IsItemHovered();

        if (ImGui::BeginDragDropTarget()) {
          const DockNode* leaf_const = HitTestNode(layout, mouse);
          if (leaf_const != nullptr &&
              leaf_const->type == DockNode::Type::kLeaf) {
            const ImRect& leaf_rect = layout.node_rects.at(leaf_const);
            const DropSuggestion suggestion = SuggestDrop(leaf_rect, mouse);
            const ImRect preview =
                ComputeDropPreviewRect(leaf_rect, suggestion);
            if (preview.GetWidth() > 0.0f && preview.GetHeight() > 0.0f) {
              ImGui::GetWindowDrawList()->AddRectFilled(
                  preview.Min, preview.Max,
                  ImGui::ColorConvertFloat4ToU32(
                      ImVec4(0.25f, 0.55f, 0.95f, 0.25f)));
            }
            gui::PanelDragPayload payload;
            if (gui::AcceptPanelDropWithinTarget(&payload)) {
              PanelEntry new_panel;
              new_panel.panel_id = payload.panel_id;
              if (const WindowContent* registered =
                      ContentRegistry::Panels::Get(new_panel.panel_id)) {
                new_panel.display_name = registered->GetDisplayName();
                new_panel.icon = registered->GetIcon();
              }
              DockNode* leaf_mut = const_cast<DockNode*>(leaf_const);
              // Snapshot before mutation so this drop is reversible.
              PushUndoSnapshot();
              if (DockNode* selected_leaf = ApplyDropSuggestion(
                      &tree_, leaf_mut, suggestion, std::move(new_panel))) {
                selected_ = selected_leaf;
              } else {
                // Applier refused (e.g. duplicate id). Discard the
                // speculative push *without* polluting redo — `Undo()`
                // would move the unchanged tree onto the redo stack and
                // invalidate `selected_` (raw pointer into what becomes
                // a stale Clone).
                undo_.PopLastPush();
              }
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (drag_.split_node != nullptr) {
          // Mid-drag: keep consuming mouse motion until release. Do not
          // gate on hovered — the mouse can leave the canvas bounds mid-
          // drag and we still want to track it until mouse-up.
          if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const bool horizontal =
                IsSplitHorizontal(drag_.split_node->split_direction);
            const float axis_delta = horizontal
                                         ? (mouse.x - drag_.start_mouse.x)
                                         : (mouse.y - drag_.start_mouse.y);
            const float axis_size = horizontal ? drag_.start_rect.GetWidth()
                                               : drag_.start_rect.GetHeight();
            drag_.split_node->split_ratio = ComputeDraggedSplitRatio(
                drag_.start_ratio, axis_delta, axis_size);
            ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeEW
                                             : ImGuiMouseCursor_ResizeNS);
          } else {
            drag_ = ActiveDrag{};
          }
        } else if (hovered) {
          const SplitBoundaryHit boundary =
              HitTestSplitBoundary(tree_, layout, mouse);
          if (boundary.split_node != nullptr) {
            ImGui::SetMouseCursor(boundary.horizontal
                                      ? ImGuiMouseCursor_ResizeEW
                                      : ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
              // Snapshot before the first ratio edit so the whole drag
              // collapses into a single undo step.
              PushUndoSnapshot();
              drag_.split_node = const_cast<DockNode*>(boundary.split_node);
              drag_.start_ratio = drag_.split_node->split_ratio;
              drag_.start_mouse = mouse;
              drag_.start_rect = layout.node_rects.at(boundary.split_node);
              // Also surface the split as the current selection so the
              // accent outline tracks the resize target.
              selected_ = drag_.split_node;
            }
          } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selected_ = HitTestNode(layout, mouse);
          }
        }
      }
    }
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(2);
    if (ImGui::BeginChild("layout_designer_properties", ImVec2(0, 0),
                          ImGuiChildFlags_None)) {
      DrawPropertiesColumn();
    }
    ImGui::EndChild();

    ImGui::EndTable();
  }

  // Popups are opened from the File row handlers, then drawn once each
  // frame at panel scope so ImGui can size them against the panel's
  // frame.
  if (open_popup_requested_) {
    ImGui::OpenPopup(kOpenPopupId);
    open_popup_requested_ = false;
  }
  if (save_as_popup_requested_) {
    ImGui::OpenPopup(kSaveAsPopupId);
    save_as_popup_requested_ = false;
  }
  DrawOpenPopup();
  DrawSaveAsPopup();

  ImGui::Separator();
  ImGui::Text("Editing: %s | Undo: %zu / Redo: %zu",
              tree_.name.empty() ? "(unnamed)" : tree_.name.c_str(),
              undo_.UndoDepth(), undo_.RedoDepth());
  if (!status_message_.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (status_is_error_) {
      ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.0f), "%s",
                         status_message_.c_str());
    } else {
      ImGui::TextUnformatted(status_message_.c_str());
    }
  }
}

void LayoutDesignerPanel::DrawPropertiesColumn() {
  // Snapshot on the first frame of any interactive ImGui item
  // (slider, input-text, combo). Collapses a continuous drag or a
  // keystroke burst into a single undo step.
  auto snapshot_on_activation = [this]() {
    if (ImGui::IsItemActivated())
      PushUndoSnapshot();
  };

  if (selected_ == nullptr) {
    ImGui::SeparatorText("Layout");
    // Bind the InputText buffer directly to the tree fields; imgui_stdlib
    // keeps the std::string in sync per keystroke, and IsItemActivated
    // fires once per focus-in so the undo step covers the whole edit.
    ImGui::InputText("Name", &tree_.name);
    snapshot_on_activation();
    ImGui::InputTextMultiline("Description", &tree_.description,
                              ImVec2(0.0f, 80.0f));
    snapshot_on_activation();
    ImGui::TextDisabled("Select a cell in the canvas to edit it.");
    return;
  }

  DockNode* node = const_cast<DockNode*>(selected_);
  if (node->type == DockNode::Type::kSplit) {
    ImGui::SeparatorText("Split");

    const SplitDirection current_dir = node->split_direction;
    if (ImGui::BeginCombo("Direction", SplitDirectionLabel(current_dir))) {
      for (int i = 0; i < 4; ++i) {
        const auto candidate = static_cast<SplitDirection>(i);
        const bool is_selected = candidate == current_dir;
        if (ImGui::Selectable(SplitDirectionLabel(candidate), is_selected)) {
          if (candidate != current_dir) {
            PushUndoSnapshot();
            node->split_direction = candidate;
          }
        }
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::SliderFloat("Ratio", &node->split_ratio, kMinSplitRatio,
                       kMaxSplitRatio, "%.2f");
    snapshot_on_activation();
    return;
  }

  // Leaf properties.
  ImGui::SeparatorText("Leaf");
  if (node->panels.empty()) {
    ImGui::TextDisabled("(no panels — drop from the palette)");
    return;
  }

  // Active-tab selector. Snapshot once on drag start; live-commit the
  // value while the slider is active.
  const int panel_count = static_cast<int>(node->panels.size());
  int active = std::clamp(node->active_tab_index, 0, panel_count - 1);
  ImGui::SliderInt("Active tab", &active, 0, panel_count - 1);
  snapshot_on_activation();
  if (active != node->active_tab_index) {
    node->active_tab_index = active;
  }

  ImGui::Separator();
  // Panel list with reorder + remove actions.
  for (int i = 0; i < panel_count; ++i) {
    ImGui::PushID(i);
    const auto& entry = node->panels[i];
    const std::string label = entry.icon.empty()
                                  ? entry.display_name
                                  : entry.icon + "  " + entry.display_name;
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine();

    ImGui::BeginDisabled(i == 0);
    if (ImGui::SmallButton("Up")) {
      PushUndoSnapshot();
      std::swap(node->panels[i], node->panels[i - 1]);
      if (node->active_tab_index == i)
        --node->active_tab_index;
      else if (node->active_tab_index == i - 1)
        ++node->active_tab_index;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(i + 1 >= panel_count);
    if (ImGui::SmallButton("Down")) {
      PushUndoSnapshot();
      std::swap(node->panels[i], node->panels[i + 1]);
      if (node->active_tab_index == i)
        ++node->active_tab_index;
      else if (node->active_tab_index == i + 1)
        --node->active_tab_index;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::SmallButton("Remove")) {
      PushUndoSnapshot();
      // If the removed entry sits before the active tab, the active
      // panel's index shifts down by one. std::clamp afterward handles
      // the "removed the active tab" and "removed the last tab" cases.
      if (i < node->active_tab_index)
        --node->active_tab_index;
      node->panels.erase(node->panels.begin() + i);
      if (!node->panels.empty()) {
        node->active_tab_index =
            std::clamp(node->active_tab_index, 0,
                       static_cast<int>(node->panels.size()) - 1);
      } else {
        node->active_tab_index = 0;
      }
      ImGui::PopID();
      break;
    }

    ImGui::PopID();
  }
}

REGISTER_PANEL(LayoutDesignerPanel);

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
