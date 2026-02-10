#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/system/panel_manager.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/events/core_events.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/resource_panel.h"
#include "app/gui/animation/animator.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"  // For ImGuiWindow and FindWindowByName
#include "util/json.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

namespace {

PanelDescriptor BuildDescriptorFromPanel(const EditorPanel& panel) {
  PanelDescriptor descriptor;
  descriptor.card_id = panel.GetId();
  descriptor.display_name = panel.GetDisplayName();
  descriptor.icon = panel.GetIcon();
  descriptor.category = panel.GetEditorCategory();
  descriptor.priority = panel.GetPriority();
  descriptor.shortcut_hint = panel.GetShortcutHint();
  descriptor.scope = panel.GetScope();
  descriptor.panel_category = panel.GetPanelCategory();
  descriptor.context_scope = panel.GetContextScope();
  descriptor.visibility_flag = nullptr;  // Created by RegisterPanel
  descriptor.window_title = panel.GetIcon() + " " + panel.GetDisplayName();
  return descriptor;
}

}  // namespace

// ============================================================================
// Category Icon Mapping
// ============================================================================

std::string PanelManager::GetCategoryIcon(const std::string& category) {
  if (category == "Dungeon")
    return ICON_MD_CASTLE;
  if (category == "Overworld")
    return ICON_MD_MAP;
  if (category == "Graphics")
    return ICON_MD_IMAGE;
  if (category == "Palette")
    return ICON_MD_PALETTE;
  if (category == "Sprite")
    return ICON_MD_PERSON;
  if (category == "Music")
    return ICON_MD_MUSIC_NOTE;
  if (category == "Message")
    return ICON_MD_MESSAGE;
  if (category == "Screen")
    return ICON_MD_TV;
  if (category == "Emulator")
    return ICON_MD_VIDEOGAME_ASSET;
  if (category == "Assembly")
    return ICON_MD_CODE;
  if (category == "Settings")
    return ICON_MD_SETTINGS;
  if (category == "Memory")
    return ICON_MD_MEMORY;
  if (category == "Agent")
    return ICON_MD_SMART_TOY;
  return ICON_MD_FOLDER;  // Default for unknown categories
}

// ============================================================================
// Category Theme Colors (Expressive Icon Theming)
// ============================================================================

PanelManager::CategoryTheme PanelManager::GetCategoryTheme(
    const std::string& category) {
  // Expressive colors for each category - vibrant when active
  // Format: {icon_r, icon_g, icon_b, icon_a, glow_r, glow_g, glow_b}

  if (category == "Dungeon") {
    // Castle gold - warm, regal
    return {0.95f, 0.75f, 0.20f, 1.0f, 0.95f, 0.75f, 0.20f};
  }
  if (category == "Overworld") {
    // Forest green - natural, expansive
    return {0.30f, 0.85f, 0.45f, 1.0f, 0.30f, 0.85f, 0.45f};
  }
  if (category == "Graphics") {
    // Image blue - creative, visual
    return {0.40f, 0.70f, 0.95f, 1.0f, 0.40f, 0.70f, 0.95f};
  }
  if (category == "Palette") {
    // Rainbow pink/magenta - colorful, artistic
    return {0.90f, 0.40f, 0.70f, 1.0f, 0.90f, 0.40f, 0.70f};
  }
  if (category == "Sprite") {
    // Character cyan - lively, animated
    return {0.30f, 0.85f, 0.85f, 1.0f, 0.30f, 0.85f, 0.85f};
  }
  if (category == "Music") {
    // Note purple - creative, rhythmic
    return {0.70f, 0.40f, 0.90f, 1.0f, 0.70f, 0.40f, 0.90f};
  }
  if (category == "Message") {
    // Text yellow - communicative, bright
    return {0.95f, 0.90f, 0.40f, 1.0f, 0.95f, 0.90f, 0.40f};
  }
  if (category == "Screen") {
    // TV white/silver - display, clean
    return {0.90f, 0.92f, 0.95f, 1.0f, 0.90f, 0.92f, 0.95f};
  }
  if (category == "Emulator") {
    // Game red - playful, active
    return {0.90f, 0.35f, 0.40f, 1.0f, 0.90f, 0.35f, 0.40f};
  }
  if (category == "Assembly") {
    // Code green - technical, precise
    return {0.40f, 0.90f, 0.50f, 1.0f, 0.40f, 0.90f, 0.50f};
  }
  if (category == "Settings") {
    // Gear gray/blue - utility, system
    return {0.60f, 0.70f, 0.80f, 1.0f, 0.60f, 0.70f, 0.80f};
  }
  if (category == "Memory") {
    // Memory orange - data, technical
    return {0.95f, 0.60f, 0.25f, 1.0f, 0.95f, 0.60f, 0.25f};
  }
  if (category == "Agent") {
    // AI purple/violet - intelligent, futuristic
    return {0.60f, 0.40f, 0.95f, 1.0f, 0.60f, 0.40f, 0.95f};
  }

  // Default - neutral blue
  return {0.50f, 0.60f, 0.80f, 1.0f, 0.50f, 0.60f, 0.80f};
}

// ============================================================================
// Session Lifecycle Management
// ============================================================================

void PanelManager::RegisterSession(size_t session_id) {
  if (session_cards_.find(session_id) == session_cards_.end()) {
    session_cards_[session_id] = std::vector<std::string>();
    session_card_mapping_[session_id] =
        std::unordered_map<std::string, std::string>();
    session_reverse_card_mapping_[session_id] =
        std::unordered_map<std::string, std::string>();
    UpdateSessionCount();
    LOG_INFO("PanelManager", "Registered session %zu (total: %zu)", session_id,
             session_count_);
  }
}

void PanelManager::UnregisterSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    UnregisterSessionPanels(session_id);
    session_cards_.erase(it);
    session_card_mapping_.erase(session_id);
    session_reverse_card_mapping_.erase(session_id);
    session_context_keys_.erase(session_id);
    UpdateSessionCount();

    // Reset active session if it was the one being removed
    if (active_session_ == session_id) {
      active_session_ = 0;
      if (!session_cards_.empty()) {
        active_session_ = session_cards_.begin()->first;
      }
    }

    LOG_INFO("PanelManager", "Unregistered session %zu (total: %zu)",
             session_id, session_count_);
  }
}

void PanelManager::SetActiveSession(size_t session_id) {
  active_session_ = session_id;
}

// ============================================================================
// Context Keys (Optional Policy Engine)
// ============================================================================

