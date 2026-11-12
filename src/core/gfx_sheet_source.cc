#include "core/gfx_sheet_source.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "app/gfx/types/snes_tile.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "util/rom_hash.h"

namespace yaze::core {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr char kSpec[] = "Data/graphics/source_sprite_sheets.json";
// The producer selects this spec instead when !ENABLE_D4_ZORA_REDESIGN is
// nonzero; it must begin with every kSpec sheet entry.
constexpr char kD4Spec[] = "Data/graphics/d4_zora_sprite_sheets.json";
constexpr char kD4Flag[] = "ENABLE_D4_ZORA_REDESIGN";
absl::Status Bad(const std::string& message) {
  return absl::FailedPreconditionError("Graphics source: " + message);
}
bool Hex(const std::string& value, size_t length) {
  return value.size() == length &&
         std::all_of(value.begin(), value.end(), [](unsigned char c) {
           return std::isdigit(c) || (c >= 'a' && c <= 'f');
         });
}
absl::StatusOr<std::string> Read(const fs::path& path) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec) || ec ||
      fs::file_size(path, ec) > 1024 * 1024 || ec)
    return Bad("missing, unreadable or oversized file: " + path.string());
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return Bad("cannot read " + path.string());
  std::string bytes{std::istreambuf_iterator<char>(input), {}};
  if (input.bad())
    return Bad("incomplete read: " + path.string());
  return bytes;
}
bool Keys(const Json& object, const std::set<std::string>& keys) {
  if (!object.is_object() || object.size() != keys.size())
    return false;
  for (const auto& key : keys)
    if (!object.contains(key))
      return false;
  return true;
}
// Decode before interpreting text so invalid UTF-8 is not silently ignored in
// comments or integer strings. Python's read_text/json.loads also reject it.
uint32_t NextCodepoint(const std::string& text, size_t& offset) {
  const auto first = static_cast<uint8_t>(text.at(offset++));
  if (first < 0x80)
    return first;
  int remaining = first >= 0xC2 && first <= 0xDF   ? 1
                  : first >= 0xE0 && first <= 0xEF ? 2
                  : first >= 0xF0 && first <= 0xF4 ? 3
                                                   : -1;
  if (remaining < 0)
    throw std::runtime_error("invalid UTF-8");
  const int width = remaining + 1;
  uint32_t codepoint = first & (0x7F >> width);
  while (remaining--) {
    if (offset == text.size())
      throw std::runtime_error("truncated UTF-8");
    const auto byte = static_cast<uint8_t>(text[offset++]);
    if ((byte & 0xC0) != 0x80)
      throw std::runtime_error("invalid UTF-8");
    codepoint = (codepoint << 6) | (byte & 0x3F);
  }
  if ((width == 2 && codepoint < 0x80) || (width == 3 && codepoint < 0x800) ||
      (width == 4 && codepoint < 0x10000) || codepoint > 0x10FFFF ||
      (codepoint >= 0xD800 && codepoint <= 0xDFFF))
    throw std::runtime_error("invalid UTF-8");
  return codepoint;
}

