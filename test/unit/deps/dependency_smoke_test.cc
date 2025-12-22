#include "httplib.h"
#include "nlohmann/json.hpp"
#include "testing.h"

namespace yaze {
namespace test {

TEST(DependencySmokeTest, JsonAndHttpLibAvailable) {
  nlohmann::json payload = nlohmann::json::parse(R"({"ok":true})");
  EXPECT_TRUE(payload["ok"].get<bool>());

  httplib::Client client("example.com");
  httplib::Headers headers{{"X-Test", "1"}};
  client.set_default_headers(headers);
  auto header_it = headers.find("X-Test");
  ASSERT_NE(header_it, headers.end());
  EXPECT_EQ(header_it->second, "1");
}

}  // namespace test
}  // namespace yaze