void PanelManager::SetContextKey(size_t session_id, PanelContextScope scope,
                                 std::string key) {
  RegisterSession(session_id);
  auto& session_map = session_context_keys_[session_id];
  const std::string old_key =
      (session_map.find(scope) != session_map.end()) ? session_map[scope] : "";
  if (old_key == key) {
    return;
  }
  session_map[scope] = std::move(key);
  ApplyContextPolicy(session_id, scope, old_key, session_map[scope]);
}

std::string PanelManager::GetContextKey(size_t session_id,
                                       PanelContextScope scope) const {
  auto sit = session_context_keys_.find(session_id);
  if (sit == session_context_keys_.end()) {
    return "";
  }
  const auto& session_map = sit->second;
  auto it = session_map.find(scope);
  if (it == session_map.end()) {
    return "";
  }
  return it->second;
}

void PanelManager::ApplyContextPolicy(size_t session_id, PanelContextScope scope,
                                      const std::string& old_key,
                                      const std::string& new_key) {
  (void)old_key;
  // Conservative default: if a context is cleared, auto-hide panels that were
  // explicitly bound to it (unless pinned). This reduces stale "mystery panels"
  // without being aggressive when context changes (selection A -> selection B).
  if (!new_key.empty()) {
    return;
  }

  auto sit = session_cards_.find(session_id);
  if (sit == session_cards_.end()) {
    return;
  }

  for (const auto& prefixed_id : sit->second) {
    auto dit = cards_.find(prefixed_id);
    if (dit == cards_.end()) {
      continue;
    }
    const PanelDescriptor& desc = dit->second;
    if (desc.context_scope != scope) {
      continue;
    }
    if (!desc.visibility_flag || !*desc.visibility_flag) {
      continue;
    }
    if (IsPanelPinned(prefixed_id)) {
      continue;
    }

    const std::string base_id = GetBaseIdForPrefixedId(session_id, prefixed_id);
    if (!base_id.empty()) {
      (void)HidePanel(session_id, base_id);
    }
  }
}

std::string PanelManager::GetBaseIdForPrefixedId(
    size_t session_id, const std::string& prefixed_id) const {
  auto sit = session_reverse_card_mapping_.find(session_id);
  if (sit == session_reverse_card_mapping_.end()) {
    return "";
  }
  const auto& reverse = sit->second;
  auto it = reverse.find(prefixed_id);
  if (it == reverse.end()) {
    return "";
  }
  return it->second;
}

// ============================================================================
// Panel Registration
// ============================================================================

void PanelManager::RegisterPanel(size_t session_id,
                                 const PanelDescriptor& base_info) {
  RegisterSession(session_id);  // Ensure session exists

  std::string panel_id =
      MakePanelId(session_id, base_info.card_id, base_info.scope);

  bool already_registered = (cards_.find(panel_id) != cards_.end());
  if (already_registered && base_info.scope != PanelScope::kGlobal) {
    LOG_WARN("PanelManager",
             "Panel '%s' already registered, skipping duplicate",
             panel_id.c_str());
  }

  if (!already_registered) {
    // Create new PanelDescriptor with final ID
    PanelDescriptor panel_info = base_info;
    panel_info.card_id = panel_id;

    // If no visibility_flag provided, create centralized one
    if (!panel_info.visibility_flag) {
      centralized_visibility_[panel_id] = false;  // Hidden by default
      panel_info.visibility_flag = &centralized_visibility_[panel_id];
    }

    // Register the card
    cards_[panel_id] = panel_info;

    LOG_INFO("PanelManager", "Registered card %s -> %s for session %zu",
             base_info.card_id.c_str(), panel_id.c_str(), session_id);
  }

  if (base_info.scope == PanelScope::kGlobal) {
    global_panel_ids_.insert(panel_id);
  }

  TrackPanelForSession(session_id, base_info.card_id, panel_id);
}

void PanelManager::RegisterPanel(size_t session_id, const std::string& card_id,
                                 const std::string& display_name,
                                 const std::string& icon,
                                 const std::string& category,
                                 const std::string& shortcut_hint, int priority,
                                 std::function<void()> on_show,
                                 std::function<void()> on_hide,
                                 bool visible_by_default) {
  PanelDescriptor info;
  info.card_id = card_id;
  info.display_name = display_name;
  info.icon = icon;
  info.category = category;
  info.shortcut_hint = shortcut_hint;
  info.priority = priority;
  info.visibility_flag = nullptr;  // Will be created in RegisterPanel
  info.on_show = on_show;
  info.on_hide = on_hide;

  RegisterPanel(session_id, info);

  // Set initial visibility if requested
  if (visible_by_default) {
    ShowPanel(session_id, card_id);
  }
}

void PanelManager::UnregisterPanel(size_t session_id,
                                   const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    LOG_INFO("PanelManager", "Unregistered card: %s", prefixed_id.c_str());
    cards_.erase(it);
    centralized_visibility_.erase(prefixed_id);
    pinned_panels_.erase(prefixed_id);
    if (global_panel_ids_.find(prefixed_id) != global_panel_ids_.end()) {
      global_panel_ids_.erase(prefixed_id);
      for (auto& [mapped_session, card_list] : session_cards_) {
        card_list.erase(std::remove(card_list.begin(), card_list.end(),
                                    prefixed_id),
                        card_list.end());
      }
      for (auto& [mapped_session, mapping] : session_card_mapping_) {
        mapping.erase(base_card_id);
      }
      return;
    }

    // Remove from session tracking
    auto& session_card_list = session_cards_[session_id];
    session_card_list.erase(std::remove(session_card_list.begin(),
                                        session_card_list.end(), prefixed_id),
                            session_card_list.end());

    session_card_mapping_[session_id].erase(base_card_id);
  }
}

void PanelManager::UnregisterPanelsWithPrefix(const std::string& prefix) {
  std::vector<std::string> to_remove;

  // Find all cards with the given prefix
  for (const auto& [card_id, card_info] : cards_) {
    if (card_id.find(prefix) == 0) {  // Starts with prefix
      to_remove.push_back(card_id);
    }
  }

  // Remove them
  for (const auto& card_id : to_remove) {
    cards_.erase(card_id);
    centralized_visibility_.erase(card_id);
    pinned_panels_.erase(card_id);
    LOG_INFO("PanelManager", "Unregistered card with prefix '%s': %s",
             prefix.c_str(), card_id.c_str());
  }

  // Also clean up session tracking
  for (auto& [session_id, card_list] : session_cards_) {
    card_list.erase(std::remove_if(card_list.begin(), card_list.end(),
                                   [&prefix](const std::string& id) {
                                     return id.find(prefix) == 0;
                                   }),
                    card_list.end());
  }
}

