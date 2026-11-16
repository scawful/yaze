#include "app/editor/overworld/live_proposal.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <system_error>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"

namespace yaze::editor {
namespace {

using nlohmann::json;

constexpr int kMapCells = 32;

bool ValidId(std::string_view id) {
  // All dots ("." / "..") is refused, as the editor-control client does.
  if (id.empty() || id.size() > 64 ||
      id.find_first_not_of('.') == std::string_view::npos)
    return false;
  for (const char c : id) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    if (!ok)
      return false;
  }
  return true;
}

std::string OptionalString(const json& object, const char* field) {
  if (!object.contains(field) || object.at(field).is_null())
    return {};
  if (!object.at(field).is_string())
    throw std::invalid_argument(std::string(field) + " must be a string");
  return object.at(field).get<std::string>();
}

int RequiredInt(const json& object, const char* field) {
  if (!object.contains(field) || !object.at(field).is_number_integer())
    throw std::invalid_argument(std::string(field) + " must be an integer");
  // Range-check before narrowing: get<int>() would wrap 2^32 + 5 to 5.
  const auto& value = object.at(field);
  const bool in_range =
      value.is_number_unsigned()
          ? value.get<uint64_t>() <=
                static_cast<uint64_t>(std::numeric_limits<int>::max())
          : value.get<int64_t>() >= std::numeric_limits<int>::min() &&
                value.get<int64_t>() <= std::numeric_limits<int>::max();
  if (!in_range)
    throw std::invalid_argument(std::string(field) + " is out of range");
  return value.get<int>();
}

// A packet-relative PNG path: no root, no "..", no backslash, ends in .png.
bool ValidAttachmentPath(std::string_view path) {
  if (path.empty() || path.size() > 255 ||
      path.find('\\') != std::string_view::npos ||
      path.find('\0') != std::string_view::npos ||
      !absl::EndsWithIgnoreCase(path, ".png"))
    return false;
  const std::filesystem::path parsed{std::string(path)};
  if (parsed.is_absolute() || parsed.has_root_name() ||
      parsed.has_root_directory())
    return false;
  for (const auto& part : parsed) {
    if (part == "..")
      return false;
  }
  return true;
}

// Reads width and height from the PNG signature and the leading IHDR chunk.
// Returns false when the bytes do not start that way. Values above INT_MAX
// come back negative and fail the caller's range check.
bool PngHeaderSize(const std::vector<uint8_t>& bytes, int& width, int& height) {
  static constexpr uint8_t kSignature[8] = {0x89, 'P',  'N',  'G',
                                            '\r', '\n', 0x1A, '\n'};
  if (bytes.size() < 24 ||
      !std::equal(kSignature, kSignature + 8, bytes.begin()))
    return false;
  const auto be32 = [&](size_t at) {
    return (static_cast<uint32_t>(bytes[at]) << 24) |
           (static_cast<uint32_t>(bytes[at + 1]) << 16) |
           (static_cast<uint32_t>(bytes[at + 2]) << 8) |
           static_cast<uint32_t>(bytes[at + 3]);
  };
  if (be32(8) != 13 || bytes[12] != 'I' || bytes[13] != 'H' ||
      bytes[14] != 'D' || bytes[15] != 'R')
    return false;
  const uint32_t w = be32(16);
  const uint32_t h = be32(20);
  width = w > static_cast<uint32_t>(std::numeric_limits<int>::max())
              ? -1
              : static_cast<int>(w);
  height = h > static_cast<uint32_t>(std::numeric_limits<int>::max())
               ? -1
               : static_cast<int>(h);
  return true;
}

bool ValidSha256(std::string_view hash) {
  if (hash.size() != 64)
    return false;
  for (const char c : hash) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return false;
  }
  return true;
}

const char* DecisionName(LiveProposalDecision decision) {
  switch (decision) {
    case LiveProposalDecision::kAccepted:
      return "accepted";
    case LiveProposalDecision::kRejected:
      return "rejected";
    case LiveProposalDecision::kPending:
      break;
  }
  return "pending";
}

}  // namespace

std::string CountLabel(int count, std::string_view singular,
                       std::string_view plural) {
  return absl::StrFormat("%d %s", count, count == 1 ? singular : plural);
}

