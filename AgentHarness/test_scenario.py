"""The parts of the harness that need no game: reading scenarios, judging
checks, marking evidence, writing reports and overriding settings.

    python -m unittest discover -s AgentHarness
"""

import copy
import unittest

import bhctl
import scenario
from bhctl import HarnessError

SETTINGS = (b"// Slash Diablo Default BH_settings\r\n"
            b"Show Settings:          True, VK_NUMPAD8\r\n"
            b"Monster Color[Normal]:   0x5B\r\n"
            b"//Whisper Color[*chat]: 9\r\n"
            b"Reveal Mode: 1\r\n")

SETTINGS_TEXT = {"kind": "text", "rect": [238, 550, 52, 10], "font": 0, "color": 0, "text": "Settings"}
CODEX_TEXT = {"kind": "text", "rect": [238, 569, 38, 10], "font": 0, "color": 0, "text": "Codex"}
CODEX_BOX = {"kind": "box", "rect": [234, 566, 46, 15], "color": 0, "trans": 0}
LEVEL_NAME = {"kind": "text", "rect": [1042, 560, 60, 7], "font": 6, "color": 4, "text": "Den of Evilÿc4"}
PATH_LINE = {"kind": "line", "from": [30, 40], "to": [10, 45], "color": 132}
FRAME = [SETTINGS_TEXT, CODEX_TEXT, CODEX_BOX, LEVEL_NAME, PATH_LINE]

GAME_STATE = {"ok": True, "screen": "game", "size": [1344, 700],
              "game": {"class": "sorceress", "area": 1, "panels": ["inventory"], "position": [5000, 4000]}}


def scenario_with(*steps, **top):
    return {"title": "a scenario", **top, "steps": list(steps)}


class SettingsOverrides(unittest.TestCase):
    def test_a_key_the_file_sets_is_replaced_where_it_is(self):
        out = bhctl.override_settings(SETTINGS, {"Show Settings": "False, VK_F11"})
        self.assertEqual(out.splitlines()[1], b"Show Settings: False, VK_F11")
        self.assertEqual(len(out.splitlines()), len(SETTINGS.splitlines()))

    def test_a_subkeyed_key_is_matched_whole(self):
        out = bhctl.override_settings(SETTINGS, {"Monster Color[Normal]": "0x84"})
        self.assertIn(b"\r\nMonster Color[Normal]: 0x84\r\n", out)

    def test_a_key_the_file_does_not_set_is_added_at_the_end(self):
        out = bhctl.override_settings(SETTINGS, {"Codex Button": "False"})
        self.assertTrue(out.endswith(b"Reveal Mode: 1\r\nCodex Button: False\r\n"))

    def test_a_commented_out_key_is_not_set_but_added(self):
        out = bhctl.override_settings(SETTINGS, {"Whisper Color[*chat]": "4"})
        self.assertIn(b"//Whisper Color[*chat]: 9\r\n", out)
        self.assertTrue(out.endswith(b"Whisper Color[*chat]: 4\r\n"))

    def test_the_rest_of_the_file_is_untouched(self):
        out = bhctl.override_settings(SETTINGS, {"Reveal Mode": "2"})
        self.assertEqual(out.replace(b"Reveal Mode: 2", b"Reveal Mode: 1"), SETTINGS)

    def test_a_file_without_a_final_newline_gets_one_before_an_addition(self):
        out = bhctl.override_settings(b"Reveal Mode: 1", {"Codex Button": "True"})
        self.assertEqual(out, b"Reveal Mode: 1\nCodex Button: True\n")

    def test_keys_are_read_up_to_their_first_colon_after_any_subkey(self):
        self.assertEqual(bhctl.settings_key(b"Item Display[flag:x]: True"), "Item Display[flag:x]")
        self.assertEqual(bhctl.settings_key(b"  Reveal Mode : 1"), "Reveal Mode")
        self.assertIsNone(bhctl.settings_key(b"// Reveal Mode: 1"))
        self.assertIsNone(bhctl.settings_key(b"no colon here"))

    def test_the_shipped_settings_set_the_settings_hotkey(self):
        self.assertIn("Show Settings", bhctl.shipped_settings_keys())


