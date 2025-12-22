#ifndef YAZE_APP_PLATFORM_WASM_CONTROL_API_H_
#define YAZE_APP_PLATFORM_WASM_CONTROL_API_H_

#ifdef __EMSCRIPTEN__

#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {

// Forward declarations
class Rom;

namespace editor {
class EditorManager;
class PanelManager;
}  // namespace editor

namespace app {
namespace platform {

/**
 * @brief Unified WASM Control API for browser/agent access
 *
 * Provides programmatic control over the editor UI from JavaScript.
 * Exposed as window.yaze.control.* in the browser.
 *
 * API Surface:
 * - window.yaze.control.switchEditor("Overworld")
 * - window.yaze.control.openPanel("dungeon.room_selector")
 * - window.yaze.control.closePanel("dungeon.room_selector")
 * - window.yaze.control.togglePanel("dungeon.room_selector")
 * - window.yaze.control.triggerMenuAction("File.Save")
 * - window.yaze.control.setPanelLayout("dungeon_default")
 * - window.yaze.control.getVisiblePanels()
 * - window.yaze.control.getCurrentEditor()
 * - window.yaze.control.getAvailableEditors()
 * - window.yaze.control.getAvailablePanels()
 */
class WasmControlApi {
 public:
  /**
   * @brief Initialize the control API with editor manager reference
   * @param editor_manager Pointer to the main editor manager
   */
  static void Initialize(editor::EditorManager* editor_manager);

  /**
   * @brief Check if the control API is ready
   * @return true if initialized and ready for use
   */
  static bool IsReady();

  /**
   * @brief Setup JavaScript bindings for window.yaze.control
   */
  static void SetupJavaScriptBindings();

  // ============================================================================
  // Editor Control
  // ============================================================================

  /**
   * @brief Switch to a specific editor by name
   * @param editor_name Name of editor ("Overworld", "Dungeon", "Graphics", etc.)
   * @return JSON result with success/error
   */
  static std::string SwitchEditor(const std::string& editor_name);

  /**
   * @brief Get the currently active editor name
   * @return JSON with editor name and type
   */
  static std::string GetCurrentEditor();

  /**
   * @brief Get list of available editors
   * @return JSON array of editor info objects
   */
  static std::string GetAvailableEditors();

  // ============================================================================
  // Panel Control
  // ============================================================================

  /**
   * @brief Open/show a panel by ID
   * @param card_id Panel identifier (e.g., "dungeon.room_selector")
   * @return JSON result with success/error
   */
  static std::string OpenPanel(const std::string& card_id);

  /**
   * @brief Close/hide a panel by ID
   * @param card_id Panel identifier
   * @return JSON result with success/error
   */
  static std::string ClosePanel(const std::string& card_id);

  /**
   * @brief Toggle a panel's visibility
   * @param card_id Panel identifier
   * @return JSON result with new visibility state
   */
  static std::string TogglePanel(const std::string& card_id);

  /**
   * @brief Get list of currently visible panels
   * @return JSON array of visible panel IDs
   */
  static std::string GetVisiblePanels();

  /**
   * @brief Get all available panels for current session
   * @return JSON array of panel info objects
   */
  static std::string GetAvailablePanels();

  /**
   * @brief Get panels for a specific category
   * @param category Category name (e.g., "Dungeon", "Overworld")
   * @return JSON array of panel info objects
   */
  static std::string GetPanelsInCategory(const std::string& category);

  /**
   * @brief Show all panels in the current session
   * @return JSON result with success/error
   */
  static std::string ShowAllPanels();

  /**
   * @brief Hide all panels in the current session
   * @return JSON result with success/error
   */
  static std::string HideAllPanels();

  /**
   * @brief Show all panels in a specific category
   * @param category Category name
   * @return JSON result with success/error
   */
  static std::string ShowAllPanelsInCategory(const std::string& category);

  /**
   * @brief Hide all panels in a specific category
   * @param category Category name
   * @return JSON result with success/error
   */
  static std::string HideAllPanelsInCategory(const std::string& category);

  /**
   * @brief Show only one panel, hiding all others in its category
   * @param card_id Panel identifier
   * @return JSON result with success/error
   */
  static std::string ShowOnlyPanel(const std::string& card_id);

