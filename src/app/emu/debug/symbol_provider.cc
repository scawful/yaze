#include "app/emu/debug/symbol_provider.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/strings/match.h"

namespace yaze {
namespace emu {
namespace debug {

namespace {

// Helper to read entire file into string
absl::StatusOr<std::string> ReadFileContent(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Failed to open file: %s", path));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Parse 24-bit hex address from string (e.g., "008034" or "$008034")
std::optional<uint32_t> ParseAddress(const std::string& str) {
  std::string clean = str;
  // Remove $ prefix if present
  if (!clean.empty() && clean[0] == '$') {
    clean = clean.substr(1);
  }
  // Remove 0x prefix if present
  if (clean.size() >= 2 && clean[0] == '0' &&
      (clean[1] == 'x' || clean[1] == 'X')) {
    clean = clean.substr(2);
  }
  // Remove any trailing colon
  if (!clean.empty() && clean.back() == ':') {
    clean.pop_back();
  }

  if (clean.empty() || clean.size() > 6) return std::nullopt;

  try {
    size_t pos;
    uint32_t addr = std::stoul(clean, &pos, 16);
    if (pos != clean.size()) return std::nullopt;
    return addr;
  } catch (...) {
    return std::nullopt;
  }
}

// Check if a string is a valid label name
bool IsValidLabelName(const std::string& name) {
  if (name.empty()) return false;
  // First char must be alpha, underscore, or dot (for local labels)
  char first = name[0];
  if (!std::isalpha(first) && first != '_' && first != '.') return false;
  // Rest must be alphanumeric, underscore, or dot
  for (size_t i = 1; i < name.size(); ++i) {
    char c = name[i];
    if (!std::isalnum(c) && c != '_' && c != '.') return false;
  }
  return true;
}

// Simple wildcard matching (supports * only)
bool WildcardMatch(const std::string& pattern, const std::string& str) {
  size_t p = 0, s = 0;
  size_t starPos = std::string::npos;
  size_t matchPos = 0;

  while (s < str.size()) {
    if (p < pattern.size() && (pattern[p] == str[s] || pattern[p] == '?')) {
      ++p;
      ++s;
    } else if (p < pattern.size() && pattern[p] == '*') {
      starPos = p++;
      matchPos = s;
    } else if (starPos != std::string::npos) {
      p = starPos + 1;
      s = ++matchPos;
    } else {
      return false;
    }
  }

  while (p < pattern.size() && pattern[p] == '*') ++p;
  return p == pattern.size();
}

// Simple path utilities that work on all platforms
std::string GetFilename(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return path;
  return path.substr(pos + 1);
}

std::string GetExtension(const std::string& path) {
  std::string filename = GetFilename(path);
  size_t pos = filename.find_last_of('.');
  if (pos == std::string::npos) return "";
  return filename.substr(pos);
}

}  // namespace

absl::Status SymbolProvider::LoadAsarAsmFile(const std::string& path) {
  auto content_or = ReadFileContent(path);
  if (!content_or.ok()) {
    return content_or.status();
  }

  return ParseAsarAsmContent(*content_or, GetFilename(path));
}

absl::Status SymbolProvider::LoadAsarAsmDirectory(
    const std::string& directory_path) {
#ifdef __EMSCRIPTEN__
  // Directory iteration not supported in WASM builds
  // Use LoadAsarAsmFile with explicit file paths instead
  (void)directory_path;
  return absl::UnimplementedError(
      "Directory loading not supported in browser builds. "
      "Please load individual symbol files.");
#else
  std::filesystem::path dir(directory_path);
  if (!std::filesystem::exists(dir)) {
    return absl::NotFoundError(
        absl::StrFormat("Directory not found: %s", directory_path));
  }

  int files_loaded = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file()) {
      auto ext = entry.path().extension().string();
      if (ext == ".asm" || ext == ".s") {
        auto status = LoadAsarAsmFile(entry.path().string());
        if (status.ok()) {
          ++files_loaded;
        }
      }
    }
  }

  if (files_loaded == 0) {
    return absl::NotFoundError("No ASM files found in directory");
  }

