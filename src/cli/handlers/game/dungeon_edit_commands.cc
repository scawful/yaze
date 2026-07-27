#include "cli/handlers/game/dungeon_edit_commands.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/gfx/types/snes_palette.h"
#include "cli/util/hex_util.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/track_collision_generator.h"
#include "zelda3/game_data.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

absl::StatusOr<std::string> GetRequiredString(
    const resources::ArgumentParser& parser, const char* name) {
  auto value = parser.GetString(name);
  if (!value.has_value()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Missing required argument '--%s'", name));
  }
  return *value;
}

absl::StatusOr<int> GetRequiredInt(const resources::ArgumentParser& parser,
                                   const char* name) {
  auto parsed = parser.GetInt(name);
  if (!parsed.ok()) {
    return parsed.status();
  }
  return parsed.value();
}

absl::StatusOr<int> GetOptionalInt(const resources::ArgumentParser& parser,
                                   const char* name, int default_value) {
  if (!parser.GetString(name).has_value()) {
    return default_value;
  }

  auto parsed = parser.GetInt(name);
  if (!parsed.ok()) {
    return parsed.status();
  }
  return parsed.value();
}

absl::Status ValidateRoomId(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Room ID out of range: 0x%X (expected 0x00-0x%02X)",
                        room_id, zelda3::kNumberOfRooms - 1));
  }
  return absl::OkStatus();
}

absl::Status ValidateSpriteCoord(int value, char axis) {
  if (value < 0 || value > 31) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%c must be 0-31 (5-bit tile coord)", axis));
  }
  return absl::OkStatus();
}

absl::Status SaveRomWithBackup(Rom* rom,
                               resources::OutputFormatter& formatter) {
  Rom::SaveSettings save_settings;
  save_settings.require_backup = true;
  auto disk_status = rom->SaveToFile(save_settings);
  if (!disk_status.ok()) {
    formatter.AddField("save_error", std::string(disk_status.message()));
    return disk_status;
  }

  formatter.AddField("save_status", "saved");
  return absl::OkStatus();
}

template <typename Mutation>
absl::Status MutateAndSaveRomWithBackup(Rom* rom,
                                        resources::OutputFormatter& formatter,
                                        Mutation&& mutation) {
  ScopedRomTransaction transaction(*rom);

  auto write_status = std::forward<Mutation>(mutation)();
  if (!write_status.ok()) {
    formatter.AddField("write_error", std::string(write_status.message()));
    return write_status;
  }
  formatter.AddField("write_status", "success");

  RETURN_IF_ERROR(SaveRomWithBackup(rom, formatter));
  transaction.Commit();
  return absl::OkStatus();
}

struct ObjectSaveManifestContext {
  core::HackManifest manifest;
  zelda3::DungeonStreamLayout allocator_layout;
  // A standalone --manifest is a safety capability, not permission to ignore
  // ownership. Unlike a project, the CLI has no separate policy setting, so
  // protected writes are always blocked.
  project::RomWritePolicy write_policy = project::RomWritePolicy::kBlock;
};

absl::StatusOr<std::optional<ObjectSaveManifestContext>>
LoadObjectSaveManifestContext(const resources::ArgumentParser& parser) {
  const auto manifest_path = parser.GetString("manifest");
  if (!manifest_path.has_value()) {
    return std::nullopt;
  }

  ObjectSaveManifestContext context;
  RETURN_IF_ERROR(context.manifest.LoadFromFile(*manifest_path));
  const core::DungeonStreamLayout* manifest_layout =
      context.manifest.GetDungeonStreamLayout(
          core::DungeonStreamType::kObjects);
  if (manifest_layout == nullptr) {
    return absl::FailedPreconditionError(
        "Manifest does not define dungeon_stream_regions.objects");
  }
  if (manifest_layout->strategy != core::DungeonWriteStrategy::kCopyOnWrite) {
    return absl::FailedPreconditionError(
        "dungeon_stream_regions.objects must use copy_on_write");
  }

  ASSIGN_OR_RETURN(context.allocator_layout,
                   core::ToDungeonStreamAllocatorLayout(
                       core::DungeonStreamType::kObjects, *manifest_layout));
  return std::optional<ObjectSaveManifestContext>(std::move(context));
}

absl::Status ValidateObjectSaveManifestConflicts(
    const Rom& source_rom, int room_id, const zelda3::Room& pending_room,
    const ObjectSaveManifestContext& context) {
  std::vector<std::pair<uint32_t, uint32_t>> ranges;

  // Match DungeonEditorV2's conservative object-save prediction: validate the
  // selected in-place stream, the door pointer, every possible COW allocation,
  // and the selected object-pointer slot before exercising either save path.
  uint32_t object_pointer_table_snes = 0;
  ASSIGN_OR_RETURN(object_pointer_table_snes,
                   source_rom.ReadLong(zelda3::kRoomObjectPointer));
  const uint32_t object_pointer_table_pc = SnesToPc(object_pointer_table_snes);
  const uint64_t current_pointer_slot =
      static_cast<uint64_t>(object_pointer_table_pc) +
      static_cast<uint64_t>(room_id) * 3u;
  if (current_pointer_slot + 3u > source_rom.size()) {
    return absl::OutOfRangeError(
        "Selected object pointer slot is outside the ROM");
  }

  uint32_t current_stream_snes = 0;
  ASSIGN_OR_RETURN(current_stream_snes,
                   source_rom.ReadLong(static_cast<int>(current_pointer_slot)));
  const uint32_t current_stream_pc = SnesToPc(current_stream_snes);
  const uint64_t current_stream_end = static_cast<uint64_t>(current_stream_pc) +
                                      pending_room.EncodeObjects().size() + 2u;
  if (current_stream_end > std::numeric_limits<uint32_t>::max()) {
    return absl::OutOfRangeError("Selected object write range overflows");
  }
  ranges.emplace_back(current_stream_pc,
                      static_cast<uint32_t>(current_stream_end));

  const uint64_t door_pointer_slot =
      static_cast<uint64_t>(zelda3::kDoorPointers) +
      static_cast<uint64_t>(room_id) * 3u;
  if (door_pointer_slot + 3u > std::numeric_limits<uint32_t>::max()) {
    return absl::OutOfRangeError("Selected door pointer range overflows");
  }
  ranges.emplace_back(static_cast<uint32_t>(door_pointer_slot),
                      static_cast<uint32_t>(door_pointer_slot + 3u));

  for (const auto& range : context.allocator_layout.allocation_ranges) {
    ranges.emplace_back(range.begin, range.end);
  }
  const uint32_t pointer_width = context.allocator_layout.pointer_encoding ==
                                         zelda3::DungeonPointerEncoding::kLong24
                                     ? 3u
                                     : 2u;
  const uint64_t cow_pointer_slot =
      static_cast<uint64_t>(context.allocator_layout.pointer_table_pc) +
      static_cast<uint64_t>(room_id) * pointer_width;
  if (cow_pointer_slot + pointer_width > std::numeric_limits<uint32_t>::max()) {
    return absl::OutOfRangeError("Selected COW pointer range overflows");
  }
  ranges.emplace_back(static_cast<uint32_t>(cow_pointer_slot),
                      static_cast<uint32_t>(cow_pointer_slot + pointer_width));

  const auto conflicts = context.manifest.AnalyzePcWriteRanges(ranges);
  if (conflicts.empty() ||
      context.write_policy != project::RomWritePolicy::kBlock) {
    return absl::OkStatus();
  }
  return absl::PermissionDeniedError("Write conflict with Hack Manifest");
}

absl::Status PreflightObjectSave(
    const Rom& source_rom, int room_id, const zelda3::RoomObject& object,
    const zelda3::Room& pending_room,
    const ObjectSaveManifestContext* manifest_context) {
  if (manifest_context != nullptr) {
    RETURN_IF_ERROR(ValidateObjectSaveManifestConflicts(
        source_rom, room_id, pending_room, *manifest_context));
  }

  // Exercise the exact Room::SaveObjects path against an isolated snapshot so
  // dry-run capacity and allocator outcomes match --write without mutating the
  // caller's ROM.
  Rom scratch_rom;
  Rom::LoadOptions options;
  options.strip_header = false;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(scratch_rom.LoadFromData(source_rom.vector(), options));

  zelda3::Room scratch_room = zelda3::LoadRoomFromRom(&scratch_rom, room_id);
  RETURN_IF_ERROR(scratch_room.AddObject(object));
  return scratch_room.SaveObjects(manifest_context != nullptr
                                      ? &manifest_context->allocator_layout
                                      : nullptr);
}

absl::StatusOr<int> GetRequiredHex(const resources::ArgumentParser& parser,
                                   const char* name) {
  auto value_or = GetRequiredString(parser, name);
  if (!value_or.ok()) {
    return value_or.status();
  }

  int value = 0;
  if (!ParseHexString(*value_or, &value)) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid --%s value '%s' (expected integer/hex)", name, *value_or));
  }
  return value;
}

absl::Status ValidateDoorType(int type, const char* argument_name) {
  if (type < 0 || type > 0x66 || (type & 1) != 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--%s must be an even door type in range 0x00-0x66", argument_name));
  }
  return absl::OkStatus();
}

bool ContainsRoomId(const std::vector<uint32_t>& room_ids, int room_id) {
  return std::find(room_ids.begin(), room_ids.end(),
                   static_cast<uint32_t>(room_id)) != room_ids.end();
}

bool IsLoRomDataPointer(uint32_t snes_address) {
  if ((snes_address & 0xFFFFu) < 0x8000u) {
    return false;
  }
  const uint8_t normalized_bank =
      static_cast<uint8_t>((snes_address >> 16) & 0x7Fu);
  if (normalized_bank >= 0x7Eu) {
    return false;
  }
  const uint32_t pc_address = SnesToPc(snes_address);
  return (PcToSnes(pc_address) & 0x7FFFFFu) == (snes_address & 0x7FFFFFu);
}

constexpr int kDungeonPaletteColorCount = 90;
constexpr int kDungeonPaletteCount = gfx::DungeonsMainPalettesMax;
constexpr uint32_t kDungeonPaletteDataStart = gfx::kDungeonMainPalettes;
constexpr uint32_t kSnesDestinationCodePc = 0x7FD9;
constexpr uint32_t kDungeonPaletteDataEnd =
    kDungeonPaletteDataStart +
    kDungeonPaletteCount * zelda3::kDungeonPaletteBytes;

