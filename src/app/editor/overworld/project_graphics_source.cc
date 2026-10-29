#include "app/editor/overworld/project_graphics_source.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <utility>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include "core/overworld_graphics_preview.h"
#include "core/source_artifact_publisher.h"

namespace yaze::editor {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

std::string Hash(const std::vector<uint8_t>& bytes) {
  return core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(bytes.data()), bytes.size()});
}

absl::StatusOr<std::string> Read(const fs::path& path) {
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error)
    return absl::NotFoundError("Cannot read graphics source file: " +
                               path.string());
  if (size > 8 * 1024 * 1024)
    return absl::ResourceExhaustedError("Graphics source file exceeds 8 MiB: " +
                                        path.string());
  std::ifstream input(path, std::ios::binary);
  std::string bytes(size, '\0');
  if (!input || !input.read(bytes.data(), bytes.size()))
    return absl::DataLossError("Cannot read graphics source file: " +
                               path.string());
  return bytes;
}

absl::StatusOr<fs::path> Resolve(const fs::path& path) {
  std::error_code error;
  const auto resolved = fs::canonical(path, error);
  if (error)
    return absl::NotFoundError("Cannot resolve graphics source path: " +
                               path.string());
  return resolved;
}

absl::StatusOr<Json> Parse(const std::string& text) {
  try {
    return Json::parse(text);
  } catch (const Json::exception& error) {
    return absl::InvalidArgumentError(
        std::string("Invalid graphics source JSON: ") + error.what());
  }
}

Json Semantics(const Json& document) {
  Json result = {{"schema_version", document.at("schema_version")},
                 {"resources", document.at("resources")},
                 {"profiles", document.at("profiles")},
                 {"bindings", document.at("bindings")}};
  for (auto& resource : result["resources"])
    resource.erase("path");
  // Keep array order aligned with the pack builder and source publisher's
  // admission rule. A separately regenerated pack should be rebound first.
  return result;
}

// Use the existing complete V2 validator, including duplicate-key rejection.
absl::StatusOr<Json> LoadCatalog(const fs::path& path,
                                 std::map<std::string, std::string>* hashes) {
  auto bytes = Read(path);
  if (!bytes.ok())
    return bytes.status();
  auto preview = core::LoadOverworldGraphicsPreview(path.string());
  if (!preview.ok())
    return preview.status();
  auto reread = Read(path);
  if (!reread.ok())
    return reread.status();
  if (*bytes != *reread)
    return absl::FailedPreconditionError(
        "Graphics catalog changed while loading; reload the source document");
  auto document = Parse(*bytes);
  if (!document.ok())
    return document.status();
  if (document->value("schema_version", 0) != 2)
    return absl::FailedPreconditionError(
        "Graphics source editing requires a version 2 catalog");
  (*hashes)[path.string()] = core::ComputeSourceArtifactSha256(*bytes);
  for (const auto& resource : document->at("resources")) {
    auto asset =
        Resolve(path.parent_path() / resource.at("path").get<std::string>());
    if (!asset.ok())
      return asset.status();
    auto content = Read(*asset);
    if (!content.ok())
      return content.status();
    const auto hash = core::ComputeSourceArtifactSha256(*content);
    auto expected = resource.at("sha256").get<std::string>();
    std::transform(expected.begin(), expected.end(), expected.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (hash != expected ||
        content->size() != resource.at("byte_count").get<size_t>())
      return absl::FailedPreconditionError(
          "Graphics resource changed while loading; reload the source "
          "document");
    (*hashes)[asset->string()] = hash;
  }
  return document;
}

absl::Status Write(const fs::path& path, const std::vector<uint8_t>& bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  output.close();
  if (!output)
    return absl::InternalError("Cannot write private graphics replacement: " +
                               path.string());
  return absl::OkStatus();
}

absl::StatusOr<Json> RunPublisher(const std::vector<std::string>& arguments,
                                  const fs::path& directory) {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
  return absl::UnimplementedError(
      "Graphics source publication requires a native POSIX Python process");
#else
  // Pass each argument directly. Project paths and IDs never enter a shell.
  const fs::path out_path = directory / "stdout.json";
  const fs::path err_path = directory / "stderr.txt";
  posix_spawn_file_actions_t actions;
  int code = posix_spawn_file_actions_init(&actions);
  if (code != 0)
    return absl::InternalError("Cannot initialize graphics publisher process");
  code = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null",
                                          O_RDONLY, 0);
  if (code == 0)
    code = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
                                            out_path.c_str(),
                                            O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (code == 0)
    code = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO,
                                            err_path.c_str(),
                                            O_WRONLY | O_CREAT | O_TRUNC, 0600);
  std::vector<char*> argv;
  for (const auto& argument : arguments)
    argv.push_back(const_cast<char*>(argument.c_str()));
  argv.push_back(nullptr);
  pid_t pid = -1;
  if (code == 0)
    code = posix_spawnp(&pid, argv.front(), &actions, nullptr, argv.data(),
                        environ);
  posix_spawn_file_actions_destroy(&actions);
  if (code != 0)
    return absl::UnavailableError("Cannot launch graphics publisher: " +
                                  std::string(std::strerror(code)));
  int status = 0;
  // Publication spans several files with rollback. Never terminate its worker
  // while it may own rollback copies or be between source and pack writes.
  pid_t waited;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0)
    return absl::InternalError("Cannot wait for graphics publisher");
  auto output = Read(out_path);
  if (!output.ok())
    return output.status();
  auto result = Parse(*output);
  if (!result.ok()) {
    auto error = Read(err_path);
    return absl::InternalError(
        "Graphics publisher returned invalid JSON" +
        (error.ok() && !error->empty() ? ": " + error->substr(0, 4096) : ""));
  }
  if (!result->is_object() || !result->contains("result") ||
      !result->at("result").is_string())
    return absl::DataLossError(
        "Graphics publisher returned an invalid result object");
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
      result->at("result") == "error") {
    return absl::FailedPreconditionError(
        result->contains("error") && result->at("error").is_string()
            ? result->at("error").get<std::string>()
            : "Graphics publisher failed");
  }
  return result;
