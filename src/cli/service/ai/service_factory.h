#ifndef YAZE_SRC_CLI_SERVICE_AI_SERVICE_FACTORY_H_
#define YAZE_SRC_CLI_SERVICE_AI_SERVICE_FACTORY_H_

#include <memory>
#include "cli/service/ai/ai_service.h"

namespace yaze {
namespace cli {

// Helper: Select AI service based on environment variables
std::unique_ptr<AIService> CreateAIService();

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AI_SERVICE_FACTORY_H_
