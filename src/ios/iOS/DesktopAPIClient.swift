import Foundation
import Combine

/// Response types for the desktop API.
struct HealthResponse: Codable {
  let status: String
  let version: String
  let service: String
}

struct CommandInfo: Identifiable, Codable {
  var id: String { name }
  let name: String
  let category: String
  let description: String
  let usage: String
  let requiresRom: Bool

  enum CodingKeys: String, CodingKey {
    case name, category, description, usage
    case requiresRom = "requires_rom"
  }
}

struct CommandListResponse: Codable {
  let commands: [CommandInfo]
  let count: Int
}

struct CommandExecuteResponse: Codable {
  let status: String
  let command: String
  let result: AnyCodable?
}

struct RoomMetadataResponse: Codable {
  let roomId: Int
  let blockset: Int
  let spriteset: Int
  let palette: Int
  let layoutId: Int
  let effect: Int
  let collision: Int
  let tag1: Int
  let tag2: Int
  let messageId: Int
  let hasCustomCollision: Bool
  let objectCount: Int
  let spriteCount: Int

  enum CodingKeys: String, CodingKey {
    case roomId = "room_id"
    case blockset, spriteset, palette
    case layoutId = "layout_id"
    case effect, collision, tag1, tag2
    case messageId = "message_id"
    case hasCustomCollision = "has_custom_collision"
    case objectCount = "object_count"
    case spriteCount = "sprite_count"
  }
}

struct RemoteAnnotation: Identifiable, Codable {
  var id: String
  var roomID: Int
  var text: String
  var priority: Int
  var category: String
  var createdAt: String?
  var modifiedAt: String?
  var createdBy: String?

  enum CodingKeys: String, CodingKey {
    case id
    case roomID = "room_id"
    case text, priority, category
    case createdAt = "created_at"
    case modifiedAt = "modified_at"
    case createdBy = "created_by"
  }
}

struct RemoteAnnotationListResponse: Codable {
  let annotations: [RemoteAnnotation]
}

/// Type-erased Codable wrapper for arbitrary JSON values in command results.
struct AnyCodable: Codable {
  let value: Any

  init(_ value: Any) {
    self.value = value
  }

  init(from decoder: Decoder) throws {
    let container = try decoder.singleValueContainer()
    if let str = try? container.decode(String.self) {
      value = str
    } else if let int = try? container.decode(Int.self) {
      value = int
    } else if let double = try? container.decode(Double.self) {
      value = double
    } else if let bool = try? container.decode(Bool.self) {
      value = bool
    } else if let dict = try? container.decode([String: AnyCodable].self) {
      value = dict.mapValues { $0.value }
    } else if let arr = try? container.decode([AnyCodable].self) {
      value = arr.map { $0.value }
    } else {
      value = NSNull()
    }
  }

  func encode(to encoder: Encoder) throws {
    var container = encoder.singleValueContainer()
    if let str = value as? String {
      try container.encode(str)
    } else if let int = value as? Int {
      try container.encode(int)
    } else if let double = value as? Double {
      try container.encode(double)
    } else if let bool = value as? Bool {
      try container.encode(bool)
    } else {
      try container.encodeNil()
    }
  }
}

/// REST client for communicating with the desktop yaze HTTP server.
final class DesktopAPIClient: ObservableObject {
  @Published var isConnected = false
  @Published var lastError: String = ""
  @Published var connectedHost: DiscoveredHost?

  private var baseURL: URL?
  private let session: URLSession
  private let decoder: JSONDecoder

  init() {
    let config = URLSessionConfiguration.default
    config.timeoutIntervalForRequest = 10
    config.timeoutIntervalForResource = 30
    session = URLSession(configuration: config)
    decoder = JSONDecoder()
  }

  // MARK: - Connection

  func connect(to host: DiscoveredHost) {
    let urlString = host.baseURL
    guard let url = URL(string: urlString) else {
      lastError = "Invalid URL: \(urlString)"
      return
    }
    baseURL = url
    connectedHost = host

    Task { @MainActor in
      do {
        let _: HealthResponse = try await get(path: "/api/v1/health")
        isConnected = true
        lastError = ""
      } catch {
        isConnected = false
        lastError = "Health check failed: \(error.localizedDescription)"
      }
    }
  }

  func connectManual(host: String, port: UInt16) {
    let discovered = DiscoveredHost(
      id: "manual-\(host):\(port)",
      name: "Manual (\(host))",
      host: host,
      port: port
    )
    connect(to: discovered)
  }

  func disconnect() {
    isConnected = false
    connectedHost = nil
    baseURL = nil
    lastError = ""
  }

  // MARK: - Phase 2: Room Viewer

  func fetchRoomImage(roomId: Int, overlays: [String] = [], scale: Float = 1.0) async throws -> Data {
    var params = "room=\(roomId)"
    if !overlays.isEmpty {
      params += "&overlays=\(overlays.joined(separator: ","))"
    }
    if scale != 1.0 {
      params += "&scale=\(scale)"
    }
    return try await getRaw(path: "/api/v1/render/dungeon?\(params)")
  }

