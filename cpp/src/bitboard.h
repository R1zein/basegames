// Представление доски 2048 в одном 64-битном слове.
//
// 16 клеток по 4 бита: клетка (row, col) лежит в нибле с индексом row * 4 + col,
// то есть в битах [4*i, 4*i+4). В нибле хранится показатель степени, а не само
// значение: 0 — пустая клетка, n — плитка 2^n. Такой упаковки хватает до 32768,
// чего для 2048 заведомо достаточно.
//
// Ходы строк предпросчитаны для всех 65536 вариантов, поэтому ход по доске —
// это четыре обращения к таблице вместо цикла со слияниями.

#pragma once

#include <cstdint>

namespace bb {

using Board = std::uint64_t;

enum class Move { Left, Right, Up, Down };
inline constexpr Move kMoves[4] = {Move::Left, Move::Right, Move::Up, Move::Down};

// Заполняет таблицы ходов. Вызывать один раз до любых расчётов.
void init();

Board moveBoard(Board board, Move move);

// Очки за слияния в этом ходу — та же величина, что показывает сама игра.
float moveScore(Board board, Move move);

inline int tileAt(Board board, int index) {
    return static_cast<int>((board >> (index * 4)) & 0xFULL);
}

inline Board withTile(Board board, int index, int exponent) {
    const int shift = index * 4;
    return (board & ~(0xFULL << shift)) | (static_cast<Board>(exponent) << shift);
}

int countEmpty(Board board);

// Транспонирование матрицы 4x4 нибблов битовыми операциями.
inline Board transpose(Board x) {
    const Board a1 = x & 0xF0F00F0FF0F00F0FULL;
    const Board a2 = x & 0x0000F0F00000F0F0ULL;
    const Board a3 = x & 0x0F0F00000F0F0000ULL;
    const Board a = a1 | (a2 << 12) | (a3 >> 12);
    const Board b1 = a & 0xFF00FF0000FF00FFULL;
    const Board b2 = a & 0x00FF00FF00000000ULL;
    const Board b3 = a & 0x00000000FF00FF00ULL;
    return b1 | (b2 >> 24) | (b3 << 24);
}

// Медленное эталонное транспонирование — только для тестов.
Board transposeReference(Board x);

// Сборка доски из 16 значений плиток (не показателей) для тестов и обмена с Python.
Board fromValues(const int values[16]);
void toValues(Board board, int values[16]);

}  // namespace bb
