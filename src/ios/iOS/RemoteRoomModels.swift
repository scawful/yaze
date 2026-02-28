import Foundation

/// Room summary for the room browser list.
struct RoomSummary: Identifiable {
  let id: Int
  let hexLabel: String

  init(roomId: Int) {
    self.id = roomId
    self.hexLabel = String(format: "0x%03X", roomId)
  }
}

/// All 296 standard dungeon rooms (0x000 - 0x127).
enum RoomCatalog {
  static let totalRooms = 296

  static var allRooms: [RoomSummary] {
    (0..<totalRooms).map { RoomSummary(roomId: $0) }
  }
}

/// Available overlay types matching the desktop render API.
enum RoomOverlay: String, CaseIterable, Identifiable {
  case collision
  case sprites
  case objects
  case track
  case camera
  case grid

  var id: String { rawValue }

  var label: String {
    switch self {
    case .collision: return "Collision"
    case .sprites: return "Sprites"
    case .objects: return "Objects"
    case .track: return "Track"
    case .camera: return "Camera"
    case .grid: return "Grid"
    }
  }

  var systemImage: String {
    switch self {
    case .collision: return "shield"
    case .sprites: return "person.2"
    case .objects: return "cube"
    case .track: return "point.topleft.down.to.point.bottomright.curvepath"
    case .camera: return "camera"
    case .grid: return "grid"
    }
  }
}
