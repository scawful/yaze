#include "zelda3/dungeon/custom_object.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/geometry/object_geometry.h"

namespace yaze {
namespace zelda3 {

namespace {

namespace fs = std::filesystem;

constexpr int kBufferStrideBytes = 128;
constexpr int kBufferWidthTiles = kBufferStrideBytes / 2;
constexpr int kBufferHeightTiles = 64;
constexpr int kBufferSizeBytes = kBufferStrideBytes * kBufferHeightTiles;
constexpr int kMaxSegmentTiles = 0x20;

const core::SourceArtifactPublisherLabels kCustomObjectPublisherLabels{
    .subject = "custom object asset",
    .published_file = "published custom object asset",
};

absl::StatusOr<std::vector<uint8_t>> ReadBinaryFileAtPath(
    const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(absl::StrFormat(
        "Could not open custom object file: %s", path.string()));
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  if (input.bad()) {
    return absl::InternalError(absl::StrFormat(
        "Could not read custom object file: %s", path.string()));
  }
  return bytes;
}

bool PathStartsWith(const fs::path& path, const fs::path& root) {
  auto path_it = path.begin();
  for (auto root_it = root.begin(); root_it != root.end();
       ++root_it, ++path_it) {
    if (path_it == path.end() || *path_it != *root_it) {
      return false;
    }
  }
  return true;
}

bool IsPortableAssetComponent(std::string_view component) {
  if (component.empty() ||
      std::isspace(static_cast<unsigned char>(component.front())) ||
      std::isspace(static_cast<unsigned char>(component.back())) ||
      component.back() == '.') {
    return false;
  }
  for (const unsigned char character : component) {
    if (character < 0x20 || character == 0x7F || character == ',' ||
        character == '<' || character == '>' || character == ':' ||
        character == '"' || character == '\\' || character == '|' ||
        character == '?' || character == '*') {
      return false;
    }
  }
  std::string basename(component.substr(0, component.find('.')));
  std::transform(basename.begin(), basename.end(), basename.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::toupper(character));
                 });
  static const std::unordered_set<std::string> kWindowsReservedNames = {
      "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
  };
  return !kWindowsReservedNames.contains(basename);
}

std::vector<CustomObject::TileMapEntry> VisibleRuntimeTiles(
    const CustomObject& object) {
  std::vector<CustomObject::TileMapEntry> visible;
  visible.reserve(object.tiles.size());
  for (const auto& tile : object.tiles) {
    if (tile.tile_data != 0) {
      visible.push_back(tile);
    }
  }
  std::sort(
      visible.begin(), visible.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.rel_y, lhs.rel_x) < std::tie(rhs.rel_y, rhs.rel_x);
      });
  return visible;
}

}  // namespace

