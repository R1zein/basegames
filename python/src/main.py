"""Точка входа: захват экрана -> распознавание -> решатель -> оверлей.

Тяжёлая часть (снимок экрана и просчёт хода) живёт в отдельном потоке,
иначе оверлей замирал бы на каждом просчёте.
"""

from __future__ import annotations

import argparse
import sys
import time

from PyQt6.QtCore import QThread, pyqtSignal
from PyQt6.QtWidgets import QApplication

import board as board_module
import solver
from capture import ScreenCapture, StableFrame
from overlay import Overlay


class SolverWorker(QThread):
    """Фоновый цикл: ищет доску, читает позицию, считает лучший ход."""

    hint = pyqtSignal(object, object, str)

    def __init__(self, monitor: int, depth: int | None, interval: float) -> None:
        super().__init__()
        self._monitor = monitor
        self._depth = depth
        self._interval = interval
        self._running = True

    def stop(self) -> None:
        self._running = False

    def run(self) -> None:  # noqa: D102 - контракт QThread
        capture = ScreenCapture(self._monitor)
        stable = StableFrame(required=2)
        try:
            while self._running:
                started = time.monotonic()
                self._tick(capture, stable)
                remaining = self._interval - (time.monotonic() - started)
                if remaining > 0:
                    self.msleep(int(remaining * 1000))
        finally:
            capture.close()

    def _tick(self, capture: ScreenCapture, stable: StableFrame) -> None:
        frame = capture.grab()
        rect = board_module.find_board(frame)
        if rect is None:
            stable.reset()
            self.hint.emit(None, None, "доска не найдена")
            return

        position = board_module.read_board(frame, rect)
        if not board_module.looks_like_game(position):
            stable.reset()
            self.hint.emit(rect, None, "доска пуста")
            return

        settled = stable.update(position)
        if settled is None:
            # Кадр поймали посреди анимации — ждём, пока картинка устоится.
            self.hint.emit(rect, None, "ход в процессе...")
            return

        move = solver.best_move(settled, self._depth)
        status = f"ход: {move}" if move else "ходов нет"
        self.hint.emit(rect, move, status)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Overlay hints for 2048")
    parser.add_argument("--monitor", type=int, default=1, help="index of monitor to watch")
    parser.add_argument("--depth", type=int, default=None, help="search depth (default: adaptive)")
    parser.add_argument("--interval", type=float, default=0.25, help="seconds between frames")
    return parser.parse_args(argv)


def main() -> int:
    args = parse_args()
    app = QApplication(sys.argv)

    overlay = Overlay()
    screen = app.primaryScreen().geometry()
    overlay.setGeometry(screen)
    overlay.show()

    worker = SolverWorker(args.monitor, args.depth, args.interval)
    worker.hint.connect(overlay.show_hint)
    app.aboutToQuit.connect(worker.stop)
    worker.start()

    code = app.exec()
    worker.stop()
    worker.wait(2000)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
