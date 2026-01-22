import Foundation

final class YazeFileImportService {
  static func importRom(from url: URL) -> URL? {
    let fileManager = FileManager.default
    guard let documents = fileManager.urls(for: .documentDirectory, in: .userDomainMask).first else {
      return nil
    }

    let root = documents.appendingPathComponent("Yaze", isDirectory: true)
    let romDir = root.appendingPathComponent("ROMs", isDirectory: true)

    do {
      try fileManager.createDirectory(at: romDir, withIntermediateDirectories: true)
    } catch {
      return nil
    }

    let baseName = url.deletingPathExtension().lastPathComponent
    let ext = url.pathExtension.isEmpty ? "sfc" : url.pathExtension
    var destination = romDir.appendingPathComponent("\(baseName).\(ext)")

    if fileManager.fileExists(atPath: destination.path) {
      let stamp = Int(Date().timeIntervalSince1970)
      destination = romDir.appendingPathComponent("\(baseName)-\(stamp).\(ext)")
    }

    let accessGranted = url.startAccessingSecurityScopedResource()
    defer {
      if accessGranted {
        url.stopAccessingSecurityScopedResource()
      }
    }

    var copyError: Error?
    let coordinator = NSFileCoordinator()
    var coordinationError: NSError?
    coordinator.coordinate(readingItemAt: url, options: [], error: &coordinationError) { readingURL in
      do {
        if fileManager.fileExists(atPath: destination.path) {
          try fileManager.removeItem(at: destination)
        }
        try fileManager.copyItem(at: readingURL, to: destination)
      } catch {
        copyError = error
      }
    }

    if coordinationError != nil || copyError != nil {
      return nil
    }

    return destination
  }
}
