#pragma once
#include <imgui_internal.h>

#include "core/Client.h"
#include "gui/Theme.h"
#include "module/HudModule.h"

// WASD / space / mouse buttons that light up when you press them
class Keystrokes : public HudModule {
public:
    Keystrokes() : HudModule("Keystrokes", "Shows which movement keys and mouse buttons you're pressing", Category::Hud,
                             Icon::Keyboard, 10.f, 130.f) {
        keySize = addSlider("Key size", 34.f, 24.f, 60.f, 0);
        showMouse = addBool("Show mouse", true);
        showSpace = addBool("Show space", true);
    }

    ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) override {
        const float k = keySize->value, gap = 4.f;
        const float full = k * 3.f + gap * 2.f;

        key(draw, pos + ImVec2(k + gap, 0.f), ImVec2(k, k), "W", 'W');
        key(draw, pos + ImVec2(0.f, k + gap), ImVec2(k, k), "A", 'A');
        key(draw, pos + ImVec2(k + gap, k + gap), ImVec2(k, k), "S", 'S');
        key(draw, pos + ImVec2((k + gap) * 2.f, k + gap), ImVec2(k, k), "D", 'D');
        float y = (k + gap) * 2.f;

        if (showMouse->value) {
            float half = (full - gap) * 0.5f;
            key(draw, pos + ImVec2(0.f, y), ImVec2(half, k * 0.8f), "LMB", VK_LBUTTON);
            key(draw, pos + ImVec2(half + gap, y), ImVec2(half, k * 0.8f), "RMB", VK_RBUTTON);
            y += k * 0.8f + gap;
        }
        if (showSpace->value) {
            key(draw, pos + ImVec2(0.f, y), ImVec2(full, k * 0.6f), "", VK_SPACE);
            float barW = full * 0.35f;
            ImVec2 c = pos + ImVec2(full * 0.5f, y + k * 0.3f);
            draw->AddLine(ImVec2(c.x - barW * 0.5f, c.y), ImVec2(c.x + barW * 0.5f, c.y),
                          Client::isKeyDown(VK_SPACE) ? Theme::textDark : Theme::text, 2.f);
            y += k * 0.6f + gap;
        }
        return ImVec2(full, y - gap);
    }

private:
    SliderSetting* keySize;
    BoolSetting* showMouse;
    BoolSetting* showSpace;
    float anim[256] = {};

    void key(ImDrawList* draw, ImVec2 pos, ImVec2 size, const char* label, int vk) {
        float& a = anim[vk];
        a = ImLerp(a, Client::isKeyDown(vk) ? 1.f : 0.f, ImMin(1.f, ImGui::GetIO().DeltaTime * 18.f));

        ImVec4 bg = ImLerp(ImGui::ColorConvertU32ToFloat4(Theme::hudBackground), Theme::accent, a);
        draw->AddRectFilled(pos, pos + size, ImGui::ColorConvertFloat4ToU32(bg), 4.f);

        ImU32 textCol = a > 0.5f ? Theme::textDark : Theme::text;
        ImVec2 ts = Theme::bold->CalcTextSizeA(Theme::bold->FontSize, FLT_MAX, 0.f, label);
        draw->AddText(Theme::bold, Theme::bold->FontSize, pos + (size - ts) * 0.5f, textCol, label);
    }
};
