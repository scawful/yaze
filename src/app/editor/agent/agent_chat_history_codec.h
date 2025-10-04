#ifndef YAZE_APP_EDITOR_AGENT_AGENT_CHAT_HISTORY_CODEC_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_CHAT_HISTORY_CODEC_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {
namespace editor {

// Bridges chat history persistence to optional JSON support. When the
// application is built without gRPC/JSON support these helpers gracefully
// degrade and report an Unimplemented status so the UI can disable
// persistence instead of failing to compile.
class AgentChatHistoryCodec {
 public:
  struct CollaborationState {
    bool active = false;
    std::string session_id;
    std::string session_name;
    std::vector<std::string> participants;
    absl::Time last_synced = absl::InfinitePast();
  };

  struct MultimodalState {
    std::optional<std::filesystem::path> last_capture_path;
    std::string status_message;
    absl::Time last_updated = absl::InfinitePast();
  };

  struct Snapshot {
    std::vector<cli::agent::ChatMessage> history;
    CollaborationState collaboration;
    MultimodalState multimodal;
  };

  // Returns true when the codec can actually serialize / deserialize history.
  static bool Available();

  static absl::StatusOr<Snapshot> Load(const std::filesystem::path& path);
  static absl::Status Save(const std::filesystem::path& path,
                           const Snapshot& snapshot);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_CHAT_HISTORY_CODEC_H_
