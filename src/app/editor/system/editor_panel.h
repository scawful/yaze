#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_PANEL_H_

#include <any>
#include <functional>
#include <string>
#include <unordered_map>

namespace yaze {
namespace editor {

/**
 * @enum PanelCategory
 * @brief Defines lifecycle behavior for editor panels
 *
 * Panels are categorized by how they behave when switching between editors:
 * - EditorBound: Hidden when switching away from parent editor (default)
 * - Persistent: Remains visible across all editor switches
 * - CrossEditor: User can "pin" to make persistent (opt-in by user)
 */
enum class PanelCategory {
  EditorBound,  ///< Hidden when switching editors (default)
  Persistent,   ///< Always visible once shown
  CrossEditor   ///< User can pin to persist across editors
};

/**
 * @enum PanelScope
 * @brief Defines whether a panel is session-scoped or global
 *
 * Session-scoped panels are registered per session (with optional prefixing).
 * Global panels share a single descriptor across all sessions.
 */
enum class PanelScope {
  kSession,
  kGlobal
};

/**
 * @class EditorPanel
 * @brief Base interface for all logical panel components
 *
 * EditorPanel represents a logical UI component that draws content within
 * a panel window. This is distinct from PanelWindow (the ImGui wrapper that
 * draws the window chrome/title bar).
 *
 * The separation allows:
 * - PanelManager to handle window creation/visibility centrally
 * - Panel components to focus purely on content drawing
 * - Consistent window behavior across all editors
 *
 * @section Usage Example
 * ```cpp
 * class UsageStatisticsPanel : public EditorPanel {
 *  public:
 *   std::string GetId() const override { return "overworld.usage_stats"; }
 *   std::string GetDisplayName() const override { return "Usage Statistics"; }
 *   std::string GetIcon() const override { return ICON_MD_ANALYTICS; }
 *   std::string GetEditorCategory() const override { return "Overworld"; }
 *
 *   void Draw(bool* p_open) override {
 *     // Draw your content here - no ImGui::Begin/End needed
 *     // PanelManager handles the window wrapper
 *     DrawUsageGrid();
 *     DrawUsageStates();
 *   }
 * };
 * ```
 *
 * @see PanelWindow - The ImGui window wrapper (draws chrome)
 * @see PanelManager - Central registry that manages panel lifecycle
 * @see PanelDescriptor - Metadata struct for panel registration
 */
class EditorPanel {
 public:
  virtual ~EditorPanel() = default;

  // ==========================================================================
  // Identity (Required)
  // ==========================================================================

  /**
   * @brief Unique identifier for this panel
   * @return Panel ID in format "{category}.{name}" (e.g., "dungeon.room_selector")
   *
   * IDs should be:
   * - Lowercase with underscores
   * - Prefixed with editor category
   * - Unique across all panels
   */
  virtual std::string GetId() const = 0;

  /**
   * @brief Human-readable name shown in menus and title bars
   * @return Display name (e.g., "Room Selector")
   */
  virtual std::string GetDisplayName() const = 0;

  /**
   * @brief Material Design icon for this panel
   * @return Icon constant (e.g., ICON_MD_LIST)
   */
  virtual std::string GetIcon() const = 0;

  /**
   * @brief Editor category this panel belongs to
   * @return Category name matching EditorType (e.g., "Dungeon", "Overworld")
   */
  virtual std::string GetEditorCategory() const = 0;

  // ==========================================================================
  // Drawing (Required)
  // ==========================================================================

  /**
   * @brief Draw the panel content
   * @param p_open Pointer to visibility flag (nullptr if not closable)
   *
   * Called by PanelManager when the panel is visible.
   * Do NOT call ImGui::Begin/End - the PanelWindow wrapper handles that.
   * Just draw your content directly.
   */
  virtual void Draw(bool* p_open) = 0;

  // ==========================================================================
  // Lifecycle Hooks (Optional)
  // ==========================================================================

  /**
   * @brief Called once before the first Draw() in a session
   *
   * Use this for expensive one-time initialization that should be deferred
   * until the panel is actually visible. This avoids loading resources for
   * panels that may never be opened.
   *
   * @note Reset via InvalidateLazyInit() on session switches if needed.
   * @see RequiresLazyInit()
   */
  virtual void OnFirstDraw() {}

  /**
   * @brief Whether this panel uses lazy initialization
   * @return true if OnFirstDraw() should be called before first Draw()
   *
   * Override to return true if your panel has expensive setup in OnFirstDraw().
   * When false (default), OnFirstDraw() is skipped for performance.
   */
  virtual bool RequiresLazyInit() const { return false; }

  /**
   * @brief Reset lazy init state so OnFirstDraw() runs again
   *
   * Call this when panel state needs to be reinitialized (e.g., session switch).
   * The next Draw() will trigger OnFirstDraw() again.
   */
  void InvalidateLazyInit();  // Implementation below private member

  /**
   * @brief Called when panel becomes visible
   *
   * Use this to initialize state, load resources, or start animations.
   * Called after the panel is shown but before first Draw().
   */
  virtual void OnOpen() {}

  /**
   * @brief Called when panel is hidden
   *
   * Use this to cleanup state, release resources, or save state.
   * Called after the panel is hidden.
   */
  virtual void OnClose() {}

  /**
   * @brief Called when panel receives focus
   *
   * Use this to update state based on becoming the active panel.
   */
  virtual void OnFocus() {}

