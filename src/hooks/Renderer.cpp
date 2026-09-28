#include "hooks/Renderer.h"

#include <atomic>
#include <mutex>
#include <vector>

#include <windows.h>

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "core/Client.h"
#include "gui/ClickGui.h"
#include "gui/Theme.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// How it works:
//  1. make a throwaway d3d12 device + swap chain to read the addresses of Present / ResizeBuffers
//     out of their vtables (every swap chain in the process shares them), then hook them with minhook
//  2. hook ID3D12CommandQueue::ExecuteCommandLists to grab the game's direct queue
//  3. on the first real Present, check whether the game runs d3d12 or d3d11.
//     for d3d12 we wrap it in a d3d11on12 device so the regular imgui dx11 backend can draw on the back buffers
//  4. subclass the game window to feed input to imgui and block it from the game while the menu is open

namespace {
    template <typename T>
    void release(T*& p) {
        if (p) p->Release();
        p = nullptr;
    }

    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ResizeBuffers1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
    using ExecuteCommandListsFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    using SetCursorPosFn = BOOL(WINAPI*)(int, int);
    using ClipCursorFn = BOOL(WINAPI*)(const RECT*);

    PresentFn oPresent = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
    ResizeBuffers1Fn oResizeBuffers1 = nullptr;
    ExecuteCommandListsFn oExecuteCommandLists = nullptr;
    SetCursorPosFn oSetCursorPos = nullptr;
    ClipCursorFn oClipCursor = nullptr;

    std::vector<void*> hookedTargets;

    // everything imgui touches is guarded by this, the window proc and Present can run on different threads
    std::recursive_mutex mutex;
    std::atomic_int inFlight = 0; // hooks currently executing, so unloading waits for them
    std::atomic_bool shuttingDown = false;

    struct InFlight {
        InFlight() { ++inFlight; }
        ~InFlight() { --inFlight; }
    };

    enum class Backend { None, DX11, DX12 };

    struct State {
        Backend backend = Backend::None;
        bool imguiReady = false;  // context + win32 backend + wndproc hook
        bool dx11Ready = false;   // imgui dx11 backend
        HWND hwnd = nullptr;
        WNDPROC originalWndProc = nullptr;
        IDXGISwapChain* swapChain = nullptr; // not owned, only compared

        ID3D11Device* device11 = nullptr;
        ID3D11DeviceContext* context11 = nullptr;
        ID3D12Device* device12 = nullptr;
        ID3D11On12Device* on12 = nullptr;

        // one entry per back buffer (only one for dx11)
        std::vector<ID3D11Resource*> wrapped;
        std::vector<ID3D11RenderTargetView*> targets;
    } g;

    // grabbed from ExecuteCommandLists, which the game calls from its own threads
    std::atomic<ID3D12CommandQueue*> gameQueue = nullptr;

    // ---------------------------------------------------------------- input

    bool isInputMessage(UINT msg) {
        return (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) || (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT;
    }

    bool isReleaseMessage(UINT msg) {
        return msg == WM_KEYUP || msg == WM_SYSKEYUP || msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP ||
               msg == WM_XBUTTONUP;
    }

    LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        InFlight guard;
        WNDPROC original = g.originalWndProc;
        if (shuttingDown || !g.imguiReady) return CallWindowProcW(original, hwnd, msg, wParam, lParam);

        bool block = false;
        {
            std::lock_guard lock(mutex);

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

            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
            if (ClickGui::isOpen() && (isInputMessage(msg) || msg == WM_CHAR)) block = true;
            // always let releases through, otherwise a key held while opening the menu gets stuck down in game
            if (isReleaseMessage(msg)) block = false;
        }

        if (block) {
            // raw input still needs DefWindowProc so windows can clean up its buffer
            if (msg == WM_INPUT) return DefWindowProcW(hwnd, msg, wParam, lParam);
            return 0;
        }
        return CallWindowProcW(original, hwnd, msg, wParam, lParam);
    }

