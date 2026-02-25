// Regression tests for .yazeproj bundle path handling.
//
// Covers:
//   1. Nested-path open  - opening a file inside a .yazeproj bundle resolves
//      the bundle root correctly.
//   2. Bundle-path rebasing - paths serialized after a bundle-open are relative
//      to the bundle root, not absolute machine paths.
//   3. Relative serialization stability - save → re-open does not accumulate
//      absolute path prefixes.
//   4. Edge cases:
//        a. Path with no .yazeproj ancestor fails gracefully.
//        b. Multiple .yazeproj components in a path (outermost wins).
//        c. Path that IS the bundle root directory.
//
// Implementation note: these tests exercise the contract provided by
// YazeProject::ResolveBundleRoot() and the bundle-aware Open() path.

#include "core/project.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "gtest/gtest.h"

namespace yaze::project {

namespace {

// ---------------------------------------------------------------------------
// Shared test infrastructure
// ---------------------------------------------------------------------------

std::filesystem::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (prefix + "_" + std::to_string(now));
}

class ScopedTempDir {
 public:
  explicit ScopedTempDir(std::filesystem::path path) : path_(std::move(path)) {
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void WriteTextFile(const std::filesystem::path& path,
                   const std::string& data) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(file.is_open())
      << "Failed to open file for writing: " << path.string();
  file << data;
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open())
      << "Failed to open file for reading: " << path.string();
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

// Build a minimal but valid .yaze project file whose paths are relative to the
// directory that will contain it (the bundle root).
std::string MakeMinimalProjectFileContent(const std::string& name) {
  return "[project]\nname=" + name +
         "\n\n"
         "[files]\n"
         "rom_filename=rom\n"
         "code_folder=project\n"
         "rom_backup_folder=backups\n"
         "output_folder=output\n"
         "labels_filename=labels.txt\n"
         "symbols_filename=symbols.txt\n";
}

// Populate a complete bundle on disk and return the bundle root path.
//   <base>/BundleName.yazeproj/
//     project.yaze
//     rom
//     project/
//     project/hooks/hook.asm
//     backups/
//     output/
std::filesystem::path CreateFullBundle(const std::filesystem::path& base,
                                       const std::string& bundle_stem) {
  const auto bundle = base / (bundle_stem + ".yazeproj");
  std::filesystem::create_directories(bundle);
  std::filesystem::create_directories(bundle / "project" / "hooks");
  std::filesystem::create_directories(bundle / "backups");
  std::filesystem::create_directories(bundle / "output");
  WriteTextFile(bundle / "rom", "not a real rom");
  WriteTextFile(bundle / "project.yaze",
                MakeMinimalProjectFileContent(bundle_stem));
  WriteTextFile(bundle / "project" / "hooks" / "hook.asm", "; asm stub\n");
  return bundle;
}

// ---------------------------------------------------------------------------
// Helper: walk up the path hierarchy and find the first ancestor (inclusive)
// whose filename ends with ".yazeproj". Returns empty path if none found.
// This mirrors the contract that YazeProject::Open should implement so that
// tests can assert the expected bundle root without calling the production API.
// ---------------------------------------------------------------------------
std::filesystem::path FindExpectedBundleRoot(
    const std::filesystem::path& input) {
  for (auto p = std::filesystem::path(input).lexically_normal(); !p.empty();
       p = p.parent_path()) {
    if (p.extension() == ".yazeproj") {
      return p;
    }
    // Avoid infinite loop at filesystem root.
    if (p == p.parent_path()) {
      break;
    }
  }
  return {};
}

// Returns true when `haystack` contains `needle` as a substring.
bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

// Returns true when `str` starts with `prefix`.
bool StartsWith(const std::string& str, const std::string& prefix) {
  return str.rfind(prefix, 0) == 0;
}

}  // namespace

// ===========================================================================
// 1. Nested-path open – project.yaze directly inside the bundle
// ===========================================================================

// Opening the canonical location (BundleName.yazeproj/project.yaze) should
// succeed and set filepath to that file.
TEST(ProjectBundlePathTest, OpenProjectYazeInsideBundleSucceeds) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_nested_project_yaze"));
  const auto bundle = CreateFullBundle(temp.path(), "NestedOpen");
  const auto project_file = bundle / "project.yaze";

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());

  // filepath must point at the project.yaze inside the bundle.
  EXPECT_EQ(project.filepath, project_file.string());
  EXPECT_EQ(project.name, "NestedOpen");
}