  // ==========================================================================
  // Behavior (Optional)
  // ==========================================================================

  /**
   * @brief Get the lifecycle category for this panel
   * @return PanelCategory determining visibility behavior on editor switch
   *
   * Default is EditorBound (hidden when switching editors).
   */
  virtual PanelCategory GetPanelCategory() const {
    return PanelCategory::EditorBound;
  }

  /**
   * @brief Get the registration scope for this panel
   * @return PanelScope indicating session or global scope
   *
   * Default is session-scoped.
   */
  virtual PanelScope GetScope() const { return PanelScope::kSession; }

  /**
   * @brief Check if this panel is currently enabled
   * @return true if panel can be shown, false if disabled
   *
   * Disabled panels appear grayed out in menus.
   * Override to implement conditional availability (e.g., requires ROM loaded).
   */
  virtual bool IsEnabled() const { return true; }

  /**
   * @brief Get tooltip text when panel is disabled
   * @return Explanation of why panel is disabled (e.g., "Requires ROM to be loaded")
   */
  virtual std::string GetDisabledTooltip() const { return ""; }

  /**
   * @brief Get keyboard shortcut hint for display
   * @return Shortcut string (e.g., "Ctrl+Shift+R")
   */
  virtual std::string GetShortcutHint() const { return ""; }

  /**
   * @brief Get display priority for menu ordering
   * @return Priority value (lower = higher in list, default 50)
   */
  virtual int GetPriority() const { return 50; }

  /**
   * @brief Get preferred width for this panel (optional)
   * @return Preferred width in pixels, or 0 to use default (250px)
   *
   * Override this to specify content-based sizing. For example, a tile
   * selector with 8 tiles at 16px × 2.0 scale would return ~276px.
   */
  virtual float GetPreferredWidth() const { return 0.0f; }

  /**
   * @brief Whether this panel should be visible by default
   * @return true if panel should be visible when editor first opens
   *
   * Override this to set panels as visible by default.
   * Most panels default to hidden to reduce UI clutter.
   */
  virtual bool IsVisibleByDefault() const { return false; }

  // ==========================================================================
  // Relationships (Optional)
  // ==========================================================================

  /**
   * @brief Get parent panel ID for cascade behavior
   * @return Parent panel ID, or empty string if no parent
   *
   * If set, this panel may be automatically closed when parent closes
   * (depending on parent's CascadeCloseChildren() setting).
   */
  virtual std::string GetParentPanelId() const { return ""; }

  /**
   * @brief Whether closing this panel should close child panels
   * @return true to cascade close to children, false to leave children open
   *
   * Only affects panels that have this panel as their parent.
   */
  virtual bool CascadeCloseChildren() const { return false; }

  // ==========================================================================
  // Internal State (Managed by PanelManager)
  // ==========================================================================

  /**
   * @brief Execute lazy initialization if needed, then call Draw()
   * @param p_open Pointer to visibility flag
   *
   * Called by PanelManager. Handles OnFirstDraw() invocation automatically.
   */
  void DrawWithLazyInit(bool* p_open) {
    if (RequiresLazyInit() && !lazy_init_done_) {
      OnFirstDraw();
      lazy_init_done_ = true;
    }
    Draw(p_open);
  }

 protected:
  // ==========================================================================
  // Caching Infrastructure (For Derived Panels)
  // ==========================================================================

  /**
   * @brief Invalidate all cached computations
   *
   * Call this when the underlying data changes and cached values are stale.
   * Common triggers:
   * - Session switch (data context changes)
   * - ROM data modification
   * - Settings changes that affect computed values
   */
  void InvalidateCache() { cache_valid_ = false; }

  /**
   * @brief Get or compute a cached value
   * @tparam T The type of the cached value
   * @param key Unique identifier for this cached value
   * @param compute Function to compute the value if not cached
   * @return Reference to the cached value
   *
   * Example usage:
   * ```cpp
   * int GetExpensiveResult() {
   *   return GetCached<int>("expensive_result", [this]() {
   *     // Expensive computation here
   *     return ComputeExpensiveValue();
   *   });
   * }
   * ```
   *
   * @note The cache is invalidated when InvalidateCache() is called.
   * @note Values are stored using std::any, so complex types work fine.
   */
  template <typename T>
  T& GetCached(const std::string& key, std::function<T()> compute) {
    if (!cache_valid_ || cache_.find(key) == cache_.end()) {
      cache_[key] = compute();
      cache_valid_ = true;  // Mark valid after first successful computation
    }
    return std::any_cast<T&>(cache_[key]);
  }

  /**
   * @brief Check if cache has been invalidated
   * @return true if cache is valid, false if InvalidateCache() was called
   */
  bool IsCacheValid() const { return cache_valid_; }

  /**
   * @brief Clear all cached values (more aggressive than InvalidateCache)
   *
   * Use this when you need to free memory, not just mark values as stale.
   * InvalidateCache() just marks the cache as invalid but keeps values for
   * potential debugging or lazy re-computation.
   */
  void ClearCache() {
    cache_.clear();
    cache_valid_ = false;
  }

 private:
  bool lazy_init_done_ = false;

  // Cache infrastructure
  bool cache_valid_ = false;
  std::unordered_map<std::string, std::any> cache_;
};

// Inline implementation (requires private member to be declared first)
inline void EditorPanel::InvalidateLazyInit() { lazy_init_done_ = false; }

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_PANEL_H_
