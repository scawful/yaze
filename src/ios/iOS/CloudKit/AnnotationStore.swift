import Foundation
import SQLite3
import UIKit

/// Priority levels for annotations, matching the C++ AnnotationPriority enum.
enum AnnotationPriority: Int, Codable {
  case note = 0
  case bug = 1
  case blocker = 2
}

/// A room-level annotation.
struct Annotation: Identifiable, Codable {
  var id: String = UUID().uuidString
  var roomID: Int
  var text: String
  var priority: AnnotationPriority
  var category: String
  var createdAt: Date
  var modifiedAt: Date
  var createdBy: String

  init(roomID: Int, text: String, priority: AnnotationPriority = .note,
       category: String = "", createdBy: String = UIDevice.current.name) {
    self.roomID = roomID
    self.text = text
    self.priority = priority
    self.category = category
    self.createdAt = Date()
    self.modifiedAt = Date()
    self.createdBy = createdBy
  }
}

/// A test result entry associated with a room.
struct TestResult: Identifiable, Codable {
  var id: String = UUID().uuidString
  var roomID: Int
  var testName: String
  var status: TestStatus
  var details: String
  var capturedAt: Date

  enum TestStatus: String, Codable {
    case pass, fail, skip
  }
}

/// Local-first SQLite annotation cache with CRUD operations.
///
/// All mutations return immediately from the local cache.
/// A future `AnnotationSyncEngine` pushes changes to CloudKit.
final class AnnotationStore: ObservableObject {
  enum SyncUpsertResult {
    case inserted
    case updated
    case unchanged
  }

  @Published var annotations: [Annotation] = []
  @Published var testResults: [TestResult] = []

  private var database: OpaquePointer?
  private let databaseURL: URL
  private var desktopAnnotationsURL: URL?
  private var externalSyncTimer: Timer?
  private var lastSeenDesktopFileStamp: String?

  init(directory: URL? = nil, projectPath: String? = nil) {
    let dir = directory ?? AnnotationStore.defaultDirectory()
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    databaseURL = dir.appendingPathComponent("annotations.sqlite")
    openDatabase()
    createTables()
    loadAll()
    configureProjectPath(projectPath)
    refreshExternalSync()
  }

  deinit {
    externalSyncTimer?.invalidate()
    sqlite3_close(database)
  }

  func configureProjectPath(_ projectPath: String?) {
    desktopAnnotationsURL = AnnotationStore.resolveDesktopAnnotationsURL(
      projectPath: projectPath
    )
    lastSeenDesktopFileStamp = nil

    if desktopAnnotationsURL == nil {
      externalSyncTimer?.invalidate()
      externalSyncTimer = nil
      return
    }
    startExternalSyncLoop()
  }

  func refreshExternalSync() {
    importDesktopJSONIfAvailable()
  }

  // MARK: - CRUD

  func addAnnotation(_ annotation: Annotation) {
    var ann = annotation
    ann.modifiedAt = Date()
    insertAnnotation(ann)
    annotations.append(ann)
    syncDesktopJSON()
  }

  func updateAnnotation(_ annotation: Annotation) {
    var ann = annotation
    ann.modifiedAt = Date()
    updateAnnotationInDB(ann)
    if let index = annotations.firstIndex(where: { $0.id == ann.id }) {
      annotations[index] = ann
    }
    syncDesktopJSON()
  }

  func deleteAnnotation(id: String) {
    deleteAnnotationFromDB(id: id)
    annotations.removeAll { $0.id == id }
    syncDesktopJSON()
  }

  /// Upsert annotation from external sync source (CloudKit/Desktop) while
  /// preserving source timestamps.
  @discardableResult
  func upsertSyncedAnnotation(_ annotation: Annotation) -> SyncUpsertResult {
    if let index = annotations.firstIndex(where: { $0.id == annotation.id }) {
      let local = annotations[index]
      if annotation.modifiedAt <= local.modifiedAt {
        return .unchanged
      }

      annotations[index] = annotation
      updateAnnotationInDB(annotation)
      syncDesktopJSON()
      return .updated
    }

    annotations.append(annotation)
    insertAnnotation(annotation)
    syncDesktopJSON()
    return .inserted
  }

  func annotationsForRoom(_ roomID: Int) -> [Annotation] {
    annotations.filter { $0.roomID == roomID }
  }

  func addTestResult(_ result: TestResult) {
    insertTestResult(result)
    testResults.append(result)
  }

  func testResultsForRoom(_ roomID: Int) -> [TestResult] {
    testResults.filter { $0.roomID == roomID }
  }

  // MARK: - Export

  /// Export annotations as JSON for desktop compatibility.
  func exportJSON() -> Data? {
    let payload = ExportPayload(annotations: annotations, testResults: testResults)
    return try? JSONEncoder().encode(payload)
  }

