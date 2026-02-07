#pragma once

#include <memory>
#include <string>

#include "framework/headless_editor_test.h"
#include "app/editor/overworld/overworld_editor.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "gtest/gtest.h"
#include "test_utils.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

class OverworldEditorTest : public HeadlessEditorTest {
 protected:
  void SetUp() override {
    HeadlessEditorTest::SetUp();

    // Load ROM
    YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "OverworldEditorTest");
    const std::string rom_path = TestRomManager::GetRomPath(RomRole::kVanilla);
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromFile(rom_path).ok())
        << "Could not load ROM from " << rom_path;

    // Load GameData
    game_data_ = std::make_unique<zelda3::GameData>(rom_.get());
    ASSERT_TRUE(zelda3::LoadGameData(*rom_, *game_data_).ok());

    // Create Dependencies
    shared_clipboard_ = std::make_unique<editor::EditorDependencies::SharedClipboard>();
    editor::EditorDependencies deps;
    deps.rom = rom_.get();
    deps.game_data = game_data_.get();
    deps.panel_manager = panel_manager_.get();
    deps.renderer = renderer_.get();
    deps.shared_clipboard = shared_clipboard_.get();

    // Create Editor
    overworld_editor_ = std::make_unique<editor::OverworldEditor>(rom_.get(), deps);
    overworld_editor_->SetGameData(game_data_.get());
    
    // Initialize and Load
    overworld_editor_->Initialize();
    ASSERT_TRUE(overworld_editor_->Load().ok());
  }

  void TearDown() override {
    overworld_editor_.reset();
    game_data_.reset();
    HeadlessEditorTest::TearDown();
  }

  std::unique_ptr<editor::OverworldEditor> overworld_editor_;
  std::unique_ptr<zelda3::GameData> game_data_;
  std::unique_ptr<editor::EditorDependencies::SharedClipboard> shared_clipboard_;
};

}  // namespace test
}  // namespace yaze
