#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/dungeon/ui/reporting/dungeon_issue_report_storage.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#ifdef YAZE_WITH_GRPC
#include "app/service/screenshot_utils.h"
#endif
#include "imgui/imgui.h"
#include "util/macro.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

constexpr float kIssueReportDialogWidth = 620.0f;
constexpr float kIssueReportNotesHeight = 120.0f;
constexpr float kIssueReportDiagnosticsHeight = 180.0f;
constexpr char kIssueReportSummaryHint[] = "What looks wrong?";
constexpr char kIssueReportPopupIdDiagnostics[] = "##DungeonIssueDiagnostics";
constexpr char kIssueReportSectionPaths[] = "Paths";
constexpr char kIssueReportSectionDiagnostics[] = "Diagnostics";
constexpr char kIssueReportStatusSavedScreenshot[] =
    "Captured room screenshot to %s";
constexpr char kIssueReportStatusSavedLog[] = "Saved issue report to %s";
constexpr char kIssueReportStatusStartedLog[] = "Started issue report in %s";
constexpr char kIssueReportStatusCopiedReport[] =
    "Copied report to clipboard and saved issue report to %s";
constexpr char kIssueReportStatusCopiedDiagnostics[] =
    "Copied diagnostics and saved issue report to %s";
constexpr char kIssueReportCaptureButtonLabel[] =
    ICON_MD_ADD_A_PHOTO " Capture Screenshot";
constexpr char kIssueReportRecaptureButtonLabel[] =
    ICON_MD_PHOTO_CAMERA " Re-capture Screenshot";

const char* GetObjectStreamLabel(int layer_value) {
  switch (layer_value) {
    case 0:
      return "Primary";
    case 1:
      return "BG2 overlay";
    case 2:
      return "BG1 overlay";
    default:
      return "Unknown";
  }
}

const char* GetBlocksetGroupName(uint8_t blockset) {
  static const char* kGroupNames[] = {
      "HC/Sewers", "Eastern", "Desert", "Hera",   "A-Tower", "PoD", "Swamp",
      "Skull",     "Thieves", "Ice",    "Misery", "Turtle",  "GT",
  };
  constexpr size_t kCount = sizeof(kGroupNames) / sizeof(kGroupNames[0]);
  return blockset < kCount ? kGroupNames[blockset] : "Custom";
}

std::string BuildIssueTimestampSlug() {
  return absl::FormatTime("%Y%m%d-%H%M%S", absl::Now(), absl::LocalTimeZone());
}

std::string BuildIssueTimestampDisplay() {
  return absl::FormatTime("%Y-%m-%d %H:%M:%S %Z", absl::Now(),
                          absl::LocalTimeZone());
}

std::string BuildEffectiveBlockListSummary(const zelda3::Room& room) {
  const auto blocks = room.blocks();
  if (blocks.size() < 8) {
    return "BG sheets: unavailable";
  }
  return absl::StrFormat(
      "BG sheets: [%02X %02X %02X %02X %02X %02X %02X %02X]",
      static_cast<int>(blocks[0]), static_cast<int>(blocks[1]),
      static_cast<int>(blocks[2]), static_cast<int>(blocks[3]),
      static_cast<int>(blocks[4]), static_cast<int>(blocks[5]),
      static_cast<int>(blocks[6]), static_cast<int>(blocks[7]));
}

std::string BuildSelectedObjectSemanticsSummary(const zelda3::RoomObject& obj) {
  const auto semantics = zelda3::GetObjectLayerSemantics(obj);
  return absl::StrFormat(
      "stream=%s draw_layer=%d effective_bg=%s routine=%d both_bgs=%s",
      GetObjectStreamLabel(obj.GetLayerValue()),
      static_cast<int>(
          zelda3::MapRoomObjectListIndexToDrawLayer(obj.GetLayerValue())),
      zelda3::EffectiveBgLayerLabel(semantics.effective_bg_layer),
      semantics.routine_id, semantics.draws_to_both_bgs ? "yes" : "no");
}

const char* TraceLayerLabel(uint8_t layer) {
  switch (static_cast<zelda3::RoomObject::LayerType>(layer)) {
    case zelda3::RoomObject::LayerType::BG1:
      return "BG1";
    case zelda3::RoomObject::LayerType::BG2:
      return "BG2";
    case zelda3::RoomObject::LayerType::BG3:
      return "BG3";
  }
  return "BG?";
}

std::string FormatObjectTileSample(std::span<const gfx::TileInfo> tiles,
                                   size_t max_count, std::string_view prefix) {
  std::string out = absl::StrFormat("%s count=%zu", std::string(prefix).c_str(),
                                    tiles.size());
  const size_t preview_count = std::min<size_t>(tiles.size(), max_count);
  for (size_t i = 0; i < preview_count; ++i) {
    const auto& tile = tiles[i];
    out += absl::StrFormat(" [%zu]=0x%03X/p%d/pr%d/h%d/v%d", i, tile.id_,
                           static_cast<int>(tile.palette_), tile.over_ ? 1 : 0,
                           tile.horizontal_mirror_ ? 1 : 0,
                           tile.vertical_mirror_ ? 1 : 0);
  }
  if (tiles.size() > preview_count) {
    out += absl::StrFormat(" ... %zu more", tiles.size() - preview_count);
  }
  return out;
}

