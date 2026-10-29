#include "core/minecart_catalog_binding.h"

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

namespace yaze::core {
namespace {
namespace fs = std::filesystem;
using zelda3::MinecartRailProfile;

std::string RomHash(const Rom& rom) {
  return ComputeSourceArtifactSha256(
      std::string_view(reinterpret_cast<const char*>(rom.data()), rom.size()));
}

#if defined(YAZE_WITH_JSON)
using Json = nlohmann::json;
constexpr size_t kRecordCount = 37;
constexpr size_t kChangedBytes = 111;

absl::Status Require(bool condition, const char* message) {
  return condition ? absl::OkStatus() : absl::FailedPreconditionError(message);
}

bool IsHash(const Json& value) {
  if (!value.is_string())
    return false;
  const auto& text = value.get_ref<const std::string&>();
  return text.size() == 64 &&
         text.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool IsInteger(const Json& value, uint64_t maximum) {
  return (value.is_number_unsigned() ||
          (value.is_number_integer() && value.get<int64_t>() >= 0)) &&
         value.get<uint64_t>() <= maximum;
}

std::string Hex(std::span<const uint8_t> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : bytes) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}

absl::StatusOr<std::array<uint8_t, 3>> RecordBytes(const Json& value) {
  if (!value.is_string()) {
    return absl::FailedPreconditionError(
        "Invalid minecart migration record bytes");
  }
  const auto& text = value.get_ref<const std::string&>();
  if (text.size() != 6 ||
      text.find_first_not_of("0123456789abcdef") != std::string::npos) {
    return absl::FailedPreconditionError(
        "Invalid minecart migration record bytes");
  }
  const auto nibble = [](char c) {
    return c <= '9' ? c - '0' : c - 'a' + 10;
  };
  std::array<uint8_t, 3> bytes;
  for (size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = static_cast<uint8_t>((nibble(text[i * 2]) << 4) |
                                    nibble(text[i * 2 + 1]));
  }
  return bytes;
}

absl::StatusOr<std::pair<fs::path, std::string>> ReadReceipt(
    const fs::path& path) {
  std::error_code error;
  const auto status = fs::symlink_status(path, error);
  if (error || !fs::is_regular_file(status)) {
    return absl::FailedPreconditionError(
        "Minecart migration receipt must be a regular non-symlink file");
  }
  const auto canonical = fs::canonical(path, error);
  if (error) {
    return absl::FailedPreconditionError(
        "Cannot resolve minecart migration receipt");
  }
  std::ifstream input(canonical, std::ios::binary);
  if (!input.is_open()) {
    return absl::FailedPreconditionError(
        "Cannot read minecart migration receipt");
  }
  constexpr size_t kLimit = 1024 * 1024;
  std::array<char, 16384> buffer;
  std::string text;
  while (input) {
    input.read(buffer.data(), buffer.size());
    const size_t count = static_cast<size_t>(input.gcount());
    if (count > kLimit - text.size()) {
      return absl::FailedPreconditionError(
          "Minecart migration receipt exceeds size limit");
    }
    text.append(buffer.data(), count);
  }
  if (!input.eof() || input.bad()) {
    return absl::DataLossError("Failed reading minecart migration receipt");
  }
  return std::make_pair(canonical, std::move(text));
}

// The immutable dependency must itself describe the fixed migration, even
// after later native Saves no longer match its initial output hash.
absl::Status ValidateMigrationReceipt(const Json& receipt) {
  RETURN_IF_ERROR(Require(
      receipt.is_object() &&
          receipt.value("schema", Json()) ==
              "oracle-minecart-object-migration/v1" &&
          receipt.value("mode", Json()) == "published" &&
          receipt.value("state", Json()) == "intermediate-old-runtime" &&
          receipt.value("record_count", Json()) == kRecordCount &&
          receipt.value("changed_bytes", Json()) == kChangedBytes &&
          receipt.value("rooms_parsed", Json()) == zelda3::kNumberOfRooms &&
          receipt.value("editor_ready", Json()) == false &&
          receipt.value("runtime_ready", Json()) == false &&
          IsHash(receipt.value("input_sha256", Json())) &&
          IsHash(receipt.value("output_sha256", Json())) &&
          IsInteger(receipt.value("bytes", Json()), 0x1000000) &&
          receipt.at("bytes").get<uint64_t>() >= 0x200000 &&
          receipt.contains("records") && receipt["records"].is_array() &&
          receipt["records"].size() == kRecordCount,
      "Invalid published minecart migration receipt"));
  const size_t bytes = receipt["bytes"].get<size_t>();
  std::set<size_t> offsets;
  for (const auto& record : receipt["records"]) {
    RETURN_IF_ERROR(
        Require(record.is_object() &&
                    IsInteger(record.value("pc", Json()), bytes - 3) &&
                    IsInteger(record.value("room", Json()),
                              zelda3::kNumberOfRooms - 1) &&
                    IsInteger(record.value("layer", Json()), 2) &&
                    IsInteger(record.value("index", Json()), bytes / 3),
                "Invalid minecart migration record coordinates"));
    const size_t pos = record["pc"].get<size_t>();
    RETURN_IF_ERROR(Require(offsets.insert(pos).second,
                            "Duplicate minecart migration record offset"));
    const auto current = offsets.find(pos);
    RETURN_IF_ERROR(Require(
        (current == offsets.begin() || pos - *std::prev(current) >= 3) &&
            (std::next(current) == offsets.end() ||
             *std::next(current) - pos >= 3),
        "Overlapping minecart migration records"));
    ASSIGN_OR_RETURN(const auto old, RecordBytes(record.value("old", Json())));
    ASSIGN_OR_RETURN(const auto next, RecordBytes(record.value("new", Json())));
    const int subtype = ((old[0] & 3) << 2) | (old[1] & 3);
    const int target = subtype - 7;
    RETURN_IF_ERROR(Require(
        old[0] < 0xFC && old[2] == 0x31 && subtype >= 12 && subtype <= 15 &&
            next[0] == ((old[0] & 0xFC) | (target >> 2)) &&
            next[1] == ((old[1] & 0xFC) | (target & 3)) && next[2] == 0x32 &&
            record.value("raw", Json()) == record["old"] &&
            record.value("x", Json()) == (old[0] >> 2) &&
            record.value("y", Json()) == (old[1] >> 2) &&
            record.value("old_object", Json()) == 0x31 &&
            record.value("old_subtype", Json()) == subtype &&
            record.value("new_object", Json()) == 0x32 &&
            record.value("new_subtype", Json()) == target &&
            record.value("aliased_rooms", Json()) ==
                Json::array({record["room"]}),
        "Minecart migration record differs from the fixed catalog mapping"));
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> MigrationRecords(const Rom& rom) {
  struct Stream {
    zelda3::DungeonStreamRecord record;
    std::vector<int> rooms;
  };
  std::map<uint32_t, Stream> streams;
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    ASSIGN_OR_RETURN(auto record, zelda3::ReadDungeonObjectStream(rom, room));
    auto& stream = streams[record.data_pc];
    stream.record = std::move(record);
    stream.rooms.push_back(room);
  }
  Json records = Json::array();
  for (const auto& [start, stream] : streams) {
    const auto& bytes = stream.record.encoded_stream;
    for (const int room : stream.rooms) {
      size_t cursor = 2;
      for (int layer = 0; layer < 3; ++layer) {
        size_t index = 0;
        while (cursor + 2 <= bytes.size()) {
          if ((bytes[cursor] == 0xFF && bytes[cursor + 1] == 0xFF) ||
              (layer == 2 && bytes[cursor] == 0xF0 &&
               bytes[cursor + 1] == 0xFF)) {
            cursor += 2;
            break;
          }
          RETURN_IF_ERROR(
              Require(cursor + 3 <= bytes.size(),
                      "Truncated minecart migration object stream"));
          const std::span<const uint8_t> raw(bytes.data() + cursor, 3);
          const int subtype = ((raw[0] & 3) << 2) | (raw[1] & 3);
          if (raw[0] < 0xFC && raw[2] < 0xF8) {
            RETURN_IF_ERROR(
                Require(raw[2] != 0x32 || subtype < 3,
                        "Ambiguous original minecart object32 catalog"));
            if (raw[2] == 0x31 && subtype >= 12) {
              RETURN_IF_ERROR(Require(
                  stream.rooms.size() == 1,
                  "Minecart migration target has aliased room consumers"));
              const int target = subtype - 7;
              const std::array<uint8_t, 3> next = {
                  static_cast<uint8_t>((raw[0] & 0xFC) | (target >> 2)),
                  static_cast<uint8_t>((raw[1] & 0xFC) | (target & 3)), 0x32};
              const auto old = Hex(raw);
              records.push_back({{"room", room},
                                 {"layer", layer},
                                 {"index", index},
                                 {"pc", start + cursor},
                                 {"raw", old},
                                 {"aliased_rooms", stream.rooms},
                                 {"old", old},
                                 {"new", Hex(next)},
                                 {"x", raw[0] >> 2},
                                 {"y", raw[1] >> 2},
                                 {"old_object", 0x31},
                                 {"old_subtype", subtype},
                                 {"new_object", 0x32},
                                 {"new_subtype", target}});
            }
          }
          cursor += 3;
          ++index;
        }
      }
    }
  }
  return records;
}

absl::Status VerifyInitialMigration(const Rom& rom, const Json& receipt) {
  RETURN_IF_ERROR(Require(receipt["bytes"] == rom.size() &&
                              receipt["output_sha256"] == RomHash(rom),
                          "Minecart migration output differs from loaded ROM"));
  auto restored = rom.vector();
  for (const auto& record : receipt["records"]) {
    const size_t pos = record["pc"].get<size_t>();
    ASSIGN_OR_RETURN(const auto next, RecordBytes(record["new"]));
    RETURN_IF_ERROR(
        Require(std::equal(next.begin(), next.end(), restored.begin() + pos),
                "Minecart migration record CAS differs from loaded ROM"));
    ASSIGN_OR_RETURN(const auto old, RecordBytes(record["old"]));
    std::copy(old.begin(), old.end(), restored.begin() + pos);
  }
  RETURN_IF_ERROR(Require(ComputeSourceArtifactSha256(std::string_view(
                              reinterpret_cast<const char*>(restored.data()),
                              restored.size())) == receipt["input_sha256"],
                          "Minecart migration original SHA-256 differs"));
  Rom original;
  RETURN_IF_ERROR(original.LoadFromData(restored));
  ASSIGN_OR_RETURN(const auto records, MigrationRecords(original));
  RETURN_IF_ERROR(
      Require(records.size() == kRecordCount && records == receipt["records"],
              "Minecart migration exhaustive record proof differs"));
  // Parsing the admitted result also checks every unrelated room boundary.
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    RETURN_IF_ERROR(zelda3::ReadDungeonObjectStream(rom, room).status());
  }
  return absl::OkStatus();
}
#endif
}  // namespace

absl::StatusOr<MinecartCatalogBinding> ResolveMinecartCatalogBinding(
    const Rom& rom, const project::YazeProject& project,
    std::optional<NativeSaveContext> native_save_context) {
  const int version = project.dungeon_overlay.minecart_catalog_version;
  if (version != 1 && version != 2) {
    return absl::InvalidArgumentError(
        "Minecart catalog version must be 1 or 2");
  }
  MinecartCatalogBinding binding;
  if (!rom.is_loaded()) {
    if (version == 1) {
      binding.validated = true;
      return binding;
    }
    return absl::FailedPreconditionError(
        "Load the ROM before resolving its minecart catalog");
  }
  binding.rom_sha256 = RomHash(rom);
  binding.profile = zelda3::DetectMinecartRailProfile(rom.vector());
  const auto marker = zelda3::kMinecartRailProfileMarkerPc;
  const bool has_prefix =
      rom.size() >= marker + 4 &&
      std::equal(rom.data() + marker, rom.data() + marker + 4, "MCR2");
  if (has_prefix && binding.profile != MinecartRailProfile::kExpanded) {
    return absl::FailedPreconditionError(
        "Unsupported MCR2 catalog counts/version");
  }
  if (version == 1) {
    binding.validated = true;
    return binding;
  }
#if !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError(
      "Minecart catalog version 2 requires JSON support");
#else
  if (project.minecart_catalog_migration_receipt.empty()) {
    return absl::FailedPreconditionError(
        "Minecart catalog version 2 requires its migration receipt");
  }
  try {
    ASSIGN_OR_RETURN(const auto source,
                     ReadReceipt(fs::u8path(project.GetAbsolutePath(
                         project.minecart_catalog_migration_receipt))));
    const auto receipt = Json::parse(source.second);
    RETURN_IF_ERROR(ValidateMigrationReceipt(receipt));
    binding.migration_receipt_path = source.first;
    binding.migration_receipt_sha256 =
        ComputeSourceArtifactSha256(source.second);
    if (binding.profile == MinecartRailProfile::kExpanded) {
      // The exact runtime marker admits this profile; declared version 2 still
      // carries its immutable dependency into subsequent native Save receipts.
    } else if (receipt["output_sha256"] == binding.rom_sha256) {
      RETURN_IF_ERROR(VerifyInitialMigration(rom, receipt));
    } else {
      std::error_code error;
      const auto descriptor =
          fs::canonical(fs::u8path(project.filepath), error);
      if (error)
        return absl::FailedPreconditionError(
            "Cannot resolve minecart project descriptor");
      const auto rom_path = fs::canonical(fs::u8path(rom.filename()), error);
      if (error)
        return absl::FailedPreconditionError(
            "Cannot resolve minecart editing source");
      const auto configured_rom = fs::canonical(
          fs::u8path(project.GetAbsolutePath(project.rom_filename)), error);
      if (error || configured_rom != rom_path) {
        return absl::FailedPreconditionError(
            "Minecart project refers to another editing source");
      }
      ASSIGN_OR_RETURN(const auto saved_project, ReadReceipt(descriptor));
      RETURN_IF_ERROR(
          project.ValidateSavedSourceBindings(saved_project.second));
      if (native_save_context.has_value() &&
          fs::u8path(native_save_context->project_path) != descriptor) {
        return absl::FailedPreconditionError(
            "Minecart native Save context belongs to another project");
      }
      const auto project_hash =
          ComputeSourceArtifactSha256(saved_project.second);
      ASSIGN_OR_RETURN(
          const auto saved,
          VerifyCurrentNativeSaveReceipt(
              descriptor.string() + ".native-save.json", native_save_context,
              {{"rom", rom_path, binding.rom_sha256},
               {"project", descriptor, project_hash},
               {"dependency.minecart_catalog_migration", source.first,
                binding.migration_receipt_sha256}},
              native_save_context.has_value()
                  ? NativeSaveReceiptVerification::kCurrentArtifacts
                  : NativeSaveReceiptVerification::kPersistedDataAdmission));
      binding.native_save_generation = saved.generation;
    }
    ASSIGN_OR_RETURN(const auto after, ReadReceipt(source.first));
    RETURN_IF_ERROR(
        Require(after.first == source.first && after.second == source.second,
                "Minecart migration receipt changed during admission"));
    binding.profile = MinecartRailProfile::kExpanded;
    binding.validated = true;
    return binding;
  } catch (const std::exception& error) {
    return absl::FailedPreconditionError(
        std::string("Invalid minecart catalog admission: ") + error.what());
  }
#endif
}
}  // namespace yaze::core