absl::StatusOr<CustomObject> DecodeCustomObjectBinary(
    const std::vector<uint8_t>& data) {
  if (data.empty()) {
    return absl::DataLossError("Custom object data is empty");
  }

  CustomObject object;
  std::unordered_set<int> occupied_positions;
  size_t cursor = 0;
  int current_buffer_pos = 0;
  bool found_terminator = false;

  while (cursor + 1 < data.size()) {
    const uint16_t header = static_cast<uint16_t>(data[cursor]) |
                            (static_cast<uint16_t>(data[cursor + 1]) << 8);
    cursor += 2;
    if (header == 0) {
      found_terminator = true;
      break;
    }
    const int encoded_count = header & 0x001F;
    const int count = encoded_count == 0 ? kMaxSegmentTiles : encoded_count;
    // The runtime decrements the complete header before testing its low five
    // count bits. A 32-tile segment is encoded as count zero, so its final
    // decrement borrows from the stored high byte.
    const int jump_offset = ((header - count) >> 8) & 0xFF;
    if ((jump_offset & 1) != 0) {
      return absl::DataLossError(
          "Custom object segment jump is not tile-aligned");
    }
    if (cursor + static_cast<size_t>(count * 2) > data.size()) {
      return absl::DataLossError("Custom object ends inside a tile segment");
    }

    const int segment_start = current_buffer_pos;
    for (int index = 0; index < count; ++index) {
      if (current_buffer_pos < 0 ||
          current_buffer_pos + 1 >= kBufferSizeBytes ||
          (current_buffer_pos & 1) != 0) {
        return absl::OutOfRangeError(
            "Custom object tile is outside the 64x64 dungeon buffer");
      }
      if (!occupied_positions.insert(current_buffer_pos).second) {
        return absl::DataLossError(
            "Custom object segments overlap the same tile position");
      }

      const uint16_t tile_data = static_cast<uint16_t>(data[cursor]) |
                                 (static_cast<uint16_t>(data[cursor + 1]) << 8);
      cursor += 2;
      object.tiles.push_back({(current_buffer_pos % kBufferStrideBytes) / 2,
                              current_buffer_pos / kBufferStrideBytes,
                              tile_data});
      current_buffer_pos += 2;
    }
    current_buffer_pos = segment_start + jump_offset;
  }

  if (!found_terminator) {
    return absl::DataLossError("Custom object is missing its terminator");
  }
  if (cursor != data.size()) {
    return absl::DataLossError(
        "Custom object contains trailing bytes after its terminator");
  }
  return object;
}

absl::StatusOr<std::vector<uint8_t>> EncodeCustomObjectBinary(
    const CustomObject& object) {
  if (object.tiles.empty()) {
    return std::vector<uint8_t>{0, 0};
  }

  struct OrderedTile {
    int position;
    uint16_t tile_data;
  };
  std::vector<OrderedTile> ordered_tiles;
  ordered_tiles.reserve(object.tiles.size());
  for (const auto& tile : object.tiles) {
    if (tile.rel_x < 0 || tile.rel_x >= kBufferWidthTiles || tile.rel_y < 0 ||
        tile.rel_y >= kBufferHeightTiles) {
      return absl::OutOfRangeError(
          "Custom object tile is outside the 64x64 dungeon buffer");
    }
    ordered_tiles.push_back(
        {tile.rel_y * kBufferWidthTiles + tile.rel_x, tile.tile_data});
  }
  std::sort(ordered_tiles.begin(), ordered_tiles.end(),
            [](const OrderedTile& lhs, const OrderedTile& rhs) {
              return lhs.position < rhs.position;
            });
  for (size_t index = 1; index < ordered_tiles.size(); ++index) {
    if (ordered_tiles[index - 1].position == ordered_tiles[index].position) {
      return absl::InvalidArgumentError(
          "Custom object contains duplicate tile positions");
    }
  }
  struct Segment {
    int start_position;
    std::vector<uint16_t> tile_words;
  };
  std::vector<Segment> segments;
  size_t tile_index = 0;
  int segment_start = 0;
  while (tile_index < ordered_tiles.size()) {
    Segment segment{segment_start, {}};
    if (ordered_tiles[tile_index].position == segment_start) {
      int next_position = segment_start;
      while (tile_index < ordered_tiles.size() &&
             ordered_tiles[tile_index].position == next_position &&
             segment.tile_words.size() < kMaxSegmentTiles) {
        segment.tile_words.push_back(ordered_tiles[tile_index].tile_data);
        ++tile_index;
        ++next_position;
      }
    } else {
      // Oracle advances for a zero payload word without touching the dungeon
      // tilemap. One no-op word gives a distant target a legal segment from
      // which the next 8-bit jump can continue.
      segment.tile_words.push_back(0);
    }
    segments.push_back(std::move(segment));

    if (tile_index >= ordered_tiles.size()) {
      break;
    }
    const int next_target = ordered_tiles[tile_index].position;
    const int minimum_next_start =
        segment_start + static_cast<int>(segments.back().tile_words.size());
    const int maximum_next_start = segment_start + 0x7F;
    segment_start = std::min(next_target, maximum_next_start);
    if (segment_start < minimum_next_start) {
      return absl::DataLossError(
          "Custom object segment planner produced an overlapping jump");
    }
  }

  std::vector<uint8_t> binary;
  binary.reserve(object.tiles.size() * 2 + segments.size() * 2 + 2);
  for (size_t index = 0; index < segments.size(); ++index) {
    int jump_bytes = 0;
    if (index + 1 < segments.size()) {
      jump_bytes = (segments[index + 1].start_position -
                    segments[index].start_position) *
                   2;
      if (jump_bytes <= 0 || jump_bytes > 0xFE) {
        return absl::DataLossError(
            "Custom object segment planner exceeded the runtime jump range");
      }
    }
    const bool encodes_thirty_two_tiles =
        segments[index].tile_words.size() == kMaxSegmentTiles;
    int stored_jump = jump_bytes;
    if (encodes_thirty_two_tiles) {
      ++stored_jump;
      if (stored_jump > 0xFF) {
        return absl::InvalidArgumentError(
            "Custom object gap after a 32-tile segment cannot be represented "
            "by its adjusted 8-bit jump");
      }
    }
    const uint16_t encoded_count =
        static_cast<uint16_t>(segments[index].tile_words.size()) & 0x001F;
    const uint16_t header =
        encoded_count | (static_cast<uint16_t>(stored_jump) << 8);
    binary.push_back(static_cast<uint8_t>(header & 0xFF));
    binary.push_back(static_cast<uint8_t>(header >> 8));
    for (uint16_t tile_word : segments[index].tile_words) {
      binary.push_back(static_cast<uint8_t>(tile_word & 0xFF));
      binary.push_back(static_cast<uint8_t>(tile_word >> 8));
    }
  }
  binary.push_back(0);
  binary.push_back(0);

  auto decoded_or = DecodeCustomObjectBinary(binary);
  if (!decoded_or.ok()) {
    return absl::DataLossError(
        absl::StrFormat("Encoded custom object failed strict validation: %s",
                        decoded_or.status().message()));
  }
  if (VisibleRuntimeTiles(*decoded_or) != VisibleRuntimeTiles(object)) {
    return absl::DataLossError(
        "Encoded custom object did not preserve its visible runtime layout");
  }
  return binary;
}

