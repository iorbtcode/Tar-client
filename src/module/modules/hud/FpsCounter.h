#pragma once
#include <cstdio>

#include "module/HudModule.h"

// Shows your frames per second
class FpsCounter : public HudModule {
public:
    FpsCounter() : HudModule("FPS Counter", "Shows your frames per second", Category::Hud, Icon::Gauge, 10.f, 50.f) {
        background = addBool("Background", true);
    }

    ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) override {
        char value[16];
        std::snprintf(value, sizeof(value), "%d", int(ImGui::GetIO().Framerate + 0.5f));
        return HudStyle::textBox(draw, pos, "FPS", value, background->value);
    }

private:
    BoolSetting* background;
};
