#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/resource/arena.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {
using Document = core::LyraTerrainDocument;
namespace fs = std::filesystem;

absl::StatusOr<std::optional<std::string>> ReadSource(const fs::path& path) {
  std::error_code ec;
  if (!fs::exists(path, ec) && !ec)
    return std::nullopt;
  if (ec || !fs::is_regular_file(path, ec) || ec)
    return absl::FailedPreconditionError(
        "Terrain source is not a regular file");
  if (fs::file_size(path, ec) > 4 * 1024 * 1024 || ec)
    return absl::InvalidArgumentError("Terrain source exceeds 4 MiB");
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return absl::PermissionDeniedError("Cannot read terrain source");
  std::string text{std::istreambuf_iterator<char>(file), {}};
  if (file.bad())
    return absl::DataLossError("Could not finish reading terrain source");
  return std::optional<std::string>(std::move(text));
}

absl::Status ValidateTarget(const fs::path& path,
                            const project::YazeProject& project) {
  if (path.extension() != ".json")
    return absl::InvalidArgumentError(
        "Lyra terrain must use a separate .json file");
  std::error_code ec;
  const auto canonical_target = fs::weakly_canonical(path, ec);
  if (ec)
    return absl::FailedPreconditionError("Cannot resolve terrain source path");
  const bool target_exists = fs::exists(path, ec);
  if (ec)
    return absl::FailedPreconditionError("Cannot inspect terrain source path");
  for (const auto& protected_path :
       {project.filepath, project.GetAbsolutePath(project.rom_filename)}) {
    if (protected_path.empty())
      continue;
    const auto canonical_protected = fs::weakly_canonical(protected_path, ec);
    if (ec)
      return absl::FailedPreconditionError(
          "Cannot resolve project or ROM path");
    if (canonical_target == canonical_protected)
      return absl::FailedPreconditionError(
          "Terrain source aliases the project or ROM");
    const bool protected_exists = fs::exists(protected_path, ec);
    if (ec)
      return absl::FailedPreconditionError(
          "Cannot inspect project or ROM path");
    if (!target_exists || !protected_exists)
      continue;
    const bool aliases = fs::equivalent(path, protected_path, ec);
    if (ec)
      return absl::FailedPreconditionError(
          "Cannot compare terrain source identity");
    if (aliases)
      return absl::FailedPreconditionError(
          "Terrain source aliases the project or ROM");
  }
  return absl::OkStatus();
}

void AddWord(std::string& bytes, uint16_t word) {
  bytes.push_back(static_cast<char>(word));
  bytes.push_back(static_cast<char>(word >> 8));
}

class TerrainStrokeAction final : public UndoAction {
 public:
  TerrainStrokeAction(std::shared_ptr<Document> document,
                      core::LyraTerrainStroke stroke)
      : document_(std::move(document)), stroke_(std::move(stroke)) {}
  absl::Status Undo() override { return document_->ApplyStroke(stroke_, true); }
  absl::Status Redo() override {
    return document_->ApplyStroke(stroke_, false);
  }
  std::string Description() const override { return "Paint Lyra terrain"; }
  size_t MemoryUsage() const override {
    return sizeof(*this) +
           stroke_.changes.size() * sizeof(core::LyraTerrainCellChange);
  }

 private:
  std::shared_ptr<Document> document_;
  core::LyraTerrainStroke stroke_;
};
}  // namespace

