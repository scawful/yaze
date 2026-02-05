#ifndef YAZE_CORE_PROJECT_H
#define YAZE_CORE_PROJECT_H

#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "core/features.h"
#include "core/rom_settings.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace project {

/**
 * @enum ProjectFormat
 * @brief Supported project file formats
 */
enum class ProjectFormat {
  kYazeNative,    // .yaze - YAZE native format
  kZScreamCompat  // .zsproj - ZScream compatibility format
};

/**
 * @struct ProjectMetadata
 * @brief Enhanced metadata for project tracking
 */
struct ProjectMetadata {
  std::string version = "2.0";
  std::string created_by = "YAZE";
  std::string created_date;
  std::string last_modified;
  std::string yaze_version;
  std::string description;
  std::vector<std::string> tags;
  std::string author;
  std::string license;
  std::string project_id;

  // ZScream compatibility
  bool zscream_compatible = false;
  std::string zscream_version;
};

/**
 * @struct WorkspaceSettings
 * @brief Consolidated workspace and UI settings
 */
struct WorkspaceSettings {
  // Display settings
  float font_global_scale = 1.0f;
  bool dark_mode = true;
  std::string ui_theme = "default";

  // Layout settings
  std::string last_layout_preset;
  std::vector<std::string> saved_layouts;
  std::string window_layout_data;  // ImGui .ini data

  // Editor preferences
  bool autosave_enabled = true;
  float autosave_interval_secs = 300.0f;  // 5 minutes
  bool backup_on_save = true;
  bool show_grid = true;
  bool show_collision = false;

  // Backup management
  int backup_retention_count = 20;  // Keep last N backups (0 = unlimited)
  bool backup_keep_daily = true;    // Keep one backup per day
  int backup_keep_daily_days = 14;  // Days to retain daily snapshots

  // Label preferences
  bool prefer_hmagic_names = true;  // Prefer Hyrule Magic sprite names

  // Advanced settings
  std::map<std::string, std::string> custom_keybindings;
  std::vector<std::string> recent_files;
  std::map<std::string, bool> editor_visibility;
};

/**
 * @struct DungeonOverlaySettings
 * @brief Dungeon overlay configuration (per-project).
 */
struct DungeonOverlaySettings {
  // Collision tile IDs used by minecart tracks and interactions.
  std::vector<uint16_t> track_tiles;
  std::vector<uint16_t> track_stop_tiles;
  std::vector<uint16_t> track_switch_tiles;

  // Object/Sprite IDs used for minecart tooling and validation.
  std::vector<uint16_t> track_object_ids;
  std::vector<uint16_t> minecart_sprite_ids;
};

enum class RomRole { kBase, kDev, kPatched, kRelease };
enum class RomWritePolicy { kAllow, kWarn, kBlock };

struct RomMetadata {
  RomRole role = RomRole::kDev;
  std::string expected_hash;  // CRC32 or SHA1 (hex)
  RomWritePolicy write_policy = RomWritePolicy::kWarn;
};

std::string RomRoleToString(RomRole role);
RomRole ParseRomRole(absl::string_view value);
std::string RomWritePolicyToString(RomWritePolicy policy);
RomWritePolicy ParseRomWritePolicy(absl::string_view value);

/**
 * @struct YazeProject
 * @brief Modern project structure with comprehensive settings consolidation
 */
struct YazeProject {
  // Basic project info
  ProjectMetadata metadata;
  std::string name;
  std::string filepath;
  ProjectFormat format = ProjectFormat::kYazeNative;

  // ROM and resources
  std::string rom_filename;
  std::string rom_backup_folder;
  std::vector<std::string> additional_roms;  // For multi-ROM projects
  RomMetadata rom_metadata;

  // Code and assets
  std::string code_folder;
  std::string assets_folder;
  std::string patches_folder;
  std::string labels_filename;
  std::string symbols_filename;
  std::string
      custom_objects_folder;  // Folder containing custom object .bin files

  // Optional custom object file mapping (object_id -> filenames per subtype).
  std::unordered_map<int, std::vector<std::string>> custom_object_files;

