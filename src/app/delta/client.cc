#include "client.h"

#include <google/protobuf/message.h>
#include <grpc/support/log.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "absl/status/status.h"
#include "src/app/delta/delta.grpc.pb.h"
#include "src/app/delta/delta.pb.h"

namespace yaze {
namespace app {
namespace delta {

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::Status;

void Client::CreateChannel() {
  auto channel = grpc::CreateChannel("localhost:50051",
                                     grpc::InsecureChannelCredentials());
  stub_ = ::YazeDelta::NewStub(channel);
}

absl::Status Client::InitRepo(std::string author_name,
                              std::string project_name) {
  Repository new_repo;
  new_repo.set_author_name(author_name);
  new_repo.set_project_name(project_name);

  InitRequest request;
  request.set_allocated_repo(&new_repo);

  InitResponse response;
  Status status = stub_->Init(&rpc_context, request, &response);

  if (!status.ok()) {
    std::cerr << status.error_code() << ": " << status.error_message()
              << std::endl;
    return absl::InternalError(status.error_message());
  }
  return absl::OkStatus();
}

}  // namespace delta
}  // namespace app
}  // namespace yaze