bool IsValidRomRange(const Rom& rom, uint64_t start, uint64_t length) {
  return start <= rom.size() && length <= rom.size() - start;
}

absl::Status ValidateSupportedDungeonPaletteRom(const Rom& rom) {
  if (!IsValidRomRange(rom, kSnesDestinationCodePc, 1)) {
    return absl::FailedPreconditionError(
        "ROM is too small to prove US/OOS dungeon palette table layout");
  }
  uint8_t destination_code = 0;
  ASSIGN_OR_RETURN(destination_code,
                   rom.ReadByte(static_cast<int>(kSnesDestinationCodePc)));
  if (destination_code != 0x01) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon palette commands currently support only the US/OOS table "
        "layout (SNES destination code 0x01); got unsupported code 0x%02X",
        destination_code));
  }
  return absl::OkStatus();
}

struct DungeonPaletteRoomMapping {
  int room_id = -1;
  uint8_t palette_set_id = 0;
  uint8_t palette_pointer_offset = 0;
  uint16_t palette_pointer_word = 0;
  int palette_index = -1;
  uint32_t room_header_pc = 0;
  uint32_t palette_set_pc = 0;
  uint32_t palette_pointer_pc = 0;
  uint32_t palette_pc = 0;
};

absl::StatusOr<uint32_t> ResolveRoomHeaderPcStrict(const Rom& rom,
                                                   int room_id) {
  RETURN_IF_ERROR(ValidateRoomId(room_id));
  if (!IsValidRomRange(rom, zelda3::kRoomHeaderPointer, 3) ||
      !IsValidRomRange(rom, zelda3::kRoomHeaderPointerBank, 1)) {
    return absl::FailedPreconditionError(
        "Dungeon room-header pointer metadata is outside the ROM");
  }

  uint32_t table_snes = 0;
  ASSIGN_OR_RETURN(table_snes, rom.ReadLong(zelda3::kRoomHeaderPointer));
  if (!IsLoRomDataPointer(table_snes)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon room-header pointer table is not a valid LoROM pointer: "
        "0x%06X",
        table_snes));
  }
  const uint32_t table_pc = SnesToPc(table_snes);
  const uint64_t slot_pc =
      static_cast<uint64_t>(table_pc) + static_cast<uint64_t>(room_id) * 2u;
  if (!IsValidRomRange(rom, slot_pc, 2)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon room 0x%03X header pointer slot is outside the ROM", room_id));
  }

  uint16_t header_offset = 0;
  uint8_t header_bank = 0;
  ASSIGN_OR_RETURN(header_offset, rom.ReadWord(static_cast<int>(slot_pc)));
  ASSIGN_OR_RETURN(header_bank, rom.ReadByte(zelda3::kRoomHeaderPointerBank));
  const uint32_t header_snes =
      (static_cast<uint32_t>(header_bank) << 16) | header_offset;
  if (!IsLoRomDataPointer(header_snes)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon room 0x%03X header is not a valid LoROM pointer: 0x%06X",
        room_id, header_snes));
  }

  const uint32_t header_pc = SnesToPc(header_snes);
  if (!IsValidRomRange(rom, header_pc, 14)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon room 0x%03X header is outside the ROM", room_id));
  }
  return header_pc;
}

absl::StatusOr<DungeonPaletteRoomMapping> ResolveDungeonPaletteRoomMapping(
    const Rom& rom, int room_id) {
  DungeonPaletteRoomMapping mapping;
  mapping.room_id = room_id;
  ASSIGN_OR_RETURN(mapping.room_header_pc,
                   ResolveRoomHeaderPcStrict(rom, room_id));
  ASSIGN_OR_RETURN(mapping.palette_set_id,
                   rom.ReadByte(static_cast<int>(mapping.room_header_pc + 1u)));
  if (mapping.palette_set_id >= zelda3::kNumPalettesets) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon room 0x%03X has out-of-range 8-bit palette-set ID 0x%02X "
        "(expected 0x00-0x%02X)",
        room_id, mapping.palette_set_id, zelda3::kNumPalettesets - 1));
  }

  mapping.palette_set_pc = zelda3::kPalettesetIdsAddress +
                           static_cast<uint32_t>(mapping.palette_set_id) * 4u;
  if (!IsValidRomRange(rom, mapping.palette_set_pc, 4)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Palette-set entry 0x%02X is outside the ROM", mapping.palette_set_id));
  }
  ASSIGN_OR_RETURN(mapping.palette_pointer_offset,
                   rom.ReadByte(static_cast<int>(mapping.palette_set_pc)));
  if ((mapping.palette_pointer_offset & 1u) != 0u ||
      mapping.palette_pointer_offset >= kDungeonPaletteCount * 2) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Palette-set 0x%02X has malformed dungeon palette pointer offset "
        "0x%02X (expected an even byte offset 0x00-0x%02X)",
        mapping.palette_set_id, mapping.palette_pointer_offset,
        kDungeonPaletteCount * 2 - 2));
  }

  mapping.palette_pointer_pc =
      zelda3::kDungeonPalettePointerTable + mapping.palette_pointer_offset;
  if (!IsValidRomRange(rom, mapping.palette_pointer_pc, 2)) {
    return absl::FailedPreconditionError(
        "Resolved dungeon palette pointer is outside the ROM");
  }
  ASSIGN_OR_RETURN(mapping.palette_pointer_word,
                   rom.ReadWord(static_cast<int>(mapping.palette_pointer_pc)));
  if (mapping.palette_pointer_word % zelda3::kDungeonPaletteBytes != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Palette-set 0x%02X resolves to malformed palette word 0x%04X "
        "(not divisible by %d)",
        mapping.palette_set_id, mapping.palette_pointer_word,
        zelda3::kDungeonPaletteBytes));
  }

  mapping.palette_index =
      mapping.palette_pointer_word / zelda3::kDungeonPaletteBytes;
  if (mapping.palette_index < 0 ||
      mapping.palette_index >= kDungeonPaletteCount) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Palette-set 0x%02X resolves to out-of-range dungeon palette %d",
        mapping.palette_set_id, mapping.palette_index));
  }

  mapping.palette_pc = kDungeonPaletteDataStart + mapping.palette_pointer_word;
  if (mapping.palette_pc < kDungeonPaletteDataStart ||
      static_cast<uint64_t>(mapping.palette_pc) + zelda3::kDungeonPaletteBytes >
          kDungeonPaletteDataEnd ||
      !IsValidRomRange(rom, mapping.palette_pc, zelda3::kDungeonPaletteBytes)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Palette-set 0x%02X resolves outside the fixed dungeon palette "
        "region [0x%06X,0x%06X)",
        mapping.palette_set_id, kDungeonPaletteDataStart,
        kDungeonPaletteDataEnd));
  }
  return mapping;
}

struct DungeonPaletteSelection {
  DungeonPaletteRoomMapping selected;
  std::vector<uint32_t> affected_rooms;
};

absl::StatusOr<DungeonPaletteSelection> ResolveDungeonPaletteSelection(
    const Rom& rom, int room_id) {
  RETURN_IF_ERROR(ValidateSupportedDungeonPaletteRom(rom));
  DungeonPaletteSelection selection;
  ASSIGN_OR_RETURN(selection.selected,
                   ResolveDungeonPaletteRoomMapping(rom, room_id));
  selection.affected_rooms.reserve(zelda3::kNumberOfRooms);
  for (int candidate = 0; candidate < zelda3::kNumberOfRooms; ++candidate) {
    auto mapping_or = ResolveDungeonPaletteRoomMapping(rom, candidate);
    if (!mapping_or.ok()) {
      return absl::Status(
          mapping_or.status().code(),
          absl::StrFormat(
              "Cannot prove complete shared palette fanout because room "
              "0x%03X mapping is invalid: %s",
              candidate, mapping_or.status().message()));
    }
    if (mapping_or->palette_index == selection.selected.palette_index) {
      selection.affected_rooms.push_back(static_cast<uint32_t>(candidate));
    }
  }
  return selection;
}

absl::Status ValidateDungeonPaletteColorIndex(int color_index) {
  if (color_index < 0 || color_index >= kDungeonPaletteColorCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--index must be in range 0-%d", kDungeonPaletteColorCount - 1));
  }
  return absl::OkStatus();
}

absl::Status ValidateSnes15BitColor(int color, const char* argument_name) {
  if (color < 0 || color > 0x7FFF) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--%s must be a 15-bit SNES color in range 0x0000-0x7FFF",
        argument_name));
  }
  return absl::OkStatus();
}

uint32_t DungeonPaletteColorPc(const DungeonPaletteRoomMapping& mapping,
                               int color_index) {
  return mapping.palette_pc + static_cast<uint32_t>(color_index) * 2u;
}

absl::StatusOr<uint16_t> ReadDungeonPaletteColor(
    const Rom& rom, const DungeonPaletteRoomMapping& mapping, int color_index) {
  RETURN_IF_ERROR(ValidateDungeonPaletteColorIndex(color_index));
  const uint32_t color_pc = DungeonPaletteColorPc(mapping, color_index);
  if (!IsValidRomRange(rom, color_pc, 2) ||
      color_pc < kDungeonPaletteDataStart ||
      static_cast<uint64_t>(color_pc) + 2u > kDungeonPaletteDataEnd) {
    return absl::FailedPreconditionError(
        "Resolved dungeon palette color is outside the fixed palette region");
  }
  uint16_t color = 0;
  ASSIGN_OR_RETURN(color, rom.ReadWord(static_cast<int>(color_pc)));
  if (color > 0x7FFF) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Resolved dungeon palette color at PC 0x%06X is not a 15-bit SNES "
        "color: 0x%04X",
        color_pc, color));
  }
  return color;
}

