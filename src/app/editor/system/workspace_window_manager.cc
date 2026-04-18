#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/system/workspace_window_manager.h"

#include <algorithm>
#include <cmath>
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

WindowDescriptor BuildDescriptorFromPanel(const WindowContent& panel) {
  WindowDescriptor descriptor;
  auto* panel_ptr = const_cast<WindowContent*>(&panel);
  descriptor.card_id = panel.GetId();
  descriptor.display_name = panel.GetDisplayName();
  descriptor.icon = panel.GetIcon();
  descriptor.category = panel.GetEditorCategory();
  descriptor.priority = panel.GetPriority();
  descriptor.workflow_group = panel.GetWorkflowGroup();
  descriptor.workflow_label = panel.GetWorkflowLabel();
  descriptor.workflow_description = panel.GetWorkflowDescription();
  descriptor.workflow_priority = panel.GetWorkflowPriority();
  descriptor.shortcut_hint = panel.GetShortcutHint();
  descriptor.scope = panel.GetScope();
  descriptor.window_lifecycle = panel.GetWindowLifecycle();
  descriptor.context_scope = panel.GetContextScope();
  descriptor.enabled_condition = [panel_ptr]() {
    return panel_ptr->IsEnabled();
  };
  descriptor.disabled_tooltip = panel.GetDisabledTooltip();
  descriptor.visibility_flag = nullptr;  // Created by RegisterPanel
  descriptor.window_title = panel.GetIcon() + " " + panel.GetDisplayName();
  return descriptor;
}

}  // namespace

// ============================================================================
// Category Icon Mapping
// ============================================================================

