#include "cli/service/agent/tools/project_graph_tool.h"

#include <filesystem>
#include <fstream>
#include <map>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/service/resources/command_context.h"
#include "util/json.h"
#include "util/log.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

namespace fs = std::filesystem;

absl::StatusOr<uint32_t> ParseAddress(absl::string_view value) {
  std::string token(value);
  if (token.empty()) {
    return absl::InvalidArgumentError("Address is empty");
  }
  if (token[0] == '$') {
    token = token.substr(1);
  } else if (token.size() > 2 && token[0] == '0' &&
             (token[1] == 'x' || token[1] == 'X')) {
    token = token.substr(2);
  }
  try {
    return static_cast<uint32_t>(std::stoul(token, nullptr, 16));
  } catch (...) {
    return absl::InvalidArgumentError(absl::StrCat("Invalid address: ", value));
  }
}

bool LoadJsonFile(const std::string& path, Json* out) {
  if (path.empty()) {
    return false;
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }
  try {
    file >> *out;
    return true;
  } catch (...) {
    return false;
  }
}

uint32_t NormalizeLoRomMirror(uint32_t address) {
  if ((address & 0xFF0000u) >= 0x800000u) {
    address &= 0x7FFFFFu;
  }
  return address;
}

int GetLoRomBankIndex(uint32_t address) {
  return static_cast<int>((NormalizeLoRomMirror(address) >> 16) & 0xFFu);
}

absl::StatusOr<int> ParseBankIndex(absl::string_view value) {
  if (value.empty()) {
    return absl::InvalidArgumentError("Bank is empty");
  }
  try {
    return std::stoi(std::string(value), nullptr, 16);
  } catch (...) {
    return absl::InvalidArgumentError(absl::StrCat("Invalid bank: ", value));
  }
}

std::string GetProjectSymbolsPath(const project::YazeProject& project) {
  if (!project.z3dk_settings.artifact_paths.symbols_mlb.empty()) {
    return project.z3dk_settings.artifact_paths.symbols_mlb;
  }
  if (!project.z3dk_settings.symbols_path.empty()) {
    return project.z3dk_settings.symbols_path;
  }
  if (!project.symbols_filename.empty()) {
    return project.symbols_filename;
  }
  return project.GetZ3dkArtifactPath("symbols.mlb");
}

std::map<std::string, core::AsarSymbol> LoadSymbolsFromMlb(
    const std::string& path) {
  std::map<std::string, core::AsarSymbol> symbols;
  std::ifstream file(path);
  if (!file.is_open()) {
    return symbols;
  }

  std::string line;
  while (std::getline(file, line)) {
    const size_t first_colon = line.find(':');
    const size_t second_colon = line.find(':', first_colon + 1);
    if (first_colon == std::string::npos || second_colon == std::string::npos) {
      continue;
    }

    auto addr_or = ParseAddress(
        line.substr(first_colon + 1, second_colon - first_colon - 1));
    if (!addr_or.ok()) {
      continue;
    }

    core::AsarSymbol symbol;
    symbol.name = line.substr(second_colon + 1);
    symbol.address = *addr_or;
    symbols[symbol.name] = std::move(symbol);
  }
  return symbols;
}

const Json* FindBestSourceEntry(const Json& sourcemap, uint32_t address) {
  if (!sourcemap.contains("entries") || !sourcemap["entries"].is_array()) {
    return nullptr;
  }

  const Json* best = nullptr;
  uint32_t best_address = 0;
  for (const auto& entry : sourcemap["entries"]) {
    auto addr_or = ParseAddress(entry.value("address", "0x0"));
    if (!addr_or.ok() || *addr_or > address) {
      continue;
    }
    if (!best || *addr_or >= best_address) {
      best = &entry;
      best_address = *addr_or;
    }
  }
  return best;
}

std::string LookupSourceFile(const Json& sourcemap, int file_id) {
  if (!sourcemap.contains("files") || !sourcemap["files"].is_array()) {
    return {};
  }
  for (const auto& file : sourcemap["files"]) {
    if (file.value("id", -1) == file_id) {
      return file.value("path", "");
    }
  }
  return {};
}

bool LoadProjectSourceMap(const project::YazeProject& project, Json* out) {
  return LoadJsonFile(
      project.z3dk_settings.artifact_paths.sourcemap_json.empty()
          ? project.GetZ3dkArtifactPath("sourcemap.json")
          : project.z3dk_settings.artifact_paths.sourcemap_json,
      out);
}

bool LoadProjectHooks(const project::YazeProject& project, Json* out) {
  return LoadJsonFile(project.z3dk_settings.artifact_paths.hooks_json.empty()
                          ? project.GetZ3dkArtifactPath("hooks.json")
                          : project.z3dk_settings.artifact_paths.hooks_json,
                      out);
}

std::string GetDisasmBankFile(const project::YazeProject& project, int bank) {
  const auto dir =
      std::filesystem::path(project.GetZ3dkArtifactPath("z3disasm"));
  return (dir / absl::StrFormat("bank_%02X.asm", bank)).string();
}

absl::Status ProjectGraphTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"query"});
}

