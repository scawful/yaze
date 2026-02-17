#include "core/oracle_menu_registry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>
#include "absl/status/status.h"

namespace yaze::core {
namespace {

std::filesystem::path MakeTempRoot() {
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_oracle_menu_registry_test_" + std::to_string(nonce));
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  ASSERT_FALSE(ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out << text;
  ASSERT_TRUE(out.good());
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.is_open());
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

}  // namespace

TEST(OracleMenuRegistryTest, BuildRegistryParsesMenuBinsDrawAndComponents) {
  const std::filesystem::path root = MakeTempRoot();
  const std::filesystem::path menu_asm = root / "Menu" / "menu.asm";
  const std::filesystem::path bin_file =
      root / "Menu" / "tilemaps" / "menu_frame.tilemap";

  WriteTextFile(menu_asm,
                "Menu_DrawBackground:\n"
                "  RTS\n"
                "menu_frame: incbin \"tilemaps/menu_frame.tilemap\"\n"
                "Menu_ItemCursorPositions:\n"
                "  dw menu_offset(6,2)  ; bow\n"
                "  dw menu_offset(6,5)  ; boom\n"
                "  JSR Menu_DrawBackground\n");

  WriteTextFile(bin_file, std::string(16, '\xAB'));

  auto registry_or = BuildOracleMenuRegistry(root);
  ASSERT_TRUE(registry_or.ok()) << registry_or.status();
  const auto& registry = registry_or.value();

  ASSERT_EQ(registry.asm_files.size(), 1u);
  ASSERT_EQ(registry.bins.size(), 1u);
  EXPECT_EQ(registry.bins[0].label, "menu_frame");
  EXPECT_TRUE(registry.bins[0].exists);
  EXPECT_EQ(registry.bins[0].size_bytes, 16u);

  ASSERT_EQ(registry.components.size(), 2u);
  EXPECT_EQ(registry.components[0].table_label, "Menu_ItemCursorPositions");
  EXPECT_EQ(registry.components[0].index, 0);
  EXPECT_EQ(registry.components[0].row, 6);
  EXPECT_EQ(registry.components[0].col, 2);
  EXPECT_EQ(registry.components[0].note, "bow");

  bool found_draw = false;
  for (const auto& routine : registry.draw_routines) {
    if (routine.label == "Menu_DrawBackground") {
      found_draw = true;
      EXPECT_GE(routine.references, 1);
    }
  }
  EXPECT_TRUE(found_draw);

  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

TEST(OracleMenuRegistryTest, SetOffsetDryRunAndWriteBehavior) {
  const std::filesystem::path root = MakeTempRoot();
  const std::filesystem::path menu_entry = root / "Menu" / "menu.asm";
  const std::filesystem::path menu_select =
      root / "Menu" / "menu_select_item.asm";

  WriteTextFile(menu_entry, "incsrc \"menu_select_item.asm\"\n");
  WriteTextFile(menu_select,
                "Menu_ItemCursorPositions:\n"
                "  dw menu_offset(6,2)  ; bow\n"
                "  dw menu_offset(6,5)  ; boom\n");

  auto preview_or =
      SetOracleMenuComponentOffset(root, "Menu/menu_select_item.asm",
                                   "Menu_ItemCursorPositions", 1, 9, 14, false);
  ASSERT_TRUE(preview_or.ok()) << preview_or.status();
  EXPECT_TRUE(preview_or->changed);
  EXPECT_FALSE(preview_or->write_applied);
  EXPECT_EQ(preview_or->old_row, 6);
  EXPECT_EQ(preview_or->old_col, 5);
  EXPECT_EQ(preview_or->new_row, 9);
  EXPECT_EQ(preview_or->new_col, 14);

  const std::string before_write = ReadTextFile(menu_select);
  EXPECT_NE(before_write.find("dw menu_offset(6,5)"), std::string::npos);

  auto apply_or =
      SetOracleMenuComponentOffset(root, "Menu/menu_select_item.asm",
                                   "Menu_ItemCursorPositions", 1, 9, 14, true);
  ASSERT_TRUE(apply_or.ok()) << apply_or.status();
  EXPECT_TRUE(apply_or->changed);
  EXPECT_TRUE(apply_or->write_applied);

  const std::string after_write = ReadTextFile(menu_select);
  EXPECT_NE(after_write.find("dw menu_offset(9,14)"), std::string::npos);

  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

TEST(OracleMenuRegistryTest, SetOffsetRejectsAsmPathOutsideProjectRoot) {
  const std::filesystem::path root = MakeTempRoot();
  const std::filesystem::path menu_entry = root / "Menu" / "menu.asm";
  const std::filesystem::path external_root = MakeTempRoot();
  const std::filesystem::path external_file = external_root / "outside.asm";

  WriteTextFile(menu_entry, "; oracle root marker\n");
  WriteTextFile(external_file,
                "Menu_ItemCursorPositions:\n"
                "  dw menu_offset(6,2)  ; bow\n");

  auto edit_or = SetOracleMenuComponentOffset(
      root, external_file.string(), "Menu_ItemCursorPositions", 0, 1, 1, true);
  EXPECT_FALSE(edit_or.ok());
  EXPECT_EQ(edit_or.status().code(), absl::StatusCode::kPermissionDenied);

  const std::string unchanged = ReadTextFile(external_file);
  EXPECT_NE(unchanged.find("dw menu_offset(6,2)"), std::string::npos);

  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::remove_all(external_root, ec);
}

}  // namespace yaze::core
