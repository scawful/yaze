#include "cli/service/api/api_handlers.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "app/emu/debug/symbol_provider.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/model_registry.h"
#include "httplib.h"
#include "nlohmann/json.hpp"

namespace yaze::cli::api {
namespace {

using json = nlohmann::json;

class ModelRegistryGuard {
 public:
  ModelRegistryGuard() {
    ::yaze::cli::ModelRegistry::GetInstance().ClearServices();
  }
  ~ModelRegistryGuard() {
    ::yaze::cli::ModelRegistry::GetInstance().ClearServices();
  }
};

class StaticModelService : public MockAIService {
 public:
  explicit StaticModelService(std::vector<ModelInfo> models)
      : models_(std::move(models)) {}

  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override {
    return models_;
  }

  std::string GetProviderName() const override { return "static"; }

 private:
  std::vector<ModelInfo> models_;
};

class CountingModelService : public MockAIService {
 public:
  CountingModelService(int* counter, std::vector<ModelInfo> models)
      : counter_(counter), models_(std::move(models)) {}

  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override {
    if (counter_) {
      ++(*counter_);
    }
    return models_;
  }

  std::string GetProviderName() const override { return "counting"; }

 private:
  int* counter_ = nullptr;
  std::vector<ModelInfo> models_;
};

TEST(HttpApiHandlersTest, HealthReturnsOk) {
  httplib::Request req;
  httplib::Response res;

  HandleHealth(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["service"], "yaze-agent-api");
}

TEST(HttpApiHandlersTest, HealthSetsCorsHeaders) {
  httplib::Request req;
  httplib::Response res;

  HandleHealth(req, res);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

TEST(HttpApiHandlersTest, CorsPreflightReturnsNoContent) {
  httplib::Request req;
  httplib::Response res;

  HandleCorsPreflight(req, res);

  EXPECT_EQ(res.status, 204);
  auto it = res.headers.find("Access-Control-Allow-Origin");
  EXPECT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

TEST(HttpApiHandlersTest, ListModelsReturnsEmptyArrayWhenNoServices) {
  ModelRegistryGuard guard;
  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["count"], 0);
  EXPECT_TRUE(body["models"].is_array());
  EXPECT_TRUE(body["models"].empty());
}

TEST(HttpApiHandlersTest, ListModelsIncludesDisplayName) {
  ModelRegistryGuard guard;
  auto service = std::make_shared<MockAIService>();
  ::yaze::cli::ModelRegistry::GetInstance().RegisterService(service);

  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  ASSERT_TRUE(body["models"].is_array());
  ASSERT_FALSE(body["models"].empty());
  const auto& model = body["models"].front();
  EXPECT_EQ(model["name"], "mock-model");
  EXPECT_EQ(model["display_name"], "Mock Model");
}

TEST(HttpApiHandlersTest, ListModelsUsesNameWhenDisplayNameMissing) {
  ModelRegistryGuard guard;
  auto service = std::make_shared<StaticModelService>(
      std::vector<ModelInfo>{{.name = "alpha"}});  // display_name empty
  ::yaze::cli::ModelRegistry::GetInstance().RegisterService(service);

  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  ASSERT_TRUE(body["models"].is_array());
  ASSERT_FALSE(body["models"].empty());
  const auto& model = body["models"].front();
  EXPECT_EQ(model["name"], "alpha");
  EXPECT_EQ(model["display_name"], "alpha");
}

TEST(HttpApiHandlersTest, ListModelsSortsByName) {
  ModelRegistryGuard guard;
  auto service_one = std::make_shared<StaticModelService>(
      std::vector<ModelInfo>{{.name = "zeta"}, {.name = "alpha"}});
  auto service_two = std::make_shared<StaticModelService>(
      std::vector<ModelInfo>{{.name = "beta"}});
  ::yaze::cli::ModelRegistry::GetInstance().RegisterService(service_one);
  ::yaze::cli::ModelRegistry::GetInstance().RegisterService(service_two);

  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  ASSERT_EQ(body["count"], 3);
  ASSERT_TRUE(body["models"].is_array());
  ASSERT_EQ(body["models"].size(), 3u);
  EXPECT_EQ(body["models"][0]["name"], "alpha");
  EXPECT_EQ(body["models"][1]["name"], "beta");
  EXPECT_EQ(body["models"][2]["name"], "zeta");
}

TEST(HttpApiHandlersTest, ListModelsSetsCorsHeaders) {
  ModelRegistryGuard guard;
  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);

  auto it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "*");
}

TEST(HttpApiHandlersTest, ListModelsRefreshBypassesCache) {
  ModelRegistryGuard guard;
  int calls = 0;
  auto service = std::make_shared<CountingModelService>(
      &calls, std::vector<ModelInfo>{{.name = "alpha"}});
  ::yaze::cli::ModelRegistry::GetInstance().RegisterService(service);

  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);
  EXPECT_EQ(res.status, 200);
  EXPECT_EQ(calls, 1);