  return absl::OkStatus();
#endif
}

absl::Status SymbolProvider::LoadSymbolFile(const std::string& path,
                                             SymbolFormat format) {
  auto content_or = ReadFileContent(path);
  if (!content_or.ok()) {
    return content_or.status();
  }

  const std::string& content = *content_or;
  std::string ext = GetExtension(path);

  // Auto-detect format if needed
  if (format == SymbolFormat::kAuto) {
    format = DetectFormat(content, ext);
  }

  switch (format) {
    case SymbolFormat::kAsar:
      return ParseAsarAsmContent(content, GetFilename(path));
    case SymbolFormat::kWlaDx:
      return ParseWlaDxSymFile(content);
    case SymbolFormat::kMesen:
      return ParseMesenMlbFile(content);
    case SymbolFormat::kBsnes:
    case SymbolFormat::kNo$snes:
      return ParseBsnesSymFile(content);
    default:
      return absl::InvalidArgumentError("Unknown symbol format");
  }
}

void SymbolProvider::AddSymbol(const Symbol& symbol) {
  symbols_by_address_.emplace(symbol.address, symbol);
  symbols_by_name_[symbol.name] = symbol;
}

void SymbolProvider::AddAsarSymbols(const std::vector<Symbol>& symbols) {
  for (const auto& sym : symbols) {
    AddSymbol(sym);
  }
}

void SymbolProvider::Clear() {
  symbols_by_address_.clear();
  symbols_by_name_.clear();
}

std::string SymbolProvider::GetSymbolName(uint32_t address) const {
  auto it = symbols_by_address_.find(address);
  if (it != symbols_by_address_.end()) {
    return it->second.name;
  }
  return "";
}

std::optional<Symbol> SymbolProvider::GetSymbol(uint32_t address) const {
  auto it = symbols_by_address_.find(address);
  if (it != symbols_by_address_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<Symbol> SymbolProvider::GetSymbolsAtAddress(
    uint32_t address) const {
  std::vector<Symbol> result;
  auto range = symbols_by_address_.equal_range(address);
  for (auto it = range.first; it != range.second; ++it) {
    result.push_back(it->second);
  }
  return result;
}

std::optional<Symbol> SymbolProvider::FindSymbol(
    const std::string& name) const {
  auto it = symbols_by_name_.find(name);
  if (it != symbols_by_name_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<Symbol> SymbolProvider::FindSymbolsMatching(
    const std::string& pattern) const {
  std::vector<Symbol> result;
  for (const auto& [name, sym] : symbols_by_name_) {
    if (WildcardMatch(pattern, name)) {
      result.push_back(sym);
    }
  }
  return result;
}

std::vector<Symbol> SymbolProvider::GetSymbolsInRange(uint32_t start,
                                                       uint32_t end) const {
  std::vector<Symbol> result;
  auto it_start = symbols_by_address_.lower_bound(start);
  auto it_end = symbols_by_address_.upper_bound(end);
  for (auto it = it_start; it != it_end; ++it) {
    result.push_back(it->second);
  }
  return result;
}

std::optional<Symbol> SymbolProvider::GetNearestSymbol(
    uint32_t address) const {
  if (symbols_by_address_.empty()) return std::nullopt;

  // Find first symbol > address
  auto it = symbols_by_address_.upper_bound(address);

  if (it == symbols_by_address_.begin()) {
    // All symbols are > address, no symbol at or before
    return std::nullopt;
  }

  // Go back to the symbol at or before address
  --it;
  return it->second;
}

std::string SymbolProvider::FormatAddress(uint32_t address,
                                           uint32_t max_offset) const {
  // Check for exact match first
  auto exact = GetSymbol(address);
  if (exact) {
    return exact->name;
  }

  // Check for nearest symbol with offset
  auto nearest = GetNearestSymbol(address);
  if (nearest) {
    uint32_t offset = address - nearest->address;
    if (offset <= max_offset) {
      return absl::StrFormat("%s+$%X", nearest->name, offset);
    }
  }

  // No symbol found, just format as hex
  return absl::StrFormat("$%06X", address);
}

std::function<std::string(uint32_t)> SymbolProvider::CreateResolver() const {
  return [this](uint32_t address) -> std::string {
    return GetSymbolName(address);
  };
}

absl::Status SymbolProvider::ParseAsarAsmContent(const std::string& content,
                                                  const std::string& filename) {
  std::istringstream stream(content);
  std::string line;
  int line_number = 0;

  std::string current_label;  // Current global label (for local label scope)
  uint32_t last_address = 0;

  // Regex patterns for usdasm format
  // Label definition: word followed by colon at start of line
  std::regex label_regex(R"(^([A-Za-z_][A-Za-z0-9_]*):)");
  // Local label: dot followed by word and colon
  std::regex local_label_regex(R"(^(\.[A-Za-z_][A-Za-z0-9_]*))");
  // Address line: #_XXXXXX: instruction
  std::regex address_regex(R"(^#_([0-9A-Fa-f]{6}):)");

  bool pending_label = false;
  std::string pending_label_name;
  bool pending_is_local = false;

  while (std::getline(stream, line)) {
    ++line_number;

    // Skip empty lines and comment-only lines
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));
    if (trimmed.empty() || trimmed[0] == ';') continue;

    std::smatch match;

    // Check for address line
    if (std::regex_search(line, match, address_regex)) {
      auto addr = ParseAddress(match[1].str());
      if (addr) {
        last_address = *addr;

        // If we have a pending label, associate it with this address
        if (pending_label) {
          Symbol sym;
          sym.name = pending_label_name;
          sym.address = *addr;
          sym.file = filename;
          sym.line = line_number;
          sym.is_local = pending_is_local;

          AddSymbol(sym);
          pending_label = false;
        }
      }
    }

    // Check for global label (at start of line, not indented)
    if (line[0] != ' ' && line[0] != '\t' && line[0] != '#') {
      if (std::regex_search(line, match, label_regex)) {
        current_label = match[1].str();
        pending_label = true;
        pending_label_name = current_label;
        pending_is_local = false;
      }
    }

    // Check for local label
    if (std::regex_search(trimmed, match, local_label_regex)) {
      std::string local_name = match[1].str();
      // Create fully qualified name: GlobalLabel.local_name
      std::string full_name = current_label.empty()
                                  ? local_name
                                  : current_label + local_name;
      pending_label = true;
      pending_label_name = full_name;
      pending_is_local = true;
    }
  }

  return absl::OkStatus();
}

absl::Status SymbolProvider::ParseWlaDxSymFile(const std::string& content) {
  // WLA-DX format:
  // [labels]
  // 00:8000 Reset
  // 00:8034 MainGameLoop

  std::istringstream stream(content);
  std::string line;
  bool in_labels_section = false;

  std::regex label_regex(R"(^([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4})\s+(\S+))");

  while (std::getline(stream, line)) {
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));

    if (trimmed == "[labels]") {
      in_labels_section = true;
      continue;
    }
    if (trimmed.empty() || trimmed[0] == '[') {
      if (trimmed[0] == '[') in_labels_section = false;
      continue;
    }

    if (!in_labels_section) continue;

    std::smatch match;
    if (std::regex_search(trimmed, match, label_regex)) {
      uint32_t bank = std::stoul(match[1].str(), nullptr, 16);
      uint32_t offset = std::stoul(match[2].str(), nullptr, 16);
      uint32_t address = (bank << 16) | offset;
      std::string name = match[3].str();

      Symbol sym(name, address);
      AddSymbol(sym);
    }
  }

  return absl::OkStatus();
}

absl::Status SymbolProvider::ParseMesenMlbFile(const std::string& content) {
  // Mesen .mlb format:
  // PRG:address:name
  // or just
  // address:name

  std::istringstream stream(content);
  std::string line;

  std::regex label_regex(R"(^(?:PRG:)?([0-9A-Fa-f]+):(\S+))");

  while (std::getline(stream, line)) {
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));
    if (trimmed.empty() || trimmed[0] == ';') continue;

