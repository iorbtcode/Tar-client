#pragma once
#include <imgui.h>

// Small line icons drawn with the imgui draw list, so no icon font is needed.
// Add a new one by adding an entry here and a case in Icons.cpp.
enum class Icon {
    None,
    Home, Search, Refresh, Gear, Users, Minus, Close, ChevronDown,
    Tag, List, Gauge, Mouse, Keyboard, Crosshair, Monitor, Clock, Timer, Bell, Palette, Eye,
};

namespace Icons {
    // size is the width/height of the box the icon fits in
    void draw(ImDrawList* draw, Icon icon, ImVec2 center, float size, ImU32 color, float thickness = 1.5f);
}