    // the game re-centers and locks the cursor every frame, stop that while the menu is open
    BOOL WINAPI hkSetCursorPos(int x, int y) {
        InFlight guard;
        if (ClickGui::isOpen() && !shuttingDown) return TRUE;
        return oSetCursorPos(x, y);
    }

    BOOL WINAPI hkClipCursor(const RECT* rect) {
        InFlight guard;
        if (ClickGui::isOpen() && !shuttingDown) return oClipCursor(nullptr);
        return oClipCursor(rect);
    }

    // ---------------------------------------------------------------- d3d

    void releaseTargets() {
        for (auto*& t : g.targets) release(t);
        for (auto*& w : g.wrapped) release(w);
        g.targets.clear();
        g.wrapped.clear();
        if (g.context11) {
            g.context11->OMSetRenderTargets(0, nullptr, nullptr);
            g.context11->Flush();
        }
    }

    void releaseDevice() {
        releaseTargets();
        if (g.dx11Ready) ImGui_ImplDX11_Shutdown();
        g.dx11Ready = false;
        release(g.on12);
        release(g.context11);
        release(g.device11);
        release(g.device12);
        g.backend = Backend::None;
    }

    bool createTargets(IDXGISwapChain* sc) {
        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(sc->GetDesc(&desc))) return false;

        if (g.backend == Backend::DX11) {
            ID3D11Texture2D* buffer = nullptr;
            if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return false;
            ID3D11RenderTargetView* rtv = nullptr;
            HRESULT hr = g.device11->CreateRenderTargetView(buffer, nullptr, &rtv);
            buffer->Release();
            if (FAILED(hr)) return false;
            g.targets.push_back(rtv);
            return true;
        }