void PanelManager::ClearAllPanels() {
  cards_.clear();
  centralized_visibility_.clear();
  pinned_panels_.clear();
  session_cards_.clear();
  session_card_mapping_.clear();
  session_reverse_card_mapping_.clear();
  session_context_keys_.clear();
  panel_instances_.clear();
  registry_panel_ids_.clear();
  global_panel_ids_.clear();
  session_count_ = 0;
  LOG_INFO("PanelManager", "Cleared all cards");
}

// ============================================================================
// EditorPanel Instance Management (Phase 4)
// ============================================================================

void PanelManager::RegisterRegistryPanel(std::unique_ptr<EditorPanel> panel) {
  if (!panel) {
    LOG_ERROR("PanelManager", "Attempted to register null EditorPanel");
    return;
  }

  // Phase 6: Resource Panel Limits
  auto* resource_panel = dynamic_cast<ResourcePanel*>(panel.get());
  if (resource_panel) {
    EnforceResourceLimits(resource_panel->GetResourceType());
  }

  std::string panel_id = panel->GetId();

  // Check if already registered
  if (panel_instances_.find(panel_id) != panel_instances_.end()) {
    LOG_WARN("PanelManager",
             "EditorPanel '%s' already registered, skipping registry add",
             panel_id.c_str());
    return;
  }

  if (panel->GetScope() == PanelScope::kGlobal) {
    global_panel_ids_.insert(panel_id);
  }
  registry_panel_ids_.insert(panel_id);

  // Store the EditorPanel instance
  panel_instances_[panel_id] = std::move(panel);

  // Phase 6: Track resource panel usage
  if (resource_panel) {
    std::string type = resource_panel->GetResourceType();
    resource_panels_[type].push_back(panel_id);
    panel_resource_types_[panel_id] = type;
  }

  LOG_INFO("PanelManager", "Registered registry EditorPanel: %s",
           panel_id.c_str());
}

void PanelManager::RegisterRegistryPanelsForSession(size_t session_id) {
  RegisterSession(session_id);
  for (const auto& panel_id : registry_panel_ids_) {
    auto it = panel_instances_.find(panel_id);
    if (it == panel_instances_.end()) {
      continue;
    }
    RegisterPanelDescriptorForSession(session_id, *it->second);
  }
}

void PanelManager::RegisterEditorPanel(std::unique_ptr<EditorPanel> panel) {
  if (!panel) {
    LOG_ERROR("PanelManager", "Attempted to register null EditorPanel");
    return;
  }

  // Phase 6: Resource Panel Limits
  auto* resource_panel = dynamic_cast<ResourcePanel*>(panel.get());
  if (resource_panel) {
    EnforceResourceLimits(resource_panel->GetResourceType());
  }

  std::string panel_id = panel->GetId();

  // Check if already registered
  if (panel_instances_.find(panel_id) != panel_instances_.end()) {
    LOG_WARN("PanelManager", "EditorPanel '%s' already registered, skipping",
             panel_id.c_str());
    return;
  }

  // Auto-register PanelDescriptor for sidebar/menu visibility
  PanelDescriptor descriptor = BuildDescriptorFromPanel(*panel);

  // Check if panel should be visible by default
  bool visible_by_default = panel->IsVisibleByDefault();

  // Register the descriptor (creates visibility flag)
  RegisterPanel(active_session_, descriptor);

  // Set initial visibility if panel should be visible by default
  if (visible_by_default) {
    ShowPanel(active_session_, panel_id);
  }

  // Store the EditorPanel instance
  panel_instances_[panel_id] = std::move(panel);

  // Phase 6: Track resource panel usage
  if (resource_panel) {
    std::string type = resource_panel->GetResourceType();
    resource_panels_[type].push_back(panel_id);
    panel_resource_types_[panel_id] = type;
  }

  LOG_INFO("PanelManager", "Registered EditorPanel: %s (%s)", panel_id.c_str(),
           descriptor.display_name.c_str());
}

// ============================================================================
// Resource Management (Phase 6)
// ============================================================================

void PanelManager::EnforceResourceLimits(const std::string& resource_type) {
  auto it = resource_panels_.find(resource_type);
  if (it == resource_panels_.end())
    return;

  auto& panel_list = it->second;
  size_t limit =
      ResourcePanelLimits::kMaxTotalResourcePanels;  // Default fallback

  // Determine limit based on type
  if (resource_type == "room")
    limit = ResourcePanelLimits::kMaxRoomPanels;
  else if (resource_type == "song")
    limit = ResourcePanelLimits::kMaxSongPanels;
  else if (resource_type == "sheet")
    limit = ResourcePanelLimits::kMaxSheetPanels;
  else if (resource_type == "map")
    limit = ResourcePanelLimits::kMaxMapPanels;

  // Evict panels until we have room for one more (current count < limit)
  // Prioritize evicting non-pinned panels first, then oldest pinned ones
  while (panel_list.size() >= limit) {
    // First pass: find oldest non-pinned panel
    std::string panel_to_evict;
    for (const auto& panel_id : panel_list) {
      if (!IsPanelPinned(panel_id)) {
        panel_to_evict = panel_id;
        break;
      }
    }

    // If all are pinned, evict the oldest (front of list) anyway
    if (panel_to_evict.empty()) {
      panel_to_evict = panel_list.front();
      LOG_INFO("PanelManager", "All %s panels pinned, evicting oldest: %s",
               resource_type.c_str(), panel_to_evict.c_str());
    } else {
      LOG_INFO("PanelManager",
               "Evicting non-pinned resource panel: %s (type: %s)",
               panel_to_evict.c_str(), resource_type.c_str());
    }

    // Remove from LRU list first to avoid iterator issues
    panel_list.remove(panel_to_evict);

    UnregisterEditorPanel(panel_to_evict);
  }
}

void PanelManager::MarkPanelUsed(const std::string& panel_id) {
  auto type_it = panel_resource_types_.find(panel_id);
  if (type_it == panel_resource_types_.end())
    return;

  std::string type = type_it->second;
  auto& list = resource_panels_[type];

  // Move to back (MRU)
  // std::list::remove is slow (linear), but list size is small (<10)
  list.remove(panel_id);
  list.push_back(panel_id);
}

