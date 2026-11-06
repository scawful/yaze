#include "zelda3/overworld/overworld_terrain_io.h"

#include <algorithm>
#include <map>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "rom/transaction.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_allocation.h"

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
  rom::WriteFence fence;
  for (const auto& range : ranges) {
    if (range.start >= range.end || range.end > image_size) {
      return absl::OutOfRangeError("Overworld terrain storage outside ROM");
    }
    RETURN_IF_ERROR(fence.Allow(range.start, range.end, range.label));
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
  for (const auto& region : storage.stream_regions) {
    if (pc < region.start || pc >= region.end)
      continue;
    const size_t limit =
        std::min<uint32_t>(region.end, (pc / 0x8000 + 1) * 0x8000);
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

absl::StatusOr<OverworldTerrainSavePlan> PlanOverworldTerrainSave(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages) {
  RETURN_IF_ERROR(rom.CheckLayoutWritable());
  ASSIGN_OR_RETURN(auto fence, StorageFence(storage, rom.size()));
  if (pages.size() != static_cast<size_t>(storage.map_count)) {
    return absl::InvalidArgumentError(
        "Terrain page count differs from storage");
  }
  OverworldTerrainSavePlan plan{storage, rom.vector(), {}};
  auto staged = plan.baseline;
  struct Request {
    uint32_t slot;
    MapStreamScan old;
    Plane wanted;
  };
  std::vector<Request> requests;
  std::map<Plane, uint32_t> existing;
  std::map<uint32_t, int> stream_sizes;
  for (size_t map = 0; map < pages.size(); ++map) {
    for (bool high : {true, false}) {
      const uint32_t slot =
          (high ? storage.high_table_pc : storage.low_table_pc) + map * 3;
      ASSIGN_OR_RETURN(auto old, ReadPlane(plan.baseline, storage, slot));
      existing.try_emplace(old.decoded, old.pc);
      stream_sizes.emplace(old.pc, old.compressed_bytes);
      requests.push_back({slot, std::move(old), PagePlane(pages[map], high)});
    }
  }

  // Reclaim old streams only after choosing every surviving pointer. This
  // includes sharing across planes and pointers into another stream's span.
  std::vector<bool> occupied(rom.size(), false);
  std::vector<bool> reusable(rom.size(), false);
  for (const auto& request : requests) {
    std::fill_n(reusable.begin() + request.old.pc, request.old.compressed_bytes,
                true);
  }
  std::map<Plane, uint32_t> placements;
  for (const auto& request : requests) {
    auto match = existing.find(request.wanted);
    if (match == existing.end())
      continue;
    // A no-op keeps its own pointer even when another identical stream exists.
    const uint32_t pc =
        request.old.decoded == request.wanted ? request.old.pc : match->second;
    placements.try_emplace(request.wanted, pc);
    std::fill_n(occupied.begin() + pc, stream_sizes.at(pc), true);
  }

  for (const auto& request : requests) {
    uint32_t pc = request.old.pc;
    if (request.old.decoded != request.wanted) {
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
      const uint32_t pointer = 0x808000 | ((pc / 0x8000) << 16) | (pc & 0x7FFF);
      AddWrite(
          &plan, &staged, request.slot,
          {static_cast<uint8_t>(pointer), static_cast<uint8_t>(pointer >> 8),
           static_cast<uint8_t>(pointer >> 16)});
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

absl::Status ApplyOverworldTerrainSave(Rom& rom,
                                       const OverworldTerrainSavePlan& plan) {
  RETURN_IF_ERROR(rom.CheckLayoutWritable());
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
  ScopedRomTransaction transaction(rom);
  rom::ScopedWriteFence scope(&rom, &fence);
  for (const auto& write : ordered)
    RETURN_IF_ERROR(rom.WriteVector(write.pc, write.after));
  transaction.Commit();
  return absl::OkStatus();
}

}  // namespace yaze::zelda3
