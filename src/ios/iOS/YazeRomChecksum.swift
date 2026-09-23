import CryptoKit
import Foundation

enum YazeRomChecksum {
  static func sha1Hex(for data: Data) -> String {
    Insecure.SHA1.hash(data: data)
      .map { String(format: "%02x", $0) }
      .joined()
  }

  static func sha1Hex(contentsOf url: URL) throws -> String {
    sha1Hex(for: try Data(contentsOf: url, options: .mappedIfSafe))
  }
}
