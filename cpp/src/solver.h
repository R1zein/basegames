// Expectimax для 2048 поверх битборда.
//
// Оценочная функция и порядок обхода в точности повторяют питоновскую
// реализацию из python/src/solver.py: обе версии на одной позиции обязаны
// выбирать один и тот же ход, и на этом строится кросс-проверка.

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "bitboard.h"

namespace bb {

class Solver {
public:
    // Лучший ход либо std::nullopt, если ходов не осталось.
    // depth = 0 означает адаптивную глубину по числу пустых клеток.
    std::optional<Move> bestMove(Board board, int depth = 0);

    double evaluate(Board board) const;

    void clearCache();
    std::size_t cacheSize() const;

private:
    static constexpr int kMaxDepth = 12;
    // Выше этого порога кеш сбрасывается: за длинную партию он иначе
    // съедает память, а польза от старых позиций почти нулевая.
    static constexpr std::size_t kCacheLimit = 1u << 22;

    double chanceNode(Board board, int depth);
    double moveNode(Board board, int depth);
    static int autoDepth(Board board);

    std::unordered_map<Board, double> chance_[kMaxDepth + 1];
    std::unordered_map<Board, double> move_[kMaxDepth + 1];
};

}  // namespace bb
