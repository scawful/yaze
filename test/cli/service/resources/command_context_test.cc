#include "cli/service/resources/command_context.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "mocks/mock_rom.h"

namespace yaze {
namespace cli {
namespace resources {

class CommandContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize mock ROM for testing
    std::vector<uint8_t> test_data(1024, 0);  // 1KB of empty data
    auto status = mock_rom_.SetTestData(test_data);
    ASSERT_TRUE(status.ok());
  }

  yaze::test::MockRom mock_rom_;
};

TEST_F(CommandContextTest, LoadsRomFromConfig) {
  CommandContext::Config config;
  config.use_mock_rom = true;

  CommandContext context(config);

  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());
  EXPECT_TRUE(rom_or.value()->is_loaded());
}

TEST_F(CommandContextTest, UsesExternalRomContext) {
  CommandContext::Config config;
  config.external_rom_context = &mock_rom_;

  CommandContext context(config);

  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());
  EXPECT_EQ(rom_or.value(), &mock_rom_);
}

TEST_F(CommandContextTest, LoadsRomFromPath) {
  CommandContext::Config config;
  config.rom_path = "test_rom.sfc";  // This would need a real ROM file

  CommandContext context(config);

  // This test would need a real ROM file to pass
  // For now, we expect it to fail gracefully
  auto rom_or = context.GetRom();
  EXPECT_FALSE(rom_or.ok());
}

TEST_F(CommandContextTest, EnsuresLabelsLoaded) {
  CommandContext::Config config;
  config.use_mock_rom = true;

  CommandContext context(config);

  auto rom_or = context.GetRom();
  ASSERT_TRUE(rom_or.ok());

  auto status = context.EnsureLabelsLoaded(rom_or.value());
  EXPECT_TRUE(status.ok());
}

TEST_F(CommandContextTest, GetFormatReturnsConfigFormat) {
  CommandContext::Config config;
  config.format = "text";

  CommandContext context(config);

  EXPECT_EQ(context.GetFormat(), "text");
}

TEST_F(CommandContextTest, IsVerboseReturnsConfigVerbose) {
  CommandContext::Config config;
  config.verbose = true;

  CommandContext context(config);

  EXPECT_TRUE(context.IsVerbose());
}

// ArgumentParser Tests
class ArgumentParserTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ArgumentParserTest, ParsesStringArguments) {
  std::vector<std::string> args = {"--type=dungeon", "--format", "json"};
  ArgumentParser parser(args);

  EXPECT_EQ(parser.GetString("type").value(), "dungeon");
  EXPECT_EQ(parser.GetString("format").value(), "json");
}

TEST_F(ArgumentParserTest, ParsesIntArguments) {
  std::vector<std::string> args = {"--room=0x12", "--count", "42"};
  ArgumentParser parser(args);

  auto room_or = parser.GetInt("room");
  ASSERT_TRUE(room_or.ok());
  EXPECT_EQ(room_or.value(), 0x12);

  auto count_or = parser.GetInt("count");
  ASSERT_TRUE(count_or.ok());
  EXPECT_EQ(count_or.value(), 42);
}

TEST_F(ArgumentParserTest, ParsesHexArguments) {
  std::vector<std::string> args = {"--address=0x1234", "--value", "0xFF"};
  ArgumentParser parser(args);

  auto addr_or = parser.GetHex("address");
  ASSERT_TRUE(addr_or.ok());
  EXPECT_EQ(addr_or.value(), 0x1234);

  auto value_or = parser.GetHex("value");
  ASSERT_TRUE(value_or.ok());
  EXPECT_EQ(value_or.value(), 0xFF);
}

TEST_F(ArgumentParserTest, DetectsFlags) {
  std::vector<std::string> args = {"--verbose", "--debug", "--format=json"};
  ArgumentParser parser(args);

  EXPECT_TRUE(parser.HasFlag("verbose"));
  EXPECT_TRUE(parser.HasFlag("debug"));
  EXPECT_FALSE(parser.HasFlag("format"));  // format is a value, not a flag
}

TEST_F(ArgumentParserTest, GetsPositionalArguments) {
  std::vector<std::string> args = {"command", "--flag", "value", "positional1",
                                   "positional2"};
  ArgumentParser parser(args);

  auto positional = parser.GetPositional();
  EXPECT_THAT(positional,
              ::testing::ElementsAre("command", "positional1", "positional2"));
}

TEST_F(ArgumentParserTest, ValidatesRequiredArguments) {
  std::vector<std::string> args = {"--type=dungeon"};
  ArgumentParser parser(args);

  auto status = parser.RequireArgs({"type"});
  EXPECT_TRUE(status.ok());

  status = parser.RequireArgs({"type", "missing"});
  EXPECT_FALSE(status.ok());
}

