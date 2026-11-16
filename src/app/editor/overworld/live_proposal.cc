#include "app/editor/overworld/live_proposal.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using nlohmann::json;

constexpr int kMapCells = 32;

bool ValidId(std::string_view id) {
  if (id.empty() || id.size() > 64)
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
  return object.at(field).get<int>();
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
      if (seen[y * kMapCells + x]) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Proposal repeats cell (%d,%d)", x, y));
      }
      seen[y * kMapCells + x] = true;
      proposal.cells.push_back({x, y, static_cast<uint16_t>(expected),
                                static_cast<uint16_t>(proposed)});
    }
    return proposal;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Proposal JSON is invalid: %s", error.what()));
  }
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

std::string LiveProposalFeedbackPath(std::string_view packet_path) {
  return (std::filesystem::path(packet_path).parent_path() /
          kLiveProposalFeedbackFile)
      .string();
}

std::string BuildLiveProposalFeedback(const LiveProposal& proposal,
                                      const LiveProposalFeedbackEntry& entry,
                                      std::string_view existing_json) {
  json history = json::array();
  if (!existing_json.empty()) {
    const auto existing = json::parse(existing_json, nullptr, false);
    if (existing.is_object() && existing.value("id", "") == proposal.id &&
        existing.contains("history") && existing.at("history").is_array()) {
      history = existing.at("history");
    }
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
      LiveProposalFeedbackPath(proposal.packet_path);
  std::string existing;
  if (std::ifstream in(target, std::ios::binary); in) {
    existing.assign(std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
  }
  const auto text = BuildLiveProposalFeedback(proposal, entry, existing);
  auto temp = target;
  temp += ".tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << text;
    out.flush();
    if (!out) {
      return absl::UnavailableError(
          absl::StrFormat("Could not write %s", temp.string()));
    }
  }
  std::error_code error;
  std::filesystem::rename(temp, target, error);
  if (error) {
    std::filesystem::remove(temp, error);
    return absl::UnavailableError(
        absl::StrFormat("Could not publish %s", target.string()));
  }
  return absl::OkStatus();
}

int LiveProposalInbox::Offer(LiveProposal proposal) {
  proposal.decision = LiveProposalDecision::kPending;
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