struct ObjectTraceReport {
  std::string summary;
  std::vector<zelda3::ObjectDrawer::TileTrace> writes;
};

ObjectTraceReport BuildObjectTraceReport(
    Rom* rom, int room_id, const zelda3::RoomObject& obj,
    const gfx::PaletteGroup& palette_group) {
  if (rom == nullptr || !rom->is_loaded()) {
    return {.summary = "\nDrawer trace: unavailable (ROM not loaded)"};
  }

  zelda3::ObjectDrawer drawer(rom, room_id, /*room_gfx_buffer=*/nullptr);
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  std::vector<zelda3::ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);
  const auto status = drawer.DrawObject(obj, bg1, bg2, palette_group);
  drawer.ClearTraceCollector();
  if (!status.ok()) {
    return {.summary = absl::StrFormat("\nDrawer trace: unavailable (%s)",
                                       std::string(status.message()).c_str())};
  }

  if (trace.empty()) {
    return {.summary = "\nDrawer trace: status=ok writes=0"};
  }

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int max_y = std::numeric_limits<int>::min();
  std::array<int, 3> layer_counts = {0, 0, 0};
  std::set<std::tuple<int, int, uint8_t>> unique_cells;
  for (const auto& write : trace) {
    min_x = std::min<int>(min_x, write.x_tile);
    min_y = std::min<int>(min_y, write.y_tile);
    max_x = std::max<int>(max_x, write.x_tile);
    max_y = std::max<int>(max_y, write.y_tile);
    if (write.layer < layer_counts.size()) {
      ++layer_counts[write.layer];
    }
    unique_cells.insert({write.x_tile, write.y_tile, write.layer});
  }

  std::string out = absl::StrFormat(
      "\nDrawer trace: status=ok writes=%zu unique_cells=%zu "
      "bounds_tiles=(%d,%d)..(%d,%d) layer_counts BG1=%d BG2=%d BG3=%d",
      trace.size(), unique_cells.size(), min_x, min_y, max_x, max_y,
      layer_counts[zelda3::RoomObject::LayerType::BG1],
      layer_counts[zelda3::RoomObject::LayerType::BG2],
      layer_counts[zelda3::RoomObject::LayerType::BG3]);

  const size_t preview_count = std::min<size_t>(trace.size(), 12);
  for (size_t i = 0; i < preview_count; ++i) {
    const auto& write = trace[i];
    const int palette = (write.flags >> 3) & 0x7;
    out += absl::StrFormat(
        "\n  write[%zu] %s tile=(%d,%d) id=0x%03X pal=%d pri=%d h=%d v=%d", i,
        TraceLayerLabel(write.layer), write.x_tile, write.y_tile, write.tile_id,
        palette, (write.flags & 0x4) ? 1 : 0, (write.flags & 0x1) ? 1 : 0,
        (write.flags & 0x2) ? 1 : 0);
  }
  if (trace.size() > preview_count) {
    out += absl::StrFormat("\n  ... %zu more write(s)",
                           trace.size() - preview_count);
  }
  return {.summary = std::move(out), .writes = std::move(trace)};
}

struct PaletteSamplePoint {
  std::string label;
  int x = 0;
  int y = 0;
};

bool PaletteIndexMatchesTracePalette(uint8_t palette_index,
                                     uint8_t trace_flags) {
  if ((palette_index & 0x0F) == 0) {
    return false;
  }
  const uint8_t trace_palette = static_cast<uint8_t>((trace_flags >> 3) & 0x7);
  return static_cast<uint8_t>((palette_index >> 4) & 0x7) == trace_palette;
}

std::optional<PaletteSamplePoint> ChooseObjectTracePaletteSample(
    std::span<const zelda3::ObjectDrawer::TileTrace> trace,
    const zelda3::PaletteDebugger& palette_debugger) {
  constexpr std::array<std::pair<int, int>, 6> kPreferredOffsets = {
      std::pair{4, 4}, std::pair{3, 3}, std::pair{0, 0},
      std::pair{7, 7}, std::pair{0, 7}, std::pair{7, 0},
  };

  for (size_t i = 0; i < trace.size(); ++i) {
    const auto& write = trace[i];
    const int tile_origin_x = write.x_tile * 8;
    const int tile_origin_y = write.y_tile * 8;
    const auto label = absl::StrFormat(
        "object-trace[%zu] tile=(%d,%d) pal=%d", i, write.x_tile, write.y_tile,
        static_cast<int>((write.flags >> 3) & 0x7));

    for (const auto& [dx, dy] : kPreferredOffsets) {
      const int sample_x = tile_origin_x + dx;
      const int sample_y = tile_origin_y + dy;
      const auto comp = palette_debugger.SamplePixelAt(sample_x, sample_y);
      if (PaletteIndexMatchesTracePalette(comp.palette_index, write.flags)) {
        return PaletteSamplePoint{.label = label, .x = sample_x, .y = sample_y};
      }
    }

    for (int dy = 0; dy < 8; ++dy) {
      for (int dx = 0; dx < 8; ++dx) {
        const int sample_x = tile_origin_x + dx;
        const int sample_y = tile_origin_y + dy;
        const auto comp = palette_debugger.SamplePixelAt(sample_x, sample_y);
        if (PaletteIndexMatchesTracePalette(comp.palette_index, write.flags)) {
          return PaletteSamplePoint{
              .label = label, .x = sample_x, .y = sample_y};
        }
      }
    }
  }

  if (!trace.empty()) {
    const auto& write = trace.front();
    return PaletteSamplePoint{
        .label = absl::StrFormat("object-trace[0] tile=(%d,%d) pal=%d fallback",
                                 write.x_tile, write.y_tile,
                                 static_cast<int>((write.flags >> 3) & 0x7)),
        .x = write.x_tile * 8 + 4,
        .y = write.y_tile * 8 + 4};
  }

  return std::nullopt;
}

