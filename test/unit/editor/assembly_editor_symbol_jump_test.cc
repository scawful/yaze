#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/code/assembly_editor.h"
#include "core/project.h"
#include "testing.h"

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

struct ScopedDirCleanup {
  std::filesystem::path path;
  ~ScopedDirCleanup() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (ctx) {
      ImGui::DestroyContext(ctx);
      ctx = nullptr;
    }
  }
};

std::filesystem::path MakeTempDir(const std::string& basename) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (basename + "_" + std::to_string(nonce));
}

TEST(AssemblyEditorSymbolJumpTest, JumpToSymbolDefinitionOpensFileAndMovesCursor) {
  ScopedImGuiContext imgui;

  const std::filesystem::path code_dir = MakeTempDir("yaze_asm_jump");
  ScopedDirCleanup cleanup{code_dir};
  std::filesystem::create_directories(code_dir);

  const std::filesystem::path asm_file = code_dir / "main.asm";
  {
    std::ofstream out(asm_file, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << "; test\n";
    out << "TestSymbol:\n";
    out << "  RTS\n";
    ASSERT_TRUE(out.good());
  }

  project::YazeProject project;
  project.code_folder = code_dir.string();
  project.filepath = (code_dir / "project.yaze").string();

  EditorDependencies deps;
  deps.project = &project;

  AssemblyEditor editor(nullptr);
  editor.SetDependencies(deps);

  auto status = editor.JumpToSymbolDefinition("TestSymbol");
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(editor.active_file_path(), asm_file.string());
  EXPECT_EQ(editor.active_cursor_position().mLine, 1);
  EXPECT_EQ(editor.active_cursor_position().mColumn, 0);

  // Second jump should reuse cached location (no behavioral change).
  status = editor.JumpToSymbolDefinition("TestSymbol");
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(editor.active_file_path(), asm_file.string());
  EXPECT_EQ(editor.active_cursor_position().mLine, 1);
  EXPECT_EQ(editor.active_cursor_position().mColumn, 0);
}

TEST(AssemblyEditorSymbolJumpTest, JumpToReferenceParsesFileLineAndMovesCursor) {
  ScopedImGuiContext imgui;

  const std::filesystem::path code_dir = MakeTempDir("yaze_asm_ref");
  ScopedDirCleanup cleanup{code_dir};
  std::filesystem::create_directories(code_dir);

  const std::filesystem::path asm_file = code_dir / "main.asm";
  {
    std::ofstream out(asm_file, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << "; line 1\n";
    out << "; line 2\n";
    out << "SomeLabel:\n";
    out << "  RTS\n";
    ASSERT_TRUE(out.good());
  }

  project::YazeProject project;
  project.code_folder = code_dir.string();
  project.filepath = (code_dir / "project.yaze").string();

  EditorDependencies deps;
  deps.project = &project;

  AssemblyEditor editor(nullptr);
  editor.SetDependencies(deps);

  // Jump to the label line via file:line reference (1-based line numbers).
  auto status = editor.JumpToReference("main.asm:3");
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(editor.active_file_path(), asm_file.string());
  EXPECT_EQ(editor.active_cursor_position().mLine, 2);
  EXPECT_EQ(editor.active_cursor_position().mColumn, 0);

  // Alternate format: file#Lline.
  status = editor.JumpToReference("main.asm#L4");
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(editor.active_file_path(), asm_file.string());
  EXPECT_EQ(editor.active_cursor_position().mLine, 3);
  EXPECT_EQ(editor.active_cursor_position().mColumn, 0);
}

TEST(AssemblyEditorSymbolJumpTest, JumpToSymbolDefinitionReturnsNotFoundForUnknown) {
  ScopedImGuiContext imgui;

  const std::filesystem::path code_dir = MakeTempDir("yaze_asm_jump_nf");
  ScopedDirCleanup cleanup{code_dir};
  std::filesystem::create_directories(code_dir);

  const std::filesystem::path asm_file = code_dir / "main.asm";
  {
    std::ofstream out(asm_file, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out << "OtherLabel:\n";
    out << "  RTS\n";
    ASSERT_TRUE(out.good());
  }

  project::YazeProject project;
  project.code_folder = code_dir.string();
  project.filepath = (code_dir / "project.yaze").string();

  EditorDependencies deps;
  deps.project = &project;

  AssemblyEditor editor(nullptr);
  editor.SetDependencies(deps);

  auto status = editor.JumpToSymbolDefinition("MissingLabel");
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound) << status;

  // Repeating should still return NotFound (and should hit negative cache).
  status = editor.JumpToSymbolDefinition("MissingLabel");
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound) << status;
}

}  // namespace
}  // namespace yaze::editor
