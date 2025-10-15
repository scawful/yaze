#include "session_card_registry.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"

namespace yaze {
namespace editor {

void SessionCardRegistry::RegisterSession(size_t session_id) {
  if (session_cards_.find(session_id) == session_cards_.end()) {
    session_cards_[session_id] = std::vector<std::string>();
    session_card_mapping_[session_id] = std::unordered_map<std::string, std::string>();
    UpdateSessionCount();
    printf("[SessionCardRegistry] Registered session %zu (total: %zu)\n", 
           session_id, session_count_);
  }
}

void SessionCardRegistry::UnregisterSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    UnregisterSessionCards(session_id);
    session_cards_.erase(it);
    session_card_mapping_.erase(session_id);
    UpdateSessionCount();
    
    // Reset active session if it was the one being removed
    if (active_session_ == session_id) {
      active_session_ = 0;
      if (!session_cards_.empty()) {
        active_session_ = session_cards_.begin()->first;
      }
    }
    
    printf("[SessionCardRegistry] Unregistered session %zu (total: %zu)\n", 
           session_id, session_count_);
  }
}

void SessionCardRegistry::SetActiveSession(size_t session_id) {
  if (session_cards_.find(session_id) != session_cards_.end()) {
    active_session_ = session_id;
    printf("[SessionCardRegistry] Set active session to %zu\n", session_id);
  }
}

void SessionCardRegistry::RegisterCard(size_t session_id, const gui::CardInfo& base_info) {
  RegisterSession(session_id);  // Ensure session exists
  
  std::string prefixed_id = MakeCardId(session_id, base_info.card_id);
  
  // Create new CardInfo with prefixed ID
  gui::CardInfo prefixed_info = base_info;
  prefixed_info.card_id = prefixed_id;
  
  // Register with the global card manager
  auto& card_mgr = gui::EditorCardManager::Get();
  card_mgr.RegisterCard(prefixed_info);
  
  // Track in our session mapping
  session_cards_[session_id].push_back(prefixed_id);
  session_card_mapping_[session_id][base_info.card_id] = prefixed_id;
  
  printf("[SessionCardRegistry] Registered card %s -> %s for session %zu\n",
         base_info.card_id.c_str(), prefixed_id.c_str(), session_id);
}

void SessionCardRegistry::RegisterCard(size_t session_id,
                                     const std::string& card_id,
                                     const std::string& display_name,
                                     const std::string& icon,
                                     const std::string& category,
                                     const std::string& shortcut_hint,
                                     int priority,
                                     std::function<void()> on_show,
                                     std::function<void()> on_hide,
                                     bool visible_by_default) {
  gui::CardInfo info;
  info.card_id = card_id;
  info.display_name = display_name;
  info.icon = icon;
  info.category = category;
  info.shortcut_hint = shortcut_hint;
  info.priority = priority;
  info.on_show = on_show;
  info.on_hide = on_hide;
  
  RegisterCard(session_id, info);
}

bool SessionCardRegistry::ShowCard(size_t session_id, const std::string& base_card_id) {
  auto& card_mgr = gui::EditorCardManager::Get();
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }
  return card_mgr.ShowCard(prefixed_id);
}

bool SessionCardRegistry::HideCard(size_t session_id, const std::string& base_card_id) {
  auto& card_mgr = gui::EditorCardManager::Get();
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }
  return card_mgr.HideCard(prefixed_id);
}

bool SessionCardRegistry::ToggleCard(size_t session_id, const std::string& base_card_id) {
  auto& card_mgr = gui::EditorCardManager::Get();
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }
  return card_mgr.ToggleCard(prefixed_id);
}

bool SessionCardRegistry::IsCardVisible(size_t session_id, const std::string& base_card_id) const {
  const auto& card_mgr = gui::EditorCardManager::Get();
  std::string prefixed_id = GetPrefixedCardId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }
  return card_mgr.IsCardVisible(prefixed_id);
}

void SessionCardRegistry::ShowAllCardsInSession(size_t session_id) {
  auto& card_mgr = gui::EditorCardManager::Get();
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      card_mgr.ShowCard(card_id);
    }
  }
}

void SessionCardRegistry::HideAllCardsInSession(size_t session_id) {
  auto& card_mgr = gui::EditorCardManager::Get();
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      card_mgr.HideCard(card_id);
    }
  }
}

void SessionCardRegistry::ShowAllCardsInCategory(size_t session_id, const std::string& category) {
  auto& card_mgr = gui::EditorCardManager::Get();
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      const auto* info = card_mgr.GetCardInfo(card_id);
      if (info && info->category == category) {
        card_mgr.ShowCard(card_id);
      }
    }
  }
}

void SessionCardRegistry::HideAllCardsInCategory(size_t session_id, const std::string& category) {
  auto& card_mgr = gui::EditorCardManager::Get();
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      const auto* info = card_mgr.GetCardInfo(card_id);
      if (info && info->category == category) {
        card_mgr.HideCard(card_id);
      }
    }
  }
}

std::string SessionCardRegistry::MakeCardId(size_t session_id, const std::string& base_id) const {
  if (ShouldPrefixCards()) {
    return absl::StrFormat("s%zu.%s", session_id, base_id);
  }
  return base_id;
}

std::vector<std::string> SessionCardRegistry::GetCardsInSession(size_t session_id) const {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    return it->second;
  }
  return {};
}

std::vector<std::string> SessionCardRegistry::GetCardsInCategory(size_t session_id, const std::string& category) const {
  std::vector<std::string> result;
  const auto& card_mgr = gui::EditorCardManager::Get();
  
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      const auto* info = card_mgr.GetCardInfo(card_id);
      if (info && info->category == category) {
        result.push_back(card_id);
      }
    }
  }
  return result;
}

std::vector<std::string> SessionCardRegistry::GetAllCategories(size_t session_id) const {
  std::vector<std::string> categories;
  const auto& card_mgr = gui::EditorCardManager::Get();
  
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      const auto* info = card_mgr.GetCardInfo(card_id);
      if (info) {
        if (std::find(categories.begin(), categories.end(), info->category) == categories.end()) {
          categories.push_back(info->category);
        }
      }
    }
  }
  return categories;
}

void SessionCardRegistry::UpdateSessionCount() {
  session_count_ = session_cards_.size();
}

std::string SessionCardRegistry::GetPrefixedCardId(size_t session_id, const std::string& base_id) const {
  auto session_it = session_card_mapping_.find(session_id);
  if (session_it != session_card_mapping_.end()) {
    auto card_it = session_it->second.find(base_id);
    if (card_it != session_it->second.end()) {
      return card_it->second;
    }
  }
  return "";  // Card not found
}

void SessionCardRegistry::UnregisterSessionCards(size_t session_id) {
  auto& card_mgr = gui::EditorCardManager::Get();
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& card_id : it->second) {
      card_mgr.UnregisterCard(card_id);
    }
  }
}

}  // namespace editor
}  // namespace yaze