std::string WorkspaceWindowManager::GetCategoryIcon(
    const std::string& category) {
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

WorkspaceWindowManager::CategoryTheme WorkspaceWindowManager::GetCategoryTheme(
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

WorkspaceWindowManager::SidePanelWidthBounds
WorkspaceWindowManager::GetSidePanelWidthBounds(float viewport_width) {
  const float fallback = GetSidePanelWidthForViewport(viewport_width);
  if (viewport_width <= 0.0f) {
    SidePanelWidthBounds bounds{};
    bounds.min_width = 220.0f;
    bounds.max_width = std::max(520.0f, fallback);
    return bounds;
  }

  const float min_width = std::max(220.0f, viewport_width * 0.18f);
  const float max_width = std::max(min_width + 20.0f, viewport_width * 0.62f);
  SidePanelWidthBounds bounds{};
  bounds.min_width = min_width;
  bounds.max_width = max_width;
  return bounds;
}

float WorkspaceWindowManager::GetActiveSidePanelWidth(
    float viewport_width) const {
  const float default_width = GetSidePanelWidthForViewport(viewport_width);
  if (browser_state_.sidebar_width <= 0.0f) {
    return default_width;
  }
  const auto bounds = GetSidePanelWidthBounds(viewport_width);
  return std::clamp(browser_state_.sidebar_width, bounds.min_width,
                    bounds.max_width);
}

void WorkspaceWindowManager::SetActiveSidePanelWidth(float width,
                                                     float viewport_width,
                                                     bool notify) {
  if (width <= 0.0f) {
    browser_state_.sidebar_width = 0.0f;
    if (notify && browser_state_.on_sidebar_width_changed) {
      browser_state_.on_sidebar_width_changed(0.0f);
    }
    return;
  }

  const auto bounds = GetSidePanelWidthBounds(viewport_width);
  const float clamped = std::clamp(width, bounds.min_width, bounds.max_width);
  if (std::abs(browser_state_.sidebar_width - clamped) < 0.5f) {
    return;
  }
  browser_state_.sidebar_width = clamped;
  if (notify && browser_state_.on_sidebar_width_changed) {
    browser_state_.on_sidebar_width_changed(browser_state_.sidebar_width);
  }
}

void WorkspaceWindowManager::ResetSidePanelWidth(bool notify) {
  if (browser_state_.sidebar_width <= 0.0f) {
    return;
  }
  browser_state_.sidebar_width = 0.0f;
  if (notify && browser_state_.on_sidebar_width_changed) {
    browser_state_.on_sidebar_width_changed(browser_state_.sidebar_width);
  }
}

void WorkspaceWindowManager::SetPanelBrowserCategoryWidth(float width,
                                                          bool notify) {
  const float fallback = GetDefaultPanelBrowserCategoryWidth();
  const float clamped = std::max(180.0f, width > 0.0f ? width : fallback);
  if (std::abs(browser_state_.window_browser_category_width - clamped) < 0.5f) {
    return;
  }
  browser_state_.window_browser_category_width = clamped;
  if (notify && browser_state_.on_window_browser_width_changed) {
    browser_state_.on_window_browser_width_changed(
        browser_state_.window_browser_category_width);
  }
}

WindowDescriptor* WorkspaceWindowManager::FindDescriptorByPrefixedId(
    const std::string& prefixed_id) {
  auto it = registry_state_.descriptors.find(prefixed_id);
  return it != registry_state_.descriptors.end() ? &it->second : nullptr;
}

const WindowDescriptor* WorkspaceWindowManager::FindDescriptorByPrefixedId(
    const std::string& prefixed_id) const {
  auto it = registry_state_.descriptors.find(prefixed_id);
  return it != registry_state_.descriptors.end() ? &it->second : nullptr;
}

std::vector<std::string>* WorkspaceWindowManager::FindSessionWindowIds(
    size_t session_id) {
  auto it = session_state_.session_windows.find(session_id);
  return it != session_state_.session_windows.end() ? &it->second : nullptr;
}

const std::vector<std::string>* WorkspaceWindowManager::FindSessionWindowIds(
    size_t session_id) const {
  auto it = session_state_.session_windows.find(session_id);
  return it != session_state_.session_windows.end() ? &it->second : nullptr;
}

std::unordered_map<std::string, std::string>*
WorkspaceWindowManager::FindSessionWindowMapping(size_t session_id) {
  auto it = session_state_.session_window_mapping.find(session_id);
  return it != session_state_.session_window_mapping.end() ? &it->second
                                                           : nullptr;
}

const std::unordered_map<std::string, std::string>*
WorkspaceWindowManager::FindSessionWindowMapping(size_t session_id) const {
  auto it = session_state_.session_window_mapping.find(session_id);
  return it != session_state_.session_window_mapping.end() ? &it->second
                                                           : nullptr;
}

std::unordered_map<std::string, std::string>*
WorkspaceWindowManager::FindSessionReverseWindowMapping(size_t session_id) {
  auto it = session_state_.session_reverse_window_mapping.find(session_id);
  return it != session_state_.session_reverse_window_mapping.end() ? &it->second
                                                                   : nullptr;
}

const std::unordered_map<std::string, std::string>*
WorkspaceWindowManager::FindSessionReverseWindowMapping(
    size_t session_id) const {
  auto it = session_state_.session_reverse_window_mapping.find(session_id);
  return it != session_state_.session_reverse_window_mapping.end() ? &it->second
                                                                   : nullptr;
}

void WorkspaceWindowManager::PublishWindowVisibilityChanged(
    size_t session_id, const std::string& prefixed_window_id,
    const std::string& base_window_id, const std::string& category,
    bool visible) const {
  if (auto* bus = ContentRegistry::Context::event_bus()) {
    bus->Publish(PanelVisibilityChangedEvent::Create(
        prefixed_window_id, base_window_id, category, visible, session_id));
  }
}

size_t WorkspaceWindowManager::GetResourceWindowLimit(
    const std::string& resource_type) const {
  if (resource_type == "room") {
    return ResourcePanelLimits::kMaxRoomPanels;
  }
  if (resource_type == "song") {
    return ResourcePanelLimits::kMaxSongPanels;
  }
  if (resource_type == "sheet") {
    return ResourcePanelLimits::kMaxSheetPanels;
  }
  if (resource_type == "map") {
    return ResourcePanelLimits::kMaxMapPanels;
  }
  return ResourcePanelLimits::kMaxTotalResourcePanels;
}

void WorkspaceWindowManager::TrackResourceWindow(
    const std::string& panel_id, ResourceWindowContent* resource_panel) {
  if (!resource_panel) {
    return;
  }
  const std::string resource_type = resource_panel->GetResourceType();
  resource_panels_[resource_type].push_back(panel_id);
  panel_resource_types_[panel_id] = resource_type;
}

void WorkspaceWindowManager::UntrackResourceWindow(
    const std::string& panel_id) {
  auto type_it = panel_resource_types_.find(panel_id);
  if (type_it == panel_resource_types_.end()) {
    return;
  }

  const std::string resource_type = type_it->second;
  auto panels_it = resource_panels_.find(resource_type);
  if (panels_it != resource_panels_.end()) {
    panels_it->second.remove(panel_id);
    if (panels_it->second.empty()) {
      resource_panels_.erase(panels_it);
    }
  }
  panel_resource_types_.erase(type_it);
}

std::string WorkspaceWindowManager::SelectResourceWindowForEviction(
    const std::list<std::string>& panel_ids) const {
  for (const auto& panel_id : panel_ids) {
    if (!IsWindowPinnedImpl(panel_id)) {
      return panel_id;
    }
  }

  return panel_ids.empty() ? std::string{} : panel_ids.front();
}

// ============================================================================
// Session Lifecycle Management
// ============================================================================

// ============================================================================
// Panel Registration
// ============================================================================

void WorkspaceWindowManager::RegisterPanel(size_t session_id,
                                           const WindowDescriptor& base_info) {
  RegisterSession(session_id);  // Ensure session exists

  WindowDescriptor canonical_info = base_info;
  canonical_info.card_id = ResolveBaseWindowId(base_info.card_id);

  std::string panel_id =
      MakeWindowId(session_id, canonical_info.card_id, canonical_info.scope);

  bool already_registered = (cards_.find(panel_id) != cards_.end());
  if (already_registered && canonical_info.scope != WindowScope::kGlobal) {
    LOG_WARN("WorkspaceWindowManager",
             "Panel '%s' already registered, skipping duplicate",
             panel_id.c_str());
  }

  if (!already_registered) {
    // Create new WindowDescriptor with final ID
    WindowDescriptor panel_info = canonical_info;
    panel_info.card_id = panel_id;

    // If no visibility_flag provided, create centralized one
    if (!panel_info.visibility_flag) {
      centralized_visibility_[panel_id] = false;  // Hidden by default
      panel_info.visibility_flag = &centralized_visibility_[panel_id];
    }

    // Register the card
    cards_[panel_id] = panel_info;

    LOG_INFO("WorkspaceWindowManager",
             "Registered card %s -> %s for session %zu",
             canonical_info.card_id.c_str(), panel_id.c_str(), session_id);
  }

  if (canonical_info.scope == WindowScope::kGlobal) {
    global_panel_ids_.insert(panel_id);
    for (const auto& [mapped_session, _] : session_cards_) {
      TrackPanelForSession(mapped_session, canonical_info.card_id, panel_id);
    }
  } else {
    TrackPanelForSession(session_id, canonical_info.card_id, panel_id);
  }
}

void WorkspaceWindowManager::RegisterPanel(
    size_t session_id, const std::string& card_id,
    const std::string& display_name, const std::string& icon,
    const std::string& category, const std::string& shortcut_hint, int priority,
    std::function<void()> on_show, std::function<void()> on_hide,
    bool visible_by_default) {
  WindowDescriptor info;
  info.card_id = ResolveBaseWindowId(card_id);
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
    OpenWindowImpl(session_id, info.card_id);
  }
}

void WorkspaceWindowManager::UnregisterPanel(size_t session_id,
                                             const std::string& base_card_id) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return;
  }

  auto it = cards_.find(prefixed_id);
  if (it != cards_.end()) {
    LOG_INFO("WorkspaceWindowManager", "Unregistered card: %s",
             prefixed_id.c_str());
    RememberPinnedStateForRemovedWindow(session_id, canonical_base_id,
                                        prefixed_id);
    UntrackResourceWindow(prefixed_id);
    cards_.erase(it);
    centralized_visibility_.erase(prefixed_id);
    pinned_panels_.erase(prefixed_id);
    if (global_panel_ids_.find(prefixed_id) != global_panel_ids_.end()) {
      global_panel_ids_.erase(prefixed_id);
      for (auto& [mapped_session, card_list] : session_cards_) {
        card_list.erase(
            std::remove(card_list.begin(), card_list.end(), prefixed_id),
            card_list.end());
      }
      for (auto& [mapped_session, mapping] : session_card_mapping_) {
        mapping.erase(canonical_base_id);
      }
      for (auto& [mapped_session, reverse_mapping] :
           session_reverse_card_mapping_) {
        reverse_mapping.erase(prefixed_id);
      }
      return;
    }

    // Remove from session tracking
    auto& session_card_list = session_cards_[session_id];
    session_card_list.erase(std::remove(session_card_list.begin(),
                                        session_card_list.end(), prefixed_id),
                            session_card_list.end());

    session_card_mapping_[session_id].erase(canonical_base_id);
  }
}