void AddDungeonPaletteScope(resources::OutputFormatter& formatter,
                            const DungeonPaletteSelection& selection) {
  const auto& mapping = selection.selected;
  formatter.AddHexField("room_id", mapping.room_id, 3);
  formatter.AddHexField("palette_set_id", mapping.palette_set_id, 2);
  formatter.AddField("resolved_palette_index", mapping.palette_index);
  formatter.AddHexField("room_header_pc", mapping.room_header_pc, 6);
  formatter.AddHexField("palette_set_pc", mapping.palette_set_pc, 6);
  formatter.AddHexField("palette_pointer_offset",
                        mapping.palette_pointer_offset, 2);
  formatter.AddHexField("palette_pointer_pc", mapping.palette_pointer_pc, 6);
  formatter.AddHexField("palette_pointer_word", mapping.palette_pointer_word,
                        4);
  formatter.AddHexField("palette_pc", mapping.palette_pc, 6);
  formatter.AddHexField("palette_snes", PcToSnes(mapping.palette_pc), 6);
  formatter.AddField("scope", "shared_global_dungeon_palette");
  formatter.AddField("affected_room_count",
                     static_cast<int>(selection.affected_rooms.size()));
  formatter.BeginArray("affected_rooms");
  for (uint32_t affected_room : selection.affected_rooms) {
    formatter.AddArrayItem(absl::StrFormat("0x%03X", affected_room));
  }
  formatter.EndArray();
}

absl::Status ValidateDungeonPaletteManifestRange(
    const core::HackManifest& manifest, uint32_t color_pc) {
  const auto conflicts =
      manifest.AnalyzePcWriteRanges({{color_pc, color_pc + 2u}});
  if (conflicts.empty()) {
    return absl::OkStatus();
  }
  const core::WriteConflict& conflict = conflicts.front();
  return absl::PermissionDeniedError(absl::StrFormat(
      "Dungeon palette color range [0x%06X,0x%06X) conflicts with Hack "
      "Manifest ownership at SNES 0x%06X (%s%s%s)",
      color_pc, color_pc + 2u, conflict.address,
      core::AddressOwnershipToString(conflict.ownership),
      conflict.module.empty() ? "" : ", module=",
      conflict.module.empty() ? "" : conflict.module));
}

absl::StatusOr<std::vector<uint32_t>> ValidateDungeonPaletteWholeRomDiff(
    const std::vector<uint8_t>& before, const Rom& after, uint32_t color_pc) {
  if (after.size() != before.size()) {
    return absl::DataLossError(
        "Dungeon palette write unexpectedly resized the ROM");
  }

  std::vector<uint32_t> changed;
  for (uint32_t pc = 0; pc < before.size(); ++pc) {
    if (before[pc] == after.data()[pc]) {
      continue;
    }
    if (pc < color_pc || pc >= color_pc + 2u) {
      return absl::DataLossError(absl::StrFormat(
          "Dungeon palette write changed unexpected ROM byte 0x%06X", pc));
    }
    changed.push_back(pc);
  }
  if (changed.empty() || changed.size() > 2u) {
    return absl::DataLossError(absl::StrFormat(
        "Dungeon palette write changed %zu bytes; expected one or two bytes "
        "inside the fenced color word",
        changed.size()));
  }
  return changed;
}

absl::Status ValidateDungeonPaletteWriteBaseline(const Rom& rom) {
  if (rom.dirty()) {
    return absl::FailedPreconditionError(
        "Dungeon palette write requires a clean file-backed ROM; save or "
        "discard unrelated in-memory edits first");
  }
  if (rom.filename().empty()) {
    return absl::FailedPreconditionError(
        "Dungeon palette write requires a ROM filename");
  }

  Rom disk_rom;
  Rom::LoadOptions options;
  options.strip_header = false;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(disk_rom.LoadFromFile(rom.filename(), options));
  if (disk_rom.vector() != rom.vector()) {
    return absl::FailedPreconditionError(
        "Dungeon palette write requires in-memory ROM bytes to exactly match "
        "the current file; refusing to persist unrelated or stale bytes");
  }
  return absl::OkStatus();
}

absl::Status VerifyDungeonPaletteExternalReadback(
    const Rom& saved_rom, int room_id, int expected_palette_set,
    int expected_palette_index, int color_index, uint16_t expected_color) {
  if (saved_rom.filename().empty()) {
    return absl::FailedPreconditionError(
        "Write mode requires a ROM filename for external readback");
  }

  Rom reopened;
  Rom::LoadOptions options;
  options.strip_header = false;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(reopened.LoadFromFile(saved_rom.filename(), options));
  if (reopened.vector() != saved_rom.vector()) {
    return absl::DataLossError(
        "External reopen bytes do not match the saved in-memory ROM");
  }

  RETURN_IF_ERROR(ValidateSupportedDungeonPaletteRom(reopened));
  DungeonPaletteRoomMapping reopened_mapping;
  ASSIGN_OR_RETURN(reopened_mapping,
                   ResolveDungeonPaletteRoomMapping(reopened, room_id));
  if (reopened_mapping.palette_set_id != expected_palette_set ||
      reopened_mapping.palette_index != expected_palette_index) {
    return absl::DataLossError(
        "External reopen changed the selected dungeon palette mapping");
  }
  uint16_t reopened_color = 0;
  ASSIGN_OR_RETURN(
      reopened_color,
      ReadDungeonPaletteColor(reopened, reopened_mapping, color_index));
  if (reopened_color != expected_color) {
    return absl::DataLossError(absl::StrFormat(
        "External reopen color mismatch: expected 0x%04X, got 0x%04X",
        expected_color, reopened_color));
  }
  return absl::OkStatus();
}

struct DoorTypeTarget {
  int door_index = -1;
  zelda3::Room::Door door;
  uint32_t object_stream_pc = 0;
  uint32_t object_stream_end_pc = 0;
  uint32_t door_pointer_pc = 0;
  uint32_t door_entry_pc = 0;
  uint32_t type_pc = 0;
};

