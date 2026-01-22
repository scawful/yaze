import SwiftUI
import UniformTypeIdentifiers

@objc(YazeOverlayHostingController)
final class YazeOverlayHostingController: UIHostingController<YazeOverlayView> {
  @objc init() {
    let store = YazeSettingsStore()
    let buildStore = RemoteBuildStore()
    super.init(rootView: YazeOverlayView(settingsStore: store, buildStore: buildStore))
    view.backgroundColor = .clear
    view.isOpaque = false
  }

  @objc required dynamic init?(coder aDecoder: NSCoder) {
    let store = YazeSettingsStore()
    let buildStore = RemoteBuildStore()
    super.init(coder: aDecoder)
    rootView = YazeOverlayView(settingsStore: store, buildStore: buildStore)
    view.backgroundColor = .clear
    view.isOpaque = false
  }
}

struct YazeOverlayView: View {
  @ObservedObject var settingsStore: YazeSettingsStore
  @ObservedObject var buildStore: RemoteBuildStore

  @State private var showSettings = false
  @State private var showAiPanel = false
  @State private var showBuildPanel = false
  @State private var showFilesystemPanel = false
  @State private var showRomPicker = false
  @State private var showProjectPicker = false
  @State private var showExportPicker = false
  @State private var exportURL: URL?

  var body: some View {
    ZStack(alignment: .top) {
      Color.clear.allowsHitTesting(false)
      VStack(spacing: 12) {
        topBar
        Spacer()
      }
      .padding(.top, 18)
      .padding(.horizontal, 16)
    }
    .sheet(isPresented: $showSettings) {
      SettingsView(settingsStore: settingsStore)
    }
    .sheet(isPresented: $showAiPanel) {
      AiHostsView(settingsStore: settingsStore)
    }
    .sheet(isPresented: $showBuildPanel) {
      BuildQueueView(settingsStore: settingsStore, buildStore: buildStore)
    }
    .sheet(isPresented: $showFilesystemPanel) {
      FilesystemSettingsView(settingsStore: settingsStore,
                             showRomPicker: $showRomPicker,
                             showProjectPicker: $showProjectPicker,
                             exportURL: $exportURL,
                             showExportPicker: $showExportPicker)
    }
    .sheet(isPresented: $showRomPicker) {
      DocumentPicker(contentTypes: [UTType(filenameExtension: "sfc") ?? .data,
                                   UTType(filenameExtension: "smc") ?? .data]) { url in
        settingsStore.updateCurrentRomPath(url.path)
        YazeIOSBridge.loadRom(atPath: url.path)
      }
    }
    .sheet(isPresented: $showProjectPicker) {
      DocumentPicker(contentTypes: [UTType.folder]) { url in
        settingsStore.updateCurrentProjectPath(url.path)
      }
    }
    .sheet(isPresented: $showExportPicker) {
      if let exportURL = exportURL {
        ExportPicker(url: exportURL)
      } else {
        Text("Nothing to export")
          .padding()
      }
    }
  }

  private var topBar: some View {
    HStack(spacing: 12) {
      VStack(alignment: .leading, spacing: 2) {
        Text("Yaze iOS")
          .font(.headline)
        Text(settingsStore.settings.general.lastRomPath.isEmpty
             ? "No ROM loaded"
             : URL(fileURLWithPath: settingsStore.settings.general.lastRomPath).lastPathComponent)
          .font(.caption)
          .foregroundStyle(.secondary)
      }
      Spacer()
      Button("ROM") { showRomPicker = true }
        .buttonStyle(.bordered)
      Button("AI") { showAiPanel = true }
        .buttonStyle(.bordered)
      Button("Build") { showBuildPanel = true }
        .buttonStyle(.bordered)
      Button("Files") { showFilesystemPanel = true }
        .buttonStyle(.bordered)
      Button("Settings") { showSettings = true }
        .buttonStyle(.borderedProminent)
    }
    .padding(.vertical, 8)
    .padding(.horizontal, 12)
    .background(.ultraThinMaterial)
    .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
    .overlay(RoundedRectangle(cornerRadius: 14).stroke(Color.white.opacity(0.2)))
  }
}

struct SettingsView: View {
  @ObservedObject var settingsStore: YazeSettingsStore

  var body: some View {
    NavigationStack {
      Form {
        Section("General") {
          Toggle("Autosave", isOn: $settingsStore.settings.general.autosaveEnabled)
          Stepper("Autosave interval: \(Int(settingsStore.settings.general.autosaveInterval))s",
                  value: $settingsStore.settings.general.autosaveInterval,
                  in: 60...900,
                  step: 30)
        }
        Section("Performance") {
          Toggle("VSync", isOn: $settingsStore.settings.performance.vsync)
          Stepper("Target FPS: \(settingsStore.settings.performance.targetFps)",
                  value: $settingsStore.settings.performance.targetFps,
                  in: 30...120,
                  step: 10)
        }
        Section("AI Defaults") {
          TextField("Model", text: $settingsStore.settings.ai.model)
          Slider(value: $settingsStore.settings.ai.temperature, in: 0...1, step: 0.05) {
            Text("Temperature")
          }
          Text("Temp: \(settingsStore.settings.ai.temperature, specifier: "%.2f")")
            .font(.caption)
        }
      }
      .navigationTitle("Settings")
      .toolbar {
        ToolbarItem(placement: .confirmationAction) {
          Button("Save") { settingsStore.saveAndReport() }
        }
      }
    }
  }
}

