#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_TEMPLATES_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_TEMPLATES_H

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

// Represents an object within a template, relative to the template's origin
struct TemplateObject {
  int id;
  int rel_x;  // Relative X offset
  int rel_y;  // Relative Y offset
  int size;
  int layer;
};

// Represents a reusable group of objects
struct ObjectTemplate {
  std::string name;
  std::string description;
  std::string category;
  std::vector<TemplateObject> objects;
};

class ObjectTemplateManager {
 public:
  ObjectTemplateManager();
  ~ObjectTemplateManager() = default;

  // Load templates from the assets directory
  absl::Status LoadTemplates(const std::string& directory_path);

  // Save a new template
  absl::Status SaveTemplate(const ObjectTemplate& tmpl,
                            const std::string& directory_path);

  // Get all loaded templates
  const std::vector<ObjectTemplate>& GetTemplates() const { return templates_; }

  // Create a template from a selection of room objects
  // origin_x/y are usually the top-left most object's coordinates or a specific
  // anchor
  static ObjectTemplate CreateFromObjects(
      const std::string& name, const std::string& description,
      const std::vector<RoomObject>& objects, int origin_x, int origin_y);

  // Instantiate a template at a specific position
  std::vector<RoomObject> InstantiateTemplate(const ObjectTemplate& tmpl,
                                              int x, int y, Rom* rom);

 private:
  std::vector<ObjectTemplate> templates_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_TEMPLATES_H
