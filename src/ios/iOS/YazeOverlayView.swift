import SwiftUI
import UniformTypeIdentifiers
import UIKit
import Combine

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
  case showProjectBrowser = "show_project_browser"
  case showOracleTools = "show_oracle_tools"
  case hideOverlay = "hide_overlay"
  case showOverlay = "show_overlay"
}

private extension Notification.Name {
  static let yazeOverlayCommand = Notification.Name("yaze.overlay.command")
  static let yazeEditorState = Notification.Name("yaze.state.editor")
}

private enum OverlayCommandPayload {
  static let key = "command"
}

private struct OverlayDungeonRoom: Identifiable, Equatable {
  let roomID: Int
  let name: String
  let isCurrent: Bool

  var id: Int { roomID }
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
  private enum OverlaySheetTarget {
    case settings
    case aiPanel
    case buildPanel
    case filesystemPanel
    case romPicker
    case projectPicker
    case exportPicker
    case mainMenu
    case projectBrowser
    case oracleTools
  }

  @ObservedObject var settingsStore: YazeSettingsStore
  @ObservedObject var buildStore: RemoteBuildStore

  @StateObject private var documentManager = YazeDocumentManager()
  @State private var showSettings = false
  @State private var showAiPanel = false
  @State private var showBuildPanel = false
  @State private var showFilesystemPanel = false
  @State private var showRomPicker = false
  @State private var showProjectPicker = false
  @State private var showExportPicker = false
  @State private var showProjectBrowser = false
  @State private var showOracleTools = false
  @State private var exportURL: URL?
  @State private var showMainMenu = false
  @State private var overlayHeight: CGFloat = 0
  @State private var lastPublishedOverlayInset: CGFloat = -1
  @State private var showDungeonSidebar = false
  @State private var dungeonRoomFilter = ""
  @State private var dungeonRooms: [OverlayDungeonRoom] = []
  @State private var selectedDungeonRoomID: Int?

  // Reactive editor state from C++ via NSNotification
  @State private var canUndo = false
  @State private var canRedo = false
  @State private var canSave = false
  @State private var isDirty = false
  @State private var currentEditorType: String = ""
  @State private var romTitle: String = ""
  private let roomRefreshTicker = Timer.publish(every: 1.0, on: .main, in: .common).autoconnect()