std::string BuildObjectTraceBoundsSummary(
    std::span<const zelda3::ObjectDrawer::TileTrace> trace, int selection_x_px,
    int selection_y_px, int selection_w_px, int selection_h_px) {
  if (trace.empty()) {
    return "trace: writes=0";
  }

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int max_y = std::numeric_limits<int>::min();
  std::set<std::tuple<int, int, uint8_t>> unique_cells;
  for (const auto& write : trace) {
    min_x = std::min<int>(min_x, write.x_tile);
    min_y = std::min<int>(min_y, write.y_tile);
    max_x = std::max<int>(max_x, write.x_tile);
    max_y = std::max<int>(max_y, write.y_tile);
    unique_cells.insert({write.x_tile, write.y_tile, write.layer});
  }

  const int trace_x_px = min_x * 8;
  const int trace_y_px = min_y * 8;
  const int trace_w_px = (max_x - min_x + 1) * 8;
  const int trace_h_px = (max_y - min_y + 1) * 8;
  return absl::StrFormat(
      "trace: writes=%zu unique_cells=%zu bounds_px=(%d,%d,%d,%d) "
      "delta_vs_selection_px=(%+d,%+d,%+d,%+d)",
      trace.size(), unique_cells.size(), trace_x_px, trace_y_px, trace_w_px,
      trace_h_px, trace_x_px - selection_x_px, trace_y_px - selection_y_px,
      trace_w_px - selection_w_px, trace_h_px - selection_h_px);
}

std::string BuildDoorIssueSummary(const zelda3::Room::Door& door, size_t index,
                                  std::string_view label) {
  const auto [tile_x, tile_y] = door.GetTileCoords();
  const auto [pixel_x, pixel_y] = door.GetPixelCoords();
  const auto dims = door.GetDimensions();
  const auto editor_dims = door.GetEditorDimensions();
  const auto [bounds_x, bounds_y, bounds_w, bounds_h] = door.GetBounds();
  const auto [editor_x, editor_y, editor_w, editor_h] = door.GetEditorBounds();
  const auto [encoded_b1, encoded_b2] = door.EncodeBytes();

  return absl::StrFormat(
      "\n%s[%zu]:\n"
      "- type=%s (0x%02X)\n"
      "- direction=%s (%d)\n"
      "- position=%d\n"
      "- tile_coords=(%d,%d) pixel_coords=(%d,%d)\n"
      "- dims_tiles=(%d,%d) editor_dims_tiles=(%d,%d)\n"
      "- bounds_px=(%d,%d,%d,%d) editor_bounds_px=(%d,%d,%d,%d)\n"
      "- encoded=(0x%02X,0x%02X) original=(0x%02X,0x%02X)",
      std::string(label).c_str(), index,
      std::string(zelda3::GetDoorTypeName(door.type)).c_str(),
      static_cast<int>(door.type),
      std::string(zelda3::GetDoorDirectionName(door.direction)).c_str(),
      static_cast<int>(door.direction), static_cast<int>(door.position), tile_x,
      tile_y, pixel_x, pixel_y, dims.width_tiles, dims.height_tiles,
      editor_dims.width_tiles, editor_dims.height_tiles, bounds_x, bounds_y,
      bounds_w, bounds_h, editor_x, editor_y, editor_w, editor_h, encoded_b1,
      encoded_b2, door.byte1, door.byte2);
}

std::string BuildRomDisplayName(const Rom& rom) {
  if (!rom.short_name().empty()) {
    return rom.short_name();
  }
  if (!rom.filename().empty()) {
    return std::filesystem::path(rom.filename()).filename().string();
  }
  const std::string title = rom.title();
  return title.empty() ? "in-memory ROM" : title;
}

std::string BuildReportSessionSummary(const Rom* rom,
                                      const project::YazeProject* project) {
  std::string summary;
  if (rom && rom->is_loaded()) {
    const std::string rom_path =
        rom->filename().empty()
            ? std::string("in-memory")
            : util::PlatformPaths::NormalizePathForDisplay(rom->filename());
    summary += absl::StrFormat(
        "ROM: %s | CRC32:%s | Path:%s", BuildRomDisplayName(*rom).c_str(),
        util::ComputeRomHash(rom->data(), rom->size()).c_str(),
        rom_path.c_str());
  } else {
    summary += "ROM: unavailable";
  }

  if (project && project->project_opened()) {
    summary += absl::StrFormat(
        "\nProject: %s | Path:%s", project->GetDisplayName().c_str(),
        util::PlatformPaths::NormalizePathForDisplay(project->filepath)
            .c_str());
  }

  return summary;
}

