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

  init(store: AnnotationStore, containerID: String? = nil) {
    self.store = store
    let resolvedContainerID = containerID
      ?? (Bundle.main.object(forInfoDictionaryKey: "YazeICloudContainerID") as? String)
      ?? "iCloud.org.halext.yaze"
    let container = CKContainer(identifier: resolvedContainerID)
    self.database = container.privateCloudDatabase
  }

  // MARK: - Push

  /// Push local annotations to CloudKit.
  func pushAnnotations() {
    syncStatus = "Pushing..."

    let records = store.annotations.map { ann -> CKRecord in
      let record = CKRecord(recordType: "OracleAnnotation",
                            recordID: CKRecord.ID(recordName: ann.id))
      record["roomID"] = ann.roomID as CKRecordValue
      record["text"] = ann.text as CKRecordValue
      record["priority"] = ann.priority.rawValue as CKRecordValue
      record["category"] = ann.category as CKRecordValue
      record["createdBy"] = ann.createdBy as CKRecordValue
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

        var imported = 0
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

          // Only import if we don't already have this annotation.
          if !self.store.annotations.contains(where: { $0.id == ann.id }) {
            self.store.addAnnotation(ann)
            imported += 1
          }
        }

        self.syncStatus = "Pulled \(imported) new annotations"
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
    pullAnnotations()
  }
}