absl::StatusOr<fs::path> ResolveCustomObjectAssetPath(
    const std::string& custom_objects_folder, const std::string& filename) {
  if (custom_objects_folder.empty()) {
    return absl::FailedPreconditionError(
        "Custom objects folder is not configured");
  }
  if (filename.empty()) {
    return absl::InvalidArgumentError("Custom object filename is empty");
  }

  const fs::path relative_path(filename);
  if (relative_path.is_absolute() || relative_path.has_root_directory() ||
      relative_path.has_root_name()) {
    return absl::InvalidArgumentError(
        "Custom object filename must be project-relative");
  }
  for (const auto& component : relative_path) {
    if (component == "..") {
      return absl::InvalidArgumentError(
          "Custom object filename cannot contain parent traversal");
    }
    if (component != "." &&
        !IsPortableAssetComponent(component.generic_string())) {
      return absl::InvalidArgumentError(
          "Custom object filename contains a non-portable path component");
    }
  }
  const fs::path normalized = relative_path.lexically_normal();
  if (normalized.empty() || normalized.filename().empty() ||
      normalized.extension() != ".bin") {
    return absl::InvalidArgumentError(
        "Custom object filename must end in .bin");
  }

  std::error_code canonical_error;
  const fs::path canonical_root =
      fs::canonical(fs::path(custom_objects_folder), canonical_error);
  if (canonical_error || !fs::is_directory(canonical_root, canonical_error) ||
      canonical_error) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Custom objects folder is unavailable: %s", custom_objects_folder));
  }
  const fs::path candidate_parent = canonical_root / normalized.parent_path();
  const fs::path canonical_parent =
      fs::canonical(candidate_parent, canonical_error);
  if (canonical_error || !fs::is_directory(canonical_parent, canonical_error) ||
      canonical_error) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Custom object parent folder is unavailable: %s",
                        candidate_parent.string()));
  }
  if (!PathStartsWith(canonical_parent, canonical_root)) {
    return absl::PermissionDeniedError(
        "Custom object filename resolves outside the configured folder");
  }

  const fs::path target = canonical_parent / normalized.filename();
  std::error_code exists_error;
  const bool target_exists = fs::exists(target, exists_error);
  if (exists_error) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Could not inspect custom object target %s: %s",
                        target.string(), exists_error.message()));
  }
  if (target_exists) {
    const fs::file_status status = fs::symlink_status(target, exists_error);
    if (exists_error || fs::is_symlink(status) ||
        !fs::is_regular_file(status)) {
      return absl::PermissionDeniedError(
          "Custom object target must be a regular non-symlink file");
    }
    const fs::path canonical_target = fs::canonical(target, exists_error);
    if (exists_error || !PathStartsWith(canonical_target, canonical_root)) {
      return absl::PermissionDeniedError(
          "Custom object target resolves outside the configured folder");
    }
    return canonical_target;
  }
  return target;
}