absl::StatusOr<std::string> PublishLyraTerrainSource(
    const project::YazeProject& project, const std::string& source_path,
    const std::optional<std::string>& source_bytes,
    const core::LyraTerrainDocument& document) {
  if (project.GetAbsolutePath(project.lyra_terrain_file) != source_path)
    return absl::FailedPreconditionError(
        "Lyra project source changed; draft retained");
  RETURN_IF_ERROR(ValidateTarget(source_path, project));
  ASSIGN_OR_RETURN(auto lock, core::AcquireSourceArtifactPublicationLock(
                                  {fs::absolute(project.filepath), source_path},
                                  {"Lyra terrain", "Lyra terrain file"}));
  ASSIGN_OR_RETURN(auto current, ReadSource(source_path));
  if (current != source_bytes)
    return absl::FailedPreconditionError(
        "Lyra terrain changed on disk; refusing stale write");
  ASSIGN_OR_RETURN(auto project_bytes, ReadSource(project.filepath));
  if (!project_bytes)
    return absl::FailedPreconditionError(
        "Save the project file before publishing Lyra terrain");
  const std::string after = document.Serialize();
  // The existing project is the primary CAS source, allowing first creation of
  // the sidecar through the same atomic publisher as existing source artifacts.
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *lock,
      {{fs::absolute(project.filepath), project_bytes, *project_bytes},
       {source_path, source_bytes, after}},
      core::ComputeSourceArtifactSha256(*project_bytes), [&]() -> absl::Status {
        ASSIGN_OR_RETURN(auto saved, ReadSource(source_path));
        if (!saved)
          return absl::DataLossError("Lyra terrain readback is missing");
        return Document::Parse(*saved, document.profile()).status();
      }));
  return after;
}

LyraTerrainCanvas::~LyraTerrainCanvas() {
  Reset();
}