absl::StatusOr<LiveProposal> ParseLiveProposal(std::string_view json_text,
                                               int map_count,
                                               int tile16_count) {
  try {
    const auto packet = json::parse(json_text);
    if (!packet.is_object())
      return absl::InvalidArgumentError("Proposal must be a JSON object");
    if (OptionalString(packet, "schema") != kLiveProposalSchema) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Proposal schema must be %s", std::string(kLiveProposalSchema)));
    }
    LiveProposal proposal;
    proposal.id = OptionalString(packet, "id");
    if (!ValidId(proposal.id)) {
      return absl::InvalidArgumentError(
          "Proposal id must be 1-64 characters of [A-Za-z0-9._-]");
    }
    proposal.title = OptionalString(packet, "title");
    if (proposal.title.empty())
      return absl::InvalidArgumentError("Proposal title is required");
    proposal.author = OptionalString(packet, "author");
    proposal.why = OptionalString(packet, "why");
    proposal.map_id = RequiredInt(packet, "map_id");
    if (proposal.map_id < 0 || proposal.map_id >= map_count) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Proposal map 0x%02X is outside the ROM map model", proposal.map_id));
    }
    if (!packet.contains("cells") || !packet.at("cells").is_array() ||
        packet.at("cells").empty()) {
      return absl::InvalidArgumentError(
          "Proposal needs a non-empty cells array");
    }
    if (packet.at("cells").size() > kLiveProposalMaxCells) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Proposal has more than %d cells", kLiveProposalMaxCells));
    }
    std::vector<bool> seen(kMapCells * kMapCells, false);
    for (const auto& cell : packet.at("cells")) {
      if (!cell.is_object())
        return absl::InvalidArgumentError("Each cell must be an object");
      const int x = RequiredInt(cell, "x");
      const int y = RequiredInt(cell, "y");
      const int expected = RequiredInt(cell, "expected");
      const int proposed = RequiredInt(cell, "proposed");
      if (x < 0 || x >= kMapCells || y < 0 || y >= kMapCells) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Cell (%d,%d) is outside the 32x32 map", x, y));
      }
      if (expected < 0 || expected >= tile16_count || proposed < 0 ||
          proposed >= tile16_count) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Cell (%d,%d) names a Tile16 outside 0..0x%X", x, y,
                            tile16_count - 1));
      }
      if (expected == proposed) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Cell (%d,%d) proposes the tile it expects; leave it out", x, y));
      }
      if (seen[y * kMapCells + x]) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Proposal repeats cell (%d,%d)", x, y));
      }
      seen[y * kMapCells + x] = true;
      proposal.cells.push_back({x, y, static_cast<uint16_t>(expected),
                                static_cast<uint16_t>(proposed)});
    }
    if (packet.contains("attachments") && !packet.at("attachments").is_null()) {
      const auto& list = packet.at("attachments");
      if (!list.is_array())
        return absl::InvalidArgumentError("attachments must be an array");
      if (list.size() > kLiveProposalMaxAttachments) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Proposal has more than %d attachments",
                            kLiveProposalMaxAttachments));
      }
      for (const auto& item : list) {
        if (!item.is_object())
          return absl::InvalidArgumentError(
              "Each attachment must be an object");
        LiveProposalAttachment attachment;
        attachment.path = OptionalString(item, "path");
        attachment.sha256 = OptionalString(item, "sha256");
        attachment.caption = OptionalString(item, "caption");
        if (!ValidAttachmentPath(attachment.path)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Attachment path \"%s\" must be a .png path inside the packet "
              "folder (relative, no \"..\")",
              attachment.path));
        }
        if (!ValidSha256(attachment.sha256)) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Attachment %s needs a lowercase hex sha256", attachment.path));
        }
        if (attachment.caption.size() > 200) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Attachment %s caption is longer than 200 characters",
              attachment.path));
        }
        proposal.attachments.push_back(std::move(attachment));
      }
    }
    return proposal;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Proposal JSON is invalid: %s", error.what()));
  }
}

