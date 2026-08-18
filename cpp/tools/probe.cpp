// Диагностика распознавания: что именно видит программа на экране.
//
// Запускать с открытой игрой. Печатает самые частые цвета экрана и результат
// поиска доски — этого достаточно, чтобы понять, отличается ли палитра клона
// от классической или дело в размере и пропорциях найденной области.
//
//   smt_probe.exe            проверить классический цвет фона BBADA0
//   smt_probe.exe RRGGBB     проверить свой цвет фона

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

#include <algorithm>
using std::max;
using std::min;

#include <objidl.h>
#include <gdiplus.h>

#include "bitboard.h"
#include "board.h"
#include "capture.h"

namespace {

void printTopColors(const app::Frame& frame, int count) {
    std::unordered_map<std::uint32_t, int> histogram;
    histogram.reserve(1 << 16);
    // Каждый второй пиксель: на распределение это не влияет, зато вдвое быстрее.
    for (int y = 0; y < frame.height; y += 2) {
        for (int x = 0; x < frame.width; x += 2) {
            ++histogram[frame.at(x, y)];
        }
    }

    std::vector<std::pair<std::uint32_t, int>> sorted(histogram.begin(), histogram.end());
    std::partial_sort(sorted.begin(), sorted.begin() + std::min<std::size_t>(count, sorted.size()), sorted.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });

    std::printf("\nsamye chastye cveta:\n");
    for (int i = 0; i < count && i < static_cast<int>(sorted.size()); ++i) {
        std::printf("  #%06X  %d px\n", sorted[i].first, sorted[i].second);
    }
}

// Поиск CLSID кодировщика PNG среди зарегистрированных в GDI+.
bool pngEncoder(CLSID& clsid) {
    UINT count = 0;
    UINT size = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0) {
        return false;
    }
    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, size, codecs) != Gdiplus::Ok) {
        return false;
    }
    for (UINT i = 0; i < count; ++i) {
        if (std::wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

// Снимок сохраняется вдвое уменьшенным: для разбора палитры этого хватает,
// а файл получается вчетверо легче.
bool savePng(const app::Frame& frame, const wchar_t* path) {
    CLSID clsid;
    if (!pngEncoder(clsid)) {
        return false;
    }
    Gdiplus::Bitmap source(frame.width, frame.height, frame.width * 4, PixelFormat32bppRGB,
                           reinterpret_cast<BYTE*>(const_cast<std::uint32_t*>(frame.pixels.data())));
    Gdiplus::Bitmap scaled(frame.width / 2, frame.height / 2, PixelFormat32bppRGB);
    Gdiplus::Graphics graphics(&scaled);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.DrawImage(&source, 0, 0, frame.width / 2, frame.height / 2);
    return scaled.Save(path, &clsid, nullptr) == Gdiplus::Ok;
}

}  // namespace

int main(int argc, char** argv) {
    bb::init();

    Gdiplus::GdiplusStartupInput startupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);

    app::ScreenCapture capture;
    app::Frame frame;
    if (!capture.grab(frame)) {
        std::printf("ne udalos snyat ekran\n");
        return 1;
    }
    std::printf("ekran: %dx%d, nachalo (%d,%d)\n", frame.width, frame.height, frame.originX, frame.originY);

    printTopColors(frame, 12);

    std::printf("\nchto nashlos po kazhdoy teme:\n");
    for (int i = 0; i < app::themeCount(); ++i) {
        const app::Candidate candidate = app::probeTheme(frame, i);
        if (!candidate.found) {
            std::printf("  %-6s #%06X: cveta na ekrane net\n", app::theme(i).name, app::theme(i).background);
            continue;
        }
        std::printf("  %-6s #%06X: x=%d y=%d w=%d h=%d ploshad=%lld %s\n",
                    app::theme(i).name, app::theme(i).background, candidate.rect.x, candidate.rect.y,
                    candidate.rect.width, candidate.rect.height, candidate.area,
                    candidate.plausible ? "podhodit" : "otbrakovano po forme ili razmeru");
    }

    if (argc > 1) {
        const std::string arg(argv[1]);
        const std::wstring path(arg.begin(), arg.end());
        const bool saved = savePng(frame, path.c_str());
        std::printf("\nsnimok: %s\n", saved ? arg.c_str() : "ne udalos sohranit");
    }

    const auto detection = app::findBoard(frame);
    if (!detection.has_value()) {
        std::printf("\ndoska ne naydena\n");
        std::printf("proverte dve veshi:\n");
        std::printf("  1) doska ne perekryta drugim oknom\n");
        std::printf("  2) sredi cvetov vyshe est fon doski temy:\n");
        for (int i = 0; i < app::themeCount(); ++i) {
            std::printf("     %-6s #%06X\n", app::theme(i).name, app::theme(i).background);
        }
        return 0;
    }


    const app::Rect* rect = &detection->rect;
    std::printf("\ndoska naydena: tema=%s x=%d y=%d w=%d h=%d\n",
                app::theme(detection->theme).name, rect->x, rect->y, rect->width, rect->height);
    const bb::Board board = app::readBoard(frame, *rect, detection->theme);
    int values[16] = {};
    bb::toValues(board, values);
    std::printf("prochitannaya poziciya:\n");
    for (int row = 0; row < 4; ++row) {
        std::printf("  %6d %6d %6d %6d\n", values[row * 4], values[row * 4 + 1], values[row * 4 + 2],
                    values[row * 4 + 3]);
    }
    std::printf("\ncveta kletok:\n");
    for (int row = 0; row < 4; ++row) {
        std::printf("  ");
        for (int col = 0; col < 4; ++col) {
            const double cellWidth = rect->width / 4.0;
            const double cellHeight = rect->height / 4.0;
            const int cx = static_cast<int>(rect->x + (col + 0.5) * cellWidth);
            const int cy = static_cast<int>(rect->y + (row + 0.5) * cellHeight);
            std::printf("#%06X ", frame.at(cx, cy));
        }
        std::printf("\n");
    }
    return 0;
}
