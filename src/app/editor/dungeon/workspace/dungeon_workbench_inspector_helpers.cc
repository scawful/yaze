#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"

#include <algorithm>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_object_selector.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "core/features.h"
#include "zelda3/dungeon/custom_object.h"

namespace yaze::editor::workbench {

bool HasEditableRoomObjectSize(std::span<const zelda3::RoomObject> objects,
                               std::span<const size_t> selected_indices) {
  return std::any_of(selected_indices.begin(), selected_indices.end(),
                     [&](size_t index) {
                       return index < objects.size() &&
                              zelda3::IsRoomObjectResizable(objects[index].id_);
                     });
}

bool DrawObjectSizeControls(const zelda3::RoomObject& object,
                            uint8_t* requested_size) {
  if (!requested_size) {
    return false;
  }
  uint8_t size = object.size_;
  const int axis_step = zelda3::RoomObjectSizeAxisStep(object.id_);
  if (axis_step > 0) {
    for (const bool horizontal : {true, false}) {
      gui::LayoutHelpers::PropertyRow(horizontal ? "Width" : "Height", [&]() {
        const int shift = horizontal ? 2 : 0;
        const int current = (size >> shift) & 3;
        const auto preview = absl::StrFormat(
            "%d tiles",
            zelda3::RoomObjectSizeAxisTiles(object.id_, size, horizontal));
        ImGui::SetNextItemWidth(-1);
        const bool open = ImGui::BeginCombo(
            horizontal ? "##SelObjWidth" : "##SelObjHeight", preview.c_str());
        if (open) {
          for (int value = 0; value < 4; ++value) {
            const auto candidate =
                static_cast<uint8_t>((size & ~(3 << shift)) | (value << shift));
            const auto label = absl::StrFormat(
                "%d tiles", zelda3::RoomObjectSizeAxisTiles(
                                object.id_, candidate, horizontal));
            if (ImGui::Selectable(label.c_str(), value == current)) {
              size = candidate;
            }
            {
              gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
              gui::AutoRegisterLastItem(
                  "selectable",
                  absl::StrFormat("object_%s_%d",
                                  horizontal ? "width" : "height", value));
            }
            if (value == current) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
        // EndCombo restores the control's item data after the popup window.
        {
          gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
          gui::AutoRegisterLastItem(
              "combo",
              horizontal ? "selected_object_width" : "selected_object_height",
              horizontal ? "Selected object width in tiles"
                         : "Selected object height in tiles");
        }
      });
    }
  } else {
    auto& manager = zelda3::CustomObjectManager::Get();
    const int variants = core::FeatureFlags::get().kEnableCustomObjects
                             ? std::min(manager.GetSubtypeCount(object.id_), 16)
                             : 0;
    if (variants > 0 && zelda3::IsRoomObjectSizeEditable(object.id_)) {
      const auto variant_label = [&](int subtype) {
        if (zelda3::CustomObjectManager::RuntimeSubtypeCountForObject(
                object.id_) > 0) {
          return GetDungeonCustomObjectSlotName(object.id_, subtype);
        }
        const auto filename = manager.ResolveFilename(object.id_, subtype);
        return filename.empty()
                   ? absl::StrFormat("Variant %d (unmapped)", subtype)
                   : std::filesystem::path(filename).stem().string();
      };
      gui::LayoutHelpers::PropertyRow("Variant", [&]() {
        const auto preview = size < variants
                                 ? variant_label(size)
                                 : absl::StrFormat("Invalid variant %d", size);
        ImGui::SetNextItemWidth(-1);
        const bool open = ImGui::BeginCombo("##SelObjVariant", preview.c_str());
        if (open) {
          for (int subtype = 0; subtype < variants; ++subtype) {
            const auto label =
                absl::StrFormat("%02X  %s", subtype, variant_label(subtype));
            const bool mapped =
                !manager.ResolveFilename(object.id_, subtype).empty();
            ImGui::BeginDisabled(!mapped);
            if (ImGui::Selectable(label.c_str(), size == subtype)) {
              size = static_cast<uint8_t>(subtype);
            }
            {
              gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
              gui::AutoRegisterLastItem(
                  "selectable", absl::StrFormat("object_variant_%d", subtype));
            }
            ImGui::EndDisabled();
          }
          ImGui::EndCombo();
        }
        {
          gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
          gui::AutoRegisterLastItem("combo", "selected_object_variant",
                                    "Selected custom object variant");
        }
      });
    } else {
      gui::LayoutHelpers::PropertyRow("Size", [&]() {
        if (!zelda3::IsRoomObjectResizable(object.id_)) {
          ImGui::TextDisabled("Fixed");
          return;
        }
        if (auto result =
                gui::InputHexByteEx("##SelObjSize", &size, 0x0F, -1.0f, true);
            !result.ShouldApply()) {
          size = object.size_;
        }
      });
    }
  }
  *requested_size = size;
  return size != object.size_;
}

void DrawInspectorSectionHeader(const char* label) {
  ImGui::SeparatorText(label);
}

bool BeginInspectorSection(const char* label, bool default_open) {
  gui::StyleVarGuard frame_padding_guard(
      ImGuiStyleVar_FramePadding,
      ImVec2(ImGui::GetStyle().FramePadding.x,
             std::max(5.0f, ImGui::GetStyle().FramePadding.y + 1.0f)));
  return ImGui::CollapsingHeader(
      label, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

bool DrawActionButton(const char* label, const ImVec2& size) {
  gui::StyleVarGuard align_guard(ImGuiStyleVar_ButtonTextAlign,
                                 ImVec2(0.08f, 0.5f));
  return ImGui::Button(label, size);
}

}  // namespace yaze::editor::workbench
