#include "cli/service/resources/command_handler.h"

#include <iostream>
#include <optional>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "util/macro.h"

ABSL_DECLARE_FLAG(bool, sandbox);

namespace yaze {
namespace cli {
namespace resources {

absl::Status CommandHandler::Run(const std::vector<std::string>& args,
                                 Rom* rom_context,
                                 std::string* captured_output) {
  // 1. Parse arguments
  ArgumentParser parser(args);

  // 2. Validate arguments
  auto validation_status = ValidateArgs(parser);
  if (!validation_status.ok()) {
    std::cerr << "Error: " << validation_status.message() << "\n\n";
    std::cerr << "Usage: " << GetUsage() << "\n";
    return validation_status;
  }

  // 3. Get format string (output format). Some commands reuse --format for
  //    data formatting (hex/ascii/both). If so, fall back to default output.
  std::string format_str =
      parser.GetString("format").value_or(GetDefaultFormat());

  // 4. Create output formatter
  auto formatter_or = OutputFormatter::FromString(format_str);
  if (!formatter_or.ok()) {
    if (format_str == "hex" || format_str == "ascii" || format_str == "both" ||
        format_str == "binary") {
      formatter_or = OutputFormatter::FromString(GetDefaultFormat());
    } else {
      return formatter_or.status();
    }
  }
  OutputFormatter formatter = std::move(formatter_or.value());

  // 5. Setup command context
  CommandContext::Config config;
  config.external_rom_context = rom_context;
  config.format = format_str;
  config.verbose = parser.HasFlag("verbose");

  // Check for --rom override
  if (auto rom_path = parser.GetString("rom"); rom_path.has_value()) {
    config.rom_path = *rom_path;
  }

  // Check for --mock-rom flag
  config.use_mock_rom = parser.HasFlag("mock-rom");

  CommandContext context(config);

  // 6. Get ROM (loads if needed) - only if command requires it
  Rom* rom = nullptr;
  std::optional<Rom> sandbox_rom;
  bool sandbox_enabled = false;
  if (RequiresRom()) {
    ASSIGN_OR_RETURN(rom, context.GetRom());
    SetRomContext(rom);

    if (absl::GetFlag(FLAGS_sandbox) || parser.HasFlag("sandbox")) {
      sandbox_enabled = true;
      auto sandbox_or =
          RomSandboxManager::Instance().CreateSandbox(*rom, "cli");
      if (!sandbox_or.ok()) {
        return sandbox_or.status();
      }
      sandbox_rom.emplace();
      auto load_status =
          sandbox_rom->LoadFromFile(sandbox_or->rom_path.string());
      if (!load_status.ok()) {
        return load_status;
      }
      rom = &*sandbox_rom;
      SetRomContext(rom);
    }

    // 7. Ensure labels are loaded if required
    if (RequiresLabels()) {
      RETURN_IF_ERROR(context.EnsureLabelsLoaded(rom));
    }
  }

  // 8. Begin output formatting
  formatter.BeginObject(GetOutputTitle());

  // 9. Execute command business logic
  auto execute_status = Execute(rom, parser, formatter);
  if (!execute_status.ok()) {
    return execute_status;
  }

  if (sandbox_enabled && rom != nullptr && rom->dirty()) {
    auto save_status = rom->SaveToFile({.save_new = false});
    if (!save_status.ok()) {
      return save_status;
    }
  }

  // 10. Finalize and print output
  formatter.EndObject();

  if (captured_output) {
    *captured_output = formatter.GetOutput();
  } else {
    formatter.Print();
  }

  return absl::OkStatus();
}

CommandHandler::Descriptor CommandHandler::Describe() const {
  Descriptor descriptor;
  descriptor.display_name = GetName();  // Use GetName() for display.
  descriptor.summary = "Command summary not provided.";
  descriptor.todo_reference = "todo#unassigned";
  return descriptor;
}

}  // namespace resources
}  // namespace cli
}  // namespace yaze
