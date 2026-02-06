#include "cli/handlers/command_handlers.h"

#include <memory>

#include "cli/handlers/game/dialogue_commands.h"
#include "cli/handlers/game/dungeon_collision_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/dungeon_graph_commands.h"
#include "cli/handlers/game/dungeon_group_commands.h"
#include "cli/handlers/game/dungeon_map_commands.h"
#include "cli/handlers/game/minecart_commands.h"
#include "cli/handlers/game/message_commands.h"
#include "cli/handlers/game/music_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/game/overworld_graph_commands.h"
#include "cli/handlers/graphics/hex_commands.h"
#include "cli/handlers/graphics/palette_commands.h"
#include "cli/handlers/graphics/sprite_commands.h"
#include "cli/handlers/rom/rom_commands.h"
#include "cli/handlers/tools/dungeon_doctor_commands.h"
#include "cli/handlers/tools/dungeon_object_validate_commands.h"
#include "cli/handlers/mesen_handlers.h"
#ifdef YAZE_WITH_GRPC
#include "cli/handlers/tools/emulator_commands.h"
#endif
#include "cli/handlers/tools/graphics_doctor_commands.h"
#include "cli/handlers/tools/gui_commands.h"
#include "cli/handlers/tools/hex_inspector_commands.h"
#include "cli/handlers/tools/message_doctor_commands.h"
#include "cli/handlers/tools/overworld_doctor_commands.h"
#include "cli/handlers/tools/overworld_validate_commands.h"
#include "cli/handlers/tools/resource_commands.h"
#include "cli/handlers/tools/rom_compare_commands.h"
#include "cli/handlers/tools/rom_doctor_commands.h"
#include "cli/handlers/tools/sprite_doctor_commands.h"
#include "cli/handlers/tools/test_cli_commands.h"
#include "cli/handlers/tools/test_helpers_commands.h"

namespace yaze {
namespace cli {
namespace handlers {

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateCliCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

  // Graphics commands
  handlers.push_back(std::make_unique<HexReadCommandHandler>());
  handlers.push_back(std::make_unique<HexWriteCommandHandler>());
  handlers.push_back(std::make_unique<HexSearchCommandHandler>());

  // Palette commands
  handlers.push_back(std::make_unique<PaletteGetColorsCommandHandler>());
  handlers.push_back(std::make_unique<PaletteSetColorCommandHandler>());
  handlers.push_back(std::make_unique<PaletteAnalyzeCommandHandler>());

  // Sprite commands
  handlers.push_back(std::make_unique<SpriteListCommandHandler>());
  handlers.push_back(std::make_unique<SpritePropertiesCommandHandler>());
  handlers.push_back(std::make_unique<SpritePaletteCommandHandler>());

  // Music commands
  handlers.push_back(std::make_unique<MusicListCommandHandler>());
  handlers.push_back(std::make_unique<MusicInfoCommandHandler>());
  handlers.push_back(std::make_unique<MusicTracksCommandHandler>());

  // Dialogue commands
  handlers.push_back(std::make_unique<DialogueListCommandHandler>());
  handlers.push_back(std::make_unique<DialogueReadCommandHandler>());
  handlers.push_back(std::make_unique<DialogueSearchCommandHandler>());

  // Message commands
  handlers.push_back(std::make_unique<MessageListCommandHandler>());
  handlers.push_back(std::make_unique<MessageReadCommandHandler>());
  handlers.push_back(std::make_unique<MessageSearchCommandHandler>());
  handlers.push_back(std::make_unique<MessageEncodeCommandHandler>());
  handlers.push_back(std::make_unique<MessageDecodeCommandHandler>());
  handlers.push_back(std::make_unique<MessageImportOrgCommandHandler>());
  handlers.push_back(std::make_unique<MessageExportOrgCommandHandler>());
  handlers.push_back(std::make_unique<MessageExportBundleCommandHandler>());
  handlers.push_back(std::make_unique<MessageImportBundleCommandHandler>());
  handlers.push_back(std::make_unique<MessageWriteCommandHandler>());
  handlers.push_back(std::make_unique<MessageExportBinCommandHandler>());
  handlers.push_back(std::make_unique<MessageExportAsmCommandHandler>());

