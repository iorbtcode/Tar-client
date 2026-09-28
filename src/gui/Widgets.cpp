#include "gui/Widgets.h"

#include <cstdio>

#include <imgui_internal.h>

#include "gui/Icons.h"
#include "gui/Theme.h"
#include "util/Keys.h"

namespace {
    // smooth 0..1 value stored per widget id
    float animate(ImGuiID id, bool target, float speed = 12.f) {
        float* v = ImGui::GetStateStorage()->GetFloatRef(id, target ? 1.f : 0.f);
        float goal = target ? 1.f : 0.f;
        *v = ImLerp(*v, goal, ImMin(1.f, ImGui::GetIO().DeltaTime * speed));
        if (ImAbs(*v - goal) < 0.001f) *v = goal;
        return *v;
    }

    ImU32 lerpColor(ImU32 a, ImU32 b, float t) {
        ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a), cb = ImGui::ColorConvertU32ToFloat4(b);
        return ImGui::ColorConvertFloat4ToU32(ImLerp(ca, cb, t));
    }

    void rowLabel(ImDrawList* d, ImVec2 pos, float height, const char* label) {
        float y = pos.y + (height - ImGui::GetFontSize()) * 0.5f;
        d->AddText(ImVec2(pos.x, y), Theme::textDim, label);
    }
}

bool Widgets::toggle(const char* id, bool* value) {
    const ImVec2 size(30.f, 15.f);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, size);
    if (clicked) *value = !*value;

    float t = animate(ImGui::GetItemID(), *value);
    ImDrawList* d = ImGui::GetWindowDrawList();
    float r = size.y * 0.5f;
    d->AddRectFilled(pos, pos + size, lerpColor(Theme::toggleOff, Theme::accentU32(), t), r);
    float knobX = ImLerp(pos.x + r, pos.x + size.x - r, t);
    d->AddCircleFilled(ImVec2(knobX, pos.y + r), r - 3.f, lerpColor(Theme::text, Theme::textDark, t), 16);
    return clicked;
}

bool Widgets::iconButton(const char* id, int icon, float size, bool active) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
    float hover = animate(ImGui::GetItemID(), ImGui::IsItemHovered() || active);
    ImDrawList* d = ImGui::GetWindowDrawList();
    if (hover > 0.f) d->AddRectFilled(pos, pos + ImVec2(size, size), IM_COL32(255, 255, 255, int(12 * hover)), 5.f);
    ImU32 col = active ? Theme::accentU32() : lerpColor(Theme::textDim, Theme::text, hover);
    Icons::draw(d, Icon(icon), pos + ImVec2(size, size) * 0.5f, size * 0.5f, col);
    return clicked;
}

bool Widgets::button(const char* label, float width, ImU32 color) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(width, 30.f);
    bool clicked = ImGui::InvisibleButton(label, size);
    float hover = animate(ImGui::GetItemID(), ImGui::IsItemHovered());
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
    c.w *= 0.16f + 0.14f * hover;
    d->AddRectFilled(pos, pos + size, ImGui::ColorConvertFloat4ToU32(c), 4.f);
    d->AddRect(pos, pos + size, color, 4.f, 0, 1.f);
    const char* end = ImGui::FindRenderedTextEnd(label);
    ImVec2 ts = ImGui::CalcTextSize(label, end);
    d->AddText(pos + (size - ts) * 0.5f, Theme::text, label, end);
    return clicked;
}

bool Widgets::keybind(const char* label, int key, bool listening, float width) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    const float h = 26.f;
    ImGui::PushID(label);
    ImDrawList* d = ImGui::GetWindowDrawList();
    rowLabel(d, pos, h, label);

    char text[64];
    if (listening) std::snprintf(text, sizeof(text), "...");
    else std::snprintf(text, sizeof(text), "%s", Keys::name(key).c_str());
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImVec2 boxSize(ImMax(56.f, ts.x + 18.f), 22.f);
    ImVec2 boxPos(pos.x + width - boxSize.x, pos.y + (h - boxSize.y) * 0.5f);

    ImGui::SetCursorScreenPos(boxPos);
    bool clicked = ImGui::InvisibleButton("##bind", boxSize);
    bool hovered = ImGui::IsItemHovered();
    d->AddRectFilled(boxPos, boxPos + boxSize, hovered ? Theme::rowBgHover : Theme::rowBg, 4.f);
    d->AddRect(boxPos, boxPos + boxSize, listening ? Theme::accentU32() : Theme::windowBorder, 4.f);
    d->AddText(boxPos + (boxSize - ts) * 0.5f, listening ? Theme::accentU32() : Theme::text, text);

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, h));
    ImGui::PopID();
    return clicked;
}

