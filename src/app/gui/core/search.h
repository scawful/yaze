#ifndef YAZE_APP_GUI_CORE_SEARCH_H
#define YAZE_APP_GUI_CORE_SEARCH_H

#include <algorithm>
#include <cctype>
#include <string>

namespace yaze {
namespace gui {

/**
 * @brief Simple fuzzy match - returns true if all chars in pattern appear in
 * str in order
 * @param pattern Search pattern (e.g., "owm" for "Overworld Main")
 * @param str String to search in
 * @return true if pattern matches str in fuzzy manner
 *
 * Examples:
 * - FuzzyMatch("owm", "Overworld Main") -> true
 * - FuzzyMatch("dung", "Dungeon Main") -> true
 * - FuzzyMatch("xyz", "Overworld") -> false
 */
inline bool FuzzyMatch(const std::string& pattern, const std::string& str) {
  if (pattern.empty()) return true;
  if (str.empty()) return false;

  size_t pattern_idx = 0;
  for (char c : str) {
    if (std::tolower(static_cast<unsigned char>(c)) ==
        std::tolower(static_cast<unsigned char>(pattern[pattern_idx]))) {
      pattern_idx++;
      if (pattern_idx >= pattern.length()) return true;
    }
  }
  return false;
}

/**
 * @brief Score a fuzzy match (higher = better, 0 = no match)
 * @param pattern Search pattern
 * @param str String to search in
 * @return Score from 0-100 (100 = exact prefix match, 0 = no match)
 *
 * Scoring:
 * - 100: Exact prefix match (pattern is prefix of str, case-insensitive)
 * - 80: Contains match (pattern appears as substring)
 * - 50: Fuzzy only (all chars appear in order but not as substring)
 * - 0: No match
 */
inline int FuzzyScore(const std::string& pattern, const std::string& str) {
  if (pattern.empty()) return 100;
  if (!FuzzyMatch(pattern, str)) return 0;

  std::string lower_pattern, lower_str;
  lower_pattern.reserve(pattern.size());
  lower_str.reserve(str.size());

  for (char c : pattern)
    lower_pattern += static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
  for (char c : str)
    lower_str += static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));

  if (lower_str.find(lower_pattern) == 0) return 100;    // Exact prefix
  if (lower_str.find(lower_pattern) != std::string::npos) return 80;  // Contains
  return 50;  // Fuzzy only
}

/**
 * @brief Check if a string matches a search filter
 * @param filter The search filter (can be empty for "show all")
 * @param str The string to check
 * @return true if str should be shown given the filter
 */
inline bool PassesFilter(const std::string& filter, const std::string& str) {
  if (filter.empty()) return true;
  return FuzzyMatch(filter, str);
}

/**
 * @brief Check if any of multiple strings match a search filter
 * @param filter The search filter
 * @param strings Vector of strings to check
 * @return true if any string matches the filter
 */
inline bool AnyPassesFilter(const std::string& filter,
                            const std::vector<std::string>& strings) {
  if (filter.empty()) return true;
  for (const auto& str : strings) {
    if (FuzzyMatch(filter, str)) return true;
  }
  return false;
}

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_SEARCH_H
