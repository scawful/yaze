#ifndef YAZE_CLI_HANDLERS_TOOLS_TEST_CLI_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_TEST_CLI_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

/**
 * @brief List available tests with labels and requirements
 *
 * Provides machine-readable test discovery for agents and CI.
 * Usage: z3ed test-list [--format json|text] [--label <label>]
 */
class TestListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "test-list"; }

  std::string GetDescription() const {
    return "List available tests with labels and requirements";
  }

  std::string GetUsage() const override {
    return "test-list [--format json|text] [--label <label>]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Test Discovery"; }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "test-list";
    desc.summary = "List available ctest labels, test counts, and requirements "
                   "for each test suite.";
    desc.todo_reference = "todo#test-list";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Run tests with structured output
 *
 * Wraps ctest execution with JSON output parsing.
 * Usage: z3ed test-run [--label <label>] [--format json|text] [--verbose]
 */
class TestRunCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "test-run"; }

  std::string GetDescription() const {
    return "Run tests with structured output";
  }

  std::string GetUsage() const override {
    return "test-run [--label <label>] [--preset <preset>] "
           "[--format json|text] [--verbose] [--filter <regex>]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Test Run"; }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "test-run";
    desc.summary = "Run ctest with specified label/preset and produce "
                   "structured output. Supports regex filtering of tests.";
    desc.todo_reference = "todo#test-run";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Show test configuration status
 *
 * Displays current ROM path, preset, and enabled test suites.
 * Usage: z3ed test-status [--format json|text]
 */
class TestStatusCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "test-status"; }

  std::string GetDescription() const {
    return "Show test configuration status (ROM path, preset, enabled suites)";
  }

  std::string GetUsage() const override {
    return "test-status [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Test Status"; }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "test-status";
    desc.summary = "Show current test configuration including ROM path, "
                   "active preset, and which test suites are enabled.";
    desc.todo_reference = "todo#test-status";
    return desc;
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_TOOLS_TEST_CLI_COMMANDS_H