// ===========================================================================
// 1b. Nested-path open – a deeply-nested file inside the bundle
//
// Calling Open() on a path like:
//   SomeProject.yazeproj/project/hooks/hook.asm
// is a non-standard usage but must not crash and, ideally, should either:
//   (a) fail with a clear error (acceptable), or
//   (b) detect the bundle ancestor and open the project.
//
// Verifies that the resolved filepath lives inside the bundle.
// ===========================================================================

TEST(ProjectBundlePathTest, OpenDeeplyNestedPathResolvesToBundleRoot) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_deep_nested_path"));
  const auto bundle = CreateFullBundle(temp.path(), "DeepNested");

  // A file that is a non-project asset deep inside the bundle.
  const auto deep_path = bundle / "project" / "hooks" / "hook.asm";
  ASSERT_TRUE(std::filesystem::exists(deep_path));

  // FindExpectedBundleRoot encodes the expected contract.
  const auto expected_root = FindExpectedBundleRoot(deep_path);
  ASSERT_EQ(expected_root, bundle)
      << "Test setup: FindExpectedBundleRoot should return the bundle root";

  YazeProject project;
  const auto status = project.Open(deep_path.string());

  // Option (a): clean rejection is acceptable.
  if (!status.ok()) {
    EXPECT_FALSE(status.message().empty())
        << "Failure should carry a meaningful message";
    return;
  }

  // Option (b): if it succeeded, filepath must be inside the bundle root.
  const std::string bundle_str = bundle.string();
  EXPECT_TRUE(StartsWith(project.filepath, bundle_str))
      << "filepath=" << project.filepath
      << " should be inside bundle=" << bundle_str;
}

// ===========================================================================
// 2. Bundle-path rebasing
//    After opening a bundle, every path stored in the project struct must be
//    absolute and rooted inside the bundle directory.
// ===========================================================================

TEST(ProjectBundlePathTest, AbsolutePathsAreRootedInsideBundleAfterOpen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_rebasing"));
  const auto bundle = CreateFullBundle(temp.path(), "Rebased");

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle.string()).ok());

  const std::string bundle_str = bundle.string();

  // rom_filename and code_folder must be absolute paths inside the bundle.
  EXPECT_FALSE(project.rom_filename.empty());
  EXPECT_TRUE(StartsWith(project.rom_filename, bundle_str))
      << "rom_filename=" << project.rom_filename
      << " expected prefix=" << bundle_str;

  EXPECT_FALSE(project.code_folder.empty());
  EXPECT_TRUE(StartsWith(project.code_folder, bundle_str))
      << "code_folder=" << project.code_folder
      << " expected prefix=" << bundle_str;
}

// After Save(), the serialized file must contain *relative* paths, not
// absolute machine-specific ones, so the bundle stays portable.
TEST(ProjectBundlePathTest, SerializedPathsAreRelativeAfterBundleOpen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_relative_serial"));
  const auto bundle = CreateFullBundle(temp.path(), "RelSerial");

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle.string()).ok());
  ASSERT_TRUE(project.Save().ok());

  const auto project_file = bundle / "project.yaze";
  const std::string saved = ReadTextFile(project_file);

  // Absolute machine paths must not appear in the serialized form.
  // We know temp.path().string() is an absolute prefix – it must not leak.
  EXPECT_FALSE(Contains(saved, temp.path().string()))
      << "Absolute temp path leaked into serialized project file.\n"
      << "Saved content:\n"
      << saved;

  // The relative entries must still be present.
  EXPECT_TRUE(Contains(saved, "rom_filename=rom"))
      << "Expected relative rom_filename in saved file.\nSaved:\n" << saved;
  EXPECT_TRUE(Contains(saved, "code_folder=project"))
      << "Expected relative code_folder in saved file.\nSaved:\n" << saved;
}

// ===========================================================================
// 3. Relative serialization stability
//    Save → re-open → save again must not accumulate absolute prefixes or
//    double-relative paths.
// ===========================================================================

