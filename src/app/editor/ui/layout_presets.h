#ifndef YAZE_APP_EDITOR_UI_LAYOUT_PRESETS_H_
#define YAZE_APP_EDITOR_UI_LAYOUT_PRESETS_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/editor.h"

namespace yaze {
namespace editor {

/**
 * @struct CardLayoutPreset
 * @brief Defines default card visibility for an editor type
 */
struct CardLayoutPreset {
  std::string name;
  std::string description;
  EditorType editor_type;
  std::vector<std::string> default_visible_cards;
  std::vector<std::string> optional_cards;  // Available but hidden by default
};

/**
 * @class LayoutPresets
 * @brief Centralized definition of default layouts per editor
 *
 * Provides default card configurations for each editor type:
 * - Overworld: Main canvas, Tile16 list (already good per user)
 * - Dungeon: Room selector, Object editor, Properties panel
 * - Graphics: Sheet browser, Palette editor, Preview pane
 * - Debug/Agent: Emulator, Memory editor, Disassembly, Agent chat
 *
 * These presets are applied when switching to an editor for the first time
 * or when user requests "Reset to Default Layout".
 */
class LayoutPresets {
 public:
  /**
   * @brief Get the default layout preset for an editor type
   * @param type The editor type
   * @return CardLayoutPreset with default cards
   */
  static CardLayoutPreset GetDefaultPreset(EditorType type);

  /**
   * @brief Get all available presets
   * @return Map of editor type to preset
   */
  static std::unordered_map<EditorType, CardLayoutPreset> GetAllPresets();

  /**
   * @brief Get default visible cards for an editor
   * @param type The editor type
   * @return Vector of card IDs that should be visible by default
   */
  static std::vector<std::string> GetDefaultCards(EditorType type);

  /**
   * @brief Get all available cards for an editor (visible + hidden)
   * @param type The editor type
   * @return Vector of all card IDs available for this editor
   */
  static std::vector<std::string> GetAllCardsForEditor(EditorType type);

  /**
   * @brief Check if a card should be visible by default
   * @param type The editor type
   * @param card_id The card ID to check
   * @return True if card should be visible by default
   */
  static bool IsDefaultCard(EditorType type, const std::string& card_id);

  // ============================================================================
  // Named Workspace Presets
  // ============================================================================

  /**
   * @brief Get the "minimal" workspace preset (minimal cards)
   */
  static CardLayoutPreset GetMinimalPreset();

  /**
   * @brief Get the "developer" workspace preset (debug-focused)
   */
  static CardLayoutPreset GetDeveloperPreset();

  /**
   * @brief Get the "designer" workspace preset (visual-focused)
   */
  static CardLayoutPreset GetDesignerPreset();

  /**
   * @brief Get the "modder" workspace preset (full-featured)
   */
  static CardLayoutPreset GetModderPreset();

  // ============================================================================
  // Card ID Constants - synced with actual editor registrations
  // ============================================================================
  struct Cards {
    // Overworld cards (overworld_editor.cc)
    static constexpr const char* kOverworldCanvas = "overworld.canvas";
    static constexpr const char* kOverworldTile16Selector = "overworld.tile16_selector";
    static constexpr const char* kOverworldTile8Selector = "overworld.tile8_selector";
    static constexpr const char* kOverworldAreaGraphics = "overworld.area_graphics";
    static constexpr const char* kOverworldScratch = "overworld.scratch";
    static constexpr const char* kOverworldGfxGroups = "overworld.gfx_groups";
    static constexpr const char* kOverworldUsageStats = "overworld.usage_stats";
    static constexpr const char* kOverworldV3Settings = "overworld.v3_settings";

    // Dungeon cards (dungeon_editor_v2.cc)
    static constexpr const char* kDungeonControlPanel = "dungeon.control_panel";
    static constexpr const char* kDungeonRoomSelector = "dungeon.room_selector";
    static constexpr const char* kDungeonRoomMatrix = "dungeon.room_matrix";
    static constexpr const char* kDungeonEntrances = "dungeon.entrances";
    static constexpr const char* kDungeonRoomGraphics = "dungeon.room_graphics";
    static constexpr const char* kDungeonObjectEditor = "dungeon.object_editor";
    static constexpr const char* kDungeonPaletteEditor = "dungeon.palette_editor";
    static constexpr const char* kDungeonDebugControls = "dungeon.debug_controls";

