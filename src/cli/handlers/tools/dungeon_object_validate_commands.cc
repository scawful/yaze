#include "cli/handlers/tools/dungeon_object_validate_commands.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::cli {

namespace {

constexpr int kType1Start = 0x00;
constexpr int kType1End = 0xF7;
constexpr int kType2Start = 0x100;
constexpr int kType2End = 0x13F;
constexpr int kType3Start = 0xF80;
constexpr int kType3End = 0xFFF;
constexpr int kNumRooms = 296;

struct TraceBounds {
  bool has_tiles = false;
  int min_x = 0;
  int min_y = 0;
  int max_x = 0;
  int max_y = 0;
  int width = 0;
  int height = 0;
};

TraceBounds ComputeBounds(
    const std::vector<zelda3::ObjectDrawer::TileTrace>& trace) {
  TraceBounds bounds;
  if (trace.empty()) {
    return bounds;
  }

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int max_y = std::numeric_limits<int>::min();

  for (const auto& entry : trace) {
    min_x = std::min(min_x, static_cast<int>(entry.x_tile));
    min_y = std::min(min_y, static_cast<int>(entry.y_tile));
    max_x = std::max(max_x, static_cast<int>(entry.x_tile));
    max_y = std::max(max_y, static_cast<int>(entry.y_tile));
  }

  bounds.has_tiles = true;
  bounds.min_x = min_x;
  bounds.min_y = min_y;
  bounds.max_x = max_x;
  bounds.max_y = max_y;
  bounds.width = max_x - min_x + 1;
  bounds.height = max_y - min_y + 1;
  return bounds;
}

// Choose an object origin (x,y) such that the expected selection bounds rectangle
// (origin + expected.offset + expected.size) fits inside the 64x64 validation
// canvas. This avoids false mismatches from traces being clipped at y<0/x<0.
std::pair<int, int> ChooseOriginForExpectedBounds(
    const zelda3::ObjectDimensionTable::SelectionBounds& expected_bounds) {
  constexpr int kRoomMaxX = zelda3::DrawContext::kMaxTilesX - 1;
  constexpr int kRoomMaxY = zelda3::DrawContext::kMaxTilesY - 1;

  const int min_x = expected_bounds.offset_x;
  const int min_y = expected_bounds.offset_y;
  const int max_x = expected_bounds.offset_x + expected_bounds.width - 1;
  const int max_y = expected_bounds.offset_y + expected_bounds.height - 1;

  const int low_x = std::max(0, -min_x);
  const int low_y = std::max(0, -min_y);
  const int high_x = std::min(kRoomMaxX, kRoomMaxX - max_x);
  const int high_y = std::min(kRoomMaxY, kRoomMaxY - max_y);

  auto choose_axis = [](int low, int high, int room_max) -> int {
    if (low <= high) {
      // Prefer origin at 0 when it fits; otherwise pick the nearest in-range.
      if (0 < low)
        return low;
      if (0 > high)
        return high;
      return 0;
    }
    // No feasible origin; clamp as a best-effort so we still produce output.
    return std::max(0, std::min(low, room_max));
  };

  return {choose_axis(low_x, high_x, kRoomMaxX),
          choose_axis(low_y, high_y, kRoomMaxY)};
}

}  // namespace

