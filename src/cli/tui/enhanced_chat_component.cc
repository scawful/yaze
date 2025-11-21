#include "cli/tui/enhanced_chat_component.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace cli {

using namespace ftxui;

EnhancedChatComponent::EnhancedChatComponent(Rom* rom_context)
    : rom_context_(rom_context) {
  // Initialize agent service
  if (rom_context_) {
    agent_service_.SetRomContext(rom_context_);
  }

  // Initialize autocomplete
  autocomplete_engine_.RegisterCommand("/help", "Show help message");
  autocomplete_engine_.RegisterCommand("/exit", "Exit the chat");
  autocomplete_engine_.RegisterCommand("/clear", "Clear chat history");
  autocomplete_engine_.RegisterCommand("/rom_info", "Display ROM information");
  autocomplete_engine_.RegisterCommand("/status", "Show chat status");

  // Create components
  input_component_ = CreateInputComponent();
  history_component_ = CreateHistoryComponent();
  chat_container_ = CreateChatContainer();

  // Set up event handlers
  input_event_handler_ = [this](const Event& event) {
    return HandleInputEvents(event);
  };

  history_event_handler_ = [this](const Event& event) {
    return HandleHistoryEvents(event);
  };
}

Component EnhancedChatComponent::GetComponent() {
  return chat_container_;
}

void EnhancedChatComponent::SetRomContext(Rom* rom_context) {
  rom_context_ = rom_context;
  if (rom_context_) {
    agent_service_.SetRomContext(rom_context_);
  }
}

void EnhancedChatComponent::SendMessage(const std::string& message) {
  if (message.empty())
    return;

  ProcessMessage(message);
  input_message_.clear();
}

void EnhancedChatComponent::ClearHistory() {
  chat_history_.clear();
  selected_history_index_ = 0;
  UpdateHistoryDisplay();
}

void EnhancedChatComponent::ResetConversation() {
  agent_service_.ResetConversation();
  ClearHistory();
}

Component EnhancedChatComponent::CreateInputComponent() {
  auto input = Input(&input_message_, "Type your message...");

  auto send_button = Button("Send", [this] { SendMessage(input_message_); });

  auto clear_button = Button("Clear", [this] { ClearHistory(); });

  auto container = Container::Horizontal({input, send_button, clear_button});

  return CatchEvent(container, input_event_handler_);
}

Component EnhancedChatComponent::CreateHistoryComponent() {
  return Renderer([this] { return RenderHistoryArea(); });
}

Component EnhancedChatComponent::CreateChatContainer() {
  auto container = Container::Vertical({history_component_, input_component_});

  return Renderer(container, [this] {
    return vbox({text("🤖 AI Chat") | bold | center, separator(),
                 history_component_->Render() | flex | frame, separator(),
                 input_component_->Render(), separator(),
                 text("Commands: /help, /exit, /clear, /rom_info, /status") |
                     dim | center}) |
           border;
  });
}

bool EnhancedChatComponent::HandleInputEvents(const Event& event) {
  if (event == Event::Return) {
    if (input_message_.empty())
      return true;

    SendMessage(input_message_);
    return true;
  }

  if (event == Event::Character('q')) {
    // Exit chat or go back
    return true;
  }

  return false;
}

bool EnhancedChatComponent::HandleHistoryEvents(const Event& event) {
  if (event == Event::ArrowUp) {
    if (selected_history_index_ > 0) {
      selected_history_index_--;
    }
    return true;
  }

  if (event == Event::ArrowDown) {
    if (selected_history_index_ < static_cast<int>(chat_history_.size()) - 1) {
      selected_history_index_++;
    }
    return true;
  }

  return false;
}

void EnhancedChatComponent::ProcessMessage(const std::string& message) {
  // Handle special commands
  if (message == "/exit") {
    // Signal to parent component to exit
    return;
  }

  if (message == "/clear") {
    ClearHistory();
    return;
  }

  if (message == "/help") {
    AddMessageToHistory(
        "System",
        "Available commands: /help, /exit, /clear, /rom_info, /status");
    return;
  }

  if (message == "/rom_info") {
    if (rom_context_) {
      AddMessageToHistory("System", absl::StrFormat("ROM: %s | Size: %d bytes",
                                                    rom_context_->title(),
                                                    rom_context_->size()));
    } else {
      AddMessageToHistory("System", "No ROM loaded");
    }
    return;
  }

  if (message == "/status") {
    AddMessageToHistory(
        "System",
        absl::StrFormat("Chat Status: %d messages, %s", chat_history_.size(),
                        focused_ ? "Focused" : "Not focused"));
    return;
  }

  // Add user message to history
  AddMessageToHistory("You", message);

  // Send to agent service
  auto response = agent_service_.SendMessage(message);
  if (response.ok()) {
    // Agent response will be handled by the service
    // For now, add a placeholder response
    AddMessageToHistory("Agent", "Response received (integration pending)");
  } else {
    AddMessageToHistory("System",
                        absl::StrCat("Error: ", response.status().message()));
  }

  UpdateHistoryDisplay();
}

void EnhancedChatComponent::AddMessageToHistory(const std::string& sender,
                                                const std::string& message) {
  chat_history_.push_back({sender, message});

  // Limit history size
  if (static_cast<int>(chat_history_.size()) > max_history_lines_) {
    chat_history_.erase(chat_history_.begin());
  }

  // Auto-scroll to bottom
  selected_history_index_ = chat_history_.size() - 1;
}

void EnhancedChatComponent::UpdateHistoryDisplay() {
  // Trigger re-render of history component
  // This is handled automatically by FTXUI
}

Element EnhancedChatComponent::RenderChatMessage(const std::string& sender,
                                                 const std::string& message) {
  Color sender_color;
  if (sender == "You") {
    sender_color = Color::Yellow;
  } else if (sender == "Agent") {
    sender_color = Color::Green;
  } else {
    sender_color = Color::Cyan;
  }

  return hbox({text(sender) | bold | color(sender_color), text(": "),
               text(message) | flex});
}

Element EnhancedChatComponent::RenderInputArea() {
  return hbox({text("You: ") | bold, input_component_->Render() | flex});
}

Element EnhancedChatComponent::RenderHistoryArea() {
  if (chat_history_.empty()) {
    return vbox({text("No messages yet. Start chatting!") | dim | center}) |
           flex | center;
  }

  std::vector<Element> messages;
  for (const auto& [sender, message] : chat_history_) {
    messages.push_back(RenderChatMessage(sender, message));
  }

  return vbox(messages) | flex;
}

}  // namespace cli
}  // namespace yaze
