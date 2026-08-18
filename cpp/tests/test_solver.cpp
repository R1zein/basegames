// Проверки битборда и решателя без внешних фреймворков.
//
// Главная из них — сверка с питоновской реализацией: обе версии считают
// одну и ту же оценочную функцию тем же порядком обхода, поэтому на общем
// наборе позиций из testdata обязаны выбирать одинаковые ходы. Расхождение
// означает ошибку в упаковке доски или в таблицах ходов.

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "bitboard.h"
#include "solver.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bb::Board boardOf(std::initializer_list<int> values) {
    int cells[16] = {};
    int i = 0;
    for (const int value : values) {
        cells[i++] = value;
    }
    return bb::fromValues(cells);
}

std::string moveName(bb::Move move) {
    switch (move) {
        case bb::Move::Left:  return "left";
        case bb::Move::Right: return "right";
        case bb::Move::Up:    return "up";
        case bb::Move::Down:  return "down";
    }
    return "?";
}

void testTranspose() {
    // Битовый трюк должен совпадать с наивной перестановкой на любых данных.
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 2000; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        check(bb::transpose(state) == bb::transposeReference(state), "transpose расходится с эталоном");
    }
}

void testRoundTrip() {
    const bb::Board board = boardOf({2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 0, 0, 0, 0, 2});
    int values[16] = {};
    bb::toValues(board, values);
    const int expected[16] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 0, 0, 0, 0, 2};
    for (int i = 0; i < 16; ++i) {
        check(values[i] == expected[i], "распаковка потеряла клетку " + std::to_string(i));
    }
}

void testMerges() {
    const bb::Board start = boardOf({2, 2, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    int values[16] = {};
    bb::toValues(bb::moveBoard(start, bb::Move::Left), values);
    check(values[0] == 4 && values[1] == 8 && values[2] == 0 && values[3] == 0, "слияние влево");
    check(bb::moveScore(start, bb::Move::Left) == 12.0f, "очки за слияние");

    // 2 2 2 2 -> 4 4, а не 8: слитая плитка в этом же ходу не участвует снова.
    const bb::Board four = boardOf({2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    bb::toValues(bb::moveBoard(four, bb::Move::Left), values);
    check(values[0] == 4 && values[1] == 4 && values[2] == 0, "повторное слияние за один ход");
}

void testLegalMoves() {
    const bb::Board start = boardOf({2, 4, 8, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    check(bb::moveBoard(start, bb::Move::Left) == start, "влево двигаться некуда");
    check(bb::moveBoard(start, bb::Move::Right) != start, "вправо ход есть");
    check(bb::moveBoard(start, bb::Move::Up) == start, "вверх двигаться некуда");
    check(bb::moveBoard(start, bb::Move::Down) != start, "вниз ход есть");
}

void testDeadPosition() {
    const bb::Board dead = boardOf({2, 4, 2, 4, 4, 2, 4, 2, 2, 4, 2, 4, 4, 2, 4, 2});
    bb::Solver solver;
    check(!solver.bestMove(dead).has_value(), "в тупике ходов быть не должно");
}

void testAgainstPython(const std::string& dataDir) {
    std::ifstream positions(dataDir + "/positions.txt");
    std::ifstream expected(dataDir + "/expected_moves.txt");
    if (!positions.is_open() || !expected.is_open()) {
        std::printf("FAIL: не найдены файлы в %s\n", dataDir.c_str());
        ++g_failures;
        return;
    }

    bb::Solver solver;
    std::string line;
    int index = 0;
    while (std::getline(positions, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        int cells[16] = {};
        std::stringstream stream(line);
        std::string cell;
        for (int i = 0; i < 16 && std::getline(stream, cell, ','); ++i) {
            cells[i] = std::stoi(cell);
        }

        std::string want;
        if (!std::getline(expected, want)) {
            check(false, "эталонных ходов меньше, чем позиций");
            return;
        }

        const auto move = solver.bestMove(bb::fromValues(cells), 4);
        const std::string got = move ? moveName(*move) : "none";
        check(got == want, "позиция " + std::to_string(index) + ": ожидался " + want + ", получен " + got);
        ++index;
    }
    std::printf("сверено с Python: %d позиций\n", index);
}

}  // namespace

int main(int argc, char** argv) {
    bb::init();

    testTranspose();
    testRoundTrip();
    testMerges();
    testLegalMoves();
    testDeadPosition();
    if (argc > 1) {
        testAgainstPython(argv[1]);
    } else {
        std::printf("пропущена сверка с Python: не передан путь к testdata\n");
    }

    if (g_failures == 0) {
        std::printf("все проверки пройдены\n");
        return 0;
    }
    std::printf("провалено проверок: %d\n", g_failures);
    return 1;
}
