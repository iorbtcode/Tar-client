#pragma once
#include <atomic>
#include <mutex>

#include <windows.h>

// Stuff both renderers share (bedrock dx and java opengl): minhook setup, the window proc
// that feeds input to imgui, the cursor hooks and the imgui context.
namespace Hooks {
    // everything imgui touches is guarded by this, the window proc and the render hook can run on different threads
    std::recursive_mutex& mutex();
    bool shuttingDown();

    // counts hooks that are currently running so unloading can wait for them to return
    struct InFlight {
        InFlight();
        ~InFlight();
    };

    // MH_Initialize + the cursor hooks
    bool init();
    bool hook(void* target, void* detour, void** original);

    // makes the imgui context (first time), inits the win32 backend and subclasses the window.
    // calling it again with a different window moves everything over to it
    void attachWindow(HWND hwnd);
    HWND window();

    // call at the start of every rendered frame, before ImGui::NewFrame
    void beginFrame();

    // step 1 of unloading: disable hooks, give the window proc back, wait for hooks to return
    void disable();
    // step 2, after the renderer released its own stuff: destroy imgui + minhook
    void destroy();
}
