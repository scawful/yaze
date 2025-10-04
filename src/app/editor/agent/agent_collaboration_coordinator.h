#ifndef YAZE_APP_EDITOR_AGENT_AGENT_COLLABORATION_COORDINATOR_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_COLLABORATION_COORDINATOR_H_

#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace editor {

// Coordinates lightweight collaboration features for the agent chat widget.
// This implementation uses the local filesystem as a shared backing store so
// multiple editor instances on the same machine can experiment with
// collaborative sessions while a full backend service is under development.
class AgentCollaborationCoordinator {
 public:
  struct SessionInfo {
    std::string session_id;
    std::string session_name;
    std::vector<std::string> participants;
  };

  AgentCollaborationCoordinator();

  absl::StatusOr<SessionInfo> HostSession(const std::string& session_name);
  absl::StatusOr<SessionInfo> JoinSession(const std::string& session_code);
  absl::Status LeaveSession();
  absl::StatusOr<SessionInfo> RefreshSession();

  bool active() const { return active_; }
  const std::string& session_id() const { return session_id_; }
  const std::string& session_name() const { return session_name_; }

 private:
  struct SessionFileData {
    std::string session_name;
    std::string session_code;
    std::string host;
    std::vector<std::string> participants;
  };

  absl::Status EnsureDirectory() const;
  std::string LocalUserName() const;
  std::string NormalizeSessionCode(const std::string& input) const;
  std::string GenerateSessionCode() const;
  std::filesystem::path SessionsDirectory() const;
  std::filesystem::path SessionFilePath(const std::string& code) const;
  absl::StatusOr<SessionFileData> LoadSessionFile(
      const std::filesystem::path& path) const;
  absl::Status WriteSessionFile(const std::filesystem::path& path,
                                const SessionFileData& data) const;

  bool active_ = false;
  bool hosting_ = false;
  std::string session_id_;
  std::string session_name_;
  std::string local_user_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_COLLABORATION_COORDINATOR_H_
