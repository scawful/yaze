/**
 * @file project_tool.cc
 * @brief Implementation of project management tools for AI agents
 */

#include "cli/service/agent/tools/project_tool.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// SHA-256 Implementation (Simplified)
// ============================================================================

namespace {

// SHA-256 constants
constexpr uint32_t kSHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

constexpr uint32_t RightRotate(uint32_t value, uint32_t count) {
  return (value >> count) | (value << (32 - count));
}

void SHA256Transform(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, h;

  // Prepare message schedule
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           (static_cast<uint32_t>(block[i * 4 + 3]));
  }

  for (int i = 16; i < 64; ++i) {
    uint32_t s0 = RightRotate(w[i - 15], 7) ^ RightRotate(w[i - 15], 18) ^
                  (w[i - 15] >> 3);
    uint32_t s1 = RightRotate(w[i - 2], 17) ^ RightRotate(w[i - 2], 19) ^
                  (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  // Initialize working variables
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];

  // Main loop
  for (int i = 0; i < 64; ++i) {
    uint32_t S1 = RightRotate(e, 6) ^ RightRotate(e, 11) ^ RightRotate(e, 25);
    uint32_t ch = (e & f) ^ ((~e) & g);
    uint32_t temp1 = h + S1 + ch + kSHA256_K[i] + w[i];
    uint32_t S0 = RightRotate(a, 2) ^ RightRotate(a, 13) ^ RightRotate(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = S0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  // Add compressed chunk to current hash value
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

std::array<uint8_t, 32> ComputeSHA256Internal(const uint8_t* data,
                                              size_t length) {
  // Initialize hash values
  uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

  size_t total_length = length;
  const uint8_t* ptr = data;

  // Process complete 64-byte blocks
  while (length >= 64) {
    SHA256Transform(state, ptr);
    ptr += 64;
    length -= 64;
  }

  // Handle remaining bytes + padding
  uint8_t block[64];
  std::memset(block, 0, sizeof(block));
  std::memcpy(block, ptr, length);

  // Append '1' bit (0x80) and pad with zeros
  block[length] = 0x80;

  // If not enough room for length, process block and create new one
  if (length >= 56) {
    SHA256Transform(state, block);
    std::memset(block, 0, sizeof(block));
  }

  // Append length in bits as 64-bit big-endian
  uint64_t bit_length = total_length * 8;
  for (int i = 0; i < 8; ++i) {
    block[63 - i] = static_cast<uint8_t>(bit_length >> (i * 8));
  }

  SHA256Transform(state, block);

  // Convert state to byte array (big-endian)
  std::array<uint8_t, 32> result;
  for (int i = 0; i < 8; ++i) {
    result[i * 4] = static_cast<uint8_t>(state[i] >> 24);
    result[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
    result[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
    result[i * 4 + 3] = static_cast<uint8_t>(state[i]);
  }

  return result;
}

}  // namespace

// ============================================================================
// ProjectToolUtils Implementation
// ============================================================================

std::array<uint8_t, 32> ProjectToolUtils::ComputeRomChecksum(const Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    // Return empty checksum
    std::array<uint8_t, 32> result;
    result.fill(0);
    return result;
  }

  // Compute checksum of ROM data
  return ComputeSHA256Internal(rom->data(), rom->size());
}

std::array<uint8_t, 32> ProjectToolUtils::ComputeSHA256(const uint8_t* data,
                                                        size_t length) {
  return ComputeSHA256Internal(data, length);
}

std::string ProjectToolUtils::FormatChecksum(
    const std::array<uint8_t, 32>& checksum) {
  std::ostringstream oss;
  for (uint8_t byte : checksum) {
    oss << absl::StrFormat("%02x", byte);
  }
  return oss.str();
}

std::string ProjectToolUtils::FormatTimestamp(
    const std::chrono::system_clock::time_point& time) {
  auto time_t = std::chrono::system_clock::to_time_t(time);
  std::tm tm = *std::gmtime(&time_t);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

absl::StatusOr<std::chrono::system_clock::time_point>
ProjectToolUtils::ParseTimestamp(const std::string& timestamp) {
  // Simple ISO 8601 parser (YYYY-MM-DDTHH:MM:SSZ)
  std::tm tm = {};
  std::istringstream ss(timestamp);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

  if (ss.fail()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid timestamp format: %s", timestamp));
  }

  auto time_t = std::mktime(&tm);
  return std::chrono::system_clock::from_time_t(time_t);
}

// ============================================================================
// ProjectSnapshot Implementation
// ============================================================================

absl::Status ProjectSnapshot::SaveToFile(const std::string& filepath) const {
  try {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
      return absl::InternalError(
          absl::StrFormat("Failed to open file for writing: %s", filepath));
    }

    // Write header
    EditFileHeader header;
    header.edit_count = static_cast<uint32_t>(edits.size());
    header.base_rom_sha256 = rom_checksum;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write edits
    for (const auto& edit : edits) {
      SerializedEdit ser_edit;
      ser_edit.address = edit.address;
      ser_edit.length = static_cast<uint32_t>(edit.new_value.size());

      file.write(reinterpret_cast<const char*>(&ser_edit), sizeof(ser_edit));
      file.write(reinterpret_cast<const char*>(edit.old_value.data()),
                 edit.old_value.size());
      file.write(reinterpret_cast<const char*>(edit.new_value.data()),
                 edit.new_value.size());
    }

    // Write metadata as JSON
    json metadata_json = {
        {"name", name},
        {"description", description},
        {"created", ProjectToolUtils::FormatTimestamp(created)},
        {"metadata", metadata}};

    std::string metadata_str = metadata_json.dump();
    uint32_t metadata_length = static_cast<uint32_t>(metadata_str.size());
    file.write(reinterpret_cast<const char*>(&metadata_length),
               sizeof(metadata_length));
    file.write(metadata_str.data(), metadata_str.size());

    return absl::OkStatus();

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to save snapshot: %s", e.what()));
  }
}

absl::StatusOr<ProjectSnapshot> ProjectSnapshot::LoadFromFile(
    const std::string& filepath) {
  try {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
      return absl::NotFoundError(
          absl::StrFormat("Failed to open file: %s", filepath));
    }

    // Read header
    EditFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != EditFileHeader::kMagic) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid file format (magic: 0x%08X)", header.magic));
    }

    if (header.version != EditFileHeader::kCurrentVersion) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unsupported version: %u", header.version));
    }

    ProjectSnapshot snapshot;
    snapshot.rom_checksum = header.base_rom_sha256;

    // Read edits
    for (uint32_t i = 0; i < header.edit_count; ++i) {
      SerializedEdit ser_edit;
      file.read(reinterpret_cast<char*>(&ser_edit), sizeof(ser_edit));

      RomEdit edit;
      edit.address = ser_edit.address;
      edit.old_value.resize(ser_edit.length);
      edit.new_value.resize(ser_edit.length);

      file.read(reinterpret_cast<char*>(edit.old_value.data()),
                ser_edit.length);
      file.read(reinterpret_cast<char*>(edit.new_value.data()),
                ser_edit.length);

      edit.timestamp = std::chrono::system_clock::now();
      edit.description = "Restored from snapshot";

      snapshot.edits.push_back(edit);
    }

    // Read metadata
    uint32_t metadata_length = 0;
    file.read(reinterpret_cast<char*>(&metadata_length),
              sizeof(metadata_length));

    std::string metadata_str(metadata_length, '\0');
    file.read(&metadata_str[0], metadata_length);

    json metadata_json = json::parse(metadata_str);
    snapshot.name = metadata_json["name"];
    snapshot.description = metadata_json["description"];

    auto timestamp_result =
        ProjectToolUtils::ParseTimestamp(metadata_json["created"]);
    if (timestamp_result.ok()) {
      snapshot.created = *timestamp_result;
    } else {
      snapshot.created = std::chrono::system_clock::now();
    }

    if (metadata_json.contains("metadata")) {
      snapshot.metadata =
          metadata_json["metadata"].get<std::map<std::string, std::string>>();
    }

    return snapshot;

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to load snapshot: %s", e.what()));
  }
}

