import SwiftUI

/// Displays live-rendered dungeon room images from the desktop.
struct RemoteRoomViewerView: View {
  @ObservedObject var apiClient: DesktopAPIClient

  @State private var selectedRoomId: Int = 0
  @State private var roomImage: UIImage?
  @State private var metadata: RoomMetadataResponse?
  @State private var isLoading = false
  @State private var errorMessage = ""
  @State private var scale: Float = 2.0
  @State private var activeOverlays: Set<RoomOverlay> = []
  @State private var showMetadata = false
  @State private var searchText = ""
  @State private var showRoomBrowser = false
  @State private var refreshID = 0
  @State private var fitToView = true

  private struct Request: Hashable {
    let room: Int
    let scale: Float
    let overlays: [String]
    let host: String?
    let connected: Bool
    let refresh: Int
  }

  private var request: Request {
    Request(room: selectedRoomId, scale: scale,
            overlays: activeOverlays.map(\.rawValue).sorted(),
            host: apiClient.connectedHost?.baseURL, connected: apiClient.isConnected,
            refresh: refreshID)
  }

  private var allRooms: [RoomSummary] {
    let rooms = RoomCatalog.allRooms
    guard !searchText.isEmpty else { return rooms }
    let query = searchText.lowercased()
    return rooms.filter { room in
      room.hexLabel.lowercased().contains(query) ||
      "\(room.id)".contains(query)
    }
  }

  var body: some View {
    VStack(spacing: 0) {
      // Room selector bar
      roomSelectorBar

      // Main content
      if !apiClient.isConnected {
        ContentUnavailableView("Desktop disconnected", systemImage: "desktopcomputer",
          description: Text("Return to Desktop Connection to reconnect and review rooms."))
      } else if isLoading {
        Spacer()
        ProgressView("Rendering room...")
        Spacer()
      } else if let image = roomImage {
        roomImageView(image)
      } else if !errorMessage.isEmpty {
        Spacer()
        ContentUnavailableView {
          Label("Room could not be loaded", systemImage: "exclamationmark.triangle")
        } description: {
          Text(errorMessage)
        } actions: {
          Button("Try again") { refreshID += 1 }
            .buttonStyle(.borderedProminent)
        }
        Spacer()
      } else {
        Spacer()
        Text("Select a room to view")
          .foregroundStyle(.secondary)
        Spacer()
      }

      // Overlay toggles
      overlayBar
    }
    .navigationTitle("Room Viewer")
    .navigationBarTitleDisplayMode(.inline)
    .toolbar {
      ToolbarItem(placement: .topBarTrailing) {
        Button { refreshID += 1 } label: { Image(systemName: "arrow.clockwise") }
          .accessibilityLabel("Refresh room from desktop")
          .disabled(!apiClient.isConnected)
      }
      ToolbarItem(placement: .topBarTrailing) {
        Button {
          showMetadata.toggle()
        } label: {
          Image(systemName: "info.circle")
        }
        .accessibilityLabel("Room details")
        .disabled(metadata == nil || isLoading)
      }
      ToolbarItem(placement: .topBarTrailing) {
        Button {
          showRoomBrowser.toggle()
        } label: {
          Image(systemName: "list.bullet")
        }
        .accessibilityLabel("Browse rooms")
      }
    }
    .sheet(isPresented: $showMetadata) {
      metadataSheet
    }
    .sheet(isPresented: $showRoomBrowser) {
      roomBrowserSheet
    }
    .onChange(of: scale) { _, _ in fitToView = false }
    .task(id: request) {
      await loadRoom(request)
    }
  }

  // MARK: - Subviews

  private var roomSelectorBar: some View {
    ViewThatFits(in: .horizontal) {
      HStack { roomNavigation; Spacer(); scalePicker }
      VStack(spacing: 8) { roomNavigation; scalePicker }
    }
    .padding(.horizontal)
    .padding(.vertical, 8)
    .background(.bar)
  }

  private var roomNavigation: some View {
    HStack(spacing: 12) {
      Button { selectedRoomId -= 1 } label: {
        Image(systemName: "chevron.left").frame(minWidth: 44, minHeight: 44)
      }
      .disabled(selectedRoomId <= 0)
      .accessibilityLabel("Previous room")
      Button { showRoomBrowser = true } label: {
        Text(String(format: "Room 0x%03X", selectedRoomId))
          .font(.headline.monospacedDigit())
          .frame(minHeight: 44)
      }
      .accessibilityHint("Opens the room browser")
      Button { selectedRoomId += 1 } label: {
        Image(systemName: "chevron.right").frame(minWidth: 44, minHeight: 44)
      }
      .disabled(selectedRoomId >= RoomCatalog.totalRooms - 1)
      .accessibilityLabel("Next room")
    }
  }

  private var scalePicker: some View {
    HStack {
      Button("Fit") { fitToView = true }
        .frame(minWidth: 44, minHeight: 44)
        .accessibilityLabel("Fit whole room")
        .accessibilityAddTraits(fitToView ? .isSelected : [])
      Picker("Render scale", selection: $scale) {
        ForEach(1...4, id: \.self) { value in
          Text("\(value)×").tag(Float(value))
        }
      }
      .pickerStyle(.segmented)
      .frame(minWidth: 180, idealWidth: 220, maxWidth: 260)
      .accessibilityLabel("Render scale")
    }
  }

