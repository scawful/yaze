#include "cli/service/agent/tool_registry.h"

#include "cli/service/resources/command_context.h"
#include "cli/handlers/game/dialogue_commands.h"
#include "cli/handlers/game/dungeon_collision_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/message_commands.h"
#include "cli/handlers/game/minecart_commands.h"
#include "cli/handlers/game/music_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/graphics/sprite_commands.h"
#include "cli/handlers/tools/test_helpers_commands.h"
#ifdef YAZE_WITH_GRPC
#include "cli/handlers/tools/emulator_commands.h"
#endif
#include "cli/handlers/tools/gui_commands.h"
#include "cli/handlers/tools/resource_commands.h"
#include "cli/service/agent/tools/code_gen_tool.h"
#include "cli/service/agent/tools/filesystem_tool.h"
#include "cli/service/agent/tools/memory_inspector_tool.h"
#include "cli/service/agent/tools/project_tool.h"
#include "cli/service/agent/tools/visual_analysis_tool.h"
#include "cli/service/agent/tools/project_graph_tool.h" // New tool

namespace yaze {
namespace cli {
namespace agent {

using namespace yaze::cli::handlers;
using namespace yaze::cli::agent::tools;

// Placeholder classes for meta-tools (they're handled specially in the dispatcher)
// We need unique classes to avoid REGISTER_AGENT_TOOL macro struct name collisions
class MetaToolsListHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-list"; }
  std::string GetUsage() const override { return "tools-list"; }
  bool RequiresRom() const override { return false; }
 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override { return absl::OkStatus(); }
  absl::Status Execute(Rom*, const resources::ArgumentParser&, resources::OutputFormatter&) override {
    return absl::UnimplementedError("Handled by ToolDispatcher meta-tool handler");
  }
};

class MetaToolsDescribeHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-describe"; }
  std::string GetUsage() const override { return "tools-describe --name=<tool>"; }
  bool RequiresRom() const override { return false; }
 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override { return absl::OkStatus(); }
  absl::Status Execute(Rom*, const resources::ArgumentParser&, resources::OutputFormatter&) override {
    return absl::UnimplementedError("Handled by ToolDispatcher meta-tool handler");
  }
};

class MetaToolsSearchHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-search"; }
  std::string GetUsage() const override { return "tools-search --query=<keyword>"; }
  bool RequiresRom() const override { return false; }
 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser&) override { return absl::OkStatus(); }
  absl::Status Execute(Rom*, const resources::ArgumentParser&, resources::OutputFormatter&) override {
    return absl::UnimplementedError("Handled by ToolDispatcher meta-tool handler");
  }
};

// Meta-tools (handled specially but registered for discovery)
REGISTER_AGENT_TOOL("tools-list", "meta", "List all available tools", "tools-list", {}, false, false, MetaToolsListHandler)
REGISTER_AGENT_TOOL("tools-describe", "meta", "Get detailed information about a tool", "tools-describe --name=<tool>", {}, false, false, MetaToolsDescribeHandler)
REGISTER_AGENT_TOOL("tools-search", "meta", "Search tools by keyword", "tools-search --query=<keyword>", {}, false, false, MetaToolsSearchHandler)

// Resource commands
REGISTER_AGENT_TOOL("resource-list", "resource", "List resource labels", "resource-list --type=<type>", {"resource-list --type=dungeon"}, true, false, ResourceListCommandHandler)
REGISTER_AGENT_TOOL("resource-search", "resource", "Search resource labels", "resource-search --query=<query>", {"resource-search --query=castle"}, true, false, ResourceSearchCommandHandler)