absl::Status LoadLiveProposalAttachments(LiveProposal& proposal) {
  if (proposal.attachments.empty())
    return absl::OkStatus();
  namespace fs = std::filesystem;
  const fs::path packet(proposal.packet_path);
  if (!packet.is_absolute()) {
    return absl::FailedPreconditionError(
        "Proposal packet path must be absolute to load attachments");
  }
  // Symlinks are resolved first, so a link (on the file or on a folder above
  // it) that points outside the packet folder is refused.
  std::error_code error;
  const fs::path folder = fs::canonical(packet.parent_path(), error);
  if (error) {
    return absl::NotFoundError("Proposal folder could not be read");
  }
  for (auto& attachment : proposal.attachments) {
    const fs::path file = fs::canonical(folder / attachment.path, error);
    if (error) {
      return absl::NotFoundError(
          absl::StrFormat("Attachment %s could not be read", attachment.path));
    }
    const fs::path inside = file.lexically_relative(folder);
    if (inside.empty() || *inside.begin() == ".." || *inside.begin() == ".") {
      return absl::InvalidArgumentError(
          absl::StrFormat("Attachment %s resolves outside the proposal folder",
                          attachment.path));
    }
    // Regular files only: a FIFO or device would block the UI thread.
    if (!fs::is_regular_file(file, error) || error) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attachment %s is not a regular file", attachment.path));
    }
    const auto size = fs::file_size(file, error);
    if (error) {
      return absl::NotFoundError(
          absl::StrFormat("Attachment %s could not be read", attachment.path));
    }
    if (size > kLiveProposalMaxAttachmentBytes) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attachment %s is larger than %d MiB", attachment.path,
          kLiveProposalMaxAttachmentBytes >> 20));
    }
    auto bytes = util::ReadBinaryFile(file.string());
    if (!bytes.ok()) {
      return absl::NotFoundError(
          absl::StrFormat("Attachment %s could not be read", attachment.path));
    }
    // The file may have grown between file_size and the read.
    if (bytes->size() > kLiveProposalMaxAttachmentBytes) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attachment %s is larger than %d MiB", attachment.path,
          kLiveProposalMaxAttachmentBytes >> 20));
    }
    const std::string_view view(reinterpret_cast<const char*>(bytes->data()),
                                bytes->size());
    if (core::ComputeSourceArtifactSha256(view) != attachment.sha256) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Attachment %s does not match its sha256", attachment.path));
    }
    // Check the IHDR size before decoding: DecodePng would otherwise allocate
    // buffers for up to 64M pixels before the side limit applies.
    int header_width = 0;
    int header_height = 0;
    if (!PngHeaderSize(*bytes, header_width, header_height)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Attachment %s is not a readable PNG: no IHDR header",
                          attachment.path));
    }
    if (header_width <= 0 || header_height <= 0 ||
        header_width > kLiveProposalMaxAttachmentSide ||
        header_height > kLiveProposalMaxAttachmentSide) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attachment %s is %dx%d; each side must be 1..%d px", attachment.path,
          header_width, header_height, kLiveProposalMaxAttachmentSide));
    }
    auto png = util::DecodePng(*bytes);
    if (!png.ok()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Attachment %s is not a readable PNG: %s",
                          attachment.path, png.status().message()));
    }
    if (png->width != header_width || png->height != header_height) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Attachment %s decoded to a different size than its header",
          attachment.path));
    }
    std::vector<uint8_t> rgba;
    if (png->indexed) {
      rgba.resize(png->indices.size() * 4);
      for (size_t i = 0; i < png->indices.size(); ++i) {
        const uint8_t index = png->indices[i];
        const auto color = index < png->palette.size()
                               ? png->palette[index]
                               : std::array<uint8_t, 4>{0, 0, 0, 0};
        for (int c = 0; c < 4; ++c)
          rgba[i * 4 + c] = color[c];
      }
    } else {
      rgba = std::move(png->rgba);
    }
    attachment.width = png->width;
    attachment.height = png->height;
    attachment.rgba =
        std::make_shared<const std::vector<uint8_t>>(std::move(rgba));
  }
  return absl::OkStatus();
}