void PanelManager::UnregisterEditorPanel(const std::string& panel_id) {
  auto it = panel_instances_.find(panel_id);
  if (it != panel_instances_.end()) {
    // Call OnClose before removing
    it->second->OnClose();
    gui::GetAnimator().ClearAnimationsForPanel(panel_id);
    panel_instances_.erase(it);
    registry_panel_ids_.erase(panel_id);
    global_panel_ids_.erase(panel_id);
    LOG_INFO("PanelManager", "Unregistered EditorPanel: %s", panel_id.c_str());
  }

  // Also unregister the descriptor
  UnregisterPanel(active_session_, panel_id);
}

EditorPanel* PanelManager::GetEditorPanel(const std::string& panel_id) {
  auto it = panel_instances_.find(panel_id);
  if (it != panel_instances_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void PanelManager::DrawAllVisiblePanels() {
  // Suppress panel drawing when dashboard is active (no editor selected yet)
  // This ensures panels don't appear until user selects an editor
  if (active_category_.empty() || active_category_ == kDashboardCategory) {
    return;
  }

  auto& animator = gui::GetAnimator();
  bool animations_enabled = animator.IsEnabled();
  const bool touch_device = gui::LayoutHelpers::IsTouchDevice();

  for (auto& [panel_id, panel] : panel_instances_) {
    // Check visibility via PanelDescriptor
    bool is_visible = IsPanelVisible(panel_id);

    // Category filtering: only draw if matches active category, pinned, or persistent
    bool should_draw = false;
    if (is_visible) {
      if (panel->GetEditorCategory() == active_category_) {
        should_draw = true;
      } else if (IsPanelPinned(panel_id)) {
        should_draw = true;
      } else if (panel->GetPanelCategory() == PanelCategory::Persistent) {
        should_draw = true;
      }
    }

    // Compute target alpha: 1.0 if should draw, 0.0 if should hide
    float target_alpha = should_draw ? 1.0f : 0.0f;

    // Animate alpha towards target (or snap if animations disabled)
    float current_alpha = 1.0f;
    if (animations_enabled) {
      current_alpha = animator.Animate(panel_id, "panel_alpha", target_alpha, 8.0f);
    } else {
      current_alpha = target_alpha;
    }

    // Skip drawing if alpha is effectively zero
    if (current_alpha < 0.01f) {
      continue;
    }

    // Get visibility flag for the panel window
    bool* visibility_flag = GetVisibilityFlag(panel_id);

    // Get display name without icon - PanelWindow will add the icon
    // This fixes the double-icon issue where both descriptor and PanelWindow added icons
    std::string display_name = panel->GetDisplayName();

    if (editor_resolver_) {
      if (Editor* editor = editor_resolver_(panel->GetEditorCategory())) {
        ContentRegistry::Context::SetCurrentEditor(editor);
      }
    }

    // Create PanelWindow and draw content
    gui::PanelWindow window(display_name.c_str(), panel->GetIcon().c_str(),
                            visibility_flag);
    window.SetStableId(panel_id);
    if (touch_device) {
      window.SetPosition(gui::PanelWindow::Position::Center);
    }

    // Use preferred width from EditorPanel if specified
    float preferred_width = panel->GetPreferredWidth();
    if (preferred_width > 0.0f) {
      window.SetDefaultSize(preferred_width, 0);  // 0 height = auto
    }

    // Enable pin functionality for cross-editor persistence
    window.SetPinnable(true);
    window.SetPinned(IsPanelPinned(panel_id));

    // Wire up pin state change callback to persist to PanelManager
    window.SetPinChangedCallback(
        [this, panel_id](bool pinned) { SetPanelPinned(panel_id, pinned); });

    // Apply fade alpha for smooth transitions
    std::optional<gui::StyleVarGuard> alpha_guard;
    if (current_alpha < 1.0f) {
      alpha_guard.emplace(ImGuiStyleVar_Alpha, current_alpha);
    }

    if (window.Begin(visibility_flag)) {
      panel->DrawWithLazyInit(visibility_flag);
    }
    window.End();

    alpha_guard.reset();

    // Handle visibility change (window closed via X button)
    if (visibility_flag && !*visibility_flag) {
      panel->OnClose();
    }
  }
}

void PanelManager::OnEditorSwitch(const std::string& from_category,
                                  const std::string& to_category) {
  if (from_category == to_category) {
    return;  // No switch needed
  }

  LOG_INFO("PanelManager", "Switching from category '%s' to '%s'",
           from_category.c_str(), to_category.c_str());

  // IMPORTANT:
  // Editor switching must *not* be treated as a user-initiated hide/show.
  //
  // Historically this function toggled visibility flags (HidePanel/ShowPanel),
  // which publishes PanelVisibilityChangedEvent and permanently overwrote user
  // prefs. Worse, dynamic "resource windows" (rooms/songs) interpret
  // IsPanelVisible()==false as "user closed", unregistering themselves.
  //
  // Visibility persistence is handled by EditorManager's category-changed
  // callback (RestoreVisibilityState / default policy). Drawing is filtered by
  // active category + pinned/persistent state in DrawAllVisiblePanels().
  SetActiveCategory(to_category);
}

// ============================================================================
// Panel Control (Programmatic, No GUI)
// ============================================================================

bool PanelManager::ShowPanel(size_t session_id,
                             const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    const bool was_visible =
        (it->second.visibility_flag && *it->second.visibility_flag);
    if (it->second.visibility_flag) {
      *it->second.visibility_flag = true;
    }
    if (it->second.on_show) {
      it->second.on_show();
    }
    if (!was_visible) {
      auto pit = panel_instances_.find(prefixed_id);
      if (pit != panel_instances_.end() && pit->second) {
        pit->second->OnOpen();
      }
    }

    // Publish visibility changed event
    if (auto* bus = ContentRegistry::Context::event_bus()) {
      bus->Publish(PanelVisibilityChangedEvent::Create(
          prefixed_id, base_card_id, it->second.category, true, session_id));
    }
    return true;
  }
  return false;
}

bool PanelManager::HidePanel(size_t session_id,
                             const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    const bool was_visible =
        (it->second.visibility_flag && *it->second.visibility_flag);
    if (it->second.visibility_flag) {
      *it->second.visibility_flag = false;
    }
    if (it->second.on_hide) {
      it->second.on_hide();
    }
    if (was_visible) {
      auto pit = panel_instances_.find(prefixed_id);
      if (pit != panel_instances_.end() && pit->second) {
        pit->second->OnClose();
      }
    }

    // Publish visibility changed event
    if (auto* bus = ContentRegistry::Context::event_bus()) {
      bus->Publish(PanelVisibilityChangedEvent::Create(
          prefixed_id, base_card_id, it->second.category, false, session_id));
    }
    return true;
  }
  return false;
}

