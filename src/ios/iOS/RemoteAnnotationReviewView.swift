import SwiftUI

/// Browse rooms with annotation overlays; add/edit/delete annotations that
/// sync to the desktop's annotations.json via REST.
struct RemoteAnnotationReviewView: View {
  @ObservedObject var apiClient: DesktopAPIClient

  @State private var annotations: [RemoteAnnotation] = []
  @State private var selectedRoomId: Int?
  @State private var roomImage: UIImage?
  @State private var isLoading = false
  @State private var errorMessage = ""
  @State private var showAddSheet = false
  @State private var editingAnnotation: RemoteAnnotation?

  /// Rooms that have at least one annotation, sorted by room ID.
  private var annotatedRooms: [(Int, [RemoteAnnotation])] {
    let grouped = Dictionary(grouping: annotations, by: \.roomID)
    return grouped.sorted { $0.key < $1.key }
  }

  /// Annotations for the currently selected room.
  private var currentAnnotations: [RemoteAnnotation] {
    guard let roomId = selectedRoomId else { return [] }
    return annotations.filter { $0.roomID == roomId }
  }

  var body: some View {
    NavigationSplitView {
      sidebar
    } detail: {
      detailView
    }
    .navigationTitle("Annotation Review")
    .navigationBarTitleDisplayMode(.inline)
    .toolbar {
      ToolbarItem(placement: .topBarTrailing) {
        Button {
          Task { await refresh() }
        } label: {
          Image(systemName: "arrow.clockwise")
        }
      }
    }
    .sheet(isPresented: $showAddSheet) {
      annotationEditorSheet(annotation: nil)
    }
    .sheet(item: $editingAnnotation) { annotation in
      annotationEditorSheet(annotation: annotation)
    }
    .task {
      await refresh()
    }
  }

  // MARK: - Sidebar

  private var sidebar: some View {
    List(selection: $selectedRoomId) {
      if annotations.isEmpty && !isLoading {
        Text("No annotations found")
          .foregroundStyle(.secondary)
      }

      ForEach(annotatedRooms, id: \.0) { roomId, roomAnnotations in
        NavigationLink(value: roomId) {
          HStack {
            Text(String(format: "Room 0x%03X", roomId))
              .font(.subheadline.monospacedDigit())
            Spacer()
            // Priority badge: show highest priority
            let maxPriority = roomAnnotations.map(\.priority).max() ?? 0
            priorityBadge(maxPriority)
            Text("\(roomAnnotations.count)")
              .font(.caption.monospacedDigit())
              .padding(.horizontal, 6)
              .padding(.vertical, 2)
              .background(Color.secondary.opacity(0.15))
              .clipShape(Capsule())
          }
        }
      }
    }
    .listStyle(.sidebar)
    .navigationTitle("Rooms")
  }

  // MARK: - Detail

  private var detailView: some View {
    Group {
      if let roomId = selectedRoomId {
        VStack(spacing: 0) {
          // Room image header
          if let image = roomImage {
            Image(uiImage: image)
              .interpolation(.none)
              .resizable()
              .aspectRatio(contentMode: .fit)
              .frame(maxHeight: 250)
              .clipShape(RoundedRectangle(cornerRadius: 8))
              .padding()
          }

          // Annotations list
          List {
            Section {
              ForEach(currentAnnotations) { annotation in
                annotationRow(annotation)
                  .swipeActions(edge: .trailing) {
                    Button(role: .destructive) {
                      deleteAnnotation(annotation)
                    } label: {
                      Label("Delete", systemImage: "trash")
                    }
                    Button {
                      editingAnnotation = annotation
                    } label: {
                      Label("Edit", systemImage: "pencil")
                    }
                    .tint(.blue)
                  }
              }
            } header: {
              HStack {
                Text("\(currentAnnotations.count) Annotations")
                Spacer()
                Button {
                  showAddSheet = true
                } label: {
                  Image(systemName: "plus")
                }
              }
            }
          }
        }
        .onChange(of: roomId) { _, newId in
          loadRoomImage(newId)
        }
        .onAppear {
          loadRoomImage(roomId)
        }
      } else {
        ContentUnavailableView("Select a Room",
                              systemImage: "square.grid.3x3",
                              description: Text("Choose a room from the sidebar to view its annotations."))
      }
    }
  }

  // MARK: - Annotation Row

