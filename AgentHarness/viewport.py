"""Where the game's canvas lies in its window.

The canvas is what the game draws on: 800x600 in the menus and the in-game
resolution in a game. Everything a scenario names is in canvas pixels, which is
also what BH's draw log is in. Under the GDI renderer the window is the canvas.
D2GL scales the canvas to fit its window and centres it, so a click has to be
aimed at the window pixel D2GL turns back into the canvas pixel meant, and a
rectangle of the canvas is another rectangle of a screenshot.

The arithmetic is D2GL's (win32.cpp, setWindowMetrics and WndProc), in the
single precision it uses, so the answers are the ones it gets.
"""

import ctypes
import math


def single(value):
    return ctypes.c_float(value).value


class Viewport:
    """The part of a window of the size given that D2GL draws a canvas of the
    size given on: as wide or as tall as the window, whichever keeps its
    shape."""

    def __init__(self, window, canvas):
        self.window = tuple(window)
        self.canvas = tuple(canvas)
        window_w, window_h = self.window
        canvas_w, canvas_h = self.canvas
        width = window_w
        height = int(single(single(single(canvas_h) / single(canvas_w)) * single(window_w)))
        if height > window_h:
            width = int(single(single(single(canvas_w) / single(canvas_h)) * single(window_h)))
            height = window_h
        self.size = (width, height)
        self.offset = (window_w // 2 - width // 2, window_h // 2 - height // 2)

    @property
    def exact(self):
        """Whether the window is the canvas, pixel for pixel."""
        return self.size == self.canvas and self.offset == (0, 0)

    def to_canvas(self, x, y):
        """The canvas pixel D2GL takes a window pixel for."""
        return tuple(self._to_canvas(axis, value) for axis, value in enumerate((x, y)))

    def _to_canvas(self, axis, value):
        unscale = single(single(self.canvas[axis]) / single(self.size[axis]))
        within = max(value - self.offset[axis], 0)
        return min(int(single(single(within) * unscale)), self.canvas[axis])

    def to_window(self, x, y):
        """The window pixel to aim at for D2GL to take it for the canvas pixel:
        the middle one of those it does, or the nearest where a window smaller
        than its canvas leaves the pixel none."""
        return tuple(self._to_window(axis, value) for axis, value in enumerate((x, y)))

    def _to_window(self, axis, value):
        scale = self.size[axis] / self.canvas[axis]
        start = self.offset[axis] + math.floor(value * scale)
        reach = math.ceil(scale) + 2
        candidates = range(max(start - reach, self.offset[axis]), start + reach + 1)
        hits = [pixel for pixel in candidates if self._to_canvas(axis, pixel) == value]
        if hits:
            return hits[len(hits) // 2]
        return min(candidates, key=lambda pixel: abs(self._to_canvas(axis, pixel) - value))

    def rect_to_window(self, rect):
        """The window pixels a rectangle of the canvas covers, as [x, y, w, h]."""
        x, y, w, h = rect
        left, right = self._span(0, x, w)
        top, bottom = self._span(1, y, h)
        return [left, top, right - left, bottom - top]

    def _span(self, axis, start, length):
        scale = self.size[axis] / self.canvas[axis]
        low = self.offset[axis] + math.floor(start * scale)
        high = self.offset[axis] + math.ceil((start + length) * scale)
        return low, max(high, low)
