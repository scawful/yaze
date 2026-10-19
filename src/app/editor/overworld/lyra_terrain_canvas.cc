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
  if (!project || project->lyra_terrain_file.empty())
    return absl::OkStatus();
  const auto path = project->GetAbsolutePath(project->lyra_terrain_file);
  if (document_ && path == source_path_)
    return absl::OkStatus();
  if (dirty())
    return absl::FailedPreconditionError(
        "Save the current Lyra terrain draft before replacing it");
  RETURN_IF_ERROR(ValidateTarget(path, *project));
  core::LyraTerrainProfile profile;
  RETURN_IF_ERROR(BindSource(overworld, &profile, false));
  ASSIGN_OR_RETURN(auto bytes, ReadSource(path));
  if (!bytes)
    return absl::NotFoundError("The project's Lyra terrain file is missing");
  ASSIGN_OR_RETURN(auto document, Document::Parse(*bytes, profile));
  RETURN_IF_ERROR(BindSource(overworld, &profile, true));
  document_ = std::make_shared<Document>(std::move(document));
  brush_ = 0;
  picker_page_ = 0;
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
  brush_ = 0;
  picker_page_ = 0;
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

void LyraTerrainCanvas::PaintLine(int x, int y) {
  int x0 = last_cell_ ? static_cast<int>(last_cell_->x) : x;
  int y0 = last_cell_ ? static_cast<int>(last_cell_->y) : y;
  const int dx = std::abs(x - x0), sx = x0 < x ? 1 : -1;
  const int dy = -std::abs(y - y0), sy = y0 < y ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    status_ = document_->Paint(x0, y0, brush_);
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

void LyraTerrainCanvas::Draw(UndoManager& history) {
  if (!document_)
    return;
  if (stroke_active_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    FinishStroke(history);
  ImGui::Text("Lyra terrain%s", dirty() ? " *" : "");
  ImGui::SameLine();
  ImGui::TextDisabled(
      "Paint: drag | Eyedropper: right-click | Pan: middle-drag | Zoom: wheel");
  if (!status_.ok())
    ImGui::TextWrapped("%s", status_.ToString().c_str());
  ImGui::SetNextItemWidth(140);
  ImGui::SliderFloat("Zoom", &zoom_, 0.15f, 3.0f, "%.2fx");
  ImGui::SameLine();
  const bool frame_vertical_maps = ImGui::Button("A0 / A8 / B0");
  ImGui::SameLine();
  ImGui::TextDisabled(
      "8 x 4 maps | 32 x 32 tiles per map | Save Project to keep edits");
  if (!atlas_.texture()) {
    ImGui::TextUnformatted("Preparing native Tile16 graphics...");
    return;
  }

  ImGui::BeginChild("LyraTilePicker", ImVec2(205, 0), true);
  const int count = document_->profile().tile_count;
  const int pages = (count + 127) / 128;
  ImGui::SetNextItemWidth(170);
  ImGui::SliderInt("Page", &picker_page_, 0, pages - 1);
  ImGui::Text("Tile $%04X", brush_);
  for (int i = picker_page_ * 128;
       i < std::min(count, (picker_page_ + 1) * 128); ++i) {
    ImGui::PushID(i);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("tile", ImVec2(22, 22))) {
      FinishStroke(history);
      brush_ = i;
    }
    DrawTile(ImGui::GetWindowDrawList(), i, pos, 22);
    if (brush_ == i)
      ImGui::GetWindowDrawList()->AddRect(pos, ImVec2(pos.x + 22, pos.y + 22),
                                          IM_COL32(255, 220, 80, 255), 0, 0, 2);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Tile16 $%04X", i);
    if ((i % 8) != 7)
      ImGui::SameLine(0, 1);
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild(
      "LyraTerrainViewport", ImVec2(0, 0), true,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 size = ImGui::GetContentRegionAvail();
  if (frame_vertical_maps) {
    pan_ = ImVec2(24, 40);
    zoom_ = std::clamp(
        std::min((size.x - 48) / 512.0f, (size.y - 64) / (3 * 512.0f)), 0.15f,
        3.0f);
  }
  ImGui::InvisibleButton("terrain", size,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight |
                             ImGuiButtonFlags_MouseButtonMiddle);
  const bool hovered = ImGui::IsItemHovered();
  auto& io = ImGui::GetIO();
  if (hovered && !stroke_active_ && io.MouseWheel != 0) {
    const float old = zoom_;
    zoom_ = std::clamp(zoom_ * std::pow(1.2f, io.MouseWheel), 0.15f, 3.0f);
    const ImVec2 local(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    pan_ = ImVec2(local.x - (local.x - pan_.x) * zoom_ / old,
                  local.y - (local.y - pan_.y) * zoom_ / old);
  }
  if (!stroke_active_ && ImGui::IsItemActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    pan_.x += io.MouseDelta.x;
    pan_.y += io.MouseDelta.y;
  }
  const float cell = 16 * zoom_;
  const ImVec2 start(origin.x + pan_.x, origin.y + pan_.y);
  auto* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                     true);
  const int x0 = std::clamp(static_cast<int>(std::floor(-pan_.x / cell)), 0,
                            Document::kWidth);
  const int y0 = std::clamp(static_cast<int>(std::floor(-pan_.y / cell)), 0,
                            Document::kHeight);
  const int x1 =
      std::clamp(static_cast<int>(std::ceil((size.x - pan_.x) / cell)), 0,
                 Document::kWidth);
  const int y1 =
      std::clamp(static_cast<int>(std::ceil((size.y - pan_.y) / cell)), 0,
                 Document::kHeight);
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      DrawTile(draw, *document_->tile(x, y),
               ImVec2(start.x + x * cell, start.y + y * cell), cell);
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 8; ++col) {
      const ImVec2 a(start.x + col * 32 * cell, start.y + row * 32 * cell);
      const ImVec2 b(a.x + 32 * cell, a.y + 32 * cell);
      draw->AddRect(a, b, IM_COL32(255, 255, 255, 180));
      const auto label = absl::StrFormat("$%02X", 0xA0 + row * 8 + col);
      draw->AddRectFilled(a, ImVec2(a.x + 39, a.y + 20),
                          IM_COL32(0, 0, 0, 190));
      draw->AddText(ImVec2(a.x + 4, a.y + 2), IM_COL32_WHITE, label.c_str());
    }
  const int x = static_cast<int>(std::floor((io.MousePos.x - start.x) / cell));
  const int y = static_cast<int>(std::floor((io.MousePos.y - start.y) / cell));
  const bool inside =
      x >= 0 && x < Document::kWidth && y >= 0 && y < Document::kHeight;
  if (hovered && inside) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      FinishStroke(history);
      brush_ = *document_->tile(x, y);
      picker_page_ = brush_ / 128;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      status_ = document_->BeginStroke();
      stroke_active_ = status_.ok();
    }
    if (stroke_active_ && ImGui::IsMouseDown(ImGuiMouseButton_Left))
      PaintLine(x, y);
    const ImVec2 a(start.x + x * cell, start.y + y * cell);
    draw->AddRect(a, ImVec2(a.x + cell, a.y + cell),
                  IM_COL32(255, 220, 80, 255), 0, 0, 2);
  } else if (stroke_active_)
    last_cell_.reset();
  draw->PopClipRect();
  ImGui::EndChild();
}

void LyraTerrainCanvas::Reset() {
  gfx::Arena::Get().RetireBitmap(atlas_);
  brush_ = 0;
  picker_page_ = 0;
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
