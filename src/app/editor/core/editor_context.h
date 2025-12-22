#ifndef YAZE_APP_EDITOR_CORE_EDITOR_CONTEXT_H_
#define YAZE_APP_EDITOR_CORE_EDITOR_CONTEXT_H_

#include "app/editor/core/event_bus.h"

namespace yaze {
class Rom;

namespace editor {

class GlobalEditorContext {
public:
    explicit GlobalEditorContext(EventBus& bus) : bus_(bus) {}

    EventBus& GetEventBus() { return bus_; }
    const EventBus& GetEventBus() const { return bus_; }

    void SetCurrentRom(Rom* rom) { rom_ = rom; }
    Rom* GetCurrentRom() const { return rom_; }

    void SetSessionId(size_t id) { session_id_ = id; }
    size_t GetSessionId() const { return session_id_; }

private:
    EventBus& bus_;
    Rom* rom_ = nullptr; 
    size_t session_id_ = 0;
};

} // namespace editor
} // namespace yaze

#endif // YAZE_APP_EDITOR_CORE_EDITOR_CONTEXT_H_