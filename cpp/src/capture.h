// Снимок экрана средствами GDI.
//
// Desktop Duplication API быстрее, но для пошаговой игры избыточен: доску
// достаточно снимать несколько раз в секунду, а BitBlt на таком темпе
// обходится дешевле, чем возня с DXGI.

#pragma once

#include <cstdint>
#include <vector>

#include <windows.h>

namespace app {

// Пиксель хранится как 0x00RRGGBB.
struct Frame {
    int width = 0;
    int height = 0;
    int originX = 0;  // экранные координаты левого верхнего угла:
    int originY = 0;  // на нескольких мониторах бывают отрицательными
    std::vector<std::uint32_t> pixels;

    std::uint32_t at(int x, int y) const { return pixels[static_cast<std::size_t>(y) * width + x]; }
};

inline int red(std::uint32_t c) { return static_cast<int>((c >> 16) & 0xFF); }
inline int green(std::uint32_t c) { return static_cast<int>((c >> 8) & 0xFF); }
inline int blue(std::uint32_t c) { return static_cast<int>(c & 0xFF); }

class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();

    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    bool grab(Frame& frame);

private:
    void release();
    bool ensureSurface(int width, int height);

    HDC screenDC_ = nullptr;
    HDC memoryDC_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    std::uint32_t* bits_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace app