  httplib::Request refresh_req;
  refresh_req.params.emplace("refresh", "1");
  httplib::Response refresh_res;
  HandleListModels(refresh_req, refresh_res);

  EXPECT_EQ(refresh_res.status, 200);
  EXPECT_EQ(calls, 2);
}

TEST(HttpApiHandlersTest, ListModelsRefreshAcceptsTruthyValues) {
  ModelRegistryGuard guard;
  int calls = 0;
  auto service = std::make_shared<CountingModelService>(
      &calls, std::vector<ModelInfo>{{.name = "alpha"}});
  ::yaze::cli::ModelRegistry::GetInstance().RegisterService(service);

  httplib::Request req;
  httplib::Response res;

  HandleListModels(req, res);
  EXPECT_EQ(res.status, 200);
  EXPECT_EQ(calls, 1);

  httplib::Request refresh_true;
  refresh_true.params.emplace("refresh", "true");
  httplib::Response refresh_true_res;
  HandleListModels(refresh_true, refresh_true_res);
  EXPECT_EQ(refresh_true_res.status, 200);
  EXPECT_EQ(calls, 2);

  httplib::Request refresh_present;
  refresh_present.params.emplace("refresh", "");
  httplib::Response refresh_present_res;
  HandleListModels(refresh_present, refresh_present_res);
  EXPECT_EQ(refresh_present_res.status, 200);
  EXPECT_EQ(calls, 3);
}

TEST(HttpApiHandlersTest, GetSymbolsReturns503WithoutProvider) {
  httplib::Request req;
  httplib::Response res;

  HandleGetSymbols(req, res, nullptr);

  EXPECT_EQ(res.status, 503);
  auto cors_it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(cors_it, res.headers.end());
  EXPECT_EQ(cors_it->second, "*");
  auto body = json::parse(res.body);
  EXPECT_TRUE(body["error"].is_string());
}

TEST(HttpApiHandlersTest, GetSymbolsReturnsJsonWhenAcceptHeaderSet) {
  yaze::emu::debug::SymbolProvider symbols;
  symbols.AddSymbol({"TestSymbol", 0x008000});
  httplib::Request req;
  req.headers.emplace("Accept", "application/json");
  httplib::Response res;

  HandleGetSymbols(req, res, &symbols);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["format"], "mesen");
  const std::string exported = body["symbols"].get<std::string>();
  EXPECT_NE(exported.find("TestSymbol"), std::string::npos);
}

TEST(HttpApiHandlersTest, GetSymbolsReturnsJsonWhenAcceptHeaderIncludesJson) {
  yaze::emu::debug::SymbolProvider symbols;
  symbols.AddSymbol({"TestSymbol", 0x008000});
  httplib::Request req;
  req.headers.emplace("Accept", "application/json, text/plain");
  httplib::Response res;

  HandleGetSymbols(req, res, &symbols);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["format"], "mesen");
  const std::string exported = body["symbols"].get<std::string>();
  EXPECT_NE(exported.find("TestSymbol"), std::string::npos);
}

TEST(HttpApiHandlersTest, GetSymbolsUsesRequestedFormat) {
  yaze::emu::debug::SymbolProvider symbols;
  symbols.AddSymbol({"TestSymbol", 0x008000});
  httplib::Request req;
  req.params.emplace("format", "asar");
  req.headers.emplace("Accept", "application/json");
  httplib::Response res;

  HandleGetSymbols(req, res, &symbols);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["format"], "asar");
}

TEST(HttpApiHandlersTest, GetSymbolsReturns400ForInvalidFormat) {
  yaze::emu::debug::SymbolProvider symbols;
  symbols.AddSymbol({"TestSymbol", 0x008000});
  httplib::Request req;
  req.params.emplace("format", "bogus");
  httplib::Response res;

  HandleGetSymbols(req, res, &symbols);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
  EXPECT_EQ(body["format"], "bogus");
  EXPECT_TRUE(body["supported"].is_array());
  bool has_mesen = false;
  for (const auto& entry : body["supported"]) {
    if (entry == "mesen") {
      has_mesen = true;
      break;
    }
  }
  EXPECT_TRUE(has_mesen);
}

