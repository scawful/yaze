#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// Forward declaration
class AgentChat;

// =============================================================================
// EditorPanel wrappers for AgentEditor panels
// Each panel uses a callback to delegate drawing to AgentEditor methods
// =============================================================================

/**
 * @brief EditorPanel for AI Configuration panel
 */
class AgentConfigurationPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentConfigurationPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.configuration"; }
  std::string GetDisplayName() const override { return "AI Configuration"; }
  std::string GetIcon() const override { return ICON_MD_SETTINGS; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 10; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Agent Status panel
 */
class AgentStatusPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentStatusPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.status"; }
  std::string GetDisplayName() const override { return "Agent Status"; }
  std::string GetIcon() const override { return ICON_MD_INFO; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 20; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Prompt Editor panel
 */
class AgentPromptEditorPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentPromptEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.prompt_editor"; }
  std::string GetDisplayName() const override { return "Prompt Editor"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 30; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Bot Profiles panel
 */
class AgentBotProfilesPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentBotProfilesPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.profiles"; }
  std::string GetDisplayName() const override { return "Bot Profiles"; }
  std::string GetIcon() const override { return ICON_MD_FOLDER; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 40; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Chat History panel
 */
class AgentChatHistoryPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentChatHistoryPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.history"; }
  std::string GetDisplayName() const override { return "Chat History"; }
  std::string GetIcon() const override { return ICON_MD_HISTORY; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 50; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Metrics Dashboard panel
 */
class AgentMetricsDashboardPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentMetricsDashboardPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.metrics"; }
  std::string GetDisplayName() const override { return "Metrics Dashboard"; }
  std::string GetIcon() const override { return ICON_MD_ANALYTICS; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 60; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Agent Builder panel
 */
class AgentBuilderPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentBuilderPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.builder"; }
  std::string GetDisplayName() const override { return "Agent Builder"; }
  std::string GetIcon() const override { return ICON_MD_AUTO_FIX_HIGH; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 70; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Mesen2 debug integration
 */
class AgentMesenDebugPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentMesenDebugPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.mesen_debug"; }
  std::string GetDisplayName() const override { return "Mesen2 Debug"; }
  std::string GetIcon() const override { return ICON_MD_BUG_REPORT; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetShortcutHint() const override { return "Ctrl+Shift+M"; }
  int GetPriority() const override { return 80; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Knowledge Base panel
 *
 * Displays learned patterns, preferences, project contexts, and conversation
 * memories from the LearnedKnowledgeService.
 */
class AgentKnowledgeBasePanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentKnowledgeBasePanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.knowledge"; }
  std::string GetDisplayName() const override { return "Knowledge Base"; }
  std::string GetIcon() const override { return ICON_MD_PSYCHOLOGY; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 25; }  // Between Status and Prompt

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Agent Chat panel
 */
class AgentChatPanel : public EditorPanel {
 public:
  explicit AgentChatPanel(AgentChat* chat)
      : chat_(chat) {}

  std::string GetId() const override { return "agent.chat"; }
  std::string GetDisplayName() const override { return "Agent Chat"; }
  std::string GetIcon() const override { return ICON_MD_CHAT; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 5; }

  void Draw(bool* p_open) override;

 private:
  AgentChat* chat_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_EDITOR_PANELS_H_
