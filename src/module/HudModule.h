#pragma once
#include "module/Module.h"

// A module that draws a box somewhere on screen (fps counter, clock, ...).
// While the click gui is open you can drag it around, and the position is saved to the config.
// Override drawHud instead of onRender.
class HudModule : public Module {
public:
    HudModule(std::string name, std::string description, Category category, Icon icon, float defX, float defY);

    // draw the element with its top-left corner at pos and return how big it was
    virtual ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) = 0;

    void onRender(ImDrawList* draw) final;

protected:
    SliderSetting* posX;
    SliderSetting* posY;

private:
    bool dragging = false;
    ImVec2 dragOffset{};
    ImVec2 lastSize{};
};

// shared look for simple "label: value" hud boxes
namespace HudStyle {
    ImVec2 textBox(ImDrawList* draw, ImVec2 pos, const char* label, const char* value, bool background = true);
}
