import Foundation
import Combine

struct YazeSettings: Codable {
  struct General: Codable {
    var fontGlobalScale: Double = 1.0
    var backupRom: Bool = false
    var saveNewAuto: Bool = true
    var autosaveEnabled: Bool = true
    var autosaveInterval: Double = 300.0
    var recentFilesLimit: Int = 10
    var lastRomPath: String = ""
    var lastProjectPath: String = ""
    var showWelcomeOnStartup: Bool = true
    var restoreLastSession: Bool = true
    var preferHmagicSpriteNames: Bool = true

    enum CodingKeys: String, CodingKey {
      case fontGlobalScale = "font_global_scale"
      case backupRom = "backup_rom"
      case saveNewAuto = "save_new_auto"
      case autosaveEnabled = "autosave_enabled"
      case autosaveInterval = "autosave_interval"
      case recentFilesLimit = "recent_files_limit"
      case lastRomPath = "last_rom_path"
      case lastProjectPath = "last_project_path"
      case showWelcomeOnStartup = "show_welcome_on_startup"
      case restoreLastSession = "restore_last_session"
      case preferHmagicSpriteNames = "prefer_hmagic_sprite_names"
    }
  }

  struct Editor: Codable {
    var backupBeforeSave: Bool = true
    var defaultEditor: Int = 0

    enum CodingKeys: String, CodingKey {
      case backupBeforeSave = "backup_before_save"
      case defaultEditor = "default_editor"
    }
  }

  struct Performance: Codable {
    var vsync: Bool = true
    var targetFps: Int = 60
    var cacheSizeMb: Int = 512
    var undoHistorySize: Int = 50

    enum CodingKeys: String, CodingKey {
      case vsync
      case targetFps = "target_fps"
      case cacheSizeMb = "cache_size_mb"
      case undoHistorySize = "undo_history_size"
    }
  }

  struct AiHost: Codable, Identifiable {
    var id: String
    var label: String
    var baseUrl: String
    var apiType: String
    var supportsVision: Bool
    var supportsTools: Bool
    var supportsStreaming: Bool
    var allowInsecure: Bool
    var credentialId: String

    enum CodingKeys: String, CodingKey {
      case id
      case label
      case baseUrl = "base_url"
      case apiType = "api_type"
      case supportsVision = "supports_vision"
      case supportsTools = "supports_tools"
      case supportsStreaming = "supports_streaming"
      case allowInsecure = "allow_insecure"
      case credentialId = "credential_id"
    }
  }

  struct AiProfile: Codable, Identifiable {
    var id: String { name }
    var name: String
    var model: String
    var temperature: Double
    var topP: Double
    var maxOutputTokens: Int
    var supportsVision: Bool
    var supportsTools: Bool

    enum CodingKeys: String, CodingKey {
      case name
      case model
      case temperature
      case topP = "top_p"
      case maxOutputTokens = "max_output_tokens"
      case supportsVision = "supports_vision"
      case supportsTools = "supports_tools"
    }
  }

  struct Ai: Codable {
    var provider: Int = 0
    var model: String = ""
    var ollamaUrl: String = "http://localhost:11434"
    var geminiApiKey: String = ""
    var temperature: Double = 0.25
    var maxTokens: Int = 2048
    var proactive: Bool = true
    var autoLearn: Bool = true
    var multimodal: Bool = true
    var hosts: [AiHost] = []
    var activeHostId: String = ""
    var profiles: [AiProfile] = []
    var activeProfile: String = ""
    var remoteBuildHostId: String = ""

    enum CodingKeys: String, CodingKey {
      case provider
      case model
      case ollamaUrl = "ollama_url"
      case geminiApiKey = "gemini_api_key"
      case temperature
      case maxTokens = "max_tokens"
      case proactive
      case autoLearn = "auto_learn"
      case multimodal
      case hosts
      case activeHostId = "active_host_id"
      case profiles
      case activeProfile = "active_profile"
      case remoteBuildHostId = "remote_build_host_id"
    }
  }

  struct Logging: Codable {
    var level: Int = 1
    var toFile: Bool = false
    var filePath: String = ""
    var aiRequests: Bool = true
    var romOperations: Bool = true
    var guiAutomation: Bool = true
    var proposals: Bool = true

    enum CodingKeys: String, CodingKey {
      case level
      case toFile = "to_file"
      case filePath = "file_path"
      case aiRequests = "ai_requests"
      case romOperations = "rom_operations"
      case guiAutomation = "gui_automation"
      case proposals
    }
  }

  struct Shortcuts: Codable {
    var panel: [String: String] = [:]
    var global: [String: String] = [:]
    var editor: [String: String] = [:]
  }

  struct Sidebar: Codable {
    var visible: Bool = true
    var panelExpanded: Bool = true
    var activeCategory: String = ""

    enum CodingKeys: String, CodingKey {
      case visible
      case panelExpanded = "panel_expanded"
      case activeCategory = "active_category"
    }
  }

  struct StatusBar: Codable {
    var visible: Bool = false
  }

  struct Layouts: Codable {
    var panelVisibility: [String: [String: Bool]] = [:]
    var pinnedPanels: [String: Bool] = [:]
    var savedLayouts: [String: [String: Bool]] = [:]

