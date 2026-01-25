#include "app/activity_file.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"

#ifdef YAZE_WITH_JSON
#include <nlohmann/json.hpp>
#endif

namespace yaze::app {

namespace fs = std::filesystem;

ActivityFile::ActivityFile(const std::string& path) : path_(path) {
  // Touch the file to create it
  std::ofstream file(path_);
  if (!file) {
    // Failed to create file - log warning but don't throw
    // Discovery will simply not work for this instance
  }
}

ActivityFile::~ActivityFile() {
  if (!path_.empty()) {
    std::remove(path_.c_str());
  }
}

ActivityFile::ActivityFile(ActivityFile&& other) noexcept
    : path_(std::move(other.path_)) {
  other.path_.clear();
}

ActivityFile& ActivityFile::operator=(ActivityFile&& other) noexcept {
  if (this != &other) {
    // Clean up existing file
    if (!path_.empty()) {
      std::remove(path_.c_str());
    }
    path_ = std::move(other.path_);
    other.path_.clear();
  }
  return *this;
}

void ActivityFile::Update(const ActivityStatus& status) {
  if (path_.empty()) return;

#ifdef YAZE_WITH_JSON
  nlohmann::json j;
  j["pid"] = status.pid;
  j["version"] = status.version;
  j["socket_path"] = status.socket_path;
  j["active_rom"] = status.active_rom;
  j["start_timestamp"] = status.start_timestamp;

  std::ofstream file(path_);
  if (file) {
    file << j.dump(2);  // Pretty print with 2-space indent
  }
#else
  // Fallback: Simple JSON string formatting without nlohmann
  std::ofstream file(path_);
  if (file) {
    file << absl::StrFormat(
        "{\n"
        "  \"pid\": %d,\n"
        "  \"version\": \"%s\",\n"
        "  \"socket_path\": \"%s\",\n"
        "  \"active_rom\": \"%s\",\n"
        "  \"start_timestamp\": %d\n"
        "}\n",
        status.pid, status.version, status.socket_path, status.active_rom,
        status.start_timestamp);
  }
#endif
}

bool ActivityFile::Exists() const {
  return !path_.empty() && fs::exists(path_);
}

}  // namespace yaze::app
