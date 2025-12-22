#ifndef YAZE_APP_EDITOR_CORE_EVENT_BUS_H_
#define YAZE_APP_EDITOR_CORE_EVENT_BUS_H_

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <algorithm>

namespace yaze {

struct Event {
    virtual ~Event() = default;
};

class EventBus {
public:
    using HandlerId = size_t;

    template<typename T>
    HandlerId Subscribe(std::function<void(const T&)> handler) {
        static_assert(std::is_base_of<Event, T>::value, "T must derive from Event");
        auto type_idx = std::type_index(typeid(T));
        auto wrapper = [handler](const Event& e) {
            handler(static_cast<const T&>(e));
        };
        
        size_t id = next_id_++;
        handlers_[type_idx].push_back({id, wrapper});
        return id;
    }

    template<typename T>
    void Publish(const T& event) {
        static_assert(std::is_base_of<Event, T>::value, "T must derive from Event");
        auto type_idx = std::type_index(typeid(T));
        if (handlers_.find(type_idx) != handlers_.end()) {
            for (const auto& handler : handlers_[type_idx]) {
                handler.fn(event);
            }
        }
    }

    void Unsubscribe(HandlerId id) {
        for (auto& [type, list] : handlers_) {
            auto it = std::remove_if(list.begin(), list.end(), 
                [id](const HandlerEntry& entry) { return entry.id == id; });
            if (it != list.end()) {
                list.erase(it, list.end());
                return; 
            }
        }
    }

private:
    struct HandlerEntry {
        HandlerId id;
        std::function<void(const Event&)> fn;
    };

    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
    HandlerId next_id_ = 1;
};

} // namespace yaze

#endif // YAZE_APP_EDITOR_CORE_EVENT_BUS_H_
