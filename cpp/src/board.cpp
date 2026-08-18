#include "board.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace app {
namespace {

// Палитра gabrielecirulli/2048. Хранится показатель степени, как в битборде.
struct Tile {
    std::uint32_t color;
    int exponent;
};

constexpr std::uint32_t kBoardBackground = 0xBBADA0;
constexpr Tile kPalette[] = {
    {0xCDC1B4, 0},   // пустая клетка
    {0xEEE4DA, 1},   // 2
    {0xEDE0C8, 2},   // 4
    {0xF2B179, 3},   // 8
    {0xF59563, 4},   // 16
    {0xF67C5F, 5},   // 32
    {0xF65E3B, 6},   // 64
    {0xEDCF72, 7},   // 128
    {0xEDCC61, 8},   // 256
    {0xEDC850, 9},   // 512
    {0xEDC53F, 10},  // 1024
    {0xEDC22E, 11},  // 2048
    {0x3C3A32, 12},  // 4096 и выше
};

constexpr int kMinBoardSize = 120;
constexpr double kCellMargin = 0.22;

int colorDistance(std::uint32_t a, std::uint32_t b) {
    return std::abs(red(a) - red(b)) + std::abs(green(a) - green(b)) + std::abs(blue(a) - blue(b));
}

int nearestExponent(std::uint32_t color) {
    int best = 0;
    int bestDistance = colorDistance(color, kPalette[0].color);
    for (const Tile& tile : kPalette) {
        const int distance = colorDistance(color, tile.color);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = tile.exponent;
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

std::optional<Rect> findBoard(const Frame& frame, int tolerance) {
    const int threshold = tolerance * 3;
    const std::size_t total = static_cast<std::size_t>(frame.width) * frame.height;

    std::vector<char> visited(total, 0);
    std::vector<int> stack;
    Rect best;
    long long bestArea = 0;

    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const std::size_t start = static_cast<std::size_t>(y) * frame.width + x;
            if (visited[start] || colorDistance(frame.pixels[start], kBoardBackground) >= threshold) {
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
                    if (visited[next] || colorDistance(frame.pixels[next], kBoardBackground) >= threshold) {
                        continue;
                    }
                    visited[next] = 1;
                    stack.push_back(static_cast<int>(next));
                }
            }

            if (area > bestArea) {
                bestArea = area;
                best = Rect{minX, minY, maxX - minX + 1, maxY - minY + 1};
            }
        }
    }

    if (best.width < kMinBoardSize || best.height < kMinBoardSize) {
        return std::nullopt;
    }
    // Доска квадратная; сильно вытянутый прямоугольник — ложное срабатывание.
    const double ratio = static_cast<double>(best.width) / best.height;
    if (ratio < 0.85 || ratio > 1.18) {
        return std::nullopt;
    }
    return best;
}

bb::Board readBoard(const Frame& frame, const Rect& rect) {
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
            const int exponent = nearestExponent(dominantColor(frame, x0, y0, x1, y1));
            board = bb::withTile(board, row * 4 + col, exponent);
        }
    }
    return board;
}

bool looksLikeGame(bb::Board board) {
    return (16 - bb::countEmpty(board)) >= 2;
}

}  // namespace app