class ReadingScenarios(unittest.TestCase):
    def assertRefused(self, data, words):
        with self.assertRaises(HarnessError) as caught:
            scenario.load_scenario(data)
        self.assertIn(words, str(caught.exception))

    def test_a_scenario_starts_in_a_game_as_agent_by_default(self):
        loaded = scenario.load_scenario(scenario_with({"do": ["key:numpad8"]}))
        self.assertEqual(loaded["start"], "game")
        self.assertEqual(loaded["name"], "Agent")
        self.assertEqual(loaded["settings"], {})

    def test_every_kind_of_step_is_read(self):
        scenario.load_scenario(scenario_with(
            {"do": ["click:400,306", "wait:0.5", "text:Agent", "key:esc"]},
            {"wait": ["main_menu", "character_list"], "timeout": 30},
            {"check": "the game is up", "state": {"screen": "game"}},
            {"check": "two buttons", "drawn": {"kind": "box", "inside": [0, 500, 400, 200]}, "count": 2,
             "hold": ["alt"], "timeout": 2},
            {"shot": "after", "crop": [0, 0, 100, 100]},
            {"drawlog": "frame"},
            {"leave": True},
            {"enter": True},
            start="main_menu", name="Tester", settings={"Show Settings": "True, VK_NUMPAD8"}))

    def test_mistakes_are_found_before_anything_runs(self):
        self.assertRefused([], "a scenario is an object")
        self.assertRefused({"steps": [{"leave": True}]}, '"title"')
        self.assertRefused(scenario_with({"leave": True}, start="lobby"), '"start"')
        self.assertRefused(scenario_with({"leave": True}, name="Agent 2"), '"name"')
        self.assertRefused(scenario_with({"leave": True}, settings={"Reveal Mode": 1}), '"settings"')
        self.assertRefused(scenario_with(), '"steps"')
        self.assertRefused(scenario_with({"leave": True}, extra=1), "unknown fields extra")

    def test_a_step_is_refused_with_its_number_and_reason(self):
        self.assertRefused(scenario_with({"leave": True}, {"jump": True}), "step 2: a step names exactly one")
        self.assertRefused(scenario_with({"leave": True, "shot": "x"}), "exactly one")
        self.assertRefused(scenario_with({"do": ["click:1"]}), "expected click:X,Y")
        self.assertRefused(scenario_with({"do": ["key:hyper"]}), "unknown key")
        self.assertRefused(scenario_with({"do": ["fly:1"]}), "unknown action")
        self.assertRefused(scenario_with({"wait": "game", "count": 1}), "does not take count")
        self.assertRefused(scenario_with({"check": "x"}), 'a check has "state", and "drawn" or "item"')
        self.assertRefused(scenario_with({"check": "x", "drawn": {"txt": "a"}}), "no field 'txt'")
        self.assertRefused(scenario_with({"check": "x", "drawn": {"kind": "circle"}}), '"kind"')
        self.assertRefused(scenario_with({"check": "x", "drawn": {"rect": [1, 2, 3]}}), '"rect"')
        self.assertRefused(scenario_with({"check": "x", "state": {"screen": "game"}, "count": 1}),
                           '"count" counts')
        self.assertRefused(scenario_with({"check": "x", "drawn": {"text": "a"}, "hold": ["hyper"]}),
                           "unknown key")
        self.assertRefused(scenario_with({"shot": "../x"}), "a name of letters")
        self.assertRefused(scenario_with({"leave": "yes"}), '"leave" is true')

    def test_a_scenario_can_play_a_character_in_a_difficulty_it_has_reached(self):
        loaded = scenario.load_scenario(scenario_with({"leave": True}, character="fixtures/Hero.d2s",
                                                      difficulty="hell"))
        self.assertEqual((loaded["character"], loaded["difficulty"]), ("fixtures/Hero.d2s", "hell"))
        self.assertIsNone(scenario.load_scenario(scenario_with({"leave": True}))["character"])

    def test_a_character_is_refused_alongside_a_new_ones_name_or_a_new_one_out_of_normal(self):
        self.assertRefused(scenario_with({"leave": True}, character="Hero.d2s", name="Hero"), '"name" names')
        self.assertRefused(scenario_with({"leave": True}, difficulty="hell"), "a new character plays Normal")
        self.assertRefused(scenario_with({"leave": True}, character="Hero.d2s", difficulty="easy"), '"difficulty"')
        self.assertRefused(scenario_with({"leave": True}, character=""), '"character"')

    def test_drops_and_item_checks_are_read(self):
        scenario.load_scenario(scenario_with(
            {"drop": {"capture": "BHTests/fixtures/filter-cases.txt", "code": "rin", "nth": 2}, "offset": [2, -1]},
            {"drop": {"packet": "9c0016"}},
            {"check": "the ring lies there", "item": {"code": "rin", "contains": "Ring"}, "count": 1,
             "hold": ["alt"]}))

    def test_a_drop_or_item_check_is_refused_with_its_reason(self):
        self.assertRefused(scenario_with({"drop": {}}), 'a "capture" to pick from or a "packet"')
        self.assertRefused(scenario_with({"drop": {"capture": "c.txt", "packet": "9c"}}), "one of them")
        self.assertRefused(scenario_with({"drop": {"packet": "9c0", }}), "as hex")
        self.assertRefused(scenario_with({"drop": {"packet": "9c", "code": "rin"}}), "pick from a")
        self.assertRefused(scenario_with({"drop": {"capture": "c.txt", "nth": -1}}), '"nth"')
        self.assertRefused(scenario_with({"drop": {"capture": "c.txt", "quality": "unique"}}), "does not take quality")
        self.assertRefused(scenario_with({"drop": {"capture": "c.txt"}, "offset": [1]}), '"offset"')
        self.assertRefused(scenario_with({"check": "x", "item": {"colour": 3}}), "no field 'colour'")
        self.assertRefused(scenario_with({"check": "x", "item": {"code": "rin"}, "drawn": {"text": "a"}}),
                           '"drawn" or "item", not both')