void WorkspaceWindowManager::UnregisterPanelsWithPrefix(
    const std::string& prefix) {
  struct RemovalInfo {
    std::string prefixed_id;
    size_t session_id = 0;
    std::string base_id;
  };
  std::vector<RemovalInfo> to_remove;

  // Find all cards with the given prefix
  for (const auto& [card_id, card_info] : cards_) {
    (void)card_info;
    if (card_id.find(prefix) == 0) {  // Starts with prefix
      RemovalInfo info;
      info.prefixed_id = card_id;
      for (const auto& [session_id, reverse_mapping] :
           session_reverse_card_mapping_) {
        auto reverse_it = reverse_mapping.find(card_id);
        if (reverse_it != reverse_mapping.end()) {
          info.session_id = session_id;
          info.base_id = reverse_it->second;
          break;
        }
      }
      to_remove.push_back(std::move(info));
    }
  }

  // Remove them
  for (const auto& info : to_remove) {
    if (!info.base_id.empty()) {
      RememberPinnedStateForRemovedWindow(info.session_id, info.base_id,
                                          info.prefixed_id);
    }
    UntrackResourceWindow(info.prefixed_id);
    cards_.erase(info.prefixed_id);
    centralized_visibility_.erase(info.prefixed_id);
    pinned_panels_.erase(info.prefixed_id);
    LOG_INFO("WorkspaceWindowManager", "Unregistered card with prefix '%s': %s",
             prefix.c_str(), info.prefixed_id.c_str());
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

void WorkspaceWindowManager::ClearAllPanels() {
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
  resource_panels_.clear();
  panel_resource_types_.clear();
  session_count_ = 0;
  LOG_INFO("WorkspaceWindowManager", "Cleared all cards");
}

// ============================================================================
// WindowContent Instance Management (Phase 4)
// ============================================================================

void WorkspaceWindowManager::RegisterRegistryWindowContent(
    std::unique_ptr<WindowContent> panel) {
  if (!panel) {
    LOG_ERROR("WorkspaceWindowManager",
              "Attempted to register null WindowContent");
    return;
  }

  auto* resource_panel = dynamic_cast<ResourceWindowContent*>(panel.get());
  if (resource_panel) {
    EnforceResourceWindowLimits(resource_panel->GetResourceType());
  }

  std::string panel_id = panel->GetId();

  // Check if already registered
  if (panel_instances_.find(panel_id) != panel_instances_.end()) {
    LOG_WARN("WorkspaceWindowManager",
             "WindowContent '%s' already registered, skipping registry add",
             panel_id.c_str());
    return;
  }

  if (panel->GetScope() == WindowScope::kGlobal) {
    global_panel_ids_.insert(panel_id);
  }
  registry_panel_ids_.insert(panel_id);

  // Store the WindowContent instance
  panel_instances_[panel_id] = std::move(panel);

  TrackResourceWindow(panel_id, resource_panel);

  LOG_INFO("WorkspaceWindowManager", "Registered registry WindowContent: %s",
           panel_id.c_str());
}

void WorkspaceWindowManager::RegisterRegistryWindowContentsForSession(
    size_t session_id) {
  RegisterSession(session_id);
  for (const auto& panel_id : registry_panel_ids_) {
    auto it = panel_instances_.find(panel_id);
    if (it == panel_instances_.end()) {
      continue;
    }
    RegisterPanelDescriptorForSession(session_id, *it->second);
  }
}

void WorkspaceWindowManager::RegisterWindowContent(
    std::unique_ptr<WindowContent> panel) {
  if (!panel) {
    LOG_ERROR("WorkspaceWindowManager",
              "Attempted to register null WindowContent");
    return;
  }

  auto* resource_panel = dynamic_cast<ResourceWindowContent*>(panel.get());
  if (resource_panel) {
    EnforceResourceWindowLimits(resource_panel->GetResourceType());
  }

  std::string panel_id = panel->GetId();

  // Check if already registered
  if (panel_instances_.find(panel_id) != panel_instances_.end()) {
    LOG_WARN("WorkspaceWindowManager",
             "WindowContent '%s' already registered, skipping",
             panel_id.c_str());
    return;
  }

  // Auto-register WindowDescriptor for sidebar/menu visibility
  WindowDescriptor descriptor = BuildDescriptorFromPanel(*panel);

  // Check if panel should be visible by default
  bool visible_by_default = panel->IsVisibleByDefault();

  // Register the descriptor (creates visibility flag)
  RegisterPanel(active_session_, descriptor);

  // Set initial visibility if panel should be visible by default
  if (visible_by_default) {
    OpenWindowImpl(active_session_, panel_id);
  }

  // Store the WindowContent instance
  panel_instances_[panel_id] = std::move(panel);

  TrackResourceWindow(panel_id, resource_panel);

  LOG_INFO("WorkspaceWindowManager", "Registered WindowContent: %s (%s)",
           panel_id.c_str(), descriptor.display_name.c_str());
}

// ============================================================================
// Resource Management (Phase 6)
// ============================================================================

void WorkspaceWindowManager::EnforceResourceLimits(
    const std::string& resource_type) {
  auto it = resource_panels_.find(resource_type);
  if (it == resource_panels_.end())
    return;

  auto& panel_list = it->second;
  const size_t limit = GetResourceWindowLimit(resource_type);

  // Evict panels until we have room for one more (current count < limit)
  while (panel_list.size() >= limit) {
    std::string panel_to_evict = SelectResourceWindowForEviction(panel_list);
    if (panel_to_evict.empty()) {
      break;
    }

    // Remove from LRU list first to avoid iterator issues
    panel_list.remove(panel_to_evict);

    UnregisterWindowContent(panel_to_evict);
  }
}

void WorkspaceWindowManager::MarkPanelUsed(const std::string& panel_id) {
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

void WorkspaceWindowManager::UnregisterWindowContent(
    const std::string& panel_id) {
  UntrackResourceWindow(panel_id);
  auto it = panel_instances_.find(panel_id);
  if (it != panel_instances_.end()) {
    // Call OnClose before removing
    it->second->OnClose();
    gui::GetAnimator().ClearAnimationsForPanel(panel_id);
    panel_instances_.erase(it);
    registry_panel_ids_.erase(panel_id);
    global_panel_ids_.erase(panel_id);
    LOG_INFO("WorkspaceWindowManager", "Unregistered WindowContent: %s",
             panel_id.c_str());
  }

  // Also unregister the descriptor
  UnregisterPanel(active_session_, panel_id);
}

WindowContent* WorkspaceWindowManager::GetWindowContent(
    const std::string& panel_id) {
  auto it = panel_instances_.find(panel_id);
  if (it != panel_instances_.end()) {
    return it->second.get();
  }
  return nullptr;
}

WindowContent* WorkspaceWindowManager::FindPanelInstance(
    const std::string& prefixed_panel_id, const std::string& base_panel_id) {
  auto prefixed_it = panel_instances_.find(prefixed_panel_id);
  if (prefixed_it != panel_instances_.end()) {
    return prefixed_it->second.get();
  }
  auto base_it = panel_instances_.find(base_panel_id);
  if (base_it != panel_instances_.end()) {
    return base_it->second.get();
  }
  return nullptr;
}

const WindowContent* WorkspaceWindowManager::FindPanelInstance(
    const std::string& prefixed_panel_id,
    const std::string& base_panel_id) const {
  auto prefixed_it = panel_instances_.find(prefixed_panel_id);
  if (prefixed_it != panel_instances_.end()) {
    return prefixed_it->second.get();
  }
  auto base_it = panel_instances_.find(base_panel_id);
  if (base_it != panel_instances_.end()) {
    return base_it->second.get();
  }
  return nullptr;
}

void WorkspaceWindowManager::DrawAllVisiblePanels() {
  // Suppress panel drawing when dashboard is active (no editor selected yet)
  // This ensures panels don't appear until user selects an editor
  if (browser_state_.active_category.empty() ||
      browser_state_.active_category == kDashboardCategory) {
    return;
  }

  auto session_it = session_cards_.find(active_session_);
  if (session_it == session_cards_.end()) {
    return;
  }

  auto& animator = gui::GetAnimator();
  bool animations_enabled = animator.IsEnabled();
  const bool touch_device = gui::LayoutHelpers::IsTouchDevice();

  const auto resolve_switch_speed = [&](float snappy, float standard,
                                        float relaxed) {
    switch (animator.motion_profile()) {
      case gui::MotionProfile::kSnappy:
        return snappy;
      case gui::MotionProfile::kRelaxed:
        return relaxed;
      case gui::MotionProfile::kStandard:
      default:
        return standard;
    }
  };
  const float category_transition_speed =
      resolve_switch_speed(8.5f, 6.0f, 4.5f);
  const float panel_fade_speed = resolve_switch_speed(11.0f, 8.0f, 6.0f);

  // Animate global category transition alpha
  float global_alpha = 1.0f;
  if (animations_enabled) {
    global_alpha = animator.Animate("global", "category_transition", 1.0f,
                                    category_transition_speed);
  }

  for (const auto& prefixed_panel_id : session_it->second) {
    auto descriptor_it = cards_.find(prefixed_panel_id);
    if (descriptor_it == cards_.end()) {
      continue;
    }
    WindowDescriptor& descriptor = descriptor_it->second;

    std::string base_panel_id =
        GetBaseIdForPrefixedId(active_session_, prefixed_panel_id);
    if (base_panel_id.empty()) {
      base_panel_id = prefixed_panel_id;
    }

    WindowContent* panel = FindPanelInstance(prefixed_panel_id, base_panel_id);
    if (!panel) {
      continue;
    }

    bool is_visible = descriptor.visibility_flag && *descriptor.visibility_flag;

    // Category filtering: draw if matches active category or user has pinned.
    // Previously a Persistent lifecycle was a third branch here; collapsed into
    // CrossEditor + default-pin via UserSettings revision-7 migration.
    bool should_draw = false;
    if (is_visible) {
      if (panel->GetEditorCategory() == browser_state_.active_category ||
          descriptor.category == browser_state_.active_category) {
        should_draw = true;
      } else if (IsWindowPinnedImpl(active_session_, base_panel_id)) {
        should_draw = true;
      }
    }

    // Compute target alpha: 1.0 if should draw, 0.0 if should hide
    float target_alpha = should_draw ? 1.0f : 0.0f;

    // Animate alpha towards target (or snap if animations disabled)
    float current_alpha = 1.0f;
    if (animations_enabled) {
      current_alpha = animator.Animate(prefixed_panel_id, "panel_alpha",
                                       target_alpha, panel_fade_speed);
      current_alpha *= global_alpha;  // Apply global category fade
    } else {
      current_alpha = target_alpha;
    }

    // Skip drawing if alpha is effectively zero
    if (current_alpha < 0.01f) {
      continue;
    }

    // Get visibility flag for the panel window
    bool* visibility_flag = descriptor.visibility_flag;

    // Get display name without icon - PanelWindow will add the icon
    // This fixes the double-icon issue where both descriptor and PanelWindow added icons
    std::string display_name = descriptor.display_name.empty()
                                   ? panel->GetDisplayName()
                                   : descriptor.display_name;
    std::string icon =
        descriptor.icon.empty() ? panel->GetIcon() : descriptor.icon;

    if (editor_resolver_) {
      if (Editor* editor = editor_resolver_(panel->GetEditorCategory())) {
        ContentRegistry::Context::SetCurrentEditor(editor);
      }
    }

    // Create PanelWindow and draw content
    gui::PanelWindow window(display_name.c_str(), icon.c_str(),
                            visibility_flag);
    window.SetStableId(prefixed_panel_id);
    if (touch_device) {
      window.SetPosition(gui::PanelWindow::Position::Center);
    }

    // Use preferred width from WindowContent if specified
    float preferred_width = panel->GetPreferredWidth();
    if (preferred_width > 0.0f) {
      window.SetDefaultSize(preferred_width, 0);  // 0 height = auto
    }

    // Enable pin functionality for cross-editor persistence
    window.SetPinnable(true);
    window.SetPinned(IsWindowPinnedImpl(active_session_, base_panel_id));

    // Wire up pin state change callback to persist to WorkspaceWindowManager
    window.SetPinChangedCallback([this, base_panel_id](bool pinned) {
      SetWindowPinnedImpl(base_panel_id, pinned);
    });

    // Apply fade alpha for smooth transitions
    std::optional<gui::StyleVarGuard> alpha_guard;
    if (current_alpha < 1.0f) {
      alpha_guard.emplace(ImGuiStyleVar_Alpha, current_alpha);
    }

    animator.ApplyPanelTransitionPre(prefixed_panel_id);
    if (window.Begin(visibility_flag)) {
      panel->DrawWithLazyInit(visibility_flag);
    }
    window.End();
    animator.ApplyPanelTransitionPost(prefixed_panel_id);

    alpha_guard.reset();

    // Handle visibility change (window closed via X button)
    if (visibility_flag && !*visibility_flag) {
      panel->OnClose();
    }
  }
}

void WorkspaceWindowManager::OnEditorSwitch(const std::string& from_category,
                                            const std::string& to_category) {
  if (from_category == to_category) {
    return;  // No switch needed
  }

  LOG_INFO("WorkspaceWindowManager", "Switching from category '%s' to '%s'",
           from_category.c_str(), to_category.c_str());

  // IMPORTANT:
  // Editor switching must *not* be treated as a user-initiated hide/show.
  //
  // Historically this function toggled visibility flags (CloseWindowImpl/OpenWindowImpl),
  // which publishes PanelVisibilityChangedEvent and permanently overwrote user
  // prefs. Worse, dynamic "resource windows" (rooms/songs) interpret
  // IsWindowVisibleImpl()==false as "user closed", unregistering themselves.
  //
  // Reset global category transition to trigger fade-in
  if (gui::GetAnimator().IsEnabled()) {
    gui::GetAnimator().BeginPanelTransition("global",
                                            gui::TransitionType::kFade);

    // Also begin a per-panel fade for every visible window in the incoming
    // category. Without this, individual panels use whatever alpha the
    // animator happened to have for their workspace fade state from the
    // previous session, which was the source of the "old bitmap still
    // visible during switch" ghosting. EditorActivator clears that narrow
    // transition state just before this call, so each transition starts from
    // alpha=0.
    const auto incoming =
        GetWindowsInCategoryImpl(active_session_, to_category);
    for (const auto& descriptor : incoming) {
      if (descriptor.visibility_flag && *descriptor.visibility_flag) {
        gui::GetAnimator().BeginPanelTransition(
            GetPrefixedWindowId(active_session_, descriptor.card_id),
            gui::TransitionType::kFade);
      }
    }
  }

  SetActiveCategory(to_category);
}

// ============================================================================
// Panel Control (Programmatic, No GUI)
// ============================================================================

bool WorkspaceWindowManager::OpenWindowImpl(size_t session_id,
                                            const std::string& base_card_id) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto* descriptor = FindDescriptorByPrefixedId(prefixed_id);
  if (descriptor) {
    const bool was_visible =
        (descriptor->visibility_flag && *descriptor->visibility_flag);
    if (descriptor->visibility_flag) {
      *descriptor->visibility_flag = true;
    }
    if (descriptor->on_show) {
      descriptor->on_show();
    }
    if (!was_visible) {
      if (WindowContent* panel =
              FindPanelInstance(prefixed_id, canonical_base_id)) {
        panel->OnOpen();
      }
    }

    PublishWindowVisibilityChanged(session_id, prefixed_id, canonical_base_id,
                                   descriptor->category, true);
    return true;
  }
  return false;
}

bool WorkspaceWindowManager::CloseWindowImpl(size_t session_id,
                                             const std::string& base_card_id) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto* descriptor = FindDescriptorByPrefixedId(prefixed_id);
  if (descriptor) {
    const bool was_visible =
        (descriptor->visibility_flag && *descriptor->visibility_flag);
    if (descriptor->visibility_flag) {
      *descriptor->visibility_flag = false;
    }
    if (descriptor->on_hide) {
      descriptor->on_hide();
    }
    if (was_visible) {
      if (WindowContent* panel =
              FindPanelInstance(prefixed_id, canonical_base_id)) {
        panel->OnClose();
      }
    }

    PublishWindowVisibilityChanged(session_id, prefixed_id, canonical_base_id,
                                   descriptor->category, false);
    return true;
  }
  return false;
}

bool WorkspaceWindowManager::ToggleWindowImpl(size_t session_id,
                                              const std::string& base_card_id) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return false;
  }

  auto* descriptor = FindDescriptorByPrefixedId(prefixed_id);
  if (descriptor && descriptor->visibility_flag) {
    bool new_state = !(*descriptor->visibility_flag);
    *descriptor->visibility_flag = new_state;

    if (new_state && descriptor->on_show) {
      descriptor->on_show();
    } else if (!new_state && descriptor->on_hide) {
      descriptor->on_hide();
    }
    if (WindowContent* panel =
            FindPanelInstance(prefixed_id, canonical_base_id)) {
      if (new_state) {
        panel->OnOpen();
      } else {
        panel->OnClose();
      }
    }

    PublishWindowVisibilityChanged(session_id, prefixed_id, canonical_base_id,
                                   descriptor->category, new_state);
    return true;
  }
  return false;
}

