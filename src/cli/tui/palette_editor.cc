#include "cli/tui/palette_editor.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include "cli/tui.h"
#include "app/gfx/snes_palette.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {

using namespace ftxui;

ftxui::Component PaletteEditorComponent::Render() {
  // static auto palette_groups = app_context.rom.palette_group();
  // static std::vector<gfx::PaletteGroup> ftx_palettes = {
  //     palette_groups.swords,
  //     palette_groups.shields,
  //     palette_groups.armors,
  //     palette_groups.overworld_main,
  //     palette_groups.overworld_aux,
  //     palette_groups.global_sprites,
  //     palette_groups.sprites_aux1,
  //     palette_groups.sprites_aux2,
  //     palette_groups.sprites_aux3,
  //     palette_groups.dungeon_main,
  //     palette_groups.overworld_mini_map,
  //     palette_groups.grass,
  //     palette_groups.object_3d,
  // };

  static int selected_palette_group = 0;
  static int selected_palette = 0;
  static int selected_color = 0;
  static std::string r_str, g_str, b_str;

  static std::vector<std::string> palette_group_names;
  if (palette_group_names.empty()) {
    for (size_t i = 0; i < 14; ++i) {
      palette_group_names.push_back(gfx::kPaletteCategoryNames[i].data());
    }
  }

  auto palette_group_menu = Menu(&palette_group_names, &selected_palette_group);

  // auto save_button = Button("Save", [&] {
  //     auto& color = ftx_palettes[selected_palette_group][selected_palette][selected_color];
  //     color.set_r(std::stoi(r_str));
  //     color.set_g(std::stoi(g_str));
  //     color.set_b(std::stoi(b_str));
  //     // TODO: Implement saving the modified palette to the ROM
  // });

  // auto back_button = Button("Back", [&] { 
  //   // TODO: This needs to be handled by the main TUI loop
  // });

  // auto component = Container::Vertical({
  //     palette_group_menu,
  //     save_button,
  //     back_button,
  // });

  // auto renderer = Renderer(component, [&] {
  //   auto& current_palette_group = ftx_palettes[selected_palette_group];
  //   std::vector<std::string> palette_names;
  //   for (size_t i = 0; i < current_palette_group.size(); ++i) {
  //       palette_names.push_back(absl::StrFormat("Palette %d", i));
  //   }
  //   auto palette_menu = Menu(&palette_names, &selected_palette);

    // auto& current_palette = current_palette_group[selected_palette];
    // std::vector<Elements> color_boxes;
    // for (int i = 0; i < current_palette.size(); ++i) {
    //     auto& color = current_palette[i];
    //     Element element = text("  ") | bgcolor(Color::RGB(color.rgb().x, color.rgb().y, color.rgb().z));
    //     if (i == selected_color) {
    //         element = element | border;
    //     }
    //     color_boxes.push_back(element);
    // }

  //   auto color_grid = Wrap("color_grid", color_boxes);

  //   r_str = std::to_string(current_palette[selected_color].rgb().x);
  //   g_str = std::to_string(current_palette[selected_color].rgb().y);
  //   b_str = std::to_string(current_palette[selected_color].rgb().z);

  //   auto selected_color_view = vbox({
  //       text("Selected Color") | bold,
  //       separator(),
  //       hbox({text("R: "), Input(&r_str, "")}),
  //       hbox({text("G: "), Input(&g_str, "")}),
  //       hbox({text("B: "), Input(&b_str, "")}),
  //       save_button->Render(),
  //   });

  //   return vbox({
  //       text("Palette Editor") | center | bold,
  //       separator(),
  //       hbox({
  //           palette_group_menu->Render() | frame,
  //           separator(),
  //           palette_menu->Render() | frame,
  //           separator(),
  //           color_grid | frame | flex,
  //           separator(),
  //           selected_color_view | frame,
  //       }),
  //       separator(),
  //       back_button->Render() | center,
  //   }) | border;
  // });

  // return renderer;
  return nullptr;
}

}  // namespace cli
}  // namespace yaze
