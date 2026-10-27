#include "app/editor/overworld/scratch_workspace.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <system_error>

#include "absl/strings/str_cat.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using Json = nlohmann::json;

absl::Status InvalidScratch() {
  return absl::DataLossError("Invalid scratch workspace data");
}

bool IsInteger(const Json& value, int low, int high) {
  if (!value.is_number_integer())
    return false;
  if (value.is_number_unsigned()) {
    const auto number = value.get<uint64_t>();
    return (low <= 0 || number >= static_cast<uint64_t>(low)) &&
           number <= static_cast<uint64_t>(high);
  }
  const auto number = value.get<int64_t>();
  return number >= low && number <= high;
}

bool IsId(const Json& value) {
  return value.is_number_unsigned() && value.get<uint64_t>() > 0 &&
         value.get<uint64_t>() < std::numeric_limits<uint64_t>::max();
}

absl::StatusOr<ScratchPage> ParsePage(const Json& root, bool legacy) {
  if (!root.is_object())
    return InvalidScratch();
  ScratchPage page;
  if (!legacy) {
    if (!root.contains("id") || !IsId(root["id"]))
      return InvalidScratch();
    page.id = root["id"].get<uint64_t>();
  }
  if (root.contains("name")) {
    if (!root["name"].is_string())
      return InvalidScratch();
    page.name = root["name"].get<std::string>();
    if (page.name.empty() || page.name.size() > 128)
      return InvalidScratch();
  }
  for (const auto* key : {"width", "height"}) {
    if (root.contains(key) && !IsInteger(root[key], 1, 32))
      return InvalidScratch();
  }
  page.width = root.value("width", 16);
  page.height = root.value("height", 16);
  if (root.contains("in_use") && !root["in_use"].is_boolean())
    return InvalidScratch();
  page.in_use = root.value("in_use", false);
  if (root.contains("source_map")) {
    if (!IsInteger(root["source_map"], -1, 0xFFFF))
      return InvalidScratch();
    page.source_map = root["source_map"].get<int>();
  }
  if (!root.contains("tiles") || !root["tiles"].is_array() ||
      root["tiles"].size() < static_cast<size_t>(page.height) ||
      root["tiles"].size() > 32)
    return InvalidScratch();
  for (size_t y = 0; y < root["tiles"].size(); ++y) {
    const auto& row = root["tiles"][y];
    if (!row.is_array() || row.size() < static_cast<size_t>(page.width) ||
        row.size() > 32)
      return InvalidScratch();
    for (size_t x = 0; x < row.size(); ++x) {
      if (!IsInteger(row[x], 0, 0xFFFF))
        return InvalidScratch();
      page.tile_data[x][y] = row[x].get<int>();
    }
  }
  return page;
}

absl::StatusOr<std::optional<std::string>> ReadContents(
    const std::filesystem::path& path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error)
    return absl::InternalError(
        absl::StrCat("Cannot inspect scratch workspace: ", error.message()));
  if (!exists)
    return std::optional<std::string>{};
  if (!std::filesystem::is_regular_file(path, error) || error)
    return absl::FailedPreconditionError(
        "Scratch workspace is not a regular file");
  const auto size = std::filesystem::file_size(path, error);
  if (error || size > 1024 * 1024)
    return absl::DataLossError("Scratch workspace exceeds the 1 MiB limit");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return absl::PermissionDeniedError("Cannot read scratch workspace");
  std::string contents((std::istreambuf_iterator<char>(input)), {});
  if (input.bad())
    return absl::InternalError("Failed to read scratch workspace");
  return std::optional<std::string>(std::move(contents));
}

}  // namespace

bool ScratchPage::operator==(const ScratchPage& other) const {
  return id == other.id && name == other.name && width == other.width &&
         height == other.height && in_use == other.in_use &&
         source_map == other.source_map && tile_data == other.tile_data;
}

