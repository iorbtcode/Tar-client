#pragma once
#include <cstdio>

#include "core/Client.h"
#include "module/HudModule.h"

// How long you've been playing since injecting
class SessionTimer : public HudModule {
public:
    SessionTimer() : HudModule("Session Timer", "Shows how long you've been playing this session", Category::Utility,
                               Icon::Timer, 10.f, 314.f) {
        background = addBool("Background", true);
    }

    void onEnable() override { start = Client::uptime(); }

    ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) override {
        int secs = int(Client::uptime() - start);
        char value[32];
        std::snprintf(value, sizeof(value), "%02d:%02d:%02d", secs / 3600, secs / 60 % 60, secs % 60);
        return HudStyle::textBox(draw, pos, "Session", value, background->value);
    }

private:
    BoolSetting* background;
    double start = 0.0;
};
