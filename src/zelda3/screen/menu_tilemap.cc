#include "zelda3/screen/menu_tilemap.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "absl/strings/str_format.h"
#include "util/macro.h"

namespace yaze::zelda3 {
namespace {

namespace fs = std::filesystem;

absl::StatusOr<std::vector<uint8_t>> ReadWholeFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    return absl::NotFoundError(
        absl::StrFormat("could not open '%s' for reading", path));
  }
  std::streamsize size = in.tellg();
  if (size < 0) {
    return absl::InternalError(
        absl::StrFormat("could not determine size of '%s'", path));
  }
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(static_cast<size_t>(size));
  if (size > 0 && !in.read(reinterpret_cast<char*>(data.data()), size)) {
    return absl::InternalError(absl::StrFormat("short read on '%s'", path));
  }
  return data;
}

absl::Status WriteWholeFileAtomically(const std::string& path,
                                      const std::vector<uint8_t>& data) {
  fs::path dest(path);
  fs::path tmp = dest;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return absl::InternalError(
          absl::StrFormat("could not open '%s' for writing", tmp.string()));
    }
    if (!data.empty() &&
        !out.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()))) {
      return absl::InternalError(
          absl::StrFormat("short write on '%s'", tmp.string()));
    }
  }
  std::error_code ec;
  fs::rename(tmp, dest, ec);
  if (ec) {
    // Cross-device or other rename failure: fall back to copy+remove so a
    // save into a different filesystem (e.g. a mounted scratch dir) still
    // works, still ending with a single well-formed destination file.
    fs::copy_file(tmp, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
      return absl::InternalError(
          absl::StrFormat("could not move '%s' to '%s': %s", tmp.string(),
                          dest.string(), ec.message()));
    }
    std::error_code remove_ec;
    fs::remove(tmp, remove_ec);
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status MenuTilemapDocument::ValidateSize(size_t size) {
  if (size == 0 || size % kBytesPerRow != 0) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "menu tilemap size %zu is not a positive multiple of %d bytes "
        "(32 words/row * 2 bytes/word)",
        size, kBytesPerRow));
  }
  if (size > static_cast<size_t>(kMaxBytes)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("menu tilemap size %zu exceeds the maximum of %d "
                        "bytes (32x32 words)",
                        size, kMaxBytes));
  }
  return absl::OkStatus();
}

absl::Status MenuTilemapDocument::LoadFromBytes(std::vector<uint8_t> bytes,
                                                std::string source_path) {
  absl::Status size_status = ValidateSize(bytes.size());
  if (!size_status.ok()) {
    return size_status;
  }
  bytes_ = std::move(bytes);
  pristine_bytes_ = bytes_;
  rows_ = static_cast<int>(bytes_.size() / kBytesPerRow);
  path_ = std::move(source_path);
  dirty_ = false;
  backup_written_ = false;
  backup_path_.clear();
  return absl::OkStatus();
}

absl::Status MenuTilemapDocument::LoadFromFile(const std::string& path) {
  ASSIGN_OR_RETURN(std::vector<uint8_t> bytes, ReadWholeFile(path));
  return LoadFromBytes(std::move(bytes), path);
}

absl::Status MenuTilemapDocument::Save() {
  if (path_.empty()) {
    return absl::FailedPreconditionError(
        "MenuTilemapDocument::Save() called with no path set; use SaveAs()");
  }
  return SaveAs(path_);
}

absl::Status MenuTilemapDocument::SaveAs(const std::string& path) {
  // Saving must never change the file's byte size (Oracle's asm incbins a
  // fixed-size buffer at a fixed VRAM offset). Since edits only ever go
  // through SetCell/SetCellWord (which write into the existing bytes_
  // buffer in place) bytes_.size() is invariant after load, so there's
  // nothing further to check here -- WriteWholeFileAtomically() writes
  // exactly bytes_.size() bytes.
  if (bytes_.empty()) {
    return absl::FailedPreconditionError(
        "MenuTilemapDocument::SaveAs() called with no data loaded");
  }

  if (!backup_written_) {
    fs::path backup = fs::path(path) += ".bak";
    absl::Status backup_status =
        WriteWholeFileAtomically(backup.string(), pristine_bytes_);
    if (backup_status.ok()) {
      backup_written_ = true;
      backup_path_ = backup.string();
    }
    // A failed backup is not fatal to the save itself, but callers can
    // check has_backup() to warn the user it wasn't captured.
  }

  RETURN_IF_ERROR(WriteWholeFileAtomically(path, bytes_));

  path_ = path;
  pristine_bytes_ = bytes_;
  dirty_ = false;

  return absl::OkStatus();
}

