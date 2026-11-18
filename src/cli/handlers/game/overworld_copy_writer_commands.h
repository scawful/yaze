#ifndef YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_COPY_WRITER_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_COPY_WRITER_COMMANDS_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

// Offline copy writers. Each command reads the loaded ROM, stages a complete
// image in memory and writes it only to --out, never in place. They do not
// use the ROM write path, so the generic refusal of writes on Lyra images is
// unchanged: these commands produce a separate file and nothing else.

// One --map rule. A quadrant word whose palette is `palette` and whose 10-bit
// char is in `chars` gets `new_char`; flip, priority and palette bits stay.
struct Tile16CharRemapRule {
  uint8_t palette = 0;
  std::vector<uint16_t> chars;
  uint16_t new_char = 0;
};

struct Tile16CharRemapChange {
  int tile16 = 0;
  std::array<uint16_t, 4> old_words{};
  std::array<uint16_t, 4> new_words{};
};

// Parses "PAL:CHAR[,CHAR..]=NEWCHAR", e.g. "7:1C0,1C1,1D0,1D1=1E4". PAL is a
// decimal 0-7; chars are hex (optional 0x or $ prefix) in 0x000-0x3FF.
absl::StatusOr<Tile16CharRemapRule> ParseTile16CharRemapRule(
    const std::string& text);

// Parses "A-B" or "A" (hex, inclusive) into [first, last] within [0, count).
absl::StatusOr<std::pair<int, int>> ParseTile16Range(const std::string& text,
                                                     int count);

// Applies `rules` to tile16 definitions [first, last] of the table at
// `table_pc` (8 bytes per tile16: TL, TR, BL, BR little-endian words).
// Refuses overlapping rules (one palette/char mapped to two chars) and a table
// outside the image before any byte changes. Returns the changed tile16s.
absl::StatusOr<std::vector<Tile16CharRemapChange>> RemapTile16Chars(
    std::vector<uint8_t>& image, size_t table_pc, int first, int last,
    const std::vector<Tile16CharRemapRule>& rules);

// Tile16 quadrant char remap.
//
// Usage:
//   overworld-tile16-remap --rom=<image> --map=PAL:CHAR[,CHAR..]=NEWCHAR
//       [--map=...] [--tiles=A-B] [--out=<copy.sfc>]
//
// Dry run by default: JSON lists the changed tile16 ids with old and new
// quadrant words. --out writes a complete copy (copier header kept) and reads
// it back. The table is Lyra directory entry 400 on a native OOSOW192 image,
// otherwise the expanded map16 table. On a Lyra image the input must pass
// InspectLyraLayout, and the copy gets refreshed directory CRCs through
// rom::RefreshLyraLayoutCrcs so it still validates.
class OverworldTile16RemapCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-tile16-remap"; }
  std::string GetUsage() const override {
    return "overworld-tile16-remap --rom=<image> "
           "--map=PAL:CHAR[,CHAR..]=NEWCHAR [--map=...] [--tiles=A-B] "
           "[--out=<copy.sfc>]";
  }
  Descriptor Describe() const override;

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Tile16 Char Remap"; }
};

// OOSOW192 Lyra directory CRC check. Lists stale entry CRCs and the descriptor
// CRC; exits nonzero (FailedPrecondition, JSON still printed) when any is
// stale. Uses the InspectLyraLayout decoder; no second parser.
//
// Usage:
//   lyra-crc-check --rom=<image>
class LyraCrcCheckCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "lyra-crc-check"; }
  std::string GetUsage() const override {
    return "lyra-crc-check --rom=<image>";
  }
  Descriptor Describe() const override;

  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::OkStatus();
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Lyra CRC Check"; }
};

// OOSOW192 Lyra directory CRC refresh. Writes a copy whose entry CRCs and
// descriptor CRC match its data; only those 4-byte fields differ from the
// input. --out is required and may not alias the input ROM.
//
// Usage:
//   lyra-crc-refresh --rom=<image> --out=<copy.sfc>
class LyraCrcRefreshCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "lyra-crc-refresh"; }
  std::string GetUsage() const override {
    return "lyra-crc-refresh --rom=<image> --out=<copy.sfc>";
  }
  Descriptor Describe() const override;

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Lyra CRC Refresh"; }
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_COPY_WRITER_COMMANDS_H_
