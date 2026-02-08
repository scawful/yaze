import UIKit
import UniformTypeIdentifiers

/// UTType for `.yazeproj` bundles.
extension UTType {
  static let yazeProject = UTType(
    exportedAs: "org.halext.yaze.project",
    conformingTo: .package
  )
}

/// Manifest stored inside each `.yazeproj` bundle.
struct YazeDocumentManifest: Codable {
  var version: Int = 2
  var name: String
  var romChecksum: String
  var createdAt: String
  var lastModifiedAt: String
  var deviceName: String

  static func create(name: String, romChecksum: String = "") -> YazeDocumentManifest {
    let now = ISO8601DateFormatter().string(from: Date())
    return YazeDocumentManifest(
      version: 2,
      name: name,
      romChecksum: romChecksum,
      createdAt: now,
      lastModifiedAt: now,
      deviceName: UIDevice.current.name
    )
  }
}

/// `UIDocument`-backed project model that uses `NSFileWrapper` for
/// incremental iCloud Drive sync. Replaces the copy-based
/// `YazeProjectBundleService` with a proper document lifecycle.
///
/// Bundle layout:
/// ```
/// MyProject.yazeproj/
/// ├── manifest.json
/// ├── rom                  (ROM binary)
/// ├── project/             (code folder snapshot)
/// ├── settings.json        (project-specific settings)
/// └── annotations/         (local annotation cache)
/// ```
final class YazeProjectDocument: UIDocument {

  // MARK: - State

  private(set) var manifest: YazeDocumentManifest?
  private(set) var romData: Data?
  private(set) var projectWrapper: FileWrapper?
  private(set) var settingsData: Data?
  private(set) var annotationsWrapper: FileWrapper?

  /// The root file wrapper representing the bundle directory.
  private var rootWrapper: FileWrapper?

  // MARK: - UIDocument overrides

  override func contents(forType typeName: String) throws -> Any {
    let root = FileWrapper(directoryWithFileWrappers: [:])

    // manifest.json
    if var m = manifest {
      m.lastModifiedAt = ISO8601DateFormatter().string(from: Date())
      m.deviceName = UIDevice.current.name
      manifest = m
      let data = try JSONEncoder().encode(m)
      root.addRegularFile(withContents: data, preferredFilename: "manifest.json")
    }

    // rom
    if let rom = romData {
      root.addRegularFile(withContents: rom, preferredFilename: "rom")
    }

    // project/
    if let proj = projectWrapper {
      proj.preferredFilename = "project"
      root.addFileWrapper(proj)
    }

    // settings.json
    if let settings = settingsData {
      root.addRegularFile(withContents: settings, preferredFilename: "settings.json")
    }

    // annotations/
    if let ann = annotationsWrapper {
      ann.preferredFilename = "annotations"
      root.addFileWrapper(ann)
    }

    rootWrapper = root
    return root
  }

  override func load(fromContents contents: Any, ofType typeName: String?) throws {
    guard let wrapper = contents as? FileWrapper,
          wrapper.isDirectory else {
      throw CocoaError(.fileReadCorruptFile)
    }

    rootWrapper = wrapper
    let children = wrapper.fileWrappers ?? [:]

    // manifest.json
    if let manifestWrapper = children["manifest.json"],
       let data = manifestWrapper.regularFileContents {
      manifest = try? JSONDecoder().decode(YazeDocumentManifest.self, from: data)
    }

    // rom
    if let romWrapper = children["rom"] {
      romData = romWrapper.regularFileContents
    }

    // project/
    projectWrapper = children["project"]

    // settings.json
    if let settingsWrapper = children["settings.json"] {
      settingsData = settingsWrapper.regularFileContents
    }

    // annotations/
    annotationsWrapper = children["annotations"]
  }

  // MARK: - Conflict Resolution

  /// Enumerate file versions for conflict resolution.
  func conflictVersions() -> [NSFileVersion] {
    guard let url = fileURL as URL? else { return [] }
    return NSFileVersion.unresolvedConflictVersionsOfItem(at: url) ?? []
  }

  /// Resolve all conflicts by keeping the current version.
  func resolveConflictsKeepingCurrent() {
    for version in conflictVersions() {
      version.isResolved = true
    }
    try? NSFileVersion.removeOtherVersionsOfItem(at: fileURL)
  }

  // MARK: - Mutation helpers

  func setRom(_ data: Data) {
    romData = data
    updateChangeCount(.done)
  }

  func setProjectFolder(_ wrapper: FileWrapper) {
    projectWrapper = wrapper
    updateChangeCount(.done)
  }

  func setSettings(_ data: Data) {
    settingsData = data
    updateChangeCount(.done)
  }

  func setManifest(_ m: YazeDocumentManifest) {
    manifest = m
    updateChangeCount(.done)
  }
}

// MARK: - Migration from legacy bundles

extension YazeProjectDocument {

  /// Migrate a legacy `YazeProjectBundleService` export into this document.
  static func migrateFromLegacy(
    legacyBundleURL: URL,
    destinationURL: URL,
    completion: @escaping (Result<YazeProjectDocument, Error>) -> Void
  ) {
    let fm = FileManager.default
    let doc = YazeProjectDocument(fileURL: destinationURL)

    // Read legacy manifest
    let legacyManifestURL = legacyBundleURL.appendingPathComponent("manifest.json")
    if let data = try? Data(contentsOf: legacyManifestURL),
       let legacy = try? JSONDecoder().decode(YazeProjectBundleManifest.self, from: data) {
      doc.manifest = YazeDocumentManifest.create(
        name: legacy.projectName.isEmpty ? "Migrated Project" : legacy.projectName
      )
    } else {
      doc.manifest = YazeDocumentManifest.create(name: "Migrated Project")
    }

    // Copy ROM
    let romURL = legacyBundleURL.appendingPathComponent("rom")
    if fm.fileExists(atPath: romURL.path) {
      doc.romData = try? Data(contentsOf: romURL)
    }

    // Copy project folder
    let projectURL = legacyBundleURL.appendingPathComponent("project")
    if fm.fileExists(atPath: projectURL.path) {
      doc.projectWrapper = try? FileWrapper(url: projectURL, options: .immediate)
    }

    // Copy settings
    let settingsURL = legacyBundleURL.appendingPathComponent("settings.json")
    if fm.fileExists(atPath: settingsURL.path) {
      doc.settingsData = try? Data(contentsOf: settingsURL)
    }

    doc.save(to: destinationURL, for: .forCreating) { success in
      if success {
        completion(.success(doc))
      } else {
        completion(.failure(CocoaError(.fileWriteUnknown)))
      }
    }
  }
}