void Widgets::setting(Setting* s, float width) {
    ImGui::PushID(s);
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    switch (s->type) {
    case SettingType::Bool: {
        auto* b = static_cast<BoolSetting*>(s);
        const float h = 24.f;
        rowLabel(d, pos, h, b->name.c_str());
        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - 30.f, pos.y + (h - 15.f) * 0.5f));
        toggle("##toggle", &b->value);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, h));
        break;
    }
    case SettingType::Slider: {
        auto* sl = static_cast<SliderSetting*>(s);
        const float h = 38.f;
        rowLabel(d, pos, 22.f, sl->name.c_str());
        char value[32];
        std::snprintf(value, sizeof(value), "%.*f", sl->decimals, sl->value);
        ImVec2 vs = ImGui::CalcTextSize(value);
        d->AddText(ImVec2(pos.x + width - vs.x, pos.y + (22.f - vs.y) * 0.5f), Theme::text, value);

        ImVec2 barPos(pos.x, pos.y + 26.f);
        ImVec2 barSize(width, 4.f);
        ImGui::SetCursorScreenPos(ImVec2(barPos.x, barPos.y - 6.f));
        ImGui::InvisibleButton("##slider", ImVec2(barSize.x, barSize.y + 12.f));
        if (ImGui::IsItemActive()) {
            float t = ImSaturate((ImGui::GetIO().MousePos.x - barPos.x) / barSize.x);
            float v = sl->min + (sl->max - sl->min) * t;
            float step = 1.f;
            for (int i = 0; i < sl->decimals; i++) step *= 0.1f;
            sl->value = ImClamp(ImFloor(v / step + 0.5f) * step, sl->min, sl->max);
        }
        float t = (sl->value - sl->min) / (sl->max - sl->min);
        d->AddRectFilled(barPos, barPos + barSize, Theme::toggleOff, 2.f);
        d->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x * t, barPos.y + barSize.y), Theme::accentU32(), 2.f);
        float knobR = ImGui::IsItemActive() || ImGui::IsItemHovered() ? 6.f : 5.f;
        d->AddCircleFilled(ImVec2(barPos.x + barSize.x * t, barPos.y + 2.f), knobR, Theme::text, 16);

        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, h));
        break;
    }
    case SettingType::Mode: {
        auto* m = static_cast<ModeSetting*>(s);
        const float h = 26.f;
        rowLabel(d, pos, h, m->name.c_str());
        const char* text = m->current().c_str();
        ImVec2 ts = ImGui::CalcTextSize(text);
        ImVec2 boxSize(ts.x + 20.f, 22.f);
        ImVec2 boxPos(pos.x + width - boxSize.x, pos.y + (h - boxSize.y) * 0.5f);
        ImGui::SetCursorScreenPos(boxPos);
        ImGui::InvisibleButton("##mode", boxSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        int count = int(m->modes.size());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) m->index = (m->index + 1) % count;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) m->index = (m->index + count - 1) % count;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("left click next, right click previous");
        d->AddRectFilled(boxPos, boxPos + boxSize, ImGui::IsItemHovered() ? Theme::rowBgHover : Theme::rowBg, 4.f);
        d->AddText(boxPos + (boxSize - ImGui::CalcTextSize(m->current().c_str())) * 0.5f, Theme::accentU32(), m->current().c_str());
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, h));
        break;
    }
    case SettingType::Color: {
        auto* c = static_cast<ColorSetting*>(s);
        const float h = 26.f;
        rowLabel(d, pos, h, c->name.c_str());
        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - 34.f, pos.y + 3.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::ColorEdit4("##color", c->value,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar |
                              ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::PopStyleVar(2);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, h));
        break;
    }
    }
    ImGui::PopID();
}
