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
  private static func resolveBundleRoot(for url: URL) -> URL? {
    if url.pathExtension.lowercased() == "yazeproj" {
      return url
    }

    // If the user navigated into the bundle and selected a file inside
    // (e.g., "project.yaze"), walk up to the bundle root.
    var cur = url
    for _ in 0..<16 {
      let parent = cur.deletingLastPathComponent()
      if parent.path == cur.path {
        break
      }
      if parent.pathExtension.lowercased() == "yazeproj" {
        return parent
      }
      cur = parent
    }

    // Fallback: some pickers may treat bundles as folders and return a folder
    // URL with no extension. In that case, detect by contents.
    let candidateDirs = [url, url.deletingLastPathComponent()]
    let fm = FileManager.default
    for dir in candidateDirs {
      var isDir: ObjCBool = false
      if !fm.fileExists(atPath: dir.path, isDirectory: &isDir) || !isDir.boolValue {
        continue
      }
      let projectFile = dir.appendingPathComponent("project.yaze")
      let romFile = dir.appendingPathComponent("rom")
      if fm.fileExists(atPath: projectFile.path) && fm.fileExists(atPath: romFile.path) {
        return dir
      }
    }

    return nil
  }

  static func openBundle(at url: URL, settingsStore: YazeSettingsStore) throws {
    let fm = FileManager.default

    // Start security-scope early: even `fileExists` can fail without it on iOS.
    // We'll switch/rebind the scope to the resolved bundle root once we have it.
    YazeSecurityScopeManager.shared.beginAccess(to: url)

    do {
      guard fm.fileExists(atPath: url.path) else {
        throw YazeProjectOpenError.notFound
      }
      guard let bundleRoot = resolveBundleRoot(for: url) else {
        throw YazeProjectOpenError.notBundle
      }

      // iCloud Drive items may be lazily downloaded. Request a download if needed.
      // (Safe no-op for non-ubiquitous URLs.)
      try? fm.startDownloadingUbiquitousItem(at: bundleRoot)
      // Also request the key bundle members we depend on during open.
      try? fm.startDownloadingUbiquitousItem(at: bundleRoot.appendingPathComponent("project.yaze"))
      try? fm.startDownloadingUbiquitousItem(at: bundleRoot.appendingPathComponent("rom"))
      try? fm.startDownloadingUbiquitousItem(at: bundleRoot.appendingPathComponent("manifest.json"))

      // Keep access alive for the duration of the project session.
      // Prefer the bundle root, but also include the originally picked URL in
      // case the system granted a narrower security scope.
      YazeSecurityScopeManager.shared.beginAccess(toAnyOf: [bundleRoot, url])

      // Update settings first so any crash/restart can recover the opened bundle.
      settingsStore.updateCurrentProjectPath(bundleRoot.path)
      let romPath = bundleRoot.appendingPathComponent("rom").path
      if fm.fileExists(atPath: romPath) {
        settingsStore.updateCurrentRomPath(romPath)
      }

      // Finally, open via the shared C++ project loader.
      let openPath: String
      if bundleRoot.pathExtension.lowercased() == "yazeproj" {
        openPath = bundleRoot.path
      } else {
        openPath = bundleRoot.appendingPathComponent("project.yaze").path
      }
      YazeIOSBridge.openProject(atPath: openPath)

      // Override the "Saved settings.json" status for clarity.
      settingsStore.statusMessage = "Opened \(bundleRoot.deletingPathExtension().lastPathComponent)"
    } catch {
      YazeSecurityScopeManager.shared.endAccess()
      throw error
    }
  }
}
