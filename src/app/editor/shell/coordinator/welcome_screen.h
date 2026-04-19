#ifndef YAZE_APP_EDITOR_SHELL_COORDINATOR_WELCOME_SCREEN_H_
#define YAZE_APP_EDITOR_SHELL_COORDINATOR_WELCOME_SCREEN_H_

#include <cstdint>
#include <functional>
#include <string>

#include "app/editor/shell/coordinator/recent_projects_model.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class UserSettings;

/**
 * @class WelcomeScreen
 * @brief Modern welcome screen with project grid and quick actions
 */
class WelcomeScreen {
 public:
  WelcomeScreen();

  /**
   * @brief Show the welcome screen
   * @param p_open Pointer to open state
   * @return True if an action was taken (ROM opened, etc.)
   */
  bool Show(bool* p_open);

  /**
   * @brief Set callback for opening ROM
   */
  void SetOpenRomCallback(std::function<void()> callback) {
    open_rom_callback_ = callback;
  }

  /**
   * @brief Set callback for creating new project
   */
  void SetNewProjectCallback(std::function<void()> callback) {
    new_project_callback_ = callback;
  }

  /**
   * @brief Set callback for creating project with template
   */
  void SetNewProjectWithTemplateCallback(
      std::function<void(const std::string&)> callback) {
    new_project_with_template_callback_ = callback;
  }

  /**
   * @brief Set callback for opening project
   */
  void SetOpenProjectCallback(
      std::function<void(const std::string&)> callback) {
    open_project_callback_ = callback;
  }

  /**
   * @brief Set callback for opening AI Agent
   */
  void SetOpenAgentCallback(std::function<void()> callback) {
    open_agent_callback_ = callback;
  }

  /**
   * @brief Open the graphics editor focused on prototype research (no ROM).
   */
  void SetOpenPrototypeResearchCallback(std::function<void()> callback) {
    open_prototype_research_callback_ = std::move(callback);
  }

  /**
   * @brief Open the assembly editor for file/folder work (no ROM required).
   */
  void SetOpenAssemblyEditorNoRomCallback(std::function<void()> callback) {
    open_assembly_editor_no_rom_callback_ = std::move(callback);
  }

  /**
   * @brief Set callback for opening the project file dialog
   */
  void SetOpenProjectDialogCallback(std::function<void()> callback) {
    open_project_dialog_callback_ = callback;
  }

  /**
   * @brief Set callback for showing project management
   */
  void SetOpenProjectManagementCallback(std::function<void()> callback) {
    open_project_management_callback_ = callback;
  }

  /**
   * @brief Set callback for showing the project file editor
   */
  void SetOpenProjectFileEditorCallback(std::function<void()> callback) {
    open_project_file_editor_callback_ = callback;
  }

  /**
   * @brief Refresh recent projects list from the project manager.
   *
   * Cheap on the no-op path: compares the RecentFilesManager generation
   * counter against a cached value and short-circuits when unchanged.
   * Pass force=true to bypass the cache (e.g. on manual "Refresh").
   */
  void RefreshRecentProjects(bool force = false);

  /**
   * @brief Update animation time for dynamic effects
   */
  void UpdateAnimations();

  /**
   * @brief Check if screen should be shown
   */
  bool ShouldShow() const { return !manually_closed_; }

  /**
   * @brief Mark as manually closed (don't show again this session)
   */
  void MarkManuallyClosed() { manually_closed_ = true; }

  /**
   * @brief Set layout offsets for sidebar awareness
   * @param left Left sidebar width (0 if hidden)
   * @param right Right panel width (0 if hidden)
   */
  void SetLayoutOffsets(float left, float right) {
    left_offset_ = left;
    right_offset_ = right;
  }

  /**
   * @brief Set context state for gating actions
   */
  void SetContextState(bool has_rom, bool has_project) {
    has_rom_ = has_rom;
    has_project_ = has_project;
  }

  /**
   * @brief Wire persisted user settings so animation tweaks survive restart.
   *
   * Call once after construction (from UICoordinator). Animation sliders then
   * load initial values from the preferences and write back + trigger Save()
   * whenever the user adjusts them.
   */
  void SetUserSettings(UserSettings* settings);