namespace detail {

zelda3::ObjectDimensionTable::SelectionBounds ClipSelectionBoundsToRoom(
    const zelda3::ObjectDimensionTable& dimension_table, int object_id,
    int size, const zelda3::ObjectDimensionTable::SelectionBounds& bounds,
    int object_x, int object_y) {
  constexpr int kRoomTilesX = zelda3::DrawContext::kMaxTilesX;
  constexpr int kRoomTilesY = zelda3::DrawContext::kMaxTilesY;
  const int room_max_x = kRoomTilesX - 1;
  const int room_max_y = kRoomTilesY - 1;

  const int min_x = object_x + bounds.offset_x;
  const int min_y = object_y + bounds.offset_y;
  int max_x = min_x + bounds.width - 1;
  int max_y = min_y + bounds.height - 1;

  if (size > 0) {
    const auto [base_w, base_h] = dimension_table.GetBaseDimensions(object_id);
    const int sel_w = bounds.width;
    const int sel_h = bounds.height;

    const bool extends_h = (sel_w > base_w) && (sel_h == base_h);
    const bool extends_v = (sel_h > base_h) && (sel_w == base_w);

    if (extends_h && max_x > room_max_x && base_w > 0) {
      const int delta = sel_w - base_w;
      if (delta > 0 && (delta % size) == 0) {
        const int spacing = delta / size;
        if (spacing > base_w) {
          // Keep only fully visible repeated segments; drop partial tail repeats.
          const int extra_room_tiles = room_max_x - (min_x + (base_w - 1));
          const int max_repeat =
              std::clamp(std::min(size, extra_room_tiles / spacing), 0, size);
          const int last_start = min_x + (max_repeat * spacing);
          const int last_end = last_start + base_w - 1;
          max_x = std::min(max_x, last_end);
        }
      }
    }

    if (extends_v && max_y > room_max_y && base_h > 0) {
      const int delta = sel_h - base_h;
      if (delta > 0 && (delta % size) == 0) {
        const int spacing = delta / size;
        if (spacing > base_h) {
          // Keep only fully visible repeated segments; drop partial tail repeats.
          const int extra_room_tiles = room_max_y - (min_y + (base_h - 1));
          const int max_repeat =
              std::clamp(std::min(size, extra_room_tiles / spacing), 0, size);
          const int last_start = min_y + (max_repeat * spacing);
          const int last_end = last_start + base_h - 1;
          max_y = std::min(max_y, last_end);
        }
      }
    }
  }

  const int clipped_min_x = std::clamp(min_x, 0, room_max_x);
  const int clipped_min_y = std::clamp(min_y, 0, room_max_y);
  const int clipped_max_x = std::clamp(max_x, 0, room_max_x);
  const int clipped_max_y = std::clamp(max_y, 0, room_max_y);

  zelda3::ObjectDimensionTable::SelectionBounds clipped = bounds;
  clipped.offset_x = clipped_min_x - object_x;
  clipped.offset_y = clipped_min_y - object_y;
  clipped.width = std::max(0, clipped_max_x - clipped_min_x + 1);
  clipped.height = std::max(0, clipped_max_y - clipped_min_y + 1);
  return clipped;
}

}  // namespace detail

namespace {

std::vector<int> BuildObjectIds(const absl::StatusOr<int>& object_arg) {
  std::vector<int> object_ids;
  if (object_arg.ok()) {
    object_ids.push_back(object_arg.value());
    return object_ids;
  }

  for (int id = kType1Start; id <= kType1End; ++id) {
    object_ids.push_back(id);
  }
  for (int id = kType2Start; id <= kType2End; ++id) {
    object_ids.push_back(id);
  }
  for (int id = kType3Start; id <= kType3End; ++id) {
    object_ids.push_back(id);
  }
  return object_ids;
}

std::vector<int> BuildSizes(const absl::StatusOr<int>& size_arg) {
  if (size_arg.ok()) {
    return {size_arg.value()};
  }
  return {0, 1, 2, 7, 15};
}

struct ValidationResult {
  int object_id = 0;
  int size = 0;
  bool has_tiles = false;
  int trace_width = 0;
  int trace_height = 0;
  int trace_min_x = 0;
  int trace_min_y = 0;
  int expected_width = 0;
  int expected_height = 0;
  int expected_offset_x = 0;
  int expected_offset_y = 0;
  int trace_offset_x = 0;
  int trace_offset_y = 0;
  bool has_room_context = false;
  int room_id = -1;
  int object_index = -1;
  int object_x = 0;
  int object_y = 0;
  int object_layer = 0;
  bool size_mismatch = false;
  bool offset_mismatch = false;

