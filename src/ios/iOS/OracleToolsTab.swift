import SwiftUI
import Foundation

/// Container organizing Oracle-specific tool views with sidebar navigation.
struct OracleToolsTab: View {
  @ObservedObject var settingsStore: YazeSettingsStore
  @StateObject private var annotationStore: AnnotationStore
  @StateObject private var annotationSyncEngine: AnnotationSyncEngine
  @State private var storyEvents: [StoryEventInfo] = []

  init(settingsStore: YazeSettingsStore) {
    self.settingsStore = settingsStore
    let store = AnnotationStore(
      projectPath: settingsStore.settings.general.lastProjectPath
    )
    _annotationStore = StateObject(
      wrappedValue: store
    )
    _annotationSyncEngine = StateObject(
      wrappedValue: AnnotationSyncEngine(store: store)
    )
  }

  var body: some View {
    NavigationSplitView {
      List {
        NavigationLink {
          AnnotationEditorView(store: annotationStore)
        } label: {
          Label("Annotations", systemImage: "note.text")
        }

        NavigationLink {
          ProgressionDashboardView()
        } label: {
          Label("Progression", systemImage: "diamond.fill")
        }

        NavigationLink {
          StoryGraphBrowserView(events: storyEvents)
        } label: {
          Label("Story Events", systemImage: "point.3.connected.trianglepath.dotted")
        }
      }
      .navigationTitle("Oracle Tools")
      .safeAreaInset(edge: .bottom) {
        if settingsStore.settings.filesystem.useIcloudSync {
          HStack(spacing: 8) {
            Image(systemName: "icloud")
              .foregroundStyle(.secondary)
            Text(annotationSyncEngine.syncStatus)
              .font(.footnote)
              .foregroundStyle(.secondary)
          }
          .padding(.horizontal, 10)
          .padding(.vertical, 6)
        }
      }
    } detail: {
      ContentUnavailableView("Select a Tool",
                             systemImage: "wrench.and.screwdriver",
                             description: Text("Choose an Oracle tool from the sidebar"))
    }
    .onAppear {
      annotationStore.configureProjectPath(
        settingsStore.settings.general.lastProjectPath
      )
      annotationStore.refreshExternalSync()
      if settingsStore.settings.filesystem.useIcloudSync {
        annotationSyncEngine.startAutomaticSync()
      } else {
        annotationSyncEngine.stopAutomaticSync()
      }
      loadStoryEvents()
    }
    .onChange(of: settingsStore.settings.general.lastProjectPath) { _, newPath in
      annotationStore.configureProjectPath(newPath)
      annotationStore.refreshExternalSync()
    }
    .onChange(of: settingsStore.settings.filesystem.useIcloudSync) {
      _, useIcloudSync in
      if useIcloudSync {
        annotationSyncEngine.startAutomaticSync()
      } else {
        annotationSyncEngine.stopAutomaticSync()
      }
    }
  }

  private func loadStoryEvents() {
    guard let json = YazeIOSBridge.getStoryEventsJSON(),
          let data = json.data(using: .utf8),
          let raw = try? JSONSerialization.jsonObject(with: data) as? [[String: Any]] else {
      storyEvents = []
      return
    }

    let parsed: [StoryEventInfo] = raw.compactMap { item in
      guard let id = item["id"] as? String,
            let name = item["name"] as? String else {
        return nil
      }

      return StoryEventInfo(
        id: id,
        name: name,
        notes: item["notes"] as? String ?? "",
        dependencies: item["dependencies"] as? [String] ?? [],
        unlocks: item["unlocks"] as? [String] ?? [],
        flags: item["flags"] as? [String] ?? [],
        locations: item["locations"] as? [String] ?? [],
        textIDs: item["text_ids"] as? [String] ?? [],
        scripts: item["scripts"] as? [String] ?? []
      )
    }

    storyEvents = parsed.sorted { $0.id < $1.id }
  }
}
