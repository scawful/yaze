#include "cli/service/ai/tool_schema_builder.h"

#include <string>

#include <gtest/gtest.h>

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
