#include "gtest/gtest.h"

#ifdef YAZE_WITH_GRPC

#include <chrono>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "app/service/unified_grpc_server.h"
#include "app/testing/test_manager.h"
#include "protos/imgui_test_harness.grpc.pb.h"
#include "protos/rom_service.grpc.pb.h"
#include "rom/rom.h"

namespace {

bool WaitForChannelReady(const std::shared_ptr<grpc::Channel>& channel) {
  const auto deadline =
      std::chrono::system_clock::now() + std::chrono::seconds(5);
  return channel->WaitForConnected(deadline);
}

}  // namespace

TEST(YazeGrpcServerSmokeTest, UnaryRpcHandlersStayAlive) {
  yaze::YazeGRPCServer server;
  yaze::YazeGRPCServer::Config config;
  config.port = 0;
  config.enable_test_harness = true;
  config.enable_rom_service = true;
  config.enable_emulator_service = false;
  config.enable_canvas_automation = false;
  server.SetConfig(config);

  yaze::YazeGRPCServer::RomGetter rom_getter = []() -> yaze::Rom* {
    return nullptr;
  };

  const absl::Status init_status = server.Initialize(
      0, nullptr, rom_getter, yaze::YazeGRPCServer::RomLoader{},
      &yaze::test::TestManager::Get());
  ASSERT_TRUE(init_status.ok()) << init_status;

  const absl::Status start_status = server.StartAsync();
  ASSERT_TRUE(start_status.ok()) << start_status;
  ASSERT_GT(server.Port(), 0);

  const std::string address = "localhost:" + std::to_string(server.Port());
  auto channel =
      grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  ASSERT_TRUE(WaitForChannelReady(channel));

  auto harness_stub = yaze::test::ImGuiTestHarness::NewStub(channel);
  yaze::test::PingRequest ping_request;
  ping_request.set_message("smoke");
  yaze::test::PingResponse ping_response;
  grpc::ClientContext ping_context;
  const grpc::Status ping_status =
      harness_stub->Ping(&ping_context, ping_request, &ping_response);
  EXPECT_TRUE(ping_status.ok()) << ping_status.error_message();
  EXPECT_EQ(ping_response.message(), "Pong: smoke");

  auto rom_stub = yaze::proto::RomService::NewStub(channel);
  yaze::proto::GetRomInfoRequest rom_request;
  yaze::proto::GetRomInfoResponse rom_response;
  grpc::ClientContext rom_context;
  const grpc::Status rom_status =
      rom_stub->GetRomInfo(&rom_context, rom_request, &rom_response);
  EXPECT_EQ(rom_status.error_code(), grpc::StatusCode::FAILED_PRECONDITION)
      << rom_status.error_message();

  server.Shutdown();
}

#endif  // YAZE_WITH_GRPC
