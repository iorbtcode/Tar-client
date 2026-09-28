#pragma once
#include <memory>
#include <string>
#include <vector>

#include "module/Module.h"

namespace ModuleManager {
    // creates every module, add yours in ModuleManager.cpp
    void init();
    void shutdown();

    std::vector<std::unique_ptr<Module>>& all();
    std::vector<Module*> inCategory(Category category);
    Module* find(const std::string& name);

    template <typename T>
    T* get() {
        for (auto& m : all())
            if (auto* casted = dynamic_cast<T*>(m.get())) return casted;
        return nullptr;
    }

    // called by the client every frame / input event
    void onFrame(ImDrawList* draw);
    void onKey(int vk, bool down);
    void onMouse(int button, bool down);
}
