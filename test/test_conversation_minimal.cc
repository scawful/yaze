#include <iostream>
#include "cli/service/ai/service_factory.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "app/rom.h"

using namespace yaze;
using namespace yaze::cli;
using namespace yaze::cli::agent;

int main() {
  std::cout << "Test 1: Creating AI Service...\n";
  auto ai_service = CreateAIService();
  std::cout << "✅ AI Service created\n";
  
  std::cout << "Test 2: Creating Conversational Agent Service...\n";
  ConversationalAgentService service;
  std::cout << "✅ Conversational Agent Service created\n";
  
  std::cout << "Test 3: Creating ROM...\n";
  Rom rom;
  std::cout << "✅ ROM created\n";
  
  std::cout << "Test 4: Setting ROM context...\n";
  service.SetRomContext(&rom);
  std::cout << "✅ ROM context set\n";
  
  std::cout << "\n🎉 All tests passed!\n";
  return 0;
}
