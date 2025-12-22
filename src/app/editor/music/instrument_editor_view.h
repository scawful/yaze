#ifndef YAZE_EDITOR_MUSIC_INSTRUMENT_EDITOR_VIEW_H
#define YAZE_EDITOR_MUSIC_INSTRUMENT_EDITOR_VIEW_H

#include <functional>
#include <vector>

#include "zelda3/music/music_bank.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

/**
 * @brief Editor for SNES instruments (ADSR, Gain, Samples).
 */
class InstrumentEditorView {
 public:
  InstrumentEditorView() = default;
  ~InstrumentEditorView() = default;

  /**
   * @brief Draw the instrument editor.
   * @param bank The music bank containing instruments.
   */
  void Draw(MusicBank& bank);

  /**
   * @brief Set callback for when edits occur (to trigger undo save).
   */
  void SetOnEditCallback(std::function<void()> callback) { on_edit_ = callback; }

  /**
   * @brief Set callback for instrument preview.
   */
  void SetOnPreviewCallback(std::function<void(int)> callback) {
    on_preview_ = callback;
  }

 private:
  // UI Helper methods
  void DrawInstrumentList(MusicBank& bank);
  void DrawProperties(MusicInstrument& instrument, MusicBank& bank);
  void DrawAdsrGraph(const MusicInstrument& instrument);

  // State
  int selected_instrument_index_ = 0;
  
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

#endif  // YAZE_EDITOR_MUSIC_INSTRUMENT_EDITOR_VIEW_H