bool PanelManager::TogglePanel(size_t session_id,
                               const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end() && it->second.visibility_flag) {
    bool new_state = !(*it->second.visibility_flag);
    *it->second.visibility_flag = new_state;

    if (new_state && it->second.on_show) {
      it->second.on_show();
    } else if (!new_state && it->second.on_hide) {
      it->second.on_hide();
    }
    auto pit = panel_instances_.find(prefixed_id);
    if (pit != panel_instances_.end() && pit->second) {
      if (new_state) {
        pit->second->OnOpen();
      } else {
        pit->second->OnClose();
      }
    }

    // Publish visibility changed event
    if (auto* bus = ContentRegistry::Context::event_bus()) {
      bus->Publish(PanelVisibilityChangedEvent::Create(
          prefixed_id, base_card_id, it->second.category, new_state,
          session_id));
    }
    return true;
  }
  return false;
}

bool PanelManager::IsPanelVisible(size_t session_id,
                                  const std::string& base_card_id) const {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end() && it->second.visibility_flag) {
    return *it->second.visibility_flag;
  }
  return false;
}

bool* PanelManager::GetVisibilityFlag(size_t session_id,
                                      const std::string& base_card_id) {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return nullptr;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    return it->second.visibility_flag;
  }
  return nullptr;
}

// ============================================================================
// Batch Operations
// ============================================================================

void PanelManager::ShowAllPanelsInSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        *card_it->second.visibility_flag = true;
        if (card_it->second.on_show) {
          card_it->second.on_show();
        }
      }
    }
  }
}

void PanelManager::HideAllPanelsInSession(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        *card_it->second.visibility_flag = false;
        if (card_it->second.on_hide) {
          card_it->second.on_hide();
        }
      }
    }
  }
}

void PanelManager::ShowAllPanelsInCategory(size_t session_id,
                                           const std::string& category) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.category == category) {
        if (card_it->second.visibility_flag) {
          *card_it->second.visibility_flag = true;
        }
        if (card_it->second.on_show) {
          card_it->second.on_show();
        }
      }
    }
  }
}

void PanelManager::HideAllPanelsInCategory(size_t session_id,
                                           const std::string& category) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.category == category) {
        if (card_it->second.visibility_flag) {
          *card_it->second.visibility_flag = false;
        }
        if (card_it->second.on_hide) {
          card_it->second.on_hide();
        }
      }
    }
  }
}

void PanelManager::ShowOnlyPanel(size_t session_id,
                                 const std::string& base_card_id) {
  // First get the category of the target card
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return;
  }

  auto target_it = cards_.find(prefixed_id);
  if (target_it == cards_.end()) {
    return;
  }

  std::string category = target_it->second.category;

  // Hide all cards in the same category
  HideAllPanelsInCategory(session_id, category);

  // Show the target card
  ShowPanel(session_id, base_card_id);
}

// ============================================================================
// Query Methods
// ============================================================================

std::vector<std::string> PanelManager::GetPanelsInSession(
    size_t session_id) const {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    return it->second;
  }
  return {};
}

std::vector<PanelDescriptor> PanelManager::GetPanelsInCategory(
    size_t session_id, const std::string& category) const {
  std::vector<PanelDescriptor> result;

  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.category == category) {
        result.push_back(card_it->second);
      }
    }
  }

  // Sort by priority
  std::sort(result.begin(), result.end(),
            [](const PanelDescriptor& a, const PanelDescriptor& b) {
              return a.priority < b.priority;
            });

  return result;
}

std::vector<std::string> PanelManager::GetAllCategories(
    size_t session_id) const {
  std::vector<std::string> categories;

  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end()) {
        if (std::find(categories.begin(), categories.end(),
                      card_it->second.category) == categories.end()) {
          categories.push_back(card_it->second.category);
        }
      }
    }
  }
  return categories;
}

const PanelDescriptor* PanelManager::GetPanelDescriptor(
    size_t session_id, const std::string& base_card_id) const {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return nullptr;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    return &it->second;
  }
  return nullptr;
}

std::vector<std::string> PanelManager::GetAllCategories() const {
  std::vector<std::string> categories;
  for (const auto& [card_id, card_info] : cards_) {
    if (std::find(categories.begin(), categories.end(), card_info.category) ==
        categories.end()) {
      categories.push_back(card_info.category);
    }
  }
  return categories;
}

// ============================================================================
// State Persistence
// ============================================================================

std::vector<std::string> PanelManager::GetVisiblePanelIds(
    size_t session_id) const {
  std::vector<std::string> visible_panels;

  auto session_it = session_card_mapping_.find(session_id);
  if (session_it == session_card_mapping_.end()) {
    return visible_panels;
  }

  for (const auto& [base_id, prefixed_id] : session_it->second) {
    auto card_it = cards_.find(prefixed_id);
    if (card_it != cards_.end() && card_it->second.visibility_flag &&
        *card_it->second.visibility_flag) {
      visible_panels.push_back(base_id);
    }
  }

  return visible_panels;
}

void PanelManager::SetVisiblePanels(
    size_t session_id, const std::vector<std::string>& panel_ids) {
  auto session_it = session_card_mapping_.find(session_id);
  if (session_it == session_card_mapping_.end()) {
    return;
  }

  // Convert panel_ids to a set for O(1) lookup
  std::unordered_set<std::string> visible_set(panel_ids.begin(),
                                               panel_ids.end());

  // Update visibility for all panels in this session
  for (const auto& [base_id, prefixed_id] : session_it->second) {
    auto card_it = cards_.find(prefixed_id);
    if (card_it != cards_.end() && card_it->second.visibility_flag) {
      bool should_be_visible = visible_set.count(base_id) > 0;
      *card_it->second.visibility_flag = should_be_visible;
    }
  }

  LOG_INFO("PanelManager", "Set %zu panels visible for session %zu",
           panel_ids.size(), session_id);
}