  var body: some View {
    GeometryReader { proxy in
      let topPadding = max(14, proxy.safeAreaInsets.top + 10)
      let bottomPadding = max(8, proxy.safeAreaInsets.bottom + 4)

      ZStack(alignment: .topLeading) {
        Color.clear.allowsHitTesting(false)
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
        .padding(.top, topPadding)
        .padding(.horizontal, 16)

        if isDungeonEditor && showDungeonSidebar {
          dungeonRoomSidebar(topPadding: topPadding)
            .transition(.move(edge: .leading).combined(with: .opacity))
        }

        if shouldShowSyncStatusStrip {
          VStack {
            Spacer()
            syncStatusStrip()
              .padding(.horizontal, 16)
              .padding(.bottom, bottomPadding)
          }
        }
      }
      .ignoresSafeArea(edges: .top)
    }
    .onPreferenceChange(OverlayTopInsetKey.self) { value in
      overlayHeight = max(0, value.rounded())
      publishOverlayTopInsetIfNeeded()
    }
    .onAppear {
      YazeIOSBridge.setTouchScale(settingsStore.settings.mobile.touchScale)
      documentManager.startMonitoring()
      refreshDungeonRoomsIfNeeded(force: true)
      publishOverlayTopInsetIfNeeded(force: true)
    }
    .onChange(of: settingsStore.settings.mobile.touchScale) { _, scale in
      YazeIOSBridge.setTouchScale(scale)
    }
    .onReceive(NotificationCenter.default.publisher(for: .yazeEditorState)) { notification in
      guard let info = notification.userInfo else { return }
      canUndo = (info["canUndo"] as? Bool) ?? false
      canRedo = (info["canRedo"] as? Bool) ?? false
      canSave = (info["canSave"] as? Bool) ?? false
      isDirty = (info["isDirty"] as? Bool) ?? false
      currentEditorType = (info["editorType"] as? String) ?? ""
      romTitle = (info["romTitle"] as? String) ?? ""
      refreshDungeonRoomsIfNeeded()
    }
    .onReceive(roomRefreshTicker) { _ in
      refreshDungeonRoomsIfNeeded()
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
                             openRomPicker: { presentSheet(.romPicker) },
                             openProjectPicker: { presentSheet(.projectPicker) },
                             presentExportPicker: { url in
                               exportURL = url
                               presentSheet(.exportPicker)
                             })
    }
    .sheet(isPresented: $showMainMenu) {
      OverlayMainMenuView(settingsStore: settingsStore,
                          buildStore: buildStore,
                          openRomPicker: openRomPicker,
                          openProjectPicker: openProjectPicker,
                          openProjectBrowser: openProjectBrowser,
                          openOracleTools: openOracleTools,
                          openAiPanel: openAiPanel,
                          openBuildPanel: openBuildPanel,
                          openFilesPanel: openFilesPanel,
                          openSettings: openSettings,
                          openPanelBrowser: { YazeIOSBridge.showPanelBrowser() },
                          openCommandPalette: { YazeIOSBridge.showCommandPalette() },
                          openProjectManager: { YazeIOSBridge.showProjectManagement() },
                          openProjectFile: { YazeIOSBridge.showProjectFileEditor() })
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
    .sheet(isPresented: $showProjectBrowser) {
      NavigationStack {
        ProjectBrowserView(documentManager: documentManager, settingsStore: settingsStore)
      }
      .presentationDetents([.medium, .large])
      .presentationDragIndicator(.visible)
    }
    .sheet(isPresented: $showOracleTools) {
      OracleToolsTab(settingsStore: settingsStore)
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }
  }

  private func topBar() -> some View {
    let controlSize: CGFloat = settingsStore.settings.mobile.largeTouchTargets ? 46 : 40

    return HStack(spacing: 8) {
      mainMenuButton(controlSize: controlSize)

      OverlayGlyphButton(systemImage: "sidebar.left", iconSize: 18, controlSize: controlSize) {
        triggerSelectionHaptic()
        withAnimation(.easeInOut(duration: 0.2)) {
          showDungeonSidebar.toggle()
        }
      }
      .opacity(isDungeonEditor ? 1.0 : 0.35)
      .disabled(!isDungeonEditor)
      .accessibilityLabel(showDungeonSidebar ? "Hide dungeon room sidebar" :
                          "Show dungeon room sidebar")

      // Center: Editor picker
      editorPicker(controlSize: controlSize)

      Spacer()

      OverlayGlyphButton(systemImage: "rectangle.stack", iconSize: 16, controlSize: controlSize) {
        triggerSelectionHaptic()
        YazeIOSBridge.showPanelBrowser()
      }
      .accessibilityLabel("Open panel browser")

      OverlayGlyphButton(systemImage: "command", iconSize: 16, controlSize: controlSize) {
        triggerSelectionHaptic()
        YazeIOSBridge.showCommandPalette()
      }
      .accessibilityLabel("Open command palette")

      // Trailing: Undo / Redo / Save
      OverlayGlyphButton(systemImage: "arrow.uturn.backward",
                         iconSize: 18, controlSize: controlSize) {
        triggerSelectionHaptic()
        YazeIOSBridge.undo()
      }
      .opacity(canUndo ? 1.0 : 0.35)
      .disabled(!canUndo)
      .accessibilityLabel("Undo")

      OverlayGlyphButton(systemImage: "arrow.uturn.forward",
                         iconSize: 18, controlSize: controlSize) {
        triggerSelectionHaptic()
        YazeIOSBridge.redo()
      }
      .opacity(canRedo ? 1.0 : 0.35)
      .disabled(!canRedo)
      .accessibilityLabel("Redo")

      OverlayGlyphButton(
        systemImage: isDirty ? "square.and.arrow.down.fill" : "square.and.arrow.down",
        iconSize: 18, controlSize: controlSize
      ) {
        triggerSelectionHaptic()
        YazeIOSBridge.saveRom()
      }
      .opacity(canSave ? 1.0 : 0.35)
      .disabled(!canSave)
      .accessibilityLabel("Save ROM")
    }
    .padding(.vertical, 2)
    .padding(.horizontal, 8)
    .background(.thinMaterial)
    .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
    .overlay(
      RoundedRectangle(cornerRadius: 12, style: .continuous)
        .stroke(Color.white.opacity(0.15))
    )
  }