absl::Status LyraTerrainCanvas::BindSource(zelda3::Overworld& overworld,
                                           core::LyraTerrainProfile* profile,
                                           bool make_atlas) {
  if (!overworld.is_loaded() || overworld.is_lyra_preview())
    return absl::FailedPreconditionError(
        "Open the main project ROM to draw Lyra terrain");
  const auto* donor = overworld.mutable_overworld_map(0x3A);
  if (!donor)
    return absl::FailedPreconditionError("Loom map $3A is unavailable");
  const auto* game_data = donor->game_data();
  if (!game_data || !game_data->gfx_groups_loaded ||
      game_data->palette_groups.grass.empty() ||
      game_data->palette_groups.grass[0].size() < 3 ||
      game_data->palette_groups.hud.empty() ||
      game_data->palette_groups.hud[0].size() < 32)
    return absl::FailedPreconditionError(
        "Load native graphics and palettes before drawing Lyra terrain");
  for (const auto sheet : profile->graphics_sheets) {
    if ((static_cast<size_t>(sheet) + 1) * 0x1000 >
        game_data->graphics_buffer.size())
      return absl::FailedPreconditionError(
          "Loom native graphics sheets are incomplete");
  }
  if ((static_cast<size_t>(profile->animated_sheet) + 1) * 0x1000 >
      game_data->graphics_buffer.size())
    return absl::FailedPreconditionError(
        "Loom native animation sheet is incomplete");
  // Work with a copy: building this atlas cannot alter normal map selection,
  // properties, dirty flags, or renderer caches.
  auto source = *donor;
  source.set_game_state(0);
  source.LoadAreaGraphics();
  for (size_t i = 0; i < profile->graphics_sheets.size(); ++i) {
    if (source.static_graphics(i) != profile->graphics_sheets[i])
      return absl::FailedPreconditionError(absl::StrFormat(
          "Loom graphics mismatch at slot %d: expected $%02X, found $%02X", i,
          profile->graphics_sheets[i], source.static_graphics(i)));
  }
  if (source.render_main_palette() != profile->main_palette ||
      source.area_palette() != profile->palette_set ||
      source.animated_sheet() != profile->animated_sheet)
    return absl::FailedPreconditionError(
        "Loom palette or animation profile mismatch");
  RETURN_IF_ERROR(source.BuildTileset());
  RETURN_IF_ERROR(source.LoadPalette());
  if (source.current_palette().size() != 256)
    return absl::FailedPreconditionError("Loom resolved palette is incomplete");
  if (overworld.tiles16().empty() || overworld.tiles16().size() > 4096)
    return absl::FailedPreconditionError(
        "Loom Tile16 definitions exceed native atlas capacity");
  profile->tile_count = overworld.tiles16().size();
  std::string definitions;
  for (const auto& tile : overworld.tiles16())
    for (const auto& quadrant : tile.tiles_info)
      AddWord(definitions, gfx::TileInfoToWord(quadrant));
  profile->tile16_definition_fingerprint =
      core::ComputeSourceArtifactSha256(definitions);
  const auto& pixels = source.current_graphics();
  profile->graphics_fingerprint = core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(pixels.data()), pixels.size()});
  std::string colors;
  for (size_t i = 0; i < source.current_palette().size(); ++i)
    AddWord(colors, source.current_palette()[i].snes());
  profile->palette_fingerprint = core::ComputeSourceArtifactSha256(colors);
  // Include the animation selector and resolved animated pixels (slot 7).
  std::string animation(1, static_cast<char>(source.animated_gfx()));
  if (pixels.size() < 0x8000)
    return absl::FailedPreconditionError(
        "Loom decoded graphics are incomplete");
  animation.append(reinterpret_cast<const char*>(pixels.data() + 0x7000),
                   0x1000);
  profile->animation_fingerprint = core::ComputeSourceArtifactSha256(animation);
  RETURN_IF_ERROR(profile->Validate());
  if (make_atlas) {
    RETURN_IF_ERROR(source.BuildTiles16Gfx(*overworld.mutable_tiles16(),
                                           profile->tile_count));
    const int height = ((profile->tile_count + 7) / 8) * 16;
    const auto& blockset = source.current_tile16_blockset();
    const size_t atlas_bytes = static_cast<size_t>(128) * height;
    if (blockset.size() < atlas_bytes)
      return absl::FailedPreconditionError(
          "Loom Tile16 atlas pixels are incomplete");
    std::vector<uint8_t> atlas(blockset.begin(),
                               blockset.begin() + atlas_bytes);
    gfx::Arena::Get().RetireBitmap(atlas_);
    atlas_.Create(128, height, 8, atlas);
    atlas_.SetPalette(source.current_palette());
    gfx::Arena::Get().QueueTextureCommand(
        atlas_.texture() ? gfx::Arena::TextureCommandType::UPDATE
                         : gfx::Arena::TextureCommandType::CREATE,
        &atlas_);
  }
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::Load(project::YazeProject* project,
                                     zelda3::Overworld& overworld) {
  if (dirty())
    return absl::FailedPreconditionError(
        "Save the current Lyra terrain draft before replacing it");
  // ROM replacement can keep the sidecar path while changing its native tile
  // profile. Retire the old view and validate the full binding on every load.
  Reset();
  if (!project || project->lyra_terrain_file.empty())
    return absl::OkStatus();
  const auto path = project->GetAbsolutePath(project->lyra_terrain_file);
  RETURN_IF_ERROR(ValidateTarget(path, *project));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile, false));
  ASSIGN_OR_RETURN(auto bytes, ReadSource(path));
  if (!bytes)
    return absl::NotFoundError("The project's Lyra terrain file is missing");
  ASSIGN_OR_RETURN(auto document, Document::Parse(*bytes, profile));
  RETURN_IF_ERROR(BindSource(overworld, &profile, true));
  document_ = std::make_shared<Document>(std::move(document));
  brush_ = {};
  source_path_ = path;
  source_bytes_ = std::move(bytes);
  // Keep the live reference absolute so project Save As can rebase it safely.
  project->lyra_terrain_file = source_path_;
  unpublished_ = false;
  return absl::OkStatus();
}

absl::Status LyraTerrainCanvas::Create(project::YazeProject* project,
                                       zelda3::Overworld& overworld) {
  if (!project || !project->project_opened())
    return absl::FailedPreconditionError(
        "Save a Yaze project before creating Lyra terrain");
  if (!project->lyra_terrain_file.empty())
    return Load(project, overworld);
  if (document_)
    return absl::FailedPreconditionError("Lyra terrain is already loaded");
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile, false));
  fs::path path(project->filepath);
  path.replace_extension(".lyra-terrain.json");
  RETURN_IF_ERROR(ValidateTarget(path, *project));
  ASSIGN_OR_RETURN(auto bytes, ReadSource(path));
  if (bytes)
    return absl::AlreadyExistsError(
        "A Lyra terrain file already exists; add its project reference to open "
        "it");
  ASSIGN_OR_RETURN(auto document, Document::Create(profile));
  RETURN_IF_ERROR(BindSource(overworld, &profile, true));
  document_ = std::make_shared<Document>(std::move(document));
  brush_ = {};
  source_path_ = fs::absolute(path).lexically_normal().string();
  source_bytes_.reset();
  project->lyra_terrain_file = source_path_;
  unpublished_ = true;
  return absl::OkStatus();
}

