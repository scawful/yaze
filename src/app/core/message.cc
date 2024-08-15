#include "message.h"

namespace yaze {
namespace app {
namespace core {

#pragma mark - MessageDispatcher

void MessageDispatcher::RegisterListener(const std::string& message_type,
                                         IMessageListener* listener) {
  listeners_[message_type].push_back(listener);
}

void MessageDispatcher::UnregisterListener(const std::string& message_type,
                                           IMessageListener* listener) {
  auto& listener_list = listeners_[message_type];
  listener_list.erase(
      std::remove(listener_list.begin(), listener_list.end(), listener),
      listener_list.end());
}

void MessageDispatcher::RegisterProtocol(IMessageProtocol* protocol) {
  protocols_.push_back(protocol);
}

void MessageDispatcher::RegisterFilteredListener(
    const std::string& message_type, IMessageListener* listener,
    std::unique_ptr<MessageFilter> filter) {
  filtered_listeners_[message_type].push_back({listener, std::move(filter)});
}

void MessageDispatcher::BindHandler(const std::string& message_type,
                                    MessageHandler handler) {
  handlers_[message_type].push_back(handler);
}

void MessageDispatcher::SendMessage(const Message& message) {
  const auto& listener_list = listeners_[message.type];
  for (auto listener : listener_list) {
    listener->OnMessageReceived(message);
  }
}

void MessageDispatcher::DispatchMessage(const Message& message) {
  for (auto protocol : protocols_) {
    if (protocol->CanHandleMessage(message)) {
      return;
    }
  }

  const auto& listener_list = listeners_[message.type];
  for (auto listener : listener_list) {
    listener->OnMessageReceived(message);
  }

  const auto& filtered_listener_list = filtered_listeners_[message.type];
  for (auto& listener : filtered_listener_list) {
    if (listener.filter->ShouldReceiveMessage(message)) {
      listener.listener->OnMessageReceived(message);
    }
  }

  const auto& handler_list = handlers_[message.type];
  for (auto& handler : handler_list) {
    handler(message);
  }
}

#pragma mark - AsyncMessageDispatcher

void AsyncMessageDispatcher::Start() {
  // Start a new thread and run the message loop.
  
}

void AsyncMessageDispatcher::Stop() {
  // Stop the message loop and join the thread.
}

void AsyncMessageDispatcher::EnqueueMessage(const Message& message) {
  // Enqueue a message to the message loop.
}

void AsyncMessageDispatcher::DispatchLoop() {
  // Dispatch messages in a loop.
}

#pragma mark - MessageFilter

template <typename T>
void Swizzler::Swizzle(T* instance, void (T::*original_method)(),
                       std::function<void()> new_method) {
  original_methods_[instance] = original_method;
  swizzled_methods_[instance] = new_method;
}

template <typename T>
void Swizzler::CallOriginal(T* instance) {
  auto it = original_methods_.find(instance);
  if (it != original_methods_.end()) {
    (instance->*(it->second))();
  }
}

template <typename T>
void Swizzler::CallSwizzled(T* instance) {
  auto it = swizzled_methods_.find(instance);
  if (it != swizzled_methods_.end()) {
    it->second();
  }
}

#pragma mark - ObjectFactory

template <typename T>
void ObjectFactory::RegisterType(const std::string& type_name) {
  creators_[type_name] = []() { return std::make_unique<T>(); };
}

std::unique_ptr<Reflectable> ObjectFactory::CreateObject(
    const std::string& object_name) const {
  auto it = creators_.find(object_name);
  if (it != creators_.end()) {
    return it->second();
  }
  return nullptr;
}

}  // namespace core
}  // namespace app
}  // namespace yaze