absl::Status MenuTilemapDocument::Revert() {
  if (path_.empty()) {
    return absl::FailedPreconditionError(
        "MenuTilemapDocument::Revert() called with no path set");
  }
  std::string path = path_;
  bool had_backup = backup_written_;
  std::string backup_path = backup_path_;
  RETURN_IF_ERROR(LoadFromFile(path));
  // Preserve backup bookkeeping across Revert(): the pristine copy already
  // on disk from *this session's* first save should not be re-taken (and
  // would otherwise silently overwrite itself with the now-current file).
  backup_written_ = had_backup;
  backup_path_ = backup_path;
  return absl::OkStatus();
}

absl::StatusOr<bool> MenuTilemapDocument::ExternalChangeDetected() const {
  if (path_.empty()) {
    return absl::FailedPreconditionError(
        "MenuTilemapDocument has no associated path");
  }
  std::error_code ec;
  if (!fs::exists(path_, ec) || ec) {
    return absl::NotFoundError(absl::StrFormat("'%s' no longer exists", path_));
  }
  // Compare content, not stat() data. A same-size rewrite that lands in the
  // same filesystem timestamp tick leaves size and mtime untouched (seen on
  // the Ubuntu CI runner), and a touch that changes only the mtime is not a
  // change. The baseline is pristine_bytes_ -- what this document last read
  // from or wrote to the file -- not bytes_, so unsaved in-memory edits never
  // look like an external change. Files are at most 2 KB.
  ASSIGN_OR_RETURN(std::vector<uint8_t> on_disk, ReadWholeFile(path_));
  return on_disk != pristine_bytes_;
}

absl::Status MenuTilemapDocument::AcknowledgeExternalChange() {
  if (path_.empty()) {
    return absl::FailedPreconditionError(
        "MenuTilemapDocument has no associated path");
  }
  ASSIGN_OR_RETURN(std::vector<uint8_t> on_disk, ReadWholeFile(path_));
  pristine_bytes_ = std::move(on_disk);
  dirty_ = bytes_ != pristine_bytes_;
  return absl::OkStatus();
}

absl::Status MenuTilemapDocument::RestoreBytes(std::vector<uint8_t> bytes) {
  if (!loaded()) {
    return absl::FailedPreconditionError(
        "MenuTilemapDocument::RestoreBytes() called with no data loaded");
  }
  if (bytes.size() != bytes_.size()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "RestoreBytes size %zu does not match the loaded size %zu",
        bytes.size(), bytes_.size()));
  }
  bytes_ = std::move(bytes);
  // Dirty means "differs from the file as last loaded/saved", so undoing
  // back to that state is clean and redoing away from it is dirty again.
  dirty_ = bytes_ != pristine_bytes_;
  return absl::OkStatus();
}

void MenuTilemapDocument::ClipRect(Rect& rect) const {
  int r0 = std::max(0, rect.row);
  int c0 = std::max(0, rect.col);
  int r1 = std::min(rows_, rect.row + rect.rows);
  int c1 = std::min(kCols, rect.col + rect.cols);
  rect.row = r0;
  rect.col = c0;
  rect.rows = std::max(0, r1 - r0);
  rect.cols = std::max(0, c1 - c0);
}

gfx::TileInfo MenuTilemapDocument::GetCell(int row, int col) const {
  return gfx::WordToTileInfo(GetCellWord(row, col));
}

uint16_t MenuTilemapDocument::GetCellWord(int row, int col) const {
  if (!InBounds(row, col))
    return 0;
  size_t offset = static_cast<size_t>(row) * kBytesPerRow + col * 2;
  return static_cast<uint16_t>(bytes_[offset] | (bytes_[offset + 1] << 8));
}

bool MenuTilemapDocument::SetCell(int row, int col, gfx::TileInfo info) {
  return SetCellWord(row, col, gfx::TileInfoToWord(info));
}

bool MenuTilemapDocument::SetCellWord(int row, int col, uint16_t word) {
  if (!InBounds(row, col))
    return false;
  size_t offset = static_cast<size_t>(row) * kBytesPerRow + col * 2;
  uint8_t lo = static_cast<uint8_t>(word & 0xFF);
  uint8_t hi = static_cast<uint8_t>((word >> 8) & 0xFF);
  if (bytes_[offset] == lo && bytes_[offset + 1] == hi) {
    return true;  // no-op write; don't mark dirty
  }
  bytes_[offset] = lo;
  bytes_[offset + 1] = hi;
  dirty_ = true;
  return true;
}

MenuTilemapDocument::Clipboard MenuTilemapDocument::CopyRect(Rect rect) const {
  ClipRect(rect);
  Clipboard clip;
  clip.rows = rect.rows;
  clip.cols = rect.cols;
  clip.cells.reserve(static_cast<size_t>(rect.rows) * rect.cols);
  for (int r = 0; r < rect.rows; ++r) {
    for (int c = 0; c < rect.cols; ++c) {
      clip.cells.push_back(GetCell(rect.row + r, rect.col + c));
    }
  }
  return clip;
}