std::string FormatPaletteDebugEventForReport(
    const zelda3::PaletteDebugEvent& event) {
  if (event.palette_id >= 0 && event.color_count > 0) {
    return absl::StrFormat("- [%s] palette=%d colors=%d %s",
                           event.location.c_str(), event.palette_id,
                           event.color_count, event.message.c_str());
  }
  if (event.palette_id >= 0) {
    return absl::StrFormat("- [%s] palette=%d %s", event.location.c_str(),
                           event.palette_id, event.message.c_str());
  }
  if (event.color_count > 0) {
    return absl::StrFormat("- [%s] colors=%d %s", event.location.c_str(),
                           event.color_count, event.message.c_str());
  }
  return absl::StrFormat("- [%s] %s", event.location.c_str(),
                         event.message.c_str());
}

using zelda3::GetObjectName;

}  // namespace

std::string DungeonCanvasViewer::BuildRoomMetadataSummary(
    const zelda3::Room& room, int room_id) const {
  const int resolved_palette_id = room.ResolveDungeonPaletteId();
  const uint8_t entrance_blockset = room.render_entrance_blockset();
  std::string entrance_context = "Entrance:none";
  if (current_entrance_id_ >= 0 || entrance_blockset != 0xFF) {
    const std::string entrance_id_text =
        current_entrance_id_ >= 0
            ? absl::StrFormat("%02X", current_entrance_id_)
            : std::string("--");
    const std::string entrance_blockset_text =
        entrance_blockset != 0xFF
            ? absl::StrFormat("%02X", static_cast<int>(entrance_blockset))
            : std::string("--");
    entrance_context =
        absl::StrFormat("Entrance:%s EB:%s", entrance_id_text.c_str(),
                        entrance_blockset_text.c_str());
  }

  std::string summary = absl::StrFormat(
      "Room 0x%03X [%s] | B:%02X P:%02X->%02X L:%02X S:%02X | %s | Group:%s | "
      "Floor:%d Effect:%d Tag1:%d Tag2:%d\n%s\n%s",
      room_id, zelda3::GetRoomLabel(room_id).c_str(), room.blockset(),
      room.palette(), resolved_palette_id, room.layout_id(), room.spriteset(),
      entrance_context, GetBlocksetGroupName(room.blockset()), room.floor1(),
      room.effect(), room.tag1(), room.tag2(),
      BuildReportSessionSummary(rom_, project_).c_str(),
      BuildEffectiveBlockListSummary(room).c_str());

  // Connectivity diagnostics: enumerate doors/staircases/holewarp without
  // reciprocity filtering so the report captures the room's actual link
  // configuration. Staircase configuration issues (stale headers, missing
  // destinations, extra placed objects beyond 4 slots) are listed
  // separately so a reader can spot phantom or unreachable connections.
  const auto link_diagnostics =
      CollectDungeonConnectedRoomLinkDiagnostics(room_id, room, nullptr);
  if (!link_diagnostics.links.empty() ||
      !link_diagnostics.staircase_issues.empty()) {
    summary += "\nConnectivity:";
    for (const auto& link : link_diagnostics.links) {
      summary += "\n  ";
      summary += FormatDungeonConnectedLinkDescription(link);
    }
    for (const auto& issue : link_diagnostics.staircase_issues) {
      summary += "\n  ";
      summary += FormatDungeonStaircaseIssueDescription(issue);
    }
  }

  return summary;
}