  private var isDungeonEditor: Bool {
    currentEditorType == "Dungeon"
  }

  private var shouldShowSyncStatusStrip: Bool {
    settingsStore.settings.mobile.showStatusPills
  }

  private var filteredDungeonRooms: [OverlayDungeonRoom] {
    let query = dungeonRoomFilter.trimmingCharacters(in: .whitespacesAndNewlines)
    guard !query.isEmpty else { return dungeonRooms }

    let loweredQuery = query.lowercased()
    return dungeonRooms.filter { room in
      if room.name.lowercased().contains(loweredQuery) {
        return true
      }
      let hexID = String(format: "%03X", room.roomID).lowercased()
      return hexID.contains(loweredQuery)
    }
  }

  private func mainMenuButton(controlSize: CGFloat) -> some View {
    Menu {
      Section("Open") {
        Button {
          openRomPicker()
        } label: {
          Label("Open ROM…", systemImage: "folder")
        }
        Button {
          openProjectPicker()
        } label: {
          Label("Open Project…", systemImage: "folder.badge.person.crop")
        }
        Button {
          openProjectBrowser()
        } label: {
          Label("Projects", systemImage: "folder.badge.gearshape")
        }
      }

      Section("Workspace") {
        if isDungeonEditor {
          Button(showDungeonSidebar ? "Hide Room Sidebar" : "Show Room Sidebar") {
            triggerSelectionHaptic()
            withAnimation(.easeInOut(duration: 0.2)) {
              showDungeonSidebar.toggle()
            }
          }
        }
        Button {
          triggerSelectionHaptic()
          YazeIOSBridge.showPanelBrowser()
        } label: {
          Label("Panel Browser", systemImage: "rectangle.stack")
        }
        Button {
          triggerSelectionHaptic()
          YazeIOSBridge.showCommandPalette()
        } label: {
          Label("Command Palette", systemImage: "command")
        }
      }

      Section("Tools") {
        Button {
          openOracleTools()
        } label: {
          Label("Oracle Tools", systemImage: "wand.and.stars")
        }
        Button {
          openAiPanel()
        } label: {
          Label("AI Hosts", systemImage: "sparkles")
        }
        Button {
          openBuildPanel()
        } label: {
          Label("Remote Build", systemImage: "hammer")
        }
        Button {
          openFilesPanel()
        } label: {
          Label("Files", systemImage: "doc.on.doc")
        }
      }

      Section("Project") {
        Button {
          triggerSelectionHaptic()
          YazeIOSBridge.showProjectManagement()
        } label: {
          Label("Project Manager", systemImage: "slider.horizontal.3")
        }
        Button {
          triggerSelectionHaptic()
          YazeIOSBridge.showProjectFileEditor()
        } label: {
          Label("Project File", systemImage: "doc.text")
        }
      }

      Section {
        Button {
          openSettings()
        } label: {
          Label("Settings", systemImage: "gearshape")
        }
      }
    } label: {
      Image(systemName: "line.3.horizontal")
        .font(.system(size: 20, weight: .semibold))
        .frame(width: controlSize, height: controlSize)
    }
    .menuOrder(.fixed)
    .accessibilityLabel("Open main menu")
  }

