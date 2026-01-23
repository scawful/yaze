#include <cstdlib>
#include <set>
#include <string>

#include "app/editor/layout/layout_manager.h"
#include "app/editor/system/panel_manager.h"
#include "app/gfx/backend/renderer_factory.h"
#include "app/platform/iwindow.h"
#include "app/platform/sdl_compat.h"
#include "app/gui/core/icons.h"
#include "lab/layout_designer/layout_designer_window.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace {

constexpr size_t kLabSessionId = 0;

struct LabPanelSeed {
  const char* id;
  const char* display_name;
  const char* window_title;
  const char* icon;
  const char* category;
  const char* shortcut;
  const char* disabled_tooltip;
  int priority;
  bool visible_by_default;
};

constexpr LabPanelSeed kLabPanels[] = {
    // Overworld
    {"overworld.canvas", "Overworld Canvas", nullptr, ICON_MD_MAP, "Overworld",
     "Ctrl+Shift+O", nullptr, 5, true},
    {"overworld.area_graphics", "Area Graphics", nullptr, ICON_MD_IMAGE,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.tile16_selector", "Tile16 Selector", nullptr, ICON_MD_GRID_ON,
     "Overworld", nullptr, nullptr, 50, true},
    {"overworld.tile16_editor", "Tile16 Editor", nullptr, ICON_MD_EDIT,
     "Overworld", nullptr, nullptr, 15, false},
    {"overworld.tile8_selector", "Tile8 Selector", nullptr, ICON_MD_GRID_3X3,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.properties", "Map Properties", nullptr, ICON_MD_TUNE,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.scratch", "Scratch Workspace", nullptr, ICON_MD_DRAW,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.usage_stats", "Usage Statistics", nullptr, ICON_MD_ANALYTICS,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.debug", "Debug Window", nullptr, ICON_MD_BUG_REPORT,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.gfx_groups", "Graphics Groups", nullptr, ICON_MD_COLLECTIONS,
     "Overworld", nullptr, nullptr, 50, false},
    {"overworld.v3_settings", "v3 Settings", nullptr, ICON_MD_SETTINGS,
     "Overworld", nullptr, nullptr, 50, false},

    // Dungeon
    {"dungeon.room_selector", "Room List", " Room List", ICON_MD_LIST,
     "Dungeon", "Ctrl+Shift+R", "Load a ROM to browse dungeon rooms", 20,
     false},
    {"dungeon.entrance_list", "Entrance List", " Entrance List",
     ICON_MD_DOOR_FRONT, "Dungeon", "Ctrl+Shift+E",
     "Load a ROM to browse dungeon entrances", 25, false},
    {"dungeon.entrance_properties", "Entrance Properties",
     " Entrance Properties", ICON_MD_TUNE, "Dungeon", nullptr,
     "Load a ROM to edit entrance properties", 26, false},
    {"dungeon.room_matrix", "Room Matrix", " Room Matrix", ICON_MD_GRID_VIEW,
     "Dungeon", "Ctrl+Shift+M", "Load a ROM to view the room matrix", 30,
     false},
    {"dungeon.room_graphics", "Room Graphics", " Room Graphics", ICON_MD_IMAGE,
     "Dungeon", "Ctrl+Shift+G", "Load a ROM to view room graphics", 50, false},
    {"dungeon.object_editor", "Object Editor", nullptr, ICON_MD_CONSTRUCTION,
     "Dungeon", nullptr, nullptr, 60, false},
    {"dungeon.sprite_editor", "Sprite Editor", nullptr, ICON_MD_PERSON,
     "Dungeon", nullptr, nullptr, 65, false},
    {"dungeon.item_editor", "Item Editor", nullptr, ICON_MD_INVENTORY,
     "Dungeon", nullptr, nullptr, 66, false},
    {"dungeon.palette_editor", "Palette Editor", " Palette Editor",
     ICON_MD_PALETTE, "Dungeon", "Ctrl+Shift+P",
     "Load a ROM to edit dungeon palettes", 70, false},

    // Graphics
    {"graphics.sheet_browser_v2", "Sheet Browser", nullptr, ICON_MD_VIEW_LIST,
     "Graphics", nullptr, nullptr, 10, true},
    {"graphics.pixel_editor", "Pixel Editor", nullptr, ICON_MD_DRAW,
     "Graphics", nullptr, nullptr, 20, true},
    {"graphics.palette_controls", "Palette Controls", nullptr, ICON_MD_PALETTE,
     "Graphics", nullptr, nullptr, 30, false},
    {"graphics.link_sprite_editor", "Link Sprite Editor", nullptr,
     ICON_MD_PERSON, "Graphics", nullptr, nullptr, 35, false},
    {"graphics.gfx_group_editor", "Graphics Groups", nullptr,
     ICON_MD_VIEW_MODULE, "Graphics", nullptr, nullptr, 39, false},
    {"graphics.paletteset_editor", "Palettesets", nullptr,
     ICON_MD_COLOR_LENS, "Graphics", nullptr, nullptr, 45, false},

    // Palette
    {"palette.control_panel", "Palette Controls", " Palette Controls",
     ICON_MD_PALETTE, "Palette", "Ctrl+Shift+P", "Load a ROM first", 10, false},
    {"palette.ow_main", "Overworld Main", " Overworld Main", ICON_MD_LANDSCAPE,
     "Palette", "Ctrl+Alt+1", "Load a ROM first", 20, false},
    {"palette.ow_animated", "Overworld Animated", " Overworld Animated",
     ICON_MD_WATER, "Palette", "Ctrl+Alt+2", "Load a ROM first", 30, false},
    {"palette.dungeon_main", "Dungeon Main", " Dungeon Main", ICON_MD_CASTLE,
     "Palette", "Ctrl+Alt+3", "Load a ROM first", 40, false},
    {"palette.sprites", "Global Sprite Palettes", " SNES Palette",
     ICON_MD_PETS, "Palette", "Ctrl+Alt+4", "Load a ROM first", 50, false},
    {"palette.sprites_aux1", "Sprites Aux 1", " Sprites Aux 1",
     ICON_MD_FILTER_1, "Palette", "Ctrl+Alt+7", "Load a ROM first", 51, false},
    {"palette.sprites_aux2", "Sprites Aux 2", " Sprites Aux 2",
     ICON_MD_FILTER_2, "Palette", "Ctrl+Alt+8", "Load a ROM first", 52, false},
    {"palette.sprites_aux3", "Sprites Aux 3", " Sprites Aux 3",
     ICON_MD_FILTER_3, "Palette", "Ctrl+Alt+9", "Load a ROM first", 53, false},
    {"palette.equipment", "Equipment Palettes", " Equipment Palettes",
     ICON_MD_SHIELD, "Palette", "Ctrl+Alt+5", "Load a ROM first", 60, false},
    {"palette.quick_access", "Quick Access", " Color Harmony",
     ICON_MD_COLOR_LENS, "Palette", "Ctrl+Alt+Q", "Load a ROM first", 70, false},
    {"palette.custom", "Custom Palette", " Palette Editor", ICON_MD_BRUSH,
     "Palette", "Ctrl+Alt+C", "Load a ROM first", 80, false},

    // Sprite
    {"sprite.vanilla_editor", "Vanilla Sprites", nullptr, ICON_MD_SMART_TOY,
     "Sprite", nullptr, nullptr, 10, true},
    {"sprite.custom_editor", "Custom Sprites", nullptr, ICON_MD_ADD_CIRCLE,
     "Sprite", nullptr, nullptr, 20, false},

    // Screen
    {"screen.dungeon_maps", "Dungeon Maps", " Dungeon Map Editor", ICON_MD_MAP,
     "Screen", "Alt+1", "Load a ROM first", 10, false},
    {"screen.inventory_menu", "Inventory Menu", " Inventory Menu",
     ICON_MD_INVENTORY, "Screen", "Alt+2", "Load a ROM first", 20, false},
    {"screen.overworld_map", "Overworld Map", " Overworld Map", ICON_MD_PUBLIC,
     "Screen", "Alt+3", "Load a ROM first", 30, false},
    {"screen.title_screen", "Title Screen", " Title Screen", ICON_MD_TITLE,
     "Screen", "Alt+4", "Load a ROM first", 40, false},
    {"screen.naming_screen", "Naming Screen", " Naming Screen", ICON_MD_EDIT,
     "Screen", "Alt+5", "Load a ROM first", 50, false},

    // Music
    {"music.song_browser", "Song Browser", " Song Browser",
     ICON_MD_LIBRARY_MUSIC, "Music", "Ctrl+Shift+B", nullptr, 5, false},
    {"music.tracker", "Playback Control", " Playback Control",
     ICON_MD_PLAY_CIRCLE, "Music", "Ctrl+Shift+M", nullptr, 10, false},
    {"music.piano_roll", "Piano Roll", " Piano Roll", ICON_MD_PIANO, "Music",
     "Ctrl+Shift+P", nullptr, 15, false},
    {"music.instrument_editor", "Instrument Editor", " Instrument Editor",
     ICON_MD_SPEAKER, "Music", "Ctrl+Shift+I", nullptr, 20, false},
    {"music.sample_editor", "Sample Editor", " Sample Editor", ICON_MD_WAVES,
     "Music", "Ctrl+Shift+S", nullptr, 25, false},
    {"music.assembly", "Assembly View", " Music Assembly", ICON_MD_CODE,
     "Music", "Ctrl+Shift+A", nullptr, 30, false},

    // Message
    {"message.message_list", "Message List", nullptr, ICON_MD_LIST, "Message",
     nullptr, nullptr, 10, false},
    {"message.message_editor", "Message Editor", nullptr, ICON_MD_EDIT,
     "Message", nullptr, nullptr, 20, false},
    {"message.font_atlas", "Font Atlas", nullptr, ICON_MD_FONT_DOWNLOAD,
     "Message", nullptr, nullptr, 30, false},
    {"message.dictionary", "Dictionary", nullptr, ICON_MD_BOOK, "Message",
     nullptr, nullptr, 40, false},

    // Assembly
    {"assembly.toolbar", "Toolbar", nullptr, ICON_MD_CONSTRUCTION, "Assembly",
     nullptr, nullptr, 5, true},
    {"assembly.code_editor", "Code Editor", nullptr, ICON_MD_CODE, "Assembly",
     nullptr, nullptr, 10, true},
    {"assembly.file_browser", "File Browser", nullptr, ICON_MD_FOLDER_OPEN,
     "Assembly", nullptr, nullptr, 20, true},
    {"assembly.symbols", "Symbols", nullptr, ICON_MD_LIST_ALT, "Assembly",
     nullptr, nullptr, 30, false},
    {"assembly.build_output", "Build Output", nullptr, ICON_MD_TERMINAL,
     "Assembly", nullptr, nullptr, 40, false},

    // Emulator + Memory
    {"emulator.cpu_debugger", "CPU Debugger", " CPU Debugger",
     ICON_MD_BUG_REPORT, "Emulator", nullptr, nullptr, 10, false},
    {"emulator.ppu_viewer", "PPU Viewer", " PPU Viewer",
     ICON_MD_VIDEOGAME_ASSET, "Emulator", nullptr, nullptr, 20, false},
    {"emulator.memory_viewer", "Memory Viewer", " Memory Viewer",
     ICON_MD_MEMORY, "Emulator", nullptr, nullptr, 30, false},
    {"emulator.breakpoints", "Breakpoints", " Breakpoints", ICON_MD_STOP,
     "Emulator", nullptr, nullptr, 40, false},
    {"emulator.performance", "Performance", " Performance", ICON_MD_SPEED,
     "Emulator", nullptr, nullptr, 50, false},
    {"emulator.ai_agent", "AI Agent", " AI Agent", ICON_MD_SMART_TOY,
     "Emulator", nullptr, nullptr, 60, false},
    {"emulator.save_states", "Save States", " Save States", ICON_MD_SAVE,
     "Emulator", nullptr, nullptr, 70, false},
    {"emulator.keyboard_config", "Keyboard Config", " Keyboard Config",
     ICON_MD_KEYBOARD, "Emulator", nullptr, nullptr, 80, false},
    {"emulator.virtual_controller", "Virtual Controller",
     " Virtual Controller", ICON_MD_SPORTS_ESPORTS, "Emulator", nullptr, nullptr,
     85, false},
    {"emulator.apu_debugger", "APU Debugger", " APU Debugger",
     ICON_MD_AUDIOTRACK, "Emulator", nullptr, nullptr, 90, false},
    {"emulator.audio_mixer", "Audio Mixer", " Audio Mixer", ICON_MD_AUDIO_FILE,
     "Emulator", nullptr, nullptr, 100, false},
    {"memory.hex_editor", "Hex Editor", nullptr, ICON_MD_MEMORY, "Memory",
     nullptr, nullptr, 10, false},
};