std::unordered_map<std::string, bool> PanelManager::SerializeVisibilityState(
    size_t session_id) const {
  std::unordered_map<std::string, bool> state;

  auto session_it = session_card_mapping_.find(session_id);
  if (session_it == session_card_mapping_.end()) {
    return state;
  }

  for (const auto& [base_id, prefixed_id] : session_it->second) {
    auto card_it = cards_.find(prefixed_id);
    if (card_it != cards_.end() && card_it->second.visibility_flag) {
      state[base_id] = *card_it->second.visibility_flag;
    }
  }

  return state;
}

void PanelManager::RestoreVisibilityState(
    size_t session_id, const std::unordered_map<std::string, bool>& state,
    bool publish_events) {
  auto session_it = session_card_mapping_.find(session_id);
  if (session_it == session_card_mapping_.end()) {
    LOG_WARN("PanelManager",
             "Cannot restore visibility: session %zu not found", session_id);
    return;
  }

  size_t restored = 0;
  for (const auto& [base_id, visible] : state) {
    auto mapping_it = session_it->second.find(base_id);
    if (mapping_it != session_it->second.end()) {
      auto card_it = cards_.find(mapping_it->second);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        *card_it->second.visibility_flag = visible;
        if (publish_events) {
          if (auto* bus = ContentRegistry::Context::event_bus()) {
            bus->Publish(PanelVisibilityChangedEvent::Create(
                mapping_it->second, base_id, card_it->second.category, visible,
                session_id));
          }
        }
        restored++;
      }
    }
  }

  LOG_INFO("PanelManager",
           "Restored visibility for %zu/%zu panels in session %zu", restored,
           state.size(), session_id);
}

std::unordered_map<std::string, bool> PanelManager::SerializePinnedState()
    const {
  std::unordered_map<std::string, bool> state;

  for (const auto& [prefixed_id, pinned] : pinned_panels_) {
    // Extract base ID by removing session prefix (format: "s{n}_{base_id}")
    size_t underscore_pos = prefixed_id.find('_');
    if (underscore_pos != std::string::npos) {
      std::string base_id = prefixed_id.substr(underscore_pos + 1);
      state[base_id] = pinned;
    }
  }

  return state;
}

void PanelManager::RestorePinnedState(
    const std::unordered_map<std::string, bool>& state) {
  // Apply pinned state to all sessions
  for (const auto& [session_id, card_mapping] : session_card_mapping_) {
    for (const auto& [base_id, prefixed_id] : card_mapping) {
      auto state_it = state.find(base_id);
      if (state_it != state.end()) {
        pinned_panels_[prefixed_id] = state_it->second;
      }
    }
  }

  LOG_INFO("PanelManager", "Restored pinned state for %zu panels",
           state.size());
}

// ============================================================================
// Sidebar Keyboard Navigation
// ============================================================================

void PanelManager::HandleSidebarKeyboardNav(
    size_t session_id, const std::vector<PanelDescriptor>& cards) {
  // Click to focus - only focus if sidebar window is hovered and mouse clicked
  if (!sidebar_has_focus_ && ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    sidebar_has_focus_ = true;
    focused_card_index_ = cards.empty() ? -1 : 0;
  }

  // No navigation if not focused or no cards
  if (!sidebar_has_focus_ || cards.empty()) {
    return;
  }

  // Escape to unfocus
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    sidebar_has_focus_ = false;
    focused_card_index_ = -1;
    return;
  }

  int card_count = static_cast<int>(cards.size());

  // Arrow keys / vim keys navigation
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_J)) {
    focused_card_index_ = std::min(focused_card_index_ + 1, card_count - 1);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
      ImGui::IsKeyPressed(ImGuiKey_K)) {
    focused_card_index_ = std::max(focused_card_index_ - 1, 0);
  }

  // Home/End for quick navigation
  if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
    focused_card_index_ = 0;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_End)) {
    focused_card_index_ = card_count - 1;
  }

  // Enter/Space to toggle card visibility
  if (focused_card_index_ >= 0 && focused_card_index_ < card_count) {
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
        ImGui::IsKeyPressed(ImGuiKey_Space)) {
      const auto& card = cards[focused_card_index_];
      TogglePanel(session_id, card.card_id);
    }
  }
}

// ============================================================================
// Favorites and Recent
// ============================================================================

void PanelManager::ToggleFavorite(const std::string& card_id) {
  if (favorite_cards_.find(card_id) != favorite_cards_.end()) {
    favorite_cards_.erase(card_id);
  } else {
    favorite_cards_.insert(card_id);
  }
  // TODO: Persist favorites to user settings
}

bool PanelManager::IsFavorite(const std::string& card_id) const {
  return favorite_cards_.find(card_id) != favorite_cards_.end();
}

void PanelManager::AddToRecent(const std::string& card_id) {
  // Remove if already exists (to move to front)
  auto it = std::find(recent_cards_.begin(), recent_cards_.end(), card_id);
  if (it != recent_cards_.end()) {
    recent_cards_.erase(it);
  }

  // Add to front
  recent_cards_.insert(recent_cards_.begin(), card_id);

  // Trim if needed
  if (recent_cards_.size() > kMaxRecentPanels) {
    recent_cards_.resize(kMaxRecentPanels);
  }
}

// ============================================================================
// Workspace Presets
// ============================================================================

void PanelManager::SavePreset(const std::string& name,
                              const std::string& description) {
  WorkspacePreset preset;
  preset.name = name;
  preset.description = description;

  // Collect all visible cards across all sessions
  for (const auto& [card_id, card_info] : cards_) {
    if (card_info.visibility_flag && *card_info.visibility_flag) {
      preset.visible_cards.push_back(card_id);
    }
  }

  presets_[name] = preset;
  SavePresetsToFile();
  LOG_INFO("PanelManager", "Saved preset: %s (%zu cards)", name.c_str(),
           preset.visible_cards.size());
}

bool PanelManager::LoadPreset(const std::string& name) {
  auto it = presets_.find(name);
  if (it == presets_.end()) {
    return false;
  }

  // First hide all cards
  for (auto& [card_id, card_info] : cards_) {
    if (card_info.visibility_flag) {
      *card_info.visibility_flag = false;
    }
  }

  // Then show preset cards
  for (const auto& card_id : it->second.visible_cards) {
    auto card_it = cards_.find(card_id);
    if (card_it != cards_.end() && card_it->second.visibility_flag) {
      *card_it->second.visibility_flag = true;
      if (card_it->second.on_show) {
        card_it->second.on_show();
      }
    }
  }

  LOG_INFO("PanelManager", "Loaded preset: %s", name.c_str());
  return true;
}

