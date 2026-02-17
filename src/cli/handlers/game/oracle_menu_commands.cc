#include "cli/handlers/game/oracle_menu_commands.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "core/oracle_menu_registry.h"
#include "util/macro.h"

namespace yaze::cli::handlers {

namespace {

std::filesystem::path ResolveProjectPath(
    const resources::ArgumentParser& parser) {
  if (auto project_opt = parser.GetString("project"); project_opt.has_value()) {
    return std::filesystem::path(*project_opt);
  }
  return std::filesystem::current_path();
}

std::string FormatSize(uintmax_t size_bytes) {
  return absl::StrFormat("%llu", static_cast<unsigned long long>(size_bytes));
}

}  // namespace

absl::Status OracleMenuIndexCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  if (auto table_filter = parser.GetString("table");
      table_filter.has_value() && table_filter->empty()) {
    return absl::InvalidArgumentError("--table cannot be empty");
  }
  return absl::OkStatus();
}

absl::Status OracleMenuIndexCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const auto root,
                   core::ResolveOracleProjectRoot(ResolveProjectPath(parser)));
  ASSIGN_OR_RETURN(auto registry, core::BuildOracleMenuRegistry(root));

  const std::string table_filter = parser.GetString("table").value_or("");
  std::string draw_filter = parser.GetString("draw-filter").value_or("");
  draw_filter = absl::AsciiStrToLower(draw_filter);
  const bool missing_bins_only = parser.HasFlag("missing-bins");

  std::vector<const core::OracleMenuBinEntry*> bins;
  bins.reserve(registry.bins.size());
  for (const auto& entry : registry.bins) {
    if (missing_bins_only && entry.exists) {
      continue;
    }
    bins.push_back(&entry);
  }

  std::vector<const core::OracleMenuDrawRoutine*> draw_routines;
  draw_routines.reserve(registry.draw_routines.size());
  for (const auto& routine : registry.draw_routines) {
    if (!draw_filter.empty()) {
      std::string label_lower = absl::AsciiStrToLower(routine.label);
      if (label_lower.find(draw_filter) == std::string::npos) {
        continue;
      }
    }
    draw_routines.push_back(&routine);
  }

  std::vector<const core::OracleMenuComponent*> components;
  components.reserve(registry.components.size());
  for (const auto& component : registry.components) {
    if (!table_filter.empty() && component.table_label != table_filter) {
      continue;
    }
    components.push_back(&component);
  }

  formatter.AddField("project_root", root.string());
  formatter.AddField("asm_files", static_cast<int>(registry.asm_files.size()));
  formatter.AddField("bin_count", static_cast<int>(bins.size()));
  formatter.AddField("draw_routine_count",
                     static_cast<int>(draw_routines.size()));
  formatter.AddField("component_count", static_cast<int>(components.size()));
  formatter.AddField("warnings", static_cast<int>(registry.warnings.size()));

  formatter.BeginArray("bins");
  for (const auto* entry : bins) {
    formatter.AddArrayItem(absl::StrFormat(
        "%s | %s:%d | %s | %s bytes | %s",
        entry->label.empty() ? "(unlabeled)" : entry->label, entry->asm_path,
        entry->line, entry->resolved_bin_path, FormatSize(entry->size_bytes),
        entry->exists ? "ok" : "missing"));
  }
  formatter.EndArray();

  formatter.BeginArray("draw_routines");
  for (const auto* routine : draw_routines) {
    formatter.AddArrayItem(absl::StrFormat(
        "%s | %s:%d | refs=%d%s", routine->label, routine->asm_path,
        routine->line, routine->references, routine->local ? " | local" : ""));
  }
  formatter.EndArray();

  formatter.BeginArray("components");
  for (const auto* component : components) {
    formatter.AddArrayItem(
        absl::StrFormat("%s[%d] | (%d,%d) | %s:%d%s%s", component->table_label,
                        component->index, component->row, component->col,
                        component->asm_path, component->line,
                        component->note.empty() ? "" : " | ", component->note));
  }
  formatter.EndArray();

  formatter.BeginArray("warnings_list");
  for (const auto& warning : registry.warnings) {
    formatter.AddArrayItem(warning);
  }
  formatter.EndArray();

  return absl::OkStatus();
}

absl::Status OracleMenuSetOffsetCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const int index, parser.GetInt("index"));
  ASSIGN_OR_RETURN(const int row, parser.GetInt("row"));
  ASSIGN_OR_RETURN(const int col, parser.GetInt("col"));

  const std::filesystem::path project_path = ResolveProjectPath(parser);
  ASSIGN_OR_RETURN(const auto root,
                   core::ResolveOracleProjectRoot(project_path));
  const std::string asm_path = parser.GetString("asm").value_or("");
  const std::string table_label = parser.GetString("table").value_or("");
  const bool write_changes = parser.HasFlag("write");

  ASSIGN_OR_RETURN(auto result, core::SetOracleMenuComponentOffset(
                                    project_path, asm_path, table_label, index,
                                    row, col, write_changes));

  formatter.AddField("project_root", root.string());
  formatter.AddField("asm", result.asm_path);
  formatter.AddField("line", result.line);
  formatter.AddField("table", result.table_label);
  formatter.AddField("index", result.index);
  formatter.AddField("old_row", result.old_row);
  formatter.AddField("old_col", result.old_col);
  formatter.AddField("new_row", result.new_row);
  formatter.AddField("new_col", result.new_col);
  formatter.AddField("changed", result.changed);
  formatter.AddField("write_applied", result.write_applied);
  formatter.AddField("mode", write_changes ? "write" : "dry-run");
  formatter.AddField("old_line", result.old_line);
  formatter.AddField("new_line", result.new_line);

  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
