#pragma once
#include "module/Module.h"

// Draws your own crosshair in the middle of the screen
class Crosshair : public Module {
public:
    Crosshair() : Module("Crosshair", "Draws a custom crosshair in the middle of the screen", Category::Render, Icon::Crosshair) {
        length = addSlider("Length", 7.f, 2.f, 25.f, 0);
        gap = addSlider("Gap", 3.f, 0.f, 15.f, 0);
        thickness = addSlider("Thickness", 2.f, 1.f, 6.f, 0);
        dot = addBool("Center dot", false);
        outline = addBool("Outline", true);
        color = addColor("Color", 1.f, 1.f, 1.f, 1.f);
    }

    void onRender(ImDrawList* draw) override {
        ImVec2 c(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        float g = gap->value, l = length->value, t = thickness->value;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(color->value[0], color->value[1], color->value[2], color->value[3]));

        const ImVec2 arms[4][2] = {
            { ImVec2(c.x - g - l, c.y), ImVec2(c.x - g, c.y) },
            { ImVec2(c.x + g, c.y), ImVec2(c.x + g + l, c.y) },
            { ImVec2(c.x, c.y - g - l), ImVec2(c.x, c.y - g) },
            { ImVec2(c.x, c.y + g), ImVec2(c.x, c.y + g + l) },
        };
        for (auto& arm : arms) {
            ImVec2 a(arm[0].x - t * 0.5f, arm[0].y - t * 0.5f);
            ImVec2 b(arm[1].x + t * 0.5f, arm[1].y + t * 0.5f);
            if (outline->value) draw->AddRectFilled(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), IM_COL32(0, 0, 0, 200));
            draw->AddRectFilled(a, b, col);
        }
        if (dot->value) {
            if (outline->value) draw->AddCircleFilled(c, t + 1.f, IM_COL32(0, 0, 0, 200));
            draw->AddCircleFilled(c, t, col);
        }
    }

private:
    SliderSetting* length;
    SliderSetting* gap;
    SliderSetting* thickness;
    BoolSetting* dot;
    BoolSetting* outline;
    ColorSetting* color;
};
