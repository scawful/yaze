import SwiftUI

/// View for discovering and connecting to desktop yaze instances.
struct DesktopConnectionView: View {
  @ObservedObject var apiClient: DesktopAPIClient
  @StateObject private var discovery = DesktopDiscoveryService()

  @State private var manualHost = ""
  @State private var manualPort = "8080"
  @State private var selectedTab = 0

  var body: some View {
    NavigationStack {
      List {
        connectionStatusSection

        if apiClient.isConnected {
          connectedActionsSection
        } else {
          discoveredHostsSection
          manualConnectionSection
        }
      }
      .navigationTitle("Desktop Connection")
      .navigationBarTitleDisplayMode(.inline)
      .onAppear {
        discovery.startBrowsing()
      }
      .onDisappear {
        discovery.stopBrowsing()
      }
    }
  }

  // MARK: - Sections

  private var connectionStatusSection: some View {
    Section {
      HStack {
        Image(systemName: apiClient.isConnected ? "checkmark.circle.fill" : "circle")
          .foregroundStyle(apiClient.isConnected ? .green : .secondary)
        VStack(alignment: .leading, spacing: 2) {
          Text(apiClient.isConnected ? "Connected" : "Disconnected")
            .font(.headline)
          if let host = apiClient.connectedHost {
            Text(host.baseURL)
              .font(.caption)
              .foregroundStyle(.secondary)
          }
        }
        Spacer()
        if apiClient.isConnected {
          Button("Disconnect") {
            apiClient.disconnect()
          }
          .buttonStyle(.bordered)
          .tint(.red)
        }
      }

      if !apiClient.lastError.isEmpty {
        Label(apiClient.lastError, systemImage: "exclamationmark.triangle")
          .font(.caption)
          .foregroundStyle(.orange)
      }
    } header: {
      Text("Status")
    }
  }

  private var discoveredHostsSection: some View {
    Section {
      if discovery.discoveredHosts.isEmpty {
        HStack {
          if discovery.isSearching {
            ProgressView()
              .scaleEffect(0.8)
            Text("Searching for yaze instances...")
              .font(.subheadline)
              .foregroundStyle(.secondary)
          } else {
            Text("No instances found")
              .font(.subheadline)
              .foregroundStyle(.secondary)
          }
        }
      } else {
        ForEach(discovery.discoveredHosts) { host in
          Button {
            apiClient.connect(to: host)
          } label: {
            HStack {
              VStack(alignment: .leading, spacing: 2) {
                Text(host.name)
                  .font(.headline)
                HStack(spacing: 8) {
                  if !host.host.isEmpty {
                    Text(host.baseURL)
                      .font(.caption)
                      .foregroundStyle(.secondary)
                  }
                  if !host.romTitle.isEmpty {
                    Text(host.romTitle)
                      .font(.caption)
                      .padding(.horizontal, 6)
                      .padding(.vertical, 2)
                      .background(.blue.opacity(0.15))
                      .clipShape(Capsule())
                  }
                }
              }
              Spacer()
              Image(systemName: "arrow.right.circle")
                .foregroundStyle(.blue)
            }
          }
          .buttonStyle(.plain)
        }
      }
    } header: {
      HStack {
        Text("Discovered Hosts")
        Spacer()
        if discovery.isSearching {
          ProgressView()
            .scaleEffect(0.6)
        }
      }
    }
  }

  private var manualConnectionSection: some View {
    Section {
      TextField("Host (e.g. 192.168.1.100)", text: $manualHost)
        .textContentType(.URL)
        .autocorrectionDisabled()
        .textInputAutocapitalization(.never)

      TextField("Port", text: $manualPort)
        .keyboardType(.numberPad)

      Button("Connect") {
        let port = UInt16(manualPort) ?? 8080
        apiClient.connectManual(host: manualHost, port: port)
      }
      .disabled(manualHost.trimmingCharacters(in: .whitespaces).isEmpty)
    } header: {
      Text("Manual Connection")
    } footer: {
      Text("Enter the IP address and port of your desktop yaze instance.")
    }
  }

  private var connectedActionsSection: some View {
    Section {
      NavigationLink {
        RemoteRoomViewerView(apiClient: apiClient)
      } label: {
        Label("Room Viewer", systemImage: "square.grid.3x3")
      }

      NavigationLink {
        RemoteCommandRunnerView(apiClient: apiClient)
      } label: {
        Label("Command Runner", systemImage: "terminal")
      }

      NavigationLink {
        RemoteAnnotationReviewView(apiClient: apiClient)
      } label: {
        Label("Annotation Review", systemImage: "note.text")
      }
    } header: {
      Text("Remote Tools")
    }
  }
}
