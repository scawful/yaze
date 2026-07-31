#include "zelda3/dungeon/object_tile_editor.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "app/gfx/types/snes_tile.h"
#include "core/features.h"
#include "rom/transaction.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

namespace {

constexpr int kPaletteBankSize = 16;

gfx::SnesPalette BuildPaddedPaletteBank(const gfx::SnesPalette& source) {
  gfx::SnesPalette padded;
  for (size_t i = 0; i < static_cast<size_t>(kPaletteBankSize); ++i) {
    if (i < source.size()) {
      padded.AddColor(source[i]);
    } else {
      padded.AddColor(gfx::SnesColor());
    }
  }
  return padded;
}

gfx::SnesPalette BuildCombinedPaletteBanks(const gfx::PaletteGroup& palette) {
  gfx::SnesPalette combined;
  for (int i = 0; i < palette.size(); ++i) {
    const auto padded = BuildPaddedPaletteBank(palette.palette_ref(i));
    for (size_t color = 0; color < padded.size(); ++color) {
      combined.AddColor(padded[color]);
    }
  }
  return combined;
}

int ResolvePaletteIndex(const gfx::PaletteGroup& palette, int requested) {
  if (palette.empty()) {
    return 0;
  }
  if (requested >= 0 && requested < static_cast<int>(palette.size())) {
    return requested;
  }
  return 0;
}

constexpr std::array<int16_t, 2> kEditableStandardObjectIds = {0x11F, 0x120};
constexpr size_t kFixed2x2SourceWordCount = 4;

absl::StatusOr<size_t> ResolveFixed2x2SourceWordIndex(
    const ObjectTileLayout::Cell& cell) {
  if (cell.rel_x < 0 || cell.rel_x > 1 || cell.rel_y < 0 || cell.rel_y > 1) {
    return absl::FailedPreconditionError(
        "Editable object cell is outside the audited 2x2 layout");
  }

  // RoomDraw_Single2x2 consumes its four source words in column-major order:
  // visual grid [0 2; 1 3].
  return static_cast<size_t>(cell.rel_x * 2 + cell.rel_y);
}

}  // namespace

// =============================================================================
// ObjectTileLayout
// =============================================================================

ObjectTileLayout ObjectTileLayout::FromTraces(
    const std::vector<ObjectDrawer::TileTrace>& traces) {
  ObjectTileLayout layout;
  if (traces.empty()) {
    return layout;
  }

  layout.object_id = static_cast<int16_t>(traces[0].object_id);

  // Find bounding box
  int min_x = traces[0].x_tile;
  int min_y = traces[0].y_tile;
  int max_x = min_x;
  int max_y = min_y;
  for (const auto& t : traces) {
    min_x = std::min(min_x, static_cast<int>(t.x_tile));
    min_y = std::min(min_y, static_cast<int>(t.y_tile));
    max_x = std::max(max_x, static_cast<int>(t.x_tile));
    max_y = std::max(max_y, static_cast<int>(t.y_tile));
  }

  layout.origin_tile_x = min_x;
  layout.origin_tile_y = min_y;
  layout.bounds_width = max_x - min_x + 1;
  layout.bounds_height = max_y - min_y + 1;

  std::unordered_map<uint32_t, size_t> last_trace_by_cell;
  last_trace_by_cell.reserve(traces.size());
  for (size_t i = 0; i < traces.size(); ++i) {
    const auto& t = traces[i];
    const uint32_t cell_key =
        (static_cast<uint32_t>(static_cast<uint16_t>(t.x_tile)) << 16) |
        static_cast<uint16_t>(t.y_tile);
    last_trace_by_cell[cell_key] = i;
  }

  std::vector<size_t> surviving_trace_indices;
  surviving_trace_indices.reserve(last_trace_by_cell.size());
  for (const auto& [_, trace_index] : last_trace_by_cell) {
    surviving_trace_indices.push_back(trace_index);
  }
  std::sort(surviving_trace_indices.begin(), surviving_trace_indices.end());

  layout.cells.reserve(surviving_trace_indices.size());
  for (size_t trace_index : surviving_trace_indices) {
    const auto& t = traces[trace_index];
    Cell cell;
    cell.rel_x = t.x_tile - min_x;
    cell.rel_y = t.y_tile - min_y;

    // Reconstruct TileInfo from trace fields
    bool h_mirror = (t.flags & 0x1) != 0;
    bool v_mirror = (t.flags & 0x2) != 0;
    bool priority = (t.flags & 0x4) != 0;
    uint8_t palette = (t.flags >> 3) & 0x7;
    cell.tile_info =
        gfx::TileInfo(t.tile_id, palette, v_mirror, h_mirror, priority);
    cell.original_word = gfx::TileInfoToWord(cell.tile_info);
    cell.write_index = static_cast<int>(trace_index);
    cell.modified = false;

    layout.cells.push_back(cell);
  }

  return layout;
}

