#include "cli/service/usage_schema.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze::cli {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

UsageParameter Flag(std::string name, bool required, std::string hint = "",
                    std::vector<std::string> choices = {}) {
  UsageParameter parameter;
  parameter.name = std::move(name);
  parameter.required = required;
  parameter.takes_value = !hint.empty();
  parameter.value_hint = std::move(hint);
  parameter.choices = std::move(choices);
  return parameter;
}

TEST(UsageSchemaTest, ParsesRequiredAndOptionalFlags) {
  EXPECT_THAT(
      ParseUsageParameters("overworld-get-tile --map <hex> --x <area_tile_x> "
                           "[--format <json|text>]"),
      ElementsAre(Flag("map", true, "hex"), Flag("x", true, "area_tile_x"),
                  Flag("format", false, "json|text", {"json", "text"})));
}

TEST(UsageSchemaTest, ParsesEqualsFormAndBareLiteralValues) {
  EXPECT_THAT(
      ParseUsageParameters("tools-extract-values --rom=<path> "
                           "[--format=<json|cpp>] [--reserved 0x7B,0x7C]"),
      ElementsAre(Flag("rom", true, "path"),
                  Flag("format", false, "json|cpp", {"json", "cpp"}),
                  Flag("reserved", false, "0x7B,0x7C")));
}

TEST(UsageSchemaTest, BareFlagsAreBooleanSwitches) {
  EXPECT_THAT(
      ParseUsageParameters("dungeon-import --in <path> [--force] [--dry-run "
                           "[--report <path>]]"),
      ElementsAre(Flag("in", true, "path"), Flag("force", false),
                  Flag("dry-run", false), Flag("report", false, "path")));
}

TEST(UsageSchemaTest, AlternativesAreNeverRequired) {
  EXPECT_THAT(
      ParseUsageParameters("cmd [--sandbox | --mock-rom] --a <x> | --b <y>"),
      ElementsAre(Flag("sandbox", false), Flag("mock-rom", false),
                  Flag("a", false, "x"), Flag("b", false, "y")));
  EXPECT_THAT(ParseUsageParameters("cmd [--sandbox|--mock-rom]"),
              ElementsAre(Flag("sandbox", false), Flag("mock-rom", false)));
  EXPECT_THAT(
      ParseUsageParameters("cmd [--screen <hex>|--map <hex>]"),
      ElementsAre(Flag("screen", false, "hex"), Flag("map", false, "hex")));
}

TEST(UsageSchemaTest, PlaceholdersWithSpacesStayIntactWithoutChoices) {
  EXPECT_THAT(ParseUsageParameters("census [--project <code folder|.yaze>]"),
              ElementsAre(Flag("project", false, "code folder|.yaze")));
}

TEST(UsageSchemaTest, ParsesPositionalPlaceholders) {
  std::vector<UsageParameter> parameters =
      ParseUsageParameters("z3ed asm-check <file> [<extra files...>]");

  ASSERT_EQ(parameters.size(), 2u);
  EXPECT_EQ(parameters[0].name, "file");
  EXPECT_TRUE(parameters[0].positional);
  EXPECT_TRUE(parameters[0].required);
  EXPECT_EQ(parameters[1].name, "extra_files");
  EXPECT_FALSE(parameters[1].required);
}

TEST(UsageSchemaTest, FreeFormUsageYieldsNoParameters) {
  EXPECT_THAT(ParseUsageParameters("message-doctor [flags]"), IsEmpty());
  EXPECT_THAT(ParseUsageParameters(""), IsEmpty());
}

TEST(UsageSchemaTest, DuplicateFlagsKeepFirstOccurrence) {
  EXPECT_THAT(ParseUsageParameters("cmd --room <hex> [--room <hex>]"),
              ElementsAre(Flag("room", true, "hex")));
}

TEST(UsageSchemaTest, JsonSchemaUsesLiteralFlagNames) {
  nlohmann::ordered_json schema = UsageParametersToJsonSchema(
      ParseUsageParameters("dungeon-set-palette-color --room <hex> "
                           "[--expect-palette-set <id>] [--format <json|text>] "
                           "[--write] <file>"));

  EXPECT_EQ(schema["type"], "object");
  EXPECT_EQ(schema["required"], nlohmann::ordered_json::array({"room"}));
  const auto& properties = schema["properties"];
  EXPECT_EQ(properties["room"]["type"], "string");
  EXPECT_EQ(properties["room"]["description"], "hex");
  EXPECT_EQ(properties["expect-palette-set"]["type"], "string");
  EXPECT_EQ(properties["format"]["enum"],
            nlohmann::ordered_json::array({"json", "text"}));
  EXPECT_EQ(properties["write"]["type"], "boolean");
  EXPECT_FALSE(properties.contains("file"));
}

TEST(UsageSchemaTest, JsonSchemaOmitsEmptyRequiredList) {
  nlohmann::ordered_json schema =
      UsageParametersToJsonSchema(ParseUsageParameters("cmd [--verbose]"));

  EXPECT_FALSE(schema.contains("required"));
  EXPECT_EQ(schema["properties"]["verbose"]["type"], "boolean");
}

}  // namespace
}  // namespace yaze::cli
