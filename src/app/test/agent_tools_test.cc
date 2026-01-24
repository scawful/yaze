#include "app/test/agent_tools_test.h"
#include "util/log.h"

namespace yaze {
namespace test {

void RegisterAgentToolsTestSuite() {
  LOG_INFO("AgentTests", "Registering Agent Tools test suite");
  TestManager::Get().RegisterTestSuite(std::make_unique<AgentToolsTestSuite>());
}

}  // namespace test
}  // namespace yaze
