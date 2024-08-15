#ifndef YAZE_APP_CORE_NOTIFICATION_H
#define YAZE_APP_CORE_NOTIFICATION_H

#include <any>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/core/message.h"

namespace yaze {
namespace app {
namespace core {

struct Notification : public Message {
  Notification(const std::string& type, void* sender, std::any payload)
      : Message{type, sender, payload} {}
};

class NotificationCenter {
 public:
  void AddObserver(const std::string& notificationType,
                   IMessageListener* observer) {
    observers[notificationType].push_back(observer);
  }

  void RemoveObserver(const std::string& notificationType,
                      IMessageListener* observer) {
    auto& observerList = observers[notificationType];
    observerList.erase(
        std::remove(observerList.begin(), observerList.end(), observer),
        observerList.end());
  }

  void PostNotification(const Notification& notification) {
    const auto& observerList = observers[notification.type];
    for (auto observer : observerList) {
      observer->onMessageReceived(notification);
    }
  }

 private:
  std::unordered_map<std::string, std::vector<IMessageListener*>> observers;
};

}  // namespace core
}  // namespace app
}  // namespace yaze