bool WorkspaceWindowManager::IsWindowVisibleImpl(
    size_t session_id, const std::string& base_card_id) const {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return false;
  }

  const auto* descriptor = FindDescriptorByPrefixedId(prefixed_id);
  if (descriptor && descriptor->visibility_flag) {
    return *descriptor->visibility_flag;
  }
  return false;
}

bool* WorkspaceWindowManager::GetVisibilityFlag(
    size_t session_id, const std::string& base_card_id) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return nullptr;
  }

  auto* descriptor = FindDescriptorByPrefixedId(prefixed_id);
  if (descriptor) {
    return descriptor->visibility_flag;
  }
  return nullptr;
}

// ============================================================================
// Batch Operations
// ============================================================================

void WorkspaceWindowManager::ShowAllWindowsInSession(size_t session_id) {
  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    for (const auto& prefixed_card_id : *session_windows) {
      if (auto* descriptor = FindDescriptorByPrefixedId(prefixed_card_id)) {
        if (descriptor->visibility_flag) {
          *descriptor->visibility_flag = true;
        }
        if (descriptor->on_show) {
          descriptor->on_show();
        }
      }
    }
  }
}

void WorkspaceWindowManager::HideAllWindowsInSession(size_t session_id) {
  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    for (const auto& prefixed_card_id : *session_windows) {
      if (auto* descriptor = FindDescriptorByPrefixedId(prefixed_card_id)) {
        if (descriptor->visibility_flag) {
          *descriptor->visibility_flag = false;
        }
        if (descriptor->on_hide) {
          descriptor->on_hide();
        }
      }
    }
  }
}

