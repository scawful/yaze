#include "app/editor/graphics/graphics_scratch_storage.h"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze::editor::graphics_scratch_storage {
namespace {

constexpr uint64_t kMaxBinContainerBytes = 64 * 1024 * 1024;

absl::StatusOr<std::filesystem::path> ResolveBinPath(const std::string& path) {
  if (path.empty() || path.find('\0') != std::string::npos)
    return absl::InvalidArgumentError("Select a BIN path");
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  if (ec)
    return absl::InvalidArgumentError("Cannot resolve BIN path: " +
                                      ec.message());
  auto resolved = std::filesystem::weakly_canonical(absolute, ec);
  if (ec)
    return absl::InvalidArgumentError("Cannot resolve BIN path: " +
                                      ec.message());
  return resolved;
}

bool PathsAlias(const std::filesystem::path& a,
                const std::filesystem::path& b) {
  std::error_code ec;
  if (std::filesystem::equivalent(a, b, ec) && !ec)
    return true;
  ec.clear();
  const auto first = std::filesystem::weakly_canonical(a, ec);
  if (ec)
    return false;
  const auto second = std::filesystem::weakly_canonical(b, ec);
  return !ec && first == second;
}

absl::StatusOr<std::optional<std::string>> ReadBinContainer(
    const std::filesystem::path& path) {
  std::error_code ec;
  const bool exists = std::filesystem::exists(path, ec);
  if (ec)
    return absl::PermissionDeniedError("Cannot inspect BIN destination: " +
                                       ec.message());
  if (!exists)
    return std::optional<std::string>{};
  if (!std::filesystem::is_regular_file(path, ec) || ec)
    return absl::FailedPreconditionError(
        "BIN destination is not a regular file");
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
    return absl::PermissionDeniedError("Cannot read BIN destination");
  const auto end = input.tellg();
  if (end < 0 || static_cast<uint64_t>(end) > kMaxBinContainerBytes)
    return absl::ResourceExhaustedError(
        "BIN container exceeds the 64 MiB export limit");
  std::string bytes(static_cast<size_t>(end), '\0');
  input.seekg(0);
  if ((!bytes.empty() && !input.read(bytes.data(), bytes.size())) ||
      input.peek() != std::char_traits<char>::eof())
    return absl::DataLossError("BIN destination changed while being read");
  return std::optional<std::string>(std::move(bytes));
}

absl::StatusOr<std::string> ReadLibrary(const std::string& path) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec)
    return absl::InternalError("Cannot inspect graphics scratch library: " +
                               ec.message());
  if (size > 4 * 1024 * 1024)
    return absl::ResourceExhaustedError(
        "Graphics scratch library exceeds 4 MiB");
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return absl::InternalError("Cannot read graphics scratch library");
  std::string bytes((std::istreambuf_iterator<char>(file)), {});
  if (file.bad())
    return absl::DataLossError(
        "Cannot finish reading graphics scratch library");
  return bytes;
}

}  // namespace

absl::StatusOr<LibrarySnapshot> LoadLibrary(const std::string& path) {
  std::error_code ec;
  const bool exists = std::filesystem::exists(path, ec);
  if (ec)
    return absl::InternalError("Cannot inspect graphics scratch path: " +
                               ec.message());
  LibrarySnapshot result;
  if (exists) {
    ASSIGN_OR_RETURN(auto bytes, ReadLibrary(path));
    ASSIGN_OR_RETURN(result.library, GraphicsScratchLibrary::Parse(bytes));
    result.bytes = std::move(bytes);
  }
  return result;
}

absl::StatusOr<std::string> SaveLibrary(
    const std::string& path, const GraphicsScratchLibrary& library,
    const std::optional<std::string>& expected_bytes) {
#ifndef __EMSCRIPTEN__
  ASSIGN_OR_RETURN(auto lock, core::AcquireSourceArtifactPublicationLock(
                                  {std::filesystem::path(path)}));
#endif
  std::error_code ec;
  const bool exists = std::filesystem::exists(path, ec);
  if (ec || exists != expected_bytes.has_value())
    return absl::FailedPreconditionError(
        "Graphics scratch library changed outside this editor; export pending "
        "assets before reloading");
  if (exists) {
    ASSIGN_OR_RETURN(const auto bytes, ReadLibrary(path));
    if (bytes != *expected_bytes)
      return absl::FailedPreconditionError(
          "Graphics scratch library changed outside this editor; export "
          "pending assets before reloading");
  }
  const auto bytes = library.Serialize();
#ifndef __EMSCRIPTEN__
  RETURN_IF_ERROR(project::WriteProjectFileAtomically(path, bytes, exists));
#else
  return absl::UnimplementedError(
      "Scratch library persistence requires the desktop editor");
#endif
  return bytes;
}

