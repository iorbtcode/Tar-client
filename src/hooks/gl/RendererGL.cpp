#include "hooks/Renderer.h"

#include <atomic>
#include <cstdio>

#include <windows.h>

#include <GL/gl.h>

#include <imgui.h>
#include <imgui_impl_opengl2.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include "core/Client.h"
#include "hooks/Common.h"

// Java edition renderer hook. How it works:
//  1. hook wglSwapBuffers in opengl32.dll, every java version (lwjgl 2 and 3) ends each frame with it
//  2. draw imgui right there in the game's own gl context. the imgui gl3 backend saves and restores
//     the gl state it touches, and we save the framebuffer binding on top of that.
//     (switching to a separate context every frame is simple but tanks fps on real drivers)
//  3. input + cursor handling is shared with the bedrock build, see hooks/Common.cpp

#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

namespace {
    using SwapBuffersFn = BOOL(WINAPI*)(HDC);
    using BindFramebufferFn = void(APIENTRY*)(GLenum, GLuint);
    SwapBuffersFn oSwapBuffers = nullptr;

    enum class Backend { None, GL3, GL2 };

    struct State {
        HGLRC context = nullptr; // the game's context we set the backend up in
        Backend backend = Backend::None;
        BindFramebufferFn bindFramebuffer = nullptr;
    } g;

    std::atomic_bool cleanupRequested = false;
    std::atomic_bool cleanedUp = false;

    void shutdownBackend() {
        if (g.backend == Backend::GL3) ImGui_ImplOpenGL3_Shutdown();
        if (g.backend == Backend::GL2) ImGui_ImplOpenGL2_Shutdown();
        g.backend = Backend::None;
        g.context = nullptr;
    }

    // the context we set up in is gone (1.8 makes a new one when toggling fullscreen).
    // shut the backend down with no context current, so its delete calls can't hit the new context's objects
    void dropBackend() {
        HGLRC current = wglGetCurrentContext();
        HDC dc = wglGetCurrentDC();
        wglMakeCurrent(nullptr, nullptr);
        shutdownBackend();
        wglMakeCurrent(dc, current);
    }

    bool initBackend(HWND hwnd) {
        Hooks::attachWindow(hwnd);

        int major = 0, minor = 0;
        if (auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION)))
            std::sscanf(version, "%d.%d", &major, &minor);

        g.bindFramebuffer = reinterpret_cast<BindFramebufferFn>(wglGetProcAddress("glBindFramebuffer"));

        if (major >= 3) {
            // 1.17+ uses a 3.2 core profile, older versions a compatibility one. #version 150 works in both
            const char* glsl = (major > 3 || minor >= 2) ? "#version 150" : "#version 130";
            if (ImGui_ImplOpenGL3_Init(glsl)) g.backend = Backend::GL3;
        }
        if (g.backend == Backend::None && ImGui_ImplOpenGL2_Init()) g.backend = Backend::GL2;
        if (g.backend == Backend::None) return false;

        g.context = wglGetCurrentContext();
        return true;
    }

    void renderFrame(HDC hdc) {
        std::lock_guard lock(Hooks::mutex());
        if (Hooks::shuttingDown() || cleanedUp) return;

        HGLRC current = wglGetCurrentContext();
        if (cleanupRequested) {
            if (g.backend != Backend::None) {
                if (current == g.context) shutdownBackend();
                else dropBackend();
            }
            cleanedUp = true;
            return;
        }

        HWND hwnd = WindowFromDC(hdc);
        if (!hwnd || !current) return;

        if (g.backend != Backend::None && current != g.context) dropBackend();
        if (g.backend == Backend::None && !initBackend(hwnd)) return;
        Hooks::attachWindow(hwnd); // no-op unless the window changed

        Hooks::beginFrame();
        if (g.backend == Backend::GL3) ImGui_ImplOpenGL3_NewFrame();
        else ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        Client::onFrame();
        ImGui::Render();

        ImDrawData* data = ImGui::GetDrawData();
        if (!data || data->CmdListsCount == 0) return;

        // mods like sodium / iris can leave their own framebuffer bound, draw onto the window
        GLint oldFramebuffer = 0;
        if (g.bindFramebuffer) {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldFramebuffer);
            if (oldFramebuffer != 0) g.bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }

        if (g.backend == Backend::GL3) ImGui_ImplOpenGL3_RenderDrawData(data);
        else ImGui_ImplOpenGL2_RenderDrawData(data);

        if (g.bindFramebuffer && oldFramebuffer != 0) g.bindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(oldFramebuffer));
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
    // gl objects have to be freed on the render thread with the game's context current,
    // so ask the next frame to do it
    cleanupRequested = true;
    for (int i = 0; i < 100 && !cleanedUp; i++) Sleep(10);

    Hooks::disable();
    {
        // game is paused / minimized and didn't render. with no context current here the
        // backend's delete calls do nothing, so this just forgets them (small leak, no crash)
        std::lock_guard lock(Hooks::mutex());
        if (!cleanedUp && g.backend != Backend::None) shutdownBackend();
    }
    Hooks::destroy();
}
