#include "cli/handlers/game/dungeon_wall_profile_commands.h"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_wall_profile.h"

namespace yaze::cli::handlers {
namespace {

absl::StatusOr<zelda3::WallProfileContext> ParseContext(
    const resources::ArgumentParser& parser) {
  zelda3::WallProfileContext context;
  ASSIGN_OR_RETURN(context.room, parser.GetInt("room"));
  if (parser.GetString("world")) {
    int world;
    ASSIGN_OR_RETURN(world, parser.GetInt("world"));
    context.world = world;
  }
  if (parser.GetString("entrance")) {
    int entrance;
    ASSIGN_OR_RETURN(entrance, parser.GetInt("entrance"));
    context.entrance = entrance;
  }
  if (context.room < 0 || context.room > 0x127 ||
      (context.world && (*context.world < 0 || *context.world > 1)) ||
      (context.entrance &&
       (*context.entrance < 0 || *context.entrance > 0x84))) {
    return absl::InvalidArgumentError("Wall profile context is out of range");
  }
  return context;
}

absl::Status RequireMissing(const std::filesystem::path& path) {
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    return absl::FailedPreconditionError("Cannot inspect preview output: " +
                                         path.string());
  }
  if (std::filesystem::exists(status) || std::filesystem::is_symlink(status)) {
    return absl::AlreadyExistsError(
        "Preview output must be a fresh artifact: " + path.string());
  }
  return absl::OkStatus();
}

absl::Status ProtectOutput(
    const std::filesystem::path& output,
    const resources::CommandInvocationContext& invocation,
    const std::vector<std::filesystem::path>& inputs) {
  RETURN_IF_ERROR(
      resources::RejectArtifactRomAliases("--output", output, invocation));
  for (const auto& input : inputs) {
    bool alias;
    ASSIGN_OR_RETURN(alias, resources::PathsAlias(output, input));
    if (alias) {
      return absl::InvalidArgumentError(
          "Preview output aliases a protected input: " + input.string());
    }
  }
  return absl::OkStatus();
}

std::string PreviewAsm(const zelda3::DungeonTilePatterns& patterns) {
  std::string result =
      "; Generated wall-profile PREVIEW overlay; not a room-scoped ROM patch.\n"
      "lorom\n";
  std::optional<uint32_t> previous;
  for (const auto& [pc, word] : patterns.words) {
    if (!previous || pc != *previous + 2) {
      result += absl::StrFormat("org $%06X\n", pc + 0x8000);
    }
    result += absl::StrFormat("  dw $%04X\n", word);
    previous = pc;
  }
  return result;
}

}  // namespace

resources::CommandHandler::Descriptor
DungeonWallProfileCommandHandler::Describe() const {
  Descriptor descriptor;
  descriptor.display_name = "Dungeon Wall Profile";
  descriptor.summary =
      "Validate and resolve a strict room-selected wall profile; optionally "
      "emit a fresh read-only preview ASM, without changing ROM or project.";
  descriptor.entries = {
      {"--source", "Version-1 wall profile JSON collection", ""},
      {"--room", "Room ID 0..0x127 (decimal or 0x-prefixed)", ""},
      {"--world", "Optional explicit world 0 or 1", ""},
      {"--entrance", "Optional explicit entrance 0..0x84", ""},
      {"--legacy-source", "Literal legacy ASM; defaults to project descriptor",
       ""},
      {"--output", "Optional fresh preview ASM artifact; never overwritten",
       ""},
  };
  return descriptor;
}

absl::Status DungeonWallProfileCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"source", "room"}));
  for (const auto* field :
       {"source", "room", "world", "entrance", "legacy-source", "output"}) {
    if (const auto value = parser.GetString(field); value && value->empty()) {
      return absl::InvalidArgumentError(std::string("--") + field +
                                        " cannot be empty");
    }
  }
  return ParseContext(parser).status();
}

absl::Status DungeonWallProfileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  resources::CommandInvocationContext context;
  if (rom != nullptr && !rom->filename().empty()) {
    context.source_rom_path = rom->filename();
    context.active_rom_path = rom->filename();
  }
  return ExecuteWithContext(rom, parser, formatter, context);
}

