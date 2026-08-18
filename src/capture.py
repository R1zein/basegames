"""Захват экрана и отсев нестабильных кадров.

Пошаговой игре не нужны 60 кадров в секунду: считаем 4-5 раз в секунду и
работаем только с кадрами, где картинка уже устоялась. Иначе кадр, пойманный
посреди анимации сдвига плиток, даёт бессмысленную позицию.
"""

from __future__ import annotations

import numpy as np
from mss import mss


class ScreenCapture:
    """Снимки монитора в виде RGB-массивов numpy."""

    def __init__(self, monitor: int = 1) -> None:
        self._sct = mss()
        self._index = monitor

    @property
    def monitor(self) -> dict:
        return self._sct.monitors[self._index]

    def grab(self) -> np.ndarray:
        shot = self._sct.grab(self.monitor)
        frame = np.asarray(shot)  # BGRA
        return frame[:, :, [2, 1, 0]]

    def close(self) -> None:
        self._sct.close()


class StableFrame:
    """Пропускает кадр дальше, только если он повторился подряд N раз."""

    def __init__(self, required: int = 2) -> None:
        self._required = required
        self._last: tuple[int, ...] | None = None
        self._repeats = 0

    def update(self, value: tuple[int, ...] | None) -> tuple[int, ...] | None:
        """Возвращает значение, когда оно устоялось, иначе None."""
        if value is None:
            self._last, self._repeats = None, 0
            return None

        if value == self._last:
            self._repeats += 1
        else:
            self._last, self._repeats = value, 1

        return value if self._repeats >= self._required else None

    def reset(self) -> None:
        self._last, self._repeats = None, 0
