"""Reading the drops an item capture recorded: python -m unittest discover -s AgentHarness"""

import tempfile
import unittest
from pathlib import Path

import captures

# The first drop BHTests/fixtures/filter-cases.txt holds, an Identify Scroll, as
# it is written there.
SCROLL_LINE = ("drop\tcode=isc\tname=Identify Scroll\taction=0\tpacketSize=22\tpacket="
               "\\x9c\\x00\\x16\\x10\\x15\\x01\\x00\\x00\\x10 \\xa0\\x00e\\x8c\\xcb\\xe3\\xaa\"mn\\x0c\\x04"
               "\tcharClass=1\tblocked=0")
SCROLL_PACKET = bytes.fromhex("9c00161015010000" "1020a000658ccbe3" "aa226d6e0c04")


class UnescapeTest(unittest.TestCase):
    def test_bytes_and_printable_text(self):
        self.assertEqual(captures.unescape("\\x9c\\x00a b"), b"\x9c\x00a b")

    def test_the_named_escapes(self):
        self.assertEqual(captures.unescape("a\\tb\\nc\\rd\\\\e"), b"a\tb\nc\rd\\e")

    def test_what_is_not_escaping_stands(self):
        self.assertEqual(captures.unescape("\\q\\x4"), b"\\q\\x4")
        self.assertEqual(captures.unescape("end\\"), b"end\\")

    def test_a_colour_code(self):
        self.assertEqual(captures.unescape("\\xffc4Ring").decode("latin-1"), "ÿc4Ring")


class ReadDropsTest(unittest.TestCase):
    def capture(self, *lines):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        path = Path(folder.name) / "item-captures.txt"
        path.write_text("\n".join(lines) + "\n", encoding="latin-1")
        return path

    def test_a_drop_is_read_with_its_packet(self):
        drops = captures.read_drops(self.capture("header\tbhVersion=BH", "rule\tcondition=x", SCROLL_LINE))
        self.assertEqual(len(drops), 1)
        self.assertEqual(drops[0]["code"], "isc")
        self.assertEqual(drops[0]["name"], "Identify Scroll")
        self.assertEqual(drops[0]["packet"], SCROLL_PACKET)
        self.assertEqual(drops[0]["blocked"], "0")
        self.assertEqual(drops[0]["line"], 3)

    def test_the_fixture_the_tests_replay(self):
        fixture = Path(__file__).resolve().parent.parent / "BHTests" / "fixtures" / "filter-cases.txt"
        drops = captures.read_drops(fixture)
        self.assertEqual(drops[0]["packet"], SCROLL_PACKET)
        self.assertTrue(all(drop["packet"][0] == 0x9C and drop["packet"][2] == len(drop["packet"])
                            for drop in drops))

    def test_a_missing_capture_says_so(self):
        with self.assertRaises(captures.CaptureError):
            captures.read_drops(Path(tempfile.gettempdir()) / "no-such-capture.txt")


class PickTest(unittest.TestCase):
    DROPS = [{"code": "isc", "name": "Identify Scroll"}, {"code": "rin", "name": "ring"},
             {"code": "isc", "name": "Identify Scroll"}]

    def test_by_code_and_number(self):
        self.assertIs(captures.pick(self.DROPS, code="isc", nth=1), self.DROPS[2])

    def test_by_part_of_the_name(self):
        self.assertIs(captures.pick(self.DROPS, name="RING"), self.DROPS[1])

    def test_one_too_many_says_how_many_there_are(self):
        with self.assertRaisesRegex(captures.CaptureError, "2 drops with code 'isc', so no number 2"):
            captures.pick(self.DROPS, code="isc", nth=2)


if __name__ == "__main__":
    unittest.main()
