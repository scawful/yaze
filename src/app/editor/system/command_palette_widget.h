#ifndef YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_WIDGET_H_
#define YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_WIDGET_H_

#include <algorithm>
#include <string>
#include <vector>

#include "app/editor/system/command_palette.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

// Visual popup widget for the CommandPalette backend.
// Invoke with Ctrl+P or programmatically via Toggle().
// Renders a fuzzy-search input with categorized command results.
class CommandPaletteWidget {
 public:
  explicit CommandPaletteWidget(CommandPalette* palette) : palette_(palette) {}

  void SetPalette(CommandPalette* palette) { palette_ = palette; }

  // Toggle visibility
  void Toggle() { visible_ = !visible_; }
  void Show() {
    visible_ = true;
    focus_input_ = true;
  }
  void Hide() { visible_ = false; }
  bool IsVisible() const { return visible_; }

  // Check keyboard shortcut (call every frame in the main loop)
  void CheckShortcut() {
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
      Toggle();
      focus_input_ = visible_;
    }
    // Escape closes
    if (visible_ && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      Hide();
    }
  }

  // Draw the palette popup (call every frame)
  void Draw() {
    if (!visible_ || !palette_)
      return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float width = std::min(600.0f, viewport->Size.x * 0.6f);
    const float x = viewport->Pos.x + (viewport->Size.x - width) * 0.5f;
    const float y = viewport->Pos.y + viewport->Size.y * 0.15f;

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 0), ImGuiCond_Always);
    ImGui::SetNextWindowFocus();

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!ImGui::Begin("##CommandPalettePopup", &visible_, kFlags)) {
      ImGui::End();
      return;
    }

    // Search input
    if (focus_input_) {
      ImGui::SetKeyboardFocusHere();
      focus_input_ = false;
    }
    ImGui::SetNextItemWidth(-1);
    bool input_changed = ImGui::InputTextWithHint(
        "##PaletteSearch", ICON_MD_SEARCH " Type to search commands...",
        search_buffer_, sizeof(search_buffer_));

    // Get results
    std::string query(search_buffer_);
    std::vector<CommandEntry> results;
    if (query.empty()) {
      // Show recent/frequent when no query
      results = palette_->GetRecentCommands(15);
      if (results.empty()) {
        results = palette_->GetFrequentCommands(15);
      }
      if (results.empty()) {
        // Show all if nothing yet
        results = palette_->GetAllCommands();
        if (results.size() > 20)
          results.resize(20);
      }
    } else {
      results = palette_->SearchCommands(query);
    }

    // Keyboard navigation
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
      selected_index_ =
          std::min(selected_index_ + 1, static_cast<int>(results.size()) - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
      selected_index_ = std::max(selected_index_ - 1, 0);
    }
    if (input_changed) {
      selected_index_ = 0;  // Reset on new search
    }

    // Results list
    ImGui::Separator();
    const float max_height = 400.0f;
    if (ImGui::BeginChild(
            "##PaletteResults",
            ImVec2(0, std::min(max_height,
                               static_cast<float>(results.size()) *
                                   ImGui::GetTextLineHeightWithSpacing())),
            false)) {
      for (int i = 0; i < static_cast<int>(results.size()); ++i) {
        const auto& cmd = results[i];
        bool is_selected = (i == selected_index_);

        ImGui::PushID(i);

        if (ImGui::Selectable("##cmd", is_selected,
                              ImGuiSelectableFlags_SpanAvailWidth)) {
          ExecuteCommand(cmd);
        }
        // Enter key executes selected
        if (is_selected && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
          ExecuteCommand(cmd);
        }

        ImGui::SameLine(0, 0);

        // Category badge
        ImGui::TextDisabled("[%s]", cmd.category.c_str());
        ImGui::SameLine();

        // Command name
        ImGui::TextUnformatted(cmd.name.c_str());

        // Shortcut (right-aligned)
        if (!cmd.shortcut.empty()) {
          float shortcut_width = ImGui::CalcTextSize(cmd.shortcut.c_str()).x;
          float avail = ImGui::GetContentRegionAvail().x;
          if (avail > shortcut_width + 8) {
            ImGui::SameLine(ImGui::GetWindowWidth() - shortcut_width -
                            ImGui::GetStyle().WindowPadding.x);
            ImGui::TextDisabled("%s", cmd.shortcut.c_str());
          }
        }

        ImGui::PopID();
      }
    }
    ImGui::EndChild();

    // Footer
    ImGui::Separator();
    ImGui::TextDisabled(ICON_MD_KEYBOARD " Enter to execute, Esc to close");

    ImGui::End();
  }

 private:
  void ExecuteCommand(const CommandEntry& cmd) {
    if (cmd.callback) {
      cmd.callback();
    }
    palette_->RecordUsage(cmd.name);
    Hide();
    search_buffer_[0] = '\0';
    selected_index_ = 0;
  }

  CommandPalette* palette_ = nullptr;
  bool visible_ = false;
  bool focus_input_ = false;
  char search_buffer_[256] = {};
  int selected_index_ = 0;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SYSTEM_COMMAND_PALETTE_WIDGET_H_