  private func roomImageView(_ image: UIImage) -> some View {
    GeometryReader { geometry in
      let ratio = image.size.width / max(image.size.height, 1)
      let width = fitToView
        ? min(geometry.size.width, geometry.size.height * ratio)
        : image.size.width
      ScrollView([.horizontal, .vertical]) {
        Image(uiImage: image)
          .interpolation(.none)
          .resizable()
          .frame(width: width, height: width / ratio)
          .frame(minWidth: geometry.size.width, minHeight: geometry.size.height)
          .accessibilityLabel(String(format: "Dungeon room %03X preview", selectedRoomId))
      }
    }
  }

  private var overlayBar: some View {
    ScrollView(.horizontal, showsIndicators: false) {
      HStack(spacing: 8) {
        ForEach(RoomOverlay.allCases) { overlay in
          let isActive = activeOverlays.contains(overlay)
          Button {
            if isActive {
              activeOverlays.remove(overlay)
            } else {
              activeOverlays.insert(overlay)
            }
          } label: {
            Label(overlay.label, systemImage: overlay.systemImage)
              .font(.caption)
              .padding(.horizontal, 10)
              .frame(minHeight: 44)
              .background(isActive ? Color.accentColor.opacity(0.2) : Color.clear)
              .clipShape(Capsule())
              .overlay(Capsule().stroke(isActive ? Color.accentColor : Color.secondary.opacity(0.3)))
          }
          .buttonStyle(.plain)
          .accessibilityValue(isActive ? "On" : "Off")
          .accessibilityAddTraits(isActive ? .isSelected : [])
        }
      }
      .padding(.horizontal)
      .padding(.vertical, 8)
    }
    .background(.bar)
  }

  private var metadataSheet: some View {
    NavigationStack {
      List {
        if let meta = metadata {
          Section("Room Properties") {
            metadataRow("Room ID", String(format: "0x%03X (%d)", meta.roomId, meta.roomId))
            metadataRow("Blockset", "\(meta.blockset)")
            metadataRow("Spriteset", "\(meta.spriteset)")
            metadataRow("Palette", "\(meta.palette)")
            metadataRow("Layout", "\(meta.layoutId)")
            metadataRow("Effect", "\(meta.effect)")
            metadataRow("Collision", "\(meta.collision)")
          }
          Section("Tags") {
            metadataRow("Tag 1", "\(meta.tag1)")
            metadataRow("Tag 2", "\(meta.tag2)")
            metadataRow("Message ID", String(format: "0x%04X", meta.messageId))
          }
          Section("Contents") {
            metadataRow("Objects", "\(meta.objectCount)")
            metadataRow("Sprites", "\(meta.spriteCount)")
            metadataRow("Custom Collision", meta.hasCustomCollision ? "Yes" : "No")
          }
        } else {
          Text("No metadata loaded")
            .foregroundStyle(.secondary)
        }
      }
      .navigationTitle("Room Details")
      .navigationBarTitleDisplayMode(.inline)
      .toolbar { ToolbarItem(placement: .confirmationAction) {
        Button("Done") { showMetadata = false }
      } }
    }
    .presentationDetents([.medium, .large])
  }

  private func metadataRow(_ label: String, _ value: String) -> some View {
    HStack {
      Text(label)
        .foregroundStyle(.secondary)
      Spacer()
      Text(value)
        .font(.body.monospacedDigit())
    }
  }

  private var roomBrowserSheet: some View {
    NavigationStack {
      List {
        ForEach(allRooms) { room in
          Button {
            selectedRoomId = room.id
            showRoomBrowser = false
          } label: {
            HStack {
              Text(room.hexLabel)
                .font(.body.monospacedDigit())
              Spacer()
              if room.id == selectedRoomId {
                Image(systemName: "checkmark")
                  .foregroundStyle(.blue)
              }
            }
          }
          .buttonStyle(.plain)
        }
      }
      .searchable(text: $searchText, prompt: "Room ID (hex or decimal)")
      .overlay {
        if allRooms.isEmpty { ContentUnavailableView.search(text: searchText) }
      }
      .toolbar { ToolbarItem(placement: .confirmationAction) {
        Button("Done") { showRoomBrowser = false }
      } }
      .navigationTitle("Room Browser")
      .navigationBarTitleDisplayMode(.inline)
    }
    .presentationDetents([.medium, .large])
  }

  // MARK: - Data loading

  @MainActor
  private func loadRoom(_ requested: Request) async {
    guard !Task.isCancelled, requested == request else { return }
    roomImage = nil
    metadata = nil
    errorMessage = ""
    isLoading = requested.connected
    guard requested.connected else { return }

    do {
      async let imageData = apiClient.fetchRoomImage(
        roomId: requested.room, overlays: requested.overlays, scale: requested.scale)
      async let metaResponse = apiClient.fetchRoomMetadata(roomId: requested.room)
      let (data, meta) = try await (imageData, metaResponse)
      try Task.checkCancellation()
      guard requested == request else { return }
      guard let image = UIImage(data: data), meta.roomId == requested.room else {
        throw URLError(.cannotDecodeContentData)
      }
      roomImage = image
      metadata = meta
      isLoading = false
    } catch {
      guard !Task.isCancelled, requested == request else { return }
      errorMessage = error.localizedDescription
      isLoading = false
    }
  }
}
