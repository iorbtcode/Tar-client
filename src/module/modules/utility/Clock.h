#pragma once
#include <ctime>

#include "module/HudModule.h"

// Your local time on screen
class Clock : public HudModule {
public:
    Clock() : HudModule("Clock", "Shows your local time", Category::Utility, Icon::Clock, 10.f, 280.f) {
        format = addMode("Format", { "24 hour", "12 hour" });
        seconds = addBool("Seconds", false);
        background = addBool("Background", true);
    }

    ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) override {
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);

        const char* fmt = format->is("24 hour") ? (seconds->value ? "%H:%M:%S" : "%H:%M")
                                                 : (seconds->value ? "%I:%M:%S %p" : "%I:%M %p");
        char value[32];
        std::strftime(value, sizeof(value), fmt, &local);
        return HudStyle::textBox(draw, pos, "Time", value, background->value);
    }

private:
    ModeSetting* format;
    BoolSetting* seconds;
    BoolSetting* background;
};
