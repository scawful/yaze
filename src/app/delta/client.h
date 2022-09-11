#ifndef YAZE_APP_DELTA_CLIENT_H
#define YAZE_APP_DELTA_CLIENT_H

#include <google/protobuf/message.h>
#include <grpc/support/log.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <vector>

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

class Client {
 public:
  void CreateChannel();
  absl::Status InitRepo(std::string author_name, std::string project_name);

 private:
  ClientContext rpc_context;
  std::vector<Repository> repos_;
  std::unique_ptr<YazeDelta::Stub> stub_;
};

}  // namespace delta
}  // namespace app
}  // namespace yaze

#endif