#endif
}
}  // namespace

struct ProjectGraphicsSource::PreviewFiles {
  fs::path directory;
  fs::path replacement;
  fs::path plan;
  std::string replacement_sha256;
  std::string plan_sha256;
  ~PreviewFiles() {
    if (!directory.empty()) {
      std::error_code ignored;
      fs::remove_all(directory, ignored);
    }
  }
};

ProjectGraphicsSource::ProjectGraphicsSource() = default;
ProjectGraphicsSource::~ProjectGraphicsSource() = default;
ProjectGraphicsSource::ProjectGraphicsSource(ProjectGraphicsSource&&) noexcept =
    default;
ProjectGraphicsSource& ProjectGraphicsSource::operator=(
    ProjectGraphicsSource&&) noexcept = default;

absl::Status ProjectGraphicsSource::Load(const std::string& source_root,
                                         const std::string& portable_catalog,
                                         int parent_map) {
  *this = ProjectGraphicsSource();
  if (source_root.empty() || portable_catalog.empty())
    return absl::FailedPreconditionError(
        "Bind an Oracle source repository and project graphics catalog first");
  auto root = Resolve(source_root);
  if (!root.ok())
    return root.status();
  auto portable_path = Resolve(portable_catalog);
  if (!portable_path.ok())
    return portable_path.status();
  auto canonical_path = Resolve(*root / "Overworld/graphics_catalog.json");
  if (!canonical_path.ok())
    return canonical_path.status();
  std::map<std::string, std::string> hashes;
  try {
    auto canonical = LoadCatalog(*canonical_path, &hashes);
    if (!canonical.ok())
      return canonical.status();
    auto portable = LoadCatalog(*portable_path, &hashes);
    if (!portable.ok())
      return portable.status();
    if (Semantics(*canonical) != Semantics(*portable))
      return absl::FailedPreconditionError(
          "Source and portable graphics catalogs differ; rebuild or rebind the "
          "project pack");
    std::string selected_profile;
    std::map<std::string, std::vector<int>> parents_by_profile;
    for (const auto& binding : canonical->at("bindings")) {
      const auto profile = binding.at("profile_id").get<std::string>();
      for (int parent : binding.at("parent_maps")) {
        parents_by_profile[profile].push_back(parent);
        if (parent == parent_map)
          selected_profile = profile;
      }
    }
    if (selected_profile.empty())
      return absl::NotFoundError(
          "This parent map has no project graphics source profile");
    std::set<std::string> bound_resources;
    for (const auto& transfer :
         canonical->at("profiles").at(selected_profile).at("transfers"))
      bound_resources.insert(transfer.at("resource_id").get<std::string>());
    std::vector<Resource> resources;
    for (const auto& id : bound_resources) {
      const auto& entry = canonical->at("resources").at(id);
      Resource resource;
      resource.id = id;
      auto source_path = Resolve(canonical_path->parent_path() /
                                 entry.at("path").get<std::string>());
      auto pack_path = Resolve(
          portable_path->parent_path() /
          portable->at("resources").at(id).at("path").get<std::string>());
      if (!source_path.ok())
        return source_path.status();
      if (!pack_path.ok())
        return pack_path.status();
      resource.source_path = source_path->string();
      resource.portable_path = pack_path->string();
      const auto identity = hashes.find(resource.source_path);
      if (identity == hashes.end())
        return absl::FailedPreconditionError(
            "Graphics resource path changed while loading; rebind the source "
            "document");
      resource.sha256 = identity->second;
      auto bytes = Read(*source_path);
      if (!bytes.ok())
        return bytes.status();
      if (core::ComputeSourceArtifactSha256(*bytes) != resource.sha256)
        return absl::FailedPreconditionError(
            "Graphics source changed while loading; reload the source "
            "document");
      resource.original_bytes.assign(bytes->begin(), bytes->end());
      resource.pending_bytes = resource.original_bytes;
      auto decoded =
          zelda3::DecodeGraphicsPreviewTransfer(resource.original_bytes, 0, 0);
      if (!decoded.ok())
        return decoded.status();
      for (const auto& [tile, pixels] : *decoded)
        resource.tiles.push_back(pixels);
      std::set<int> affected_parents;
      for (const auto& [profile_id, profile] :
           canonical->at("profiles").items()) {
        bool used = false;
        for (const auto& transfer : profile.at("transfers")) {
          if (transfer.at("resource_id") != id)
            continue;
          used = true;
          Slice slice;
          slice.profile_id = profile_id;
          slice.parent_maps = parents_by_profile[profile_id];
          slice.source_offset = transfer.value("source_offset", size_t{0});
          slice.byte_count = transfer.at("byte_count").get<size_t>();
          slice.vram_word_address = transfer.at("vram_word_address").get<int>();
          slice.bg_character_base_word =
              profile.at("bg_character_base_word").get<int>();
          resource.slices.push_back(std::move(slice));
        }
        if (used) {
          resource.profile_ids.push_back(profile_id);
          const auto& parents = parents_by_profile[profile_id];
          affected_parents.insert(parents.begin(), parents.end());
        }
      }
      resource.parent_maps.assign(affected_parents.begin(),
                                  affected_parents.end());
      resources.push_back(std::move(resource));
    }
    source_root_ = root->string();
    portable_catalog_ = portable_path->string();
    parent_map_ = parent_map;
    profile_id_ = selected_profile;
    resources_ = std::move(resources);
    histories_.resize(resources_.size());
    loaded_hashes_ = std::move(hashes);
    auto status = CheckUnchanged();
    if (!status.ok())
      *this = ProjectGraphicsSource();
    return status;
  } catch (const Json::exception& error) {
    *this = ProjectGraphicsSource();
    return absl::InvalidArgumentError(
        std::string("Invalid graphics source catalog: ") + error.what());
  }
}

