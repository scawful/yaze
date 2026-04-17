#include "app/editor/system/command_palette.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/events/core_events.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/editor/ui/recent_projects_model.h"
#include "core/project.h"
#include "util/json.h"
#include "util/log.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

void CommandPalette::AddCommand(const std::string& name,
                                const std::string& category,
                                const std::string& description,
                                const std::string& shortcut,
                                std::function<void()> callback) {
  CommandEntry entry;
  entry.name = name;
  entry.category = category;
  entry.description = description;
  entry.shortcut = shortcut;
  entry.callback = callback;
  commands_[name] = entry;
}

void CommandPalette::RecordUsage(const std::string& name) {
  auto it = commands_.find(name);
  if (it != commands_.end()) {
    it->second.usage_count++;
    it->second.last_used_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
  }
}

/*static*/ int CommandPalette::FuzzyScore(const std::string& text,
                                          const std::string& query) {
  if (query.empty())
    return 0;

  int score = 0;
  size_t text_idx = 0;
  size_t query_idx = 0;

  std::string text_lower = text;
  std::string query_lower = query;
  std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(),
                 ::tolower);
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                 ::tolower);

  // Exact match bonus
  if (text_lower == query_lower)
    return 1000;

  // Starts with bonus
  if (text_lower.find(query_lower) == 0)
    return 500;

  // Contains bonus
  if (text_lower.find(query_lower) != std::string::npos)
    return 250;

  // Fuzzy match - characters in order
  while (text_idx < text_lower.length() && query_idx < query_lower.length()) {
    if (text_lower[text_idx] == query_lower[query_idx]) {
      score += 10;
      query_idx++;
    }
    text_idx++;
  }

  // Penalty if not all characters matched
  if (query_idx != query_lower.length())
    return 0;

  return score;
}

std::vector<CommandEntry> CommandPalette::SearchCommands(
    const std::string& query) {
  std::vector<std::pair<int, CommandEntry>> scored;

  for (const auto& [name, entry] : commands_) {
    int score = FuzzyScore(entry.name, query);

    // Also check category and description
    score += FuzzyScore(entry.category, query) / 2;
    score += FuzzyScore(entry.description, query) / 4;

    // Frecency bonus (frequency + recency)
    score += entry.usage_count * 2;

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    int64_t age_ms = now_ms - entry.last_used_ms;
    if (age_ms < 60000) {  // Used in last minute
      score += 50;
    } else if (age_ms < 3600000) {  // Last hour
      score += 25;
    }

    if (score > 0) {
      scored.push_back({score, entry});
    }
  }

  // Sort by score descending
  std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<CommandEntry> results;
  for (const auto& [score, entry] : scored) {
    results.push_back(entry);
  }

  return results;
}

std::vector<CommandEntry> CommandPalette::GetRecentCommands(int limit) {
  std::vector<CommandEntry> recent;

  for (const auto& [name, entry] : commands_) {
    if (entry.usage_count > 0) {
      recent.push_back(entry);
    }
  }

  std::sort(recent.begin(), recent.end(),
            [](const CommandEntry& a, const CommandEntry& b) {
              return a.last_used_ms > b.last_used_ms;
            });

  if (recent.size() > static_cast<size_t>(limit)) {
    recent.resize(limit);
  }

  return recent;
}

std::vector<CommandEntry> CommandPalette::GetFrequentCommands(int limit) {
  std::vector<CommandEntry> frequent;

  for (const auto& [name, entry] : commands_) {
    if (entry.usage_count > 0) {
      frequent.push_back(entry);
    }
  }

  std::sort(frequent.begin(), frequent.end(),
            [](const CommandEntry& a, const CommandEntry& b) {
              return a.usage_count > b.usage_count;
            });

  if (frequent.size() > static_cast<size_t>(limit)) {
    frequent.resize(limit);
  }

  return frequent;
}

void CommandPalette::SaveHistory(const std::string& filepath) {
  try {
    yaze::Json j;
    j["version"] = 1;
    j["commands"] = yaze::Json::object();

    for (const auto& [name, entry] : commands_) {
      if (entry.usage_count > 0) {
        yaze::Json cmd;
        cmd["usage_count"] = entry.usage_count;
        cmd["last_used_ms"] = entry.last_used_ms;
        j["commands"][name] = cmd;
      }
    }

    std::ofstream file(filepath);
    if (file.is_open()) {
      file << j.dump(2);
      LOG_INFO("CommandPalette", "Saved command history to %s",
               filepath.c_str());
    }
  } catch (const std::exception& e) {
    LOG_ERROR("CommandPalette", "Failed to save command history: %s", e.what());
  }
}

