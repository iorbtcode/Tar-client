#include "config/Config.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "core/Client.h"
#include "gui/Theme.h"
#include "module/ModuleManager.h"
#include "module/modules/client/Notifications.h"

namespace fs = std::filesystem;

namespace {
    bool canWrite(const fs::path& dir) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        fs::path test = dir / ".write_test";
        {
            std::ofstream f(test);
            if (!f) return false;
        }
        fs::remove(test, ec);
        return true;
    }

    std::string findPath() {
        // newer bedrock (gdk) can write to %APPDATA%, older uwp builds only to their RoamingState
        std::vector<fs::path> candidates;
        if (const char* appdata = std::getenv("APPDATA"))
            candidates.push_back(fs::path(appdata) / "TarClient");
        if (const char* local = std::getenv("LOCALAPPDATA"))
            candidates.push_back(fs::path(local) / "Packages" / "Microsoft.MinecraftUWP_8wekyb3d8bbwe" / "RoamingState" / "TarClient");

        for (auto& dir : candidates)
            if (canWrite(dir)) return (dir / "config.txt").string();

        // last resort, next to the dll
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(Client::self(), buf, MAX_PATH);
        return (fs::path(buf).parent_path() / "tarclient_config.txt").string();
    }

    std::string trim(std::string s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
        size_t start = s.find_first_not_of(' ');
        return start == std::string::npos ? "" : s.substr(start);
    }
}

const std::string& Config::path() {
    static std::string cached = findPath();
    return cached;
}

void Config::save() {
    std::ofstream f(path(), std::ios::trunc);
    if (!f) return;

    f << "# " TAR_CLIENT_NAME " config\n\n[client]\n";
    f << "menuKey=" << Client::menuKey << "\n";
    char accent[64];
    std::snprintf(accent, sizeof(accent), "%.3f,%.3f,%.3f", Theme::accent.x, Theme::accent.y, Theme::accent.z);
    f << "accent=" << accent << "\n";
    for (auto& name : Client::friends) f << "friend=" << name << "\n";

    for (auto& m : ModuleManager::all()) {
        f << "\n[module " << m->getName() << "]\n";
        f << "enabled=" << (m->isEnabled() ? 1 : 0) << "\n";
        f << "key=" << m->key << "\n";
        for (auto& s : m->getSettings()) f << "setting." << s->name << "=" << s->serialize() << "\n";
    }
}

void Config::load() {
    std::ifstream f(path());
    if (!f) return;

    // don't spam a toast for every module the config turns on
    Notifications::muted = true;
    Client::friends.clear();

    Module* current = nullptr;
    bool inClient = false;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            std::string section = line.substr(1, line.size() - 2);
            inClient = section == "client";
            current = section.rfind("module ", 0) == 0 ? ModuleManager::find(section.substr(7)) : nullptr;
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);

        try {
            if (inClient) {
                if (key == "menuKey") Client::menuKey = std::stoi(value);
                else if (key == "friend") Client::friends.push_back(value);
                else if (key == "accent")
                    std::sscanf(value.c_str(), "%f,%f,%f", &Theme::accent.x, &Theme::accent.y, &Theme::accent.z);
            } else if (current) {
                if (key == "enabled") current->setEnabled(value == "1");
                else if (key == "key") current->key = std::stoi(value);
                else if (key.rfind("setting.", 0) == 0)
                    if (Setting* s = current->findSetting(key.substr(8))) s->deserialize(value);
            }
        } catch (...) {
            // bad line, skip it
        }
    }
    Notifications::muted = false;
}
