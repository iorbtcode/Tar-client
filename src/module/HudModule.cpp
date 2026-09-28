#include "module/HudModule.h"

#include "core/Client.h"
#include "gui/Theme.h"

#include <imgui_internal.h>

HudModule::HudModule(std::string name, std::string description, Category category, Icon icon, float defX, float defY)
    : Module(std::move(name), std::move(description), category, icon) {
    posX = addSlider("X", defX, 0.f, 10000.f, 0);
    posY = addSlider("Y", defY, 0.f, 10000.f, 0);
    posX->hidden = posY->hidden = true;
}

void HudModule::onRender(ImDrawList* draw) {
    ImGuiIO& io = ImGui::GetIO();

    // keep it on screen if the window got smaller
    ImVec2 pos(ImMin(posX->value, ImMax(0.f, io.DisplaySize.x - lastSize.x)),
               ImMin(posY->value, ImMax(0.f, io.DisplaySize.y - lastSize.y)));

    if (Client::isMenuOpen()) {
        ImVec2 max(pos.x + lastSize.x, pos.y + lastSize.y);
        bool hovered = ImGui::IsMouseHoveringRect(pos, max, false) && !io.WantCaptureMouse;

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            dragging = true;
            dragOffset = ImVec2(io.MousePos.x - pos.x, io.MousePos.y - pos.y);
        }
        if (dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                pos.x = ImClamp(io.MousePos.x - dragOffset.x, 0.f, ImMax(0.f, io.DisplaySize.x - lastSize.x));
                pos.y = ImClamp(io.MousePos.y - dragOffset.y, 0.f, ImMax(0.f, io.DisplaySize.y - lastSize.y));
                posX->value = pos.x;
                posY->value = pos.y;
            } else {
                dragging = false;
            }
        }

        ImU32 outline = (hovered || dragging) ? Theme::accentU32() : Theme::accentU32(0.35f);
        draw->AddRect(ImVec2(pos.x - 3, pos.y - 3), ImVec2(max.x + 3, max.y + 3), outline, 4.f, 0, 1.f);
    } else {
        dragging = false;
    }

    lastSize = drawHud(draw, pos);
}

ImVec2 HudStyle::textBox(ImDrawList* draw, ImVec2 pos, const char* label, const char* value, bool background) {
    ImFont* font = Theme::bold;
    float size = font->FontSize;
    ImVec2 labelSize = font->CalcTextSizeA(size, FLT_MAX, 0.f, label);
    ImVec2 valueSize = font->CalcTextSizeA(size, FLT_MAX, 0.f, value);

    const float padX = 8.f, padY = 5.f, gap = 5.f;
    ImVec2 box(padX * 2 + labelSize.x + gap + valueSize.x, padY * 2 + size);

    if (background) {
        draw->AddRectFilled(pos, ImVec2(pos.x + box.x, pos.y + box.y), Theme::hudBackground, 4.f);
        draw->AddRectFilled(pos, ImVec2(pos.x + 2.f, pos.y + box.y), Theme::accentU32(), 4.f, ImDrawFlags_RoundCornersLeft);
    }
    draw->AddText(font, size, ImVec2(pos.x + padX, pos.y + padY), Theme::accentU32(), label);
    draw->AddText(font, size, ImVec2(pos.x + padX + labelSize.x + gap, pos.y + padY), Theme::text, value);
    return box;
}
