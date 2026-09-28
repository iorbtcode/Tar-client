#include "gui/ClickGui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui_internal.h>

#include "config/Config.h"
#include "core/Client.h"
#include "gui/Icons.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"
#include "module/ModuleManager.h"
#include "module/modules/client/Notifications.h"
#include "util/Keys.h"

namespace {
    enum class Page { Home, Category, Settings, Friends };

    constexpr float WIDTH = 660.f;
    constexpr float HEIGHT = 400.f;
    constexpr float TITLE_H = 36.f;
    constexpr float SIDEBAR_W = 142.f;

    bool open = false;
    bool collapsed = false;
    float openAnim = 0.f;
    Page page = Page::Category;
    Category selected = Category::Hud;
    char search[64] = {};
    char friendInput[32] = {};
    int* bindTarget = nullptr;

    bool containsNoCase(const std::string& haystack, const char* needle) {
        auto it = std::search(haystack.begin(), haystack.end(), needle, needle + std::strlen(needle),
                              [](char a, char b) { return std::tolower(uint8_t(a)) == std::tolower(uint8_t(b)); });
        return it != haystack.end();
    }

    // multiplies the alpha of everything drawn since vtxStart, used for the fade animations
    void fade(ImDrawList* d, int vtxStart, float alpha) {
        if (alpha >= 1.f) return;
        for (int i = vtxStart; i < d->VtxBuffer.Size; i++) {
            ImU32& c = d->VtxBuffer[i].col;
            uint32_t a = (c >> IM_COL32_A_SHIFT) & 0xFF;
            c = (c & ~IM_COL32_A_MASK) | (uint32_t(a * alpha) << IM_COL32_A_SHIFT);
        }
    }

    void heading(const char* title, const char* subtitle) {
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        d->AddText(Theme::large, Theme::large->FontSize, pos, Theme::text, title);
        d->AddText(ImVec2(pos.x, pos.y + Theme::large->FontSize + 2.f), Theme::textDim, subtitle);
        ImGui::Dummy(ImVec2(1, Theme::large->FontSize + ImGui::GetFontSize() + 12.f));
    }