    std::smatch match;
    if (std::regex_search(trimmed, match, label_regex)) {
      auto addr = ParseAddress(match[1].str());
      if (addr) {
        Symbol sym(match[2].str(), *addr);
        AddSymbol(sym);
      }
    }
  }

  return absl::OkStatus();
}

absl::Status SymbolProvider::ParseBsnesSymFile(const std::string& content) {
  // bsnes/No$snes format:
  // 008000 Reset
  // 008034 MainGameLoop

  std::istringstream stream(content);
  std::string line;

  std::regex label_regex(R"(^([0-9A-Fa-f]{6})\s+(\S+))");

  while (std::getline(stream, line)) {
    std::string trimmed = std::string(absl::StripAsciiWhitespace(line));
    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') continue;

    std::smatch match;
    if (std::regex_search(trimmed, match, label_regex)) {
      auto addr = ParseAddress(match[1].str());
      if (addr) {
        Symbol sym(match[2].str(), *addr);
        AddSymbol(sym);
      }
    }
  }

  return absl::OkStatus();
}

SymbolFormat SymbolProvider::DetectFormat(const std::string& content,
                                           const std::string& extension) const {
  // Check extension first
  if (extension == ".asm" || extension == ".s") {
    return SymbolFormat::kAsar;
  }
  if (extension == ".mlb") {
    return SymbolFormat::kMesen;
  }

  // Check content for format hints
  if (content.find("[labels]") != std::string::npos) {
    return SymbolFormat::kWlaDx;
  }
  if (content.find("PRG:") != std::string::npos) {
    return SymbolFormat::kMesen;
  }
  if (content.find("#_") != std::string::npos) {
    return SymbolFormat::kAsar;
  }

  // Default to bsnes format (most generic)
  return SymbolFormat::kBsnes;
}

}  // namespace debug
}  // namespace emu
}  // namespace yaze
