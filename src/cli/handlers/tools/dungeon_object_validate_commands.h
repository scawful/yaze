#ifndef YAZE_CLI_HANDLERS_TOOLS_DUNGEON_OBJECT_VALIDATE_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_DUNGEON_OBJECT_VALIDATE_COMMANDS_H

#include "cli/service/resources/command_handler.h"
#include "zelda3/dungeon/object_dimensions.h"

namespace yaze::cli {

namespace detail {

zelda3::ObjectDimensionTable::SelectionBounds ClipSelectionBoundsToRoom(
    const zelda3::ObjectDimensionTable& dimension_table, int object_id,
    int size, const zelda3::ObjectDimensionTable::SelectionBounds& bounds,
    int object_x, int object_y);

}  // namespace detail

class DungeonObjectValidateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-object-validate"; }

  std::string GetDescription() const {
    return "Validate dungeon object draw bounds against dimension table";
  }

  std::string GetUsage() const override {
    return "dungeon-object-validate --rom <path> [--object <hex>] [--size <n>] "
           "[--room <id>] [--report <path>] [--trace-out <path>] "
           "[--format json|text] [--verbose]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override {
    return "Dungeon Object Validation";
  }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "dungeon-object-validate";
    desc.summary =
        "Trace dungeon object draws and compare bounds to selection "
        "dimensions.";
    desc.todo_reference = "todo#dungeon-object-validate";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    (void)parser;
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_DUNGEON_OBJECT_VALIDATE_COMMANDS_H
