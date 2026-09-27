#ifndef YAZE_APP_EDITOR_CUTSCENE_UI_WINDOW_CUTSCENE_CAMERA_PANEL_H_
#define YAZE_APP_EDITOR_CUTSCENE_UI_WINDOW_CUTSCENE_CAMERA_PANEL_H_

#include <filesystem>
#include <optional>
#include <string>

#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/cutscene/cutscene_shot.h"

namespace yaze::editor {

// Frames overworld cutscene shots: a 256x224 viewport dragged over an area,
// clamped to the game's camera limits, plus Link and actor markers. Shots are
// saved to the project's [files] cutscene_shots JSON; without that key the
// window is read-only and offers Copy JSON. It never writes the ROM.
//
// When the Overworld editor is open, area sizes come from the ROM
// (ZSCustomOverworld small/large/wide/tall) and the area's screens are drawn
// behind the viewport; otherwise the vanilla layout and an outline are used.
class CutsceneCameraPanel : public WindowContent {
 public:
  std::string GetId() const override { return "overworld.cutscene_camera"; }
  std::string GetDisplayName() const override { return "Cutscene Camera"; }
  std::string GetIcon() const override { return ICON_MD_VIDEOCAM; }
  std::string GetEditorCategory() const override { return "Overworld"; }
  WindowScope GetScope() const override { return WindowScope::kGlobal; }

  void Draw(bool* p_open) override;

 private:
  friend class CutsceneCameraPanelTestPeer;

  std::optional<std::filesystem::path> ShotsPath() const;
  void Load();
  void Save();
  void DrawShotList();
  void DrawShotFields(zelda3::CutsceneShot& shot);
  void DrawCanvas(zelda3::CutsceneShot& shot);
  zelda3::AreaExtent ExtentFor(const zelda3::CutsceneShot& shot) const;
  // The open Overworld editor, for the ROM's area sizes and screen images.
  class OverworldEditor* OverworldSource() const;

  zelda3::CutsceneShotSet shots_;
  std::optional<std::string> loaded_bytes_;  // file contents at last load
  std::string loaded_path_;
  std::string status_;
  bool status_is_error_ = false;
  bool loaded_once_ = false;
  bool dirty_ = false;
  int selected_ = -1;
  int size_override_ =
      0;  // 0 ROM/vanilla, 1 512x512, 2 1024x1024, 3 wide, 4 tall
  enum class Drag { kNone, kViewport, kLink, kActor };
  Drag drag_ = Drag::kNone;
  int drag_actor_ = -1;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_CUTSCENE_UI_WINDOW_CUTSCENE_CAMERA_PANEL_H_
