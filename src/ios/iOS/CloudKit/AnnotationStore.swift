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
  @Published var annotations: [Annotation] = []
  @Published var testResults: [TestResult] = []

  private var database: OpaquePointer?
  private let databaseURL: URL

  init(directory: URL? = nil) {
    let dir = directory ?? AnnotationStore.defaultDirectory()
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    databaseURL = dir.appendingPathComponent("annotations.sqlite")
    openDatabase()
    createTables()
    loadAll()
  }

  deinit {
    sqlite3_close(database)
  }

  // MARK: - CRUD

  func addAnnotation(_ annotation: Annotation) {
    var ann = annotation
    ann.modifiedAt = Date()
    insertAnnotation(ann)
    annotations.append(ann)
  }

  func updateAnnotation(_ annotation: Annotation) {
    var ann = annotation
    ann.modifiedAt = Date()
    updateAnnotationInDB(ann)
    if let index = annotations.firstIndex(where: { $0.id == ann.id }) {
      annotations[index] = ann
    }
  }

  func deleteAnnotation(id: String) {
    deleteAnnotationFromDB(id: id)
    annotations.removeAll { $0.id == id }
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

  // MARK: - SQLite internals

  private static func defaultDirectory() -> URL {
    FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
      .appendingPathComponent("Yaze/Annotations")
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
