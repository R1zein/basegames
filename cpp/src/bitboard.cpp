#include "bitboard.h"

#include <array>

namespace bb {
namespace {

constexpr int kRows = 65536;

std::array<std::uint16_t, kRows> g_left{};
std::array<std::uint16_t, kRows> g_right{};
std::array<float, kRows> g_leftScore{};
std::array<float, kRows> g_rightScore{};
bool g_ready = false;

std::uint16_t packRow(const int cells[4]) {
    return static_cast<std::uint16_t>(cells[0] | (cells[1] << 4) | (cells[2] << 8) | (cells[3] << 12));
}

void unpackRow(std::uint16_t row, int cells[4]) {
    for (int i = 0; i < 4; ++i) {
        cells[i] = (row >> (i * 4)) & 0xF;
    }
}

// Сдвиг к началу строки со слияниями. Слитая плитка в этом же ходу больше
// не участвует — отсюда шаг на две позиции после слияния.
std::uint16_t compressRow(std::uint16_t row, float& score) {
    int cells[4];
    unpackRow(row, cells);

    int packed[4] = {0, 0, 0, 0};
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (cells[i] != 0) {
            packed[count++] = cells[i];
        }
    }

    int result[4] = {0, 0, 0, 0};
    int out = 0;
    score = 0.0f;
    for (int i = 0; i < count;) {
        if (i + 1 < count && packed[i] == packed[i + 1]) {
            const int merged = packed[i] + 1;
            result[out++] = merged;
            score += static_cast<float>(1u << merged);
            i += 2;
        } else {
            result[out++] = packed[i];
            i += 1;
        }
    }
    return packRow(result);
}

std::uint16_t reverseRow(std::uint16_t row) {
    return static_cast<std::uint16_t>((row >> 12) | ((row >> 4) & 0x00F0) | ((row << 4) & 0x0F00) | (row << 12));
}

inline Board rowsToBoard(const std::uint16_t rows[4]) {
    Board board = 0;
    for (int i = 0; i < 4; ++i) {
        board |= static_cast<Board>(rows[i]) << (i * 16);
    }
    return board;
}

}  // namespace

void init() {
    if (g_ready) {
        return;
    }
    for (int row = 0; row < kRows; ++row) {
        float score = 0.0f;
        g_left[row] = compressRow(static_cast<std::uint16_t>(row), score);
        g_leftScore[row] = score;

        const std::uint16_t reversed = reverseRow(static_cast<std::uint16_t>(row));
        g_right[row] = reverseRow(compressRow(reversed, score));
        g_rightScore[row] = score;
    }
    g_ready = true;
}

Board moveBoard(Board board, Move move) {
    const bool vertical = (move == Move::Up || move == Move::Down);
    const Board work = vertical ? transpose(board) : board;
    const bool toStart = (move == Move::Left || move == Move::Up);

    std::uint16_t rows[4];
    for (int i = 0; i < 4; ++i) {
        const std::uint16_t row = static_cast<std::uint16_t>((work >> (i * 16)) & 0xFFFFULL);
        rows[i] = toStart ? g_left[row] : g_right[row];
    }

    const Board result = rowsToBoard(rows);
    return vertical ? transpose(result) : result;
}

float moveScore(Board board, Move move) {
    const bool vertical = (move == Move::Up || move == Move::Down);
    const Board work = vertical ? transpose(board) : board;
    const bool toStart = (move == Move::Left || move == Move::Up);

    float score = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const std::uint16_t row = static_cast<std::uint16_t>((work >> (i * 16)) & 0xFFFFULL);
        score += toStart ? g_leftScore[row] : g_rightScore[row];
    }
    return score;
}

int countEmpty(Board board) {
    int empty = 0;
    for (int i = 0; i < 16; ++i) {
        if (tileAt(board, i) == 0) {
            ++empty;
        }
    }
    return empty;
}

Board transposeReference(Board x) {
    Board result = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            const Board value = (x >> ((row * 4 + col) * 4)) & 0xFULL;
            result |= value << ((col * 4 + row) * 4);
        }
    }
    return result;
}

Board fromValues(const int values[16]) {
    Board board = 0;
    for (int i = 0; i < 16; ++i) {
        int exponent = 0;
        for (int value = values[i]; value > 1; value >>= 1) {
            ++exponent;
        }
        board |= static_cast<Board>(exponent) << (i * 4);
    }
    return board;
}

void toValues(Board board, int values[16]) {
    for (int i = 0; i < 16; ++i) {
        const int exponent = tileAt(board, i);
        values[i] = exponent == 0 ? 0 : (1 << exponent);
    }
}

}  // namespace bb
