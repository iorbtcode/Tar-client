#include "core/Client.h"

#include <atomic>
#include <chrono>

#include <imgui.h>

#include "config/Config.h"
#include "gui/ClickGui.h"
#include "module/ModuleManager.h"
#include "module/modules/client/Notifications.h"

namespace {
    HMODULE selfModule = nullptr;
    std::atomic_bool unloadRequested = false;
    bool keys[256] = {};
    const auto startTime = std::chrono::steady_clock::now();
}

void Client::init(HMODULE module) {
    selfModule = module;
    ModuleManager::init();
    Config::load();
}

HMODULE Client::self() { return selfModule; }

void Client::requestUnload() { unloadRequested = true; }
bool Client::shouldUnload() { return unloadRequested; }

bool Client::isMenuOpen() { return ClickGui::isOpen(); }

double Client::uptime() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
}

bool Client::isKeyDown(int vk) { return vk > 0 && vk < 256 && keys[vk]; }

void Client::onFrame() {
    ImGuiIO& io = ImGui::GetIO();
    // show imgui's cursor only while the menu is open, otherwise leave the game's cursor alone
    io.MouseDrawCursor = ClickGui::isOpen();
    if (ClickGui::isOpen()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ModuleManager::onFrame(draw);
    ClickGui::render();
    Notifications::renderToasts(ImGui::GetForegroundDrawList());
}

bool Client::onKey(int vk, bool down) {
    if (vk <= 0 || vk >= 256) return false;
    keys[vk] = down;

    if (ClickGui::isBinding()) {
        if (down) ClickGui::finishBinding(vk == VK_ESCAPE || vk == VK_BACK ? 0 : vk);
        return true;
    }

    if (down && vk == menuKey) {
        ClickGui::setOpen(!ClickGui::isOpen());
        return true;
    }
    if (ClickGui::isOpen()) {
        if (down && vk == VK_ESCAPE) ClickGui::setOpen(false);
        return true; // the game shouldn't see typing while the menu is open
    }

    ModuleManager::onKey(vk, down);
    return false;
}

bool Client::onMouse(int button, bool down) {
    static const int buttonKeys[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    int vk = buttonKeys[button];

    keys[vk] = down;

    // middle and side buttons can be used as keybinds too
    if (button >= 2 && down) {
        if (ClickGui::isBinding()) {
            ClickGui::finishBinding(vk);
            return true;
        }
        if (vk == menuKey) {
            ClickGui::setOpen(!ClickGui::isOpen());
            return true;
        }
    }
    if (ClickGui::isOpen()) return true;

    if (button >= 2 && down)
        for (auto& m : ModuleManager::all())
            if (m->key == vk) m->toggle();
    ModuleManager::onMouse(button, down);
    return false;
}
