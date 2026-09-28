# Tar Client

An injectable ImGui overlay/click GUI for Minecraft Bedrock (Windows). It's a DLL that hooks the game's DirectX 12 (or DirectX 11) swap chain and draws a dark, modular menu over the game.

- Click GUI with a home page, search, categories, settings, and friends pages
- Module system with settings (toggle, slider, mode, and color), keybinds, and animations
- Draggable HUD elements
- Config that saves on its own (module states, keybinds, settings, accent color, and friends)
- Toast notifications and an accent color you can change
- A small injector

To add your own modules, see **[MODULES.txt](MODULES.txt)**.

![preview](docs/preview.png)

## Built-in modules

| Tab | Modules |
| --- | --- |
| HUD | Watermark, Array List, FPS Counter, CPS Counter, Keystrokes |
| Render | Crosshair, Background Dim |
| Utility | Clock, Session Timer |
| Client | Notifications, Rainbow Theme |

All of them only draw overlays. None of them read or change game memory.

## Building

You need Visual Studio 2022 (with the "Desktop development with C++" workload), CMake 3.20+, and git. On the first configure, CMake downloads Dear ImGui and MinHook.

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

This gives you `build/Release/TarClient.dll` and `build/Release/TarInjector.exe`.

MinGW-w64 works too: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`

## Using it

1. Start Minecraft Bedrock.
2. Put `TarInjector.exe` next to `TarClient.dll` and run it, or use any other LoadLibrary injector with `Minecraft.Windows.exe`.
3. In game, press **Insert** to open the menu. You can change this key on the settings page.

Controls:
- Insert or Esc closes the menu.
- Click a module's row to open its settings. The switch turns it on or off.
- To bind a key, click the keybind box, then press a key. Esc or Backspace clears it.
- While the menu is open, drag HUD elements around.
- "Unload client" on the settings page takes the DLL out cleanly.

The config is saved to `%APPDATA%\TarClient\config.txt`. Older UWP builds use the game's `RoamingState` folder instead.

## Project layout

```
src/
  dllmain.cpp            entry point, starts the client thread
  core/Client.*          glue between the hooks, modules and the gui
  hooks/Renderer.*       dx12/dx11 present hook, d3d11on12, wndproc, cursor hooks
  gui/ClickGui.*         the menu
  gui/Widgets.*          toggles, sliders, keybind boxes...
  gui/Icons.*            line icons drawn with the draw list
  gui/Theme.*            colors, fonts, imgui style
  module/                Module / HudModule / settings / ModuleManager
  module/modules/<tab>/  the modules
  config/Config.*        save / load
injector/Injector.cpp    loadlibrary injector
```

## Notes

- Only use this where client mods are allowed. Many servers ban them.
- Bedrock updates can change how the game renders. If the overlay stops showing up after an update, look at the swap chain hook in `hooks/Renderer.cpp` first.