const ProjectGraphicsSource::Resource*
ProjectGraphicsSource::selected_resource() const {
  return selected_ < resources_.size() ? &resources_[selected_] : nullptr;
}

bool ProjectGraphicsSource::backend_available() const {
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
  return false;
#else
  std::error_code error;
  return !source_root_.empty() &&
         fs::is_regular_file(fs::path(source_root_) /
                                 "Scripts/Graphics/edit_graphics_source.py",
                             error);
#endif
}

absl::Status ProjectGraphicsSource::SelectResource(const std::string& id) {
  for (size_t index = 0; index < resources_.size(); ++index) {
    if (resources_[index].id == id) {
      if (selected_ != index)
        InvalidatePreview();
      selected_ = index;
      return absl::OkStatus();
    }
  }
  return absl::NotFoundError(
      "Resource is not bound to this graphics profile: " + id);
}

absl::Status ProjectGraphicsSource::CheckPixel(size_t tile, int x,
                                               int y) const {
  const auto* resource = selected_resource();
  if (!resource)
    return absl::FailedPreconditionError(
        "Load a project graphics resource first");
  if (tile >= resource->tiles.size() || x < 0 || x >= 8 || y < 0 || y >= 8)
    return absl::OutOfRangeError(
        "Graphics source pixel is outside its resource");
  return absl::OkStatus();
}

absl::StatusOr<uint8_t> ProjectGraphicsSource::GetPixel(size_t tile, int x,
                                                        int y) const {
  auto status = CheckPixel(tile, x, y);
  if (!status.ok())
    return status;
  return resources_[selected_].tiles[tile][y * 8 + x];
}