TEST(HttpApiHandlersTest, GetSymbolsReturnsPlainTextByDefault) {
  yaze::emu::debug::SymbolProvider symbols;
  symbols.AddSymbol({"TestSymbol", 0x008000});
  httplib::Request req;
  httplib::Response res;

  HandleGetSymbols(req, res, &symbols);

  EXPECT_EQ(res.status, 200);
  EXPECT_NE(res.body.find("TestSymbol"), std::string::npos);
  auto it = res.headers.find("Content-Type");
  ASSERT_NE(it, res.headers.end());
  EXPECT_EQ(it->second, "text/plain");
}

TEST(HttpApiHandlersTest, NavigateReturnsOkForValidPayload) {
  httplib::Request req;
  req.body = R"({"address":4660,"source":"test"})";
  httplib::Response res;

  HandleNavigate(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["address"], 4660);
}

TEST(HttpApiHandlersTest, NavigateReturns400ForInvalidJson) {
  httplib::Request req;
  req.body = "not-json";
  httplib::Response res;

  HandleNavigate(req, res);

  EXPECT_EQ(res.status, 400);
  auto cors_it = res.headers.find("Access-Control-Allow-Origin");
  ASSERT_NE(cors_it, res.headers.end());
  EXPECT_EQ(cors_it->second, "*");
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST(HttpApiHandlersTest, BreakpointHitReturnsOkForValidPayload) {
  httplib::Request req;
  req.body = R"({"address":4660,"source":"test","cpu_state":{"a":1}})";
  httplib::Response res;

  HandleBreakpointHit(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["address"], 4660);
}

TEST(HttpApiHandlersTest, BreakpointHitReturns400ForInvalidJson) {
  httplib::Request req;
  req.body = "{";
  httplib::Response res;

  HandleBreakpointHit(req, res);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST(HttpApiHandlersTest, StateUpdateReturnsOkForValidPayload) {
  httplib::Request req;
  req.body = R"({"source":"test","state":{"status":"ok"}})";
  httplib::Response res;

  HandleStateUpdate(req, res);

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
}

TEST(HttpApiHandlersTest, StateUpdateReturns400ForInvalidJson) {
  httplib::Request req;
  req.body = "{";
  httplib::Response res;

  HandleStateUpdate(req, res);

  EXPECT_EQ(res.status, 400);
  auto body = json::parse(res.body);
  EXPECT_TRUE(body.contains("error"));
}

TEST(HttpApiHandlersTest, WindowShowReturns501WhenUnavailable) {
  httplib::Request req;
  httplib::Response res;

  HandleWindowShow(req, res, {});

  EXPECT_EQ(res.status, 501);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "error");
  EXPECT_EQ(body["message"], "window control unavailable");
}

TEST(HttpApiHandlersTest, WindowShowReturnsOkWhenActionSucceeds) {
  httplib::Request req;
  httplib::Response res;

  HandleWindowShow(req, res, []() { return true; });

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["window"], "shown");
}

TEST(HttpApiHandlersTest, WindowShowReturns500WhenActionFails) {
  httplib::Request req;
  httplib::Response res;

  HandleWindowShow(req, res, []() { return false; });

  EXPECT_EQ(res.status, 500);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "error");
  EXPECT_EQ(body["message"], "window action failed");
}

TEST(HttpApiHandlersTest, WindowHideReturns501WhenUnavailable) {
  httplib::Request req;
  httplib::Response res;

  HandleWindowHide(req, res, {});

  EXPECT_EQ(res.status, 501);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "error");
  EXPECT_EQ(body["message"], "window control unavailable");
}

TEST(HttpApiHandlersTest, WindowHideReturnsOkWhenActionSucceeds) {
  httplib::Request req;
  httplib::Response res;

  HandleWindowHide(req, res, []() { return true; });

  EXPECT_EQ(res.status, 200);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "ok");
  EXPECT_EQ(body["window"], "hidden");
}

TEST(HttpApiHandlersTest, WindowHideReturns500WhenActionFails) {
  httplib::Request req;
  httplib::Response res;

  HandleWindowHide(req, res, []() { return false; });

  EXPECT_EQ(res.status, 500);
  auto body = json::parse(res.body);
  EXPECT_EQ(body["status"], "error");
  EXPECT_EQ(body["message"], "window action failed");
}

}  // namespace
}  // namespace yaze::cli::api
