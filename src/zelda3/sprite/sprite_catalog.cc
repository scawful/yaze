#include "zelda3/sprite/sprite_catalog.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include "absl/status/status.h"
#include "absl/strings/strip.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3 {
namespace {
using nlohmann::json;
constexpr size_t kMaxDocumentBytes = 1024 * 1024;

absl::StatusOr<std::string> ReadDocument(const std::string& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) {
    return absl::NotFoundError("Source file is unavailable: " + path);
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return absl::NotFoundError("Cannot open source file: " + path);
  std::string text(kMaxDocumentBytes + 1, '\0');
  stream.read(text.data(), text.size());
  text.resize(static_cast<size_t>(stream.gcount()));
  if (stream.bad())
    return absl::DataLossError("Cannot read source file: " + path);
  if (text.size() > kMaxDocumentBytes) {
    return absl::ResourceExhaustedError("Source file exceeds 1 MiB: " + path);
  }
  return text;
}

bool SafeRelativePath(const std::string& value) {
  if (value.empty() || value.find('\0') != std::string::npos ||
      value.find('\\') != std::string::npos ||
      value.find(':') != std::string::npos)
    return false;
  const std::filesystem::path path(value);
  if (path.is_absolute() || path.has_root_name())
    return false;
  for (const auto& part : path) {
    if (part == "..")
      return false;
  }
  return true;
}

bool IsIdentifier(const std::string& value) {
  return !value.empty() && value.find_first_not_of(
                               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU"
                               "VWXYZ0123456789_.-") == std::string::npos;
}

// Unknown fields are rejected so an unsupported selector cannot silently degrade
// into the v1 exact-value/fallback semantics. The catalog has no save operation.
void CheckFields(const json& object,
                 std::initializer_list<const char*> fields) {
  if (!object.is_object())
    throw std::runtime_error("Expected an object");
  for (const auto& item : object.items()) {
    if (std::none_of(fields.begin(), fields.end(),
                     [&](const char* field) { return item.key() == field; })) {
      throw std::runtime_error("Unsupported field: " + item.key());
    }
  }
}

int ReadInteger(const json& value, int min, int max) {
  if (!value.is_number_integer() || value < min || value > max) {
    throw std::runtime_error("Integer outside supported range");
  }
  return value.get<int>();
}

std::string ReadString(const json& object, const char* key, bool identifier) {
  auto value = object.at(key).get<std::string>();
  if (value.empty() || value.find('\0') != std::string::npos ||
      (identifier && !IsIdentifier(value))) {
    throw std::runtime_error(std::string("Invalid ") + key);
  }
  return value;
}

std::vector<SpriteSourceBinding> ReadBindings(const json& array) {
  if (!array.is_array() || array.empty()) {
    throw std::runtime_error("Source bindings must be a nonempty array");
  }
  std::set<std::string> roles;
  std::vector<SpriteSourceBinding> bindings;
  for (const auto& source : array) {
    CheckFields(source, {"role", "path", "label", "adapter"});
    SpriteSourceBinding binding{ReadString(source, "role", true),
                                ReadString(source, "path", false),
                                ReadString(source, "label", true)};
    if (source.contains("adapter")) {
      binding.adapter = source.at("adapter").get<std::string>();
      if (binding.role != "draw" || (binding.adapter != "literal_v1" &&
                                     binding.adapter != "oracle_maple_v1"))
        throw std::runtime_error("Unsupported draw adapter");
    }
    if (!SafeRelativePath(binding.path) || !roles.insert(binding.role).second) {
      throw std::runtime_error("Unsafe source path or duplicate source role");
    }
    bindings.push_back(std::move(binding));
  }
  return bindings;
}
}  // namespace

absl::StatusOr<SpriteCatalog> SpriteCatalog::Parse(absl::string_view text) {
  if (text.size() > kMaxDocumentBytes) {
    return absl::ResourceExhaustedError("Catalog exceeds 1 MiB");
  }
  try {
    std::vector<std::set<std::string>> object_keys;
    const auto root = json::parse(
        text.begin(), text.end(),
        [&](int depth, json::parse_event_t event, json& parsed) {
          if (depth > 32)
            throw std::runtime_error("Catalog nesting exceeds limit");
          if (event == json::parse_event_t::object_start) {
            object_keys.emplace_back();
          } else if (event == json::parse_event_t::key) {
            if (!object_keys.back().insert(parsed.get<std::string>()).second)
              throw std::runtime_error("Duplicate JSON field");
          } else if (event == json::parse_event_t::object_end) {
            object_keys.pop_back();
          }
          return true;
        });
    CheckFields(root, {"schema_version", "profile", "families"});
    if (ReadInteger(root.at("schema_version"), 1, 255) != 1) {
      return absl::UnimplementedError("Unsupported sprite catalog version");
    }
    if (ReadString(root, "profile", true) != "oracle") {
      return absl::UnimplementedError("Unsupported sprite catalog profile");
    }
    const auto& families = root.at("families");
    if (!families.is_array() || families.empty()) {
      return absl::InvalidArgumentError("Catalog requires a families array");
    }
    SpriteCatalog catalog;
    std::set<std::string> keys;
    std::set<int> ids;
    for (const auto& entry : families) {
      CheckFields(
          entry, {"key", "name", "main_id", "selector", "sources", "variants"});
      SpriteCatalogFamily family;
      family.key = ReadString(entry, "key", true);
      family.name = ReadString(entry, "name", false);
      family.main_id = ReadInteger(entry.at("main_id"), 0, 0xF2);
      if (!keys.insert(family.key).second || !ids.insert(family.main_id).second)
        return absl::InvalidArgumentError("Duplicate family key or main ID");
      const auto& selector = entry.at("selector");
      CheckFields(selector, {"kind", "fallback_variant"});
      if (ReadString(selector, "kind", true) != "exact_with_fallback") {
        return absl::UnimplementedError("Unsupported sprite selector");
      }
      family.fallback_variant = ReadString(selector, "fallback_variant", true);
      family.sources = ReadBindings(entry.at("sources"));
      const auto& variants = entry.at("variants");
      if (!variants.is_array() || variants.empty()) {
        return absl::InvalidArgumentError("Family requires variants");
      }
      std::set<int> subtypes;
      bool found_fallback = false;
      for (const auto& variant : variants) {
        CheckFields(variant, {"key", "name", "authored_subtype", "sources"});
        SpriteCatalogVariant parsed;
        parsed.key = ReadString(variant, "key", true);
        parsed.name = ReadString(variant, "name", false);
        parsed.authored_subtype =
            ReadInteger(variant.at("authored_subtype"), 0, 31);
        if (!keys.insert(parsed.key).second ||
            !subtypes.insert(parsed.authored_subtype).second) {
          return absl::InvalidArgumentError("Duplicate variant key or subtype");
        }
        parsed.sources = ReadBindings(variant.at("sources"));
        found_fallback |= parsed.key == family.fallback_variant;
        family.variants.push_back(std::move(parsed));
      }
      if (!found_fallback) {
        return absl::InvalidArgumentError(
            "Fallback must reference this family's variant");
      }
      catalog.families_.push_back(std::move(family));
    }
    return catalog;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid sprite catalog: ") +
                                      error.what());
  }
}

