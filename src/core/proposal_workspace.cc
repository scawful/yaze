#include "core/proposal_workspace.h"

#include <chrono>
#include <fstream>
#include <set>
#include <sstream>

#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze::core {
namespace fs = std::filesystem;
#if defined(YAZE_WITH_JSON)
namespace {

nlohmann::json ReadJson(const fs::path& path) {
  std::ifstream stream(path);
  if (!stream)
    throw std::runtime_error("Cannot read " + path.string());
  return nlohmann::json::parse(stream);
}

absl::StatusOr<fs::path> ContainedPath(const fs::path& root,
                                       const std::string& value) {
  const fs::path relative(value);
  if (relative.empty() || relative.is_absolute()) {
    return absl::InvalidArgumentError("Proposal path must be relative");
  }
  auto current = root;
  for (const auto& part : relative) {
    if (part == "..")
      return absl::InvalidArgumentError("Proposal path escapes its packet");
    current /= part;
    if (fs::is_symlink(current))
      return absl::InvalidArgumentError("Proposal path contains a symlink");
  }
  return current;
}

}  // namespace
#endif

absl::StatusOr<ProposalWorkspace> LoadProposalWorkspace(
    const fs::path& packet_root) {
#if defined(YAZE_WITH_JSON)
  try {
    ProposalWorkspace result;
    result.root = fs::canonical(packet_root);
    const auto packet = ReadJson(result.root / "proposal.json");
    if (packet.at("schema") != "yaze.proposal-workbench.v1")
      return absl::InvalidArgumentError("Unsupported proposal schema");
    result.proposal_id = packet.at("proposal_id").get<std::string>();
    result.domain = packet.at("domain").get<std::string>();
    if (!packet.at("target").is_number_integer())
      return absl::InvalidArgumentError("Proposal target must be an integer");
    result.target = packet.at("target").get<int>();
    const int limit = result.domain == "dungeon" ? 296 : 192;
    if ((result.domain != "dungeon" && result.domain != "overworld") ||
        result.target < 0 || result.target >= limit) {
      return absl::InvalidArgumentError("Invalid proposal domain or target");
    }
    if (result.domain == "dungeon") {
      if (!packet.at("graphics_entrance_slot").is_number_integer())
        return absl::InvalidArgumentError(
            "Graphics entrance must be an integer");
      result.graphics_entrance_slot =
          packet.at("graphics_entrance_slot").get<int>();
      if (result.graphics_entrance_slot < 0 ||
          result.graphics_entrance_slot >= zelda3::kNumDungeonEntranceSlots) {
        return absl::InvalidArgumentError("Invalid proposal graphics entrance");
      }
    }
    ASSIGN_OR_RETURN(
        auto draft,
        ContainedPath(result.root,
                      packet.at("draft_project").get<std::string>()));
    result.project_file = draft / "project.yaze";
    ASSIGN_OR_RETURN(result.rom_file,
                     ContainedPath(draft, packet.at("rom").get<std::string>()));
    if (fs::is_symlink(result.project_file) ||
        fs::is_symlink(result.root / "baseline.rom") ||
        fs::is_symlink(result.root / "baseline.json") ||
        !fs::is_regular_file(result.project_file) ||
        !fs::is_regular_file(result.rom_file)) {
      return absl::InvalidArgumentError(
          "Proposal draft project or ROM is missing");
    }
    ASSIGN_OR_RETURN(auto baseline_hash, ComputeSourceArtifactFileSha256(
                                             result.root / "baseline.rom"));
    result.baseline_rom_sha256 =
        packet.at("baseline_rom_sha256").get<std::string>();
    ASSIGN_OR_RETURN(auto inventory_hash, ComputeSourceArtifactFileSha256(
                                              result.root / "baseline.json"));
    if (baseline_hash != result.baseline_rom_sha256 ||
        inventory_hash != packet.at("baseline_inventory_sha256")) {
      return absl::FailedPreconditionError("Proposal baseline changed");
    }
    ASSIGN_OR_RETURN(
        auto source_inventory_hash,
        ComputeSourceArtifactFileSha256(result.root / "baseline-files.json"));
    if (source_inventory_hash != packet.at("baseline_files_inventory_sha256"))
      return absl::FailedPreconditionError(
          "Proposal source baseline inventory changed");
    const auto sources = ReadJson(result.root / "baseline-files.json");
    for (auto it = sources.begin(); it != sources.end(); ++it) {
      ASSIGN_OR_RETURN(auto path,
                       ContainedPath(result.root / "baseline-files", it.key()));
      ASSIGN_OR_RETURN(auto hash, ComputeSourceArtifactFileSha256(path));
      if (hash != it.value())
        return absl::FailedPreconditionError(
            "Proposal source baseline changed");
    }
    result.unbound_references = packet.at("references").size();
    return result;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(error.what());
  }
#else
  return absl::UnimplementedError("Proposal workspaces require JSON support");
#endif
}

