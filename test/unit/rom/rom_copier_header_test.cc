// Copier (SMC/SWC) header detection at load and preservation on save.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "unique_temp_path.h"

namespace yaze {
namespace {

constexpr size_t kLoRomHeader = 0x7FC0;
constexpr size_t kHiRomHeader = 0xFFC0;

// Headerless image with a valid internal header at `header_offset`. The
// filler pattern never forms a valid checksum/complement pair elsewhere.
std::vector<uint8_t> MakeImage(size_t size, size_t header_offset = kLoRomHeader,
                               uint8_t map_mode = 0x20) {
  std::vector<uint8_t> image(size);
  for (size_t i = 0; i < size; ++i) {
    image[i] = static_cast<uint8_t>(i * 7 + 3);
  }
  const char kTitle[] = "THE LEGEND OF ZELDA  ";
  std::memcpy(&image[header_offset], kTitle, 21);
  image[header_offset + 0x15] = map_mode;
  image[header_offset + 0x1C] = 0x0D;  // complement 0xAF0D
  image[header_offset + 0x1D] = 0xAF;
  image[header_offset + 0x1E] = 0xF2;  // checksum 0x50F2
  image[header_offset + 0x1F] = 0x50;
  return image;
}

std::vector<uint8_t> MakeCopierHeader() {
  std::vector<uint8_t> header(kCopierHeaderSize, 0x00);
  header[0] = 0x80;  // size in 8 KB blocks, as copiers write it
  header[8] = 0xAA;
  header[9] = 0xBB;
  header[10] = 0x04;
  header[kCopierHeaderSize - 1] = 0x5A;
  return header;
}

std::vector<uint8_t> WithHeader(const std::vector<uint8_t>& image) {
  std::vector<uint8_t> file = MakeCopierHeader();
  file.insert(file.end(), image.begin(), image.end());
  return file;
}

CopierHeaderReason Detect(const std::vector<uint8_t>& bytes) {
  return DetectCopierHeader(bytes.data(), bytes.size());
}

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteBytes(const std::filesystem::path& path,
                const std::vector<uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

class ScopedDir {
 public:
  ScopedDir() : path_(test::UniqueTempPath("rom_copier_header")) {
    std::filesystem::create_directories(path_);
  }
  ~ScopedDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

TEST(CopierHeaderDetectTest, AlignedImagesHaveNoHeader) {
  EXPECT_EQ(Detect(MakeImage(0x100000)), CopierHeaderReason::kNone);
  EXPECT_EQ(Detect(MakeImage(0x200000)), CopierHeaderReason::kNone);
  EXPECT_EQ(Detect(std::vector<uint8_t>(0x200000, 0)),
            CopierHeaderReason::kNone);
}

TEST(CopierHeaderDetectTest, SizeAlignmentDetectsAnyBankMultiple) {
  // 1 MB + 0x200 was the only size recognized before; any 32 KB multiple
  // plus 0x200 is a copier layout.
  EXPECT_EQ(Detect(WithHeader(MakeImage(0x100000))),
            CopierHeaderReason::kSizeAlignment);
  EXPECT_EQ(Detect(WithHeader(MakeImage(0x180000))),
            CopierHeaderReason::kSizeAlignment);
  EXPECT_EQ(Detect(WithHeader(MakeImage(0x20000))),
            CopierHeaderReason::kSizeAlignment);
  // No internal header anywhere: the size alone still decides.
  EXPECT_EQ(Detect(std::vector<uint8_t>(0x100200, 0)),
            CopierHeaderReason::kSizeAlignment);
}

TEST(CopierHeaderDetectTest, SizeAlignmentKeepsImageWhoseHeaderIsInPlace) {
  // A headerless image followed by 0x200 bytes of trailing data.
  EXPECT_EQ(Detect(MakeImage(0x100200)), CopierHeaderReason::kNone);
}

TEST(CopierHeaderDetectTest, NonstandardSizeNeedsShiftedInternalHeader) {
  // LYRA-v038b.smc: 0x200 header + 0x10063B image = 0x10083B bytes.
  const auto lyra_like = WithHeader(MakeImage(0x10063B));
  ASSERT_EQ(lyra_like.size(), 0x10083Bu);
  EXPECT_EQ(Detect(lyra_like), CopierHeaderReason::kInternalHeaderOffset);

  // HiROM layouts are checked too.
  EXPECT_EQ(Detect(WithHeader(MakeImage(0x10063B, kHiRomHeader, 0x21))),
            CopierHeaderReason::kInternalHeaderOffset);

  // Oracle of Secrets builds are headerless with trailing data (0x20D13A).
  EXPECT_EQ(Detect(MakeImage(0x20D13A)), CopierHeaderReason::kNone);
  // No internal header at all: leave the file alone.
  EXPECT_EQ(Detect(std::vector<uint8_t>(0x10083B, 0)),
            CopierHeaderReason::kNone);
}

TEST(CopierHeaderDetectTest, InvalidInternalHeadersAreIgnored) {
  auto bad_pair = MakeImage(0x10063B);
  bad_pair[kLoRomHeader + 0x1F] ^= 0x01;
  EXPECT_EQ(Detect(WithHeader(bad_pair)), CopierHeaderReason::kNone);

  auto bad_map_mode = MakeImage(0x10063B, kLoRomHeader, 0x00);
  EXPECT_EQ(Detect(WithHeader(bad_map_mode)), CopierHeaderReason::kNone);

  EXPECT_EQ(DetectCopierHeader(nullptr, 0x100200), CopierHeaderReason::kNone);
  EXPECT_EQ(Detect(std::vector<uint8_t>(0x200, 0)), CopierHeaderReason::kNone);
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

TEST(RomCopierHeaderTest, LoadStripsHeaderAndKeepsTrailingData) {
  const auto image = MakeImage(0x10063B);
  const auto file = WithHeader(image);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(file).ok());
  EXPECT_TRUE(rom.has_copier_header());
  EXPECT_EQ(rom.copier_header_reason(),
            CopierHeaderReason::kInternalHeaderOffset);
  EXPECT_EQ(rom.copier_header(), MakeCopierHeader());
  EXPECT_EQ(rom.size(), image.size());
  EXPECT_EQ(rom.vector(), image);
  EXPECT_EQ(rom.title(), "THE LEGEND OF ZELDA");
}

TEST(RomCopierHeaderTest, ReloadingStrippedDataDoesNotStripAgain) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(WithHeader(MakeImage(0x10063B))).ok());
  Rom copy;
  ASSERT_TRUE(copy.LoadFromData(rom.vector()).ok());
  EXPECT_FALSE(copy.has_copier_header());
  EXPECT_EQ(copy.vector(), rom.vector());
}

TEST(RomCopierHeaderTest, StripHeaderFalseKeepsFileBytes) {
  const auto file = WithHeader(MakeImage(0x100000));
  Rom::LoadOptions options;
  options.strip_header = false;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(file, options).ok());
  EXPECT_FALSE(rom.has_copier_header());
  EXPECT_EQ(rom.vector(), file);
}

TEST(RomCopierHeaderTest, CopiesKeepHeaderAndCloseClearsIt) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(WithHeader(MakeImage(0x100000))).ok());
  ASSERT_TRUE(rom.has_copier_header());
  Rom copied(rom);
  EXPECT_EQ(copied.copier_header(), rom.copier_header());
  Rom assigned;
  assigned = rom;
  EXPECT_EQ(assigned.copier_header_reason(),
            CopierHeaderReason::kSizeAlignment);
  rom.Close();
  EXPECT_FALSE(rom.has_copier_header());
  EXPECT_EQ(rom.copier_header_reason(), CopierHeaderReason::kNone);
  // A later headerless load replaces the previous header state.
  ASSERT_TRUE(copied.LoadFromData(MakeImage(0x100000)).ok());
  EXPECT_FALSE(copied.has_copier_header());
}

