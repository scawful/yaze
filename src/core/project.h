#ifndef YAZE_CORE_PROJECT_H
#define YAZE_CORE_PROJECT_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "core/features.h"
#include "core/hack_manifest.h"
#include "core/rom_settings.h"
#include "core/sprite_asset.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace core {
class LyraTerrainDocument;
}
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

/**
 * @brief Project rules for graphics sheets (`[graphics_sheets]`).
 *
 * reserved_sheets are never written and never offered as free space (merged
 * with the hack manifest's graphics_sheet_regions.reserved_sheets).
 * flagged_sheets look free but need the owner's approval before art goes in.
 * reserved_blocks maps a sheet to 16x16 block indices (0-15, row-major over
 * the 8x2 block grid) that must keep their current pixels.
 */
struct GraphicsSheetSettings {
  std::vector<uint16_t> reserved_sheets;
  std::vector<uint16_t> flagged_sheets;
  std::map<uint16_t, std::vector<uint16_t>> reserved_blocks;

  bool empty() const {
    return reserved_sheets.empty() && flagged_sheets.empty() &&
           reserved_blocks.empty();
  }
};

/**
 * @brief Atlas map views (`[atlas]`).
 *
 * Paths are kept as written in the project file (normally relative to it);
 * use YazeProject::AtlasAnnotationPaths() and AtlasSymbolsPath() to resolve
 * them. Values are not validated here: the Atlas loader reports a bad
 * `defaults` value or a missing file when it runs, and the project still
 * opens.
 */
struct AtlasSettings {
  // Annotation files, merged in order (yaze.atlas/1 or the Oracle of Secrets
  // room/overworld files). `annotations=a.json,b.json`.
  std::vector<std::string> annotations;
  // "vanilla" (built-in vanilla names and colors) or "none".
  std::string defaults = "vanilla";
  // Optional .sym for symbol links. Empty: the .sym next to the ROM, then
  // [files] symbols_filename.
  std::string symbols;
  // Folder for the editor's Export buttons. Empty: the project folder.
  std::string export_folder;
  // The loaded file had an [atlas] section; save writes it back even when
  // every value is the default.
  bool section_present = false;

  bool IsDefault() const {
    return annotations.empty() && defaults == "vanilla" && symbols.empty() &&
           export_folder.empty();
  }
  bool operator==(const AtlasSettings& other) const {
    return annotations == other.annotations && defaults == other.defaults &&
           symbols == other.symbols && export_folder == other.export_folder;
  }
};

/**
 * @brief Planning editor sidecar (`[planning]`).
 *
 * `file` names a yaze.plan/1 JSON file (core/project_plan.h), absolute or
 * relative to the project file; use YazeProject::PlanningFilePath() to
 * resolve it. The plan may live outside the repository (for example in a
 * shared notes folder) so several checkouts can share one plan.
 */
struct PlanningSettings {
  std::string file;
  // The loaded file had a [planning] section; save writes it back even when
  // `file` is empty.
  bool section_present = false;

  bool IsDefault() const { return file.empty(); }
  bool operator==(const PlanningSettings& other) const {
    return file == other.file;
  }
};

/**
 * @brief Lines of a `.yaze` file that this build does not model.
 *
 * ParseFromString records them and SerializeToString writes them back, so a
 * project file written by a newer build, another tool or by hand survives a
 * save by this build:
 * - sections this build does not know: verbatim, in file order, after the
 *   known sections;
 * - keys this build does not know inside a known section: verbatim, in file
 *   order, at the end of that section;
 * - comments: before the key they preceded in the file, else at the end of
 *   their section.
 * Blank lines and the generated header and footer comments are not kept.
 */
struct ProjectFilePassthrough {
  struct Line {
    enum class Kind : uint8_t { kComment, kKnownKey, kUnknownLine };
    Kind kind = Kind::kComment;
    std::string key;   // kKnownKey and kUnknownLine (empty without '=').
    std::string text;  // The line as written, without its line ending.
  };
  struct Section {
    std::string name;    // Without brackets. "" = before the first section.
    bool known = false;  // A section this build parses.
    std::vector<Line> lines;
  };
  std::vector<Section> sections;  // File order, one entry per section name.

  bool empty() const {
    for (const auto& section : sections) {
      for (const auto& line : section.lines) {
        if (line.kind != Line::Kind::kKnownKey) {
          return false;
        }
      }
    }
    return true;
  }
};

enum class RomRole { kBase, kDev, kPatched, kRelease };
// kBlock rejects identity/protected-range conflicts; kReadOnly forbids ROM writes.
enum class RomWritePolicy { kAllow, kWarn, kBlock, kReadOnly };

struct RomMetadata {
  RomRole role = RomRole::kDev;
  std::string expected_hash;  // CRC32 or SHA1 (hex)
  RomWritePolicy write_policy = RomWritePolicy::kWarn;
};