absl::Status ProjectGraphTool::Execute(Rom* rom,
                                       const resources::ArgumentParser& parser,
                                       resources::OutputFormatter& formatter) {
  (void)rom;
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
    return GetSymbolTable(formatter);
  } else if (query_type == "lookup") {
    return LookupAddressOrSymbol(parser, formatter);
  } else if (query_type == "writes") {
    return GetWriteCoverage(formatter);
  } else if (query_type == "bank") {
    return GetBankContext(parser, formatter);
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown query type: ", query_type));
  }
}

absl::Status ProjectGraphTool::GetProjectInfo(
    resources::OutputFormatter& formatter) const {
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

absl::Status ProjectGraphTool::GetFileStructure(
    const std::string& path, resources::OutputFormatter& formatter) const {
  fs::path abs_path = project_->GetAbsolutePath(path);
  if (!fs::exists(abs_path)) {
    return absl::NotFoundError(absl::StrCat("Path not found: ", path));
  }

  formatter.BeginArray("files");
  for (const auto& entry : fs::directory_iterator(abs_path)) {
    formatter.BeginObject();
    formatter.AddField("name", entry.path().filename().string());
    formatter.AddField("type", entry.is_directory() ? "directory" : "file");
    formatter.AddField("path",
                       project_->GetRelativePath(entry.path().string()));
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

absl::Status ProjectGraphTool::GetSymbolTable(
    resources::OutputFormatter& formatter) const {
  // Prefer the backend-agnostic pointer populated by the dispatcher.
  // Fall back to the Asar wrapper so CLI-only builds (no editor) still
  // get symbols from direct ApplyPatch calls.
  // Note: AsarWrapper::GetSymbolTable() returns by value, so we must own
  // a local copy in the fallback branch — we can't bind a reference to it.
  std::map<std::string, core::AsarSymbol> fallback;
  const bool had_live_source =
      assembly_symbol_table_ != nullptr || asar_wrapper_ != nullptr;
  bool loaded_from_artifact = false;
  const std::map<std::string, core::AsarSymbol>* symbols_ptr =
      assembly_symbol_table_;
  if (!symbols_ptr && asar_wrapper_) {
    fallback = asar_wrapper_->GetSymbolTable();
    symbols_ptr = &fallback;
  } else if (!symbols_ptr && project_) {
    fallback = LoadSymbolsFromMlb(GetProjectSymbolsPath(*project_));
    loaded_from_artifact = !fallback.empty();
    symbols_ptr = &fallback;
  }
  if (!symbols_ptr) {
    static const std::map<std::string, core::AsarSymbol> kEmpty;
    symbols_ptr = &kEmpty;
  }
  const auto& symbols = *symbols_ptr;
  if (symbols.empty()) {
    if (!had_live_source && !loaded_from_artifact) {
      return absl::FailedPreconditionError(
          "No assembler backend or emitted symbol artifact is available.");
    }
    return absl::NotFoundError(
        "No symbols loaded. Load symbols via the Assemble menu or ensure the "
        "build script generates them.");
  }

  Json sourcemap;
  const bool has_sourcemap =
      project_ && LoadProjectSourceMap(*project_, &sourcemap);

  formatter.BeginArray("symbols");
  for (const auto& [name, symbol] : symbols) {
    formatter.BeginObject();
    formatter.AddField("name", symbol.name);
    formatter.AddField("address", absl::StrFormat("$%06X", symbol.address));
    formatter.AddField("bank", absl::StrFormat("$%02X", symbol.address >> 16));
    if (has_sourcemap) {
      if (const Json* entry = FindBestSourceEntry(sourcemap, symbol.address)) {
        const int file_id = entry->value("file_id", -1);
        const int line = entry->value("line", 0);
        const std::string path = LookupSourceFile(sourcemap, file_id);
        if (!path.empty()) {
          formatter.AddField("file", path);
        }
        if (line > 0) {
          formatter.AddField("line", line);
        }
      }
    }
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

absl::Status ProjectGraphTool::LookupAddressOrSymbol(
    const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) const {
  if (!project_) {
    return absl::FailedPreconditionError("Project context not available.");
  }

  std::map<std::string, core::AsarSymbol> symbols;
  if (assembly_symbol_table_) {
    symbols = *assembly_symbol_table_;
  } else if (asar_wrapper_) {
    symbols = asar_wrapper_->GetSymbolTable();
  } else {
    symbols = LoadSymbolsFromMlb(GetProjectSymbolsPath(*project_));
  }

  uint32_t address = 0;
  if (auto symbol = parser.GetString("symbol"); symbol.has_value()) {
    auto it = symbols.find(*symbol);
    if (it == symbols.end()) {
      return absl::NotFoundError(absl::StrCat("Unknown symbol: ", *symbol));
    }
    address = it->second.address;
    formatter.AddField("symbol", it->second.name);
  } else if (auto address_arg = parser.GetString("address");
             address_arg.has_value()) {
    auto address_or = ParseAddress(*address_arg);
    if (!address_or.ok()) {
      return address_or.status();
    }
    address = *address_or;
  } else {
    return absl::InvalidArgumentError(
        "lookup query requires --symbol or --address");
  }

  formatter.AddField("address", absl::StrFormat("$%06X", address));
  formatter.AddField("bank", absl::StrFormat("$%02X", address >> 16));

  formatter.BeginArray("matching_symbols");
  for (const auto& [name, symbol] : symbols) {
    if (symbol.address != address) {
      continue;
    }
    formatter.BeginObject();
    formatter.AddField("name", name);
    formatter.EndObject();
  }
  formatter.EndArray();

  Json sourcemap;
  if (LoadProjectSourceMap(*project_, &sourcemap)) {
    if (const Json* entry = FindBestSourceEntry(sourcemap, address)) {
      formatter.BeginObject("source");
      formatter.AddField(
          "file", LookupSourceFile(sourcemap, entry->value("file_id", -1)));
      formatter.AddField("line", entry->value("line", 0));
      formatter.AddField("entry_address", entry->value("address", "0x0"));
      formatter.EndObject();
    }
  }

  return absl::OkStatus();
}

absl::Status ProjectGraphTool::GetWriteCoverage(
    resources::OutputFormatter& formatter) const {
  if (!project_) {
    return absl::FailedPreconditionError("Project context not available.");
  }

  Json hooks;
  if (!LoadProjectHooks(*project_, &hooks) || !hooks.contains("hooks") ||
      !hooks["hooks"].is_array()) {
    return absl::NotFoundError(
        "No z3dk hook coverage artifact found. Assemble with z3dk first.");
  }

  std::map<int, int> bytes_by_bank;
  formatter.BeginArray("writes");
  for (const auto& hook : hooks["hooks"]) {
    const std::string address_str = hook.value("address", "0x0");
    auto address_or = ParseAddress(address_str);
    if (!address_or.ok()) {
      continue;
    }
    const uint32_t address = *address_or;
    const int bank = GetLoRomBankIndex(address);
    const int size = hook.value("size", 0);
    bytes_by_bank[bank] += size;

    formatter.BeginObject();
    formatter.AddField("address", absl::StrFormat("$%06X", address));
    formatter.AddField("bank", absl::StrFormat("$%02X", bank));
    formatter.AddField("size", size);
    formatter.AddField("kind", hook.value("kind", "patch"));
    formatter.AddField("name", hook.value("name", ""));
    if (hook.contains("source")) {
      formatter.AddField("source", hook.value("source", ""));
    }
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("banks");
  for (const auto& [bank, bytes] : bytes_by_bank) {
    formatter.BeginObject();
    formatter.AddField("bank", absl::StrFormat("$%02X", bank));
    formatter.AddField("bytes_written", bytes);
    formatter.EndObject();
  }
  formatter.EndArray();
  return absl::OkStatus();
}

absl::Status ProjectGraphTool::GetBankContext(
    const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) const {
  if (!project_) {
    return absl::FailedPreconditionError("Project context not available.");
  }

  auto bank_arg = parser.GetString("bank");
  if (!bank_arg.has_value()) {
    return absl::InvalidArgumentError("bank query requires --bank=<hex>");
  }
  auto bank_or = ParseBankIndex(*bank_arg);
  if (!bank_or.ok()) {
    return bank_or.status();
  }
  const int bank = *bank_or;

  formatter.AddField("bank", absl::StrFormat("$%02X", bank));
  const std::string disasm_file = GetDisasmBankFile(*project_, bank);
  formatter.AddField("disasm_file", disasm_file);
  formatter.AddField("disasm_file_exists",
                     static_cast<bool>(std::filesystem::exists(disasm_file)));

  Json sourcemap;
  formatter.BeginArray("sources");
  if (LoadProjectSourceMap(*project_, &sourcemap) &&
      sourcemap.contains("entries") && sourcemap["entries"].is_array()) {
    for (const auto& entry : sourcemap["entries"]) {
      auto address_or = ParseAddress(entry.value("address", "0x0"));
      if (!address_or.ok() || GetLoRomBankIndex(*address_or) != bank) {
        continue;
      }
      formatter.BeginObject();
      formatter.AddField("address", absl::StrFormat("$%06X", *address_or));
      formatter.AddField(
          "file", LookupSourceFile(sourcemap, entry.value("file_id", -1)));
      formatter.AddField("line", entry.value("line", 0));
      formatter.EndObject();
    }
  }
  formatter.EndArray();

  Json hooks;
  formatter.BeginArray("hooks");
  if (LoadProjectHooks(*project_, &hooks) && hooks.contains("hooks") &&
      hooks["hooks"].is_array()) {
    for (const auto& hook : hooks["hooks"]) {
      auto address_or = ParseAddress(hook.value("address", "0x0"));
      if (!address_or.ok() || GetLoRomBankIndex(*address_or) != bank) {
        continue;
      }
      formatter.BeginObject();
      formatter.AddField("address", absl::StrFormat("$%06X", *address_or));
      formatter.AddField("kind", hook.value("kind", "patch"));
      formatter.AddField("name", hook.value("name", ""));
      formatter.AddField("size", hook.value("size", 0));
      formatter.AddField("source", hook.value("source", ""));
      formatter.EndObject();
    }
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
