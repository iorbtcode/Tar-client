#include "hooks/Common.h"

#include <vector>

#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_win32.h>

#include "core/Client.h"
#include "gui/ClickGui.h"
#include "gui/Theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    using SetCursorPosFn = BOOL(WINAPI*)(int, int);
    using ClipCursorFn = BOOL(WINAPI*)(const RECT*);
    SetCursorPosFn oSetCursorPos = nullptr;
    ClipCursorFn oClipCursor = nullptr;

    std::recursive_mutex lock;
    std::atomic_int inFlight = 0;
    std::atomic_bool stopping = false;
    std::vector<void*> hookedTargets;

    HWND hwnd = nullptr;
    WNDPROC originalWndProc = nullptr;
    bool win32Ready = false;
    bool menuWasOpen = false;

    bool isInputMessage(UINT msg) {
        return (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT;
    }

    bool isReleaseMessage(UINT msg) {
        return msg == WM_KEYUP || msg == WM_SYSKEYUP || msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP ||
               msg == WM_XBUTTONUP;
    }

    LRESULT CALLBACK hkWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        Hooks::InFlight guard;
        WNDPROC original = originalWndProc;
        if (stopping || !win32Ready) return CallWindowProcW(original, wnd, msg, wParam, lParam);

        bool block = false;
        {
            std::lock_guard guardLock(lock);

            switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (!(lParam & (1 << 30))) block |= Client::onKey(int(wParam), true); // ignore auto repeat
                else block |= ClickGui::isOpen();
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP: block |= Client::onKey(int(wParam), false); break;
            case WM_LBUTTONDOWN: block |= Client::onMouse(0, true); break;
            case WM_LBUTTONUP: block |= Client::onMouse(0, false); break;
            case WM_RBUTTONDOWN: block |= Client::onMouse(1, true); break;
            case WM_RBUTTONUP: block |= Client::onMouse(1, false); break;
            case WM_MBUTTONDOWN: block |= Client::onMouse(2, true); break;
            case WM_MBUTTONUP: block |= Client::onMouse(2, false); break;
            case WM_XBUTTONDOWN: block |= Client::onMouse(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4, true); break;
            case WM_XBUTTONUP: block |= Client::onMouse(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4, false); break;
            case WM_KILLFOCUS: ClickGui::setOpen(false); break;
            default: break;
            }

            ImGui_ImplWin32_WndProcHandler(wnd, msg, wParam, lParam);
            if (ClickGui::isOpen() && (isInputMessage(msg) || msg == WM_CHAR)) block = true;
            // the game hides its cursor on WM_SETCURSOR, imgui draws its own while the menu is open
            if (ClickGui::isOpen() && msg == WM_SETCURSOR) block = true;
            // always let releases through, otherwise a key held while opening the menu gets stuck down in game
            if (isReleaseMessage(msg)) block = false;
        }

        if (block) {
            // raw input still needs DefWindowProc so windows can clean up its buffer
            if (msg == WM_INPUT) return DefWindowProcW(wnd, msg, wParam, lParam);
            return msg == WM_SETCURSOR ? TRUE : 0;
        }
        return CallWindowProcW(original, wnd, msg, wParam, lParam);
    }

    // games re-center and lock the cursor every frame, stop that while the menu is open
    BOOL WINAPI hkSetCursorPos(int x, int y) {
        Hooks::InFlight guard;
        if (ClickGui::isOpen() && !stopping) return TRUE;
        return oSetCursorPos(x, y);
    }

    BOOL WINAPI hkClipCursor(const RECT* rect) {
        Hooks::InFlight guard;
        if (ClickGui::isOpen() && !stopping) return oClipCursor(nullptr);
        return oClipCursor(rect);
    }

    void restoreWndProc() {
        if (hwnd && originalWndProc && IsWindow(hwnd)) SetWindowLongPtrW(hwnd, GWLP_WNDPROC, LONG_PTR(originalWndProc));
        originalWndProc = nullptr;
    }
}

std::recursive_mutex& Hooks::mutex() { return lock; }
bool Hooks::shuttingDown() { return stopping; }

Hooks::InFlight::InFlight() { ++inFlight; }
Hooks::InFlight::~InFlight() { --inFlight; }

bool Hooks::hook(void* target, void* detour, void** original) {
    if (!target) return false;
    if (MH_CreateHook(target, detour, original) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) return false;
    hookedTargets.push_back(target);
    return true;
}

bool Hooks::init() {
    if (MH_Initialize() != MH_OK) return false;
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        hook(reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos")), reinterpret_cast<void*>(&hkSetCursorPos),
             reinterpret_cast<void**>(&oSetCursorPos));
        hook(reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor")), reinterpret_cast<void*>(&hkClipCursor),
             reinterpret_cast<void**>(&oClipCursor));
    }
    return true;
}

void Hooks::attachWindow(HWND wnd) {
    if (wnd == hwnd && win32Ready) return;

    if (!ImGui::GetCurrentContext()) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        Theme::loadFonts();
        Theme::applyStyle();
    }

    // moving to a new window (java 1.8 recreates it when toggling fullscreen)
    if (win32Ready) {
        restoreWndProc();
        ImGui_ImplWin32_Shutdown();
        win32Ready = false;
    }

    hwnd = wnd;
    ImGui_ImplWin32_Init(hwnd);
    originalWndProc = WNDPROC(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, LONG_PTR(hkWndProc)));
    win32Ready = true;
}

HWND Hooks::window() { return hwnd; }

void Hooks::beginFrame() {
    // free the cursor the moment the menu opens, the game only re-clips it when it wants to
    bool open = ClickGui::isOpen();
    if (open && !menuWasOpen && oClipCursor) oClipCursor(nullptr);
    menuWasOpen = open;
}

void Hooks::disable() {
    stopping = true;
    for (void* target : hookedTargets) MH_DisableHook(target);
    {
        std::lock_guard guardLock(lock);
        restoreWndProc();
    }
    // let any hook that was mid-call finish before tearing things down
    while (inFlight > 0) Sleep(10);
    Sleep(100);
}

void Hooks::destroy() {
    std::lock_guard guardLock(lock);
    if (ImGui::GetCurrentContext()) {
        if (win32Ready) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    win32Ready = false;
    MH_Uninitialize();
}
