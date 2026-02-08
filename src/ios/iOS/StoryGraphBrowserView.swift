import SwiftUI

/// A story event node for SwiftUI display.
struct StoryEventInfo: Identifiable {
  let id: String       // "EV-001"
  let name: String     // "Intro begins"
  let notes: String
  let dependencies: [String]
  let unlocks: [String]
  let flags: [String]
  let locations: [String]
  let textIDs: [String]
  let scripts: [String]
}

/// Expandable tree view of Oracle story events.
struct StoryGraphBrowserView: View {
  let events: [StoryEventInfo]

  @State private var expandedEvents: Set<String> = []
  @State private var searchText = ""

  var body: some View {
    List {
      ForEach(filteredEvents) { event in
        DisclosureGroup(
          isExpanded: Binding(
            get: { expandedEvents.contains(event.id) },
            set: { expanded in
              if expanded {
                expandedEvents.insert(event.id)
              } else {
                expandedEvents.remove(event.id)
              }
            }
          )
        ) {
          eventDetailView(event)
        } label: {
          HStack(spacing: 8) {
            Text(event.id)
              .font(.caption.monospaced().weight(.bold))
              .foregroundStyle(.secondary)
            Text(event.name)
              .font(.body)
          }
        }
      }
    }
    .searchable(text: $searchText, prompt: "Filter events")
    .navigationTitle("Story Events")
  }

  private var filteredEvents: [StoryEventInfo] {
    if searchText.isEmpty {
      return events
    }
    return events.filter { event in
      event.name.localizedCaseInsensitiveContains(searchText) ||
      event.id.localizedCaseInsensitiveContains(searchText) ||
      event.notes.localizedCaseInsensitiveContains(searchText)
    }
  }

  @ViewBuilder
  private func eventDetailView(_ event: StoryEventInfo) -> some View {
    VStack(alignment: .leading, spacing: 8) {
      if !event.flags.isEmpty {
        detailSection(title: "Flags", items: event.flags, icon: "flag")
      }
      if !event.locations.isEmpty {
        detailSection(title: "Locations", items: event.locations, icon: "mappin")
      }
      if !event.textIDs.isEmpty {
        detailSection(title: "Text IDs", items: event.textIDs, icon: "text.bubble")
      }
      if !event.scripts.isEmpty {
        detailSection(title: "Scripts", items: event.scripts, icon: "chevron.left.forwardslash.chevron.right")
      }
      if !event.dependencies.isEmpty {
        detailSection(title: "Requires", items: event.dependencies, icon: "arrow.down.left")
      }
      if !event.unlocks.isEmpty {
        detailSection(title: "Unlocks", items: event.unlocks, icon: "arrow.up.right")
      }
      if !event.notes.isEmpty {
        Text(event.notes)
          .font(.caption)
          .foregroundStyle(.secondary)
          .padding(.top, 4)
      }
    }
    .padding(.vertical, 4)
  }

  private func detailSection(title: String, items: [String], icon: String) -> some View {
    VStack(alignment: .leading, spacing: 2) {
      Label(title, systemImage: icon)
        .font(.caption.weight(.semibold))
        .foregroundStyle(.secondary)
      ForEach(items, id: \.self) { item in
        Text("  \(item)")
          .font(.caption)
      }
    }
  }
}
