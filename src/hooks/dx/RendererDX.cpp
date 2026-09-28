#include "hooks/Renderer.h"

#include <atomic>
#include <vector>

#include <windows.h>

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "core/Client.h"
#include "hooks/Common.h"

// Bedrock renderer hook. How it works:
//  1. make a throwaway d3d12 device + swap chain to read the addresses of Present / ResizeBuffers
//     out of their vtables (every swap chain in the process shares them), then hook them with minhook
//  2. hook ID3D12CommandQueue::ExecuteCommandLists to grab the game's direct queue
//  3. on the first real Present, check whether the game runs d3d12 or d3d11.
//     for d3d12 we wrap it in a d3d11on12 device so the regular imgui dx11 backend can draw on the back buffers
//  4. input + cursor handling is shared with the java build, see hooks/Common.cpp

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

    PresentFn oPresent = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
    ResizeBuffers1Fn oResizeBuffers1 = nullptr;
    ExecuteCommandListsFn oExecuteCommandLists = nullptr;

    enum class Backend { None, DX11, DX12 };

    using Hooks::InFlight;

    struct State {
        Backend backend = Backend::None;
        bool dx11Ready = false;   // imgui dx11 backend
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
        Hooks::attachWindow(desc.OutputWindow);

        if (!ImGui_ImplDX11_Init(g.device11, g.context11)) return false;
        g.dx11Ready = true;
        return true;
    }

    void renderFrame(IDXGISwapChain* sc) {
        std::lock_guard lock(Hooks::mutex());
        if (Hooks::shuttingDown()) return;

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

        Hooks::beginFrame();
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
            std::lock_guard lock(Hooks::mutex());
            releaseTargets(); // the game can't resize while we still hold the back buffers
        }
        return oResizeBuffers(sc, count, w, h, fmt, flags);
    }

    HRESULT STDMETHODCALLTYPE hkResizeBuffers1(IDXGISwapChain3* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags,
                                               const UINT* nodeMask, IUnknown* const* queues) {
        InFlight guard;
        {
            std::lock_guard lock(Hooks::mutex());
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
    if (!Hooks::init()) return false;

    VTables vt;
    {
        DummyWindow window;
        if (!window.hwnd) return false;
        if (!findDx12(window.hwnd, vt) && !findDx11(window.hwnd, vt)) return false;
    }

    if (!Hooks::hook(vt.present, reinterpret_cast<void*>(&hkPresent), reinterpret_cast<void**>(&oPresent))) return false;
    if (!Hooks::hook(vt.resizeBuffers, reinterpret_cast<void*>(&hkResizeBuffers), reinterpret_cast<void**>(&oResizeBuffers))) return false;
    if (vt.resizeBuffers1)
        Hooks::hook(vt.resizeBuffers1, reinterpret_cast<void*>(&hkResizeBuffers1), reinterpret_cast<void**>(&oResizeBuffers1));
    if (vt.executeCommandLists)
        Hooks::hook(vt.executeCommandLists, reinterpret_cast<void*>(&hkExecuteCommandLists), reinterpret_cast<void**>(&oExecuteCommandLists));

    return true;
}

void Renderer::uninstall() {
    Hooks::disable();
    {
        std::lock_guard lock(Hooks::mutex());
        releaseDevice();
        if (ID3D12CommandQueue* queue = gameQueue.exchange(nullptr)) queue->Release();
    }
    Hooks::destroy();
}
