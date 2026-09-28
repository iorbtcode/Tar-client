#include "module/modules/client/Notifications.h"

#include <deque>

#include <imgui_internal.h>

#include "gui/Theme.h"

namespace {
    struct Toast {
        std::string text;
        bool positive;
        double start;
    };
    std::deque<Toast> toasts;
    Notifications* instance = nullptr;

    double now() { return ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0; }
}

Notifications::Notifications()
    : Module("Notifications", "Shows a popup in the corner when you toggle a module", Category::Client, Icon::Bell) {
    duration = addSlider("Duration", 2.f, 0.5f, 6.f, 1);
    instance = this;
    setEnabled(true);
}

void Notifications::push(const std::string& text, bool positive, bool always) {
    if (muted || !ImGui::GetCurrentContext()) return;
    if (!always && !(instance && instance->isEnabled())) return;
    toasts.push_back({ text, positive, now() });
    while (toasts.size() > 6) toasts.pop_front();
}

void Notifications::renderToasts(ImDrawList* draw) {
    const float life = instance ? instance->duration->value : 2.f;
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float y = display.y - 20.f;
    double t = now();

    while (!toasts.empty() && t - toasts.front().start > life + 0.3) toasts.pop_front();

    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        float age = float(t - it->start);
        // slide in, wait, slide out
        float in = ImSaturate(age / 0.2f);
        float out = ImSaturate((life + 0.3f - age) / 0.3f);
        float a = ImMin(in, out);

        ImVec2 ts = ImGui::CalcTextSize(it->text.c_str());
        ImVec2 size(ts.x + 34.f, 34.f);
        float x = display.x - 20.f - size.x + (1.f - a) * 40.f;
        y -= size.y;
        ImVec2 pos(x, y);

        ImU32 accent = it->positive ? Theme::accentU32(a) : IM_COL32(230, 72, 72, int(255 * a));
        draw->AddRectFilled(pos, pos + size, IM_COL32(18, 19, 20, int(235 * a)), 5.f);
        draw->AddRectFilled(pos, pos + ImVec2(3.f, size.y), accent, 5.f, ImDrawFlags_RoundCornersLeft);
        draw->AddCircleFilled(pos + ImVec2(15.f, size.y * 0.5f), 3.5f, accent);
        draw->AddText(pos + ImVec2(26.f, (size.y - ts.y) * 0.5f), IM_COL32(232, 234, 236, int(255 * a)), it->text.c_str());
        y -= 8.f;
    }
}
