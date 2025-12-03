#include "object_templates.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace zelda3 {

using json = nlohmann::json;

// JSON serialization helpers
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TemplateObject, id, rel_x, rel_y, size,
                                   layer)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectTemplate, name, description, category,
                                   objects)

ObjectTemplateManager::ObjectTemplateManager() {
  // Initialize with some default templates if needed
}

absl::Status ObjectTemplateManager::LoadTemplates(
    const std::string& directory_path) {
  templates_.clear();

  if (!std::filesystem::exists(directory_path)) {
    // It's okay if the directory doesn't exist yet, just return empty
    return absl::OkStatus();
  }

  for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
    if (entry.path().extension() == ".json") {
      try {
        std::ifstream i(entry.path());
        json j;
        i >> j;
        ObjectTemplate tmpl = j.get<ObjectTemplate>();
        templates_.push_back(tmpl);
      } catch (const std::exception& e) {
        std::cerr << "Failed to load template " << entry.path() << ": "
                  << e.what() << std::endl;
        // Continue loading other templates
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ObjectTemplateManager::SaveTemplate(
    const ObjectTemplate& tmpl, const std::string& directory_path) {
  if (!std::filesystem::exists(directory_path)) {
    std::filesystem::create_directories(directory_path);
  }

  // Sanitize filename
  std::string filename = tmpl.name;
  std::replace(filename.begin(), filename.end(), ' ', '_');
  std::string path =
      absl::StrFormat("%s/%s.json", directory_path.c_str(), filename.c_str());

  try {
    json j = tmpl;
    std::ofstream o(path);
    o << std::setw(4) << j << std::endl;
    
    // Add to in-memory list if not present
    bool exists = false;
    for (auto& t : templates_) {
      if (t.name == tmpl.name) {
        t = tmpl; // Update existing
        exists = true;
        break;
      }
    }
    if (!exists) {
      templates_.push_back(tmpl);
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrFormat("Failed to save template: %s", e.what()));
  }
}

ObjectTemplate ObjectTemplateManager::CreateFromObjects(
    const std::string& name, const std::string& description,
    const std::vector<RoomObject>& objects, int origin_x, int origin_y) {
  ObjectTemplate tmpl;
  tmpl.name = name;
  tmpl.description = description;
  tmpl.category = "Custom"; // Default category

  for (const auto& obj : objects) {
    TemplateObject t_obj;
    t_obj.id = obj.id_;
    t_obj.rel_x = obj.x_ - origin_x;
    t_obj.rel_y = obj.y_ - origin_y;
    t_obj.size = obj.size_;
    t_obj.layer = static_cast<int>(obj.layer_);
    tmpl.objects.push_back(t_obj);
  }

  return tmpl;
}

std::vector<RoomObject> ObjectTemplateManager::InstantiateTemplate(
    const ObjectTemplate& tmpl, int x, int y, Rom* rom) {
  std::vector<RoomObject> new_objects;

  for (const auto& t_obj : tmpl.objects) {
    int obj_x = x + t_obj.rel_x;
    int obj_y = y + t_obj.rel_y;

    // Basic bounds check
    if (obj_x >= 0 && obj_x < 64 && obj_y >= 0 && obj_y < 64) {
      RoomObject obj(t_obj.id, obj_x, obj_y, t_obj.size, t_obj.layer);
      if (rom) {
        obj.SetRom(rom);
        obj.EnsureTilesLoaded();
      }
      new_objects.push_back(obj);
    }
  }

  return new_objects;
}

}  // namespace zelda3
}  // namespace yaze
