// Поиск доски 2048 на снимке экрана и чтение позиции.
//
// OCR не нужен: в классической вёрстке игры у каждого номинала плитки свой
// уникальный цвет фона, поэтому клетка читается сопоставлением цвета с
// таблицей. Цифры занимают меньшую часть площади клетки, так что берём
// доминирующий цвет, а не цвет центрального пикселя.

#pragma once

#include <optional>

#include "bitboard.h"
#include "capture.h"

namespace app {

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Ищет доску по её фоновому цвету. Фон образует связную решётку между
// плитками, поэтому заливки достаточно — морфология не нужна.
std::optional<Rect> findBoard(const Frame& frame, int tolerance = 24);

bb::Board readBoard(const Frame& frame, const Rect& rect);

// Отсекает мусор: в живой партии на доске есть хотя бы две плитки.
bool looksLikeGame(bb::Board board);

}  // namespace app
