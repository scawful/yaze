#include "zelda3/overworld/overworld_terrain_io.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

using Plane = std::vector<uint8_t>;

absl::StatusOr<rom::WriteFence> StorageFence(
    const OverworldTerrainStorage& storage, size_t image_size) {
  if ((storage.map_count != 160 && storage.map_count != 192) ||
      storage.stream_regions.empty() || image_size > 0x400000) {
    return absl::InvalidArgumentError(
        "Invalid overworld terrain capacity/storage");
  }
  auto ranges = storage.stream_regions;
  ranges.push_back({storage.high_table_pc,
                    storage.high_table_pc + storage.map_count * 3,
                    "terrain high pointers"});
  ranges.push_back({storage.low_table_pc,
                    storage.low_table_pc + storage.map_count * 3,
                    "terrain low pointers"});
  if (storage.native_prefix) {
    const auto& prefix = *storage.native_prefix;
    if (prefix.map_count != 160 || prefix.map_count > storage.map_count) {
      return absl::InvalidArgumentError("Invalid native terrain prefix count");
    }
    ranges.push_back({prefix.high_table_pc,
                      prefix.high_table_pc + prefix.map_count * 3,
                      "native high pointers"});
    ranges.push_back({prefix.low_table_pc,
                      prefix.low_table_pc + prefix.map_count * 3,
                      "native low pointers"});
  }
  rom::WriteFence fence;
  rom::WriteFence all_storage;
  for (const auto& range : ranges) {
    if (range.start >= range.end || range.end > image_size) {
      return absl::OutOfRangeError("Overworld terrain storage outside ROM");
    }
    RETURN_IF_ERROR(fence.Allow(range.start, range.end, range.label));
    RETURN_IF_ERROR(all_storage.Allow(range.start, range.end, range.label));
  }
  for (const auto& range : storage.read_only_stream_regions) {
    if (range.start >= range.end || range.end > image_size) {
      return absl::OutOfRangeError("Read-only terrain storage outside ROM");
    }
    RETURN_IF_ERROR(all_storage.Allow(range.start, range.end, range.label));
  }
  return fence;
}

uint32_t Pointer(const std::vector<uint8_t>& image, uint32_t slot) {
  return image[slot] | (image[slot + 1] << 8) | (image[slot + 2] << 16);
}

absl::StatusOr<MapStreamScan> ReadPlane(const std::vector<uint8_t>& image,
                                        const OverworldTerrainStorage& storage,
                                        uint32_t pointer_slot) {
  const uint32_t pointer = Pointer(image, pointer_slot);
  const uint32_t raw_bank = pointer >> 16;
  if ((pointer & 0x8000) == 0 || raw_bank == 0x7E || raw_bank == 0x7F) {
    return absl::FailedPreconditionError(
        "Invalid terrain LoROM stream pointer");
  }
  const uint32_t pc = (raw_bank & 0x7F) * 0x8000 + (pointer & 0x7FFF);
  const rom::WriteRange* containing = nullptr;
  for (const auto* regions :
       {&storage.stream_regions, &storage.read_only_stream_regions}) {
    for (const auto& region : *regions) {
      if (pc >= region.start && pc < region.end)
        containing = &region;
    }
  }
  if (containing) {
    const size_t limit =
        std::min<uint32_t>(containing->end, (pc / 0x8000 + 1) * 0x8000);
    auto scan = ScanMapStream(image.data(), limit, pc, 256);
    if (!scan.error.empty() || !scan.terminated || scan.decoded_bytes != 256) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Invalid terrain stream at 0x%06X: %s", pc, scan.error));
    }
    return scan;
  }
  return absl::FailedPreconditionError(
      "Terrain stream outside assigned storage");
}

Plane PagePlane(const OverworldTerrainPage& page, bool high) {
  Plane bytes;
  bytes.reserve(page.size());
  for (uint16_t tile : page)
    bytes.push_back(static_cast<uint8_t>(high ? tile >> 8 : tile));
  return bytes;
}

