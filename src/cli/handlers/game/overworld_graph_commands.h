#ifndef YAZE_CLI_HANDLERS_OVERWORLD_GRAPH_COMMANDS_H_
#define YAZE_CLI_HANDLERS_OVERWORLD_GRAPH_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for exporting overworld connectivity graph
 *
 * Extracts world graph data from ROM including:
 * - Area definitions (ID, name, world, grid position, bounds)
 * - Area connections (from transition tables)
 * - Entrances (overworld to underworld warps)
 * - Exits (underworld to overworld returns)
 *
 * Output format is JSON suitable for pathfinding navigation systems.
 */
class OverworldExportGraphCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-export-graph"; }
  std::string GetDescription() const {
    return "Export overworld connectivity graph for navigation";
  }
  std::string GetUsage() const override {
    return "overworld-export-graph [--world <light|dark|all>] "
           "[--output <path>] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    return absl::OkStatus();  // All args are optional
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  // Structures for graph data
  struct AreaInfo {
    int id;
    std::string name;
    int world;
    int grid_x;
    int grid_y;
    std::string size;
    int parent_id;
    int min_x, min_y, max_x, max_y;  // Pixel bounds
  };

  struct AreaConnection {
    int from_area;
    int to_area;
    std::string direction;
    int edge_x;
    int edge_y;
    bool bidirectional;
  };

  struct EntranceInfo {
    int id;
    std::string name;
    int area_id;
    int position_x;
    int position_y;
    int dest_room_id;
    bool is_hole;
  };

  struct ExitInfo {
    int room_id;
    std::string name;
    int return_area_id;
    int return_x;
    int return_y;
  };
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_OVERWORLD_GRAPH_COMMANDS_H_
