#include "cli/handlers/commands.h"

#include "cli/handlers/tools/resource_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/game/message_commands.h"
#include "cli/handlers/game/dialogue_commands.h"
#include "cli/handlers/game/music_commands.h"
#include "cli/handlers/graphics/hex_commands.h"
#include "cli/handlers/graphics/palette_commands.h"
// #include "cli/handlers/graphics/sprite_commands.h"  // Implementations not available
#include "cli/handlers/tools/gui_commands.h"
#include "cli/handlers/tools/emulator_commands.h"
// #include "cli/handlers/rom/rom_commands.h"  // Not used in stubs
// #include "cli/handlers/rom/project_commands.h"  // Not used in stubs

namespace yaze {
namespace cli {
namespace handlers {

// Resource commands
absl::Status HandleResourceListCommand(const std::vector<std::string>& args, Rom* rom) {
  ResourceListCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleResourceSearchCommand(const std::vector<std::string>& args, Rom* rom) {
  ResourceSearchCommandHandler handler;
  return handler.Run(args, rom);
}

// Dungeon commands
absl::Status HandleDungeonListSpritesCommand(const std::vector<std::string>& args, Rom* rom) {
  DungeonListSpritesCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonDescribeRoomCommand(const std::vector<std::string>& args, Rom* rom) {
  DungeonDescribeRoomCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonExportRoomCommand(const std::vector<std::string>& args, Rom* rom) {
  DungeonExportRoomCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonListObjectsCommand(const std::vector<std::string>& args, Rom* rom) {
  DungeonListObjectsCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonGetRoomTilesCommand(const std::vector<std::string>& args, Rom* rom) {
  DungeonGetRoomTilesCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonSetRoomPropertyCommand(const std::vector<std::string>& args, Rom* rom) {
  DungeonSetRoomPropertyCommandHandler handler;
  return handler.Run(args, rom);
}

// Overworld commands
absl::Status HandleOverworldFindTileCommand(const std::vector<std::string>& args, Rom* rom) {
  OverworldFindTileCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldDescribeMapCommand(const std::vector<std::string>& args, Rom* rom) {
  OverworldDescribeMapCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldListWarpsCommand(const std::vector<std::string>& args, Rom* rom) {
  OverworldListWarpsCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldListSpritesCommand(const std::vector<std::string>& args, Rom* rom) {
  OverworldListSpritesCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldGetEntranceCommand(const std::vector<std::string>& args, Rom* rom) {
  OverworldGetEntranceCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldTileStatsCommand(const std::vector<std::string>& args, Rom* rom) {
  OverworldTileStatsCommandHandler handler;
  return handler.Run(args, rom);
}

// Message commands
absl::Status HandleMessageListCommand(const std::vector<std::string>& args, Rom* rom) {
  MessageListCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleMessageReadCommand(const std::vector<std::string>& args, Rom* rom) {
  MessageReadCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleMessageSearchCommand(const std::vector<std::string>& args, Rom* rom) {
  MessageSearchCommandHandler handler;
  return handler.Run(args, rom);
}

// Dialogue commands
absl::Status HandleDialogueListCommand(const std::vector<std::string>& args, Rom* rom) {
  DialogueListCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDialogueReadCommand(const std::vector<std::string>& args, Rom* rom) {
  DialogueReadCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDialogueSearchCommand(const std::vector<std::string>& args, Rom* rom) {
  DialogueSearchCommandHandler handler;
  return handler.Run(args, rom);
}

// Music commands
absl::Status HandleMusicListCommand(const std::vector<std::string>& args, Rom* rom) {
  MusicListCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleMusicInfoCommand(const std::vector<std::string>& args, Rom* rom) {
  MusicInfoCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleMusicTracksCommand(const std::vector<std::string>& args, Rom* rom) {
  MusicTracksCommandHandler handler;
  return handler.Run(args, rom);
}

// Sprite commands (stubs - implementations not available)
absl::Status HandleSpriteListCommand(const std::vector<std::string>& /*args*/, Rom* /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandleSpritePropertiesCommand(const std::vector<std::string>& /*args*/, Rom* /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandleSpritePaletteCommand(const std::vector<std::string>& /*args*/, Rom* /*rom*/) {
  return absl::OkStatus();
}

// GUI commands
absl::Status HandleGuiPlaceTileCommand(const std::vector<std::string>& args, Rom* rom) {
  GuiPlaceTileCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleGuiClickCommand(const std::vector<std::string>& args, Rom* rom) {
  GuiClickCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleGuiDiscoverToolCommand(const std::vector<std::string>& args, Rom* rom) {
  GuiDiscoverToolCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleGuiScreenshotCommand(const std::vector<std::string>& args, Rom* rom) {
  GuiScreenshotCommandHandler handler;
  return handler.Run(args, rom);
}

// Emulator commands
absl::Status HandleEmulatorStepCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorStepCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorRunCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorRunCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorPauseCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorPauseCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorResetCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorResetCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorGetStateCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorGetStateCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorSetBreakpointCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorSetBreakpointCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorClearBreakpointCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorClearBreakpointCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorListBreakpointsCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorListBreakpointsCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorReadMemoryCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorReadMemoryCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorWriteMemoryCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorWriteMemoryCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorGetRegistersCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorGetRegistersCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleEmulatorGetMetricsCommand(const std::vector<std::string>& args, Rom* rom) {
  EmulatorGetMetricsCommandHandler handler;
  return handler.Run(args, rom);
}

// Hex commands
absl::Status HandleHexRead(const std::vector<std::string>& args, Rom* rom) {
  HexReadCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleHexWrite(const std::vector<std::string>& args, Rom* rom) {
  HexWriteCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleHexSearch(const std::vector<std::string>& args, Rom* rom) {
  HexSearchCommandHandler handler;
  return handler.Run(args, rom);
}

// Palette commands
absl::Status HandlePaletteGetColors(const std::vector<std::string>& args, Rom* rom) {
  PaletteGetColorsCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandlePaletteSetColor(const std::vector<std::string>& args, Rom* rom) {
  PaletteSetColorCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandlePaletteAnalyze(const std::vector<std::string>& args, Rom* rom) {
  PaletteAnalyzeCommandHandler handler;
  return handler.Run(args, rom);
}

// Agent-specific commands (stubs for now)
absl::Status HandleRunCommand(const std::vector<std::string>& /*args*/, Rom& /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandlePlanCommand(const std::vector<std::string>& /*args*/) {
  return absl::OkStatus();
}

absl::Status HandleDiffCommand(Rom& /*rom*/, const std::vector<std::string>& /*args*/) {
  return absl::OkStatus();
}

absl::Status HandleAcceptCommand(const std::vector<std::string>& /*args*/, Rom& /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandleTestCommand(const std::vector<std::string>& /*args*/) {
  return absl::OkStatus();
}

absl::Status HandleGuiCommand(const std::vector<std::string>& /*args*/) {
  return absl::OkStatus();
}

absl::Status HandleLearnCommand(const std::vector<std::string>& /*args*/) {
  return absl::OkStatus();
}

absl::Status HandleListCommand() {
  return absl::OkStatus();
}

absl::Status HandleCommitCommand(Rom& /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandleRevertCommand(Rom& /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandleDescribeCommand(const std::vector<std::string>& /*arg_vec*/) {
  return absl::OkStatus();
}

absl::Status HandleChatCommand(Rom& /*rom*/) {
  return absl::OkStatus();
}

absl::Status HandleSimpleChatCommand(const std::vector<std::string>&, Rom* /*rom*/, bool /*quiet*/) {
  return absl::OkStatus();
}

absl::Status HandleTestConversationCommand(const std::vector<std::string>& /*arg_vec*/) {
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