absl::StatusOr<DoorTypeTarget> ResolveDoorTypeTarget(
    Rom* rom, int room_id, int x, int y,
    const ObjectSaveManifestContext& manifest_context) {
  if (manifest_context.allocator_layout.pointer_encoding !=
      zelda3::DungeonPointerEncoding::kLong24) {
    return absl::FailedPreconditionError(
        "Dungeon object manifest pointer encoding must be long24");
  }
  if (manifest_context.allocator_layout.pointer_count !=
      zelda3::kNumberOfRooms) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon object manifest pointer_count must equal %d rooms",
        zelda3::kNumberOfRooms));
  }

  uint32_t runtime_pointer_table_snes = 0;
  ASSIGN_OR_RETURN(runtime_pointer_table_snes,
                   rom->ReadLong(zelda3::kRoomObjectPointer));
  if (!IsLoRomDataPointer(runtime_pointer_table_snes)) {
    return absl::FailedPreconditionError(
        "Runtime object pointer table is not a valid LoROM data pointer");
  }
  const uint32_t runtime_pointer_table_pc =
      SnesToPc(runtime_pointer_table_snes);
  if (runtime_pointer_table_pc !=
      manifest_context.allocator_layout.pointer_table_pc) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Manifest object pointer table PC 0x%06X does not match runtime "
        "table PC 0x%06X",
        manifest_context.allocator_layout.pointer_table_pc,
        runtime_pointer_table_pc));
  }

  zelda3::DungeonStreamInventory inventory;
  ASSIGN_OR_RETURN(inventory, zelda3::InventoryDungeonStreams(
                                  *rom, manifest_context.allocator_layout));
  if (static_cast<size_t>(room_id) >= inventory.streams.size()) {
    return absl::FailedPreconditionError(
        "Selected room is absent from the object stream inventory");
  }
  for (const auto& issue : inventory.issues) {
    if (issue.room_id == static_cast<uint32_t>(room_id)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Selected object stream is invalid: %s", issue.message));
    }
  }

  const auto& stream = inventory.streams[room_id];
  if (!stream.valid) {
    return absl::FailedPreconditionError("Selected object stream is invalid");
  }
  for (const auto& alias : inventory.aliases) {
    if (ContainsRoomId(alias.room_ids, room_id)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Selected object stream at PC 0x%06X is shared by %d rooms",
          alias.data_pc, static_cast<int>(alias.room_ids.size())));
    }
  }
  for (const auto& overlap : inventory.overlaps) {
    if (ContainsRoomId(overlap.first_room_ids, room_id) ||
        ContainsRoomId(overlap.second_room_ids, room_id)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Selected object stream overlaps PC range [0x%06X, 0x%06X)",
          overlap.intersection.begin, overlap.intersection.end));
    }
  }
  for (const auto& other_stream : inventory.streams) {
    if (other_stream.room_id == static_cast<uint32_t>(room_id)) {
      continue;
    }
    if (other_stream.data_pc >= stream.data_pc &&
        other_stream.data_pc < stream.logical_end_pc) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X object pointer PC 0x%06X starts inside selected "
          "object stream [0x%06X, 0x%06X)",
          other_stream.room_id, other_stream.data_pc, stream.data_pc,
          stream.logical_end_pc));
    }
  }
  for (int other_room_id = 0; other_room_id < zelda3::kNumberOfRooms;
       ++other_room_id) {
    if (other_room_id == room_id) {
      continue;
    }
    const uint32_t other_door_pointer_slot =
        zelda3::kDoorPointers + static_cast<uint32_t>(other_room_id) * 3u;
    uint32_t other_door_pointer_snes = 0;
    ASSIGN_OR_RETURN(other_door_pointer_snes,
                     rom->ReadLong(static_cast<int>(other_door_pointer_slot)));
    if (!IsLoRomDataPointer(other_door_pointer_snes)) {
      continue;
    }
    const uint32_t other_door_pointer_pc = SnesToPc(other_door_pointer_snes);
    if (other_door_pointer_pc >= stream.data_pc &&
        other_door_pointer_pc < stream.logical_end_pc) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X door pointer PC 0x%06X points inside selected "
          "object stream [0x%06X, 0x%06X)",
          other_room_id, other_door_pointer_pc, stream.data_pc,
          stream.logical_end_pc));
    }
  }

  const uint64_t runtime_pointer_slot =
      static_cast<uint64_t>(runtime_pointer_table_pc) +
      static_cast<uint64_t>(room_id) * 3u;
  if (runtime_pointer_slot + 3u > rom->size() ||
      runtime_pointer_slot != stream.pointer_slot_pc) {
    return absl::FailedPreconditionError(
        "Selected runtime object pointer slot does not match the manifest");
  }
  uint32_t runtime_stream_snes = 0;
  ASSIGN_OR_RETURN(runtime_stream_snes,
                   rom->ReadLong(static_cast<int>(runtime_pointer_slot)));
  if (!IsLoRomDataPointer(runtime_stream_snes)) {
    return absl::FailedPreconditionError(
        "Selected runtime object stream is not a valid LoROM data pointer");
  }
  const uint32_t runtime_stream_pc = SnesToPc(runtime_stream_snes);
  if (runtime_stream_pc != stream.data_pc) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Selected runtime object stream PC 0x%06X does not match inventory "
        "PC 0x%06X",
        runtime_stream_pc, stream.data_pc));
  }

  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);
  const auto& doors = room.GetDoors();
  int selected_index = -1;
  for (int index = 0; index < static_cast<int>(doors.size()); ++index) {
    const auto [door_x, door_y] = doors[index].GetTileCoords();
    if (door_x != x || door_y != y) {
      continue;
    }
    if (selected_index >= 0) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Multiple doors match tile coordinates (%d, %d)", x, y));
    }
    selected_index = index;
  }
  if (selected_index < 0) {
    return absl::NotFoundError(absl::StrFormat(
        "No door matches tile coordinates (%d, %d) in room 0x%03X", x, y,
        room_id));
  }

  const uint64_t door_pointer_slot =
      static_cast<uint64_t>(zelda3::kDoorPointers) +
      static_cast<uint64_t>(room_id) * 3u;
  if (door_pointer_slot + 3u > rom->size()) {
    return absl::OutOfRangeError("Selected door pointer slot is outside ROM");
  }
  uint32_t door_pointer_snes = 0;
  ASSIGN_OR_RETURN(door_pointer_snes,
                   rom->ReadLong(static_cast<int>(door_pointer_slot)));
  if (!IsLoRomDataPointer(door_pointer_snes)) {
    return absl::FailedPreconditionError(
        "Selected door pointer is not a valid LoROM data pointer");
  }
  const uint32_t door_pointer_pc = SnesToPc(door_pointer_snes);
  const uint64_t door_entry_pc = static_cast<uint64_t>(door_pointer_pc) +
                                 static_cast<uint64_t>(selected_index) * 2u;
  const uint64_t expected_stream_end =
      static_cast<uint64_t>(door_pointer_pc) +
      static_cast<uint64_t>(doors.size()) * 2u + 2u;
  if (door_pointer_pc < stream.data_pc ||
      door_entry_pc + 2u > stream.logical_end_pc ||
      expected_stream_end != stream.logical_end_pc) {
    return absl::FailedPreconditionError(
        "Door pointer/count does not match the inventoried object stream end");
  }

  uint8_t raw_byte1 = 0;
  uint8_t raw_byte2 = 0;
  for (int index = 0; index < static_cast<int>(doors.size()); ++index) {
    const uint32_t entry_pc =
        door_pointer_pc + static_cast<uint32_t>(index) * 2u;
    uint8_t entry_byte1 = 0;
    uint8_t entry_byte2 = 0;
    ASSIGN_OR_RETURN(entry_byte1, rom->ReadByte(static_cast<int>(entry_pc)));
    ASSIGN_OR_RETURN(entry_byte2,
                     rom->ReadByte(static_cast<int>(entry_pc + 1u)));
    const auto [model_byte1, model_byte2] = doors[index].EncodeBytes();
    if (model_byte1 != entry_byte1 || model_byte2 != entry_byte2 ||
        doors[index].byte1 != entry_byte1 ||
        doors[index].byte2 != entry_byte2) {
      return absl::DataLossError(
          "Door model does not match the raw door-pointer entries");
    }
    if (index == selected_index) {
      raw_byte1 = entry_byte1;
      raw_byte2 = entry_byte2;
    }
  }
  uint16_t door_terminator = 0;
  ASSIGN_OR_RETURN(door_terminator,
                   rom->ReadWord(static_cast<int>(expected_stream_end - 2u)));
  if (door_terminator != 0xFFFFu) {
    return absl::DataLossError(
        "Door list does not end with the expected 0xFFFF terminator");
  }

  const zelda3::Room::Door raw_door =
      zelda3::Room::Door::FromRomBytes(raw_byte1, raw_byte2);
  const zelda3::Room::Door& model_door = doors[selected_index];
  const auto [raw_x, raw_y] = raw_door.GetTileCoords();
  if (model_door.position != raw_door.position ||
      model_door.direction != raw_door.direction ||
      model_door.type != raw_door.type || raw_x != x || raw_y != y) {
    return absl::DataLossError(
        "Door model does not match the raw door-pointer entry");
  }

  const uint32_t type_pc = static_cast<uint32_t>(door_entry_pc + 1u);
  const auto conflicts =
      manifest_context.manifest.AnalyzePcWriteRanges({{type_pc, type_pc + 1u}});
  if (!conflicts.empty()) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "Door type byte at PC 0x%06X conflicts with Hack Manifest (%s)",
        type_pc, core::AddressOwnershipToString(conflicts.front().ownership)));
  }

  DoorTypeTarget target;
  target.door_index = selected_index;
  target.door = model_door;
  target.object_stream_pc = stream.data_pc;
  target.object_stream_end_pc = stream.logical_end_pc;
  target.door_pointer_pc = door_pointer_pc;
  target.door_entry_pc = static_cast<uint32_t>(door_entry_pc);
  target.type_pc = type_pc;
  return target;
}

struct PotItemManifestContext {
  core::HackManifest manifest;
  zelda3::DungeonStreamLayout layout;
};

absl::StatusOr<PotItemManifestContext> LoadPotItemManifestContext(
    const resources::ArgumentParser& parser) {
  std::string manifest_path;
  ASSIGN_OR_RETURN(manifest_path, GetRequiredString(parser, "manifest"));

  PotItemManifestContext context;
  RETURN_IF_ERROR(context.manifest.LoadFromFile(manifest_path));
  const core::DungeonStreamLayout* manifest_layout =
      context.manifest.GetDungeonStreamLayout(
          core::DungeonStreamType::kPotItems);
  if (manifest_layout == nullptr) {
    return absl::FailedPreconditionError(
        "Manifest does not define dungeon_stream_regions.pot_items");
  }
  ASSIGN_OR_RETURN(context.layout,
                   core::ToDungeonStreamAllocatorLayout(
                       core::DungeonStreamType::kPotItems, *manifest_layout));
  return context;
}

struct PotItemEntryView {
  int index = -1;
  zelda3::PotItem item;
  uint32_t entry_pc = 0;
  uint32_t item_pc = 0;
};

struct PotItemStreamView {
  uint32_t pointer_slot_pc = 0;
  uint32_t stream_pc = 0;
  uint32_t stream_end_pc = 0;
  std::vector<PotItemEntryView> entries;
};

absl::StatusOr<PotItemStreamView> ResolveRuntimePotItemStream(Rom* rom,
                                                              int room_id) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  const uint64_t pointer_table_end =
      static_cast<uint64_t>(zelda3::kRoomItemsPointers) +
      static_cast<uint64_t>(zelda3::kNumberOfRooms) * 2u;
  if (pointer_table_end > rom->size()) {
    return absl::OutOfRangeError(
        "Complete pot-item pointer table is outside ROM");
  }
  const uint32_t pointer_slot_pc =
      zelda3::kRoomItemsPointers + static_cast<uint32_t>(room_id) * 2u;
  uint16_t pointer_word = 0;
  ASSIGN_OR_RETURN(pointer_word,
                   rom->ReadWord(static_cast<int>(pointer_slot_pc)));
  const uint32_t pointer_snes = 0x010000u | pointer_word;
  if (!IsLoRomDataPointer(pointer_snes)) {
    return absl::FailedPreconditionError(
        "Selected pot-item pointer is not a valid bank-01 LoROM pointer");
  }
  const uint32_t stream_pc = SnesToPc(pointer_snes);
  const uint64_t bank_end =
      (static_cast<uint64_t>(stream_pc) / 0x8000u + 1u) * 0x8000u;
  const uint32_t parse_end =
      static_cast<uint32_t>(std::min<uint64_t>(rom->size(), bank_end));
  if (stream_pc >= parse_end) {
    return absl::OutOfRangeError("Selected pot-item stream is outside ROM");
  }

  PotItemStreamView view;
  view.pointer_slot_pc = pointer_slot_pc;
  view.stream_pc = stream_pc;

  uint32_t cursor = stream_pc;
  while (true) {
    if (static_cast<uint64_t>(cursor) + 2u > parse_end) {
      return absl::DataLossError("Pot-item stream has no 0xFFFF terminator");
    }
    uint16_t position = 0;
    ASSIGN_OR_RETURN(position, rom->ReadWord(static_cast<int>(cursor)));
    if (position == 0xFFFFu) {
      view.stream_end_pc = cursor + 2u;
      break;
    }
    if (static_cast<uint64_t>(cursor) + 3u > parse_end) {
      return absl::DataLossError("Pot-item stream has a truncated record");
    }
    uint8_t item = 0;
    ASSIGN_OR_RETURN(item, rom->ReadByte(static_cast<int>(cursor + 2u)));
    view.entries.push_back({.index = static_cast<int>(view.entries.size()),
                            .item = {.position = position, .item = item},
                            .entry_pc = cursor,
                            .item_pc = cursor + 2u});
    cursor += 3u;
  }

  zelda3::Room room(room_id, rom);
  room.LoadPotItems();
  if (!room.ArePotItemsLoaded() ||
      room.GetPotItems().size() != view.entries.size()) {
    return absl::DataLossError(
        "Pot-item room model count does not match the raw stream");
  }
  for (size_t index = 0; index < view.entries.size(); ++index) {
    const zelda3::PotItem& model = room.GetPotItems()[index];
    if (model.position != view.entries[index].item.position ||
        model.item != view.entries[index].item.item) {
      return absl::DataLossError(
          "Pot-item room model does not match the raw stream");
    }
  }
  return view;
}

struct PotItemTarget {
  PotItemEntryView entry;
  uint32_t pointer_slot_pc = 0;
  uint32_t stream_pc = 0;
  uint32_t stream_end_pc = 0;
};

