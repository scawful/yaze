// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_control_api.h"

#include <emscripten.h>
#include <emscripten/bind.h>

#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "app/editor/session_types.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/automation/widget_measurement.h"
#include "app/rom.h"
#include "nlohmann/json.hpp"
#include "util/log.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layout.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace app {
namespace platform {

// Static member initialization
editor::EditorManager* WasmControlApi::editor_manager_ = nullptr;
bool WasmControlApi::initialized_ = false;

// ============================================================================
// JavaScript Bindings Setup
// ============================================================================

EM_JS(void, SetupYazeControlApi, (), {
  if (typeof Module === 'undefined') return;
  
  // Create unified window.yaze namespace if not exists
  if (!window.yaze) {
    window.yaze = {};
  }
  
  // Control API namespace
  window.yaze.control = {
    // Editor control
    switchEditor: function(editorName) {
      if (Module.controlSwitchEditor) {
        try { return JSON.parse(Module.controlSwitchEditor(editorName)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getCurrentEditor: function() {
      if (Module.controlGetCurrentEditor) {
        try { return JSON.parse(Module.controlGetCurrentEditor()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getAvailableEditors: function() {
      if (Module.controlGetAvailableEditors) {
        try { return JSON.parse(Module.controlGetAvailableEditors()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Card control
    openCard: function(cardId) {
      if (Module.controlOpenCard) {
        try { return JSON.parse(Module.controlOpenCard(cardId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    closeCard: function(cardId) {
      if (Module.controlCloseCard) {
        try { return JSON.parse(Module.controlCloseCard(cardId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    toggleCard: function(cardId) {
      if (Module.controlToggleCard) {
        try { return JSON.parse(Module.controlToggleCard(cardId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getVisibleCards: function() {
      if (Module.controlGetVisibleCards) {
        try { return JSON.parse(Module.controlGetVisibleCards()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getAvailableCards: function() {
      if (Module.controlGetAvailableCards) {
        try { return JSON.parse(Module.controlGetAvailableCards()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getCardsInCategory: function(category) {
      if (Module.controlGetCardsInCategory) {
        try { return JSON.parse(Module.controlGetCardsInCategory(category)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Layout control
    setCardLayout: function(layoutName) {
      if (Module.controlSetCardLayout) {
        try { return JSON.parse(Module.controlSetCardLayout(layoutName)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getAvailableLayouts: function() {
      if (Module.controlGetAvailableLayouts) {
        try { return JSON.parse(Module.controlGetAvailableLayouts()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    saveCurrentLayout: function(layoutName) {
      if (Module.controlSaveCurrentLayout) {
        try { return JSON.parse(Module.controlSaveCurrentLayout(layoutName)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Menu/UI actions
    triggerMenuAction: function(actionPath) {
      if (Module.controlTriggerMenuAction) {
        try { return JSON.parse(Module.controlTriggerMenuAction(actionPath)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getAvailableMenuActions: function() {
      if (Module.controlGetAvailableMenuActions) {
        try { return JSON.parse(Module.controlGetAvailableMenuActions()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Session control
    getSessionInfo: function() {
      if (Module.controlGetSessionInfo) {
        try { return JSON.parse(Module.controlGetSessionInfo()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    createSession: function() {
      if (Module.controlCreateSession) {
        try { return JSON.parse(Module.controlCreateSession()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    switchSession: function(sessionIndex) {
      if (Module.controlSwitchSession) {
        try { return JSON.parse(Module.controlSwitchSession(sessionIndex)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // ROM control
    getRomStatus: function() {
      if (Module.controlGetRomStatus) {
        try { return JSON.parse(Module.controlGetRomStatus()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    readRomBytes: function(address, count) {
      count = count || 16;
      if (Module.controlReadRomBytes) {
        try { return JSON.parse(Module.controlReadRomBytes(address, count)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    writeRomBytes: function(address, bytes) {
      if (Module.controlWriteRomBytes) {
        try { return JSON.parse(Module.controlWriteRomBytes(address, JSON.stringify(bytes))); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    saveRom: function() {
      if (Module.controlSaveRom) {
        try { return JSON.parse(Module.controlSaveRom()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Utility
    isReady: function() {
      return Module.controlIsReady ? Module.controlIsReady() : false;
    }
  };

  // Editor State API namespace (for LLM agents and automation)
  window.yaze.editor = {
    getSnapshot: function() {
      if (Module.editorGetSnapshot) {
        try { return JSON.parse(Module.editorGetSnapshot()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getCurrentRoom: function() {
      if (Module.editorGetCurrentDungeonRoom) {
        try { return JSON.parse(Module.editorGetCurrentDungeonRoom()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getCurrentMap: function() {
      if (Module.editorGetCurrentOverworldMap) {
        try { return JSON.parse(Module.editorGetCurrentOverworldMap()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getSelection: function() {
      if (Module.editorGetSelection) {
        try { return JSON.parse(Module.editorGetSelection()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    }
  };

  // Data API namespace (read-only access to ROM data)
  window.yaze.data = {
    // Dungeon data
    getRoomTiles: function(roomId) {
      if (Module.dataGetRoomTileData) {
        try { return JSON.parse(Module.dataGetRoomTileData(roomId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getRoomObjects: function(roomId) {
      if (Module.dataGetRoomObjects) {
        try { return JSON.parse(Module.dataGetRoomObjects(roomId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getRoomProperties: function(roomId) {
      if (Module.dataGetRoomProperties) {
        try { return JSON.parse(Module.dataGetRoomProperties(roomId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    // Overworld data
    getMapTiles: function(mapId) {
      if (Module.dataGetMapTileData) {
        try { return JSON.parse(Module.dataGetMapTileData(mapId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getMapEntities: function(mapId) {
      if (Module.dataGetMapEntities) {
        try { return JSON.parse(Module.dataGetMapEntities(mapId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getMapProperties: function(mapId) {
      if (Module.dataGetMapProperties) {
        try { return JSON.parse(Module.dataGetMapProperties(mapId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    // Palette data
    getPalette: function(groupName, paletteId) {
      if (Module.dataGetPaletteData) {
        try { return JSON.parse(Module.dataGetPaletteData(groupName, paletteId)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },

    getPaletteGroups: function() {
      if (Module.dataListPaletteGroups) {
        try { return JSON.parse(Module.dataListPaletteGroups()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    }
  };

  console.log("[yaze] window.yaze.control API initialized");
  console.log("[yaze] window.yaze.editor API initialized");
  console.log("[yaze] window.yaze.data API initialized");
});

// ============================================================================
// Initialization
// ============================================================================

void WasmControlApi::Initialize(editor::EditorManager* editor_manager) {
  editor_manager_ = editor_manager;
  initialized_ = (editor_manager_ != nullptr);
  
  if (initialized_) {
    SetupJavaScriptBindings();
    LOG_INFO("WasmControlApi", "Control API initialized");
  }
}

bool WasmControlApi::IsReady() {
  return initialized_ && editor_manager_ != nullptr;
}

void WasmControlApi::SetupJavaScriptBindings() {
  SetupYazeControlApi();
}

// ============================================================================
// Helper Methods
// ============================================================================

editor::EditorCardRegistry* WasmControlApi::GetCardRegistry() {
  if (!IsReady() || !editor_manager_) {
    return nullptr;
  }
  return &editor_manager_->card_registry();
}

std::string WasmControlApi::EditorTypeToString(int type) {
  if (type >= 0 && type < static_cast<int>(editor::kEditorNames.size())) {
    return editor::kEditorNames[type];
  }
  return "Unknown";
}

int WasmControlApi::StringToEditorType(const std::string& name) {
  for (size_t i = 0; i < editor::kEditorNames.size(); ++i) {
    if (editor::kEditorNames[i] == name) {
      return static_cast<int>(i);
    }
  }
  return 0;  // Unknown
}

// ============================================================================
// Editor Control Implementation
// ============================================================================

std::string WasmControlApi::SwitchEditor(const std::string& editor_name) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  int editor_type = StringToEditorType(editor_name);
  if (editor_type == 0 && editor_name != "Unknown") {
    result["success"] = false;
    result["error"] = "Unknown editor: " + editor_name;
    return result.dump();
  }
  
  editor_manager_->SwitchToEditor(static_cast<editor::EditorType>(editor_type));
  
  result["success"] = true;
  result["editor"] = editor_name;
  return result.dump();
}

std::string WasmControlApi::GetCurrentEditor() {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  auto* current = editor_manager_->GetCurrentEditor();
  if (current) {
    result["name"] = EditorTypeToString(static_cast<int>(current->type()));
    result["type"] = static_cast<int>(current->type());
    result["active"] = *current->active();
  } else {
    result["name"] = "None";
    result["type"] = 0;
    result["active"] = false;
  }

  return result.dump();
}

std::string WasmControlApi::GetAvailableEditors() {
  nlohmann::json result = nlohmann::json::array();
  
  for (size_t i = 1; i < editor::kEditorNames.size(); ++i) {  // Skip "Unknown"
    nlohmann::json editor_info;
    editor_info["name"] = editor::kEditorNames[i];
    editor_info["type"] = static_cast<int>(i);
    result.push_back(editor_info);
  }
  
  return result.dump();
}

// ============================================================================
// Card Control Implementation
// ============================================================================

std::string WasmControlApi::OpenCard(const std::string& card_id) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  // TODO: Integrate with EditorCardRegistry through EditorManager
  // For now, return a placeholder
  result["success"] = true;
  result["card_id"] = card_id;
  result["visible"] = true;
  
  LOG_INFO("WasmControlApi", "OpenCard: %s", card_id.c_str());
  return result.dump();
}

std::string WasmControlApi::CloseCard(const std::string& card_id) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  result["success"] = true;
  result["card_id"] = card_id;
  result["visible"] = false;
  
  LOG_INFO("WasmControlApi", "CloseCard: %s", card_id.c_str());
  return result.dump();
}

std::string WasmControlApi::ToggleCard(const std::string& card_id) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  result["success"] = true;
  result["card_id"] = card_id;
  
  LOG_INFO("WasmControlApi", "ToggleCard: %s", card_id.c_str());
  return result.dump();
}

std::string WasmControlApi::GetVisibleCards() {
  nlohmann::json result = nlohmann::json::array();

  if (!IsReady()) {
    return result.dump();
  }

  auto* registry = GetCardRegistry();
  if (!registry) {
    return result.dump();
  }

  // Use default session ID (0) for WASM single-session mode
  constexpr size_t session_id = 0;
  auto card_ids = registry->GetCardsInSession(session_id);
  for (const auto& card_id : card_ids) {
    // Extract base card ID (remove session prefix like "s0.")
    std::string base_id = card_id;
    if (base_id.size() > 3 && base_id[0] == 's' && base_id[2] == '.') {
      base_id = base_id.substr(3);
    }
    if (registry->IsCardVisible(session_id, base_id)) {
      result.push_back(base_id);
    }
  }

  return result.dump();
}

std::string WasmControlApi::GetAvailableCards() {
  nlohmann::json result = nlohmann::json::array();

  if (!IsReady()) {
    return result.dump();
  }

  auto* registry = GetCardRegistry();
  if (!registry) {
    return result.dump();
  }

  // Use default session ID (0) for WASM single-session mode
  constexpr size_t session_id = 0;
  auto categories = registry->GetAllCategories(session_id);

  for (const auto& category : categories) {
    auto cards = registry->GetCardsInCategory(session_id, category);
    for (const auto& card : cards) {
      nlohmann::json card_json;
      card_json["id"] = card.card_id;
      card_json["display_name"] = card.display_name;
      card_json["window_title"] = card.window_title;
      card_json["icon"] = card.icon;
      card_json["category"] = card.category;
      card_json["priority"] = card.priority;
      card_json["visible"] = registry->IsCardVisible(session_id, card.card_id);
      card_json["shortcut_hint"] = card.shortcut_hint;
      if (card.enabled_condition) {
        card_json["enabled"] = card.enabled_condition();
      } else {
        card_json["enabled"] = true;
      }
      result.push_back(card_json);
    }
  }

  return result.dump();
}

std::string WasmControlApi::GetCardsInCategory(const std::string& category) {
  nlohmann::json result = nlohmann::json::array();

  if (!IsReady()) {
    return result.dump();
  }

  auto* registry = GetCardRegistry();
  if (!registry) {
    return result.dump();
  }

  // Use default session ID (0) for WASM single-session mode
  constexpr size_t session_id = 0;
  auto cards = registry->GetCardsInCategory(session_id, category);

  for (const auto& card : cards) {
    nlohmann::json card_json;
    card_json["id"] = card.card_id;
    card_json["display_name"] = card.display_name;
    card_json["window_title"] = card.window_title;
    card_json["icon"] = card.icon;
    card_json["category"] = card.category;
    card_json["priority"] = card.priority;
    card_json["visible"] = registry->IsCardVisible(session_id, card.card_id);
    result.push_back(card_json);
  }

  return result.dump();
}

// ============================================================================
// Layout Control Implementation
// ============================================================================

std::string WasmControlApi::SetCardLayout(const std::string& layout_name) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  // TODO: Implement layout presets
  result["success"] = true;
  result["layout"] = layout_name;
  
  LOG_INFO("WasmControlApi", "SetCardLayout: %s", layout_name.c_str());
  return result.dump();
}

std::string WasmControlApi::GetAvailableLayouts() {
  nlohmann::json result = nlohmann::json::array();
  
  // Built-in layouts
  result.push_back("overworld_default");
  result.push_back("dungeon_default");
  result.push_back("graphics_default");
  result.push_back("debug_default");
  result.push_back("minimal");
  result.push_back("all_cards");
  
  return result.dump();
}

std::string WasmControlApi::SaveCurrentLayout(const std::string& layout_name) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  // TODO: Save to workspace presets
  result["success"] = true;
  result["layout"] = layout_name;
  
  return result.dump();
}

// ============================================================================
// Menu/UI Actions Implementation
// ============================================================================

std::string WasmControlApi::TriggerMenuAction(const std::string& action_path) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  // Parse action path (e.g., "File.Save", "Edit.Undo")
  // TODO: Map to actual menu callbacks
  
  if (action_path == "File.Save") {
    auto status = editor_manager_->SaveRom();
    result["success"] = status.ok();
    if (!status.ok()) {
      result["error"] = status.ToString();
    }
  } else if (action_path == "View.ShowEmulator") {
    editor_manager_->ShowEmulator();
    result["success"] = true;
  } else if (action_path == "View.ShowWelcome") {
    editor_manager_->ShowWelcomeScreen();
    result["success"] = true;
  } else {
    result["success"] = false;
    result["error"] = "Unknown action: " + action_path;
  }
  
  return result.dump();
}

std::string WasmControlApi::GetAvailableMenuActions() {
  nlohmann::json result = nlohmann::json::array();
  
  // File menu
  result.push_back("File.Open");
  result.push_back("File.Save");
  result.push_back("File.SaveAs");
  result.push_back("File.NewProject");
  result.push_back("File.OpenProject");
  
  // Edit menu
  result.push_back("Edit.Undo");
  result.push_back("Edit.Redo");
  result.push_back("Edit.Cut");
  result.push_back("Edit.Copy");
  result.push_back("Edit.Paste");
  
  // View menu
  result.push_back("View.ShowEmulator");
  result.push_back("View.ShowWelcome");
  result.push_back("View.ShowCardBrowser");
  result.push_back("View.ShowMemoryEditor");
  result.push_back("View.ShowHexEditor");
  
  // Tools menu
  result.push_back("Tools.GlobalSearch");
  result.push_back("Tools.CommandPalette");
  
  return result.dump();
}

// ============================================================================
// Session Control Implementation
// ============================================================================

std::string WasmControlApi::GetSessionInfo() {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  result["session_index"] = editor_manager_->GetCurrentSessionIndex();
  result["session_count"] = editor_manager_->GetActiveSessionCount();
  
  auto* rom = editor_manager_->GetCurrentRom();
  if (rom && rom->is_loaded()) {
    result["rom_loaded"] = true;
    result["rom_filename"] = rom->filename();
    result["rom_title"] = rom->title();
  } else {
    result["rom_loaded"] = false;
  }
  
  auto* current_editor = editor_manager_->GetCurrentEditor();
  if (current_editor) {
    result["current_editor"] = EditorTypeToString(static_cast<int>(current_editor->type()));
  }
  
  return result.dump();
}

std::string WasmControlApi::CreateSession() {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  editor_manager_->CreateNewSession();
  result["success"] = true;
  result["session_index"] = editor_manager_->GetCurrentSessionIndex();
  
  return result.dump();
}

std::string WasmControlApi::SwitchSession(int session_index) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  if (session_index < 0 || static_cast<size_t>(session_index) >= editor_manager_->GetActiveSessionCount()) {
    result["success"] = false;
    result["error"] = "Invalid session index";
    return result.dump();
  }
  
  editor_manager_->SwitchToSession(static_cast<size_t>(session_index));
  result["success"] = true;
  result["session_index"] = session_index;
  
  return result.dump();
}

// ============================================================================
// ROM Control Implementation
// ============================================================================

std::string WasmControlApi::GetRomStatus() {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  auto* rom = editor_manager_->GetCurrentRom();
  if (rom && rom->is_loaded()) {
    result["loaded"] = true;
    result["filename"] = rom->filename();
    result["title"] = rom->title();
    result["size"] = rom->size();
    result["dirty"] = rom->dirty();
  } else {
    result["loaded"] = false;
  }
  
  return result.dump();
}

std::string WasmControlApi::ReadRomBytes(int address, int count) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    result["error"] = "No ROM loaded";
    return result.dump();
  }
  
  // Limit read size
  count = std::min(count, 256);
  
  if (address < 0 || static_cast<size_t>(address + count) > rom->size()) {
    result["error"] = "Address out of range";
    return result.dump();
  }
  
  result["address"] = address;
  result["count"] = count;
  
  nlohmann::json bytes = nlohmann::json::array();
  for (int i = 0; i < count; ++i) {
    auto byte_result = rom->ReadByte(address + i);
    if (byte_result.ok()) {
      bytes.push_back(*byte_result);
    } else {
      bytes.push_back(0);
    }
  }
  result["bytes"] = bytes;
  
  return result.dump();
}

std::string WasmControlApi::WriteRomBytes(int address, const std::string& bytes_json) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }
  
  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    result["success"] = false;
    result["error"] = "No ROM loaded";
    return result.dump();
  }
  
  try {
    auto bytes = nlohmann::json::parse(bytes_json);
    if (!bytes.is_array()) {
      result["success"] = false;
      result["error"] = "Invalid bytes format - expected array";
      return result.dump();
    }
    
    for (size_t i = 0; i < bytes.size(); ++i) {
      uint8_t value = bytes[i].get<uint8_t>();
      auto status = rom->WriteByte(address + static_cast<int>(i), value);
      if (!status.ok()) {
        result["success"] = false;
        result["error"] = status.ToString();
        return result.dump();
      }
    }
    
    result["success"] = true;
    result["bytes_written"] = bytes.size();
    
  } catch (const std::exception& e) {
    result["success"] = false;
    result["error"] = e.what();
  }
  
  return result.dump();
}

std::string WasmControlApi::SaveRom() {
  nlohmann::json result;

  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  auto status = editor_manager_->SaveRom();
  result["success"] = status.ok();
  if (!status.ok()) {
    result["error"] = status.ToString();
  }

  return result.dump();
}

// ============================================================================
// Editor State APIs Implementation
// ============================================================================

std::string WasmControlApi::GetEditorSnapshot() {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  auto* current = editor_manager_->GetCurrentEditor();
  if (!current) {
    result["editor_type"] = "none";
    result["active"] = false;
    return result.dump();
  }

  result["editor_type"] = EditorTypeToString(static_cast<int>(current->type()));
  result["editor_type_id"] = static_cast<int>(current->type());
  result["active"] = *current->active();

  // Add ROM status
  auto* rom = editor_manager_->GetCurrentRom();
  if (rom && rom->is_loaded()) {
    result["rom_loaded"] = true;
    result["rom_title"] = rom->title();
  } else {
    result["rom_loaded"] = false;
  }

  // Add editor-specific data based on type
  nlohmann::json active_data;
  auto* editor_set = editor_manager_->GetCurrentEditorSet();

  if (current->type() == editor::EditorType::kDungeon && editor_set) {
    auto& dungeon = editor_set->dungeon_editor_;
    active_data["current_room_id"] = dungeon.current_room_id();

    nlohmann::json active_rooms = nlohmann::json::array();
    for (int i = 0; i < dungeon.active_rooms().size(); ++i) {
      active_rooms.push_back(dungeon.active_rooms()[i]);
    }
    active_data["active_rooms"] = active_rooms;
    active_data["room_count"] = dungeon.active_rooms().size();

  } else if (current->type() == editor::EditorType::kOverworld && editor_set) {
    auto& overworld = editor_set->overworld_editor_;
    active_data["current_map"] = overworld.overworld().current_map_id();
    active_data["current_world"] = overworld.overworld().current_world();
    active_data["map_count"] = zelda3::kNumOverworldMaps;
  }

  result["active_data"] = active_data;

  return result.dump();
}

std::string WasmControlApi::GetCurrentDungeonRoom() {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  auto* current = editor_manager_->GetCurrentEditor();
  if (!current || current->type() != editor::EditorType::kDungeon) {
    result["error"] = "Dungeon editor not active";
    result["editor_type"] = current ? EditorTypeToString(static_cast<int>(current->type())) : "none";
    return result.dump();
  }

  auto* editor_set = editor_manager_->GetCurrentEditorSet();
  if (!editor_set) {
    result["error"] = "No editor set available";
    return result.dump();
  }

  auto& dungeon = editor_set->dungeon_editor_;
  result["room_id"] = dungeon.current_room_id();

  // Get active rooms list
  nlohmann::json active_rooms = nlohmann::json::array();
  for (int i = 0; i < dungeon.active_rooms().size(); ++i) {
    active_rooms.push_back(dungeon.active_rooms()[i]);
  }
  result["active_rooms"] = active_rooms;
  result["room_count"] = dungeon.active_rooms().size();

  // Card visibility state
  nlohmann::json cards;
  cards["room_selector"] = dungeon.show_room_selector_;
  cards["room_matrix"] = dungeon.show_room_matrix_;
  cards["entrances_list"] = dungeon.show_entrances_list_;
  cards["room_graphics"] = dungeon.show_room_graphics_;
  cards["object_editor"] = dungeon.show_object_editor_;
  cards["palette_editor"] = dungeon.show_palette_editor_;
  cards["debug_controls"] = dungeon.show_debug_controls_;
  cards["control_panel"] = dungeon.show_control_panel_;
  result["visible_cards"] = cards;

  return result.dump();
}

std::string WasmControlApi::GetCurrentOverworldMap() {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  auto* current = editor_manager_->GetCurrentEditor();
  if (!current || current->type() != editor::EditorType::kOverworld) {
    result["error"] = "Overworld editor not active";
    result["editor_type"] = current ? EditorTypeToString(static_cast<int>(current->type())) : "none";
    return result.dump();
  }

  auto* editor_set = editor_manager_->GetCurrentEditorSet();
  if (!editor_set) {
    result["error"] = "No editor set available";
    return result.dump();
  }

  auto& overworld = editor_set->overworld_editor_;
  auto& ow_data = overworld.overworld();

  result["map_id"] = ow_data.current_map_id();
  result["world"] = ow_data.current_world();
  result["world_name"] = ow_data.current_world() == 0 ? "Light World" :
                         (ow_data.current_world() == 1 ? "Dark World" : "Special World");
  result["map_count"] = zelda3::kNumOverworldMaps;

  return result.dump();
}

std::string WasmControlApi::GetEditorSelection() {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  auto* current = editor_manager_->GetCurrentEditor();
  if (!current) {
    result["error"] = "No editor active";
    return result.dump();
  }

  result["editor_type"] = EditorTypeToString(static_cast<int>(current->type()));
  result["selection"] = nlohmann::json::array(); // Placeholder for future selection data

  // TODO: Implement editor-specific selection queries
  // For now, return empty selection
  result["has_selection"] = false;

  return result.dump();
}

// ============================================================================
// Read-only Data APIs Implementation
// ============================================================================

std::string WasmControlApi::GetRoomTileData(int room_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  if (room_id < 0 || room_id >= 296) {
    result["error"] = "Invalid room ID (must be 0-295)";
    return result.dump();
  }

  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    result["error"] = "ROM not loaded";
    return result.dump();
  }

  // Load room from ROM
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);
  room.LoadRoomGraphics();
  room.LoadObjects();

  result["room_id"] = room_id;
  result["width"] = 512;
  result["height"] = 512;

  // Get layout objects for both layers
  const auto& layout = room.GetLayout();
  const auto& layout_objects = layout.GetObjects();

  // Extract tile data for layer 1 and layer 2
  nlohmann::json layer1_tiles = nlohmann::json::array();
  nlohmann::json layer2_tiles = nlohmann::json::array();

  for (const auto& obj : layout_objects) {
    nlohmann::json tile_obj;
    tile_obj["x"] = obj.x();
    tile_obj["y"] = obj.y();

    auto tile_result = obj.GetTile(0);
    if (tile_result.ok()) {
      const auto* tile_info = tile_result.value();
      tile_obj["tile_id"] = tile_info->id_;
      tile_obj["palette"] = tile_info->palette_;
      tile_obj["priority"] = tile_info->over_;
      tile_obj["h_flip"] = tile_info->horizontal_mirror_;
      tile_obj["v_flip"] = tile_info->vertical_mirror_;

      if (obj.GetLayerValue() == 1) {
        layer2_tiles.push_back(tile_obj);
      } else {
        layer1_tiles.push_back(tile_obj);
      }
    }
  }

  result["layer1"] = layer1_tiles;
  result["layer2"] = layer2_tiles;
  result["layer1_count"] = layer1_tiles.size();
  result["layer2_count"] = layer2_tiles.size();

  return result.dump();
}

std::string WasmControlApi::GetRoomObjects(int room_id) {
  nlohmann::json result = nlohmann::json::array();

  if (!IsReady()) {
    nlohmann::json error;
    error["error"] = "Control API not initialized";
    return error.dump();
  }

  if (room_id < 0 || room_id >= 296) {
    nlohmann::json error;
    error["error"] = "Invalid room ID (must be 0-295)";
    return error.dump();
  }

  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    nlohmann::json error;
    error["error"] = "ROM not loaded";
    return error.dump();
  }

  // Load room from ROM
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);
  room.LoadObjects();

  // Get tile objects from the room
  const auto& tile_objects = room.GetTileObjects();

  for (const auto& obj : tile_objects) {
    nlohmann::json obj_data;
    obj_data["id"] = obj.id_;
    obj_data["x"] = obj.x();
    obj_data["y"] = obj.y();
    obj_data["size"] = obj.size();
    obj_data["layer"] = obj.GetLayerValue();

    // Add object type information
    auto options = static_cast<int>(obj.options());
    obj_data["is_door"] = (options & static_cast<int>(zelda3::ObjectOption::Door)) != 0;
    obj_data["is_chest"] = (options & static_cast<int>(zelda3::ObjectOption::Chest)) != 0;
    obj_data["is_block"] = (options & static_cast<int>(zelda3::ObjectOption::Block)) != 0;
    obj_data["is_torch"] = (options & static_cast<int>(zelda3::ObjectOption::Torch)) != 0;
    obj_data["is_stairs"] = (options & static_cast<int>(zelda3::ObjectOption::Stairs)) != 0;

    result.push_back(obj_data);
  }

  return result.dump();
}

std::string WasmControlApi::GetRoomProperties(int room_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  if (room_id < 0 || room_id >= 296) {
    result["error"] = "Invalid room ID (must be 0-295)";
    return result.dump();
  }

  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    result["error"] = "ROM not loaded";
    return result.dump();
  }

  // Load room from ROM
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  result["room_id"] = room_id;
  result["blockset"] = room.blockset;
  result["spriteset"] = room.spriteset;
  result["palette"] = room.palette;
  result["floor1"] = room.floor1();
  result["floor2"] = room.floor2();
  result["layout"] = room.layout;
  result["holewarp"] = room.holewarp;
  result["message_id"] = room.message_id_;

  // Effect and tags
  result["effect"] = static_cast<int>(room.effect());
  result["tag1"] = static_cast<int>(room.tag1());
  result["tag2"] = static_cast<int>(room.tag2());
  result["collision"] = static_cast<int>(room.collision());

  // Layer merging info
  const auto& layer_merge = room.layer_merging();
  result["layer_merging"] = {
      {"id", layer_merge.ID},
      {"name", layer_merge.Name},
      {"layer2_visible", layer_merge.Layer2Visible},
      {"layer2_on_top", layer_merge.Layer2OnTop},
      {"layer2_translucent", layer_merge.Layer2Translucent}
  };

  result["is_light"] = room.IsLight();
  result["is_loaded"] = room.IsLoaded();

  return result.dump();
}

std::string WasmControlApi::GetMapTileData(int map_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  if (map_id < 0 || map_id >= static_cast<int>(zelda3::kNumOverworldMaps)) {
    result["error"] = "Invalid map ID (must be 0-159)";
    return result.dump();
  }

  auto* overworld = editor_manager_->overworld();
  if (!overworld) {
    result["error"] = "Overworld not loaded";
    return result.dump();
  }

  auto* map = overworld->overworld_map(map_id);
  if (!map) {
    result["error"] = "Map not found";
    return result.dump();
  }

  result["map_id"] = map_id;
  result["width"] = 32;
  result["height"] = 32;

  // Get tile blockset data (this is the 32x32 tile16 data for the map)
  auto blockset = map->current_tile16_blockset();

  // Instead of dumping all 1024 tiles, provide summary information
  result["has_tile_data"] = !blockset.empty();
  result["tile_count"] = blockset.size();
  result["is_built"] = map->is_built();
  result["is_large_map"] = map->is_large_map();

  // Note: Full tile extraction would be very large (1024 tiles)
  // Only extract a small sample or provide it on request
  if (blockset.size() >= 64) {
    nlohmann::json sample_tiles = nlohmann::json::array();
    // Extract first 8x8 corner as a sample
    for (int i = 0; i < 64; i++) {
      sample_tiles.push_back(static_cast<int>(blockset[i]));
    }
    result["sample_tiles"] = sample_tiles;
    result["sample_note"] = "First 8x8 tiles from top-left corner";
  }

  return result.dump();
}

std::string WasmControlApi::GetMapEntities(int map_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  if (map_id < 0 || map_id >= static_cast<int>(zelda3::kNumOverworldMaps)) {
    result["error"] = "Invalid map ID (must be 0-159)";
    return result.dump();
  }

  auto* overworld = editor_manager_->overworld();
  if (!overworld) {
    result["error"] = "Overworld not loaded";
    return result.dump();
  }

  result["map_id"] = map_id;
  result["entrances"] = nlohmann::json::array();
  result["exits"] = nlohmann::json::array();
  result["items"] = nlohmann::json::array();
  result["sprites"] = nlohmann::json::array();

  // Get entrances for this map
  for (const auto& entrance : overworld->entrances()) {
    if (entrance.map_id_ == static_cast<uint16_t>(map_id)) {
      nlohmann::json e;
      e["id"] = entrance.entrance_id_;
      e["x"] = entrance.x_;
      e["y"] = entrance.y_;
      e["map_id"] = entrance.map_id_;
      result["entrances"].push_back(e);
    }
  }

  // Get exits for this map
  auto* exits = overworld->exits();
  if (exits) {
    for (const auto& exit : *exits) {
      if (exit.map_id_ == static_cast<uint16_t>(map_id)) {
        nlohmann::json ex;
        ex["x"] = exit.x_;
        ex["y"] = exit.y_;
        ex["map_id"] = exit.map_id_;
        ex["room_id"] = exit.room_id_;
        result["exits"].push_back(ex);
      }
    }
  }

  // Get items for this map (using map_id_ from GameEntity base class)
  for (const auto& item : overworld->all_items()) {
    if (item.map_id_ == static_cast<uint16_t>(map_id)) {
      nlohmann::json i;
      i["id"] = item.id_;
      i["x"] = item.x_;
      i["y"] = item.y_;
      result["items"].push_back(i);
    }
  }

  return result.dump();
}

std::string WasmControlApi::GetMapProperties(int map_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  if (map_id < 0 || map_id >= static_cast<int>(zelda3::kNumOverworldMaps)) {
    result["error"] = "Invalid map ID (must be 0-159)";
    return result.dump();
  }

  auto* overworld = editor_manager_->overworld();
  if (!overworld) {
    result["error"] = "Overworld not loaded";
    return result.dump();
  }

  auto* map = overworld->overworld_map(map_id);
  if (!map) {
    result["error"] = "Map not found";
    return result.dump();
  }

  result["map_id"] = map_id;
  result["world"] = map_id / 64;
  result["parent_id"] = map->parent();
  result["area_graphics"] = map->area_graphics();
  result["area_palette"] = map->area_palette();
  result["sprite_graphics"] = {map->sprite_graphics(0), map->sprite_graphics(1), map->sprite_graphics(2)};
  result["sprite_palette"] = {map->sprite_palette(0), map->sprite_palette(1), map->sprite_palette(2)};
  result["message_id"] = map->message_id();
  result["is_large_map"] = map->is_large_map();

  return result.dump();
}

std::string WasmControlApi::GetPaletteData(const std::string& group_name, int palette_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  auto* rom = editor_manager_->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    result["error"] = "ROM not loaded";
    return result.dump();
  }

  result["group"] = group_name;
  result["palette_id"] = palette_id;

  try {
    auto palette_groups = rom->palette_group();
    auto* group = palette_groups.get_group(group_name);

    if (!group) {
      result["error"] = "Invalid palette group name";
      return result.dump();
    }

    if (palette_id < 0 || palette_id >= static_cast<int>(group->size())) {
      result["error"] = "Invalid palette ID for this group";
      result["max_palette_id"] = group->size() - 1;
      return result.dump();
    }

    auto palette = (*group)[palette_id];
    nlohmann::json colors = nlohmann::json::array();

    // Extract color values
    for (size_t i = 0; i < palette.size(); i++) {
      const auto& color = palette[i];
      nlohmann::json color_data;
      color_data["index"] = i;

      // Convert SNES color to RGB
      auto snes_color = color.snes();
      auto rgb_color = color.rgb();

      // ImVec4 uses x,y,z,w for r,g,b,a in 0.0-1.0 range
      int r = static_cast<int>(rgb_color.x * 255);
      int g = static_cast<int>(rgb_color.y * 255);
      int b = static_cast<int>(rgb_color.z * 255);
      color_data["r"] = r;
      color_data["g"] = g;
      color_data["b"] = b;
      color_data["hex"] = absl::StrFormat("#%02X%02X%02X", r, g, b);
      color_data["snes_value"] = snes_color;

      colors.push_back(color_data);
    }

    result["colors"] = colors;
    result["color_count"] = palette.size();

  } catch (const std::exception& e) {
    result["error"] = std::string("Failed to extract palette: ") + e.what();
  }

  return result.dump();
}

std::string WasmControlApi::ListPaletteGroups() {
  nlohmann::json result = nlohmann::json::array();

  // List available palette groups (matching PaletteGroupMap structure)
  result.push_back("ow_main");
  result.push_back("ow_aux");
  result.push_back("ow_animated");
  result.push_back("hud");
  result.push_back("global_sprites");
  result.push_back("armors");
  result.push_back("swords");
  result.push_back("shields");
  result.push_back("sprites_aux1");
  result.push_back("sprites_aux2");
  result.push_back("sprites_aux3");
  result.push_back("dungeon_main");
  result.push_back("grass");
  result.push_back("3d_object");
  result.push_back("ow_mini_map");

  return result.dump();
}

// ============================================================================
// GUI Automation APIs Implementation
// ============================================================================

std::string WasmControlApi::GetUIElementTree() {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    result["elements"] = nlohmann::json::array();
    return result.dump();
  }

  // Query the WidgetIdRegistry for all registered widgets
  auto& registry = gui::WidgetIdRegistry::Instance();
  const auto& all_widgets = registry.GetAllWidgets();

  nlohmann::json elements = nlohmann::json::array();

  // Convert WidgetInfo to JSON elements
  for (const auto& [path, info] : all_widgets) {
    nlohmann::json elem;
    elem["id"] = info.full_path;
    elem["type"] = info.type;
    elem["label"] = info.label;
    elem["enabled"] = info.enabled;
    elem["visible"] = info.visible;
    elem["window"] = info.window_name;

    // Add bounds if available
    if (info.bounds.valid) {
      elem["bounds"] = {
        {"x", info.bounds.min_x},
        {"y", info.bounds.min_y},
        {"width", info.bounds.max_x - info.bounds.min_x},
        {"height", info.bounds.max_y - info.bounds.min_y}
      };
    } else {
      elem["bounds"] = {
        {"x", 0}, {"y", 0}, {"width", 0}, {"height", 0}
      };
    }

    // Add metadata
    if (!info.description.empty()) {
      elem["description"] = info.description;
    }
    elem["imgui_id"] = static_cast<uint32_t>(info.imgui_id);
    elem["last_seen_frame"] = info.last_seen_frame;

    elements.push_back(elem);
  }

  result["elements"] = elements;
  result["count"] = elements.size();
  result["source"] = "WidgetIdRegistry";

  return result.dump();
}

std::string WasmControlApi::GetUIElementBounds(const std::string& element_id) {
  nlohmann::json result;

  if (!IsReady()) {
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  // Query the WidgetIdRegistry for the specific widget
  auto& registry = gui::WidgetIdRegistry::Instance();
  const auto* widget_info = registry.GetWidgetInfo(element_id);

  result["id"] = element_id;

  if (widget_info == nullptr) {
    result["found"] = false;
    result["error"] = "Element not found: " + element_id;
    return result.dump();
  }

  result["found"] = true;
  result["visible"] = widget_info->visible;
  result["enabled"] = widget_info->enabled;
  result["type"] = widget_info->type;
  result["label"] = widget_info->label;
  result["window"] = widget_info->window_name;

  // Add bounds if available
  if (widget_info->bounds.valid) {
    result["x"] = widget_info->bounds.min_x;
    result["y"] = widget_info->bounds.min_y;
    result["width"] = widget_info->bounds.max_x - widget_info->bounds.min_x;
    result["height"] = widget_info->bounds.max_y - widget_info->bounds.min_y;
    result["bounds_valid"] = true;
  } else {
    result["x"] = 0;
    result["y"] = 0;
    result["width"] = 0;
    result["height"] = 0;
    result["bounds_valid"] = false;
  }

  // Add metadata
  result["imgui_id"] = static_cast<uint32_t>(widget_info->imgui_id);
  result["last_seen_frame"] = widget_info->last_seen_frame;

  if (!widget_info->description.empty()) {
    result["description"] = widget_info->description;
  }

  return result.dump();
}

std::string WasmControlApi::SetSelection(const std::string& ids_json) {
  nlohmann::json result;

  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Control API not initialized";
    return result.dump();
  }

  try {
    auto ids = nlohmann::json::parse(ids_json);

    // TODO: Implement actual selection setting based on active editor
    // For now, return success with the IDs that would be selected
    result["success"] = true;
    result["selected_ids"] = ids;
    result["note"] = "Selection setting not yet fully implemented";

  } catch (const std::exception& e) {
    result["success"] = false;
    result["error"] = std::string("Invalid JSON: ") + e.what();
  }

  return result.dump();
}

// ============================================================================
// Emscripten Bindings
// ============================================================================

EMSCRIPTEN_BINDINGS(wasm_control_api) {
  emscripten::function("controlIsReady", &WasmControlApi::IsReady);
  emscripten::function("controlSwitchEditor", &WasmControlApi::SwitchEditor);
  emscripten::function("controlGetCurrentEditor", &WasmControlApi::GetCurrentEditor);
  emscripten::function("controlGetAvailableEditors", &WasmControlApi::GetAvailableEditors);
  emscripten::function("controlOpenCard", &WasmControlApi::OpenCard);
  emscripten::function("controlCloseCard", &WasmControlApi::CloseCard);
  emscripten::function("controlToggleCard", &WasmControlApi::ToggleCard);
  emscripten::function("controlGetVisibleCards", &WasmControlApi::GetVisibleCards);
  emscripten::function("controlGetAvailableCards", &WasmControlApi::GetAvailableCards);
  emscripten::function("controlGetCardsInCategory", &WasmControlApi::GetCardsInCategory);
  emscripten::function("controlSetCardLayout", &WasmControlApi::SetCardLayout);
  emscripten::function("controlGetAvailableLayouts", &WasmControlApi::GetAvailableLayouts);
  emscripten::function("controlSaveCurrentLayout", &WasmControlApi::SaveCurrentLayout);
  emscripten::function("controlTriggerMenuAction", &WasmControlApi::TriggerMenuAction);
  emscripten::function("controlGetAvailableMenuActions", &WasmControlApi::GetAvailableMenuActions);
  emscripten::function("controlGetSessionInfo", &WasmControlApi::GetSessionInfo);
  emscripten::function("controlCreateSession", &WasmControlApi::CreateSession);
  emscripten::function("controlSwitchSession", &WasmControlApi::SwitchSession);
  emscripten::function("controlGetRomStatus", &WasmControlApi::GetRomStatus);
  emscripten::function("controlReadRomBytes", &WasmControlApi::ReadRomBytes);
  emscripten::function("controlWriteRomBytes", &WasmControlApi::WriteRomBytes);
  emscripten::function("controlSaveRom", &WasmControlApi::SaveRom);

  // Editor State APIs
  emscripten::function("editorGetSnapshot", &WasmControlApi::GetEditorSnapshot);
  emscripten::function("editorGetCurrentDungeonRoom", &WasmControlApi::GetCurrentDungeonRoom);
  emscripten::function("editorGetCurrentOverworldMap", &WasmControlApi::GetCurrentOverworldMap);
  emscripten::function("editorGetSelection", &WasmControlApi::GetEditorSelection);

  // Read-only Data APIs
  emscripten::function("dataGetRoomTileData", &WasmControlApi::GetRoomTileData);
  emscripten::function("dataGetRoomObjects", &WasmControlApi::GetRoomObjects);
  emscripten::function("dataGetRoomProperties", &WasmControlApi::GetRoomProperties);
  emscripten::function("dataGetMapTileData", &WasmControlApi::GetMapTileData);
  emscripten::function("dataGetMapEntities", &WasmControlApi::GetMapEntities);
  emscripten::function("dataGetMapProperties", &WasmControlApi::GetMapProperties);
  emscripten::function("dataGetPaletteData", &WasmControlApi::GetPaletteData);
  emscripten::function("dataListPaletteGroups", &WasmControlApi::ListPaletteGroups);

  // GUI Automation APIs
  emscripten::function("guiGetUIElementTree", &WasmControlApi::GetUIElementTree);
  emscripten::function("guiGetUIElementBounds", &WasmControlApi::GetUIElementBounds);
  emscripten::function("guiSetSelection", &WasmControlApi::SetSelection);
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

