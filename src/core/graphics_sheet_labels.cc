#include "core/graphics_sheet_labels.h"

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "zelda3/gfx_sheet_inventory.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::core {

std::string GetGraphicsSheetLabel(const project::YazeProject& project,
                                  uint16_t sheet) {
  const auto type = project.resource_labels.find(kGraphicsSheetLabelType);
  if (type == project.resource_labels.end()) {
    return "";
  }
  for (const std::string& key :
       {std::to_string(sheet), absl::StrFormat("0x%02X", sheet)}) {
    if (auto it = type->second.find(key);
        it != type->second.end() && !it->second.empty()) {
      return it->second;
    }
  }
  return "";
}

void SetGraphicsSheetLabel(project::YazeProject& project, uint16_t sheet,
                           const std::string& label) {
  auto& labels = project.resource_labels[kGraphicsSheetLabelType];
  labels.erase(absl::StrFormat("0x%02X", sheet));
  if (label.empty()) {
    labels.erase(std::to_string(sheet));
  } else {
    labels[std::to_string(sheet)] = label;
  }
}

int ImportSpritesetSheetLabels(project::YazeProject& project,
                               const std::string& csv_text, bool overwrite) {
  int written = 0;
  for (const auto& [value, names] : zelda3::ParseSpritesetLabelCsv(csv_text)) {
    if (value >= zelda3::kSpriteSheetValueCount) {
      continue;
    }
    const auto sheet = static_cast<uint16_t>(value + zelda3::kSpriteSheetBase);
    if (!overwrite && !GetGraphicsSheetLabel(project, sheet).empty()) {
      continue;
    }
    SetGraphicsSheetLabel(project, sheet, absl::StrJoin(names, "; "));
    ++written;
  }
  return written;
}

}  // namespace yaze::core
