#pragma once
#include "core/Client.h"
#include "module/Module.h"

// Darkens the game behind the click gui
class BackgroundDim : public Module {
public:
    BackgroundDim() : Module("Background Dim", "Darkens the game while the menu is open", Category::Render, Icon::Monitor) {
        opacity = addSlider("Opacity", 0.45f, 0.05f, 0.9f, 2);
        setEnabled(true);
    }

    void onRender(ImDrawList* draw) override {
        fade += ((Client::isMenuOpen() ? 1.f : 0.f) - fade) * ImMin(1.f, ImGui::GetIO().DeltaTime * 12.f);
        if (fade < 0.01f) return;
        draw->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, int(255 * opacity->value * fade)));
    }

private:
    SliderSetting* opacity;
    float fade = 0.f;
};