absl::StatusOr<GraphicsScratchAsset> ImportBin(const std::string& path, int bpp,
                                               size_t offset,
                                               const std::string& name) {
  if (bpp < 2 || bpp > 4)
    return absl::InvalidArgumentError("Select 2, 3 or 4 bpp");
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
    return absl::NotFoundError("Cannot open BIN file");
  const auto size = input.tellg();
  const size_t count = 64 * 8 * bpp;
  if (size < 0 || offset > static_cast<uint64_t>(size) ||
      count > static_cast<uint64_t>(size) - offset)
    return absl::OutOfRangeError(
        "BIN range does not contain 64 complete tiles");
  input.seekg(offset);
  std::vector<uint8_t> bytes(count);
  if (!input.read(reinterpret_cast<char*>(bytes.data()), count))
    return absl::DataLossError("Cannot finish reading BIN tiles");
  auto asset = DecodeScratchBin(bytes, bpp, name);
  if (!asset.ok())
    return asset.status();
  asset->source = path + " @ " + std::to_string(offset);
  ASSIGN_OR_RETURN(const auto resolved, ResolveBinPath(path));
  asset->source_path = resolved.string();
  asset->source_offset = offset;
  asset->original_chunk = bytes;
  return std::move(*asset);
}

absl::StatusOr<BinExportResult> ExportBin(
    const GraphicsScratchAsset& asset, const std::string& path,
    const std::string& library_path,
    const std::function<absl::Status(const std::string&)>& validator) {
  ASSIGN_OR_RETURN(const auto encoded, EncodeScratchBin(asset));
  ASSIGN_OR_RETURN(const auto target, ResolveBinPath(path));
  if (PathsAlias(target, library_path))
    return absl::FailedPreconditionError(
        "BIN export cannot replace the scratch library");
  if (validator)
    RETURN_IF_ERROR(validator(target.string()));
#ifndef __EMSCRIPTEN__
  ASSIGN_OR_RETURN(auto lock, core::AcquireSourceArtifactPublicationLock(
                                  {target}, {"graphics BIN", "exported BIN"}));
  ASSIGN_OR_RETURN(const auto before, ReadBinContainer(target));
  const bool same_source =
      !asset.source_path.empty() && PathsAlias(target, asset.source_path);
  if (same_source) {
    ASSIGN_OR_RETURN(const auto original_path,
                     ResolveBinPath(asset.source_path));
    if (target != original_path)
      return absl::FailedPreconditionError(
          "BIN destination is a hard-link alias of the source; select the "
          "original BIN path");
  }
  std::string after(reinterpret_cast<const char*>(encoded.data()),
                    encoded.size());
  if (same_source) {
    if (!before || asset.source_offset > before->size() ||
        encoded.size() > before->size() - asset.source_offset)
      return absl::FailedPreconditionError(
          "Original BIN source range is missing or truncated; asset retained");
    const std::string original(
        reinterpret_cast<const char*>(asset.original_chunk.data()),
        asset.original_chunk.size());
    if (before->compare(asset.source_offset, original.size(), original) != 0)
      return absl::AbortedError(
          "BIN source chunk changed since import/export; reimport to review "
          "it");
    after = *before;
    after.replace(asset.source_offset, encoded.size(),
                  reinterpret_cast<const char*>(encoded.data()),
                  encoded.size());
  } else if (before && before->size() != encoded.size()) {
    return absl::FailedPreconditionError(
        "Standalone BIN export cannot replace a differently sized file; choose "
        "a new BIN path");
  }
  if (before) {
    RETURN_IF_ERROR(core::PublishSourceArtifacts(
        *lock, {{target, before, after}},
        core::ComputeSourceArtifactSha256(*before)));
  } else {
    RETURN_IF_ERROR(
        project::WriteProjectFileAtomically(target.string(), after, false));
  }
  return BinExportResult{same_source, encoded};
#else
  return absl::UnimplementedError("BIN export requires the desktop editor");
#endif
}

}  // namespace yaze::editor::graphics_scratch_storage