absl::StatusOr<CustomObjectAsset> LoadCustomObjectAsset(
    const std::string& custom_objects_folder, const std::string& filename) {
  fs::path resolved_path;
  ASSIGN_OR_RETURN(resolved_path, ResolveCustomObjectAssetPath(
                                      custom_objects_folder, filename));
  std::vector<uint8_t> source_bytes;
  ASSIGN_OR_RETURN(source_bytes, ReadBinaryFileAtPath(resolved_path));
  CustomObject object;
  ASSIGN_OR_RETURN(object, DecodeCustomObjectBinary(source_bytes));
  return CustomObjectAsset{.object = std::move(object),
                           .source_bytes = std::move(source_bytes),
                           .resolved_path = std::move(resolved_path)};
}

absl::StatusOr<std::vector<uint8_t>> PublishCustomObjectBinary(
    const std::string& custom_objects_folder, const std::string& filename,
    const CustomObject& object,
    const std::vector<uint8_t>& expected_source_bytes,
    const fs::path& expected_resolved_path) {
#if defined(__EMSCRIPTEN__)
  return absl::FailedPreconditionError(
      "Custom object publishing is unavailable in browser builds because "
      "durable atomic project-file publication cannot be guaranteed");
#else
  if (expected_source_bytes.empty()) {
    return absl::FailedPreconditionError(
        "Custom object editing requires an exact source-byte snapshot");
  }
  if (expected_resolved_path.empty()) {
    return absl::FailedPreconditionError(
        "Custom object editing requires its original resolved asset path");
  }
  // Reject a corrupt or mismatched capture before acquiring a write lock.
  RETURN_IF_ERROR(DecodeCustomObjectBinary(expected_source_bytes).status());

  std::vector<uint8_t> binary;
  ASSIGN_OR_RETURN(binary, EncodeCustomObjectBinary(object));
  fs::path target;
  ASSIGN_OR_RETURN(
      target, ResolveCustomObjectAssetPath(custom_objects_folder, filename));
  if (target != expected_resolved_path) {
    return absl::AbortedError(
        "Custom object project or asset path changed after it was opened; "
        "edits were kept");
  }

  std::unique_ptr<core::SourceArtifactPublicationLock> publication_lock;
  ASSIGN_OR_RETURN(publication_lock,
                   core::AcquireSourceArtifactPublicationLock(
                       {target}, kCustomObjectPublisherLabels));

  fs::path locked_target;
  ASSIGN_OR_RETURN(locked_target, ResolveCustomObjectAssetPath(
                                      custom_objects_folder, filename));
  if (locked_target != target) {
    return absl::AbortedError(
        "Custom object path changed while acquiring its publication lock");
  }
  std::vector<uint8_t> source_before;
  ASSIGN_OR_RETURN(source_before, ReadBinaryFileAtPath(locked_target));
  if (source_before != expected_source_bytes) {
    return absl::AbortedError(
        "Custom object source changed after it was opened; edits were kept");
  }

  const std::string before(source_before.begin(), source_before.end());
  const std::string after(binary.begin(), binary.end());
  const std::string expected_sha256 = core::ComputeSourceArtifactSha256(before);
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *publication_lock,
      {{.target = locked_target, .before = before, .after = after}},
      expected_sha256, [&]() -> absl::Status {
        std::vector<uint8_t> reopened;
        ASSIGN_OR_RETURN(reopened, ReadBinaryFileAtPath(locked_target));
        if (reopened != binary) {
          return absl::DataLossError(
              "Published custom object failed exact byte readback");
        }
        CustomObject decoded;
        ASSIGN_OR_RETURN(decoded, DecodeCustomObjectBinary(reopened));
        if (VisibleRuntimeTiles(decoded) != VisibleRuntimeTiles(object)) {
          return absl::DataLossError(
              "Published custom object changed its visible runtime layout");
        }
        return absl::OkStatus();
      }));
  return binary;