void PanelManager::DeletePreset(const std::string& name) {
  presets_.erase(name);
  SavePresetsToFile();
}

std::vector<PanelManager::WorkspacePreset> PanelManager::GetPresets() const {
  std::vector<WorkspacePreset> result;
  for (const auto& [name, preset] : presets_) {
    result.push_back(preset);
  }
  return result;
}

// ============================================================================
// Quick Actions
// ============================================================================

void PanelManager::ShowAll(size_t session_id) {
  ShowAllPanelsInSession(session_id);
}

void PanelManager::HideAll(size_t session_id) {
  HideAllPanelsInSession(session_id);
}

void PanelManager::ResetToDefaults(size_t session_id) {
  // Hide all cards first
  HideAllPanelsInSession(session_id);

  // TODO: Load default visibility from config file or hardcoded defaults
  LOG_INFO("PanelManager", "Reset to defaults for session %zu", session_id);
}

void PanelManager::ResetToDefaults(size_t session_id, EditorType editor_type) {
  // Get category for this editor
  std::string category = EditorRegistry::GetEditorCategory(editor_type);
  if (category.empty()) {
    LOG_WARN("PanelManager",
             "No category found for editor type %d, skipping reset",
             static_cast<int>(editor_type));
    return;
  }

  // Hide all cards in this category first
  HideAllPanelsInCategory(session_id, category);

  // Get default cards from LayoutPresets
  auto default_panels = LayoutPresets::GetDefaultPanels(editor_type);

  // Show each default card
  for (const auto& card_id : default_panels) {
    if (ShowPanel(session_id, card_id)) {
      LOG_INFO("PanelManager", "Showing default card: %s", card_id.c_str());
    }
  }

  LOG_INFO("PanelManager", "Reset %s editor to defaults (%zu cards visible)",
           category.c_str(), default_panels.size());
}

// ============================================================================
// Statistics
// ============================================================================

size_t PanelManager::GetVisiblePanelCount(size_t session_id) const {
  size_t count = 0;
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      auto card_it = cards_.find(prefixed_card_id);
      if (card_it != cards_.end() && card_it->second.visibility_flag) {
        if (*card_it->second.visibility_flag) {
          count++;
        }
      }
    }
  }
  return count;
}

// ============================================================================
// Session Prefixing Utilities
// ============================================================================

std::string PanelManager::MakePanelId(size_t session_id,
                                      const std::string& base_id) const {
  return MakePanelId(session_id, base_id, PanelScope::kSession);
}

std::string PanelManager::MakePanelId(size_t session_id,
                                      const std::string& base_id,
                                      PanelScope scope) const {
  if (scope == PanelScope::kGlobal) {
    return base_id;
  }
  if (ShouldPrefixPanels()) {
    return absl::StrFormat("s%zu.%s", session_id, base_id);
  }
  return base_id;
}

// ============================================================================
// Helper Methods (Private)
// ============================================================================

void PanelManager::UpdateSessionCount() {
  session_count_ = session_cards_.size();
}

std::string PanelManager::GetPrefixedPanelId(size_t session_id,
                                             const std::string& base_id) const {
  auto session_it = session_card_mapping_.find(session_id);
  if (session_it != session_card_mapping_.end()) {
    auto card_it = session_it->second.find(base_id);
    if (card_it != session_it->second.end()) {
      return card_it->second;
    }
  }

  // Fallback: try unprefixed ID (for single session or direct access)
  if (cards_.find(base_id) != cards_.end()) {
    return base_id;
  }

  return "";  // Panel not found
}

void PanelManager::RegisterPanelDescriptorForSession(
    size_t session_id, const EditorPanel& panel) {
  RegisterSession(session_id);
  std::string panel_id =
      MakePanelId(session_id, panel.GetId(), panel.GetScope());
  bool already_registered = (cards_.find(panel_id) != cards_.end());
  PanelDescriptor descriptor = BuildDescriptorFromPanel(panel);
  RegisterPanel(session_id, descriptor);
  if (!already_registered && panel.IsVisibleByDefault()) {
    ShowPanel(session_id, panel.GetId());
  }
}

void PanelManager::TrackPanelForSession(size_t session_id,
                                        const std::string& base_id,
                                        const std::string& panel_id) {
  auto& card_list = session_cards_[session_id];
  if (std::find(card_list.begin(), card_list.end(), panel_id) ==
      card_list.end()) {
    card_list.push_back(panel_id);
  }
  session_card_mapping_[session_id][base_id] = panel_id;
  session_reverse_card_mapping_[session_id][panel_id] = base_id;
}

void PanelManager::UnregisterSessionPanels(size_t session_id) {
  auto it = session_cards_.find(session_id);
  if (it != session_cards_.end()) {
    for (const auto& prefixed_card_id : it->second) {
      if (global_panel_ids_.find(prefixed_card_id) != global_panel_ids_.end()) {
        continue;
      }
      cards_.erase(prefixed_card_id);
      centralized_visibility_.erase(prefixed_card_id);
      pinned_panels_.erase(prefixed_card_id);
    }
  }
}

void PanelManager::SavePresetsToFile() {
  auto config_dir_result = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_result.ok()) {
    LOG_ERROR("PanelManager", "Failed to get config directory: %s",
              config_dir_result.status().ToString().c_str());
    return;
  }

  std::filesystem::path presets_file =
      *config_dir_result / "layout_presets.json";

  try {
    yaze::Json j;
    j["version"] = 1;
    j["presets"] = yaze::Json::object();

    for (const auto& [name, preset] : presets_) {
      yaze::Json preset_json;
      preset_json["name"] = preset.name;
      preset_json["description"] = preset.description;
      preset_json["visible_cards"] = preset.visible_cards;
      j["presets"][name] = preset_json;
    }

    std::ofstream file(presets_file);
    if (!file.is_open()) {
      LOG_ERROR("PanelManager", "Failed to open file for writing: %s",
                presets_file.string().c_str());
      return;
    }

    file << j.dump(2);
    file.close();

    LOG_INFO("PanelManager", "Saved %zu presets to %s", presets_.size(),
             presets_file.string().c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("PanelManager", "Error saving presets: %s", e.what());
  }
}