void AddWrite(OverworldTerrainSavePlan* plan, std::vector<uint8_t>* staged,
              uint32_t pc, const std::vector<uint8_t>& bytes) {
  auto start = plan->baseline.begin() + pc;
  if (!std::equal(bytes.begin(), bytes.end(), start)) {
    plan->writes.push_back({pc, {start, start + bytes.size()}, bytes});
    std::copy(bytes.begin(), bytes.end(), staged->begin() + pc);
  }
}

}  // namespace

absl::StatusOr<OverworldTile32Packing> PlanOverworldTile32Packing(
    const OverworldMapTiles& tiles, int map_count, size_t definition_capacity) {
  if ((map_count != 160 && map_count != 192) || definition_capacity < 4 ||
      definition_capacity > 65536)
    return absl::InvalidArgumentError("Invalid Tile32 packing capacity");
  const std::array<const OverworldBlockset*, 3> grids = {
      &tiles.light_world, &tiles.dark_world, &tiles.special_world};
  for (size_t kind = 0; kind < grids.size(); ++kind) {
    const size_t rows = kind == 2 ? (map_count - 128) / 8 * 32 : 256;
    const auto& grid = *grids[kind];
    if (grid.size() < 256 || std::any_of(grid.begin(), grid.begin() + 256,
                                         [rows](const auto& column) {
                                           return column.size() < rows;
                                         }))
      return absl::FailedPreconditionError(
          "Overworld terrain grid is incomplete");
  }
  std::vector<uint64_t> packed;
  packed.reserve(map_count * 256);
  for (int map = 0; map < map_count; ++map) {
    const auto& grid = *grids[map / 64];
    const int sx = map % 8 * 32, sy = map % 64 / 8 * 32;
    for (int y = 0; y < 32; y += 2)
      for (int x = 0; x < 32; x += 2) {
        const gfx::Tile32 tile(grid[sx + x][sy + y], grid[sx + x + 1][sy + y],
                               grid[sx + x][sy + y + 1],
                               grid[sx + x + 1][sy + y + 1]);
        if (tile.tile0_ >= 4096 || tile.tile1_ >= 4096 || tile.tile2_ >= 4096 ||
            tile.tile3_ >= 4096)
          return absl::InvalidArgumentError(
              "Overworld terrain uses an undefined Tile16");
        packed.push_back(tile.GetPackedValue());
      }
  }
  const std::set<uint64_t> unique(packed.begin(), packed.end());
  if ((unique.size() + 3) / 4 * 4 > definition_capacity)
    return absl::ResourceExhaustedError("Tile32 dictionary is full");
  OverworldTile32Packing plan;
  std::map<uint64_t, uint16_t> ids;
  for (uint64_t value : unique) {
    ids.emplace(value, static_cast<uint16_t>(plan.definitions.size()));
    plan.definitions.emplace_back(value);
  }
  while (plan.definitions.size() % 4)
    plan.definitions.emplace_back(0, 0, 0, 0);
  plan.pages.resize(map_count);
  for (size_t i = 0; i < packed.size(); ++i)
    plan.pages[i / 256][i % 256] = ids.at(packed[i]);
  for (int q = 0; q < 4; ++q) {
    auto& bytes = plan.quadrant_bytes[q];
    bytes.reserve(plan.definitions.size() / 4 * 6);
    for (size_t i = 0; i < plan.definitions.size(); i += 4) {
      std::array<uint16_t, 4> values;
      for (size_t n = 0; n < 4; ++n) {
        const auto& t = plan.definitions[i + n];
        const std::array<uint16_t, 4> quadrants = {t.tile0_, t.tile1_, t.tile2_,
                                                   t.tile3_};
        values[n] = quadrants[q];
        bytes.push_back(static_cast<uint8_t>(values[n]));
      }
      bytes.push_back(((values[0] >> 4) & 0xF0) | ((values[1] >> 8) & 0xF));
      bytes.push_back(((values[2] >> 4) & 0xF0) | ((values[3] >> 8) & 0xF));
    }
  }
  return plan;
}

