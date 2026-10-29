#include "app/editor/graphics/catalog_tile_transfer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "app/editor/graphics/graphics_editor_state.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/registry/undo_manager.h"
#include "app/gfx/types/snes_tile.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {
namespace {

constexpr size_t kPixels = 128 * 32;
constexpr size_t kMaxCatalogBytes = 4 * 1024 * 1024;
constexpr size_t kMaxSources = 4096;
constexpr int kMaxAssetPages = 256;
constexpr int kMaxAssetDimension = 2048;
constexpr size_t kMaxAssetPixels = 1024 * 1024;

absl::Status ValidatePixels(const std::vector<uint8_t>& pixels) {
  if (pixels.size() != kPixels ||
      std::any_of(pixels.begin(), pixels.end(),
                  [](uint8_t pixel) { return pixel > 7; })) {
    return absl::InvalidArgumentError(
        "Catalog transfer requires 4096 pixels with indices 0-7");
  }
  return absl::OkStatus();
}

absl::Status ValidateDestination(uint16_t sheet) {
  if (sheet >= zelda3::kGfxSheetCount) {
    return absl::InvalidArgumentError("Destination sheet is out of range");
  }
  if (zelda3::GetGfxSheetStorageKind(sheet) ==
      zelda3::GfxSheetStorageKind::kCompressed2bpp) {
    return absl::InvalidArgumentError(
        "Catalog transfer does not support 2bpp destination sheets");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> ReadFile(const std::filesystem::path& path,
                                     bool sheet = false,
                                     size_t maximum = 64 * 1024 * 1024) {
  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  if (error || !std::filesystem::exists(status)) {
    if (error == std::errc::permission_denied)
      return absl::PermissionDeniedError("Cannot inspect catalog asset: " +
                                         path.string());
    return absl::NotFoundError("Catalog asset is missing: " + path.string());
  }
  if (!std::filesystem::is_regular_file(status))
    return absl::FailedPreconditionError("Catalog asset is not a file: " +
                                         path.string());
  constexpr auto readable = std::filesystem::perms::owner_read |
                            std::filesystem::perms::group_read |
                            std::filesystem::perms::others_read;
  if ((status.permissions() & readable) == std::filesystem::perms::none)
    return absl::PermissionDeniedError("Catalog asset is unreadable: " +
                                       path.string());
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    return absl::PermissionDeniedError("Cannot inspect catalog asset size: " +
                                       path.string());
  }
  if ((sheet && size != 1536) || size > maximum) {
    return absl::InvalidArgumentError("Invalid catalog asset size: " +
                                      path.string());
  }
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return absl::PermissionDeniedError("Cannot open catalog asset: " +
                                       path.string());
  std::string bytes(static_cast<size_t>(size), '\0');
  if ((!bytes.empty() && !input.read(bytes.data(), bytes.size())) ||
      input.peek() != std::char_traits<char>::eof()) {
    return absl::DataLossError("Cannot read complete catalog asset: " +
                               path.string());
  }
  return bytes;
}

absl::Status CheckHash(const std::string& bytes, std::string expected) {
  if (expected.size() != 64 ||
      !std::all_of(expected.begin(), expected.end(),
                   [](unsigned char c) { return std::isxdigit(c); })) {
    return absl::InvalidArgumentError("Catalog SHA256 must have 64 hex digits");
  }
  std::transform(expected.begin(), expected.end(), expected.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (core::ComputeSourceArtifactSha256(bytes) != expected) {
    return absl::DataLossError("Catalog asset SHA256 mismatch");
  }
  return absl::OkStatus();
}

size_t PixelOffset(int tile, int x, int y) {
  return ((tile / 16) * 8 + y) * 128 + (tile % 16) * 8 + x;
}

using Json = nlohmann::json;

absl::StatusOr<std::string> Text(const Json& value) {
  if (!value.is_string() || value.get_ref<const std::string&>().empty() ||
      value.get_ref<const std::string&>().size() > 4096 ||
      value.get_ref<const std::string&>().find('\0') != std::string::npos)
    return absl::InvalidArgumentError("Invalid catalog text field");
  return value.get<std::string>();
}

absl::StatusOr<int> Integer(const Json& value, int minimum, int maximum) {
  if (!value.is_number_integer() || value < minimum || value > maximum)
    return absl::InvalidArgumentError("Catalog integer is outside its range");
  return value.get<int>();
}

// Research catalogs carry optional notes of varying shape; only usable text
// is retained.
std::string OptionalText(const Json& reference, const char* key) {
  if (!reference.contains(key))
    return {};
  auto text = Text(reference.at(key));
  return text.ok() ? *std::move(text) : std::string();
}

CatalogResourceKind ResolveResourceKind(const Json& reference,
                                        const std::string& category) {
  static constexpr std::pair<const char*, CatalogResourceKind> kNames[] = {
      {"overworld-tile", CatalogResourceKind::kOverworldTile},
      {"dungeon-tile", CatalogResourceKind::kDungeonTile},
      {"interior-tile", CatalogResourceKind::kInteriorTile},
      {"sprite-pose", CatalogResourceKind::kSpritePose},
      {"other-sheet", CatalogResourceKind::kOtherSheet}};
  static constexpr std::pair<const char*, CatalogResourceKind> kCategories[] = {
      {"overworld", CatalogResourceKind::kOverworldTile},
      {"dungeon", CatalogResourceKind::kDungeonTile},
      {"interior", CatalogResourceKind::kInteriorTile},
      {"sprite", CatalogResourceKind::kSpritePose}};
  const auto explicit_kind = OptionalText(reference, "resource_kind");
  for (const auto& [name, kind] : kNames)
    if (explicit_kind == name)
      return kind;
  for (const auto& [name, kind] : kCategories)
    if (category == name)
      return kind;
  return CatalogResourceKind::kOtherSheet;
}

absl::Status CheckResourcesUnbound(const Json& reference) {
  if (!reference.contains("resources_bound") ||
      reference.at("resources_bound").is_null())
    return absl::OkStatus();
  if (!reference.at("resources_bound").is_boolean())
    return absl::InvalidArgumentError(
        "Catalog resources_bound must be a boolean");
  if (reference.at("resources_bound").get<bool>())
    return absl::FailedPreconditionError(
        "Catalog art resources must remain unbound");
  return absl::OkStatus();
}

void ReadResearchProvenance(const Json& reference, CatalogSheetSource& source) {
  const std::pair<const char*, std::string*> fields[] = {
      {"notes", &source.notes},
      {"conversion", &source.conversion},
      {"behavior_status", &source.behavior_status},
      {"provenance_status", &source.provenance_status},
      {"identification_confidence", &source.identification_confidence},
      {"source_url", &source.source_url}};
  for (const auto& [key, target] : fields)
    *target = OptionalText(reference, key);
  source.color_reduction = reference.contains("color_reduction") &&
                           reference.at("color_reduction").is_boolean() &&
                           reference.at("color_reduction").get<bool>();
  source.resource_kind = ResolveResourceKind(reference, source.category);
}

absl::StatusOr<std::filesystem::path> ContainedPath(
    const std::filesystem::path& root, const std::string& name) {
  namespace fs = std::filesystem;
  const fs::path configured(name);
  if (name.empty() || name.find('\0') != std::string::npos ||
      (!configured.is_absolute() && (name.find('\\') != std::string::npos ||
                                     name.find(':') != std::string::npos)))
    return absl::InvalidArgumentError("Invalid catalog payload path");
  for (const auto& part : configured)
    if (part == "..")
      return absl::InvalidArgumentError(
          "Catalog payload path contains traversal");
  const auto candidate = (root / configured).lexically_normal();
  const auto lexical_mismatch = std::mismatch(
      root.begin(), root.end(), candidate.begin(), candidate.end());
  if (lexical_mismatch.first != root.end())
    return absl::InvalidArgumentError("Catalog payload escapes its asset root");
  std::error_code error;
  const auto path = fs::canonical(candidate, error);
  if (error) {
    if (error == std::errc::permission_denied)
      return absl::PermissionDeniedError("Cannot resolve catalog payload: " +
                                         name);
    return absl::NotFoundError("Catalog payload is missing: " + name);
  }
  const auto mismatch =
      std::mismatch(root.begin(), root.end(), path.begin(), path.end());
  if (mismatch.first != root.end())
    return absl::InvalidArgumentError("Catalog payload escapes its asset root");
  return path;
}

absl::Status ValidatePage(const CatalogSheetSource& source) {
  if ((source.bpp != 3 && source.bpp != 4) ||
      source.indexed_pixels.size() != kPixels ||
      std::any_of(source.indexed_pixels.begin(), source.indexed_pixels.end(),
                  [&](uint8_t pixel) { return pixel >= (1 << source.bpp); }))
    return absl::InvalidArgumentError("Invalid indexed catalog page/depth");
  if (!source.palette.empty() &&
      (source.palette.size() != static_cast<size_t>(1 << source.bpp) ||
       std::any_of(source.palette.begin(), source.palette.end(),
                   [](uint16_t color) { return color > 0x7fff; })))
    return absl::InvalidArgumentError("Invalid catalog page palette");
  return absl::OkStatus();
}

absl::Status ValidateAssembly(const CatalogSheetSource& source) {
  if (source.page_count < 1 || source.page_count > kMaxAssetPages ||
      source.pixel_width < 8 || source.pixel_width > kMaxAssetDimension ||
      source.pixel_height < 8 || source.pixel_height > kMaxAssetDimension ||
      source.pixel_width % 8 || source.pixel_height % 8 ||
      static_cast<size_t>(source.pixel_width) * source.pixel_height >
          kMaxAssetPixels)
    return absl::InvalidArgumentError("Invalid catalog asset dimensions/pages");
  if (source.assembly.empty()) {
    if (source.page_count != 1 || source.pixel_width != 128 ||
        source.pixel_height != 32)
      return absl::InvalidArgumentError(
          "Logical catalog asset requires assembly");
    return absl::OkStatus();
  }
  if (source.assembly.size() > kMaxAssetPixels / 64)
    return absl::InvalidArgumentError("Catalog assembly exceeds cell limit");
  std::set<std::pair<int, int>> positions;
  for (const auto& cell : source.assembly) {
    if (cell.x < 0 || cell.x >= source.pixel_width / 8 || cell.y < 0 ||
        cell.y >= source.pixel_height / 8 || cell.page < 0 ||
        cell.page >= source.page_count || cell.page_tile < 0 ||
        cell.page_tile >= 64 || !positions.insert({cell.x, cell.y}).second)
      return absl::InvalidArgumentError(
          "Invalid or duplicate catalog assembly cell");
  }
  return absl::OkStatus();
}

// The suffix never selects an encoding; the reference bpp does.
absl::Status CheckPlanarPage(const Json& page,
                             const std::filesystem::path& root,
                             CatalogSheetSource& source) {
  ASSIGN_OR_RETURN(auto name, Text(page.at("binary_path")));
  if (!page.contains("binary_sha256"))
    return absl::InvalidArgumentError(
        "Catalog planar page requires binary_sha256");
  ASSIGN_OR_RETURN(auto hash, Text(page.at("binary_sha256")));
  ASSIGN_OR_RETURN(auto path, ContainedPath(root, name));
  ASSIGN_OR_RETURN(auto bytes, ReadFile(path, false, kMaxCatalogBytes));
  RETURN_IF_ERROR(CheckHash(bytes, hash));
  const int bpp = source.bpp;
  if (bytes.size() != static_cast<size_t>(64 * 8 * bpp))
    return absl::InvalidArgumentError(
        "Catalog planar page must hold 64 tiles at " + std::to_string(bpp) +
        "bpp: " + path.string());
  const std::vector<uint8_t> planar(bytes.begin(), bytes.end());
  for (int tile = 0; tile < 64; ++tile) {
    const auto decoded = gfx::UnpackBppTile(planar, tile * 8 * bpp, bpp);
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        if (decoded.data[y * 8 + x] !=
            source.indexed_pixels[PixelOffset(tile, x, y)])
          return absl::DataLossError(
              "Catalog planar page differs from its indexed PNG at " +
              std::to_string(bpp) + "bpp: " + path.string());
  }
  source.binary_path = path.string();
  source.binary_sha256 = core::ComputeSourceArtifactSha256(bytes);
  return absl::OkStatus();
}

absl::Status LoadIndexedReference(const Json& reference,
                                  const std::filesystem::path& root,
                                  const std::string& id,
                                  const std::string& title,
                                  std::vector<CatalogSheetSource>& sources) {
  CatalogSheetSource base;
  base.asset_id = id;
  base.name = title;
  base.source_kind = "indexed-png";
  const std::pair<const char*, std::string*> fields[] = {
      {"group", &base.group},
      {"name", &base.name},
      {"category", &base.category},
      {"material", &base.material},
      {"game", &base.game},
      {"credit", &base.credit},
      {"source_origin", &base.source_origin},
      {"source_sha256", &base.source_sha256}};
  for (const auto& [key, target] : fields)
    if (reference.contains(key)) {
      ASSIGN_OR_RETURN(*target, Text(reference.at(key)));
    }
  if (!base.source_sha256.empty()) {
    // Validate provenance syntax without opening an external source hint.
    if (base.source_sha256.size() != 64 ||
        !std::all_of(base.source_sha256.begin(), base.source_sha256.end(),
                     [](unsigned char c) { return std::isxdigit(c); }))
      return absl::InvalidArgumentError("Invalid catalog source SHA256");
  }
  if (reference.contains("bpp")) {
    ASSIGN_OR_RETURN(base.bpp, Integer(reference.at("bpp"), 3, 4));
  }
  RETURN_IF_ERROR(CheckResourcesUnbound(reference));
  ReadResearchProvenance(reference, base);
  if (reference.contains("physical_binding") &&
      !reference.at("physical_binding").is_null() &&
      reference.at("physical_binding") != "UNBOUND")
    return absl::FailedPreconditionError(
        "Catalog art resources must remain UNBOUND");
  if (reference.contains("palette_bgr555")) {
    const auto& palette = reference.at("palette_bgr555");
    if (!palette.is_array() || palette.size() != size_t(1 << base.bpp))
      return absl::InvalidArgumentError(
          "Catalog palette does not match its depth");
    for (const auto& color : palette) {
      ASSIGN_OR_RETURN(int word, Integer(color, 0, 0x7fff));
      base.palette.push_back(static_cast<uint16_t>(word));
    }
  }
  Json pages;
  if (reference.contains("pages")) {
    pages = reference.at("pages");
    if (!pages.is_array() || pages.empty() || pages.size() > kMaxAssetPages)
      return absl::InvalidArgumentError("Catalog requires 1-256 indexed pages");
  } else {
    pages = Json::array({{{"path", reference.at("indexed_path")},
                          {"sha256", reference.at("indexed_sha256")}}});
  }
  base.page_count = static_cast<int>(pages.size());
  if (sources.size() + pages.size() > kMaxSources)
    return absl::ResourceExhaustedError("Catalog exceeds source page limit");
  if (reference.contains("pixel_size")) {
    const auto& size = reference.at("pixel_size");
    if (!size.is_array() || size.size() != 2)
      return absl::InvalidArgumentError(
          "Catalog pixel_size requires width/height");
    ASSIGN_OR_RETURN(base.pixel_width, Integer(size[0], 8, kMaxAssetDimension));
    ASSIGN_OR_RETURN(base.pixel_height,
                     Integer(size[1], 8, kMaxAssetDimension));
  }
  if (reference.contains("assembly")) {
    const auto& assembly = reference.at("assembly");
    if (!assembly.is_array() || assembly.size() > kMaxAssetPixels / 64)
      return absl::InvalidArgumentError("Invalid catalog assembly array");
    for (const auto& item : assembly) {
      CatalogAssemblyCell cell;
      ASSIGN_OR_RETURN(cell.x,
                       Integer(item.at("x"), 0, kMaxAssetDimension / 8));
      ASSIGN_OR_RETURN(cell.y,
                       Integer(item.at("y"), 0, kMaxAssetDimension / 8));
      ASSIGN_OR_RETURN(cell.page,
                       Integer(item.at("page"), 0, kMaxAssetPages - 1));
      ASSIGN_OR_RETURN(cell.page_tile, Integer(item.at("page_tile"), 0, 63));
      for (const auto& [key, target] :
           std::vector<std::pair<const char*, bool*>>{{"h", &cell.h},
                                                      {"v", &cell.v}}) {
        if (item.contains(key)) {
          if (!item.at(key).is_boolean())
            return absl::InvalidArgumentError("Catalog flips must be booleans");
          *target = item.at(key).get<bool>();
        }
      }
      base.assembly.push_back(cell);
    }
  }
  RETURN_IF_ERROR(ValidateAssembly(base));
  size_t retained_cells = base.assembly.size() * pages.size();
  for (const auto& source : sources)
    retained_cells += source.assembly.size();
  if (retained_cells > 256 * 1024)
    return absl::ResourceExhaustedError(
        "Catalog exceeds retained assembly cell limit");
  if (reference.contains("preview_path")) {
    ASSIGN_OR_RETURN(auto name, Text(reference.at("preview_path")));
    ASSIGN_OR_RETURN(auto path, ContainedPath(root, name));
    ASSIGN_OR_RETURN(auto bytes, ReadFile(path, false, kMaxCatalogBytes));
    if (reference.contains("preview_sha256")) {
      ASSIGN_OR_RETURN(auto hash, Text(reference.at("preview_sha256")));
      RETURN_IF_ERROR(CheckHash(bytes, hash));
    }
    ASSIGN_OR_RETURN(auto png, util::DecodePng(std::vector<uint8_t>(
                                   bytes.begin(), bytes.end())));
    if (static_cast<size_t>(png.width) * png.height > kMaxAssetPixels)
      return absl::InvalidArgumentError(
          "Catalog source preview exceeds pixel limit");
    base.preview_path = path.string();
  }
  std::vector<CatalogSheetSource> loaded;
  for (size_t page = 0; page < pages.size(); ++page) {
    ASSIGN_OR_RETURN(auto name, Text(pages[page].at("path")));
    ASSIGN_OR_RETURN(auto hash, Text(pages[page].at("sha256")));
    ASSIGN_OR_RETURN(auto path, ContainedPath(root, name));
    ASSIGN_OR_RETURN(auto bytes, ReadFile(path, false, kMaxCatalogBytes));
    RETURN_IF_ERROR(CheckHash(bytes, hash));
    ASSIGN_OR_RETURN(auto png, util::DecodePng(std::vector<uint8_t>(
                                   bytes.begin(), bytes.end())));
    if (!png.indexed || png.width != 128 || png.height != 32)
      return absl::InvalidArgumentError(
          "Catalog PNG page must be indexed 128x32");
    if (std::any_of(png.indices.begin(), png.indices.end(),
                    [&](uint8_t index) { return index >= png.palette.size(); }))
      return absl::InvalidArgumentError(
          "Catalog PNG pixel references an undefined palette entry");
    if (std::find(png.indices.begin(), png.indices.end(), 0) !=
            png.indices.end() &&
        png.palette[0][3] != 0)
      return absl::InvalidArgumentError(
          "Catalog PNG index0 must be transparent when used");
    CatalogSheetSource source = base;
    source.id =
        base.page_count == 1 ? id : id + ".page-" + std::to_string(page);
    source.title = base.page_count == 1
                       ? title
                       : title + " (page " + std::to_string(page + 1) + "/" +
                             std::to_string(base.page_count) + ")";
    source.source_path = path.string();
    source.page_sha256 = core::ComputeSourceArtifactSha256(bytes);
    source.page = static_cast<int>(page);
    source.indexed_pixels = std::move(png.indices);
    std::vector<uint16_t> png_palette(size_t(1 << source.bpp), 0);
    for (size_t i = 0; i < png_palette.size() && i < png.palette.size(); ++i) {
      const auto& color = png.palette[i];
      if ((i != 0 && color[3] != 255) ||
          (i == 0 && color[3] != 0 && color[3] != 255))
        return absl::InvalidArgumentError(
            "Catalog palette alpha must be binary index0 transparency");
      png_palette[i] = uint16_t((color[0] >> 3) | ((color[1] >> 3) << 5) |
                                ((color[2] >> 3) << 10));
    }
    if (!base.palette.empty() && base.palette != png_palette)
      return absl::InvalidArgumentError(
          "Catalog PNG palette differs from declared BGR555 palette");
    source.palette = std::move(png_palette);
    if (source.source_sha256.empty())
      source.source_sha256 = source.page_sha256;
    RETURN_IF_ERROR(ValidatePage(source));
    if (pages[page].contains("binary_path"))
      RETURN_IF_ERROR(CheckPlanarPage(pages[page], root, source));
    if (!loaded.empty() && loaded.front().palette != source.palette)
      return absl::InvalidArgumentError(
          "Catalog assembly pages use different palettes");
    loaded.push_back(std::move(source));
  }
  sources.insert(sources.end(), std::make_move_iterator(loaded.begin()),
                 std::make_move_iterator(loaded.end()));
  return absl::OkStatus();
}

bool ContainsFolded(std::string value, std::string needle) {
  const auto lower = [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  };
  std::transform(value.begin(), value.end(), value.begin(), lower);
  std::transform(needle.begin(), needle.end(), needle.begin(), lower);
  return value.find(needle) != std::string::npos;
}

}  // namespace

const char* CatalogResourceKindName(CatalogResourceKind kind) {
  switch (kind) {
    case CatalogResourceKind::kOverworldTile:
      return "Overworld tile motif";
    case CatalogResourceKind::kDungeonTile:
      return "Dungeon tile motif";
    case CatalogResourceKind::kInteriorTile:
      return "Interior tile motif";
    case CatalogResourceKind::kSpritePose:
      return "Sprite family poses";
    case CatalogResourceKind::kOtherSheet:
      break;
  }
  return "Other source sheet";
}

bool CatalogSourceMatches(const CatalogSheetSource& source,
                          const CatalogSourceFilter& filter) {
  if (filter.kind && source.resource_kind != *filter.kind)
    return false;
  if (filter.palettes_only && source.palette.empty())
    return false;
  if (!filter.group.empty() && source.group != filter.group)
    return false;
  if (!filter.category.empty() && source.category != filter.category)
    return false;
  return ContainsFolded(source.title + " " + source.name + " " + source.id +
                            " " + source.material + " " + source.game + " " +
                            source.group + " " + source.category + " " +
                            source.credit + " " + source.notes,
                        filter.search);
}

absl::Status ValidateCatalogSourceForRomSheet(
    const CatalogSheetSource& source) {
  if (source.bpp != 3)
    return absl::FailedPreconditionError(
        "This " + std::to_string(source.bpp) +
        "bpp page cannot be written into a 3bpp ROM sheet; IDs and palette "
        "rows do not convert implicitly");
  return absl::OkStatus();
}

absl::StatusOr<std::vector<CatalogSheetSource>> LoadCatalogSheetSources(
    const std::string& filename) {
  if (filename.empty() || filename.find('\0') != std::string::npos)
    return absl::InvalidArgumentError("Select a catalog file");
  ASSIGN_OR_RETURN(auto bytes, ReadFile(filename, false, kMaxCatalogBytes));
  try {
    const auto catalog = nlohmann::json::parse(bytes);
    if (!catalog.is_object() || !catalog.contains("schema_version") ||
        !catalog.at("schema_version").is_number_integer() ||
        catalog.at("schema_version") != 1 || !catalog.contains("references") ||
        !catalog.at("references").is_array() ||
        catalog.at("references").size() > kMaxSources) {
      return absl::InvalidArgumentError("Expected schema_version 1 references");
    }
    const auto parent = std::filesystem::absolute(filename).parent_path();
    const auto canonical_parent = std::filesystem::canonical(parent);
    auto asset_root = canonical_parent;
    if (catalog.contains("asset_root")) {
      ASSIGN_OR_RETURN(auto name, Text(catalog.at("asset_root")));
      ASSIGN_OR_RETURN(asset_root, ContainedPath(canonical_parent, name));
      if (!std::filesystem::is_directory(asset_root))
        return absl::InvalidArgumentError(
            "Catalog asset_root is not a directory");
    }
    auto resolve = [&parent](const std::string& path) {
      return (parent / std::filesystem::path(path)).lexically_normal();
    };
    std::vector<CatalogSheetSource> sources;
    std::set<std::string> ids;
    // Hash each distinct source only once, but check every reference's hash.
    std::map<std::filesystem::path, std::string> source_hashes;
    for (const auto& reference : catalog.at("references")) {
      if (!reference.is_object()) {
        continue;
      }
      if (reference.contains("role") &&
          reference.at("role") != "source-sheet") {
        continue;
      }
      const bool indexed =
          reference.contains("pages") || reference.contains("indexed_path");
      if (reference.contains("source_kind") &&
          reference.at("source_kind") == "indexed-png" && !indexed)
        return absl::InvalidArgumentError(
            "Indexed catalog reference has no pages");
      if (!reference.contains("raw_path") && !indexed)
        continue;
      if (reference.contains("raw_path") && indexed)
        return absl::InvalidArgumentError(
            "Catalog reference has ambiguous payloads");
      ASSIGN_OR_RETURN(auto id, Text(reference.at("id")));
      ASSIGN_OR_RETURN(
          auto title, Text(reference.contains("title") ? reference.at("title")
                                                       : reference.at("name")));
      if (!ids.insert(id).second)
        return absl::InvalidArgumentError("Duplicate catalog asset ID");
      if (indexed) {
        if (!reference.contains("source_kind") ||
            reference.at("source_kind") != "indexed-png")
          return absl::InvalidArgumentError(
              "Indexed catalog requires source_kind indexed-png");
        RETURN_IF_ERROR(
            LoadIndexedReference(reference, asset_root, id, title, sources));
        continue;
      }
      if ((reference.contains("bpp") && reference.at("bpp") != 3) ||
          (reference.contains("source_kind") &&
           reference.at("source_kind") != "snes-3bpp"))
        return absl::InvalidArgumentError(
            "Raw catalog sheets require SNES 3bpp");
      const auto raw_name = reference.at("raw_path").get<std::string>();
      const auto source_name =
          reference.contains("source_rom")
              ? reference.at("source_rom").get<std::string>()
              : reference.at("local_hint").get<std::string>();
      if (raw_name.empty() || source_name.empty() ||
          raw_name.find('\0') != std::string::npos ||
          source_name.find('\0') != std::string::npos) {
        return absl::InvalidArgumentError(
            "Malformed catalog sheet identity/path");
      }
      ASSIGN_OR_RETURN(auto raw, ReadFile(resolve(raw_name), /*sheet=*/true));
      RETURN_IF_ERROR(
          CheckHash(raw, reference.at("decoded_sha256").get<std::string>()));
      const auto source_path = resolve(source_name);
      if (!source_hashes.contains(source_path)) {
        ASSIGN_OR_RETURN(auto rom, ReadFile(source_path));
        source_hashes.emplace(source_path,
                              core::ComputeSourceArtifactSha256(rom));
      }
      auto expected = reference.at("source_sha256").get<std::string>();
      if (expected.size() != 64 ||
          !std::all_of(expected.begin(), expected.end(),
                       [](unsigned char c) { return std::isxdigit(c); })) {
        return absl::InvalidArgumentError("Invalid catalog source SHA256");
      }
      std::transform(expected.begin(), expected.end(), expected.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (source_hashes.at(source_path) != expected) {
        return absl::DataLossError("Catalog source ROM SHA256 mismatch: " +
                                   source_path.string());
      }
      const std::vector<uint8_t> native(raw.begin(), raw.end());
      auto pixels = gfx::SnesTo8bppSheet(native, /*bpp=*/3);
      RETURN_IF_ERROR(ValidatePixels(pixels));
      CatalogSheetSource source{id, title, source_path.string(),
                                std::move(pixels)};
      source.asset_id = id;
      source.name = title;
      source.source_kind = "snes-3bpp";
      source.source_sha256 = expected;
      source.page_sha256 = core::ComputeSourceArtifactSha256(raw);
      const std::pair<const char*, std::string*> metadata[] = {
          {"group", &source.group},
          {"name", &source.name},
          {"category", &source.category},
          {"material", &source.material},
          {"game", &source.game},
          {"credit", &source.credit},
          {"source_origin", &source.source_origin}};
      for (const auto& [key, target] : metadata)
        if (reference.contains(key)) {
          ASSIGN_OR_RETURN(*target, Text(reference.at(key)));
        }
      RETURN_IF_ERROR(CheckResourcesUnbound(reference));
      ReadResearchProvenance(reference, source);
      if (reference.contains("palette_bgr555")) {
        const auto& palette = reference.at("palette_bgr555");
        if (!palette.is_array() || palette.size() != 8)
          return absl::InvalidArgumentError(
              "Raw catalog palette requires eight BGR555 colors");
        for (const auto& color : palette) {
          ASSIGN_OR_RETURN(int word, Integer(color, 0, 0x7fff));
          source.palette.push_back(static_cast<uint16_t>(word));
        }
      }
      sources.push_back(std::move(source));
      if (sources.size() > kMaxSources)
        return absl::ResourceExhaustedError(
            "Catalog exceeds source page limit");
    }
    std::set<std::string> page_ids;
    for (const auto& source : sources)
      if (!page_ids.insert(source.id).second)
        return absl::InvalidArgumentError("Duplicate catalog source page ID");
    return sources;
  } catch (const nlohmann::json::exception& error) {
    return absl::InvalidArgumentError(std::string("Malformed catalog sheet: ") +
                                      error.what());
  } catch (const std::filesystem::filesystem_error& error) {
    return absl::InvalidArgumentError(error.what());
  }
}

absl::StatusOr<CatalogAssetPreview> AssembleCatalogAsset(
    const std::vector<CatalogSheetSource>& sources,
    const std::string& asset_id) {
  if (asset_id.empty())
    return absl::InvalidArgumentError("Select a catalog asset ID");
  std::vector<const CatalogSheetSource*> selected;
  for (const auto& source : sources)
    if ((source.asset_id.empty() ? source.id : source.asset_id) == asset_id)
      selected.push_back(&source);
  if (selected.empty())
    return absl::NotFoundError("Catalog asset was not found");
  const auto& first = *selected.front();
  RETURN_IF_ERROR(ValidateAssembly(first));
  if (selected.size() != static_cast<size_t>(first.page_count))
    return absl::InvalidArgumentError("Catalog assembly is missing a page");
  std::vector<const CatalogSheetSource*> pages(first.page_count, nullptr);
  for (const auto* source : selected) {
    RETURN_IF_ERROR(ValidatePage(*source));
    if (source->page < 0 || source->page >= first.page_count ||
        pages[source->page] || source->page_count != first.page_count ||
        source->bpp != first.bpp || source->pixel_width != first.pixel_width ||
        source->pixel_height != first.pixel_height ||
        source->assembly != first.assembly ||
        source->palette != first.palette || source->resources_bound)
      return absl::InvalidArgumentError(
          "Catalog assembly page metadata differs");
    pages[source->page] = source;
  }
  CatalogAssetPreview preview;
  preview.width = first.pixel_width;
  preview.height = first.pixel_height;
  preview.bpp = first.bpp;
  preview.palette = first.palette;
  if (first.assembly.empty()) {
    preview.indexed_pixels = first.indexed_pixels;
    return preview;
  }
  preview.indexed_pixels.assign(
      static_cast<size_t>(preview.width) * preview.height, 0);
  for (const auto& cell : first.assembly) {
    const auto& pixels = pages[cell.page]->indexed_pixels;
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        preview
            .indexed_pixels[(cell.y * 8 + y) * preview.width + cell.x * 8 + x] =
            pixels[PixelOffset(cell.page_tile, cell.h ? 7 - x : x,
                               cell.v ? 7 - y : y)];
  }
  return preview;
}

absl::StatusOr<CatalogTileTransferPlan> PreviewCatalogTileTransfer(
    uint16_t destination_sheet, const std::vector<uint8_t>& destination_pixels,
    const std::vector<uint8_t>& source_pixels,
    const std::vector<int>& source_tiles, int first_destination_tile,
    bool preserve_layout) {
  RETURN_IF_ERROR(ValidateDestination(destination_sheet));
  RETURN_IF_ERROR(ValidatePixels(destination_pixels));
  RETURN_IF_ERROR(ValidatePixels(source_pixels));
  if (source_tiles.empty() || first_destination_tile < 0 ||
      first_destination_tile >= 64 ||
      source_tiles.size() > static_cast<size_t>(64 - first_destination_tile)) {
    return absl::InvalidArgumentError("Catalog tile selection exceeds sheet");
  }
  int min_x = 15, min_y = 3, max_x = 0, max_y = 0;
  for (int source : source_tiles) {
    if (source < 0 || source >= 64)
      return absl::InvalidArgumentError("Catalog source tile is out of range");
    min_x = std::min(min_x, source % 16);
    max_x = std::max(max_x, source % 16);
    min_y = std::min(min_y, source / 16);
    max_y = std::max(max_y, source / 16);
  }
  if (preserve_layout && (first_destination_tile % 16 + max_x - min_x >= 16 ||
                          first_destination_tile / 16 + max_y - min_y >= 4)) {
    return absl::InvalidArgumentError(
        "Selected tile layout exceeds destination sheet");
  }
  std::array<bool, 64> seen{};
  CatalogTileTransferPlan plan{
      destination_sheet, destination_pixels, destination_pixels, {}};
  int destination = first_destination_tile;
  for (int source : source_tiles) {
    if (source < 0 || source >= 64 || seen[source]) {
      return absl::InvalidArgumentError(
          "Invalid or duplicate catalog source tile");
    }
    seen[source] = true;
    if (preserve_layout) {
      destination = first_destination_tile + (source / 16 - min_y) * 16 +
                    source % 16 - min_x;
    }
    bool changed = false;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto value = source_pixels[PixelOffset(source, x, y)];
        const auto offset = PixelOffset(destination, x, y);
        changed |= plan.after_pixels[offset] != value;
        plan.after_pixels[offset] = value;
      }
    }
    if (changed)
      plan.changed_tiles.push_back(destination);
    ++destination;
  }
  return plan;
}

absl::Status ApplyCatalogTileTransfer(const CatalogTileTransferPlan& plan,
                                      GraphicsEditorState& state,
                                      UndoManager* undo_manager) {
  if (undo_manager == nullptr) {
    return absl::FailedPreconditionError(
        "Catalog transfer requires undo history");
  }
  if (!plan.destination_owner || plan.destination_owner != state.game_data ||
      state.current_sheet_id != plan.sheet_id || state.selection.is_floating) {
    return absl::FailedPreconditionError(
        "Catalog destination owner/sheet changed or selection is floating");
  }
  RETURN_IF_ERROR(ValidateDestination(plan.sheet_id));
  RETURN_IF_ERROR(ValidatePixels(plan.before_pixels));
  RETURN_IF_ERROR(ValidatePixels(plan.after_pixels));
  const auto* store = ActiveSheetStore(state);
  if (!store || !store->HasSheet(plan.sheet_id)) {
    return absl::FailedPreconditionError(
        "Catalog destination session is inactive");
  }
  if (plan.destination_revision == 0 ||
      plan.destination_revision != store->Revision(plan.sheet_id)) {
    return absl::FailedPreconditionError(
        "Catalog destination revision changed; preview again");
  }
  const auto current = store->Sheet(plan.sheet_id);
  if (!std::equal(current.begin(), current.end(), plan.before_pixels.begin())) {
    return absl::FailedPreconditionError(
        "Catalog destination changed after preview; preview again");
  }
  auto diff = MakeSheetPixelDiff(plan.sheet_id, current, plan.after_pixels);
  if (diff.empty())
    return absl::OkStatus();
  auto action = std::make_unique<GraphicsPixelEditAction>(
      &state, std::move(diff), "Copy catalog tiles");
  RETURN_IF_ERROR(CommitSheetPixels(state, plan.sheet_id, plan.after_pixels));
  undo_manager->Push(std::move(action));
  return absl::OkStatus();
}

}  // namespace yaze::editor