class StateChecks(unittest.TestCase):
    def test_every_path_must_hold_its_value(self):
        ok, seen = scenario.check_state({"screen": "game", "game.class": "sorceress"}, GAME_STATE)
        self.assertTrue(ok)
        self.assertEqual(seen, {"screen": "game", "game.class": "sorceress"})

    def test_a_value_that_differs_fails_and_says_what_was_there(self):
        ok, seen = scenario.check_state({"game.panels": []}, GAME_STATE)
        self.assertFalse(ok)
        self.assertEqual(seen, {"game.panels": ["inventory"]})

    def test_a_path_into_a_list_takes_an_index(self):
        ok, _ = scenario.check_state({"size.0": 1344, "game.panels.0": "inventory"}, GAME_STATE)
        self.assertTrue(ok)

    def test_a_missing_path_fails_even_against_null(self):
        ok, seen = scenario.check_state({"game.mercenary": None}, GAME_STATE)
        self.assertFalse(ok)
        self.assertEqual(seen, {"game.mercenary": None})


class DrawnChecks(unittest.TestCase):
    def test_text_is_matched_as_a_player_reads_it(self):
        ok, found = scenario.check_drawn({"text": "Den of Evil"}, None, FRAME)
        self.assertTrue(ok)
        self.assertEqual(found, [LEVEL_NAME])

    def test_every_field_given_must_match(self):
        self.assertTrue(scenario.check_drawn({"text": "Codex", "font": 0, "color": 0,
                                              "rect": [238, 569, 38, 10]}, None, FRAME)[0])
        self.assertFalse(scenario.check_drawn({"text": "Codex", "rect": [238, 570, 38, 10]}, None, FRAME)[0])
        self.assertFalse(scenario.check_drawn({"text": "Codex", "color": 4}, None, FRAME)[0])

    def test_contains_finds_part_of_a_text(self):
        ok, found = scenario.check_drawn({"contains": "Sett"}, None, FRAME)
        self.assertEqual(found, [SETTINGS_TEXT])

    def test_inside_takes_what_lies_wholly_within_an_area(self):
        _, found = scenario.check_drawn({"inside": [230, 562, 60, 25]}, None, FRAME)
        self.assertEqual(found, [CODEX_TEXT, CODEX_BOX])
        _, found = scenario.check_drawn({"inside": [235, 562, 60, 25]}, None, FRAME)
        self.assertEqual(found, [CODEX_TEXT])

    def test_a_line_is_matched_by_the_box_around_it(self):
        _, found = scenario.check_drawn({"kind": "line", "rect": [10, 40, 20, 5]}, None, FRAME)
        self.assertEqual(found, [PATH_LINE])

    def test_count_is_exact_and_zero_means_not_drawn(self):
        self.assertTrue(scenario.check_drawn({"font": 0}, 2, FRAME)[0])
        self.assertFalse(scenario.check_drawn({"font": 0}, 1, FRAME)[0])
        self.assertTrue(scenario.check_drawn({"text": "Help"}, 0, FRAME)[0])
        self.assertFalse(scenario.check_drawn({"text": "Codex"}, 0, FRAME)[0])

    def test_a_check_that_found_nothing_shows_what_read_alike(self):
        near = scenario.near_misses({"text": "codex", "color": 4}, FRAME)
        self.assertEqual(near, [CODEX_TEXT])
        self.assertEqual(scenario.near_misses({"kind": "box"}, FRAME), [])

    def test_items_are_described_as_a_person_reads_them(self):
        self.assertEqual(scenario.describe_item(LEVEL_NAME),
                         'text "Den of Evil" at 1042,560 60x7, font 6, colour 4')
        self.assertEqual(scenario.describe_item(PATH_LINE), "line 30,40 to 10,45, colour 132")
        self.assertEqual(scenario.describe_item({"kind": "frame", "rect": [0, 0, 460, 400]}),
                         "frame around 0,0 460x400")
        self.assertEqual(scenario.describe_matcher({"text": "Codex", "font": 0}, None),
                         'at least one item with text "Codex", font 0')
        self.assertEqual(scenario.describe_matcher({"kind": "box"}, 0), 'no item with kind "box"')
        self.assertEqual(scenario.describe_matcher({"kind": "box"}, 2), 'exactly 2 items with kind "box"')