  /// Import annotations from desktop JSON.
  func importJSON(_ data: Data) {
    guard let payload = try? JSONDecoder().decode(ExportPayload.self, from: data) else { return }
    for ann in payload.annotations {
      if !annotations.contains(where: { $0.id == ann.id }) {
        addAnnotation(ann)
      }
    }
    for result in payload.testResults {
      if !testResults.contains(where: { $0.id == result.id }) {
        addTestResult(result)
      }
    }
  }

  private func syncDesktopJSON() {
    guard let url = desktopAnnotationsURL else { return }

    let payload = DesktopAnnotationPayload(
      annotations: annotations.map {
        DesktopAnnotationRecord(
          id: $0.id,
          roomID: $0.roomID,
          text: $0.text,
          priority: $0.priority.rawValue,
          category: $0.category,
          createdAt: AnnotationStore.iso8601Formatter.string(from: $0.createdAt),
          modifiedAt: AnnotationStore.iso8601Formatter.string(from: $0.modifiedAt),
          createdBy: $0.createdBy
        )
      }
    )

    do {
      try FileManager.default.createDirectory(
        at: url.deletingLastPathComponent(),
        withIntermediateDirectories: true
      )
      let encoder = JSONEncoder()
      encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
      let data = try encoder.encode(payload)
      try data.write(to: url, options: [.atomic])
      lastSeenDesktopFileStamp = AnnotationStore.fileStamp(for: url)
    } catch {
      // Keep local sqlite source-of-truth even if desktop mirror write fails.
    }
  }

  private func importDesktopJSONIfAvailable() {
    guard let url = desktopAnnotationsURL else { return }
    let currentStamp = AnnotationStore.fileStamp(for: url)
    if let currentStamp, currentStamp == lastSeenDesktopFileStamp {
      return
    }

    guard let data = try? Data(contentsOf: url) else { return }
    guard let payload = try? JSONDecoder().decode(DesktopAnnotationPayload.self, from: data) else {
      return
    }

    var changed = false
    for record in payload.annotations {
      var annotation = Annotation(
        roomID: record.roomID,
        text: record.text,
        priority: AnnotationPriority(rawValue: record.priority) ?? .note,
        category: record.category,
        createdBy: record.createdBy ?? UIDevice.current.name
      )
      annotation.id = AnnotationStore.stableRecordID(for: record)
      if let createdAt = AnnotationStore.parseDate(record.createdAt) {
        annotation.createdAt = createdAt
      }
      if let modifiedAt = AnnotationStore.parseDate(record.modifiedAt) {
        annotation.modifiedAt = modifiedAt
      } else {
        annotation.modifiedAt = annotation.createdAt
      }

      if let idx = annotations.firstIndex(where: { $0.id == annotation.id }) {
        let local = annotations[idx]
        if local.modifiedAt < annotation.modifiedAt {
          annotations[idx] = annotation
          updateAnnotationInDB(annotation)
          changed = true
        }
      } else {
        annotations.append(annotation)
        insertAnnotation(annotation)
        changed = true
      }
    }

    if changed {
      syncDesktopJSON()
    } else {
      lastSeenDesktopFileStamp = currentStamp
    }
  }

