#include "cli/service/ai_service.h"

namespace yaze {
namespace cli {

absl::StatusOr<std::vector<std::string>> MockAIService::GetCommands(
    const std::string& prompt) {
  if (prompt == "Make all the soldiers in Hyrule Castle wear red armor.") {
    return std::vector<std::string>{
        "palette export --group sprites_aux1 --id 4 --to soldier_palette.col",
        "palette set-color --file soldier_palette.col --index 5 --color \"#FF0000\"",
        "palette import --group sprites_aux1 --id 4 --from soldier_palette.col"};
  } else if (prompt.find("Place a tree at coordinates") != std::string::npos) {
      // Example prompt: "Place a tree at coordinates (10, 20) on the light world map"
      // For simplicity, we'll hardcode the tile id for a tree
      return std::vector<std::string>{"overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E"};
  }
  return absl::UnimplementedError("Prompt not supported by mock AI service.");
}

}  // namespace cli
}  // namespace yaze

