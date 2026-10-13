#include "cli/handlers/tools/overworld_allocation_commands.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/emu/debug/symbol_provider.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/resource_labels.h"

namespace yaze::cli {

namespace {

// YazeProject::Open installs its manifest in the global label provider;
// restore the previous binding when the command finishes.
class ScopedLabelManifestRestore {
 public:
  explicit ScopedLabelManifestRestore(zelda3::ResourceLabelProvider& provider)
      : provider_(provider), previous_(provider.hack_manifest()) {}
  ~ScopedLabelManifestRestore() { provider_.SetHackManifest(previous_); }
  ScopedLabelManifestRestore(const ScopedLabelManifestRestore&) = delete;
  ScopedLabelManifestRestore& operator=(const ScopedLabelManifestRestore&) =
      delete;

 private:
  zelda3::ResourceLabelProvider& provider_;
  const core::HackManifest* previous_ = nullptr;
};

std::map<int, std::string> AreaNamesFromManifest(
    const core::HackManifest& manifest) {
  std::map<int, std::string> names;
  const auto& registry = manifest.project_registry();
  if (auto it = registry.all_resource_labels.find("overworld_map");
      it != registry.all_resource_labels.end()) {
    for (const auto& [key, label] : it->second) {
      int id = 0;
      if (absl::SimpleAtoi(key, &id) && id >= 0 &&
          id < zelda3::kAllocationAreaCount && !label.empty()) {
        names[id] = label;
      }
    }
  }
  for (const auto& area : registry.overworld_areas) {
    if (area.area_id >= 0 && area.area_id < zelda3::kAllocationAreaCount &&
        !area.name.empty()) {
      names[area.area_id] = area.name;
    }
  }
  return names;
}

zelda3::TailCheckAnnotator MakeAnnotator(
    const core::HackManifest* manifest,
    const emu::debug::SymbolProvider* symbols) {
  zelda3::TailCheckAnnotator annotator;
  if (manifest != nullptr && manifest->loaded()) {
    annotator.ownership =
        [manifest](
            uint32_t snes) -> std::optional<std::pair<std::string, bool>> {
      const auto ownership = manifest->ClassifyAddress(snes);
      if (ownership == core::AddressOwnership::kVanillaSafe) {
        return std::nullopt;
      }
      std::string note = core::AddressOwnershipToString(ownership);
      if (ownership == core::AddressOwnership::kHookPatched) {
        const uint32_t address = snes & 0x7FFFFF;
        for (const auto& hook : manifest->protected_regions()) {
          if (address >= hook.start && address < hook.end &&
              !hook.module.empty()) {
            note = absl::StrFormat("%s (%s)", note, hook.module);
            break;
          }
        }
      } else {
        const auto& banks = manifest->owned_banks();
        if (auto it = banks.find(static_cast<uint8_t>((snes >> 16) & 0x7F));
            it != banks.end() && !it->second.ownership_note.empty()) {
          note = absl::StrFormat("%s: %s", note, it->second.ownership_note);
        }
      }
      const bool blocking =
          ownership == core::AddressOwnership::kAsmOwned ||
          ownership == core::AddressOwnership::kAsmExpansion ||
          ownership == core::AddressOwnership::kHookPatched;
      return std::make_pair(note, blocking);
    };
  }
  if (symbols != nullptr && symbols->HasSymbols()) {
    // Name the nearest labelled symbol at or before the address, skipping
    // asar's anonymous +/- labels (pos_N_M, neg_N_M).
    annotator.symbol = [symbols](uint32_t snes) -> std::string {
      auto anonymous = [](const std::string& name) {
        return name.empty() || name[0] == ':' || name.rfind("pos_", 0) == 0 ||
               name.rfind("neg_", 0) == 0;
      };
      const uint32_t start = snes >= 0x1000 ? snes - 0x1000 : 0;
      auto candidates = symbols->GetSymbolsInRange(start, snes);
      const emu::debug::Symbol* best = nullptr;
      for (const auto& symbol : candidates) {
        if (symbol.address <= snes && !anonymous(symbol.name) &&
            (best == nullptr || symbol.address >= best->address)) {
          best = &symbol;
        }
      }
      if (best == nullptr) {
        return std::string();
      }
      return best->address == snes
                 ? best->name
                 : absl::StrFormat("%s+$%X", best->name, snes - best->address);
    };
  }
  return annotator;
}

}  // namespace

absl::Status OverworldAllocationCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Declared before the label restore so the restore runs first on exit.
  std::unique_ptr<project::YazeProject> opened_project;
  std::optional<ScopedLabelManifestRestore> restore_labels;
  const core::HackManifest* manifest = nullptr;
  if (auto project_arg = parser.GetString("project"); project_arg.has_value()) {
    opened_project = std::make_unique<project::YazeProject>();
    restore_labels.emplace(zelda3::GetResourceLabels());
    absl::Status status;
    try {
      status = opened_project->Open(*project_arg);
    } catch (const std::exception& error) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Cannot load project '%s': %s", *project_arg, error.what()));
    }
    if (!status.ok()) {
      return status;
    }
    manifest = &opened_project->hack_manifest;
  } else if (project_ != nullptr) {
    manifest = &project_->hack_manifest;
  }

  zelda3::OverworldAllocationOptions options;
  if (manifest != nullptr && manifest->loaded()) {
    options.area_names = AreaNamesFromManifest(*manifest);
  }

  auto report_or = zelda3::BuildOverworldAllocationReport(rom, options);
  if (!report_or.ok()) {
    return report_or.status();
  }
  const zelda3::OverworldAllocationReport& report = *report_or;

  std::optional<zelda3::TailMapExpansionCheck> tail_check;
  if (parser.HasFlag("tail-check")) {
    tail_check = zelda3::CheckTailMapExpansion(
        *rom, zelda3::TailMapExpansionPlan{},
        MakeAnnotator(manifest, symbol_provider_));
  }
  const zelda3::TailMapExpansionCheck* tail =
      tail_check.has_value() ? &*tail_check : nullptr;

  if (formatter.IsJson()) {
    const auto json = nlohmann::ordered_json::parse(
        zelda3::OverworldAllocationToJson(report, tail));
    for (auto it = json.begin(); it != json.end(); ++it) {
      formatter.AddRawJsonField(it.key(), it.value().dump());
    }
  } else {
    formatter.AddRawText(zelda3::FormatOverworldAllocationText(report, tail));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli
