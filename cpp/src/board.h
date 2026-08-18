// Поиск доски 2048 на снимке экрана и чтение позиции.
//
// OCR не нужен: у каждого номинала плитки свой цвет фона, поэтому клетка
// читается сопоставлением цвета с таблицей. Цифры занимают меньшую часть
// площади клетки, так что берём доминирующий цвет, а не центральный пиксель.
//
// Палитра зависит от темы сайта, поэтому их несколько и подходящая
// определяется по цвету фона доски.

#pragma once

#include <cstdint>
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

struct Theme {
    const char* name;
    std::uint32_t background;
    // Индекс — показатель степени: 0 пустая клетка, 1 плитка 2, ... 12 «super».
    std::uint32_t cells[13];
};

struct Detection {
    Rect rect;
    int theme = 0;
};

const Theme& theme(int index);
int themeCount();

// Допуск задаётся суммой отклонений по трём каналам. Он намеренно узкий:
// в тёмной теме фон доски отстоит от фона страницы всего на 43, и стоит
// взять порог шире — заливка растекается на всё окно браузера.
constexpr int kDefaultTolerance = 20;

// Ищет доску по цвету фона каждой известной темы. Фон образует связную
// решётку между плитками, поэтому заливки достаточно.
std::optional<Detection> findBoard(const Frame& frame, int tolerance = kDefaultTolerance);

// Что нашлось по конкретной теме — для диагностики распознавания.
struct Candidate {
    bool found = false;
    bool plausible = false;
    Rect rect;
    long long area = 0;
};

Candidate probeTheme(const Frame& frame, int themeIndex, int tolerance = kDefaultTolerance);

bb::Board readBoard(const Frame& frame, const Rect& rect, int themeIndex);

// Отсекает мусор: в живой партии на доске есть хотя бы две плитки.
bool looksLikeGame(bb::Board board);

}  // namespace app