struct Z3dkMemoryRange {
  uint32_t start = 0;
  uint32_t end = 0;
  std::string reason;
};

struct Z3dkArtifactPaths {
  std::string symbols_mlb;
  std::string sourcemap_json;
  std::string annotations_json;
  std::string hooks_json;
  std::string lint_json;
};

struct Z3dkSettings {
  bool loaded = false;
  std::string config_path;
  std::string preset;
  std::vector<std::string> include_paths;
  std::vector<std::pair<std::string, std::string>> defines;
  std::vector<std::string> emits;
  std::vector<std::string> main_files;
  std::string std_includes_path;
  std::string std_defines_path;
  std::string mapper;
  int rom_size = 0;
  std::string symbols_format;
  std::string symbols_path;
  std::vector<Z3dkMemoryRange> prohibited_memory_ranges;
  std::optional<bool> lsp_log_enabled;
  std::string lsp_log_path;
  bool warn_unused_symbols = true;
  bool warn_branch_outside_bank = true;
  bool warn_unknown_width = true;
  bool warn_org_collision = true;
  bool warn_unauthorized_hook = true;
  bool warn_stack_balance = true;
  bool warn_hook_return = true;
  std::string rom_path;
  Z3dkArtifactPaths artifact_paths;
};

std::string RomRoleToString(RomRole role);
RomRole ParseRomRole(absl::string_view value);
std::string RomWritePolicyToString(RomWritePolicy policy);
RomWritePolicy ParseRomWritePolicy(absl::string_view value);

// Native files use atomic replacement. Browser files use synchronous VFS
// replacement or exclusive creation. When replace_existing is false, an
// existing destination is rejected without mutation.
absl::Status WriteProjectFileAtomically(absl::string_view target_path,
                                        absl::string_view contents,
                                        bool replace_existing);

/**
 * @struct YazeProject
 * @brief Modern project structure with comprehensive settings consolidation
 */
struct YazeProject {
  YazeProject() = default;
  YazeProject(const YazeProject&) = default;
  YazeProject(YazeProject&&) = default;
  YazeProject& operator=(const YazeProject&) = default;
  YazeProject& operator=(YazeProject&&) = default;
  // Open() and InitializeResourceLabelProvider() point the process-wide
  // zelda3::GetResourceLabels() at this project's members; the destructor
  // releases any binding that still refers to them.
  ~YazeProject();

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
  // Overworld cutscene camera/actor shots ([files] cutscene_shots). Empty:
  // the Cutscene Camera window is read-only and offers Copy JSON.
  std::string cutscene_shots;
  // Optional read-only raw DMA graphics preview manifest, relative to project.
  std::string overworld_graphics_preview;
  // Read-only literal ASM tile patterns for the standard dungeon renderer.
  std::string dungeon_tile_patterns;
  // Tracked custom collision source kept in step with the ROM on every save
  // ([files] custom_collision_json). Empty: collision lives only in the ROM.
  std::string custom_collision_json;
  std::string
      custom_objects_folder;  // Folder containing custom object .bin files
  std::string
      hack_manifest_file;  // Path to hack_manifest.json (ASM integration)
  // Opt-in, read-only sprite catalog. Both paths resolve relative to the project.
  std::string sprite_catalog_file;
  std::string sprite_source_root;
  std::vector<SpriteAssetBinding> sprite_assets;

  // Optional game-independent terrain labels, stored outside the ROM.
  std::string terrain_catalog_file;

  // Optional editable Lyra terrain document, independent of ROM map storage.
  std::string lyra_terrain_file;

  // Optional producer request; saving does not refresh its source bindings.
  std::string lyra_build_request_file;

  // Optional build-transfer control references. Save preserves their paths,
  // but does not create, copy, or refresh their contents. The canonical Save
  // receipt remains adjacent to the descriptor, even with a legacy reference.
  std::string native_save_producer_file;
  std::string native_save_transfer_proof_file;
  std::string native_save_receipt_file;

  // Optional read-only palette names, contextual sprite uses and provenance.
  std::string palette_catalog_file;

  // Optional custom object file mapping (object_id -> filenames per subtype).
  std::unordered_map<int, std::vector<std::string>> custom_object_files;

  // Consolidated settings (previously scattered across multiple files)
  core::FeatureFlags::Flags feature_flags;
  WorkspaceSettings workspace_settings;
  DungeonOverlaySettings dungeon_overlay;
  GraphicsSheetSettings graphics_sheets;
  AtlasSettings atlas;
  PlanningSettings planning;
  core::RomAddressOverrides rom_address_overrides;
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      resource_labels;

  // Embedded labels flag - when true, resource_labels contains all default
  // Zelda3 labels
  bool use_embedded_labels = true;

  // ASM hack integration manifest (loaded from hack_manifest_file)
  core::HackManifest hack_manifest;

  // Optional z3dk project configuration discovered from z3dk.toml.
  Z3dkSettings z3dk_settings;

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

