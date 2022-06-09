#include "Event.h"

namespace yaze {
namespace Application {
namespace Events {

void Event::Assign(const std::function<void()>& event) { event_ = event; }
void Event::Trigger() const { event_(); }

}  // namespace Events
}  // namespace Application
}  // namespace yaze