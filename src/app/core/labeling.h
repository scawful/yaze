#ifndef YAZE_APP_CORE_LABELING_H_
#define YAZE_APP_CORE_LABELING_H_

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/common.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace core {

std::string UppercaseHexByte(uint8_t byte, bool leading = false);
std::string UppercaseHexWord(uint16_t word);
std::string UppercaseHexLong(uint32_t dword);
std::string UppercaseHexLongLong(uint64_t qword);

bool StringReplace(std::string& str, const std::string& from,
                   const std::string& to);

// Default types
static constexpr absl::string_view kDefaultTypes[] = {
    "Dungeon Names", "Dungeon Room Names", "Overworld Map Names"};

struct ResourceLabelManager {
  bool LoadLabels(const std::string& filename);
  bool SaveLabels();
  void DisplayLabels(bool* p_open);
  void EditLabel(const std::string& type, const std::string& key,
                 const std::string& newValue);
  void SelectableLabelWithNameEdit(bool selected, const std::string& type,
                                   const std::string& key,
                                   const std::string& defaultValue);
  std::string CreateOrGetLabel(const std::string& type, const std::string& key,
                               const std::string& defaultValue);

  bool labels_loaded_ = false;
  std::string filename_;
  struct ResourceType {
    std::string key_name;
    std::string display_description;
  };

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      labels_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_LABELING_H_