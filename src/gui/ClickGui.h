#pragma once

namespace ClickGui {
    void render();

    bool isOpen();
    void setOpen(bool open);

    // keybind capture, while listening the next key press goes to the gui instead of the game
    bool isBinding();
    void finishBinding(int vk); // vk 0 = clear
}
