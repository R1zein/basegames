#include "solver.h"

#include <algorithm>

namespace bb {
namespace {

// «Змейка»: максимум держим в левом верхнем углу, дальше значения убывают по
// строкам туда-обратно. Веса растут степенями двойки, поэтому нарушение
// порядка обходится дороже любого локального выигрыша.
constexpr double kWeights[16] = {
    32768.0, 16384.0, 8192.0, 4096.0,
    256.0,   512.0,   1024.0, 2048.0,
    128.0,   64.0,    32.0,   16.0,
    1.0,     2.0,     4.0,    8.0,
};
constexpr double kEmptyBonus = 4096.0;

// Появление новой плитки после хода: 2 с вероятностью 90%, 4 — 10%.
struct Spawn {
    int exponent;
    double probability;
};
constexpr Spawn kSpawns[2] = {{1, 0.9}, {2, 0.1}};

}  // namespace

double Solver::evaluate(Board board) const {
    double positional = 0.0;
    int empty = 0;
    for (int i = 0; i < 16; ++i) {
        const int exponent = tileAt(board, i);
        if (exponent == 0) {
            ++empty;
        } else {
            positional += kWeights[i] * static_cast<double>(1u << exponent);
        }
    }
    return positional + empty * kEmptyBonus;
}

double Solver::chanceNode(Board board, int depth) {
    if (depth >= 0 && depth <= kMaxDepth) {
        const auto it = chance_[depth].find(board);
        if (it != chance_[depth].end()) {
            return it->second;
        }
    }

    int empties[16];
    int count = 0;
    for (int i = 0; i < 16; ++i) {
        if (tileAt(board, i) == 0) {
            empties[count++] = i;
        }
    }
    if (count == 0) {
        return evaluate(board);
    }

    double total = 0.0;
    for (int i = 0; i < count; ++i) {
        for (const Spawn& spawn : kSpawns) {
            const Board spawned = withTile(board, empties[i], spawn.exponent);
            total += spawn.probability * moveNode(spawned, depth - 1);
        }
    }
    const double result = total / count;

    if (depth >= 0 && depth <= kMaxDepth) {
        chance_[depth][board] = result;
    }
    return result;
}

double Solver::moveNode(Board board, int depth) {
    if (depth <= 0) {
        return evaluate(board);
    }
    if (depth <= kMaxDepth) {
        const auto it = move_[depth].find(board);
        if (it != move_[depth].end()) {
            return it->second;
        }
    }

    bool found = false;
    double best = 0.0;
    for (const Move move : kMoves) {
        const Board moved = moveBoard(board, move);
        if (moved == board) {
            continue;
        }
        const double value = moveScore(board, move) + chanceNode(moved, depth);
        if (!found || value > best) {
            best = value;
            found = true;
        }
    }

    // Ходов нет — позиция проиграна, оцениваем как есть.
    const double result = found ? best : evaluate(board);
    if (depth <= kMaxDepth) {
        move_[depth][board] = result;
    }
    return result;
}

int Solver::autoDepth(Board board) {
    const int empty = countEmpty(board);
    if (empty <= 3) {
        return 6;
    }
    if (empty <= 6) {
        return 5;
    }
    return 4;
}

std::optional<Move> Solver::bestMove(Board board, int depth) {
    if (cacheSize() > kCacheLimit) {
        clearCache();
    }
    if (depth <= 0) {
        depth = autoDepth(board);
    }
    depth = std::min(depth, kMaxDepth);

    std::optional<Move> best;
    double bestValue = 0.0;
    for (const Move move : kMoves) {
        const Board moved = moveBoard(board, move);
        if (moved == board) {
            continue;
        }
        const double value = moveScore(board, move) + chanceNode(moved, depth);
        if (!best || value > bestValue) {
            bestValue = value;
            best = move;
        }
    }
    return best;
}

void Solver::clearCache() {
    for (int i = 0; i <= kMaxDepth; ++i) {
        chance_[i].clear();
        move_[i].clear();
    }
}

std::size_t Solver::cacheSize() const {
    std::size_t total = 0;
    for (int i = 0; i <= kMaxDepth; ++i) {
        total += chance_[i].size() + move_[i].size();
    }
    return total;
}

}  // namespace bb
