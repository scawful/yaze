#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// Forward declaration
class AgentChat;

// =============================================================================
// WindowContent wrappers for AgentEditor panels
// Each panel uses a callback to delegate drawing to AgentEditor methods
// =============================================================================

/**
 * @brief WindowContent for AI Configuration panel
 */
class AgentConfigurationPanel : public WindowContent {
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
 * @brief WindowContent for Agent Status panel
 */
class AgentStatusPanel : public WindowContent {
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
 * @brief WindowContent for Prompt Editor panel
 */
class AgentPromptEditorPanel : public WindowContent {
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
 * @brief WindowContent for Bot Profiles panel
 */
class AgentBotProfilesPanel : public WindowContent {
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
 * @brief WindowContent for Chat History panel
 */
class AgentChatHistoryPanel : public WindowContent {
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
 * @brief WindowContent for Metrics Dashboard panel
 */
class AgentMetricsDashboardPanel : public WindowContent {
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
 * @brief WindowContent for Agent Builder panel
 */
class AgentBuilderPanel : public WindowContent {
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
 * @brief WindowContent for Mesen2 debug integration
 */
class AgentMesenDebugPanel : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;

  explicit AgentMesenDebugPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.mesen_debug"; }
  std::string GetDisplayName() const override { return "Mesen2 Debug"; }
  std::string GetIcon() const override { return ICON_MD_BUG_REPORT; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Live Debugging"; }
  std::string GetWorkflowDescription() const override {
    return "Inspect and control a connected Mesen2 debugging session";
  }
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
 * @brief WindowContent for Knowledge Base panel
 *
 * Displays learned patterns, preferences, project contexts, and conversation
 * memories from the LearnedKnowledgeService.
 */
class AgentKnowledgeBasePanel : public WindowContent {
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
 * @brief WindowContent for Agent Chat panel
 */
class AgentChatPanel : public WindowContent {
 public:
  explicit AgentChatPanel(AgentChat* chat) : chat_(chat) {}

  std::string GetId() const override { return "agent.chat"; }
  std::string GetDisplayName() const override { return "Agent Chat"; }
  std::string GetIcon() const override { return ICON_MD_CHAT; }
  std::string GetEditorCategory() const override { return "Agent"; }
  int GetPriority() const override { return 5; }

  void Draw(bool* p_open) override;

 private:
  AgentChat* chat_;
};

/**
 * @brief WindowContent for Oracle State Library management
 *
 * Provides UI for managing Oracle of Secrets save states:
 * - View all states with status (canon/draft/deprecated)
 * - Load states into Mesen2 emulator
 * - Verify and promote draft states to canon
 * - Deprecate bad states
 */
class OracleStateLibraryEditorPanel : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;

  explicit OracleStateLibraryEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.oracle_states"; }
  std::string GetDisplayName() const override { return "Oracle States"; }
  std::string GetIcon() const override { return ICON_MD_SAVE; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Live Debugging"; }
  std::string GetWorkflowLabel() const override { return "State Library"; }
  std::string GetWorkflowDescription() const override {
    return "Manage saved emulator states for hack debugging and regression "
           "checks";
  }
  std::string GetShortcutHint() const override { return "Ctrl+Shift+O"; }
  int GetPriority() const override { return 85; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief WindowContent for Feature Flag Editor
 *
 * Displays ASM feature flags from hack_manifest.json and allows toggling
 * them by writing to Config/feature_flags.asm.
 */
class FeatureFlagEditorEditorPanel : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;

  explicit FeatureFlagEditorEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.feature_flags"; }
  std::string GetDisplayName() const override { return "Feature Flags"; }
  std::string GetIcon() const override { return ICON_MD_FLAG; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Build & Config"; }
  std::string GetWorkflowDescription() const override {
    return "Inspect and edit project feature flags";
  }
  int GetPriority() const override { return 90; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief WindowContent for Hack Manifest Status + Protected Regions Inspector
 *
 * Card B3: manifest freshness (path, mtime, reload button)
 * Card B4: searchable protected regions table
 */
class ManifestEditorPanel : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;

  explicit ManifestEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.manifest"; }
  std::string GetDisplayName() const override { return "Hack Manifest"; }
  std::string GetIcon() const override { return ICON_MD_DESCRIPTION; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Build & Config"; }
  std::string GetWorkflowDescription() const override {
    return "Inspect manifest metadata, protected regions, and registry state";
  }
  int GetPriority() const override { return 95; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief WindowContent for SRAM Variable Viewer
 *
 * Displays live SRAM variable values from a running Mesen2 emulator,
 * using variable definitions from hack_manifest.json.
 */
class SramViewerEditorPanel : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;

  explicit SramViewerEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.sram_viewer"; }
  std::string GetDisplayName() const override { return "SRAM Viewer"; }
  std::string GetIcon() const override { return ICON_MD_MEMORY; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Game State"; }
  std::string GetWorkflowDescription() const override {
    return "Inspect live SRAM variables from the active emulator session";
  }
  int GetPriority() const override { return 88; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief WindowContent for Mesen2 live screenshot preview
 *
 * Polls screenshots from a running Mesen2 emulator and renders them as a
 * GPU texture in the ImGui viewport.
 */
class MesenScreenshotEditorPanel : public WindowContent {
 public:
  using DrawCallback = std::function<void()>;

  explicit MesenScreenshotEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "agent.mesen_screenshot"; }
  std::string GetDisplayName() const override {
    return "Mesen2 Screenshot Preview";
  }
  std::string GetIcon() const override { return ICON_MD_CAMERA_ALT; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Live Debugging"; }
  std::string GetWorkflowDescription() const override {
    return "Preview live emulator screenshots while debugging hack changes";
  }
  int GetPriority() const override { return 82; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_AGENT_EDITOR_PANELS_H_