TileBrush ScratchPage::AsBrush() const {
  TileBrush brush;
  brush.width = width;
  brush.height = height;
  brush.tile_ids.reserve(width * height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      brush.tile_ids.push_back(tile_data[x][y]);
  return brush;
}

bool ScratchWorkspaceState::operator==(
    const ScratchWorkspaceState& other) const {
  return pages == other.pages && selected_id == other.selected_id &&
         next_id == other.next_id;
}

void ScratchWorkspace::Restore(const ScratchWorkspaceState& state) {
  // Restoring an Undo snapshot must not recycle an ID already issued later.
  const uint64_t next_id = std::max(state_.next_id, state.next_id);
  state_ = state;
  state_.next_id = next_id;
}

ScratchPage& ScratchWorkspace::current() {
  return const_cast<ScratchPage&>(
      static_cast<const ScratchWorkspace*>(this)->current());
}

const ScratchPage& ScratchWorkspace::current() const {
  const auto it = std::find_if(
      state_.pages.begin(), state_.pages.end(),
      [&](const auto& page) { return page.id == state_.selected_id; });
  return *it;
}

bool ScratchWorkspace::Select(uint64_t id) {
  for (const auto& page : state_.pages) {
    if (page.id == id) {
      state_.selected_id = id;
      return true;
    }
  }
  return false;
}

absl::Status ScratchWorkspace::AddPage(bool duplicate) {
  if (state_.pages.size() >= kMaxPages ||
      state_.next_id >= std::numeric_limits<uint64_t>::max() - 1)
    return absl::ResourceExhaustedError("Scratch workspace is full (32 pages)");
  ScratchPage page = duplicate ? current() : ScratchPage{};
  page.id = state_.next_id++;
  page.name = duplicate ? page.name.substr(0, 123) + " copy"
                        : absl::StrCat("Scratch ", page.id);
  state_.selected_id = page.id;
  state_.pages.push_back(std::move(page));
  return absl::OkStatus();
}

absl::Status ScratchWorkspace::Rename(absl::string_view name) {
  if (name.empty() || name.size() > 128)
    return absl::InvalidArgumentError(
        "Scratch name must contain 1 to 128 bytes");
  current().name = std::string(name);
  return absl::OkStatus();
}

absl::Status ScratchWorkspace::DeletePage() {
  if (state_.pages.size() <= 1)
    return absl::FailedPreconditionError("Keep at least one scratch layout");
  const uint64_t selected = state_.selected_id;
  state_.pages.erase(std::remove_if(state_.pages.begin(), state_.pages.end(),
                                    [selected](const auto& page) {
                                      return page.id == selected;
                                    }),
                     state_.pages.end());
  state_.selected_id = state_.pages.front().id;
  return absl::OkStatus();
}

absl::Status ScratchWorkspace::Resize(int width, int height) {
  if (width < 1 || width > 32 || height < 1 || height > 32)
    return absl::InvalidArgumentError(
        "Scratch dimensions must be 1 to 32 tiles");
  current().width = width;
  current().height = height;
  return absl::OkStatus();
}

absl::Status ScratchWorkspace::Stamp(const TileBrush& brush, int x, int y) {
  if (!brush.valid())
    return absl::InvalidArgumentError("Invalid scratch Tile16 brush");
  if (x < 0 || y < 0 || x >= current().width || y >= current().height)
    return absl::OutOfRangeError("Scratch stamp anchor is outside the page");
  auto& page = current();
  for (int sy = 0; sy < brush.height && y + sy < page.height; ++sy)
    for (int sx = 0; sx < brush.width && x + sx < page.width; ++sx)
      page.tile_data[x + sx][y + sy] = brush.at(sx, sy);
  page.in_use = true;
  return absl::OkStatus();
}

absl::Status ScratchWorkspace::Capture(const TileBrush& brush, int source_map) {
  if (!brush.valid() || brush.width > 32 || brush.height > 32)
    return absl::InvalidArgumentError(
        "Capture a selection up to 32 by 32 tiles");
  if (source_map < -1 || source_map > 0xFFFF)
    return absl::InvalidArgumentError("Invalid scratch source map");
  const auto status = AddPage();
  if (!status.ok())
    return status;
  auto& page = current();
  page.width = brush.width;
  page.height = brush.height;
  page.source_map = source_map;
  page.name = absl::StrCat("Map ", source_map, " selection");
  return Stamp(brush, 0, 0);
}

void ScratchWorkspace::Clear() {
  auto& page = current();
  page.in_use = false;
  for (auto& column : page.tile_data)
    column.fill(0);
}

absl::Status ScratchWorkspace::Parse(absl::string_view contents) {
  if (contents.size() > 1024 * 1024)
    return absl::DataLossError("Scratch workspace exceeds the 1 MiB limit");
  const auto root =
      Json::parse(contents.begin(), contents.end(), nullptr, false);
  if (!root.is_object() || !root.contains("version") ||
      !IsInteger(root["version"], 1, 2))
    return InvalidScratch();
  ScratchWorkspaceState parsed;
  if (root["version"].get<int>() == 1) {
    const auto page = ParsePage(root, true);
    if (!page.ok())
      return page.status();
    parsed.pages = {*page};
  } else {
    if (!root.contains("pages") || !root["pages"].is_array() ||
        root["pages"].empty() || root["pages"].size() > kMaxPages ||
        !root.contains("selected_id") || !IsId(root["selected_id"]) ||
        !root.contains("next_id") || !IsId(root["next_id"]))
      return InvalidScratch();
    parsed.pages.clear();
    std::set<uint64_t> ids;
    for (const auto& entry : root["pages"]) {
      const auto page = ParsePage(entry, false);
      if (!page.ok())
        return page.status();
      if (!ids.insert(page->id).second)
        return InvalidScratch();
      parsed.pages.push_back(*page);
    }
    parsed.selected_id = root["selected_id"].get<uint64_t>();
    parsed.next_id = root["next_id"].get<uint64_t>();
    if (!ids.count(parsed.selected_id) || parsed.next_id <= *ids.rbegin())
      return InvalidScratch();
  }
  state_ = std::move(parsed);
  return absl::OkStatus();
}

std::string ScratchWorkspace::Serialize() const {
  Json root = {{"version", 2},
               {"selected_id", state_.selected_id},
               {"next_id", state_.next_id},
               {"pages", Json::array()}};
  for (const auto& page : state_.pages) {
    Json entry = {{"id", page.id},         {"name", page.name},
                  {"width", page.width},   {"height", page.height},
                  {"in_use", page.in_use}, {"source_map", page.source_map},
                  {"tiles", Json::array()}};
    // Preserve hidden cells when a page is temporarily shrunk.
    for (int y = 0; y < 32; ++y) {
      Json row = Json::array();
      for (int x = 0; x < 32; ++x)
        row.push_back(page.tile_data[x][y]);
      entry["tiles"].push_back(std::move(row));
    }
    root["pages"].push_back(std::move(entry));
  }
  return root.dump(2) + '\n';
}

absl::Status ScratchWorkspace::Load(const std::filesystem::path& path) {
  file_loaded_ = false;
  const auto contents = ReadContents(path);
  if (!contents.ok())
    return contents.status();
  if (*contents) {
    const auto status = Parse(**contents);
    if (!status.ok())
      return status;
  } else {
    state_ = ScratchWorkspaceState{};
  }
  saved_contents_ = *contents;
  file_loaded_ = true;
  return absl::OkStatus();
}

absl::Status ScratchWorkspace::Save(const std::filesystem::path& path) {
  if (!file_loaded_)
    return absl::FailedPreconditionError(
        "Scratch workspace was not loaded safely; preserve and repair its "
        "file");
#ifndef __EMSCRIPTEN__
  auto lock = core::AcquireSourceArtifactPublicationLock(
      {std::filesystem::absolute(path)});
  if (!lock.ok())
    return lock.status();
#endif
  const auto contents = ReadContents(path);
  if (!contents.ok())
    return contents.status();
  if (*contents != saved_contents_)
    return absl::AbortedError(
        "Scratch workspace changed in another session; unsaved pages retained");
  const std::string serialized = Serialize();
#ifndef __EMSCRIPTEN__
  const auto status = project::WriteProjectFileAtomically(
      path.string(), serialized, contents->has_value());
#else
  // Emscripten's virtual filesystem has no native project-writer backend.
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << serialized;
  output.flush();
  const auto status =
      output.good() ? absl::OkStatus()
                    : absl::InternalError("Cannot save scratch workspace");
#endif
  if (!status.ok())
    return status;
  saved_contents_ = serialized;
  return absl::OkStatus();
}

}  // namespace yaze::editor
