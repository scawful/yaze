#include "cli/service/ai/tool_schema_builder.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

#include "cli/service/ai/tool_call_argument_codec.h"
#include "nlohmann/json.hpp"

namespace yaze::cli {
namespace {

#if defined(YAZE_AI_RUNTIME_AVAILABLE) && defined(YAZE_WITH_JSON)

TEST(ToolSchemaBuilderTest,
     BuildFunctionDeclarationsProducesBareFunctionDeclarations) {
  ToolSpecification spec;
  spec.name = "resource-list";
  spec.description = "List ROM resources.";
  spec.usage_notes = "Use this before guessing.";
  spec.arguments = {
      ToolArgument{"type", "Resource type", true, "dungeon"},
      ToolArgument{"limit", "Optional result limit", false, "10"}};

  const nlohmann::json declarations =
      ToolSchemaBuilder::BuildFunctionDeclarations({spec});

  ASSERT_TRUE(declarations.is_array());
  ASSERT_EQ(declarations.size(), 1u);
  const auto& function = declarations[0];
  EXPECT_EQ(function["name"], "resource-list");
  EXPECT_FALSE(function.contains("type"));
  EXPECT_FALSE(function.contains("function"));
  EXPECT_EQ(function["description"],
            "List ROM resources. Use this before guessing.");
  ASSERT_TRUE(function["parameters"]["required"].is_array());
  EXPECT_EQ(function["parameters"]["required"][0], "type");
  EXPECT_TRUE(function["parameters"]["properties"].contains("limit"));
}

TEST(ToolSchemaBuilderTest,
     ParseFunctionDeclarationsNormalizesLegacySchemaWrappers) {
  const std::string openai_wrapped = R"json(
    [
      {
        "type": "function",
        "function": {
          "name": "resource-search",
          "description": "Search ROM resources.",
          "parameters": {"type": "object", "properties": {}}
        }
      }
    ]
  )json";
  auto openai_or = ToolSchemaBuilder::ParseFunctionDeclarations(openai_wrapped);
  ASSERT_TRUE(openai_or.ok()) << openai_or.status();
  ASSERT_EQ(openai_or->size(), 1u);
  EXPECT_EQ((*openai_or)[0]["name"], "resource-search");
  EXPECT_FALSE((*openai_or)[0].contains("function"));

  const std::string gemini_wrapped = R"json(
    {
      "function_declarations": [
        {
          "name": "resource-list",
          "description": "List ROM resources.",
          "parameters": {"type": "object", "properties": {}}
        }
      ]
    }
  )json";
  auto gemini_or = ToolSchemaBuilder::ParseFunctionDeclarations(gemini_wrapped);
  ASSERT_TRUE(gemini_or.ok()) << gemini_or.status();
  ASSERT_EQ(gemini_or->size(), 1u);
  EXPECT_EQ((*gemini_or)[0]["name"], "resource-list");
}

TEST(ToolSchemaBuilderTest,
     PromptBuilderSchemasRoundTripIntoOpenAIToolsPayload) {
  PromptBuilder prompt_builder;
  ASSERT_TRUE(prompt_builder.LoadResourceCatalogue("").ok());

  auto declarations_or = ToolSchemaBuilder::ParseFunctionDeclarations(
      prompt_builder.BuildFunctionCallSchemas());
  ASSERT_TRUE(declarations_or.ok()) << declarations_or.status();
  ASSERT_FALSE(declarations_or->empty());

  const nlohmann::json openai_tools =
      ToolSchemaBuilder::BuildOpenAITools(*declarations_or);
  ASSERT_TRUE(openai_tools.is_array());
  ASSERT_FALSE(openai_tools.empty());
  EXPECT_EQ(openai_tools[0]["type"], "function");
  EXPECT_TRUE(openai_tools[0]["function"].contains("name"));
  EXPECT_TRUE(openai_tools[0]["function"].contains("parameters"));
}

