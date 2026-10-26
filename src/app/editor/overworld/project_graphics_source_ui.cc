#include "app/editor/overworld/overworld_editor.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

#include "absl/strings/str_format.h"
#include "core/project.h"
#include "imgui/imgui.h"

namespace yaze::editor {

absl::Status OverworldEditor::OpenProjectGraphicsSource() {
  auto* owner = TerrainProject();
  const auto* map = overworld_.overworld_map(current_map_);
  if (!owner || owner->git_repository.empty()) {
    return absl::FailedPreconditionError(
        "Select a native map in a project with a bound graphics source");
  }
  const std::string root = owner->GetAbsolutePath(owner->git_repository);
  const std::string catalog = ProjectGraphicsPath();
  // A combined import action retains this document for main Undo/Redo even
  // after publication has made its byte baseline clean.
  if (graphics_source_ &&
      (graphics_source_->any_dirty() || graphics_import_applied_)) {
    graphics_source_open_ = true;
    if (graphics_source_project_ != owner ||
        graphics_source_->source_root() != root ||
        graphics_source_->portable_catalog() != catalog) {
      return absl::FailedPreconditionError(
          "The graphics draft belongs to another project. Return to that "
          "project before editing or publishing its graphics.");
    }
    return absl::OkStatus();
  }
  if (!map || is_lyra_preview() || IsProjectTerrainMap(current_map_)) {
    return absl::FailedPreconditionError(
        "Select a native map before opening its project graphics source");
  }
  auto next = std::make_shared<ProjectGraphicsSource>();
  const auto status = next->Load(root, catalog, map->parent());
  if (!status.ok())
    return status;
  graphics_source_ = std::move(next);
  graphics_source_project_ = owner;
  graphics_source_tile_ = 0;
  graphics_source_open_ = true;
  return absl::OkStatus();
}

void OverworldEditor::DrawProjectGraphicsSource() {
  constexpr const char* kPopup = "Edit Project Graphics";
  if (graphics_source_open_ && !ImGui::IsPopupOpen(kPopup))
    ImGui::OpenPopup(kPopup);
  if (!ImGui::IsPopupOpen(kPopup))
    return;
  const auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowSize(
      ImVec2(std::min(720.0f, viewport->WorkSize.x * 0.9f),
             std::min(740.0f, viewport->WorkSize.y * 0.9f)),
      ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(kPopup))
    return;
  // Session teardown can clear the source while ImGui retains the modal's
  // open flag for this frame.
  if (!graphics_source_ || !graphics_source_open_) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  auto& source = *graphics_source_;
  const auto* owner = TerrainProject();
  const bool bound =
      graphics_source_project_ == owner &&
      source.portable_catalog() == ProjectGraphicsPath() && owner &&
      source.source_root() == owner->GetAbsolutePath(owner->git_repository);
  const auto show_status = [this](const absl::Status& status) {
    graphics_source_status_ = status;
    graphics_source_message_.clear();
  };
  const auto refresh_after_edit = [this,
                                   &show_status](const absl::Status& status) {
    show_status(status.ok() ? RefreshPendingProjectGraphics() : status);
  };
  if (!bound) {
    ImGui::TextWrapped(
        "This draft belongs to another project. Return to its project to "
        "edit or publish its graphics.");
  }
  if (!graphics_source_status_.ok())
    ImGui::TextWrapped("%s", graphics_source_status_.ToString().c_str());
  if (!graphics_source_message_.empty())
    ImGui::TextWrapped("%s", graphics_source_message_.c_str());
  ImGui::TextWrapped(
      "Publish graphics first, then Save ROM for palettes, definitions and "
      "map placement. Build Project includes source assets in the game.");

  if (ImGui::BeginChild("Resource",
                        ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true)) {
    ImGui::BeginDisabled(!bound);
    const auto* resource = source.selected_resource();
    ImGui::BeginDisabled(graphics_import_applied_);
    if (resource &&
        ImGui::BeginCombo("Resource", resource->source_path.c_str())) {
      for (const auto& choice : source.resources()) {
        ImGui::PushID(choice.id.c_str());
        const std::string label =
            choice.source_path + (choice.dirty() ? " *" : "");
        if (ImGui::Selectable(label.c_str(), choice.id == resource->id)) {
          show_status(source.SelectResource(choice.id));
          graphics_source_tile_ = 0;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    resource = source.selected_resource();
    if (resource) {
      ImGui::TextWrapped("Content ID: %s", resource->id.c_str());
      std::string consumers;
      for (int parent : resource->parent_maps)
        consumers += absl::StrFormat("$%02X ", parent);
      ImGui::TextWrapped("Affected parent maps: %s", consumers.c_str());
      if (ImGui::TreeNode("Source slices")) {
        for (const auto& slice : resource->slices) {
          ImGui::TextWrapped(
              "%s: resource tiles %zu..%zu -> BG characters $%03X..$%03X",
              slice.profile_id.c_str(), slice.first_resource_tile(),
              slice.first_resource_tile() + slice.byte_count / 32 - 1,
              slice.first_character(),
              slice.first_character() +
                  static_cast<int>(slice.byte_count / 32) - 1);
        }
        ImGui::TreePop();
      }
      DrawProjectGraphicsImport();
      ImGui::SliderInt("Preview palette row", &graphics_source_palette_, 0, 7);
      const auto* map = overworld_.overworld_map(source.parent_map());
      const auto color = [&](uint8_t index) {
        const size_t at = graphics_source_palette_ * 16 + index;
        if (map && at < map->current_palette().size())
          return gfx::SnesTo8bppColor(map->current_palette()[at].snes());
        const float value = index / 15.0f;
        return ImVec4(value, value, value, 1);
      };
      for (int index = 0; index < 16; ++index) {
        ImGui::PushID(index);
        if (index % 8)
          ImGui::SameLine(0, 2);
        if (ImGui::ColorButton("Index", color(index),
                               ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24)))
          graphics_source_index_ = index;
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Pixel index %d%s", index,
                            index == 0 ? " (transparent)" : "");
        if (index == graphics_source_index_)
          ImGui::GetWindowDrawList()->AddRect(
              ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
              IM_COL32(255, 255, 255, 255), 0, 0, 2);
        ImGui::PopID();
      }
      ImGui::Text("Paint index: %d", graphics_source_index_);
      graphics_source_tile_ =
          std::clamp(graphics_source_tile_, 0,
                     static_cast<int>(resource->tiles.size()) - 1);
      ImGui::SliderInt("Resource tile", &graphics_source_tile_, 0,
                       static_cast<int>(resource->tiles.size()) - 1);
      const auto draw_tile = [&](int tile, ImVec2 origin, float pixel_size) {
        auto* list = ImGui::GetWindowDrawList();
        for (int y = 0; y < 8; ++y) {
          for (int x = 0; x < 8; ++x) {
            const auto index = source.GetPixel(tile, x, y);
            if (!index.ok())
              continue;
            const ImVec2 pos(origin.x + x * pixel_size,
                             origin.y + y * pixel_size);
            list->AddRectFilled(pos,
                                ImVec2(pos.x + pixel_size, pos.y + pixel_size),
                                ImGui::ColorConvertFloat4ToU32(color(*index)));
          }
        }
      };
      if (ImGui::BeginChild("Tiles", ImVec2(0, 136), true)) {
        const int columns = std::max(
            1, static_cast<int>(ImGui::GetContentRegionAvail().x / 28));
        ImGuiListClipper clipper;
        clipper.Begin(
            (static_cast<int>(resource->tiles.size()) + columns - 1) / columns,
            24 + ImGui::GetStyle().ItemSpacing.y);
        while (clipper.Step()) {
          for (int row = clipper.DisplayStart; row < clipper.DisplayEnd;
               ++row) {
            for (int column = 0; column < columns; ++column) {
              const int tile = row * columns + column;
              if (tile >= static_cast<int>(resource->tiles.size()))
                break;
              if (column)
                ImGui::SameLine(0, 4);
              ImGui::PushID(tile);
              const auto origin = ImGui::GetCursorScreenPos();
              if (ImGui::InvisibleButton("Tile", ImVec2(24, 24)))
                graphics_source_tile_ = tile;
              draw_tile(tile, origin, 3);
              if (tile == graphics_source_tile_)
                ImGui::GetWindowDrawList()->AddRect(
                    origin, ImVec2(origin.x + 24, origin.y + 24),
                    IM_COL32(255, 255, 255, 255), 0, 0, 2);
              ImGui::PopID();
            }
          }
        }
      }
      ImGui::EndChild();
      const auto origin = ImGui::GetCursorScreenPos();
      ImGui::BeginDisabled(graphics_import_applied_);
      ImGui::InvisibleButton("Paint resource tile", ImVec2(160, 160));
      draw_tile(graphics_source_tile_, origin, 20);
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto mouse = ImGui::GetIO().MousePos;
        refresh_after_edit(source.SetPixel(
            graphics_source_tile_, static_cast<int>((mouse.x - origin.x) / 20),
            static_cast<int>((mouse.y - origin.y) / 20),
            graphics_source_index_));
      }
      ImGui::BeginDisabled(!source.can_undo());
      if (ImGui::Button("Undo Pixel"))
        refresh_after_edit(source.Undo());
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(!source.can_redo());
      if (ImGui::Button("Redo Pixel"))
        refresh_after_edit(source.Redo());
      ImGui::EndDisabled();
      ImGui::EndDisabled();
      if (graphics_import_applied_)
        ImGui::TextWrapped(
            "Use the main Undo and Redo commands to revert or restore the "
            "combined graphics import, palette, definitions and placement.");
      ImGui::BeginDisabled(!source.dirty() || !source.backend_available());
      if (ImGui::Button("Review Source Publish")) {
        const auto preview = source.Preview();
        show_status(preview.status());
      }
      ImGui::EndDisabled();
      if (!source.backend_available()) {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
        ImGui::TextWrapped(
            "Source publication currently requires a native macOS or Linux "
            "build with Python 3.");
#else
        ImGui::TextWrapped(
            "The bound source needs Scripts/Graphics/edit_graphics_source.py "
            "before this draft can be published.");
#endif
      }
      if (source.has_preview()) {
        const auto& plan = source.preview_json();
        ImGui::TextWrapped(
            "New content ID: %s",
            plan.value("after_resource_id", std::string()).c_str());
        if (ImGui::TreeNode("Review files and fingerprints")) {
          ImGui::TextWrapped("%s", plan.dump(2).c_str());
          ImGui::TreePop();
        }
        if (ImGui::Button("Publish Reviewed Source")) {
          const auto published = source.Publish();
          show_status(published.status());
          if (published.ok()) {
            // Publication is durable before render reload. A reload error
            // must not be reported as a source transaction rollback.
            const auto reload = ReloadProjectGraphics();
            if (!reload.ok()) {
              show_status(absl::FailedPreconditionError(
                  "Source published; graphics reload needs attention: " +
                  std::string(reload.message())));
            } else {
              graphics_source_message_ =
                  "Graphics published and reloaded. Save ROM to persist "
                  "palettes, definitions and placement, then Build Project "
                  "to include the source assets in the game.";
            }
          }
        }
      }
    }
    ImGui::EndDisabled();
  }
  ImGui::EndChild();
  if (ImGui::Button("Close (keep draft)")) {
    graphics_source_open_ = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

}  // namespace yaze::editor