// ============================================================================
// ProjectManager Implementation
// ============================================================================

absl::Status ProjectManager::Initialize(const std::string& base_path) {
  try {
    project_path_ = (fs::path(base_path) / ".yaze-project").string();
    snapshots_path_ = (fs::path(project_path_) / "snapshots").string();

    // Create directory structure
    fs::create_directories(snapshots_path_);

    // Create metadata file if it doesn't exist
    std::string metadata_path =
        (fs::path(project_path_) / "project.json").string();
    if (!fs::exists(metadata_path)) {
      json metadata = {{"version", "1.0"},
                       {"created", ProjectToolUtils::FormatTimestamp(
                                       std::chrono::system_clock::now())}};

      std::ofstream file(metadata_path);
      file << metadata.dump(2);
    }

    return LoadSnapshots();

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to initialize project: %s", e.what()));
  }
}

absl::Status ProjectManager::CreateSnapshot(
    const std::string& name, const std::string& description,
    const std::vector<RomEdit>& edits,
    const std::array<uint8_t, 32>& rom_checksum) {
  if (name.empty()) {
    return absl::InvalidArgumentError("Snapshot name cannot be empty");
  }

  if (snapshots_.find(name) != snapshots_.end()) {
    return absl::AlreadyExistsError(
        absl::StrFormat("Snapshot already exists: %s", name));
  }

  ProjectSnapshot snapshot;
  snapshot.name = name;
  snapshot.description = description;
  snapshot.created = std::chrono::system_clock::now();
  snapshot.edits = edits;
  snapshot.rom_checksum = rom_checksum;

  // Save to file
  std::string filepath = GetSnapshotFilePath(name);
  auto status = snapshot.SaveToFile(filepath);
  if (!status.ok()) {
    return status;
  }

  snapshots_[name] = snapshot;
  return SaveProjectMetadata();
}