absl::StatusOr<OverworldTerrainStorage> ResolveNativeOverworldTerrainStorage(
    const Rom& rom) {
  const auto layout = rom.InspectLyraLayout();
  if (!rom::IsNativeSpecialOverworldLayout(layout))
    return absl::FailedPreconditionError("Native terrain directory required");
  auto entry = [&](int id) -> const rom::LyraLayoutEntry& {
    return *std::find_if(layout.entries.begin(), layout.entries.end(),
                         [id](const auto& e) { return e.id == id; });
  };
  OverworldTerrainStorage result;
  result.high_table_pc = entry(20).pc_address;
  result.low_table_pc = entry(21).pc_address;
  result.map_count = layout.capacity;
  for (int id : {200, 201, 202, 210, 211, 212, 213}) {
    const auto& e = entry(id);
    auto& ranges =
        id < 210 ? result.read_only_stream_regions : result.stream_regions;
    ranges.push_back(
        {e.pc_address, e.pc_address + e.count * e.stride,
         id < 210 ? "native source streams" : "native terrain streams"});
  }
  if (result.high_table_pc != 0x1794D || result.low_table_pc != 0x17B2D)
    result.native_prefix = OverworldTerrainPointerTables{0x1794D, 0x17B2D, 160};
  return result;
}

namespace {
absl::Status CheckNativeStorage(const Rom& rom,
                                const OverworldTerrainStorage& storage) {
  if (!rom::IsNativeSpecialOverworldLayout(rom.InspectLyraLayout()))
    return absl::OkStatus();
  ASSIGN_OR_RETURN(auto expected, ResolveNativeOverworldTerrainStorage(rom));
  auto equal_ranges = [](const auto& a, const auto& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                              [](const auto& x, const auto& y) {
                                                return x.start == y.start &&
                                                       x.end == y.end;
                                              });
  };
  const bool prefix_matches =
      storage.native_prefix.has_value() == expected.native_prefix.has_value() &&
      (!expected.native_prefix ||
       (storage.native_prefix->high_table_pc ==
            expected.native_prefix->high_table_pc &&
        storage.native_prefix->low_table_pc ==
            expected.native_prefix->low_table_pc &&
        storage.native_prefix->map_count == expected.native_prefix->map_count));
  if (storage.high_table_pc != expected.high_table_pc ||
      storage.low_table_pc != expected.low_table_pc ||
      storage.map_count != expected.map_count || !prefix_matches ||
      !equal_ranges(storage.stream_regions, expected.stream_regions) ||
      !equal_ranges(storage.read_only_stream_regions,
                    expected.read_only_stream_regions))
    return absl::FailedPreconditionError(
        "Terrain storage differs from the native directory");
  return absl::OkStatus();
}
}  // namespace