struct AiHostsView: View {
  @ObservedObject var settingsStore: YazeSettingsStore
  @State private var selectedHost: YazeSettings.AiHost?
  @State private var showAdd = false

  var body: some View {
    NavigationStack {
      List {
        Section("Active Host") {
          Picker("Host", selection: $settingsStore.settings.ai.activeHostId) {
            ForEach(settingsStore.settings.ai.hosts) { host in
              Text(host.label).tag(host.id)
            }
          }
        }

        Section("Hosts") {
          ForEach(settingsStore.settings.ai.hosts) { host in
            Button {
              selectedHost = host
            } label: {
              VStack(alignment: .leading) {
                Text(host.label)
                Text(host.baseUrl).font(.caption).foregroundStyle(.secondary)
              }
            }
          }
          .onDelete { indexSet in
            settingsStore.settings.ai.hosts.remove(atOffsets: indexSet)
            settingsStore.saveAndReport()
          }
        }

        Section("Profiles") {
          ForEach(settingsStore.settings.ai.profiles, id: \.name) { profile in
            Text("\(profile.name) · \(profile.model)")
          }
        }
      }
      .navigationTitle("AI Hosts")
      .toolbar {
        ToolbarItem(placement: .navigationBarTrailing) {
          Button("Add") { showAdd = true }
        }
        ToolbarItem(placement: .confirmationAction) {
          Button("Save") { settingsStore.saveAndReport() }
        }
      }
      .sheet(item: $selectedHost) { host in
        AiHostEditorView(settingsStore: settingsStore, host: host)
      }
      .sheet(isPresented: $showAdd) {
        AiHostEditorView(settingsStore: settingsStore,
                         host: YazeSettings.AiHost(id: UUID().uuidString,
                                                   label: "New Host",
                                                   baseUrl: "http://",
                                                   apiType: "openai",
                                                   supportsVision: false,
                                                   supportsTools: true,
                                                   supportsStreaming: true,
                                                   allowInsecure: false,
                                                   credentialId: ""))
      }
    }
  }
}

struct AiHostEditorView: View {
  @Environment(\.dismiss) var dismiss
  @ObservedObject var settingsStore: YazeSettingsStore
  @State var host: YazeSettings.AiHost
  @State private var apiKey: String = ""
  @State private var pingStatus: String = ""

  var body: some View {
    NavigationStack {
      Form {
        TextField("Label", text: $host.label)
        TextField("Base URL", text: $host.baseUrl)
        TextField("API Type", text: $host.apiType)
        Toggle("Supports Vision", isOn: $host.supportsVision)
        Toggle("Supports Tools", isOn: $host.supportsTools)
        Toggle("Supports Streaming", isOn: $host.supportsStreaming)
        Toggle("Allow Insecure", isOn: $host.allowInsecure)
        SecureField("API Key (stored in Keychain)", text: $apiKey)
        Button("Ping Host") { pingHost() }
        if !pingStatus.isEmpty {
          Text(pingStatus).font(.caption).foregroundStyle(.secondary)
        }
      }
      .navigationTitle("Host")
      .toolbar {
        ToolbarItem(placement: .confirmationAction) {
          Button("Save") {
            upsertHost()
            dismiss()
          }
        }
      }
      .onAppear {
        if !host.credentialId.isEmpty,
           let value = try? KeychainStore.load(key: host.credentialId) {
          apiKey = value ?? ""
        }
      }
    }
  }

  private func upsertHost() {
    var updated = host
    if !apiKey.isEmpty {
      if updated.credentialId.isEmpty {
        updated.credentialId = "yaze-ai-\(UUID().uuidString)"
      }
      try? KeychainStore.save(value: apiKey, forKey: updated.credentialId)
    }

    if let index = settingsStore.settings.ai.hosts.firstIndex(where: { $0.id == updated.id }) {
      settingsStore.settings.ai.hosts[index] = updated
    } else {
      settingsStore.settings.ai.hosts.append(updated)
    }
    settingsStore.saveAndReport()
  }

  private func pingHost() {
    guard let url = URL(string: host.baseUrl + "/health") else {
      pingStatus = "Invalid host URL"
      return
    }
    pingStatus = "Pinging..."
    URLSession.shared.dataTask(with: url) { _, response, error in
      DispatchQueue.main.async {
        if let error = error {
          pingStatus = "Ping failed: \(error.localizedDescription)"
          return
        }
        if let http = response as? HTTPURLResponse {
          pingStatus = "Ping status: \(http.statusCode)"
        } else {
          pingStatus = "Ping complete"
        }
      }
    }.resume()
  }
}

struct BuildQueueView: View {
  @ObservedObject var settingsStore: YazeSettingsStore
  @ObservedObject var buildStore: RemoteBuildStore

