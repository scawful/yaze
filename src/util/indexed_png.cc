#include "util/indexed_png.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "miniz/miniz.h"

namespace yaze::util {
namespace {

constexpr uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
// Refuse images this large rather than allocating for a hostile header.
constexpr uint64_t kMaxPixels = 64ull * 1024 * 1024;

void PutU32(std::vector<uint8_t>& out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value >> 24));
  out.push_back(static_cast<uint8_t>(value >> 16));
  out.push_back(static_cast<uint8_t>(value >> 8));
  out.push_back(static_cast<uint8_t>(value));
}

uint32_t GetU32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

void PutChunk(std::vector<uint8_t>& out, const char type[4],
              const std::vector<uint8_t>& data) {
  PutU32(out, static_cast<uint32_t>(data.size()));
  const size_t crc_start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  const mz_ulong crc =
      mz_crc32(MZ_CRC32_INIT, out.data() + crc_start, data.size() + 4);
  PutU32(out, static_cast<uint32_t>(crc));
}

uint8_t Paeth(uint8_t a, uint8_t b, uint8_t c) {
  const int p = int(a) + int(b) - int(c);
  const int pa = std::abs(p - int(a));
  const int pb = std::abs(p - int(b));
  const int pc = std::abs(p - int(c));
  if (pa <= pb && pa <= pc) {
    return a;
  }
  return pb <= pc ? b : c;
}

}  // namespace

absl::StatusOr<std::vector<uint8_t>> EncodeIndexedPng(
    int width, int height, const std::vector<uint8_t>& indices,
    const std::vector<std::array<uint8_t, 4>>& palette) {
  if (width <= 0 || height <= 0 ||
      indices.size() != static_cast<size_t>(width) * height) {
    return absl::InvalidArgumentError("PNG size does not match pixel count");
  }
  if (palette.empty() || palette.size() > 256) {
    return absl::InvalidArgumentError("PNG palette needs 1-256 entries");
  }
  for (uint8_t index : indices) {
    if (index >= palette.size()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Pixel index %d is past the %zu-entry palette", index,
                          palette.size()));
    }
  }

  std::vector<uint8_t> raw;
  raw.reserve(static_cast<size_t>(height) * (width + 1));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);  // filter: none
    raw.insert(raw.end(), indices.begin() + y * width,
               indices.begin() + (y + 1) * width);
  }
  mz_ulong compressed_size = mz_compressBound(raw.size());
  std::vector<uint8_t> compressed(compressed_size);
  if (mz_compress2(compressed.data(), &compressed_size, raw.data(), raw.size(),
                   MZ_BEST_COMPRESSION) != MZ_OK) {
    return absl::InternalError("PNG deflate failed");
  }
  compressed.resize(compressed_size);

  std::vector<uint8_t> out(kSignature, kSignature + 8);
  std::vector<uint8_t> header;
  PutU32(header, static_cast<uint32_t>(width));
  PutU32(header, static_cast<uint32_t>(height));
  header.insert(header.end(), {8, 3, 0, 0, 0});  // 8-bit indexed
  PutChunk(out, "IHDR", header);
  std::vector<uint8_t> plte;
  std::vector<uint8_t> trns;
  bool any_alpha = false;
  for (const auto& entry : palette) {
    plte.insert(plte.end(), {entry[0], entry[1], entry[2]});
    trns.push_back(entry[3]);
    any_alpha |= entry[3] != 255;
  }
  PutChunk(out, "PLTE", plte);
  if (any_alpha) {
    PutChunk(out, "tRNS", trns);
  }
  PutChunk(out, "IDAT", compressed);
  PutChunk(out, "IEND", {});
  return out;
}

