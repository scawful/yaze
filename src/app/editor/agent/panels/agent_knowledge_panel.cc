#include "app/editor/agent/panels/agent_knowledge_panel.h"

#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/core/icons.h"
#include "cli/service/agent/learned_knowledge_service.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void AgentKnowledgePanel::Draw(
    AgentUIContext* context,
    cli::agent::LearnedKnowledgeService* knowledge_service,
    const Callbacks& callbacks, ToastManager* toast_manager) {
  if (!knowledge_service) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                       "Knowledge service not available");
    ImGui::TextWrapped(
        "The knowledge service is only available when built with Z3ED_AI "
        "support.");
    return;
  }

  // Header with stats
  RenderStatsSection(knowledge_service);

  ImGui::Separator();

  // Tab bar for different categories
  if (ImGui::BeginTabBar("##KnowledgeTabs")) {
    if (ImGui::BeginTabItem(ICON_MD_SETTINGS " Preferences")) {
      selected_tab_ = 0;
      RenderPreferencesTab(knowledge_service, callbacks, toast_manager);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(ICON_MD_PATTERN " Patterns")) {
      selected_tab_ = 1;
      RenderPatternsTab(knowledge_service);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(ICON_MD_FOLDER " Projects")) {
      selected_tab_ = 2;
      RenderProjectsTab(knowledge_service);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(ICON_MD_PSYCHOLOGY " Memories")) {
      selected_tab_ = 3;
      RenderMemoriesTab(knowledge_service);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::Separator();

  // Action buttons
  if (ImGui::Button(ICON_MD_REFRESH " Refresh")) {
    if (callbacks.refresh_knowledge) {
      callbacks.refresh_knowledge();
    }
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_UPLOAD " Export")) {
    if (callbacks.export_knowledge) {
      callbacks.export_knowledge();
    }
  }

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
  if (ImGui::Button(ICON_MD_DELETE_FOREVER " Clear All")) {
    ImGui::OpenPopup("Confirm Clear");
  }
  ImGui::PopStyleColor();

  // Confirm clear popup
  if (ImGui::BeginPopupModal("Confirm Clear", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Clear all learned knowledge?");
    ImGui::Text("This action cannot be undone.");
    ImGui::Separator();

    if (ImGui::Button("Yes, Clear All", ImVec2(120, 0))) {
      if (callbacks.clear_all_knowledge) {
        callbacks.clear_all_knowledge();
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void AgentKnowledgePanel::RenderStatsSection(
    cli::agent::LearnedKnowledgeService* service) {
  auto stats = service->GetStats();

  auto& theme = AgentUI::GetTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
  ImGui::BeginChild("##StatsSection", ImVec2(0, 60), true);

  // Stats row
  float column_width = ImGui::GetContentRegionAvail().x / 4;

  ImGui::Columns(4, "##StatsColumns", false);
  ImGui::SetColumnWidth(0, column_width);
  ImGui::SetColumnWidth(1, column_width);
  ImGui::SetColumnWidth(2, column_width);
  ImGui::SetColumnWidth(3, column_width);

  ImGui::TextColored(theme.accent_color, "%d", stats.preference_count);
  ImGui::Text("Preferences");
  ImGui::NextColumn();

  ImGui::TextColored(theme.accent_color, "%d", stats.pattern_count);
  ImGui::Text("Patterns");
  ImGui::NextColumn();

  ImGui::TextColored(theme.accent_color, "%d", stats.project_count);
  ImGui::Text("Projects");
  ImGui::NextColumn();

  ImGui::TextColored(theme.accent_color, "%d", stats.memory_count);
  ImGui::Text("Memories");
  ImGui::NextColumn();

  ImGui::Columns(1);

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void AgentKnowledgePanel::RenderPreferencesTab(
    cli::agent::LearnedKnowledgeService* service, const Callbacks& callbacks,
    ToastManager* /*toast_manager*/) {
  // Add new preference
  ImGui::Text("Add Preference:");
  ImGui::PushItemWidth(150);
  ImGui::InputText("##PrefKey", new_pref_key_, sizeof(new_pref_key_));
  ImGui::PopItemWidth();
  ImGui::SameLine();
  ImGui::PushItemWidth(200);
  ImGui::InputText("##PrefValue", new_pref_value_, sizeof(new_pref_value_));
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_ADD " Add")) {
    if (strlen(new_pref_key_) > 0 && strlen(new_pref_value_) > 0) {
      if (callbacks.set_preference) {
        callbacks.set_preference(new_pref_key_, new_pref_value_);
      }
      new_pref_key_[0] = '\0';
      new_pref_value_[0] = '\0';
    }
  }

  ImGui::Separator();

  // List existing preferences
  auto prefs = service->GetAllPreferences();
  if (prefs.empty()) {
    ImGui::TextDisabled("No preferences stored");
  } else {
    ImGui::BeginChild("##PrefsList", ImVec2(0, 0), true);
    for (const auto& [key, value] : prefs) {
      ImGui::PushID(key.c_str());

      // Key column
      ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "%s", key.c_str());
      ImGui::SameLine(200);

      // Value column
      ImGui::TextWrapped("%s", value.c_str());
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);

      // Delete button
      if (ImGui::SmallButton(ICON_MD_DELETE)) {
        if (callbacks.remove_preference) {
          callbacks.remove_preference(key);
        }
      }

      ImGui::PopID();
      ImGui::Separator();
    }
    ImGui::EndChild();
  }
}

void AgentKnowledgePanel::RenderPatternsTab(
    cli::agent::LearnedKnowledgeService* service) {
  auto patterns = service->QueryPatterns("");

  if (patterns.empty()) {
    ImGui::TextDisabled("No patterns learned yet");
    ImGui::TextWrapped(
        "Patterns are learned automatically as you work with ROMs. "
        "The agent remembers frequently accessed rooms, sprite "
        "distributions, and tile usage patterns.");
    return;
  }

  ImGui::BeginChild("##PatternsList", ImVec2(0, 0), true);
  for (size_t i = 0; i < patterns.size(); ++i) {
    const auto& pattern = patterns[i];
    ImGui::PushID(static_cast<int>(i));

    // Pattern type header
    bool open =
        ImGui::TreeNode("##Pattern", "%s %s", ICON_MD_PATTERN, pattern.pattern_type.c_str());

    if (open) {
      ImGui::TextDisabled("ROM Hash: %s...",
                          pattern.rom_hash.substr(0, 16).c_str());
      ImGui::TextDisabled("Confidence: %.0f%%", pattern.confidence * 100);
      ImGui::TextDisabled("Access Count: %d", pattern.access_count);

      // Show truncated data
      if (pattern.pattern_data.size() > 100) {
        ImGui::TextWrapped("Data: %s...",
                           pattern.pattern_data.substr(0, 100).c_str());
      } else {
        ImGui::TextWrapped("Data: %s", pattern.pattern_data.c_str());
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  }
  ImGui::EndChild();
}

void AgentKnowledgePanel::RenderProjectsTab(
    cli::agent::LearnedKnowledgeService* service) {
  auto projects = service->GetAllProjects();

  if (projects.empty()) {
    ImGui::TextDisabled("No project contexts saved");
    ImGui::TextWrapped(
        "Project contexts store ROM-specific notes, goals, and custom labels. "
        "They're saved automatically when working with a project.");
    return;
  }

  ImGui::BeginChild("##ProjectsList", ImVec2(0, 0), true);
  for (size_t i = 0; i < projects.size(); ++i) {
    const auto& project = projects[i];
    ImGui::PushID(static_cast<int>(i));

    bool open = ImGui::TreeNode("##Project", "%s %s", ICON_MD_FOLDER,
                                project.project_name.c_str());

    if (open) {
      ImGui::TextDisabled("ROM Hash: %s...",
                          project.rom_hash.substr(0, 16).c_str());

      // Show truncated context
      if (project.context_data.size() > 200) {
        ImGui::TextWrapped("Context: %s...",
                           project.context_data.substr(0, 200).c_str());
      } else {
        ImGui::TextWrapped("Context: %s", project.context_data.c_str());
      }

      ImGui::TreePop();
    }

    ImGui::PopID();
  }
  ImGui::EndChild();
}

void AgentKnowledgePanel::RenderMemoriesTab(
    cli::agent::LearnedKnowledgeService* service) {
  // Search bar
  ImGui::Text("Search:");
  ImGui::SameLine();
  ImGui::PushItemWidth(300);
  bool search_changed =
      ImGui::InputText("##MemSearch", memory_search_, sizeof(memory_search_));
  ImGui::PopItemWidth();

  ImGui::Separator();

  // Get memories (search or recent)
  std::vector<cli::agent::LearnedKnowledgeService::ConversationMemory> memories;
  if (strlen(memory_search_) > 0) {
    memories = service->SearchMemories(memory_search_);
  } else {
    memories = service->GetRecentMemories(20);
  }

  if (memories.empty()) {
    if (strlen(memory_search_) > 0) {
      ImGui::TextDisabled("No memories match '%s'", memory_search_);
    } else {
      ImGui::TextDisabled("No conversation memories stored");
      ImGui::TextWrapped(
          "Conversation memories are summaries of past discussions with the "
          "agent. They help maintain context across sessions.");
    }
    return;
  }

  ImGui::BeginChild("##MemoriesList", ImVec2(0, 0), true);
  for (size_t i = 0; i < memories.size(); ++i) {
    const auto& memory = memories[i];
    ImGui::PushID(static_cast<int>(i));

    bool open = ImGui::TreeNode("##Memory", "%s %s", ICON_MD_PSYCHOLOGY,
                                memory.topic.c_str());

    if (open) {
      ImGui::TextWrapped("%s", memory.summary.c_str());

      if (!memory.key_facts.empty()) {
        ImGui::Text("Key Facts:");
        for (const auto& fact : memory.key_facts) {
          ImGui::BulletText("%s", fact.c_str());
        }
      }

      ImGui::TextDisabled("Access Count: %d", memory.access_count);

      ImGui::TreePop();
    }

    ImGui::PopID();
  }
  ImGui::EndChild();
}

}  // namespace editor
}  // namespace yaze
