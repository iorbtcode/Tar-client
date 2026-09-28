#pragma once
#include <string>

// saves / loads everything (module states, keybinds, settings, theme, friends) to a text file
namespace Config {
    void load();
    void save();
    const std::string& path();
}
