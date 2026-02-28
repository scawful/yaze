import SwiftUI

/// Sends z3ed CLI commands to the desktop and displays JSON results.
struct RemoteCommandRunnerView: View {
  @ObservedObject var apiClient: DesktopAPIClient

  @State private var commandText = ""
  @State private var commandHistory: [CommandHistoryEntry] = []
  @State private var availableCommands: [CommandInfo] = []
  @State private var isExecuting = false
  @State private var showCatalog = false
  @State private var catalogSearch = ""
  @State private var showWriteConfirmation = false
  @State private var pendingWriteCommand: (name: String, args: [String])?

  private var filteredCommands: [CommandInfo] {
    guard !catalogSearch.isEmpty else { return availableCommands }
    let query = catalogSearch.lowercased()
    return availableCommands.filter {
      $0.name.lowercased().contains(query) ||
      $0.category.lowercased().contains(query) ||
      $0.description.lowercased().contains(query)
    }
  }

  private var groupedCommands: [(String, [CommandInfo])] {
    let grouped = Dictionary(grouping: filteredCommands, by: \.category)
    return grouped.sorted { $0.key < $1.key }
  }

  /// Autocomplete suggestions based on current input.
  private var suggestions: [String] {
    let input = commandText.trimmingCharacters(in: .whitespaces).lowercased()
    guard !input.isEmpty else { return [] }
    return availableCommands
      .map(\.name)
      .filter { $0.lowercased().hasPrefix(input) }
      .prefix(5)
      .map { $0 }
  }

  var body: some View {
    VStack(spacing: 0) {
      // History
      ScrollViewReader { proxy in
        ScrollView {
          LazyVStack(alignment: .leading, spacing: 12) {
            ForEach(commandHistory) { entry in
              commandEntryView(entry)
                .id(entry.id)
            }
          }
          .padding()
        }
        .onChange(of: commandHistory.count) { _, _ in
          if let last = commandHistory.last {
            withAnimation {
              proxy.scrollTo(last.id, anchor: .bottom)
            }
          }
        }
      }

      Divider()

      // Suggestions
      if !suggestions.isEmpty {
        ScrollView(.horizontal, showsIndicators: false) {
          HStack(spacing: 6) {
            ForEach(suggestions, id: \.self) { suggestion in
              Button(suggestion) {
                commandText = suggestion + " "
              }
              .font(.caption.monospaced())
              .padding(.horizontal, 8)
              .padding(.vertical, 4)
              .background(Color.accentColor.opacity(0.1))
              .clipShape(Capsule())
            }
          }
          .padding(.horizontal)
          .padding(.vertical, 4)
        }
        .background(.bar)
      }

      // Input bar
      HStack(spacing: 8) {
        TextField("Enter command...", text: $commandText)
          .textFieldStyle(.roundedBorder)
          .font(.body.monospaced())
          .autocorrectionDisabled()
          .textInputAutocapitalization(.never)
          .onSubmit {
            executeCurrentCommand()
          }

        Button {
          executeCurrentCommand()
        } label: {
          Image(systemName: "play.fill")
            .frame(width: 36, height: 36)
        }
        .disabled(commandText.trimmingCharacters(in: .whitespaces).isEmpty || isExecuting)
        .buttonStyle(.borderedProminent)
      }
      .padding()
      .background(.bar)
    }
    .navigationTitle("Command Runner")
    .navigationBarTitleDisplayMode(.inline)
    .toolbar {
      ToolbarItem(placement: .topBarTrailing) {
        Button {
          showCatalog.toggle()
        } label: {
          Image(systemName: "book")
        }
      }
      ToolbarItem(placement: .topBarTrailing) {
        Button {
          commandHistory.removeAll()
        } label: {
          Image(systemName: "trash")
        }
        .disabled(commandHistory.isEmpty)
      }
    }
    .sheet(isPresented: $showCatalog) {
      catalogSheet
    }
    .alert("Write Mode Warning", isPresented: $showWriteConfirmation) {
      Button("Cancel", role: .cancel) {
        pendingWriteCommand = nil
      }
      Button("Execute", role: .destructive) {
        if let pending = pendingWriteCommand {
          runCommand(name: pending.name, args: pending.args)
          pendingWriteCommand = nil
        }
      }
    } message: {
      Text("This command includes --write flags which will modify data on the desktop. Are you sure you want to proceed?")
    }
    .task {
      await loadCommandList()
    }
  }

