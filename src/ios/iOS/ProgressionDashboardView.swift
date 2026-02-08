import SwiftUI

/// Crystal bit mapping (non-sequential, matching ASM)
private struct CrystalInfo: Identifiable {
  let id: Int  // Dungeon number (1-7)
  let name: String
  let mask: UInt8
}

private let crystals: [CrystalInfo] = [
  CrystalInfo(id: 1, name: "D1 Mushroom Grotto", mask: 0x01),
  CrystalInfo(id: 2, name: "D2 Tail Palace", mask: 0x10),
  CrystalInfo(id: 3, name: "D3 Kalyxo Castle", mask: 0x40),
  CrystalInfo(id: 4, name: "D4 Zora Temple", mask: 0x20),
  CrystalInfo(id: 5, name: "D5 Glacia Estate", mask: 0x04),
  CrystalInfo(id: 6, name: "D6 Goron Mines", mask: 0x02),
  CrystalInfo(id: 7, name: "D7 Dragon Ship", mask: 0x08),
]

/// Game state phases
private let gameStatePhases = ["Start", "Loom Beach", "Kydrog Complete", "Farore Rescued"]

/// Native SwiftUI version of the progression dashboard.
struct ProgressionDashboardView: View {
  @State private var crystalBitfield: UInt8 = 0
  @State private var gameState: Int = 0

  var body: some View {
    ScrollView {
      VStack(alignment: .leading, spacing: 24) {
        crystalSection
        gameStateSection
        dungeonGrid
        statsSection
      }
      .padding()
    }
    .navigationTitle("Progression")
  }

  // MARK: - Crystal Tracker

  private var crystalSection: some View {
    VStack(alignment: .leading, spacing: 12) {
      Label("Crystals", systemImage: "diamond.fill")
        .font(.headline)

      LazyVGrid(columns: [GridItem(.adaptive(minimum: 70))], spacing: 8) {
        ForEach(crystals) { crystal in
          let complete = (crystalBitfield & crystal.mask) != 0

          Button {
            crystalBitfield ^= crystal.mask
          } label: {
            VStack(spacing: 4) {
              Image(systemName: complete ? "diamond.fill" : "diamond")
                .font(.title2)
                .foregroundStyle(complete ? .green : .gray)
              Text("D\(crystal.id)")
                .font(.caption.weight(.bold))
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 8)
            .background(complete ? Color.green.opacity(0.15) : Color.gray.opacity(0.1))
            .clipShape(RoundedRectangle(cornerRadius: 10))
          }
          .buttonStyle(.plain)
        }
      }

      Text("\(crystalCount) / 7 Crystals")
        .font(.subheadline)
        .foregroundStyle(.secondary)
    }
  }

  // MARK: - Game State

  private var gameStateSection: some View {
    VStack(alignment: .leading, spacing: 12) {
      Label("Game State", systemImage: "flag.fill")
        .font(.headline)

      HStack(spacing: 2) {
        ForEach(0..<gameStatePhases.count, id: \.self) { index in
          let active = index <= gameState
          VStack(spacing: 2) {
            Rectangle()
              .fill(active ? Color.blue : Color.gray.opacity(0.3))
              .frame(height: 8)
              .clipShape(Capsule())
            Text(gameStatePhases[index])
              .font(.caption2)
              .foregroundStyle(active ? .primary : .tertiary)
          }
          .onTapGesture {
            gameState = index
          }
        }
      }

      Text("Phase: \(gameStatePhases[min(gameState, gameStatePhases.count - 1)])")
        .font(.subheadline)
        .foregroundStyle(.secondary)
    }
  }

  // MARK: - Dungeon Grid

  private var dungeonGrid: some View {
    VStack(alignment: .leading, spacing: 12) {
      Label("Dungeon Completion", systemImage: "building.columns.fill")
        .font(.headline)

      LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 6) {
        ForEach(crystals) { crystal in
          let complete = (crystalBitfield & crystal.mask) != 0
          HStack {
            Image(systemName: complete ? "checkmark.circle.fill" : "circle")
              .foregroundStyle(complete ? .green : .gray)
            Text(crystal.name)
              .font(.caption)
            Spacer()
          }
          .padding(.vertical, 4)
          .padding(.horizontal, 8)
          .background(complete ? Color.green.opacity(0.1) : Color.clear)
          .clipShape(RoundedRectangle(cornerRadius: 6))
        }

        // Special dungeons (no crystal tracking)
        ForEach(["FOS Fortress", "SOP Shrine of Power", "SOW Shrine of Wisdom"], id: \.self) { name in
          HStack {
            Image(systemName: "circle")
              .foregroundStyle(.gray)
            Text(name)
              .font(.caption)
            Spacer()
          }
          .padding(.vertical, 4)
          .padding(.horizontal, 8)
        }
      }
    }
  }

  // MARK: - Stats

  private var statsSection: some View {
    VStack(alignment: .leading, spacing: 8) {
      Label("Quick Controls", systemImage: "slider.horizontal.3")
        .font(.headline)

      HStack {
        Button("Clear All") {
          crystalBitfield = 0
          gameState = 0
        }
        .buttonStyle(.bordered)

        Button("Complete All") {
          crystalBitfield = 0x7F
          gameState = 3
        }
        .buttonStyle(.borderedProminent)
      }
    }
  }

  private var crystalCount: Int {
    var count = 0
    var bits = crystalBitfield & 0x7F
    while bits != 0 {
      count += Int(bits & 1)
      bits >>= 1
    }
    return count
  }
}
