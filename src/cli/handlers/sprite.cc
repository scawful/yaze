#include "cli/cli.h"
#include "app/zelda3/sprite/sprite_builder.h"
#include "absl/flags/flag.h"

namespace yaze {
namespace cli {

absl::Status SpriteCreate::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2 || arg_vec[0] != "--name") {
    return absl::InvalidArgumentError("Usage: sprite create --name <sprite_name>");
  }

  std::string sprite_name = arg_vec[1];

  // Create a simple sprite with a single action
  auto builder = zelda3::SpriteBuilder::Create(sprite_name)
    .SetProperty("!Health", 1)
    .SetProperty("!Damage", 2)
    .AddAction(zelda3::SpriteAction::Create("MAIN")
      .AddInstruction(zelda3::SpriteInstruction::ApplySpeedTowardsPlayer(1))
      .AddInstruction(zelda3::SpriteInstruction::MoveXyz())
    );

  std::cout << builder.Build() << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
