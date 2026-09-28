#pragma once
#include <string>

#include "module/Module.h"

// Little toasts in the bottom right. Other code calls Notifications::push(...)
class Notifications : public Module {
public:
    Notifications();

    // positive picks the accent color, otherwise red. always = show even if this module is off
    static void push(const std::string& text, bool positive = true, bool always = false);
    static void renderToasts(ImDrawList* draw);
    static inline bool muted = false;

private:
    SliderSetting* duration;
};