// Dungeon commands
REGISTER_AGENT_TOOL("dungeon-list-sprites", "dungeon", "List sprites in a dungeon room", "dungeon-list-sprites --room=<id>", {}, true, false, DungeonListSpritesCommandHandler)
REGISTER_AGENT_TOOL("dungeon-describe-room", "dungeon", "Describe a room", "dungeon-describe-room --room=<id>", {}, true, false, DungeonDescribeRoomCommandHandler)
REGISTER_AGENT_TOOL("dungeon-export-room", "dungeon", "Export room data", "dungeon-export-room --room=<id>", {}, true, false, DungeonExportRoomCommandHandler)
REGISTER_AGENT_TOOL("dungeon-list-objects", "dungeon", "List room objects", "dungeon-list-objects --room=<id>", {}, true, false, DungeonListObjectsCommandHandler)
REGISTER_AGENT_TOOL("dungeon-list-custom-collision", "dungeon", "List custom collision tiles for a room", "dungeon-list-custom-collision --room=<id>", {}, true, false, DungeonListCustomCollisionCommandHandler)
REGISTER_AGENT_TOOL("dungeon-export-custom-collision-json", "dungeon", "Export custom collision maps to JSON", "dungeon-export-custom-collision-json --out=<path> [--room=<id>|--rooms=<ids>|--all]", {}, true, true, DungeonExportCustomCollisionJsonCommandHandler)
REGISTER_AGENT_TOOL("dungeon-import-custom-collision-json", "dungeon", "Import custom collision maps from JSON", "dungeon-import-custom-collision-json --in=<path> [--replace-all]", {}, true, true, DungeonImportCustomCollisionJsonCommandHandler)
REGISTER_AGENT_TOOL("dungeon-export-water-fill-json", "dungeon", "Export water fill zones to JSON", "dungeon-export-water-fill-json --out=<path> [--room=<id>|--rooms=<ids>|--all]", {}, true, true, DungeonExportWaterFillJsonCommandHandler)
REGISTER_AGENT_TOOL("dungeon-import-water-fill-json", "dungeon", "Import water fill zones from JSON", "dungeon-import-water-fill-json --in=<path>", {}, true, true, DungeonImportWaterFillJsonCommandHandler)
REGISTER_AGENT_TOOL("dungeon-minecart-audit", "dungeon", "Audit minecart-related room data (objects/sprites/collision)", "dungeon-minecart-audit --room=<id>", {}, true, false, DungeonMinecartAuditCommandHandler)
REGISTER_AGENT_TOOL("dungeon-get-room-tiles", "dungeon", "Get room tiles", "dungeon-get-room-tiles --room=<id>", {}, true, false, DungeonGetRoomTilesCommandHandler)
REGISTER_AGENT_TOOL("dungeon-set-room-property", "dungeon", "Set room property", "dungeon-set-room-property --room=<id> --key=<key> --value=<val>", {}, true, false, DungeonSetRoomPropertyCommandHandler)
REGISTER_AGENT_TOOL("dungeon-room-header", "dungeon", "Debug room header bytes", "dungeon-room-header --room=<id>", {}, true, false, DungeonRoomHeaderCommandHandler)

// Overworld commands
REGISTER_AGENT_TOOL("overworld-find-tile", "overworld", "Find tile locations", "overworld-find-tile --tile=<id>", {}, true, false, OverworldFindTileCommandHandler)
REGISTER_AGENT_TOOL("overworld-describe-map", "overworld", "Describe a map", "overworld-describe-map --map=<id>", {}, true, false, OverworldDescribeMapCommandHandler)
REGISTER_AGENT_TOOL("overworld-list-warps", "overworld", "List warps", "overworld-list-warps --map=<id>", {}, true, false, OverworldListWarpsCommandHandler)
REGISTER_AGENT_TOOL("overworld-list-sprites", "overworld", "List sprites", "overworld-list-sprites --map=<id>", {}, true, false, OverworldListSpritesCommandHandler)
REGISTER_AGENT_TOOL("overworld-get-entrance", "overworld", "Get entrance info", "overworld-get-entrance --id=<id>", {}, true, false, OverworldGetEntranceCommandHandler)
REGISTER_AGENT_TOOL("overworld-tile-stats", "overworld", "Get tile statistics", "overworld-tile-stats", {}, true, false, OverworldTileStatsCommandHandler)

// Message & Dialogue commands
REGISTER_AGENT_TOOL("message-list", "message", "List messages", "message-list", {}, true, false, MessageListCommandHandler)
REGISTER_AGENT_TOOL("message-read", "message", "Read message", "message-read --id=<id>", {}, true, false, MessageReadCommandHandler)
REGISTER_AGENT_TOOL("message-search", "message", "Search messages", "message-search --query=<text>", {}, true, false, MessageSearchCommandHandler)
REGISTER_AGENT_TOOL("dialogue-list", "dialogue", "List dialogues", "dialogue-list", {}, true, false, DialogueListCommandHandler)
REGISTER_AGENT_TOOL("dialogue-read", "dialogue", "Read dialogue", "dialogue-read --id=<id>", {}, true, false, DialogueReadCommandHandler)
REGISTER_AGENT_TOOL("dialogue-search", "dialogue", "Search dialogues", "dialogue-search --query=<text>", {}, true, false, DialogueSearchCommandHandler)

