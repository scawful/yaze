import SwiftUI
import UniformTypeIdentifiers

private enum OverlayCommand: String {
  case showMenu = "show_menu"
  case openRom = "open_rom"
  case openProject = "open_project"
  case openAi = "open_ai"
  case openBuild = "open_build"
  case openFiles = "open_files"
  case openSettings = "open_settings"
  case showPanelBrowser = "show_panel_browser"
  case showCommandPalette = "show_command_palette"
  case showProjectManager = "show_project_manager"
  case showProjectFile = "show_project_file"
  case hideOverlay = "hide_overlay"
  case showOverlay = "show_overlay"
}

private extension Notification.Name {
  static let yazeOverlayCommand = Notification.Name("yaze.overlay.command")
}

private enum OverlayCommandPayload {
  static let key = "command"
}

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
  @State private var showMainMenu = false
  @State private var overlayCollapsed = false
  @State private var overlayHeight: CGFloat = 0

  var body: some View {
    GeometryReader { proxy in
      ZStack(alignment: .top) {
        Color.clear.allowsHitTesting(false)
        let expandedTopPadding = max(8, proxy.safeAreaInsets.top + 6)
        let collapsedTopPadding = max(18, proxy.safeAreaInsets.top + 12)
        if overlayCollapsed {
          collapsedHandle
            .padding(.top, collapsedTopPadding)
            .padding(.horizontal, 16)
        } else {
          VStack(spacing: 12) {
            topBar()
              .background(
                GeometryReader { barProxy in
                  Color.clear.preference(
                    key: OverlayTopInsetKey.self,
                    value: barProxy.frame(in: .global).maxY
                  )
                }
              )
            Spacer()
          }
          .padding(.top, expandedTopPadding)
          .padding(.horizontal, 16)
        }
      }
      .ignoresSafeArea(edges: .top)
    }
    .onPreferenceChange(OverlayTopInsetKey.self) { value in
      overlayHeight = value
      let inset = overlayCollapsed ? 0 : value
      YazeIOSBridge.setOverlayTopInset(Double(inset))
    }
    .onAppear {
      YazeIOSBridge.setTouchScale(settingsStore.settings.mobile.touchScale)
    }
    .onChange(of: overlayCollapsed) { _, collapsed in
      let inset = collapsed ? 0 : overlayHeight
      YazeIOSBridge.setOverlayTopInset(Double(inset))
    }
    .onChange(of: settingsStore.settings.mobile.touchScale) { _, scale in
      YazeIOSBridge.setTouchScale(scale)
    }
    .onReceive(NotificationCenter.default.publisher(for: .yazeOverlayCommand)) { notification in
      guard let payload = notification.userInfo?[OverlayCommandPayload.key] as? String,
            let command = OverlayCommand(rawValue: payload) else {
        return
      }
      handleOverlayCommand(command)
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
    .sheet(isPresented: $showMainMenu) {
      OverlayMainMenuView(settingsStore: settingsStore,
                          buildStore: buildStore,
                          openRomPicker: openRomPicker,
                          openProjectPicker: openProjectPicker,
                          openAiPanel: openAiPanel,
                          openBuildPanel: openBuildPanel,
                          openFilesPanel: openFilesPanel,
                          openSettings: openSettings,
                          openPanelBrowser: { YazeIOSBridge.showPanelBrowser() },
                          openCommandPalette: { YazeIOSBridge.showCommandPalette() },
                          openProjectManager: { YazeIOSBridge.showProjectManagement() },
                          openProjectFile: { YazeIOSBridge.showProjectFileEditor() },
                          hideOverlay: { overlayCollapsed = true })
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }
    .sheet(isPresented: $showRomPicker) {
      DocumentPicker(contentTypes: [UTType(filenameExtension: "sfc") ?? .data,
                                   UTType(filenameExtension: "smc") ?? .data]) { url in
        if let imported = YazeFileImportService.importRom(from: url) {
          settingsStore.updateCurrentRomPath(imported.path)
          YazeIOSBridge.loadRom(atPath: imported.path)
        } else {
          settingsStore.statusMessage = "Failed to import ROM"
        }
      }
    }
    .sheet(isPresented: $showProjectPicker) {
      DocumentPicker(
        // Files app sometimes exposes package directories as folders in the picker.
        // Accept `.folder` as a fallback and validate by contents in the open service.
        contentTypes: [.yazeProject, .folder],
        asCopy: false
      ) { url in
        do {
          try YazeProjectOpenService.openBundle(at: url, settingsStore: settingsStore)
        } catch {
          settingsStore.statusMessage = "Open failed: \(error.localizedDescription)"
        }
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

  private func topBar() -> some View {
    let useLargeControls = settingsStore.settings.mobile.largeTouchTargets
    let controlSize: CGFloat = useLargeControls ? 36 : 30
    let iconSize: CGFloat = useLargeControls ? 20 : 18

    return HStack {
      Spacer()
      HStack(spacing: 10) {
        OverlayGlyphButton(systemImage: "line.3.horizontal",
                           iconSize: iconSize,
                           controlSize: controlSize) {
          showMainMenu = true
        }
        OverlayGlyphButton(systemImage: "folder",
                           iconSize: iconSize,
                           controlSize: controlSize) {
          showRomPicker = true
        }
        OverlayGlyphButton(systemImage: "rectangle.stack",
                           iconSize: iconSize,
                           controlSize: controlSize) {
          YazeIOSBridge.showPanelBrowser()
        }
      }
      .padding(.vertical, 6)
      .padding(.horizontal, 10)
      .background(.ultraThinMaterial)
      .clipShape(Capsule())
      .overlay(Capsule().stroke(Color.white.opacity(0.2)))
      Spacer()
    }
  }

  private var collapsedHandle: some View {
    HStack {
      Spacer()
      Button {
        overlayCollapsed = false
      } label: {
        Image(systemName: "line.3.horizontal")
          .font(.headline)
          .padding(.vertical, 6)
          .padding(.horizontal, 12)
      }
      .buttonStyle(.borderedProminent)
      .shadow(radius: 6)
      Spacer()
    }
  }

  private func openSettings() {
    showMainMenu = false
    showSettings = true
  }

  private func openAiPanel() {
    showMainMenu = false
    showAiPanel = true
  }

  private func openBuildPanel() {
    showMainMenu = false
    showBuildPanel = true
  }

  private func openFilesPanel() {
    showMainMenu = false
    showFilesystemPanel = true
  }

  private func openRomPicker() {
    showMainMenu = false
    showRomPicker = true
  }

  private func openProjectPicker() {
    showMainMenu = false
    showProjectPicker = true
  }

  private func handleOverlayCommand(_ command: OverlayCommand) {
    switch command {
    case .showMenu:
      showMainMenu = true
    case .openRom:
      openRomPicker()
    case .openProject:
      openProjectPicker()
    case .openAi:
      openAiPanel()
    case .openBuild:
      openBuildPanel()
    case .openFiles:
      openFilesPanel()
    case .openSettings:
      openSettings()
    case .showPanelBrowser:
      YazeIOSBridge.showPanelBrowser()
    case .showCommandPalette:
      YazeIOSBridge.showCommandPalette()
    case .showProjectManager:
      YazeIOSBridge.showProjectManagement()
    case .showProjectFile:
      YazeIOSBridge.showProjectFileEditor()
    case .hideOverlay:
      overlayCollapsed = true
    case .showOverlay:
      overlayCollapsed = false
    }
  }
}

private struct OverlayGlyphButton: View {
  let systemImage: String
  let iconSize: CGFloat
  let controlSize: CGFloat
  let action: () -> Void

  var body: some View {
    Button(action: action) {
      Image(systemName: systemImage)
        .font(.system(size: iconSize, weight: .semibold))
        .frame(width: controlSize, height: controlSize)
    }
    .buttonStyle(.plain)
  }
}

private struct OverlayTopInsetKey: PreferenceKey {
  static var defaultValue: CGFloat = 0
  static func reduce(value: inout CGFloat, nextValue: () -> CGFloat) {
    value = max(value, nextValue())
  }
}

private struct OverlayMainMenuView: View {
  @Environment(\.dismiss) private var dismiss
  @ObservedObject var settingsStore: YazeSettingsStore
  @ObservedObject var buildStore: RemoteBuildStore

  let openRomPicker: () -> Void
  let openProjectPicker: () -> Void
  let openAiPanel: () -> Void
  let openBuildPanel: () -> Void
  let openFilesPanel: () -> Void
  let openSettings: () -> Void
  let openPanelBrowser: () -> Void
  let openCommandPalette: () -> Void
  let openProjectManager: () -> Void
  let openProjectFile: () -> Void
  let hideOverlay: () -> Void

  var body: some View {
    let romLabel = settingsStore.settings.general.lastRomPath.isEmpty
      ? "No ROM loaded"
      : URL(fileURLWithPath: settingsStore.settings.general.lastRomPath).lastPathComponent
    let projectLabel = settingsStore.settings.general.lastProjectPath.isEmpty
      ? "No project"
      : URL(fileURLWithPath: settingsStore.settings.general.lastProjectPath).lastPathComponent
    let statusValue = settingsStore.statusMessage.isEmpty
      ? "Ready"
      : settingsStore.statusMessage
    let hasProject = !settingsStore.settings.general.lastProjectPath.isEmpty

    NavigationStack {
      GeometryReader { proxy in
        let columns = proxy.size.width > 700
          ? [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())]
          : [GridItem(.flexible()), GridItem(.flexible())]
        ScrollView {
          VStack(alignment: .leading, spacing: 18) {
            LazyVGrid(columns: columns, spacing: 12) {
              OverlayMenuTile(title: "Open ROM", systemImage: "folder") {
                openRomPicker(); dismiss()
              }
              OverlayMenuTile(title: "Open Project", systemImage: "folder.badge.person.crop") {
                openProjectPicker(); dismiss()
              }
              OverlayMenuTile(title: "Panel Browser", systemImage: "rectangle.stack") {
                openPanelBrowser(); dismiss()
              }
              OverlayMenuTile(title: "Command Palette", systemImage: "command") {
                openCommandPalette(); dismiss()
              }
              OverlayMenuTile(title: "AI Hosts", systemImage: "sparkles") {
                openAiPanel(); dismiss()
              }
              OverlayMenuTile(title: "Remote Build", systemImage: "hammer") {
                openBuildPanel(); dismiss()
              }
              OverlayMenuTile(title: "Files", systemImage: "doc.on.doc") {
                openFilesPanel(); dismiss()
              }
              OverlayMenuTile(title: "Settings", systemImage: "gearshape") {
                openSettings(); dismiss()
              }
            }

            VStack(alignment: .leading, spacing: 10) {
              Text("Project")
                .font(.headline)
              HStack(spacing: 12) {
                OverlayMenuTile(title: "Project Manager", systemImage: "tray.full",
                                isEnabled: hasProject) {
                  openProjectManager(); dismiss()
                }
                OverlayMenuTile(title: "Project File", systemImage: "doc.text",
                                isEnabled: hasProject) {
                  openProjectFile(); dismiss()
                }
              }
            }

            VStack(alignment: .leading, spacing: 8) {
              Text("Status")
                .font(.headline)
              OverlayStatusRow(label: "ROM", value: romLabel)
              OverlayStatusRow(label: "Project", value: projectLabel)
              OverlayStatusRow(label: "State", value: statusValue)
            }

            OverlayMenuTile(title: "Hide Top Bar", systemImage: "chevron.up") {
              hideOverlay(); dismiss()
            }
          }
          .padding(.horizontal, 18)
          .padding(.vertical, 16)
        }
      }
      .navigationTitle("Yaze Menu")
      .toolbar {
        ToolbarItem(placement: .confirmationAction) {
          Button("Done") { dismiss() }
        }
      }
    }
  }
}

private struct OverlayMenuTile: View {
  let title: String
  let systemImage: String
  var isEnabled: Bool = true
  let action: () -> Void

  var body: some View {
    Button(action: action) {
      HStack(spacing: 12) {
        Image(systemName: systemImage)
          .font(.system(size: 20, weight: .semibold))
          .frame(width: 32, height: 32)
        Text(title)
          .font(.body.weight(.semibold))
        Spacer()
      }
      .padding(.vertical, 12)
      .padding(.horizontal, 12)
      .frame(maxWidth: .infinity)
      .background(.ultraThinMaterial)
      .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
      .overlay(RoundedRectangle(cornerRadius: 14).stroke(Color.white.opacity(0.18)))
    }
    .buttonStyle(.plain)
    .disabled(!isEnabled)
    .opacity(isEnabled ? 1.0 : 0.4)
  }
}

private struct OverlayStatusRow: View {
  let label: String
  let value: String

  var body: some View {
    HStack {
      Text(label)
        .font(.caption)
        .foregroundStyle(.secondary)
      Spacer()
      Text(value)
        .font(.caption)
        .lineLimit(1)
    }
    .padding(.vertical, 6)
    .padding(.horizontal, 10)
    .background(Color.black.opacity(0.15))
    .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
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
        Section("Mobile UI") {
          Toggle("Large touch targets", isOn: $settingsStore.settings.mobile.largeTouchTargets)
          Slider(value: $settingsStore.settings.mobile.touchScale, in: 0.85...1.4, step: 0.05) {
            Text("Touch scale")
          }
          Text("Touch scale: \(settingsStore.settings.mobile.touchScale, specifier: "%.2f")")
            .font(.caption)
            .foregroundStyle(.secondary)
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
          Button("Open Project Bundle (Files/iCloud)") { showBundleImporter = true }
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
        DocumentPicker(
          // Files app sometimes exposes package directories as folders in the picker.
          // Accept `.folder` as a fallback and validate by contents in the open service.
          contentTypes: [.yazeProject, .folder],
          asCopy: false
        ) { url in
          do {
            try YazeProjectOpenService.openBundle(at: url, settingsStore: settingsStore)
          } catch {
            settingsStore.statusMessage = "Open failed: \(error.localizedDescription)"
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