namespace {
absl::StatusOr<OverworldTerrainSavePlan> PlanTerrainSave(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages, bool initial_import) {
  RETURN_IF_ERROR(rom.CheckLayoutSaveable());
  if (initial_import) {
    if (rom.InspectLyraLayout().presence != rom::LyraLayoutPresence::kAbsent ||
        !storage.native_prefix ||
        storage.native_prefix->high_table_pc != 0x1794D ||
        storage.native_prefix->low_table_pc != 0x17B2D ||
        storage.native_prefix->map_count != 160)
      return absl::FailedPreconditionError(
          "Initial terrain import requires the original 160-map pointer tables "
          "and no existing native directory");
  } else {
    RETURN_IF_ERROR(CheckNativeStorage(rom, storage));
  }
  ASSIGN_OR_RETURN(auto fence, StorageFence(storage, rom.size()));
  if (pages.size() != static_cast<size_t>(storage.map_count)) {
    return absl::InvalidArgumentError(
        "Terrain page count differs from storage");
  }
  OverworldTerrainSavePlan plan{storage, rom.vector(), {}, initial_import};
  auto staged = plan.baseline;
  if (storage.native_prefix && !initial_import) {
    const auto& prefix = *storage.native_prefix;
    for (int map = 0; map < prefix.map_count; ++map) {
      if (Pointer(staged, prefix.high_table_pc + map * 3) !=
              Pointer(staged, storage.high_table_pc + map * 3) ||
          Pointer(staged, prefix.low_table_pc + map * 3) !=
              Pointer(staged, storage.low_table_pc + map * 3)) {
        return absl::FailedPreconditionError(
            "Native terrain pointers disagree with directory prefix");
      }
    }
  }
  struct Request {
    uint32_t slot;
    std::optional<MapStreamScan> old;
    Plane wanted;
  };
  std::vector<Request> requests;
  std::map<Plane, uint32_t> existing;
  std::map<uint32_t, int> stream_sizes;
  for (size_t map = 0; map < pages.size(); ++map) {
    for (bool high : {true, false}) {
      const uint32_t slot =
          (high ? storage.high_table_pc : storage.low_table_pc) + map * 3;
      std::optional<MapStreamScan> old;
      if (!initial_import || map < 160) {
        const uint32_t source_slot =
            initial_import ? (high ? storage.native_prefix->high_table_pc
                                   : storage.native_prefix->low_table_pc) +
                                 map * 3
                           : slot;
        ASSIGN_OR_RETURN(auto scan,
                         ReadPlane(plan.baseline, storage, source_slot));
        if (initial_import &&
            std::none_of(storage.read_only_stream_regions.begin(),
                         storage.read_only_stream_regions.end(),
                         [&](const auto& region) {
                           return scan.pc >= region.start &&
                                  scan.pc + scan.compressed_bytes <= region.end;
                         }))
          return absl::FailedPreconditionError(
              "Initial terrain source stream must be read-only");
        existing.try_emplace(scan.decoded, scan.pc);
        stream_sizes.emplace(scan.pc, scan.compressed_bytes);
        old = std::move(scan);
      }
      requests.push_back({slot, std::move(old), PagePlane(pages[map], high)});
    }
  }

  // Reclaim old streams only after choosing every surviving pointer. This
  // includes sharing across planes and pointers into another stream's span.
  std::vector<bool> occupied(rom.size(), false);
  std::vector<bool> reusable(rom.size(), false);
  for (const auto& request : requests) {
    if (request.old)
      std::fill_n(reusable.begin() + request.old->pc,
                  request.old->compressed_bytes, true);
  }
  std::map<Plane, uint32_t> placements;
  for (const auto& request : requests) {
    auto match = existing.find(request.wanted);
    if (match == existing.end())
      continue;
    // A no-op keeps its own pointer even when another identical stream exists.
    const uint32_t pc = request.old && request.old->decoded == request.wanted
                            ? request.old->pc
                            : match->second;
    placements.try_emplace(request.wanted, pc);
    std::fill_n(occupied.begin() + pc, stream_sizes.at(pc), true);
  }

  for (const auto& request : requests) {
    uint32_t pc = request.old ? request.old->pc : 0;
    const bool unchanged =
        request.old && request.old->decoded == request.wanted;
    if (!unchanged) {
      auto match = placements.find(request.wanted);
      if (match != placements.end()) {
        pc = match->second;
      } else {
        int compressed_size = 0;
        auto bytes = gfx::HyruleMagicCompress(request.wanted.data(), 256,
                                              &compressed_size, 1);
        if (bytes.empty() ||
            compressed_size != static_cast<int>(bytes.size())) {
          return absl::InternalError("Error compressing overworld terrain");
        }
        bool found = false;
        for (const auto& region : storage.stream_regions) {
          size_t run = 0;
          for (uint32_t at = region.start; at < region.end; ++at) {
            if ((at & 0x7FFF) == 0)
              run = 0;
            const bool available =
                !occupied[at] &&
                (reusable[at] || staged[at] == 0 || staged[at] == 0xFF);
            run = available ? run + 1 : 0;
            if (run == bytes.size()) {
              pc = at + 1 - bytes.size();
              AddWrite(&plan, &staged, pc, bytes);
              std::fill_n(occupied.begin() + pc, bytes.size(), true);
              placements.emplace(request.wanted, pc);
              found = true;
              break;
            }
          }
          if (found)
            break;
        }
        if (!found)
          return absl::ResourceExhaustedError(
              "Assigned overworld terrain stream storage is full");
      }
    }
    if (initial_import || !unchanged) {
      const uint32_t pointer = 0x808000 | ((pc / 0x8000) << 16) | (pc & 0x7FFF);
      AddWrite(
          &plan, &staged, request.slot,
          {static_cast<uint8_t>(pointer), static_cast<uint8_t>(pointer >> 8),
           static_cast<uint8_t>(pointer >> 16)});
    }
  }
  if (storage.native_prefix) {
    const auto& prefix = *storage.native_prefix;
    for (int map = 0; map < prefix.map_count; ++map) {
      for (bool high : {true, false}) {
        const uint32_t from =
            (high ? storage.high_table_pc : storage.low_table_pc) + map * 3;
        const uint32_t to =
            (high ? prefix.high_table_pc : prefix.low_table_pc) + map * 3;
        AddWrite(&plan, &staged, to,
                 {staged.begin() + from, staged.begin() + from + 3});
        ASSIGN_OR_RETURN(auto mirror, ReadPlane(staged, storage, to));
        if (mirror.decoded != PagePlane(pages[map], high)) {
          return absl::InternalError(
              "Staged native terrain prefix readback differs");
        }
      }
    }
  }
  for (const auto& request : requests) {
    ASSIGN_OR_RETURN(auto scan, ReadPlane(staged, storage, request.slot));
    if (scan.decoded != request.wanted)
      return absl::InternalError("Staged overworld terrain readback differs");
  }
  for (const auto& write : plan.writes)
    RETURN_IF_ERROR(fence.Check(write.pc, write.after.size(), "terrain plan"));
  return plan;
}
}  // namespace