int MenuTilemapDocument::PasteRect(int row, int col, const Clipboard& clip) {
  int touched = 0;
  for (int r = 0; r < clip.rows; ++r) {
    for (int c = 0; c < clip.cols; ++c) {
      if (SetCell(row + r, col + c, clip.cells[r * clip.cols + c])) {
        if (InBounds(row + r, col + c))
          touched++;
      }
    }
  }
  return touched;
}

int MenuTilemapDocument::FillRect(Rect rect, gfx::TileInfo info) {
  ClipRect(rect);
  int touched = 0;
  for (int r = 0; r < rect.rows; ++r) {
    for (int c = 0; c < rect.cols; ++c) {
      if (SetCell(rect.row + r, rect.col + c, info))
        touched++;
    }
  }
  return touched;
}

int MenuTilemapDocument::EraseRect(Rect rect, uint16_t erase_word) {
  ClipRect(rect);
  int touched = 0;
  for (int r = 0; r < rect.rows; ++r) {
    for (int c = 0; c < rect.cols; ++c) {
      if (SetCellWord(rect.row + r, rect.col + c, erase_word))
        touched++;
    }
  }
  return touched;
}

std::vector<uint8_t> MenuTilemapDocument::RenderIndexed(
    const ChrPixelFn& chr) const {
  int width = render_width();
  int height = render_height();
  std::vector<uint8_t> out(static_cast<size_t>(width) * height, 0);
  if (!chr)
    return out;
  for (int row = 0; row < rows_; ++row) {
    for (int col = 0; col < kCols; ++col) {
      gfx::TileInfo info = GetCell(row, col);
      int base_x = col * 8;
      int base_y = row * 8;
      for (int y = 0; y < 8; ++y) {
        int sy = info.vertical_mirror_ ? 7 - y : y;
        for (int x = 0; x < 8; ++x) {
          int sx = info.horizontal_mirror_ ? 7 - x : x;
          uint8_t color = chr(info.id_, sx, sy) & 0x03;
          uint8_t index =
              static_cast<uint8_t>((info.palette_ & 0x07) * 4 + color);
          size_t out_idx =
              static_cast<size_t>(base_y + y) * width + (base_x + x);
          out[out_idx] = index;
        }
      }
    }
  }
  return out;
}

std::vector<uint8_t> ComposeMenuTilemapRgba(
    const std::vector<uint8_t>& indexed, int width, int height,
    const std::array<gfx::SnesColor, 32>& subpalette_colors) {
  std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4, 0);
  for (size_t i = 0; i < indexed.size() && i * 4 + 3 < rgba.size(); ++i) {
    uint8_t idx = indexed[i];
    uint8_t color_in_subpalette = idx & 0x03;
    size_t out = i * 4;
    if (color_in_subpalette == 0) {
      rgba[out + 0] = 0;
      rgba[out + 1] = 0;
      rgba[out + 2] = 0;
      rgba[out + 3] = 0;  // transparent
      continue;
    }
    const auto& color = subpalette_colors[idx];
    auto rgb = color.rgb();
    rgba[out + 0] = static_cast<uint8_t>(rgb.x);
    rgba[out + 1] = static_cast<uint8_t>(rgb.y);
    rgba[out + 2] = static_cast<uint8_t>(rgb.z);
    rgba[out + 3] = 255;
  }
  return rgba;
}

MenuTilemapDocument::ChrPixelFn MakeChrPixelFn(
    const std::vector<uint8_t>& sheet_8bpp_indexed) {
  return
      [&sheet_8bpp_indexed](int tile_id, int local_x, int local_y) -> uint8_t {
        if (tile_id < 0 || local_x < 0 || local_x >= 8 || local_y < 0 ||
            local_y >= 8) {
          return 0;
        }
        constexpr int kTilesPerChunk = 128;
        constexpr int kTilesPerChunkRow = 16;
        constexpr int kChunkWidth = 128;
        constexpr size_t kChunkBytes = 8192;  // 128 * 64
        int chunk = tile_id / kTilesPerChunk;
        int local_tile = tile_id % kTilesPerChunk;
        int tile_col = local_tile % kTilesPerChunkRow;
        int tile_row = local_tile / kTilesPerChunkRow;
        size_t base = static_cast<size_t>(chunk) * kChunkBytes;
        size_t idx = base +
                     static_cast<size_t>(tile_row * 8 + local_y) * kChunkWidth +
                     (tile_col * 8 + local_x);
        if (idx >= sheet_8bpp_indexed.size())
          return 0;
        return sheet_8bpp_indexed[idx] & 0x03;
      };
}

}  // namespace yaze::zelda3
