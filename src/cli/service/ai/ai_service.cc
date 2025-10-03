#include "cli/service/ai/ai_service.h"

namespace yaze {
namespace cli {

absl::StatusOr<std::vector<std::string>> MockAIService::GetCommands(
    const std::string& prompt) {
  // NOTE: These commands use positional arguments (not --flags) because
  // the command handlers haven't been updated to parse flags yet.
  // TODO: Update handlers to use absl::flags parsing
  
  if (prompt == "Make all the soldiers in Hyrule Castle wear red armor.") {
    // Simplified command sequence - just export then import
    // (In reality, you'd modify the palette file between export and import)
    return std::vector<std::string>{
        "palette export sprites_aux1 4 soldier_palette.col"
        // Would normally modify soldier_palette.col here to change colors
        // Then import it back
    };
  } else if (prompt == "Place a tree") {
      // Example: Place a tree on the light world map
      // Command format: map_id x y tile_id (hex)
      return std::vector<std::string>{"overworld set-tile 0 10 20 0x02E"};
  }
  return absl::UnimplementedError("Prompt not supported by mock AI service. Try: 'Make all the soldiers in Hyrule Castle wear red armor.' or 'Place a tree'");
}

}  // namespace cli
}  // namespace yaze

