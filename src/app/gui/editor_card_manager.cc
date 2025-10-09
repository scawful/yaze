#include "editor_card_manager.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze {
namespace gui {

EditorCardManager& EditorCardManager::Get() {
  static EditorCardManager instance;
  return instance;
}

void EditorCardManager::RegisterCard(const CardInfo& info) {
  if (info.card_id.empty()) {
    printf("[EditorCardManager] Warning: Attempted to register card with empty ID\n");
    return;
  }
  
  // Check if already registered to avoid duplicates
  if (cards_.find(info.card_id) != cards_.end()) {
    printf("[EditorCardManager] WARNING: Card '%s' already registered, skipping duplicate\n", 
           info.card_id.c_str());
    return;
  }
  
  cards_[info.card_id] = info;
  printf("[EditorCardManager] Registered card: %s (%s)\n", 
         info.card_id.c_str(), info.display_name.c_str());
}

void EditorCardManager::UnregisterCard(const std::string& card_id) {
  auto it = cards_.find(card_id);
  if (it != cards_.end()) {
    printf("[EditorCardManager] Unregistered card: %s\n", card_id.c_str());
    cards_.erase(it);
  }
}

void EditorCardManager::ClearAllCards() {
  printf("[EditorCardManager] Clearing all %zu registered cards\n", cards_.size());
  cards_.clear();
}

bool EditorCardManager::ShowCard(const std::string& card_id) {
  auto it = cards_.find(card_id);
  if (it == cards_.end()) {
    return false;
  }
  
  if (it->second.visibility_flag) {
    *it->second.visibility_flag = true;
    
    if (it->second.on_show) {
      it->second.on_show();
    }
    
    return true;
  }
  
  return false;
}

bool EditorCardManager::HideCard(const std::string& card_id) {
  auto it = cards_.find(card_id);
  if (it == cards_.end()) {
    return false;
  }
  
  if (it->second.visibility_flag) {
    *it->second.visibility_flag = false;
    
    if (it->second.on_hide) {
      it->second.on_hide();
    }
    
    return true;
  }
  
  return false;
}

bool EditorCardManager::ToggleCard(const std::string& card_id) {
  auto it = cards_.find(card_id);
  if (it == cards_.end()) {
    return false;
  }
  
  if (it->second.visibility_flag) {
    bool new_state = !(*it->second.visibility_flag);
    *it->second.visibility_flag = new_state;
    
    if (new_state && it->second.on_show) {
      it->second.on_show();
    } else if (!new_state && it->second.on_hide) {
      it->second.on_hide();
    }
    
    return true;
  }
  
  return false;
}

bool EditorCardManager::IsCardVisible(const std::string& card_id) const {
  auto it = cards_.find(card_id);
  if (it != cards_.end() && it->second.visibility_flag) {
    return *it->second.visibility_flag;
  }
  return false;
}

void EditorCardManager::ShowAllCardsInCategory(const std::string& category) {
  for (auto& [id, info] : cards_) {
    if (info.category == category && info.visibility_flag) {
      *info.visibility_flag = true;
      if (info.on_show) info.on_show();
    }
  }
}

void EditorCardManager::HideAllCardsInCategory(const std::string& category) {
  for (auto& [id, info] : cards_) {
    if (info.category == category && info.visibility_flag) {
      *info.visibility_flag = false;
      if (info.on_hide) info.on_hide();
    }
  }
}

void EditorCardManager::ShowOnlyCard(const std::string& card_id) {
  auto target = cards_.find(card_id);
  if (target == cards_.end()) {
    return;
  }
  
  std::string category = target->second.category;
  
  // Hide all cards in the same category
  for (auto& [id, info] : cards_) {
    if (info.category == category && info.visibility_flag) {
      *info.visibility_flag = (id == card_id);
      
      if (id == card_id && info.on_show) {
        info.on_show();
      } else if (id != card_id && info.on_hide) {
        info.on_hide();
      }
    }
  }
}

std::vector<CardInfo> EditorCardManager::GetCardsInCategory(const std::string& category) const {
  std::vector<CardInfo> result;
  
  for (const auto& [id, info] : cards_) {
    if (info.category == category) {
      result.push_back(info);
    }
  }
  
  // Sort by priority
  std::sort(result.begin(), result.end(), 
           [](const CardInfo& a, const CardInfo& b) {
             return a.priority < b.priority;
           });
  
  return result;
}

std::vector<std::string> EditorCardManager::GetAllCategories() const {
  std::vector<std::string> categories;
  
  for (const auto& [id, info] : cards_) {
    if (std::find(categories.begin(), categories.end(), info.category) == categories.end()) {
      categories.push_back(info.category);
    }
  }
  
  std::sort(categories.begin(), categories.end());
  return categories;
}

const CardInfo* EditorCardManager::GetCardInfo(const std::string& card_id) const {
  auto it = cards_.find(card_id);
  return (it != cards_.end()) ? &it->second : nullptr;
}

void EditorCardManager::DrawViewMenuSection(const std::string& category) {
  auto cards_in_category = GetCardsInCategory(category);
  
  if (cards_in_category.empty()) {
    ImGui::MenuItem("(No cards registered)", nullptr, false, false);
    return;
  }
  
  for (const auto& info : cards_in_category) {
    if (!info.visibility_flag) continue;
    
    std::string label = info.icon.empty() 
        ? info.display_name 
        : info.icon + " " + info.display_name;
    
    bool visible = *info.visibility_flag;
    
    if (ImGui::MenuItem(label.c_str(), 
                       info.shortcut_hint.empty() ? nullptr : info.shortcut_hint.c_str(),
                       visible)) {
      ToggleCard(info.card_id);
    }
  }
}

void EditorCardManager::DrawViewMenuAll() {
  auto categories = GetAllCategories();
  
  if (categories.empty()) {
    ImGui::TextDisabled("No cards registered");
    return;
  }
  
  for (const auto& category : categories) {
    if (ImGui::BeginMenu(category.c_str())) {
      DrawViewMenuSection(category);
      ImGui::Separator();
      
      // Category-level actions
      if (ImGui::MenuItem(absl::StrFormat("%s Show All", ICON_MD_VISIBILITY).c_str())) {
        ShowAllCardsInCategory(category);
      }
      
      if (ImGui::MenuItem(absl::StrFormat("%s Hide All", ICON_MD_VISIBILITY_OFF).c_str())) {
        HideAllCardsInCategory(category);
      }
      
      ImGui::EndMenu();
    }
  }
  
  ImGui::Separator();
  
  // Global actions
  if (ImGui::MenuItem(absl::StrFormat("%s Show All Cards", ICON_MD_VISIBILITY).c_str())) {
    ShowAll();
  }
  
  if (ImGui::MenuItem(absl::StrFormat("%s Hide All Cards", ICON_MD_VISIBILITY_OFF).c_str())) {
    HideAll();
  }
  
  ImGui::Separator();
  
  if (ImGui::MenuItem(absl::StrFormat("%s Card Browser", ICON_MD_DASHBOARD).c_str(), 
                     "Ctrl+Shift+B")) {
    // This will be shown by EditorManager
  }
}

void EditorCardManager::DrawCompactCardControl(const std::string& category) {
  auto cards_in_category = GetCardsInCategory(category);
  
  if (cards_in_category.empty()) {
    return;  // Nothing to show
  }
  
  // Use theme colors for button
  ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
  
  if (ImGui::SmallButton(ICON_MD_VIEW_MODULE)) {
    ImGui::OpenPopup("CardControlPopup");
  }
  
  ImGui::PopStyleColor(3);
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s Card Controls", category.c_str());
  }
  
