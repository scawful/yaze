#ifndef YAZE_APP_GFX_PALETTE_MANAGER_H
#define YAZE_APP_GFX_PALETTE_MANAGER_H

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "app/rom.h"

namespace yaze {
namespace gfx {

/**
 * @brief Represents a single color change operation
 */
struct PaletteColorChange {
  std::string group_name;        ///< Palette group name (e.g., "ow_main")
  int palette_index;             ///< Index of palette within group
  int color_index;               ///< Index of color within palette
  SnesColor original_color;      ///< Original color before change
  SnesColor new_color;           ///< New color after change
  uint64_t timestamp_ms;         ///< Timestamp in milliseconds
};

/**
 * @brief Event notification for palette changes
 */
struct PaletteChangeEvent {
  enum class Type {
    kColorChanged,        ///< Single color was modified
    kPaletteReset,        ///< Entire palette was reset
    kGroupSaved,          ///< Palette group was saved to ROM
    kGroupDiscarded,      ///< Palette group changes were discarded
    kAllSaved,            ///< All changes saved to ROM
    kAllDiscarded         ///< All changes discarded
  };

  Type type;
  std::string group_name;
  int palette_index = -1;
  int color_index = -1;
};

/**
 * @brief Centralized palette management system
 *
 * Singleton coordinator for ALL palette editing operations.
 * Provides:
 * - Global dirty tracking across all palette groups
 * - Transaction-based editing with automatic ROM synchronization
 * - Unified undo/redo stack shared across all editors
 * - Batch operations (save all, discard all)
 * - Change notifications via observer pattern
 * - Conflict resolution when multiple editors modify same palette
 *
 * Thread-safety: This class is NOT thread-safe. All operations must
 * be called from the main UI thread.
 */
class PaletteManager {
 public:
  using ChangeCallback = std::function<void(const PaletteChangeEvent&)>;

  /// Get the singleton instance
  static PaletteManager& Get() {
    static PaletteManager instance;
    return instance;
  }

  // Delete copy/move constructors and assignment operators
  PaletteManager(const PaletteManager&) = delete;
  PaletteManager& operator=(const PaletteManager&) = delete;
  PaletteManager(PaletteManager&&) = delete;
  PaletteManager& operator=(PaletteManager&&) = delete;

  // ========== Initialization ==========

  /**
   * @brief Initialize the palette manager with ROM data
   * @param rom Pointer to ROM instance (must outlive PaletteManager)
   */
  void Initialize(Rom* rom);

  /**
   * @brief Check if manager is initialized
   */
  bool IsInitialized() const { return rom_ != nullptr; }

  // ========== Color Operations ==========

  /**
   * @brief Get a color from a palette
   * @param group_name Palette group name
   * @param palette_index Palette index within group
   * @param color_index Color index within palette
   * @return The color, or default SnesColor if invalid indices
   */
  SnesColor GetColor(const std::string& group_name, int palette_index,
                     int color_index) const;

  /**
   * @brief Set a color in a palette (records change for undo)
   * @param group_name Palette group name
   * @param palette_index Palette index within group
   * @param color_index Color index within palette
   * @param new_color The new color value
   * @return Status of the operation
   */
  absl::Status SetColor(const std::string& group_name, int palette_index,
                        int color_index, const SnesColor& new_color);

  /**
   * @brief Reset a single color to its original ROM value
   */
  absl::Status ResetColor(const std::string& group_name, int palette_index,
                          int color_index);

  /**
   * @brief Reset an entire palette to original ROM values
   */
  absl::Status ResetPalette(const std::string& group_name, int palette_index);

  // ========== Dirty Tracking ==========

  /**
   * @brief Check if there are ANY unsaved changes
   */
  bool HasUnsavedChanges() const;

  /**
   * @brief Get list of modified palette group names
   */
  std::vector<std::string> GetModifiedGroups() const;

  /**
   * @brief Check if a specific palette group has modifications
   */
  bool IsGroupModified(const std::string& group_name) const;

  /**
   * @brief Check if a specific palette is modified
   */
  bool IsPaletteModified(const std::string& group_name,
                         int palette_index) const;