std::string DungeonCanvasViewer::BuildDrawIssueReport(const zelda3::Room& room,
                                                      int room_id) const {
  std::string report = "Dungeon Draw Issue Report\n";
  report += BuildRoomMetadataSummary(room, room_id);
  report += absl::StrFormat(
      "\nResolved palette group: 0x%02X\nInteraction mode: %s\nView: BG1=%s "
      "BG2=%s Grid=%s Bounds=%s Sprites=%s Pots=%s Collision=%s Camera=%s",
      room.ResolveDungeonPaletteId(),
      object_interaction_.mode_manager().GetModeName(),
      IsBG1Visible(room_id) ? "on" : "off",
      IsBG2Visible(room_id) ? "on" : "off", show_grid_ ? "on" : "off",
      show_object_bounds_ ? "on" : "off",
      entity_visibility_.show_sprites ? "on" : "off",
      entity_visibility_.show_pot_items ? "on" : "off",
      show_custom_collision_overlay_ ? "on" : "off",
      show_camera_quadrant_overlay_ ? "on" : "off");

  const auto selected = object_interaction_.GetSelectedObjectIndices();
  if (!selected.empty()) {
    report += absl::StrFormat("\nSelected objects: %zu", selected.size());
    if (selected.size() == 1) {
      const auto& objects = room.GetTileObjects();
      const size_t index = selected.front();
      if (index < objects.size()) {
        const auto& obj = objects[index];
        report += absl::StrFormat(
            "\nObject: id=0x%03X name=%s pos=(%d,%d) size=0x%02X %s", obj.id_,
            GetObjectName(obj.id_).c_str(), obj.x_, obj.y_, obj.size_,
            BuildSelectedObjectSemanticsSummary(obj).c_str());
        if (auto tiles = obj.GetTiles(); tiles.ok()) {
          report += "\n";
          report += FormatObjectTileSample(*tiles, 16, "Object tiles:");
        }

        auto [bounds_x, bounds_y, bounds_w, bounds_h] =
            zelda3::DimensionService::Get().GetSelectionBoundsPixels(obj);
        bounds_w = std::max(bounds_w, 8);
        bounds_h = std::max(bounds_h, 8);
        const int origin_x = obj.x_ * 8;
        const int origin_y = obj.y_ * 8;
        const int center_x = bounds_x + std::max(0, (bounds_w / 2) - 1);
        const int center_y = bounds_y + std::max(0, (bounds_h / 2) - 1);
        report += absl::StrFormat(
            "\nObject geometry: selection_bounds_px=(%d,%d,%d,%d) "
            "object_origin_px=(%d,%d) center_px=(%d,%d)",
            bounds_x, bounds_y, bounds_w, bounds_h, origin_x, origin_y,
            center_x, center_y);
        const auto trace_report =
            BuildObjectTraceReport(rom_, room_id, obj, current_palette_group_);
        report += trace_report.summary;

        auto& palette_debugger = zelda3::PaletteDebugger::Get();
        auto append_sample = [&](std::string_view label, int sample_x,
                                 int sample_y) {
          const auto comp = palette_debugger.SamplePixelAt(sample_x, sample_y);
          palette_debugger.AddComparison(comp);
          const std::string label_text(label);
          report += absl::StrFormat(
              "\nPalette sample %s (%d,%d): idx=%d expected=(%d,%d,%d) "
              "actual=(%d,%d,%d) match=%s",
              label_text.c_str(), sample_x, sample_y,
              static_cast<int>(comp.palette_index),
              static_cast<int>(comp.expected_r),
              static_cast<int>(comp.expected_g),
              static_cast<int>(comp.expected_b),
              static_cast<int>(comp.actual_r), static_cast<int>(comp.actual_g),
              static_cast<int>(comp.actual_b), comp.matches ? "yes" : "no");
        };

        if (auto trace_sample = ChooseObjectTracePaletteSample(
                trace_report.writes, palette_debugger)) {
          append_sample(trace_sample->label, trace_sample->x, trace_sample->y);
        } else {
          append_sample("selection-origin", bounds_x, bounds_y);
          if (center_x != bounds_x || center_y != bounds_y) {
            append_sample("selection-center", center_x, center_y);
          }
        }
      }
    }
  }

  if (object_interaction_.HasEntitySelection()) {
    const auto selected_entity = object_interaction_.GetSelectedEntity();
    switch (selected_entity.type) {
      case EntityType::Door: {
        const auto& doors = room.GetDoors();
        if (selected_entity.index < doors.size()) {
          const auto& door = doors[selected_entity.index];
          report += BuildDoorIssueSummary(door, selected_entity.index,
                                          "Selected door");
        }
        break;
      }
      case EntityType::Sprite: {
        const auto& sprites = room.GetSprites();
        if (selected_entity.index < sprites.size()) {
          const auto& sprite = sprites[selected_entity.index];
          report += absl::StrFormat(
              "\nSelected entity: sprite id=0x%02X name=%s pos=(%d,%d) "
              "layer=%d",
              sprite.id(), zelda3::ResolveSpriteName(sprite.id()), sprite.x(),
              sprite.y(), sprite.layer());
        }
        break;
      }
      case EntityType::Item: {
        const auto& items = room.GetPotItems();
        if (selected_entity.index < items.size()) {
          const auto& item = items[selected_entity.index];
          report +=
              absl::StrFormat("\nSelected entity: pot item=0x%02X pos=(%d,%d)",
                              item.item, item.GetPixelX(), item.GetPixelY());
        }
        break;
      }
      default:
        break;
    }
  }

  const auto& palette_events = zelda3::PaletteDebugger::Get().GetEvents();
  if (!palette_events.empty()) {
    report +=
        absl::StrFormat("\nPalette debug events: %zu", palette_events.size());
    const size_t preview_count = std::min<size_t>(palette_events.size(), 4);
    const size_t start = palette_events.size() - preview_count;
    for (size_t i = start; i < palette_events.size(); ++i) {
      const auto& event = palette_events[i];
      report += "\n";
      report += FormatPaletteDebugEventForReport(event);
    }
  }

  return report;
}

