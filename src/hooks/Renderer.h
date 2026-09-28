#pragma once

// Hooks the game's swap chain (d3d12 or d3d11) + window proc so imgui can draw on top of bedrock.
namespace Renderer {
    bool install();
    void uninstall();
}
