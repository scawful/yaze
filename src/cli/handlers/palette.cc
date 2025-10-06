#include "cli/cli.h"
#include "cli/tui/palette_editor.h"

#include "app/gfx/scad_format.h"
#include "app/gfx/snes_palette.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

Palette::Palette() {}

absl::Status Palette::Run(const std::vector<std::string>& arg_vec) {
    if (arg_vec.empty()) {
        return absl::InvalidArgumentError("Usage: palette <export|import> [options]");
    }

    const std::string& action = arg_vec[0];
    std::vector<std::string> new_args(arg_vec.begin() + 1, arg_vec.end());

    if (action == "export") {
        PaletteExport handler;
        return handler.Run(new_args);
    } else if (action == "import") {
        PaletteImport handler;
        return handler.Run(new_args);
    }

    return absl::InvalidArgumentError("Invalid action for palette command.");
}

void Palette::RunTUI(ftxui::ScreenInteractive& screen) {
    // TODO: Implement palette editor TUI
    (void)screen; // Suppress unused parameter warning
}

absl::Status PaletteExport::Run(const std::vector<std::string>& arg_vec) {
  std::string group_name;
  int palette_id = -1;
  std::string output_file;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--group") && i + 1 < arg_vec.size()) {
      group_name = arg_vec[++i];
    } else if ((arg == "--id") && i + 1 < arg_vec.size()) {
      palette_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--to") && i + 1 < arg_vec.size()) {
      output_file = arg_vec[++i];
    }
  }

  if (group_name.empty() || palette_id == -1 || output_file.empty()) {
    return absl::InvalidArgumentError("Usage: palette export --group <group> --id <id> --to <file>");
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  auto palette_group = rom_.palette_group().get_group(group_name);
  if (!palette_group) {
      return absl::NotFoundError("Palette group not found.");
  }

  auto palette = palette_group->palette(palette_id);
  if (palette.empty()) {
      return absl::NotFoundError("Palette not found.");
  }

  std::vector<SDL_Color> sdl_palette;
  for (const auto& color : palette) {
      SDL_Color sdl_color;
      sdl_color.r = color.rgb().x;
      sdl_color.g = color.rgb().y;
      sdl_color.b = color.rgb().z;
      sdl_color.a = 255;
      sdl_palette.push_back(sdl_color);
  }

  auto status = gfx::SaveCol(output_file, sdl_palette);
  if (!status.ok()) {
      return status;
  }

  std::cout << "Successfully exported palette " << palette_id << " from group " << group_name << " to " << output_file << std::endl;

  return absl::OkStatus();
}

absl::Status PaletteImport::Run(const std::vector<std::string>& arg_vec) {
  std::string group_name;
  int palette_id = -1;
  std::string input_file;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--group") && i + 1 < arg_vec.size()) {
      group_name = arg_vec[++i];
    } else if ((arg == "--id") && i + 1 < arg_vec.size()) {
      palette_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--from") && i + 1 < arg_vec.size()) {
      input_file = arg_vec[++i];
    }
  }

  if (group_name.empty() || palette_id == -1 || input_file.empty()) {
    return absl::InvalidArgumentError("Usage: palette import --group <group> --id <id> --from <file>");
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  auto sdl_palette = gfx::DecodeColFile(input_file);
  if (sdl_palette.empty()) {
      return absl::AbortedError("Failed to load palette file.");
  }

  gfx::SnesPalette snes_palette;
  for (const auto& sdl_color : sdl_palette) {
      snes_palette.AddColor(gfx::SnesColor(sdl_color.r, sdl_color.g, sdl_color.b));
  }

  auto palette_group = rom_.palette_group().get_group(group_name);
  if (!palette_group) {
      return absl::NotFoundError("Palette group not found.");
  }

  // Replace the palette at the specified index
  auto* pal = palette_group->mutable_palette(palette_id);
  *pal = snes_palette;

  // TODO: Implement saving the modified palette back to the ROM.
  auto save_status = rom_.SaveToFile({.save_new = false});
  if (!save_status.ok()) {
    return save_status;
  }

  std::cout << "Successfully imported palette " << palette_id << " to group " << group_name << " from " << input_file << std::endl;
  std::cout << "✅ ROM saved to: " << rom_.filename() << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
