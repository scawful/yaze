#include "app/editor/agent/agent_session.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace editor {

AgentSessionManager::AgentSessionManager() {
  // Create a default session on startup
  CreateSession("Agent 1");
}

std::string AgentSessionManager::CreateSession(const std::string& name) {
  AgentSession session;
  session.agent_id = GenerateAgentId();
  session.display_name =
      name.empty() ? absl::StrFormat("Agent %d", next_session_number_++) : name;
  session.is_active = sessions_.empty();  // First session is active by default

  sessions_.push_back(std::move(session));

  // If this is the first session, set it as active
  if (sessions_.size() == 1) {
    active_session_id_ = sessions_.back().agent_id;
  }

  LOG_INFO("AgentSessionManager", "Created session '%s' with ID '%s'",
           sessions_.back().display_name.c_str(),
           sessions_.back().agent_id.c_str());

  if (on_session_created_) {
    on_session_created_(sessions_.back().agent_id);
  }

  return sessions_.back().agent_id;
}

void AgentSessionManager::CloseSession(const std::string& agent_id) {
  int index = FindSessionIndex(agent_id);
  if (index < 0) {
    LOG_WARN("AgentSessionManager", "Attempted to close unknown session: %s",
             agent_id.c_str());
    return;
  }

  // Close card if open
  if (sessions_[index].has_card_open) {
    ClosePanelForSession(agent_id);
  }

  bool was_active = (active_session_id_ == agent_id);

  // Remove the session
  sessions_.erase(sessions_.begin() + index);

  // If we removed the active session, activate another
  if (was_active && !sessions_.empty()) {
    // Activate the previous session, or the first one
    int new_active_index = std::max(0, index - 1);
    active_session_id_ = sessions_[new_active_index].agent_id;
    sessions_[new_active_index].is_active = true;
  } else if (sessions_.empty()) {
    active_session_id_.clear();
  }

  LOG_INFO("AgentSessionManager", "Closed session: %s", agent_id.c_str());

  if (on_session_closed_) {
    on_session_closed_(agent_id);
  }
}

void AgentSessionManager::RenameSession(const std::string& agent_id,
                                        const std::string& new_name) {
  AgentSession* session = GetSession(agent_id);
  if (session) {
    session->display_name = new_name;
    LOG_INFO("AgentSessionManager", "Renamed session %s to '%s'",
             agent_id.c_str(), new_name.c_str());
  }
}

AgentSession* AgentSessionManager::GetActiveSession() {
  if (active_session_id_.empty()) {
    return nullptr;
  }
  return GetSession(active_session_id_);
}

const AgentSession* AgentSessionManager::GetActiveSession() const {
  if (active_session_id_.empty()) {
    return nullptr;
  }
  return GetSession(active_session_id_);
}

void AgentSessionManager::SetActiveSession(const std::string& agent_id) {
  // Deactivate current
  if (auto* current = GetSession(active_session_id_)) {
    current->is_active = false;
  }

  // Activate new
  if (auto* new_session = GetSession(agent_id)) {
    new_session->is_active = true;
    active_session_id_ = agent_id;
    LOG_DEBUG("AgentSessionManager", "Switched to session: %s (%s)",
              new_session->display_name.c_str(), agent_id.c_str());
  }
}

AgentSession* AgentSessionManager::GetSession(const std::string& agent_id) {
  for (auto& session : sessions_) {
    if (session.agent_id == agent_id) {
      return &session;
    }
  }
  return nullptr;
}

const AgentSession* AgentSessionManager::GetSession(
    const std::string& agent_id) const {
  for (const auto& session : sessions_) {
    if (session.agent_id == agent_id) {
      return &session;
    }
  }
  return nullptr;
}

void AgentSessionManager::OpenPanelForSession(const std::string& agent_id) {
  AgentSession* session = GetSession(agent_id);
  if (!session) {
    LOG_WARN("AgentSessionManager",
             "Attempted to open card for unknown session: %s",
             agent_id.c_str());
    return;
  }

  if (session->has_card_open) {
    LOG_DEBUG("AgentSessionManager", "Panel already open for session: %s",
              agent_id.c_str());
    return;
  }

  session->has_card_open = true;
  LOG_INFO("AgentSessionManager", "Opened card for session: %s (%s)",
           session->display_name.c_str(), agent_id.c_str());

  if (on_card_opened_) {
    on_card_opened_(agent_id);
  }
}

void AgentSessionManager::ClosePanelForSession(const std::string& agent_id) {
  AgentSession* session = GetSession(agent_id);
  if (!session) {
    return;
  }

  if (!session->has_card_open) {
    return;
  }

  session->has_card_open = false;
  LOG_INFO("AgentSessionManager", "Closed card for session: %s",
           agent_id.c_str());

  if (on_card_closed_) {
    on_card_closed_(agent_id);
  }
}

bool AgentSessionManager::IsPanelOpenForSession(
    const std::string& agent_id) const {
  const AgentSession* session = GetSession(agent_id);
  return session ? session->has_card_open : false;
}

std::vector<std::string> AgentSessionManager::GetOpenPanelSessionIds() const {
  std::vector<std::string> result;
  for (const auto& session : sessions_) {
    if (session.has_card_open) {
      result.push_back(session.agent_id);
    }
  }
  return result;
}

std::string AgentSessionManager::GenerateAgentId() {
  static int id_counter = 0;
  return absl::StrFormat("agent_%d", ++id_counter);
}

int AgentSessionManager::FindSessionIndex(const std::string& agent_id) const {
  for (size_t i = 0; i < sessions_.size(); ++i) {
    if (sessions_[i].agent_id == agent_id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace editor
}  // namespace yaze
