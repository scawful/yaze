#ifndef YAZE_CORE_ORACLE_PROGRESSION_H
#define YAZE_CORE_ORACLE_PROGRESSION_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace yaze::core {

/**
 * @brief Oracle of Secrets game progression state parsed from SRAM.
 *
 * This struct represents the key SRAM variables that track game progress:
 * - Crystal bitfield ($7EF37A) — non-sequential bit mapping for 7 dungeons
 * - Game state ($7EF3C5) — main story phase
 * - Story flags ($7EF3D6, $7EF3C6) — individual story events
 * - Side quest progress ($7EF3D7) — optional content tracking
 * - Pendants ($7EF374) — shrine completion tracking
 *
 * Crystal bit mapping (intentionally non-sequential for non-linear design):
 *   D1 Mushroom Grotto  = 0x01    D2 Tail Palace   = 0x10
 *   D6 Goron Mines      = 0x02    D4 Zora Temple   = 0x20
 *   D5 Glacia Estate    = 0x04    D3 Kalyxo Castle = 0x40
 *   D7 Dragon Ship      = 0x08
 */
struct OracleProgressionState {
  uint8_t crystal_bitfield = 0;  // $7EF37A
  uint8_t game_state = 0;        // $7EF3C5
  uint8_t oosprog = 0;           // $7EF3D6
  uint8_t oosprog2 = 0;          // $7EF3C6
  uint8_t side_quest = 0;        // $7EF3D7
  uint8_t pendants = 0;          // $7EF374

  // ─── SRAM offsets (relative to save file start at $7EF000) ────

  static constexpr uint16_t kCrystalOffset = 0x37A;
  static constexpr uint16_t kGameStateOffset = 0x3C5;
  static constexpr uint16_t kOosProgOffset = 0x3D6;
  static constexpr uint16_t kOosProg2Offset = 0x3C6;
  static constexpr uint16_t kSideQuestOffset = 0x3D7;
  static constexpr uint16_t kPendantOffset = 0x374;

  // ─── Crystal bit constants ─────────────────────────────────────

  static constexpr uint8_t kCrystalD1 = 0x01;  // Mushroom Grotto
  static constexpr uint8_t kCrystalD6 = 0x02;  // Goron Mines
  static constexpr uint8_t kCrystalD5 = 0x04;  // Glacia Estate
  static constexpr uint8_t kCrystalD7 = 0x08;  // Dragon Ship
  static constexpr uint8_t kCrystalD2 = 0x10;  // Tail Palace
  static constexpr uint8_t kCrystalD4 = 0x20;  // Zora Temple
  static constexpr uint8_t kCrystalD3 = 0x40;  // Kalyxo Castle

  // ─── Parsing ───────────────────────────────────────────────────

  /**
   * @brief Parse progression state from raw SRAM data.
   *
   * @param data Pointer to SRAM data starting at $7EF000.
   * @param len Length of the data buffer.
   * @return Parsed state. Fields default to 0 if offset is out of range.
   */
  static OracleProgressionState ParseFromSRAM(const uint8_t* data, size_t len);

  // ─── Queries ───────────────────────────────────────────────────

  /**
   * @brief Count completed dungeons using popcount on crystal bitfield.
   *
   * Only counts the 7 valid crystal bits (D1-D7). Upper bit is unused.
   */
  [[nodiscard]] int GetCrystalCount() const;

  /**
   * @brief Check if a specific dungeon is complete.
   *
   * @param dungeon_number 1-7 (D1-D7). Returns false for out-of-range.
   */
  [[nodiscard]] bool IsDungeonComplete(int dungeon_number) const;

  /**
   * @brief Get human-readable name for the current game state.
   */
  [[nodiscard]] std::string GetGameStateName() const;

  /**
   * @brief Get the dungeon name for a dungeon number (1-7).
   */
  [[nodiscard]] static std::string GetDungeonName(int dungeon_number);

  /**
   * @brief Get the crystal bitmask for a dungeon number (1-7).
   *
   * Returns 0 for out-of-range values.
   */
  [[nodiscard]] static uint8_t GetCrystalMask(int dungeon_number);
};

}  // namespace yaze::core

#endif  // YAZE_CORE_ORACLE_PROGRESSION_H