LiveProposalClassification ClassifyLiveProposal(
    const LiveProposal& proposal,
    const std::function<uint16_t(int x, int y)>& current) {
  LiveProposalClassification result;
  result.states.reserve(proposal.cells.size());
  for (const auto& cell : proposal.cells) {
    const uint16_t found = current(cell.x, cell.y);
    if (found == cell.expected) {
      result.states.push_back(LiveProposalCellState::kReady);
      result.ready_cells.push_back(cell);
      ++result.ready;
    } else if (found == cell.proposed) {
      result.states.push_back(LiveProposalCellState::kAlreadyApplied);
      ++result.already_applied;
    } else {
      result.states.push_back(LiveProposalCellState::kConflict);
      result.conflict_cells.push_back({cell.x, cell.y, cell.expected, found});
      ++result.conflicts;
    }
  }
  return result;
}

std::string LiveProposalFeedbackPath(std::string_view packet_path,
                                     std::string_view id) {
  return (std::filesystem::path(packet_path).parent_path() /
          absl::StrFormat("feedback-%s.json", id))
      .string();
}

absl::StatusOr<std::string> BuildLiveProposalFeedback(
    const LiveProposal& proposal, const LiveProposalFeedbackEntry& entry,
    std::string_view existing_json) {
  json history = json::array();
  if (!existing_json.empty()) {
    const auto existing = json::parse(existing_json, nullptr, false);
    if (existing.is_discarded() || !existing.is_object() ||
        !existing.contains("history") || !existing.at("history").is_array()) {
      return absl::FailedPreconditionError(
          "Existing feedback file is not a feedback object; it was left "
          "untouched");
    }
    if (!existing.contains("id") || !existing.at("id").is_string() ||
        existing.at("id").get<std::string>() != proposal.id) {
      return absl::FailedPreconditionError(
          "Existing feedback file belongs to another proposal id; it was left "
          "untouched");
    }
    history = existing.at("history");
  }
  json conflicts = json::array();
  for (const auto& conflict : entry.conflicts) {
    conflicts.push_back({{"x", conflict.x},
                         {"y", conflict.y},
                         {"expected", conflict.expected},
                         {"found", conflict.found}});
  }
  history.push_back({{"decision", entry.decision},
                     {"comment", entry.comment},
                     {"packet_sha256", proposal.packet_sha256},
                     {"cells_total", proposal.cells.size()},
                     {"cells_applied", entry.cells_applied},
                     {"already_applied", entry.already_applied},
                     {"conflicts", conflicts},
                     {"rom_path", entry.rom_path},
                     {"at", entry.at_utc}});
  const json feedback = {{"schema", kLiveProposalFeedbackSchema},
                         {"id", proposal.id},
                         {"map_id", proposal.map_id},
                         {"status", DecisionName(proposal.decision)},
                         {"history", history}};
  return feedback.dump(2) + "\n";
}

absl::Status WriteLiveProposalFeedback(const LiveProposal& proposal,
                                       const LiveProposalFeedbackEntry& entry) {
  if (proposal.packet_path.empty())
    return absl::FailedPreconditionError("Proposal has no packet path");
  const std::filesystem::path target =
      LiveProposalFeedbackPath(proposal.packet_path, proposal.id);
  std::string existing;
  std::error_code error;
  if (std::filesystem::exists(target, error)) {
    std::ifstream in(target, std::ios::binary);
    if (!in) {
      return absl::UnavailableError(
          absl::StrFormat("Could not read %s", target.string()));
    }
    existing.assign(std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
    if (existing.empty()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "%s is empty; it was left untouched", target.string()));
    }
  }
  auto text = BuildLiveProposalFeedback(proposal, entry, existing);
  if (!text.ok()) {
    return absl::Status(
        text.status().code(),
        absl::StrFormat("%s: %s", target.string(), text.status().message()));
  }
  // Unique temp name: two writers never share or clobber a temp file.
  static std::atomic<uint64_t> counter{0};
  auto temp = target;
  temp += absl::StrFormat(".tmp-%08x-%d", std::random_device{}(),
                          counter.fetch_add(1));
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << *text;
    out.flush();
    if (!out) {
      out.close();
      std::filesystem::remove(temp, error);
      return absl::UnavailableError(
          absl::StrFormat("Could not write %s", temp.string()));
    }
  }
  std::filesystem::rename(temp, target, error);
  if (error) {
    std::filesystem::remove(temp, error);
    return absl::UnavailableError(
        absl::StrFormat("Could not publish %s", target.string()));
  }
  return absl::OkStatus();
}

