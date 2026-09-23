#include "cli/service/resources/command_handler.h"

#include <iostream>
#include <optional>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "util/macro.h"

ABSL_DECLARE_FLAG(bool, sandbox);

namespace yaze {
namespace cli {
namespace resources {
namespace {

absl::StatusOr<std::filesystem::path> CaptureRomPathIdentity(
    const std::filesystem::path& path) {
  std::error_code absolute_ec;
  const std::filesystem::path absolute =
      std::filesystem::absolute(path, absolute_ec);
  if (absolute_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot capture ROM path identity for %s: %s",
                        path.string(), absolute_ec.message()));
  }

  // Resolve symlinks before collapsing '..': link/../rom.sfc can identify a
  // different file than the lexically normalized spelling.
  std::error_code canonical_ec;
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(absolute, canonical_ec);
  if (canonical_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot capture ROM path identity for %s: %s",
                        absolute.string(), canonical_ec.message()));
  }
  return canonical.lexically_normal();
}

}  // namespace

absl::StatusOr<std::filesystem::path> ResolveStableArtifactPath(
    const std::filesystem::path& path) {
  std::error_code absolute_ec;
  const std::filesystem::path absolute =
      std::filesystem::absolute(path, absolute_ec);
  if (absolute_ec) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Cannot normalize path %s: %s", path.string(), absolute_ec.message()));
  }
  // Resolve every existing component once. Publication then uses this stable
  // path instead of following a caller-supplied parent symlink a second time.
  // Do not collapse '..' before symlink resolution changes its parent.
  std::error_code canonical_ec;
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(absolute, canonical_ec);
  if (canonical_ec) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Cannot safely resolve path %s: %s", absolute.string(),
                        canonical_ec.message()));
  }
  const std::filesystem::path resolved = canonical.lexically_normal();
  const std::filesystem::path parent = resolved.parent_path();
  std::error_code parent_ec;
  const auto parent_status = std::filesystem::status(parent, parent_ec);
  if (parent_ec || !std::filesystem::exists(parent_status) ||
      !std::filesystem::is_directory(parent_status)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Artifact parent directory must already exist: %s%s", parent.string(),
        !parent_ec ? "" : absl::StrFormat(" (%s)", parent_ec.message())));
  }
  return resolved;
}

absl::StatusOr<bool> PathsAlias(const std::filesystem::path& lhs,
                                const std::filesystem::path& rhs) {
  ASSIGN_OR_RETURN(const auto normalized_lhs, ResolveStableArtifactPath(lhs));
  ASSIGN_OR_RETURN(const auto normalized_rhs, ResolveStableArtifactPath(rhs));
  if (normalized_lhs == normalized_rhs) {
    return true;
  }

  std::error_code equivalent_ec;
  const bool equivalent = std::filesystem::equivalent(
      normalized_lhs, normalized_rhs, equivalent_ec);
  if (!equivalent_ec) {
    return equivalent;
  }

  // equivalent() reports an error when either path does not exist. Lexical
  // normalization above is sufficient in that case. If both paths do exist,
  // fail closed rather than risk truncating a ROM we could not compare.
  std::error_code lhs_exists_ec;
  std::error_code rhs_exists_ec;
  const bool lhs_exists =
      std::filesystem::exists(normalized_lhs, lhs_exists_ec);
  const bool rhs_exists =
      std::filesystem::exists(normalized_rhs, rhs_exists_ec);
  if (lhs_exists_ec || rhs_exists_ec || (lhs_exists && rhs_exists)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Could not safely compare paths %s and %s: %s", normalized_lhs.string(),
        normalized_rhs.string(), equivalent_ec.message()));
  }
  return false;
}

absl::Status RejectArtifactRomAliases(
    absl::string_view option_name, const std::filesystem::path& artifact_path,
    const CommandInvocationContext& invocation_context) {
  if (invocation_context.active_rom_path.has_value()) {
    ASSIGN_OR_RETURN(
        const bool aliases_active_rom,
        PathsAlias(artifact_path, *invocation_context.active_rom_path));
    if (aliases_active_rom) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s path aliases the active ROM; choose a separate artifact file: "
          "%s",
          option_name, artifact_path.string()));
    }
  }

  if (invocation_context.source_rom_path.has_value()) {
    ASSIGN_OR_RETURN(
        const bool aliases_source_rom,
        PathsAlias(artifact_path, *invocation_context.source_rom_path));
    if (aliases_source_rom) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s path aliases the %s; choose a separate artifact file: %s",
          option_name,
          invocation_context.sandbox_enabled ? "sandbox source ROM"
                                             : "source ROM",
          artifact_path.string()));
    }
  }

  return absl::OkStatus();
}

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

  // Check for --symbols override
  if (auto symbols_path = parser.GetString("symbols");
      symbols_path.has_value()) {
    config.symbols_path = *symbols_path;
  }

  // Optional project runtime context used to mirror editor feature/custom object
  // behavior during CLI execution.
  if (auto project_path = parser.GetString("project-context");
      project_path.has_value()) {
    config.project_context_path = *project_path;
  }

  // Check for --mock-rom flag
  config.use_mock_rom = parser.HasFlag("mock-rom");

  CommandContext context(config);

  // 6. Get ROM (loads if needed) - only if command requires it
  Rom* rom = nullptr;
  std::optional<Rom> sandbox_rom;
  bool sandbox_enabled = false;
  CommandInvocationContext invocation_context;

  // Set symbol provider regardless of ROM loading (it might load its own symbols)
  SetSymbolProvider(context.GetSymbolProvider());

  if (RequiresRom()) {
    ASSIGN_OR_RETURN(rom, context.GetRom());
    if (!rom->filename().empty()) {
      ASSIGN_OR_RETURN(
          const auto rom_path_identity,
          CaptureRomPathIdentity(std::filesystem::path(rom->filename())));
      invocation_context.source_rom_path = rom_path_identity;
      invocation_context.active_rom_path = rom_path_identity;
    }
    SetRomContext(rom);
    SetProjectContext(context.GetProjectContext());

    if (absl::GetFlag(FLAGS_sandbox) || parser.HasFlag("sandbox")) {
      sandbox_enabled = true;
      auto sandbox_or =
          RomSandboxManager::Instance().CreateSandbox(*rom, "cli");
      if (!sandbox_or.ok()) {
        return sandbox_or.status();
      }
      invocation_context.sandbox_enabled = true;
      if (!invocation_context.source_rom_path.has_value()) {
        ASSIGN_OR_RETURN(invocation_context.source_rom_path,
                         CaptureRomPathIdentity(
                             std::filesystem::path(sandbox_or->source_rom)));
      }
      ASSIGN_OR_RETURN(invocation_context.active_rom_path,
                       CaptureRomPathIdentity(sandbox_or->rom_path));
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
  auto execute_status =
      ExecuteWithContext(rom, parser, formatter, invocation_context);
  if (!execute_status.ok()) {
    // Preserve structured output for failing commands so callers can inspect
    // machine-readable diagnostics even when status is non-OK.
    formatter.EndObject();
    if (captured_output) {
      *captured_output = formatter.GetOutput();
    } else {
      formatter.Print();
    }
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