  private func annotationRow(_ annotation: RemoteAnnotation) -> some View {
    VStack(alignment: .leading, spacing: 4) {
      HStack {
        priorityBadge(annotation.priority)
        if !annotation.category.isEmpty {
          Text(annotation.category)
            .font(.caption2)
            .padding(.horizontal, 6)
            .padding(.vertical, 2)
            .background(Color.blue.opacity(0.1))
            .clipShape(Capsule())
        }
        Spacer()
        if let createdBy = annotation.createdBy, !createdBy.isEmpty {
          Text(createdBy)
            .font(.caption2)
            .foregroundStyle(.secondary)
        }
      }
      Text(annotation.text)
        .font(.subheadline)
      if let modifiedAt = annotation.modifiedAt, !modifiedAt.isEmpty {
        Text(modifiedAt)
          .font(.caption2)
          .foregroundStyle(.tertiary)
      }
    }
    .padding(.vertical, 4)
  }

  private func priorityBadge(_ priority: Int) -> some View {
    let (label, color): (String, Color) = {
      switch priority {
      case 2: return ("Blocker", .red)
      case 1: return ("Bug", .orange)
      default: return ("Note", .blue)
      }
    }()

    return Text(label)
      .font(.caption2.weight(.semibold))
      .padding(.horizontal, 6)
      .padding(.vertical, 2)
      .background(color.opacity(0.15))
      .foregroundStyle(color)
      .clipShape(Capsule())
  }

  // MARK: - Editor Sheet

  private func annotationEditorSheet(annotation: RemoteAnnotation?) -> some View {
    AnnotationEditorView(
      apiClient: apiClient,
      roomId: selectedRoomId ?? 0,
      existingAnnotation: annotation
    ) {
      Task { await refresh() }
    }
  }

  // MARK: - Data

  private func refresh() async {
    isLoading = true
    errorMessage = ""
    do {
      let fetched = try await apiClient.fetchAnnotations()
      await MainActor.run {
        annotations = fetched
        isLoading = false
      }
    } catch {
      await MainActor.run {
        errorMessage = error.localizedDescription
        isLoading = false
      }
    }
  }

  private func loadRoomImage(_ roomId: Int) {
    Task {
      do {
        let data = try await apiClient.fetchRoomImage(roomId: roomId, scale: 2.0)
        await MainActor.run {
          roomImage = UIImage(data: data)
        }
      } catch {
        await MainActor.run {
          roomImage = nil
        }
      }
    }
  }

  private func deleteAnnotation(_ annotation: RemoteAnnotation) {
    Task {
      do {
        try await apiClient.deleteAnnotation(id: annotation.id)
        await MainActor.run {
          annotations.removeAll { $0.id == annotation.id }
        }
      } catch {
        // Show error if needed
      }
    }
  }
}

// MARK: - Annotation Editor

private struct AnnotationEditorView: View {
  @Environment(\.dismiss) private var dismiss
  @ObservedObject var apiClient: DesktopAPIClient

  let roomId: Int
  let existingAnnotation: RemoteAnnotation?
  let onSave: () -> Void

  @State private var text = ""
  @State private var priority = 0
  @State private var category = ""
  @State private var isSaving = false

  var body: some View {
    NavigationStack {
      Form {
        Section("Room") {
          Text(String(format: "0x%03X", roomId))
            .font(.body.monospacedDigit())
        }

        Section("Priority") {
          Picker("Priority", selection: $priority) {
            Text("Note").tag(0)
            Text("Bug").tag(1)
            Text("Blocker").tag(2)
          }
          .pickerStyle(.segmented)
        }

        Section("Category") {
          TextField("Category (optional)", text: $category)
        }

        Section("Note") {
          TextEditor(text: $text)
            .frame(minHeight: 100)
        }
      }
      .navigationTitle(existingAnnotation == nil ? "New Annotation" : "Edit Annotation")
      .navigationBarTitleDisplayMode(.inline)
      .toolbar {
        ToolbarItem(placement: .cancellationAction) {
          Button("Cancel") { dismiss() }
        }
        ToolbarItem(placement: .confirmationAction) {
          Button("Save") {
            save()
          }
          .disabled(text.trimmingCharacters(in: .whitespaces).isEmpty || isSaving)
        }
      }
      .onAppear {
        if let existing = existingAnnotation {
          text = existing.text
          priority = existing.priority
          category = existing.category
        }
      }
    }
    .presentationDetents([.medium])
  }

  private func save() {
    isSaving = true
    let iso = ISO8601DateFormatter()
    iso.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
    let now = iso.string(from: Date())

    Task {
      do {
        if var existing = existingAnnotation {
          existing.text = text
          existing.priority = priority
          existing.category = category
          existing.modifiedAt = now
          try await apiClient.updateAnnotation(existing)
        } else {
          let annotation = RemoteAnnotation(
            id: UUID().uuidString,
            roomID: roomId,
            text: text,
            priority: priority,
            category: category,
            createdAt: now,
            modifiedAt: now,
            createdBy: UIDevice.current.name
          )
          try await apiClient.pushAnnotation(annotation)
        }
        await MainActor.run {
          onSave()
          dismiss()
        }
      } catch {
        await MainActor.run {
          isSaving = false
        }
      }
    }
  }
}
