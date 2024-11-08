#include "command_manager.h"

#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

// When the player presses Space, a popup will appear fixed to the bottom of the
// ImGui window with a list of the available key commands which can be used.
void CommandManager::ShowWhichKey() {
  if (commands_.empty()) {
    InitializeDefaults();
  }

  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
    ImGui::OpenPopup("WhichKey");
  }

  ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 100),
                          ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 100),
                           ImGuiCond_Always);
  if (ImGui::BeginPopup("WhichKey")) {
    // ESC to close the popup
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      ImGui::CloseCurrentPopup();
    }

    const ImVec4 colors[] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),  // Soft Red
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),  // Soft Green
        ImVec4(0.2f, 0.2f, 0.8f, 1.0f),  // Soft Blue
        ImVec4(0.8f, 0.8f, 0.2f, 1.0f),  // Soft Yellow
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f),  // Soft Magenta
        ImVec4(0.2f, 0.8f, 0.8f, 1.0f)   // Soft Cyan
    };
    const int numColors = sizeof(colors) / sizeof(colors[0]);
    int colorIndex = 0;

    if (ImGui::BeginTable("CommandsTable", commands_.size(),
                          ImGuiTableFlags_SizingStretchProp)) {
      for (const auto& [shortcut, info] : commands_) {
        ImGui::TableNextColumn();
        ImGui::TextColored(colors[colorIndex], "%c: %s", info.mnemonic,
                           info.name.c_str());
        colorIndex = (colorIndex + 1) % numColors;
      }
      ImGui::EndTable();
    }
    ImGui::EndPopup();
  }
}

void CommandManager::InitializeDefaults() {
  commands_ = {
      {"O",
       {[]() { /* Open ROM logic */ }, 'O', "Open ROM",
        "Open a ROM file for editing"}},
      {"S",
       {[]() { /* Save ROM logic */ }, 'S', "Save ROM",
        "Save the current ROM"}},
      {"I",
       {[]() { /* Import Data logic */ }, 'I', "Import Data",
        "Import data into the ROM"}},
      {"E",
       {[]() { /* Export Data logic */ }, 'E', "Export Data",
        "Export data from the ROM"}},
      {"P",
       {[]() { /* Patch ROM logic */ }, 'P', "Patch ROM",
        "Apply a patch to the ROM"}},
      {"U", {[]() { /* Undo logic */ }, 'U', "Undo", "Undo the last action"}},
      {"R",
       {[]() { /* Redo logic */ }, 'R', "Redo", "Redo the last undone action"}},
      {"F", {[]() { /* Find logic */ }, 'F', "Find", "Find data in the ROM"}},
      {"G",
       {[]() { /* Goto logic */ }, 'G', "Goto",
        "Go to a specific address in the ROM"}},
      {"H", {[]() { /* Help logic */ }, 'H', "Help", "Show help information"}}};
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