#endif
}

uint16_t CustomObjectRuntimeTileWord(int object_id, uint16_t source_word) {
  if (source_word == 0) {
    return 0;
  }

  // Oracle's SpriteObjectsDraw handler forces Kydreeok/Manhandla body tiles
  // into character page 0x300 after checking for a zero/no-op source word.
  if (object_id == 0x54) {
    return source_word | 0x0300;
  }
  return source_word;
}

// These are subtypes of custom object 0x31 itself. The corner-named assets do
// not override standard wall-corner objects 0x100-0x103.
const std::vector<std::string> CustomObjectManager::kSubtype1Filenames = {
    "track_LR.bin",               // 00
    "track_UD.bin",               // 01
    "track_corner_TL.bin",        // 02
    "track_corner_TR.bin",        // 03
    "track_corner_BL.bin",        // 04
    "track_corner_BR.bin",        // 05
    "track_floor_UD.bin",         // 06
    "track_floor_LR.bin",         // 07
    "track_floor_corner_TL.bin",  // 08
    "track_floor_corner_TR.bin",  // 09
    "track_floor_corner_BL.bin",  // 10
    "track_floor_corner_BR.bin",  // 11
    "track_floor_any.bin",        // 12
    "wall_sword_house.bin",       // 13
    "track_any.bin",              // 14
    "small_statue.bin",           // 15
};

const std::vector<std::string> CustomObjectManager::kSubtype2Filenames = {
    "furnace.bin",    // 00
    "firewood.bin",   // 01
    "ice_chair.bin",  // 02
};

const std::vector<std::string> CustomObjectManager::kSubtype54Filenames = {
    "kydreeok_body.bin",      // 00
    "manhandla_body_1a.bin",  // 01
};

CustomObjectManager& CustomObjectManager::Get() {
  static CustomObjectManager instance;
  return instance;
}

CustomObjectManager::CustomObjectManager() {
  standalone_context_.asset_generation = NextAssetGeneration();
}

CustomObjectManager::RuntimeContext& CustomObjectManager::ActiveContext() {
  if (!active_runtime_context_id_.has_value()) {
    return standalone_context_;
  }
  return runtime_contexts_.at(*active_runtime_context_id_);
}

const CustomObjectManager::RuntimeContext& CustomObjectManager::ActiveContext()
    const {
  if (!active_runtime_context_id_.has_value()) {
    return standalone_context_;
  }
  return runtime_contexts_.at(*active_runtime_context_id_);
}

uint64_t CustomObjectManager::NextAssetGeneration() {
  return next_asset_generation_++;
}

