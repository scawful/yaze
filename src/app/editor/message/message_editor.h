#ifndef YAZE_APP_EDITOR_MESSAGE_EDITOR_H
#define YAZE_APP_EDITOR_MESSAGE_EDITOR_H

#include <array>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/message/message_data.h"
#include "app/editor/message/panels/message_editor_panels.h"
#include "app/editor/message/message_preview.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/style.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

constexpr int kGfxFont = 0x70000;  // 2bpp format
constexpr int kCharactersWidth = 0x74ADF;
constexpr int kNumMessages = 396;
constexpr int kFontGfxMessageSize = 128;
constexpr int kFontGfxMessageDepth =
    8;  // Fixed: Must be 8 for indexed palette mode
constexpr int kFontGfx16Size = 172 * 4096;

constexpr uint8_t kBlockTerminator = 0x80;
constexpr uint8_t kMessageBankChangeId = 0x80;

class MessageEditor : public Editor {
 public:
  explicit MessageEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kMessage;
  }

  explicit MessageEditor(Rom* rom, const EditorDependencies& deps)
      : MessageEditor(rom) {
    dependencies_ = deps;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  void SetGameData(zelda3::GameData* game_data) override;

  void DrawMessageList();
  void DrawCurrentMessage();
  void DrawFontAtlas();
  void DrawTextCommands();
  void DrawSpecialCharacters();
  void DrawExpandedMessageSettings();
  void DrawDictionary();
  void DrawMessagePreview();
  void UpdateCurrentMessageFromText(const std::string& text);

  absl::Status Save() override;
  absl::Status SaveExpandedMessages();

  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override;
  void Delete();
  void SelectAll();

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  bool case_sensitive_ = false;
  bool match_whole_word_ = false;
  std::string search_text_ = "";

  std::array<uint8_t, 0x4000> raw_font_gfx_data_;
  std::vector<std::string> parsed_messages_;
  std::vector<MessageData> list_of_texts_;
  std::vector<MessageData> expanded_messages_;

  MessageData current_message_;
  MessagePreview message_preview_;

  gfx::Bitmap font_gfx_bitmap_;
  gfx::Bitmap current_font_gfx16_bitmap_;
  gfx::SnesPalette font_preview_colors_;

  gui::Canvas font_gfx_canvas_{"##FontGfxCanvas", ImVec2(256, 256)};
  gui::Canvas current_font_gfx16_canvas_{"##CurrentMessageGfx",
                                         ImVec2(172 * 2, 4096)};
  gui::TextBox message_text_box_;
  Rom* rom_;
  Rom expanded_message_bin_;
  std::string expanded_message_path_;
  int expanded_message_base_id_ = 0;
  int current_message_index_ = 0;
  bool current_message_is_expanded_ = false;

  // Panel visibility states
  bool show_message_list_ = false;
  bool show_message_editor_ = false;
  bool show_font_atlas_ = false;
  bool show_dictionary_ = false;

  void ResolveFontPalette();
  gfx::SnesPalette BuildFallbackFontPalette() const;
  void LoadFontGraphics();
  void RefreshFontAtlasBitmap(const std::vector<uint8_t>& font_data);
  void ApplyFontPalette();
  void EnsureFontTexturesReady();
  void ImportMessageBundleFromFile(const std::string& path);

  bool font_graphics_loaded_ = false;
  std::string message_bundle_status_;
  bool message_bundle_status_error_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_EDITOR_H