    void moduleRow(Module* m, float width) {
        ImGui::PushID(m);
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        const float h = 36.f;
        const float iconW = 34.f;

        // the row itself, clicking it opens the settings
        ImGui::InvisibleButton("##row", ImVec2(width - 56.f, h), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
            m->expanded = !m->expanded;
        if (hovered && !m->expanded && GImGui->HoveredIdTimer > 0.6f)
            ImGui::SetTooltip("%s", m->getDescription().c_str());

        d->AddRectFilled(pos, pos + ImVec2(width, h), hovered ? Theme::rowBgHover : Theme::rowBg, 4.f);
        d->AddRectFilled(pos, pos + ImVec2(iconW, h), Theme::accentU32(), 4.f, ImDrawFlags_RoundCornersLeft);
        Icons::draw(d, m->getIcon(), pos + ImVec2(iconW * 0.5f, h * 0.5f), 14.f, Theme::textDark);

        ImFont* f = Theme::bold;
        d->AddText(f, f->FontSize, ImVec2(pos.x + iconW + 14.f, pos.y + (h - f->FontSize) * 0.5f), Theme::text, m->getName().c_str());

        if (m->key != 0) {
            std::string key = Keys::name(m->key);
            ImVec2 ks = ImGui::CalcTextSize(key.c_str());
            d->AddText(ImVec2(pos.x + width - 82.f - ks.x, pos.y + (h - ks.y) * 0.5f), Theme::textDim, key.c_str());
        }
        Icons::draw(d, Icon::ChevronDown, ImVec2(pos.x + width - 62.f, pos.y + h * 0.5f), 9.f,
                    m->expanded ? Theme::accentU32() : Theme::textDim);

        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - 44.f, pos.y + (h - 15.f) * 0.5f));
        bool enabled = m->isEnabled();
        if (Widgets::toggle("##enabled", &enabled)) m->setEnabled(enabled);

        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, h));

        // settings panel
        m->expandAnim = ImLerp(m->expandAnim, m->expanded ? 1.f : 0.f, ImMin(1.f, ImGui::GetIO().DeltaTime * 14.f));
        if (m->expanded) {
            ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() - ImVec2(0, 4.f));
            ImVec2 panelPos = ImGui::GetCursorScreenPos();
            int vtxStart = d->VtxBuffer.Size;
            d->ChannelsSplit(2);
            d->ChannelsSetCurrent(1);

            const float pad = 14.f;
            float inner = width - pad * 2.f;
            ImGui::SetCursorScreenPos(panelPos + ImVec2(pad, 12.f));
            ImGui::BeginGroup();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + inner);
            ImGui::TextDisabled("%s", m->getDescription().c_str());
            ImGui::PopTextWrapPos();
            for (auto& s : m->getSettings())
                if (!s->hidden) Widgets::setting(s.get(), inner);
            if (Widgets::keybind("Keybind", m->key, bindTarget == &m->key, inner))
                bindTarget = &m->key;
            ImGui::EndGroup();

            float panelH = ImGui::GetItemRectMax().y - panelPos.y + 10.f;
            d->ChannelsSetCurrent(0);
            d->AddRectFilled(panelPos, panelPos + ImVec2(width, panelH), Theme::panelBg, 4.f, ImDrawFlags_RoundCornersBottom);
            d->AddRectFilled(panelPos, panelPos + ImVec2(2.f, panelH), Theme::accentU32(0.6f), 0.f);
            d->ChannelsMerge();
            fade(d, vtxStart, m->expandAnim);

            ImGui::SetCursorScreenPos(panelPos);
            ImGui::Dummy(ImVec2(width, panelH));
        }
        ImGui::PopID();
    }

    void moduleList(const std::vector<Module*>& modules, float width) {
        if (modules.empty()) {
            ImGui::TextDisabled("no modules found");
            return;
        }
        for (Module* m : modules) moduleRow(m, width);
    }

    void statCard(const char* label, const char* value, Icon icon, float width) {
        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(width, 70.f);
        d->AddRectFilled(pos, pos + size, Theme::rowBg, 6.f);
        Icons::draw(d, icon, pos + ImVec2(22.f, 22.f), 14.f, Theme::accentU32());
        d->AddText(pos + ImVec2(38.f, 13.f), Theme::textDim, label);
        d->AddText(Theme::large, Theme::large->FontSize, pos + ImVec2(14.f, 36.f), Theme::text, value);
        ImGui::Dummy(size);
    }

    void homePage(float width) {
        heading("Welcome back", TAR_CLIENT_NAME " v" TAR_CLIENT_VERSION " for " TAR_CLIENT_EDITION);

        int enabled = 0;
        for (auto& m : ModuleManager::all()) enabled += m->isEnabled();
        char modules[32], session[32];
        std::snprintf(modules, sizeof(modules), "%d / %d", enabled, int(ModuleManager::all().size()));
        int secs = int(Client::uptime());
        std::snprintf(session, sizeof(session), "%02d:%02d:%02d", secs / 3600, secs / 60 % 60, secs % 60);
        std::string menuKey = Keys::name(Client::menuKey);

        float cardW = (width - 16.f) / 3.f;
        statCard("Modules on", modules, Icon::List, cardW);
        ImGui::SameLine(0, 8.f);
        statCard("Session", session, Icon::Timer, cardW);
        ImGui::SameLine(0, 8.f);
        statCard("Menu key", menuKey.c_str(), Icon::Keyboard, cardW);

        ImGui::Dummy(ImVec2(1, 6));
        ImGui::TextDisabled("tips");
        ImGui::BulletText("click a module to open its settings");
        ImGui::BulletText("drag hud elements around while this menu is open");
        ImGui::BulletText("your config saves when you close the menu");
        ImGui::BulletText("the search box looks through every category");
    }

    void settingsPage(float width) {
        heading("Settings", "client options");

        if (Widgets::keybind("Menu key", Client::menuKey, bindTarget == &Client::menuKey, width))
            bindTarget = &Client::menuKey;

        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        d->AddText(ImVec2(pos.x, pos.y + 5.f), Theme::textDim, "Accent color");
        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - 34.f, pos.y + 3.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
        ImGui::ColorEdit3("##accent", &Theme::accent.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::PopStyleVar(2);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, 26.f));

        ImGui::Dummy(ImVec2(1, 8));
        float bw = (width - 16.f) / 3.f;
        if (Widgets::button("Save config", bw, Theme::accentU32())) {
            Config::save();
            Notifications::push("Config saved", true, true);
        }
        ImGui::SameLine(0, 8.f);
        if (Widgets::button("Reload config", bw, Theme::accentU32())) {
            Config::load();
            Notifications::push("Config reloaded", true, true);
        }
        ImGui::SameLine(0, 8.f);
        if (Widgets::button("Unload client", bw, Theme::danger)) Client::requestUnload();

        ImGui::Dummy(ImVec2(1, 6));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        ImGui::TextDisabled("config file: %s", Config::path().c_str());
        ImGui::PopTextWrapPos();
    }

    void friendsPage(float width) {
        heading("Friends", "people you play with");

        ImDrawList* d = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float inputW = width - 88.f;
        d->AddRectFilled(pos, pos + ImVec2(inputW, 30.f), Theme::rowBg, 4.f);
        ImGui::SetCursorScreenPos(pos + ImVec2(4.f, 2.f));
        ImGui::SetNextItemWidth(inputW - 8.f);
        bool submit = ImGui::InputTextWithHint("##friend", "gamertag", friendInput, sizeof(friendInput),
                                               ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + inputW + 8.f, pos.y));
        submit |= Widgets::button("Add", 80.f, Theme::accentU32());
        if (submit && friendInput[0]) {
            std::string name = friendInput;
            if (std::find(Client::friends.begin(), Client::friends.end(), name) == Client::friends.end())
                Client::friends.push_back(name);
            friendInput[0] = 0;
        }
        ImGui::Dummy(ImVec2(1, 4));

        if (Client::friends.empty()) ImGui::TextDisabled("no friends added yet");
        for (size_t i = 0; i < Client::friends.size(); i++) {
            ImGui::PushID(int(i));
            ImVec2 p = ImGui::GetCursorScreenPos();
            d->AddRectFilled(p, p + ImVec2(width, 32.f), Theme::rowBg, 4.f);
            Icons::draw(d, Icon::Users, p + ImVec2(18.f, 16.f), 13.f, Theme::accentU32());
            d->AddText(p + ImVec2(36.f, 16.f - ImGui::GetFontSize() * 0.5f), Theme::text, Client::friends[i].c_str());
            ImGui::SetCursorScreenPos(p + ImVec2(width - 30.f, 5.f));
            bool remove = Widgets::iconButton("##remove", int(Icon::Close), 22.f);
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy(ImVec2(width, 32.f));
            ImGui::PopID();
            if (remove) {
                Client::friends.erase(Client::friends.begin() + i);
                break;
            }
        }
    }
}