  // Compact popup with checkboxes
  if (ImGui::BeginPopup("CardControlPopup")) {
    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s %s Cards", 
                      ICON_MD_DASHBOARD, category.c_str());
    ImGui::Separator();
    
    for (const auto& info : cards_in_category) {
      if (!info.visibility_flag) continue;
      
      std::string label = info.icon.empty() 
          ? info.display_name 
          : info.icon + " " + info.display_name;
      
      bool visible = *info.visibility_flag;
      if (ImGui::Checkbox(label.c_str(), &visible)) {
        *info.visibility_flag = visible;
        if (visible && info.on_show) {
          info.on_show();
        } else if (!visible && info.on_hide) {
          info.on_hide();
        }
      }
      
      // Show shortcut hint
      if (!info.shortcut_hint.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", info.shortcut_hint.c_str());
      }
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem(absl::StrFormat("%s Show All", ICON_MD_VISIBILITY).c_str())) {
      ShowAllCardsInCategory(category);
    }
    
    if (ImGui::MenuItem(absl::StrFormat("%s Hide All", ICON_MD_VISIBILITY_OFF).c_str())) {
      HideAllCardsInCategory(category);
    }
    
    ImGui::EndPopup();
  }
}

void EditorCardManager::DrawInlineCardToggles(const std::string& category) {
  auto cards_in_category = GetCardsInCategory(category);
  
  if (cards_in_category.empty()) {
    return;
  }
  
  // Show visible count as indicator
  size_t visible_count = 0;
  for (const auto& info : cards_in_category) {
    if (info.visibility_flag && *info.visibility_flag) {
      visible_count++;
    }
  }
  
  ImGui::TextDisabled("%s %zu/%zu", ICON_MD_DASHBOARD, visible_count, cards_in_category.size());
  
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s cards: %zu visible, %zu hidden", 
                     category.c_str(), visible_count, 
                     cards_in_category.size() - visible_count);
  }
}

