"""Эталонные ходы для сверки C++-реализации с питоновской.

Читает testdata/positions.txt, считает лучший ход на фиксированной глубине и
печатает по строке на позицию. Глубина зафиксирована намеренно: адаптивная
завязана на число пустых клеток, и любое расхождение в ней смазало бы картину.

    py python/tools/reference_moves.py > testdata/expected_moves.txt
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import solver  # noqa: E402  - путь дописывается выше

DEPTH = 4
ROOT = Path(__file__).resolve().parents[2]


def load_positions(path: Path) -> list[tuple[int, ...]]:
    positions = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        positions.append(tuple(int(v) for v in line.split(",")))
    return positions


def main() -> int:
    for position in load_positions(ROOT / "testdata" / "positions.txt"):
        move = solver.best_move(position, depth=DEPTH)
        print(move if move else "none")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
