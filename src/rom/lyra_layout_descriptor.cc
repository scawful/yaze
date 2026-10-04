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
constexpr size_t kEntryCount = 35;
constexpr size_t kUsedSize = kHeaderSize + kEntryCount * kEntrySize;
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
    {201, 3, 16671, 1}, {202, 3, 32768, 1},
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

bool StreamPointerPc(size_t size, uint32_t snes, uint32_t* pc) {
  // Existing stream tables use $8B/$8C/$A6 cartridge mirrors. Only bit 23
  // is normalized; every result must still land in a declared stream window.
  return ToPc(snes & 0x7FFFFF, pc) && *pc < size && InStreamWindow(*pc);
}

bool DecodeStream(const uint8_t* image, size_t size, uint32_t start,
                  std::array<uint8_t, 256>* decoded) {
  size_t limit = std::min<size_t>(size, ((start / 0x8000) + 1) * 0x8000);
  for (const auto& [begin, end] : kStreamWindows) {
    if (start >= begin && start < end) {
      limit = std::min<size_t>(limit, end);
      break;
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
    if (command == 0xFF)
      return used == decoded->size();
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
  const bool directory_fingerprint =
      available >= kUsedSize && U16(d + 0x0A) == kHeaderSize &&
      U16(d + 0x0C) == kUsedSize && U16(d + 0x0E) == 192 &&
      U16(d + 0x10) == 160 && U16(d + 0x12) == kEntryCount &&
      U16(d + kHeaderSize) == 1 && U16(d + kHeaderSize + 2) == 1;
  if (!signature_prefix && !directory_fingerprint) {
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
  auto bad = [&](std::string error) {
    return Invalid(std::move(error), revision, capacity, active);
  };
  if (revision != 1)
    return bad("Unsupported Lyra ROM-format revision");
  if (U16(d + 0x0A) != kHeaderSize || U16(d + 0x0C) != kUsedSize ||
      U16(d + 0x12) != kEntryCount) {
    return bad("Invalid Lyra descriptor length or directory count");
  }
  if (capacity != 192 || active != 160) {
    return bad("Invalid Lyra capacity or active count");
  }
  if (d[0x14] != 0x3C || d[0x15] != 1 || U16(d + 0x16) != 384) {
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
  if (std::any_of(d + kUsedSize, d + kDescriptorSize,
                  [](uint8_t b) { return b != 0; })) {
    return bad("Nonzero Lyra descriptor padding");
  }

  LyraLayoutInspection result;
  result.presence = LyraLayoutPresence::kValid;
  result.revision = revision;
  result.capacity = capacity;
  result.active_count = active;
  result.write_supported = false;
  result.entries.reserve(kEntryCount);
  for (size_t i = 0; i < kEntryCount; ++i) {
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
    } else if (e.pc_address != kStreamWindows[i - 32].first) {
      return bad("Lyra stream region changed");
    }
    for (const auto& prior : result.entries) {
      const uint32_t prior_end = prior.pc_address + prior.count * prior.stride;
      if (e.pc_address < prior_end &&
          prior.pc_address < e.pc_address + length) {
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
      if (!DecodeStream(image, size, stream_pc, table == 19 ? &high : &low)) {
        return bad("Lyra compressed map stream invalid");
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
  }
  return result;
}

}  // namespace yaze::rom
