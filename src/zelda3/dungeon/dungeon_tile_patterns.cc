#include "zelda3/dungeon/dungeon_tile_patterns.h"

#include <filesystem>
#include <fstream>
#include <optional>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "core/source_artifact_publisher.h"

namespace yaze::zelda3 {
namespace {

constexpr size_t kMaxSourceBytes = 65536;
constexpr uint32_t kFirstPatternPc = 0x1B52;
constexpr uint32_t kPatternBankEnd = 0x8000;

std::optional<uint32_t> HexLiteral(absl::string_view text, size_t digits) {
  text = absl::StripAsciiWhitespace(text);
  if (text.size() < 2 || text.size() > digits + 1 || text.front() != '$') {
    return std::nullopt;
  }
  uint32_t value = 0;
  for (char ch : text.substr(1)) {
    if (!absl::ascii_isxdigit(ch)) {
      return std::nullopt;
    }
    value = value * 16 +
            (ch <= '9' ? ch - '0' : absl::ascii_tolower(ch) - 'a' + 10);
  }
  return value;
}

// Semicolons inside an assertion message are text, not assembler comments.
absl::string_view StripComment(absl::string_view line) {
  bool quoted = false;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '"')
      quoted = !quoted;
    if (line[i] == ';' && !quoted)
      return line.substr(0, i);
  }
  return line;
}

std::optional<uint32_t> ProgramCounterAssertion(absl::string_view operand) {
  if (!absl::ConsumePrefix(&operand, "pc()"))
    return std::nullopt;
  operand = absl::StripAsciiWhitespace(operand);
  if (!absl::ConsumePrefix(&operand, "=="))
    return std::nullopt;
  const auto comma = operand.find(',');
  const auto address = HexLiteral(operand.substr(0, comma), 6);
  if (!address || *address < 0x009B52 || *address > 0x00FFFF)
    return std::nullopt;
  if (comma != absl::string_view::npos) {
    auto message = absl::StripAsciiWhitespace(operand.substr(comma + 1));
    if (message.size() < 2 || message.front() != '"' || message.back() != '"')
      return std::nullopt;
    message.remove_prefix(1);
    message.remove_suffix(1);
    if (message.find_first_of("\\\"") != absl::string_view::npos)
      return std::nullopt;
  }
  return address;
}

bool IsLabel(absl::string_view text) {
  if (text.empty() || text.back() != ':')
    return false;
  text.remove_suffix(1);
  if (!text.empty() && text.front() == '#')
    text.remove_prefix(1);
  if (text.empty() ||
      !(absl::ascii_isalpha(text.front()) || text.front() == '_')) {
    return false;
  }
  for (char ch : text) {
    if (!(absl::ascii_isalnum(ch) || ch == '_'))
      return false;
  }
  return true;
}

}  // namespace

absl::StatusOr<DungeonTilePatterns> ParseDungeonTilePatterns(
    absl::string_view source) {
  if (source.size() > kMaxSourceBytes) {
    return absl::ResourceExhaustedError(
        "Dungeon pattern source exceeds 64 KiB");
  }
  DungeonTilePatterns result;
  result.source_text = std::string(source);
  std::optional<uint32_t> cursor;
  int line_number = 0;
  for (absl::string_view line : absl::StrSplit(source, '\n')) {
    ++line_number;
    line = absl::StripAsciiWhitespace(StripComment(line));
    if (line.empty() || line == "lorom" || IsLabel(line))
      continue;
    const auto fail = [&](absl::string_view reason) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Dungeon pattern line %d: %s", line_number, reason));
    };
    const size_t split = line.find_first_of(" \t");
    const auto command = line.substr(0, split);
    const auto operand = split == absl::string_view::npos
                             ? absl::string_view{}
                             : absl::StripAsciiWhitespace(line.substr(split));
    if (command == "org") {
      const auto address = HexLiteral(operand, 6);
      if (!address || *address < 0x009B52 || *address > 0x00FFFE ||
          (*address & 1) != 0) {
        return fail(
            "org must name an aligned bank-00 object-data address "
            "($009B52-$00FFFE)");
      }
      cursor = *address - 0x8000;
    } else if (command == "dw") {
      if (!cursor)
        return fail("dw requires an org first");
      for (absl::string_view token : absl::StrSplit(operand, ',')) {
        const auto word = HexLiteral(token, 4);
        if (!word)
          return fail("dw requires literal $0000-$FFFF words");
        if (*cursor < kFirstPatternPc || *cursor + 2 > kPatternBankEnd) {
          return fail("tile words cross the bank-00 object-data boundary");
        }
        if (!result.words.emplace(*cursor, static_cast<uint16_t>(*word))
                 .second) {
          return fail("tile words overlap an earlier declaration");
        }
        token = absl::StripAsciiWhitespace(token);
        result.tokens.emplace(
            *cursor, DungeonTilePatterns::Token{
                         static_cast<size_t>(token.data() - source.data()),
                         token.size()});
        *cursor += 2;
      }
    } else if (command == "assert") {
      if (!cursor)
        return fail("assert pc() requires an org first");
      const auto address = ProgramCounterAssertion(operand);
      if (!address) {
        return fail(
            "assert requires pc() == $00xxxx with an optional quoted "
            "message; expressions are not supported");
      }
      const uint32_t actual = *cursor + 0x8000;
      if (*address != actual) {
        return fail(absl::StrFormat(
            "assert pc() failed: expected $%06X, current address $%06X",
            *address, actual));
      }
    } else {
      return fail(
          "unsupported statement; only labels, lorom, org, literal dw "
          "and assert pc() == $00xxxx are supported");
    }
  }
  if (result.words.empty()) {
    return absl::InvalidArgumentError(
        "Dungeon pattern source contains no tile words");
  }
  return result;
}