bool ClickGui::isOpen() { return open; }

void ClickGui::setOpen(bool value) {
    if (open == value) return;
    open = value;
    if (!open) {
        bindTarget = nullptr;
        Config::save();
    }
}

bool ClickGui::isBinding() { return open && bindTarget != nullptr; }

void ClickGui::finishBinding(int vk) {
    if (bindTarget) *bindTarget = vk;
    bindTarget = nullptr;
}

void ClickGui::render() {
    ImGuiIO& io = ImGui::GetIO();
    openAnim = ImLerp(openAnim, open ? 1.f : 0.f, ImMin(1.f, io.DeltaTime * 14.f));
    if (!open && openAnim < 0.01f) {
        openAnim = 0.f;
        return;
    }

    float height = collapsed ? TITLE_H + 4.f : HEIGHT;
    ImGui::SetNextWindowSize(ImVec2(WIDTH, height));
    ImGui::SetNextWindowPos(io.DisplaySize * 0.5f, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;
    if (!open) flags |= ImGuiWindowFlags_NoInputs;
    ImGui::Begin("##tarclient", nullptr, flags);

    ImDrawList* d = ImGui::GetWindowDrawList();
    int vtxStart = d->VtxBuffer.Size;
    ImVec2 p = ImGui::GetWindowPos();

    d->AddRectFilled(p, p + ImVec2(WIDTH, height), Theme::windowBg, 8.f);
    d->AddRect(p, p + ImVec2(WIDTH, height), Theme::windowBorder, 8.f, 0, 1.f);

    // ---- title bar (drag to move) ----
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##drag", ImVec2(WIDTH - 80.f, TITLE_H));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f))
        ImGui::SetWindowPos(ImGui::GetWindowPos() + io.MouseDelta);
    d->AddText(Theme::bold, 13.f, p + ImVec2(20.f, 12.f), Theme::textDim, "TAR CLIENT");

    ImGui::SetCursorPos(ImVec2(WIDTH - 64.f, 7.f));
    if (Widgets::iconButton("##min", int(Icon::Minus), 22.f)) collapsed = !collapsed;
    ImGui::SetCursorPos(ImVec2(WIDTH - 36.f, 7.f));
    if (Widgets::iconButton("##close", int(Icon::Close), 22.f)) setOpen(false);

    if (!collapsed) {
        // ---- top bar ----
        const float barY = 44.f;
        ImGui::SetCursorPos(ImVec2(16.f, barY));
        if (Widgets::iconButton("##home", int(Icon::Home), 30.f, page == Page::Home && !search[0])) {
            page = Page::Home;
            search[0] = 0;
        }

        ImVec2 boxPos = p + ImVec2(84.f, barY + 1.f);
        ImVec2 boxSize(190.f, 28.f);
        bool searching = search[0] != 0;
        ImGuiID searchId = ImGui::GetID("##search");
        bool focused = ImGui::GetActiveID() == searchId;
        Icons::draw(d, Icon::Search, p + ImVec2(66.f, barY + 15.f), 14.f,
                    focused || searching ? Theme::accentU32() : Theme::textDim);
        d->AddRectFilled(boxPos, boxPos + boxSize, Theme::rowBg, 4.f);
        d->AddRect(boxPos, boxPos + boxSize, focused || searching ? Theme::accentU32() : Theme::windowBorder, 4.f, 0,
                   focused ? 1.5f : 1.f);
        ImGui::SetCursorPos(ImVec2(88.f, barY + 2.f));
        ImGui::SetNextItemWidth(boxSize.x - 8.f);
        ImGui::InputTextWithHint("##search", "Search modules", search, sizeof(search));
        searching = search[0] != 0;

        ImGui::SetCursorPos(ImVec2(WIDTH - 84.f, barY));
        if (Widgets::iconButton("##reload", int(Icon::Refresh), 30.f)) {
            Config::load();
            Notifications::push("Config reloaded", true, true);
        }
        ImGui::SetCursorPos(ImVec2(WIDTH - 48.f, barY));
        if (Widgets::iconButton("##settings", int(Icon::Gear), 30.f, page == Page::Settings && !searching)) {
            page = Page::Settings;
            search[0] = 0;
        }

        // ---- sidebar ----
        for (int i = 0; i < int(Category::Count); i++) {
            Category cat = Category(i);
            ImGui::SetCursorPos(ImVec2(16.f, 94.f + i * 36.f));
            ImVec2 itemPos = ImGui::GetCursorScreenPos();
            bool active = page == Page::Category && selected == cat && !searching;
            if (ImGui::InvisibleButton(categoryName(cat), ImVec2(SIDEBAR_W - 30.f, 28.f))) {
                page = Page::Category;
                selected = cat;
                search[0] = 0;
            }
            bool hovered = ImGui::IsItemHovered();
            if (active) d->AddRectFilled(itemPos + ImVec2(0, 7.f), itemPos + ImVec2(2.5f, 21.f), Theme::accentU32(), 2.f);
            ImU32 col = active ? Theme::text : hovered ? IM_COL32(200, 202, 205, 255) : Theme::textDim;
            ImFont* f = active ? Theme::bold : Theme::regular;
            d->AddText(f, f->FontSize, itemPos + ImVec2(10.f, 14.f - f->FontSize * 0.5f), col, categoryName(cat));
        }

        ImGui::SetCursorPos(ImVec2(16.f, HEIGHT - 46.f));
        if (Widgets::iconButton("##friends", int(Icon::Users), 30.f, page == Page::Friends && !searching)) {
            page = Page::Friends;
            search[0] = 0;
        }

        // ---- content ----
        ImGui::SetCursorPos(ImVec2(SIDEBAR_W, 88.f));
        ImGui::BeginChild("##content", ImVec2(WIDTH - SIDEBAR_W - 18.f, HEIGHT - 88.f - 16.f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground);
        ImDrawList* cd = ImGui::GetWindowDrawList();
        int contentVtx = cd->VtxBuffer.Size;
        float width = ImGui::GetContentRegionAvail().x - 8.f;

        if (search[0]) {
            std::vector<Module*> found;
            for (auto& m : ModuleManager::all())
                if (containsNoCase(m->getName(), search)) found.push_back(m.get());
            moduleList(found, width);
        } else {
            switch (page) {
            case Page::Home: homePage(width); break;
            case Page::Category: moduleList(ModuleManager::inCategory(selected), width); break;
            case Page::Settings: settingsPage(width); break;
            case Page::Friends: friendsPage(width); break;
            }
        }
        ImGui::Dummy(ImVec2(1, 4));
        fade(cd, contentVtx, openAnim);
        ImGui::EndChild();
    }

    fade(d, vtxStart, openAnim);
    ImGui::End();
}