absl::StatusOr<OverworldTerrainSavePlan> PlanOverworldTerrainSave(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages) {
  return PlanTerrainSave(rom, storage, pages, false);
}

absl::StatusOr<OverworldTerrainSavePlan> PlanInitialOverworldTerrainImport(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages) {
  return PlanTerrainSave(rom, storage, pages, true);
}

absl::Status ApplyOverworldTerrainSave(Rom& rom,
                                       const OverworldTerrainSavePlan& plan) {
  if (plan.initial_import)
    return absl::FailedPreconditionError(
        "Initial terrain import must publish with its complete dictionaries "
        "and native directory");
  RETURN_IF_ERROR(rom.CheckLayoutSaveable());
  RETURN_IF_ERROR(CheckNativeStorage(rom, plan.storage));
  ASSIGN_OR_RETURN(auto fence, StorageFence(plan.storage, rom.size()));
  if (rom.vector() != plan.baseline) {
    return absl::FailedPreconditionError(
        "ROM changed after terrain save was planned");
  }
  auto ordered = plan.writes;
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& a, const auto& b) { return a.pc < b.pc; });
  uint64_t previous_end = 0;
  for (const auto& write : ordered) {
    if (write.before.empty() || write.before.size() != write.after.size() ||
        write.pc < previous_end) {
      return absl::InvalidArgumentError(
          "Invalid or overlapping terrain save writes");
    }
    RETURN_IF_ERROR(fence.Check(write.pc, write.after.size(), "terrain save"));
    if (!std::equal(write.before.begin(), write.before.end(),
                    rom.vector().begin() + write.pc)) {
      return absl::FailedPreconditionError("Terrain save preimage differs");
    }
    previous_end = static_cast<uint64_t>(write.pc) + write.after.size();
  }
  if (ordered.empty())
    return absl::OkStatus();
  if (rom::IsNativeSpecialOverworldLayout(rom.InspectLyraLayout())) {
    auto candidate = plan.baseline;
    for (const auto& write : ordered)
      std::copy(write.after.begin(), write.after.end(),
                candidate.begin() + write.pc);
    return rom.CommitNativeTerrainImage(plan.baseline, std::move(candidate));
  }
  ScopedRomTransaction transaction(rom);
  rom::ScopedWriteFence scope(&rom, &fence);
  for (const auto& write : ordered)
    RETURN_IF_ERROR(rom.WriteVector(write.pc, write.after));
  transaction.Commit();
  return absl::OkStatus();
}

}  // namespace yaze::zelda3
