#pragma once
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "gui/Icons.h"
#include "module/Setting.h"

enum class Category { Hud, Render, Utility, Client, Count };

inline const char* categoryName(Category c) {
    static const char* names[] = { "HUD", "Render", "Utility", "Client" };
    return names[int(c)];
}

// Base class for every module. See MODULES.txt in the repo root for a full walkthrough.
class Module {
public:
    Module(std::string name, std::string description, Category category, Icon icon, int key = 0);
    virtual ~Module() = default;

    // ---- events, override whichever ones you need ----
    virtual void onEnable() {}
    virtual void onDisable() {}
    // every frame while enabled, before anything is drawn
    virtual void onTick() {}
    // every frame while enabled, draw your overlay here (drawn on top of the game)
    virtual void onRender(ImDrawList* draw) {}
    // raw key / mouse button events from the game window while enabled.
    // vk is a windows virtual key code (VK_SPACE, 'W', ...)
    virtual void onKey(int vk, bool down) {}
    // button: 0 left, 1 right, 2 middle
    virtual void onMouse(int button, bool down) {}

    // ---- state ----
    void setEnabled(bool enabled);
    void toggle() { setEnabled(!enabled); }
    bool isEnabled() const { return enabled; }

    const std::string& getName() const { return name; }
    const std::string& getDescription() const { return description; }
    Category getCategory() const { return category; }
    Icon getIcon() const { return icon; }

    int key; // toggle keybind, 0 = none
    std::vector<std::unique_ptr<Setting>>& getSettings() { return settings; }
    Setting* findSetting(const std::string& settingName);

    // gui animation state, not saved
    float expandAnim = 0.f;
    bool expanded = false;

protected:
    BoolSetting* addBool(std::string settingName, bool def);
    SliderSetting* addSlider(std::string settingName, float def, float min, float max, int decimals = 1);
    ModeSetting* addMode(std::string settingName, std::vector<std::string> modes, int def = 0);
    ColorSetting* addColor(std::string settingName, float r, float g, float b, float a = 1.f);

private:
    std::string name;
    std::string description;
    Category category;
    Icon icon;
    bool enabled = false;
    std::vector<std::unique_ptr<Setting>> settings;
};