// GUI Automation commands
REGISTER_AGENT_TOOL("gui-place-tile", "gui", "Place a tile on canvas", "gui-place-tile --x=<x> --y=<y> --tile=<id>", {}, false, false, GuiPlaceTileCommandHandler)
REGISTER_AGENT_TOOL("gui-click", "gui", "Click GUI element", "gui-click --element=<id>", {}, false, false, GuiClickCommandHandler)
REGISTER_AGENT_TOOL("gui-discover-tool", "gui", "Discover GUI tools", "gui-discover-tool", {}, false, false, GuiDiscoverToolCommandHandler)
REGISTER_AGENT_TOOL("gui-screenshot", "gui", "Take screenshot", "gui-screenshot", {}, false, false, GuiScreenshotCommandHandler)
REGISTER_AGENT_TOOL("gui-summarize-widgets", "gui", "Get a high-level summary of visible GUI widgets", "gui-summarize-widgets", {}, false, false, GuiSummarizeWidgetsCommandHandler)

// Music commands
REGISTER_AGENT_TOOL("music-list", "music", "List music tracks", "music-list", {}, true, false, MusicListCommandHandler)
REGISTER_AGENT_TOOL("music-info", "music", "Get music info", "music-info --id=<id>", {}, true, false, MusicInfoCommandHandler)
REGISTER_AGENT_TOOL("music-tracks", "music", "List tracks", "music-tracks", {}, true, false, MusicTracksCommandHandler)

// Sprite commands
REGISTER_AGENT_TOOL("sprite-list", "sprite", "List sprites", "sprite-list", {}, true, false, SpriteListCommandHandler)
REGISTER_AGENT_TOOL("sprite-properties", "sprite", "Get sprite properties", "sprite-properties --id=<id>", {}, true, false, SpritePropertiesCommandHandler)
REGISTER_AGENT_TOOL("sprite-palette", "sprite", "Get sprite palette", "sprite-palette --id=<id>", {}, true, false, SpritePaletteCommandHandler)

// Filesystem commands
REGISTER_AGENT_TOOL("filesystem-list", "filesystem", "List directory contents", "filesystem-list --path=<path>", {}, false, false, FileSystemListTool)
REGISTER_AGENT_TOOL("filesystem-read", "filesystem", "Read file contents", "filesystem-read --path=<path>", {}, false, false, FileSystemReadTool)
REGISTER_AGENT_TOOL("filesystem-exists", "filesystem", "Check file existence", "filesystem-exists --path=<path>", {}, false, false, FileSystemExistsTool)
REGISTER_AGENT_TOOL("filesystem-info", "filesystem", "Get file info", "filesystem-info --path=<path>", {}, false, false, FileSystemInfoTool)

// Memory inspector commands
REGISTER_AGENT_TOOL("memory-analyze", "memory", "Analyze memory region", "memory-analyze --address=<addr> --length=<len>", {}, true, false, MemoryAnalyzeTool)
REGISTER_AGENT_TOOL("memory-search", "memory", "Search for byte pattern", "memory-search --pattern=<hex>", {}, true, false, MemorySearchTool)
REGISTER_AGENT_TOOL("memory-compare", "memory", "Compare memory regions", "memory-compare", {}, true, false, MemoryCompareTool)
REGISTER_AGENT_TOOL("memory-check", "memory", "Check memory", "memory-check", {}, true, false, MemoryCheckTool)
REGISTER_AGENT_TOOL("memory-regions", "memory", "List known memory regions", "memory-regions", {}, false, false, MemoryRegionsTool)

// Test helper tools
REGISTER_AGENT_TOOL("tools-helper-list", "tools", "List test helper tools", "tools-helper-list", {}, false, false, ToolsListCommandHandler)
REGISTER_AGENT_TOOL("tools-harness-state", "tools", "Generate WRAM state", "tools-harness-state", {}, false, false, ToolsHarnessStateCommandHandler)
REGISTER_AGENT_TOOL("tools-extract-values", "tools", "Extract vanilla ROM values", "tools-extract-values", {}, true, false, ToolsExtractValuesCommandHandler)
REGISTER_AGENT_TOOL("tools-extract-golden", "tools", "Extract golden data", "tools-extract-golden", {}, true, false, ToolsExtractGoldenCommandHandler)
REGISTER_AGENT_TOOL("tools-patch-v3", "tools", "Create v3 patched ROM", "tools-patch-v3", {}, true, false, ToolsPatchV3CommandHandler)