absl::StatusOr<PotItemTarget> ResolvePotItemTarget(
    Rom* rom, int room_id, int index,
    const PotItemManifestContext& manifest_context) {
  if (manifest_context.layout.pointer_count != zelda3::kNumberOfRooms) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon pot-item manifest pointer_count must equal %d rooms",
        zelda3::kNumberOfRooms));
  }

  zelda3::DungeonStreamInventory inventory;
  ASSIGN_OR_RETURN(inventory, zelda3::InventoryDungeonStreams(
                                  *rom, manifest_context.layout));
  if (static_cast<size_t>(room_id) >= inventory.streams.size()) {
    return absl::FailedPreconditionError(
        "Selected room is absent from the pot-item stream inventory");
  }
  if (!inventory.issues.empty()) {
    const auto& issue = inventory.issues.front();
    return absl::FailedPreconditionError(absl::StrFormat(
        "Pot-item stream inventory is invalid at room 0x%03X: %s",
        issue.room_id, issue.message));
  }

  const auto& stream = inventory.streams[room_id];
  if (!stream.valid) {
    return absl::FailedPreconditionError("Selected pot-item stream is invalid");
  }
  for (const auto& alias : inventory.aliases) {
    if (ContainsRoomId(alias.room_ids, room_id)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Selected pot-item stream at PC 0x%06X is shared by %d rooms",
          alias.data_pc, static_cast<int>(alias.room_ids.size())));
    }
  }
  for (const auto& overlap : inventory.overlaps) {
    if (ContainsRoomId(overlap.first_room_ids, room_id) ||
        ContainsRoomId(overlap.second_room_ids, room_id)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Selected pot-item stream overlaps PC range [0x%06X, 0x%06X)",
          overlap.intersection.begin, overlap.intersection.end));
    }
  }
  for (const auto& other_stream : inventory.streams) {
    if (other_stream.room_id == static_cast<uint32_t>(room_id)) {
      continue;
    }
    if (other_stream.data_pc >= stream.data_pc &&
        other_stream.data_pc < stream.logical_end_pc) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X pot-item pointer PC 0x%06X starts inside selected "
          "pot-item stream [0x%06X, 0x%06X)",
          other_stream.room_id, other_stream.data_pc, stream.data_pc,
          stream.logical_end_pc));
    }
  }

  PotItemStreamView runtime;
  ASSIGN_OR_RETURN(runtime, ResolveRuntimePotItemStream(rom, room_id));
  if (runtime.pointer_slot_pc != stream.pointer_slot_pc ||
      runtime.stream_pc != stream.data_pc ||
      runtime.stream_end_pc != stream.logical_end_pc ||
      runtime.entries.size() * 3u + 2u != stream.encoded_stream.size()) {
    return absl::DataLossError(
        "Runtime pot-item stream does not match the manifest inventory");
  }
  if (index < 0 || index >= static_cast<int>(runtime.entries.size())) {
    return absl::OutOfRangeError(
        absl::StrFormat("Pot-item index %d out of range (room has %d entries)",
                        index, static_cast<int>(runtime.entries.size())));
  }

  const PotItemEntryView& entry = runtime.entries[index];
  const auto conflicts = manifest_context.manifest.AnalyzePcWriteRanges(
      {{entry.item_pc, entry.item_pc + 1u}});
  if (!conflicts.empty()) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "Pot-item byte at PC 0x%06X conflicts with Hack Manifest (%s)",
        entry.item_pc,
        core::AddressOwnershipToString(conflicts.front().ownership)));
  }

  return PotItemTarget{
      .entry = entry,
      .pointer_slot_pc = runtime.pointer_slot_pc,
      .stream_pc = runtime.stream_pc,
      .stream_end_pc = runtime.stream_end_pc,
  };
}

}  // namespace

// ---------------------------------------------------------------------------
// dungeon-place-sprite
// ---------------------------------------------------------------------------

