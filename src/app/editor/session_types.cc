#include "app/editor/session_types.h"

#include "app/editor/editor.h"  // For EditorDependencies, needed by ApplyDependencies
#include "app/editor/system/user_settings.h"  // For UserSettings forward decl in header

namespace yaze::editor {

EditorSet::EditorSet(Rom* rom, UserSettings* user_settings, size_t session_id)
    : session_id_(session_id),
      assembly_editor_(rom),
      dungeon_editor_(rom),
      graphics_editor_(rom),
      music_editor_(rom),
      overworld_editor_(rom),
      palette_editor_(rom),
      screen_editor_(rom),
      sprite_editor_(rom),
      settings_editor_(rom, user_settings),
      message_editor_(rom),
      memory_editor_(rom) {
  active_editors_ = {&overworld_editor_, &dungeon_editor_, &graphics_editor_,
                     &palette_editor_,   &sprite_editor_,  &message_editor_,
                     &music_editor_,     &screen_editor_,  &settings_editor_,
                     &assembly_editor_};
}

void EditorSet::set_user_settings(UserSettings* settings) {
  settings_editor_.set_user_settings(settings);
}

void EditorSet::ApplyDependencies(const EditorDependencies& dependencies) {
  for (auto* editor : active_editors_) {
    editor->SetDependencies(dependencies);
  }
  memory_editor_.set_rom(dependencies.rom);
}

RomSession::RomSession(Rom&& r, UserSettings* user_settings, size_t session_id)
    : rom(std::move(r)), editors(&rom, user_settings, session_id) {
  filepath = rom.filename();
  feature_flags = core::FeatureFlags::Flags{};
}

std::string RomSession::GetDisplayName() const {
  if (!custom_name.empty()) {
    return custom_name;
  }
  return rom.title().empty() ? "Untitled Session" : rom.title();
}

}  // namespace yaze::editor
