#include "rom/lyra_layout_descriptor.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace yaze::rom {
namespace {

constexpr size_t kDescriptorPc = 0x147000;
constexpr size_t kDescriptorSize = 1024;
constexpr size_t kHeaderSize = 32;
constexpr size_t kEntrySize = 16;
constexpr size_t kRevision1EntryCount = 35;
constexpr size_t kEntryCount = 36;
constexpr size_t kRevision1UsedSize =
    kHeaderSize + kRevision1EntryCount * kEntrySize;
constexpr char kMagic[] = "OOSOW192";

struct EntrySpec {
  uint16_t id;
  uint16_t kind;
  uint16_t count;
  uint16_t stride;
};

// Revision-1's directory order, domains and widths. Addresses are read from
// the descriptor and then checked against the audited allocation windows.
constexpr std::array<EntrySpec, kEntryCount> kSpecs = {{
    {1, 1, 192, 2},     {2, 1, 192, 1},     {3, 1, 192, 1},
    {4, 1, 192, 1},     {5, 1, 192, 2},     {6, 1, 192, 8},
    {7, 1, 192, 1},     {8, 1, 192, 2},     {9, 1, 192, 2},
    {10, 1, 192, 2},    {11, 1, 192, 2},    {12, 1, 192, 2},
    {13, 1, 192, 2},    {14, 1, 192, 2},    {15, 1, 192, 2},
    {16, 1, 192, 2},    {17, 1, 192, 2},    {18, 1, 192, 2},
    {19, 1, 192, 2},    {20, 1, 192, 3},    {21, 1, 192, 3},
    {22, 1, 192, 2},    {23, 1, 192, 2},    {24, 1, 64, 1},
    {25, 1, 64, 1},     {26, 1, 192, 1},    {27, 1, 64, 1},
    {28, 1, 128, 1},    {29, 1, 192, 1},    {30, 1, 192, 1},
    {31, 1, 192, 1},    {100, 2, 16384, 1}, {200, 3, 32368, 1},
    {201, 3, 16671, 1}, {202, 3, 32768, 1}, {203, 4, 2, 512},
}};

constexpr std::array<std::pair<uint32_t, uint32_t>, 3> kStreamWindows = {{
    {0x58000, 0x5FE70},
    {0x60000, 0x6411F},
    {0x130000, 0x138000},
}};

uint16_t U16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t U24(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16);
}

uint32_t U32(const uint8_t* p) {
  return U24(p) | (static_cast<uint32_t>(p[3]) << 24);
}

uint32_t Crc32(const uint8_t* p, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= p[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0U);
    }
  }
  return crc ^ 0xFFFFFFFF;
}

bool ToPc(uint32_t snes, uint32_t* pc) {
  const uint32_t bank = snes >> 16;
  if (bank > 0x3F || (snes & 0x8000) == 0) {
    return false;
  }
  *pc = bank * 0x8000 + (snes & 0x7FFF);
  return true;
}

bool NativeToPc(uint32_t snes, uint32_t* pc) {
  const uint32_t bank = snes >> 16;
  if (bank > 0xFF || bank == 0x7E || bank == 0x7F || (snes & 0x8000) == 0) {
    return false;
  }
  *pc = (bank & 0x7F) * 0x8000 + (snes & 0x7FFF);
  return true;
}

const LyraLayoutEntry* FindEntry(const LyraLayoutInspection& layout,
                                 uint16_t id) {
  const auto it = std::find_if(layout.entries.begin(), layout.entries.end(),
                               [id](const auto& e) { return e.id == id; });
  return it == layout.entries.end() ? nullptr : &*it;
}

LyraLayoutInspection Invalid(std::string error, uint16_t revision = 0,
                             uint16_t capacity = 0, uint16_t active = 0) {
  LyraLayoutInspection result;
  result.presence = LyraLayoutPresence::kInvalid;
  result.revision = revision;
  result.capacity = capacity;
  result.active_count = active;
  result.write_supported = false;
  result.error = std::move(error);
  return result;
}

bool InStreamWindow(uint32_t pc) {
  for (const auto& [begin, end] : kStreamWindows) {
    if (pc >= begin && pc < end) {
      return true;
    }
  }
  return false;
}

bool StreamPointerPc(size_t size, uint32_t snes, uint32_t* pc,
                     const LyraLayoutInspection* layout = nullptr) {
  if (layout != nullptr && layout->revision == 3) {
    if (!NativeToPc(snes, pc) || *pc >= size)
      return false;
    return std::any_of(layout->entries.begin(), layout->entries.end(),
                       [pc](const auto& e) {
                         return e.kind == 3 && *pc >= e.pc_address &&
                                *pc - e.pc_address < e.count * e.stride;
                       });
  }
  // Existing stream tables use $8B/$8C/$A6 cartridge mirrors. Only bit 23
  // is normalized; every result must still land in a declared stream window.
  return ToPc(snes & 0x7FFFFF, pc) && *pc < size && InStreamWindow(*pc);
}

