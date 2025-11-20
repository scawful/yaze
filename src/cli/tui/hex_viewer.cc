#include "cli/tui/hex_viewer.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {

using namespace ftxui;

HexViewerComponent::HexViewerComponent(Rom* rom, std::function<void()> on_back)
    : rom_(rom), on_back_(std::move(on_back)) {}

ftxui::Component HexViewerComponent::Render() {
  if (component_) {
    return component_;
  }

  auto renderer = Renderer([this] {
    if (!rom_ || !rom_->is_loaded()) {
      return vbox({
                 text("Hex Viewer") | bold | center,
                 separator(),
                 text("No ROM loaded.") | center | color(Color::Red),
             }) |
             border;
    }

    std::vector<Element> rows;
    for (int i = 0; i < lines_to_show_; ++i) {
      int current_offset = offset_ + (i * 16);
      if (current_offset >= static_cast<int>(rom_->size())) {
        break;
      }

      Elements row;
      row.push_back(text(absl::StrFormat("0x%08X: ", current_offset)) |
                    color(Color::Yellow));

      for (int j = 0; j < 16; ++j) {
        if (current_offset + j < static_cast<int>(rom_->size())) {
          row.push_back(text(
              absl::StrFormat("%02X ", rom_->vector()[current_offset + j])));
        } else {
          row.push_back(text("   "));
        }
      }
      row.push_back(separator());
      for (int j = 0; j < 16; ++j) {
        if (current_offset + j < static_cast<int>(rom_->size())) {
          char c = rom_->vector()[current_offset + j];
          row.push_back(text(std::isprint(c) ? std::string(1, c) : "."));
        } else {
          row.push_back(text(" "));
        }
      }
      rows.push_back(hbox(row));
    }

    return vbox({text("Hex Viewer") | center | bold, separator(),
                 vbox(rows) | frame | flex, separator(),
                 hbox({
                     text(absl::StrFormat("Offset: 0x%08X", offset_)),
                     filler(),
                     text("↑↓ PgUp/PgDn: Scroll | Esc/b: Back") | dim,
                 })}) |
           border;
  });

  component_ = CatchEvent(renderer, [this](const Event& event) {
    if (!rom_ || !rom_->is_loaded())
      return false;

    bool handled = false;
    if (event == Event::ArrowUp) {
      offset_ = std::max(0, offset_ - 16);
      handled = true;
    } else if (event == Event::ArrowDown) {
      offset_ = std::min(static_cast<int>(rom_->size()) - (lines_to_show_ * 16),
                         offset_ + 16);
      handled = true;
    } else if (event == Event::PageUp) {
      offset_ = std::max(0, offset_ - (lines_to_show_ * 16));
      handled = true;
    } else if (event == Event::PageDown) {
      offset_ = std::min(static_cast<int>(rom_->size()) - (lines_to_show_ * 16),
                         offset_ + (lines_to_show_ * 16));
      handled = true;
    } else if (event == Event::Escape || event == Event::Character('b')) {
      if (on_back_)
        on_back_();
      handled = true;
    }

    return handled;
  });

  return component_;
}

}  // namespace cli
}  // namespace yaze
