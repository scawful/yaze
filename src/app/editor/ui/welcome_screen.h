#ifndef YAZE_APP_EDITOR_UI_WELCOME_SCREEN_H_
#define YAZE_APP_EDITOR_UI_WELCOME_SCREEN_H_

#include <functional>
#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @struct RecentProject
 * @brief Information about a recently used project
 */
struct RecentProject {
  std::string name;
  std::string filepath;
  std::string rom_title;
  std::string last_modified;
  std::string thumbnail_path;  // Optional screenshot
  int days_ago = 0;
};

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
   * @brief Set callback for opening project
   */
  void SetOpenProjectCallback(std::function<void(const std::string&)> callback) {
    open_project_callback_ = callback;
  }
  
  /**
   * @brief Refresh recent projects list from the project manager
   */
  void RefreshRecentProjects();
  
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
  
 private:
  void DrawHeader();
  void DrawQuickActions();
  void DrawRecentProjects();
  void DrawProjectCard(const RecentProject& project, int index);
  void DrawTemplatesSection();
  void DrawTipsSection();
  void DrawWhatsNew();
  
  std::vector<RecentProject> recent_projects_;
  bool manually_closed_ = false;
  
  // Callbacks
  std::function<void()> open_rom_callback_;
  std::function<void()> new_project_callback_;
  std::function<void(const std::string&)> open_project_callback_;
  
  // UI state
  int selected_template_ = 0;
  static constexpr int kMaxRecentProjects = 6;
  
  // Animation state
  float animation_time_ = 0.0f;
  float card_hover_scale_[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
  int hovered_card_ = -1;
  
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
  
  // Triforce animation settings
  bool show_triforce_settings_ = false;
  float triforce_alpha_multiplier_ = 1.0f;
  float triforce_speed_multiplier_ = 0.3f;  // Default slower speed
  float triforce_size_multiplier_ = 1.0f;
  bool triforce_mouse_repel_enabled_ = true;
  bool particles_enabled_ = true;
  float particle_spawn_rate_ = 2.0f;  // Particles per second
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_WELCOME_SCREEN_H_