void ProjectGraphicsSource::WritePixel(size_t tile, int x, int y,
                                       uint8_t index) {
  auto& resource = resources_[selected_];
  resource.tiles[tile][y * 8 + x] = index;
  for (int plane = 0; plane < 4; ++plane) {
    const size_t offset = tile * 32 + (plane / 2) * 16 + y * 2 + plane % 2;
    const uint8_t mask = 1 << (7 - x);
    auto& byte = resource.pending_bytes[offset];
    byte = (byte & ~mask) | ((index & (1 << plane)) ? mask : 0);
  }
  InvalidatePreview();
}

absl::Status ProjectGraphicsSource::SetPixel(size_t tile, int x, int y,
                                             uint8_t index) {
  auto before = GetPixel(tile, x, y);
  if (!before.ok())
    return before.status();
  if (index > 15)
    return absl::InvalidArgumentError(
        "SNES 4bpp color index must be between 0 and 15");
  if (*before == index)
    return absl::OkStatus();
  histories_[selected_].undo.push_back({tile, x, y, *before, index});
  histories_[selected_].redo.clear();
  WritePixel(tile, x, y, index);
  return absl::OkStatus();
}

absl::Status ProjectGraphicsSource::Undo() {
  if (!can_undo())
    return absl::FailedPreconditionError("No graphics source edit to undo");
  auto& history = histories_[selected_];
  const auto action = history.undo.back();
  history.undo.pop_back();
  history.redo.push_back(action);
  WritePixel(action.tile, action.x, action.y, action.before);
  return absl::OkStatus();
}

absl::Status ProjectGraphicsSource::Redo() {
  if (!can_redo())
    return absl::FailedPreconditionError("No graphics source edit to redo");
  auto& history = histories_[selected_];
  const auto action = history.redo.back();
  history.redo.pop_back();
  history.undo.push_back(action);
  WritePixel(action.tile, action.x, action.y, action.after);
  return absl::OkStatus();
}

bool ProjectGraphicsSource::can_undo() const {
  return selected_ < histories_.size() && !histories_[selected_].undo.empty();
}
bool ProjectGraphicsSource::can_redo() const {
  return selected_ < histories_.size() && !histories_[selected_].redo.empty();
}
bool ProjectGraphicsSource::dirty() const {
  const auto* resource = selected_resource();
  return resource && resource->dirty();
}
bool ProjectGraphicsSource::any_dirty() const {
  return std::any_of(resources_.begin(), resources_.end(),
                     [](const Resource& resource) { return resource.dirty(); });
}

absl::Status ProjectGraphicsSource::ReplaceResourceBytes(
    const std::vector<uint8_t>& bytes) {
  return ReplaceResourceBytesAt(selected_, bytes);
}

absl::Status ProjectGraphicsSource::ReplaceResourceBytes(
    const std::string& source_path, const std::vector<uint8_t>& bytes) {
  if (!loaded())
    return absl::FailedPreconditionError(
        "Load a project graphics resource first");
  for (size_t index = 0; index < resources_.size(); ++index) {
    if (resources_[index].source_path == source_path)
      return ReplaceResourceBytesAt(index, bytes);
  }
  return absl::NotFoundError(
      "Source path is not bound to this graphics profile: " + source_path);
}

absl::Status ProjectGraphicsSource::ReplaceResourceBytesAt(
    size_t index, const std::vector<uint8_t>& bytes) {
  if (index >= resources_.size())
    return absl::FailedPreconditionError(
        "Load a project graphics resource first");
  auto& resource = resources_[index];
  if (bytes.empty() || bytes.size() % 32 != 0 ||
      bytes.size() != resource.original_bytes.size())
    return absl::InvalidArgumentError(
        "Replacement must preserve the resource's aligned 4bpp byte count");
  auto status = CheckUnchanged();
  if (!status.ok())
    return status;
  auto decoded = zelda3::DecodeGraphicsPreviewTransfer(bytes, 0, 0);
  if (!decoded.ok())
    return decoded.status();
  if (bytes == resource.pending_bytes)
    return absl::OkStatus();

  // Prepare both representations before changing the document. Swaps commit
  // the byte snapshot and decoded pixels together after validation succeeds.
  std::vector<uint8_t> replacement(bytes);
  std::vector<zelda3::PreviewTile8> tiles;
  tiles.reserve(decoded->size());
  for (const auto& [character, pixels] : *decoded)
    tiles.push_back(pixels);
  resource.pending_bytes.swap(replacement);
  resource.tiles.swap(tiles);
  histories_[index].undo.clear();
  histories_[index].redo.clear();
  InvalidatePreview();
  return absl::OkStatus();
}

