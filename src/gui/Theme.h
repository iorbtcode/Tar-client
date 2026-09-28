#pragma once
#include <imgui.h>

namespace Theme {
    // main accent color, changed from the settings page (or the Rainbow Theme module)
    inline ImVec4 accent = ImVec4(0.12f, 0.80f, 0.52f, 1.f);

    inline ImU32 accentU32(float alphaMul = 1.f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(accent.x, accent.y, accent.z, accent.w * alphaMul));
    }

    // fixed palette (matches the dark look of the menu)
    constexpr ImU32 windowBg      = IM_COL32(18, 19, 20, 250);
    constexpr ImU32 windowBorder  = IM_COL32(40, 42, 44, 255);
    constexpr ImU32 rowBg         = IM_COL32(29, 30, 32, 255);
    constexpr ImU32 rowBgHover    = IM_COL32(35, 37, 39, 255);
    constexpr ImU32 panelBg       = IM_COL32(24, 25, 27, 255);
    constexpr ImU32 text          = IM_COL32(232, 234, 236, 255);
    constexpr ImU32 textDim       = IM_COL32(150, 154, 158, 255);
    constexpr ImU32 textDark      = IM_COL32(16, 18, 18, 255);
    constexpr ImU32 toggleOff     = IM_COL32(58, 61, 64, 255);
    constexpr ImU32 danger        = IM_COL32(230, 72, 72, 255);
    constexpr ImU32 hudBackground = IM_COL32(14, 15, 16, 190);

    inline ImFont* regular = nullptr; // ui text
    inline ImFont* bold = nullptr;    // module names, hud
    inline ImFont* large = nullptr;   // headings

    void loadFonts();
    void applyStyle();
}
