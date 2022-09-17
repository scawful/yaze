#include <asar/interface-lib.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <google/protobuf/repeated_field.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "app/delta/client.h"
#include "app/delta/service.h"
#include "app/core/constants.h"
#include "src/app/delta/delta.grpc.pb.h"
#include "src/app/delta/delta.pb.h"
#include "app/rom.h"

namespace yaze_test {
namespace delta_test {

TEST(DeltaTest, InitRepoAndPushOk) {
  yaze::app::delta::DeltaService service;
  yaze::app::ROM rom;
  Bytes test_bytes;
  test_bytes.push_back(0x40);
  rom.LoadFromBytes(test_bytes);
  grpc::ServerContext* context;
  InitRequest init_request;
  auto repo = init_request.mutable_repo();
  repo->set_project_name("test_repo");
  Branch branch;
  branch.set_branch_name("test_branch");
  auto new_mutable_commits = branch.mutable_commits();
  new_mutable_commits->Reserve(5);
  for (int i = 0; i < 5; ++i) {
    auto new_commit = new Commit();
    new_commit->set_commit_id(i);
    new_commit->set_data(rom.char_data());
    new_mutable_commits->Add();
    new_mutable_commits->at(i) = *new_commit;
  }
  auto mutable_tree = repo->mutable_tree();
  mutable_tree->Add();
  mutable_tree->at(0) = branch;
  InitResponse init_response;
  auto init_status = service.Init(context, &init_request, &init_response);
  EXPECT_TRUE(init_status.ok());

  PushRequest request;
  request.set_branch_name("test_branch");
  request.set_repository_name("test_repo");
  auto mutable_commits = request.mutable_commits();
  mutable_commits->Reserve(5);
  for (int i = 0; i < 5; ++i) {
    auto new_commit = new Commit();
    new_commit->set_commit_id(i * 2);
    mutable_commits->Add();
    mutable_commits->at(i) = *new_commit;
  }
  PushResponse reply;
  auto status = service.Push(context, &request, &reply);
  EXPECT_TRUE(status.ok());

  auto repos = service.Repos();
  auto result_branch = repos.at(0).tree();
  std::cerr << result_branch.at(0).DebugString() << std::endl;
}



}  // namespace asm_test
}  // namespace yaze_test