ObjectTileLayout ObjectTileLayout::CreateEmpty(int width, int height,
                                               int16_t object_id,
                                               const std::string& filename) {
  ObjectTileLayout layout;
  layout.object_id = object_id;
  layout.origin_tile_x = 0;
  layout.origin_tile_y = 0;
  layout.bounds_width = width;
  layout.bounds_height = height;
  layout.tile_data_address = -1;
  layout.is_custom = true;
  layout.custom_filename = filename;

  layout.cells.reserve(width * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      Cell cell;
      cell.rel_x = x;
      cell.rel_y = y;
      cell.tile_info = gfx::TileInfo(0, 2, false, false, false);
      cell.original_word = gfx::TileInfoToWord(cell.tile_info);
      cell.write_index = static_cast<int>(layout.cells.size());
      cell.modified = true;
      layout.cells.push_back(cell);
    }
  }

  return layout;
}

ObjectTileLayout::Cell* ObjectTileLayout::FindCell(int rel_x, int rel_y) {
  for (auto& cell : cells) {
    if (cell.rel_x == rel_x && cell.rel_y == rel_y)
      return &cell;
  }
  return nullptr;
}

const ObjectTileLayout::Cell* ObjectTileLayout::FindCell(int rel_x,
                                                         int rel_y) const {
  for (const auto& cell : cells) {
    if (cell.rel_x == rel_x && cell.rel_y == rel_y)
      return &cell;
  }
  return nullptr;
}

bool ObjectTileLayout::HasModifications() const {
  for (const auto& cell : cells) {
    if (cell.modified)
      return true;
  }
  return false;
}

void ObjectTileLayout::RevertAll() {
  for (auto& cell : cells) {
    if (cell.modified) {
      cell.tile_info = gfx::WordToTileInfo(cell.original_word);
      cell.modified = false;
    }
  }
}

// =============================================================================
// ObjectTileEditor
// =============================================================================

ObjectTileEditor::ObjectTileEditor(Rom* rom) : rom_(rom) {}

absl::StatusOr<ObjectTileLayout> ObjectTileEditor::CaptureObjectLayout(
    int16_t object_id, const Room& room, const gfx::PaletteGroup& palette) {
  return CaptureObjectLayout(object_id, room, palette,
                             DefaultRoomObjectSizeForPlacement(object_id));
}

absl::StatusOr<ObjectTileLayout> ObjectTileEditor::CaptureObjectLayout(
    int16_t object_id, const Room& room, const gfx::PaletteGroup& palette,
    uint8_t object_size) {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Resolve the canvas anchor through ObjectGeometry so routines that
  // draw upward or leftward (acute diagonals 0x09-0x14 / 0x15-0x20,
  // diagonal ceilings 0xA0-0xAC) replay their full extent without
  // clipping. Hardcoded (2, 2) previously
  // dropped tile writes at negative tile coordinates because
  // DrawRoutineUtils::WriteTile8 short-circuits via IsValidTilePosition
  // before invoking the trace hook.
  const uint8_t preview_size = CanonicalRoomObjectSize(object_id, object_size);
  auto [anchor_x, anchor_y] =
      ObjectGeometry::Get().ResolveAnchor(object_id, preview_size);
  RoomObject obj(object_id, anchor_x, anchor_y, preview_size, 0);
  obj.SetRom(rom_);
  obj.EnsureTilesLoaded();

  // Check if this is a custom object
  bool is_custom = false;
  std::string custom_filename;
  int subtype = obj.size_ & 0x1F;
  if (core::FeatureFlags::get().kEnableCustomObjects) {
    auto custom_result =
        CustomObjectManager::Get().GetObjectInternal(object_id, subtype);
    if (custom_result.ok()) {
      is_custom = true;
      custom_filename =
          CustomObjectManager::Get().ResolveFilename(object_id, subtype);
    }
  }

  // Create drawer and set up trace collection
  ObjectDrawer drawer(rom_, room.id(), room.get_gfx_buffer().data());

  std::vector<ObjectDrawer::TileTrace> traces;
  traces.reserve(256);
  drawer.SetTraceCollector(&traces, /*trace_only=*/true);

  // Draw the object to collect traces
  gfx::BackgroundBuffer dummy_bg1(512, 512);
  gfx::BackgroundBuffer dummy_bg2(512, 512);
  auto status = drawer.DrawObject(obj, dummy_bg1, dummy_bg2, palette);
  drawer.ClearTraceCollector();

  if (!status.ok()) {
    return status;
  }

  if (traces.empty()) {
    return absl::NotFoundError("Object produced no tile traces");
  }

  // Build layout from traces
  ObjectTileLayout layout = ObjectTileLayout::FromTraces(traces);
  // A draw trace proves visual placement, not ROM source ownership. Keep
  // generic captures preview-only even when RoomObject happens to expose a
  // legacy tile-data pointer.
  layout.tile_data_address = -1;
  layout.source_provenance.reset();
  layout.is_custom = is_custom;
  layout.custom_filename = custom_filename;

  return layout;
}