bool LyraTerrainCanvas::dirty() const {
  return document_ &&
         (unpublished_ || pending_project_commit_ || document_->dirty());
}

absl::Status LyraTerrainCanvas::Save(project::YazeProject* project,
                                     zelda3::Overworld& overworld) {
  if (!dirty())
    return absl::OkStatus();
  if (!project ||
      project->GetAbsolutePath(project->lyra_terrain_file) != source_path_)
    return absl::FailedPreconditionError(
        "Lyra project source changed; draft retained");
  RETURN_IF_ERROR(ValidateTarget(source_path_, *project));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile, false));
  if (profile != document_->profile())
    return absl::FailedPreconditionError(
        "Lyra tile definitions or graphics profile changed; draft retained");
  ASSIGN_OR_RETURN(auto after,
                   PublishLyraTerrainSource(*project, source_path_,
                                            source_bytes_, *document_));
  source_bytes_ = after;
  unpublished_ = false;
  pending_project_commit_ = true;
  document_->MarkSaved();
  return absl::OkStatus();
}

void LyraTerrainCanvas::FinishStroke(UndoManager& history) {
  if (!stroke_active_ || !document_)
    return;
  auto stroke = document_->EndStroke();
  stroke_active_ = false;
  last_cell_.reset();
  if (!stroke.ok()) {
    status_ = stroke.status();
    return;
  }
  if (!stroke->empty())
    history.Push(
        std::make_unique<TerrainStrokeAction>(document_, std::move(*stroke)));
}

absl::Status LyraTerrainCanvas::PaintPattern(const TileBrush& brush, int x,
                                             int y, int width, int height,
                                             bool finalize,
                                             UndoManager& history) {
  if (!document_)
    return absl::FailedPreconditionError("Lyra terrain is unavailable");
  if (!brush.valid() || width <= 0 || width > Document::kWidth || height <= 0 ||
      height > 256 || x < 0 || x >= Document::kWidth || y < 0 ||
      y >= Document::kHeight)
    return absl::InvalidArgumentError("Invalid Lyra terrain brush footprint");
  for (const int tile : brush.tile_ids) {
    if (tile >= document_->profile().tile_count)
      return absl::InvalidArgumentError(
          "Brush contains a Tile16 outside the Lyra graphics profile");
  }
  // Discrete paste/fill operations and changed brushes each own an undo step.
  if (stroke_active_ &&
      (finalize || brush.width != brush_.width ||
       brush.height != brush_.height || brush.tile_ids != brush_.tile_ids ||
       width != pattern_width_ || height != pattern_height_))
    FinishStroke(history);
  if (!stroke_active_) {
    RETURN_IF_ERROR(document_->BeginStroke());
    stroke_active_ = true;
    brush_ = brush;
    pattern_width_ = width;
    pattern_height_ = height;
    last_cell_.reset();
  }
  status_ = absl::OkStatus();
  PaintLine(x, y);
  if (!status_.ok()) {
    document_->CancelStroke();
    stroke_active_ = false;
    last_cell_.reset();
    return status_;
  }
  if (finalize)
    FinishStroke(history);
  return status_;
}

void LyraTerrainCanvas::PaintStamp(int x, int y) {
  for (int dy = 0; dy < pattern_height_ && y + dy < Document::kHeight; ++dy) {
    for (int dx = 0; dx < pattern_width_ && x + dx < Document::kWidth; ++dx) {
      status_ = document_->Paint(
          x + dx, y + dy, brush_.at(dx % brush_.width, dy % brush_.height));
      if (!status_.ok())
        return;
    }
  }
}

