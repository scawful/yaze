import SwiftUI
import Foundation

/// Container organizing Oracle-specific tool views with sidebar navigation.
struct OracleToolsTab: View {
  @StateObject private var annotationStore = AnnotationStore()
  @State private var storyEvents: [StoryEventInfo] = []

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
    } detail: {
      ContentUnavailableView("Select a Tool",
                             systemImage: "wrench.and.screwdriver",
                             description: Text("Choose an Oracle tool from the sidebar"))
    }
    .onAppear {
      loadStoryEvents()
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
