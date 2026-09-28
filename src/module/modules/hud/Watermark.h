#pragma once
#include "core/Client.h"
#include "gui/Theme.h"
#include "module/HudModule.h"

// The client name in the corner
class Watermark : public HudModule {
public:
    Watermark() : HudModule("Watermark", "Shows the client name on screen", Category::Hud, Icon::Tag, 10.f, 10.f) {
        showVersion = addBool("Show version", true);
        scale = addSlider("Scale", 1.3f, 0.8f, 2.5f, 1);
        setEnabled(true);
    }

    ImVec2 drawHud(ImDrawList* draw, ImVec2 pos) override {
        ImFont* font = Theme::bold;
        float size = font->FontSize * scale->value;
        const char* first = "Tar";
        const char* second = " Client";
        ImVec2 a = font->CalcTextSizeA(size, FLT_MAX, 0.f, first);
        ImVec2 b = font->CalcTextSizeA(size, FLT_MAX, 0.f, second);

        // soft shadow then the text
        draw->AddText(font, size, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 120), first);
        draw->AddText(font, size, pos, Theme::accentU32(), first);
        draw->AddText(font, size, ImVec2(pos.x + a.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 120), second);
        draw->AddText(font, size, ImVec2(pos.x + a.x, pos.y), Theme::text, second);

        float w = a.x + b.x;
        if (showVersion->value) {
            const char* version = "v" TAR_CLIENT_VERSION;
            float vs = size * 0.55f;
            draw->AddText(Theme::regular, vs, ImVec2(pos.x + w + 4.f, pos.y + size - vs - 2.f), Theme::textDim, version);
            w += 4.f + Theme::regular->CalcTextSizeA(vs, FLT_MAX, 0.f, version).x;
        }
        return ImVec2(w, size);
    }

private:
    BoolSetting* showVersion;
    SliderSetting* scale;
};
