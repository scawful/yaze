import SwiftUI
import UniformTypeIdentifiers

/// iCloud-aware project document browser with sync status icons.
struct ProjectBrowserView: View {
  @ObservedObject var documentManager: YazeDocumentManager
  @ObservedObject var settingsStore: YazeSettingsStore
  @State private var showCreateSheet = false
  @State private var newProjectName = ""
  @State private var showLegacyMigration = false
  @State private var legacyBundles: [URL] = []

  var body: some View {
    List {
      Section {
        HStack {
          Image(systemName: documentManager.isICloudAvailable
                ? "checkmark.icloud" : "xmark.icloud")
            .foregroundStyle(documentManager.isICloudAvailable ? .green : .red)
          Text(documentManager.isICloudAvailable
               ? "iCloud Drive Connected"
               : "iCloud Not Available")
            .font(.subheadline)
        }
      }

      Section("Projects") {
        if documentManager.documents.isEmpty {
          ContentUnavailableView("No Projects",
                                systemImage: "folder.badge.questionmark",
                                description: Text("Create a new project or import a ROM"))
        }

        ForEach(documentManager.documents) { doc in
          Button {
            openDocument(doc)
          } label: {
            HStack {
              VStack(alignment: .leading, spacing: 4) {
                Text(doc.name)
                  .font(.headline)
                if let date = doc.modifiedDate {
                  Text(date, style: .relative)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
              }
              Spacer()
              Image(systemName: doc.syncStatus.sfSymbolName)
                .foregroundStyle(syncColor(for: doc.syncStatus))
            }
          }
          .swipeActions(edge: .trailing) {
            if doc.syncStatus == .conflict {
              Button("Resolve") {
                // Open conflict resolver
              }
              .tint(.orange)
            }
          }
        }
      }

      if !legacyBundles.isEmpty {
        Section("Legacy Bundles Available") {
          ForEach(legacyBundles, id: \.self) { url in
            HStack {
              Image(systemName: "arrow.up.doc")
              Text(url.lastPathComponent)
              Spacer()
              Button("Migrate") {
                migrateLegacyBundle(url)
              }
              .buttonStyle(.bordered)
              .tint(.blue)
            }
          }
        }
      }
    }
    .navigationTitle("Projects")
    .toolbar {
      ToolbarItem(placement: .primaryAction) {
        Button {
          showCreateSheet = true
        } label: {
          Image(systemName: "plus")
        }
      }
      ToolbarItem(placement: .topBarLeading) {
        Button {
          documentManager.startMonitoring()
        } label: {
          Image(systemName: "arrow.clockwise")
        }
      }
    }
    .sheet(isPresented: $showCreateSheet) {
      createProjectSheet
    }
    .onAppear {
      documentManager.startMonitoring()
      legacyBundles = documentManager.detectLegacyBundles()
    }
  }

  private var createProjectSheet: some View {
    NavigationStack {
      Form {
        TextField("Project Name", text: $newProjectName)
      }
      .navigationTitle("New Project")
      .toolbar {
        ToolbarItem(placement: .cancellationAction) {
          Button("Cancel") { showCreateSheet = false }
        }
        ToolbarItem(placement: .confirmationAction) {
          Button("Create") {
            createProject()
            showCreateSheet = false
          }
          .disabled(newProjectName.isEmpty)
        }
      }
    }
  }

  private func openDocument(_ doc: DocumentInfo) {
    if doc.syncStatus == .downloading {
      documentManager.downloadDocument(at: doc.url)
      return
    }
    // Open via bridge
    let projectPath = doc.url.appendingPathComponent("project").path
    let romPath = doc.url.appendingPathComponent("rom").path

    if FileManager.default.fileExists(atPath: projectPath) {
      settingsStore.updateCurrentProjectPath(projectPath)
      YazeIOSBridge.openProject(atPath: projectPath)
    }
    if FileManager.default.fileExists(atPath: romPath) {
      settingsStore.updateCurrentRomPath(romPath)
      YazeIOSBridge.loadRom(atPath: romPath)
    }
  }

  private func createProject() {
    documentManager.createDocument(name: newProjectName) { _ in
      newProjectName = ""
    }
  }

  private func migrateLegacyBundle(_ url: URL) {
    let dest = (documentManager.iCloudContainerURL ?? documentManager.localDocumentsURL)
      .appendingPathComponent("\(url.deletingPathExtension().lastPathComponent).yazeproj")

    YazeProjectDocument.migrateFromLegacy(
      legacyBundleURL: url,
      destinationURL: dest
    ) { result in
      DispatchQueue.main.async {
        switch result {
        case .success:
          documentManager.startMonitoring()
          legacyBundles.removeAll { $0 == url }
        case .failure:
          settingsStore.statusMessage = "Migration failed"
        }
      }
    }
  }

  private func syncColor(for status: DocumentSyncStatus) -> Color {
    switch status {
    case .synced: return .green
    case .downloading, .uploading: return .blue
    case .conflict: return .orange
    case .error: return .red
    case .local: return .secondary
    }
  }
}
