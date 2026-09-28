#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

// Settings are the little options that show up when you expand a module in the gui.
// A module creates them in its constructor with addBool / addSlider / addMode / addColor
// and keeps the returned pointer around to read the value.

enum class SettingType { Bool, Slider, Mode, Color };

class Setting {
public:
    Setting(std::string name, SettingType type) : name(std::move(name)), type(type) {}
    virtual ~Setting() = default;

    std::string name;
    SettingType type;
    bool hidden = false; // hidden settings are saved to the config but not drawn in the gui

    virtual std::string serialize() const = 0;
    virtual void deserialize(const std::string& text) = 0;
};

class BoolSetting : public Setting {
public:
    BoolSetting(std::string name, bool def) : Setting(std::move(name), SettingType::Bool), value(def) {}
    bool value;

    std::string serialize() const override { return value ? "1" : "0"; }
    void deserialize(const std::string& text) override { value = text == "1" || text == "true"; }
};

class SliderSetting : public Setting {
public:
    SliderSetting(std::string name, float def, float min, float max, int decimals)
        : Setting(std::move(name), SettingType::Slider), value(def), min(min), max(max), decimals(decimals) {}
    float value, min, max;
    int decimals; // 0 = whole numbers

    std::string serialize() const override { return std::to_string(value); }
    void deserialize(const std::string& text) override {
        try { value = std::clamp(std::stof(text), min, max); } catch (...) {}
    }
};

class ModeSetting : public Setting {
public:
    ModeSetting(std::string name, std::vector<std::string> modes, int def)
        : Setting(std::move(name), SettingType::Mode), modes(std::move(modes)), index(def) {}
    std::vector<std::string> modes;
    int index;

    const std::string& current() const { return modes[index]; }
    bool is(const std::string& mode) const { return current() == mode; }

    std::string serialize() const override { return current(); }
    void deserialize(const std::string& text) override {
        auto it = std::find(modes.begin(), modes.end(), text);
        if (it != modes.end()) index = int(it - modes.begin());
    }
};

class ColorSetting : public Setting {
public:
    ColorSetting(std::string name, float r, float g, float b, float a)
        : Setting(std::move(name), SettingType::Color), value{ r, g, b, a } {}
    float value[4];

    std::string serialize() const override {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.3f,%.3f,%.3f,%.3f", value[0], value[1], value[2], value[3]);
        return buf;
    }
    void deserialize(const std::string& text) override {
        float c[4];
        if (std::sscanf(text.c_str(), "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]) == 4)
            for (int i = 0; i < 4; i++) value[i] = std::clamp(c[i], 0.f, 1.f);
    }
};
