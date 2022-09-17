#include "service.h"

#include <google/protobuf/message.h>
#include <grpc/support/log.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
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

namespace {
auto FindRepository(std::vector<Repository>& repos, const std::string& name) {
  for (auto& repo : repos) {
    if (repo.project_name() == name) {
      return repo.mutable_tree();
    }
  }
}

auto FindBranch(google::protobuf::RepeatedPtrField<Branch>* repo,
                const std::string& branch_name) {
  for (auto it = repo->begin(); it != repo->end(); ++it) {
    if (it->branch_name() == branch_name) {
      return it->mutable_commits();
    }
  }
}
}  // namespace

Status DeltaService::Init(grpc::ServerContext* context,
                          const InitRequest* request, InitResponse* reply) {
  std::filesystem::create_directories("./.yaze");
  repos_.push_back(request->repo());
  return Status::OK;
}

Status DeltaService::Push(grpc::ServerContext* context,
                          const PushRequest* request, PushResponse* reply) {
  const auto& repository_name = request->repository_name();
  const auto& branch_name = request->branch_name();
  auto repo = FindRepository(repos_, repository_name);
  auto mutable_commits = FindBranch(repo, branch_name);
  auto size = request->commits().size();
  for (int i = 1; i < size; ++i) {
    *mutable_commits->Add() = request->commits().at(i);
  }
  return Status::OK;
}

Status DeltaService::Pull(grpc::ServerContext* context,
                          const PullRequest* request, PullResponse* reply) {
  return Status::OK;
}

Status DeltaService::Clone(grpc::ServerContext* context,
                           const CloneRequest* request, CloneResponse* reply) {
  return Status::OK;
}

}  // namespace delta
}  // namespace app
}  // namespace yaze