        for (UINT i = 0; i < desc.BufferCount; i++) {
            ID3D12Resource* buffer = nullptr;
            if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&buffer)))) return false;

            D3D11_RESOURCE_FLAGS flags{};
            flags.BindFlags = D3D11_BIND_RENDER_TARGET;
            ID3D11Resource* wrapped = nullptr;
            HRESULT hr = g.on12->CreateWrappedResource(buffer, &flags, D3D12_RESOURCE_STATE_PRESENT,
                                                       D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&wrapped));
            buffer->Release();
            if (FAILED(hr)) return false;

            ID3D11RenderTargetView* rtv = nullptr;
            if (FAILED(g.device11->CreateRenderTargetView(wrapped, nullptr, &rtv))) {
                wrapped->Release();
                return false;
            }
            g.wrapped.push_back(wrapped);
            g.targets.push_back(rtv);
        }
        return true;
    }

    bool createDevice(IDXGISwapChain* sc) {
        if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&g.device12)))) {
            // need the game's command queue before we can make the 11on12 device
            ID3D12CommandQueue* queue = gameQueue;
            ID3D12Device* queueDevice = nullptr;
            if (queue) queue->GetDevice(IID_PPV_ARGS(&queueDevice));
            bool sameDevice = queueDevice == g.device12;
            release(queueDevice);
            if (!sameDevice) {
                // grabbed a queue from some other device, forget it and wait for the right one
                if (queue && gameQueue.compare_exchange_strong(queue, nullptr)) queue->Release();
                release(g.device12);
                return false;
            }
            IUnknown* queues[] = { queue };
            if (FAILED(D3D11On12CreateDevice(g.device12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0,
                                             &g.device11, &g.context11, nullptr)) ||
                FAILED(g.device11->QueryInterface(IID_PPV_ARGS(&g.on12)))) {
                releaseDevice();
                return false;
            }
            g.backend = Backend::DX12;
        } else if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&g.device11)))) {
            g.device11->GetImmediateContext(&g.context11);
            g.backend = Backend::DX11;
        } else {
            return false;
        }
        return true;
    }

    bool initImGui(IDXGISwapChain* sc) {
        DXGI_SWAP_CHAIN_DESC desc{};
        sc->GetDesc(&desc);
        g.hwnd = desc.OutputWindow;

        if (!ImGui::GetCurrentContext()) {
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            Theme::loadFonts();
            Theme::applyStyle();
            ImGui_ImplWin32_Init(g.hwnd);
        }
        if (!ImGui_ImplDX11_Init(g.device11, g.context11)) return false;
        g.dx11Ready = true;

        if (!g.originalWndProc)
            g.originalWndProc = WNDPROC(SetWindowLongPtrW(g.hwnd, GWLP_WNDPROC, LONG_PTR(hkWndProc)));
        g.imguiReady = true;
        return true;
    }

    void renderFrame(IDXGISwapChain* sc) {
        std::lock_guard lock(mutex);
        if (shuttingDown) return;

        // swap chain changed (fullscreen toggle, device reset...) -> rebuild what depends on it
        if (sc != g.swapChain) {
            releaseTargets();
            g.swapChain = sc;
        }
        if (g.backend == Backend::DX12) {
            ID3D12Device* current = nullptr;
            sc->GetDevice(IID_PPV_ARGS(&current));
            if (current != g.device12) releaseDevice();
            release(current);
        }

        if (g.backend == Backend::None) {
            if (!createDevice(sc)) return;
            if (!initImGui(sc)) {
                releaseDevice();
                return;
            }
        }
        if (g.targets.empty() && !createTargets(sc)) {
            releaseTargets();
            return;
        }

        UINT index = 0;
        if (g.backend == Backend::DX12) {
            IDXGISwapChain3* sc3 = nullptr;
            if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc3)))) {
                index = sc3->GetCurrentBackBufferIndex();
                sc3->Release();
            }
            if (index >= g.targets.size()) return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        Client::onFrame();
        ImGui::Render();

        if (g.backend == Backend::DX12) g.on12->AcquireWrappedResources(&g.wrapped[index], 1);
        g.context11->OMSetRenderTargets(1, &g.targets[index], nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (g.backend == Backend::DX12) {
            g.on12->ReleaseWrappedResources(&g.wrapped[index], 1);
            g.context11->Flush();
        }
    }

    // ---------------------------------------------------------------- hooks

    HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* sc, UINT syncInterval, UINT flags) {
        InFlight guard;
        if (!(flags & DXGI_PRESENT_TEST)) renderFrame(sc);
        return oPresent(sc, syncInterval, flags);
    }

    HRESULT STDMETHODCALLTYPE hkResizeBuffers(IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags) {
        InFlight guard;
        {
            std::lock_guard lock(mutex);
            releaseTargets(); // the game can't resize while we still hold the back buffers
        }
        return oResizeBuffers(sc, count, w, h, fmt, flags);
    }

    HRESULT STDMETHODCALLTYPE hkResizeBuffers1(IDXGISwapChain3* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags,
                                               const UINT* nodeMask, IUnknown* const* queues) {
        InFlight guard;
        {
            std::lock_guard lock(mutex);
            releaseTargets();
        }
        return oResizeBuffers1(sc, count, w, h, fmt, flags, nodeMask, queues);
    }

    void STDMETHODCALLTYPE hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
        InFlight guard;
        if (!gameQueue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            ID3D12CommandQueue* expected = nullptr;
            queue->AddRef();
            if (!gameQueue.compare_exchange_strong(expected, queue)) queue->Release();
        }
        oExecuteCommandLists(queue, count, lists);
    }

    bool hook(void* target, void* detour, void** original) {
        if (MH_CreateHook(target, detour, original) != MH_OK) return false;
        if (MH_EnableHook(target) != MH_OK) return false;
        hookedTargets.push_back(target);
        return true;
    }

    void* vfunc(void* object, int index) { return (*static_cast<void***>(object))[index]; }

    // ---------------------------------------------------------------- vtable grabbing

    struct DummyWindow {
        HWND hwnd = nullptr;
        WNDCLASSEXW wc{};
        DummyWindow() {
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"TarClientDummy";
            RegisterClassExW(&wc);
            hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
        }
        ~DummyWindow() {
            if (hwnd) DestroyWindow(hwnd);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
        }
    };

    struct VTables {
        void* present = nullptr;
        void* resizeBuffers = nullptr;
        void* resizeBuffers1 = nullptr;
        void* executeCommandLists = nullptr;
    };

    bool findDx12(HWND hwnd, VTables& out) {
        IDXGIFactory4* factory = nullptr;
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        IDXGISwapChain1* sc1 = nullptr;
        bool ok = false;

        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            D3D12_COMMAND_QUEUE_DESC qd{};
            qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) {
                DXGI_SWAP_CHAIN_DESC1 sd{};
                sd.Width = 100;
                sd.Height = 100;
                sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                sd.SampleDesc.Count = 1;
                sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                sd.BufferCount = 2;
                sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                if (SUCCEEDED(factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &sc1))) {
                    out.present = vfunc(sc1, 8);
                    out.resizeBuffers = vfunc(sc1, 13);
                    out.resizeBuffers1 = vfunc(sc1, 39);
                    out.executeCommandLists = vfunc(queue, 10);
                    ok = true;
                }
            }
        }
        release(sc1);
        release(queue);
        release(device);
        release(factory);
        return ok;
    }

    bool findDx11(HWND hwnd, VTables& out) {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* sc = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                                 &sd, &sc, &device, nullptr, &context)))
            return false;
        out.present = vfunc(sc, 8);
        out.resizeBuffers = vfunc(sc, 13);
        release(sc);
        release(context);
        release(device);
        return true;
    }
}

