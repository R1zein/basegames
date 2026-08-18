"""Expectimax-решатель для 2048.

Доска — кортеж из 16 значений плиток (0 = пустая клетка), row-major:
index = row * 4 + col. Кортеж выбран ради хешируемости: узлы дерева
кешируются через lru_cache, и на повторяющихся позициях это даёт
кратный выигрыш по времени.
"""

from __future__ import annotations

from functools import lru_cache

SIZE = 4
CELLS = SIZE * SIZE
MOVES = ("left", "right", "up", "down")

Board = tuple[int, ...]

# «Змейка»: максимум держим в левом верхнем углу, дальше значения убывают
# по строкам туда-обратно. Веса растут степенями двойки, поэтому нарушение
# порядка обходится дороже любого локального выигрыша.
_WEIGHTS = (
    2 ** 15, 2 ** 14, 2 ** 13, 2 ** 12,
    2 ** 8,  2 ** 9,  2 ** 10, 2 ** 11,
    2 ** 7,  2 ** 6,  2 ** 5,  2 ** 4,
    2 ** 0,  2 ** 1,  2 ** 2,  2 ** 3,
)
_EMPTY_BONUS = 1 << 12

# Появление новой плитки после хода: 2 с вероятностью 90%, 4 — 10%.
_SPAWNS = ((2, 0.9), (4, 0.1))


def _compress(row: list[int]) -> tuple[list[int], int]:
    """Сдвигает плитки к началу строки и сливает пары. Возвращает (строка, очки)."""
    tiles = [v for v in row if v]
    merged: list[int] = []
    score = 0
    i = 0
    while i < len(tiles):
        if i + 1 < len(tiles) and tiles[i] == tiles[i + 1]:
            value = tiles[i] * 2
            merged.append(value)
            score += value
            i += 2
        else:
            merged.append(tiles[i])
            i += 1
    merged.extend([0] * (SIZE - len(merged)))
    return merged, score


def _rows(board: Board) -> list[list[int]]:
    return [list(board[r * SIZE:(r + 1) * SIZE]) for r in range(SIZE)]


def _transpose(rows: list[list[int]]) -> list[list[int]]:
    return [list(col) for col in zip(*rows)]


def apply_move(board: Board, move: str) -> tuple[Board, int]:
    """Применяет ход. Если доска не изменилась — ход невозможен."""
    rows = _rows(board)
    vertical = move in ("up", "down")
    if vertical:
        rows = _transpose(rows)
    reverse = move in ("right", "down")

    score = 0
    out: list[list[int]] = []
    for row in rows:
        if reverse:
            row = row[::-1]
        new, gained = _compress(row)
        if reverse:
            new = new[::-1]
        score += gained
        out.append(new)

    if vertical:
        out = _transpose(out)
    return tuple(v for row in out for v in row), score


def legal_moves(board: Board) -> list[str]:
    return [m for m in MOVES if apply_move(board, m)[0] != board]


@lru_cache(maxsize=1 << 20)
def _heuristic(board: Board) -> float:
    positional = sum(w * v for w, v in zip(_WEIGHTS, board))
    empty = sum(1 for v in board if v == 0)
    return positional + empty * _EMPTY_BONUS


@lru_cache(maxsize=1 << 20)
def _chance_node(board: Board, depth: int) -> float:
    """Ход игры: усредняем по всем местам появления новой плитки."""
    empties = [i for i, v in enumerate(board) if v == 0]
    if not empties:
        return _heuristic(board)

    total = 0.0
    for i in empties:
        for value, prob in _SPAWNS:
            spawned = board[:i] + (value,) + board[i + 1:]
            total += prob * _move_node(spawned, depth - 1)
    return total / len(empties)


@lru_cache(maxsize=1 << 20)
def _move_node(board: Board, depth: int) -> float:
    """Ход игрока: берём максимум по доступным направлениям."""
    if depth <= 0:
        return _heuristic(board)

    best = None
    for move in MOVES:
        moved, gained = apply_move(board, move)
        if moved == board:
            continue
        value = gained + _chance_node(moved, depth)
        if best is None or value > best:
            best = value
    # Ходов нет — позиция проиграна, оцениваем как есть.
    return _heuristic(board) if best is None else best


def _auto_depth(board: Board) -> int:
    """Чем теснее доска, тем глубже смотрим: ветвление падает вместе с пустыми клетками."""
    empty = sum(1 for v in board if v == 0)
    if empty <= 3:
        return 6
    if empty <= 6:
        return 5
    return 4


def best_move(board: Board, depth: int | None = None) -> str | None:
    """Лучший ход для позиции, либо None если ходов не осталось."""
    if len(board) != CELLS:
        raise ValueError(f"ожидалось {CELLS} клеток, получено {len(board)}")

    if depth is None:
        depth = _auto_depth(board)

    best: str | None = None
    best_value = float("-inf")
    for move in MOVES:
        moved, gained = apply_move(board, move)
        if moved == board:
            continue
        value = gained + _chance_node(moved, depth)
        if value > best_value:
            best_value, best = value, move
    return best
