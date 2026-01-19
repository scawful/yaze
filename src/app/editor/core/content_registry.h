#ifndef YAZE_APP_EDITOR_CORE_CONTENT_REGISTRY_H_
#define YAZE_APP_EDITOR_CORE_CONTENT_REGISTRY_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace yaze {
class Rom;
class EventBus;

namespace zelda3 {
class GameData;
}  // namespace zelda3

namespace editor {
class EditorPanel;
class Editor;
struct EditorDependencies;

/**
 * @brief Central registry for editor content and services.
 *
 * ContentRegistry provides a static, namespace-based API for accessing
 * shared resources across the editor. This pattern is inspired by ImHex's
 * ContentRegistry and provides a cleaner alternative to passing dependencies
 * through constructors.
 *
 * Usage:
 *   // Set context (usually in EditorManager::Initialize)
 *   ContentRegistry::Context::SetRom(rom_);
 *
 *   // Access from anywhere
 *   auto* rom = ContentRegistry::Context::rom();
 *
 * Thread Safety:
 *   All operations are protected by mutex for thread-safe access.
 */
namespace ContentRegistry {

/**
 * @brief Global context for accessing shared resources.
 *
 * Context provides access to the current ROM and other shared state.
 * This replaces the need for panels to receive dependencies via constructor.
 */
namespace Context {
  /**
   * @brief Get the current ROM instance.
   * @return Pointer to the current ROM, or nullptr if not set.
   */
  Rom* rom();

  /**
   * @brief Set the current ROM instance.
   * @param rom Pointer to the ROM to set as current.
   */
  void SetRom(Rom* rom);

  /**
   * @brief Get the current EventBus instance.
   * @return Pointer to the current EventBus, or nullptr if not set.
   */
  ::yaze::EventBus* event_bus();

  /**
   * @brief Set the current EventBus instance.
   * @param bus Pointer to the EventBus to set as current.
   */
  void SetEventBus(::yaze::EventBus* bus);

  /**
   * @brief Get the currently active editor.
   * @return Pointer to the current editor, or nullptr if none active.
   */
  Editor* current_editor();

  /**
   * @brief Set the currently active editor.
   * @param editor Pointer to the editor to set as current.
   *
   * Called when switching between editors (Overworld, Dungeon, etc.)
   */
  void SetCurrentEditor(Editor* editor);

  /**
   * @brief Get the current game data instance.
   * @return Pointer to the current GameData, or nullptr if not set.
   */
  ::yaze::zelda3::GameData* game_data();

  /**
   * @brief Set the current game data instance.
   * @param data Pointer to the GameData to set as current.
   *
   * Called when loading a ROM or switching sessions.
   */
  void SetGameData(::yaze::zelda3::GameData* data);

  /**
   * @brief Clear all context state.
   *
   * Called during shutdown or when switching sessions.
   */
  void Clear();
}  // namespace Context

/**
 * @brief Registry for editor panels.
 *
 * Panels can register themselves here for centralized management.
 * This enables features like:
 * - Discovery of available panels
 * - Lazy initialization
 * - Lifecycle management
 */
namespace Panels {
  using PanelFactory = std::function<std::unique_ptr<EditorPanel>()>;

  /**
   * @brief Register a panel factory.
   * @param factory Function that creates a new instance of the panel.
   */
  void RegisterFactory(PanelFactory factory);

  /**
   * @brief Register a panel type for auto-creation.
   * @tparam T The panel class to register (must have default constructor).
   */
  template<typename T>
  void add() {
    RegisterFactory([]() { return std::make_unique<T>(); });
  }

  /**
   * @brief Create new instances of all registered panels.
   * @return Vector of unique pointers to new panel instances.
   */
  std::vector<std::unique_ptr<EditorPanel>> CreateAll();

  /**
   * @brief Register a panel instance (Legacy/Global).
   * @param panel Unique pointer to the panel to register.
   */
  void Register(std::unique_ptr<EditorPanel> panel);

  /**
   * @brief Get all registered panels.
   * @return Vector of raw pointers to all registered panels.
   */
  std::vector<EditorPanel*> GetAll();

  /**
   * @brief Get a specific panel by its ID.
   * @param id The unique identifier of the panel.
   * @return Pointer to the panel, or nullptr if not found.
   */
  EditorPanel* Get(const std::string& id);

  /**
   * @brief Clear all registered panels.
   *
   * Called during shutdown for cleanup.
   */
  void Clear();
}  // namespace Panels

/**
 * @brief Registry for Editors (full workspace tabs).
 */
namespace Editors {
  using EditorFactory = std::function<std::unique_ptr<Editor>(const EditorDependencies&)>;

  void RegisterFactory(EditorFactory factory);

  template<typename T>
  void add() {
    RegisterFactory([](const EditorDependencies& deps) {
      auto editor = std::make_unique<T>();
      editor->SetDependencies(deps);
      return editor;
    });
  }

  std::vector<std::unique_ptr<Editor>> CreateAll(const EditorDependencies& deps);
} // namespace Editors

/**
 * @brief Registry for Keyboard Shortcuts.
 * Allows components to declare their shortcuts statically.
 */
namespace Shortcuts {
  struct ShortcutDef {
    std::string id;
    std::string default_key;
    std::string description;
  };

  void Register(const ShortcutDef& shortcut);
  void add(const std::string& id, const std::string& key, const std::string& desc);
  std::vector<ShortcutDef> GetAll();
} // namespace Shortcuts

/**
 * @brief Registry for User Settings.
 * Allows components to declare their settings statically.
 */
namespace Settings {
  struct SettingDef {
    std::string key;
    std::string section;
    std::string default_value; // serialized
    std::string description;
  };

  void Register(const SettingDef& setting);
  void add(const std::string& section, const std::string& key, const std::string& default_val, const std::string& desc);
  std::vector<SettingDef> GetAll();
} // namespace Settings

}  // namespace ContentRegistry

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CORE_CONTENT_REGISTRY_H_
