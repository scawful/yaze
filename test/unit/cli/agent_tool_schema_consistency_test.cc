#include <set>
#include <string>

#include <gtest/gtest.h>

#include "cli/service/agent/tool_registry.h"
#include "cli/service/ai/prompt_builder.h"
#include "cli/service/ai/tool_schema_builder.h"
#include "cli/service/usage_schema.h"
#include "nlohmann/json.hpp"

namespace yaze::cli {
namespace {

// Tool declarations shown to models must use the flag names handlers parse;
// ArgumentParser matches names exactly, so `room_id` never reaches `--room`.

std::set<std::string> FlagNames(const std::string& usage) {
  std::set<std::string> names;
  for (const UsageParameter& parameter : ParseUsageParameters(usage)) {
    if (!parameter.positional) {
      names.insert(parameter.name);
    }
  }
  return names;
}

struct HandlerContract {
  std::string usage;
  std::set<std::string> flags;
  std::set<std::string> required;
};

// The handler's own usage string plus the flags CommandHandler::Run consumes
// for every command.
HandlerContract ContractFor(const std::string& tool_name) {
  auto handler_or = agent::ToolRegistry::Get().CreateHandler(tool_name);
  if (!handler_or.ok()) {
    return {};
  }
  HandlerContract contract;
  contract.usage = (*handler_or)->GetUsage();
  for (const UsageParameter& parameter : ParseUsageParameters(contract.usage)) {
    if (parameter.positional) {
      continue;
    }
    contract.flags.insert(parameter.name);
    if (parameter.required && parameter.takes_value) {
      contract.required.insert(parameter.name);
    }
  }
  contract.flags.insert({"format", "rom", "symbols", "project-context",
                         "mock-rom", "sandbox", "verbose"});
  return contract;
}

bool IsRegisteredTool(const std::string& name) {
  return agent::ToolRegistry::Get().GetToolDefinition(name).has_value();
}

bool MayBeUnregisteredInThisBuild(const std::string& name) {
#ifdef YAZE_WITH_GRPC
  (void)name;
  return false;
#else
  return name.rfind("emulator-", 0) == 0;
#endif
}

TEST(AgentToolSchemaConsistencyTest, RegisteredToolUsageUsesHandlerFlags) {
  for (const agent::ToolDefinition& def :
       agent::ToolRegistry::Get().GetAllTools()) {
    const HandlerContract contract = ContractFor(def.name);
    const std::set<std::string> advertised = FlagNames(def.usage);
    for (const std::string& flag : advertised) {
      EXPECT_TRUE(contract.flags.contains(flag))
          << def.name << " tool usage advertises --" << flag
          << "\n  tool usage:    " << def.usage
          << "\n  handler usage: " << contract.usage;
    }
    for (const std::string& flag : contract.required) {
      EXPECT_TRUE(advertised.contains(flag))
          << def.name << " tool usage omits required --" << flag
          << "\n  tool usage:    " << def.usage
          << "\n  handler usage: " << contract.usage;
    }
  }
}

#if defined(YAZE_AI_RUNTIME_AVAILABLE) && defined(YAZE_WITH_JSON)

TEST(AgentToolSchemaConsistencyTest, PromptCatalogueToolsUseHandlerFlags) {
  PromptBuilder prompt_builder;
  ASSERT_TRUE(prompt_builder.LoadResourceCatalogue("").ok());
  ASSERT_FALSE(prompt_builder.tool_specs().empty());

  for (const ToolSpecification& spec : prompt_builder.tool_specs()) {
    if (!IsRegisteredTool(spec.name)) {
      EXPECT_TRUE(MayBeUnregisteredInThisBuild(spec.name))
          << "prompt catalogue tool is not a registered agent tool: "
          << spec.name;
      continue;
    }
    const HandlerContract contract = ContractFor(spec.name);
    std::set<std::string> declared;
    for (const ToolArgument& argument : spec.arguments) {
      declared.insert(argument.name);
      EXPECT_TRUE(contract.flags.contains(argument.name))
          << spec.name << " catalogue argument '" << argument.name
          << "' is not a handler flag\n  handler usage: " << contract.usage;
    }
    for (const std::string& flag : contract.required) {
      EXPECT_TRUE(declared.contains(flag))
          << spec.name << " catalogue omits required argument '" << flag
          << "'\n  handler usage: " << contract.usage;
    }
  }
}

TEST(AgentToolSchemaConsistencyTest, FallbackFunctionSchemasUseHandlerFlags) {
  auto declarations_or = ToolSchemaBuilder::LoadFunctionDeclarationsFromAsset();
  ASSERT_TRUE(declarations_or.ok()) << declarations_or.status();
  ASSERT_FALSE(declarations_or->empty());

  for (const nlohmann::json& declaration : *declarations_or) {
    const std::string name = declaration.value("name", "");
    if (!IsRegisteredTool(name)) {
      EXPECT_TRUE(MayBeUnregisteredInThisBuild(name))
          << "function_schemas.json declares an unregistered tool: " << name;
      continue;
    }
    const HandlerContract contract = ContractFor(name);
    const nlohmann::json properties =
        declaration.value("parameters", nlohmann::json::object())
            .value("properties", nlohmann::json::object());
    for (const auto& [property, _] : properties.items()) {
      EXPECT_TRUE(contract.flags.contains(property))
          << name << " schema property '" << property
          << "' is not a handler flag\n  handler usage: " << contract.usage;
    }
    for (const std::string& flag : contract.required) {
      EXPECT_TRUE(properties.contains(flag))
          << name << " schema omits required property '" << flag
          << "'\n  handler usage: " << contract.usage;
    }
  }
}

#endif  // YAZE_AI_RUNTIME_AVAILABLE && YAZE_WITH_JSON

}  // namespace
}  // namespace yaze::cli