absl::StatusOr<LiveProposalClassification> ClassifyLiveProposalWith(
    const LiveProposal& proposal, const LiveProposalHooks& hooks) {
  absl::Status read_error = absl::OkStatus();
  auto result = ClassifyLiveProposal(proposal, [&](int x, int y) -> uint16_t {
    auto tile = hooks.read(proposal.map_id, x, y);
    if (!tile.ok()) {
      if (read_error.ok())
        read_error = tile.status();
      return 0xFFFF;  // never a valid Tile16: classifies as a conflict
    }
    return *tile;
  });
  if (!read_error.ok())
    return read_error;
  return result;
}

absl::StatusOr<int> AcceptLiveProposalWith(LiveProposal& proposal,
                                           const LiveProposalHooks& hooks) {
  if (proposal.decision != LiveProposalDecision::kPending)
    return absl::FailedPreconditionError("Proposal is already decided");
  auto classification = ClassifyLiveProposalWith(proposal, hooks);
  if (!classification.ok())
    return classification.status();
  if (classification->ready == 0 && classification->already_applied == 0) {
    return absl::FailedPreconditionError(
        "Every cell conflicts with the current map; nothing to accept");
  }
  // Feedback first: the agent's record must never miss a decision the map
  // already shows. A failed write leaves everything as it was.
  LiveProposal decided = proposal;
  decided.decision = LiveProposalDecision::kAccepted;
  LiveProposalFeedbackEntry entry;
  entry.decision = "accepted";
  entry.cells_applied = classification->ready;
  entry.already_applied = classification->already_applied;
  entry.conflicts = classification->conflict_cells;
  entry.rom_path = hooks.rom_path;
  entry.at_utc = hooks.at_utc;
  if (const auto written = hooks.write_feedback(decided, entry);
      !written.ok()) {
    proposal.last_result = absl::StrFormat(
        "Not accepted: feedback file could not be written (%s). Nothing "
        "changed; Accept again to retry.",
        written.message());
    return written;
  }
  int applied = 0;
  if (classification->ready > 0) {
    // `apply` re-checks every ready cell and writes all or none, as one
    // non-merging undo step.
    auto result = hooks.apply(proposal.map_id, classification->ready_cells);
    if (!result.ok()) {
      LiveProposalFeedbackEntry failed;
      failed.decision = "accept_failed";
      failed.comment = std::string(result.status().message());
      failed.rom_path = hooks.rom_path;
      failed.at_utc = hooks.at_utc;
      const auto noted = hooks.write_feedback(proposal, failed);
      proposal.last_result = absl::StrFormat(
          "Not accepted: %s. No cell was written%s.", result.status().message(),
          noted.ok() ? "" : "; feedback file NOT updated");
      return result.status();
    }
    applied = *result;
  }
  proposal.decision = LiveProposalDecision::kAccepted;
  proposal.applied_cells = applied > 0 ? classification->ready_cells
                                       : std::vector<GuardedTileCell>{};
  proposal.undone = false;
  proposal.last_result = absl::StrFormat(
      "Accepted: %s written (one undo step), %s skipped",
      CountLabel(applied, "cell", "cells"),
      CountLabel(classification->conflicts, "conflict", "conflicts"));
  return applied;
}

absl::Status RejectLiveProposalWith(LiveProposal& proposal, std::string comment,
                                    const LiveProposalHooks& hooks) {
  if (proposal.decision != LiveProposalDecision::kPending)
    return absl::FailedPreconditionError("Proposal is already decided");
  LiveProposal decided = proposal;
  decided.decision = LiveProposalDecision::kRejected;
  LiveProposalFeedbackEntry entry;
  entry.decision = "rejected";
  entry.comment = std::move(comment);
  entry.rom_path = hooks.rom_path;
  entry.at_utc = hooks.at_utc;
  if (const auto written = hooks.write_feedback(decided, entry);
      !written.ok()) {
    proposal.last_result = absl::StrFormat(
        "Not rejected: feedback file could not be written (%s). Reject again "
        "to retry.",
        written.message());
    return written;
  }
  proposal.decision = LiveProposalDecision::kRejected;
  proposal.last_result = "Rejected; feedback written";
  return absl::OkStatus();
}

