#include "capture.h"

namespace app {

ScreenCapture::ScreenCapture() {
    screenDC_ = GetDC(nullptr);
    memoryDC_ = CreateCompatibleDC(screenDC_);
}

ScreenCapture::~ScreenCapture() {
    release();
    if (memoryDC_ != nullptr) {
        DeleteDC(memoryDC_);
    }
    if (screenDC_ != nullptr) {
        ReleaseDC(nullptr, screenDC_);
    }
}

void ScreenCapture::release() {
    if (bitmap_ != nullptr) {
        SelectObject(memoryDC_, previous_);
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
        bits_ = nullptr;
    }
}

bool ScreenCapture::ensureSurface(int width, int height) {
    if (bitmap_ != nullptr && width == width_ && height == height_) {
        return true;
    }
    release();

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    // Отрицательная высота даёт top-down порядок строк: иначе картинка
    // приходит перевёрнутой и координаты пришлось бы пересчитывать.
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    bitmap_ = CreateDIBSection(screenDC_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (bitmap_ == nullptr) {
        return false;
    }

    previous_ = SelectObject(memoryDC_, bitmap_);
    bits_ = static_cast<std::uint32_t*>(pixels);
    width_ = width;
    height_ = height;
    return true;
}

bool ScreenCapture::grab(Frame& frame) {
    return grabRegion(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                      GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN), frame);
}

bool ScreenCapture::grabRegion(int originX, int originY, int width, int height, Frame& frame) {
    if (width <= 0 || height <= 0 || !ensureSurface(width, height)) {
        return false;
    }

    if (!BitBlt(memoryDC_, 0, 0, width, height, screenDC_, originX, originY, SRCCOPY | CAPTUREBLT)) {
        return false;
    }

    frame.width = width;
    frame.height = height;
    frame.originX = originX;
    frame.originY = originY;
    frame.pixels.assign(bits_, bits_ + static_cast<std::size_t>(width) * height);
    for (std::uint32_t& pixel : frame.pixels) {
        pixel &= 0x00FFFFFFu;  // альфа из BitBlt не осмысленна
    }
    return true;
}

}  // namespace app
