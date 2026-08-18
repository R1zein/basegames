#include "input.h"

namespace app {
namespace {

WORD virtualKey(bb::Move move) {
    switch (move) {
        case bb::Move::Left:  return VK_LEFT;
        case bb::Move::Right: return VK_RIGHT;
        case bb::Move::Up:    return VK_UP;
        case bb::Move::Down:  return VK_DOWN;
    }
    return 0;
}

}  // namespace

void sendMove(bb::Move move) {
    const WORD key = virtualKey(move);
    if (key == 0) {
        return;
    }

    INPUT events[2] = {};
    events[0].type = INPUT_KEYBOARD;
    events[0].ki.wVk = key;
    events[1].type = INPUT_KEYBOARD;
    events[1].ki.wVk = key;
    events[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, events, sizeof(INPUT));
}

bool boardWindowIsActive(int screenX, int screenY) {
    const POINT point = {screenX, screenY};
    // WindowFromPoint пропускает окна с WS_EX_TRANSPARENT, поэтому собственный
    // оверлей поверх доски результату не мешает.
    const HWND under = WindowFromPoint(point);
    if (under == nullptr) {
        return false;
    }
    return GetAncestor(under, GA_ROOT) == GetForegroundWindow();
}

}  // namespace app
