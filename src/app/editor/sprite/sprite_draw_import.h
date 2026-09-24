#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_DRAW_IMPORT_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_DRAW_IMPORT_H_

#include <array>
#include <cctype>
#include <charconv>

#include <string_view>
#include "core/source_artifact_publisher.h"

#include "app/editor/sprite/sprite_authoring.h"

namespace yaze::editor::sprite_authoring {
// Reads only literal tables in one already-resolved global label scope. This is
// a frame-data copy, not an assembler or a claim about the surrounding driver.
inline absl::StatusOr<std::vector<zsprite::Frame>> ImportDrawTables(
    const std::vector<std::string>& lines, size_t first_line) {
  constexpr std::array<std::string_view, 7> names = {
      ".start_index", ".nbr_of_tiles", ".x_offsets", ".y_offsets",
      ".chr",         ".properties",   ".sizes"};
  std::array<std::vector<int>, 7> values;
  std::array<bool, 7> seen{};
  int active = -1;
  auto trim = [](std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r");
    return begin == std::string_view::npos
               ? std::string_view{}
               : value.substr(begin,
                              value.find_last_not_of(" \t\r") - begin + 1);
  };
  auto invalid = [] {
    return absl::InvalidArgumentError(
        "Unsupported draw tables: need seven unique literal db/dw tables in "
        "one label scope");
  };
  for (size_t i = first_line; i < lines.size(); ++i) {
    auto line = trim(std::string_view(lines[i]).substr(0, lines[i].find(';')));
    if (line.empty() || line == "{")
      continue;
    if (line == "}")
      break;
    if (line.back() == ':' && line.front() != '.')
      break;
    int table = -1;
    for (size_t j = 0; j < names.size(); ++j) {
      if (line == names[j] ||
          (line.back() == ':' && line.substr(0, line.size() - 1) == names[j]))
        table = j;
    }
    if (table >= 0) {
      if (seen[table])
        return invalid();
      seen[table] = true;
      active = table;
      continue;
    }
    if (active < 0)
      continue;  // Skip instructions before the first table.
    if (line.size() < 4 ||
        line.substr(0, 2) != (active == 2 || active == 3 ? "dw" : "db") ||
        (line[2] != ' ' && line[2] != '\t'))
      return invalid();
    line = trim(line.substr(3));
    while (!line.empty()) {
      auto comma = line.find(',');
      auto token = trim(line.substr(0, comma));
      if (token.empty())
        return invalid();
      int base = 10;
      if (token.front() == '$') {
        base = 16;
        token.remove_prefix(1);
      } else if (token.front() == '%') {
        base = 2;
        token.remove_prefix(1);
      }
      int value = 0;
      auto result = std::from_chars(token.data(), token.data() + token.size(),
                                    value, base);
      if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
        return invalid();
      if (active == 2 || active == 3) {
        if (value < -32768 || value > 65535)
          return invalid();
        if (value > 32767)
          value -= 65536;
      } else if (value < 0 || value > 255)
        return invalid();
      values[active].push_back(value);
      if (values[active].size() > 256)
        return invalid();
      if (comma == std::string_view::npos)
        break;
      line = trim(line.substr(comma + 1));
      if (line.empty())
        return invalid();
    }
  }
  for (size_t i = 0; i < names.size(); ++i)
    if (!seen[i] || values[i].empty())
      return invalid();
  if (values[0].size() != values[1].size())
    return invalid();
  const size_t tiles = values[2].size();
  if (tiles > 128)
    return invalid();
  for (size_t i = 3; i < names.size(); ++i)
    if (values[i].size() != tiles)
      return invalid();
  std::vector<zsprite::Frame> frames;
  for (size_t i = 0; i < values[0].size(); ++i) {
    const size_t start = values[0][i], count = values[1][i] + 1;
    if (start + count > tiles)
      return invalid();
    zsprite::Frame frame;
    for (size_t j = start; j < start + count; ++j) {
      if (values[2][j] < -128 || values[2][j] > 127 || values[3][j] < -112 ||
          values[3][j] > 143 || (values[6][j] != 0 && values[6][j] != 2))
        return invalid();
      const int property = values[5][j];
      frame.Tiles.emplace_back(
          values[2][j] + kOriginX, values[3][j] + kOriginY, property & 64,
          property & 128, values[4][j] | ((property & 1) << 8),
          (property >> 1) & 7, values[6][j] == 2, (property >> 4) & 3);
    }
    frames.push_back(std::move(frame));
  }
  return frames;
}

// Explicit adapter for the reviewed Oracle Maple driver. Unlike the generic
// importer, this verifies the complete instruction body before supplying the
// two constants absent from its tables. Code changes require adapter review.
inline absl::StatusOr<std::vector<zsprite::Frame>> ImportMapleDrawTables(
    const std::vector<std::string>& lines, size_t first_line) {
  std::string driver;
  size_t table_start = lines.size();
  for (size_t i = first_line; i < lines.size(); ++i) {
    auto text = lines[i].substr(0, lines[i].find(';'));
    text.erase(std::remove_if(text.begin(), text.end(),
                              [](unsigned char c) { return std::isspace(c); }),
               text.end());
    if (text == ".start_index" || text == ".start_index:") {
      table_start = i;
      break;
    }
    if (text.empty() || text == "{")
      continue;
    if (text == "}")
      break;
    driver += text;
  }
  constexpr const char* kReviewedDriverSha256 =
      "45fc82df667e131b3979473c4c6e9d12c890a7fb1e7589b024405a98b3987e6f";
  if (core::ComputeSourceArtifactSha256(driver) != kReviewedDriverSha256)
    return absl::FailedPreconditionError(
        "Maple draw driver changed; adapter review required");
  std::vector<std::string> tables;
  size_t tile_count = 0;
  bool reading_chr = false;
  for (size_t i = table_start; i < lines.size(); ++i) {
    auto text = lines[i].substr(0, lines[i].find(';'));
    auto begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos)
      continue;
    text = text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
    if (text == "}" || (text.back() == ':' && text.front() != '.'))
      break;
    if (text.front() == '.')
      reading_chr = (text == ".chr" || text == ".chr:");
    else if (reading_chr && text.rfind("db", 0) == 0)
      tile_count += std::count(text.begin(), text.end(), ',') + 1;
    tables.push_back(text);
  }
  if (tile_count == 0 || tile_count > 128)
    return absl::InvalidArgumentError("Invalid Maple tile count");
  std::string xs = "dw 0", sizes = "db 2";
  for (size_t i = 1; i < tile_count; ++i) {
    xs += ", 0";
    sizes += ", 2";
  }
  tables.insert(tables.end(), {".x_offsets", xs, ".sizes", sizes});
  return ImportDrawTables(tables, 0);
}

inline absl::StatusOr<std::vector<zsprite::Frame>> ImportDrawAsset(
    const std::vector<std::string>& lines, size_t first_line,
    const std::string& adapter) {
  if (adapter == "literal_v1")
    return ImportDrawTables(lines, first_line);
  if (adapter == "oracle_maple_v1")
    return ImportMapleDrawTables(lines, first_line);
  return absl::InvalidArgumentError("Unsupported draw adapter");
}
}  // namespace yaze::editor::sprite_authoring
#endif
