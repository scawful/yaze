#include "core/patch/asm_patch.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "core/patch/patch_manager.h"

namespace yaze {
namespace core {
namespace {

// Helper to create a temporary patch file
class TempPatchFile {
 public:
  explicit TempPatchFile(const std::string& content) {
    path_ = std::filesystem::temp_directory_path() /
            ("test_patch_" + std::to_string(rand()) + ".asm");
    std::ofstream file(path_);
    file << content;
    file.close();
  }

  ~TempPatchFile() {
    if (std::filesystem::exists(path_)) {
      std::filesystem::remove(path_);
    }
  }

  std::string path() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

// ============================================================================
// Basic Parsing Tests
// ============================================================================

TEST(AsmPatchTest, ParseBasicMetadata) {
  const std::string content = R"(;#PATCH_NAME=Test Patch
;#PATCH_AUTHOR=Test Author
;#PATCH_VERSION=1.0
;#ENABLED=true

lorom
org $1BBDF4
NOP
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "TestFolder");

  EXPECT_TRUE(patch.is_valid());
  EXPECT_EQ(patch.name(), "Test Patch");
  EXPECT_EQ(patch.author(), "Test Author");
  EXPECT_EQ(patch.version(), "1.0");
  EXPECT_TRUE(patch.enabled());
  EXPECT_EQ(patch.folder(), "TestFolder");
}

TEST(AsmPatchTest, ParseDescription) {
  const std::string content = R"(;#PATCH_NAME=Test Patch
;#PATCH_DESCRIPTION
; This is a multi-line
; description of the patch.
;#ENDPATCH_DESCRIPTION
;#ENABLED=true

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  EXPECT_EQ(patch.description(), "This is a multi-line\ndescription of the patch.");
}

TEST(AsmPatchTest, ParseDisabledPatch) {
  const std::string content = R"(;#PATCH_NAME=Disabled Patch
;#ENABLED=false

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  EXPECT_FALSE(patch.enabled());
}

// ============================================================================
// Parameter Parsing Tests
// ============================================================================

TEST(AsmPatchTest, ParseByteParameter) {
  const std::string content = R"(;#PATCH_NAME=Byte Test
;#ENABLED=true

;#DEFINE_START
;#name=Test Byte Value
;#type=byte
;#range=$00,$FF
!TEST_BYTE = $42
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  ASSERT_EQ(patch.parameters().size(), 1u);

  const auto& param = patch.parameters()[0];
  EXPECT_EQ(param.define_name, "!TEST_BYTE");
  EXPECT_EQ(param.display_name, "Test Byte Value");
  EXPECT_EQ(param.type, PatchParameterType::kByte);
  EXPECT_EQ(param.value, 0x42);
  EXPECT_EQ(param.min_value, 0x00);
  EXPECT_EQ(param.max_value, 0xFF);
}

TEST(AsmPatchTest, ParseWordParameter) {
  const std::string content = R"(;#PATCH_NAME=Word Test
;#ENABLED=true

;#DEFINE_START
;#name=Test Word Value
;#type=word
!TEST_WORD = $1234
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  ASSERT_EQ(patch.parameters().size(), 1u);

  const auto& param = patch.parameters()[0];
  EXPECT_EQ(param.type, PatchParameterType::kWord);
  EXPECT_EQ(param.value, 0x1234);
  EXPECT_EQ(param.max_value, 0xFFFF);
}

TEST(AsmPatchTest, ParseBoolParameter) {
  const std::string content = R"(;#PATCH_NAME=Bool Test
;#ENABLED=true

;#DEFINE_START
;#name=Enable Feature
;#type=bool
;#checkedvalue=$01
;#uncheckedvalue=$00
!FEATURE_ON = $01
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  ASSERT_EQ(patch.parameters().size(), 1u);

  const auto& param = patch.parameters()[0];
  EXPECT_EQ(param.type, PatchParameterType::kBool);
  EXPECT_EQ(param.value, 0x01);
  EXPECT_EQ(param.checked_value, 0x01);
  EXPECT_EQ(param.unchecked_value, 0x00);
}

TEST(AsmPatchTest, ParseChoiceParameter) {
  const std::string content = R"(;#PATCH_NAME=Choice Test
;#ENABLED=true

;#DEFINE_START
;#name=Select Mode
;#type=choice
;#choice0=Mode A
;#choice1=Mode B
;#choice2=Mode C
!MODE_SELECT = $01
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  ASSERT_EQ(patch.parameters().size(), 1u);

  const auto& param = patch.parameters()[0];
  EXPECT_EQ(param.type, PatchParameterType::kChoice);
  EXPECT_EQ(param.value, 0x01);
  ASSERT_EQ(param.choices.size(), 3u);
  EXPECT_EQ(param.choices[0], "Mode A");
  EXPECT_EQ(param.choices[1], "Mode B");
  EXPECT_EQ(param.choices[2], "Mode C");
}

TEST(AsmPatchTest, ParseBitfieldParameter) {
  const std::string content = R"(;#PATCH_NAME=Bitfield Test
;#ENABLED=true

;#DEFINE_START
;#name=Crystal Requirements
;#type=bitfield
;#bit0=Crystal 1
;#bit1=Crystal 2
;#bit6=Crystal 7
!CRYSTAL_BITS = $43
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  ASSERT_EQ(patch.parameters().size(), 1u);

  const auto& param = patch.parameters()[0];
  EXPECT_EQ(param.type, PatchParameterType::kBitfield);
  EXPECT_EQ(param.value, 0x43);  // bits 0, 1, 6 set
  ASSERT_GE(param.choices.size(), 7u);
  EXPECT_EQ(param.choices[0], "Crystal 1");
  EXPECT_EQ(param.choices[1], "Crystal 2");
  EXPECT_EQ(param.choices[6], "Crystal 7");
}

TEST(AsmPatchTest, ParseMultipleParameters) {
  const std::string content = R"(;#PATCH_NAME=Multi Param Test
;#ENABLED=true

;#DEFINE_START
;#name=First Value
;#type=byte
!FIRST = $10

;#name=Second Value
;#type=word
!SECOND = $2000

;#name=Third Flag
;#type=bool
!THIRD = $01
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  EXPECT_EQ(patch.parameters().size(), 3u);
}

// ============================================================================
// Value Modification Tests
// ============================================================================

TEST(AsmPatchTest, SetParameterValue) {
  const std::string content = R"(;#PATCH_NAME=Value Test
;#ENABLED=true

;#DEFINE_START
;#name=Test Value
;#type=byte
!TEST_VAL = $10
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.SetParameterValue("!TEST_VAL", 0x42));
  EXPECT_EQ(patch.GetParameter("!TEST_VAL")->value, 0x42);
}

TEST(AsmPatchTest, SetParameterValueClamped) {
  const std::string content = R"(;#PATCH_NAME=Clamp Test
;#ENABLED=true

;#DEFINE_START
;#name=Test Value
;#type=byte
;#range=$10,$20
!TEST_VAL = $15
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  // Try to set out of range
  patch.SetParameterValue("!TEST_VAL", 0x50);
  EXPECT_EQ(patch.GetParameter("!TEST_VAL")->value, 0x20);  // Clamped to max

  patch.SetParameterValue("!TEST_VAL", 0x05);
  EXPECT_EQ(patch.GetParameter("!TEST_VAL")->value, 0x10);  // Clamped to min
}

TEST(AsmPatchTest, SetNonExistentParameter) {
  const std::string content = R"(;#PATCH_NAME=Test
;#ENABLED=true
lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_FALSE(patch.SetParameterValue("!NONEXISTENT", 0x42));
}

// ============================================================================
// Content Generation Tests
// ============================================================================

TEST(AsmPatchTest, GenerateContentPreservesStructure) {
  const std::string content = R"(;#PATCH_NAME=Gen Test
;#ENABLED=true

;#DEFINE_START
;#name=Test
;#type=byte
!TEST = $10
;#DEFINE_END

lorom
org $1BBDF4
NOP
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  std::string generated = patch.GenerateContent();

  // Should contain the ASM code
  EXPECT_NE(generated.find("lorom"), std::string::npos);
  EXPECT_NE(generated.find("org $1BBDF4"), std::string::npos);
  EXPECT_NE(generated.find("NOP"), std::string::npos);
}

TEST(AsmPatchTest, GenerateContentUpdatesEnabled) {
  const std::string content = R"(;#PATCH_NAME=Enabled Test
;#ENABLED=true
lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  patch.set_enabled(false);
  std::string generated = patch.GenerateContent();

  EXPECT_NE(generated.find(";#ENABLED=false"), std::string::npos);
  EXPECT_EQ(generated.find(";#ENABLED=true"), std::string::npos);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(AsmPatchTest, ParseDecimalValue) {
  const std::string content = R"(;#PATCH_NAME=Decimal Test
;#ENABLED=true

;#DEFINE_START
;#name=Decimal Value
;#type=byte
;#decimal
!DEC_VAL = 42
;#DEFINE_END

lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  EXPECT_TRUE(patch.is_valid());
  ASSERT_EQ(patch.parameters().size(), 1u);

  const auto& param = patch.parameters()[0];
  EXPECT_EQ(param.value, 42);
  EXPECT_TRUE(param.use_decimal);
}

TEST(AsmPatchTest, DefaultNameFromFilename) {
  const std::string content = R"(;#ENABLED=true
lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  // Should use filename (minus .asm extension) as default name
  EXPECT_FALSE(patch.name().empty());
}

TEST(AsmPatchTest, HandleMissingEnabledLine) {
  const std::string content = R"(;#PATCH_NAME=No Enabled Test
lorom
)";

  TempPatchFile file(content);
  AsmPatch patch(file.path(), "Test");

  // Should default to enabled and prepend the line
  EXPECT_TRUE(patch.is_valid());
  EXPECT_TRUE(patch.enabled());
}

// ============================================================================
// PatchManager Tests
// ============================================================================

class PatchManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create temp directory structure
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("test_patches_" + std::to_string(rand()));
    std::filesystem::create_directories(temp_dir_ / "Misc");
    std::filesystem::create_directories(temp_dir_ / "Sprites");

