#pragma once
#include <string>
#include <vector>

#include <windows.h>

#define TAR_CLIENT_NAME "Tar Client"
#define TAR_CLIENT_VERSION "1.0.0"

// TAR_JAVA is defined by cmake for TarClientJava.dll
#ifdef TAR_JAVA
#define TAR_CLIENT_EDITION "Java"
#else
#define TAR_CLIENT_EDITION "Bedrock"
#endif

namespace Client {
    // settings that live on the settings / friends pages
    inline int menuKey = VK_INSERT;
    inline std::vector<std::string> friends;

    void init(HMODULE self);
    HMODULE self();

    void requestUnload();
    bool shouldUnload();

    bool isMenuOpen();
    double uptime(); // seconds since inject
    bool isKeyDown(int vk);

    // ---- called from the hooks ----
    // inside the imgui frame, draws modules + gui
    void onFrame();
    // return true to hide the event from the game
    bool onKey(int vk, bool down);
    bool onMouse(int button, bool down);
}