  // Consolidated settings (previously scattered across multiple files)
  core::FeatureFlags::Flags feature_flags;
  WorkspaceSettings workspace_settings;
  DungeonOverlaySettings dungeon_overlay;
  core::RomAddressOverrides rom_address_overrides;
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      resource_labels;

  // Embedded labels flag - when true, resource_labels contains all default
  // Zelda3 labels
  bool use_embedded_labels = true;

  // Build and deployment
  std::string build_script;
  std::string output_folder;
  std::vector<std::string> build_configurations;
  std::string build_target;
  std::string asm_entry_point;
  std::vector<std::string> asm_sources;

  // Version control integration
  std::string git_repository;
  bool track_changes = true;
  std::string last_build_hash;
  int build_number = 0;

  // AI Agent Settings
  struct AgentSettings {
    std::string ai_provider =
        "auto";            // auto, gemini, anthropic, openai, ollama, mock
    std::string ai_model;  // e.g., "gemini-2.5-flash", "llama3:latest"
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;  // Optional, can use env var
    std::string
        custom_system_prompt;  // Path to custom prompt (relative to project)
    bool use_custom_prompt = false;
    bool show_reasoning = true;
    bool verbose = false;
    int max_tool_iterations = 4;
    int max_retry_attempts = 3;
    float temperature = 0.25f;
    float top_p = 0.95f;
    int max_output_tokens = 2048;
    bool stream_responses = false;
    std::vector<std::string> favorite_models;
    std::vector<std::string> model_chain;
    int chain_mode = 0;
    bool enable_tool_resources = true;
    bool enable_tool_dungeon = true;
    bool enable_tool_overworld = true;
    bool enable_tool_messages = true;
    bool enable_tool_dialogue = true;
    bool enable_tool_gui = true;
    bool enable_tool_music = true;
    bool enable_tool_sprite = true;
    bool enable_tool_emulator = true;
    bool enable_tool_memory_inspector = true;
    std::string builder_blueprint_path;  // Saved agent builder configuration
  } agent_settings;

  // ZScream compatibility (for importing existing projects)
  std::string zscream_project_file;  // Path to original .zsproj if importing
  std::map<std::string, std::string> zscream_mappings;  // Field mappings

  // WASM persistence (project + music state in IndexedDB)
  struct MusicPersistence {
    bool persist_custom_music = true;
    std::string storage_key;
    std::string last_saved_at;
  } music_persistence;

  // Methods
  absl::Status Create(const std::string& project_name,
                      const std::string& base_path);
  absl::Status Open(const std::string& project_path);
  absl::Status Save();
  absl::Status SaveAs(const std::string& new_path);
  absl::Status ImportZScreamProject(const std::string& zscream_project_path);
  absl::Status ExportForZScream(const std::string& target_path);

  // Settings management
  absl::Status LoadAllSettings();
  absl::Status SaveAllSettings();
  absl::Status ResetToDefaults();

  // Labels management
  absl::Status InitializeEmbeddedLabels(
      const std::unordered_map<std::string,
                               std::unordered_map<std::string, std::string>>&
          labels);  // Load all default Zelda3 labels
  std::string GetLabel(const std::string& resource_type, int id,
                       const std::string& default_value = "") const;

  /**
   * @brief Import labels from a ZScream DefaultNames.txt file
   * @param filepath Path to the DefaultNames.txt file
   * @return Status indicating success or parse errors
   */
  absl::Status ImportLabelsFromZScream(const std::string& filepath);

  /**
   * @brief Import labels from ZScream format content directly
   * @param content The file content to parse
   * @return Status indicating success or parse errors
   */
  absl::Status ImportLabelsFromZScreamContent(const std::string& content);

  /**
   * @brief Initialize the global ResourceLabelProvider with this project's labels
   */
  void InitializeResourceLabelProvider();

  // Validation and integrity
  absl::Status Validate() const;
  std::vector<std::string> GetMissingFiles() const;
  absl::Status RepairProject();

  // Utilities
  std::string GetDisplayName() const;
  std::string GetRelativePath(const std::string& absolute_path) const;
  std::string GetAbsolutePath(const std::string& relative_path) const;
  bool IsEmpty() const;
  std::string MakeStorageKey(absl::string_view suffix) const;

