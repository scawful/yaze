#ifndef YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_SOURCE_H_
#define YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_SOURCE_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"
#include "zelda3/overworld/graphics_preview.h"

namespace yaze::editor {

// A source document for external project graphics. Tile indices belong to the
// selected resource, never to the ROM's native graphics-sheet namespace.
class ProjectGraphicsSource {
 public:
  struct Slice {
    std::string profile_id;
    std::vector<int> parent_maps;
    size_t source_offset = 0;
    size_t byte_count = 0;
    int vram_word_address = 0;
    int bg_character_base_word = 0;
    size_t first_resource_tile() const { return source_offset / 32; }
    int first_character() const {
      return (vram_word_address - bg_character_base_word) / 16;
    }
  };

  struct Resource {
    std::string id;
    std::string source_path;
    std::string portable_path;
    std::string sha256;
    std::vector<uint8_t> original_bytes;
    std::vector<uint8_t> pending_bytes;
    std::vector<zelda3::PreviewTile8> tiles;
    // All consumers, including profiles outside the opened parent map.
    std::vector<Slice> slices;
    std::vector<std::string> profile_ids;
    std::vector<int> parent_maps;
    bool dirty() const { return original_bytes != pending_bytes; }
  };

  ProjectGraphicsSource();
  ~ProjectGraphicsSource();
  ProjectGraphicsSource(ProjectGraphicsSource&&) noexcept;
  ProjectGraphicsSource& operator=(ProjectGraphicsSource&&) noexcept;
  ProjectGraphicsSource(const ProjectGraphicsSource&) = delete;
  ProjectGraphicsSource& operator=(const ProjectGraphicsSource&) = delete;

  // source_root is an explicitly bound Oracle repository root. Both complete
  // V2 catalogs must validate and describe the same resources and consumers.
  // A failed Load clears the document, preventing cross-project edits.
  absl::Status Load(const std::string& source_root,
                    const std::string& portable_catalog, int parent_map);
  const std::vector<Resource>& resources() const { return resources_; }
  const Resource* selected_resource() const;
  std::string selected_resource_id() const {
    const auto* resource = selected_resource();
    return resource ? resource->id : std::string();
  }
  absl::Status SelectResource(const std::string& resource_id);
  const std::string& source_root() const { return source_root_; }
  const std::string& portable_catalog() const { return portable_catalog_; }
  const std::string& profile_id() const { return profile_id_; }
  int parent_map() const { return parent_map_; }
  bool loaded() const { return !resources_.empty(); }
  bool backend_available() const;

  absl::StatusOr<uint8_t> GetPixel(size_t tile, int x, int y) const;
  absl::Status SetPixel(size_t tile, int x, int y, uint8_t index);
  absl::Status Undo();
  absl::Status Redo();
  bool can_undo() const;
  bool can_redo() const;
  bool dirty() const;
  bool any_dirty() const;

  // Atomically replace one loaded resource without recording pixel actions.
  // The source_path overload binds to the exact current Resource::source_path
  // and does not change the selected resource. Publication may change that path.
  // A changed replacement clears only that resource's pixel history; callers
  // own before/after snapshots for a combined import command's Undo/Redo.
  absl::Status ReplaceResourceBytes(const std::vector<uint8_t>& bytes);
  absl::Status ReplaceResourceBytes(const std::string& source_path,
                                    const std::vector<uint8_t>& bytes);

  // Resolve pending bytes directly into character overlays without temporary
  // files or publication. Includes every known consumer of the loaded resources.
  // Other profiles may also use resources not loaded in this document; callers
  // must merge these overlays into their complete project graphics preview.
  absl::StatusOr<zelda3::OverworldGraphicsPreview> PendingGraphicsPreview()
      const;

  // Preview writes only a private replacement and the publisher's review plan.
  // Any pixel/history/selection change invalidates that held preview. Publish
  // verifies the loaded disk bytes, replacement and plan before invoking apply.
  absl::StatusOr<nlohmann::json> Preview();
  absl::StatusOr<nlohmann::json> Publish();
  bool has_preview() const { return preview_ != nullptr; }
  const nlohmann::json& preview_json() const { return preview_json_; }

 private:
  struct PixelAction {
    size_t tile;
    int x;
    int y;
    uint8_t before;
    uint8_t after;
  };
  struct History {
    std::vector<PixelAction> undo;
    std::vector<PixelAction> redo;
  };
  struct PreviewFiles;

  absl::Status CheckUnchanged() const;
  absl::Status ReplaceResourceBytesAt(size_t resource,
                                      const std::vector<uint8_t>& bytes);
  absl::Status CheckPixel(size_t tile, int x, int y) const;
  void WritePixel(size_t tile, int x, int y, uint8_t index);
  void InvalidatePreview();
  std::string source_root_;
  std::string portable_catalog_;
  std::string profile_id_;
  int parent_map_ = -1;
  size_t selected_ = 0;
  std::vector<Resource> resources_;
  std::vector<History> histories_;
  std::map<std::string, std::string> loaded_hashes_;
  std::unique_ptr<PreviewFiles> preview_;
  nlohmann::json preview_json_;
};

}  // namespace yaze::editor
#endif