absl::Status ProjectManager::RestoreSnapshot(const std::string& name,
                                             Rom* rom) {
  auto it = snapshots_.find(name);
  if (it == snapshots_.end()) {
    return absl::NotFoundError(absl::StrFormat("Snapshot not found: %s", name));
  }

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const ProjectSnapshot& snapshot = it->second;

  // Validate ROM checksum
  auto current_checksum = ProjectToolUtils::ComputeRomChecksum(rom);
  if (current_checksum != snapshot.rom_checksum) {
    return absl::FailedPreconditionError(
        "ROM checksum mismatch - snapshot is for a different ROM");
  }

  // Apply edits
  for (const auto& edit : snapshot.edits) {
    for (size_t i = 0; i < edit.new_value.size(); ++i) {
      rom->WriteByte(edit.address + i, edit.new_value[i]);
    }
  }

  return absl::OkStatus();
}

std::vector<std::string> ProjectManager::ListSnapshots() const {
  std::vector<std::string> names;
  for (const auto& [name, _] : snapshots_) {
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

absl::StatusOr<ProjectSnapshot> ProjectManager::GetSnapshot(
    const std::string& name) const {
  auto it = snapshots_.find(name);
  if (it == snapshots_.end()) {
    return absl::NotFoundError(absl::StrFormat("Snapshot not found: %s", name));
  }
  return it->second;
}

absl::Status ProjectManager::DeleteSnapshot(const std::string& name) {
  auto it = snapshots_.find(name);
  if (it == snapshots_.end()) {
    return absl::NotFoundError(absl::StrFormat("Snapshot not found: %s", name));
  }

  // Delete file
  std::string filepath = GetSnapshotFilePath(name);
  try {
    fs::remove(filepath);
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to delete snapshot file: %s", e.what()));
  }

  snapshots_.erase(it);
  return SaveProjectMetadata();
}

absl::Status ProjectManager::ExportProject(const std::string& export_path,
                                           bool include_rom) {
  // TODO: Implement project export (create tar/zip archive)
  return absl::UnimplementedError("Project export not yet implemented");
}

absl::Status ProjectManager::ImportProject(const std::string& archive_path) {
  // TODO: Implement project import (extract tar/zip archive)
  return absl::UnimplementedError("Project import not yet implemented");
}

absl::StatusOr<std::string> ProjectManager::DiffSnapshots(
    const std::string& snapshot1, const std::string& snapshot2) const {
  auto snap1_result = GetSnapshot(snapshot1);
  if (!snap1_result.ok()) {
    return snap1_result.status();
  }

  auto snap2_result = GetSnapshot(snapshot2);
  if (!snap2_result.ok()) {
    return snap2_result.status();
  }

  const ProjectSnapshot& snap1 = *snap1_result;
  const ProjectSnapshot& snap2 = *snap2_result;

  std::ostringstream diff;
  diff << absl::StrFormat("Comparing snapshots:\n");
  diff << absl::StrFormat("  %s (%zu edits)\n", snapshot1, snap1.edits.size());
  diff << absl::StrFormat("  %s (%zu edits)\n", snapshot2, snap2.edits.size());
  diff << "\n";

  // Build address maps for comparison
  std::map<uint32_t, const RomEdit*> snap1_map;
  std::map<uint32_t, const RomEdit*> snap2_map;

  for (const auto& edit : snap1.edits) {
    snap1_map[edit.address] = &edit;
  }
  for (const auto& edit : snap2.edits) {
    snap2_map[edit.address] = &edit;
  }

  // Find differences
  std::set<uint32_t> all_addresses;
  for (const auto& [addr, _] : snap1_map) {
    all_addresses.insert(addr);
  }
  for (const auto& [addr, _] : snap2_map) {
    all_addresses.insert(addr);
  }

  int added = 0, removed = 0, modified = 0;

  for (uint32_t addr : all_addresses) {
    bool in_snap1 = snap1_map.find(addr) != snap1_map.end();
    bool in_snap2 = snap2_map.find(addr) != snap2_map.end();

    if (in_snap1 && !in_snap2) {
      diff << absl::StrFormat("- Removed at 0x%06X\n", addr);
      removed++;
    } else if (!in_snap1 && in_snap2) {
      diff << absl::StrFormat("+ Added at 0x%06X\n", addr);
      added++;
    } else if (in_snap1 && in_snap2) {
      const auto& edit1 = *snap1_map[addr];
      const auto& edit2 = *snap2_map[addr];

      if (edit1.new_value != edit2.new_value) {
        diff << absl::StrFormat("~ Modified at 0x%06X\n", addr);
        modified++;
      }
    }
  }

  diff << absl::StrFormat("\nSummary: +%d -%d ~%d\n", added, removed, modified);

  return diff.str();
}

absl::Status ProjectManager::LoadSnapshots() {
  try {
    if (!fs::exists(snapshots_path_)) {
      return absl::OkStatus();
    }

    for (const auto& entry : fs::directory_iterator(snapshots_path_)) {
      if (entry.path().extension() == ".edits") {
        auto snapshot_result =
            ProjectSnapshot::LoadFromFile(entry.path().string());
        if (snapshot_result.ok()) {
          ProjectSnapshot& snapshot = *snapshot_result;
          snapshots_[snapshot.name] = snapshot;
        }
      }
    }

    return absl::OkStatus();

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to load snapshots: %s", e.what()));
  }
}

absl::Status ProjectManager::SaveProjectMetadata() {
  try {
    std::string metadata_path =
        (fs::path(project_path_) / "project.json").string();

    json metadata = {{"version", "1.0"},
                     {"updated", ProjectToolUtils::FormatTimestamp(
                                     std::chrono::system_clock::now())},
                     {"snapshots", json::array()}};

    for (const auto& [name, snapshot] : snapshots_) {
      metadata["snapshots"].push_back({
          {"name", name},
          {"created", ProjectToolUtils::FormatTimestamp(snapshot.created)},
          {"edit_count", snapshot.edits.size()},
      });
    }

    std::ofstream file(metadata_path);
    file << metadata.dump(2);

    return absl::OkStatus();

  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to save metadata: %s", e.what()));
  }
}

std::string ProjectManager::GetSnapshotFilePath(const std::string& name) const {
  // Sanitize name for filename
  std::string safe_name = name;
  std::replace(safe_name.begin(), safe_name.end(), ' ', '_');
  std::replace(safe_name.begin(), safe_name.end(), '/', '_');
  std::replace(safe_name.begin(), safe_name.end(), '\\', '_');

  return (fs::path(snapshots_path_) / (safe_name + ".edits")).string();
}

// ============================================================================
// ProjectToolBase Implementation
// ============================================================================

absl::StatusOr<ProjectManager*> ProjectToolBase::GetProjectManager(
    AgentContext* /*context*/) const {
  // For now, create a temporary project manager
  // In a full implementation, this would be stored in AgentContext
  static ProjectManager manager;
  return &manager;
}

std::string ProjectToolBase::FormatEdits(
    const std::vector<RomEdit>& edits) const {
  std::ostringstream oss;
  for (const auto& edit : edits) {
    oss << absl::StrFormat("  Address: 0x%06X, Length: %zu bytes, %s\n",
                           edit.address, edit.new_value.size(),
                           edit.description);
  }
  return oss.str();
}

std::string ProjectToolBase::FormatSnapshot(
    const ProjectSnapshot& snapshot) const {
  std::ostringstream oss;
  oss << absl::StrFormat("Snapshot: %s\n", snapshot.name);
  oss << absl::StrFormat("Description: %s\n", snapshot.description);
  oss << absl::StrFormat("Created: %s\n",
                         ProjectToolUtils::FormatTimestamp(snapshot.created));
  oss << absl::StrFormat("Edits: %zu\n", snapshot.edits.size());
  oss << absl::StrFormat("ROM Checksum: %s\n", ProjectToolUtils::FormatChecksum(
                                                   snapshot.rom_checksum));
  return oss.str();
}

// ============================================================================
// Tool Implementations
// ============================================================================

absl::Status ProjectStatusTool::Execute(
    Rom* rom, const resources::ArgumentParser& /*parser*/,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Project Status");

  // Get project manager (would come from context in full implementation)
  auto manager_result = GetProjectManager(nullptr);
  if (!manager_result.ok()) {
    return manager_result.status();
  }

  ProjectManager* manager = *manager_result;

  if (manager->IsInitialized()) {
    formatter.AddField("project_path", manager->GetProjectPath());
    formatter.AddField("initialized", true);

    auto snapshots = manager->ListSnapshots();
    formatter.AddField("snapshot_count", static_cast<int>(snapshots.size()));

    formatter.BeginArray("snapshots");
    for (const auto& name : snapshots) {
      formatter.AddArrayItem(name);
    }
    formatter.EndArray();
  } else {
    formatter.AddField("initialized", false);
    formatter.AddField("message",
                       "No project initialized. Use project-snapshot to create "
                       "first snapshot.");
  }

  if (rom && rom->is_loaded()) {
    auto checksum = ProjectToolUtils::ComputeRomChecksum(rom);
    formatter.AddField("rom_checksum",
                       ProjectToolUtils::FormatChecksum(checksum));
    formatter.AddField("rom_size", static_cast<uint64_t>(rom->size()));
  }

  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status ProjectSnapshotTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  auto name = parser.GetString("name");
  if (!name.has_value()) {
    return absl::InvalidArgumentError("Missing --name argument");
  }

  auto description = parser.GetString("description").value_or("");

  // Get project manager
  auto manager_result = GetProjectManager(nullptr);
  if (!manager_result.ok()) {
    return manager_result.status();
  }
  ProjectManager* manager = *manager_result;

  // Initialize project if not already done
  if (!manager->IsInitialized()) {
    auto init_status = manager->Initialize(".");
    if (!init_status.ok()) {
      return init_status;
    }
  }

  // Get edits from context (empty for now - would come from AgentContext)
  std::vector<RomEdit> edits;

  // Compute ROM checksum
  auto checksum = ProjectToolUtils::ComputeRomChecksum(rom);

  // Create snapshot
  auto status = manager->CreateSnapshot(*name, description, edits, checksum);
  if (!status.ok()) {
    return status;
  }

  formatter.BeginObject("Snapshot Created");
  formatter.AddField("name", *name);
  formatter.AddField("description", description);
  formatter.AddField("edit_count", static_cast<int>(edits.size()));
  formatter.AddField("rom_checksum",
                     ProjectToolUtils::FormatChecksum(checksum));
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status ProjectRestoreTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  auto name = parser.GetString("name");
  if (!name.has_value()) {
    return absl::InvalidArgumentError("Missing --name argument");
  }

  auto manager_result = GetProjectManager(nullptr);
  if (!manager_result.ok()) {
    return manager_result.status();
  }
  ProjectManager* manager = *manager_result;

  if (!manager->IsInitialized()) {
    return absl::FailedPreconditionError("No project initialized");
  }

  auto status = manager->RestoreSnapshot(*name, rom);
  if (!status.ok()) {
    return status;
  }

  auto snapshot_result = manager->GetSnapshot(*name);
  if (!snapshot_result.ok()) {
    return snapshot_result.status();
  }

  const ProjectSnapshot& snapshot = *snapshot_result;

  formatter.BeginObject("Snapshot Restored");
  formatter.AddField("name", snapshot.name);
  formatter.AddField("edits_applied", static_cast<int>(snapshot.edits.size()));
  formatter.AddField("description", snapshot.description);
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status ProjectExportTool::Execute(Rom* /*rom*/,
                                        const resources::ArgumentParser& parser,
                                        resources::OutputFormatter& formatter) {
  auto path = parser.GetString("path");
  if (!path.has_value()) {
    return absl::InvalidArgumentError("Missing --path argument");
  }

  bool include_rom = parser.HasFlag("include-rom");

  auto manager_result = GetProjectManager(nullptr);
  if (!manager_result.ok()) {
    return manager_result.status();
  }
  ProjectManager* manager = *manager_result;

  auto status = manager->ExportProject(*path, include_rom);
  if (!status.ok()) {
    return status;
  }

  formatter.BeginObject("Project Exported");
  formatter.AddField("path", *path);
  formatter.AddField("include_rom", include_rom);
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status ProjectImportTool::Execute(Rom* /*rom*/,
                                        const resources::ArgumentParser& parser,
                                        resources::OutputFormatter& formatter) {
  auto path = parser.GetString("path");
  if (!path.has_value()) {
    return absl::InvalidArgumentError("Missing --path argument");
  }

  auto manager_result = GetProjectManager(nullptr);
  if (!manager_result.ok()) {
    return manager_result.status();
  }
  ProjectManager* manager = *manager_result;

  auto status = manager->ImportProject(*path);
  if (!status.ok()) {
    return status;
  }

  formatter.BeginObject("Project Imported");
  formatter.AddField("path", *path);
  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status ProjectDiffTool::Execute(Rom* /*rom*/,
                                      const resources::ArgumentParser& parser,
                                      resources::OutputFormatter& formatter) {
  auto snapshot1 = parser.GetString("snapshot1");
  auto snapshot2 = parser.GetString("snapshot2");

  if (!snapshot1.has_value() || !snapshot2.has_value()) {
    return absl::InvalidArgumentError("Missing snapshot names");
  }

  auto manager_result = GetProjectManager(nullptr);
  if (!manager_result.ok()) {
    return manager_result.status();
  }
  ProjectManager* manager = *manager_result;

  if (!manager->IsInitialized()) {
    return absl::FailedPreconditionError("No project initialized");
  }

  auto diff_result = manager->DiffSnapshots(*snapshot1, *snapshot2);
  if (!diff_result.ok()) {
    return diff_result.status();
  }

  formatter.BeginObject("Snapshot Diff");
  formatter.AddField("snapshot1", *snapshot1);
  formatter.AddField("snapshot2", *snapshot2);
  formatter.AddField("diff", *diff_result);
  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
