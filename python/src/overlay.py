"""Прозрачный оверлей поверх экрана.

Окно не перехватывает мышь и клавиатуру: WindowTransparentForInput делает
его сквозным, поэтому играть можно как обычно, подсказка просто висит сверху.
"""

from __future__ import annotations

from PyQt6.QtCore import Qt, QPointF
from PyQt6.QtGui import QColor, QFont, QPainter, QPen, QPolygonF
from PyQt6.QtWidgets import QWidget

ARROW_COLOR = QColor(80, 220, 120, 210)
TEXT_COLOR = QColor(255, 255, 255, 230)
FRAME_COLOR = QColor(80, 220, 120, 120)

_DIRECTIONS = {
    "left": (-1.0, 0.0),
    "right": (1.0, 0.0),
    "up": (0.0, -1.0),
    "down": (0.0, 1.0),
}


class Overlay(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
            | Qt.WindowType.WindowTransparentForInput
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating)

        self._rect: tuple[int, int, int, int] | None = None
        self._move: str | None = None
        self._status = "поиск доски..."

    def show_hint(self, rect: tuple[int, int, int, int] | None, move: str | None, status: str) -> None:
        self._rect, self._move, self._status = rect, move, status
        self.update()

    def paintEvent(self, event) -> None:  # noqa: N802 - имя задано Qt
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        ratio = self.devicePixelRatioF()
        painter.setFont(QFont("Segoe UI", 11))
        painter.setPen(QPen(TEXT_COLOR))
        painter.drawText(16, 28, self._status)

        if self._rect is None:
            return

        # mss отдаёт физические пиксели, Qt рисует в логических.
        x, y, w, h = (v / ratio for v in self._rect)
        painter.setPen(QPen(FRAME_COLOR, 2))
        painter.drawRect(int(x), int(y), int(w), int(h))

        if self._move not in _DIRECTIONS:
            return
        self._draw_arrow(painter, x + w / 2, y + h / 2, min(w, h) * 0.28, self._move)

    def _draw_arrow(self, painter: QPainter, cx: float, cy: float, length: float, move: str) -> None:
        dx, dy = _DIRECTIONS[move]
        tip = QPointF(cx + dx * length, cy + dy * length)
        tail = QPointF(cx - dx * length * 0.55, cy - dy * length * 0.55)

        painter.setPen(QPen(ARROW_COLOR, max(6.0, length * 0.16), Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        painter.drawLine(tail, tip)

        # Наконечник: две точки, отложенные от острия перпендикулярно ходу.
        wing = length * 0.42
        px, py = -dy, dx
        head = QPolygonF([
            tip,
            QPointF(tip.x() - dx * wing + px * wing * 0.6, tip.y() - dy * wing + py * wing * 0.6),
            QPointF(tip.x() - dx * wing - px * wing * 0.6, tip.y() - dy * wing - py * wing * 0.6),
        ])
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(ARROW_COLOR)
        painter.drawPolygon(head)
