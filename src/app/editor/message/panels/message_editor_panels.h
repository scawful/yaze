#ifndef YAZE_APP_EDITOR_MESSAGE_PANELS_MESSAGE_EDITOR_PANELS_H_
#define YAZE_APP_EDITOR_MESSAGE_PANELS_MESSAGE_EDITOR_PANELS_H_

#include <functional>
#include <string>

#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

// =============================================================================
// EditorPanel wrappers for MessageEditor panels
// =============================================================================

/**
 * @brief EditorPanel for Message List
 */
class MessageListPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit MessageListPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "message.message_list"; }
  std::string GetDisplayName() const override { return "Message List"; }
  std::string GetIcon() const override { return ICON_MD_LIST; }
  std::string GetEditorCategory() const override { return "Message"; }
  int GetPriority() const override { return 10; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Message Editor
 */
class MessageEditorPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit MessageEditorPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "message.message_editor"; }
  std::string GetDisplayName() const override { return "Message Editor"; }
  std::string GetIcon() const override { return ICON_MD_EDIT; }
  std::string GetEditorCategory() const override { return "Message"; }
  int GetPriority() const override { return 20; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Font Atlas
 */
class FontAtlasPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit FontAtlasPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "message.font_atlas"; }
  std::string GetDisplayName() const override { return "Font Atlas"; }
  std::string GetIcon() const override { return ICON_MD_FONT_DOWNLOAD; }
  std::string GetEditorCategory() const override { return "Message"; }
  int GetPriority() const override { return 30; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

/**
 * @brief EditorPanel for Dictionary
 */
class DictionaryPanel : public EditorPanel {
 public:
  using DrawCallback = std::function<void()>;

  explicit DictionaryPanel(DrawCallback draw_callback)
      : draw_callback_(std::move(draw_callback)) {}

  std::string GetId() const override { return "message.dictionary"; }
  std::string GetDisplayName() const override { return "Dictionary"; }
  std::string GetIcon() const override { return ICON_MD_BOOK; }
  std::string GetEditorCategory() const override { return "Message"; }
  int GetPriority() const override { return 40; }

  void Draw(bool* p_open) override {
    if (draw_callback_) {
      draw_callback_();
    }
  }

 private:
  DrawCallback draw_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_PANELS_MESSAGE_EDITOR_PANELS_H_
