#include "editor_registry.h"

#include <unordered_set>
#include "absl/strings/str_format.h"
#include "app/editor/editor.h"

namespace yaze {
namespace editor {

// Static mappings for editor types
const std::unordered_map<EditorType, std::string>
    EditorRegistry::kEditorCategories = {{EditorType::kDungeon, "Dungeon"},
                                         {EditorType::kOverworld, "Overworld"},
                                         {EditorType::kGraphics, "Graphics"},
                                         {EditorType::kPalette, "Palette"},
                                         {EditorType::kSprite, "Sprite"},
                                         {EditorType::kScreen, "Screen"},
                                         {EditorType::kMessage, "Message"},
                                         {EditorType::kMusic, "Music"},
                                         {EditorType::kAssembly, "Assembly"},
                                         {EditorType::kEmulator, "Emulator"},
                                         {EditorType::kHex, "Hex"},
                                         {EditorType::kAgent, "Agent"},
                                         {EditorType::kSettings, "System"}};

const std::unordered_map<EditorType, std::string> EditorRegistry::kEditorNames =
    {{EditorType::kDungeon, "Dungeon Editor"},
     {EditorType::kOverworld, "Overworld Editor"},
     {EditorType::kGraphics, "Graphics Editor"},
     {EditorType::kPalette, "Palette Editor"},
     {EditorType::kSprite, "Sprite Editor"},
     {EditorType::kScreen, "Screen Editor"},
     {EditorType::kMessage, "Message Editor"},
     {EditorType::kMusic, "Music Editor"},
     {EditorType::kAssembly, "Assembly Editor"},
     {EditorType::kEmulator, "Emulator Editor"},
     {EditorType::kHex, "Hex Editor"},
     {EditorType::kAgent, "Agent Editor"},
     {EditorType::kSettings, "Settings Editor"}};

const std::unordered_map<EditorType, bool> EditorRegistry::kCardBasedEditors = {
    {EditorType::kDungeon, true},
    {EditorType::kOverworld, true},
    {EditorType::kGraphics, true},
    {EditorType::kPalette, true},
    {EditorType::kSprite, true},
    {EditorType::kScreen, true},
    {EditorType::kMessage, true},
    {EditorType::kMusic, true},
    {EditorType::kAssembly, true},
    {EditorType::kEmulator, true},
    {EditorType::kHex, true},
    {EditorType::kAgent, false},  // Agent: Traditional UI
    {EditorType::kSettings,
     true}  // Settings: Now card-based for better organization
};

bool EditorRegistry::IsCardBasedEditor(EditorType type) {
  auto it = kCardBasedEditors.find(type);
  return it != kCardBasedEditors.end() && it->second;
}

std::string EditorRegistry::GetEditorCategory(EditorType type) {
  auto it = kEditorCategories.find(type);
  if (it != kEditorCategories.end()) {
    return it->second;
  }
  return "Unknown";
}

EditorType EditorRegistry::GetEditorTypeFromCategory(
    const std::string& category) {
  for (const auto& [type, cat] : kEditorCategories) {
    if (cat == category) {
      return type;  // Return first match
    }
  }
  return EditorType::kSettings;  // Default fallback
}

void EditorRegistry::JumpToDungeonRoom(int room_id) {
  auto it = registered_editors_.find(EditorType::kDungeon);
  if (it != registered_editors_.end() && it->second) {
    // TODO: Implement dungeon room jumping
    // This would typically call a method on the dungeon editor
    printf("[EditorRegistry] Jumping to dungeon room %d\n", room_id);
  }
}

void EditorRegistry::JumpToOverworldMap(int map_id) {
  auto it = registered_editors_.find(EditorType::kOverworld);
  if (it != registered_editors_.end() && it->second) {
    // TODO: Implement overworld map jumping
    // This would typically call a method on the overworld editor
    printf("[EditorRegistry] Jumping to overworld map %d\n", map_id);
  }
}

void EditorRegistry::SwitchToEditor(EditorType editor_type) {
  ValidateEditorType(editor_type);

  auto it = registered_editors_.find(editor_type);
  if (it != registered_editors_.end() && it->second) {
    // Deactivate all other editors
    for (auto& [type, editor] : registered_editors_) {
      if (type != editor_type && editor) {
        editor->set_active(false);
      }
    }

    // Activate the target editor
    it->second->set_active(true);
    printf("[EditorRegistry] Switched to %s\n",
           GetEditorDisplayName(editor_type).c_str());
  }
}

void EditorRegistry::HideCurrentEditorCards() {
  for (auto& [type, editor] : registered_editors_) {
    if (editor && IsCardBasedEditor(type)) {
      // TODO: Hide cards for this editor
      printf("[EditorRegistry] Hiding cards for %s\n",
             GetEditorDisplayName(type).c_str());
    }
  }
}

void EditorRegistry::ShowEditorCards(EditorType editor_type) {
  ValidateEditorType(editor_type);

  if (IsCardBasedEditor(editor_type)) {
    // TODO: Show cards for this editor
    printf("[EditorRegistry] Showing cards for %s\n",
           GetEditorDisplayName(editor_type).c_str());
  }
}

void EditorRegistry::ToggleEditorCards(EditorType editor_type) {
  ValidateEditorType(editor_type);

  if (IsCardBasedEditor(editor_type)) {
    // TODO: Toggle cards for this editor
    printf("[EditorRegistry] Toggling cards for %s\n",
           GetEditorDisplayName(editor_type).c_str());
  }
}

std::vector<EditorType> EditorRegistry::GetEditorsInCategory(
    const std::string& category) const {
  std::vector<EditorType> editors;

  for (const auto& [type, cat] : kEditorCategories) {
    if (cat == category) {
      editors.push_back(type);
    }
  }

  return editors;
}

std::vector<std::string> EditorRegistry::GetAvailableCategories() const {
  std::vector<std::string> categories;
  std::unordered_set<std::string> seen;

  for (const auto& [type, category] : kEditorCategories) {
    if (seen.find(category) == seen.end()) {
      categories.push_back(category);
      seen.insert(category);
    }
  }

  return categories;
}

std::string EditorRegistry::GetEditorDisplayName(EditorType type) const {
  auto it = kEditorNames.find(type);
  if (it != kEditorNames.end()) {
    return it->second;
  }
  return "Unknown Editor";
}

void EditorRegistry::RegisterEditor(EditorType type, Editor* editor) {
  ValidateEditorType(type);

  if (!editor) {
    throw std::invalid_argument("Editor pointer cannot be null");
  }

  registered_editors_[type] = editor;
  printf("[EditorRegistry] Registered %s\n",
         GetEditorDisplayName(type).c_str());
}

void EditorRegistry::UnregisterEditor(EditorType type) {
  ValidateEditorType(type);

  auto it = registered_editors_.find(type);
  if (it != registered_editors_.end()) {
    registered_editors_.erase(it);
    printf("[EditorRegistry] Unregistered %s\n",
           GetEditorDisplayName(type).c_str());
  }
}

Editor* EditorRegistry::GetEditor(EditorType type) const {
  ValidateEditorType(type);

  auto it = registered_editors_.find(type);
  if (it != registered_editors_.end()) {
    return it->second;
  }
  return nullptr;
}

bool EditorRegistry::IsEditorActive(EditorType type) const {
  ValidateEditorType(type);

  auto it = registered_editors_.find(type);
  if (it != registered_editors_.end() && it->second) {
    return it->second->active();
  }
  return false;
}

bool EditorRegistry::IsEditorVisible(EditorType type) const {
  ValidateEditorType(type);

  auto it = registered_editors_.find(type);
  if (it != registered_editors_.end() && it->second) {
    return it->second->active();
  }
  return false;
}

void EditorRegistry::SetEditorActive(EditorType type, bool active) {
  ValidateEditorType(type);

  auto it = registered_editors_.find(type);
  if (it != registered_editors_.end() && it->second) {
    it->second->set_active(active);
  }
}

bool EditorRegistry::IsValidEditorType(EditorType type) const {
  return kEditorCategories.find(type) != kEditorCategories.end();
}

void EditorRegistry::ValidateEditorType(EditorType type) const {
  if (!IsValidEditorType(type)) {
    throw std::invalid_argument(
        absl::StrFormat("Invalid editor type: %d", static_cast<int>(type)));
  }
}

}  // namespace editor
}  // namespace yaze