// Visual analysis tools
REGISTER_AGENT_TOOL("visual-find-similar-tiles", "visual", "Find tiles with similar patterns", "visual-find-similar-tiles", {}, true, false, TileSimilarityTool)
REGISTER_AGENT_TOOL("visual-analyze-spritesheet", "visual", "Identify unused regions", "visual-analyze-spritesheet", {}, true, false, SpritesheetAnalysisTool)
REGISTER_AGENT_TOOL("visual-palette-usage", "visual", "Analyze palette usage", "visual-palette-usage", {}, true, false, PaletteUsageTool)
REGISTER_AGENT_TOOL("visual-tile-histogram", "visual", "Generate tile usage histogram", "visual-tile-histogram", {}, true, false, TileHistogramTool)

// Code generation tools
REGISTER_AGENT_TOOL("codegen-asm-hook", "codegen", "Generate ASM hook", "codegen-asm-hook", {}, true, false, CodeGenAsmHookTool)
REGISTER_AGENT_TOOL("codegen-freespace-patch", "codegen", "Generate patch", "codegen-freespace-patch", {}, true, false, CodeGenFreespacePatchTool)
REGISTER_AGENT_TOOL("codegen-sprite-template", "codegen", "Generate sprite ASM", "codegen-sprite-template", {}, false, false, CodeGenSpriteTemplateTool)
REGISTER_AGENT_TOOL("codegen-event-handler", "codegen", "Generate event handler", "codegen-event-handler", {}, false, false, CodeGenEventHandlerTool)

// Project management tools
REGISTER_AGENT_TOOL("project-status", "project", "Show current project state", "project-status", {}, true, true, ProjectStatusTool)
REGISTER_AGENT_TOOL("project-snapshot", "project", "Create named checkpoint", "project-snapshot", {}, true, true, ProjectSnapshotTool)
REGISTER_AGENT_TOOL("project-restore", "project", "Restore ROM to checkpoint", "project-restore", {}, true, true, ProjectRestoreTool)
REGISTER_AGENT_TOOL("project-export", "project", "Export project", "project-export", {}, true, true, ProjectExportTool)
REGISTER_AGENT_TOOL("project-import", "project", "Import project", "project-import", {}, false, true, ProjectImportTool)
REGISTER_AGENT_TOOL("project-diff", "project", "Compare project states", "project-diff", {}, true, true, ProjectDiffTool)
REGISTER_AGENT_TOOL("project-graph", "project", "Query project graph info (files, symbols, config)", "project-graph --query=<info|files|symbols> [--path=<folder>]", {}, true, true, ProjectGraphTool)

#ifdef YAZE_WITH_GRPC
REGISTER_AGENT_TOOL("emulator-step", "emulator", "Step emulator", "emulator-step", {}, true, false, EmulatorStepCommandHandler)
REGISTER_AGENT_TOOL("emulator-run", "emulator", "Run emulator", "emulator-run", {}, true, false, EmulatorRunCommandHandler)
REGISTER_AGENT_TOOL("emulator-pause", "emulator", "Pause emulator", "emulator-pause", {}, true, false, EmulatorPauseCommandHandler)
REGISTER_AGENT_TOOL("emulator-reset", "emulator", "Reset emulator", "emulator-reset", {}, true, false, EmulatorResetCommandHandler)
REGISTER_AGENT_TOOL("emulator-get-state", "emulator", "Get emulator state", "emulator-get-state", {}, true, false, EmulatorGetStateCommandHandler)
REGISTER_AGENT_TOOL("emulator-set-breakpoint", "emulator", "Set breakpoint", "emulator-set-breakpoint", {}, true, false, EmulatorSetBreakpointCommandHandler)
REGISTER_AGENT_TOOL("emulator-clear-breakpoint", "emulator", "Clear breakpoint", "emulator-clear-breakpoint", {}, true, false, EmulatorClearBreakpointCommandHandler)
REGISTER_AGENT_TOOL("emulator-list-breakpoints", "emulator", "List breakpoints", "emulator-list-breakpoints", {}, true, false, EmulatorListBreakpointsCommandHandler)
REGISTER_AGENT_TOOL("emulator-read-memory", "emulator", "Read memory", "emulator-read-memory", {}, true, false, EmulatorReadMemoryCommandHandler)
REGISTER_AGENT_TOOL("emulator-write-memory", "emulator", "Write memory", "emulator-write-memory", {}, true, false, EmulatorWriteMemoryCommandHandler)
REGISTER_AGENT_TOOL("emulator-get-registers", "emulator", "Get registers", "emulator-get-registers", {}, true, false, EmulatorGetRegistersCommandHandler)
REGISTER_AGENT_TOOL("emulator-get-metrics", "emulator", "Get metrics", "emulator-get-metrics", {}, true, false, EmulatorGetMetricsCommandHandler)
#endif

} // namespace agent
} // namespace cli
} // namespace yaze