  func fetchRoomMetadata(roomId: Int) async throws -> RoomMetadataResponse {
    try await get(path: "/api/v1/render/dungeon/metadata?room=\(roomId)")
  }

  // MARK: - Phase 3: Command Runner

  func fetchCommandList() async throws -> [CommandInfo] {
    let response: CommandListResponse = try await get(path: "/api/v1/command/list")
    return response.commands
  }

  func executeCommand(name: String, args: [String]) async throws -> CommandExecuteResponse {
    let body: [String: Any] = [
      "command": name,
      "args": args
    ]
    return try await post(path: "/api/v1/command/execute", jsonBody: body)
  }

  // MARK: - Phase 4: Annotations

  func fetchAnnotations(roomId: Int? = nil) async throws -> [RemoteAnnotation] {
    var path = "/api/v1/annotations"
    if let roomId {
      path += "?room=\(roomId)"
    }
    let response: RemoteAnnotationListResponse = try await get(path: path)
    return response.annotations
  }

  func pushAnnotation(_ annotation: RemoteAnnotation) async throws {
    let _: [String: String] = try await post(
      path: "/api/v1/annotations",
      encodable: annotation
    )
  }

  func updateAnnotation(_ annotation: RemoteAnnotation) async throws {
    let _: [String: String] = try await put(
      path: "/api/v1/annotations/\(annotation.id)",
      encodable: annotation
    )
  }

  func deleteAnnotation(id: String) async throws {
    try await deleteRequest(path: "/api/v1/annotations/\(id)")
  }

  // MARK: - HTTP helpers

  private func get<T: Decodable>(path: String) async throws -> T {
    guard let base = baseURL else { throw APIError.notConnected }
    guard let url = URL(string: path, relativeTo: base) else {
      throw APIError.invalidURL(path)
    }
    let (data, response) = try await session.data(from: url)
    try checkHTTPResponse(response)
    return try decoder.decode(T.self, from: data)
  }

  private func getRaw(path: String) async throws -> Data {
    guard let base = baseURL else { throw APIError.notConnected }
    guard let url = URL(string: path, relativeTo: base) else {
      throw APIError.invalidURL(path)
    }
    let (data, response) = try await session.data(from: url)
    try checkHTTPResponse(response)
    return data
  }

  private func post<T: Decodable>(path: String, jsonBody: [String: Any]) async throws -> T {
    guard let base = baseURL else { throw APIError.notConnected }
    guard let url = URL(string: path, relativeTo: base) else {
      throw APIError.invalidURL(path)
    }
    var request = URLRequest(url: url)
    request.httpMethod = "POST"
    request.setValue("application/json", forHTTPHeaderField: "Content-Type")
    request.httpBody = try JSONSerialization.data(withJSONObject: jsonBody)
    let (data, response) = try await session.data(for: request)
    try checkHTTPResponse(response)
    return try decoder.decode(T.self, from: data)
  }

  private func post<T: Decodable, E: Encodable>(path: String, encodable: E) async throws -> T {
    guard let base = baseURL else { throw APIError.notConnected }
    guard let url = URL(string: path, relativeTo: base) else {
      throw APIError.invalidURL(path)
    }
    var request = URLRequest(url: url)
    request.httpMethod = "POST"
    request.setValue("application/json", forHTTPHeaderField: "Content-Type")
    request.httpBody = try JSONEncoder().encode(encodable)
    let (data, response) = try await session.data(for: request)
    try checkHTTPResponse(response)
    return try decoder.decode(T.self, from: data)
  }

  private func put<T: Decodable, E: Encodable>(path: String, encodable: E) async throws -> T {
    guard let base = baseURL else { throw APIError.notConnected }
    guard let url = URL(string: path, relativeTo: base) else {
      throw APIError.invalidURL(path)
    }
    var request = URLRequest(url: url)
    request.httpMethod = "PUT"
    request.setValue("application/json", forHTTPHeaderField: "Content-Type")
    request.httpBody = try JSONEncoder().encode(encodable)
    let (data, response) = try await session.data(for: request)
    try checkHTTPResponse(response)
    return try decoder.decode(T.self, from: data)
  }

  private func deleteRequest(path: String) async throws {
    guard let base = baseURL else { throw APIError.notConnected }
    guard let url = URL(string: path, relativeTo: base) else {
      throw APIError.invalidURL(path)
    }
    var request = URLRequest(url: url)
    request.httpMethod = "DELETE"
    let (_, response) = try await session.data(for: request)
    try checkHTTPResponse(response)
  }

  private func checkHTTPResponse(_ response: URLResponse) throws {
    guard let http = response as? HTTPURLResponse else {
      throw APIError.invalidResponse
    }
    guard (200...299).contains(http.statusCode) else {
      throw APIError.httpError(http.statusCode)
    }
  }

  enum APIError: LocalizedError {
    case notConnected
    case invalidURL(String)
    case invalidResponse
    case httpError(Int)

    var errorDescription: String? {
      switch self {
      case .notConnected: return "Not connected to desktop"
      case .invalidURL(let path): return "Invalid URL: \(path)"
      case .invalidResponse: return "Invalid response"
      case .httpError(let code): return "HTTP error \(code)"
      }
    }
  }
}