std::string IntegerText(const std::string& text) {
  // Python int(str, 0) normalizes Unicode decimal digits and non-ASCII
  // whitespace before parsing. Decimal blocks are from Unicode 17, used by
  // the checked producer runtime; ASCII prefix letters retain their meaning.
  static constexpr uint32_t kDecimalZero[] = {
      0x30,    0x660,   0x6f0,   0x7c0,   0x966,   0x9e6,   0xa66,   0xae6,
      0xb66,   0xbe6,   0xc66,   0xce6,   0xd66,   0xde6,   0xe50,   0xed0,
      0xf20,   0x1040,  0x1090,  0x17e0,  0x1810,  0x1946,  0x19d0,  0x1a80,
      0x1a90,  0x1b50,  0x1bb0,  0x1c40,  0x1c50,  0xa620,  0xa8d0,  0xa900,
      0xa9d0,  0xa9f0,  0xaa50,  0xabf0,  0xff10,  0x104a0, 0x10d30, 0x10d40,
      0x11066, 0x110f0, 0x11136, 0x111d0, 0x112f0, 0x11450, 0x114d0, 0x11650,
      0x116c0, 0x116d0, 0x116da, 0x11730, 0x118e0, 0x11950, 0x11bf0, 0x11c50,
      0x11d50, 0x11da0, 0x11de0, 0x11f50, 0x16130, 0x16a60, 0x16ac0, 0x16b50,
      0x16d70, 0x1ccf0, 0x1d7ce, 0x1d7d8, 0x1d7e2, 0x1d7ec, 0x1d7f6, 0x1e140,
      0x1e2f0, 0x1e4f0, 0x1e5f1, 0x1e950, 0x1fbf0};
  std::string normalized;
  for (size_t offset = 0; offset < text.size();) {
    const uint32_t cp = NextCodepoint(text, offset);
    if (cp < 0x80) {
      normalized += static_cast<char>(cp);
      continue;
    }
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
        (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
        cp == 0x202F || cp == 0x205F || cp == 0x3000) {
      normalized += ' ';
      continue;
    }
    bool decimal = false;
    for (uint32_t zero : kDecimalZero)
      if (cp >= zero && cp < zero + 10) {
        normalized += static_cast<char>('0' + cp - zero);
        decimal = true;
        break;
      }
    if (!decimal)
      throw std::runtime_error("invalid integer character");
  }
  const auto space = [](char c) {
    return c == ' ' || (c >= '\t' && c <= '\r');
  };
  size_t begin = 0, end = normalized.size();
  while (begin < end && space(normalized[begin]))
    ++begin;
  while (end > begin && space(normalized[end - 1]))
    --end;
  return normalized.substr(begin, end - begin);
}