  private func dungeonRoomSidebar(topPadding: CGFloat) -> some View {
    VStack(alignment: .leading, spacing: 10) {
      HStack {
        Label("Dungeon Rooms", systemImage: "building.columns")
          .font(.subheadline.weight(.semibold))
        Spacer()
        Text("\(filteredDungeonRooms.count)")
          .font(.caption.monospacedDigit())
          .foregroundStyle(.secondary)
      }

      TextField("Filter rooms (ID or name)", text: $dungeonRoomFilter)
        .textFieldStyle(.roundedBorder)

      ScrollView {
        LazyVStack(spacing: 6) {
          ForEach(filteredDungeonRooms) { room in
            let isSelected = room.roomID == selectedDungeonRoomID || room.isCurrent
            Button {
              focusDungeonRoom(room.roomID)
            } label: {
              HStack(spacing: 8) {
                Text(String(format: "%03X", room.roomID))
                  .font(.caption.monospacedDigit())
                  .foregroundStyle(.secondary)
                Text(room.name)
                  .font(.subheadline)
                  .lineLimit(1)
                Spacer()
                if isSelected {
                  Image(systemName: "checkmark.circle.fill")
                    .foregroundStyle(.green)
                }
              }
              .padding(.vertical, 6)
              .padding(.horizontal, 8)
              .frame(maxWidth: .infinity, alignment: .leading)
              .background(
                RoundedRectangle(cornerRadius: 8)
                  .fill(isSelected ? Color.accentColor.opacity(0.18) : Color.clear)
              )
            }
            .buttonStyle(.plain)
          }
        }
        .frame(maxWidth: .infinity)
      }
      .frame(maxHeight: 260)

      HStack(spacing: 8) {
        Button {
          triggerSelectionHaptic()
          YazeIOSBridge.showPanelBrowser()
        } label: {
          Label("Panels", systemImage: "rectangle.grid.2x2")
        }
        .buttonStyle(.bordered)

        Button {
          openOracleTools()
        } label: {
          Label("Oracle", systemImage: "wand.and.stars")
        }
        .buttonStyle(.bordered)
      }
    }
    .padding(12)
    .frame(width: 280)
    .background(.ultraThinMaterial)
    .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
    .overlay(
      RoundedRectangle(cornerRadius: 14, style: .continuous)
        .stroke(Color.white.opacity(0.18))
    )
    .padding(.top, topPadding + 52)
    .padding(.leading, 16)
  }

  private func syncStatusStrip() -> some View {
    let projectPath = settingsStore.settings.general.lastProjectPath
    let projectLabel = projectPath.isEmpty ? "No project" :
      URL(fileURLWithPath: projectPath).lastPathComponent
    let syncEnabled = settingsStore.settings.filesystem.useIcloudSync
    let cloudLabel: String = {
      if !syncEnabled { return "Sync Off" }
      return documentManager.isICloudAvailable ? "iCloud Sync" : "iCloud Unavailable"
    }()
    let statusValue = settingsStore.statusMessage.isEmpty ? "Ready" : settingsStore.statusMessage
    let roomLabel = YazeIOSBridge.currentRoomStatus() ?? "No room"

    return HStack(spacing: 10) {
      Label(cloudLabel, systemImage: syncEnabled ? "icloud" : "icloud.slash")
        .font(.caption)
      Divider()
      Label(projectLabel, systemImage: "folder")
        .font(.caption)
      Divider()
      Label(roomLabel, systemImage: "square.grid.3x3")
        .font(.caption)
      Divider()
      Text(statusValue)
        .font(.caption)
        .lineLimit(1)
      Spacer(minLength: 0)
      if isDirty {
        Label("Unsaved", systemImage: "circle.fill")
          .font(.caption.weight(.semibold))
          .foregroundStyle(.orange)
      }
    }
    .padding(.vertical, 4)
    .padding(.horizontal, 8)
    .background(.thinMaterial)
    .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
    .overlay(
      RoundedRectangle(cornerRadius: 10, style: .continuous)
        .stroke(Color.white.opacity(0.15))
    )
  }

