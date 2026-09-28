#include <windows.h>

#include "config/Config.h"
#include "core/Client.h"
#include "hooks/Renderer.h"
#include "module/ModuleManager.h"

namespace {
    DWORD WINAPI mainThread(LPVOID param) {
        HMODULE self = static_cast<HMODULE>(param);

        Client::init(self);
        if (!Renderer::install()) {
            MessageBoxA(nullptr, "Failed to hook the renderer", TAR_CLIENT_NAME, MB_ICONERROR);
            Renderer::uninstall();
            FreeLibraryAndExitThread(self, 1);
        }

        while (!Client::shouldUnload()) Sleep(50);

        Config::save();
        Renderer::uninstall();
        ModuleManager::shutdown();
        Sleep(200);
        FreeLibraryAndExitThread(self, 0);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, mainThread, module, 0, nullptr)) CloseHandle(thread);
    }
    return TRUE;
}