void CommandPalette::LoadHistory(const std::string& filepath) {
  if (!std::filesystem::exists(filepath)) {
    return;
  }

  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      return;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    yaze::Json j = yaze::Json::parse(content);

    if (!j.contains("commands") || !j["commands"].is_object()) {
      return;
    }

    int loaded = 0;
    for (auto& [name, cmd_json] : j["commands"].items()) {
      auto it = commands_.find(name);
      if (it != commands_.end()) {
        it->second.usage_count = cmd_json.value("usage_count", 0);
        it->second.last_used_ms = cmd_json.value("last_used_ms", int64_t{0});
        loaded++;
      }
    }

    LOG_INFO("CommandPalette", "Loaded %d command history entries from %s",
             loaded, filepath.c_str());
  } catch (const std::exception& e) {
    LOG_ERROR("CommandPalette", "Failed to load command history: %s", e.what());
  }
}

std::vector<CommandEntry> CommandPalette::GetAllCommands() const {
  std::vector<CommandEntry> result;
  result.reserve(commands_.size());
  for (const auto& [name, entry] : commands_) {
    result.push_back(entry);
  }
  return result;
}

void CommandPalette::RegisterPanelCommands(
    WorkspaceWindowManager* window_manager, size_t session_id) {
  if (!window_manager)
    return;

  for (const auto& base_id : window_manager->GetWindowsInSession(session_id)) {
    const auto* descriptor =
        window_manager->GetWindowDescriptor(session_id, base_id);
    if (!descriptor) {
      continue;
    }

    // Create show command
    std::string show_name =
        absl::StrFormat("Show: %s", descriptor->display_name);
    std::string show_desc =
        absl::StrFormat("Open the %s window", descriptor->display_name);

    AddCommand(show_name, CommandCategory::kPanel, show_desc,
               descriptor->shortcut_hint,
               [window_manager, base_id, session_id]() {
                 window_manager->OpenWindow(session_id, base_id);
               });

    // Create hide command
    std::string hide_name =
        absl::StrFormat("Hide: %s", descriptor->display_name);
    std::string hide_desc =
        absl::StrFormat("Close the %s window", descriptor->display_name);

    AddCommand(hide_name, CommandCategory::kPanel, hide_desc, "",
               [window_manager, base_id, session_id]() {
                 window_manager->CloseWindow(session_id, base_id);
               });

    // Create toggle command
    std::string toggle_name =
        absl::StrFormat("Toggle: %s", descriptor->display_name);
    std::string toggle_desc = absl::StrFormat("Toggle the %s window visibility",
                                              descriptor->display_name);

    AddCommand(toggle_name, CommandCategory::kPanel, toggle_desc, "",
               [window_manager, base_id, session_id]() {
                 window_manager->ToggleWindow(session_id, base_id);
               });
  }
}

void CommandPalette::RegisterEditorCommands(
    std::function<void(const std::string&)> switch_callback) {
  // Get all editor categories
  auto categories = EditorRegistry::GetAllEditorCategories();

  for (const auto& category : categories) {
    std::string name = absl::StrFormat("Switch to: %s Editor", category);
    std::string desc =
        absl::StrFormat("Switch to the %s editor category", category);

    AddCommand(name, CommandCategory::kEditor, desc, "",
               [switch_callback, category]() { switch_callback(category); });
  }
}

