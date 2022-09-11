#include "service.h"

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

Status DeltaService::Init(grpc::ServerContext* context,
                          const InitRequest* request, InitResponse* reply) {
  return Status::OK;
}

Status DeltaService::Push(grpc::ServerContext* context,
                          const PushRequest* request, PushResponse* reply) {
  return Status::OK;
}

Status DeltaService::Pull(grpc::ServerContext* context,
                          const PullRequest* request, PullResponse* reply) {
  return Status::OK;
}

}  // namespace delta
}  // namespace app
}  // namespace yaze