void WorkspaceWindowManager::ShowAllWindowsInCategory(
    size_t session_id, const std::string& category) {
  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    for (const auto& prefixed_card_id : *session_windows) {
      if (auto* descriptor = FindDescriptorByPrefixedId(prefixed_card_id)) {
        if (descriptor->category != category) {
          continue;
        }
        if (descriptor->visibility_flag) {
          *descriptor->visibility_flag = true;
        }
        if (descriptor->on_show) {
          descriptor->on_show();
        }
      }
    }
  }
}

void WorkspaceWindowManager::HideAllWindowsInCategory(
    size_t session_id, const std::string& category) {
  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    for (const auto& prefixed_card_id : *session_windows) {
      if (auto* descriptor = FindDescriptorByPrefixedId(prefixed_card_id)) {
        if (descriptor->category != category) {
          continue;
        }
        if (descriptor->visibility_flag) {
          *descriptor->visibility_flag = false;
        }
        if (descriptor->on_hide) {
          descriptor->on_hide();
        }
      }
    }
  }
}

void WorkspaceWindowManager::ShowOnlyWindow(size_t session_id,
                                            const std::string& base_card_id) {
  // First get the category of the target card
  std::string prefixed_id = GetPrefixedWindowId(session_id, base_card_id);
  if (prefixed_id.empty()) {
    return;
  }

  const auto* target = FindDescriptorByPrefixedId(prefixed_id);
  if (!target) {
    return;
  }

  std::string category = target->category;

  // Hide all cards in the same category
  HideAllWindowsInCategory(session_id, category);

  // Show the target card
  OpenWindowImpl(session_id, base_card_id);
}

