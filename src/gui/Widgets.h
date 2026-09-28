#pragma once
#include <imgui.h>

#include "module/Setting.h"

// custom widgets used by the click gui, all drawn with the imgui draw list
namespace Widgets {
    // animated on/off switch at the current cursor, returns true when clicked
    bool toggle(const char* id, bool* value);
    // round icon button at the current cursor, returns true when clicked
    bool iconButton(const char* id, int icon, float size, bool active = false);
    // flat button filling the given width
    bool button(const char* label, float width, ImU32 color);
    // keybind button, returns true when it wants to start listening
    bool keybind(const char* label, int key, bool listening, float width);

    // draw a module setting row, width is the space available
    void setting(Setting* setting, float width);
}
