import Foundation
import Combine

/// Sync status for an iCloud-backed document.
enum DocumentSyncStatus: Equatable {
  case local           // On-device only, not in iCloud
  case synced          // Up-to-date with iCloud
  case downloading     // Being downloaded from iCloud
  case uploading       // Being uploaded to iCloud
  case conflict        // Unresolved conflict
  case error(String)   // Sync error

  static func == (lhs: DocumentSyncStatus, rhs: DocumentSyncStatus) -> Bool {
    switch (lhs, rhs) {
    case (.local, .local), (.synced, .synced),
         (.downloading, .downloading), (.uploading, .uploading),
         (.conflict, .conflict):
      return true
    case let (.error(a), .error(b)):
      return a == b
    default:
      return false
    }
  }

  var sfSymbolName: String {
    switch self {
    case .local: return "externaldrive"
    case .synced: return "checkmark.icloud"
    case .downloading: return "icloud.and.arrow.down"
    case .uploading: return "icloud.and.arrow.up"
    case .conflict: return "exclamationmark.icloud"
    case .error: return "xmark.icloud"
    }
  }
}

/// Summary info for a discovered project document.
struct DocumentInfo: Identifiable {
  let id: URL
  let name: String
  let modifiedDate: Date?
  var syncStatus: DocumentSyncStatus
  let url: URL

  init(url: URL, syncStatus: DocumentSyncStatus = .local) {
    self.id = url
    self.url = url
    self.name = url.deletingPathExtension().lastPathComponent
    self.modifiedDate = try? FileManager.default
      .attributesOfItem(atPath: url.path)[.modificationDate] as? Date
    self.syncStatus = syncStatus
  }
}

/// Manages the lifecycle of `.yazeproj` documents, including
/// iCloud Drive discovery via `NSMetadataQuery`.
final class YazeDocumentManager: ObservableObject {
  @Published var documents: [DocumentInfo] = []
  @Published var isSearching = false

  private var metadataQuery: NSMetadataQuery?
  private var observers: [Any] = []

  /// The iCloud ubiquity container URL, if available.
  var iCloudContainerURL: URL? {
    FileManager.default.url(forUbiquityContainerIdentifier: nil)?
      .appendingPathComponent("Documents")
  }

  /// Whether iCloud is available on this device.
  var isICloudAvailable: Bool {
    FileManager.default.ubiquityIdentityToken != nil
  }

  /// Local documents directory (fallback when iCloud is unavailable).
  var localDocumentsURL: URL {
    let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
    return docs.appendingPathComponent("Yaze/Projects", isDirectory: true)
  }

  // MARK: - Lifecycle

  func startMonitoring() {
    // Avoid stacking multiple NSMetadataQuery instances and observers when
    // called repeatedly (e.g. via a Refresh button).
    if metadataQuery != nil || !observers.isEmpty {
      stopMonitoring()
    }

    // Always scan local
    scanLocalDocuments()

    // Set up iCloud metadata query if available
    guard isICloudAvailable else { return }

    let query = NSMetadataQuery()
    query.searchScopes = [NSMetadataQueryUbiquitousDocumentsScope]
    query.predicate = NSPredicate(format: "%K LIKE '*.yazeproj'",
                                  NSMetadataItemFSNameKey)

    let nc = NotificationCenter.default

    observers.append(
      nc.addObserver(forName: .NSMetadataQueryDidFinishGathering,
                     object: query, queue: .main) { [weak self] _ in
        self?.processQueryResults()
      }
    )

    observers.append(
      nc.addObserver(forName: .NSMetadataQueryDidUpdate,
                     object: query, queue: .main) { [weak self] _ in
        self?.processQueryResults()
      }
    )

    isSearching = true
    query.start()
    metadataQuery = query
  }

  func stopMonitoring() {
    metadataQuery?.stop()
    metadataQuery = nil
    for observer in observers {
      NotificationCenter.default.removeObserver(observer)
    }
    observers.removeAll()
    isSearching = false
  }

  deinit {
    stopMonitoring()
  }

  // MARK: - Document operations

  /// Create a new empty project document.
  func createDocument(
    name: String,
    in directory: URL? = nil,
    completion: @escaping (Result<YazeProjectDocument, Error>) -> Void
  ) {
    let dir = directory ?? (iCloudContainerURL ?? localDocumentsURL)

    // Ensure directory exists
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)