  std::string FormatText(bool include_room_fields) const {
    if (!has_tiles) {
      if (include_room_fields && has_room_context) {
        return absl::StrFormat(
            "room 0x%02X obj#%d (0x%03X size %d @%d,%d L%d): no tiles drawn",
            room_id, object_index, object_id, size, object_x, object_y,
            object_layer);
      }
      return absl::StrFormat("obj 0x%03X size %d: no tiles drawn", object_id,
                             size);
    }
    std::string status = (size_mismatch || offset_mismatch) ? "MISMATCH" : "OK";
    if (include_room_fields && has_room_context) {
      return absl::StrFormat(
          "room 0x%02X obj#%d (0x%03X size %d @%d,%d L%d): %s trace=%dx%d "
          "offset=(%d,%d) expected=%dx%d expected_offset=(%d,%d)",
          room_id, object_index, object_id, size, object_x, object_y,
          object_layer, status, trace_width, trace_height, trace_offset_x,
          trace_offset_y, expected_width, expected_height, expected_offset_x,
          expected_offset_y);
    }
    return absl::StrFormat(
        "obj 0x%03X size %d: %s trace=%dx%d min=(%d,%d) offset=(%d,%d) "
        "expected=%dx%d expected_offset=(%d,%d)",
        object_id, size, status, trace_width, trace_height, trace_min_x,
        trace_min_y, trace_offset_x, trace_offset_y, expected_width,
        expected_height, expected_offset_x, expected_offset_y);
  }

  std::string FormatJson(bool include_room_fields) const {
    if (include_room_fields && has_room_context) {
      return absl::StrFormat(
          R"({"object_id":"0x%03X","size":%d,"has_tiles":%s,)"
          R"("trace_width":%d,"trace_height":%d,"trace_min_x":%d,"trace_min_y":%d,)"
          R"("trace_offset_x":%d,"trace_offset_y":%d,)"
          R"("expected_width":%d,"expected_height":%d,"expected_offset_x":%d,"expected_offset_y":%d,)"
          R"("room_id":%d,"object_index":%d,"object_x":%d,"object_y":%d,"object_layer":%d,)"
          R"("size_mismatch":%s,"offset_mismatch":%s})",
          object_id, size, has_tiles ? "true" : "false", trace_width,
          trace_height, trace_min_x, trace_min_y, trace_offset_x,
          trace_offset_y, expected_width, expected_height, expected_offset_x,
          expected_offset_y, room_id, object_index, object_x, object_y,
          object_layer, size_mismatch ? "true" : "false",
          offset_mismatch ? "true" : "false");
    }
    return absl::StrFormat(
        R"({"object_id":"0x%03X","size":%d,"has_tiles":%s,)"
        R"("trace_width":%d,"trace_height":%d,"trace_min_x":%d,"trace_min_y":%d,)"
        R"("trace_offset_x":%d,"trace_offset_y":%d,)"
        R"("expected_width":%d,"expected_height":%d,"expected_offset_x":%d,"expected_offset_y":%d,)"
        R"("size_mismatch":%s,"offset_mismatch":%s})",
        object_id, size, has_tiles ? "true" : "false", trace_width,
        trace_height, trace_min_x, trace_min_y, trace_offset_x, trace_offset_y,
        expected_width, expected_height, expected_offset_x, expected_offset_y,
        size_mismatch ? "true" : "false", offset_mismatch ? "true" : "false");
  }
};

struct ReportPaths {
  std::string json_path;
  std::string csv_path;
};

struct TraceDumpCase {
  int object_id = 0;
  int size = 0;
  bool has_room_context = false;
  int room_id = -1;
  int object_index = -1;
  int object_x = 0;
  int object_y = 0;
  int object_layer = 0;
  std::vector<zelda3::ObjectDrawer::TileTrace> tiles;
};

bool EndsWith(const std::string& value, const std::string& suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
         0;
}

ReportPaths ResolveReportPaths(const std::string& report_base) {
  std::string base =
      report_base.empty() ? "dungeon_object_validation_report" : report_base;
  ReportPaths paths{};
  if (EndsWith(base, ".json")) {
    paths.json_path = base;
    paths.csv_path = base.substr(0, base.size() - 5) + ".csv";
  } else if (EndsWith(base, ".csv")) {
    paths.csv_path = base;
    paths.json_path = base.substr(0, base.size() - 4) + ".json";
  } else {
    paths.json_path = base + ".json";
    paths.csv_path = base + ".csv";
  }
  return paths;
}

absl::Status WriteJsonReport(const ReportPaths& paths, bool include_room_fields,
                             int object_count, int size_cases, int test_cases,
                             int mismatch_count, int empty_traces,
                             int negative_offsets, int skipped_nothing,
                             const std::vector<ValidationResult>& mismatches) {
  std::ofstream out(paths.json_path);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open report file: %s", paths.json_path));
  }

