#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/system/workspace/workspace_window_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "app/gui/animation/animator.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/json.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

void WorkspaceWindowManager::SavePresetsToFile() {
  auto config_dir_result = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_result.ok()) {
    LOG_ERROR("WorkspaceWindowManager", "Failed to get config directory: %s",
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
      LOG_ERROR("WorkspaceWindowManager", "Failed to open file for writing: %s",
                presets_file.string().c_str());
      return;
    }

    file << j.dump(2);
    file.close();

    LOG_INFO("WorkspaceWindowManager", "Saved %zu presets to %s",
             presets_.size(), presets_file.string().c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("WorkspaceWindowManager", "Error saving presets: %s", e.what());
  }
}

void WorkspaceWindowManager::LoadPresetsFromFile() {
  auto config_dir_result = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_result.ok()) {
    LOG_WARN("WorkspaceWindowManager", "Failed to get config directory: %s",
             config_dir_result.status().ToString().c_str());
    return;
  }

  std::filesystem::path presets_file =
      *config_dir_result / "layout_presets.json";

  if (!util::PlatformPaths::Exists(presets_file)) {
    LOG_INFO("WorkspaceWindowManager", "No presets file found at %s",
             presets_file.string().c_str());
    return;
  }

  try {
    std::ifstream file(presets_file);
    if (!file.is_open()) {
      LOG_WARN("WorkspaceWindowManager", "Failed to open presets file: %s",
               presets_file.string().c_str());
      return;
    }

    yaze::Json j;
    file >> j;
    file.close();

    if (!j.contains("presets")) {
      LOG_WARN("WorkspaceWindowManager", "Invalid presets file format");
      return;
    }

    size_t loaded_count = 0;
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

    LOG_INFO("WorkspaceWindowManager", "Loaded %zu presets from %s",
             loaded_count, presets_file.string().c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("WorkspaceWindowManager", "Error loading presets: %s", e.what());
  }
}

FileBrowser* WorkspaceWindowManager::GetFileBrowser(
    const std::string& category) {
  auto it = browser_state_.category_file_browsers.find(category);
  if (it != browser_state_.category_file_browsers.end()) {
    return it->second.get();
  }
  return nullptr;
}

void WorkspaceWindowManager::EnableFileBrowser(const std::string& category,
                                               const std::string& root_path) {
  if (browser_state_.category_file_browsers.find(category) ==
      browser_state_.category_file_browsers.end()) {
    auto browser = std::make_unique<FileBrowser>();

    browser->SetFileClickedCallback([this, category](const std::string& path) {
      if (on_file_clicked_) {
        on_file_clicked_(category, path);
      }
      if (browser_state_.on_window_clicked) {
        browser_state_.on_window_clicked(category);
      }
    });

    if (!root_path.empty()) {
      browser->SetRootPath(root_path);
    }

    if (category == "Assembly") {
      browser->SetFileFilter({".asm", ".s", ".65c816", ".inc", ".h"});
    }

    browser_state_.category_file_browsers[category] = std::move(browser);
    LOG_INFO("WorkspaceWindowManager", "Enabled file browser for category: %s",
             category.c_str());
  }
}

void WorkspaceWindowManager::DisableFileBrowser(const std::string& category) {
  browser_state_.category_file_browsers.erase(category);
}

bool WorkspaceWindowManager::HasFileBrowser(const std::string& category) const {
  return browser_state_.category_file_browsers.find(category) !=
         browser_state_.category_file_browsers.end();
}

void WorkspaceWindowManager::SetFileBrowserPath(const std::string& category,
                                                const std::string& path) {
  auto it = browser_state_.category_file_browsers.find(category);
  if (it != browser_state_.category_file_browsers.end()) {
    it->second->SetRootPath(path);
  }
}

void WorkspaceWindowManager::SetWindowPinnedImpl(
    size_t session_id, const std::string& base_card_id, bool pinned) {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    prefixed_id = MakeWindowId(session_id, canonical_base_id);
  }
  pinned_panels_[prefixed_id] = pinned;
}

