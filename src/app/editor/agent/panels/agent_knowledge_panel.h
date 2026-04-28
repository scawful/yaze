#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_KNOWLEDGE_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_KNOWLEDGE_PANEL_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/agent/agent_state.h"

namespace yaze {

namespace cli {
namespace agent {
class LearnedKnowledgeService;
}  // namespace agent
}  // namespace cli

namespace editor {

class ToastManager;

/**
 * @class AgentKnowledgePanel
 * @brief Panel for viewing/editing learned knowledge patterns
 *
 * Provides UI for:
 * - User preferences management
 * - ROM pattern viewing
 * - Project context management
 * - Conversation memory browsing
 */
class AgentKnowledgePanel {
 public:
  struct Callbacks {
    std::function<void(const std::string&, const std::string&)> set_preference;
    std::function<void(const std::string&)> remove_preference;
    std::function<void()> clear_all_knowledge;
    std::function<void()> export_knowledge;
    std::function<void(const std::string&)> import_knowledge;
    std::function<void()> refresh_knowledge;
  };

  AgentKnowledgePanel() = default;

  void Draw(AgentUIContext* context,
            cli::agent::LearnedKnowledgeService* knowledge_service,
            const Callbacks& callbacks, ToastManager* toast_manager);

 private:
  void RenderPreferencesTab(cli::agent::LearnedKnowledgeService* service,
                            const Callbacks& callbacks,
                            ToastManager* toast_manager);
  void RenderPatternsTab(cli::agent::LearnedKnowledgeService* service);
  void RenderProjectsTab(cli::agent::LearnedKnowledgeService* service);
  void RenderMemoriesTab(cli::agent::LearnedKnowledgeService* service);
  void RenderStatsSection(cli::agent::LearnedKnowledgeService* service);

  // UI state
  char new_pref_key_[128] = {};
  char new_pref_value_[256] = {};
  char memory_search_[256] = {};
  int selected_tab_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_KNOWLEDGE_PANEL_H_