  // Project state
  bool project_opened() const { return !name.empty() && !filepath.empty(); }

 private:
  absl::StatusOr<std::string> SerializeToString() const;
  absl::Status ParseFromString(const std::string& content);
  absl::Status LoadFromYazeFormat(const std::string& project_path);
  absl::Status SaveToYazeFormat();
  absl::Status ImportFromZScreamFormat(const std::string& project_path);

#ifdef YAZE_ENABLE_JSON_PROJECT_FORMAT
  absl::Status LoadFromJsonFormat(const std::string& project_path);
  absl::Status SaveToJsonFormat();
#endif

  void InitializeDefaults();
  std::string GenerateProjectId() const;
};

/**
 * @class ProjectManager
 * @brief Enhanced project management with templates and validation
 */
class ProjectManager {
 public:
  // Project templates
  struct ProjectTemplate {
    std::string name;
    std::string description;
    std::string icon;
    YazeProject template_project;
  };

  static std::vector<ProjectTemplate> GetProjectTemplates();
  static absl::StatusOr<YazeProject> CreateFromTemplate(
      const std::string& template_name, const std::string& project_name,
      const std::string& base_path);

  // Project discovery and management
  static std::vector<std::string> FindProjectsInDirectory(
      const std::string& directory);
  static absl::Status BackupProject(const YazeProject& project);
  static absl::Status RestoreProject(const std::string& backup_path);

  // Format conversion utilities
  static absl::Status ConvertProject(const std::string& source_path,
                                     const std::string& target_path,
                                     ProjectFormat target_format);

  // Validation and repair
  static absl::Status ValidateProjectStructure(const YazeProject& project);
  static std::vector<std::string> GetRecommendedFixesForProject(
      const YazeProject& project);
};

// Compatibility - ResourceLabelManager (still used by ROM class)
struct ResourceLabelManager {
  bool LoadLabels(const std::string& filename);
  bool SaveLabels();
  void DisplayLabels(bool* p_open);
  void EditLabel(const std::string& type, const std::string& key,
                 const std::string& newValue);
  void SelectableLabelWithNameEdit(bool selected, const std::string& type,
                                   const std::string& key,
                                   const std::string& defaultValue);
  std::string GetLabel(const std::string& type, const std::string& key);
  std::string CreateOrGetLabel(const std::string& type, const std::string& key,
                               const std::string& defaultValue);

  bool labels_loaded_ = false;
  std::string filename_;
  struct ResourceType {
    std::string key_name;
    std::string display_description;
  };

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      labels_;
};

// Compatibility - RecentFilesManager
const std::string kRecentFilesFilename = "recent_files.txt";

class RecentFilesManager {
 public:
  // Singleton pattern - get the global instance
  static RecentFilesManager& GetInstance() {
    static RecentFilesManager instance;
    return instance;
  }

  // Delete copy constructor and assignment operator
  RecentFilesManager(const RecentFilesManager&) = delete;
  RecentFilesManager& operator=(const RecentFilesManager&) = delete;

  void AddFile(const std::string& file_path) {
    // Add a file to the list, avoiding duplicates
    // Move to front if already exists (MRU - Most Recently Used)
    auto it = std::find(recent_files_.begin(), recent_files_.end(), file_path);
    if (it != recent_files_.end()) {
      recent_files_.erase(it);
    }
    recent_files_.insert(recent_files_.begin(), file_path);

    // Limit to 20 most recent files
    if (recent_files_.size() > 20) {
      recent_files_.resize(20);
    }
  }

  void RemoveFile(const std::string& file_path) {
    auto it = std::find(recent_files_.begin(), recent_files_.end(), file_path);
    if (it != recent_files_.end()) {
      recent_files_.erase(it);
    }
  }

  void Save();

  void Load();

  const std::vector<std::string>& GetRecentFiles() const {
    return recent_files_;
  }

  void Clear() { recent_files_.clear(); }

 private:
  RecentFilesManager() {
    Load();  // Load on construction
  }

  std::string GetFilePath() const;

  std::vector<std::string> recent_files_;
};

}  // namespace project
}  // namespace yaze

#endif  // YAZE_CORE_PROJECT_H
