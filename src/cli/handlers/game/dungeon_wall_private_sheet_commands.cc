#include "cli/handlers/game/dungeon_wall_private_sheet_commands.h"

#include <fstream>
#include <iterator>
#include <string>

#include "absl/status/status.h"
#include "cli/handlers/game/dungeon_render_object_plan.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_wall_private_sheet.h"

namespace yaze::cli::handlers {
namespace {

using Json = nlohmann::json;

absl::StatusOr<zelda3::WallPrivateSheetContext> ParseContext(
    const resources::ArgumentParser& parser) {
  zelda3::WallPrivateSheetContext context;
  ASSIGN_OR_RETURN(context.room, parser.GetInt("room"));
  ASSIGN_OR_RETURN(context.profile, parser.GetInt("profile"));
  ASSIGN_OR_RETURN(context.blockset, parser.GetInt("blockset"));
  context.indoor = !parser.HasFlag("outdoor");
  if (parser.GetString("dungeon")) {
    ASSIGN_OR_RETURN(context.dungeon, parser.GetInt("dungeon"));
  } else {
    context.dungeon = zelda3::kWallPrivateArmoryDungeon;
  }
  return context;
}

}  // namespace

resources::CommandHandler::Descriptor
DungeonWallPrivateSheetCommandHandler::Describe() const {
  Descriptor descriptor;
  descriptor.display_name = "Dungeon Wall Private Sheet";
  descriptor.summary =
      "Resolve room54 WSP1 private-stream sheet overrides from kit+3bpp with "
      "positive/negative context controls and optional object-plan recipe "
      "dry-run; never writes ROM or global sheet tables.";
  descriptor.entries = {
      {"--kit", "prison_armory.kit.json with private_pointer sheet", ""},
      {"--sheet", "Exact 0x600 decoded 3bpp private stream", ""},
      {"--room", "Room ID (positive control uses 0x54)", ""},
      {"--profile", "WSR1 profile index (positive control uses 4)", ""},
      {"--blockset", "Effective secondary blockset (positive uses 0x19)", ""},
      {"--dungeon", "Optional dungeon id (default 10)", ""},
      {"--outdoor", "Negative control: force outdoor refusal", ""},
      {"--binding-contract", "Optional BINDING_CONTRACT*.json for object pins",
       ""},
      {"--object-plan", "Optional OBJECT_PLAN_54.json recipe dry-run", ""},
  };
  return descriptor;
}

absl::Status DungeonWallPrivateSheetCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(
      parser.RequireArgs({"kit", "sheet", "room", "profile", "blockset"}));
  for (const char* field : {"kit", "sheet", "room", "profile", "blockset",
                            "dungeon", "binding-contract", "object-plan"}) {
    if (const auto value = parser.GetString(field); value && value->empty()) {
      return absl::InvalidArgumentError(std::string("--") + field +
                                        " cannot be empty");
    }
  }
  return ParseContext(parser).status();
}

absl::Status DungeonWallPrivateSheetCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  RETURN_IF_ERROR(ValidateArgs(parser));
  zelda3::WallPrivateSheetContext context;
  ASSIGN_OR_RETURN(context, ParseContext(parser));

  zelda3::WallPrivateSheetBinding binding;
  ASSIGN_OR_RETURN(
      binding, zelda3::LoadWallPrivateSheetBinding(*parser.GetString("kit"),
                                                   *parser.GetString("sheet")));

  if (const auto contract_path = parser.GetString("binding-contract")) {
    std::ifstream input(*contract_path, std::ios::binary);
    if (!input) {
      return absl::NotFoundError("Cannot open --binding-contract");
    }
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    Json contract = Json::parse(text, nullptr, false);
    if (contract.is_discarded()) {
      return absl::InvalidArgumentError("--binding-contract is not valid JSON");
    }
    RETURN_IF_ERROR(
        zelda3::AttachWallPrivateSheetObjectBindings(&binding, contract));
    formatter.AddField("binding_contract", *contract_path);
    formatter.AddField("binding_contract_sha256",
                       core::ComputeSourceArtifactSha256(text));
  }

  ASSIGN_OR_RETURN(const auto resolved,
                   zelda3::ResolveWallPrivateSheet(binding, context));

  formatter.AddField("kit", binding.kit_path);
  formatter.AddField("sheet", binding.sheet_path);
  formatter.AddField("kit_id", binding.kit_id);
  formatter.AddField("profile_id", binding.profile_id);
  formatter.AddField("decoded_sha256", binding.decoded_sha256);
  formatter.AddField("room", context.room);
  formatter.AddField("profile", context.profile);
  formatter.AddField("blockset", context.blockset);
  formatter.AddField("dungeon", context.dungeon);
  formatter.AddField("indoor", context.indoor);
  formatter.AddField("applied", resolved.applied);
  formatter.AddField("reason", resolved.reason);
  formatter.AddField("bg_sheet", static_cast<int>(resolved.bg_sheet));
  formatter.AddField("sprite_sheet", static_cast<int>(resolved.sprite_sheet));
  formatter.AddField(
      "override_count",
      static_cast<uint64_t>(resolved.sheet_overrides_8bpp.size()));
  formatter.AddField("rom_written", false);
  formatter.AddField("global_sheet_mutated", false);
  formatter.AddField("scope", "source_private_stream_not_global_08_or_36");

  if (const auto plan_path = parser.GetString("object-plan")) {
    if (binding.objects.empty()) {
      return absl::FailedPreconditionError(
          "--object-plan requires --binding-contract object pins");
    }
    ASSIGN_OR_RETURN(const Json plan, LoadDungeonObjectPlanFile(*plan_path));
    ASSIGN_OR_RETURN(const auto recipe,
                     zelda3::DryRunWallPrivateSheetRecipe(binding, plan));
    formatter.AddField("recipe_path", *plan_path);
    formatter.AddField("recipe_accepted", recipe.accepted);
    formatter.AddField("recipe_reason", recipe.reason);
    formatter.AddField("recipe_plan_sha256", recipe.plan_sha256);
    formatter.AddField("recipe_edit_count", recipe.edit_count);
    formatter.BeginArray("recipe_matched_assets");
    for (const auto& asset : recipe.matched_assets) {
      formatter.BeginObject();
      formatter.AddField("file", asset);
      formatter.EndObject();
    }
    formatter.EndArray();
    if (!recipe.accepted) {
      return absl::FailedPreconditionError(recipe.reason);
    }
  }

  // Positive resolution returns Ok with applied=true; negative controls also
  // return Ok so agents can assert refusal without treating it as a CLI crash.
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