// ============================================================================
// Query Methods
// ============================================================================

std::vector<std::string> WorkspaceWindowManager::GetWindowsInSessionImpl(
    size_t session_id) const {
  std::vector<std::string> result;
  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    result.reserve(session_windows->size());
    for (const auto& prefixed_window_id : *session_windows) {
      const std::string base_window_id =
          GetBaseIdForPrefixedId(session_id, prefixed_window_id);
      result.push_back(base_window_id.empty() ? prefixed_window_id
                                              : base_window_id);
    }
  }
  return result;
}

std::vector<WindowDescriptor> WorkspaceWindowManager::GetWindowsInCategoryImpl(
    size_t session_id, const std::string& category) const {
  std::vector<WindowDescriptor> result;

  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    for (const auto& prefixed_window_id : *session_windows) {
      if (const auto* descriptor =
              FindDescriptorByPrefixedId(prefixed_window_id)) {
        if (descriptor->category == category) {
          WindowDescriptor window = *descriptor;
          const std::string base_window_id =
              GetBaseIdForPrefixedId(session_id, prefixed_window_id);
          if (!base_window_id.empty()) {
            window.card_id = base_window_id;
          }
          result.push_back(std::move(window));
        }
      }
    }
  }

  // Sort by priority
  std::sort(result.begin(), result.end(),
            [](const WindowDescriptor& a, const WindowDescriptor& b) {
              return a.priority < b.priority;
            });

  return result;
}