RING = {"id": 2130706433, "code": "rin", "name": "ÿc5T6 ÿc3Ringÿc3", "position": [4876, 4230]}
SCROLL = {"id": 2130706432, "code": "isc", "name": "ÿc:+ ÿc0ID", "position": [4875, 4230]}
GROUND = [SCROLL, RING]


class ItemChecks(unittest.TestCase):
    def test_an_item_is_named_as_a_player_reads_it(self):
        self.assertTrue(scenario.check_items({"code": "rin", "name": "T6 Ring"}, None, GROUND)[0])
        self.assertFalse(scenario.check_items({"code": "rin", "name": "Ring"}, None, GROUND)[0])

    def test_every_field_given_must_match(self):
        self.assertEqual(scenario.check_items({"contains": "Ring", "id": RING["id"]}, None, GROUND)[1], [RING])
        self.assertFalse(scenario.check_items({"contains": "Ring", "code": "isc"}, None, GROUND)[0])

    def test_count_zero_means_it_is_not_on_the_ground(self):
        self.assertTrue(scenario.check_items({"code": "amu"}, 0, GROUND)[0])
        self.assertFalse(scenario.check_items({"code": "rin"}, 0, GROUND)[0])

    def test_a_check_that_found_nothing_shows_the_items_most_like_it(self):
        self.assertEqual(scenario.near_item_misses({"code": "rin", "name": "T5 Ring"}, GROUND), [RING])
        self.assertEqual(scenario.near_item_misses({"contains": "id"}, GROUND), [SCROLL])

    def test_an_item_is_described_by_its_code_name_and_place(self):
        self.assertEqual(scenario.describe_ground_item(RING), 'rin "T6 Ring" at 4876,4230, id 2130706433')
        self.assertEqual(scenario.describe_item_matcher({"code": "rin"}, 0), 'no item on the ground with code "rin"')


class ResolvingDrops(unittest.TestCase):
    def test_a_drop_is_picked_from_its_capture_before_the_game_starts(self):
        loaded = scenario.load_scenario(scenario_with(
            {"drop": {"capture": "BHTests/fixtures/filter-cases.txt", "code": "rin"}}, {"leave": True},
            {"drop": {"packet": "9c0016"}}))
        drops = scenario.resolve_drops(loaded)
        packet, dropped = drops[1]
        self.assertEqual((packet[0], dropped["code"], dropped["name"]), (0x9C, "rin", "ring"))
        self.assertEqual(dropped["capture"], "BHTests\\fixtures\\filter-cases.txt")
        self.assertEqual(drops[3], (b"\x9c\x00\x16", {"packet": "9c0016"}))

    def test_a_drop_the_capture_lacks_is_refused_with_its_step(self):
        loaded = scenario.load_scenario(scenario_with(
            {"leave": True}, {"drop": {"capture": "BHTests/fixtures/filter-cases.txt", "code": "zzz"}}))
        with self.assertRaisesRegex(HarnessError, "step 2: the capture has 0 drops with code 'zzz'"):
            scenario.resolve_drops(loaded)