void RegisterLabPanels(yaze::editor::PanelManager* panel_manager) {
  panel_manager->RegisterSession(kLabSessionId);
  panel_manager->SetActiveSession(kLabSessionId);

  for (const auto& panel : kLabPanels) {
    yaze::editor::PanelDescriptor descriptor;
    descriptor.card_id = panel.id;
    descriptor.display_name = panel.display_name;
    descriptor.window_title = panel.window_title ? panel.window_title : "";
    descriptor.icon = panel.icon ? panel.icon : "";
    descriptor.category = panel.category ? panel.category : "";
    descriptor.shortcut_hint = panel.shortcut ? panel.shortcut : "";
    descriptor.disabled_tooltip =
        panel.disabled_tooltip ? panel.disabled_tooltip : "";
    descriptor.visibility_flag = nullptr;
    descriptor.priority = panel.priority;

    panel_manager->RegisterPanel(kLabSessionId, descriptor);
    if (panel.visible_by_default) {
      panel_manager->ShowPanel(kLabSessionId, panel.id);
    }
  }
}

void DrawDockspace() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking |
                                  ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoNavFocus |
                                  ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("LabDockspace", nullptr, window_flags);
  ImGui::PopStyleVar(3);

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                   ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::End();
}

void DrawLabPanels(const yaze::editor::PanelManager& panel_manager) {
  const auto& descriptors = panel_manager.GetAllPanelDescriptors();
  for (const auto& [panel_id, descriptor] : descriptors) {
    if (descriptor.visibility_flag && !*descriptor.visibility_flag) {
      continue;
    }

    bool open = true;
    bool* open_ptr = descriptor.visibility_flag ? descriptor.visibility_flag : &open;
    const std::string title = descriptor.GetWindowTitle();

    if (ImGui::Begin(title.c_str(), open_ptr)) {
      ImGui::Text("Lab panel: %s", descriptor.display_name.c_str());
      ImGui::Text("Panel ID: %s", panel_id.c_str());
    }
    ImGui::End();
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  yaze::util::LogManager::instance().configure(
      yaze::util::LogLevel::INFO, "", std::set<std::string>{});

  auto window_backend = yaze::platform::WindowBackendFactory::Create(
      yaze::platform::WindowBackendFactory::GetDefaultType());
  if (!window_backend) {
    return EXIT_FAILURE;
  }

  yaze::platform::WindowConfig config;
  config.title = "Yaze Lab";
  config.width = 1400;
  config.height = 900;
  config.resizable = true;
  config.high_dpi = false;

  if (!window_backend->Initialize(config).ok()) {
    return EXIT_FAILURE;
  }

  auto renderer = yaze::gfx::RendererFactory::Create();
  if (!renderer || !window_backend->InitializeRenderer(renderer.get())) {
    window_backend->Shutdown();
    return EXIT_FAILURE;
  }

  if (!window_backend->InitializeImGui(renderer.get()).ok()) {
    renderer->Shutdown();
    window_backend->Shutdown();
    return EXIT_FAILURE;
  }

  yaze::editor::PanelManager panel_manager;
  RegisterLabPanels(&panel_manager);

  yaze::editor::LayoutManager layout_manager;
  layout_manager.SetPanelManager(&panel_manager);

  yaze::editor::layout_designer::LayoutDesignerWindow layout_designer;
  layout_designer.Initialize(&panel_manager, &layout_manager, nullptr);
  layout_designer.Open();

  bool running = true;
  yaze::platform::WindowEvent event;

  while (running && window_backend->IsActive()) {
    while (window_backend->PollEvent(event)) {
      if (event.type == yaze::platform::WindowEventType::Quit ||
          event.type == yaze::platform::WindowEventType::Close) {
        running = false;
      }
    }

    window_backend->NewImGuiFrame();
    ImGui::NewFrame();

    DrawDockspace();
    layout_designer.Draw();
    DrawLabPanels(panel_manager);

    ImGui::Render();
    renderer->Clear();
    window_backend->RenderImGui(renderer.get());
    renderer->Present();
  }

  window_backend->ShutdownImGui();
  renderer->Shutdown();
  window_backend->Shutdown();
  return EXIT_SUCCESS;
}
