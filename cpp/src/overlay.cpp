#include "overlay.h"

#include <cmath>

// gdiplus.h рассчитывает на min/max в глобальной области, а NOMINMAX их убрал.
#include <algorithm>
using std::max;
using std::min;

#include <objidl.h>
#include <gdiplus.h>

namespace app {
namespace {

constexpr wchar_t kClassName[] = L"Smt2048Overlay";

const Gdiplus::Color kArrowColor(210, 80, 220, 120);
const Gdiplus::Color kFrameColor(120, 80, 220, 120);
const Gdiplus::Color kTextColor(230, 255, 255, 255);
const Gdiplus::Color kShadowColor(160, 0, 0, 0);

LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void directionOf(bb::Move move, float& dx, float& dy) {
    switch (move) {
        case bb::Move::Left:  dx = -1.0f; dy =  0.0f; break;
        case bb::Move::Right: dx =  1.0f; dy =  0.0f; break;
        case bb::Move::Up:    dx =  0.0f; dy = -1.0f; break;
        case bb::Move::Down:  dx =  0.0f; dy =  1.0f; break;
    }
}

}  // namespace

bool Overlay::create(HINSTANCE instance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = overlayProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    originX_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
    originY_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    window_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"", WS_POPUP,
        originX_, originY_, width, height,
        nullptr, nullptr, instance, nullptr);
    if (window_ == nullptr) {
        return false;
    }

    ShowWindow(window_, SW_SHOWNOACTIVATE);
    return ensureSurface(width, height);
}

bool Overlay::ensureSurface(int width, int height) {
    if (bitmap_ != nullptr && width == width_ && height == height_) {
        return true;
    }
    if (bitmap_ != nullptr) {
        SelectObject(memoryDC_, previous_);
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    if (memoryDC_ == nullptr) {
        HDC screen = GetDC(nullptr);
        memoryDC_ = CreateCompatibleDC(screen);
        ReleaseDC(nullptr, screen);
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    bitmap_ = CreateDIBSection(memoryDC_, &info, DIB_RGB_COLORS, &bits_, nullptr, 0);
    if (bitmap_ == nullptr) {
        return false;
    }
    previous_ = SelectObject(memoryDC_, bitmap_);
    width_ = width;
    height_ = height;
    return true;
}

void Overlay::render(const std::optional<Rect>& board, std::optional<bb::Move> move, const std::wstring& status) {
    if (window_ == nullptr || bits_ == nullptr) {
        return;
    }

    // PARGB: альфа уже умножена на цвет — этого формата ждёт UpdateLayeredWindow.
    Gdiplus::Bitmap surface(width_, height_, width_ * 4, PixelFormat32bppPARGB,
                            static_cast<BYTE*>(bits_));
    Gdiplus::Graphics graphics(&surface);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    const Gdiplus::FontFamily family(L"Segoe UI");
    const Gdiplus::Font font(&family, 15.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    const Gdiplus::SolidBrush shadow(kShadowColor);
    const Gdiplus::SolidBrush text(kTextColor);
    graphics.DrawString(status.c_str(), -1, &font, Gdiplus::PointF(21.0f, 21.0f), &shadow);
    graphics.DrawString(status.c_str(), -1, &font, Gdiplus::PointF(20.0f, 20.0f), &text);

    if (board.has_value()) {
        // Экран снимается в физических пикселях, а процесс объявлен
        // per-monitor DPI aware, поэтому пересчёт координат не нужен.
        const float x = static_cast<float>(board->x);
        const float y = static_cast<float>(board->y);
        const float w = static_cast<float>(board->width);
        const float h = static_cast<float>(board->height);

        const Gdiplus::Pen frame(kFrameColor, 2.0f);
        graphics.DrawRectangle(&frame, x, y, w, h);

        if (move.has_value()) {
            float dx = 0.0f;
            float dy = 0.0f;
            directionOf(*move, dx, dy);

            const float cx = x + w / 2.0f;
            const float cy = y + h / 2.0f;
            const float length = std::min(w, h) * 0.28f;

            const float tipX = cx + dx * length;
            const float tipY = cy + dy * length;
            const float tailX = cx - dx * length * 0.55f;
            const float tailY = cy - dy * length * 0.55f;

            Gdiplus::Pen shaft(kArrowColor, std::max(6.0f, length * 0.16f));
            shaft.SetStartCap(Gdiplus::LineCapRound);
            shaft.SetEndCap(Gdiplus::LineCapRound);
            graphics.DrawLine(&shaft, tailX, tailY, tipX, tipY);

            // Наконечник: две точки, отложенные от острия перпендикулярно ходу.
            const float wing = length * 0.42f;
            const float px = -dy;
            const float py = dx;
            const Gdiplus::PointF head[3] = {
                Gdiplus::PointF(tipX, tipY),
                Gdiplus::PointF(tipX - dx * wing + px * wing * 0.6f, tipY - dy * wing + py * wing * 0.6f),
                Gdiplus::PointF(tipX - dx * wing - px * wing * 0.6f, tipY - dy * wing - py * wing * 0.6f),
            };
            const Gdiplus::SolidBrush arrow(kArrowColor);
            graphics.FillPolygon(&arrow, head, 3);
        }
    }

    POINT source = {0, 0};
    POINT position = {originX_, originY_};
    SIZE size = {width_, height_};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(window_, nullptr, &position, &size, memoryDC_, &source, 0, &blend, ULW_ALPHA);
}

void Overlay::destroy() {
    if (bitmap_ != nullptr) {
        SelectObject(memoryDC_, previous_);
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    if (memoryDC_ != nullptr) {
        DeleteDC(memoryDC_);
        memoryDC_ = nullptr;
    }
    if (window_ != nullptr) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
}

}  // namespace app
