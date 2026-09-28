#include "gui/Theme.h"

#include <windows.h>

namespace {
    ImFont* tryFont(const char* path, float size) {
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return nullptr;
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        return ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, &cfg);
    }
}

void Theme::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // segoe ui ships with windows, fall back to imgui's font if it's missing
    regular = tryFont("C:\\Windows\\Fonts\\segoeui.ttf", 16.f);
    bold = tryFont("C:\\Windows\\Fonts\\segoeuib.ttf", 16.f);
    large = tryFont("C:\\Windows\\Fonts\\segoeuib.ttf", 22.f);

    if (!regular) regular = io.Fonts->AddFontDefault();
    if (!bold) bold = regular;
    if (!large) large = bold;
    io.FontDefault = regular;
}

void Theme::applyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.WindowPadding = ImVec2(0, 0);
    style.WindowBorderSize = 0.f;
    style.WindowRounding = 8.f;
    style.ChildRounding = 0.f;
    style.FrameRounding = 4.f;
    style.PopupRounding = 6.f;
    style.GrabRounding = 4.f;
    style.ScrollbarSize = 4.f;
    style.ScrollbarRounding = 4.f;
    style.ItemSpacing = ImVec2(8, 7);
    style.FramePadding = ImVec2(8, 5);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(text);
    c[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(textDim);
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(panelBg);
    c[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(windowBorder);
    c[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBgActive] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.12f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1, 1, 1, 0.2f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(1, 1, 1, 0.3f);
}