class Marking(unittest.TestCase):
    def pixel(self, picture, width, x, y):
        offset = (y * width + x) * 4
        return picture[offset:offset + 4]

    def test_a_rectangle_is_outlined_just_outside_itself(self):
        width, height = 8, 8
        black = bytes(width * height * 4)
        marked = scenario.mark(black, width, height, [([2, 2, 3, 3], scenario.MATCH_BGRA)])
        for x, y in [(1, 1), (5, 1), (1, 5), (5, 5), (3, 1), (1, 3)]:
            self.assertEqual(self.pixel(marked, width, x, y), scenario.MATCH_BGRA, (x, y))
        for x, y in [(2, 2), (4, 4), (3, 3), (6, 6), (0, 0)]:
            self.assertEqual(self.pixel(marked, width, x, y), b"\0\0\0\0", (x, y))

    def test_each_rectangle_takes_its_own_colour(self):
        width, height = 8, 8
        marked = scenario.mark(bytes(width * height * 4), width, height,
                               [([1, 1, 1, 1], scenario.MATCH_BGRA), ([5, 5, 1, 1], scenario.NEAR_BGRA)])
        self.assertEqual(self.pixel(marked, width, 0, 0), scenario.MATCH_BGRA)
        self.assertEqual(self.pixel(marked, width, 4, 4), scenario.NEAR_BGRA)

    def test_an_outline_off_the_edge_is_clipped_and_the_original_kept(self):
        width, height = 4, 4
        black = bytes(width * height * 4)
        marked = scenario.mark(black, width, height, [([0, 0, 10, 10], scenario.MATCH_BGRA),
                                                      ([50, 50, 2, 2], scenario.MATCH_BGRA)])
        self.assertEqual(len(marked), len(black))
        self.assertEqual(black, bytes(width * height * 4))
        self.assertEqual(self.pixel(marked, width, 0, 0), b"\0\0\0\0")