std::string DungeonCanvasViewer::BuildSelectionIssueReport(
    const zelda3::Room& room, int room_id) const {
  std::string report = "Dungeon Selection Issue Report\n";
  report += BuildRoomMetadataSummary(room, room_id);
  report +=
      absl::StrFormat("\nInteraction mode: %s\nResolved palette group: 0x%02X",
                      object_interaction_.mode_manager().GetModeName(),
                      room.ResolveDungeonPaletteId());

  const auto selected_objects = object_interaction_.GetSelectedObjectIndices();
  if (!selected_objects.empty()) {
    report +=
        absl::StrFormat("\nSelected objects: %zu", selected_objects.size());
    const auto& objects = room.GetTileObjects();
    const size_t preview_count = std::min<size_t>(selected_objects.size(), 4);
    for (size_t i = 0; i < preview_count; ++i) {
      const size_t index = selected_objects[i];
      if (index >= objects.size()) {
        continue;
      }
      const auto& obj = objects[index];
      report += absl::StrFormat(
          "\n- object[%zu] id=0x%03X name=%s pos=(%d,%d) size=0x%02X %s", index,
          obj.id_, GetObjectName(obj.id_).c_str(), obj.x_, obj.y_, obj.size_,
          BuildSelectedObjectSemanticsSummary(obj).c_str());
      auto [bounds_x, bounds_y, bounds_w, bounds_h] =
          zelda3::DimensionService::Get().GetSelectionBoundsPixels(obj);
      bounds_w = std::max(bounds_w, 8);
      bounds_h = std::max(bounds_h, 8);
      report += absl::StrFormat(
          "\n  geometry: selection_bounds_px=(%d,%d,%d,%d) "
          "object_origin_px=(%d,%d)",
          bounds_x, bounds_y, bounds_w, bounds_h, obj.x_ * 8, obj.y_ * 8);
      if (auto tiles = obj.GetTiles(); tiles.ok() && !tiles->empty()) {
        report += "\n  ";
        report += FormatObjectTileSample(*tiles, 10, "tiles:");
      }
      const auto trace_report =
          BuildObjectTraceReport(rom_, room_id, obj, current_palette_group_);
      report += "\n  ";
      report += BuildObjectTraceBoundsSummary(trace_report.writes, bounds_x,
                                              bounds_y, bounds_w, bounds_h);
    }
    if (selected_objects.size() > preview_count) {
      report += absl::StrFormat("\n- ... %zu more object(s)",
                                selected_objects.size() - preview_count);
    }
    return report;
  }

  if (object_interaction_.HasEntitySelection()) {
    const auto selected_entity = object_interaction_.GetSelectedEntity();
    switch (selected_entity.type) {
      case EntityType::Door: {
        const auto& doors = room.GetDoors();
        if (selected_entity.index < doors.size()) {
          const auto& door = doors[selected_entity.index];
          report += BuildDoorIssueSummary(door, selected_entity.index,
                                          "Selected door");
          return report;
        }
        break;
      }
      case EntityType::Sprite: {
        const auto& sprites = room.GetSprites();
        if (selected_entity.index < sprites.size()) {
          const auto& sprite = sprites[selected_entity.index];
          report += absl::StrFormat(
              "\nSelected sprite[%zu]:\n"
              "- id=0x%02X\n"
              "- name=%s\n"
              "- pos=(%d,%d)\n"
              "- layer=%d",
              selected_entity.index, sprite.id(),
              zelda3::ResolveSpriteName(sprite.id()), sprite.x(), sprite.y(),
              sprite.layer());
          return report;
        }
        break;
      }
      case EntityType::Item: {
        const auto& items = room.GetPotItems();
        if (selected_entity.index < items.size()) {
          const auto& item = items[selected_entity.index];
          report += absl::StrFormat(
              "\nSelected pot item[%zu]:\n"
              "- item=0x%02X\n"
              "- pixel_pos=(%d,%d)",
              selected_entity.index, item.item, item.GetPixelX(),
              item.GetPixelY());
          return report;
        }
        break;
      }
      default:
        break;
    }
  }

  report += "\nNo active object or entity selection.";
  return report;
}

void DungeonCanvasViewer::RefreshIssueReportOutputTargets() {
  issue_report_popup_log_target_path_.clear();
  issue_report_popup_screenshot_dir_.clear();

  auto paths_or = ResolveDungeonIssueReportPaths();
  if (!paths_or.ok()) {
    SetIssueReportPopupStatus(std::string(paths_or.status().message()), true);
    return;
  }

  issue_report_popup_log_target_path_ =
      util::PlatformPaths::NormalizePathForDisplay(paths_or->log_path);
  issue_report_popup_screenshot_dir_ =
      util::PlatformPaths::NormalizePathForDisplay(paths_or->screenshots_dir);
}

void DungeonCanvasViewer::SetIssueReportPopupStatus(std::string message,
                                                    bool is_error) {
  issue_report_popup_status_message_ = std::move(message);
  issue_report_popup_status_is_error_ = is_error;
}

void DungeonCanvasViewer::MarkIssueReportDirty() {
  issue_report_popup_persisted_ = false;
}

void DungeonCanvasViewer::DrawIssueReportStorageSummary() const {
  if (issue_report_popup_log_target_path_.empty() &&
      issue_report_popup_screenshot_dir_.empty() &&
      issue_report_popup_screenshot_path_.empty() &&
      issue_report_popup_last_log_path_.empty()) {
    return;
  }

  gui::SeparatorText(kIssueReportSectionPaths);
  if (!issue_report_popup_log_target_path_.empty()) {
    ImGui::TextWrapped("Log target: %s",
                       issue_report_popup_log_target_path_.c_str());
  }
  if (!issue_report_popup_screenshot_dir_.empty() &&
      issue_report_popup_screenshot_path_.empty()) {
    ImGui::TextWrapped("Screenshot dir: %s",
                       issue_report_popup_screenshot_dir_.c_str());
  }
  if (!issue_report_popup_screenshot_path_.empty()) {
    ImGui::TextWrapped("Screenshot: %s",
                       issue_report_popup_screenshot_path_.c_str());
  }
  if (!issue_report_popup_last_log_path_.empty()) {
    ImGui::TextWrapped("Issue log: %s",
                       issue_report_popup_last_log_path_.c_str());
  }
}

void DungeonCanvasViewer::DrawIssueReportStatusMessage() const {
  if (issue_report_popup_status_message_.empty()) {
    return;
  }

  const ImVec4 color = issue_report_popup_status_is_error_
                           ? gui::GetErrorColor()
                           : gui::GetSuccessColor();
  ImGui::TextColored(color, "%s", issue_report_popup_status_message_.c_str());
}