  // Unknown sections, unknown keys and comments from the loaded .yaze file,
  // written back on save (see ProjectFilePassthrough).
  ProjectFilePassthrough file_passthrough;

  // Bundle resolution
  // Walk up the path hierarchy to find the first ancestor directory whose
  // filename ends with ".yazeproj". If `path` itself is a .yazeproj directory,
  // returns it as-is. Returns an empty string when no bundle root is found.
  static std::string ResolveBundleRoot(const std::string& path);

  // Methods
  absl::Status Create(const std::string& project_name,
                      const std::string& base_path);
  absl::Status Open(const std::string& project_path);
  // Parse an in-memory native project descriptor as if it lived at
  // `project_path`. Used by the raw project editor to validate and adopt a
  // draft before it replaces the active descriptor on disk.
  absl::Status LoadFromString(const std::string& content,
                              const std::string& project_path);
  // Checks the persisted source/build settings against this live context.
  // UI layout, timestamps and recent-file history do not qualify build inputs.
  absl::Status ValidateSavedSourceBindings(const std::string& content) const;
  // Returns no selection when lyra_build_request_file is empty. Native-save
  // producer/proof references alone describe capability, not expanded admission.
  // Expanded claims require the existing completed build and its current
  // hash-bound Save/transfer evidence. Failure must not use a manifest fallback.
  // This does not compile, grant allocation, or create completion evidence.
  absl::StatusOr<std::optional<std::string>> ResolveExpandedBuildRunTarget(
      const std::string& editing_source_path) const;
  // Serializes without writing, for coordinated descriptor/sidecar publication.
  absl::StatusOr<std::string> SerializeToString() const;
  absl::Status Save();
  // Create a new descriptor without replacing a file that appeared after the
  // caller's preflight check.
  absl::Status SaveNew();
  absl::Status SaveAs(const std::string& new_path);
  // Saves an independent project-owned terrain copy without modifying the
  // original terrain. Both destination files roll back together on failure.
  absl::Status SaveAsWithLyraTerrain(
      const std::string& new_path, const core::LyraTerrainDocument& terrain,
      const std::optional<std::string>& expected_source_bytes);
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
  // Normalize filesystem paths to absolute paths based on the project file
  // location. This avoids relying on the process working directory when
  // opening ROMs/projects (important for iOS and portable bundles).
  void NormalizePathsToAbsolute();
  std::string GetDisplayName() const;
  std::string GetRelativePath(const std::string& absolute_path) const;
  std::string GetAbsolutePath(const std::string& relative_path) const;
  // [atlas] annotations / symbols resolved against the project file.
  std::vector<std::string> AtlasAnnotationPaths() const;
  std::string AtlasSymbolsPath() const;
  // [planning] file resolved against the project file; empty when unset.
  std::string PlanningFilePath() const;
  bool IsEmpty() const;
  std::string MakeStorageKey(absl::string_view suffix) const;

  // Project state
  bool project_opened() const { return !name.empty() && !filepath.empty(); }
  // Manifest identity, independent of the project filename or display name.
  bool IsOracleCompatible() const;

  // Hack manifest (ASM integration) reload hook (safe no-op if unset).
  void ReloadHackManifest();
  void ReloadZ3dkSettings();
  bool HasZ3dkConfig() const { return z3dk_settings.loaded; }
  std::string GetZ3dkArtifactPath(absl::string_view artifact_name) const;

 private:
  absl::Status ParseFromString(const std::string& content);
  absl::Status LoadFromYazeFormat(const std::string& project_path);
  absl::Status SaveToYazeFormat(bool replace_existing = true);
  absl::Status ImportFromZScreamFormat(const std::string& project_path);

#ifdef YAZE_ENABLE_JSON_PROJECT_FORMAT
  absl::Status LoadFromJsonFormat(const std::string& project_path);
  absl::Status SaveToJsonFormat();
#endif

  void InitializeDefaults();
  void TryLoadZ3dkConfig();
  void TryLoadHackManifest();
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
    BumpGeneration();
  }

  void RemoveFile(const std::string& file_path) {
    auto it = std::find(recent_files_.begin(), recent_files_.end(), file_path);
    if (it != recent_files_.end()) {
      recent_files_.erase(it);
      BumpGeneration();
    }
  }

  void Save();

  void Load();

  const std::vector<std::string>& GetRecentFiles() const {
    return recent_files_;
  }

  void Clear() {
    recent_files_.clear();
    BumpGeneration();
  }

  /// Bumps when recent file list changes (add/remove/clear/load).
  [[nodiscard]] std::uint64_t GetGeneration() const { return generation_; }

 private:
  RecentFilesManager() {
    Load();  // Load on construction
  }

  std::string GetFilePath() const;

  void BumpGeneration() { ++generation_; }

  std::vector<std::string> recent_files_;
  std::uint64_t generation_ = 0;
};

}  // namespace project
}  // namespace yaze

#endif  // YAZE_CORE_PROJECT_H
