import SwiftUI

/// Room annotation editor with CRUD operations.
struct AnnotationEditorView: View {
  @ObservedObject var store: AnnotationStore
  @State private var selectedRoomID: Int = 0
  @State private var newText = ""
  @State private var newPriority: AnnotationPriority = .note
  @State private var newCategory = ""
  @State private var editingAnnotation: Annotation?

  var body: some View {
    VStack(spacing: 0) {
      // Room picker
      HStack {
        Text("Room:")
          .font(.subheadline.weight(.medium))
        TextField("0x00", value: $selectedRoomID, format: .number)
          .textFieldStyle(.roundedBorder)
          .frame(width: 80)
        Spacer()
        Text("\(filteredAnnotations.count) annotations")
          .font(.caption)
          .foregroundStyle(.secondary)
      }
      .padding()

      Divider()

      // Annotation list
      List {
        ForEach(filteredAnnotations) { ann in
          AnnotationRow(annotation: ann) {
            editingAnnotation = ann
            newText = ann.text
            newPriority = ann.priority
            newCategory = ann.category
          }
        }
        .onDelete { indexSet in
          let items = filteredAnnotations
          for index in indexSet {
            store.deleteAnnotation(id: items[index].id)
          }
        }
      }

      Divider()

      // Add / Edit form
      VStack(spacing: 12) {
        TextField("Annotation text", text: $newText, axis: .vertical)
          .textFieldStyle(.roundedBorder)
          .lineLimit(3)

        HStack {
          Picker("Priority", selection: $newPriority) {
            Text("Note").tag(AnnotationPriority.note)
            Text("Bug").tag(AnnotationPriority.bug)
            Text("Blocker").tag(AnnotationPriority.blocker)
          }
          .pickerStyle(.segmented)

          TextField("Category", text: $newCategory)
            .textFieldStyle(.roundedBorder)
            .frame(width: 100)
        }

        HStack {
          if editingAnnotation != nil {
            Button("Update") {
              updateAnnotation()
            }
            .buttonStyle(.borderedProminent)

            Button("Cancel") {
              clearForm()
            }
            .buttonStyle(.bordered)
          } else {
            Button("Add Annotation") {
              addAnnotation()
            }
            .buttonStyle(.borderedProminent)
            .disabled(newText.isEmpty)
          }
          Spacer()
        }
      }
      .padding()
    }
    .navigationTitle("Annotations")
  }

  private var filteredAnnotations: [Annotation] {
    if selectedRoomID == 0 {
      return store.annotations
    }
    return store.annotationsForRoom(selectedRoomID)
  }

  private func addAnnotation() {
    let ann = Annotation(
      roomID: selectedRoomID,
      text: newText,
      priority: newPriority,
      category: newCategory
    )
    store.addAnnotation(ann)
    clearForm()
  }

  private func updateAnnotation() {
    guard var ann = editingAnnotation else { return }
    ann.text = newText
    ann.priority = newPriority
    ann.category = newCategory
    ann.roomID = selectedRoomID
    store.updateAnnotation(ann)
    clearForm()
  }

  private func clearForm() {
    newText = ""
    newPriority = .note
    newCategory = ""
    editingAnnotation = nil
  }
}

private struct AnnotationRow: View {
  let annotation: Annotation
  let onTap: () -> Void

  var body: some View {
    Button(action: onTap) {
      HStack(spacing: 12) {
        Circle()
          .fill(priorityColor)
          .frame(width: 10, height: 10)

        VStack(alignment: .leading, spacing: 2) {
          Text(annotation.text)
            .font(.body)
            .lineLimit(2)
          HStack {
            Text("Room 0x\(String(format: "%02X", annotation.roomID))")
              .font(.caption2)
              .foregroundStyle(.secondary)
            if !annotation.category.isEmpty {
              Text(annotation.category)
                .font(.caption2)
                .padding(.horizontal, 4)
                .padding(.vertical, 1)
                .background(.ultraThinMaterial)
                .clipShape(Capsule())
            }
          }
        }

        Spacer()

        Text(annotation.modifiedAt, style: .relative)
          .font(.caption2)
          .foregroundStyle(.tertiary)
      }
    }
    .buttonStyle(.plain)
  }

  private var priorityColor: Color {
    switch annotation.priority {
    case .blocker: return .red
    case .bug: return .orange
    case .note: return .blue
    }
  }
}
