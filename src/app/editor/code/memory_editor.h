#ifndef YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H
#define YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/project.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/code/memory_editor.h"
#include "app/editor/dungeon/dungeon_editor.h"
#include "app/editor/editor.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/music/music_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/sprite/sprite_editor.h"
#include "app/emu/emulator.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_memory_editor.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::SameLine;
using ImGui::Text;

struct MemoryEditorWithDiffChecker : public SharedRom {
  void Update(bool &show_memory_editor) {
    static MemoryEditor mem_edit;
    static MemoryEditor comp_edit;
    static bool show_compare_rom = false;
    static Rom comparison_rom;
    ImGui::Begin("Hex Editor", &show_memory_editor);
    if (ImGui::Button("Compare Rom")) {
      auto file_name = core::FileDialogWrapper::ShowOpenFileDialog();
      PRINT_IF_ERROR(comparison_rom.LoadFromFile(file_name));
      show_compare_rom = true;
    }

    static uint64_t convert_address = 0;
    gui::InputHex("SNES to PC", (int *)&convert_address, 6, 200.f);
    SameLine();
    Text("%x", core::SnesToPc(convert_address));

    // mem_edit.DrawWindow("Memory Editor", (void*)&(*rom()), rom()->size());
    BEGIN_TABLE("Memory Comparison", 2, ImGuiTableFlags_Resizable);
    SETUP_COLUMN("Source")
    SETUP_COLUMN("Dest")

    NEXT_COLUMN()
    Text("%s", rom()->filename().data());
    mem_edit.DrawContents((void *)&(*rom()), rom()->size());

    NEXT_COLUMN()
    if (show_compare_rom) {
      comp_edit.SetComparisonData((void *)&(*rom()));
      ImGui::BeginGroup();
      ImGui::BeginChild("Comparison ROM");
      Text("%s", comparison_rom.filename().data());
      comp_edit.DrawContents((void *)&(comparison_rom), comparison_rom.size());
      ImGui::EndChild();
      ImGui::EndGroup();
    }
    END_TABLE()

    ImGui::End();
  }
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CODE_MEMORY_EDITOR_H