// ---------------------------------------------------------------------------
// Save: the header is written back, never silently dropped.
// ---------------------------------------------------------------------------

TEST(RomCopierHeaderTest, InPlaceSaveOfUneditedRomIsByteIdentical) {
  ScopedDir dir;
  const auto path = dir.path() / "lyra_like.smc";
  const auto file = WithHeader(MakeImage(0x10063B));
  WriteBytes(path, file);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path.string()).ok());
  ASSERT_TRUE(rom.has_copier_header());
  ASSERT_TRUE(rom.SaveToFile({}).ok());
  EXPECT_EQ(ReadBytes(path), file);
}

TEST(RomCopierHeaderTest, InPlaceSaveKeepsHeaderAndFileOffsets) {
  ScopedDir dir;
  const auto path = dir.path() / "work.smc";
  auto file = WithHeader(MakeImage(0x100000));
  WriteBytes(path, file);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path.string()).ok());
  ASSERT_TRUE(rom.WriteByte(0x1234, 0xEE).ok());
  ASSERT_TRUE(rom.SaveToFile({}).ok());

  file[kCopierHeaderSize + 0x1234] = 0xEE;
  EXPECT_EQ(ReadBytes(path), file);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
  EXPECT_TRUE(reopened.has_copier_header());
  EXPECT_EQ(reopened.vector(), rom.vector());
}

TEST(RomCopierHeaderTest, SaveAsKeepsTheSourceLayout) {
  ScopedDir dir;
  const auto source = dir.path() / "source.smc";
  const auto target = dir.path() / "copy.sfc";
  const auto file = WithHeader(MakeImage(0x10063B));
  WriteBytes(source, file);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(source.string()).ok());
  Rom::SaveSettings settings;
  settings.filename = target.string();
  ASSERT_TRUE(rom.SaveToFile(settings).ok());
  EXPECT_EQ(ReadBytes(target), file);
  EXPECT_EQ(ReadBytes(source), file);
}

TEST(RomCopierHeaderTest, HeaderlessSaveIsUnchanged) {
  ScopedDir dir;
  const auto path = dir.path() / "plain.sfc";
  const auto image = MakeImage(0x100000);
  WriteBytes(path, image);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path.string()).ok());
  EXPECT_FALSE(rom.has_copier_header());
  ASSERT_TRUE(rom.SaveToFile({}).ok());
  EXPECT_EQ(ReadBytes(path), image);
}

// ---------------------------------------------------------------------------
// Real headered ROM: set YAZE_TEST_ROM_HEADERED to a copier-headered dump
// (for example a COPY of LYRA-v038b.smc). The test works on its own copy.
// ---------------------------------------------------------------------------

TEST(RomCopierHeaderTest, RealHeaderedRomRoundTripsByteForByte) {
  const char* env = std::getenv("YAZE_TEST_ROM_HEADERED");
  if (env == nullptr || !std::filesystem::exists(env)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_HEADERED to a copier-headered ROM";
  }
  ScopedDir dir;
  const auto path = dir.path() / "headered.smc";
  std::filesystem::copy_file(env, path);
  const auto original = ReadBytes(path);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path.string()).ok());
  ASSERT_TRUE(rom.has_copier_header());
  EXPECT_EQ(rom.size() + kCopierHeaderSize, original.size());
  EXPECT_FALSE(rom.title().empty());
  ASSERT_TRUE(rom.SaveToFile({}).ok());
  EXPECT_EQ(ReadBytes(path), original);
}

}  // namespace
}  // namespace yaze
