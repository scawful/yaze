#include "app/editor/overworld/map_properties.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "rom/rom.h"

namespace yaze::editor {
namespace {

struct ScopedTempDir {
  std::filesystem::path path;
  explicit ScopedTempDir(std::filesystem::path p) : path(std::move(p)) {
    std::filesystem::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

std::filesystem::path MakeTempDir(const std::string& stem) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (stem + "_" + std::to_string(nonce));
}

void WriteRomFile(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

gui::CanvasMenuItem* FindMenuItem(gui::Canvas& canvas,
                                  const std::string& label_prefix) {
  for (auto& section : canvas.editor_menu().sections) {
    for (auto& item : section.items) {
      if (item.label.find(label_prefix) != std::string::npos) {
        return &item;
      }
    }
  }
  return nullptr;
}

TEST(MapPropertiesContextMenuTest, LockItemMutatesReferencedMapLockState) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_map_properties_lock"));
  const auto rom_path = temp_dir.path / "test.sfc";
  WriteRomFile(rom_path);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path.string()).ok());

  gui::Canvas canvas;
  canvas.Init("map_properties_test_canvas", ImVec2(512.0f, 512.0f));

  MapPropertiesSystem system(nullptr, &rom);
  bool current_map_lock = false;
  bool show_map_properties_panel = false;
  bool show_custom_bg_color_editor = false;
  bool show_overlay_editor = false;

  system.SetupCanvasContextMenu(canvas, 0x10, current_map_lock,
                                show_map_properties_panel,
                                show_custom_bg_color_editor,
                                show_overlay_editor, 1);

  auto* lock_item = FindMenuItem(canvas, "Lock to This Map");
  ASSERT_NE(lock_item, nullptr);
  ASSERT_TRUE(lock_item->callback);

  lock_item->callback();
  EXPECT_TRUE(current_map_lock);
}

TEST(MapPropertiesContextMenuTest,
     AreaConfigurationUsesPanelCallbackInsteadOfLegacyBool) {
  ScopedTempDir temp_dir(MakeTempDir("yaze_map_properties_panel"));
  const auto rom_path = temp_dir.path / "test.sfc";
  WriteRomFile(rom_path);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path.string()).ok());

  gui::Canvas canvas;
  canvas.Init("map_properties_panel_canvas", ImVec2(512.0f, 512.0f));

  MapPropertiesSystem system(nullptr, &rom);
  bool current_map_lock = false;
  bool show_map_properties_panel = false;
  bool show_custom_bg_color_editor = false;
  bool show_overlay_editor = false;
  bool panel_open_requested = false;

  system.SetupCanvasContextMenu(
      canvas, 0x10, current_map_lock, show_map_properties_panel,
      show_custom_bg_color_editor, show_overlay_editor, 1,
      [&panel_open_requested]() { panel_open_requested = true; });

  auto* properties_item = FindMenuItem(canvas, "Area Configuration");
  ASSERT_NE(properties_item, nullptr);
  ASSERT_TRUE(properties_item->callback);

  properties_item->callback();
  EXPECT_TRUE(panel_open_requested);
  EXPECT_FALSE(show_map_properties_panel);
}

}  // namespace
}  // namespace yaze::editor