absl::StatusOr<zelda3::OverworldGraphicsPreview>
ProjectGraphicsSource::PendingGraphicsPreview() const {
  if (!loaded())
    return absl::FailedPreconditionError(
        "Load a project graphics resource first");
  auto status = CheckUnchanged();
  if (!status.ok())
    return status;
  zelda3::OverworldGraphicsPreview result;
  for (const auto& resource : resources_) {
    for (const auto& slice : resource.slices) {
      if (slice.source_offset > resource.pending_bytes.size() ||
          slice.byte_count >
              resource.pending_bytes.size() - slice.source_offset)
        return absl::InvalidArgumentError(
            "Pending graphics resource slice is outside its resource");
      auto decoded = zelda3::DecodeGraphicsPreviewTransfer(
          std::span<const uint8_t>(resource.pending_bytes)
              .subspan(slice.source_offset, slice.byte_count),
          slice.vram_word_address, slice.bg_character_base_word);
      if (!decoded.ok())
        return decoded.status();
      for (int parent : slice.parent_maps) {
        auto& area = result[parent];
        for (const auto& [character, pixels] : *decoded) {
          if (!area.emplace(character, pixels).second)
            return absl::InvalidArgumentError(
                "Overlapping pending graphics preview transfers");
        }
      }
    }
  }
  return result;
}

void ProjectGraphicsSource::InvalidatePreview() {
  preview_.reset();
  preview_json_ = Json();
}