  // ROM commands
  handlers.push_back(std::make_unique<RomReadCommandHandler>());
  handlers.push_back(std::make_unique<RomWriteCommandHandler>());
  handlers.push_back(std::make_unique<RomInfoCommandHandler>());
  handlers.push_back(std::make_unique<RomValidateCommandHandler>());
  handlers.push_back(std::make_unique<RomDiffCommandHandler>());
  handlers.push_back(std::make_unique<RomGenerateGoldenCommandHandler>());
  handlers.push_back(std::make_unique<RomResolveAddressCommandHandler>());
  handlers.push_back(std::make_unique<RomFindSymbolCommandHandler>());

  // Resource inspection tools
  handlers.push_back(std::make_unique<ResourceListCommandHandler>());
  handlers.push_back(std::make_unique<ResourceSearchCommandHandler>());

  // Dungeon inspection
  handlers.push_back(std::make_unique<DungeonListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonDescribeRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListChestsCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGetEntranceCommandHandler>());
  handlers.push_back(std::make_unique<DungeonExportRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListObjectsCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListCustomCollisionCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGetRoomTilesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonSetRoomPropertyCommandHandler>());
  handlers.push_back(std::make_unique<DungeonRoomHeaderCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGraphCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGroupCommandHandler>());
  handlers.push_back(std::make_unique<DungeonMapCommandHandler>());
  handlers.push_back(std::make_unique<DungeonMinecartAuditCommandHandler>());
  handlers.push_back(
      std::make_unique<DungeonGenerateTrackCollisionCommandHandler>());
  handlers.push_back(std::make_unique<EntranceInfoCommandHandler>());
  handlers.push_back(std::make_unique<DungeonDiscoverCommandHandler>());

  // Overworld inspection
  handlers.push_back(std::make_unique<OverworldFindTileCommandHandler>());
  handlers.push_back(std::make_unique<OverworldDescribeMapCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListWarpsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListItemsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldGetEntranceCommandHandler>());
  handlers.push_back(std::make_unique<OverworldTileStatsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldExportGraphCommandHandler>());

  // GUI automation tools
  handlers.push_back(std::make_unique<GuiPlaceTileCommandHandler>());
  handlers.push_back(std::make_unique<GuiClickCommandHandler>());
  handlers.push_back(std::make_unique<GuiDiscoverToolCommandHandler>());
  handlers.push_back(std::make_unique<GuiScreenshotCommandHandler>());

  // Emulator & debugger commands
#ifdef YAZE_WITH_GRPC
  handlers.push_back(std::make_unique<EmulatorStepCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorRunCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorPauseCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorResetCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorGetStateCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorSetBreakpointCommandHandler>());
  handlers.push_back(
      std::make_unique<EmulatorClearBreakpointCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorListBreakpointsCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorReadMemoryCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorWriteMemoryCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorGetRegistersCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorGetMetricsCommandHandler>());
#endif

  // Mesen2 command handlers (socket-based)
  for (auto& handler : CreateMesenCommandHandlers()) {
    handlers.push_back(std::move(handler));
  }

  // Test helper tools
  handlers.push_back(std::make_unique<ToolsListCommandHandler>());
  handlers.push_back(std::make_unique<ToolsHarnessStateCommandHandler>());
  handlers.push_back(std::make_unique<ToolsExtractValuesCommandHandler>());
  handlers.push_back(std::make_unique<ToolsExtractGoldenCommandHandler>());
  handlers.push_back(std::make_unique<ToolsPatchV3CommandHandler>());

  // Hex Inspector
  handlers.push_back(std::make_unique<HexDumpCommandHandler>());
  handlers.push_back(std::make_unique<HexCompareCommandHandler>());
  handlers.push_back(std::make_unique<HexAnnotateCommandHandler>());

  // Test CLI commands
  handlers.push_back(std::make_unique<TestListCommandHandler>());
  handlers.push_back(std::make_unique<TestRunCommandHandler>());
  handlers.push_back(std::make_unique<TestStatusCommandHandler>());

  // Validation and repair tools (doctor suite)
  handlers.push_back(std::make_unique<OverworldValidateCommandHandler>());
  handlers.push_back(std::make_unique<OverworldDoctorCommandHandler>());
  handlers.push_back(std::make_unique<DungeonDoctorCommandHandler>());
  handlers.push_back(std::make_unique<DungeonObjectValidateCommandHandler>());
  handlers.push_back(std::make_unique<RomDoctorCommandHandler>());
  handlers.push_back(std::make_unique<MessageDoctorCommandHandler>());
  handlers.push_back(std::make_unique<SpriteDoctorCommandHandler>());
  handlers.push_back(std::make_unique<GraphicsDoctorCommandHandler>());
  handlers.push_back(std::make_unique<RomCompareCommandHandler>());

  return handlers;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
