#include "board.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace app {
namespace {

// Цвета сняты прямо из CSS 2048.io. Светлая тема совпадает с классической
// вёрсткой gabrielecirulli с точностью до единиц по каналу.
constexpr Theme kThemes[] = {
    {
        "light",
        0xBBADA0,
        {0xCDC1B4, 0xEEE4DA, 0xEEE1C9, 0xF3B27A, 0xF69664, 0xF77D5F, 0xF75F3B,
         0xEDD073, 0xEDCC62, 0xEDC950, 0xEDC53F, 0xEDC22E, 0x3C3A33},
    },
    {
        "dark",
        0x342D24,
        // В тёмной теме 2048 и «super» окрашены одинаково, поэтому всё, что
        // больше 2048, читается как 2048. Для игры этого достаточно.
        {0x3F3729, 0x876849, 0x845A31, 0xC67D33, 0xD26F41, 0xCA4F31, 0xAD362B,
         0xE0A13A, 0xE4AB33, 0xE8B52D, 0xEDC026, 0xF2CB1E, 0xF2CB1E},
    },
};

constexpr int kThemeCount = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));
constexpr int kMinBoardSize = 120;
constexpr int kMinBackgroundPixels = 4000;
constexpr double kCellMargin = 0.22;

int colorDistance(std::uint32_t a, std::uint32_t b) {
    return std::abs(red(a) - red(b)) + std::abs(green(a) - green(b)) + std::abs(blue(a) - blue(b));
}

int nearestExponent(const Theme& t, std::uint32_t color) {
    int best = 0;
    int bestDistance = colorDistance(color, t.cells[0]);
    for (int exponent = 1; exponent < 13; ++exponent) {
        const int distance = colorDistance(color, t.cells[exponent]);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = exponent;
        }
    }
    return best;
}

std::uint32_t dominantColor(const Frame& frame, int x0, int y0, int x1, int y1) {
    // Шаг подбираем так, чтобы на клетку приходилось около 400 проб: этого
    // хватает, чтобы фон уверенно перевесил пиксели цифр.
    const int stepX = std::max(1, (x1 - x0) / 20);
    const int stepY = std::max(1, (y1 - y0) / 20);

    std::unordered_map<std::uint32_t, int> counts;
    for (int y = y0; y < y1; y += stepY) {
        for (int x = x0; x < x1; x += stepX) {
            ++counts[frame.at(x, y)];
        }
    }

    std::uint32_t best = 0;
    int bestCount = -1;
    for (const auto& [color, count] : counts) {
        if (count > bestCount) {
            bestCount = count;
            best = color;
        }
    }
    return best;
}

}  // namespace

namespace {

// Наибольшая связная область заданного цвета: её габарит и площадь.
bool largestComponent(const Frame& frame, std::uint32_t color, int threshold, Rect& out, long long& outArea) {
    const std::size_t total = static_cast<std::size_t>(frame.width) * frame.height;

    // Дешёвая отсечка: если пикселей нужного цвета мало, заливку можно не
    // запускать вовсе. На полноэкранном кадре это экономит десятки миллисекунд.
    int matching = 0;
    for (std::size_t i = 0; i < total; i += 16) {
        if (colorDistance(frame.pixels[i], color) < threshold) {
            ++matching;
        }
    }
    if (matching * 16 < kMinBackgroundPixels) {
        return false;
    }

    std::vector<char> visited(total, 0);
    std::vector<int> stack;
    long long bestArea = 0;
    bool found = false;

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t start = static_cast<std::size_t>(y) * frame.width + x;
            if (visited[start] || colorDistance(frame.pixels[start], color) >= threshold) {
                continue;
            }

            int minX = x, maxX = x, minY = y, maxY = y;
            long long area = 0;
            visited[start] = 1;
            stack.clear();
            stack.push_back(static_cast<int>(start));

            while (!stack.empty()) {
                const int index = stack.back();
                stack.pop_back();
                const int cx = index % frame.width;
                const int cy = index / frame.width;
                ++area;
                minX = std::min(minX, cx);
                maxX = std::max(maxX, cx);
                minY = std::min(minY, cy);
                maxY = std::max(maxY, cy);

                const int neighbours[4][2] = {{cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
                for (const auto& neighbour : neighbours) {
                    const int nx = neighbour[0];
                    const int ny = neighbour[1];
                    if (nx < 0 || ny < 0 || nx >= frame.width || ny >= frame.height) {
                        continue;
                    }
                    const std::size_t next = static_cast<std::size_t>(ny) * frame.width + nx;
                    if (visited[next] || colorDistance(frame.pixels[next], color) >= threshold) {
                        continue;
                    }
                    visited[next] = 1;
                    stack.push_back(static_cast<int>(next));
                }
            }

            if (area > bestArea) {
                bestArea = area;
                out = Rect{minX, minY, maxX - minX + 1, maxY - minY + 1};
                found = true;
            }
        }
    }

    outArea = bestArea;
    return found;
}

bool plausibleBoard(const Rect& rect) {
    if (rect.width < kMinBoardSize || rect.height < kMinBoardSize) {
        return false;
    }
    // Доска квадратная; сильно вытянутый прямоугольник — ложное срабатывание.
    const double ratio = static_cast<double>(rect.width) / rect.height;
    return ratio >= 0.85 && ratio <= 1.18;
}

}  // namespace

const Theme& theme(int index) { return kThemes[index]; }

int themeCount() { return kThemeCount; }

std::optional<Detection> findBoard(const Frame& frame, int tolerance) {
    const int threshold = tolerance * 3;

    std::optional<Detection> best;
    long long bestArea = 0;
    for (int i = 0; i < kThemeCount; ++i) {
        Rect rect;
        long long area = 0;
        if (!largestComponent(frame, kThemes[i].background, threshold, rect, area)) {
            continue;
        }
        if (!plausibleBoard(rect) || area <= bestArea) {
            continue;
        }
        bestArea = area;
        best = Detection{rect, i};
    }
    return best;
}

bb::Board readBoard(const Frame& frame, const Rect& rect, int themeIndex) {
    const Theme& t = kThemes[themeIndex];
    const double cellWidth = rect.width / 4.0;
    const double cellHeight = rect.height / 4.0;
    const double insetX = cellWidth * kCellMargin;
    const double insetY = cellHeight * kCellMargin;

    bb::Board board = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            const int x0 = std::max(0, static_cast<int>(rect.x + col * cellWidth + insetX));
            const int x1 = std::min(frame.width, static_cast<int>(rect.x + (col + 1) * cellWidth - insetX));
            const int y0 = std::max(0, static_cast<int>(rect.y + row * cellHeight + insetY));
            const int y1 = std::min(frame.height, static_cast<int>(rect.y + (row + 1) * cellHeight - insetY));
            if (x1 <= x0 || y1 <= y0) {
                continue;
            }
            board = bb::withTile(board, row * 4 + col, nearestExponent(t, dominantColor(frame, x0, y0, x1, y1)));
        }
    }
    return board;
}

bool looksLikeGame(bb::Board board) {
    return (16 - bb::countEmpty(board)) >= 2;
}

}  // namespace app