  // ============================================================================
  // Layout Control
  // ============================================================================

  /**
   * @brief Apply a predefined panel layout
   * @param layout_name Layout preset name ("dungeon_default", "overworld_default", etc.)
   * @return JSON result with success/error
   */
  static std::string SetPanelLayout(const std::string& layout_name);

  /**
   * @brief Get list of available layout presets
   * @return JSON array of layout names
   */
  static std::string GetAvailableLayouts();

  /**
   * @brief Save current panel visibility as a custom layout
   * @param layout_name Name for the new layout
   * @return JSON result with success/error
   */
  static std::string SaveCurrentLayout(const std::string& layout_name);

  // ============================================================================
  // Menu/UI Actions
  // ============================================================================

  /**
   * @brief Trigger a menu action by path
   * @param action_path Menu path (e.g., "File.Save", "Edit.Undo", "View.ShowEmulator")
   * @return JSON result with success/error
   */
  static std::string TriggerMenuAction(const std::string& action_path);

  /**
   * @brief Get list of available menu actions
   * @return JSON array of action paths
   */
  static std::string GetAvailableMenuActions();
  
  /**
   * @brief Toggle the visibility of the menu bar
   * @return JSON result with new visibility state
   */
  static std::string ToggleMenuBar();

  // ============================================================================
  // Session Control
  // ============================================================================

  /**
   * @brief Get current session information
   * @return JSON with session ID, ROM info, editor state
   */
  static std::string GetSessionInfo();

  /**
   * @brief Create a new editing session
   * @return JSON with new session info
   */
  static std::string CreateSession();

  /**
   * @brief Switch to a different session by index
   * @param session_index Session index to switch to
   * @return JSON result with success/error
   */
  static std::string SwitchSession(int session_index);

  // ============================================================================
  // ROM Control
  // ============================================================================

  /**
   * @brief Get ROM status and basic info
   * @return JSON with ROM loaded state, filename, size
   */
  static std::string GetRomStatus();

  /**
   * @brief Read bytes from ROM
   * @param address ROM address
   * @param count Number of bytes to read
   * @return JSON with byte array
   */
  static std::string ReadRomBytes(int address, int count);

  /**
   * @brief Write bytes to ROM
   * @param address ROM address
   * @param bytes JSON array of bytes to write
   * @return JSON result with success/error
   */
  static std::string WriteRomBytes(int address, const std::string& bytes_json);

  /**
   * @brief Trigger ROM save
   * @return JSON result with success/error
   */
  static std::string SaveRom();

  // ============================================================================
  // Editor State APIs (for LLM agents and automation)
  // ============================================================================

  /**
   * @brief Get a comprehensive snapshot of the current editor state
   * @return JSON with editor_type, active_data, visible_cards, etc.
   */
  static std::string GetEditorSnapshot();

  /**
   * @brief Get current dungeon room information
   * @return JSON with room_id, room_count, active_rooms, etc.
   */
  static std::string GetCurrentDungeonRoom();

  /**
   * @brief Get current overworld map information
   * @return JSON with map_id, world, game_state, etc.
   */
  static std::string GetCurrentOverworldMap();

  /**
   * @brief Get the current selection in the active editor
   * @return JSON with selected items/entities
   */
  static std::string GetEditorSelection();

  // ============================================================================
  // Read-only Data APIs
  // ============================================================================

  /**
   * @brief Get dungeon room tile data
   * @param room_id Room ID (0-295)
   * @return JSON with layer1, layer2 tile arrays
   */
  static std::string GetRoomTileData(int room_id);

  /**
   * @brief Get objects in a dungeon room
   * @param room_id Room ID (0-295)
   * @return JSON array of room objects
   */
  static std::string GetRoomObjects(int room_id);

  /**
   * @brief Get dungeon room properties
   * @param room_id Room ID (0-295)
   * @return JSON with music, palette, tileset, etc.
   */
  static std::string GetRoomProperties(int room_id);

  /**
   * @brief Get overworld map tile data
   * @param map_id Map ID (0-159)
   * @return JSON with tile array
   */
  static std::string GetMapTileData(int map_id);