    enum CodingKeys: String, CodingKey {
      case panelVisibility = "panel_visibility"
      case pinnedPanels = "pinned_panels"
      case savedLayouts = "saved_layouts"
    }
  }

  struct Filesystem: Codable {
    var projectRootPaths: [String] = []
    var defaultProjectRoot: String = ""
    var useFilesApp: Bool = true
    var useIcloudSync: Bool = true

    enum CodingKeys: String, CodingKey {
      case projectRootPaths = "project_root_paths"
      case defaultProjectRoot = "default_project_root"
      case useFilesApp = "use_files_app"
      case useIcloudSync = "use_icloud_sync"
    }
  }

  struct Mobile: Codable {
    var compactToolbar: Bool = true
    var showQuickActions: Bool = false
    var showStatusPills: Bool = false
    var largeTouchTargets: Bool = true
    var touchScale: Double = 1.0

    enum CodingKeys: String, CodingKey {
      case compactToolbar = "compact_toolbar"
      case showQuickActions = "show_quick_actions"
      case showStatusPills = "show_status_pills"
      case largeTouchTargets = "large_touch_targets"
      case touchScale = "touch_scale"
    }
  }

  var version: Int = 1
  var general: General = General()
  var editor: Editor = Editor()
  var performance: Performance = Performance()
  var ai: Ai = Ai()
  var logging: Logging = Logging()
  var shortcuts: Shortcuts = Shortcuts()
  var sidebar: Sidebar = Sidebar()
  var statusBar: StatusBar = StatusBar()
  var layouts: Layouts = Layouts()
  var filesystem: Filesystem = Filesystem()
  var mobile: Mobile = Mobile()
}

final class YazeSettingsStore: ObservableObject {
  @Published var settings: YazeSettings = YazeSettings()
  @Published var statusMessage: String = ""
  @Published var loadError: String = ""

  private let fileURL: URL
  private let encoder = JSONEncoder()
  private let decoder = JSONDecoder()

  var settingsFileURL: URL {
    fileURL
  }

  init() {
    let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
    let root = documents?.appendingPathComponent("Yaze", isDirectory: true)
    fileURL = root?.appendingPathComponent("settings.json") ?? URL(fileURLWithPath: "settings.json")
    encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
    load()
  }

  func load() {
    loadError = ""
    do {
      try ensureDirectory()
      guard FileManager.default.fileExists(atPath: fileURL.path) else {
        settings = defaultSettings()
        try save()
        return
      }
      let data = try Data(contentsOf: fileURL)
      if data.isEmpty {
        settings = defaultSettings()
        return
      }
      settings = try decoder.decode(YazeSettings.self, from: data)
      normalizeSettings()
    } catch {
      loadError = error.localizedDescription
      settings = defaultSettings()
    }
  }

  func save() throws {
    try ensureDirectory()
    let data = try encoder.encode(settings)
    try data.write(to: fileURL, options: [.atomic])
    statusMessage = "Saved settings.json"
  }

  func saveAndReport() {
    do {
      try save()
    } catch {
      statusMessage = "Save failed: \(error.localizedDescription)"
    }
  }

  func updateCurrentRomPath(_ path: String) {
    settings.general.lastRomPath = path
    saveAndReport()
  }

  func updateCurrentProjectPath(_ path: String) {
    settings.general.lastProjectPath = path
    saveAndReport()
  }

  private func normalizeSettings() {
    if settings.ai.hosts.isEmpty, !settings.ai.ollamaUrl.isEmpty {
      let host = YazeSettings.AiHost(
        id: "ollama-local",
        label: "Ollama (local)",
        baseUrl: settings.ai.ollamaUrl,
        apiType: "ollama",
        supportsVision: false,
        supportsTools: true,
        supportsStreaming: true,
        allowInsecure: false,
        credentialId: ""
      )
      settings.ai.hosts.append(host)
    }
    if settings.ai.activeHostId.isEmpty {
      settings.ai.activeHostId = settings.ai.hosts.first?.id ?? ""
    }
    if settings.ai.remoteBuildHostId.isEmpty {
      settings.ai.remoteBuildHostId = settings.ai.activeHostId
    }
    if settings.ai.profiles.isEmpty, !settings.ai.model.isEmpty {
      settings.ai.profiles.append(YazeSettings.AiProfile(
        name: "default",
        model: settings.ai.model,
        temperature: settings.ai.temperature,
        topP: 0.95,
        maxOutputTokens: settings.ai.maxTokens,
        supportsVision: false,
        supportsTools: true
      ))
      settings.ai.activeProfile = "default"
    }
    if settings.filesystem.projectRootPaths.isEmpty {
      settings.filesystem.projectRootPaths = [fileURL.deletingLastPathComponent().path]
      settings.filesystem.defaultProjectRoot = settings.filesystem.projectRootPaths.first ?? ""
    }
  }

  private func ensureDirectory() throws {
    let dir = fileURL.deletingLastPathComponent()
    if !FileManager.default.fileExists(atPath: dir.path) {
      try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    }
  }

  private func defaultSettings() -> YazeSettings {
    var defaults = YazeSettings()
    defaults.filesystem.projectRootPaths = [fileURL.deletingLastPathComponent().path]
    defaults.filesystem.defaultProjectRoot = defaults.filesystem.projectRootPaths.first ?? ""
    return defaults
  }
}
