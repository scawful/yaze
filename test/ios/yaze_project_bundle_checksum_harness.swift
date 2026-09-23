import Foundation

struct HarnessGeneralSettings {
  var lastProjectPath = ""
  var lastRomPath = ""
}

struct HarnessSettings {
  var general = HarnessGeneralSettings()
}

final class YazeSettingsStore {
  let settingsFileURL: URL
  var settings: HarnessSettings

  init(settingsFileURL: URL, romPath: String) {
    self.settingsFileURL = settingsFileURL
    self.settings = HarnessSettings(
      general: HarnessGeneralSettings(lastRomPath: romPath)
    )
  }

  func updateCurrentRomPath(_ path: String) {}
  func updateCurrentProjectPath(_ path: String) {}
}

enum ChecksumHarnessError: Error {
  case failed(String)
}

@main
struct YazeProjectBundleChecksumHarness {
  static func require(_ condition: @autoclosure () -> Bool, _ message: String) throws {
    if !condition() {
      throw ChecksumHarnessError.failed(message)
    }
  }

  static func readManifest(at bundleURL: URL) throws -> YazeProjectBundleManifest {
    let manifestURL = bundleURL.appendingPathComponent("manifest.json")
    return try JSONDecoder().decode(
      YazeProjectBundleManifest.self,
      from: Data(contentsOf: manifestURL)
    )
  }

  static func main() throws {
    guard CommandLine.arguments.count == 2 else {
      throw ChecksumHarnessError.failed("usage: checksum-harness OUTPUT.yazeproj")
    }

    let fileManager = FileManager.default
    let outputURL = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
    guard !fileManager.fileExists(atPath: outputURL.path) else {
      throw ChecksumHarnessError.failed("output already exists: \(outputURL.path)")
    }
    let workURL = fileManager.temporaryDirectory.appendingPathComponent(
      "yaze-ios-checksum-harness-\(UUID().uuidString)", isDirectory: true)
    try fileManager.createDirectory(at: workURL, withIntermediateDirectories: true)
    defer { try? fileManager.removeItem(at: workURL) }

    let settingsURL = workURL.appendingPathComponent("settings.json")
    try Data("{}\n".utf8).write(to: settingsURL)
    let sourceRomURL = workURL.appendingPathComponent("source.sfc")
    let firstRom = Data("abc".utf8)
    try firstRom.write(to: sourceRomURL)

    let settingsStore = YazeSettingsStore(
      settingsFileURL: settingsURL,
      romPath: sourceRomURL.path
    )
    guard let firstBundle = YazeProjectBundleService.exportBundle(settingsStore: settingsStore)
    else {
      throw ChecksumHarnessError.failed("initial export failed")
    }
    defer { try? fileManager.removeItem(at: firstBundle) }
    let firstCopiedRom = try Data(contentsOf: firstBundle.appendingPathComponent("rom"))
    let firstManifest = try readManifest(at: firstBundle)
    let knownSha1 = "a9993e364706816aba3e25717850c26c9cd0d89d"
    try require(
      YazeRomChecksum.sha1Hex(for: firstRom) == knownSha1,
      "known SHA1 vector mismatch")
    try require(firstCopiedRom == firstRom, "initial copied ROM differs from source")
    try require(
      firstManifest.romChecksum == YazeRomChecksum.sha1Hex(for: firstCopiedRom),
      "manifest checksum differs from copied ROM")

    let replacementRom = Data("replacement-rom".utf8)
    try replacementRom.write(to: sourceRomURL, options: .atomic)
    guard
      let replacementBundle = YazeProjectBundleService.exportBundle(
        settingsStore: settingsStore
      )
    else {
      throw ChecksumHarnessError.failed("replacement export failed")
    }
    defer { try? fileManager.removeItem(at: replacementBundle) }
    let replacementCopiedRom = try Data(
      contentsOf: replacementBundle.appendingPathComponent("rom")
    )
    let replacementManifest = try readManifest(at: replacementBundle)
    try require(
      replacementCopiedRom == replacementRom,
      "replacement copied ROM differs from source")
    try require(
      replacementManifest.romChecksum == YazeRomChecksum.sha1Hex(for: replacementCopiedRom),
      "replacement checksum differs from copied ROM")
    try require(
      replacementManifest.romChecksum != firstManifest.romChecksum,
      "replacement export retained the prior checksum")

    let legacyManifest = Data(
      #"{"version":1,"createdAt":"now","projectName":"Legacy","romName":"rom","notes":"old"}"#.utf8
    )
    let decodedLegacy = try JSONDecoder().decode(
      YazeProjectBundleManifest.self,
      from: legacyManifest
    )
    try require(
      decodedLegacy.romChecksum == nil,
      "legacy manifest without romChecksum did not decode")

    try fileManager.copyItem(at: replacementBundle, to: outputURL)
    print("iOS bundle checksum producer: 4/4 PASS")
    print("fixture=\(outputURL.path)")
  }
}
