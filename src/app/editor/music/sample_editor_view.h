#ifndef YAZE_EDITOR_MUSIC_SAMPLE_EDITOR_VIEW_H
#define YAZE_EDITOR_MUSIC_SAMPLE_EDITOR_VIEW_H

#include <functional>
#include <vector>

#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

/**
 * @brief Editor for SNES BRR samples.
 */
class SampleEditorView {
 public:
  SampleEditorView() = default;
  ~SampleEditorView() = default;

  /**
   * @brief Draw the sample editor.
   * @param bank The music bank containing samples.
   */
  void Draw(MusicBank& bank);

  /**
   * @brief Set callback for when edits occur.
   */
  void SetOnEditCallback(std::function<void()> callback) { on_edit_ = callback; }

  /**
   * @brief Set callback for sample preview.
   */
  void SetOnPreviewCallback(std::function<void(int)> callback) {
    on_preview_ = callback;
  }

 private:
  // UI Helper methods
  void DrawSampleList(MusicBank& bank);
  void DrawProperties(MusicSample& sample);
  void DrawWaveform(const MusicSample& sample);

  // State
  int selected_sample_index_ = 0;
  
  // Plot data
  std::vector<float> plot_x_;
  std::vector<float> plot_y_;

  // Callbacks
  std::function<void()> on_edit_;
  std::function<void(int)> on_preview_;
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_EDITOR_MUSIC_SAMPLE_EDITOR_VIEW_H