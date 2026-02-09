import Foundation

/// Manages security-scoped access for files opened via the iOS Files app.
///
/// For a "seamless Mac <-> iPad" workflow we prefer opening `.yazeproj` bundles
/// in-place from iCloud Drive. Those URLs are typically security-scoped and
/// must remain accessible for as long as the core is reading/writing files.
final class YazeSecurityScopeManager {
  static let shared = YazeSecurityScopeManager()

  private var grantedURLs: [URL] = []

  private init() {}

  func beginAccess(to url: URL) {
    beginAccess(toAnyOf: [url])
  }

  func beginAccess(toAnyOf urls: [URL]) {
    endAccess()

    // Keep the access set minimal and deterministic.
    var seen = Set<URL>()
    for url in urls {
      if seen.contains(url) { continue }
      seen.insert(url)
      if url.startAccessingSecurityScopedResource() {
        grantedURLs.append(url)
      }
    }
  }

  func endAccess() {
    for url in grantedURLs {
      url.stopAccessingSecurityScopedResource()
    }
    grantedURLs.removeAll()
  }
}