void CustomObjectManager::InvalidateCaches() {
  auto& context = ActiveContext();
  context.cache.clear();
  context.asset_generation = NextAssetGeneration();
  ObjectGeometry::Get().ClearCache();
}

void CustomObjectManager::Initialize(const std::string& custom_objects_folder) {
  ActiveContext().state.base_path = custom_objects_folder;
  InvalidateCaches();
#if !defined(NDEBUG)
  LOG_INFO("CustomObjectManager", "Initialize: base_path='%s'",
           GetBasePath().c_str());
  if (const auto* list = ResolveFileList(0x31)) {
    LOG_INFO("CustomObjectManager", "Object 0x31 file list has %zu entries",
             list->size());
  }
#endif
}

void CustomObjectManager::SetObjectFileMap(
    const std::unordered_map<int, std::vector<std::string>>& map) {
  ActiveContext().state.custom_file_map = map;
  InvalidateCaches();
}

void CustomObjectManager::ClearObjectFileMap() {
  ActiveContext().state.custom_file_map.clear();
  InvalidateCaches();
}

bool CustomObjectManager::HasCustomFileMap() const {
  return !ActiveContext().state.custom_file_map.empty();
}

void CustomObjectManager::ActivateRuntimeContext(uint64_t context_id,
                                                 const State& state) {
  auto [context_it, inserted] = runtime_contexts_.try_emplace(context_id);
  auto& context = context_it->second;
  if (inserted || context.state != state) {
    context.state = state;
    context.cache.clear();
    context.asset_generation = NextAssetGeneration();
    ObjectGeometry::Get().ClearCache();
  }
  active_runtime_context_id_ = context_id;
}

void CustomObjectManager::ActivateStandaloneContext() {
  active_runtime_context_id_.reset();
}

void CustomObjectManager::RemoveRuntimeContext(uint64_t context_id) {
  if (active_runtime_context_id_ == context_id) {
    active_runtime_context_id_.reset();
  }
  runtime_contexts_.erase(context_id);
}

const std::vector<std::string>* CustomObjectManager::ResolveFileList(
    int object_id) const {
  const auto& custom_file_map = ActiveContext().state.custom_file_map;
  auto custom_it = custom_file_map.find(object_id);
  if (custom_it != custom_file_map.end()) {
    return &custom_it->second;
  }
  if (object_id == 0x31) {
    return &kSubtype1Filenames;
  }
  if (object_id == 0x32) {
    return &kSubtype2Filenames;
  }
  if (object_id == 0x54) {
    return &kSubtype54Filenames;
  }
  return nullptr;
}

absl::StatusOr<std::shared_ptr<CustomObject>> CustomObjectManager::LoadObject(
    const std::string& filename) {
  auto& cache = ActiveContext().cache;
  if (cache.contains(filename)) {
    return cache[filename];
  }

  auto asset_or = LoadCustomObjectAsset(GetBasePath(), filename);
  if (!asset_or.ok()) {
    LOG_ERROR("CustomObjectManager", "%s",
              asset_or.status().ToString().c_str());
    return asset_or.status();
  }

  auto object_ptr = std::make_shared<CustomObject>(std::move(asset_or->object));
  cache[filename] = object_ptr;

  return object_ptr;
}

absl::StatusOr<std::shared_ptr<CustomObject>>
CustomObjectManager::GetObjectInternal(int object_id, int subtype) {
  const std::vector<std::string>* list = ResolveFileList(object_id);
  if (!list) {
    return absl::NotFoundError("Object ID not mapped to custom object");
  }

  const int runtime_count = RuntimeSubtypeCountForObject(object_id);
  if (subtype < 0 || (runtime_count > 0 && subtype >= runtime_count) ||
      subtype >= static_cast<int>(list->size())) {
    return absl::OutOfRangeError("Subtype index out of range");
  }

  return LoadObject((*list)[subtype]);
}