  out << "{\n";
  out << "  \"summary\": {\n";
  out << absl::StrFormat("    \"object_count\": %d,\n", object_count);
  out << absl::StrFormat("    \"size_cases\": %d,\n", size_cases);
  out << absl::StrFormat("    \"test_cases\": %d,\n", test_cases);
  out << absl::StrFormat("    \"mismatch_count\": %d,\n", mismatch_count);
  out << absl::StrFormat("    \"empty_traces\": %d,\n", empty_traces);
  out << absl::StrFormat("    \"negative_offsets\": %d,\n", negative_offsets);
  out << absl::StrFormat("    \"skipped_nothing\": %d\n", skipped_nothing);
  out << "  },\n";

  out << "  \"mismatches\": [\n";
  for (size_t i = 0; i < mismatches.size(); ++i) {
    out << "    " << mismatches[i].FormatJson(include_room_fields);
    if (i + 1 < mismatches.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return absl::OkStatus();
}

absl::Status WriteCsvReport(const ReportPaths& paths, bool include_room_fields,
                            const std::vector<ValidationResult>& mismatches) {
  std::ofstream out(paths.csv_path);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open report file: %s", paths.csv_path));
  }

  out << "category,object_id,size,has_tiles,trace_width,trace_height,trace_min_"
         "x,trace_min_y,";
  if (include_room_fields) {
    out << "trace_offset_x,trace_offset_y,room_id,object_index,object_x,object_"
           "y,object_layer,";
  }
  out << "expected_width,expected_height,expected_offset_x,expected_offset_y,"
         "size_mismatch,offset_mismatch\n";
  auto write_row = [&](const ValidationResult& result) {
    const std::string category = result.has_tiles ? "mismatch" : "empty";
    out << category << "," << absl::StrFormat("0x%03X", result.object_id) << ","
        << result.size << "," << (result.has_tiles ? "true" : "false") << ","
        << result.trace_width << "," << result.trace_height << ","
        << result.trace_min_x << "," << result.trace_min_y << ",";
    if (include_room_fields) {
      out << result.trace_offset_x << "," << result.trace_offset_y << ","
          << result.room_id << "," << result.object_index << ","
          << result.object_x << "," << result.object_y << ","
          << result.object_layer << ",";
    }
    out << "" << result.expected_width << "," << result.expected_height << ","
        << result.expected_offset_x << "," << result.expected_offset_y << ","
        << (result.size_mismatch ? "true" : "false") << ","
        << (result.offset_mismatch ? "true" : "false") << "\n";
  };

  for (const auto& result : mismatches) {
    write_row(result);
  }

  return absl::OkStatus();
}

std::vector<zelda3::ObjectDrawer::TileTrace> NormalizeTrace(
    const std::vector<zelda3::ObjectDrawer::TileTrace>& trace) {
  std::vector<zelda3::ObjectDrawer::TileTrace> normalized = trace;
  std::sort(normalized.begin(), normalized.end(),
            [](const zelda3::ObjectDrawer::TileTrace& left,
               const zelda3::ObjectDrawer::TileTrace& right) {
              if (left.y_tile != right.y_tile) {
                return left.y_tile < right.y_tile;
              }
              if (left.x_tile != right.x_tile) {
                return left.x_tile < right.x_tile;
              }
              if (left.tile_id != right.tile_id) {
                return left.tile_id < right.tile_id;
              }
              if (left.layer != right.layer) {
                return left.layer < right.layer;
              }
              return left.flags < right.flags;
            });
  return normalized;
}

absl::Status WriteTraceDump(const std::string& path, bool include_room_fields,
                            int empty_case_count,
                            const std::vector<TraceDumpCase>& cases) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open trace dump file: %s", path));
  }

  out << "{\n";
  out << absl::StrFormat("  \"case_count\": %d,\n",
                         static_cast<int>(cases.size()));
  out << absl::StrFormat("  \"empty_case_count\": %d,\n", empty_case_count);
  out << "  \"cases\": [\n";

  for (size_t i = 0; i < cases.size(); ++i) {
    const auto& entry = cases[i];
    out << "    {\n";
    out << absl::StrFormat("      \"object_id\": \"0x%03X\",\n",
                           entry.object_id);
    out << absl::StrFormat("      \"object_id_dec\": %d,\n", entry.object_id);
    out << absl::StrFormat("      \"size\": %d,\n", entry.size);
    if (include_room_fields && entry.has_room_context) {
      out << absl::StrFormat("      \"room_id\": %d,\n", entry.room_id);
      out << absl::StrFormat("      \"object_index\": %d,\n",
                             entry.object_index);
      out << absl::StrFormat("      \"object_x\": %d,\n", entry.object_x);
      out << absl::StrFormat("      \"object_y\": %d,\n", entry.object_y);
      out << absl::StrFormat("      \"object_layer\": %d,\n",
                             entry.object_layer);
    }
    out << "      \"tiles\": [\n";

    for (size_t t = 0; t < entry.tiles.size(); ++t) {
      const auto& tile = entry.tiles[t];
      out << absl::StrFormat(
          "        {\"x_tile\":%d,\"y_tile\":%d,"
          "\"tile_id\":\"0x%04X\",\"tile_id_dec\":%d,"
          "\"layer\":%d,\"flags\":%d}",
          tile.x_tile, tile.y_tile, tile.tile_id, tile.tile_id, tile.layer,
          tile.flags);
      if (t + 1 < entry.tiles.size()) {
        out << ",";
      }
      out << "\n";
    }
    out << "      ]\n";
    out << "    }";
    if (i + 1 < cases.size()) {
      out << ",";
    }
    out << "\n";
  }

  out << "  ]\n";
  out << "}\n";
  return absl::OkStatus();
}

}  // namespace