void EditorCardManager::DrawCardBrowser(bool* p_open) {
  if (!p_open || !*p_open) return;
  
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), 
                         ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  
  if (!ImGui::Begin(absl::StrFormat("%s Card Browser", ICON_MD_DASHBOARD).c_str(), 
                    p_open)) {
    ImGui::End();
    return;
  }
  
  // Search filter
  static char search_filter[256] = "";
  ImGui::SetNextItemWidth(-100);
  ImGui::InputTextWithHint("##CardSearch", 
                          absl::StrFormat("%s Search cards...", ICON_MD_SEARCH).c_str(),
                          search_filter, sizeof(search_filter));
  
  ImGui::SameLine();
  if (ImGui::Button(absl::StrFormat("%s Clear", ICON_MD_CLEAR).c_str())) {
    search_filter[0] = '\0';
  }
  
  ImGui::Separator();
  
  // Statistics
  ImGui::Text("%s Total Cards: %zu | Visible: %zu", 
             ICON_MD_INFO, GetCardCount(), GetVisibleCardCount());
  
  ImGui::Separator();
  
  // Category tabs
  if (ImGui::BeginTabBar("CardBrowserTabs")) {
    
    // All Cards tab
    if (ImGui::BeginTabItem(absl::StrFormat("%s All", ICON_MD_APPS).c_str())) {
      DrawCardBrowserTable(search_filter, "");
      ImGui::EndTabItem();
    }
    
    // Category tabs
    for (const auto& category : GetAllCategories()) {
      std::string tab_label = category;
      if (ImGui::BeginTabItem(tab_label.c_str())) {
        DrawCardBrowserTable(search_filter, category);
        ImGui::EndTabItem();
      }
    }
    
    // Presets tab
    if (ImGui::BeginTabItem(absl::StrFormat("%s Presets", ICON_MD_BOOKMARK).c_str())) {
      DrawPresetsTab();
      ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
  }
  
  ImGui::End();
}

void EditorCardManager::DrawCardBrowserTable(const char* search_filter, 
                                            const std::string& category_filter) {
  if (ImGui::BeginTable("CardBrowserTable", 4, 
                       ImGuiTableFlags_Borders | 
                       ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_Resizable |
                       ImGuiTableFlags_ScrollY)) {
    
    ImGui::TableSetupColumn("Card", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 0.2f);
    ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, 0.2f);
    ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableHeadersRow();
    
    // Collect and sort cards
    std::vector<CardInfo> display_cards;
    for (const auto& [id, info] : cards_) {
      // Apply filters
      if (!category_filter.empty() && info.category != category_filter) {
        continue;
      }
      
      if (search_filter && search_filter[0] != '\0') {
        std::string search_lower = search_filter;
        std::transform(search_lower.begin(), search_lower.end(), 
                      search_lower.begin(), ::tolower);
        
        std::string name_lower = info.display_name;
        std::transform(name_lower.begin(), name_lower.end(), 
                      name_lower.begin(), ::tolower);
        
        if (name_lower.find(search_lower) == std::string::npos) {
          continue;
        }
      }
      
      display_cards.push_back(info);
    }
    
    // Sort by category then priority
    std::sort(display_cards.begin(), display_cards.end(),
             [](const CardInfo& a, const CardInfo& b) {
               if (a.category != b.category) return a.category < b.category;
               return a.priority < b.priority;
             });
    
    // Draw rows
    for (const auto& info : display_cards) {
      ImGui::PushID(info.card_id.c_str());
      
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      
      // Card name with icon
      std::string label = info.icon.empty() 
          ? info.display_name 
          : info.icon + " " + info.display_name;
      ImGui::Text("%s", label.c_str());
      
      ImGui::TableNextColumn();
      ImGui::TextDisabled("%s", info.category.c_str());
      
      ImGui::TableNextColumn();
      if (!info.shortcut_hint.empty()) {
        ImGui::TextDisabled("%s", info.shortcut_hint.c_str());
      }
      
      ImGui::TableNextColumn();
      if (info.visibility_flag) {
        bool visible = *info.visibility_flag;
        if (ImGui::Checkbox("##Visible", &visible)) {
          *info.visibility_flag = visible;
          if (visible && info.on_show) {
            info.on_show();
          } else if (!visible && info.on_hide) {
            info.on_hide();
          }
        }
      }
      
      ImGui::PopID();
    }
    
    ImGui::EndTable();
  }
}

