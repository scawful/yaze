#include "zelda3/dungeon/dungeon_wall_profile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3 {
namespace {

using Json = nlohmann::json;
constexpr uint32_t kFirstPatternPc = 0x1B52;
constexpr uint32_t kPatternBankEnd = 0x8000;

class JsonSourceError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

bool ValidIdentifier(const std::string& value) {
  if (value.empty() || value.size() > 48 || value.front() < 'a' ||
      value.front() > 'z') {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
  });
}

absl::Status CheckObject(const Json& value,
                         std::initializer_list<absl::string_view> allowed,
                         std::initializer_list<absl::string_view> required,
                         const std::string& where) {
  if (!value.is_object()) {
    return absl::InvalidArgumentError(where + " must be an object");
  }
  for (const auto& item : value.items()) {
    if (std::none_of(allowed.begin(), allowed.end(),
                     [&](auto key) { return key == item.key(); })) {
      return absl::InvalidArgumentError(where + ": unknown field " +
                                        item.key());
    }
  }
  for (auto key : required) {
    if (!value.contains(std::string(key))) {
      return absl::InvalidArgumentError(where + ": missing field " +
                                        std::string(key));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<uint64_t> ReadUnsigned(const Json& value, uint64_t maximum,
                                      const std::string& where) {
  if (!value.is_number_unsigned()) {
    return absl::InvalidArgumentError(where +
                                      " must be an unsigned JSON integer");
  }
  const uint64_t number = value.get<uint64_t>();
  if (number > maximum) {
    return absl::InvalidArgumentError(where + " is out of range");
  }
  return number;
}

absl::StatusOr<std::string> ReadIdentifier(const Json& value,
                                           const std::string& where) {
  if (!value.is_string() ||
      !ValidIdentifier(value.get_ref<const std::string&>())) {
    return absl::InvalidArgumentError(where +
                                      " must match [a-z][a-z0-9_]{0,47}");
  }
  return value.get<std::string>();
}

absl::Status CheckArray(const Json& value, size_t maximum,
                        const std::string& where) {
  if (!value.is_array()) {
    return absl::InvalidArgumentError(where + " must be an array");
  }
  if (value.size() > maximum) {
    return absl::ResourceExhaustedError(where + " exceeds its item limit");
  }
  return absl::OkStatus();
}

}  // namespace

DungeonWallProfileCollection::DungeonWallProfileCollection(
    std::vector<DungeonWallProfile> profiles,
    std::vector<DungeonWallProfileBinding> bindings, std::string source_text,
    std::string source_path)
    : profiles_(std::move(profiles)),
      bindings_(std::move(bindings)),
      source_text_(std::move(source_text)),
      source_path_(std::move(source_path)) {}

ResolvedWallProfile::ResolvedWallProfile(std::optional<std::string> profile_id,
                                         DungeonTilePatterns patterns)
    : profile_id_(std::move(profile_id)), patterns_(std::move(patterns)) {}

absl::StatusOr<std::shared_ptr<const DungeonWallProfileCollection>>
ParseDungeonWallProfiles(absl::string_view source) {
  if (source.size() > kWallProfileMaxSourceBytes) {
    return absl::ResourceExhaustedError("Wall profile source exceeds 1 MiB");
  }
  if (source.substr(0, 3) == "\xEF\xBB\xBF") {
    return absl::InvalidArgumentError(
        "Wall profile JSON must not contain a UTF-8 BOM");
  }
  Json root;
  try {
    std::vector<std::set<std::string>> keys;
    const auto callback = [&](int depth, Json::parse_event_t event,
                              Json& parsed) {
      if (depth > 16) {
        throw JsonSourceError(
            "Wall profile JSON nesting exceeds the v1 schema");
      }
      if (event == Json::parse_event_t::object_start) {
        keys.emplace_back();
      } else if (event == Json::parse_event_t::key) {
        const auto& key = parsed.get_ref<const std::string&>();
        if (keys.empty() || !keys.back().insert(key).second) {
          throw JsonSourceError("Duplicate wall profile JSON key: " + key);
        }
      } else if (event == Json::parse_event_t::object_end) {
        keys.pop_back();
      }
      return true;
    };
    root = Json::parse(source.begin(), source.end(), callback);
  } catch (const JsonSourceError& error) {
    return absl::InvalidArgumentError(error.what());
  } catch (const Json::exception& error) {
    return absl::InvalidArgumentError(
        std::string("Invalid wall profile JSON: ") + error.what());
  }

  auto status = CheckObject(root, {"schema_version", "profiles", "bindings"},
                            {"schema_version", "profiles", "bindings"}, "root");
  if (!status.ok())
    return status;
  auto version = ReadUnsigned(root["schema_version"], 1, "schema_version");
  if (!version.ok())
    return version.status();
  if (*version != 1) {
    return absl::InvalidArgumentError(
        "Only wall profile schema_version 1 is supported");
  }
  status = CheckArray(root["profiles"], kWallProfileMaxProfiles, "profiles");
  if (!status.ok())
    return status;
  if (root["profiles"].empty()) {
    return absl::InvalidArgumentError("Profiles must not be empty");
  }
  status = CheckArray(root["bindings"], kWallProfileMaxBindings, "bindings");
  if (!status.ok())
    return status;

  std::vector<DungeonWallProfile> profiles;
  std::set<std::string> ids;
  for (const auto& item : root["profiles"]) {
    status =
        CheckObject(item, {"id", "patterns"}, {"id", "patterns"}, "profile");
    if (!status.ok())
      return status;
    auto id = ReadIdentifier(item["id"], "profile.id");
    if (!id.ok())
      return id.status();
    if (!ids.insert(*id).second) {
      return absl::InvalidArgumentError("Duplicate wall profile id: " + *id);
    }
    status = CheckArray(item["patterns"], kWallProfileMaxPatterns,
                        "profile.patterns");
    if (!status.ok())
      return status;
    if (item["patterns"].empty()) {
      return absl::InvalidArgumentError("Profile patterns must not be empty");
    }
    DungeonWallProfile profile{*id, {}};
    std::set<std::string> names;
    std::set<uint32_t> addresses;
    size_t profile_words = 0;
    for (const auto& entry : item["patterns"]) {
      status = CheckObject(entry, {"name", "pc_offset", "words"},
                           {"name", "pc_offset", "words"}, "pattern");
      if (!status.ok())
        return status;
      auto name = ReadIdentifier(entry["name"], "pattern.name");
      if (!name.ok())
        return name.status();
      if (!names.insert(*name).second) {
        return absl::InvalidArgumentError("Duplicate pattern name: " + *name);
      }
      auto pc = ReadUnsigned(entry["pc_offset"], kPatternBankEnd - 2,
                             "pattern.pc_offset");
      if (!pc.ok())
        return pc.status();
      if (*pc < kFirstPatternPc || (*pc & 1) != 0) {
        return absl::InvalidArgumentError(
            "Pattern PC must be aligned in 0x1B52-0x7FFE");
      }
      status = CheckArray(entry["words"], kWallProfileMaxWordsPerProfile,
                          "pattern.words");
      if (!status.ok())
        return status;
      if (entry["words"].empty()) {
        return absl::InvalidArgumentError("Pattern words must not be empty");
      }
      profile_words += entry["words"].size();
      if (profile_words > kWallProfileMaxWordsPerProfile) {
        return absl::ResourceExhaustedError(
            "Wall profile word budget exceeded");
      }
      if (*pc + entry["words"].size() * 2 > kPatternBankEnd) {
        return absl::InvalidArgumentError(
            "Pattern words cross bank-00 data boundary");
      }
      DungeonWallPattern pattern{*name, static_cast<uint32_t>(*pc), {}};
      for (const auto& word_json : entry["words"]) {
        auto word = ReadUnsigned(word_json, 0xFFFF, "pattern.words[]");
        if (!word.ok())
          return word.status();
        const uint32_t address = pattern.pc_offset + pattern.words.size() * 2;
        if (!addresses.insert(address).second) {
          return absl::InvalidArgumentError(
              "Wall profile pattern spans overlap");
        }
        pattern.words.push_back(static_cast<uint16_t>(*word));
      }
      profile.patterns.push_back(std::move(pattern));
    }
    profiles.push_back(std::move(profile));
  }

  std::vector<DungeonWallProfileBinding> bindings;
  for (const auto& item : root["bindings"]) {
    status = CheckObject(item, {"room", "profile", "world", "entrance"},
                         {"room", "profile"}, "binding");
    if (!status.ok())
      return status;
    auto room = ReadUnsigned(item["room"], 0x127, "binding.room");
    if (!room.ok())
      return room.status();
    auto profile = ReadIdentifier(item["profile"], "binding.profile");
    if (!profile.ok())
      return profile.status();
    if (!ids.contains(*profile)) {
      return absl::InvalidArgumentError("Binding names an unknown profile: " +
                                        *profile);
    }
    DungeonWallProfileBinding binding{static_cast<uint16_t>(*room), *profile,
                                      std::nullopt, std::nullopt};
    if (item.contains("world")) {
      auto world = ReadUnsigned(item["world"], 1, "binding.world");
      if (!world.ok())
        return world.status();
      binding.world = static_cast<uint8_t>(*world);
    }
    if (item.contains("entrance")) {
      auto entrance = ReadUnsigned(item["entrance"], 0x84, "binding.entrance");
      if (!entrance.ok())
        return entrance.status();
      binding.entrance = static_cast<uint16_t>(*entrance);
    }
    const int rank = binding.world.has_value() + binding.entrance.has_value();
    for (const auto& other : bindings) {
      const int other_rank =
          other.world.has_value() + other.entrance.has_value();
      const bool world_overlaps =
          !binding.world || !other.world || binding.world == other.world;
      const bool entrance_overlaps = !binding.entrance || !other.entrance ||
                                     binding.entrance == other.entrance;
      if (other.room == binding.room && rank == other_rank && world_overlaps &&
          entrance_overlaps) {
        return absl::InvalidArgumentError(
            "Ambiguous equal-specificity wall profile bindings");
      }
    }
    bindings.push_back(std::move(binding));
  }
  return std::shared_ptr<const DungeonWallProfileCollection>(
      new DungeonWallProfileCollection(std::move(profiles), std::move(bindings),
                                       std::string(source), ""));
}

absl::StatusOr<std::shared_ptr<const DungeonWallProfileCollection>>
LoadDungeonWallProfiles(const std::string& path) {
  std::error_code error;
  const bool exists = std::filesystem::exists(path, error);
  if (error) {
    return absl::InternalError("Cannot inspect wall profile source: " + path);
  }
  if (!exists)
    return absl::NotFoundError("Wall profile source is missing: " + path);
  const auto size = std::filesystem::file_size(path, error);
  if (error)
    return absl::FailedPreconditionError("Cannot size wall profile source: " +
                                         path);
  if (size > kWallProfileMaxSourceBytes) {
    return absl::ResourceExhaustedError("Wall profile source exceeds 1 MiB: " +
                                        path);
  }
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return absl::PermissionDeniedError("Cannot open wall profile source: " +
                                       path);
  std::string source(kWallProfileMaxSourceBytes + 1, '\0');
  input.read(source.data(), static_cast<std::streamsize>(source.size()));
  source.resize(static_cast<size_t>(input.gcount()));
  if (input.bad())
    return absl::DataLossError("Cannot read wall profile source: " + path);
  auto parsed = ParseDungeonWallProfiles(source);
  if (!parsed.ok())
    return parsed.status();
  const auto canonical = std::filesystem::canonical(path, error);
  if (error)
    return absl::NotFoundError("Cannot resolve wall profile source: " + path);
  return std::shared_ptr<const DungeonWallProfileCollection>(
      new DungeonWallProfileCollection((*parsed)->profiles(),
                                       (*parsed)->bindings(), std::move(source),
                                       canonical.string()));
}

absl::StatusOr<ResolvedWallProfile> ResolveWallProfile(
    const DungeonWallProfileCollection& collection,
    const WallProfileContext& context, const DungeonTilePatterns& legacy) {
  if (context.room < 0 || context.room > 0x127 ||
      (context.world && (*context.world < 0 || *context.world > 1)) ||
      (context.entrance &&
       (*context.entrance < 0 || *context.entrance > 0x84))) {
    return absl::InvalidArgumentError("Wall profile context is out of range");
  }
  const DungeonWallProfileBinding* selected = nullptr;
  int best_specificity = -1;
  bool ambiguous = false;
  for (const auto& binding : collection.bindings()) {
    if (binding.room != context.room ||
        (binding.world &&
         (!context.world || *binding.world != *context.world)) ||
        (binding.entrance &&
         (!context.entrance || *binding.entrance != *context.entrance))) {
      continue;
    }
    const int specificity =
        binding.world.has_value() + binding.entrance.has_value();
    if (specificity > best_specificity) {
      selected = &binding;
      best_specificity = specificity;
      ambiguous = false;
    } else if (specificity == best_specificity) {
      ambiguous = true;
    }
  }
  if (ambiguous) {
    return absl::FailedPreconditionError(
        "Ambiguous equally specific wall profile bindings");
  }
  if (selected == nullptr)
    return ResolvedWallProfile(std::nullopt, legacy);
  const auto profile = std::find_if(
      collection.profiles().begin(), collection.profiles().end(),
      [&](const auto& value) { return value.id == selected->profile; });
  if (profile == collection.profiles().end()) {
    return absl::InternalError(
        "Validated wall profile binding lost its profile");
  }
  DungeonTilePatterns merged = legacy;
  merged.source_path.clear();
  merged.source_text.clear();
  merged.tokens.clear();
  for (const auto& pattern : profile->patterns) {
    for (size_t i = 0; i < pattern.words.size(); ++i) {
      merged.words[pattern.pc_offset + i * 2] = pattern.words[i];
    }
  }
  return ResolvedWallProfile(profile->id, std::move(merged));
}

}  // namespace yaze::zelda3
