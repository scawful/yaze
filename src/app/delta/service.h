#ifndef YAZE_APP_DELTA_SERVICE_H
#define YAZE_APP_DELTA_SERVICE_H

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

class DeltaService final : public ::YazeDelta::Service {
 public:
  Status Init(grpc::ServerContext* context, const InitRequest* request,
              InitResponse* reply) override;

  Status Push(grpc::ServerContext* context, const PushRequest* request,
              PushResponse* reply) override;

  Status Pull(grpc::ServerContext* context, const PullRequest* request,
              PullResponse* reply) override;

 private:
  std::vector<Repository> repos_;
};

}  // namespace delta
}  // namespace app
}  // namespace yaze