absl::Status DungeonWallProfileCommandHandler::ExecuteWithContext(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter,
    const resources::CommandInvocationContext& invocation) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  zelda3::WallProfileContext context;
  ASSIGN_OR_RETURN(context, ParseContext(parser));
  std::shared_ptr<const zelda3::DungeonWallProfileCollection> collection;
  ASSIGN_OR_RETURN(
      collection, zelda3::LoadDungeonWallProfiles(*parser.GetString("source")));
  zelda3::DungeonTilePatterns legacy;
  std::optional<std::string> legacy_path = parser.GetString("legacy-source");
  if (!legacy_path && project_ != nullptr &&
      !project_->dungeon_tile_patterns.empty()) {
    legacy_path = project_->GetAbsolutePath(project_->dungeon_tile_patterns);
  }
  if (legacy_path) {
    ASSIGN_OR_RETURN(legacy, zelda3::LoadDungeonTilePatterns(*legacy_path));
  }
  ASSIGN_OR_RETURN(zelda3::ResolvedWallProfile resolved,
                   zelda3::ResolveWallProfile(*collection, context, legacy));

  std::set<uint32_t> profile_addresses;
  if (resolved.profile_id()) {
    for (const auto& profile : collection->profiles()) {
      if (profile.id != *resolved.profile_id())
        continue;
      for (const auto& pattern : profile.patterns) {
        for (size_t i = 0; i < pattern.words.size(); ++i) {
          profile_addresses.insert(pattern.pc_offset + i * 2);
        }
      }
    }
  }
  const auto output = parser.GetString("output");
  if (output) {
    if (resolved.patterns().words.empty()) {
      return absl::FailedPreconditionError(
          "No resolved words to emit; retain the existing ROM/project "
          "fallback");
    }
    std::filesystem::path destination;
    ASSIGN_OR_RETURN(destination,
                     resources::ResolveStableArtifactPath(*output));
    std::vector<std::filesystem::path> inputs{collection->source_path()};
    if (legacy_path)
      inputs.push_back(legacy.source_path);
    if (project_ != nullptr && !project_->filepath.empty()) {
      inputs.push_back(project_->filepath);
    }
    RETURN_IF_ERROR(ProtectOutput(destination, invocation, inputs));
    RETURN_IF_ERROR(RequireMissing(*output));
    RETURN_IF_ERROR(RequireMissing(destination));
    const std::string preview = PreviewAsm(resolved.patterns());
    auto parsed = zelda3::ParseDungeonTilePatterns(preview);
    if (!parsed.ok())
      return parsed.status();
    if (parsed->words != resolved.patterns().words) {
      return absl::DataLossError("Preview ASM changed resolved tile words");
    }
    const core::SourceArtifactPublisherLabels labels{
        .subject = "wall profile preview", .published_file = "preview ASM"};
    std::unique_ptr<core::SourceArtifactPublicationLock> lock;
    ASSIGN_OR_RETURN(lock, core::AcquireSourceArtifactPublicationLock(
                               {destination}, labels, inputs));
    RETURN_IF_ERROR(ProtectOutput(destination, invocation, inputs));
    RETURN_IF_ERROR(RequireMissing(destination));
    std::vector<core::SourceArtifactUpdate> updates{
        {collection->source_path(), collection->source_text(),
         collection->source_text()}};
    if (legacy_path) {
      updates.push_back(
          {legacy.source_path, legacy.source_text, legacy.source_text});
    }
    updates.push_back({destination, std::nullopt, preview});
    RETURN_IF_ERROR(core::PublishSourceArtifacts(
        *lock, std::move(updates),
        core::ComputeSourceArtifactSha256(collection->source_text()), [&]() {
          auto readback = zelda3::LoadDungeonTilePatterns(destination.string());
          if (!readback.ok())
            return readback.status();
          if (readback->words != resolved.patterns().words) {
            return absl::DataLossError("Preview ASM readback differs");
          }
          return absl::OkStatus();
        }));
    formatter.AddField("output", destination.string());
  }
  formatter.AddField("source", collection->source_path());
  formatter.AddField("source_sha256", core::ComputeSourceArtifactSha256(
                                          collection->source_text()));
  formatter.AddField("selected", resolved.profile_id().has_value());
  formatter.AddField("selected_profile", resolved.profile_id().value_or(""));
  formatter.AddField("room", context.room);
  formatter.AddField("resolved_word_count",
                     static_cast<uint64_t>(resolved.patterns().words.size()));
  formatter.AddField("profile_word_count",
                     static_cast<uint64_t>(profile_addresses.size()));
  formatter.AddField("legacy_word_count",
                     static_cast<uint64_t>(resolved.patterns().words.size() -
                                           profile_addresses.size()));
  formatter.AddField("preview_written", output.has_value());
  formatter.AddField("rom_written", false);
  formatter.AddField("scope", "preview_overlay_only_not_native_dispatch");
  formatter.BeginArray("words");
  for (const auto& [pc, word] : resolved.patterns().words) {
    formatter.BeginObject();
    formatter.AddField("pc_offset", static_cast<uint64_t>(pc));
    formatter.AddField("word", static_cast<int>(word));
    formatter.AddField("origin",
                       profile_addresses.contains(pc) ? "profile" : "legacy");
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