absl::StatusOr<PngImage> DecodePng(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 8 || std::memcmp(bytes.data(), kSignature, 8) != 0) {
    return absl::InvalidArgumentError("Not a PNG file");
  }
  PngImage image;
  int bit_depth = 0;
  int color_type = -1;
  std::vector<uint8_t> idat;
  std::vector<uint8_t> trns;
  size_t pos = 8;
  bool seen_end = false;
  while (pos + 12 <= bytes.size()) {
    const uint32_t length = GetU32(&bytes[pos]);
    if (length > bytes.size() - pos - 12) {
      return absl::InvalidArgumentError("Truncated PNG chunk");
    }
    const std::string type(reinterpret_cast<const char*>(&bytes[pos + 4]), 4);
    const uint8_t* data = &bytes[pos + 8];
    const mz_ulong crc = mz_crc32(MZ_CRC32_INIT, &bytes[pos + 4], length + 4);
    if (crc != GetU32(data + length)) {
      return absl::InvalidArgumentError("PNG chunk " + type + " fails CRC");
    }
    if (type == "IHDR") {
      if (length != 13) {
        return absl::InvalidArgumentError("Bad PNG header");
      }
      image.width = static_cast<int>(GetU32(data));
      image.height = static_cast<int>(GetU32(data + 4));
      bit_depth = data[8];
      color_type = data[9];
      if (data[12] != 0) {
        return absl::UnimplementedError(
            "Interlaced PNGs are not supported; save without interlacing");
      }
    } else if (type == "PLTE") {
      for (uint32_t i = 0; i + 2 < length; i += 3) {
        image.palette.push_back({data[i], data[i + 1], data[i + 2], 255});
      }
    } else if (type == "tRNS") {
      trns.assign(data, data + length);
    } else if (type == "IDAT") {
      idat.insert(idat.end(), data, data + length);
    } else if (type == "IEND") {
      seen_end = true;
      break;
    }
    pos += 12 + length;
  }
  if (!seen_end || image.width <= 0 || image.height <= 0) {
    return absl::InvalidArgumentError("Incomplete PNG");
  }
  if (static_cast<uint64_t>(image.width) * image.height > kMaxPixels) {
    return absl::InvalidArgumentError("PNG is too large");
  }

  int channels = 0;
  switch (color_type) {
    case 0:
      channels = 1;
      break;
    case 2:
      channels = 3;
      break;
    case 3:
      channels = 1;
      break;
    case 4:
      channels = 2;
      break;
    case 6:
      channels = 4;
      break;
    default:
      return absl::InvalidArgumentError("Unknown PNG color type");
  }
  const bool indexed = color_type == 3;
  if (indexed ? (bit_depth != 1 && bit_depth != 2 && bit_depth != 4 &&
                 bit_depth != 8)
              : bit_depth != 8) {
    return absl::UnimplementedError(
        absl::StrFormat("PNG bit depth %d is not supported for color type %d",
                        bit_depth, color_type));
  }
  if (indexed && image.palette.empty()) {
    return absl::InvalidArgumentError("Indexed PNG has no palette");
  }
  for (size_t i = 0; i < trns.size() && i < image.palette.size(); ++i) {
    image.palette[i][3] = trns[i];
  }

  const size_t bits_per_pixel = static_cast<size_t>(bit_depth) * channels;
  const size_t row_bytes = (image.width * bits_per_pixel + 7) / 8;
  const size_t pixel_bytes = std::max<size_t>(1, bits_per_pixel / 8);
  mz_ulong raw_size = static_cast<mz_ulong>((row_bytes + 1) * image.height);
  std::vector<uint8_t> raw(raw_size);
  if (mz_uncompress(raw.data(), &raw_size, idat.data(), idat.size()) != MZ_OK ||
      raw_size != raw.size()) {
    return absl::InvalidArgumentError("PNG image data does not inflate");
  }

  std::vector<uint8_t> previous(row_bytes, 0);
  std::vector<uint8_t> row(row_bytes);
  image.indexed = indexed;
  if (indexed) {
    image.indices.resize(static_cast<size_t>(image.width) * image.height);
  } else {
    image.rgba.resize(static_cast<size_t>(image.width) * image.height * 4);
  }
  for (int y = 0; y < image.height; ++y) {
    const uint8_t* line = &raw[y * (row_bytes + 1)];
    const uint8_t filter = line[0];
    for (size_t x = 0; x < row_bytes; ++x) {
      const uint8_t a = x >= pixel_bytes ? row[x - pixel_bytes] : 0;
      const uint8_t b = previous[x];
      const uint8_t c = x >= pixel_bytes ? previous[x - pixel_bytes] : 0;
      uint8_t value = line[1 + x];
      switch (filter) {
        case 0:
          break;
        case 1:
          value += a;
          break;
        case 2:
          value += b;
          break;
        case 3:
          value += static_cast<uint8_t>((int(a) + int(b)) / 2);
          break;
        case 4:
          value += Paeth(a, b, c);
          break;
        default:
          return absl::InvalidArgumentError("Unknown PNG row filter");
      }
      row[x] = value;
    }
    for (int x = 0; x < image.width; ++x) {
      const size_t out = static_cast<size_t>(y) * image.width + x;
      if (indexed) {
        const size_t bit = static_cast<size_t>(x) * bit_depth;
        const uint8_t packed = row[bit / 8];
        const int shift = 8 - bit_depth - static_cast<int>(bit % 8);
        image.indices[out] =
            static_cast<uint8_t>((packed >> shift) & ((1 << bit_depth) - 1));
        continue;
      }
      const uint8_t* p = &row[static_cast<size_t>(x) * channels];
      uint8_t* dst = &image.rgba[out * 4];
      switch (color_type) {
        case 0:
          dst[0] = dst[1] = dst[2] = p[0];
          dst[3] = 255;
          break;
        case 4:
          dst[0] = dst[1] = dst[2] = p[0];
          dst[3] = p[1];
          break;
        case 2:
          dst[0] = p[0];
          dst[1] = p[1];
          dst[2] = p[2];
          dst[3] = 255;
          break;
        default:
          std::memcpy(dst, p, 4);
          break;
      }
    }
    std::swap(previous, row);
  }
  return image;
}

absl::StatusOr<std::vector<uint8_t>> ReadBinaryFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::NotFoundError("Cannot read " + path);
  }
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
}

absl::Status WriteBinaryFile(const std::string& path,
                             const std::vector<uint8_t>& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return absl::PermissionDeniedError("Cannot write " + path);
  }
  file.write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  if (!file) {
    return absl::DataLossError("Failed writing " + path);
  }
  return absl::OkStatus();
}

}  // namespace yaze::util
