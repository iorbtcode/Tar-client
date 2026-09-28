#include "module/ModuleManager.h"

#include <cstring>

#include "module/modules/client/Notifications.h"
#include "module/modules/client/RainbowTheme.h"
#include "module/modules/hud/ArrayList.h"
#include "module/modules/hud/CpsCounter.h"
#include "module/modules/hud/FpsCounter.h"
#include "module/modules/hud/Keystrokes.h"
#include "module/modules/hud/Watermark.h"
#include "module/modules/render/BackgroundDim.h"
#include "module/modules/render/Crosshair.h"
#include "module/modules/utility/Clock.h"
#include "module/modules/utility/SessionTimer.h"

namespace {
    std::vector<std::unique_ptr<Module>> modules;

    template <typename T>
    void add() { modules.push_back(std::make_unique<T>()); }
}

void ModuleManager::init() {
    // ---- register modules here, the order is the order they show in the gui ----
    add<Watermark>();
    add<ArrayList>();
    add<FpsCounter>();
    add<CpsCounter>();
    add<Keystrokes>();

    add<Crosshair>();
    add<BackgroundDim>();

    add<Clock>();
    add<SessionTimer>();

    add<Notifications>();
    add<RainbowTheme>();
}

void ModuleManager::shutdown() {
    for (auto& m : modules)
        if (m->isEnabled()) m->onDisable();
    modules.clear();
}

std::vector<std::unique_ptr<Module>>& ModuleManager::all() { return modules; }

std::vector<Module*> ModuleManager::inCategory(Category category) {
    std::vector<Module*> out;
    for (auto& m : modules)
        if (m->getCategory() == category) out.push_back(m.get());
    return out;
}

Module* ModuleManager::find(const std::string& name) {
    for (auto& m : modules)
        if (_stricmp(m->getName().c_str(), name.c_str()) == 0) return m.get();
    return nullptr;
}

void ModuleManager::onFrame(ImDrawList* draw) {
    for (auto& m : modules)
        if (m->isEnabled()) m->onTick();
    for (auto& m : modules)
        if (m->isEnabled()) m->onRender(draw);
}

void ModuleManager::onKey(int vk, bool down) {
    for (auto& m : modules) {
        if (down && m->key != 0 && m->key == vk) m->toggle();
        if (m->isEnabled()) m->onKey(vk, down);
    }
}

void ModuleManager::onMouse(int button, bool down) {
    for (auto& m : modules)
        if (m->isEnabled()) m->onMouse(button, down);
}
