import SwiftUI

/// A conflict version entry for display.
struct ConflictVersion: Identifiable {
  let id = UUID()
  let version: NSFileVersion
  let deviceName: String
  let modificationDate: Date?
  let isCurrent: Bool
}

/// SwiftUI sheet for resolving iCloud document conflicts.
///
/// Shows local vs remote versions with metadata (timestamp, device name)
/// and lets the user pick which to keep or duplicate.
struct YazeConflictResolverView: View {
  let document: YazeProjectDocument
  let onResolved: () -> Void

  @Environment(\.dismiss) private var dismiss
  @State private var versions: [ConflictVersion] = []
  @State private var resolving = false

  var body: some View {
    NavigationStack {
      List {
        Section("Conflicting Versions") {
          ForEach(versions) { version in
            HStack {
              VStack(alignment: .leading, spacing: 4) {
                HStack {
                  Text(version.deviceName)
                    .font(.headline)
                  if version.isCurrent {
                    Text("Current")
                      .font(.caption2)
                      .padding(.horizontal, 6)
                      .padding(.vertical, 2)
                      .background(Color.blue.opacity(0.2))
                      .clipShape(Capsule())
                  }
                }
                if let date = version.modificationDate {
                  Text(date, style: .relative)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
              }
              Spacer()
              Button("Keep") {
                resolveKeeping(version)
              }
              .buttonStyle(.bordered)
              .disabled(resolving)
            }
          }
        }

        Section {
          Button("Keep Current Version") {
            resolveKeepCurrent()
          }
          .disabled(resolving)
        }
      }
      .navigationTitle("Resolve Conflict")
      .toolbar {
        ToolbarItem(placement: .cancellationAction) {
          Button("Cancel") { dismiss() }
        }
      }
      .onAppear { loadVersions() }
    }
  }

  private func loadVersions() {
    let conflicts = document.conflictVersions()

    // Current version
    var items = [ConflictVersion(
      version: NSFileVersion.currentVersionOfItem(at: document.fileURL)!,
      deviceName: UIDevice.current.name,
      modificationDate: document.fileModificationDate,
      isCurrent: true
    )]

    // Conflict versions
    for v in conflicts {
      items.append(ConflictVersion(
        version: v,
        deviceName: v.localizedNameOfSavingComputer ?? "Unknown Device",
        modificationDate: v.modificationDate,
        isCurrent: false
      ))
    }

    versions = items
  }

  private func resolveKeepCurrent() {
    resolving = true
    document.resolveConflictsKeepingCurrent()
    resolving = false
    onResolved()
    dismiss()
  }

  private func resolveKeeping(_ selected: ConflictVersion) {
    resolving = true

    if selected.isCurrent {
      resolveKeepCurrent()
      return
    }

    // Replace current with selected version
    do {
      try selected.version.replaceItem(at: document.fileURL, options: [])
      // Mark all conflicts as resolved
      for v in document.conflictVersions() {
        v.isResolved = true
      }
      try? NSFileVersion.removeOtherVersionsOfItem(at: document.fileURL)

      // Reload document
      document.revert(toContentsOf: document.fileURL) { success in
        DispatchQueue.main.async {
          resolving = false
          if success {
            onResolved()
          }
          dismiss()
        }
      }
    } catch {
      resolving = false
      dismiss()
    }
  }
}