bool Renderer::install() {
    if (MH_Initialize() != MH_OK) return false;

    VTables vt;
    {
        DummyWindow window;
        if (!window.hwnd) return false;
        if (!findDx12(window.hwnd, vt) && !findDx11(window.hwnd, vt)) return false;
    }

    if (!hook(vt.present, reinterpret_cast<void*>(&hkPresent), reinterpret_cast<void**>(&oPresent))) return false;
    if (!hook(vt.resizeBuffers, reinterpret_cast<void*>(&hkResizeBuffers), reinterpret_cast<void**>(&oResizeBuffers))) return false;
    if (vt.resizeBuffers1)
        hook(vt.resizeBuffers1, reinterpret_cast<void*>(&hkResizeBuffers1), reinterpret_cast<void**>(&oResizeBuffers1));
    if (vt.executeCommandLists)
        hook(vt.executeCommandLists, reinterpret_cast<void*>(&hkExecuteCommandLists), reinterpret_cast<void**>(&oExecuteCommandLists));

    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        hook(reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos")), reinterpret_cast<void*>(&hkSetCursorPos),
             reinterpret_cast<void**>(&oSetCursorPos));
        hook(reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor")), reinterpret_cast<void*>(&hkClipCursor),
             reinterpret_cast<void**>(&oClipCursor));
    }
    return true;
}

void Renderer::uninstall() {
    shuttingDown = true;
    for (void* target : hookedTargets) MH_DisableHook(target);

    {
        std::lock_guard lock(mutex);
        if (g.hwnd && g.originalWndProc) SetWindowLongPtrW(g.hwnd, GWLP_WNDPROC, LONG_PTR(g.originalWndProc));
    }

    // let any hook that was mid-call finish before tearing things down
    while (inFlight > 0) Sleep(10);
    Sleep(100);

    std::lock_guard lock(mutex);
    releaseDevice();
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    g.imguiReady = false;
    if (ID3D12CommandQueue* queue = gameQueue.exchange(nullptr)) queue->Release();

    MH_Uninitialize();
}
