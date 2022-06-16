#include "Data/rom.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Graphics/tile.h"
#include "compressions/stdnintendo.h"

// Mode 2 for Zelda3
#define std_nintendo_decompress(a, b, c, d, e) \
  std_nintendo_decompress(a, b, c, d, e, D_NINTENDO_C_MODE2)
#define std_nintendo_compress(a, b, c, d) \
  std_nintendo_compress(a, b, c, d, D_NINTENDO_C_MODE2)
#define BUILD_HEADER(command, lenght) (command << 5) + (lenght - 1)

namespace YazeTests {
namespace RomTestSuite {

std::vector<unsigned char> ArrayToVector(const unsigned char *array,
                                         unsigned int size) {
  std::vector<unsigned char> result;
  for (unsigned int i = 0; i < size + 2; i++) {
    result.push_back(array[i]);
  }
  return result;
}

class DecompressionTest : public ::testing::Test {
 private:
  void SetUpTilePreset() {}

 protected:
  void SetUp() override {}

  void TearDown() override {}

  unsigned int c_size_;
  yaze::Application::Data::ROM rom_;
  yaze::Application::Graphics::TilePreset tile_preset_;
};

TEST_F(DecompressionTest, test_valid_command_decompress) {
  unsigned int size;

  unsigned char simplecopy[4] = {BUILD_HEADER(0, 2), 42, 69, 0xFF};
  std_nintendo_decompress((char *)simplecopy, 0, 4, &size, &c_size_);
  auto check_vec = ArrayToVector(simplecopy, size);
  ASSERT_THAT(check_vec, testing::ElementsAre(testing::_, 42, 69, 0xFF))
      << "Simple Copy failed.";
  std::cout << std_nintendo_decompression_error << std::endl;
  ASSERT_EQ(2, size);

  unsigned char simpleset[4] = {BUILD_HEADER(1, 2), 42, 0xFF};
  std_nintendo_decompress((char *)simpleset, 0, 4, &size, &c_size_);
  check_vec = ArrayToVector(simpleset, size);
  ASSERT_THAT(check_vec, testing::ElementsAre(testing::_, 42, 42, 0xFF))
      << "Simple Set failed.";
  ASSERT_EQ(2, size);

  // std::vector<unsigned char> simplecmd2_i{BUILD_HEADER(2, 6), 42, 69, 0xFF};
  // std_nintendo_decompress((char*)simplecmd2_i.data(), 0, 4, &size, &c_size_);
  // ASSERT_THAT(simplecmd2_i, testing::ElementsAre(testing::_, 42, 69, 42, 69,
  // 42,
  //                                                69, testing::_))
  //     << "Simple command 2 (ABAB..) failed.";
}

TEST_F(DecompressionTest, test_compress_decompress) {
  char buffer[32];
  unsigned int compress_size;
  std::ifstream file("assets/test_tiles/testsnestilebpp3.tl", std::ios::binary);
  if (!file.is_open()) {
    std::cout << "Can't open testsnestilebpp4.tl" << std::endl;
    return;
  }

  char data[1024];
  for (unsigned int i = 0; i < c_size_; i++) {
    char byte_read_ = ' ';
    file.read(&byte_read_, sizeof(char));
    data[i] = byte_read_;
  }
  file.close();

  // read(fd, buffer, 32);
  // ///

  // char *comdata = std_nintendo_compress(buffer, 0, 32, &compress_size);
  // CuAssertDataEquals_Msg(
  //     tc, "Compressing/Uncompress testtilebpp4.tl", buffer, 32,
  //     std_nintendo_decompress(comdata, 0, 0, &compress_size, &c_size));
}

TEST_F(DecompressionTest, basic_test) {
  rom_.LoadFromFile("assets/alttp.sfc");
  tile_preset_.bpp_ = 4;
  tile_preset_.length_ = 28672;
  tile_preset_.pc_tiles_location_ = 0x80000;
  tile_preset_.SNESTilesLocation = 0x0000;
  tile_preset_.pc_palette_location_ = 0xDD326;
  tile_preset_.SNESPaletteLocation = 0x0000;
  auto tiles_ = rom_.ExtractTiles(tile_preset_);
  auto current_palette_ = rom_.ExtractPalette(tile_preset_);
}

}  // namespace RomTestSuite
}  // namespace YazeTests