  private func startExternalSyncLoop() {
    if externalSyncTimer != nil {
      return
    }

    let timer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) {
      [weak self] _ in
      self?.refreshExternalSync()
    }
    RunLoop.main.add(timer, forMode: .common)
    externalSyncTimer = timer
  }

  // MARK: - SQLite internals

  private static func defaultDirectory() -> URL {
    FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
      .appendingPathComponent("Yaze/Annotations")
  }

  private static func resolveDesktopAnnotationsURL(projectPath: String?) -> URL? {
    guard let projectPath, !projectPath.isEmpty else { return nil }

    let projectURL = URL(fileURLWithPath: projectPath)
    let root: URL
    switch projectURL.pathExtension.lowercased() {
    case "yazeproj":
      root = projectURL
    case "yaze":
      root = projectURL.deletingLastPathComponent()
    default:
      if projectURL.lastPathComponent.lowercased() == "project.yaze" {
        root = projectURL.deletingLastPathComponent()
      } else {
        root = projectURL
      }
    }

    var candidates: [URL] = [
      root.appendingPathComponent("Docs/Dev/Planning/annotations.json"),
      root.appendingPathComponent("code/Docs/Dev/Planning/annotations.json"),
    ]

    let projectFile = root.appendingPathComponent("project.yaze")
    if let codeFolder = readCodeFolder(fromProjectFileAt: projectFile),
       !codeFolder.isEmpty {
      if codeFolder.hasPrefix("/") {
        let absolute = URL(fileURLWithPath: codeFolder, isDirectory: true)
        candidates.insert(
          absolute.appendingPathComponent("Docs/Dev/Planning/annotations.json"),
          at: 0
        )
      } else {
        candidates.insert(
          root.appendingPathComponent(codeFolder)
            .appendingPathComponent("Docs/Dev/Planning/annotations.json"),
          at: 0
        )
      }
    }

    for candidate in candidates {
      if FileManager.default.fileExists(atPath: candidate.path) {
        return candidate
      }
    }
    return candidates.first
  }

  private static func readCodeFolder(fromProjectFileAt projectFileURL: URL) -> String? {
    guard let text = try? String(contentsOf: projectFileURL, encoding: .utf8) else {
      return nil
    }
    for rawLine in text.split(whereSeparator: \.isNewline) {
      let line = rawLine.trimmingCharacters(in: .whitespacesAndNewlines)
      guard line.hasPrefix("code_folder=") else { continue }
      let value = line.replacingOccurrences(of: "code_folder=", with: "")
      return value.trimmingCharacters(in: .whitespacesAndNewlines)
    }
    return nil
  }

  private static func parseDate(_ value: String?) -> Date? {
    guard let value, !value.isEmpty else { return nil }
    if let date = iso8601Formatter.date(from: value) {
      return date
    }
    return legacyDateFormatter.date(from: value)
  }

  private static func stableRecordID(for record: DesktopAnnotationRecord) -> String {
    if let id = record.id, !id.isEmpty {
      return id
    }
    let seed = "\(record.roomID)|\(record.priority)|\(record.category)|\(record.text)|\(record.createdAt ?? "")"
    let encoded = Data(seed.utf8).base64EncodedString()
      .replacingOccurrences(of: "/", with: "_")
      .replacingOccurrences(of: "+", with: "-")
      .replacingOccurrences(of: "=", with: "")
    return "desktop-\(encoded)"
  }

  private static let iso8601Formatter: ISO8601DateFormatter = {
    let formatter = ISO8601DateFormatter()
    formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    return formatter
  }()

  private static let legacyDateFormatter: DateFormatter = {
    let formatter = DateFormatter()
    formatter.locale = Locale(identifier: "en_US_POSIX")
    formatter.timeZone = TimeZone(secondsFromGMT: 0)
    formatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
    return formatter
  }()

  private static func fileStamp(for url: URL) -> String? {
    let attrs = try? FileManager.default.attributesOfItem(atPath: url.path)
    guard let attrs else { return nil }
    let modified = attrs[.modificationDate] as? Date
    let size = attrs[.size] as? NSNumber
    return "\(modified?.timeIntervalSince1970 ?? 0)-\(size?.int64Value ?? -1)"
  }

  private func openDatabase() {
    sqlite3_open(databaseURL.path, &database)
  }

  private func createTables() {
    let annotationSQL = """
    CREATE TABLE IF NOT EXISTS annotations (
      id TEXT PRIMARY KEY,
      room_id INTEGER,
      text TEXT,
      priority INTEGER,
      category TEXT,
      created_at REAL,
      modified_at REAL,
      created_by TEXT
    );
    """
    let testSQL = """
    CREATE TABLE IF NOT EXISTS test_results (
      id TEXT PRIMARY KEY,
      room_id INTEGER,
      test_name TEXT,
      status TEXT,
      details TEXT,
      captured_at REAL
    );
    """
    sqlite3_exec(database, annotationSQL, nil, nil, nil)
    sqlite3_exec(database, testSQL, nil, nil, nil)
  }

  private func loadAll() {
    annotations = loadAnnotations()
    testResults = loadTestResults()
  }

  private func loadAnnotations() -> [Annotation] {
    var results: [Annotation] = []
    var stmt: OpaquePointer?
    let sql = "SELECT id, room_id, text, priority, category, created_at, modified_at, created_by FROM annotations"
    guard sqlite3_prepare_v2(database, sql, -1, &stmt, nil) == SQLITE_OK else { return results }
    defer { sqlite3_finalize(stmt) }

    while sqlite3_step(stmt) == SQLITE_ROW {
      var ann = Annotation(roomID: Int(sqlite3_column_int(stmt, 1)),
                           text: String(cString: sqlite3_column_text(stmt, 2)))
      ann.id = String(cString: sqlite3_column_text(stmt, 0))
      ann.priority = AnnotationPriority(rawValue: Int(sqlite3_column_int(stmt, 3))) ?? .note
      ann.category = String(cString: sqlite3_column_text(stmt, 4))
      ann.createdAt = Date(timeIntervalSince1970: sqlite3_column_double(stmt, 5))
      ann.modifiedAt = Date(timeIntervalSince1970: sqlite3_column_double(stmt, 6))
      ann.createdBy = String(cString: sqlite3_column_text(stmt, 7))
      results.append(ann)
    }
    return results
  }

  private func insertAnnotation(_ ann: Annotation) {
    var stmt: OpaquePointer?
    let sql = "INSERT OR REPLACE INTO annotations (id, room_id, text, priority, category, created_at, modified_at, created_by) VALUES (?,?,?,?,?,?,?,?)"
    guard sqlite3_prepare_v2(database, sql, -1, &stmt, nil) == SQLITE_OK else { return }
    defer { sqlite3_finalize(stmt) }

    sqlite3_bind_text(stmt, 1, (ann.id as NSString).utf8String, -1, nil)
    sqlite3_bind_int(stmt, 2, Int32(ann.roomID))
    sqlite3_bind_text(stmt, 3, (ann.text as NSString).utf8String, -1, nil)
    sqlite3_bind_int(stmt, 4, Int32(ann.priority.rawValue))
    sqlite3_bind_text(stmt, 5, (ann.category as NSString).utf8String, -1, nil)
    sqlite3_bind_double(stmt, 6, ann.createdAt.timeIntervalSince1970)
    sqlite3_bind_double(stmt, 7, ann.modifiedAt.timeIntervalSince1970)
    sqlite3_bind_text(stmt, 8, (ann.createdBy as NSString).utf8String, -1, nil)
    sqlite3_step(stmt)
  }

  private func updateAnnotationInDB(_ ann: Annotation) {
    insertAnnotation(ann)  // UPSERT via INSERT OR REPLACE
  }

  private func deleteAnnotationFromDB(id: String) {
    var stmt: OpaquePointer?
    let sql = "DELETE FROM annotations WHERE id = ?"
    guard sqlite3_prepare_v2(database, sql, -1, &stmt, nil) == SQLITE_OK else { return }
    defer { sqlite3_finalize(stmt) }
    sqlite3_bind_text(stmt, 1, (id as NSString).utf8String, -1, nil)
    sqlite3_step(stmt)
  }

  private func loadTestResults() -> [TestResult] {
    var results: [TestResult] = []
    var stmt: OpaquePointer?
    let sql = "SELECT id, room_id, test_name, status, details, captured_at FROM test_results"
    guard sqlite3_prepare_v2(database, sql, -1, &stmt, nil) == SQLITE_OK else { return results }
    defer { sqlite3_finalize(stmt) }

    while sqlite3_step(stmt) == SQLITE_ROW {
      var result = TestResult(
        roomID: Int(sqlite3_column_int(stmt, 1)),
        testName: String(cString: sqlite3_column_text(stmt, 2)),
        status: TestResult.TestStatus(rawValue: String(cString: sqlite3_column_text(stmt, 3))) ?? .skip,
        details: String(cString: sqlite3_column_text(stmt, 4)),
        capturedAt: Date(timeIntervalSince1970: sqlite3_column_double(stmt, 5))
      )
      result.id = String(cString: sqlite3_column_text(stmt, 0))
      results.append(result)
    }
    return results
  }

  private func insertTestResult(_ result: TestResult) {
    var stmt: OpaquePointer?
    let sql = "INSERT OR REPLACE INTO test_results (id, room_id, test_name, status, details, captured_at) VALUES (?,?,?,?,?,?)"
    guard sqlite3_prepare_v2(database, sql, -1, &stmt, nil) == SQLITE_OK else { return }
    defer { sqlite3_finalize(stmt) }

    sqlite3_bind_text(stmt, 1, (result.id as NSString).utf8String, -1, nil)
    sqlite3_bind_int(stmt, 2, Int32(result.roomID))
    sqlite3_bind_text(stmt, 3, (result.testName as NSString).utf8String, -1, nil)
    sqlite3_bind_text(stmt, 4, (result.status.rawValue as NSString).utf8String, -1, nil)
    sqlite3_bind_text(stmt, 5, (result.details as NSString).utf8String, -1, nil)
    sqlite3_bind_double(stmt, 6, result.capturedAt.timeIntervalSince1970)
    sqlite3_step(stmt)
  }
}

private struct ExportPayload: Codable {
  var annotations: [Annotation]
  var testResults: [TestResult]
}

private struct DesktopAnnotationPayload: Codable {
  var annotations: [DesktopAnnotationRecord]
}

private struct DesktopAnnotationRecord: Codable {
  var id: String?
  var roomID: Int
  var text: String
  var priority: Int
  var category: String
  var createdAt: String?
  var modifiedAt: String?
  var createdBy: String?

  enum CodingKeys: String, CodingKey {
    case id
    case roomID = "room_id"
    case text
    case priority
    case category
    case createdAt = "created_at"
    case modifiedAt = "modified_at"
    case createdBy = "created_by"
  }
}