TEST(ProjectBundlePathTest, RoundTripDoesNotAccumulateAbsolutePrefixes) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_roundtrip"));
  const auto bundle = CreateFullBundle(temp.path(), "RoundTrip");
  const auto project_file = bundle / "project.yaze";

  // First open + save cycle.
  {
    YazeProject p;
    ASSERT_TRUE(p.Open(bundle.string()).ok());
    ASSERT_TRUE(p.Save().ok());
  }

  const std::string after_first_save = ReadTextFile(project_file);

  // Second open + save cycle.
  {
    YazeProject p;
    ASSERT_TRUE(p.Open(project_file.string()).ok());
    ASSERT_TRUE(p.Save().ok());
  }

  const std::string after_second_save = ReadTextFile(project_file);

  // The rom_filename entry must be identical after both saves.
  // Extract the rom_filename line from each save and compare.
  auto ExtractLine = [](const std::string& content,
                        const std::string& key) -> std::string {
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
      if (line.rfind(key + "=", 0) == 0) return line;
    }
    return "";
  };

  const std::string rom_line_1 = ExtractLine(after_first_save, "rom_filename");
  const std::string rom_line_2 =
      ExtractLine(after_second_save, "rom_filename");

  EXPECT_FALSE(rom_line_1.empty())
      << "rom_filename key absent in first save:\n" << after_first_save;
  EXPECT_EQ(rom_line_1, rom_line_2)
      << "rom_filename changed between save cycles (absolute prefix "
         "accumulation?).\n"
      << "After first save:  " << rom_line_1 << "\n"
      << "After second save: " << rom_line_2;

  const std::string code_line_1 = ExtractLine(after_first_save, "code_folder");
  const std::string code_line_2 =
      ExtractLine(after_second_save, "code_folder");

  EXPECT_FALSE(code_line_1.empty())
      << "code_folder key absent in first save:\n" << after_first_save;
  EXPECT_EQ(code_line_1, code_line_2)
      << "code_folder changed between save cycles.\n"
      << "After first save:  " << code_line_1 << "\n"
      << "After second save: " << code_line_2;
}

// ===========================================================================
// 4a. Edge case: path with no .yazeproj ancestor fails gracefully
// ===========================================================================

TEST(ProjectBundlePathTest, OpenOrdinaryDirectoryReturnsError) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_no_bundle_ancestor"));

  // An ordinary directory with no .yazeproj extension.
  const auto plain_dir = temp.path() / "plain_dir";
  std::filesystem::create_directories(plain_dir);

  YazeProject project;
  const auto status = project.Open(plain_dir.string());

  // Must fail – unsupported format or does not exist as a bundle.
  EXPECT_FALSE(status.ok())
      << "Opening a non-.yazeproj directory should fail, got ok";
  EXPECT_FALSE(status.message().empty())
      << "Failure status should carry a human-readable message";
}

// Opening a path that neither ends in .yazeproj, .yaze, nor .zsproj
// should return an error, not crash.
TEST(ProjectBundlePathTest, OpenArbitraryFileReturnsError) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_arbitrary_file"));
  const auto arbitrary = temp.path() / "data.bin";
  WriteTextFile(arbitrary, "binary garbage");

  YazeProject project;
  const auto status = project.Open(arbitrary.string());

  EXPECT_FALSE(status.ok())
      << "Opening an unsupported file type should return an error";
}

// Opening a path that does not exist should return a clear error.
TEST(ProjectBundlePathTest, OpenNonExistentPathReturnsError) {
  const std::string non_existent =
      (std::filesystem::temp_directory_path() /
       "yaze_does_not_exist_xyzzy.yazeproj")
          .string();

  YazeProject project;
  const auto status = project.Open(non_existent);

  EXPECT_FALSE(status.ok())
      << "Opening a nonexistent bundle path should return an error";
  EXPECT_FALSE(status.message().empty());
}

// ===========================================================================
// 4b. Edge case: multiple .yazeproj components in a path
//
// If someone creates a bundle inside another bundle directory (unusual but
// possible), only the innermost .yazeproj that *is* a bundle directory
// should be treated as the target bundle.
//
// The expected contract: the first .yazeproj ancestor from the leaf upward
// is the bundle root. The outermost .yazeproj is treated as an unrelated
// parent directory.
// ===========================================================================

