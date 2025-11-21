#include "cli/handlers/tools/emulator_commands.h"

#include <grpcpp/grpcpp.h>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "protos/emulator_service.grpc.pb.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

// A simple client for the EmulatorService
class EmulatorClient {
 public:
  EmulatorClient() {
    auto channel = grpc::CreateChannel("localhost:50051",
                                       grpc::InsecureChannelCredentials());
    stub_ = agent::EmulatorService::NewStub(channel);
  }

  template <typename TRequest, typename TResponse>
  absl::StatusOr<TResponse> CallRpc(
      grpc::Status (agent::EmulatorService::Stub::*rpc_method)(
          grpc::ClientContext*, const TRequest&, TResponse*),
      const TRequest& request) {
    TResponse response;
    grpc::ClientContext context;

    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
    context.set_deadline(deadline);

    grpc::Status status =
        (stub_.get()->*rpc_method)(&context, request, &response);

    if (!status.ok()) {
      return absl::UnavailableError(absl::StrFormat(
          "RPC failed: (%d) %s", status.error_code(), status.error_message()));
    }
    return response;
  }

 private:
  std::unique_ptr<agent::EmulatorService::Stub> stub_;
};

// Helper to parse button from string
absl::StatusOr<agent::Button> StringToButton(absl::string_view s) {
  if (s == "A")
    return agent::Button::A;
  if (s == "B")
    return agent::Button::B;
  if (s == "X")
    return agent::Button::X;
  if (s == "Y")
    return agent::Button::Y;
  if (s == "L")
    return agent::Button::L;
  if (s == "R")
    return agent::Button::R;
  if (s == "SELECT")
    return agent::Button::SELECT;
  if (s == "START")
    return agent::Button::START;
  if (s == "UP")
    return agent::Button::UP;
  if (s == "DOWN")
    return agent::Button::DOWN;
  if (s == "LEFT")
    return agent::Button::LEFT;
  if (s == "RIGHT")
    return agent::Button::RIGHT;
  return absl::InvalidArgumentError(absl::StrCat("Unknown button: ", s));
}

}  // namespace

// --- Command Implementations ---

absl::Status EmulatorResetCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::Empty request;
  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::Reset, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("EmulatorReset");
  formatter.AddField("success", response.success());
  formatter.AddField("message", response.message());
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorGetStateCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::GameStateRequest request;
  request.set_include_screenshot(parser.HasFlag("screenshot"));

  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::GetGameState, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("EmulatorState");
  formatter.AddField("game_mode", static_cast<uint64_t>(response.game_mode()));
  formatter.AddField("link_state",
                     static_cast<uint64_t>(response.link_state()));
  formatter.AddField("link_pos_x",
                     static_cast<uint64_t>(response.link_pos_x()));
  formatter.AddField("link_pos_y",
                     static_cast<uint64_t>(response.link_pos_y()));
  formatter.AddField("link_health",
                     static_cast<uint64_t>(response.link_health()));
  if (!response.screenshot_png().empty()) {
    formatter.AddField("screenshot_size",
                       static_cast<uint64_t>(response.screenshot_png().size()));
  }
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorReadMemoryCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::MemoryRequest request;

  uint32_t address;
  if (!absl::SimpleHexAtoi(parser.GetString("address").value(), &address)) {
    return absl::InvalidArgumentError("Invalid address format.");
  }
  request.set_address(address);
  request.set_size(parser.GetInt("length").value_or(16));

  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::ReadMemory, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("MemoryRead");
  formatter.AddHexField("address", response.address());
  formatter.AddField("data_hex", absl::BytesToHexString(response.data()));
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorWriteMemoryCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::MemoryWriteRequest request;

  uint32_t address;
  if (!absl::SimpleHexAtoi(parser.GetString("address").value(), &address)) {
    return absl::InvalidArgumentError("Invalid address format.");
  }
  request.set_address(address);

  std::string data_hex = parser.GetString("data").value();
  request.set_data(absl::HexStringToBytes(data_hex));

  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::WriteMemory, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("MemoryWrite");
  formatter.AddField("success", response.success());
  formatter.AddField("message", response.message());
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorPressButtonsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::ButtonRequest request;
  std::vector<std::string> buttons =
      absl::StrSplit(parser.GetString("buttons").value(), ',');
  for (const auto& btn_str : buttons) {
    auto button_or = StringToButton(btn_str);
    if (!button_or.ok())
      return button_or.status();
    request.add_buttons(button_or.value());
  }

  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::PressButtons, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("PressButtons");
  formatter.AddField("success", response.success());
  formatter.AddField("message", response.message());
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorReleaseButtonsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::ButtonRequest request;
  std::vector<std::string> buttons =
      absl::StrSplit(parser.GetString("buttons").value(), ',');
  for (const auto& btn_str : buttons) {
    auto button_or = StringToButton(btn_str);
    if (!button_or.ok())
      return button_or.status();
    request.add_buttons(button_or.value());
  }

  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::ReleaseButtons, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("ReleaseButtons");
  formatter.AddField("success", response.success());
  formatter.AddField("message", response.message());
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorHoldButtonsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  EmulatorClient client;
  agent::ButtonHoldRequest request;
  std::vector<std::string> buttons =
      absl::StrSplit(parser.GetString("buttons").value(), ',');
  for (const auto& btn_str : buttons) {
    auto button_or = StringToButton(btn_str);
    if (!button_or.ok())
      return button_or.status();
    request.add_buttons(button_or.value());
  }
  request.set_duration_ms(parser.GetInt("duration").value());

  auto response_or =
      client.CallRpc(&agent::EmulatorService::Stub::HoldButtons, request);
  if (!response_or.ok()) {
    return response_or.status();
  }
  auto response = response_or.value();

  formatter.BeginObject("HoldButtons");
  formatter.AddField("success", response.success());
  formatter.AddField("message", response.message());
  formatter.EndObject();
  return absl::OkStatus();
}

// --- Placeholder Implementations for commands not yet migrated to gRPC ---

absl::Status EmulatorStepCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Step");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorRunCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Run");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorPauseCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Pause");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorSetBreakpointCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Breakpoint Set");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorClearBreakpointCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Breakpoint Cleared");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorListBreakpointsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Breakpoints");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorGetRegistersCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Registers");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EmulatorGetMetricsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  formatter.BeginObject("Emulator Metrics");
  formatter.AddField("status", "not_implemented");
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
