#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <chrono>
#include <memory>
#include <utility>

#include "core/project.h"
#include "core/rom_settings.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

std::vector<DungeonRenderEntranceCandidate>
DungeonEditorV2::BuildDungeonRenderEntranceCandidates() const {
  std::vector<DungeonRenderEntranceCandidate> candidates;
  candidates.reserve(entrances_.size());
  for (int slot = 0; slot < zelda3::kNumDungeonSpawnPoints; ++slot) {
    const auto& spawn = spawn_points_[slot];
    if (spawn.spawn_id() != slot) {
      continue;
    }
    candidates.push_back({
        .slot = slot,
        .room_id = spawn.room_id,
        .main_gfx = spawn.main_gfx,
    });
  }
  for (int slot = zelda3::kNumDungeonSpawnPoints;
       slot < static_cast<int>(entrances_.size()); ++slot) {
    const auto& entrance = entrances_[slot];
    candidates.push_back({
        .slot = slot,
        .room_id = entrance.room_,
        .main_gfx = entrance.blockset_,
    });
  }
  return candidates;
}

const zelda3::RoomCensus* DungeonEditorV2::GetDungeonRenderContextCensus()
    const {
  if (dungeon_render_context_census_pending_.valid()) {
    if (dungeon_render_context_census_pending_.wait_for(
            std::chrono::seconds(0)) != std::future_status::ready) {
      return nullptr;
    }
    auto census = dungeon_render_context_census_pending_.get();
    if (dungeon_render_context_pending_generation_ ==
        dungeon_render_context_census_generation_) {
      if (census.ok()) {
        dungeon_render_context_census_ = std::move(census).value();
        dungeon_render_context_census_error_.clear();
      } else {
        dungeon_render_context_census_.reset();
        dungeon_render_context_census_error_ =
            std::string(census.status().message());
        LOG_WARN("DungeonEditorV2", "Render-context census failed: %s",
                 dungeon_render_context_census_error_);
      }
    } else {
      dungeon_render_context_census_attempted_ = false;
    }
  }
  if (dungeon_render_context_census_.has_value()) {
    return &*dungeon_render_context_census_;
  }
  if (dungeon_render_context_census_attempted_) {
    return nullptr;
  }
  dungeon_render_context_census_attempted_ = true;
  dungeon_render_context_census_error_.clear();
  if (rom_ == nullptr || !rom_->is_loaded()) {
    dungeon_render_context_census_error_ = "ROM is not loaded";
    return nullptr;
  }
  if (dependencies_.project == nullptr ||
      !dependencies_.project->hack_manifest.loaded()) {
    return nullptr;
  }

  // Capture while this session owns the runtime context. The worker must not
  // consult another session's assets or settings after a tab switch/teardown.
  // Registry construction reads the initial feature mode on this thread.
  (void)zelda3::DrawRoutineRegistry::Get();
  auto custom_objects =
      zelda3::CustomObjectManager::Get().CaptureRenderSnapshot(
          core::FeatureFlags::get().kEnableCustomObjects);
  auto addresses = core::RomSettings::Get().address_overrides();
  auto rom_copy = std::make_shared<Rom>(*rom_);
  core::ProjectRegistry project_registry =
      dependencies_.project->hack_manifest.project_registry();
  core::HackManifest manifest = dependencies_.project->hack_manifest;
  dungeon_render_context_pending_generation_ =
      dungeon_render_context_census_generation_;
  dungeon_render_context_census_pending_ =
      std::async(std::launch::async,
                 [rom_copy, project_registry = std::move(project_registry),
                  manifest = std::move(manifest),
                  custom_objects = std::move(custom_objects),
                  addresses = std::move(addresses)]() mutable {
                   zelda3::RoomCensusOptions options;
                   options.project = &project_registry;
                   options.manifest = &manifest;
                   options.custom_objects = &custom_objects;
                   options.address_overrides = &addresses;
                   return zelda3::ComputeRoomCensus(rom_copy.get(), options);
                 });
  return nullptr;
}

void DungeonEditorV2::InvalidateDungeonRenderContextCensus() {
  dungeon_render_context_census_.reset();
  dungeon_render_context_census_attempted_ = false;
  ++dungeon_render_context_census_generation_;
  dungeon_render_context_census_error_.clear();
}

DungeonRenderContext DungeonEditorV2::ResolveDungeonRenderContextForRoom(
    int room_id) const {
  uint8_t room_header_blockset = 0xFF;
  if (room_id >= 0 && room_id < static_cast<int>(rooms_.size())) {
    if (const auto* room = rooms_.GetIfMaterialized(room_id)) {
      room_header_blockset = room->blockset();
    } else if (rom_ != nullptr && rom_->is_loaded()) {
      room_header_blockset =
          zelda3::LoadRoomHeaderFromRom(rom_, room_id).blockset();
    }
  }
  const auto candidates = BuildDungeonRenderEntranceCandidates();
  zelda3::RoomDefaultEntrance default_entrance;
  if (game_data_ != nullptr && room_id >= 0 &&
      room_id < static_cast<int>(game_data_->room_default_entrances.size())) {
    default_entrance = game_data_->room_default_entrances[room_id];
    if (default_entrance.main_blockset >=
        game_data_->main_blockset_ids.size()) {
      default_entrance = {};
    }
  }
  return ResolveDungeonRenderContext(
      room_id, current_entrance_id_, room_header_blockset,
      GetDungeonRenderContextCensus(), candidates, default_entrance);
}

}  // namespace yaze::editor