  private func editorPicker(controlSize: CGFloat) -> some View {
    let editors = YazeIOSBridge.availableEditorTypes()
    let label = currentEditorType.isEmpty
      ? (romTitle.isEmpty ? "Yaze" : romTitle)
      : currentEditorType

    return Menu {
      ForEach(editors, id: \.self) { editor in
        Button {
          triggerSelectionHaptic()
          YazeIOSBridge.switchToEditor(editor)
        } label: {
          Label(editor, systemImage: editorIcon(for: editor))
        }
        .disabled(editor == currentEditorType)
      }
      Divider()
      Button {
        triggerSelectionHaptic()
        presentSheet(.romPicker)
      } label: {
        Label("Open ROM...", systemImage: "folder")
      }
    } label: {
      HStack(spacing: 4) {
        Text(label)
          .font(.subheadline.weight(.semibold))
          .lineLimit(1)
        Image(systemName: "chevron.up.chevron.down")
          .font(.system(size: 10, weight: .bold))
      }
      .frame(height: controlSize)
      .padding(.horizontal, 8)
    }
  }

  private func editorIcon(for name: String) -> String {
    switch name {
    case "Overworld": return "map"
    case "Dungeon": return "building.columns"
    case "Graphics": return "paintbrush"
    case "Palette": return "paintpalette"
    case "Music": return "music.note"
    case "Sprite": return "figure.walk"
    case "Screen": return "display"
    case "Message": return "text.bubble"
    case "Assembly": return "chevron.left.forwardslash.chevron.right"
    case "Emulator": return "gamecontroller"
    default: return "questionmark"
    }
  }

  private func openSettings() {
    triggerSelectionHaptic()
    presentSheet(.settings)
  }

  private func openAiPanel() {
    triggerSelectionHaptic()
    presentSheet(.aiPanel)
  }

  private func openBuildPanel() {
    triggerSelectionHaptic()
    presentSheet(.buildPanel)
  }

  private func openFilesPanel() {
    triggerSelectionHaptic()
    presentSheet(.filesystemPanel)
  }

  private func openRomPicker() {
    triggerSelectionHaptic()
    presentSheet(.romPicker)
  }

  private func openProjectPicker() {
    triggerSelectionHaptic()
    presentSheet(.projectPicker)
  }

  private func openProjectBrowser() {
    triggerSelectionHaptic()
    presentSheet(.projectBrowser)
  }

  private func openOracleTools() {
    triggerSelectionHaptic()
    presentSheet(.oracleTools)
  }