absl::StatusOr<fs::path> ExportProposalFeedback(
    const ProposalWorkspace& workspace, const std::string& note) {
#if defined(YAZE_WITH_JSON)
  ASSIGN_OR_RETURN(auto fresh, LoadProposalWorkspace(workspace.root));
  if (fresh.project_file != workspace.project_file ||
      fresh.rom_file != workspace.rom_file ||
      fresh.proposal_id != workspace.proposal_id ||
      fresh.domain != workspace.domain || fresh.target != workspace.target ||
      fresh.graphics_entrance_slot != workspace.graphics_entrance_slot ||
      fresh.baseline_rom_sha256 != workspace.baseline_rom_sha256) {
    return absl::FailedPreconditionError(
        "Proposal identity changed; reopen it");
  }
  try {
    const auto baseline = ReadJson(fresh.root / "baseline.json");
    nlohmann::json current = nlohmann::json::object();
    const auto draft = fresh.project_file.parent_path();
    for (const auto& entry : fs::recursive_directory_iterator(draft)) {
      if (entry.is_symlink())
        return absl::FailedPreconditionError("Draft contains a symlink");
      if (!entry.is_regular_file())
        continue;
      ASSIGN_OR_RETURN(auto hash,
                       ComputeSourceArtifactFileSha256(entry.path()));
      current[fs::relative(entry.path(), draft).generic_string()] = hash;
    }
    std::set<std::string> names;
    for (auto it = baseline.begin(); it != baseline.end(); ++it)
      names.insert(it.key());
    for (auto it = current.begin(); it != current.end(); ++it)
      names.insert(it.key());
    auto changed = nlohmann::json::array();
    for (const auto& name : names) {
      const auto before = baseline.value(name, nlohmann::json(nullptr));
      const auto after = current.value(name, nlohmann::json(nullptr));
      if (before != after)
        changed.push_back({{"path", name},
                           {"before_sha256", before},
                           {"after_sha256", after}});
    }
    ASSIGN_OR_RETURN(auto edited_hash,
                     ComputeSourceArtifactFileSha256(fresh.rom_file));
    nlohmann::json feedback = {
        {"schema", "yaze.proposal-feedback.v1"},
        {"proposal_id", fresh.proposal_id},
        {"domain", fresh.domain},
        {"target", fresh.target},
        {"graphics_entrance_slot", fresh.graphics_entrance_slot},
        {"baseline_rom_sha256", fresh.baseline_rom_sha256},
        {"edited_rom_sha256", edited_hash},
        {"changed_files", changed},
        {"note", note},
        {"unbound_reference_count", fresh.unbound_references},
        {"semantic_diff",
         "pending domain exporter; file changes do not prove room/map scope"},
        {"adoption", "never automatic"}};
    const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const auto path =
        fresh.root / ("feedback-" + std::to_string(stamp) + ".json");
    RETURN_IF_ERROR(project::WriteProjectFileAtomically(
        path.string(), feedback.dump(2) + "\n", false));
    return path;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(error.what());
  }
#else
  return absl::UnimplementedError("Proposal workspaces require JSON support");
#endif
}
}  // namespace yaze::core
