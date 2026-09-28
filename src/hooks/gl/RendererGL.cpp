#include "hooks/Renderer.h"

#include <atomic>

#include <windows.h>

#include <GL/gl.h>

#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_win32.h>

#include "core/Client.h"
#include "hooks/Common.h"

// Java edition renderer hook. How it works:
//  1. hook wglSwapBuffers in opengl32.dll, every java version (lwjgl 2 and 3) ends each frame with it
//  2. make our own gl context on the game's window. we switch to it, draw imgui, and switch back,
//     so none of our gl state can leak into the game (and it doesn't matter if the game uses a core profile)
//  3. input + cursor handling is shared with the bedrock build, see hooks/Common.cpp

namespace {
    using SwapBuffersFn = BOOL(WINAPI*)(HDC);
    SwapBuffersFn oSwapBuffers = nullptr;

    struct State {
        HWND hwnd = nullptr;
        HDC hdc = nullptr;
        HGLRC context = nullptr; // ours
        bool backendReady = false;
    } g;

    std::atomic_bool cleanupRequested = false;
    std::atomic_bool cleanedUp = false;

    // frees our context + the imgui gl backend. only touches gl through our own context,
    // if that can't be made current the gl calls go nowhere instead of hitting the game's objects
    void destroyContext() {
        if (!g.context) return;
        HGLRC prevContext = wglGetCurrentContext();
        HDC prevDc = wglGetCurrentDC();

        if (!wglMakeCurrent(g.hdc, g.context)) wglMakeCurrent(nullptr, nullptr);
        if (g.backendReady) ImGui_ImplOpenGL2_Shutdown();
        g.backendReady = false;
        wglMakeCurrent(prevDc, prevContext);

        wglDeleteContext(g.context);
        g.context = nullptr;
    }

    void renderFrame(HDC hdc) {
        std::lock_guard lock(Hooks::mutex());
        if (Hooks::shuttingDown() || cleanedUp) return;

        if (cleanupRequested) {
            destroyContext();
            cleanedUp = true;
            return;
        }

        HWND hwnd = WindowFromDC(hdc);
        HGLRC gameContext = wglGetCurrentContext();
        HDC gameDc = wglGetCurrentDC();
        if (!hwnd || !gameContext) return;

        // new window (1.8 makes a new one when you toggle fullscreen) -> start over on it
        if (hwnd != g.hwnd || hdc != g.hdc) {
            destroyContext();
            g.hwnd = hwnd;
            g.hdc = hdc;
        }
        if (!g.context && !(g.context = wglCreateContext(hdc))) return;

        if (!wglMakeCurrent(hdc, g.context)) {
            wglMakeCurrent(gameDc, gameContext);
            destroyContext();
            return;
        }

        if (!g.backendReady) {
            Hooks::attachWindow(hwnd);
            g.backendReady = ImGui_ImplOpenGL2_Init();
        }

        if (g.backendReady) {
            Hooks::beginFrame();
            ImGui_ImplOpenGL2_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            Client::onFrame();
            ImGui::Render();

            // draw straight onto the window, on top of whatever the game just rendered
            RECT rect{};
            GetClientRect(hwnd, &rect);
            glViewport(0, 0, rect.right - rect.left, rect.bottom - rect.top);
            glDrawBuffer(GL_BACK);
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        }

        wglMakeCurrent(gameDc, gameContext);
    }

    BOOL WINAPI hkSwapBuffers(HDC hdc) {
        Hooks::InFlight guard;
        renderFrame(hdc);
        return oSwapBuffers(hdc);
    }
}

bool Renderer::install() {
    HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
    if (!opengl) opengl = LoadLibraryW(L"opengl32.dll");
    if (!opengl || !Hooks::init()) return false;

    return Hooks::hook(reinterpret_cast<void*>(GetProcAddress(opengl, "wglSwapBuffers")),
                       reinterpret_cast<void*>(&hkSwapBuffers), reinterpret_cast<void**>(&oSwapBuffers));
}

void Renderer::uninstall() {
    // gl objects are best freed on the render thread, so ask the next frame to do it
    cleanupRequested = true;
    for (int i = 0; i < 100 && !cleanedUp; i++) Sleep(10);

    Hooks::disable();
    {
        // game is paused / minimized and didn't render, do it from here instead
        std::lock_guard lock(Hooks::mutex());
        if (!cleanedUp) destroyContext();
    }
    Hooks::destroy();
}
