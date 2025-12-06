#ifndef YAZE_APP_EDITOR_EVENTS_UI_EVENTS_H_
#define YAZE_APP_EDITOR_EVENTS_UI_EVENTS_H_

#include "app/editor/core/event_bus.h"
#include <string>

namespace yaze::editor {

struct StatusUpdateEvent : public Event {
    enum class Type {
        Cursor,
        Selection,
        Zoom,
        Mode,
        Message,
        Clear
    };

    Type type;
    std::string text; // For Mode, Message
    int x = 0, y = 0; // For Cursor
    int count = 0, width = 0, height = 0; // For Selection
    float zoom = 1.0f; // For Zoom
    std::string key; // For Custom Segments
    
    // Helpers for construction
    static StatusUpdateEvent Cursor(int x, int y, const std::string& label = "Pos") {
        StatusUpdateEvent e;
        e.type = Type::Cursor;
        e.x = x; e.y = y;
        e.text = label;
        return e;
    }

    static StatusUpdateEvent Selection(int count, int width = 0, int height = 0) {
        StatusUpdateEvent e;
        e.type = Type::Selection;
        e.count = count;
        e.width = width;
        e.height = height;
        return e;
    }

    static StatusUpdateEvent ClearAll() {
        StatusUpdateEvent e;
        e.type = Type::Clear;
        return e;
    }
};

struct PanelToggleEvent : public Event {
    std::string panel_id;
    bool visible;
};

} // namespace yaze::editor

#endif // YAZE_APP_EDITOR_EVENTS_UI_EVENTS_H_