  /**
   * @brief Get entities on an overworld map
   * @param map_id Map ID (0-159)
   * @return JSON with entrances, exits, items, sprites
   */
  static std::string GetMapEntities(int map_id);

  /**
   * @brief Get overworld map properties
   * @param map_id Map ID (0-159)
   * @return JSON with gfx_group, palette_group, area_size, etc.
   */
  static std::string GetMapProperties(int map_id);

  /**
   * @brief Get palette colors
   * @param group_name Palette group name
   * @param palette_id Palette ID within group
   * @return JSON with colors array
   */
  static std::string GetPaletteData(const std::string& group_name, int palette_id);

  /**
   * @brief Get list of available palette groups
   * @return JSON array of group names
   */
  static std::string ListPaletteGroups();

  /**
   * @brief Load a font from binary data
   * @param name Font name
   * @param data Binary font data
   * @param size Font size
   * @return JSON result with success/error
   */
  static std::string LoadFont(const std::string& name, const std::string& data, float size);

  // ============================================================================
  // GUI Automation APIs (for LLM agents)
  // ============================================================================

  /**
   * @brief Get the UI element tree for automation
   * @return JSON with UI elements, their bounds, and types
   */
  static std::string GetUIElementTree();

  /**
   * @brief Get bounds of a specific UI element by ID
   * @param element_id Element identifier
   * @return JSON with x, y, width, height, visible
   */
  static std::string GetUIElementBounds(const std::string& element_id);

  /**
   * @brief Set selection in active editor
   * @param ids_json JSON array of IDs to select
   * @return JSON result with success/error
   */
  static std::string SetSelection(const std::string& ids_json);

  // ============================================================================
  // Platform Info API
  // ============================================================================

  /**
   * @brief Get platform information for keyboard shortcuts and UI display
   * @return JSON with platform name, is_mac, ctrl_name, alt_name
   *
   * Example response:
   * {
   *   "platform": "WebMac",
   *   "is_mac": true,
   *   "ctrl_display": "Cmd",
   *   "alt_display": "Opt",
   *   "shift_display": "Shift"
   * }
   */
  static std::string GetPlatformInfo();

  // ============================================================================
  // Agent API (for AI/LLM agent integration)
  // ============================================================================

  /**
   * @brief Check if agent system is ready
   * @return true if agent is initialized and available
   */
  static bool AgentIsReady();

  /**
   * @brief Send a message to the AI agent
   * @param message User message to send
   * @return JSON with response, status, proposals (if any)
   */
  static std::string AgentSendMessage(const std::string& message);

  /**
   * @brief Get chat history
   * @return JSON array of chat messages
   */
  static std::string AgentGetChatHistory();

  /**
   * @brief Get current agent configuration
   * @return JSON with provider, model, host, etc.
   */
  static std::string AgentGetConfig();

  /**
   * @brief Set agent configuration
   * @param config_json JSON configuration object
   * @return JSON result with success/error
   */
  static std::string AgentSetConfig(const std::string& config_json);

  /**
   * @brief Get available AI providers
   * @return JSON array of provider info
   */
  static std::string AgentGetProviders();

  /**
   * @brief Get list of pending/recent proposals
   * @return JSON array of proposal info
   */
  static std::string AgentGetProposals();

  /**
   * @brief Accept a proposal by ID
   * @param proposal_id Proposal ID to accept
   * @return JSON result with success/error
   */
  static std::string AgentAcceptProposal(const std::string& proposal_id);

  /**
   * @brief Reject a proposal by ID
   * @param proposal_id Proposal ID to reject
   * @return JSON result with success/error
   */
  static std::string AgentRejectProposal(const std::string& proposal_id);

  /**
   * @brief Get detailed proposal information
   * @param proposal_id Proposal ID
   * @return JSON with proposal details, diff, etc.
   */
  static std::string AgentGetProposalDetails(const std::string& proposal_id);

  /**
   * @brief Open the agent sidebar
   * @return JSON result with success/error
   */
  static std::string AgentOpenSidebar();

  /**
   * @brief Close the agent sidebar
   * @return JSON result with success/error
   */
  static std::string AgentCloseSidebar();

 private:
  static editor::EditorManager* editor_manager_;
  static bool initialized_;

  // Helper to get card registry
  static editor::PanelManager* GetPanelRegistry();