void PanelManager::LoadPresetsFromFile() {
  auto config_dir_result = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_result.ok()) {
    LOG_WARN("PanelManager", "Failed to get config directory: %s",
             config_dir_result.status().ToString().c_str());
    return;
  }

  std::filesystem::path presets_file =
      *config_dir_result / "layout_presets.json";

  if (!util::PlatformPaths::Exists(presets_file)) {
    LOG_INFO("PanelManager", "No presets file found at %s",
             presets_file.string().c_str());
    return;
  }

  try {
    std::ifstream file(presets_file);
    if (!file.is_open()) {
      LOG_WARN("PanelManager", "Failed to open presets file: %s",
               presets_file.string().c_str());
      return;
    }

    yaze::Json j;
    file >> j;
    file.close();

    if (!j.contains("presets")) {
      LOG_WARN("PanelManager", "Invalid presets file format");
      return;
    }

    size_t loaded_count = 0;
    // Note: iterating over yaze::Json or nlohmann::json requires standard loop if using alias
    // However, yaze::Json alias is just nlohmann::json when enabled.
    // When disabled, the loop will just not execute or stub loop.
    // But wait, nlohmann::json iterators return key/value pair or special iterator.
    // Let's check how the loop was written: for (auto& [name, preset_json] : j["presets"].items())
    // My stub has items(), but nlohmann::json uses items() too.
    for (auto& [name, preset_json] : j["presets"].items()) {
      WorkspacePreset preset;
      preset.name = preset_json.value("name", name);
      preset.description = preset_json.value("description", "");

      if (preset_json.contains("visible_cards")) {
        yaze::Json visible_cards = preset_json["visible_cards"];
        if (visible_cards.is_array()) {
          for (const auto& card : visible_cards) {
            if (card.is_string()) {
              preset.visible_cards.push_back(card.get<std::string>());
            }
          }
        }
      }

      presets_[name] = preset;
      loaded_count++;
    }

    LOG_INFO("PanelManager", "Loaded %zu presets from %s", loaded_count,
             presets_file.string().c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("PanelManager", "Error loading presets: %s", e.what());
  }
}

// =============================================================================
// File Browser Integration
// =============================================================================

FileBrowser* PanelManager::GetFileBrowser(const std::string& category) {
  auto it = category_file_browsers_.find(category);
  if (it != category_file_browsers_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void PanelManager::EnableFileBrowser(const std::string& category,
                                     const std::string& root_path) {
  if (category_file_browsers_.find(category) == category_file_browsers_.end()) {
    auto browser = std::make_unique<FileBrowser>();

    // Set callback to forward file clicks
    browser->SetFileClickedCallback([this, category](const std::string& path) {
      if (on_file_clicked_) {
        on_file_clicked_(category, path);
      }
      // Also activate the editor for this category
      if (on_card_clicked_) {
        on_card_clicked_(category);
      }
    });

    if (!root_path.empty()) {
      browser->SetRootPath(root_path);
    }

    // Set defaults for Assembly file browser
    if (category == "Assembly") {
      browser->SetFileFilter({".asm", ".s", ".65c816", ".inc", ".h"});
    }

    category_file_browsers_[category] = std::move(browser);
    LOG_INFO("PanelManager", "Enabled file browser for category: %s",
             category.c_str());
  }
}

void PanelManager::DisableFileBrowser(const std::string& category) {
  category_file_browsers_.erase(category);
}

bool PanelManager::HasFileBrowser(const std::string& category) const {
  return category_file_browsers_.find(category) !=
         category_file_browsers_.end();
}

void PanelManager::SetFileBrowserPath(const std::string& category,
                                      const std::string& path) {
  auto it = category_file_browsers_.find(category);
  if (it != category_file_browsers_.end()) {
    it->second->SetRootPath(path);
  }
}

// ============================================================================
// Pinning (Phase 3 scaffold)
// ============================================================================

void PanelManager::SetPanelPinned(size_t session_id,
                                  const std::string& base_card_id,
                                  bool pinned) {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    prefixed_id = MakePanelId(session_id, base_card_id);
  }
  pinned_panels_[prefixed_id] = pinned;
}

bool PanelManager::IsPanelPinned(size_t session_id,
                                 const std::string& base_card_id) const {
  std::string prefixed_id = GetPrefixedPanelId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    prefixed_id = MakePanelId(session_id, base_card_id);
  }
  auto it = pinned_panels_.find(prefixed_id);
  return it != pinned_panels_.end() && it->second;
}

std::vector<std::string> PanelManager::GetPinnedPanels(
    size_t session_id) const {
  std::vector<std::string> result;
  auto session_it = session_cards_.find(session_id);
  if (session_it == session_cards_.end()) {
    return result;
  }
  const auto& session_panels = session_it->second;
  for (const auto& [panel_id, pinned] : pinned_panels_) {
    if (!pinned) {
      continue;
    }
    if (std::find(session_panels.begin(), session_panels.end(), panel_id) !=
        session_panels.end()) {
      result.push_back(panel_id);
    }
  }
  return result;
}

void PanelManager::SetPanelPinned(const std::string& base_card_id,
                                  bool pinned) {
  SetPanelPinned(active_session_, base_card_id, pinned);
}

bool PanelManager::IsPanelPinned(const std::string& base_card_id) const {
  return IsPanelPinned(active_session_, base_card_id);
}

std::vector<std::string> PanelManager::GetPinnedPanels() const {
  return GetPinnedPanels(active_session_);
}

// =============================================================================
// Panel Validation
// =============================================================================

PanelManager::PanelValidationResult PanelManager::ValidatePanel(
    const std::string& card_id) const {
  PanelValidationResult result;
  result.card_id = card_id;

  auto it = cards_.find(card_id);
  if (it == cards_.end()) {
    result.expected_title = "";
    result.found_in_imgui = false;
    result.message = "Panel not registered";
    return result;
  }

  const PanelDescriptor& info = it->second;
  result.expected_title = info.GetWindowTitle();

  if (result.expected_title.empty()) {
    result.found_in_imgui = false;
    result.message = "FAIL - Missing window title";
    return result;
  }

  // Check if ImGui has a window with this title
  ImGuiWindow* window = ImGui::FindWindowByName(result.expected_title.c_str());
  result.found_in_imgui = (window != nullptr);

  if (result.found_in_imgui) {
    result.message = "OK - Window found";
  } else {
    result.message = "FAIL - No window with title: " + result.expected_title;
  }

  return result;
}

std::vector<PanelManager::PanelValidationResult> PanelManager::ValidatePanels()
    const {
  std::vector<PanelValidationResult> results;
  results.reserve(cards_.size());

  for (const auto& [card_id, info] : cards_) {
    results.push_back(ValidatePanel(card_id));
  }

  return results;
}

}  // namespace editor
}  // namespace yaze
