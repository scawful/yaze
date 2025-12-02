#ifndef YAZE_CLI_SERVICE_TILE16_PROPOSAL_GENERATOR_H_
#define YAZE_CLI_SERVICE_TILE16_PROPOSAL_GENERATOR_H_

#include <chrono>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/core/bitmap.h"
#include "rom/rom.h"

namespace yaze {
namespace cli {

/**
 * @brief Represents a single tile16 change in a proposal.
 */
struct Tile16Change {
  int map_id;         // Overworld map ID
  int x;              // Tile16 X coordinate
  int y;              // Tile16 Y coordinate
  uint16_t old_tile;  // Original tile16 ID (for rollback)
  uint16_t new_tile;  // New tile16 ID to apply

  std::string ToString() const;
};

/**
 * @brief Represents a proposal for tile16 edits on the overworld.
 *
 * This is the core data structure for the accept/reject workflow.
 * AI generates proposals, which are then applied to a sandbox ROM
 * for preview before being committed to the main ROM.
 */
struct Tile16Proposal {
  std::string id;                     // Unique proposal ID (UUID-like)
  std::string prompt;                 // Original user prompt
  std::vector<Tile16Change> changes;  // List of tile changes
  std::string reasoning;              // AI's explanation
  std::string ai_service;             // "gemini", "ollama", "mock"
  std::chrono::system_clock::time_point created_at;  // Timestamp

  // Proposal state
  enum class Status {
    PENDING,   // Generated but not reviewed
    ACCEPTED,  // User accepted, changes applied
    REJECTED,  // User rejected, changes discarded
    APPLIED    // Successfully applied to ROM
  };
  Status status = Status::PENDING;

  std::string ToJson() const;
  static absl::StatusOr<Tile16Proposal> FromJson(const std::string& json);
};

/**
 * @brief Generates and manages tile16 editing proposals.
 *
 * This class bridges the AI service with the overworld editing system,
 * providing a safe sandbox workflow for reviewing and applying changes.
 */
class Tile16ProposalGenerator {
 public:
  Tile16ProposalGenerator() = default;

  // Allow testing of private methods
  friend class Tile16ProposalGeneratorTest;

  /**
   * @brief Generate a tile16 proposal from an AI-generated command list.
   *
   * @param prompt The original user prompt
   * @param commands List of commands from AI (e.g., "overworld set-tile ...")
   * @param ai_service Name of the AI service used
   * @param rom Reference ROM for validation
   * @return Tile16Proposal with parsed changes
   */
  absl::StatusOr<Tile16Proposal> GenerateFromCommands(
      const std::string& prompt, const std::vector<std::string>& commands,
      const std::string& ai_service, Rom* rom);

  /**
   * @brief Apply a proposal to a ROM (typically a sandbox).
   *
   * This modifies the ROM in memory but doesn't save to disk.
   * Used for preview and testing.
   *
   * @param proposal The proposal to apply
   * @param rom The ROM to modify
   * @return Status indicating success or failure
   */
  absl::Status ApplyProposal(const Tile16Proposal& proposal, Rom* rom);

  /**
   * @brief Generate a visual diff bitmap for a proposal.
   *
   * Creates a side-by-side or overlay comparison of before/after state.
   *
   * @param proposal The proposal to visualize
   * @param before_rom ROM in original state
   * @param after_rom ROM with proposal applied
   * @return Bitmap showing the visual difference
   */
  absl::StatusOr<gfx::Bitmap> GenerateDiff(const Tile16Proposal& proposal,
                                           Rom* before_rom, Rom* after_rom);

  /**
   * @brief Save a proposal to a JSON file for later review.
   *
   * @param proposal The proposal to save
   * @param path File path to save to
   * @return Status indicating success or failure
   */
  absl::Status SaveProposal(const Tile16Proposal& proposal,
                            const std::string& path);

  /**
   * @brief Load a proposal from a JSON file.
   *
   * @param path File path to load from
   * @return The loaded proposal or error
   */
  absl::StatusOr<Tile16Proposal> LoadProposal(const std::string& path);

 private:
  /**
   * @brief Parse a single "overworld set-tile" command into a Tile16Change.
   *
   * Expected format: "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E"
   */
  absl::StatusOr<Tile16Change> ParseSetTileCommand(const std::string& command,
                                                   Rom* rom);

  /**
   * @brief Parse a "overworld set-area" command into multiple Tile16Changes.
   *
   * Expected format: "overworld set-area --map 0 --x 10 --y 20 --width 5
   * --height 3 --tile 0x02E"
   */
  absl::StatusOr<std::vector<Tile16Change>> ParseSetAreaCommand(
      const std::string& command, Rom* rom);

  /**
   * @brief Parse a "overworld replace-tile" command into multiple
   * Tile16Changes.
   *
   * Expected format: "overworld replace-tile --map 0 --old-tile 0x02E
   * --new-tile 0x030" Can also specify optional bounds: --x-min 0 --y-min 0
   * --x-max 31 --y-max 31
   */
  absl::StatusOr<std::vector<Tile16Change>> ParseReplaceTileCommand(
      const std::string& command, Rom* rom);

  /**
   * @brief Generate a unique proposal ID.
   */
  std::string GenerateProposalId() const;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_TILE16_PROPOSAL_GENERATOR_H_