bool WorkspaceWindowManager::IsWindowPinnedImpl(
    size_t session_id, const std::string& base_card_id) const {
  const std::string canonical_base_id = ResolveBaseWindowId(base_card_id);
  std::string prefixed_id = GetPrefixedWindowId(session_id, canonical_base_id);
  if (prefixed_id.empty()) {
    prefixed_id = MakeWindowId(session_id, canonical_base_id);
  }
  auto it = pinned_panels_.find(prefixed_id);
  return it != pinned_panels_.end() && it->second;
}

std::vector<std::string> WorkspaceWindowManager::GetPinnedWindowsImpl(
    size_t session_id) const {
  std::vector<std::string> result;
  const auto* session_panels = FindSessionWindowIds(session_id);
  if (!session_panels) {
    return result;
  }
  for (const auto& [panel_id, pinned] : pinned_panels_) {
    if (!pinned) {
      continue;
    }
    if (std::find(session_panels->begin(), session_panels->end(), panel_id) !=
        session_panels->end()) {
      const std::string base_id = GetBaseIdForPrefixedId(session_id, panel_id);
      result.push_back(base_id.empty() ? panel_id : base_id);
    }
  }
  return result;
}

void WorkspaceWindowManager::RememberPinnedStateForRemovedWindow(
    size_t session_id, const std::string& base_card_id,
    const std::string& prefixed_id) {
  auto pinned_it = pinned_panels_.find(prefixed_id);
  if (pinned_it == pinned_panels_.end()) {
    return;
  }

  bool has_other_live_instance = false;
  for (const auto& [mapped_session, mapping] : session_card_mapping_) {
    (void)mapped_session;
    auto mapping_it = mapping.find(base_card_id);
    if (mapping_it == mapping.end()) {
      continue;
    }
    if (mapping_it->second == prefixed_id) {
      continue;
    }
    if (cards_.find(mapping_it->second) != cards_.end()) {
      has_other_live_instance = true;
      break;
    }
  }

  if (!has_other_live_instance) {
    pending_pinned_base_ids_[base_card_id] = pinned_it->second;
  }
}

void WorkspaceWindowManager::SetWindowPinnedImpl(
    const std::string& base_card_id, bool pinned) {
  SetWindowPinnedImpl(active_session_, base_card_id, pinned);
}

bool WorkspaceWindowManager::IsWindowPinnedImpl(
    const std::string& base_card_id) const {
  return IsWindowPinnedImpl(active_session_, base_card_id);
}

std::vector<std::string> WorkspaceWindowManager::GetPinnedWindowsImpl() const {
  return GetPinnedWindowsImpl(active_session_);
}

WorkspaceWindowManager::WindowValidationResult
WorkspaceWindowManager::ValidateWindow(const std::string& card_id) const {
  WindowValidationResult result;
  result.card_id = card_id;

  auto it = cards_.find(card_id);
  if (it == cards_.end()) {
    result.expected_title = "";
    result.found_in_imgui = false;
    result.message = "Panel not registered";
    return result;
  }

  const WindowDescriptor& info = it->second;
  result.expected_title = GetWindowNameImpl(info);

  if (result.expected_title.empty()) {
    result.found_in_imgui = false;
    result.message = "FAIL - Missing window name";
    return result;
  }

  ImGuiWindow* window = ImGui::FindWindowByName(result.expected_title.c_str());
  result.found_in_imgui = (window != nullptr);

  if (result.found_in_imgui) {
    result.message = "OK - Window found";
  } else {
    result.message = "FAIL - No window with name: " + result.expected_title;
  }

  return result;
}

std::vector<WorkspaceWindowManager::WindowValidationResult>
WorkspaceWindowManager::ValidateWindows() const {
  std::vector<WindowValidationResult> results;
  results.reserve(cards_.size());

  for (const auto& [card_id, info] : cards_) {
    (void)info;
    results.push_back(ValidateWindow(card_id));
  }

  return results;
}

}  // namespace editor
}  // namespace yaze
