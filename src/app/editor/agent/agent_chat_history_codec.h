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

  struct AgentConfigSnapshot {
    struct ToolFlags {
      bool resources = true;
      bool dungeon = true;
      bool overworld = true;
      bool dialogue = true;
      bool messages = true;
      bool gui = true;
      bool music = true;
      bool sprite = true;
      bool emulator = true;
    };
    struct ModelPreset {
      std::string name;
      std::string model;
      std::string host;
      std::vector<std::string> tags;
      bool pinned = false;
    };

    std::string provider;
    std::string model;
    std::string ollama_host;
    std::string gemini_api_key;
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
    int max_retry_attempts = 3;
    float temperature = 0.25f;
    float top_p = 0.95f;
    int max_output_tokens = 2048;
    bool stream_responses = false;
    int chain_mode = 0;
    std::vector<std::string> favorite_models;
    std::vector<std::string> model_chain;
    std::vector<ModelPreset> model_presets;
    std::string persona_notes;
    std::vector<std::string> goals;
    ToolFlags tools;
  };

  struct Snapshot {
    std::vector<cli::agent::ChatMessage> history;
    CollaborationState collaboration;
    MultimodalState multimodal;
    std::optional<AgentConfigSnapshot> agent_config;
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
