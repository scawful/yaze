#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_SERIALIZATION_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_SERIALIZATION_H_

#include <string>

#include "app/editor/layout_designer/layout_definition.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @class LayoutSerializer
 * @brief Handles JSON serialization and deserialization of layouts
 */
class LayoutSerializer {
 public:
  /**
   * @brief Serialize a layout to JSON string
   * @param layout The layout to serialize
   * @return JSON string representation
   */
  static std::string ToJson(const LayoutDefinition& layout);
  
  /**
   * @brief Deserialize a layout from JSON string
   * @param json_str The JSON string
   * @return LayoutDefinition or error status
   */
  static absl::StatusOr<LayoutDefinition> FromJson(const std::string& json_str);
  
  /**
   * @brief Save layout to JSON file
   * @param layout The layout to save
   * @param filepath Path to save to
   * @return Success status
   */
  static absl::Status SaveToFile(const LayoutDefinition& layout,
                                  const std::string& filepath);
  
  /**
   * @brief Load layout from JSON file
   * @param filepath Path to load from
   * @return LayoutDefinition or error status
   */
  static absl::StatusOr<LayoutDefinition> LoadFromFile(const std::string& filepath);

 private:
  // Helper methods for converting individual components
  static std::string SerializePanel(const LayoutPanel& panel);
  static std::string SerializeDockNode(const DockNode& node);
  
  static LayoutPanel DeserializePanel(const std::string& json);
  static std::unique_ptr<DockNode> DeserializeDockNode(const std::string& json);
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_SERIALIZATION_H_