int64_t Number(const Json& value) {
  if (value.is_number_unsigned()) {
    const auto n = value.get<uint64_t>();
    if (n > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
      throw std::runtime_error("integer exceeds source field range");
    return static_cast<int64_t>(n);
  }
  if (value.is_number_integer())
    return value.get<int64_t>();
  if (!value.is_string())
    throw std::runtime_error("expected an integer or hexadecimal string");
  const std::string text = IntegerText(value.get<std::string>());
  size_t offset = 0;
  const bool negative = !text.empty() && text[0] == '-';
  if (!text.empty() && (text[0] == '-' || text[0] == '+'))
    ++offset;
  const size_t digits_begin = offset;
  int base = 10;
  bool prefixed = false;
  if (offset + 1 < text.size() && text[offset] == '0') {
    const char prefix = text[offset + 1];
    if (prefix == 'x' || prefix == 'X')
      base = 16;
    else if (prefix == 'o' || prefix == 'O')
      base = 8;
    else if (prefix == 'b' || prefix == 'B')
      base = 2;
    if (base != 10) {
      prefixed = true;
      offset += 2;
      if (offset < text.size() && text[offset] == '_')
        ++offset;
    }
  }
  const auto digit = [base](char c) {
    const int d = c >= '0' && c <= '9'   ? c - '0'
                  : c >= 'a' && c <= 'f' ? c - 'a' + 10
                  : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                         : -1;
    return d >= 0 && d < base ? d : -1;
  };
  uint64_t result = 0;
  const uint64_t limit =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + negative;
  bool previous_digit = false;
  for (; offset < text.size(); ++offset) {
    if (text[offset] == '_') {
      if (!previous_digit || offset + 1 == text.size() ||
          digit(text[offset + 1]) < 0)
        throw std::runtime_error("invalid integer separator");
      previous_digit = false;
      continue;
    }
    const int d = digit(text[offset]);
    if (d < 0 || result > (limit - d) / base)
      throw std::runtime_error("invalid or out-of-range integer");
    result = result * base + d;
    previous_digit = true;
  }
  if (!previous_digit ||
      (!prefixed && text[digits_begin] == '0' && result != 0))
    throw std::runtime_error("invalid base-0 integer");
  if (negative && result == limit)
    return std::numeric_limits<int64_t>::min();
  return negative ? -static_cast<int64_t>(result)
                  : static_cast<int64_t>(result);
}
absl::StatusOr<std::string> ReplaceTextPixels(
    const std::string& before, const std::vector<uint8_t>& pixels) {
  std::string after = before;
  size_t pixel = 0;
  const auto replace = [&](size_t begin, size_t end) -> absl::Status {
    if (begin == end || before[begin] == '#')
      return absl::OkStatus();
    if (end - begin != 128 || pixel + 128 > pixels.size())
      return Bad("source text template no longer matches its pixels");
    for (size_t index = begin; index < end; ++index, ++pixel) {
      const char original = before[index];
      if (original != '.' && (original < '0' || original > '7'))
        return Bad("source text template has invalid pixel characters");
      const uint8_t previous = original == '.' ? 0 : original - '0';
      if (pixels[pixel] != previous)
        after[index] =
            pixels[pixel] == 0 ? '.' : static_cast<char>('0' + pixels[pixel]);
    }
    return absl::OkStatus();
  };
  try {
    size_t line_begin = 0;
    for (size_t offset = 0; offset < before.size();) {
      const size_t character = offset;
      const auto cp = NextCodepoint(before, offset);
      if (cp == '\n' || cp == '\r' || cp == '\v' || cp == '\f' ||
          (cp >= 0x1C && cp <= 0x1E) || cp == 0x85 || cp == 0x2028 ||
          cp == 0x2029) {
        RETURN_IF_ERROR(replace(line_begin, character));
        if (cp == '\r' && offset < before.size() && before[offset] == '\n')
          ++offset;
        line_begin = offset;
      }
    }
    RETURN_IF_ERROR(replace(line_begin, before.size()));
  } catch (const std::exception& error) {
    return Bad(error.what());
  }
  if (pixel != pixels.size())
    return Bad("source text template has the wrong number of rows");
  return after;
}

absl::StatusOr<fs::path> Root(const project::YazeProject& project) {
  if (project.code_folder.empty())
    return Bad("select the actual code folder");
  std::error_code ec;
  auto root = fs::canonical(project.GetAbsolutePath(project.code_folder), ec);
  if (ec || !fs::is_directory(root))
    return Bad("code folder is unavailable");
  return root;
}

bool FlagName(const std::string& name) {
  constexpr std::string_view kPrefix = "ENABLE_";
  return name.size() > kPrefix.size() && name.starts_with(kPrefix) &&
         std::all_of(name.begin(), name.end(), [](unsigned char c) {
           return std::isupper(c) || std::isdigit(c) || c == '_';
         });
}

// The define files the producer reads before code, in include order. The
// last assignment wins; nullopt marks a value that is not a plain integer.
absl::StatusOr<std::map<std::string, std::optional<int64_t>>> ReadFlags(
    const fs::path& root, const std::set<std::string>& names,
    std::map<std::string, std::optional<std::string>>& files) {
  std::map<std::string, std::optional<int64_t>> values;
  const auto trim = [](const std::string& text) {
    const auto begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos)
      return std::string();
    return text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
  };
  for (const char* name : {"Util/macros.asm", "Config/module_flags.asm",
                           "Config/feature_flags.asm"}) {
    const auto path = root / name;
    std::error_code ec;
    if (!fs::exists(path, ec) && !ec) {
      files.emplace(path.string(), std::nullopt);
      continue;
    }
    ASSIGN_OR_RETURN(auto text, Read(path));
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
      line = trim(line.substr(0, line.find(';')));
      if (line.empty() || line[0] != '!')
        continue;
      const auto end = line.find_first_not_of(
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_", 1);
      const auto define =
          line.substr(1, end == std::string::npos ? end : end - 1);
      if (!names.contains(define))
        continue;
      const auto rest = trim(end == std::string::npos ? "" : line.substr(end));
      if (rest.empty() || rest[0] != '=')
        return Bad("unsupported assignment of !" + define + " in " + name);
      const auto value = trim(rest.substr(1));
      const bool hex = !value.empty() && value[0] == '$';
      const auto digits = hex ? value.substr(1) : value;
      if (digits.empty() || digits.size() > 8 ||
          !std::all_of(digits.begin(), digits.end(), [hex](unsigned char c) {
            return hex ? std::isxdigit(c) : std::isdigit(c);
          })) {
        values[define] = std::nullopt;
        continue;
      }
      values[define] = std::stoll(digits, nullptr, hex ? 16 : 10);
    }
    files.emplace(path.string(), std::move(text));
  }
  return values;
}
bool SupportedHeader(const Json& spec) {
  return Keys(spec,
              {"schema_version", "start", "capacity", "encoding", "sheets"}) &&
         spec.at("schema_version").is_number_integer() &&
         spec.at("schema_version") == 2 &&
         Number(spec.at("start")) == 0x40D000 &&
         Number(spec.at("capacity")) == 0x3000 &&
         (spec.at("encoding") == "stored" || spec.at("encoding") == "lz2") &&
         spec.at("sheets").is_array() && !spec.at("sheets").empty();
}
bool MatchesSheet(const Json& entry, uint16_t id, const std::string& flag) {
  return entry.contains("global_sheet") &&
         Number(entry.at("global_sheet")) == id &&
         entry.value("flag", std::string()) == flag;
}
}  // namespace

