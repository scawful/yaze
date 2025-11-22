#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace resources {

CommandHandler::Descriptor CommandHandler::Describe() const {
  Descriptor descriptor;
  descriptor.display_name = GetUsage();
  descriptor.summary = "Command summary not provided.";
  descriptor.todo_reference = "todo#unassigned";
  return descriptor;
}

}  // namespace resources
}  // namespace cli
}  // namespace yaze

#include <iostream>

#include "absl/strings/str_format.h"
#include "cli/service/resources/command_handler.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace resources {

absl::Status CommandHandler::Run(const std::vector<std::string>& args,
                                 Rom* rom_context) {
  // 1. Parse arguments
  ArgumentParser parser(args);

  // 2. Validate arguments
  auto validation_status = ValidateArgs(parser);
  if (!validation_status.ok()) {
    std::cerr << "Error: " << validation_status.message() << "\n\n";
    std::cerr << "Usage: " << GetUsage() << "\n";
    return validation_status;
  }

  // 3. Get format string
  std::string format_str =
      parser.GetString("format").value_or(GetDefaultFormat());

  // 4. Create output formatter
  auto formatter_or = OutputFormatter::FromString(format_str);
  if (!formatter_or.ok()) {
    return formatter_or.status();
  }
  OutputFormatter formatter = std::move(formatter_or.value());

  // 5. Setup command context
  CommandContext::Config config;
  config.external_rom_context = rom_context;
  config.format = format_str;

  // Check for --rom override
  if (auto rom_path = parser.GetString("rom"); rom_path.has_value()) {
    config.rom_path = *rom_path;
  }

  // Check for --mock-rom flag
  config.use_mock_rom = parser.HasFlag("mock-rom");

  CommandContext context(config);

  // 6. Get ROM (loads if needed)
  ASSIGN_OR_RETURN(Rom * rom, context.GetRom());

  // 7. Ensure labels are loaded if required
  if (RequiresLabels()) {
    RETURN_IF_ERROR(context.EnsureLabelsLoaded(rom));
  }

  // 8. Begin output formatting
  formatter.BeginObject(GetOutputTitle());

  // 9. Execute command business logic
  auto execute_status = Execute(rom, parser, formatter);
  if (!execute_status.ok()) {
    return execute_status;
  }

  // 10. Finalize and print output
  formatter.EndObject();
  formatter.Print();

  return absl::OkStatus();
}

}  // namespace resources
}  // namespace cli
}  // namespace yaze