  var body: some View {
    NavigationStack {
      VStack {
        Form {
          Section("Remote Host") {
            Picker("Build Host", selection: $settingsStore.settings.ai.remoteBuildHostId) {
              ForEach(settingsStore.settings.ai.hosts) { host in
                Text(host.label).tag(host.id)
              }
            }
          }

          Section("Actions") {
            Button("Queue Build") { queueBuild() }
            Button("Queue Tests") { queueTests() }
            Button("Refresh Jobs") { refreshJobs() }
          }

          Section("Jobs") {
            if buildStore.jobs.isEmpty {
              Text("No queued jobs")
            }
            ForEach(buildStore.jobs) { job in
              VStack(alignment: .leading) {
                Text("\(job.kind.rawValue.uppercased()) · \(job.status.rawValue)")
                Text(job.message).font(.caption).foregroundStyle(.secondary)
              }
            }
          }
        }
        Text(buildStore.lastStatus).font(.caption).foregroundStyle(.secondary)
        if !buildStore.lastError.isEmpty {
          Text(buildStore.lastError).font(.caption).foregroundStyle(.red)
        }
      }
      .navigationTitle("Remote Build/Test")
      .toolbar {
        ToolbarItem(placement: .confirmationAction) {
          Button("Save") { settingsStore.saveAndReport() }
        }
      }
    }
  }

  private func activeHost() -> YazeSettings.AiHost? {
    settingsStore.settings.ai.hosts.first(where: { $0.id == settingsStore.settings.ai.remoteBuildHostId })
  }

  private func queueBuild() {
    guard let host = activeHost() else { return }
    buildStore.enqueueBuild(host: host, projectPath: settingsStore.settings.general.lastProjectPath)
  }

  private func queueTests() {
    guard let host = activeHost() else { return }
    buildStore.enqueueTests(host: host, projectPath: settingsStore.settings.general.lastProjectPath)
  }

  private func refreshJobs() {
    guard let host = activeHost() else { return }
    buildStore.refreshJobs(host: host)
  }
}

struct FilesystemSettingsView: View {
  @ObservedObject var settingsStore: YazeSettingsStore
  @Binding var showRomPicker: Bool
  @Binding var showProjectPicker: Bool
  @Binding var exportURL: URL?
  @Binding var showExportPicker: Bool
  @State private var showBundleImporter = false
  @State private var showFolderImporter = false

  var body: some View {
    NavigationStack {
      Form {
        Section("Roots") {
          ForEach(settingsStore.settings.filesystem.projectRootPaths, id: \.self) { path in
            Text(path)
          }
          .onDelete { indexSet in
            settingsStore.settings.filesystem.projectRootPaths.remove(atOffsets: indexSet)
          }
          Button("Add Project Root") { showFolderImporter = true }
        }

        Section("Defaults") {
          Picker("Default Root", selection: $settingsStore.settings.filesystem.defaultProjectRoot) {
            ForEach(settingsStore.settings.filesystem.projectRootPaths, id: \.self) { path in
              Text(path).tag(path)
            }
          }
          Toggle("Use Files App", isOn: $settingsStore.settings.filesystem.useFilesApp)
          Toggle("Use iCloud Sync", isOn: $settingsStore.settings.filesystem.useIcloudSync)
        }

        Section("Quick Actions") {
          Button("Open ROM") { showRomPicker = true }
          Button("Open Project") { showProjectPicker = true }
          Button("Export Project Bundle") { exportBundle() }
          Button("Import Project Bundle") { showBundleImporter = true }
        }
      }
      .navigationTitle("Files")
      .toolbar {
        ToolbarItem(placement: .confirmationAction) {
          Button("Save") { settingsStore.saveAndReport() }
        }
      }
      .fileImporter(isPresented: $showFolderImporter,
                    allowedContentTypes: [.folder],
                    allowsMultipleSelection: false) { result in
        switch result {
        case .success(let urls):
          guard let url = urls.first else { return }
          settingsStore.settings.filesystem.projectRootPaths.append(url.path)
          settingsStore.saveAndReport()
        case .failure:
          break
        }
      }
      .sheet(isPresented: $showBundleImporter) {
        DocumentPicker(contentTypes: [UTType(filenameExtension: "yazeproj") ?? .data]) { url in
          if let imported = YazeProjectBundleService.importBundle(from: url, settingsStore: settingsStore) {
            settingsStore.statusMessage = "Imported \(imported.lastPathComponent)"
            if !settingsStore.settings.general.lastProjectPath.isEmpty {
              YazeIOSBridge.openProject(atPath: settingsStore.settings.general.lastProjectPath)
            }
            if !settingsStore.settings.general.lastRomPath.isEmpty {
              YazeIOSBridge.loadRom(atPath: settingsStore.settings.general.lastRomPath)
            }
          } else {
            settingsStore.statusMessage = "Import failed"
          }
        }
      }
    }
  }

  private func exportBundle() {
    if let bundleURL = YazeProjectBundleService.exportBundle(settingsStore: settingsStore) {
      exportURL = bundleURL
      showExportPicker = true
    } else {
      settingsStore.statusMessage = "Export failed"
    }
  }
}