void CommandPalette::RegisterLayoutCommands(
    std::function<void(const std::string&)> apply_callback) {
  struct ProfileInfo {
    const char* id;
    const char* name;
    const char* description;
  };

  static const ProfileInfo profiles[] = {
      {"code", "Code", "Focused editing workspace with minimal panel noise"},
      {"debug", "Debug",
       "Debugger-first workspace for tracing and memory tools"},
      {"mapping", "Mapping",
       "Map-centric workspace for overworld/dungeon flows"},
      {"chat", "Chat + Agent",
       "Agent collaboration workspace with chat-centric layout"},
  };

  for (const auto& profile : profiles) {
    std::string name = absl::StrFormat("Apply Profile: %s", profile.name);
    AddCommand(name, CommandCategory::kLayout, profile.description, "",
               [apply_callback, profile_id = std::string(profile.id)]() {
                 apply_callback("profile:" + profile_id);
               });
  }

  AddCommand("Capture Layout Snapshot", CommandCategory::kLayout,
             "Capture current layout as temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:capture"); });
  AddCommand("Restore Layout Snapshot", CommandCategory::kLayout,
             "Restore temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:restore"); });
  AddCommand("Clear Layout Snapshot", CommandCategory::kLayout,
             "Clear temporary session snapshot", "",
             [apply_callback]() { apply_callback("session:clear"); });

  // Legacy named workspace presets
  struct PresetInfo {
    const char* name;
    const char* description;
  };

  static const PresetInfo presets[] = {
      {"Minimal", "Minimal workspace with essential panels only"},
      {"Developer", "Debug-focused layout with emulator and memory tools"},
      {"Designer", "Visual-focused layout for graphics and palette editing"},
      {"Modder", "Full-featured layout with all panels available"},
      {"Overworld Expert", "Optimized layout for overworld editing"},
      {"Dungeon Expert", "Optimized layout for dungeon editing"},
      {"Testing", "QA-focused layout with testing tools"},
      {"Audio", "Music and sound editing focused layout"},
  };

  for (const auto& preset : presets) {
    std::string name = absl::StrFormat("Apply Layout: %s", preset.name);

    AddCommand(name, CommandCategory::kLayout, preset.description, "",
               [apply_callback, preset_name = std::string(preset.name)]() {
                 apply_callback(preset_name);
               });
  }

  // Reset to default layout command
  AddCommand("Reset Layout: Default", CommandCategory::kLayout,
             "Reset to the default layout for current editor", "",
             [apply_callback]() { apply_callback("Default"); });
}

void CommandPalette::RegisterRecentFilesCommands(
    std::function<void(const std::string&)> open_callback) {
  const auto& recent_files =
      project::RecentFilesManager::GetInstance().GetRecentFiles();

  for (const auto& filepath : recent_files) {
    // Skip files that no longer exist
    if (!std::filesystem::exists(filepath)) {
      continue;
    }

    // Extract just the filename for display
    std::filesystem::path path(filepath);
    std::string filename = path.filename().string();

    std::string name = absl::StrFormat("Open Recent: %s", filename);
    std::string desc = absl::StrFormat("Open file %s", filepath);

    AddCommand(name, CommandCategory::kFile, desc, "",
               [open_callback, filepath]() { open_callback(filepath); });
  }
}

void CommandPalette::RegisterDungeonRoomCommands(size_t session_id) {
  constexpr int kTotalRooms = 0x128;
  for (int room_id = 0; room_id < kTotalRooms; ++room_id) {
    const std::string label = zelda3::GetRoomLabel(room_id);
    const std::string room_name =
        label.empty() ? absl::StrFormat("Room %03X", room_id) : label;

    const std::string name =
        absl::StrFormat("Dungeon: Open Room [%03X] %s", room_id, room_name);
    const std::string desc =
        absl::StrFormat("Jump to dungeon room %03X", room_id);

    AddCommand(
        name, CommandCategory::kNavigation, desc, "", [room_id, session_id]() {
          if (auto* bus = ContentRegistry::Context::event_bus()) {
            bus->Publish(JumpToRoomRequestEvent::Create(room_id, session_id));
          }
        });
  }
}