int CustomObjectManager::GetSubtypeCount(int object_id) const {
  const int runtime_count = RuntimeSubtypeCountForObject(object_id);
  if (runtime_count > 0) {
    return runtime_count;
  }
  if (const auto* list = ResolveFileList(object_id)) {
    return static_cast<int>(list->size());
  }
  return 0;
}

int CustomObjectManager::RuntimeSubtypeCountForObject(int object_id) {
  return static_cast<int>(DefaultSubtypeFilenamesForObject(object_id).size());
}

const std::array<int, 3>& CustomObjectManager::RuntimeObjectIds() {
  static constexpr std::array<int, 3> kRuntimeObjectIds = {0x31, 0x32, 0x54};
  return kRuntimeObjectIds;
}

std::vector<std::string> CustomObjectManager::GetEffectiveFileList(
    int object_id) const {
  const auto* list = ResolveFileList(object_id);
  if (list)
    return *list;
  return {};
}

const std::vector<std::string>&
CustomObjectManager::DefaultSubtypeFilenamesForObject(int object_id) {
  static const std::vector<std::string> kEmpty;
  if (object_id == 0x31) {
    return kSubtype1Filenames;
  }
  if (object_id == 0x32) {
    return kSubtype2Filenames;
  }
  if (object_id == 0x54) {
    return kSubtype54Filenames;
  }
  return kEmpty;
}

void CustomObjectManager::ReloadAll() {
  InvalidateCaches();
}

absl::StatusOr<CustomObjectSlotBinding> CustomObjectManager::ResolveSlotBinding(
    int object_id, int subtype) const {
  const int runtime_count = RuntimeSubtypeCountForObject(object_id);
  if (runtime_count <= 0) {
    return absl::NotFoundError("Object ID has no fixed custom runtime slots");
  }
  if (subtype < 0 || subtype >= runtime_count) {
    return absl::OutOfRangeError("Custom runtime subtype is out of range");
  }

  CustomObjectSlotBinding binding;
  const auto& custom_file_map = ActiveContext().state.custom_file_map;
  const auto mapped_it = custom_file_map.find(object_id);
  if (mapped_it == custom_file_map.end()) {
    const auto& defaults = DefaultSubtypeFilenamesForObject(object_id);
    binding.filename = defaults[static_cast<size_t>(subtype)];
    binding.origin = CustomObjectMappingOrigin::kDefaultFilename;
  } else if (subtype >= static_cast<int>(mapped_it->second.size()) ||
             mapped_it->second[static_cast<size_t>(subtype)].empty()) {
    binding.origin = CustomObjectMappingOrigin::kConfiguredSlotUnmapped;
  } else {
    binding.filename = mapped_it->second[static_cast<size_t>(subtype)];
    binding.origin = CustomObjectMappingOrigin::kConfiguredFilename;
  }
  return binding;
}

std::string CustomObjectManager::ResolveFilename(int object_id,
                                                 int subtype) const {
  if (RuntimeSubtypeCountForObject(object_id) > 0) {
    const auto binding = ResolveSlotBinding(object_id, subtype);
    return binding.ok() ? binding->filename : "";
  }
  const auto* list = ResolveFileList(object_id);
  const int runtime_count = RuntimeSubtypeCountForObject(object_id);
  if (list && subtype >= 0 && (runtime_count == 0 || subtype < runtime_count) &&
      subtype < static_cast<int>(list->size())) {
    return (*list)[subtype];
  }
  return "";
}

CustomObjectManager::State CustomObjectManager::SnapshotState() const {
  return ActiveContext().state;
}

void CustomObjectManager::RestoreState(const State& state) {
  ActiveContext().state = state;
  InvalidateCaches();
}

const std::string& CustomObjectManager::GetBasePath() const {
  return ActiveContext().state.base_path;
}

uint64_t CustomObjectManager::asset_generation() const {
  return ActiveContext().asset_generation;
}

}  // namespace zelda3
}  // namespace yaze