absl::Status DungeonObjectValidateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto object_arg = parser.GetInt("object");
  auto size_arg = parser.GetInt("size");
  auto room_arg = parser.GetInt("room");
  auto report_arg = parser.GetString("report");
  auto trace_out_arg = parser.GetString("trace-out");
  bool verbose = parser.HasFlag("verbose");
  const bool room_mode = room_arg.ok();
  const int room_id = room_mode ? room_arg.value() : -1;
  if (room_mode && (room_id < 0 || room_id >= kNumRooms)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Room ID must be between 0 and %d", kNumRooms - 1));
  }

  auto& dimension_table = zelda3::ObjectDimensionTable::Get();
  auto load_status = dimension_table.LoadFromRom(rom);
  if (!load_status.ok()) {
    return load_status;
  }

  std::vector<int> object_ids = BuildObjectIds(object_arg);
  std::vector<int> sizes = BuildSizes(size_arg);

  zelda3::ObjectDrawer drawer(rom, 0, nullptr);
  std::vector<zelda3::ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, true);

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  int total_tests = 0;
  int mismatch_count = 0;
  int empty_trace_count = 0;
  int negative_offset_count = 0;
  int skipped_nothing = 0;
  int room_object_count = 0;
  std::vector<ValidationResult> mismatches;
  std::vector<ValidationResult> empty_traces;
  std::vector<TraceDumpCase> trace_cases;

  ReportPaths report_paths = ResolveReportPaths(
      report_arg.has_value() ? report_arg.value() : std::string());
  const bool write_trace_dump = trace_out_arg.has_value();
  if (write_trace_dump) {
    trace_cases.reserve(object_ids.size() * sizes.size());
  }

  bool prev_custom_objects = core::FeatureFlags::get().kEnableCustomObjects;
  core::FeatureFlags::get().kEnableCustomObjects = false;

  if (room_mode) {
    zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
    room.LoadObjects();
    const auto& room_objects = room.GetTileObjects();
    room_object_count = static_cast<int>(room_objects.size());
    if (write_trace_dump) {
      trace_cases.reserve(room_objects.size());
    }

    for (size_t idx = 0; idx < room_objects.size(); ++idx) {
      const auto& room_obj = room_objects[idx];
      int object_id = room_obj.id_;
      int routine_id = drawer.GetDrawRoutineId(object_id);
      if (routine_id == zelda3::DrawRoutineIds::kNothing) {
        skipped_nothing++;
        continue;
      }
      total_tests++;
      trace.clear();

      zelda3::RoomObject obj = room_obj;
      obj.SetRom(rom);
      auto draw_status = drawer.DrawObject(obj, bg1, bg2, palette_group);
      if (!draw_status.ok()) {
        return draw_status;
      }

      auto expected_bounds =
          dimension_table.GetSelectionBounds(object_id, obj.size_);
      expected_bounds = detail::ClipSelectionBoundsToRoom(
          dimension_table, object_id, obj.size_, expected_bounds, obj.x_,
          obj.y_);

      TraceBounds bounds = ComputeBounds(trace);
      if (write_trace_dump) {
        TraceDumpCase trace_case{};
        trace_case.object_id = object_id;
        trace_case.size = obj.size_;
        trace_case.has_room_context = true;
        trace_case.room_id = room_id;
        trace_case.object_index = static_cast<int>(idx);
        trace_case.object_x = obj.x_;
        trace_case.object_y = obj.y_;
        trace_case.object_layer = obj.GetLayerValue();
        trace_case.tiles = NormalizeTrace(trace);
        trace_cases.push_back(std::move(trace_case));
      }

      ValidationResult result{};
      result.object_id = object_id;
      result.size = obj.size_;
      result.has_room_context = true;
      result.room_id = room_id;
      result.object_index = static_cast<int>(idx);
      result.object_x = obj.x_;
      result.object_y = obj.y_;
      result.object_layer = obj.GetLayerValue();
      result.expected_width = expected_bounds.width;
      result.expected_height = expected_bounds.height;
      result.expected_offset_x = expected_bounds.offset_x;
      result.expected_offset_y = expected_bounds.offset_y;

      if (!bounds.has_tiles) {
        empty_trace_count++;
        result.has_tiles = false;
        result.trace_width = 0;
        result.trace_height = 0;
        result.trace_min_x = 0;
        result.trace_min_y = 0;
        result.trace_offset_x = 0;
        result.trace_offset_y = 0;
        result.size_mismatch = true;
        result.offset_mismatch = false;
        mismatch_count++;
        mismatches.push_back(result);
        if (verbose) {
          empty_traces.push_back(result);
        }
        continue;
      }

      result.has_tiles = true;
      result.trace_width = bounds.width;
      result.trace_height = bounds.height;
      result.trace_min_x = bounds.min_x;
      result.trace_min_y = bounds.min_y;
      result.trace_offset_x = bounds.min_x - obj.x_;
      result.trace_offset_y = bounds.min_y - obj.y_;
      result.size_mismatch = (bounds.width != expected_bounds.width ||
                              bounds.height != expected_bounds.height);
      result.offset_mismatch =
          (result.trace_offset_x != expected_bounds.offset_x ||
           result.trace_offset_y != expected_bounds.offset_y);

      if (expected_bounds.offset_x < 0 || expected_bounds.offset_y < 0) {
        negative_offset_count++;
      }

      if (result.size_mismatch || result.offset_mismatch) {
        mismatch_count++;
        mismatches.push_back(result);
      }
    }
  } else {
    for (int object_id : object_ids) {
      int routine_id = drawer.GetDrawRoutineId(object_id);
      if (routine_id == zelda3::DrawRoutineIds::kNothing) {
        skipped_nothing++;
        continue;
      }
      for (int size : sizes) {
        total_tests++;
        trace.clear();

        auto expected_bounds =
            dimension_table.GetSelectionBounds(object_id, size);
        const auto [origin_x, origin_y] =
            ChooseOriginForExpectedBounds(expected_bounds);

        zelda3::RoomObject obj(object_id, origin_x, origin_y,
                               static_cast<uint8_t>(size), 0);
        obj.layer_ = zelda3::RoomObject::LayerType::BG1;

        // Apply the same clipping logic used in room-mode so the selection
        // bounds contract matches what can actually be traced inside a room.
        expected_bounds = detail::ClipSelectionBoundsToRoom(
            dimension_table, object_id, size, expected_bounds, obj.x_, obj.y_);

        if (expected_bounds.offset_x < 0 || expected_bounds.offset_y < 0) {
          negative_offset_count++;
        }

        auto draw_status = drawer.DrawObject(obj, bg1, bg2, palette_group);
        if (!draw_status.ok()) {
          return draw_status;
        }

        TraceBounds bounds = ComputeBounds(trace);
        if (write_trace_dump) {
          TraceDumpCase trace_case{};
          trace_case.object_id = object_id;
          trace_case.size = size;
          trace_case.tiles = NormalizeTrace(trace);
          trace_cases.push_back(std::move(trace_case));
        }
        if (!bounds.has_tiles) {
          empty_trace_count++;
          ValidationResult result{};
          result.object_id = object_id;
          result.size = size;
          result.has_tiles = false;
          result.trace_width = 0;
          result.trace_height = 0;
          result.trace_min_x = 0;
          result.trace_min_y = 0;
          result.expected_width = expected_bounds.width;
          result.expected_height = expected_bounds.height;
          result.expected_offset_x = expected_bounds.offset_x;
          result.expected_offset_y = expected_bounds.offset_y;
          result.size_mismatch = true;
          result.offset_mismatch = false;
          mismatch_count++;
          mismatches.push_back(result);
          if (verbose) {
            empty_traces.push_back(result);
          }
          continue;
        }

        ValidationResult result{};
        result.object_id = object_id;
        result.size = size;
        result.has_tiles = true;
        result.trace_width = bounds.width;
        result.trace_height = bounds.height;
        result.trace_min_x = bounds.min_x;
        result.trace_min_y = bounds.min_y;
        result.trace_offset_x = bounds.min_x - obj.x_;
        result.trace_offset_y = bounds.min_y - obj.y_;
        result.expected_width = expected_bounds.width;
        result.expected_height = expected_bounds.height;
        result.expected_offset_x = expected_bounds.offset_x;
        result.expected_offset_y = expected_bounds.offset_y;
        result.size_mismatch = (bounds.width != expected_bounds.width ||
                                bounds.height != expected_bounds.height);
        result.offset_mismatch =
            (result.trace_offset_x != expected_bounds.offset_x ||
             result.trace_offset_y != expected_bounds.offset_y);

        if (result.size_mismatch || result.offset_mismatch) {
          mismatch_count++;
          mismatches.push_back(result);
        }
      }
    }
  }

  core::FeatureFlags::get().kEnableCustomObjects = prev_custom_objects;

  const int object_count =
      room_mode ? room_object_count : static_cast<int>(object_ids.size());
  auto json_status =
      WriteJsonReport(report_paths, room_mode, object_count,
                      room_mode ? 1 : static_cast<int>(sizes.size()),
                      total_tests, mismatch_count, empty_trace_count,
                      negative_offset_count, skipped_nothing, mismatches);
  if (!json_status.ok()) {
    return json_status;
  }

  auto csv_status = WriteCsvReport(report_paths, room_mode, mismatches);
  if (!csv_status.ok()) {
    return csv_status;
  }

  if (write_trace_dump) {
    auto trace_status = WriteTraceDump(trace_out_arg.value(), room_mode,
                                       empty_trace_count, trace_cases);
    if (!trace_status.ok()) {
      return trace_status;
    }
    formatter.AddField("trace_dump", trace_out_arg.value());
  }

  formatter.AddField("object_count", object_count);
  formatter.AddField("size_cases",
                     room_mode ? 1 : static_cast<int>(sizes.size()));
  formatter.AddField("test_cases", total_tests);
  formatter.AddField("mismatch_count", mismatch_count);
  formatter.AddField("empty_traces", empty_trace_count);
  formatter.AddField("negative_offsets", negative_offset_count);
  formatter.AddField("skipped_nothing", skipped_nothing);
  if (room_mode) {
    formatter.AddField("room_id", room_id);
  }
  formatter.AddField("report_json", report_paths.json_path);
  formatter.AddField("report_csv", report_paths.csv_path);

  formatter.BeginArray("mismatches");
  for (const auto& result : mismatches) {
    formatter.AddArrayItem(formatter.IsJson() ? result.FormatJson(room_mode)
                                              : result.FormatText(room_mode));
  }
  formatter.EndArray();

  if (verbose) {
    formatter.BeginArray("empty_trace_objects");
    for (const auto& result : empty_traces) {
      formatter.AddArrayItem(formatter.IsJson() ? result.FormatJson(room_mode)
                                                : result.FormatText(room_mode));
    }
    formatter.EndArray();
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