  /**
   * @brief Check if a specific color is modified
   */
  bool IsColorModified(const std::string& group_name, int palette_index,
                       int color_index) const;

  /**
   * @brief Get count of modified colors across all groups
   */
  size_t GetModifiedColorCount() const;

  // ========== Persistence ==========

  /**
   * @brief Save a specific palette group to ROM
   */
  absl::Status SaveGroup(const std::string& group_name);

  /**
   * @brief Save ALL modified palettes to ROM
   */
  absl::Status SaveAllToRom();

  /**
   * @brief Discard changes for a specific group
   */
  void DiscardGroup(const std::string& group_name);

  /**
   * @brief Discard ALL unsaved changes
   */
  void DiscardAllChanges();

  // ========== Undo/Redo ==========

  /**
   * @brief Undo the most recent change
   */
  void Undo();

  /**
   * @brief Redo the most recently undone change
   */
  void Redo();

  /**
   * @brief Check if undo is available
   */
  bool CanUndo() const { return !undo_stack_.empty(); }

  /**
   * @brief Check if redo is available
   */
  bool CanRedo() const { return !redo_stack_.empty(); }

  /**
   * @brief Get size of undo stack
   */
  size_t GetUndoStackSize() const { return undo_stack_.size(); }

  /**
   * @brief Get size of redo stack
   */
  size_t GetRedoStackSize() const { return redo_stack_.size(); }

  /**
   * @brief Clear undo/redo history
   */
  void ClearHistory();

  // ========== Change Notifications ==========

  /**
   * @brief Register a callback for palette change events
   * @return Unique ID for this callback (use to unregister)
   */
  int RegisterChangeListener(ChangeCallback callback);

  /**
   * @brief Unregister a change listener
   */
  void UnregisterChangeListener(int callback_id);

  // ========== Batch Operations ==========

  /**
   * @brief Begin a batch operation (groups multiple changes into one undo step)
   * @note Must be paired with EndBatch()
   */
  void BeginBatch();

  /**
   * @brief End a batch operation
   */
  void EndBatch();

  /**
   * @brief Check if currently in a batch operation
   */
  bool InBatch() const { return batch_depth_ > 0; }

 private:
  PaletteManager() = default;
  ~PaletteManager() = default;

  /// Helper: Get mutable palette group
  PaletteGroup* GetMutableGroup(const std::string& group_name);

  /// Helper: Get const palette group
  const PaletteGroup* GetGroup(const std::string& group_name) const;

  /// Helper: Get original color from snapshot
  SnesColor GetOriginalColor(const std::string& group_name, int palette_index,
                              int color_index) const;

  /// Helper: Record a change for undo
  void RecordChange(const PaletteColorChange& change);

  /// Helper: Notify all listeners of an event
  void NotifyListeners(const PaletteChangeEvent& event);

  /// Helper: Mark a color as modified
  void MarkModified(const std::string& group_name, int palette_index,
                    int color_index);

  /// Helper: Clear modified flags for a group
  void ClearModifiedFlags(const std::string& group_name);

  // ========== Member Variables ==========

  /// ROM instance (not owned)
  Rom* rom_ = nullptr;

  /// Original palette snapshots (loaded from ROM for reset/comparison)
  /// Key: group_name, Value: vector of original palettes
  std::unordered_map<std::string, std::vector<SnesPalette>>
      original_palettes_;

  /// Modified tracking
  /// Key: group_name, Value: set of modified palette indices
  std::unordered_map<std::string, std::unordered_set<int>>
      modified_palettes_;

  /// Detailed color modification tracking
  /// Key: group_name, Value: map of palette_index -> set of color indices
  std::unordered_map<std::string,
                     std::unordered_map<int, std::unordered_set<int>>>
      modified_colors_;

  /// Undo/redo stacks
  std::deque<PaletteColorChange> undo_stack_;
  std::deque<PaletteColorChange> redo_stack_;
  static constexpr size_t kMaxUndoHistory = 500;

  /// Change listeners
  std::unordered_map<int, ChangeCallback> change_listeners_;
  int next_callback_id_ = 1;

  /// Batch operation support
  int batch_depth_ = 0;
  std::vector<PaletteColorChange> batch_changes_;
};

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_MANAGER_H