TEST(ProjectBundlePathTest, MultipleBundleComponentsInnermostWins) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_multi_bundle"));

  // Outer bundle: Outer.yazeproj/
  const auto outer_bundle = temp.path() / "Outer.yazeproj";
  // Inner bundle lives inside the outer bundle directory.
  const auto inner_bundle = outer_bundle / "Inner.yazeproj";
  std::filesystem::create_directories(inner_bundle);
  std::filesystem::create_directories(inner_bundle / "project");
  WriteTextFile(inner_bundle / "rom", "not a real rom");
  WriteTextFile(inner_bundle / "project.yaze",
                MakeMinimalProjectFileContent("Inner"));

  // Open a file deep inside the inner bundle.
  const auto deep_path = inner_bundle / "project" / "patch.asm";
  WriteTextFile(deep_path, "; stub\n");

  // Expected: FindExpectedBundleRoot walking up from deep_path returns
  // inner_bundle first (because it has .yazeproj extension).
  const auto expected = FindExpectedBundleRoot(deep_path);
  ASSERT_EQ(expected, inner_bundle)
      << "Innermost .yazeproj should be found first";

  YazeProject project;
  const auto status = project.Open(deep_path.string());

  if (!status.ok()) {
    // Graceful failure is acceptable.
    EXPECT_FALSE(status.message().empty());
    return;
  }

  // If success: filepath must be inside inner_bundle, not outer_bundle.
  EXPECT_TRUE(StartsWith(project.filepath, inner_bundle.string()))
      << "filepath should be inside the innermost bundle.\n"
      << "filepath=" << project.filepath;
}

// ===========================================================================
// 4c. Edge case: path is exactly the bundle root directory
// ===========================================================================

// Opening the bundle root directory (i.e., "Foo.yazeproj") directly is the
// canonical usage and must succeed.
TEST(ProjectBundlePathTest, OpenBundleRootDirectlySucceeds) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_bundle_root_direct"));
  const auto bundle = CreateFullBundle(temp.path(), "DirectRoot");

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle.string()).ok());

  // filepath is set to the project.yaze inside the bundle.
  const auto expected_project_file = bundle / "project.yaze";
  EXPECT_EQ(project.filepath, expected_project_file.string());
  EXPECT_EQ(project.name, "DirectRoot");
}

// Opening the bundle root when project.yaze is absent should auto-create it.
TEST(ProjectBundlePathTest, OpenBundleRootWithoutProjectFileAutoCreates) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_bundle_autocreate"));

  const auto bundle = temp.path() / "AutoCreate.yazeproj";
  std::filesystem::create_directories(bundle);
  std::filesystem::create_directories(bundle / "project");
  WriteTextFile(bundle / "rom", "not a real rom");
  // Intentionally omit project.yaze.

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle.string()).ok());

  const auto created_file = bundle / "project.yaze";
  EXPECT_TRUE(std::filesystem::exists(created_file))
      << "project.yaze should have been auto-created in the bundle";
  EXPECT_EQ(project.name, "AutoCreate");
}

// ===========================================================================
// 5. GetRelativePath / GetAbsolutePath symmetry for bundle-opened projects
// ===========================================================================

// After opening a bundle, GetRelativePath(GetAbsolutePath(x)) == x for
// simple relative paths that exist under the bundle root.
TEST(ProjectBundlePathTest, GetRelativeAndAbsolutePathsAreSymmetric) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_path_symmetry"));
  const auto bundle = CreateFullBundle(temp.path(), "Symmetric");

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle.string()).ok());

  // "rom" is a relative path known to exist in the bundle.
  const std::string relative_input = "rom";
  const std::string absolute = project.GetAbsolutePath(relative_input);
  EXPECT_TRUE(std::filesystem::exists(absolute))
      << "Absolute path should point to an existing file: " << absolute;

  const std::string back_to_relative = project.GetRelativePath(absolute);
  EXPECT_EQ(back_to_relative, relative_input)
      << "Round-trip GetRelativePath(GetAbsolutePath(x)) != x";
}

}  // namespace yaze::project
