#ifndef YAZE_APP_CORE_MESSAGE_H
#define YAZE_APP_CORE_MESSAGE_H

#include <any>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace app {
namespace core {

struct Message {
  std::string type;
  void* sender;
  std::any payload;

  Message() = default;
  Message(const std::string& type, void* sender, const std::any& payload)
      : type(type), sender(sender), payload(payload) {}
};

class IMessageListener {
 public:
  virtual ~IMessageListener() = default;
  virtual absl::Status OnMessageReceived(const Message& message) = 0;
};

class IMessageProtocol {
 public:
  virtual ~IMessageProtocol() = default;
  virtual bool CanHandleMessage(const Message& message) const = 0;
};

class MessageFilter {
 public:
  virtual ~MessageFilter() = default;
  virtual bool ShouldReceiveMessage(const Message& message) const = 0;
};

using MessageHandler = std::function<void(const Message&)>;

class MessageDispatcher {
 public:
  void RegisterListener(const std::string& message_type,
                        IMessageListener* listener);
  void UnregisterListener(const std::string& message_type,
                          IMessageListener* listener);
  void RegisterProtocol(IMessageProtocol* protocol);
  void RegisterFilteredListener(const std::string& message_type,
                                IMessageListener* listener,
                                std::unique_ptr<MessageFilter> filter);
  void BindHandler(const std::string& message_type, MessageHandler handler);
  void SendMessage(const Message& message);
  void DispatchMessage(const Message& message);

 private:
  struct ListenerWithFilter {
    IMessageListener* listener;
    std::unique_ptr<MessageFilter> filter;
  };
  std::unordered_map<std::string, std::vector<IMessageListener*>> listeners_;
  std::unordered_map<std::string, std::vector<ListenerWithFilter>>
      filtered_listeners_;
  std::unordered_map<std::string, std::vector<MessageHandler>> handlers_;
  std::vector<IMessageProtocol*> protocols_;
};

class AsyncMessageDispatcher : public MessageDispatcher {
 public:
  void Start();
  void Stop();
  void EnqueueMessage(const Message& message);

 private:
  void DispatchLoop();
  std::queue<Message> messageQueue_;
  std::mutex queueMutex_;
  std::thread dispatchThread_;
  bool running_ = false;
};

class Swizzler {
 public:
  template <typename T>
  void Swizzle(T* instance, void (T::*original_method)(),
               std::function<void()> new_method);

  template <typename T>
  void CallOriginal(T* instance);

  template <typename T>
  void CallSwizzled(T* instance);

 private:
  std::unordered_map<void*, std::function<void()>> swizzled_methods_;
  std::unordered_map<void*, void*> original_methods_;
};

class Reflectable {
 public:
  virtual ~Reflectable() = default;
  virtual std::string GetTypeName() const = 0;
  virtual std::vector<std::string> GetPropertyNames() const = 0;
  virtual std::any GetPropertyValue(const std::string& property_name) const = 0;
  virtual void SetPropertyValue(const std::string& property_name,
                                const std::any& value) = 0;
  virtual std::any InvokeMethod(const std::string& method_name,
                                const std::vector<std::any>& args) = 0;
};

class ObjectFactory {
 public:
  template <typename T>
  void RegisterType(const std::string& type_name);

  std::unique_ptr<Reflectable> CreateObject(const std::string& type_name) const;

 private:
  std::unordered_map<std::string, std::function<std::unique_ptr<Reflectable>()>>
      creators_;
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_MESSAGE_H