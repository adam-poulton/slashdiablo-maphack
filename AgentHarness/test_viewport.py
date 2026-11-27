"""Where the canvas lies in a window, and that a click aimed through it lands
on the pixel meant.

    python -m unittest discover -s AgentHarness
"""

import unittest

from viewport import Viewport

MENU = (800, 600)
GAME = (1344, 700)


class Placing(unittest.TestCase):
    def test_a_window_the_size_of_its_canvas_is_the_canvas(self):
        view = Viewport(GAME, GAME)
        self.assertTrue(view.exact)
        self.assertEqual(view.to_window(0, 0), (0, 0))
        self.assertEqual(view.to_window(1343, 699), (1343, 699))
        self.assertEqual(view.rect_to_window([238, 550, 52, 10]), [238, 550, 52, 10])

    def test_a_narrower_canvas_is_as_tall_as_the_window_and_centred(self):
        view = Viewport(GAME, MENU)
        self.assertFalse(view.exact)
        self.assertEqual(view.size, (933, 700))
        self.assertEqual(view.offset, (206, 0))

    def test_a_wider_canvas_is_as_wide_as_the_window_and_centred(self):
        view = Viewport((1600, 1200), GAME)
        self.assertEqual(view.size, (1600, 833))
        self.assertEqual(view.offset, (0, 184))

    def test_a_canvas_of_the_windows_shape_fills_it(self):
        view = Viewport((1612, 840), GAME)
        self.assertEqual((view.size, view.offset), ((1612, 839), (0, 1)))


class Aiming(unittest.TestCase):
    def assertEveryPixelCanBeHit(self, window, canvas):
        view = Viewport(window, canvas)
        for x in range(canvas[0]):
            self.assertEqual(view.to_canvas(*view.to_window(x, 0))[0], x, (window, canvas, "x", x))
        for y in range(canvas[1]):
            self.assertEqual(view.to_canvas(*view.to_window(0, y))[1], y, (window, canvas, "y", y))

    def test_every_canvas_pixel_can_be_hit_in_a_window_at_least_its_size(self):
        for window in (GAME, (1612, 840), (1680, 875), (2560, 1440), (1600, 1200)):
            for canvas in (MENU, GAME, (640, 480), (1068, 600)):
                if window[0] >= canvas[0] and window[1] >= canvas[1]:
                    self.assertEveryPixelCanBeHit(window, canvas)

    def test_a_click_is_aimed_at_the_middle_of_the_pixels_that_hit(self):
        view = Viewport((1600, 1200), MENU)
        self.assertEqual(view.to_window(400, 300), (801, 601))
        self.assertEqual(view.to_canvas(800, 600), (400, 300))
        self.assertEqual(view.to_canvas(801, 601), (400, 300))

    def test_a_pixel_a_smaller_window_cannot_show_is_aimed_at_its_neighbour(self):
        view = Viewport((800, 600), GAME)
        for x in (0, 1, 671, 672, 1343):
            landed = view.to_canvas(*view.to_window(x, 0))[0]
            self.assertLessEqual(abs(landed - x), 1, x)

    def test_the_bars_beside_the_canvas_are_its_edge(self):
        view = Viewport(GAME, MENU)
        self.assertEqual(view.to_canvas(0, 350)[0], 0)
        self.assertEqual(view.to_canvas(1343, 350)[0], 800)


class Measuring(unittest.TestCase):
    def test_a_rectangle_covers_every_window_pixel_its_canvas_pixels_touch(self):
        view = Viewport(GAME, MENU)
        self.assertEqual(view.rect_to_window([0, 0, 800, 600]), [206, 0, 933, 700])
        x, y, w, h = view.rect_to_window([400, 300, 1, 1])
        self.assertEqual((x, y), (206 + 466, 350))
        self.assertEqual((w, h), (2, 2))

    def test_a_rectangle_with_no_size_keeps_none(self):
        view = Viewport((1600, 1200), MENU)
        self.assertEqual(view.rect_to_window([10, 10, 0, 5]), [20, 20, 0, 10])


if __name__ == "__main__":
    unittest.main()