absl::Status CommentLiveProposalWith(LiveProposal& proposal,
                                     std::string comment,
                                     const LiveProposalHooks& hooks) {
  if (comment.empty())
    return absl::InvalidArgumentError("Comment is empty");
  LiveProposalFeedbackEntry entry;
  entry.decision = "commented";
  entry.comment = std::move(comment);
  entry.rom_path = hooks.rom_path;
  entry.at_utc = hooks.at_utc;
  const auto written = hooks.write_feedback(proposal, entry);
  proposal.last_result =
      written.ok()
          ? "Comment sent; proposal stays pending"
          : absl::StrFormat("Comment NOT written (%s)", written.message());
  return written;
}

bool SyncLiveProposalWith(LiveProposal& proposal,
                          const LiveProposalHooks& hooks) {
  if (proposal.applied_cells.empty())
    return false;
  const bool accepted = proposal.decision == LiveProposalDecision::kAccepted;
  const bool undone =
      proposal.undone && proposal.decision == LiveProposalDecision::kPending;
  if (!accepted && !undone)
    return false;
  bool all_expected = true;
  bool all_proposed = true;
  for (const auto& cell : proposal.applied_cells) {
    auto tile = hooks.read(proposal.map_id, cell.x, cell.y);
    if (!tile.ok())
      return false;
    all_expected &= *tile == cell.expected;
    all_proposed &= *tile == cell.proposed;
  }
  const int count = static_cast<int>(proposal.applied_cells.size());
  LiveProposalFeedbackEntry entry;
  if (accepted && all_expected) {
    // Back to pending: the ghost returns and Accept works again.
    proposal.decision = LiveProposalDecision::kPending;
    proposal.undone = true;
    entry.decision = "undone";
  } else if (undone && all_proposed) {
    proposal.decision = LiveProposalDecision::kAccepted;
    proposal.undone = false;
    entry.decision = "redone";
    entry.cells_applied = count;
  } else {
    return false;
  }
  entry.rom_path = hooks.rom_path;
  entry.at_utc = hooks.at_utc;
  const auto written = hooks.write_feedback(proposal, entry);
  proposal.last_result = absl::StrFormat(
      "%s: %s %s%s", accepted ? "Undone" : "Redone",
      CountLabel(count, "cell", "cells"),
      accepted ? "restored" : "written again",
      written.ok() ? ""
                   : absl::StrFormat(" — feedback file NOT written (%s)",
                                     written.message()));
  return true;
}

int LiveProposalInbox::Offer(LiveProposal proposal) {
  proposal.decision = LiveProposalDecision::kPending;
  proposal.offer_serial = next_offer_serial_++;
  for (size_t i = 0; i < proposals_.size(); ++i) {
    if (proposals_[i].id == proposal.id) {
      proposals_[i] = std::move(proposal);
      selected_ = static_cast<int>(i);
      return selected_;
    }
  }
  proposals_.push_back(std::move(proposal));
  selected_ = static_cast<int>(proposals_.size()) - 1;
  return selected_;
}

void LiveProposalInbox::Clear() {
  proposals_.clear();
  selected_ = -1;
}

LiveProposal* LiveProposalInbox::Find(std::string_view id) {
  for (auto& proposal : proposals_) {
    if (proposal.id == id)
      return &proposal;
  }
  return nullptr;
}

LiveProposal* LiveProposalInbox::Selected() {
  if (selected_ < 0 || selected_ >= static_cast<int>(proposals_.size()))
    return nullptr;
  return &proposals_[selected_];
}

int LiveProposalInbox::PendingCount() const {
  int count = 0;
  for (const auto& proposal : proposals_) {
    if (proposal.decision == LiveProposalDecision::kPending)
      ++count;
  }
  return count;
}

void LiveProposalInbox::Select(int index) {
  if (index >= 0 && index < static_cast<int>(proposals_.size()))
    selected_ = index;
}

}  // namespace yaze::editor
