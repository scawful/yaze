#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_LIST_QUERY_H_
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_LIST_QUERY_H_

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace yaze {
namespace editor {

// Which bank the message list is showing. Independent of find/replace
// case-sensitivity: the list filter is always a case-insensitive browse.
enum class MessageBankFilter {
  kAll = 0,
  kVanilla = 1,
  kExpanded = 2,
};

inline char AsciiLower(unsigned char c) {
  return static_cast<char>(std::tolower(c));
}

inline bool ContainsCaseInsensitive(std::string_view haystack,
                                    std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > haystack.size()) {
    return false;
  }
  for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      if (AsciiLower(static_cast<unsigned char>(haystack[i + j])) !=
          AsciiLower(static_cast<unsigned char>(needle[j]))) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

inline std::string_view StripHexPrefix(std::string_view query) {
  if (query.size() >= 2 && query[0] == '0' &&
      (query[1] == 'x' || query[1] == 'X')) {
    return query.substr(2);
  }
  if (!query.empty() && (query[0] == '$' || query[0] == '#')) {
    return query.substr(1);
  }
  return query;
}

inline bool HexDigitsContain(uint32_t value, int width,
                             std::string_view query) {
  query = StripHexPrefix(query);
  if (query.empty() || width <= 0 || width >= 16) {
    return false;
  }
  char buf[16];
  static const char* digits = "0123456789ABCDEF";
  uint32_t remaining = value;
  for (int i = width - 1; i >= 0; --i) {
    buf[i] = digits[remaining & 0xF];
    remaining >>= 4;
  }
  return ContainsCaseInsensitive(
      std::string_view(buf, static_cast<size_t>(width)), query);
}

inline bool DecimalContains(int value, std::string_view query) {
  char buf[16];
  const int written = std::snprintf(buf, sizeof(buf), "%d", value);
  if (written <= 0) {
    return false;
  }
  return ContainsCaseInsensitive(
      std::string_view(buf, static_cast<size_t>(written)), query);
}

inline std::string MessageListPreview(std::string_view text, size_t max_chars);

// True when a list row should stay visible for the current bank + text filter.
inline bool MessageListQueryMatches(int display_id, uint32_t address,
                                    std::string_view contents, bool expanded,
                                    MessageBankFilter bank,
                                    std::string_view query) {
  if (bank == MessageBankFilter::kVanilla && expanded) {
    return false;
  }
  if (bank == MessageBankFilter::kExpanded && !expanded) {
    return false;
  }
  if (query.empty()) {
    return true;
  }
  if (ContainsCaseInsensitive(contents, query)) {
    return true;
  }
  // Also search the same normalized prose shown in the list, without its
  // display truncation. Raw matching above keeps control-token searches.
  if (ContainsCaseInsensitive(MessageListPreview(contents, contents.size()),
                              query)) {
    return true;
  }
  if (DecimalContains(display_id, query)) {
    return true;
  }
  const int id_width = display_id > 0xFFFF ? 6 : 4;
  if (HexDigitsContain(static_cast<uint32_t>(display_id), id_width, query)) {
    return true;
  }
  return HexDigitsContain(address, 6, query);
}

inline bool IsWordChar(std::string_view text, size_t index) {
  if (index >= text.size()) {
    return false;
  }
  return std::isalnum(static_cast<unsigned char>(text[index])) != 0;
}

// Next match at or after `start`. Empty needle never matches.
inline std::optional<size_t> FindNextTextMatch(std::string_view text,
                                               std::string_view needle,
                                               size_t start,
                                               bool case_sensitive,
                                               bool whole_word) {
  if (needle.empty() || start > text.size()) {
    return std::nullopt;
  }
  for (size_t i = start; i + needle.size() <= text.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      unsigned char left = static_cast<unsigned char>(text[i + j]);
      unsigned char right = static_cast<unsigned char>(needle[j]);
      if (!case_sensitive) {
        left = static_cast<unsigned char>(AsciiLower(left));
        right = static_cast<unsigned char>(AsciiLower(right));
      }
      if (left != right) {
        match = false;
        break;
      }
    }
    if (!match) {
      continue;
    }
    if (whole_word) {
      const bool start_ok = (i == 0) || !IsWordChar(text, i - 1);
      const bool end_ok = !IsWordChar(text, i + needle.size());
      if (!start_ok || !end_ok) {
        continue;
      }
    }
    return i;
  }
  return std::nullopt;
}

// Last match strictly before `before`. `npos` takes the last match in `text`.
inline std::optional<size_t> FindPreviousTextMatch(std::string_view text,
                                                   std::string_view needle,
                                                   size_t before,
                                                   bool case_sensitive,
                                                   bool whole_word) {
  if (needle.empty() || before == 0) {
    return std::nullopt;
  }
  std::optional<size_t> last;
  size_t from = 0;
  while (auto found = FindNextTextMatch(text, needle, from, case_sensitive,
                                        whole_word)) {
    if (before != std::string::npos && *found >= before) {
      break;
    }
    last = found;
    from = *found + 1;
    if (from > text.size()) {
      break;
    }
  }
  return last;
}

// Formatting commands do not contribute visible text to a list preview.
// Keep placeholders such as [L] and button glyphs: they represent game text.
inline bool IsMessagePreviewControl(std::string_view token) {
  if (token == "1" || token == "2" || token == "3" || token == "V" ||
      token == "K" || token == "CH3" || token == "CH2" || token == "CH2L" ||
      token == "CH2I" || token == "CHI" || token == "IMG" || token == "BANK") {
    return true;
  }
  const size_t colon = token.find(':');
  if (colon == std::string_view::npos) {
    return false;
  }
  const auto name = token.substr(0, colon);
  return name == "S" || name == "W" || name == "P" || name == "SPD" ||
         name == "C" || name == "WT" || name == "SFX";
}

// A compact single line of visible text, with formatting commands removed.
inline std::string MessageListPreview(std::string_view text,
                                      size_t max_chars = 96) {
  if (max_chars == 0) {
    return {};
  }
  std::string line;
  line.reserve(std::min(text.size(), max_chars));
  bool pending_space = false;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '[') {
      const size_t end = text.find(']', i + 1);
      if (end != std::string_view::npos) {
        const auto token = text.substr(i + 1, end - i - 1);
        if (IsMessagePreviewControl(token)) {
          if (token == "1" || token == "2" || token == "3" || token == "V" ||
              token == "K") {
            pending_space = pending_space || !line.empty();
          }
          i = end;
          continue;
        }
      }
    }
    const char c = text[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      pending_space = pending_space || !line.empty();
      continue;
    }
    if (pending_space) {
      line.push_back(' ');
      pending_space = false;
    }
    line.push_back(c);
  }
  if (line.size() > max_chars) {
    size_t end = max_chars;
    while (end > 0 && (static_cast<unsigned char>(line[end]) & 0xC0) == 0x80) {
      --end;
    }
    line.resize(end);
    line.append("...");
  }
  return line;
}

// Prefer an actual multi-line dialogue over menu fragments on first open.
inline bool HasMultilineMessageProse(std::string_view text) {
  int nonempty_lines = 0;
  int line_letters = 0;
  int total_letters = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '[') {
      const size_t end = text.find(']', i + 1);
      if (end != std::string_view::npos) {
        const auto token = text.substr(i + 1, end - i - 1);
        if (token == "2" || token == "3" || token == "V") {
          if (line_letters > 0)
            ++nonempty_lines;
          line_letters = 0;
        }
        i = end;
        continue;
      }
    }
    if (text[i] == '\n' || text[i] == '\r') {
      if (line_letters > 0)
        ++nonempty_lines;
      line_letters = 0;
    } else if (std::isalnum(static_cast<unsigned char>(text[i]))) {
      ++line_letters;
      ++total_letters;
    }
  }
  if (line_letters > 0)
    ++nonempty_lines;
  return nonempty_lines >= 2 && total_letters >= 20;
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_LIST_QUERY_H_
