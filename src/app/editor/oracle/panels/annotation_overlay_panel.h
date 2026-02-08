#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_ANNOTATION_OVERLAY_PANEL_H
#define YAZE_APP_EDITOR_ORACLE_PANELS_ANNOTATION_OVERLAY_PANEL_H

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/editor/core/content_registry.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {

/**
 * @brief Annotation priority levels.
 */
enum class AnnotationPriority : int {
  kNote = 0,
  kBug = 1,
  kBlocker = 2,
};

/**
 * @brief A room-level annotation entry.
 */
struct AnnotationEntry {
  int room_id = 0;
  std::string text;
  AnnotationPriority priority = AnnotationPriority::kNote;
  std::string category;  // "design", "test", "bug", etc.
  std::string created_at;
};

/**
 * @class AnnotationOverlayPanel
 * @brief Room-level annotation management for Oracle projects.
 *
 * Reads/writes annotations from `annotations.json` in the project folder.
 * Provides a list view for browsing and editing annotations.
 *
 * On iOS (F3), annotations sync via CloudKit. On desktop, this JSON file
 * can be committed to git for version-controlled notes.
 *
 * Integration with DungeonMapPanel: colored dots drawn on rooms
 * (red=blocker, orange=bug, blue=note).
 */
class AnnotationOverlayPanel : public EditorPanel {
 public:
  std::string GetId() const override { return "oracle.annotations"; }
  std::string GetDisplayName() const override { return "Annotations"; }
  std::string GetIcon() const override { return ICON_MD_NOTES; }
  std::string GetEditorCategory() const override { return "Oracle"; }
  PanelCategory GetPanelCategory() const override {
    return PanelCategory::CrossEditor;
  }

  void SetAnnotationsPath(const std::string& path) {
    annotations_path_ = path;
    LoadAnnotations();
  }

  void Draw(bool* /*p_open*/) override {
    // Lazily discover annotations path from project context
    if (annotations_path_.empty()) {
      if (auto* project = ContentRegistry::Context::current_project()) {
        if (!project->code_folder.empty()) {
          namespace fs = std::filesystem;
          fs::path candidate =
              fs::path(project->GetAbsolutePath(project->code_folder)) /
              "Docs" / "Dev" / "Planning" / "annotations.json";
          SetAnnotationsPath(candidate.string());
        }
      }
    }

    // Filter controls
    ImGui::Text("Room filter:");
    ImGui::SameLine();
    ImGui::InputInt("##room_filter", &filter_room_id_);
    ImGui::SameLine();
    if (ImGui::Button("All")) filter_room_id_ = -1;

    ImGui::SameLine();
    const char* priorities[] = {"All", "Note", "Bug", "Blocker"};
    ImGui::Combo("Priority", &filter_priority_, priorities, 4);

    ImGui::Separator();

    // Annotation list
    ImGui::BeginChild("annotation_list", ImVec2(0, -120),
                      ImGuiChildFlags_Borders);

    for (size_t i = 0; i < annotations_.size(); ++i) {
      const auto& ann = annotations_[i];

      // Apply filters
      if (filter_room_id_ >= 0 && ann.room_id != filter_room_id_) continue;
      if (filter_priority_ > 0 &&
          static_cast<int>(ann.priority) != (filter_priority_ - 1))
        continue;

      ImU32 dot_color = GetPriorityColor(ann.priority);
      ImVec2 cursor = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddCircleFilled(
          ImVec2(cursor.x + 6, cursor.y + 10), 5.0f, dot_color);
      ImGui::Dummy(ImVec2(16, 0));
      ImGui::SameLine();

      char label[128];
      snprintf(label, sizeof(label), "Room 0x%02X: %s##ann_%zu", ann.room_id,
               ann.text.c_str(), i);

      if (ImGui::Selectable(label, selected_index_ == static_cast<int>(i))) {
        selected_index_ = static_cast<int>(i);
        // Copy to edit buffer
        edit_room_ = ann.room_id;
        snprintf(edit_text_, sizeof(edit_text_), "%s", ann.text.c_str());
        edit_priority_ = static_cast<int>(ann.priority);
        snprintf(edit_category_, sizeof(edit_category_), "%s",
                 ann.category.c_str());
      }
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Edit / Add form
    ImGui::Text("Room:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("##edit_room", &edit_room_);

    ImGui::SameLine();
    ImGui::Text("Priority:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    const char* pri_names[] = {"Note", "Bug", "Blocker"};
    ImGui::Combo("##edit_priority", &edit_priority_, pri_names, 3);

    ImGui::InputText("Text", edit_text_, sizeof(edit_text_));
    ImGui::InputText("Category", edit_category_, sizeof(edit_category_));

    if (ImGui::Button("Add")) {
      AnnotationEntry entry;
      entry.room_id = edit_room_;
      entry.text = edit_text_;
      entry.priority = static_cast<AnnotationPriority>(edit_priority_);
      entry.category = edit_category_;
      annotations_.push_back(entry);
      SaveAnnotations();
    }

    ImGui::SameLine();
    if (selected_index_ >= 0 &&
        selected_index_ < static_cast<int>(annotations_.size())) {
      if (ImGui::Button("Update")) {
        auto& ann = annotations_[selected_index_];
        ann.room_id = edit_room_;
        ann.text = edit_text_;
        ann.priority = static_cast<AnnotationPriority>(edit_priority_);
        ann.category = edit_category_;
        SaveAnnotations();
      }
      ImGui::SameLine();
      if (ImGui::Button("Delete")) {
        annotations_.erase(annotations_.begin() + selected_index_);
        selected_index_ = -1;
        SaveAnnotations();
      }
    }
  }

  // ─── Public API for DungeonMapPanel integration ───────────────

  /**
   * @brief Get annotations for a specific room.
   */
  std::vector<const AnnotationEntry*> GetAnnotationsForRoom(
      int room_id) const {
    std::vector<const AnnotationEntry*> result;
    for (const auto& ann : annotations_) {
      if (ann.room_id == room_id) {
        result.push_back(&ann);
      }
    }
    return result;
  }

  /**
   * @brief Get the highest priority for a room (for dot color).
   * Returns -1 if no annotations.
   */
  int GetMaxPriorityForRoom(int room_id) const {
    int max_pri = -1;
    for (const auto& ann : annotations_) {
      if (ann.room_id == room_id) {
        max_pri = std::max(max_pri, static_cast<int>(ann.priority));
      }
    }
    return max_pri;
  }

  static ImU32 GetPriorityColor(AnnotationPriority priority) {
    switch (priority) {
      case AnnotationPriority::kBlocker:
        return IM_COL32(220, 50, 50, 255);   // Red
      case AnnotationPriority::kBug:
        return IM_COL32(220, 150, 30, 255);  // Orange
      case AnnotationPriority::kNote:
      default:
        return IM_COL32(60, 120, 220, 255);  // Blue
    }
  }

 private:
  void LoadAnnotations() {
    annotations_.clear();
    if (annotations_path_.empty()) return;

    std::ifstream file(annotations_path_);
    if (!file.is_open()) return;

    try {
      nlohmann::json root;
      file >> root;

      if (root.contains("annotations") && root["annotations"].is_array()) {
        for (const auto& item : root["annotations"]) {
          AnnotationEntry entry;
          entry.room_id = item.value("room_id", 0);
          entry.text = item.value("text", "");
          entry.priority =
              static_cast<AnnotationPriority>(item.value("priority", 0));
          entry.category = item.value("category", "");
          entry.created_at = item.value("created_at", "");
          annotations_.push_back(std::move(entry));
        }
      }
    } catch (...) {
      // Silently ignore parse errors
    }
  }

  void SaveAnnotations() {
    if (annotations_path_.empty()) return;

    nlohmann::json root;
    nlohmann::json arr = nlohmann::json::array();

    for (const auto& ann : annotations_) {
      nlohmann::json item;
      item["room_id"] = ann.room_id;
      item["text"] = ann.text;
      item["priority"] = static_cast<int>(ann.priority);
      item["category"] = ann.category;
      item["created_at"] = ann.created_at;
      arr.push_back(item);
    }

    root["annotations"] = arr;

    std::ofstream file(annotations_path_);
    if (file.is_open()) {
      file << root.dump(2) << std::endl;
    }
  }

  std::string annotations_path_;
  std::vector<AnnotationEntry> annotations_;

  // Filter state
  int filter_room_id_ = -1;
  int filter_priority_ = 0;

  // Edit state
  int selected_index_ = -1;
  int edit_room_ = 0;
  char edit_text_[256] = {0};
  int edit_priority_ = 0;
  char edit_category_[64] = {0};
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_ANNOTATION_OVERLAY_PANEL_H
