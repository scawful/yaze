#include "app/editor/system/default_editor_factories.h"

#include <memory>

#include "app/editor/code/assembly_editor.h"
#include "app/editor/code/memory_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/ui/settings_panel.h"
#include "rom/rom.h"

namespace yaze::editor {

void RegisterDefaultEditorFactories(EditorRegistry* registry) {
  if (!registry) {
    return;
  }

  // Core editor set (used by RomSession/EditorSet construction).
  registry->RegisterFactory(EditorType::kAssembly,
                            [](Rom* rom) {
                              return std::make_unique<AssemblyEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kDungeon,
                            [](Rom* rom) {
                              return std::make_unique<DungeonEditorV2>(rom);
                            });
  registry->RegisterFactory(EditorType::kGraphics,
                            [](Rom* rom) {
                              return std::make_unique<GraphicsEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kMusic,
                            [](Rom* rom) {
                              return std::make_unique<MusicEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kOverworld,
                            [](Rom* rom) {
                              return std::make_unique<OverworldEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kPalette,
                            [](Rom* rom) {
                              return std::make_unique<PaletteEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kScreen,
                            [](Rom* rom) {
                              return std::make_unique<ScreenEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kSprite,
                            [](Rom* rom) {
                              return std::make_unique<SpriteEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kMessage,
                            [](Rom* rom) {
                              return std::make_unique<MessageEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kHex,
                            [](Rom* rom) {
                              return std::make_unique<MemoryEditor>(rom);
                            });
  registry->RegisterFactory(EditorType::kSettings,
                            [](Rom*) { return std::make_unique<SettingsPanel>(); });
}

}  // namespace yaze::editor