absl::StatusOr<DungeonTilePatterns> LoadDungeonTilePatterns(
    const std::string& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    return absl::NotFoundError("Cannot read dungeon pattern source: " + path);
  if (size > kMaxSourceBytes) {
    return absl::ResourceExhaustedError(
        "Dungeon pattern source exceeds 64 KiB: " + path);
  }
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return absl::NotFoundError("Cannot open dungeon pattern source: " + path);
  // Bound the read even if the file grows after file_size().
  std::string text(kMaxSourceBytes + 1, '\0');
  file.read(text.data(), static_cast<std::streamsize>(text.size()));
  text.resize(static_cast<size_t>(file.gcount()));
  if (file.bad())
    return absl::DataLossError("Cannot read dungeon pattern source: " + path);
  auto result = ParseDungeonTilePatterns(text);
  if (!result.ok()) {
    return absl::Status(result.status().code(),
                        path + ": " + std::string(result.status().message()));
  }
  const auto canonical_path = std::filesystem::canonical(path, error);
  if (error)
    return absl::NotFoundError("Cannot resolve dungeon pattern source: " +
                               path);
  result->source_path = canonical_path.string();
  return result;
}

absl::Status PublishDungeonTilePatterns(
    const DungeonTilePatterns& expected,
    const std::map<uint32_t, uint16_t>& replacements) {
  if (expected.source_path.empty()) {
    return absl::FailedPreconditionError("Pattern source has no file path");
  }
  auto lock = core::AcquireSourceArtifactPublicationLock(
      {expected.source_path},
      {"dungeon tile patterns", "dungeon pattern source"});
  if (!lock.ok())
    return lock.status();
  auto current = LoadDungeonTilePatterns(expected.source_path);
  if (!current.ok())
    return current.status();
  if (current->source_text != expected.source_text) {
    return absl::AbortedError(
        "Wall pattern source changed after opening; reopen before applying");
  }
  std::map<size_t, std::pair<size_t, uint16_t>, std::greater<size_t>> edits;
  for (const auto& [pc, word] : replacements) {
    const auto token = current->tokens.find(pc);
    if (token == current->tokens.end()) {
      return absl::FailedPreconditionError(
          "Tile is not owned by this pattern source");
    }
    edits.emplace(token->second.offset,
                  std::make_pair(token->second.length, word));
  }
  std::string after = current->source_text;
  for (const auto& [offset, edit] : edits) {
    after.replace(offset, edit.first, absl::StrFormat("$%04X", edit.second));
  }
  auto parsed = ParseDungeonTilePatterns(after);
  if (!parsed.ok())
    return parsed.status();
  auto expected_words = current->words;
  for (const auto& [pc, word] : replacements)
    expected_words.at(pc) = word;
  if (parsed->words != expected_words) {
    return absl::DataLossError("Pattern edit changed unrelated tile words");
  }
  return core::PublishSourceArtifacts(
      **lock,
      {{.target = expected.source_path,
        .before = expected.source_text,
        .after = after}},
      core::ComputeSourceArtifactSha256(expected.source_text));
}

}  // namespace yaze::zelda3