  /**
   * @brief Accessor used by command-palette wiring so the same recent list
   * drives both the visual cards and the palette's per-entry actions.
   */
  RecentProjectsModel& recent_projects() { return recent_projects_model_; }
  const RecentProjectsModel& recent_projects() const {
    return recent_projects_model_;
  }

 private:
  void DrawHeader();
  void DrawQuickActions();
  void DrawRecentProjects();
  void DrawProjectPanel(const RecentProject& project, int index,
                        const ImVec2& card_size);
  void DrawTemplatesSection();
  void DrawTipsSection();
  void DrawWhatsNew();
  void DrawFirstRunGuide();
  void DrawRecentAnnotationPopup();
  void DrawUndoRemovalBanner();

  RecentProjectsModel recent_projects_model_;
  bool manually_closed_ = false;

  // Callbacks
  std::function<void()> open_rom_callback_;
  std::function<void()> new_project_callback_;
  std::function<void(const std::string&)> open_project_callback_;
  std::function<void(const std::string&)> new_project_with_template_callback_;
  std::function<void()> open_agent_callback_;
  std::function<void()> open_prototype_research_callback_;
  std::function<void()> open_assembly_editor_no_rom_callback_;
  std::function<void()> open_project_dialog_callback_;
  std::function<void()> open_project_management_callback_;
  std::function<void()> open_project_file_editor_callback_;

  // UI state
  int selected_template_ = 0;

  // Animation state
  float animation_time_ = 0.0f;
  float card_hover_scale_[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  int hovered_card_ = -1;

  // Staggered entry animations
  bool entry_animations_started_ = false;
  float entry_time_ = 0.0f;  // Time since welcome screen opened
  static constexpr float kEntryAnimDuration = 0.4f;   // Duration per section
  static constexpr float kEntryStaggerDelay = 0.08f;  // Delay between sections

  // Interactive triforce positions (smooth interpolation)
  static constexpr int kNumTriforces = 6;
  ImVec2 triforce_positions_[kNumTriforces] = {};
  ImVec2 triforce_base_positions_[kNumTriforces] = {};
  bool triforce_positions_initialized_ = false;

  // Particle system
  static constexpr int kMaxParticles = 50;
  struct Particle {
    ImVec2 position;
    ImVec2 velocity;
    float size;
    float alpha;
    float lifetime;
    float max_lifetime;
  };
  Particle particles_[kMaxParticles] = {};
  int active_particle_count_ = 0;
  // Fractional accumulator for particle spawning. Previously a static local
  // inside Show(), which silently shared state between (hypothetical) multiple
  // WelcomeScreen instances and couldn't be reset.
  float particle_spawn_accumulator_ = 0.0f;

  // Triforce animation settings
  bool show_triforce_settings_ = false;
  float triforce_alpha_multiplier_ = 1.0f;
  float triforce_speed_multiplier_ = 0.3f;  // Default slower speed
  float triforce_size_multiplier_ = 1.0f;
  bool triforce_mouse_repel_enabled_ = true;
  bool particles_enabled_ = true;
  float particle_spawn_rate_ = 2.0f;  // Particles per second

  // Layout offsets for sidebar awareness (so welcome screen centers in dockspace)
  float left_offset_ = 0.0f;
  float right_offset_ = 0.0f;

  // Context state for gating actions
  bool has_rom_ = false;
  bool has_project_ = false;

  // Inline popup state for rename / edit-notes flows triggered from the
  // recent-project context menu. Single-slot (one popup at a time).
  enum class RecentAnnotationKind { None, Rename, EditNotes };
  RecentAnnotationKind pending_annotation_kind_ = RecentAnnotationKind::None;
  std::string pending_annotation_path_;
  char rename_buffer_[256] = {};
  char notes_buffer_[1024] = {};

  // Optional link to persisted user preferences. When set, animation tweaks
  // flow back through it and survive app restarts.
  UserSettings* user_settings_ = nullptr;

  void PersistAnimationSettings();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_COORDINATOR_WELCOME_SCREEN_H_