absl::StatusOr<SpriteCatalog> SpriteCatalog::Load(const std::string& path) {
  auto text = ReadDocument(path);
  if (!text.ok())
    return text.status();
  return Parse(*text);
}

const SpriteCatalogFamily* SpriteCatalog::FindFamily(int main_id) const {
  for (const auto& family : families_) {
    if (family.main_id == main_id)
      return &family;
  }
  return nullptr;
}

SpriteCatalogResolution SpriteCatalog::Resolve(int main_id,
                                               int raw_subtype) const {
  SpriteCatalogResolution result;
  result.raw_subtype = raw_subtype;
  const auto* family = FindFamily(main_id);
  if (!family || raw_subtype < 0 || raw_subtype > 255) {
    result.explanation =
        "Unknown family or subtype outside byte range; value preserved.";
    return result;
  }
  for (const auto& variant : family->variants) {
    if (variant.authored_subtype == raw_subtype) {
      result.variant = &variant;
      result.explanation = "Canonical subtype. Catalog browsing only.";
      return result;
    }
  }
  for (const auto& variant : family->variants) {
    if (variant.key == family->fallback_variant) {
      result.variant = &variant;
      result.used_fallback = true;
      result.explanation =
          "Legacy fallback; raw subtype preserved, not normalized.";
      return result;
    }
  }
  return result;
}

absl::StatusOr<SpriteSourceDocument> ReadSpriteSourceFile(
    const std::string& source_root, const std::string& relative_path) {
  if (source_root.empty()) {
    return absl::FailedPreconditionError(
        "Configure sprite_source_root to view source.");
  }
  if (!SafeRelativePath(relative_path)) {
    return absl::InvalidArgumentError("Invalid source binding");
  }
  std::error_code ec;
  const auto root = std::filesystem::canonical(source_root, ec);
  if (ec)
    return absl::NotFoundError("Source root is unavailable: " + source_root);
  const auto path = std::filesystem::canonical(root / relative_path, ec);
  if (ec)
    return absl::NotFoundError("Source file is unavailable: " + relative_path);
  auto root_it = root.begin();
  auto path_it = path.begin();
  for (; root_it != root.end(); ++root_it, ++path_it) {
    if (path_it == path.end() || *root_it != *path_it) {
      return absl::PermissionDeniedError(
          "Source binding escapes configured root");
    }
  }
  auto text = ReadDocument(path.string());
  if (!text.ok())
    return text.status();
  SpriteSourceDocument document;
  document.content = *text;
  document.path = path.string();
  std::istringstream stream(*text);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    document.lines.push_back(line);
  }
  return document;
}

absl::StatusOr<SpriteSourceDocument> ReadSpriteSource(
    const std::string& source_root, const SpriteSourceBinding& binding) {
  if (!IsIdentifier(binding.label))
    return absl::InvalidArgumentError("Invalid source label");
  auto result = ReadSpriteSourceFile(source_root, binding.path);
  if (!result.ok())
    return result.status();
  auto document = std::move(*result);
  for (size_t i = 0; i < document.lines.size(); ++i) {
    const auto& line = document.lines[i];
    auto code = absl::StripAsciiWhitespace(
        absl::string_view(line).substr(0, line.find(';')));
    const auto colon = code.find(':');
    if (colon != absl::string_view::npos &&
        absl::StripAsciiWhitespace(code.substr(0, colon)) == binding.label) {
      if (document.label_line) {
        return absl::FailedPreconditionError("Source label is ambiguous: " +
                                             binding.label);
      }
      document.label_line = static_cast<int>(i + 1);
    }
  }
  if (!document.label_line)
    return absl::NotFoundError("Source label not found: " + binding.label);
  return document;
}
}  // namespace yaze::zelda3
