#ifndef YAZE_CLI_HANDLERS_AGENT_COMMANDS_H_
#define YAZE_CLI_HANDLERS_AGENT_COMMANDS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace cli {
namespace agent {

absl::Status HandleRunCommand(const std::vector<std::string>& args,
							  Rom& rom);
absl::Status HandlePlanCommand(const std::vector<std::string>& args);
absl::Status HandleDiffCommand(Rom& rom,
							   const std::vector<std::string>& args);
absl::Status HandleAcceptCommand(const std::vector<std::string>& args, Rom& rom);
absl::Status HandleTestCommand(const std::vector<std::string>& args);
absl::Status HandleGuiCommand(const std::vector<std::string>& args);
absl::Status HandleLearnCommand(const std::vector<std::string>& args = {});
absl::Status HandleListCommand();
absl::Status HandleCommitCommand(Rom& rom);
absl::Status HandleRevertCommand(Rom& rom);
absl::Status HandleDescribeCommand(const std::vector<std::string>& arg_vec);
absl::Status HandleResourceListCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleResourceSearchCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleDungeonListSpritesCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleDungeonDescribeRoomCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleOverworldFindTileCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleOverworldDescribeMapCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleOverworldListWarpsCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleOverworldListSpritesCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleOverworldGetEntranceCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleOverworldTileStatsCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleMessageListCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleMessageReadCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleMessageSearchCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);

// GUI Automation Tools
absl::Status HandleGuiPlaceTileCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleGuiClickCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleGuiDiscoverToolCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleGuiScreenshotCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);

// Dialogue Inspection Tools
absl::Status HandleDialogueListCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleDialogueReadCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleDialogueSearchCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);

// Music Data Tools
absl::Status HandleMusicListCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleMusicInfoCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleMusicTracksCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);

// Sprite Property Tools
absl::Status HandleSpriteListCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleSpritePropertiesCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleSpritePaletteCommand(
	const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleChatCommand(Rom& rom);
absl::Status HandleSimpleChatCommand(const std::vector<std::string>&, Rom* rom, bool quiet);
absl::Status HandleTestConversationCommand(
	const std::vector<std::string>& arg_vec);

// Hex manipulation commands
absl::Status HandleHexRead(const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleHexWrite(const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandleHexSearch(const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);

// Palette manipulation commands
absl::Status HandlePaletteGetColors(const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandlePaletteSetColor(const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);
absl::Status HandlePaletteAnalyze(const std::vector<std::string>& arg_vec,
	Rom* rom_context = nullptr);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_AGENT_COMMANDS_H_
