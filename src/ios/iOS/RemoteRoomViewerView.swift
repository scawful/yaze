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
      if isLoading {
        Spacer()
        ProgressView("Rendering room...")
        Spacer()
      } else if let image = roomImage {
        roomImageView(image)
      } else if !errorMessage.isEmpty {
        Spacer()
        Label(errorMessage, systemImage: "exclamationmark.triangle")
          .foregroundStyle(.orange)
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
        Button {
          showMetadata.toggle()
        } label: {
          Image(systemName: "info.circle")
        }
      }
      ToolbarItem(placement: .topBarTrailing) {
        Button {
          showRoomBrowser.toggle()
        } label: {
          Image(systemName: "list.bullet")
        }
      }
    }
    .sheet(isPresented: $showMetadata) {
      metadataSheet
    }
    .sheet(isPresented: $showRoomBrowser) {
      roomBrowserSheet
    }
    .onChange(of: selectedRoomId) { _, _ in
      loadRoom()
    }
    .onChange(of: activeOverlays) { _, _ in
      loadRoom()
    }
    .onChange(of: scale) { _, _ in
      loadRoom()
    }
  }

  // MARK: - Subviews

  private var roomSelectorBar: some View {
    HStack(spacing: 12) {
      Button {
        if selectedRoomId > 0 { selectedRoomId -= 1 }
      } label: {
        Image(systemName: "chevron.left")
      }
      .disabled(selectedRoomId <= 0)

      Text(String(format: "Room 0x%03X", selectedRoomId))
        .font(.headline.monospacedDigit())
        .frame(minWidth: 120)

      Button {
        if selectedRoomId < RoomCatalog.totalRooms - 1 { selectedRoomId += 1 }
      } label: {
        Image(systemName: "chevron.right")
      }
      .disabled(selectedRoomId >= RoomCatalog.totalRooms - 1)

      Spacer()

      HStack(spacing: 4) {
        Text("Scale:")
          .font(.caption)
          .foregroundStyle(.secondary)
        Picker("Scale", selection: $scale) {
          Text("1x").tag(Float(1.0))
          Text("2x").tag(Float(2.0))
          Text("3x").tag(Float(3.0))
          Text("4x").tag(Float(4.0))
        }
        .pickerStyle(.segmented)
        .frame(width: 180)
      }
    }
    .padding(.horizontal)
    .padding(.vertical, 8)
    .background(.bar)
  }

  private func roomImageView(_ image: UIImage) -> some View {
    ScrollView([.horizontal, .vertical]) {
      Image(uiImage: image)
        .interpolation(.none)
        .resizable()
        .aspectRatio(contentMode: .fit)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
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
              .padding(.vertical, 6)
              .background(isActive ? Color.accentColor.opacity(0.2) : Color.clear)
              .clipShape(Capsule())
              .overlay(Capsule().stroke(isActive ? Color.accentColor : Color.secondary.opacity(0.3)))
          }
          .buttonStyle(.plain)
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
      .navigationTitle("Room Metadata")
      .navigationBarTitleDisplayMode(.inline)
    }
    .presentationDetents([.medium])
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
        TextField("Search rooms...", text: $searchText)
          .textFieldStyle(.roundedBorder)

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
      .navigationTitle("Room Browser")
      .navigationBarTitleDisplayMode(.inline)
    }
    .presentationDetents([.medium, .large])
  }

  // MARK: - Data loading

  private func loadRoom() {
    guard apiClient.isConnected else { return }
    isLoading = true
    errorMessage = ""

    let overlayList = activeOverlays.map(\.rawValue)

    Task {
      do {
        async let imageData = apiClient.fetchRoomImage(
          roomId: selectedRoomId,
          overlays: overlayList,
          scale: scale
        )
        async let metaResponse = apiClient.fetchRoomMetadata(roomId: selectedRoomId)

        let data = try await imageData
        let meta = try await metaResponse

        await MainActor.run {
          roomImage = UIImage(data: data)
          metadata = meta
          isLoading = false
        }
      } catch {
        await MainActor.run {
          errorMessage = error.localizedDescription
          isLoading = false
        }
      }
    }
  }
}