absl::Status DungeonCanvasViewer::PrepareIssueReportPopup(
    const std::string& title, const std::string& summary,
    const std::string& kind_label, const std::string& diagnostics, int room_id,
    int default_category_index) {
  issue_report_popup_title_ = title;
  issue_report_popup_kind_ = kind_label;
  issue_report_popup_diagnostics_ = diagnostics;
  issue_report_popup_room_id_ = room_id;
  issue_report_category_index_ =
      std::clamp(default_category_index, 0,
                 static_cast<int>(kDungeonIssueCategoryLabels.size()) - 1);
  issue_report_popup_screenshot_path_.clear();
  issue_report_popup_last_log_path_.clear();
  issue_report_popup_persisted_ = false;
  SetIssueReportPopupStatus("", false);
  RefreshIssueReportOutputTargets();
  std::snprintf(issue_report_summary_, sizeof(issue_report_summary_), "%s",
                summary.c_str());
  issue_report_notes_[0] = '\0';
  const auto initial_persist_status = EnsureIssueReportPersisted();
  if (initial_persist_status.ok()) {
    SetIssueReportPopupStatus(
        absl::StrFormat(kIssueReportStatusStartedLog,
                        issue_report_popup_last_log_path_.c_str()),
        false);
  } else {
    SetIssueReportPopupStatus(std::string(initial_persist_status.message()),
                              true);
  }
  return initial_persist_status;
}