bool ObjectTileEditor::IsEditableStandardObject(int16_t object_id) {
  return std::find(kEditableStandardObjectIds.begin(),
                   kEditableStandardObjectIds.end(),
                   object_id) != kEditableStandardObjectIds.end();
}

absl::StatusOr<ObjectTileLayout> ObjectTileEditor::CaptureEditableObjectLayout(
    int16_t object_id, const Room& room, const gfx::PaletteGroup& palette) {
  if (!IsEditableStandardObject(object_id)) {
    return absl::UnimplementedError(
        "Standard object tile editing is not supported for this object");
  }
  if (rom_ == nullptr || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  auto layout_or = CaptureObjectLayout(object_id, room, palette);
  if (!layout_or.ok()) {
    return layout_or.status();
  }
  ObjectTileLayout layout = std::move(*layout_or);
  if (layout.is_custom) {
    return absl::FailedPreconditionError(
        "Custom objects do not use standard ROM source provenance");
  }
  if (layout.object_id != object_id ||
      layout.cells.size() != kFixed2x2SourceWordCount ||
      layout.bounds_width != 2 || layout.bounds_height != 2) {
    return absl::FailedPreconditionError(
        "Object draw does not match the audited fixed 2x2 layout");
  }

  const uint32_t descriptor_pc_address =
      static_cast<uint32_t>(kRoomObjectSubtype2 + (object_id - 0x100) * 2);
  auto descriptor_or = rom_->ReadWord(static_cast<int>(descriptor_pc_address));
  if (!descriptor_or.ok()) {
    return descriptor_or.status();
  }
  const uint16_t descriptor_word = *descriptor_or;
  const int64_t source_address =
      static_cast<int64_t>(kRoomObjectTileAddress) + descriptor_word;
  if (source_address < 0 ||
      source_address + static_cast<int64_t>(kFixed2x2SourceWordCount * 2) >
          static_cast<int64_t>(rom_->size()) ||
      source_address > std::numeric_limits<int>::max()) {
    return absl::OutOfRangeError(
        "Editable object tile source is outside the loaded ROM");
  }

  ObjectTileSourceSpan span;
  span.pc_address = static_cast<uint32_t>(source_address);
  span.expected_words.reserve(kFixed2x2SourceWordCount);
  for (size_t word_index = 0; word_index < kFixed2x2SourceWordCount;
       ++word_index) {
    auto word_or =
        rom_->ReadWord(static_cast<int>(span.pc_address + word_index * 2));
    if (!word_or.ok()) {
      return word_or.status();
    }
    span.expected_words.push_back(*word_or);
  }

  std::array<bool, kFixed2x2SourceWordCount> mapped_words{};
  for (auto& cell : layout.cells) {
    auto source_word_index_or = ResolveFixed2x2SourceWordIndex(cell);
    if (!source_word_index_or.ok()) {
      return source_word_index_or.status();
    }
    const size_t source_word_index = *source_word_index_or;
    if (mapped_words[source_word_index]) {
      return absl::FailedPreconditionError(
          "Editable object layout maps multiple cells to one source word");
    }
    if (cell.original_word != span.expected_words[source_word_index]) {
      return absl::FailedPreconditionError(
          "Draw trace does not match the resolved ROM source word");
    }
    mapped_words[source_word_index] = true;
    cell.source_ref = ObjectTileSourceRef{/*span_index=*/0, source_word_index};
  }
  if (std::find(mapped_words.begin(), mapped_words.end(), false) !=
      mapped_words.end()) {
    return absl::FailedPreconditionError(
        "Editable object layout does not cover every source word");
  }

  ObjectTileSourceProvenance provenance;
  provenance.object_id = object_id;
  provenance.descriptor_pc_address = descriptor_pc_address;
  provenance.expected_descriptor_word = descriptor_word;
  provenance.spans.push_back(std::move(span));
  layout.tile_data_address = static_cast<int>(source_address);
  layout.source_provenance = std::move(provenance);
  return layout;
}

absl::Status ObjectTileEditor::RenderLayoutToBitmap(
    const ObjectTileLayout& layout, gfx::Bitmap& bitmap,
    const uint8_t* room_gfx_buffer, const gfx::PaletteGroup& palette) {
  if (!room_gfx_buffer) {
    return absl::FailedPreconditionError("No room graphics buffer");
  }
  if (layout.cells.empty()) {
    return absl::OkStatus();
  }

  int bmp_w = layout.bounds_width * 8;
  int bmp_h = layout.bounds_height * 8;

  // Create or resize bitmap
  std::vector<uint8_t> pixel_data(bmp_w * bmp_h, 0);
  bitmap.Create(bmp_w, bmp_h, 8, pixel_data);

  // Preview rendering uses tile palette bank offsets (pal * 16), so the bitmap
  // needs a combined banked palette rather than a single sub-palette.
  if (!palette.empty()) {
    bitmap.SetPalette(BuildCombinedPaletteBanks(palette));
  }

  // Use a temporary ObjectDrawer just for its DrawTileToBitmap utility
  ObjectDrawer drawer(rom_, 0, room_gfx_buffer);

  for (const auto& cell : layout.cells) {
    int px = cell.rel_x * 8;
    int py = cell.rel_y * 8;
    drawer.DrawTileToBitmap(bitmap, cell.tile_info, px, py, room_gfx_buffer);
  }

  return absl::OkStatus();
}

absl::Status ObjectTileEditor::BuildTile8Atlas(gfx::Bitmap& atlas,
                                               const uint8_t* room_gfx_buffer,
                                               const gfx::PaletteGroup& palette,
                                               int display_palette) {
  if (!room_gfx_buffer) {
    return absl::FailedPreconditionError("No room graphics buffer");
  }

  std::vector<uint8_t> pixel_data(kAtlasWidthPx * kAtlasHeightPx, 0);
  atlas.Create(kAtlasWidthPx, kAtlasHeightPx, 8, pixel_data);

  const int resolved_palette = ResolvePaletteIndex(palette, display_palette);
  if (!palette.empty()) {
    atlas.SetPalette(
        BuildPaddedPaletteBank(palette.palette_ref(resolved_palette)));
  }

  ObjectDrawer drawer(rom_, 0, room_gfx_buffer);

  for (int tile_id = 0; tile_id < kAtlasTileCount; ++tile_id) {
    int col = tile_id % kAtlasTilesPerRow;
    int row = tile_id / kAtlasTilesPerRow;
    int px = col * 8;
    int py = row * 8;

    // The atlas bitmap already contains the selected palette bank, so tiles
    // should draw into palette row 0 inside that local 16-color palette.
    gfx::TileInfo info(static_cast<uint16_t>(tile_id), /*palette=*/0, false,
                       false, false);
    drawer.DrawTileToBitmap(atlas, info, px, py, room_gfx_buffer);
  }

  return absl::OkStatus();
}

absl::Status ObjectTileEditor::WriteBack(const ObjectTileLayout& layout) {
  if (!layout.HasModifications()) {
    return absl::OkStatus();
  }

  if (layout.is_custom) {
    // Custom object: serialize to binary format and write .bin file
    if (layout.custom_filename.empty()) {
      return absl::FailedPreconditionError(
          "Custom object has no filename for write-back");
    }

    // Group cells by rel_y to form segments
    std::map<int, std::vector<const ObjectTileLayout::Cell*>> rows;
    for (const auto& cell : layout.cells) {
      rows[cell.rel_y].push_back(&cell);
    }

    // Serialize to binary matching CustomObjectManager::ParseBinaryData format
    std::vector<uint8_t> binary;
    constexpr int kBufferStride = 128;

    int prev_buffer_pos = 0;
    for (auto& [row_y, row_cells] : rows) {
      // Sort cells by rel_x
      std::sort(
          row_cells.begin(), row_cells.end(),
          [](const auto* a, const auto* b) { return a->rel_x < b->rel_x; });

      int count = static_cast<int>(row_cells.size());
      int buffer_pos_for_row = row_y * kBufferStride + row_cells[0]->rel_x * 2;
      int jump_offset = (row_y == rows.rbegin()->first)
                            ? 0
                            : kBufferStride;  // Jump to next row

      // Header: low 5 bits = count, high byte = jump_offset
      uint16_t header = (count & 0x1F) | ((jump_offset & 0xFF) << 8);
      binary.push_back(header & 0xFF);
      binary.push_back((header >> 8) & 0xFF);

      for (const auto* cell : row_cells) {
        uint16_t word = gfx::TileInfoToWord(cell->tile_info);
        binary.push_back(word & 0xFF);
        binary.push_back((word >> 8) & 0xFF);
      }

      prev_buffer_pos = buffer_pos_for_row + count * 2;
    }

    // Terminator
    binary.push_back(0);
    binary.push_back(0);

    // Write to file
    auto& mgr = CustomObjectManager::Get();
    std::filesystem::path full_path =
        std::filesystem::path(mgr.GetBasePath()) / layout.custom_filename;
    std::ofstream file(full_path, std::ios::binary);
    if (!file) {
      return absl::InternalError("Failed to open file for writing: " +
                                 full_path.string());
    }
    file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    file.close();

    // Reload cache
    mgr.ReloadAll();
    return absl::OkStatus();
  }

  auto plan_or = BuildStandardWritePlan(layout);
  if (!plan_or.ok()) {
    return plan_or.status();
  }
  return ApplyStandardWritePlan(*plan_or);
}

absl::StatusOr<ObjectTileWritePlan> ObjectTileEditor::BuildStandardWritePlan(
    const ObjectTileLayout& layout) const {
  if (layout.is_custom) {
    return absl::InvalidArgumentError(
        "Cannot build a standard ROM write plan for a custom object");
  }

  ObjectTileWritePlan plan;
  if (rom_ == nullptr || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!layout.source_provenance.has_value()) {
    return absl::FailedPreconditionError(
        "Standard object layout has no editable source provenance");
  }
  if (!IsEditableStandardObject(layout.object_id)) {
    return absl::UnimplementedError(
        "Standard object tile editing is not supported for this object");
  }

  const auto& provenance = *layout.source_provenance;
  if (provenance.object_id != layout.object_id) {
    return absl::FailedPreconditionError(
        "Object tile source provenance does not match the layout object");
  }
  const uint32_t expected_descriptor_address = static_cast<uint32_t>(
      kRoomObjectSubtype2 + (layout.object_id - 0x100) * 2);
  if (provenance.descriptor_pc_address != expected_descriptor_address) {
    return absl::FailedPreconditionError(
        "Object tile source descriptor address does not match the object");
  }
  if (provenance.spans.size() != 1 ||
      provenance.spans.front().expected_words.size() !=
          kFixed2x2SourceWordCount) {
    return absl::FailedPreconditionError(
        "Object tile source provenance does not match the audited source map");
  }

  const int64_t rom_size = static_cast<int64_t>(rom_->size());
  if (provenance.descriptor_pc_address >
          static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      static_cast<int64_t>(provenance.descriptor_pc_address) + 2 > rom_size) {
    return absl::OutOfRangeError(
        "Object tile source descriptor is outside the loaded ROM");
  }
  auto current_descriptor_or =
      rom_->ReadWord(static_cast<int>(provenance.descriptor_pc_address));
  if (!current_descriptor_or.ok()) {
    return current_descriptor_or.status();
  }
  if (*current_descriptor_or != provenance.expected_descriptor_word) {
    return absl::FailedPreconditionError(
        "Object tile source descriptor changed after capture");
  }

  const auto& span = provenance.spans.front();
  const int64_t expected_span_address =
      static_cast<int64_t>(kRoomObjectTileAddress) +
      provenance.expected_descriptor_word;
  if (span.pc_address != expected_span_address) {
    return absl::FailedPreconditionError(
        "Object tile source span does not match its descriptor");
  }
  if (span.pc_address >
          static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      static_cast<int64_t>(span.pc_address) +
              static_cast<int64_t>(span.expected_words.size() * 2) >
          rom_size) {
    return absl::OutOfRangeError(
        "Object tile source span is outside the loaded ROM");
  }

  std::unordered_set<int> precondition_addresses;
  const int descriptor_address =
      static_cast<int>(provenance.descriptor_pc_address);
  precondition_addresses.insert(descriptor_address);
  plan.preconditions.push_back(
      {descriptor_address, provenance.expected_descriptor_word});
  for (size_t word_index = 0; word_index < span.expected_words.size();
       ++word_index) {
    const int address = static_cast<int>(span.pc_address + word_index * 2);
    if (!precondition_addresses.insert(address).second) {
      return absl::FailedPreconditionError(
          "Object tile descriptor and source preconditions alias");
    }
    auto current_word_or = rom_->ReadWord(address);
    if (!current_word_or.ok()) {
      return current_word_or.status();
    }
    if (*current_word_or != span.expected_words[word_index]) {
      return absl::FailedPreconditionError(
          "Object tile source word changed after capture");
    }
    plan.preconditions.push_back({address, span.expected_words[word_index]});
  }

  if (layout.cells.size() != kFixed2x2SourceWordCount ||
      layout.bounds_width != 2 || layout.bounds_height != 2) {
    return absl::FailedPreconditionError(
        "Object tile layout does not match the audited fixed 2x2 shape");
  }

  std::unordered_set<int> resolved_addresses;
  for (const auto& cell : layout.cells) {
    if (!cell.source_ref.has_value()) {
      return absl::FailedPreconditionError(
          "Object tile cell has no editable source reference");
    }
    const auto& source_ref = *cell.source_ref;
    if (source_ref.span_index >= provenance.spans.size()) {
      return absl::FailedPreconditionError(
          "Object tile source span reference is out of bounds");
    }
    const auto& referenced_span = provenance.spans[source_ref.span_index];
    if (source_ref.word_index >= referenced_span.expected_words.size()) {
      return absl::FailedPreconditionError(
          "Object tile source word reference is out of bounds");
    }

    auto expected_source_index_or = ResolveFixed2x2SourceWordIndex(cell);
    if (!expected_source_index_or.ok()) {
      return expected_source_index_or.status();
    }
    if (source_ref.span_index != 0 ||
        source_ref.word_index != *expected_source_index_or) {
      return absl::FailedPreconditionError(
          "Object tile cell source reference does not match the audited map");
    }

    const int64_t address64 = static_cast<int64_t>(referenced_span.pc_address) +
                              static_cast<int64_t>(source_ref.word_index) * 2;
    if (address64 < 0 || address64 + 2 > rom_size ||
        address64 > std::numeric_limits<int>::max()) {
      return absl::OutOfRangeError(
          "Object tile write range is outside the loaded ROM");
    }
    const int address = static_cast<int>(address64);
    if (!resolved_addresses.insert(address).second) {
      return absl::FailedPreconditionError(
          "Object tile cells resolve to a duplicate ROM address");
    }

    const uint16_t expected_word =
        referenced_span.expected_words[source_ref.word_index];
    if (cell.original_word != expected_word) {
      return absl::FailedPreconditionError(
          "Object tile cell original word does not match its source");
    }
    const uint16_t current_layout_word = gfx::TileInfoToWord(cell.tile_info);
    if (!cell.modified && current_layout_word != cell.original_word) {
      return absl::FailedPreconditionError(
          "Object tile cell changed without being marked modified");
    }
    if (!cell.modified) {
      continue;
    }

    plan.writes.push_back({address, expected_word, current_layout_word});
    plan.write_ranges.emplace_back(static_cast<uint32_t>(address),
                                   static_cast<uint32_t>(address + 2));
  }

  return plan;
}

absl::Status ObjectTileEditor::ApplyStandardWritePlan(
    const ObjectTileWritePlan& plan) {
  if (plan.writes.empty()) {
    return absl::OkStatus();
  }
  if (plan.write_ranges.size() != plan.writes.size()) {
    return absl::FailedPreconditionError(
        "Object tile write plan ranges do not match its writes");
  }
  if (plan.preconditions.empty()) {
    return absl::FailedPreconditionError(
        "Object tile write plan has no source provenance preconditions");
  }
  if (rom_ == nullptr || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  const int64_t rom_size = static_cast<int64_t>(rom_->size());
  std::unordered_map<int, uint16_t> precondition_words;
  for (const auto& precondition : plan.preconditions) {
    const int64_t address = static_cast<int64_t>(precondition.address);
    if (address < 0 || address + 2 > rom_size) {
      return absl::OutOfRangeError(
          "Object tile precondition is outside the loaded ROM");
    }
    if (!precondition_words
             .emplace(precondition.address, precondition.expected_word)
             .second) {
      return absl::FailedPreconditionError(
          "Object tile plan has duplicate CAS preconditions");
    }
  }

  std::unordered_set<int> write_addresses;
  for (size_t write_index = 0; write_index < plan.writes.size();
       ++write_index) {
    const auto& write = plan.writes[write_index];
    const int64_t address = static_cast<int64_t>(write.address);
    if (address < 0 || address + 2 > rom_size) {
      return absl::OutOfRangeError(
          "Object tile write range is outside the loaded ROM");
    }
    if (!write_addresses.insert(write.address).second) {
      return absl::FailedPreconditionError(
          "Object tile plan has duplicate write addresses");
    }
    const std::pair<uint32_t, uint32_t> expected_range = {
        static_cast<uint32_t>(write.address),
        static_cast<uint32_t>(write.address + 2)};
    if (plan.write_ranges[write_index] != expected_range) {
      return absl::FailedPreconditionError(
          "Object tile write plan range does not match its write address");
    }
    const auto precondition = precondition_words.find(write.address);
    if (precondition == precondition_words.end() ||
        precondition->second != write.expected_word) {
      return absl::FailedPreconditionError(
          "Object tile write has no matching source precondition");
    }
  }

  // Recheck every captured descriptor/source word immediately before the
  // transaction. This is the compare-and-swap boundary for a prepared plan.
  for (const auto& precondition : plan.preconditions) {
    auto current_word_or = rom_->ReadWord(precondition.address);
    if (!current_word_or.ok()) {
      return current_word_or.status();
    }
    if (*current_word_or != precondition.expected_word) {
      return absl::FailedPreconditionError(
          "Object tile source changed after the write plan was built");
    }
  }
  for (const auto& write : plan.writes) {
    auto current_word_or = rom_->ReadWord(write.address);
    if (!current_word_or.ok()) {
      return current_word_or.status();
    }
    if (*current_word_or != write.expected_word) {
      return absl::FailedPreconditionError(
          "Object tile write target changed after the plan was built");
    }
  }

  const bool was_dirty = rom_->dirty();
  Transaction transaction(*rom_);
  for (const auto& write : plan.writes) {
    transaction.WriteWord(write.address, write.word);
  }
  const absl::Status status = transaction.Commit();
  if (!status.ok()) {
    rom_->set_dirty(was_dirty);
  }
  return status;
}

int ObjectTileEditor::CountObjectsSharingTileData(int16_t object_id) const {
  if (!rom_ || !rom_->is_loaded())
    return 0;

  // Create temporary object to get its tile_data_ptr_
  RoomObject test_obj(object_id, 0, 0, 0, 0);
  test_obj.SetRom(rom_);
  test_obj.EnsureTilesLoaded();
  int target_ptr = test_obj.tile_data_ptr_;
  if (target_ptr < 0)
    return 0;

  // Scan type 1 objects (0x00-0xFF) for shared pointers
  int count = 0;
  for (int id = 0; id < 0x100; ++id) {
    RoomObject scan_obj(static_cast<int16_t>(id), 0, 0, 0, 0);
    scan_obj.SetRom(rom_);
    scan_obj.EnsureTilesLoaded();
    if (scan_obj.tile_data_ptr_ == target_ptr) {
      ++count;
    }
  }

  return count;
}

}  // namespace zelda3
}  // namespace yaze