bool DecodeStream(const uint8_t* image, size_t size, uint32_t start,
                  std::array<uint8_t, 256>* decoded,
                  size_t* consumed_end = nullptr,
                  const LyraLayoutInspection* layout = nullptr) {
  size_t limit = std::min<size_t>(size, ((start / 0x8000) + 1) * 0x8000);
  if (layout != nullptr && layout->revision == 3) {
    bool found = false;
    for (const auto& e : layout->entries) {
      if (e.kind == 3 && start >= e.pc_address &&
          start - e.pc_address < e.count * e.stride) {
        limit = std::min<size_t>(limit, e.pc_address + e.count * e.stride);
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  } else {
    for (const auto& [begin, end] : kStreamWindows) {
      if (start >= begin && start < end) {
        limit = std::min<size_t>(limit, end);
        break;
      }
    }
  }
  size_t p = start;
  size_t used = 0;
  auto take = [&](uint8_t* byte) {
    if (p >= limit)
      return false;
    *byte = image[p++];
    return true;
  };
  while (p < limit) {
    uint8_t command = 0;
    if (!take(&command))
      return false;
    if (command == 0xFF) {
      if (consumed_end != nullptr)
        *consumed_end = p;
      return used == decoded->size();
    }
    unsigned kind = 0;
    unsigned count = 0;
    if (command >= 0xE0) {
      uint8_t low = 0;
      if (!take(&low))
        return false;
      kind = (command >> 2) & 7;
      count = ((command & 3) << 8) + low + 1;
    } else {
      kind = command >> 5;
      count = (command & 31) + 1;
    }
    if (used + count > decoded->size())
      return false;
    uint8_t a = 0;
    uint8_t b = 0;
    if (kind == 0) {
      for (unsigned i = 0; i < count; ++i) {
        if (!take(&(*decoded)[used++]))
          return false;
      }
    } else if (kind == 1) {
      if (!take(&a))
        return false;
      std::fill_n(decoded->begin() + used, count, a);
      used += count;
    } else if (kind == 2) {
      if (!take(&a) || !take(&b))
        return false;
      for (unsigned i = 0; i < count; ++i) {
        (*decoded)[used++] = i % 2 ? b : a;
      }
    } else if (kind == 3) {
      if (!take(&a))
        return false;
      for (unsigned i = 0; i < count; ++i) {
        (*decoded)[used++] = static_cast<uint8_t>(a + i);
      }
    } else {
      if (!take(&a) || !take(&b))
        return false;
      const unsigned offset = (a << 8) | b;
      for (unsigned i = 0; i < count; ++i) {
        if (offset + i >= used)
          return false;
        (*decoded)[used++] = (*decoded)[offset + i];
      }
    }
  }
  return false;
}

bool CheckSpritePointer(const uint8_t* image, size_t size, uint16_t word) {
  if (word < 0xC000) {
    return false;
  }
  size_t pc = 0x1E0000 + (word & 0x7FFF);
  for (int record = 0; record <= 128; ++record) {
    if (pc >= 0x1E8000 || pc >= size) {
      return false;
    }
    if (image[pc] == 0xFF) {
      return true;
    }
    if (record == 128 || pc + 3 > 0x1E8000) {
      return false;
    }
    pc += 3;
  }
  return false;
}

std::optional<EntrySpec> NativeSpec(uint16_t id, uint16_t capacity) {
  constexpr std::array<uint16_t, 39> kStrides = {
      2, 1, 1, 1, 2, 8, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3,
      3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  if (id >= 1 && id <= kStrides.size())
    return EntrySpec{id, 1, capacity, kStrides[id - 1]};
  if (id == 100)
    return EntrySpec{id, 2, 0, 1};
  if (id >= 200 && id <= 202) {
    const auto [begin, end] = kStreamWindows[id - 200];
    return EntrySpec{id, 3, static_cast<uint16_t>(end - begin), 1};
  }
  if (id >= 210 && id <= 213)
    return EntrySpec{id, 3, 32768, 1};
  if (id == 300)
    return EntrySpec{id, 5, 0, 1};
  if (id == 301)
    return EntrySpec{id, 6, 0, 16};
  if (id == 400)
    return EntrySpec{id, 7, 4096, 8};
  if (id >= 401 && id <= 404)
    return EntrySpec{id, 7, 4432, 6};
  return std::nullopt;
}

LyraLayoutInspection InspectNativeLayout(const uint8_t* image, size_t size,
                                         const uint8_t* d) {
  const uint16_t capacity = U16(d + 0x0E);
  const uint16_t materialized = U16(d + 0x10);
  auto bad = [&](std::string error) {
    return Invalid(std::move(error), 3, capacity, materialized);
  };
  constexpr size_t kNativeEntryCount = 54;
  constexpr size_t kUsedSize = kHeaderSize + kNativeEntryCount * kEntrySize;
  if (U16(d + 0x0A) != kHeaderSize || U16(d + 0x0C) != kUsedSize ||
      U16(d + 0x12) != kNativeEntryCount) {
    return bad("Invalid native special-overworld directory size");
  }
  if ((capacity != 160 && capacity != 192) || materialized != capacity ||
      d[0x15] != 0x0C || U16(d + 0x16) != 2 * capacity) {
    return bad(
        "Invalid native special-overworld capacity, flags or phase stride");
  }
  if (size > 0x400000 || U32(d + 0x18) != size) {
    return bad("Native special-overworld image length mismatch");
  }
  if (image[0x140145] != 3 || image[0x1423FF] == 0xEA) {
    return bad("Conflicting ZS version or older $EA tail identity");
  }
  std::array<uint8_t, kDescriptorSize> checked;
  std::copy_n(d, checked.size(), checked.begin());
  std::fill_n(checked.begin() + 0x1C, 4, 0);
  if (Crc32(checked.data(), checked.size()) != U32(d + 0x1C) ||
      std::any_of(d + kUsedSize, d + kDescriptorSize,
                  [](uint8_t byte) { return byte != 0; })) {
    return bad("Native special-overworld descriptor CRC or padding invalid");
  }
  LyraLayoutInspection result;
  result.presence = LyraLayoutPresence::kValid;
  result.revision = 3;
  result.capacity = capacity;
  result.active_count = materialized;
  result.tail_actor_bank = d[0x14];
  result.write_supported = false;
  result.offline_only = false;
  result.entries.reserve(kNativeEntryCount);
  for (size_t i = 0; i < kNativeEntryCount; ++i) {
    const uint8_t* p = d + kHeaderSize + i * kEntrySize;
    LyraLayoutEntry e{U16(p),     U16(p + 2),  U32(p + 4), 0,
                      U16(p + 8), U16(p + 10), U32(p + 12)};
    const auto spec = NativeSpec(e.id, capacity);
    if (!spec || e.kind != spec->kind || e.stride != spec->stride ||
        e.count == 0 || (spec->count != 0 && e.count != spec->count) ||
        FindEntry(result, e.id) != nullptr) {
      return bad("Invalid or duplicate native special-overworld entry");
    }
    const uint32_t length = static_cast<uint32_t>(e.count) * e.stride;
    if (!NativeToPc(e.snes_address, &e.pc_address) ||
        (e.snes_address & 0xFFFF) + length > 0x10000 || e.pc_address > size ||
        length > size - e.pc_address) {
      return bad("Native special-overworld entry outside bank or image");
    }
    if (e.pc_address < kDescriptorPc + kDescriptorSize &&
        kDescriptorPc < e.pc_address + length) {
      return bad("Native special-overworld entry overlaps descriptor");
    }
    if (e.id >= 200 && e.id <= 202 &&
        e.pc_address != kStreamWindows[e.id - 200].first) {
      return bad("Native special-overworld source stream pool changed");
    }
    if (e.id >= 210 && e.id <= 213 && (e.snes_address & 0xFFFF) != 0x8000) {
      return bad("Native special-overworld writable pool is not bank-aligned");
    }
    if (e.id == 100 && (e.snes_address >> 16) != result.tail_actor_bank) {
      return bad("Native special-overworld actor bank mismatch");
    }
    for (const auto& prior : result.entries) {
      if (e.pc_address < prior.pc_address + prior.count * prior.stride &&
          prior.pc_address < e.pc_address + length) {
        return bad("Native special-overworld directory overlap");
      }
    }
    if (Crc32(image + e.pc_address, length) != e.crc32)
      return bad("Native special-overworld data CRC mismatch");
    result.entries.push_back(e);
  }
  // All 54 unique recognized IDs are required; directory order is not an ABI.
  const auto& parents = *FindEntry(result, 7);
  const auto& roles = *FindEntry(result, 29);
  const auto& sizes = *FindEntry(result, 30);
  const auto& actors = *FindEntry(result, 100);
  const auto& high_table = *FindEntry(result, 20);
  const auto& low_table = *FindEntry(result, 21);
  for (size_t map = 0; map < capacity; ++map) {
    const auto parent = image[parents.pc_address + map];
    const auto area_size = image[sizes.pc_address + map];
    const auto role = image[roles.pc_address + map];
    if (parent >= capacity || area_size > 3 || (role > 3 && role != 0xFF) ||
        image[parents.pc_address + parent] != parent ||
        image[sizes.pc_address + parent] != area_size) {
      return bad("Native special-overworld parent, size or role invalid");
    }
    // ZSCustomOverworld enum: small=0, large=1, wide=2, tall=3.
    const size_t width = (area_size == 1 || area_size == 2) ? 2 : 1;
    const size_t height = (area_size == 1 || area_size == 3) ? 2 : 1;
    if (parent / 64 != map / 64 || parent % 8 + width > 8 ||
        (parent % 64) / 8 + height > 8 || map < parent ||
        map - parent >= 8 * height || (map - parent) % 8 >= width) {
      return bad("Native special-overworld area footprint invalid");
    }
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        const size_t child = parent + y * 8 + x;
        if (child >= capacity || image[parents.pc_address + child] != parent ||
            image[sizes.pc_address + child] != area_size) {
          return bad("Native special-overworld area child mismatch");
        }
      }
    }
    std::array<uint8_t, 256> high{}, low{};
    for (const auto* table : {&high_table, &low_table}) {
      uint32_t pc = 0;
      if (!StreamPointerPc(size, U24(image + table->pc_address + map * 3), &pc,
                           &result) ||
          !DecodeStream(image, size, pc, table == &high_table ? &high : &low,
                        nullptr, &result)) {
        return bad("Native special-overworld terrain stream invalid");
      }
    }
    for (size_t tile = 0; tile < high.size(); ++tile) {
      if ((static_cast<uint16_t>(high[tile]) << 8 | low[tile]) >= 17728)
        return bad("Native special-overworld Tile32 reference out of range");
    }
    if (map < 0xA0)
      continue;
    for (uint16_t id : {16, 17, 18}) {
      const auto& pointers = *FindEntry(result, id);
      uint32_t pc = 0;
      if (!NativeToPc((result.tail_actor_bank << 16) |
                          U16(image + pointers.pc_address + map * 2),
                      &pc) ||
          pc < actors.pc_address || pc - actors.pc_address >= actors.count) {
        return bad("Native special-overworld actor pointer outside storage");
      }
      const size_t end = actors.pc_address + actors.count;
      size_t count = 0;
      while (pc < end && image[pc] != 0xFF && count < 128) {
        if (end - pc < 3)
          return bad("Native special-overworld actor record truncated");
        pc += 3;
        ++count;
      }
      if (pc >= end || image[pc] != 0xFF)
        return bad("Native special-overworld actor list unterminated");
    }
  }
  const auto& code = *FindEntry(result, 300);
  const auto& bindings = *FindEntry(result, 301);
  std::array<bool, 4> terrain_bound{};
  bool actor_bound = false;
  std::vector<uint32_t> sites;
  for (size_t i = 0; i < bindings.count; ++i) {
    const uint8_t* p = image + bindings.pc_address + i * bindings.stride;
    uint32_t site = 0, target = 0;
    const auto* table = FindEntry(result, U16(p + 8));
    const uint8_t kind = p[10];
    if (!NativeToPc(U32(p), &site) || !NativeToPc(U32(p + 4), &target) ||
        (U32(p) & 0xFFFF) + 4 > 0x10000 || site > size || 4 > size - site ||
        target < code.pc_address || target - code.pc_address >= code.count ||
        table == nullptr || table->kind != 1 || table->id == 29 || kind < 1 ||
        kind > 5 || p[11] != 0 || (p[12] != 0x22 && p[12] != 0x5C) ||
        U24(p + 13) != U32(p + 4) || U32(image + site) != U32(p + 12)) {
      return bad("Native special-overworld reader binding invalid");
    }
    if ((site < kDescriptorPc + kDescriptorSize && kDescriptorPc < site + 4) ||
        std::any_of(result.entries.begin(), result.entries.end(),
                    [site](const auto& e) {
                      return site < e.pc_address + e.count * e.stride &&
                             e.pc_address < site + 4;
                    }) ||
        std::any_of(sites.begin(), sites.end(), [site](uint32_t prior) {
          return site < prior + 4 && prior < site + 4;
        })) {
      return bad("Native special-overworld reader site overlap");
    }
    sites.push_back(site);
    if (kind == 1 || kind == 2) {
      constexpr std::array<uint32_t, 4> kSites = {0x02F59D, 0x02F5C8, 0x02F7E3,
                                                  0x02F80E};
      const auto it = std::find(kSites.begin(), kSites.end(), U32(p));
      if (it == kSites.end())
        return bad("Native special-overworld terrain reader site changed");
      const size_t index = it - kSites.begin();
      if (kind != (index < 2 ? 1 : 2) || table->id != 20 + index % 2 ||
          p[12] != 0x22)
        return bad("Native special-overworld terrain reader ABI mismatch");
      terrain_bound[index] = true;
    }
    if (kind == 4) {
      if (table->id < 16 || table->id > 18 || p[12] != 0x5C)
        return bad("Native special-overworld actor reader ABI mismatch");
      actor_bound = true;
    }
  }
  if (!actor_bound || std::any_of(terrain_bound.begin(), terrain_bound.end(),
                                  [](bool bound) { return !bound; })) {
    return bad("Native special-overworld required reader binding missing");
  }
  for (uint16_t id : {20, 21}) {
    const auto& e = *FindEntry(result, id);
    const size_t prefix = id == 20 ? 0x1794D : 0x17B2D;
    if (!std::equal(image + e.pc_address, image + e.pc_address + 160 * 3,
                    image + prefix))
      return bad("Native terrain pointer mirrors disagree");
  }
  return result;
}

}  // namespace

