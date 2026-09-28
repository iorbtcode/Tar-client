#pragma once
#include <cstdio>
#include <deque>

#include "module/HudModule.h"

// Counts your own clicks per second
class CpsCounter : public HudModule {
public:
    CpsCounter() : HudModule("CPS Counter", "Shows how many times you click per second", Category::Hud, Icon::Mouse, 10.f, 84.f) {
        showRight = addBool("Show right click", true);
        background = addBool("Background", true);
    }

    void onMouse(int button, bool down) override {
        if (!down) return;
        if (button == 0) left.push_back(ImGui::GetTime());
        if (button == 1) right.push_back(ImGui::GetTime());
    }

    ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) override {
        double now = ImGui::GetTime();
        while (!left.empty() && now - left.front() > 1.0) left.pop_front();
        while (!right.empty() && now - right.front() > 1.0) right.pop_front();

        char value[32];
        if (showRight->value) std::snprintf(value, sizeof(value), "%d | %d", int(left.size()), int(right.size()));
        else std::snprintf(value, sizeof(value), "%d", int(left.size()));
        return HudStyle::textBox(draw, pos, "CPS", value, background->value);
    }

private:
    BoolSetting* showRight;
    BoolSetting* background;
    std::deque<double> left, right;
};
