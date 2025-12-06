#include "cli/service/agent/tools/project_graph_tool.h"

#include <filesystem>

#include "absl/strings/str_cat.h"
#include "cli/service/resources/command_context.h"
#include "absl/strings/str_join.h"
#include "util/log.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

namespace fs = std::filesystem;

absl::Status ProjectGraphTool::ValidateArgs(const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"query"});
}

absl::Status ProjectGraphTool::Execute(Rom* rom, const resources::ArgumentParser& parser,
                                       resources::OutputFormatter& formatter) {
  if (!project_) {
    return absl::FailedPreconditionError("Project context not available.");
  }

  std::string query_type = parser.GetString("query").value();

  if (query_type == "info") {
    return GetProjectInfo(formatter);
  } else if (query_type == "files") {
    std::string path = parser.GetString("path").value_or(project_->code_folder);
    return GetFileStructure(path, formatter);
  } else if (query_type == "symbols") {
    if (!asar_wrapper_) {
      return absl::FailedPreconditionError("Asar wrapper not available for symbols query.");
    }
    return GetSymbolTable(formatter);
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown query type: ", query_type));
  }
}

absl::Status ProjectGraphTool::GetProjectInfo(resources::OutputFormatter& formatter) const {
  formatter.AddField("name", project_->name);
  formatter.AddField("description", project_->metadata.description);
  formatter.AddField("filepath", project_->filepath);
  formatter.AddField("rom_filename", project_->rom_filename);
  formatter.AddField("code_folder", project_->code_folder);
  formatter.AddField("symbols_filename", project_->symbols_filename);
  formatter.AddField("build_script", project_->build_script);
  formatter.AddField("git_repository", project_->git_repository);
  formatter.AddField("last_build_hash", project_->last_build_hash);
  return absl::OkStatus();
}

absl::Status ProjectGraphTool::GetFileStructure(const std::string& path,
                                               resources::OutputFormatter& formatter) const {
  fs::path abs_path = project_->GetAbsolutePath(path);
  if (!fs::exists(abs_path)) {
    return absl::NotFoundError(absl::StrCat("Path not found: ", path));
  }

  formatter.BeginArray("files");
  for (const auto& entry : fs::directory_iterator(abs_path)) {
    formatter.BeginObject();
    formatter.AddField("name", entry.path().filename().string());
    formatter.AddField("type", entry.is_directory() ? "directory" : "file");
    formatter.AddField("path", project_->GetRelativePath(entry.path().string()));
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

absl::Status ProjectGraphTool::GetSymbolTable(resources::OutputFormatter& formatter) const {
  const auto& symbols = asar_wrapper_->GetSymbolTable();
  if (symbols.empty()) {
    return absl::NotFound("No symbols loaded. Load symbols via the Assemble menu or ensure the build script generates them.");
  }

  formatter.BeginArray("symbols");
  for (const auto& [name, symbol] : symbols) {
    formatter.BeginObject();
    formatter.AddField("name", symbol.name);
    formatter.AddField("address", absl::StrFormat("$%06X", symbol.address));
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