  // Helper to convert EditorType to string
  static std::string EditorTypeToString(int type);

  // Helper to convert string to EditorType
  static int StringToEditorType(const std::string& name);
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub for non-WASM builds
namespace yaze {
namespace editor {
class EditorManager;
}

namespace app {
namespace platform {

class WasmControlApi {
 public:
  static void Initialize(editor::EditorManager*) {}
  static bool IsReady() { return false; }
  static void SetupJavaScriptBindings() {}
  static std::string SwitchEditor(const std::string&) { return "{}"; }
  static std::string GetCurrentEditor() { return "{}"; }
  static std::string GetAvailableEditors() { return "[]"; }
  static std::string OpenPanel(const std::string&) { return "{}"; }
  static std::string ClosePanel(const std::string&) { return "{}"; }
  static std::string TogglePanel(const std::string&) { return "{}"; }
  static std::string GetVisiblePanels() { return "[]"; }
  static std::string GetAvailablePanels() { return "[]"; }
  static std::string GetPanelsInCategory(const std::string&) { return "[]"; }
  static std::string ShowAllPanels() { return "{}"; }
  static std::string HideAllPanels() { return "{}"; }
  static std::string ShowAllPanelsInCategory(const std::string&) { return "{}"; }
  static std::string HideAllPanelsInCategory(const std::string&) { return "{}"; }
  static std::string ShowOnlyPanel(const std::string&) { return "{}"; }
  static std::string SetPanelLayout(const std::string&) { return "{}"; }
  static std::string GetAvailableLayouts() { return "[]"; }
  static std::string SaveCurrentLayout(const std::string&) { return "{}"; }
  static std::string TriggerMenuAction(const std::string&) { return "{}"; }
  static std::string GetAvailableMenuActions() { return "[]"; }
  static std::string ToggleMenuBar() { return "{}"; }
  static std::string GetSessionInfo() { return "{}"; }
  static std::string CreateSession() { return "{}"; }
  static std::string SwitchSession(int) { return "{}"; }
  static std::string GetRomStatus() { return "{}"; }
  static std::string ReadRomBytes(int, int) { return "{}"; }
  static std::string WriteRomBytes(int, const std::string&) { return "{}"; }
  static std::string SaveRom() { return "{}"; }
  // Editor State APIs
  static std::string GetEditorSnapshot() { return "{}"; }
  static std::string GetCurrentDungeonRoom() { return "{}"; }
  static std::string GetCurrentOverworldMap() { return "{}"; }
  static std::string GetEditorSelection() { return "{}"; }
  // Read-only Data APIs
  static std::string GetRoomTileData(int) { return "{}"; }
  static std::string GetRoomObjects(int) { return "[]"; }
  static std::string GetRoomProperties(int) { return "{}"; }
  static std::string GetMapTileData(int) { return "{}"; }
  static std::string GetMapEntities(int) { return "{}"; }
  static std::string GetMapProperties(int) { return "{}"; }
  static std::string GetPaletteData(const std::string&, int) { return "{}"; }
  static std::string ListPaletteGroups() { return "[]"; }
  // GUI Automation APIs
  static std::string GetUIElementTree() { return "{}"; }
  static std::string GetUIElementBounds(const std::string&) { return "{}"; }
  static std::string SetSelection(const std::string&) { return "{}"; }
  // Platform Info API
  static std::string GetPlatformInfo() { return "{}"; }
  // Agent API
  static bool AgentIsReady() { return false; }
  static std::string AgentSendMessage(const std::string&) { return "{}"; }
  static std::string AgentGetChatHistory() { return "[]"; }
  static std::string AgentGetConfig() { return "{}"; }
  static std::string AgentSetConfig(const std::string&) { return "{}"; }
  static std::string AgentGetProviders() { return "[]"; }
  static std::string AgentGetProposals() { return "[]"; }
  static std::string AgentAcceptProposal(const std::string&) { return "{}"; }
  static std::string AgentRejectProposal(const std::string&) { return "{}"; }
  static std::string AgentGetProposalDetails(const std::string&) { return "{}"; }
  static std::string AgentOpenSidebar() { return "{}"; }
  static std::string AgentCloseSidebar() { return "{}"; }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_CONTROL_API_H_