absl::Status ProjectGraphicsSource::CheckUnchanged() const {
  for (const auto& [path, hash] : loaded_hashes_) {
    auto bytes = Read(path);
    if (!bytes.ok())
      return bytes.status();
    if (core::ComputeSourceArtifactSha256(*bytes) != hash)
      return absl::FailedPreconditionError(
          "Graphics source changed on disk; keep the draft and reload or "
          "rebind before publication: " +
          path);
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> ProjectGraphicsSource::Preview() {
  InvalidatePreview();
  if (!dirty())
    return absl::FailedPreconditionError(
        "The selected graphics resource has no pending changes");
  if (!backend_available())
    return absl::FailedPreconditionError(
        "The bound source repository has no supported graphics publisher at "
        "Scripts/Graphics/edit_graphics_source.py");
  auto status = CheckUnchanged();
  if (!status.ok())
    return status;
  auto files = std::make_unique<PreviewFiles>();
  std::error_code error;
  const auto temporary_root = fs::temp_directory_path(error);
  if (error)
    return absl::InternalError(
        "Cannot locate a private graphics preview directory");
  const auto temp = fs::canonical(temporary_root, error);
  if (error)
    return absl::InternalError(
        "Cannot resolve the private graphics preview directory");
  static std::atomic<uint64_t> sequence{0};
  for (int attempt = 0; attempt < 100; ++attempt) {
    const auto candidate =
        temp /
        ("yaze-graphics-source-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         "-" + std::to_string(sequence++));
    if (fs::create_directory(candidate, error)) {
      files->directory = candidate;
      break;
    }
    if (error && error != std::errc::file_exists)
      return absl::InternalError(
          "Cannot create a private graphics preview directory");
  }
  if (files->directory.empty())
    return absl::AlreadyExistsError(
        "Cannot reserve a private graphics preview directory");
  fs::permissions(files->directory, fs::perms::owner_all, error);
  if (error)
    return absl::InternalError(
        "Cannot protect the private graphics preview directory");
  files->replacement = files->directory / "replacement.4bpp";
  files->plan = files->directory / "review-plan.json";
  const auto& resource = resources_[selected_];
  status = Write(files->replacement, resource.pending_bytes);
  if (!status.ok())
    return status;
  files->replacement_sha256 = Hash(resource.pending_bytes);
  auto result = RunPublisher(
      {"python3", "-B",
       (fs::path(source_root_) / "Scripts/Graphics/edit_graphics_source.py")
           .string(),
       "plan", "--source-root", source_root_, "--portable-catalog",
       portable_catalog_, "--resource-id", resource.id, "--replacement",
       files->replacement.string(), "--plan", files->plan.string()},
      files->directory);
  if (!result.ok())
    return result.status();
  auto plan_bytes = Read(files->plan);
  if (!plan_bytes.ok())
    return plan_bytes.status();
  auto plan = Parse(*plan_bytes);
  if (!plan.ok())
    return plan.status();
  try {
    if (*plan != *result || plan->at("operation") != "plan" ||
        plan->at("result") != "ok" ||
        plan->at("before_resource_id") != resource.id ||
        plan->at("resource_sha256") != resource.sha256 ||
        plan->at("replacement_sha256") != files->replacement_sha256 ||
        plan->at("source_root") != source_root_ ||
        plan->at("portable_catalog") != portable_catalog_ ||
        plan->at("replacement_path") != files->replacement.string() ||
        !plan->at("after_resource_id").is_string())
      return absl::DataLossError(
          "Graphics publisher plan does not match the selected draft");
  } catch (const Json::exception&) {
    return absl::DataLossError(
        "Graphics publisher returned an incomplete review plan");
  }
  status = CheckUnchanged();
  if (!status.ok())
    return status;
  files->plan_sha256 = core::ComputeSourceArtifactSha256(*plan_bytes);
  preview_json_ = std::move(*plan);
  preview_ = std::move(files);
  return preview_json_;
}

absl::StatusOr<Json> ProjectGraphicsSource::Publish() {
  if (!preview_)
    return absl::FailedPreconditionError(
        "Preview this graphics source draft before publication");
  auto status = CheckUnchanged();
  if (!status.ok())
    return status;
  auto replacement = Read(preview_->replacement);
  auto plan = Read(preview_->plan);
  if (!replacement.ok())
    return replacement.status();
  if (!plan.ok())
    return plan.status();
  if (core::ComputeSourceArtifactSha256(*replacement) !=
          preview_->replacement_sha256 ||
      core::ComputeSourceArtifactSha256(*plan) != preview_->plan_sha256)
    return absl::FailedPreconditionError(
        "Graphics review plan changed; preview the draft again");
  auto result = RunPublisher(
      {"python3", "-B",
       (fs::path(source_root_) / "Scripts/Graphics/edit_graphics_source.py")
           .string(),
       "apply", "--source-root", source_root_, "--portable-catalog",
       portable_catalog_, "--resource-id", selected_resource()->id, "--plan",
       preview_->plan.string(), "--expected-plan-sha256",
       preview_->plan_sha256},
      preview_->directory);
  if (!result.ok())
    return result.status();
  if (!result->contains("operation") || result->at("operation") != "apply" ||
      result->at("result") != "published" ||
      !result->contains("before_resource_id") ||
      result->at("before_resource_id") != selected_resource()->id ||
      !result->contains("after_resource_id") ||
      result->at("after_resource_id") != preview_json_.at("after_resource_id"))
    return absl::DataLossError(
        "Graphics publisher did not confirm publication");

  // Reload the published identity while retaining independent drafts whose
  // original resource bytes are still unchanged by this publication.
  ProjectGraphicsSource refreshed;
  status = refreshed.Load(source_root_, portable_catalog_, parent_map_);
  if (!status.ok())
    return absl::DataLossError(
        "Graphics source published, but reload failed: " +
        std::string(status.message()));
  for (size_t old = 0; old < resources_.size(); ++old) {
    if (old == selected_ || !resources_[old].dirty())
      continue;
    bool retained = false;
    for (size_t next = 0; next < refreshed.resources_.size(); ++next) {
      if (refreshed.resources_[next].id == resources_[old].id &&
          refreshed.resources_[next].original_bytes ==
              resources_[old].original_bytes) {
        refreshed.resources_[next].pending_bytes =
            resources_[old].pending_bytes;
        refreshed.resources_[next].tiles = resources_[old].tiles;
        refreshed.histories_[next] = histories_[old];
        retained = true;
      }
    }
    if (!retained)
      return absl::DataLossError(
          "Graphics source published, but another pending resource changed; "
          "keep the draft and rebind");
  }
  const auto after_id =
      preview_json_.at("after_resource_id").get<std::string>();
  status = refreshed.SelectResource(after_id);
  if (!status.ok())
    return absl::DataLossError(
        "Graphics source published, but new resource identity is missing");
  if (refreshed.selected_resource()->original_bytes !=
      selected_resource()->pending_bytes)
    return absl::DataLossError(
        "Graphics source published, but replacement bytes did not match on "
        "reload");
  // Saving changes the dirty baseline, not the user's pixel-edit history.
  refreshed.histories_[refreshed.selected_] = histories_[selected_];
  *this = std::move(refreshed);
  return result;
}

}  // namespace yaze::editor
