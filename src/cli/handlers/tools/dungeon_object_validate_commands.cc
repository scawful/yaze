#include "cli/handlers/tools/dungeon_object_validate_commands.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::cli {

namespace {

constexpr int kType1Start = 0x00;
constexpr int kType1End = 0xF7;
constexpr int kType2Start = 0x100;
constexpr int kType2End = 0x13F;
constexpr int kType3Start = 0xF80;
constexpr int kType3End = 0xFFF;

struct TraceBounds {
  bool has_tiles = false;
  int min_x = 0;
  int min_y = 0;
  int max_x = 0;
  int max_y = 0;
  int width = 0;
  int height = 0;
};

TraceBounds ComputeBounds(const std::vector<zelda3::ObjectDrawer::TileTrace>& trace) {
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
  bool size_mismatch = false;
  bool offset_mismatch = false;

  std::string FormatText() const {
    if (!has_tiles) {
      return absl::StrFormat("obj 0x%03X size %d: no tiles drawn", object_id,
                             size);
    }
    std::string status =
        (size_mismatch || offset_mismatch) ? "MISMATCH" : "OK";
    return absl::StrFormat(
        "obj 0x%03X size %d: %s trace=%dx%d offset=(%d,%d) expected=%dx%d "
        "expected_offset=(%d,%d)",
        object_id, size, status, trace_width, trace_height, trace_min_x,
        trace_min_y, expected_width, expected_height, expected_offset_x,
        expected_offset_y);
  }

  std::string FormatJson() const {
    return absl::StrFormat(
        R"({"object_id":"0x%03X","size":%d,"has_tiles":%s,)"
        R"("trace_width":%d,"trace_height":%d,"trace_min_x":%d,"trace_min_y":%d,)"
        R"("expected_width":%d,"expected_height":%d,"expected_offset_x":%d,"expected_offset_y":%d,)"
        R"("size_mismatch":%s,"offset_mismatch":%s})",
        object_id, size, has_tiles ? "true" : "false", trace_width,
        trace_height, trace_min_x, trace_min_y, expected_width, expected_height,
        expected_offset_x, expected_offset_y, size_mismatch ? "true" : "false",
        offset_mismatch ? "true" : "false");
  }
};

struct ReportPaths {
  std::string json_path;
  std::string csv_path;
};

bool EndsWith(const std::string& value, const std::string& suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

ReportPaths ResolveReportPaths(const std::string& report_base) {
  std::string base = report_base.empty() ? "dungeon_object_validation_report"
                                         : report_base;
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

absl::Status WriteJsonReport(const ReportPaths& paths,
                             int object_count,
                             int size_cases,
                             int test_cases,
                             int mismatch_count,
                             int empty_traces,
                             int negative_offsets,
                             int skipped_nothing,
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
    out << "    " << mismatches[i].FormatJson();
    if (i + 1 < mismatches.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return absl::OkStatus();
}

absl::Status WriteCsvReport(const ReportPaths& paths,
                            const std::vector<ValidationResult>& mismatches) {
  std::ofstream out(paths.csv_path);
  if (!out.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open report file: %s", paths.csv_path));
  }

  out << "category,object_id,size,has_tiles,trace_width,trace_height,trace_min_x,trace_min_y,"
         "expected_width,expected_height,expected_offset_x,expected_offset_y,size_mismatch,offset_mismatch\n";
  auto write_row = [&](const ValidationResult& result) {
    const std::string category = result.has_tiles ? "mismatch" : "empty";
    out << category << ","
        << absl::StrFormat("0x%03X", result.object_id) << ","
        << result.size << ","
        << (result.has_tiles ? "true" : "false") << ","
        << result.trace_width << ","
        << result.trace_height << ","
        << result.trace_min_x << ","
        << result.trace_min_y << ","
        << result.expected_width << ","
        << result.expected_height << ","
        << result.expected_offset_x << ","
        << result.expected_offset_y << ","
        << (result.size_mismatch ? "true" : "false") << ","
        << (result.offset_mismatch ? "true" : "false") << "\n";
  };

  for (const auto& result : mismatches) {
    write_row(result);
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status DungeonObjectValidateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto object_arg = parser.GetInt("object");
  auto size_arg = parser.GetInt("size");
  auto report_arg = parser.GetString("report");
  bool verbose = parser.HasFlag("verbose");

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
  std::vector<ValidationResult> mismatches;
  std::vector<ValidationResult> empty_traces;

  ReportPaths report_paths = ResolveReportPaths(
      report_arg.has_value() ? report_arg.value() : std::string());

  bool prev_custom_objects = core::FeatureFlags::get().kEnableCustomObjects;
  core::FeatureFlags::get().kEnableCustomObjects = false;

  for (int object_id : object_ids) {
    int routine_id = drawer.GetDrawRoutineId(object_id);
    if (routine_id == zelda3::DrawRoutineIds::kNothing) {
      skipped_nothing++;
      continue;
    }
    for (int size : sizes) {
      total_tests++;
      trace.clear();

      zelda3::RoomObject obj(object_id, 0, 0, static_cast<uint8_t>(size), 0);
      obj.layer_ = zelda3::RoomObject::LayerType::BG1;

      auto draw_status = drawer.DrawObject(obj, bg1, bg2, palette_group);
      if (!draw_status.ok()) {
        return draw_status;
      }

      auto expected_bounds =
          dimension_table.GetSelectionBounds(object_id, size);

      TraceBounds bounds = ComputeBounds(trace);
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
      result.expected_width = expected_bounds.width;
      result.expected_height = expected_bounds.height;
      result.expected_offset_x = expected_bounds.offset_x;
      result.expected_offset_y = expected_bounds.offset_y;
      result.size_mismatch =
          (bounds.width != expected_bounds.width ||
           bounds.height != expected_bounds.height);
      result.offset_mismatch =
          (bounds.min_x != expected_bounds.offset_x ||
           bounds.min_y != expected_bounds.offset_y);

      if (result.offset_mismatch) {
        negative_offset_count++;
      }

      if (result.size_mismatch || result.offset_mismatch) {
        mismatch_count++;
        mismatches.push_back(result);
      }
    }
  }

  core::FeatureFlags::get().kEnableCustomObjects = prev_custom_objects;

  auto json_status =
      WriteJsonReport(report_paths, static_cast<int>(object_ids.size()),
                      static_cast<int>(sizes.size()), total_tests,
                      mismatch_count, empty_trace_count, negative_offset_count,
                      skipped_nothing,
                      mismatches);
  if (!json_status.ok()) {
    return json_status;
  }

  auto csv_status = WriteCsvReport(report_paths, mismatches);
  if (!csv_status.ok()) {
    return csv_status;
  }

  formatter.AddField("object_count", static_cast<int>(object_ids.size()));
  formatter.AddField("size_cases", static_cast<int>(sizes.size()));
  formatter.AddField("test_cases", total_tests);
  formatter.AddField("mismatch_count", mismatch_count);
  formatter.AddField("empty_traces", empty_trace_count);
  formatter.AddField("negative_offsets", negative_offset_count);
  formatter.AddField("skipped_nothing", skipped_nothing);
  formatter.AddField("report_json", report_paths.json_path);
  formatter.AddField("report_csv", report_paths.csv_path);

  formatter.BeginArray("mismatches");
  for (const auto& result : mismatches) {
    formatter.AddArrayItem(formatter.IsJson() ? result.FormatJson()
                                              : result.FormatText());
  }
  formatter.EndArray();

  if (verbose) {
    formatter.BeginArray("empty_trace_objects");
    for (const auto& result : empty_traces) {
      formatter.AddArrayItem(formatter.IsJson() ? result.FormatJson()
                                                : result.FormatText());
    }
    formatter.EndArray();
  }

  return absl::OkStatus();
}

}  // namespace yaze::cli
