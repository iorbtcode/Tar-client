#pragma once
#include <algorithm>
#include <cmath>

#include "gui/Theme.h"
#include "module/ModuleManager.h"

// List of enabled modules down the side of the screen
class ArrayList : public Module {
public:
    ArrayList() : Module("Array List", "Lists every enabled module on the side of the screen", Category::Hud, Icon::List) {
        side = addMode("Side", { "Right", "Left" });
        background = addBool("Background", true);
        rainbow = addBool("Rainbow", false);
        setEnabled(true);
    }

    void onRender(ImDrawList* draw) override {
        std::vector<Module*> shown;
        for (auto& m : ModuleManager::all())
            if (m->isEnabled() && m.get() != this) shown.push_back(m.get());

        ImFont* font = Theme::bold;
        float size = font->FontSize;
        auto width = [&](Module* m) { return font->CalcTextSizeA(size, FLT_MAX, 0.f, m->getName().c_str()).x; };
        std::sort(shown.begin(), shown.end(), [&](Module* a, Module* b) { return width(a) > width(b); });

        ImVec2 display = ImGui::GetIO().DisplaySize;
        const float padX = 6.f, lineH = size + 6.f;
        float y = 6.f;
        bool right = side->is("Right");

        for (size_t i = 0; i < shown.size(); i++) {
            float w = width(shown[i]) + padX * 2.f;
            float x = right ? display.x - w - 6.f : 6.f;
            ImU32 col = Theme::accentU32();
            if (rainbow->value) {
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(std::fmod(float(ImGui::GetTime()) * 0.15f + i * 0.05f, 1.f), 0.7f, 1.f, r, g, b);
                col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1.f));
            }
            if (background->value) draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + lineH), Theme::hudBackground);
            float barX = right ? x + w : x - 2.f;
            draw->AddRectFilled(ImVec2(barX, y), ImVec2(barX + 2.f, y + lineH), col);
            draw->AddText(font, size, ImVec2(x + padX, y + 3.f), col, shown[i]->getName().c_str());
            y += lineH;
        }
    }

private:
    ModeSetting* side;
    BoolSetting* background;
    BoolSetting* rainbow;
};
