#include "gui/Icons.h"

#include <cmath>

namespace {
    constexpr float PI = 3.14159265f;
}

void Icons::draw(ImDrawList* d, Icon icon, ImVec2 c, float size, ImU32 col, float t) {
    const float s = size * 0.5f;
    auto P = [&](float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); };
    auto line = [&](float x1, float y1, float x2, float y2) { d->AddLine(P(x1, y1), P(x2, y2), col, t); };

    switch (icon) {
    case Icon::None:
        break;

    case Icon::Home: {
        ImVec2 roof[] = { P(-1.f, -0.05f), P(0.f, -0.95f), P(1.f, -0.05f) };
        d->AddPolyline(roof, 3, col, 0, t);
        ImVec2 body[] = { P(-0.72f, -0.3f), P(-0.72f, 0.95f), P(0.72f, 0.95f), P(0.72f, -0.3f) };
        d->AddPolyline(body, 4, col, 0, t);
        ImVec2 door[] = { P(-0.22f, 0.95f), P(-0.22f, 0.35f), P(0.22f, 0.35f), P(0.22f, 0.95f) };
        d->AddPolyline(door, 4, col, 0, t);
        break;
    }
    case Icon::Search:
        d->AddCircle(P(-0.15f, -0.15f), s * 0.62f, col, 24, t);
        line(0.3f, 0.3f, 0.95f, 0.95f);
        break;

    case Icon::Refresh: {
        d->PathArcTo(c, s * 0.8f, -PI * 0.25f, PI * 1.45f, 24);
        d->PathStroke(col, 0, t);
        ImVec2 tip(c.x + std::cos(-PI * 0.25f) * s * 0.8f, c.y + std::sin(-PI * 0.25f) * s * 0.8f);
        d->AddLine(tip, ImVec2(tip.x, tip.y - s * 0.55f), col, t);
        d->AddLine(tip, ImVec2(tip.x - s * 0.55f, tip.y), col, t);
        break;
    }
    case Icon::Gear:
        d->AddCircle(c, s * 0.28f, col, 16, t);
        d->AddCircle(c, s * 0.66f, col, 24, t);
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4.f;
            ImVec2 dir(std::cos(a), std::sin(a));
            d->AddLine(ImVec2(c.x + dir.x * s * 0.66f, c.y + dir.y * s * 0.66f),
                       ImVec2(c.x + dir.x * s * 0.98f, c.y + dir.y * s * 0.98f), col, t * 1.8f);
        }
        break;

    case Icon::Users:
        d->AddCircle(P(-0.25f, -0.4f), s * 0.36f, col, 16, t);
        d->PathArcTo(P(-0.25f, 0.95f), s * 0.72f, PI, PI * 2.f, 16);
        d->PathStroke(col, 0, t);
        d->PathArcTo(P(0.45f, -0.4f), s * 0.36f, -PI * 0.5f, PI * 0.5f, 12);
        d->PathStroke(col, 0, t);
        d->PathArcTo(P(0.45f, 0.95f), s * 0.72f, PI * 1.55f, PI * 2.f, 10);
        d->PathStroke(col, 0, t);
        break;

    case Icon::Minus:
        line(-0.8f, 0.f, 0.8f, 0.f);
        break;

    case Icon::Close:
        line(-0.75f, -0.75f, 0.75f, 0.75f);
        line(0.75f, -0.75f, -0.75f, 0.75f);
        break;

    case Icon::ChevronDown: {
        ImVec2 pts[] = { P(-0.7f, -0.3f), P(0.f, 0.4f), P(0.7f, -0.3f) };
        d->AddPolyline(pts, 3, col, 0, t);
        break;
    }
    case Icon::Tag: {
        ImVec2 pts[] = { P(-0.95f, -0.95f), P(0.05f, -0.95f), P(0.95f, -0.05f), P(-0.05f, 0.95f), P(-0.95f, 0.05f) };
        d->AddPolyline(pts, 5, col, ImDrawFlags_Closed, t);
        d->AddCircleFilled(P(-0.45f, -0.45f), s * 0.16f, col);
        break;
    }
    case Icon::List:
        for (int i = -1; i <= 1; i++) {
            float y = i * 0.65f;
            d->AddCircleFilled(P(-0.85f, y), s * 0.12f, col);
            line(-0.45f, y, 0.95f, y);
        }
        break;

    case Icon::Gauge:
        d->PathArcTo(P(0.f, 0.25f), s * 0.9f, PI * 0.85f, PI * 2.15f, 24);
        d->PathStroke(col, 0, t);
        line(0.f, 0.25f, 0.45f, -0.3f);
        d->AddCircleFilled(P(0.f, 0.25f), s * 0.14f, col);
        break;

    case Icon::Mouse:
        d->AddRect(P(-0.6f, -0.95f), P(0.6f, 0.95f), col, s * 0.6f, 0, t);
        line(0.f, -0.95f, 0.f, -0.3f);
        line(-0.6f, -0.3f, 0.6f, -0.3f);
        break;

    case Icon::Keyboard:
        d->AddRect(P(-1.f, -0.65f), P(1.f, 0.65f), col, 2.f, 0, t);
        for (int row = 0; row < 2; row++)
            for (int k = 0; k < 4; k++)
                d->AddRectFilled(P(-0.68f + k * 0.45f - 0.08f, -0.35f + row * 0.35f - 0.08f),
                                 P(-0.68f + k * 0.45f + 0.08f, -0.35f + row * 0.35f + 0.08f), col);
        line(-0.45f, 0.38f, 0.45f, 0.38f);
        break;

    case Icon::Crosshair:
        d->AddCircle(c, s * 0.62f, col, 24, t);
        line(0.f, -1.f, 0.f, -0.3f);
        line(0.f, 0.3f, 0.f, 1.f);
        line(-1.f, 0.f, -0.3f, 0.f);
        line(0.3f, 0.f, 1.f, 0.f);
        break;

    case Icon::Monitor:
        d->AddRect(P(-1.f, -0.8f), P(1.f, 0.45f), col, 2.f, 0, t);
        line(0.f, 0.45f, 0.f, 0.85f);
        line(-0.5f, 0.9f, 0.5f, 0.9f);
        break;

    case Icon::Clock:
        d->AddCircle(c, s * 0.92f, col, 24, t);
        line(0.f, 0.f, 0.f, -0.55f);
        line(0.f, 0.f, 0.4f, 0.2f);
        break;

    case Icon::Timer:
        d->AddCircle(P(0.f, 0.12f), s * 0.8f, col, 24, t);
        line(-0.3f, -0.95f, 0.3f, -0.95f);
        line(0.f, 0.12f, 0.f, -0.35f);
        break;

    case Icon::Bell:
        d->PathArcTo(P(0.f, -0.2f), s * 0.6f, PI, PI * 2.f, 14);
        d->PathLineTo(P(0.6f, 0.45f));
        d->PathLineTo(P(0.9f, 0.65f));
        d->PathLineTo(P(-0.9f, 0.65f));
        d->PathLineTo(P(-0.6f, 0.45f));
        d->PathStroke(col, ImDrawFlags_Closed, t);
        d->AddCircleFilled(P(0.f, 0.9f), s * 0.16f, col);
        break;

    case Icon::Palette:
        d->AddCircle(c, s * 0.92f, col, 24, t);
        d->AddCircleFilled(P(-0.35f, -0.3f), s * 0.15f, col);
        d->AddCircleFilled(P(0.3f, -0.4f), s * 0.15f, col);
        d->AddCircleFilled(P(0.45f, 0.2f), s * 0.15f, col);
        d->AddCircleFilled(P(-0.25f, 0.4f), s * 0.2f, col);
        break;

    case Icon::Eye:
        d->PathLineTo(P(-1.f, 0.f));
        d->PathBezierCubicCurveTo(P(-0.5f, -0.85f), P(0.5f, -0.85f), P(1.f, 0.f));
        d->PathBezierCubicCurveTo(P(0.5f, 0.85f), P(-0.5f, 0.85f), P(-1.f, 0.f));
        d->PathStroke(col, ImDrawFlags_Closed, t);
        d->AddCircle(c, s * 0.3f, col, 12, t);
        break;
    }
}