absl::Status DungeonPlaceSpriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  auto sprite_id_str_or = GetRequiredString(parser, "id");
  if (!sprite_id_str_or.ok()) {
    return sprite_id_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();
  const std::string sprite_id_str = sprite_id_str_or.value();

  int room_id, sprite_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  if (!ParseHexString(sprite_id_str, &sprite_id)) {
    return absl::InvalidArgumentError("Invalid sprite ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }

  auto x_or = GetRequiredInt(parser, "x");
  if (!x_or.ok()) {
    return x_or.status();
  }
  auto y_or = GetRequiredInt(parser, "y");
  if (!y_or.ok()) {
    return y_or.status();
  }
  auto subtype_or = GetOptionalInt(parser, "subtype", 0);
  if (!subtype_or.ok()) {
    return subtype_or.status();
  }
  auto layer_or = GetOptionalInt(parser, "layer", 0);
  if (!layer_or.ok()) {
    return layer_or.status();
  }

  int x = x_or.value();
  int y = y_or.value();
  int subtype = subtype_or.value();
  int layer = layer_or.value();
  bool do_write = parser.HasFlag("write");

  // Validate ranges
  RETURN_IF_ERROR(ValidateSpriteCoord(x, 'X'));
  RETURN_IF_ERROR(ValidateSpriteCoord(y, 'Y'));
  if (sprite_id < 0 || sprite_id > 0xFF) {
    return absl::InvalidArgumentError("Sprite ID must be 0x00-0xFF");
  }
  if (subtype < 0 || subtype > 0x1F) {
    return absl::InvalidArgumentError("Subtype must be 0-31 (5-bit flags)");
  }
  if (layer < 0 || layer > 1) {
    return absl::InvalidArgumentError("Layer must be 0 or 1");
  }

  // Load room and its sprites
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();

  int count_before = static_cast<int>(room.GetSprites().size());

  // Add the new sprite
  room.GetSprites().emplace_back(
      static_cast<uint8_t>(sprite_id), static_cast<uint8_t>(x),
      static_cast<uint8_t>(y), static_cast<uint8_t>(subtype),
      static_cast<uint8_t>(layer));
  room.MarkSpritesDirty();

  formatter.BeginObject("Place Sprite");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddHexField("sprite_id", sprite_id, 2);
  formatter.AddField("sprite_name", zelda3::ResolveSpriteName(sprite_id));
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  formatter.AddField("subtype", subtype);
  formatter.AddField("layer", layer);
  formatter.AddField("sprites_before", count_before);
  formatter.AddField("sprites_after",
                     static_cast<int>(room.GetSprites().size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  if (do_write) {
    auto save_status = MutateAndSaveRomWithBackup(
        rom, formatter, [&room]() { return room.SaveSprites(); });
    if (!save_status.ok()) {
      formatter.EndObject();
      return save_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-remove-sprite
// ---------------------------------------------------------------------------

absl::Status DungeonRemoveSpriteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }

  bool do_write = parser.HasFlag("write");

  // Load room and its sprites
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();

  auto& sprites = room.GetSprites();
  int count_before = static_cast<int>(sprites.size());

  // Find sprite to remove: by --index or by --x/--y position.
  const bool has_index = parser.GetString("index").has_value();
  const bool has_x = parser.GetString("x").has_value();
  const bool has_y = parser.GetString("y").has_value();
  if (has_index && (has_x || has_y)) {
    return absl::InvalidArgumentError(
        "Use either --index or --x/--y, not both");
  }
  if (!has_index && has_x != has_y) {
    return absl::InvalidArgumentError(
        "Both --x and --y are required when removing by position");
  }
  if (!has_index && !has_x) {
    return absl::InvalidArgumentError(
        "Either --index or both --x and --y are required");
  }

  int remove_index = -1;
  if (has_index) {
    auto index_or = GetRequiredInt(parser, "index");
    if (!index_or.ok()) {
      return index_or.status();
    }
    remove_index = index_or.value();
  } else {
    auto x_or = GetRequiredInt(parser, "x");
    if (!x_or.ok()) {
      return x_or.status();
    }
    auto y_or = GetRequiredInt(parser, "y");
    if (!y_or.ok()) {
      return y_or.status();
    }

    const int x = x_or.value();
    const int y = y_or.value();
    RETURN_IF_ERROR(ValidateSpriteCoord(x, 'X'));
    RETURN_IF_ERROR(ValidateSpriteCoord(y, 'Y'));

    for (int i = 0; i < static_cast<int>(sprites.size()); ++i) {
      if (sprites[i].x() == x && sprites[i].y() == y) {
        remove_index = i;
        break;
      }
    }
    if (remove_index < 0) {
      return absl::NotFoundError(absl::StrFormat(
          "No sprite at (%d, %d) in room 0x%02X", x, y, room_id));
    }
  }

  if (remove_index < 0 || remove_index >= static_cast<int>(sprites.size())) {
    return absl::OutOfRangeError(
        absl::StrFormat("Sprite index %d out of range (room has %d sprites)",
                        remove_index, count_before));
  }

  // Report which sprite we're removing
  const auto& target = sprites[remove_index];
  formatter.BeginObject("Remove Sprite");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddField("removed_index", remove_index);
  formatter.AddHexField("sprite_id", target.id(), 2);
  formatter.AddField("sprite_name", zelda3::ResolveSpriteName(target.id()));
  formatter.AddField("x", target.x());
  formatter.AddField("y", target.y());
  formatter.AddField("sprites_before", count_before);

  // Remove
  sprites.erase(sprites.begin() + remove_index);
  room.MarkSpritesDirty();
  formatter.AddField("sprites_after", static_cast<int>(sprites.size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  if (do_write) {
    auto save_status = MutateAndSaveRomWithBackup(
        rom, formatter, [&room]() { return room.SaveSprites(); });
    if (!save_status.ok()) {
      formatter.EndObject();
      return save_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-place-object
// ---------------------------------------------------------------------------

absl::Status DungeonPlaceObjectCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  auto object_id_str_or = GetRequiredString(parser, "id");
  if (!object_id_str_or.ok()) {
    return object_id_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();
  const std::string object_id_str = object_id_str_or.value();

  int room_id, object_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  if (!ParseHexString(object_id_str, &object_id)) {
    return absl::InvalidArgumentError("Invalid object ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }
  if (object_id < 0 || object_id > 0xFFFF) {
    return absl::InvalidArgumentError("Object ID must be 0x0000-0xFFFF");
  }

  auto x_or = GetRequiredInt(parser, "x");
  if (!x_or.ok()) {
    return x_or.status();
  }
  auto y_or = GetRequiredInt(parser, "y");
  if (!y_or.ok()) {
    return y_or.status();
  }
  auto layer_or = GetOptionalInt(parser, "layer", 0);
  if (!layer_or.ok()) {
    return layer_or.status();
  }

  int x = x_or.value();
  int y = y_or.value();
  // Preserve the command's legacy omitted Type 1 size of zero while deriving
  // the fixed, ID-backed size required by Type 2/3 stream entries.
  int size = zelda3::CanonicalRoomObjectSize(object_id, 0);
  if (parser.GetString("size").has_value()) {
    auto size_or = parser.GetInt("size");
    if (!size_or.ok()) {
      return size_or.status();
    }
    size = size_or.value();
  }
  int layer = layer_or.value();
  bool do_write = parser.HasFlag("write");

  // Validate ranges
  if (x < 0 || x > 63) {
    return absl::InvalidArgumentError("X must be 0-63");
  }
  if (y < 0 || y > 63) {
    return absl::InvalidArgumentError("Y must be 0-63");
  }
  if (layer < 0 || layer > 2) {
    return absl::InvalidArgumentError("Layer must be 0, 1, or 2");
  }
  if (size < 0 || size > 0xFF) {
    return absl::InvalidArgumentError("Size must be 0-255");
  }
  const uint8_t canonical_size =
      zelda3::CanonicalRoomObjectSize(object_id, static_cast<uint8_t>(size));
  if (size != canonical_size) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Size %d is not encodable for object 0x%03X; expected %d", size,
        object_id, canonical_size));
  }

  std::optional<ObjectSaveManifestContext> manifest_context;
  ASSIGN_OR_RETURN(manifest_context, LoadObjectSaveManifestContext(parser));
  const zelda3::DungeonStreamLayout* layout =
      manifest_context.has_value() ? &manifest_context->allocator_layout
                                   : nullptr;

  // Load room with full objects
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  int count_before = static_cast<int>(room.GetTileObjects().size());

  // Create the new object
  zelda3::RoomObject obj(static_cast<int16_t>(object_id),
                         static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                         static_cast<uint8_t>(size),
                         static_cast<uint8_t>(layer));

  // Determine type for reporting
  int type = zelda3::RoomObject::DetermineObjectType((object_id & 0xFF),
                                                     (object_id >> 8));

  formatter.BeginObject("Place Object");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddHexField("object_id", object_id, 4);
  formatter.AddField("object_name", zelda3::GetObjectName(object_id));
  formatter.AddField("object_type", type);
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  formatter.AddField("size", size);
  formatter.AddField("layer", layer);
  formatter.AddField("objects_before", count_before);
  if (const auto manifest_path = parser.GetString("manifest");
      manifest_path.has_value()) {
    formatter.AddField("manifest", *manifest_path);
    formatter.AddField("manifest_write_policy", "block");
  }
  formatter.AddField("allocator_capability",
                     layout != nullptr ? "copy_on_write" : "none");

  // Add the object
  auto add_status = room.AddObject(obj);
  if (!add_status.ok()) {
    formatter.AddField("error", std::string(add_status.message()));
    formatter.EndObject();
    return add_status;
  }

  formatter.AddField("objects_after",
                     static_cast<int>(room.GetTileObjects().size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  const auto preflight_status = PreflightObjectSave(
      *rom, room_id, obj, room,
      manifest_context.has_value() ? &*manifest_context : nullptr);
  if (!preflight_status.ok()) {
    formatter.AddField("preflight_status", "failed");
    formatter.AddField("preflight_error",
                       std::string(preflight_status.message()));
    formatter.EndObject();
    return preflight_status;
  }
  formatter.AddField("preflight_status", "success");

  if (do_write) {
    auto save_status = MutateAndSaveRomWithBackup(
        rom, formatter, [&room, layout]() { return room.SaveObjects(layout); });
    if (!save_status.ok()) {
      formatter.EndObject();
      return save_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-get-palette
// ---------------------------------------------------------------------------

absl::Status DungeonGetPaletteCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  int room_id = 0;
  ASSIGN_OR_RETURN(room_id, GetRequiredHex(parser, "room"));
  RETURN_IF_ERROR(ValidateRoomId(room_id));

  std::optional<int> requested_index;
  if (parser.GetString("index").has_value()) {
    int color_index = 0;
    ASSIGN_OR_RETURN(color_index, GetRequiredInt(parser, "index"));
    RETURN_IF_ERROR(ValidateDungeonPaletteColorIndex(color_index));
    requested_index = color_index;
  }

  DungeonPaletteSelection selection;
  ASSIGN_OR_RETURN(selection, ResolveDungeonPaletteSelection(*rom, room_id));

  formatter.BeginObject("Dungeon Palette");
  AddDungeonPaletteScope(formatter, selection);
  formatter.AddField("color_count", kDungeonPaletteColorCount);

  if (requested_index.has_value()) {
    const uint32_t color_pc =
        DungeonPaletteColorPc(selection.selected, *requested_index);
    uint16_t color = 0;
    ASSIGN_OR_RETURN(color, ReadDungeonPaletteColor(*rom, selection.selected,
                                                    *requested_index));
    formatter.AddField("color_index", *requested_index);
    formatter.AddHexField("color_pc", color_pc, 6);
    formatter.AddHexField("color_snes_address", PcToSnes(color_pc), 6);
    formatter.AddHexField("color_snes", color, 4);
  } else {
    formatter.BeginArray("colors");
    for (int color_index = 0; color_index < kDungeonPaletteColorCount;
         ++color_index) {
      const uint32_t color_pc =
          DungeonPaletteColorPc(selection.selected, color_index);
      uint16_t color = 0;
      ASSIGN_OR_RETURN(color, ReadDungeonPaletteColor(*rom, selection.selected,
                                                      color_index));
      formatter.BeginObject();
      formatter.AddField("index", color_index);
      formatter.AddHexField("pc", color_pc, 6);
      formatter.AddHexField("snes_address", PcToSnes(color_pc), 6);
      formatter.AddHexField("snes_color", color, 4);
      formatter.EndObject();
    }
    formatter.EndArray();
  }

  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-set-palette-color
// ---------------------------------------------------------------------------

absl::Status DungeonSetPaletteColorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const bool do_write = parser.HasFlag("write");
#if defined(__EMSCRIPTEN__)
  if (do_write) {
    return absl::FailedPreconditionError(
        "dungeon-set-palette-color --write is unavailable in the browser "
        "terminal because durable filesystem sync cannot be guaranteed; use "
        "native z3ed on a disposable ROM copy");
  }
#endif
  if (do_write && rom->filename().empty()) {
    return absl::FailedPreconditionError(
        "dungeon-set-palette-color --write requires a ROM loaded from a "
        "filesystem path");
  }

  int room_id = 0;
  int color_index = 0;
  int expected_palette_set = 0;
  int expected_palette_index = 0;
  int expected_color = 0;
  int replacement_color = 0;
  ASSIGN_OR_RETURN(room_id, GetRequiredHex(parser, "room"));
  ASSIGN_OR_RETURN(color_index, GetRequiredInt(parser, "index"));
  ASSIGN_OR_RETURN(expected_palette_set,
                   GetRequiredHex(parser, "expect-palette-set"));
  ASSIGN_OR_RETURN(expected_palette_index,
                   GetRequiredInt(parser, "expect-palette-index"));
  ASSIGN_OR_RETURN(expected_color, GetRequiredHex(parser, "expect-color"));
  ASSIGN_OR_RETURN(replacement_color, GetRequiredHex(parser, "color"));

  RETURN_IF_ERROR(ValidateRoomId(room_id));
  RETURN_IF_ERROR(ValidateDungeonPaletteColorIndex(color_index));
  if (expected_palette_set < 0 || expected_palette_set > 0xFF) {
    return absl::InvalidArgumentError(
        "--expect-palette-set must be an 8-bit value in range 0x00-0xFF");
  }
  if (expected_palette_index < 0 ||
      expected_palette_index >= kDungeonPaletteCount) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--expect-palette-index must be in range 0-%d",
                        kDungeonPaletteCount - 1));
  }
  RETURN_IF_ERROR(ValidateSnes15BitColor(expected_color, "expect-color"));
  RETURN_IF_ERROR(ValidateSnes15BitColor(replacement_color, "color"));

  const std::string manifest_path = parser.GetString("manifest").value();
  core::HackManifest manifest;
  RETURN_IF_ERROR(manifest.LoadFromFile(manifest_path));

  DungeonPaletteSelection selection;
  ASSIGN_OR_RETURN(selection, ResolveDungeonPaletteSelection(*rom, room_id));
  if (selection.selected.palette_set_id != expected_palette_set) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Palette-set CAS failed for room 0x%03X: expected full 8-bit ID "
        "0x%02X, got 0x%02X",
        room_id, expected_palette_set, selection.selected.palette_set_id));
  }
  if (selection.selected.palette_index != expected_palette_index) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Resolved palette CAS failed for room 0x%03X: expected %d, got %d",
        room_id, expected_palette_index, selection.selected.palette_index));
  }

  uint16_t actual_color = 0;
  ASSIGN_OR_RETURN(actual_color, ReadDungeonPaletteColor(
                                     *rom, selection.selected, color_index));
  if (actual_color != expected_color) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Color CAS failed at palette %d index %d: expected 0x%04X, got "
        "0x%04X",
        selection.selected.palette_index, color_index, expected_color,
        actual_color));
  }
  if (replacement_color == actual_color) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Refusing no-op dungeon palette write: color is already 0x%04X",
        actual_color));
  }

  const uint32_t color_pc =
      DungeonPaletteColorPc(selection.selected, color_index);
  RETURN_IF_ERROR(ValidateDungeonPaletteManifestRange(manifest, color_pc));
  if (do_write) {
    RETURN_IF_ERROR(ValidateDungeonPaletteWriteBaseline(*rom));
  }

  formatter.BeginObject("Set Dungeon Palette Color");
  AddDungeonPaletteScope(formatter, selection);
  formatter.AddField("manifest", manifest_path);
  formatter.AddField("manifest_write_policy", "block");
  formatter.AddField("mode", do_write ? "write" : "dry-run");
  formatter.AddField("color_index", color_index);
  formatter.AddHexField("color_pc", color_pc, 6);
  formatter.AddHexField("color_snes_address", PcToSnes(color_pc), 6);
  formatter.AddHexField("old_color", actual_color, 4);
  formatter.AddHexField("new_color", replacement_color, 4);
  formatter.AddField("preflight_status", "success");

  if (!do_write) {
    formatter.AddField("status", "dry-run");
    formatter.EndObject();
    return absl::OkStatus();
  }

  rom::WriteFence write_fence;
  RETURN_IF_ERROR(
      write_fence.Allow(color_pc, color_pc + 2u, "DungeonPaletteColor"));
  const std::vector<uint8_t> before = rom->vector();
  ScopedRomTransaction transaction(*rom);
  {
    rom::ScopedWriteFence scoped_fence(rom, &write_fence);
    RETURN_IF_ERROR(
        rom->WriteShort(color_pc, static_cast<uint16_t>(replacement_color)));
  }

  std::vector<uint32_t> changed_bytes;
  ASSIGN_OR_RETURN(changed_bytes,
                   ValidateDungeonPaletteWholeRomDiff(before, *rom, color_pc));
  uint16_t memory_color = 0;
  ASSIGN_OR_RETURN(memory_color, ReadDungeonPaletteColor(
                                     *rom, selection.selected, color_index));
  if (memory_color != replacement_color) {
    return absl::DataLossError(
        "In-memory dungeon palette readback did not match the replacement");
  }
  formatter.AddField("write_status", "success");
  formatter.AddField("whole_rom_diff_status", "verified");
  formatter.AddField("changed_byte_count",
                     static_cast<int>(changed_bytes.size()));
  formatter.BeginArray("changed_byte_pcs");
  for (uint32_t changed_pc : changed_bytes) {
    formatter.AddArrayItem(absl::StrFormat("0x%06X", changed_pc));
  }
  formatter.EndArray();
  formatter.AddField("memory_readback_status", "verified");

  const absl::Status save_status = SaveRomWithBackup(rom, formatter);
  if (!save_status.ok()) {
    formatter.AddField("status", "error");
    formatter.EndObject();
    return save_status;
  }

  // The atomic disk replacement has succeeded. Commit the caller's in-memory
  // transaction before external verification so a later reopen/readback
  // failure cannot roll memory back while leaving the persisted file changed.
  transaction.Commit();
  const absl::Status reopen_status = VerifyDungeonPaletteExternalReadback(
      *rom, room_id, expected_palette_set, expected_palette_index, color_index,
      static_cast<uint16_t>(replacement_color));
  if (!reopen_status.ok()) {
    formatter.AddField("readback_status", "external_reopen_failed");
    formatter.AddField("readback_error", std::string(reopen_status.message()));
    formatter.AddField("status", "error");
    formatter.EndObject();
    return reopen_status;
  }

  formatter.AddField("readback_status", "external_reopen_verified");
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-set-door-type
// ---------------------------------------------------------------------------

absl::Status DungeonSetDoorTypeCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  int room_id = 0;
  ASSIGN_OR_RETURN(room_id, GetRequiredHex(parser, "room"));
  RETURN_IF_ERROR(ValidateRoomId(room_id));

  int x = 0;
  int y = 0;
  ASSIGN_OR_RETURN(x, GetRequiredInt(parser, "x"));
  ASSIGN_OR_RETURN(y, GetRequiredInt(parser, "y"));
  if (x < 0 || x > 63 || y < 0 || y > 63) {
    return absl::InvalidArgumentError(
        "Door tile coordinates must be in range 0-63");
  }

  int new_type = 0;
  int expected_type = 0;
  ASSIGN_OR_RETURN(new_type, GetRequiredHex(parser, "type"));
  ASSIGN_OR_RETURN(expected_type, GetRequiredHex(parser, "expect-type"));
  RETURN_IF_ERROR(ValidateDoorType(new_type, "type"));
  RETURN_IF_ERROR(ValidateDoorType(expected_type, "expect-type"));

  std::optional<ObjectSaveManifestContext> manifest_context;
  ASSIGN_OR_RETURN(manifest_context, LoadObjectSaveManifestContext(parser));
  if (!manifest_context.has_value()) {
    return absl::FailedPreconditionError(
        "--manifest is required for dungeon-set-door-type");
  }

  DoorTypeTarget target;
  ASSIGN_OR_RETURN(
      target, ResolveDoorTypeTarget(rom, room_id, x, y, *manifest_context));
  const int old_type = static_cast<int>(target.door.type);
  if (old_type != expected_type) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Door type compare-and-swap failed at (%d, %d): expected 0x%02X, "
        "found 0x%02X",
        x, y, expected_type, old_type));
  }
  if (new_type == old_type) {
    return absl::InvalidArgumentError(
        "--type must differ from the current door type");
  }

  const bool do_write = parser.HasFlag("write");
  if (do_write && rom->filename().empty()) {
    return absl::FailedPreconditionError("Write mode requires a ROM filename");
  }

  formatter.BeginObject("Set Door Type");
  formatter.AddHexField("room_id", room_id, 3);
  formatter.AddField("door_index", target.door_index);
  formatter.AddField("x", x);
  formatter.AddField("y", y);
  formatter.AddField("position", target.door.position);
  formatter.AddField("direction", std::string(target.door.GetDirectionName()));
  formatter.AddHexField("old_type", old_type, 2);
  formatter.AddField("old_type_name", std::string(target.door.GetTypeName()));
  formatter.AddHexField("new_type", new_type, 2);
  formatter.AddField(
      "new_type_name",
      std::string(zelda3::GetDoorTypeName(
          zelda3::DoorTypeFromRaw(static_cast<uint8_t>(new_type)))));
  formatter.AddHexField("object_stream_pc", target.object_stream_pc, 6);
  formatter.AddHexField("object_stream_end_pc", target.object_stream_end_pc, 6);
  formatter.AddHexField("door_pointer_pc", target.door_pointer_pc, 6);
  formatter.AddHexField("door_entry_pc", target.door_entry_pc, 6);
  formatter.AddHexField("type_pc", target.type_pc, 6);
  formatter.AddHexField("type_snes", PcToSnes(target.type_pc), 6);
  formatter.AddField("manifest", *parser.GetString("manifest"));
  formatter.AddField("manifest_write_policy", "block");
  formatter.AddField("stream_alias_status", "unique");
  formatter.AddField("mode", do_write ? "write" : "dry-run");
  formatter.AddField("preflight_status", "success");

  if (!do_write) {
    formatter.AddField("write_status", "not_requested");
    formatter.AddField("save_status", "not_requested");
    formatter.AddField("readback_status", "not_requested");
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }

  const auto finish_error = [&formatter](const absl::Status& status,
                                         const char* field) -> absl::Status {
    formatter.AddField("status", "error");
    formatter.AddField(field, std::string(status.message()));
    formatter.EndObject();
    return status;
  };
  const auto verify_readback =
      [&target, new_type](const DoorTypeTarget& readback) -> absl::Status {
    if (readback.door_index != target.door_index ||
        readback.type_pc != target.type_pc ||
        readback.door.position != target.door.position ||
        readback.door.direction != target.door.direction ||
        static_cast<int>(readback.door.type) != new_type) {
      return absl::DataLossError(
          "Door type readback does not match the requested change");
    }
    return absl::OkStatus();
  };

  rom::WriteFence write_fence;
  const absl::Status allow_status = write_fence.Allow(
      target.type_pc, target.type_pc + 1u, "dungeon door type");
  if (!allow_status.ok()) {
    return finish_error(allow_status, "write_error");
  }

  const std::vector<uint8_t> before = rom->vector();
  ScopedRomTransaction transaction(*rom);
  {
    rom::ScopedWriteFence write_scope(rom, &write_fence);
    const absl::Status write_status = rom->WriteByte(
        static_cast<int>(target.type_pc), static_cast<uint8_t>(new_type));
    if (!write_status.ok()) {
      return finish_error(write_status, "write_error");
    }
  }

  if (rom->size() != before.size()) {
    return finish_error(
        absl::DataLossError("Door type edit unexpectedly resized the ROM"),
        "write_error");
  }
  int changed_bytes = 0;
  uint32_t changed_pc = 0;
  for (uint32_t pc = 0; pc < before.size(); ++pc) {
    if (before[pc] != rom->data()[pc]) {
      ++changed_bytes;
      changed_pc = pc;
    }
  }
  if (changed_bytes != 1 || changed_pc != target.type_pc) {
    return finish_error(
        absl::DataLossError(absl::StrFormat(
            "Door type edit changed %d bytes; expected only PC 0x%06X",
            changed_bytes, target.type_pc)),
        "write_error");
  }

  auto memory_readback =
      ResolveDoorTypeTarget(rom, room_id, x, y, *manifest_context);
  if (!memory_readback.ok()) {
    return finish_error(memory_readback.status(), "readback_error");
  }
  const absl::Status memory_verify = verify_readback(*memory_readback);
  if (!memory_verify.ok()) {
    return finish_error(memory_verify, "readback_error");
  }
  formatter.AddField("readback_status", "pre_save_verified");
  formatter.AddField("write_status", "success");

  const absl::Status save_status = SaveRomWithBackup(rom, formatter);
  if (!save_status.ok()) {
    formatter.AddField("status", "error");
    formatter.EndObject();
    return save_status;
  }

  transaction.Commit();
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-list-pot-items
// ---------------------------------------------------------------------------

