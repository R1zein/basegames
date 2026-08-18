"""Распознавание доски 2048 на скриншоте.

OCR не нужен: в классической вёрстке игры у каждого номинала плитки свой
уникальный цвет фона, поэтому клетка читается сопоставлением цвета с
таблицей. Цифры на плитке занимают меньшую часть площади, так что берём
доминирующий цвет клетки, а не цвет центрального пикселя.
"""

from __future__ import annotations

import cv2
import numpy as np

SIZE = 4

# Палитра gabrielecirulli/2048, RGB.
BOARD_BG = (187, 173, 160)
TILE_COLORS: dict[tuple[int, int, int], int] = {
    (205, 193, 180): 0,
    (238, 228, 218): 2,
    (237, 224, 200): 4,
    (242, 177, 121): 8,
    (245, 149, 99): 16,
    (246, 124, 95): 32,
    (246, 94, 59): 64,
    (237, 207, 114): 128,
    (237, 204, 97): 256,
    (237, 200, 80): 512,
    (237, 197, 63): 1024,
    (237, 194, 46): 2048,
    (60, 58, 50): 4096,
}

_PALETTE = np.array(list(TILE_COLORS.keys()), dtype=np.int16)
_VALUES = list(TILE_COLORS.values())

Rect = tuple[int, int, int, int]  # x, y, w, h


def find_board(image: np.ndarray, tolerance: int = 24) -> Rect | None:
    """Ищет доску по её фоновому цвету. Возвращает (x, y, w, h) либо None."""
    bg = np.array(BOARD_BG, dtype=np.int16)
    distance = np.abs(image.astype(np.int16) - bg).sum(axis=2)
    mask = (distance < tolerance * 3).astype(np.uint8) * 255

    # Замыкаем разрывы: фон доски разрезан плитками на решётку.
    kernel = np.ones((9, 9), np.uint8)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return None

    x, y, w, h = cv2.boundingRect(max(contours, key=cv2.contourArea))
    if w < 120 or h < 120:
        return None
    # Доска квадратная; сильно вытянутый прямоугольник — ложное срабатывание.
    if not 0.85 < w / h < 1.18:
        return None
    return x, y, w, h


def _dominant_color(patch: np.ndarray) -> np.ndarray:
    """Самый частый цвет области — фон плитки, а не пиксели цифр."""
    flat = patch.reshape(-1, 3)
    colors, counts = np.unique(flat, axis=0, return_counts=True)
    return colors[counts.argmax()].astype(np.int16)


def read_board(image: np.ndarray, rect: Rect, margin: float = 0.22) -> tuple[int, ...]:
    """Читает 16 клеток внутри найденного прямоугольника доски."""
    x, y, w, h = rect
    cell_w, cell_h = w / SIZE, h / SIZE
    inset_x, inset_y = cell_w * margin, cell_h * margin

    values: list[int] = []
    for row in range(SIZE):
        for col in range(SIZE):
            x0 = int(x + col * cell_w + inset_x)
            x1 = int(x + (col + 1) * cell_w - inset_x)
            y0 = int(y + row * cell_h + inset_y)
            y1 = int(y + (row + 1) * cell_h - inset_y)
            patch = image[y0:y1, x0:x1]
            if patch.size == 0:
                values.append(0)
                continue
            color = _dominant_color(patch)
            nearest = np.abs(_PALETTE - color).sum(axis=1).argmin()
            values.append(_VALUES[nearest])
    return tuple(values)


def looks_like_game(board: tuple[int, ...]) -> bool:
    """Отсекает мусор: в живой партии есть хотя бы две плитки."""
    return sum(1 for v in board if v) >= 2
