#ifndef YAZE_APPLICATION_EVENTS_EVENT_H
#define YAZE_APPLICATION_EVENTS_EVENT_H

#include <functional>

namespace yaze {
namespace Application {
namespace Events {

class Event {
 public:
  void Assign(const std::function<void()> & event);
  void Trigger() const;

 private:
  std::function<void()> event_;
};

}  // namespace Events
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_EVENTS_EVENT_H