import Foundation

enum YazeProjectOpenError: Error, LocalizedError {
  case notFound
  case notBundle

  var errorDescription: String? {
    switch self {
    case .notFound:
      return "The selected project could not be found."
    case .notBundle:
      return "The selected item is not a .yazeproj project bundle."
    }
  }
}

/// Centralizes the iOS "open project" path so we keep settings + security-scope
/// handling consistent across menus/views.
final class YazeProjectOpenService {
  static func openBundle(at url: URL, settingsStore: YazeSettingsStore) throws {
    let fm = FileManager.default

    guard fm.fileExists(atPath: url.path) else {
      throw YazeProjectOpenError.notFound
    }
    guard url.pathExtension.lowercased() == "yazeproj" else {
      throw YazeProjectOpenError.notBundle
    }

    // Keep access alive for the duration of the project session.
    YazeSecurityScopeManager.shared.beginAccess(to: url)

    // Update settings first so any crash/restart can recover the opened bundle.
    settingsStore.updateCurrentProjectPath(url.path)
    let romPath = url.appendingPathComponent("rom").path
    if fm.fileExists(atPath: romPath) {
      settingsStore.updateCurrentRomPath(romPath)
    }

    // Finally, open via the shared C++ project loader.
    YazeIOSBridge.openProject(atPath: url.path)

    // Override the "Saved settings.json" status for clarity.
    settingsStore.statusMessage = "Opened \(url.deletingPathExtension().lastPathComponent)"
  }
}