void CommandPalette::RegisterWorkflowCommands(
    WorkspaceWindowManager* window_manager, size_t session_id) {
  if (window_manager) {
    const auto categories = window_manager->GetAllCategories(session_id);
    for (const auto& category : categories) {
      for (const auto& descriptor :
           window_manager->GetWindowsInCategory(session_id, category)) {
        if (descriptor.workflow_group.empty()) {
          continue;
        }
        if (descriptor.enabled_condition && !descriptor.enabled_condition()) {
          continue;
        }
        const std::string label = descriptor.workflow_label.empty()
                                      ? descriptor.display_name
                                      : descriptor.workflow_label;
        const std::string group = descriptor.workflow_group.empty()
                                      ? std::string("General")
                                      : descriptor.workflow_group;
        const std::string description =
            descriptor.workflow_description.empty()
                ? absl::StrFormat("Open %s", descriptor.display_name)
                : descriptor.workflow_description;
        AddCommand(
            absl::StrFormat("%s: %s", group, label), CommandCategory::kWorkflow,
            description, descriptor.shortcut_hint,
            [window_manager, session_id, panel_id = descriptor.card_id]() {
              window_manager->OpenWindow(session_id, panel_id);
            });
      }
    }
  }

  for (const auto& action : ContentRegistry::WorkflowActions::GetAll()) {
    if (action.enabled && !action.enabled()) {
      continue;
    }
    const std::string group =
        action.group.empty() ? std::string("General") : action.group;
    AddCommand(absl::StrFormat("%s: %s", group, action.label),
               CommandCategory::kWorkflow, action.description, action.shortcut,
               action.callback);
  }
}

void CommandPalette::RegisterWelcomeCommands(
    const RecentProjectsModel* model,
    const std::vector<std::string>& template_names,
    std::function<void(const std::string&)> remove_callback,
    std::function<void(const std::string&)> toggle_pin_callback,
    std::function<void()> undo_remove_callback,
    std::function<void()> clear_recents_callback,
    std::function<void(const std::string&)> create_from_template_callback,
    std::function<void()> dismiss_welcome_callback,
    std::function<void()> show_welcome_callback) {
  // Per-entry remove/pin commands. Keeping the palette fully driven by the
  // same model that powers the welcome screen means this list stays in sync
  // with pins/renames automatically on the next RefreshCommands().
  if (model) {
    for (const auto& entry : model->entries()) {
      if (entry.unavailable)
        continue;  // Platform-gated; skip silently.
      const std::string label = entry.display_name_override.empty()
                                    ? entry.name
                                    : entry.display_name_override;
      const std::string path = entry.filepath;

      if (remove_callback) {
        const std::string name =
            absl::StrFormat("Welcome: Remove Recent \"%s\"", label);
        const std::string desc =
            absl::StrFormat("Remove %s from the welcome screen's recents list.",
                            entry.filepath);
        AddCommand(name, CommandCategory::kFile, desc, "",
                   [remove_callback, path]() { remove_callback(path); });
      }
      if (toggle_pin_callback) {
        const std::string name = absl::StrFormat(
            "Welcome: %s Recent \"%s\"", entry.pinned ? "Unpin" : "Pin", label);
        const std::string desc =
            absl::StrFormat("%s %s on the welcome screen.",
                            entry.pinned ? "Unpin" : "Pin", entry.filepath);
        AddCommand(
            name, CommandCategory::kFile, desc, "",
            [toggle_pin_callback, path]() { toggle_pin_callback(path); });
      }
    }
  }

  if (clear_recents_callback) {
    AddCommand("Welcome: Clear Recent Files", CommandCategory::kFile,
               "Forget every entry in the welcome screen's recent list.", "",
               clear_recents_callback);
  }

  if (undo_remove_callback) {
    AddCommand("Welcome: Undo Last Recent Removal", CommandCategory::kFile,
               "Restore the last recent-project entry removed via Forget.", "",
               undo_remove_callback);
  }

  if (create_from_template_callback) {
    for (const auto& template_name : template_names) {
      if (template_name.empty())
        continue;
      const std::string name = absl::StrFormat(
          "Welcome: Create Project from Template: %s", template_name);
      const std::string desc = absl::StrFormat(
          "Start a new project using the \"%s\" template.", template_name);
      AddCommand(name, CommandCategory::kFile, desc, "",
                 [create_from_template_callback, template_name]() {
                   create_from_template_callback(template_name);
                 });
    }
  }

  if (show_welcome_callback) {
    AddCommand("Welcome: Show Welcome Screen", CommandCategory::kView,
               "Bring back the welcome screen if it's been dismissed.", "",
               show_welcome_callback);
  }
  if (dismiss_welcome_callback) {
    AddCommand("Welcome: Dismiss Welcome Screen", CommandCategory::kView,
               "Hide the welcome screen for the rest of this session.", "",
               dismiss_welcome_callback);
  }
}

}  // namespace editor
}  // namespace yaze
