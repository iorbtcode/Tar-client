#include "util/Keys.h"

#include <windows.h>

std::string Keys::name(int vk) {
    switch (vk) {
    case 0: return "None";
    case VK_LBUTTON: return "Mouse 1";
    case VK_RBUTTON: return "Mouse 2";
    case VK_MBUTTON: return "Mouse 3";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_RSHIFT: return "Right Shift";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_RMENU: return "Right Alt";
    default: break;
    }

    UINT scan = MapVirtualKeyA(UINT(vk), MAPVK_VK_TO_VSC);
    char buf[64] = {};
    if (scan && GetKeyNameTextA(LONG(scan << 16), buf, sizeof(buf)) > 0) return buf;
    return "Key " + std::to_string(vk);
}