LyraLayoutInspection InspectLyraLayout(const uint8_t* image, size_t size) {
  LyraLayoutInspection absent;
  if (image == nullptr || size <= kDescriptorPc) {
    return absent;
  }
  const uint8_t* d = image + kDescriptorPc;
  const size_t available = std::min(kDescriptorSize, size - kDescriptorPc);
  const bool signature_prefix =
      available >= 5 && std::memcmp(d, "OOSOW", 5) == 0;
  // A damaged magic still belongs to this format when its independent header
  // and directory fingerprint survive. Nonblank bytes alone mean nothing.
  const bool legacy_fingerprint =
      available >= kRevision1UsedSize && U16(d + 0x0A) == kHeaderSize &&
      U16(d + 0x0E) == 192 && (U16(d + 0x10) == 160 || U16(d + 0x10) == 161) &&
      (U16(d + 0x12) == kRevision1EntryCount || U16(d + 0x12) == kEntryCount) &&
      U16(d + kHeaderSize) == 1 && U16(d + kHeaderSize + 2) == 1;
  const bool native_header =
      available == kDescriptorSize && U16(d + 8) == 3 &&
      U16(d + 0x0A) == kHeaderSize && U16(d + 0x0C) == 896 &&
      (U16(d + 0x0E) == 160 || U16(d + 0x0E) == 192) &&
      U16(d + 0x10) == U16(d + 0x0E) && U16(d + 0x12) == 54;
  bool native_fingerprint = false;
  if (native_header) {
    auto has_record = [&](uint16_t id, uint16_t kind, uint16_t count,
                          uint16_t stride) {
      for (size_t i = 0; i < U16(d + 0x12); ++i) {
        const auto* p = d + kHeaderSize + i * kEntrySize;
        if (U16(p) == id && U16(p + 2) == kind && U16(p + 8) == count &&
            U16(p + 10) == stride)
          return true;
      }
      return false;
    };
    native_fingerprint = has_record(1, 1, U16(d + 0x0E), 2) &&
                         has_record(20, 1, U16(d + 0x0E), 3) &&
                         has_record(400, 7, 4096, 8);
  }
  if (!signature_prefix && !legacy_fingerprint && !native_fingerprint) {
    return absent;
  }
  if (available != kDescriptorSize) {
    return Invalid("Truncated Lyra descriptor region");
  }
  if (std::memcmp(d, kMagic, 8) != 0) {
    return Invalid("Unknown Lyra descriptor magic");
  }

  const uint16_t revision = U16(d + 8);
  const uint16_t capacity = U16(d + 0x0E);
  const uint16_t active = U16(d + 0x10);
  if (revision == 3)
    return InspectNativeLayout(image, size, d);
  auto bad = [&](std::string error) {
    return Invalid(std::move(error), revision, capacity, active);
  };
  if (revision != 1 && revision != 2)
    return bad("Unsupported Lyra ROM-format revision");
  const size_t entry_count = revision == 1 ? kRevision1EntryCount : kEntryCount;
  const size_t used_size = kHeaderSize + entry_count * kEntrySize;
  if (U16(d + 0x0A) != kHeaderSize || U16(d + 0x0C) != used_size ||
      U16(d + 0x12) != entry_count) {
    return bad("Invalid Lyra descriptor length or directory count");
  }
  const bool offline_only = d[0x15] & 1;
  const bool supported_revision2 =
      revision == 2 &&
      ((d[0x15] == 3 && active == 160) || (d[0x15] == 2 && active == 161));
  if (capacity != 192 ||
      (revision == 1 ? active != 160 : !supported_revision2)) {
    return bad("Invalid Lyra capacity or active count");
  }
  if (d[0x14] != 0x3C || (revision == 1 && d[0x15] != 1) ||
      U16(d + 0x16) != 384) {
    return bad("Unsupported Lyra sprite bank, flags or phase stride");
  }
  if (U32(d + 0x18) != size) {
    return bad("Lyra image length mismatch");
  }
  if (image[0x140145] != 3 || image[0x1423FF] == 0xEA) {
    return bad("Conflicting ZS version or older $EA tail identity");
  }
  std::array<uint8_t, kDescriptorSize> checked;
  std::copy_n(d, kDescriptorSize, checked.begin());
  std::fill_n(checked.begin() + 0x1C, 4, 0);
  if (Crc32(checked.data(), checked.size()) != U32(d + 0x1C)) {
    return bad("Lyra descriptor CRC mismatch");
  }
  if (std::any_of(d + used_size, d + kDescriptorSize,
                  [](uint8_t b) { return b != 0; })) {
    return bad("Nonzero Lyra descriptor padding");
  }

  LyraLayoutInspection result;
  result.presence = LyraLayoutPresence::kValid;
  result.revision = revision;
  result.capacity = capacity;
  result.active_count = active;
  result.tail_actor_bank = d[0x14];
  result.write_supported = false;
  result.a0_terrain_write_supported = supported_revision2 && offline_only;
  result.offline_only = offline_only;
  result.entries.reserve(entry_count);
  for (size_t i = 0; i < entry_count; ++i) {
    const uint8_t* p = d + kHeaderSize + i * kEntrySize;
    const auto& spec = kSpecs[i];
    LyraLayoutEntry e{U16(p),     U16(p + 2),  U32(p + 4), 0,
                      U16(p + 8), U16(p + 10), U32(p + 12)};
    if (e.id != spec.id || e.kind != spec.kind || e.count != spec.count ||
        e.stride != spec.stride) {
      return bad("Invalid Lyra directory entry");
    }
    if (!ToPc(e.snes_address, &e.pc_address)) {
      return bad("Invalid Lyra directory bank or address");
    }
    const uint32_t length = static_cast<uint32_t>(e.count) * e.stride;
    if ((e.snes_address & 0xFFFF) + length > 0x10000) {
      return bad("Lyra table crosses bank");
    }
    if (e.pc_address > size || length > size - e.pc_address) {
      return bad("Lyra directory outside image extent");
    }
    if (i < 31) {
      if (i == 29 && e.snes_address != 0x02F88D) {
        return bad("Invalid Lyra area-size table address");
      }
      if (i == 30 && e.snes_address != 0x09C635) {
        return bad("Invalid Lyra palette-set table address");
      }
      if (i < 29 &&
          (e.pc_address < 0x144000 || e.pc_address + length > 0x146840)) {
        return bad("Lyra table outside owned bank-28 reservation");
      }
      if (i < 19 && (e.snes_address >> 16) != 0x28) {
        return bad("Lyra Pool table bank mismatch");
      }
      if (i > 0 && i < 19 &&
          result.entries.back().pc_address +
                  result.entries.back().count * result.entries.back().stride !=
              e.pc_address) {
        return bad("Lyra Pool table overlap or gap");
      }
    } else if (i == 31) {
      if (e.pc_address != 0x1E4000) {
        return bad("Lyra sprite list region changed");
      }
    } else if (i < 35 && e.pc_address != kStreamWindows[i - 32].first) {
      return bad("Lyra stream region changed");
    } else if (i == 35 && e.pc_address != kLyraA0HighStreamPc) {
      return bad("Lyra A0 stream reservation changed");
    }
    for (const auto& prior : result.entries) {
      const uint32_t prior_end = prior.pc_address + prior.count * prior.stride;
      if (e.pc_address < prior_end &&
          prior.pc_address < e.pc_address + length) {
        // Entry 203 subdivides region 202. No other directory overlap is legal.
        if (e.id == 203 && prior.id == 202 &&
            e.pc_address >= prior.pc_address &&
            e.pc_address + length <= prior_end) {
          continue;
        }
        return bad("Lyra directory overlap");
      }
    }
    result.entries.push_back(e);
  }
  if (result.entries[16].pc_address - result.entries[15].pc_address != 384 ||
      result.entries[17].pc_address - result.entries[16].pc_address != 384) {
    return bad("Lyra sprite phase stride mismatch");
  }
  for (const auto& e : result.entries) {
    const size_t length = static_cast<size_t>(e.count) * e.stride;
    if (Crc32(image + e.pc_address, length) != e.crc32) {
      return bad("Lyra data CRC mismatch");
    }
  }
  for (size_t map = 0; map < 192; ++map) {
    std::array<uint8_t, 256> high{};
    std::array<uint8_t, 256> low{};
    for (size_t table : {19U, 20U}) {
      const auto& e = result.entries[table];
      uint32_t stream_pc = 0;
      if (!StreamPointerPc(size, U24(image + e.pc_address + map * 3),
                           &stream_pc)) {
        return bad("Lyra map pointer outside owned stream regions");
      }
      size_t consumed_end = 0;
      if (!DecodeStream(image, size, stream_pc, table == 19 ? &high : &low,
                        &consumed_end)) {
        return bad("Lyra compressed map stream invalid");
      }
      if (supported_revision2) {
        const uint32_t slot_begin =
            table == 19 ? kLyraA0HighStreamPc : kLyraA0LowStreamPc;
        const uint32_t owned_end = kLyraA0LowStreamPc + kLyraA0StreamSlotSize;
        if (map == 0xA0) {
          if (stream_pc != slot_begin ||
              consumed_end > slot_begin + kLyraA0StreamSlotSize) {
            return bad("Lyra A0 stream exceeds its dedicated slot");
          }
        } else if (stream_pc < owned_end &&
                   consumed_end > kLyraA0HighStreamPc) {
          return bad("Lyra A0 stream reservation overlaps another map");
        }
      }
    }
    for (size_t tile = 0; tile < high.size(); ++tile) {
      if ((static_cast<uint16_t>(high[tile]) << 8 | low[tile]) >= 17728) {
        return bad("Lyra Tile32 reference exceeds existing capacity");
      }
    }
    for (size_t table : {15U, 16U, 17U}) {
      const auto& e = result.entries[table];
      if (!CheckSpritePointer(image, size,
                              U16(image + e.pc_address + map * 2))) {
        return bad("Lyra sprite pointer outside list region or unterminated");
      }
    }
    if (map >= active) {
      const auto& parent = result.entries[6];
      const auto& role = result.entries[28];
      const auto& overlay = result.entries[4];
      const auto& area_size = result.entries[29];
      if (image[parent.pc_address + map] != map ||
          image[role.pc_address + map] != 3 ||
          U16(image + overlay.pc_address + map * 2) != 0x00FF ||
          image[area_size.pc_address + map] != 0) {
        return bad("Lyra inactive tail role or parent is invalid");
      }
    }
    if (supported_revision2 && map == 0xA0 &&
        (image[result.entries[6].pc_address + map] != map ||
         image[result.entries[28].pc_address + map] != (offline_only ? 3 : 0) ||
         U16(image + result.entries[4].pc_address + map * 2) != 0x00FF ||
         image[result.entries[29].pc_address + map] != 0)) {
      return bad("Lyra A0 terrain role, parent, overlay or size is invalid");
    }
  }
  return result;
}

