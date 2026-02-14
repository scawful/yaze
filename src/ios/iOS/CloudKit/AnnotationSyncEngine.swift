import CloudKit
import Combine
import Foundation

/// Manages push/pull synchronization between local AnnotationStore and CloudKit.
///
/// Uses CKDatabaseSubscription for real-time pull notifications.
/// Last-writer-wins for simple fields.
final class AnnotationSyncEngine: ObservableObject {
  @Published var syncStatus: String = "Idle"
  @Published var lastSyncDate: Date?

  private let database: CKDatabase
  private let store: AnnotationStore
  private var changeToken: CKServerChangeToken?
  private var subscription: CKDatabaseSubscription?
  private var cancellables = Set<AnyCancellable>()
  private var autoSyncEnabled = false
  private var isApplyingRemoteChanges = false

  init(store: AnnotationStore, containerID: String? = nil) {
    self.store = store
    let resolvedContainerID = containerID
      ?? (Bundle.main.object(forInfoDictionaryKey: "YazeICloudContainerID") as? String)
      ?? "iCloud.org.halext.yaze"
    let container = CKContainer(identifier: resolvedContainerID)
    self.database = container.privateCloudDatabase

    store.$annotations
      .dropFirst()
      .debounce(for: .seconds(1.2), scheduler: DispatchQueue.main)
      .sink { [weak self] _ in
        guard let self else { return }
        guard self.autoSyncEnabled && !self.isApplyingRemoteChanges else { return }
        self.pushAnnotations()
      }
      .store(in: &cancellables)
  }

  func startAutomaticSync() {
    guard !autoSyncEnabled else { return }
    autoSyncEnabled = true
    setupSubscription()
    pullAnnotations()
  }

  func stopAutomaticSync() {
    autoSyncEnabled = false
    syncStatus = "iCloud sync paused"
  }

  // MARK: - Push

  /// Push local annotations to CloudKit.
  func pushAnnotations() {
    guard autoSyncEnabled else { return }
    syncStatus = "Pushing..."

    let records = store.annotations.map { ann -> CKRecord in
      let record = CKRecord(recordType: "OracleAnnotation",
                            recordID: CKRecord.ID(recordName: ann.id))
      record["roomID"] = ann.roomID as CKRecordValue
      record["text"] = ann.text as CKRecordValue
      record["priority"] = ann.priority.rawValue as CKRecordValue
      record["category"] = ann.category as CKRecordValue
      record["createdBy"] = ann.createdBy as CKRecordValue
      record["createdAt"] = ann.createdAt as CKRecordValue
      record["modifiedAt"] = ann.modifiedAt as CKRecordValue
      return record
    }

    let operation = CKModifyRecordsOperation(recordsToSave: records)
    operation.savePolicy = .changedKeys
    operation.modifyRecordsResultBlock = { [weak self] result in
      DispatchQueue.main.async {
        switch result {
        case .success:
          self?.syncStatus = "Pushed \(records.count) annotations"
          self?.lastSyncDate = Date()
        case .failure(let error):
          self?.syncStatus = "Push error: \(error.localizedDescription)"
        }
      }
    }

    database.add(operation)
  }

  // MARK: - Pull

  /// Pull remote annotations from CloudKit.
  func pullAnnotations() {
    guard autoSyncEnabled else { return }
    syncStatus = "Pulling..."

    let query = CKQuery(recordType: "OracleAnnotation",
                        predicate: NSPredicate(value: true))
    query.sortDescriptors = [NSSortDescriptor(key: "modificationDate", ascending: false)]

    database.perform(query, inZoneWith: nil) { [weak self] records, error in
      DispatchQueue.main.async {
        guard let self else { return }
        if let error {
          self.syncStatus = "Pull error: \(error.localizedDescription)"
          return
        }

        guard let records else {
          self.syncStatus = "Pull: no records"
          return
        }

        self.isApplyingRemoteChanges = true
        var imported = 0
        var updated = 0
        for record in records {
          var ann = Annotation(
            roomID: (record["roomID"] as? Int) ?? 0,
            text: (record["text"] as? String) ?? "",
            priority: AnnotationPriority(rawValue: (record["priority"] as? Int) ?? 0) ?? .note,
            category: (record["category"] as? String) ?? "",
            createdBy: (record["createdBy"] as? String) ?? "Unknown"
          )

          // Preserve CloudKit record identity to avoid duplicates on repeated pulls.
          ann.id = record.recordID.recordName
          ann.createdAt = (record["createdAt"] as? Date)
            ?? record.creationDate
            ?? ann.createdAt
          ann.modifiedAt = (record["modifiedAt"] as? Date)
            ?? record.modificationDate
            ?? ann.createdAt

          switch self.store.upsertSyncedAnnotation(ann) {
          case .inserted:
            imported += 1
          case .updated:
            updated += 1
          case .unchanged:
            break
          }
        }
        self.isApplyingRemoteChanges = false

        self.syncStatus = "Pulled \(imported) new, \(updated) updated"
        self.lastSyncDate = Date()
      }
    }
  }

  // MARK: - Subscription

  /// Set up a CKDatabaseSubscription for real-time updates.
  func setupSubscription() {
    let sub = CKDatabaseSubscription(subscriptionID: "annotation-changes")
    let info = CKSubscription.NotificationInfo()
    info.shouldSendContentAvailable = true
    sub.notificationInfo = info

    database.save(sub) { [weak self] saved, error in
      DispatchQueue.main.async {
        if error != nil {
          self?.syncStatus = "Subscription setup failed"
        } else {
          self?.subscription = saved as? CKDatabaseSubscription
          self?.syncStatus = "Subscription active"
        }
      }
    }
  }

  /// Handle a remote notification (call from AppDelegate).
  func handleNotification() {
    if autoSyncEnabled {
      pullAnnotations()
    }
  }
}