bool GfxSheetSource::Selected(const project::YazeProject& project) {
  return !project.graphics_sheets.source_spec.empty() ||
         !project.graphics_sheets.source_spec_sha1.empty();
}
std::string GfxSheetSource::SpecSha1(const std::string& bytes) {
  return util::ComputeSha1Hex(reinterpret_cast<const uint8_t*>(bytes.data()),
                              bytes.size());
}
std::string GfxSheetSource::DecodedSha256(const std::vector<uint8_t>& pixels) {
  const auto raw = gfx::IndexedToSnesSheet(pixels, 3);
  return ComputeSourceArtifactSha256(
      std::string_view(reinterpret_cast<const char*>(raw.data()), raw.size()));
}
absl::StatusOr<std::vector<uint8_t>> GfxSheetSource::DecodeText(
    const std::string& text) {
  std::vector<uint8_t> pixels;
  std::u32string line;
  const auto append = [&]() -> absl::Status {
    if (line.empty() || line.front() == '#') {
      line.clear();
      return absl::OkStatus();
    }
    if (line.size() != 128 || pixels.size() >= 4096)
      return Bad("text sheet needs 128 columns and 32 rows");
    for (char32_t c : line) {
      if (c != '.' && (c < '0' || c > '7'))
        return Bad("text sheet indices must be 0..7");
      pixels.push_back(c == '.' ? 0 : c - '0');
    }
    line.clear();
    return absl::OkStatus();
  };
  try {
    for (size_t offset = 0; offset < text.size();) {
      const uint32_t cp = NextCodepoint(text, offset);
      // Match str.splitlines(), including CRLF as one boundary. Only truly
      // empty lines and column-zero comments are ignored by sheet_codec.
      if (cp == '\n' || cp == '\r' || cp == '\v' || cp == '\f' ||
          (cp >= 0x1C && cp <= 0x1E) || cp == 0x85 || cp == 0x2028 ||
          cp == 0x2029) {
        if (cp == '\r' && offset < text.size() && text[offset] == '\n')
          ++offset;
        RETURN_IF_ERROR(append());
      } else
        line += static_cast<char32_t>(cp);
    }
    RETURN_IF_ERROR(append());
  } catch (const std::exception& error) {
    return Bad(error.what());
  }
  if (pixels.size() != 4096)
    return Bad("text sheet needs 128 columns and 32 rows");
  return pixels;
}
std::string GfxSheetSource::EncodeText(const std::vector<uint8_t>& pixels) {
  std::string text;
  text.reserve(4128);
  for (size_t i = 0; i < pixels.size(); ++i) {
    text += pixels[i] == 0 ? '.' : static_cast<char>('0' + pixels[i]);
    if (i % 128 == 127)
      text += '\n';
  }
  return text;
}
absl::StatusOr<GfxSheetSource> GfxSheetSource::Load(
    const project::YazeProject& project) {
  const auto& selected = project.graphics_sheets.source_spec;
  const bool d4 = selected == kD4Spec;
  if ((selected != kSpec && !d4) ||
      !Hex(project.graphics_sheets.source_spec_sha1, 40))
    return Bad("select a canonical source_spec and its current SHA1");
  ASSIGN_OR_RETURN(auto root, Root(project));
  const auto expected = root / selected;
  std::error_code ec;
  auto spec_path = fs::canonical(expected, ec);
  if (ec || spec_path != expected)
    return Bad("canonical spec must not be redirected");
  ASSIGN_OR_RETURN(auto bytes, Read(spec_path));
  if (SpecSha1(bytes) != project.graphics_sheets.source_spec_sha1)
    return Bad("current spec SHA1 mismatch; draft retained");
  ASSIGN_OR_RETURN(auto descriptor, Read(project.filepath));
  GfxSheetSource result;
  result.descriptor_ = std::move(descriptor);
  result.root_ = root.string();
  result.spec_path_ = spec_path.string();
  result.spec_ = bytes;
  result.spec_sha1_ = SpecSha1(bytes);
  const auto alternate_expected = root / (d4 ? kSpec : kD4Spec);
  result.alternate_spec_path_ = alternate_expected.string();
  const bool alternate_exists = fs::exists(alternate_expected, ec);
  if (ec)
    return Bad("alternate spec is unreadable");
  if (d4 || alternate_exists) {
    const auto alternate_path = fs::canonical(alternate_expected, ec);
    if (ec || alternate_path != alternate_expected)
      return Bad("canonical spec must not be redirected");
    ASSIGN_OR_RETURN(auto alternate, Read(alternate_path));
    result.alternate_spec_ = std::move(alternate);
  }
  try {
    const auto spec = Json::parse(bytes);
    if (!SupportedHeader(spec))
      return Bad("unsupported schema2 sheet spec");
    if (result.alternate_spec_) {
      const auto alternate = Json::parse(*result.alternate_spec_);
      if (!SupportedHeader(alternate))
        return Bad("unsupported schema2 alternate sheet spec");
      const auto& standard = d4 ? alternate : spec;
      const auto& extended = d4 ? spec : alternate;
      const auto& prefix = standard.at("sheets");
      if (prefix.size() > extended.at("sheets").size() ||
          !std::equal(prefix.begin(), prefix.end(),
                      extended.at("sheets").begin()))
        return Bad("D4 sheet spec must preserve every standard sheet entry");
    }
    // A flagged global assembles only under its define and then replaces the
    // unflagged entry for that ID (Scripts/Generate/sheet_overrides.py).
    std::set<std::pair<int, std::string>> ids;
    std::map<uint16_t, Sheet> unflagged, flagged;
    std::vector<fs::path> sources;
    for (const auto& entry : spec.at("sheets")) {
      const bool global = entry.contains("global_sheet");
      auto keys = global
                      ? std::set<std::string>{"global_sheet", "source",
                                              "decoded_sha256", "why"}
                      : std::set<std::string>{"sprite_sheet", "source",
                                              "base_sha256", "identity", "why"};
      if (global && entry.contains("flag"))
        keys.insert("flag");
      if (!Keys(entry, keys))
        return Bad("invalid sheet entry keys");
      std::string flag;
      if (keys.contains("flag")) {
        if (!entry.at("flag").is_string() ||
            !FlagName(flag = entry.at("flag").get<std::string>()))
          return Bad("flag must name an ENABLE_* define");
      }
      const auto source_id =
          Number(entry.at(global ? "global_sheet" : "sprite_sheet"));
      if ((global && (source_id < 0xDF || source_id > 0xFE)) ||
          (!global && (source_id < 0x0C || source_id > 0x66)))
        return Bad("unsupported logical sheet ID");
      // Bounds precede conversion/addition: JSON integers must never wrap to
      // another logical ID (Python's integers do not narrow).
      const int id = static_cast<int>(source_id) + (global ? 0 : 0x73);
      if (!ids.insert({id, flag}).second)
        return Bad("duplicate logical sheet ID");
      if (!flag.empty() && flagged.contains(id))
        return Bad("only one flagged variant per global sheet");
      const auto why = entry.at("why").get<std::string>();
      if (why.find_first_of("\r\n") != std::string::npos)
        return Bad("why must be one line");
      const auto digest = entry.at(global ? "decoded_sha256" : "base_sha256")
                              .get<std::string>();
      if (!Hex(digest, 64) || (!global && !entry.at("identity").is_boolean()))
        return Bad("invalid decoded hash or legacy identity");
      const auto name = entry.at("source").get<std::string>();
      const fs::path relative(name);
      if (name.empty() || name.find('\0') != std::string::npos ||
          relative.is_absolute())
        return Bad("invalid source path");
      for (const auto& part : relative)
        if (part == "..")
          return Bad("source traversal refused");
      auto path = fs::canonical(spec_path.parent_path() / relative, ec);
      if (ec || path.lexically_relative(spec_path.parent_path()).empty() ||
          *path.lexically_relative(spec_path.parent_path()).begin() == "..")
        return Bad("source escapes its spec directory or is missing");
      for (const auto& other : sources) {
        if (fs::equivalent(path, other, ec) || ec)
          return Bad("shared source alias refused");
      }
      if (fs::equivalent(path, spec_path, ec) || ec)
        return Bad("source aliases spec");
      sources.push_back(path);
      ASSIGN_OR_RETURN(auto input_bytes, Read(path));
      result.inputs_.emplace(path.string(), input_bytes);
      if (!global)
        continue;
      std::string extension = path.extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (extension == ".png")
        return Bad(
            "PNG source publication is unsupported; source left unchanged");
      ASSIGN_OR_RETURN(auto source, Read(path));
      ASSIGN_OR_RETURN(auto pixels, DecodeText(source));
      if (DecodedSha256(pixels) != digest)
        return Bad("decoded source SHA256 mismatch");
      (flag.empty() ? unflagged : flagged)
          .emplace(id, Sheet{path.string(), std::move(source),
                             std::move(pixels), flag});
    }
    if (unflagged.empty() && flagged.empty())
      return Bad("no declared DF..FE source sheets");
    std::set<std::string> names{kD4Flag};
    for (const auto& [id, sheet] : flagged)
      names.insert(sheet.flag);
    ASSIGN_OR_RETURN(auto values, ReadFlags(root, names, result.defines_));
    // Like the producer, an undefined D4 flag selects the standard spec.
    const auto d4_value = values.find(kD4Flag);
    if (d4_value != values.end() && !d4_value->second)
      return Bad("!" + std::string(kD4Flag) + " is not a literal integer");
    if ((d4_value != values.end() && *d4_value->second != 0) != d4)
      return Bad("source_spec disagrees with !" + std::string(kD4Flag));
    result.sheets_ = std::move(unflagged);
    for (auto& [id, sheet] : flagged) {
      const auto value = values.find(sheet.flag);
      if (value == values.end())
        return Bad("!" + sheet.flag + " is not defined");
      if (!value->second)
        return Bad("!" + sheet.flag + " is not a literal integer");
      if (*value->second != 0)
        result.sheets_.insert_or_assign(id, std::move(sheet));
    }
  } catch (const std::exception& e) {
    return Bad(e.what());
  }
  return result;
}
absl::Status GfxSheetSource::CheckBinding(
    const project::YazeProject& project) const {
  ASSIGN_OR_RETURN(auto root, Root(project));
  if (root.string() != root_ ||
      root / project.graphics_sheets.source_spec != fs::path(spec_path_) ||
      project.graphics_sheets.source_spec_sha1 != spec_sha1_)
    return Bad("project source binding changed; draft retained");
  return absl::OkStatus();
}
absl::Status GfxSheetSource::CheckDisk() const {
  std::error_code spec_error;
  if (fs::canonical(spec_path_, spec_error) != fs::path(spec_path_) ||
      spec_error)
    return Bad("spec target was redirected");
  ASSIGN_OR_RETURN(auto spec, Read(spec_path_));
  if (spec != spec_)
    return Bad("spec changed on disk; draft retained");
  const bool alternate_exists = fs::exists(alternate_spec_path_, spec_error);
  if (spec_error || alternate_exists != alternate_spec_.has_value())
    return Bad("alternate spec changed on disk; draft retained");
  if (alternate_exists) {
    if (fs::canonical(alternate_spec_path_, spec_error) !=
            fs::path(alternate_spec_path_) ||
        spec_error)
      return Bad("spec target was redirected");
    ASSIGN_OR_RETURN(auto alternate, Read(alternate_spec_path_));
    if (alternate != *alternate_spec_)
      return Bad("alternate spec changed on disk; draft retained");
  }
  for (const auto& [path, before] : inputs_) {
    std::error_code ec;
    if (fs::canonical(path, ec) != fs::path(path) || ec)
      return Bad("source target was redirected");
    ASSIGN_OR_RETURN(auto bytes, Read(path));
    if (bytes != before)
      return Bad("source changed on disk; draft retained");
  }
  for (const auto& [path, before] : defines_) {
    std::error_code ec;
    const bool exists = fs::exists(path, ec);
    if (ec)
      return Bad("feature flags unreadable; draft retained");
    if (exists != before.has_value())
      return Bad("feature flags changed on disk; draft retained");
    if (!exists)
      continue;
    ASSIGN_OR_RETURN(auto bytes, Read(path));
    if (bytes != *before)
      return Bad("feature flags changed on disk; draft retained");
  }
  return absl::OkStatus();
}
absl::StatusOr<GfxSheetSource::Plan> GfxSheetSource::Prepare(
    const project::YazeProject& project,
    const std::map<uint16_t, std::vector<uint8_t>>& pixels) const {
  RETURN_IF_ERROR(CheckBinding(project));
  RETURN_IF_ERROR(CheckDisk());
  Plan plan;
  plan.descriptor = descriptor_;
  plan.sheets = sheets_;
  auto spec = Json::parse(spec_);
  // Shared artwork must retain equal hashes in both specs, whichever is open.
  auto alternate = alternate_spec_ ? Json::parse(*alternate_spec_) : Json();
  bool changed = false, alternate_changed = false;
  for (const auto& [id, edited] : pixels) {
    auto found = plan.sheets.find(id);
    if (found == plan.sheets.end() || edited.size() != 4096 ||
        std::any_of(edited.begin(), edited.end(),
                    [](uint8_t i) { return i > 7; }))
      return Bad("save contains an unbound sheet or invalid indices");
    if (edited != found->second.pixels) {
      changed = true;
      const auto& reserved = project.graphics_sheets.reserved_sheets;
      // Manifest sheet reservations protect compressed ROM storage, including
      // every source-owned ID emitted by the canonical producer. This route
      // writes only the explicitly bound source; user project reservations
      // still protect source edits, and the raw-ROM policy is unchanged.
      if (std::find(reserved.begin(), reserved.end(), id) != reserved.end())
        return Bad("sheet is reserved by the project");
      const auto blocks = project.graphics_sheets.reserved_blocks.find(id);
      if (blocks != project.graphics_sheets.reserved_blocks.end()) {
        for (int block : blocks->second) {
          if (block < 0 || block > 15)
            return Bad("invalid reserved block");
          const int ox = (block % 8) * 16, oy = (block / 8) * 16;
          for (int y = oy; y < oy + 16; ++y)
            for (int x = ox; x < ox + 16; ++x)
              if (edited[y * 128 + x] != found->second.pixels[y * 128 + x])
                return Bad("source edit changes a reserved block");
        }
      }
    }
    auto& sheet = found->second;
    if (sheet.pixels != edited) {
      ASSIGN_OR_RETURN(sheet.bytes, ReplaceTextPixels(sheet.bytes, edited));
    }
    sheet.pixels = edited;
    const auto digest = DecodedSha256(edited);
    for (auto& entry : spec["sheets"])
      if (MatchesSheet(entry, id, sheet.flag))
        entry["decoded_sha256"] = digest;
    if (alternate_spec_)
      for (auto& entry : alternate["sheets"])
        if (MatchesSheet(entry, id, sheet.flag) &&
            entry["decoded_sha256"] != digest) {
          entry["decoded_sha256"] = digest;
          alternate_changed = true;
        }
  }
  plan.spec = changed ? spec.dump(2) + "\n" : spec_;
  plan.sha1 = SpecSha1(plan.spec);
  plan.updates.push_back({spec_path_, spec_, plan.spec});
  if (alternate_spec_)
    plan.updates.push_back(
        {alternate_spec_path_, *alternate_spec_,
         alternate_changed ? alternate.dump(2) + "\n" : *alternate_spec_});
  for (const auto& [path, before] : inputs_) {
    std::string after = before;
    for (const auto& [id, sheet] : plan.sheets)
      if (sheet.path == path)
        after = sheet.bytes;
    plan.updates.push_back({path, before, std::move(after)});
  }
  return plan;
}
void GfxSheetSource::Commit(Plan plan, std::string descriptor) {
  descriptor_ = std::move(descriptor);
  for (const auto& update : plan.updates) {
    const auto target = update.target.string();
    if (target == alternate_spec_path_)
      alternate_spec_ = update.after;
    else if (target != spec_path_)
      inputs_[target] = update.after;
  }
  spec_ = std::move(plan.spec);
  spec_sha1_ = std::move(plan.sha1);
  sheets_ = std::move(plan.sheets);
}
}  // namespace yaze::core