    // Graphics cards (graphics_editor.cc)
    static constexpr const char* kGraphicsSheetEditor = "graphics.sheet_editor";
    static constexpr const char* kGraphicsSheetBrowser = "graphics.sheet_browser";
    static constexpr const char* kGraphicsPlayerAnimations = "graphics.player_animations";
    static constexpr const char* kGraphicsPrototypeViewer = "graphics.prototype_viewer";

    // Palette cards (palette_editor.cc)
    static constexpr const char* kPaletteControlPanel = "palette.control_panel";
    static constexpr const char* kPaletteOwMain = "palette.ow_main";
    static constexpr const char* kPaletteOwAnimated = "palette.ow_animated";
    static constexpr const char* kPaletteDungeonMain = "palette.dungeon_main";
    static constexpr const char* kPaletteSprites = "palette.sprites";
    static constexpr const char* kPaletteSpritesAux1 = "palette.sprites_aux1";
    static constexpr const char* kPaletteSpritesAux2 = "palette.sprites_aux2";
    static constexpr const char* kPaletteSpritesAux3 = "palette.sprites_aux3";
    static constexpr const char* kPaletteEquipment = "palette.equipment";
    static constexpr const char* kPaletteQuickAccess = "palette.quick_access";
    static constexpr const char* kPaletteCustom = "palette.custom";

    // Sprite cards (sprite_editor.cc)
    static constexpr const char* kSpriteVanillaEditor = "sprite.vanilla_editor";
    static constexpr const char* kSpriteCustomEditor = "sprite.custom_editor";

    // Screen cards (screen_editor.cc)
    static constexpr const char* kScreenDungeonMaps = "screen.dungeon_maps";
    static constexpr const char* kScreenInventoryMenu = "screen.inventory_menu";
    static constexpr const char* kScreenOverworldMap = "screen.overworld_map";
    static constexpr const char* kScreenTitleScreen = "screen.title_screen";
    static constexpr const char* kScreenNamingScreen = "screen.naming_screen";

    // Music cards (music_editor.cc)
    static constexpr const char* kMusicTracker = "music.tracker";
    static constexpr const char* kMusicInstrumentEditor = "music.instrument_editor";
    static constexpr const char* kMusicAssembly = "music.assembly";

    // Message cards (message_editor.cc)
    static constexpr const char* kMessageList = "message.message_list";
    static constexpr const char* kMessageEditor = "message.message_editor";
    static constexpr const char* kMessageFontAtlas = "message.font_atlas";
    static constexpr const char* kMessageDictionary = "message.dictionary";

    // Assembly cards (assembly_editor.cc)
    static constexpr const char* kAssemblyEditor = "assembly.editor";
    static constexpr const char* kAssemblyFileBrowser = "assembly.file_browser";

    // Emulator cards (editor_manager.cc)
    static constexpr const char* kEmulatorCpuDebugger = "emulator.cpu_debugger";
    static constexpr const char* kEmulatorPpuViewer = "emulator.ppu_viewer";
    static constexpr const char* kEmulatorMemoryViewer = "emulator.memory_viewer";
    static constexpr const char* kEmulatorBreakpoints = "emulator.breakpoints";
    static constexpr const char* kEmulatorPerformance = "emulator.performance";
    static constexpr const char* kEmulatorAiAgent = "emulator.ai_agent";
    static constexpr const char* kEmulatorSaveStates = "emulator.save_states";
    static constexpr const char* kEmulatorKeyboardConfig = "emulator.keyboard_config";
    static constexpr const char* kEmulatorApuDebugger = "emulator.apu_debugger";
    static constexpr const char* kEmulatorAudioMixer = "emulator.audio_mixer";

    // Memory cards (editor_manager.cc)
    static constexpr const char* kMemoryHexEditor = "memory.hex_editor";

    // Settings cards (settings_editor.cc)
    static constexpr const char* kSettingsGeneral = "settings.general";
    static constexpr const char* kSettingsAppearance = "settings.appearance";
    static constexpr const char* kSettingsEditorBehavior = "settings.editor_behavior";
    static constexpr const char* kSettingsPerformance = "settings.performance";
    static constexpr const char* kSettingsAiAgent = "settings.ai_agent";
    static constexpr const char* kSettingsShortcuts = "settings.shortcuts";
  };
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_LAYOUT_PRESETS_H_