class Reports(unittest.TestCase):
    def report(self, records, **extra):
        steps = [{"do": ["key:numpad8"]},
                 {"check": "the settings window opens", "drawn": {"text": "Settings", "font": 0}},
                 {"check": "no panel is open", "state": {"game.panels": []}},
                 {"shot": "after"}]
        report = {"title": "BH's settings open", "scenario": "smoke.json",
                  "build": {"bh": "AgentRelease\\BH.dll", "sha256": "0123456789ab", "built": "2026-11-26 20:00",
                            "commit": "061d2131", "dirty": True},
                  "settings": {"Show Settings": "True, VK_NUMPAD8"}, "notes": [], "started": "2026-11-26 20:01:00",
                  "seconds": 31, "run": "20261126-200100-abcd", "steps": steps, "records": records, **extra}
        report["result"] = scenario.result_of(report)
        return report

    PASSED = [{"index": 1, "outcome": "done"},
              {"index": 2, "outcome": "passed", "check": "the settings window opens",
               "looked_for": 'at least one drawn with text "Settings", font 0', "found": [SETTINGS_TEXT],
               "drawn_items": 5, "drawlog": "02-x.drawlog.json", "evidence": ["02-x.png", "02-x.marked.png"]},
              {"index": 3, "outcome": "passed", "check": "no panel is open", "expected": {"game.panels": []},
               "seen": {"game.panels": []}, "evidence": ["03-y.png"]},
              {"index": 4, "outcome": "done", "evidence": ["04-after.png"]}]

    def test_a_passing_report_says_so_and_shows_its_evidence(self):
        text = scenario.render_markdown(self.report(self.PASSED))
        self.assertIn("**Passed:** all 2 checks hold.", text)
        self.assertIn("| 2 | the settings window opens | passed |", text)
        self.assertIn('- text "Settings" at 238,550 52x10, font 0, colour 0', text)
        self.assertIn("![02-x.marked.png](02-x.marked.png)\n\nUnmarked: [02-x.png](02-x.png)", text)
        self.assertNotIn("![02-x.png]", text)
        self.assertIn("- `game.panels` is `[]`, as expected", text)
        self.assertIn("| Checkout | `061d2131`, with uncommitted changes |", text)

    def test_a_failing_check_shows_what_was_there_instead(self):
        records = copy.deepcopy(self.PASSED)
        records[2].update(outcome="failed", seen={"game.panels": ["inventory"]})
        report = self.report(records)
        self.assertEqual(report["result"], "failed")
        text = scenario.render_markdown(report)
        self.assertIn("**Failed:** 1 of 2 checks do not hold.", text)
        self.assertIn('- `game.panels` is `["inventory"]`, expected `[]`', text)

    def test_an_error_names_the_step_and_leaves_the_rest_not_run(self):
        records = copy.deepcopy(self.PASSED[:1])
        records.append({"index": 2, "outcome": "error", "error": "the game exited"})
        report = self.report(records, error="the game exited", error_step=2, error_evidence=["02-at-error.png"])
        self.assertEqual(report["result"], "error")
        text = scenario.render_markdown(report)
        self.assertIn("**Could not finish:** the game exited", text)
        self.assertIn("### 2. the settings window opens (error)", text)
        self.assertIn("![02-at-error.png](02-at-error.png)", text)
        self.assertIn("| 3 | no panel is open | not run |", text)
        self.assertIn("### 4. shot `after` (not run)", text)

    def test_a_failure_to_start_is_reported_before_the_steps(self):
        report = self.report([], error="launch failed (3)", error_step=0)
        text = scenario.render_markdown(report)
        self.assertIn("Starting the game failed: launch failed (3)", text)
        self.assertIn("### 1. do `key:numpad8` (not run)", text)

    def test_text_from_the_game_cannot_break_the_markdown(self):
        self.assertEqual(scenario.md_text("a|b *c* <d>"), r"a\|b \*c\* \<d\>")

    def test_code_is_shown_verbatim_but_for_a_table_cell_ending_bar(self):
        self.assertEqual(scenario.md_code("Show Settings: True, VK_F10"), "`Show Settings: True, VK_F10`")
        self.assertEqual(scenario.md_code("a|b"), r"`a\|b`")
        self.assertEqual(scenario.md_code("`x`"), "`` `x` ``")

    def test_settings_are_listed_verbatim(self):
        text = scenario.render_markdown(self.report(self.PASSED))
        self.assertIn("| Settings | `Show Settings: True, VK_NUMPAD8` |", text)

    def test_a_near_miss_is_listed_under_its_colour(self):
        records = copy.deepcopy(self.PASSED)
        records[1].update(outcome="failed", found=[], near_misses=[SETTINGS_TEXT])
        text = scenario.render_markdown(self.report(records))
        self.assertIn("Found 0.\n\nWhat read most like it, outlined in yellow:\n- text \"Settings\"", text)

    def drop_report(self, **landing):
        steps = [{"drop": {"capture": "BHTests/fixtures/filter-cases.txt", "code": "rin"}, "offset": [2, 0]},
                 {"check": "the ring lies there", "item": {"code": "rin", "name": "T6 Ring"}}]
        records = [{"index": 1, "outcome": "done", "id": RING["id"], "position": [4876, 4230], "landed": True,
                    "dropped": {"capture": "BHTests\\fixtures\\filter-cases.txt", "line": 6533, "code": "rin",
                                "name": "ring"}, **landing},
                   {"index": 2, "outcome": "passed", "check": "the ring lies there", "found_items": [RING],
                    "ground_items": 2, "looked_for": 'at least one item on the ground with code "rin"',
                    "evidence": ["02-the-ring-lies-there.png"]}]
        return self.report(records, character="AgentHarness/fixtures/Hero.d2s", difficulty="hell",
                           steps=steps)

    def test_a_drop_says_what_it_was_and_where_it_landed(self):
        text = scenario.render_markdown(self.drop_report())
        self.assertIn("### 1. drop from `BHTests/fixtures/filter-cases.txt`, code `rin`, 2,0 from the character",
                      text)
        self.assertIn('`rin` "ring", recorded at line 6533 of `BHTests\\fixtures\\filter-cases.txt`, landed at'
                      ' 4876,4230 as item 2130706433.', text)
        self.assertIn("Found 1:\n- rin \"T6 Ring\" at 4876,4230, id 2130706433", text)
        self.assertIn("| Character | `AgentHarness/fixtures/Hero.d2s`, in Hell |", text)

    def test_a_drop_the_filter_hid_says_so(self):
        text = scenario.render_markdown(self.drop_report(landed=False))
        self.assertIn("did not land at 4876,4230: BH's item filter stopped it", text)

    def test_evidence_files_are_named_from_the_check(self):
        self.assertEqual(scenario.slug("The Codex button sits under Settings!"), "the-codex-button-sits-under-settings")
        self.assertEqual(scenario.slug("!!!"), "check")
        self.assertLessEqual(len(scenario.slug("word " * 30)), 40)


if __name__ == "__main__":
    unittest.main()