  private func focusDungeonRoom(_ roomID: Int) {
    guard roomID >= 0 else { return }
    triggerSelectionHaptic()
    selectedDungeonRoomID = roomID
    YazeIOSBridge.focusDungeonRoom(roomID)
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
      refreshDungeonRoomsIfNeeded(force: true)
    }
  }

  private func refreshDungeonRoomsIfNeeded(force: Bool = false) {
    guard isDungeonEditor else {
      if force || !dungeonRooms.isEmpty {
        dungeonRooms = []
        selectedDungeonRoomID = nil
      }
      return
    }

    let rawRooms = YazeIOSBridge.getActiveDungeonRooms()
    var parsedRooms: [OverlayDungeonRoom] = []
    parsedRooms.reserveCapacity(rawRooms.count)

    var currentRoomID: Int?

    for dict in rawRooms {
      let numericID = (dict["room_id"] as? NSNumber)?.intValue ?? (dict["room_id"] as? Int)
      guard let roomID = numericID else { continue }
      let name = (dict["name"] as? String) ?? String(format: "Room 0x%03X", roomID)
      let isCurrent = (dict["is_current"] as? Bool) ??
        ((dict["is_current"] as? NSNumber)?.boolValue ?? false)
      if isCurrent {
        currentRoomID = roomID
      }
      parsedRooms.append(
        OverlayDungeonRoom(roomID: roomID, name: name, isCurrent: isCurrent)
      )
    }

    parsedRooms.sort { lhs, rhs in
      lhs.roomID < rhs.roomID
    }
    dungeonRooms = parsedRooms

    if let currentRoomID {
      selectedDungeonRoomID = currentRoomID
    } else if let selected = selectedDungeonRoomID,
              !parsedRooms.contains(where: { $0.roomID == selected }) {
      selectedDungeonRoomID = parsedRooms.first?.roomID
    }
  }

  private func triggerSelectionHaptic() {
    UISelectionFeedbackGenerator().selectionChanged()
  }

  private func dismissAllSheets() {
    showSettings = false
    showAiPanel = false
    showBuildPanel = false
    showFilesystemPanel = false
    showRomPicker = false
    showProjectPicker = false
    showExportPicker = false
    showMainMenu = false
    showProjectBrowser = false
    showOracleTools = false
  }

  private func presentSheet(_ target: OverlaySheetTarget) {
    dismissAllSheets()
    DispatchQueue.main.async {
      switch target {
      case .settings:
        showSettings = true
      case .aiPanel:
        showAiPanel = true
      case .buildPanel:
        showBuildPanel = true
      case .filesystemPanel:
        showFilesystemPanel = true
      case .romPicker:
        showRomPicker = true
      case .projectPicker:
        showProjectPicker = true
      case .exportPicker:
        showExportPicker = true
      case .mainMenu:
        showMainMenu = true
      case .projectBrowser:
        showProjectBrowser = true
      case .oracleTools:
        showOracleTools = true
      }
    }
  }

  private func publishOverlayTopInsetIfNeeded(force: Bool = false) {
    let inset = overlayHeight
    if force || abs(inset - lastPublishedOverlayInset) >= 0.5 {
      lastPublishedOverlayInset = inset
      YazeIOSBridge.setOverlayTopInset(Double(inset))
    }
  }

  private func handleOverlayCommand(_ command: OverlayCommand) {
    switch command {
    case .showMenu:
      presentSheet(.mainMenu)
    case .openRom:
      presentSheet(.romPicker)
    case .openProject:
      presentSheet(.projectPicker)
    case .openAi:
      presentSheet(.aiPanel)
    case .openBuild:
      presentSheet(.buildPanel)
    case .openFiles:
      presentSheet(.filesystemPanel)
    case .openSettings:
      presentSheet(.settings)
    case .showPanelBrowser:
      triggerSelectionHaptic()
      YazeIOSBridge.showPanelBrowser()
    case .showCommandPalette:
      triggerSelectionHaptic()
      YazeIOSBridge.showCommandPalette()
    case .showProjectManager:
      triggerSelectionHaptic()
      YazeIOSBridge.showProjectManagement()
    case .showProjectFile:
      triggerSelectionHaptic()
      YazeIOSBridge.showProjectFileEditor()
    case .showProjectBrowser:
      presentSheet(.projectBrowser)
    case .showOracleTools:
      presentSheet(.oracleTools)
    case .hideOverlay:
      break  // Toolbar is always visible now
    case .showOverlay:
      break  // Toolbar is always visible now
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
  let openProjectBrowser: () -> Void
  let openOracleTools: () -> Void
  let openAiPanel: () -> Void
  let openBuildPanel: () -> Void
  let openFilesPanel: () -> Void
  let openSettings: () -> Void
  let openPanelBrowser: () -> Void
  let openCommandPalette: () -> Void
  let openProjectManager: () -> Void
  let openProjectFile: () -> Void

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
      List {
        Section {
          VStack(alignment: .leading, spacing: 4) {
            Text("Yaze")
              .font(.headline)
            Text("Quick access to project, tools, and settings")
              .font(.caption)
              .foregroundStyle(.secondary)
          }
          .padding(.vertical, 2)
        }

        Section("Quick Actions") {
          OverlayMenuRow(title: "Open ROM", systemImage: "folder") {
            runAndDismiss(openRomPicker)
          }
          OverlayMenuRow(title: "Open Project", systemImage: "folder.badge.person.crop") {
            runAndDismiss(openProjectPicker)
          }
          OverlayMenuRow(title: "Projects", systemImage: "folder.badge.gearshape") {
            runAndDismiss(openProjectBrowser)
          }
          OverlayMenuRow(title: "Oracle Tools", systemImage: "wand.and.stars") {
            runAndDismiss(openOracleTools)
          }
        }

        Section("Tools") {
          OverlayMenuRow(title: "Panel Browser", systemImage: "rectangle.stack") {
            runAndDismiss(openPanelBrowser)
          }
          OverlayMenuRow(title: "Command Palette", systemImage: "command") {
            runAndDismiss(openCommandPalette)
          }
          OverlayMenuRow(title: "AI Hosts", systemImage: "sparkles") {
            runAndDismiss(openAiPanel)
          }
          OverlayMenuRow(title: "Remote Build", systemImage: "hammer") {
            runAndDismiss(openBuildPanel)
          }
          OverlayMenuRow(title: "Files", systemImage: "doc.on.doc") {
            runAndDismiss(openFilesPanel)
          }
          OverlayMenuRow(title: "Settings", systemImage: "gearshape") {
            runAndDismiss(openSettings)
          }
        }

        Section("Project") {
          OverlayMenuRow(title: "Project Manager", systemImage: "tray.full",
                         isEnabled: hasProject) {
            runAndDismiss(openProjectManager)
          }
          OverlayMenuRow(title: "Project File", systemImage: "doc.text",
                         isEnabled: hasProject) {
            runAndDismiss(openProjectFile)
          }
        }

        Section("Status") {
          LabeledContent("ROM", value: romLabel)
          LabeledContent("Project", value: projectLabel)
          LabeledContent("State", value: statusValue)
            .foregroundStyle(statusValue == "Ready" ? .secondary : .primary)
        }
      }
      .listStyle(.insetGrouped)
      .navigationTitle("Menu")
      .toolbar {
        ToolbarItem(placement: .confirmationAction) {
          Button("Done") { dismiss() }
        }
      }
    }
  }

  private func runAndDismiss(_ action: () -> Void) {
    action()
    dismiss()
  }
}

private struct OverlayMenuRow: View {
  let title: String
  let systemImage: String
  var isEnabled: Bool = true
  let action: () -> Void

  var body: some View {
    Button(action: action) {
      HStack(spacing: 12) {
        Image(systemName: systemImage)
          .font(.system(size: 17, weight: .semibold))
          .frame(width: 28, height: 28)
          .foregroundColor(isEnabled ? .accentColor : .secondary)

        Text(title)
          .font(.body.weight(.medium))

        Spacer()

        Image(systemName: "chevron.right")
          .font(.caption2.weight(.semibold))
          .foregroundStyle(.tertiary)
      }
      .contentShape(Rectangle())
      .padding(.vertical, 4)
    }
    .buttonStyle(.plain)
    .disabled(!isEnabled)
    .opacity(isEnabled ? 1.0 : 0.45)
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
          apiKey = value
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
  let openRomPicker: () -> Void
  let openProjectPicker: () -> Void
  let presentExportPicker: (URL) -> Void
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
          Button("Open ROM") { openRomPicker() }
          Button("Open Project") { openProjectPicker() }
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
      presentExportPicker(bundleURL)
    } else {
      settingsStore.statusMessage = "Export failed"
    }
  }
}
