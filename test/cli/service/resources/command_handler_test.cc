#include "cli/service/resources/command_handler.h"

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace yaze {
namespace cli {
namespace resources {
namespace {

class NoRomCommandHandler : public CommandHandler {
 public:
  std::string GetName() const override { return "no-rom"; }
  std::string GetUsage() const override { return "no-rom"; }
  bool RequiresRom() const override { return false; }

  Rom* last_rom() const { return last_rom_; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const ArgumentParser&,
                       OutputFormatter& formatter) override {
    last_rom_ = rom;
    formatter.AddField("ok", true);
    return absl::OkStatus();
  }

 private:
  Rom* last_rom_ = nullptr;
};

class MockRomCommandHandler : public CommandHandler {
 public:
  std::string GetName() const override { return "mock-rom"; }
  std::string GetUsage() const override { return "mock-rom"; }

  bool rom_loaded() const { return rom_loaded_; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const ArgumentParser&,
                       OutputFormatter& formatter) override {
    rom_loaded_ = (rom != nullptr && rom->is_loaded());
    formatter.AddField("rom_loaded", rom_loaded_);
    return absl::OkStatus();
  }

 private:
  bool rom_loaded_ = false;
};

TEST(CommandHandlerTest, RunWithoutRomUsesFormatter) {
  NoRomCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--format=text"}, nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(handler.last_rom(), nullptr);
  EXPECT_THAT(output, ::testing::HasSubstr("=== Result ==="));
  EXPECT_THAT(output, ::testing::HasSubstr("ok"));
}

TEST(CommandHandlerTest, RunWithMockRomLoadsRom) {
  MockRomCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--mock-rom"}, nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(handler.rom_loaded());
  EXPECT_THAT(output, ::testing::HasSubstr("rom_loaded"));
}

TEST(CommandHandlerTest, RunRejectsUnknownFormat) {
  NoRomCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--format=bogus"}, nullptr, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              ::testing::HasSubstr("Unknown format"));
}

}  // namespace
}  // namespace resources
}  // namespace cli
}  // namespace yaze