    // Create test patches
    CreatePatchFile("Misc", "TestPatch1.asm", R"(;#PATCH_NAME=Test Patch 1
;#PATCH_AUTHOR=Test
;#ENABLED=true
lorom
)");

    CreatePatchFile("Misc", "TestPatch2.asm", R"(;#PATCH_NAME=Test Patch 2
;#ENABLED=false
lorom
)");

    CreatePatchFile("Sprites", "SpritePatch.asm", R"(;#PATCH_NAME=Sprite Patch
;#ENABLED=true
lorom
)");
  }

  void TearDown() override {
    if (std::filesystem::exists(temp_dir_)) {
      std::filesystem::remove_all(temp_dir_);
    }
  }

  void CreatePatchFile(const std::string& folder, const std::string& name,
                       const std::string& content) {
    std::ofstream file(temp_dir_ / folder / name);
    file << content;
    file.close();
  }

  std::filesystem::path temp_dir_;
};

TEST_F(PatchManagerTest, LoadPatches) {
  PatchManager manager;
  auto status = manager.LoadPatches(temp_dir_.string());

  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_TRUE(manager.is_loaded());
  EXPECT_EQ(manager.patches().size(), 3u);
}

TEST_F(PatchManagerTest, GetPatchesInFolder) {
  PatchManager manager;
  manager.LoadPatches(temp_dir_.string());

  auto misc_patches = manager.GetPatchesInFolder("Misc");
  EXPECT_EQ(misc_patches.size(), 2u);

  auto sprite_patches = manager.GetPatchesInFolder("Sprites");
  EXPECT_EQ(sprite_patches.size(), 1u);
}

TEST_F(PatchManagerTest, GetPatchByName) {
  PatchManager manager;
  manager.LoadPatches(temp_dir_.string());

  auto* patch = manager.GetPatch("Misc", "TestPatch1.asm");
  ASSERT_NE(patch, nullptr);
  EXPECT_EQ(patch->name(), "Test Patch 1");
}

TEST_F(PatchManagerTest, GetEnabledPatchCount) {
  PatchManager manager;
  manager.LoadPatches(temp_dir_.string());

  // TestPatch1 and SpritePatch are enabled, TestPatch2 is disabled
  EXPECT_EQ(manager.GetEnabledPatchCount(), 2);
}

TEST_F(PatchManagerTest, GetFolders) {
  PatchManager manager;
  manager.LoadPatches(temp_dir_.string());

  const auto& folders = manager.folders();
  EXPECT_EQ(folders.size(), 2u);
  EXPECT_NE(std::find(folders.begin(), folders.end(), "Misc"), folders.end());
  EXPECT_NE(std::find(folders.begin(), folders.end(), "Sprites"), folders.end());
}

}  // namespace
}  // namespace core
}  // namespace yaze