  // MARK: - Subviews

  private func commandEntryView(_ entry: CommandHistoryEntry) -> some View {
    VStack(alignment: .leading, spacing: 6) {
      // Command input
      HStack(spacing: 6) {
        Text(">")
          .font(.caption.monospaced().weight(.bold))
          .foregroundStyle(.green)
        Text(entry.input)
          .font(.caption.monospaced())
          .foregroundStyle(.primary)
      }

      // Result
      if entry.isLoading {
        ProgressView()
          .scaleEffect(0.7)
      } else if let error = entry.error {
        Text(error)
          .font(.caption.monospaced())
          .foregroundStyle(.red)
      } else {
        Text(entry.output)
          .font(.caption.monospaced())
          .foregroundStyle(.secondary)
          .textSelection(.enabled)
      }
    }
    .padding(10)
    .frame(maxWidth: .infinity, alignment: .leading)
    .background(Color(.systemGray6))
    .clipShape(RoundedRectangle(cornerRadius: 8))
  }

  private var catalogSheet: some View {
    NavigationStack {
      List {
        TextField("Search commands...", text: $catalogSearch)
          .textFieldStyle(.roundedBorder)

        ForEach(groupedCommands, id: \.0) { category, commands in
          Section(category) {
            ForEach(commands) { cmd in
              Button {
                commandText = cmd.name + " "
                showCatalog = false
              } label: {
                VStack(alignment: .leading, spacing: 2) {
                  HStack {
                    Text(cmd.name)
                      .font(.subheadline.monospaced().weight(.medium))
                    if cmd.requiresRom {
                      Image(systemName: "doc")
                        .font(.caption2)
                        .foregroundStyle(.orange)
                    }
                  }
                  Text(cmd.description)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(2)
                }
              }
              .buttonStyle(.plain)
            }
          }
        }
      }
      .navigationTitle("Command Catalog")
      .navigationBarTitleDisplayMode(.inline)
    }
    .presentationDetents([.medium, .large])
  }

  // MARK: - Actions

  private func executeCurrentCommand() {
    let input = commandText.trimmingCharacters(in: .whitespaces)
    guard !input.isEmpty else { return }

    let parts = input.split(separator: " ", omittingEmptySubsequences: true).map(String.init)
    guard let name = parts.first else { return }
    let args = Array(parts.dropFirst())

    // Check for --write flags requiring confirmation
    if args.contains(where: { $0.contains("--write") }) {
      pendingWriteCommand = (name: name, args: args)
      showWriteConfirmation = true
      return
    }

    runCommand(name: name, args: args)
  }

  private func runCommand(name: String, args: [String]) {
    let input = ([name] + args).joined(separator: " ")
    let entry = CommandHistoryEntry(input: input)
    commandHistory.append(entry)
    commandText = ""
    isExecuting = true

    Task {
      do {
        let response = try await apiClient.executeCommand(name: name, args: args)
        await MainActor.run {
          if let idx = commandHistory.firstIndex(where: { $0.id == entry.id }) {
            var formatted: String
            if let result = response.result {
              if let jsonData = try? JSONSerialization.data(
                withJSONObject: result.value,
                options: [.prettyPrinted, .sortedKeys]
              ), let jsonStr = String(data: jsonData, encoding: .utf8) {
                formatted = jsonStr
              } else {
                formatted = "\(result.value)"
              }
            } else {
              formatted = "OK"
            }
            commandHistory[idx].output = formatted
            commandHistory[idx].isLoading = false
          }
          isExecuting = false
        }
      } catch {
        await MainActor.run {
          if let idx = commandHistory.firstIndex(where: { $0.id == entry.id }) {
            commandHistory[idx].error = error.localizedDescription
            commandHistory[idx].isLoading = false
          }
          isExecuting = false
        }
      }
    }
  }

  private func loadCommandList() async {
    do {
      let commands = try await apiClient.fetchCommandList()
      await MainActor.run {
        availableCommands = commands
      }
    } catch {
      // Silently ignore - catalog will be empty
    }
  }
}

/// A single command execution entry in the history.
struct CommandHistoryEntry: Identifiable {
  let id = UUID()
  let input: String
  var output: String = ""
  var error: String?
  var isLoading: Bool = true
}