std::vector<std::string> WorkspaceWindowManager::GetAllCategories(
    size_t session_id) const {
  std::vector<std::string> categories;

  if (const auto* session_windows = FindSessionWindowIds(session_id)) {
    for (const auto& prefixed_window_id : *session_windows) {
      if (const auto* descriptor =
              FindDescriptorByPrefixedId(prefixed_window_id)) {
        if (std::find(categories.begin(), categories.end(),
                      descriptor->category) == categories.end()) {
          categories.push_back(descriptor->category);
        }
      }
    }
  }
  return categories;
}

const WindowDescriptor* WorkspaceWindowManager::GetWindowDescriptorImpl(
    size_t session_id, const std::string& base_card_id) const {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    return nullptr;
  }

  return FindDescriptorByPrefixedId(prefixed_id);
}

std::vector<std::string> WorkspaceWindowManager::GetAllCategories() const {
  std::vector<std::string> categories;
  for (const auto& [card_id, card_info] : registry_state_.descriptors) {
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

std::vector<std::string> WorkspaceWindowManager::GetVisibleWindowIdsImpl(
    size_t session_id) const {
  std::vector<std::string> visible_panels;

  const auto* window_mapping = FindSessionWindowMapping(session_id);
  if (!window_mapping) {
    return visible_panels;
  }

  for (const auto& [base_id, prefixed_id] : *window_mapping) {
    if (const auto* descriptor = FindDescriptorByPrefixedId(prefixed_id)) {
      if (descriptor->visibility_flag && *descriptor->visibility_flag) {
        visible_panels.push_back(base_id);
      }
    }
  }

  return visible_panels;
}

// ============================================================================
// Workspace Presets
// ============================================================================

void WorkspaceWindowManager::SavePreset(const std::string& name,
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
  LOG_INFO("WorkspaceWindowManager", "Saved preset: %s (%zu cards)",
           name.c_str(), preset.visible_cards.size());
}

bool WorkspaceWindowManager::LoadPreset(const std::string& name) {
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

  LOG_INFO("WorkspaceWindowManager", "Loaded preset: %s", name.c_str());
  return true;
}

void WorkspaceWindowManager::DeletePreset(const std::string& name) {
  presets_.erase(name);
  SavePresetsToFile();
}

std::vector<WorkspaceWindowManager::WorkspacePreset>
WorkspaceWindowManager::GetPresets() const {
  std::vector<WorkspacePreset> result;
  for (const auto& [name, preset] : presets_) {
    result.push_back(preset);
  }
  return result;
}

// ============================================================================
// Quick Actions
// ============================================================================

void WorkspaceWindowManager::ShowAll(size_t session_id) {
  ShowAllWindowsInSession(session_id);
}

void WorkspaceWindowManager::HideAll(size_t session_id) {
  HideAllWindowsInSession(session_id);
}

void WorkspaceWindowManager::ResetToDefaults(size_t session_id) {
  // Hide all cards first
  HideAllWindowsInSession(session_id);

  // TODO: Load default visibility from config file or hardcoded defaults
  LOG_INFO("WorkspaceWindowManager", "Reset to defaults for session %zu",
           session_id);
}

void WorkspaceWindowManager::ResetToDefaults(size_t session_id,
                                             EditorType editor_type) {
  // Get category for this editor
  std::string category = EditorRegistry::GetEditorCategory(editor_type);
  if (category.empty()) {
    LOG_WARN("WorkspaceWindowManager",
             "No category found for editor type %d, skipping reset",
             static_cast<int>(editor_type));
    return;
  }

  // Hide all cards in this category first
  HideAllWindowsInCategory(session_id, category);

  // Get default cards from LayoutPresets
  auto default_panels = LayoutPresets::GetDefaultPanels(editor_type);

  // Show each default card
  for (const auto& card_id : default_panels) {
    if (OpenWindowImpl(session_id, card_id)) {
      LOG_INFO("WorkspaceWindowManager", "Showing default card: %s",
               card_id.c_str());
    }
  }

  LOG_INFO("WorkspaceWindowManager",
           "Reset %s editor to defaults (%zu cards visible)", category.c_str(),
           default_panels.size());
}

// ============================================================================
// Session Prefixing Utilities
// ============================================================================

std::string WorkspaceWindowManager::MakeWindowId(
    size_t session_id, const std::string& base_id) const {
  return MakeWindowId(session_id, base_id, WindowScope::kSession);
}

std::string WorkspaceWindowManager::MakeWindowId(size_t session_id,
                                                 const std::string& base_id,
                                                 WindowScope scope) const {
  const std::string canonical_base_id = ResolveBaseWindowId(base_id);
  if (scope == WindowScope::kGlobal) {
    return canonical_base_id;
  }
  if (ShouldPrefixPanels()) {
    return absl::StrFormat("s%zu.%s", session_id, canonical_base_id);
  }
  return canonical_base_id;
}

}  // namespace editor
}  // namespace yaze