TEST(ToolSchemaBuilderTest,
     PromptBuilderSchemasRoundTripIntoGeminiAndAnthropicPayloads) {
  PromptBuilder prompt_builder;
  ASSERT_TRUE(prompt_builder.LoadResourceCatalogue("").ok());

  auto declarations_or =
      ToolSchemaBuilder::ResolveFunctionDeclarations(prompt_builder);
  ASSERT_TRUE(declarations_or.ok()) << declarations_or.status();
  ASSERT_FALSE(declarations_or->empty());

  const nlohmann::json gemini_tools =
      ToolSchemaBuilder::BuildGeminiTools(*declarations_or);
  ASSERT_TRUE(gemini_tools.is_array());
  ASSERT_EQ(gemini_tools.size(), 1u);
  ASSERT_TRUE(gemini_tools[0].contains("function_declarations"));
  ASSERT_TRUE(gemini_tools[0]["function_declarations"].is_array());
  EXPECT_EQ(gemini_tools[0]["function_declarations"][0]["name"],
            (*declarations_or)[0]["name"]);

  const nlohmann::json anthropic_tools =
      ToolSchemaBuilder::BuildAnthropicTools(*declarations_or);
  ASSERT_TRUE(anthropic_tools.is_array());
  ASSERT_FALSE(anthropic_tools.empty());
  EXPECT_TRUE(anthropic_tools[0].contains("name"));
  EXPECT_TRUE(anthropic_tools[0].contains("description"));
  EXPECT_TRUE(anthropic_tools[0].contains("input_schema"));
  EXPECT_FALSE(anthropic_tools[0].contains("parameters"));
}

TEST(ToolSchemaBuilderTest,
     ActiveCataloguesExposeGuardedDungeonPaletteToolsToProviders) {
  for (const std::string catalogue : {"", "agent/prompt_catalogue_v2.yaml"}) {
    SCOPED_TRACE(catalogue.empty() ? "default" : catalogue);
    PromptBuilder prompt_builder;
    ASSERT_TRUE(prompt_builder.LoadResourceCatalogue(catalogue).ok());

    auto declarations_or =
        ToolSchemaBuilder::ResolveFunctionDeclarations(prompt_builder);
    ASSERT_TRUE(declarations_or.ok()) << declarations_or.status();

    const nlohmann::json* getter = nullptr;
    const nlohmann::json* setter = nullptr;
    for (const auto& declaration : *declarations_or) {
      const std::string name = declaration.value("name", "");
      if (name == "dungeon-get-palette") {
        getter = &declaration;
      } else if (name == "dungeon-set-palette-color") {
        setter = &declaration;
      }
    }
    ASSERT_NE(getter, nullptr);
    ASSERT_NE(setter, nullptr);
    EXPECT_NE(getter->at("description").get<std::string>().find("Read-only"),
              std::string::npos);
    EXPECT_NE(setter->at("description").get<std::string>().find("mutating"),
              std::string::npos);

    std::set<std::string> required;
    for (const auto& name : setter->at("parameters").at("required")) {
      required.insert(name.get<std::string>());
    }
    EXPECT_EQ(required,
              (std::set<std::string>{"room", "index", "expect-palette-set",
                                     "expect-palette-index", "expect-color",
                                     "color", "manifest"}));
    EXPECT_TRUE(setter->at("parameters").at("properties").contains("write"));
    EXPECT_FALSE(required.contains("write"));
  }
}

TEST(ToolSchemaBuilderTest,
     ProviderToolArgumentCodecPreservesDungeonPaletteIntegersAndWriteFlag) {
  const nlohmann::json provider_arguments = {
      {"room", "0xA8"},
      {"index", 7},
      {"expect-palette-set", "0x07"},
      {"expect-palette-index", 7},
      {"expect-color", "0x7FFF"},
      {"color", "0x7FFE"},
      {"manifest", "Roms/hack_manifest.json"},
      {"write", true},
  };

  const auto decoded = ai::DecodeToolCallArguments(provider_arguments);

  EXPECT_EQ(decoded.at("room"), "0xA8");
  EXPECT_EQ(decoded.at("index"), "7");
  EXPECT_EQ(decoded.at("expect-palette-index"), "7");
  EXPECT_EQ(decoded.at("write"), "true");
}

TEST(ToolSchemaBuilderTest,
     LoadFunctionDeclarationsFromAssetNormalizesGeminiStyleFallback) {
  auto declarations_or = ToolSchemaBuilder::LoadFunctionDeclarationsFromAsset();
  ASSERT_TRUE(declarations_or.ok()) << declarations_or.status();
  ASSERT_TRUE(declarations_or->is_array());
  ASSERT_FALSE(declarations_or->empty());
  EXPECT_TRUE((*declarations_or)[0].contains("name"));
  EXPECT_FALSE((*declarations_or)[0].contains("function"));
}

#else

TEST(ToolSchemaBuilderTest, RequiresAiRuntimeAndJson) {
  GTEST_SKIP() << "Tool schema builder tests require AI runtime and JSON";
}

#endif

}  // namespace
}  // namespace yaze::cli
