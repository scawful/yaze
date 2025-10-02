#include "app/core/test_script_parser.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"

namespace yaze {
namespace test {
namespace {

constexpr int kSupportedSchemaVersion = 1;

std::string FormatIsoTimestamp(absl::Time time) {
  if (time == absl::InfinitePast()) {
    return "";
  }
  return absl::FormatTime("%Y-%m-%dT%H:%M:%S%Ez", time, absl::UTCTimeZone());
}

absl::StatusOr<absl::Time> ParseIsoTimestamp(const nlohmann::json& node,
                                             const char* field_name) {
  if (!node.contains(field_name)) {
    return absl::InfinitePast();
  }
  const std::string value = node.at(field_name).get<std::string>();
  if (value.empty()) {
    return absl::InfinitePast();
  }
  absl::Time parsed;
  std::string err;
  if (!absl::ParseTime("%Y-%m-%dT%H:%M:%S%Ez", value, &parsed, &err)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse timestamp '%s': %s", value, err));
  }
  return parsed;
}

void WriteExpectSection(const TestScriptStep& step, nlohmann::json* node) {
  nlohmann::json expect;
  expect["success"] = step.expect_success;
  if (!step.expect_status.empty()) {
    expect["status"] = step.expect_status;
  }
  if (!step.expect_message.empty()) {
    expect["message"] = step.expect_message;
  }
  if (!step.expect_assertion_failures.empty()) {
    expect["assertion_failures"] = step.expect_assertion_failures;
  }
  if (!step.expect_metrics.empty()) {
    nlohmann::json metrics(nlohmann::json::value_t::object);
    for (const auto& [key, value] : step.expect_metrics) {
      metrics[key] = value;
    }
    expect["metrics"] = std::move(metrics);
  }
  (*node)["expect"] = std::move(expect);
}

void PopulateStepNode(const TestScriptStep& step, nlohmann::json* node) {
  (*node)["action"] = step.action;
  if (!step.target.empty()) {
    (*node)["target"] = step.target;
  }
  if (!step.click_type.empty()) {
    (*node)["click_type"] = step.click_type;
  }
  if (!step.text.empty()) {
    (*node)["text"] = step.text;
  }
  if (step.clear_first) {
    (*node)["clear_first"] = step.clear_first;
  }
  if (!step.condition.empty()) {
    (*node)["condition"] = step.condition;
  }
  if (step.timeout_ms > 0) {
    (*node)["timeout_ms"] = step.timeout_ms;
  }
  if (!step.region.empty()) {
    (*node)["region"] = step.region;
  }
  if (!step.format.empty()) {
    (*node)["format"] = step.format;
  }
  WriteExpectSection(step, node);
}

absl::StatusOr<TestScriptStep> ParseStep(const nlohmann::json& node) {
  if (!node.contains("action")) {
    return absl::InvalidArgumentError(
        "Test script step missing required field 'action'");
  }

  TestScriptStep step;
  step.action = absl::AsciiStrToLower(node.at("action").get<std::string>());
  if (node.contains("target")) {
    step.target = node.at("target").get<std::string>();
  }
  if (node.contains("click_type")) {
    step.click_type =
        absl::AsciiStrToLower(node.at("click_type").get<std::string>());
  }
  if (node.contains("text")) {
    step.text = node.at("text").get<std::string>();
  }
  if (node.contains("clear_first")) {
    step.clear_first = node.at("clear_first").get<bool>();
  }
  if (node.contains("condition")) {
    step.condition = node.at("condition").get<std::string>();
  }
  if (node.contains("timeout_ms")) {
    step.timeout_ms = node.at("timeout_ms").get<int>();
  }
  if (node.contains("region")) {
    step.region = node.at("region").get<std::string>();
  }
  if (node.contains("format")) {
    step.format = node.at("format").get<std::string>();
  }

  if (node.contains("expect")) {
    const auto& expect = node.at("expect");
    if (expect.contains("success")) {
      step.expect_success = expect.at("success").get<bool>();
    }
    if (expect.contains("status")) {
      step.expect_status =
          absl::AsciiStrToLower(expect.at("status").get<std::string>());
    }
    if (expect.contains("message")) {
      step.expect_message = expect.at("message").get<std::string>();
    }
    if (expect.contains("assertion_failures")) {
      for (const auto& value : expect.at("assertion_failures")) {
        step.expect_assertion_failures.push_back(value.get<std::string>());
      }
    }
    if (expect.contains("metrics")) {
      for (auto it = expect.at("metrics").begin();
           it != expect.at("metrics").end(); ++it) {
        step.expect_metrics[it.key()] = it.value().get<int32_t>();
      }
    }
  }

  return step;
}

}  // namespace

absl::Status TestScriptParser::WriteToFile(const TestScript& script,
                                           const std::string& path) {
  nlohmann::json root;
  root["schema_version"] = script.schema_version;
  root["recording_id"] = script.recording_id;
  root["name"] = script.name;
  root["description"] = script.description;
  root["created_at"] = FormatIsoTimestamp(script.created_at);
  root["duration_ms"] = absl::ToInt64Milliseconds(script.duration);

  nlohmann::json steps_json = nlohmann::json::array();
  for (const auto& step : script.steps) {
    nlohmann::json step_node(nlohmann::json::value_t::object);
    PopulateStepNode(step, &step_node);
    steps_json.push_back(std::move(step_node));
  }
  root["steps"] = std::move(steps_json);

  std::filesystem::path output_path(path);
  std::error_code ec;
  auto parent = output_path.parent_path();
  if (!parent.empty() && !std::filesystem::exists(parent)) {
    if (!std::filesystem::create_directories(parent, ec)) {
      return absl::InternalError(absl::StrFormat(
          "Failed to create directory '%s': %s", parent.string(), ec.message()));
    }
  }

  std::ofstream ofs(output_path, std::ios::out | std::ios::trunc);
  if (!ofs.good()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open '%s' for writing", path));
  }
  ofs << root.dump(2) << '\n';
  return absl::OkStatus();
}

absl::StatusOr<TestScript> TestScriptParser::ParseFromFile(
    const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs.good()) {
    return absl::NotFoundError(
        absl::StrFormat("Test script '%s' not found", path));
  }

  nlohmann::json root;
  try {
    ifs >> root;
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse JSON: %s", e.what()));
  }

  TestScript script;
  script.schema_version =
      root.contains("schema_version") ? root["schema_version"].get<int>() : 1;

  if (script.schema_version != kSupportedSchemaVersion) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unsupported test script schema version %d", script.schema_version));
  }

  if (root.contains("recording_id")) {
    script.recording_id = root["recording_id"].get<std::string>();
  }
  if (root.contains("name")) {
    script.name = root["name"].get<std::string>();
  }
  if (root.contains("description")) {
    script.description = root["description"].get<std::string>();
  }

  ASSIGN_OR_RETURN(script.created_at,
                   ParseIsoTimestamp(root, "created_at"));
  if (root.contains("duration_ms")) {
    script.duration = absl::Milliseconds(root["duration_ms"].get<int64_t>());
  }

  if (!root.contains("steps") || !root["steps"].is_array()) {
    return absl::InvalidArgumentError(
        "Test script missing required array field 'steps'");
  }

  for (const auto& step_node : root["steps"]) {
  ASSIGN_OR_RETURN(auto step, ParseStep(step_node));
    script.steps.push_back(std::move(step));
  }

  return script;
}

}  // namespace test
}  // namespace yaze