    let docURL = dir.appendingPathComponent("\(name).yazeproj")
    let doc = YazeProjectDocument(fileURL: docURL)
    doc.setManifest(YazeDocumentManifest.create(name: name))

    doc.save(to: docURL, for: .forCreating) { success in
      DispatchQueue.main.async {
        if success {
          self.scanLocalDocuments()
          completion(.success(doc))
        } else {
          completion(.failure(CocoaError(.fileWriteUnknown)))
        }
      }
    }
  }

  /// Request download of an iCloud-only document.
  func downloadDocument(at url: URL) {
    try? FileManager.default.startDownloadingUbiquitousItem(at: url)
  }

  /// Detect and return legacy bundles that can be migrated.
  func detectLegacyBundles() -> [URL] {
    let importDir = FileManager.default
      .urls(for: .documentDirectory, in: .userDomainMask).first?
      .appendingPathComponent("Yaze/Imports")

    guard let importDir, FileManager.default.fileExists(atPath: importDir.path) else {
      return []
    }

    let contents = (try? FileManager.default.contentsOfDirectory(
      at: importDir,
      includingPropertiesForKeys: nil
    )) ?? []

    return contents.filter { url in
      // Legacy bundles have a manifest.json with version 1
      let manifestURL = url.appendingPathComponent("manifest.json")
      guard let data = try? Data(contentsOf: manifestURL),
            let json = try? JSONDecoder().decode(YazeProjectBundleManifest.self, from: data) else {
        return false
      }
      return json.version == 1
    }
  }

  // MARK: - Internal

  private func scanLocalDocuments() {
    var found: [DocumentInfo] = []

    // Scan local documents directory
    let dirs = [localDocumentsURL]
    for dir in dirs {
      guard FileManager.default.fileExists(atPath: dir.path) else { continue }
      let contents = (try? FileManager.default.contentsOfDirectory(
        at: dir,
        includingPropertiesForKeys: [.contentModificationDateKey]
      )) ?? []

      for url in contents where url.pathExtension == "yazeproj" {
        found.append(DocumentInfo(url: url, syncStatus: .local))
      }
    }

    // Merge with existing iCloud documents (don't overwrite their sync status)
    let iCloudURLs = Set(documents.filter { $0.syncStatus != .local }.map(\.url))
    let localOnly = found.filter { !iCloudURLs.contains($0.url) }

    documents = documents.filter { $0.syncStatus != .local } + localOnly
    documents.sort { ($0.modifiedDate ?? .distantPast) > ($1.modifiedDate ?? .distantPast) }
  }

  private func processQueryResults() {
    guard let query = metadataQuery else { return }

    query.disableUpdates()
    defer { query.enableUpdates() }

    var iCloudDocs: [DocumentInfo] = []

    for item in query.results {
      guard let metadataItem = item as? NSMetadataItem,
            let url = metadataItem.value(forAttribute: NSMetadataItemURLKey) as? URL else {
        continue
      }

      let status = syncStatus(for: metadataItem)
      iCloudDocs.append(DocumentInfo(url: url, syncStatus: status))
    }

    // Merge: iCloud docs replace local entries for same URL
    let iCloudURLs = Set(iCloudDocs.map(\.url))
    let localOnly = documents.filter { $0.syncStatus == .local && !iCloudURLs.contains($0.url) }
    documents = iCloudDocs + localOnly
    documents.sort { ($0.modifiedDate ?? .distantPast) > ($1.modifiedDate ?? .distantPast) }
    isSearching = false
  }

  private func syncStatus(for item: NSMetadataItem) -> DocumentSyncStatus {
    let downloadStatus = item.value(
      forAttribute: NSMetadataUbiquitousItemDownloadingStatusKey
    ) as? String

    let isUploading = (item.value(
      forAttribute: NSMetadataUbiquitousItemIsUploadingKey
    ) as? Bool) ?? false

    let hasConflicts = (item.value(
      forAttribute: NSMetadataUbiquitousItemHasUnresolvedConflictsKey
    ) as? Bool) ?? false

    if hasConflicts { return .conflict }
    if isUploading { return .uploading }

    switch downloadStatus {
    case NSMetadataUbiquitousItemDownloadingStatusCurrent:
      return .synced
    case NSMetadataUbiquitousItemDownloadingStatusDownloaded:
      return .synced
    case NSMetadataUbiquitousItemDownloadingStatusNotDownloaded:
      return .downloading
    default:
      return .local
    }
  }
}