std::optional<std::span<const uint8_t>> LyraLayoutRecord(
    const LyraLayoutInspection& layout, const uint8_t* image, size_t size,
    uint16_t table_id, size_t index) {
  if (layout.presence != LyraLayoutPresence::kValid || image == nullptr) {
    return std::nullopt;
  }
  for (const auto& entry : layout.entries) {
    if (entry.id != table_id) {
      continue;
    }
    if (entry.kind != 1 || index >= entry.count) {
      return std::nullopt;
    }
    const size_t start = entry.pc_address + index * entry.stride;
    if (start > size || entry.stride > size - start) {
      return std::nullopt;
    }
    return std::span<const uint8_t>(image + start, entry.stride);
  }
  return std::nullopt;
}

std::optional<std::array<uint16_t, 256>> LyraMapTile32(
    const LyraLayoutInspection& layout, const uint8_t* image, size_t size,
    size_t map_index) {
  auto high_record = LyraLayoutRecord(layout, image, size, 20, map_index);
  auto low_record = LyraLayoutRecord(layout, image, size, 21, map_index);
  if (!high_record || !low_record || high_record->size() != 3 ||
      low_record->size() != 3) {
    return std::nullopt;
  }
  std::array<uint8_t, 256> high{};
  std::array<uint8_t, 256> low{};
  uint32_t high_pc = 0;
  uint32_t low_pc = 0;
  if (!StreamPointerPc(size, U24(high_record->data()), &high_pc, &layout) ||
      !StreamPointerPc(size, U24(low_record->data()), &low_pc, &layout) ||
      !DecodeStream(image, size, high_pc, &high, nullptr, &layout) ||
      !DecodeStream(image, size, low_pc, &low, nullptr, &layout)) {
    return std::nullopt;
  }
  std::array<uint16_t, 256> tiles{};
  for (size_t i = 0; i < tiles.size(); ++i) {
    tiles[i] = static_cast<uint16_t>((high[i] << 8) | low[i]);
    if (tiles[i] >= 17728) {
      return std::nullopt;
    }
  }
  return tiles;
}