void DungeonCanvasViewer::OpenIssueReportPopup(const std::string& title,
                                               const std::string& summary,
                                               const std::string& kind_label,
                                               const std::string& diagnostics,
                                               int room_id,
                                               int default_category_index) {
  (void)PrepareIssueReportPopup(title, summary, kind_label, diagnostics,
                                room_id, default_category_index);
  canvas_.OpenPersistentPopup(issue_report_popup_id_, [this]() {
    if (ImGui::BeginPopupModal(issue_report_popup_id_.c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      if (!issue_report_popup_title_.empty()) {
        ImGui::TextUnformatted(issue_report_popup_title_.c_str());
        if (!issue_report_popup_kind_.empty()) {
          ImGui::TextDisabled("%s", issue_report_popup_kind_.c_str());
        }
        ImGui::Separator();
      }

      const char* category_preview =
          GetIssueCategoryLabel(issue_report_category_index_);
      if (ImGui::BeginCombo("Category", category_preview)) {
        for (size_t i = 0; i < kDungeonIssueCategoryLabels.size(); ++i) {
          const bool selected =
              issue_report_category_index_ == static_cast<int>(i);
          if (ImGui::Selectable(kDungeonIssueCategoryLabels[i], selected)) {
            issue_report_category_index_ = static_cast<int>(i);
            MarkIssueReportDirty();
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }

      ImGui::SetNextItemWidth(kIssueReportDialogWidth);
      if (ImGui::InputTextWithHint("Summary", kIssueReportSummaryHint,
                                   issue_report_summary_,
                                   sizeof(issue_report_summary_))) {
        MarkIssueReportDirty();
      }
      if (ImGui::InputTextMultiline(
              "Observed issue", issue_report_notes_,
              sizeof(issue_report_notes_),
              ImVec2(kIssueReportDialogWidth, kIssueReportNotesHeight))) {
        MarkIssueReportDirty();
      }

      DrawIssueReportStorageSummary();
      DrawIssueReportStatusMessage();
      ImGui::TextDisabled(
          "Reports auto-save on open, screenshot capture, copy, or close.");

      if (ImGui::CollapsingHeader(kIssueReportSectionDiagnostics,
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputTextMultiline(
            kIssueReportPopupIdDiagnostics,
            issue_report_popup_diagnostics_.data(),
            issue_report_popup_diagnostics_.size() + 1,
            ImVec2(kIssueReportDialogWidth, kIssueReportDiagnosticsHeight),
            ImGuiInputTextFlags_ReadOnly);
      }

#ifdef YAZE_WITH_GRPC
      const char* capture_label = issue_report_popup_screenshot_path_.empty()
                                      ? kIssueReportCaptureButtonLabel
                                      : kIssueReportRecaptureButtonLabel;
      if (ImGui::Button(capture_label)) {
        const auto status = CaptureIssueReportScreenshot();
        if (status.ok()) {
          MarkIssueReportDirty();
          const auto persist_status = EnsureIssueReportPersisted();
          if (persist_status.ok()) {
            SetIssueReportPopupStatus(
                absl::StrFormat(kIssueReportStatusSavedScreenshot,
                                issue_report_popup_screenshot_path_.c_str()),
                false);
          } else {
            SetIssueReportPopupStatus(std::string(persist_status.message()),
                                      true);
          }
        } else {
          SetIssueReportPopupStatus(std::string(status.message()), true);
        }
      }
      ImGui::SameLine();
#endif
      if (ImGui::Button(ICON_MD_SAVE_AS " Save to Issue Log")) {
        const auto status = EnsureIssueReportPersisted();
        if (status.ok()) {
          SetIssueReportPopupStatus(
              absl::StrFormat(kIssueReportStatusSavedLog,
                              issue_report_popup_last_log_path_.c_str()),
              false);
        } else {
          SetIssueReportPopupStatus(std::string(status.message()), true);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_CONTENT_COPY " Copy Report")) {
        std::string report = BuildIssueReportClipboardText();
        ImGui::SetClipboardText(report.c_str());
        const auto status = EnsureIssueReportPersisted();
        if (status.ok()) {
          SetIssueReportPopupStatus(
              absl::StrFormat(kIssueReportStatusCopiedReport,
                              issue_report_popup_last_log_path_.c_str()),
              false);
        } else {
          SetIssueReportPopupStatus(std::string(status.message()), true);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ASSIGNMENT " Copy Diagnostics")) {
        ImGui::SetClipboardText(issue_report_popup_diagnostics_.c_str());
        const auto status = EnsureIssueReportPersisted();
        if (status.ok()) {
          SetIssueReportPopupStatus(
              absl::StrFormat(kIssueReportStatusCopiedDiagnostics,
                              issue_report_popup_last_log_path_.c_str()),
              false);
        } else {
          SetIssueReportPopupStatus(std::string(status.message()), true);
        }
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_CLOSE " Close")) {
        const auto status = EnsureIssueReportPersisted();
        if (!status.ok()) {
          SetIssueReportPopupStatus(std::string(status.message()), true);
          ImGui::EndPopup();
          return;
        }
        canvas_.ClosePersistentPopup(issue_report_popup_id_);
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
      return;
    }

    if (!issue_report_popup_persisted_) {
      (void)EnsureIssueReportPersisted();
    }
  });
}

std::string DungeonCanvasViewer::BuildIssueReportClipboardText() const {
  std::string report;
  if (!issue_report_popup_kind_.empty()) {
    report += issue_report_popup_kind_ + "\n";
  }
  report += absl::StrFormat(
      "Category: %s\n", GetIssueCategoryLabel(issue_report_category_index_));
  if (issue_report_summary_[0] != '\0') {
    report += absl::StrFormat("Summary: %s\n", issue_report_summary_);
  }
  if (!issue_report_popup_screenshot_path_.empty()) {
    report += absl::StrFormat("Screenshot: %s\n",
                              issue_report_popup_screenshot_path_.c_str());
  }
  if (issue_report_notes_[0] != '\0') {
    report += absl::StrFormat("\nObserved issue:\n%s\n", issue_report_notes_);
  }
  report += "\nDiagnostics:\n";
  report += issue_report_popup_diagnostics_;
  return report;
}

absl::Status DungeonCanvasViewer::CaptureIssueReportScreenshot() {
#ifdef YAZE_WITH_GRPC
  if (!has_canvas_capture_region_ || canvas_capture_width_ <= 0 ||
      canvas_capture_height_ <= 0) {
    return absl::FailedPreconditionError(
        "Room canvas region is not available for screenshot capture");
  }

  const std::string timestamp = BuildIssueTimestampSlug();
  auto screenshot_path_or = BuildDungeonIssueScreenshotPath(
      issue_report_popup_room_id_,
      GetIssueCategoryLabel(issue_report_category_index_), timestamp);
  if (!screenshot_path_or.ok()) {
    return screenshot_path_or.status();
  }

  yaze::test::CaptureRegion region;
  region.x = canvas_capture_x_;
  region.y = canvas_capture_y_;
  region.width = canvas_capture_width_;
  region.height = canvas_capture_height_;

  auto artifact_or = yaze::test::CaptureHarnessScreenshotRegion(
      std::optional<yaze::test::CaptureRegion>(region),
      screenshot_path_or->string(),
      /*reveal_to_user=*/false);
  if (!artifact_or.ok()) {
    return artifact_or.status();
  }

  issue_report_popup_screenshot_path_ =
      util::PlatformPaths::NormalizePathForDisplay(artifact_or->file_path);
  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "Screenshot capture is unavailable without YAZE_WITH_GRPC");
#endif
}

absl::Status DungeonCanvasViewer::AppendIssueReportToLog() {
  DungeonIssueLogEntry entry;
  entry.timestamp_display = BuildIssueTimestampDisplay();
  entry.heading = (issue_report_summary_[0] != '\0') ? issue_report_summary_
                                                     : issue_report_popup_kind_;
  entry.category_label = GetIssueCategoryLabel(issue_report_category_index_);
  entry.scope = issue_report_popup_kind_;
  entry.room_id = issue_report_popup_room_id_;
  entry.screenshot_path = issue_report_popup_screenshot_path_;
  entry.observed_issue = issue_report_notes_;
  entry.diagnostics = issue_report_popup_diagnostics_;

  auto log_path_or = AppendDungeonIssueLogEntry(entry);
  if (!log_path_or.ok()) {
    return log_path_or.status();
  }

  issue_report_popup_last_log_path_ =
      util::PlatformPaths::NormalizePathForDisplay(*log_path_or);
  return absl::OkStatus();
}

absl::Status DungeonCanvasViewer::EnsureIssueReportPersisted() {
  if (issue_report_popup_persisted_ &&
      !issue_report_popup_last_log_path_.empty()) {
    return absl::OkStatus();
  }

  RETURN_IF_ERROR(AppendIssueReportToLog());
  issue_report_popup_persisted_ = true;
  return absl::OkStatus();
}

}  // namespace yaze::editor
