#include "module/Module.h"

#include "module/modules/client/Notifications.h"

Module::Module(std::string name, std::string description, Category category, Icon icon, int key)
    : key(key), name(std::move(name)), description(std::move(description)), category(category), icon(icon) {}

void Module::setEnabled(bool value) {
    if (value == enabled) return;
    enabled = value;
    if (enabled) onEnable();
    else onDisable();
    Notifications::push(name + (enabled ? " enabled" : " disabled"), enabled);
}

Setting* Module::findSetting(const std::string& settingName) {
    for (auto& s : settings)
        if (s->name == settingName) return s.get();
    return nullptr;
}

BoolSetting* Module::addBool(std::string settingName, bool def) {
    auto* s = new BoolSetting(std::move(settingName), def);
    settings.emplace_back(s);
    return s;
}

SliderSetting* Module::addSlider(std::string settingName, float def, float min, float max, int decimals) {
    auto* s = new SliderSetting(std::move(settingName), def, min, max, decimals);
    settings.emplace_back(s);
    return s;
}

ModeSetting* Module::addMode(std::string settingName, std::vector<std::string> modes, int def) {
    auto* s = new ModeSetting(std::move(settingName), std::move(modes), def);
    settings.emplace_back(s);
    return s;
}

ColorSetting* Module::addColor(std::string settingName, float r, float g, float b, float a) {
    auto* s = new ColorSetting(std::move(settingName), r, g, b, a);
    settings.emplace_back(s);
    return s;
}