void EditorCardManager::DrawPresetsTab() {
  ImGui::Text("%s Workspace Presets", ICON_MD_BOOKMARK);
  ImGui::Separator();
  
  // Save current as preset
  static char preset_name[256] = "";
  static char preset_desc[512] = "";
  
  ImGui::Text("Save Current Layout:");
  ImGui::InputText("Name", preset_name, sizeof(preset_name));
  ImGui::InputText("Description", preset_desc, sizeof(preset_desc));
  
  if (ImGui::Button(absl::StrFormat("%s Save Preset", ICON_MD_SAVE).c_str())) {
    if (preset_name[0] != '\0') {
      SavePreset(preset_name, preset_desc);
      preset_name[0] = '\0';
      preset_desc[0] = '\0';
    }
  }
  
  ImGui::Separator();
  ImGui::Text("Saved Presets:");
  
  // List presets
  auto presets = GetPresets();
  if (presets.empty()) {
    ImGui::TextDisabled("No presets saved");
  } else {
    for (const auto& preset : presets) {
      ImGui::PushID(preset.name.c_str());
      
      if (ImGui::Button(absl::StrFormat("%s Load", ICON_MD_FOLDER_OPEN).c_str())) {
        LoadPreset(preset.name);
      }
      
      ImGui::SameLine();
      if (ImGui::Button(absl::StrFormat("%s Delete", ICON_MD_DELETE).c_str())) {
        DeletePreset(preset.name);
      }
      
      ImGui::SameLine();
      ImGui::Text("%s %s", ICON_MD_BOOKMARK, preset.name.c_str());
      
      if (!preset.description.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("- %s", preset.description.c_str());
      }
      
      ImGui::SameLine();
      ImGui::TextDisabled("(%zu cards)", preset.visible_cards.size());
      
      ImGui::PopID();
    }
  }
}

void EditorCardManager::SavePreset(const std::string& name, const std::string& description) {
  WorkspacePreset preset;
  preset.name = name;
  preset.description = description;
  
  // Save currently visible cards
  for (const auto& [id, info] : cards_) {
    if (info.visibility_flag && *info.visibility_flag) {
      preset.visible_cards.push_back(id);
    }
  }
  
  presets_[name] = preset;
  SavePresetsToFile();
  
  printf("[EditorCardManager] Saved preset '%s' with %zu cards\n", 
         name.c_str(), preset.visible_cards.size());
}

bool EditorCardManager::LoadPreset(const std::string& name) {
  auto it = presets_.find(name);
  if (it == presets_.end()) {
    return false;
  }
  
  // Hide all cards first
  HideAll();
  
  // Show cards in preset
  for (const auto& card_id : it->second.visible_cards) {
    ShowCard(card_id);
  }
  
  printf("[EditorCardManager] Loaded preset '%s' with %zu cards\n", 
         name.c_str(), it->second.visible_cards.size());
  
  return true;
}

void EditorCardManager::DeletePreset(const std::string& name) {
  auto it = presets_.find(name);
  if (it != presets_.end()) {
    presets_.erase(it);
    SavePresetsToFile();
    printf("[EditorCardManager] Deleted preset '%s'\n", name.c_str());
  }
}

std::vector<EditorCardManager::WorkspacePreset> EditorCardManager::GetPresets() const {
  std::vector<WorkspacePreset> result;
  for (const auto& [name, preset] : presets_) {
    result.push_back(preset);
  }
  return result;
}

void EditorCardManager::ShowAll() {
  for (auto& [id, info] : cards_) {
    if (info.visibility_flag) {
      *info.visibility_flag = true;
      if (info.on_show) info.on_show();
    }
  }
}

void EditorCardManager::HideAll() {
  for (auto& [id, info] : cards_) {
    if (info.visibility_flag) {
      *info.visibility_flag = false;
      if (info.on_hide) info.on_hide();
    }
  }
}

void EditorCardManager::ResetToDefaults() {
  // Default visibility based on priority
  for (auto& [id, info] : cards_) {
    if (info.visibility_flag) {
      // Show high-priority cards (priority < 50)
      *info.visibility_flag = (info.priority < 50);
      
      if (*info.visibility_flag && info.on_show) {
        info.on_show();
      } else if (!*info.visibility_flag && info.on_hide) {
        info.on_hide();
      }
    }
  }
}

size_t EditorCardManager::GetVisibleCardCount() const {
  size_t count = 0;
  for (const auto& [id, info] : cards_) {
    if (info.visibility_flag && *info.visibility_flag) {
      count++;
    }
  }
  return count;
}

void EditorCardManager::SavePresetsToFile() {
  // Save presets to JSON or simple format
  // TODO: Implement file I/O
  printf("[EditorCardManager] Saving %zu presets to file\n", presets_.size());
}

void EditorCardManager::LoadPresetsFromFile() {
  // Load presets from file
  // TODO: Implement file I/O
  printf("[EditorCardManager] Loading presets from file\n");
}

}  // namespace gui
}  // namespace yaze

