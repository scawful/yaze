import Foundation

/// Manages security-scoped access for files opened via the iOS Files app.
///
/// For a "seamless Mac <-> iPad" workflow we prefer opening `.yazeproj` bundles
/// in-place from iCloud Drive. Those URLs are typically security-scoped and
/// must remain accessible for as long as the core is reading/writing files.
final class YazeSecurityScopeManager {
  static let shared = YazeSecurityScopeManager()

  private var currentURL: URL?
  private var currentGranted: Bool = false

  private init() {}

  func beginAccess(to url: URL) {
    endAccess()
    currentURL = url
    currentGranted = url.startAccessingSecurityScopedResource()
  }

  func endAccess() {
    if let url = currentURL, currentGranted {
      url.stopAccessingSecurityScopedResource()
    }
    currentURL = nil
    currentGranted = false
  }
}

