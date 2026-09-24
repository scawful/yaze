#ifndef YAZE_CORE_SPRITE_ASSET_JSON_H_
#define YAZE_CORE_SPRITE_ASSET_JSON_H_
#include <set>
#include <stdexcept>
#include <vector>

#include "absl/status/statusor.h"
#include "core/sprite_asset.h"
#include "core/sprite_behavior_json.h"
#include "nlohmann/json.hpp"
namespace yaze::project {
inline nlohmann::json SpriteAssetToJson(const SpriteAssetBinding& asset) {
  nlohmann::json rows = nlohmann::json::array();
  for (const auto& row : asset.palette_rows)
    rows.push_back({{"group", row.group}, {"index", row.index}});
  nlohmann::json result = {{"version", 1},
                           {"zsm_path", asset.zsm_path},
                           {"catalog_key", asset.catalog_key},
                           {"draw_adapter", asset.draw_adapter},
                           {"source_path", asset.source_path},
                           {"source_label", asset.source_label},
                           {"source_sha256", asset.source_sha256},
                           {"sheets", asset.sheets},
                           {"palette_rows", rows}};
  if (!asset.behavior.profile.empty()) {
    result["version"] = 2;
    result["behavior"] = SpriteBehaviorToJson(asset.behavior);
  }
  return result;
}

inline absl::StatusOr<SpriteAssetBinding> SpriteAssetFromJson(
    const nlohmann::json& value) {
  auto invalid = [] {
    return absl::InvalidArgumentError("Invalid sprite asset binding v1");
  };
  try {
    if (!value.is_object() || !value.at("version").is_number_integer())
      return invalid();
    const bool behavior_version = value.at("version") == 2;
    if ((!behavior_version && value.at("version") != 1) ||
        value.size() != (behavior_version ? 10 : 9))
      return invalid();
    SpriteAssetBinding asset;
    if (behavior_version) {
      auto behavior = SpriteBehaviorFromJson(value.at("behavior"));
      if (!behavior.ok())
        return behavior.status();
      asset.behavior = std::move(*behavior);
    }
    asset.zsm_path = value.at("zsm_path").get<std::string>();
    asset.catalog_key = value.at("catalog_key").get<std::string>();
    asset.draw_adapter = value.at("draw_adapter").get<std::string>();
    asset.source_path = value.at("source_path").get<std::string>();
    asset.source_label = value.at("source_label").get<std::string>();
    asset.source_sha256 = value.at("source_sha256").get<std::string>();
    if (asset.zsm_path.empty() ||
        asset.zsm_path.find('\0') != std::string::npos ||
        (asset.draw_adapter != "literal_v1" &&
         asset.draw_adapter != "oracle_maple_v1"))
      return invalid();
    if (!asset.source_path.empty()) {
      if (asset.source_label.empty() || asset.source_sha256.size() != 64 ||
          asset.source_sha256.find_first_not_of("0123456789abcdef") !=
              std::string::npos)
        return invalid();
    } else if (!asset.source_label.empty() || !asset.source_sha256.empty()) {
      return invalid();
    }
    const auto& sheets = value.at("sheets");
    const auto& rows = value.at("palette_rows");
    if (!sheets.is_array() || sheets.size() != 8 || !rows.is_array() ||
        rows.size() != 8)
      return invalid();
    for (size_t i = 0; i < 8; ++i) {
      if (!sheets[i].is_number_integer() || sheets[i] < 0 || sheets[i] > 255)
        return invalid();
      asset.sheets[i] = sheets[i].get<int>();
      if (!rows[i].is_object() || rows[i].size() != 2 ||
          !rows[i].at("index").is_number_integer() || rows[i].at("index") < 0 ||
          rows[i].at("index") > 255)
        return invalid();
      auto& row = asset.palette_rows[i];
      row.group = rows[i].at("group").get<std::string>();
      row.index = rows[i].at("index").get<int>();
      if (row.group != "auto" && row.group != "global_sprites" &&
          row.group != "sprites_aux1" && row.group != "sprites_aux2" &&
          row.group != "sprites_aux3")
        return invalid();
    }
    return asset;
  } catch (const nlohmann::json::exception&) {
    return invalid();
  }
}

inline absl::StatusOr<SpriteAssetBinding> ParseSpriteAssetBinding(
    const std::string& text) {
  if (text.size() > 65536)
    return absl::ResourceExhaustedError("Sprite asset binding exceeds 64 KiB");
  try {
    std::vector<std::set<std::string>> keys;
    auto value = nlohmann::json::parse(
        text, [&](int depth, nlohmann::json::parse_event_t event,
                  nlohmann::json& node) {
          if (depth > 16)
            throw std::runtime_error("Sprite asset nesting exceeds limit");
          if (event == nlohmann::json::parse_event_t::object_start)
            keys.emplace_back();
          else if (event == nlohmann::json::parse_event_t::key) {
            if (!keys.back().insert(node.get<std::string>()).second)
              throw std::runtime_error("Duplicate sprite asset field");
          } else if (event == nlohmann::json::parse_event_t::object_end)
            keys.pop_back();
          return true;
        });
    return SpriteAssetFromJson(value);
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(e.what());
  }
}
}  // namespace yaze::project
#endif
