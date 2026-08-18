// Прозрачное окно поверх всех остальных.
//
// WS_EX_TRANSPARENT делает окно сквозным для мыши и клавиатуры, поэтому играть
// можно как обычно. Содержимое отдаётся системе через UpdateLayeredWindow
// целым кадром с попиксельной альфой, так что WM_PAINT здесь не участвует.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include "board.h"
#include "bitboard.h"

namespace app {

class Overlay {
public:
    bool create(HINSTANCE instance);
    void destroy();

    HWND handle() const { return window_; }

    void render(const std::optional<Rect>& board, std::optional<bb::Move> move, const std::wstring& status);

private:
    bool ensureSurface(int width, int height);

    HWND window_ = nullptr;
    HDC memoryDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    void* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int originX_ = 0;
    int originY_ = 0;
};

}  // namespace app