void LyraTerrainCanvas::PaintLine(int x, int y) {
  int x0 = last_cell_ ? static_cast<int>(last_cell_->x) : x;
  int y0 = last_cell_ ? static_cast<int>(last_cell_->y) : y;
  const int dx = std::abs(x - x0), sx = x0 < x ? 1 : -1;
  const int dy = -std::abs(y - y0), sy = y0 < y ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    PaintStamp(x0, y0);
    if (!status_.ok() || (x0 == x && y0 == y))
      break;
    const int twice = 2 * error;
    if (twice >= dy) {
      error += dy;
      x0 += sx;
    }
    if (twice <= dx) {
      error += dx;
      y0 += sy;
    }
  }
  last_cell_ = ImVec2(x, y);
}

void LyraTerrainCanvas::DrawTile(ImDrawList* draw, int tile, ImVec2 min,
                                 float size) const {
  const float height = atlas_.height();
  const ImVec2 uv0((tile % 8) * 16.0f / 128.0f, (tile / 8) * 16.0f / height);
  const ImVec2 uv1(uv0.x + 16.0f / 128.0f, uv0.y + 16.0f / height);
  draw->AddImage((ImTextureID)(intptr_t)atlas_.texture(), min,
                 ImVec2(min.x + size, min.y + size), uv0, uv1);
}

void LyraTerrainCanvas::DrawSpecialWorld(const gui::CanvasRuntime& canvas) {
  if (!document_ || !canvas.draw_list || !atlas_.texture() ||
      !std::isfinite(canvas.scale) || canvas.scale <= 0) {
    return;
  }

  // The existing special maps occupy world pixels Y=0..2047. Sidecar map A0
  // begins directly below them; navigation and scrolling remain canvas-owned.
  constexpr float kWorldYOffset =
      4 * Document::kMapSize * Document::kTilePixels;
  const float cell = Document::kTilePixels * canvas.scale;
  const ImVec2 start(
      canvas.canvas_p0.x + canvas.scrolling.x,
      canvas.canvas_p0.y + canvas.scrolling.y + kWorldYOffset * canvas.scale);
  if (!std::isfinite(start.x) || !std::isfinite(start.y)) {
    return;
  }
  auto* draw = canvas.draw_list;
  const ImVec2 clip_min = draw->GetClipRectMin();
  const ImVec2 clip_max = draw->GetClipRectMax();
  // Clamp in floating point before converting: the canvas can be far outside
  // the viewport while panning, and focus loss can supply extreme coordinates.
  const auto first_cell = [cell](float clip, float origin, int count) {
    return static_cast<int>(std::clamp(std::floor((clip - origin) / cell), 0.0f,
                                       static_cast<float>(count)));
  };
  const auto last_cell = [cell](float clip, float origin, int count) {
    return static_cast<int>(std::clamp(std::ceil((clip - origin) / cell), 0.0f,
                                       static_cast<float>(count)));
  };
  const int x0 = first_cell(clip_min.x, start.x, Document::kWidth);
  const int y0 = first_cell(clip_min.y, start.y, Document::kHeight);
  const int x1 = last_cell(clip_max.x, start.x, Document::kWidth);
  const int y1 = last_cell(clip_max.y, start.y, Document::kHeight);
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      DrawTile(draw, *document_->tile(x, y),
               ImVec2(start.x + x * cell, start.y + y * cell), cell);
    }
  }
  // The shared Overworld canvas owns the grid and selected-map outline.
  // Keep project terrain free of an additional always-on map-ID overlay.
}

void LyraTerrainCanvas::Reset() {
  gfx::Arena::Get().RetireBitmap(atlas_);
  brush_ = {};
  if (document_ && stroke_active_)
    document_->CancelStroke();
  document_.reset();
  source_path_.clear();
  source_bytes_.reset();
  unpublished_ = false;
  stroke_active_ = false;
  last_cell_.reset();
  pending_project_commit_ = false;
  status_ = absl::OkStatus();
}
}  // namespace yaze::editor