absl::StatusOr<std::vector<uint8_t>> SealNativeOverworldTerrain(
    const std::vector<uint8_t>& baseline, std::vector<uint8_t> candidate,
    std::span<const NativeOverworldPropertyEdit> properties) {
  const auto layout = InspectLyraLayout(baseline.data(), baseline.size());
  if (layout.presence != LyraLayoutPresence::kValid || layout.revision != 3 ||
      candidate.size() != baseline.size()) {
    return absl::FailedPreconditionError(
        "Native terrain publication requires a valid unchanged image extent");
  }
  auto writable = [](uint16_t id) {
    return id == 20 || id == 21 || (id >= 210 && id <= 213) ||
           (id >= 400 && id <= 404);
  };
  std::vector<std::pair<size_t, size_t>> ranges = {
      {0x1794D, 0x1794D + 160 * 3}, {0x17B2D, 0x17B2D + 160 * 3}};
  for (const auto& e : layout.entries)
    if (writable(e.id))
      ranges.emplace_back(e.pc_address, e.pc_address + e.count * e.stride);
  for (size_t pc = 0; pc < baseline.size(); ++pc) {
    if (baseline[pc] == candidate[pc])
      continue;
    if (std::none_of(ranges.begin(), ranges.end(), [pc](const auto& r) {
          return pc >= r.first && pc < r.second;
        })) {
      return absl::PermissionDeniedError(
          "Native terrain candidate changes an unowned byte");
    }
  }
  for (uint16_t id : {20, 21}) {
    const auto& e = *FindEntry(layout, id);
    const size_t prefix = id == 20 ? 0x1794D : 0x17B2D;
    if (!std::equal(candidate.begin() + e.pc_address,
                    candidate.begin() + e.pc_address + 160 * 3,
                    candidate.begin() + prefix)) {
      return absl::FailedPreconditionError(
          "Native terrain pointer mirrors disagree");
    }
  }
  struct PropertyStorage {
    uint16_t id;
    size_t native_pc;
    uint16_t native_first;
    uint16_t native_count;
  };
  // Existing reader storage, not allocation inferred from unreferenced bytes.
  constexpr std::array<PropertyStorage, 21> kProperties = {{
      {1, 0x140000, 0, 160},  {2, 0x140160, 0, 160},  {3, 0x140200, 0, 160},
      {4, 0x1402A0, 0, 160},  {5, 0x140340, 0, 160},  {6, 0x140480, 0, 160},
      {19, 0x1417F8, 0, 160}, {24, 0x166E1, 128, 32}, {25, 0x16701, 128, 32},
      {26, 0x7C9C, 0, 160},   {27, 0x16821, 128, 32}, {28, 0x14403, 64, 96},
      {31, 0x4C635, 0, 160},  {32, 0x14303, 0, 64},   {33, 0x14343, 0, 64},
      {34, 0x14383, 0, 64},   {35, 0x143C3, 0, 64},   {36, 0x166E1, 128, 32},
      {37, 0x16701, 128, 32}, {38, 0x16701, 128, 32}, {39, 0x166E1, 128, 32},
  }};
  constexpr std::array<PropertyStorage, 19> kOtherNativeStorage = {{
      {7, 0x140998, 0, 160},
      {8, 0x140A38, 0, 160},
      {9, 0x140B78, 0, 160},
      {10, 0x140CB8, 0, 160},
      {11, 0x140DF8, 0, 160},
      {12, 0x140F38, 0, 160},
      {13, 0x141078, 0, 160},
      {14, 0x1411B8, 0, 160},
      {15, 0x1412F8, 0, 160},
      {16, 0x141438, 0, 160},
      {17, 0x141578, 0, 160},
      {18, 0x1416B8, 0, 160},
      {20, 0x1794D, 0, 160},
      {21, 0x17B2D, 0, 160},
      {22, 0x12644, 0, 160},
      {23, 0x12784, 0, 160},
      {30, 0x1788D, 0, 160},
      // Native LW/DW actor selectors are not expanded directory properties.
      {500, 0x7A41, 0, 256},
      {501, 0x7B41, 0, 256},
  }};
  std::vector<uint16_t> changed_properties;
  auto changed = [&](uint16_t id) {
    if (std::find(changed_properties.begin(), changed_properties.end(), id) ==
        changed_properties.end())
      changed_properties.push_back(id);
  };
  // Validate the whole batch before publishing anything to the caller.
  std::vector<std::pair<uint16_t, uint16_t>> edited_slots;
  for (const auto& edit : properties) {
    const auto storage =
        std::find_if(kProperties.begin(), kProperties.end(),
                     [&](const auto& p) { return p.id == edit.table_id; });
    const auto* entry = FindEntry(layout, edit.table_id);
    const auto key = std::pair{edit.table_id, edit.map_id};
    if (storage == kProperties.end() || entry == nullptr ||
        edit.map_id >= layout.capacity || edit.map_id < storage->native_first ||
        edit.value.size() != entry->stride ||
        std::find(edited_slots.begin(), edited_slots.end(), key) !=
            edited_slots.end())
      return absl::InvalidArgumentError(
          "Invalid, unsupported or duplicate native property edit");
    edited_slots.push_back(key);
    const size_t at = entry->pc_address + edit.map_id * entry->stride;
    // A valid directory may relocate a full-capacity table onto an original
    // shorter table. Its tail must not become permission to change the next
    // native family, even when all directory CRCs would still validate.
    for (const auto& native_ranges :
         {std::span<const PropertyStorage>(kProperties),
          std::span<const PropertyStorage>(kOtherNativeStorage)}) {
      for (const auto& native : native_ranges) {
        const size_t stride =
            native.id >= 500 ? 1 : FindEntry(layout, native.id)->stride;
        const size_t end = native.native_pc + native.native_count * stride;
        if (at >= end || native.native_pc >= at + entry->stride)
          continue;
        const bool exact_mirror =
            edit.map_id < storage->native_first + storage->native_count &&
            native.native_pc == storage->native_pc &&
            native.native_first == storage->native_first &&
            native.native_count == storage->native_count &&
            stride == entry->stride &&
            at == storage->native_pc +
                      (edit.map_id - storage->native_first) * entry->stride;
        if (!exact_mirror)
          return absl::PermissionDeniedError(
              "Native property allocation aliases another native table");
      }
    }
    if (!std::equal(edit.value.begin(), edit.value.end(),
                    candidate.begin() + at)) {
      std::copy(edit.value.begin(), edit.value.end(), candidate.begin() + at);
      changed(edit.table_id);
    }
  }
  for (const auto& edit : properties) {
    const auto& entry = *FindEntry(layout, edit.table_id);
    const size_t at = entry.pc_address + edit.map_id * entry.stride;
    if (std::equal(candidate.begin() + at,
                   candidate.begin() + at + entry.stride,
                   baseline.begin() + at))
      continue;
    auto storage =
        *std::find_if(kProperties.begin(), kProperties.end(),
                      [&](const auto& p) { return p.id == edit.table_id; });
    // Native dark/special music has one slot, unlike the tail's four phases.
    // Do not silently collapse an authored phase difference into that slot.
    const bool shared_music =
        edit.map_id >= 64 && edit.map_id < 160 &&
        (edit.table_id == 28 || (edit.table_id >= 32 && edit.table_id <= 35));
    std::array<uint16_t, 4> aliases{};
    size_t alias_count = 0;
    if (shared_music) {
      aliases = {32, 33, 34, 35};
      alias_count = 4;
      storage = {28, 0x14403, 64, 96};
    } else if (edit.map_id < 160) {
      if (edit.table_id == 24 || edit.table_id == 36 || edit.table_id == 39) {
        aliases = {24, 36, 39, 0};
        alias_count = 3;
      } else if (edit.table_id == 25 || edit.table_id == 37 ||
                 edit.table_id == 38) {
        aliases = {25, 37, 38, 0};
        alias_count = 3;
      }
    }
    for (size_t i = 0; i < alias_count; ++i) {
      const auto& alias = *FindEntry(layout, aliases[i]);
      if (candidate[alias.pc_address + edit.map_id] != candidate[at])
        return absl::FailedPreconditionError(
            "Native property phases share one legacy slot");
    }
    if (shared_music && edit.table_id != 28) {
      const auto& packed = *FindEntry(layout, 28);
      candidate[packed.pc_address + edit.map_id] = candidate[at];
      changed(28);
    }
    if (edit.map_id >= storage.native_first + storage.native_count)
      continue;
    const size_t native =
        storage.native_pc + (edit.map_id - storage.native_first) * entry.stride;
    if (native > candidate.size() || entry.stride > candidate.size() - native)
      return absl::OutOfRangeError("Native property mirror is outside ROM");
    for (const auto& other : layout.entries) {
      const size_t end = other.pc_address + other.count * other.stride;
      if (native < end && other.pc_address < native + entry.stride &&
          (other.id != edit.table_id || native != at))
        return absl::PermissionDeniedError(
            "Native property mirror overlaps another directory allocation");
    }
    std::copy_n(candidate.begin() + at, entry.stride,
                candidate.begin() + native);
  }
  auto put32 = [&](size_t pc, uint32_t value) {
    for (int i = 0; i < 4; ++i)
      candidate[pc + i] = static_cast<uint8_t>(value >> (i * 8));
  };
  for (size_t i = 0; i < layout.entries.size(); ++i) {
    const auto& e = layout.entries[i];
    if (writable(e.id) ||
        std::find(changed_properties.begin(), changed_properties.end(), e.id) !=
            changed_properties.end())
      put32(kDescriptorPc + kHeaderSize + i * kEntrySize + 12,
            Crc32(candidate.data() + e.pc_address, e.count * e.stride));
  }
  put32(kDescriptorPc + 28, 0);
  put32(kDescriptorPc + 28,
        Crc32(candidate.data() + kDescriptorPc, kDescriptorSize));
  const auto checked = InspectLyraLayout(candidate.data(), candidate.size());
  if (checked.presence != LyraLayoutPresence::kValid || checked.revision != 3)
    return absl::FailedPreconditionError(
        "Native terrain candidate is invalid: " + checked.error);
  return candidate;
}

