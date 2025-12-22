#ifndef YAZE_TEST_UTILS_MOCK_ROM_GENERATOR_H
#define YAZE_TEST_UTILS_MOCK_ROM_GENERATOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace yaze {
namespace test {

/**
 * @struct MockRomConfig
 * @brief Configuration for generating mock ROM data
 */
struct MockRomConfig {
  size_t rom_size = 1024 * 1024;  // Default 1MB

  bool include_overworld_data = true;
  bool include_dungeon_data = true;
  bool include_graphics_data = true;
  bool include_sprite_data = true;

  // Fill with recognizable test patterns instead of zeros
  bool populate_with_test_data = true;

  // ZSCustomOverworld version (0 = disabled)
  uint8_t zscustom_version = 0;

  // Seed for deterministic random data
  uint32_t random_seed = 12345;
};

/**
 * @class MockRomGenerator
 * @brief Generates minimal valid SNES ROM data for testing
 *
 * Creates ROMs with valid SNES headers, checksums, and optional
 * game-specific data structures. Useful for unit tests that need
 * ROM data but don't require actual game assets.
 *
 * Example usage:
 * @code
 * MockRomConfig config;
 * config.rom_size = 2 * 1024 * 1024;  // 2MB
 * config.zscustom_version = 3;
 *
 * MockRomGenerator generator(config);
 * std::vector<uint8_t> rom_data = generator.Generate();
 *
 * Rom rom;
 * rom.LoadFromBytes(rom_data);
 * @endcode
 */
class MockRomGenerator {
 public:
  explicit MockRomGenerator(const MockRomConfig& config);

  // Generate ROM data according to configuration
  std::vector<uint8_t> Generate();

 private:
  void WriteSNESHeader(std::vector<uint8_t>& data);
  void WriteOverworldData(std::vector<uint8_t>& data);
  void WriteDungeonData(std::vector<uint8_t>& data);
  void WriteGraphicsData(std::vector<uint8_t>& data);
  void WriteSpriteData(std::vector<uint8_t>& data);
  void WriteChecksum(std::vector<uint8_t>& data);

  uint8_t CalculateRomSizeCode(size_t rom_size);

  MockRomConfig config_;
};

/**
 * @brief Factory functions for common ROM variants
 */

// Standard 1MB mock ROM with all data structures
std::vector<uint8_t> CreateDefaultMockRom();

// Minimal 256KB ROM with just header (fast loading for unit tests)
std::vector<uint8_t> CreateMinimalMockRom();

// 2MB ROM with ZSCustomOverworld v3 features enabled
std::vector<uint8_t> CreateZSCustomV3MockRom();

// ROM with intentionally corrupted data (for error handling tests)
std::vector<uint8_t> CreateCorruptedMockRom();

// Large 4MB ROM for stress testing
std::vector<uint8_t> CreateLargeMockRom();

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_UTILS_MOCK_ROM_GENERATOR_H
