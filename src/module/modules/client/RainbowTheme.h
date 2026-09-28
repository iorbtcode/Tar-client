#pragma once
#include <cmath>

#include "gui/Theme.h"
#include "module/Module.h"

// Cycles the accent color through the rainbow
class RainbowTheme : public Module {
public:
    RainbowTheme() : Module("Rainbow Theme", "Slowly cycles the accent color through every hue", Category::Client, Icon::Palette) {
        speed = addSlider("Speed", 0.1f, 0.02f, 1.f, 2);
        saturation = addSlider("Saturation", 0.75f, 0.f, 1.f, 2);
    }

    void onEnable() override { saved = Theme::accent; }
    void onDisable() override { Theme::accent = saved; }

    void onTick() override {
        float hue = std::fmod(float(ImGui::GetTime()) * speed->value, 1.f);
        ImGui::ColorConvertHSVtoRGB(hue, saturation->value, 0.9f, Theme::accent.x, Theme::accent.y, Theme::accent.z);
    }

private:
    SliderSetting* speed;
    SliderSetting* saturation;
    ImVec4 saved = Theme::accent;
};