absl::StatusOr<std::vector<uint8_t>> SerializeLyraA0Terrain(
    const std::vector<uint8_t>& baseline,
    const std::array<uint16_t, 256>& tiles) {
  const auto layout = InspectLyraLayout(baseline.data(), baseline.size());
  if (layout.presence != LyraLayoutPresence::kValid ||
      !layout.a0_terrain_write_supported) {
    return absl::FailedPreconditionError(
        "Revision-2 A0 terrain capability required for Save As");
  }
  if (std::any_of(tiles.begin(), tiles.end(), [](uint16_t tile) {
        return tile != 0x2103 && tile != 0x20B2;
      })) {
    return absl::InvalidArgumentError(
        "Lyra pilot terrain supports only existing Tile32 2103 and 20B2");
  }
  for (size_t y : {7U, 8U}) {
    for (size_t x : {7U, 8U}) {
      if (tiles[y * 16 + x] != 0x2103) {
        return absl::FailedPreconditionError(
            "Lyra A0 entry and return anchors must remain walkable");
      }
    }
  }
  auto staged = baseline;
  // A literal block has bounded size: two-byte command + 256 bytes + FF.
  // Preserve all unused slot bytes; never repack shared map-stream storage.
  for (int plane = 0; plane < 2; ++plane) {
    const size_t pc = plane == 0 ? kLyraA0HighStreamPc : kLyraA0LowStreamPc;
    staged[pc] = 0xE0;
    staged[pc + 1] = 0xFF;
    for (size_t tile = 0; tile < tiles.size(); ++tile) {
      staged[pc + 2 + tile] =
          static_cast<uint8_t>(plane == 0 ? tiles[tile] >> 8 : tiles[tile]);
    }
    staged[pc + 258] = 0xFF;
  }
  auto put32 = [&](size_t pc, uint32_t value) {
    for (size_t i = 0; i < 4; ++i)
      staged[pc + i] = static_cast<uint8_t>(value >> (8 * i));
  };
  for (size_t index : {34U, 35U}) {
    const auto& e = layout.entries[index];
    put32(kDescriptorPc + kHeaderSize + index * kEntrySize + 12,
          Crc32(staged.data() + e.pc_address, e.count * e.stride));
  }
  put32(kDescriptorPc + 28, 0);
  put32(kDescriptorPc + 28,
        Crc32(staged.data() + kDescriptorPc, kDescriptorSize));
  for (size_t pc = 0; pc < baseline.size(); ++pc) {
    const bool allowed =
        (pc >= kLyraA0HighStreamPc && pc < kLyraA0HighStreamPc + 259) ||
        (pc >= kLyraA0LowStreamPc && pc < kLyraA0LowStreamPc + 259) ||
        (pc >= kDescriptorPc + 28 && pc < kDescriptorPc + 32) ||
        (pc >= kDescriptorPc + 32 + 34 * 16 + 12 &&
         pc < kDescriptorPc + 32 + 34 * 16 + 16) ||
        (pc >= kDescriptorPc + 32 + 35 * 16 + 12 &&
         pc < kDescriptorPc + 32 + 35 * 16 + 16);
    if (!allowed && staged[pc] != baseline[pc]) {
      return absl::InternalError(
          "Lyra terrain staging changed an unowned byte");
    }
  }
  const auto checked = InspectLyraLayout(staged.data(), staged.size());
  if (checked.presence != LyraLayoutPresence::kValid ||
      LyraMapTile32(checked, staged.data(), staged.size(), 0xA0) != tiles) {
    return absl::InternalError("Lyra terrain staged image failed readback");
  }
  return staged;
}

}  // namespace yaze::rom