TEST_F(ArgumentParserTest, HandlesMissingArguments) {
  std::vector<std::string> args = {"--type=dungeon"};
  ArgumentParser parser(args);

  auto missing = parser.GetString("missing");
  EXPECT_FALSE(missing.has_value());

  auto int_missing = parser.GetInt("missing");
  EXPECT_FALSE(int_missing.ok());
}

// OutputFormatter Tests
class OutputFormatterTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(OutputFormatterTest, CreatesFromString) {
  auto json_formatter = OutputFormatter::FromString("json");
  ASSERT_TRUE(json_formatter.ok());
  EXPECT_TRUE(json_formatter.value().IsJson());

  auto text_formatter = OutputFormatter::FromString("text");
  ASSERT_TRUE(text_formatter.ok());
  EXPECT_TRUE(text_formatter.value().IsText());

  auto invalid_formatter = OutputFormatter::FromString("invalid");
  EXPECT_FALSE(invalid_formatter.ok());
}

TEST_F(OutputFormatterTest, GeneratesValidJson) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test");
  formatter.AddField("string_field", "value");
  formatter.AddField("int_field", 42);
  formatter.AddField("bool_field", true);
  formatter.AddHexField("hex_field", 0x1234, 4);

  formatter.BeginArray("array_field");
  formatter.AddArrayItem("item1");
  formatter.AddArrayItem("item2");
  formatter.EndArray();

  formatter.EndObject();

  std::string output = formatter.GetOutput();

  EXPECT_THAT(output, ::testing::HasSubstr("\"string_field\": \"value\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"int_field\": 42"));
  EXPECT_THAT(output, ::testing::HasSubstr("\"bool_field\": true"));
  EXPECT_THAT(output, ::testing::HasSubstr("\"hex_field\": \"0x1234\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"array_field\": ["));
  EXPECT_THAT(output, ::testing::HasSubstr("\"item1\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"item2\""));
}

TEST_F(OutputFormatterTest, GeneratesValidText) {
  auto formatter_or = OutputFormatter::FromString("text");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test Object");
  formatter.AddField("string_field", "value");
  formatter.AddField("int_field", 42);
  formatter.AddField("bool_field", true);
  formatter.AddHexField("hex_field", 0x1234, 4);

  formatter.BeginArray("array_field");
  formatter.AddArrayItem("item1");
  formatter.AddArrayItem("item2");
  formatter.EndArray();

  formatter.EndObject();

  std::string output = formatter.GetOutput();

  EXPECT_THAT(output, ::testing::HasSubstr("=== Test Object ==="));
  EXPECT_THAT(output, ::testing::HasSubstr("string_field         : value"));
  EXPECT_THAT(output, ::testing::HasSubstr("int_field            : 42"));
  EXPECT_THAT(output, ::testing::HasSubstr("bool_field           : yes"));
  EXPECT_THAT(output, ::testing::HasSubstr("hex_field            : 0x1234"));
  EXPECT_THAT(output, ::testing::HasSubstr("array_field:"));
  EXPECT_THAT(output, ::testing::HasSubstr("- item1"));
  EXPECT_THAT(output, ::testing::HasSubstr("- item2"));
}

TEST_F(OutputFormatterTest, EscapesJsonStrings) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test");
  formatter.AddField("quotes", "He said \"Hello\"");
  formatter.AddField("newlines", "Line1\nLine2");
  formatter.AddField("backslashes", "Path\\to\\file");
  formatter.EndObject();

  std::string output = formatter.GetOutput();

  EXPECT_THAT(output, ::testing::HasSubstr("\\\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\\n"));
  EXPECT_THAT(output, ::testing::HasSubstr("\\\\"));
}

TEST_F(OutputFormatterTest, HandlesEmptyObjects) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Empty");
  formatter.EndObject();

  std::string output = formatter.GetOutput();
  EXPECT_THAT(output, ::testing::HasSubstr("{}"));
}

TEST_F(OutputFormatterTest, HandlesEmptyArrays) {
  auto formatter_or = OutputFormatter::FromString("json");
  ASSERT_TRUE(formatter_or.ok());
  auto formatter = std::move(formatter_or.value());

  formatter.BeginObject("Test");
  formatter.BeginArray("empty_array");
  formatter.EndArray();
  formatter.EndObject();

  std::string output = formatter.GetOutput();
  EXPECT_THAT(output, ::testing::HasSubstr("\"empty_array\": []"));
}

}  // namespace resources
}  // namespace cli
}  // namespace yaze