absl::Status DungeonListPotItemsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  int room_id = 0;
  ASSIGN_OR_RETURN(room_id, GetRequiredHex(parser, "room"));
  RETURN_IF_ERROR(ValidateRoomId(room_id));

  PotItemStreamView stream;
  ASSIGN_OR_RETURN(stream, ResolveRuntimePotItemStream(rom, room_id));

  formatter.BeginObject("Dungeon Pot Items");
  formatter.AddHexField("room_id", room_id, 3);
  formatter.AddHexField("pointer_slot_pc", stream.pointer_slot_pc, 6);
  formatter.AddHexField("stream_pc", stream.stream_pc, 6);
  formatter.AddHexField("stream_end_pc", stream.stream_end_pc, 6);
  formatter.AddField("item_count", static_cast<int>(stream.entries.size()));
  formatter.BeginArray("items");
  for (const PotItemEntryView& entry : stream.entries) {
    formatter.BeginObject();
    formatter.AddField("index", entry.index);
    formatter.AddHexField("position", entry.item.position, 4);
    formatter.AddField("tile_x", entry.item.GetTileX());
    formatter.AddField("tile_y", entry.item.GetTileY());
    formatter.AddHexField("item_id", entry.item.item, 2);
    formatter.AddHexField("item_pc", entry.item_pc, 6);
    formatter.AddHexField("item_snes", PcToSnes(entry.item_pc), 6);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-set-pot-item
// ---------------------------------------------------------------------------

absl::Status DungeonSetPotItemCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  int room_id = 0;
  int index = 0;
  int expected_position = 0;
  int expected_item = 0;
  int new_item = 0;
  ASSIGN_OR_RETURN(room_id, GetRequiredHex(parser, "room"));
  RETURN_IF_ERROR(ValidateRoomId(room_id));
  ASSIGN_OR_RETURN(index, GetRequiredInt(parser, "index"));
  ASSIGN_OR_RETURN(expected_position,
                   GetRequiredHex(parser, "expect-position"));
  ASSIGN_OR_RETURN(expected_item, GetRequiredHex(parser, "expect-item"));
  ASSIGN_OR_RETURN(new_item, GetRequiredHex(parser, "item"));
  if (expected_position < 0 || expected_position > 0xFFFF) {
    return absl::InvalidArgumentError(
        "--expect-position must be in range 0x0000-0xFFFF");
  }
  if (expected_item < 0 || expected_item > 0xFF) {
    return absl::InvalidArgumentError(
        "--expect-item must be in range 0x00-0xFF");
  }
  if (new_item < 0 || new_item > 0xFF) {
    return absl::InvalidArgumentError("--item must be in range 0x00-0xFF");
  }

  PotItemManifestContext manifest_context;
  ASSIGN_OR_RETURN(manifest_context, LoadPotItemManifestContext(parser));
  PotItemTarget target;
  ASSIGN_OR_RETURN(target,
                   ResolvePotItemTarget(rom, room_id, index, manifest_context));
  if (target.entry.item.position != expected_position ||
      target.entry.item.item != expected_item) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Pot-item compare-and-swap failed at index %d: expected "
        "position/item 0x%04X/0x%02X, found 0x%04X/0x%02X",
        index, expected_position, expected_item, target.entry.item.position,
        target.entry.item.item));
  }
  if (new_item == expected_item) {
    return absl::InvalidArgumentError(
        "--item must differ from the current pot item");
  }

  const bool do_write = parser.HasFlag("write");
  if (do_write && rom->filename().empty()) {
    return absl::FailedPreconditionError("Write mode requires a ROM filename");
  }

  formatter.BeginObject("Set Pot Item");
  formatter.AddHexField("room_id", room_id, 3);
  formatter.AddField("index", index);
  formatter.AddHexField("position", target.entry.item.position, 4);
  formatter.AddField("tile_x", target.entry.item.GetTileX());
  formatter.AddField("tile_y", target.entry.item.GetTileY());
  formatter.AddHexField("old_item", expected_item, 2);
  formatter.AddHexField("new_item", new_item, 2);
  formatter.AddHexField("pointer_slot_pc", target.pointer_slot_pc, 6);
  formatter.AddHexField("stream_pc", target.stream_pc, 6);
  formatter.AddHexField("stream_end_pc", target.stream_end_pc, 6);
  formatter.AddHexField("item_pc", target.entry.item_pc, 6);
  formatter.AddHexField("item_snes", PcToSnes(target.entry.item_pc), 6);
  formatter.AddField("manifest", *parser.GetString("manifest"));
  formatter.AddField("manifest_write_policy", "block");
  formatter.AddField("stream_alias_status", "unique");
  formatter.AddField("mode", do_write ? "write" : "dry-run");
  formatter.AddField("preflight_status", "success");

  if (!do_write) {
    formatter.AddField("write_status", "not_requested");
    formatter.AddField("save_status", "not_requested");
    formatter.AddField("readback_status", "not_requested");
    formatter.AddField("status", "success");
    formatter.EndObject();
    return absl::OkStatus();
  }

  const auto finish_error = [&formatter](const absl::Status& status,
                                         const char* field) -> absl::Status {
    formatter.AddField("status", "error");
    formatter.AddField(field, std::string(status.message()));
    formatter.EndObject();
    return status;
  };
  const auto verify_readback =
      [&target, new_item](const PotItemTarget& readback) -> absl::Status {
    if (readback.entry.index != target.entry.index ||
        readback.entry.entry_pc != target.entry.entry_pc ||
        readback.entry.item_pc != target.entry.item_pc ||
        readback.entry.item.position != target.entry.item.position ||
        readback.entry.item.item != new_item ||
        readback.stream_pc != target.stream_pc ||
        readback.stream_end_pc != target.stream_end_pc) {
      return absl::DataLossError(
          "Pot-item readback does not match the requested change");
    }
    return absl::OkStatus();
  };

  rom::WriteFence write_fence;
  const absl::Status allow_status = write_fence.Allow(
      target.entry.item_pc, target.entry.item_pc + 1u, "dungeon pot item");
  if (!allow_status.ok()) {
    return finish_error(allow_status, "write_error");
  }

  const std::vector<uint8_t> before = rom->vector();
  ScopedRomTransaction transaction(*rom);
  {
    rom::ScopedWriteFence write_scope(rom, &write_fence);
    const absl::Status write_status = rom->WriteByte(
        static_cast<int>(target.entry.item_pc), static_cast<uint8_t>(new_item));
    if (!write_status.ok()) {
      return finish_error(write_status, "write_error");
    }
  }

  if (rom->size() != before.size()) {
    return finish_error(
        absl::DataLossError("Pot-item edit unexpectedly resized the ROM"),
        "write_error");
  }
  int changed_bytes = 0;
  uint32_t changed_pc = 0;
  for (uint32_t pc = 0; pc < before.size(); ++pc) {
    if (before[pc] != rom->data()[pc]) {
      ++changed_bytes;
      changed_pc = pc;
    }
  }
  if (changed_bytes != 1 || changed_pc != target.entry.item_pc) {
    return finish_error(
        absl::DataLossError(absl::StrFormat(
            "Pot-item edit changed %d bytes; expected only PC 0x%06X",
            changed_bytes, target.entry.item_pc)),
        "write_error");
  }

  auto memory_readback =
      ResolvePotItemTarget(rom, room_id, index, manifest_context);
  if (!memory_readback.ok()) {
    return finish_error(memory_readback.status(), "readback_error");
  }
  const absl::Status memory_verify = verify_readback(*memory_readback);
  if (!memory_verify.ok()) {
    return finish_error(memory_verify, "readback_error");
  }
  formatter.AddField("readback_status", "pre_save_verified");
  formatter.AddField("write_status", "success");

  const absl::Status save_status = SaveRomWithBackup(rom, formatter);
  if (!save_status.ok()) {
    formatter.AddField("status", "error");
    formatter.EndObject();
    return save_status;
  }

  transaction.Commit();
  formatter.AddField("status", "success");
  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// dungeon-set-collision-tile
// ---------------------------------------------------------------------------

absl::Status DungeonSetCollisionTileCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str_or = GetRequiredString(parser, "room");
  if (!room_id_str_or.ok()) {
    return room_id_str_or.status();
  }
  auto tiles_str_or = GetRequiredString(parser, "tiles");
  if (!tiles_str_or.ok()) {
    return tiles_str_or.status();
  }
  const std::string room_id_str = room_id_str_or.value();
  const std::string tiles_str = tiles_str_or.value();

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError("Invalid room ID. Must be hex.");
  }
  auto room_status = ValidateRoomId(room_id);
  if (!room_status.ok()) {
    return room_status;
  }

  bool do_write = parser.HasFlag("write");

  // Parse tile specifications: "x,y,tile;x,y,tile;..."
  struct TileSpec {
    int x, y, tile;
  };
  std::vector<TileSpec> specs;

  for (absl::string_view entry :
       absl::StrSplit(tiles_str, ';', absl::SkipEmpty())) {
    std::vector<std::string> parts =
        absl::StrSplit(entry, ',', absl::SkipEmpty());
    if (parts.size() != 3) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid tile spec '%s'. Expected x,y,tile (e.g. 10,5,0xB7)", entry));
    }
    TileSpec spec;

    // x and y are decimal tile coords
    if (!absl::SimpleAtoi(parts[0], &spec.x)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid X coord '%s'", parts[0]));
    }
    if (!absl::SimpleAtoi(parts[1], &spec.y)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid Y coord '%s'", parts[1]));
    }
    if (!ParseHexString(parts[2], &spec.tile)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid tile value '%s'. Must be hex.", parts[2]));
    }

    // Validate ranges
    if (spec.x < 0 || spec.x > 63 || spec.y < 0 || spec.y > 63) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Tile coords (%d,%d) out of range (0-63)", spec.x, spec.y));
    }
    if (spec.tile < 0 || spec.tile > 0xFF) {
      return absl::InvalidArgumentError("Tile value must be 0x00-0xFF");
    }

    specs.push_back(spec);
  }

  if (specs.empty()) {
    return absl::InvalidArgumentError("No tile specs provided");
  }

  // Load room with custom collision data
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  formatter.BeginObject("Set Collision Tiles");
  formatter.AddHexField("room_id", room_id, 2);
  formatter.AddField("had_custom_collision", room.has_custom_collision());
  formatter.AddField("tile_count", static_cast<int>(specs.size()));
  formatter.AddField("mode", do_write ? "write" : "dry-run");

  // Apply each tile change
  formatter.BeginArray("changes");
  for (const auto& spec : specs) {
    uint8_t old_value = room.GetCollisionTile(spec.x, spec.y);
    room.SetCollisionTile(spec.x, spec.y, static_cast<uint8_t>(spec.tile));

    formatter.BeginObject();
    formatter.AddField("x", spec.x);
    formatter.AddField("y", spec.y);
    formatter.AddHexField("old_tile", old_value, 2);
    formatter.AddHexField("new_tile", spec.tile, 2);
    formatter.EndObject();
  }
  formatter.EndArray();

  if (do_write) {
    auto save_status =
        MutateAndSaveRomWithBackup(rom, formatter, [&rom, &room]() mutable {
          // Flush collision changes to ROM.
          std::array<zelda3::Room, 1> rooms_arr = {std::move(room)};
          return zelda3::SaveAllCollision(rom, absl::MakeSpan(rooms_arr));
        });
    if (!save_status.ok()) {
      formatter.EndObject();
      return save_status;
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
