"""Runs a scenario: a sandboxed game driven through a list of steps, with
checks on what it shows, written up as a report that carries the evidence for
every check. The format is in README.md, under Scenarios.

    python AgentHarness/bhctl.py scenario FILE.json [--out DIR] [--keep]
"""

import hashlib
import json
import re
import subprocess
import time
from pathlib import Path

import bhctl
import captures
from bhctl import HarnessError

REPORTS = bhctl.REPO / "AgentHarness" / "reports"
STARTS = ("main_menu", "game")
TOP_FIELDS = {"title", "start", "name", "character", "difficulty", "settings", "steps"}
CHECK_TIMEOUT_SECONDS = 5
CHECK_RETRY_SECONDS = 0.25
NEAR_MISSES = 5
# Items are outlined one pixel outside their rectangle, so the outline never
# covers what it marks: what a check matched in magenta, and for a check that
# matched nothing, what read most like it in yellow.
MATCH_BGRA = b"\xff\x00\xff\xff"
NEAR_BGRA = b"\x00\xff\xff\xff"

STEP_KINDS = {
    "do": {"do"},
    "wait": {"wait", "timeout"},
    "check": {"check", "state", "drawn", "item", "count", "hold", "timeout"},
    "drop": {"drop", "offset"},
    "shot": {"shot", "crop"},
    "drawlog": {"drawlog"},
    "leave": {"leave"},
    "enter": {"enter"},
}
MATCHER_FIELDS = {"kind", "text", "contains", "font", "color", "rect", "inside"}
DRAWN_KINDS = ("text", "box", "frame", "line")
ITEM_FIELDS = {"code", "name", "contains", "id"}
DROP_FIELDS = {"packet", "capture", "code", "name", "nth"}


# --- reading a scenario -------------------------------------------------------

def fail(where, message):
    raise HarnessError(f"scenario: {where}: {message}")


def is_rect(value):
    return (isinstance(value, list) and len(value) == 4
            and all(isinstance(v, int) and not isinstance(v, bool) for v in value))


def validate_matcher(where, matcher):
    if not isinstance(matcher, dict) or not matcher:
        fail(where, '"drawn" is an object of fields to match')
    for field in matcher:
        if field not in MATCHER_FIELDS:
            fail(where, f'"drawn" has no field {field!r}; it takes {", ".join(sorted(MATCHER_FIELDS))}')
    if "kind" in matcher and matcher["kind"] not in DRAWN_KINDS:
        fail(where, f'"kind" is one of {", ".join(DRAWN_KINDS)}')
    for field in ("text", "contains"):
        if field in matcher and not isinstance(matcher[field], str):
            fail(where, f'"{field}" is a string')
    for field in ("font", "color"):
        if field in matcher and (not isinstance(matcher[field], int) or isinstance(matcher[field], bool)):
            fail(where, f'"{field}" is a number')
    for field in ("rect", "inside"):
        if field in matcher and not is_rect(matcher[field]):
            fail(where, f'"{field}" is [x, y, width, height]')


def is_count(value):
    return isinstance(value, int) and not isinstance(value, bool) and value >= 0


def validate_item_matcher(where, matcher):
    if not isinstance(matcher, dict) or not matcher:
        fail(where, '"item" is an object of fields to match')
    for field in matcher:
        if field not in ITEM_FIELDS:
            fail(where, f'"item" has no field {field!r}; it takes {", ".join(sorted(ITEM_FIELDS))}')
    for field in ("code", "name", "contains"):
        if field in matcher and not isinstance(matcher[field], str):
            fail(where, f'"{field}" is a string')
    if "id" in matcher and not is_count(matcher["id"]):
        fail(where, '"id" is a unit id')


def validate_drop(where, step):
    source = step["drop"]
    if not isinstance(source, dict):
        fail(where, '"drop" is an object: {"capture": FILE, "code": CODE} or {"packet": HEX}')
    extra = set(source) - DROP_FIELDS
    if extra:
        fail(where, f'"drop" does not take {", ".join(sorted(extra))}')
    if ("packet" in source) == ("capture" in source):
        fail(where, '"drop" takes a "capture" to pick from or a "packet", one of them')
    if "packet" in source:
        if set(source) != {"packet"}:
            fail(where, '"code", "name" and "nth" pick from a "capture"')
        if not isinstance(source["packet"], str) or not re.fullmatch(r"(?:[0-9a-fA-F]{2})+", source["packet"]):
            fail(where, '"packet" is the item packet as hex')
    else:
        if not isinstance(source["capture"], str) or not source["capture"]:
            fail(where, '"capture" is the path of an item capture')
        for field in ("code", "name"):
            if field in source and not isinstance(source[field], str):
                fail(where, f'"{field}" is a string')
        if "nth" in source and not is_count(source["nth"]):
            fail(where, '"nth" is which of the matching drops, from 0')
    offset = step.get("offset", [0, 0])
    if not (isinstance(offset, list) and len(offset) == 2
            and all(isinstance(v, int) and not isinstance(v, bool) for v in offset)):
        fail(where, '"offset" is [dx, dy] from the character, in map tiles')


def validate_step(index, step):
    where = f"step {index}"
    if not isinstance(step, dict):
        fail(where, "a step is an object")
    kinds = [kind for kind in STEP_KINDS if kind in step]
    if len(kinds) != 1:
        fail(where, f"a step names exactly one of {', '.join(STEP_KINDS)}")
    kind = kinds[0]
    extra = set(step) - STEP_KINDS[kind]
    if extra:
        fail(where, f"a {kind} step does not take {', '.join(sorted(extra))}")
    if "timeout" in step and (not isinstance(step["timeout"], (int, float)) or step["timeout"] <= 0):
        fail(where, '"timeout" is a number of seconds')

    if kind == "do":
        actions = step["do"]
        if not isinstance(actions, list) or not actions or not all(isinstance(a, str) for a in actions):
            fail(where, '"do" is a list of input actions')
        for action in actions:
            try:
                bhctl.validate_action(action)
            except HarnessError as error:
                fail(where, str(error).splitlines()[0])
    elif kind == "wait":
        screens = step["wait"] if isinstance(step["wait"], list) else [step["wait"]]
        if not screens or not all(isinstance(s, str) and s for s in screens):
            fail(where, '"wait" is a screen, or a list of them')
    elif kind == "check":
        if not isinstance(step["check"], str) or not step["check"].strip():
            fail(where, '"check" says what should be true')
        if "state" not in step and "drawn" not in step and "item" not in step:
            fail(where, 'a check has "state", and "drawn" or "item"')
        if "drawn" in step and "item" in step:
            fail(where, 'a check has "drawn" or "item", not both')
        if "state" in step and (not isinstance(step["state"], dict) or not step["state"]):
            fail(where, '"state" is an object of paths to the values expected there')
        if "drawn" in step:
            validate_matcher(where, step["drawn"])
        if "item" in step:
            validate_item_matcher(where, step["item"])
        if "count" in step:
            if "drawn" not in step and "item" not in step:
                fail(where, '"count" counts what "drawn" or "item" matches')
            if not is_count(step["count"]):
                fail(where, '"count" is a whole number')
        if "hold" in step:
            if not isinstance(step["hold"], list) or not all(isinstance(k, str) for k in step["hold"]):
                fail(where, '"hold" is a list of keys')
            for key in step["hold"]:
                try:
                    bhctl.virtual_key(key)
                except HarnessError as error:
                    fail(where, str(error))
    elif kind == "drop":
        validate_drop(where, step)
    elif kind in ("shot", "drawlog"):
        if not isinstance(step[kind], str) or not re.fullmatch(r"[A-Za-z0-9_-]+", step[kind]):
            fail(where, f'"{kind}" is a name of letters, digits, - and _, for the file')
        if "crop" in step and not is_rect(step["crop"]):
            fail(where, '"crop" is [x, y, width, height]')
    elif step[kind] is not True:
        fail(where, f'"{kind}" is true')


def load_scenario(data):
    """The scenario, checked whole before anything runs, so a mistake in step
    nine costs nothing to find."""
    if not isinstance(data, dict):
        fail("top", "a scenario is an object")
    extra = set(data) - TOP_FIELDS
    if extra:
        fail("top", f"unknown fields {', '.join(sorted(extra))}")
    if not isinstance(data.get("title"), str) or not data["title"].strip():
        fail("top", '"title" says what the scenario shows')
    start = data.get("start", "game")
    if start not in STARTS:
        fail("top", f'"start" is one of {", ".join(STARTS)}')
    name = data.get("name", "Agent")
    if not isinstance(name, str) or not name.isalpha() or not 2 <= len(name) <= 15:
        fail("top", '"name" is a character name: 2 to 15 letters')
    character = data.get("character")
    if character is not None:
        if not isinstance(character, str) or not character:
            fail("top", '"character" is the path of a character save (.d2s)')
        if "name" in data:
            fail("top", '"name" names a new character, and a scenario with a "character" plays that one')
    difficulty = data.get("difficulty", "normal")
    if difficulty not in bhctl.DIFFICULTIES:
        fail("top", f'"difficulty" is one of {", ".join(bhctl.DIFFICULTIES)}')
    if difficulty != "normal" and character is None:
        fail("top", 'a new character plays Normal: another "difficulty" needs a "character" that has reached it')
    settings = data.get("settings", {})
    if not isinstance(settings, dict) or not all(
            isinstance(k, str) and isinstance(v, str) and k and "\n" not in k + v for k, v in settings.items()):
        fail("top", '"settings" maps BH_settings.cfg keys to the values to write, both strings')
    steps = data.get("steps")
    if not isinstance(steps, list) or not steps:
        fail("top", '"steps" is a list of steps')
    for index, step in enumerate(steps, 1):
        validate_step(index, step)
    return {"title": data["title"], "start": start, "name": name, "character": character,
            "difficulty": difficulty, "settings": settings, "steps": steps}


# --- checks -------------------------------------------------------------------

COLOR_CODE = re.compile("ÿc.")


def plain_text(text):
    """Drawn text without the game's colour codes, as a player reads it."""
    return COLOR_CODE.sub("", text)


MISSING = object()


def lookup(state, path):
    value = state
    for part in path.split("."):
        if isinstance(value, dict) and part in value:
            value = value[part]
        elif isinstance(value, list) and part.isdigit() and int(part) < len(value):
            value = value[int(part)]
        else:
            return MISSING
    return value


def check_state(expected, state):
    """Whether every path holds its expected value, and what each held."""
    seen = {path: lookup(state, path) for path in expected}
    ok = all(seen[path] is not MISSING and seen[path] == value for path, value in expected.items())
    return ok, {path: (None if value is MISSING else value) for path, value in seen.items()}


def bounds(item):
    if "rect" in item:
        return item["rect"]
    (x1, y1), (x2, y2) = item["from"], item["to"]
    return [min(x1, x2), min(y1, y2), abs(x2 - x1), abs(y2 - y1)]


def inside(rect, area):
    x, y, w, h = rect
    ax, ay, aw, ah = area
    return ax <= x and ay <= y and x + w <= ax + aw and y + h <= ay + ah


def matches(matcher, item):
    if "kind" in matcher and item["kind"] != matcher["kind"]:
        return False
    text = plain_text(item.get("text", ""))
    if "text" in matcher and (item["kind"] != "text" or text != matcher["text"]):
        return False
    if "contains" in matcher and (item["kind"] != "text" or matcher["contains"] not in text):
        return False
    for field in ("font", "color"):
        if field in matcher and item.get(field) != matcher[field]:
            return False
    if "rect" in matcher and bounds(item) != matcher["rect"]:
        return False
    if "inside" in matcher and not inside(bounds(item), matcher["inside"]):
        return False
    return True


def near_misses(matcher, items):
    """Text that reads like what was asked for, for a check that found none:
    usually the right thing, a pixel or a colour away."""
    needle = matcher.get("text", matcher.get("contains"))
    if needle is None:
        return []
    needle = needle.lower()
    return [item for item in items
            if item["kind"] == "text" and needle in plain_text(item["text"]).lower()][:NEAR_MISSES]


def check_drawn(matcher, count, items):
    """Whether what BH drew matches: count items exactly, or at least one when
    count is None. Returns (ok, the matching items)."""
    found = [item for item in items if matches(matcher, item)]
    ok = len(found) == count if count is not None else bool(found)
    return ok, found


def describe_item(item):
    x, y, w, h = bounds(item)
    if item["kind"] == "text":
        return (f'text "{plain_text(item["text"])}" at {x},{y} {w}x{h}, '
                f'font {item.get("font")}, colour {item.get("color")}')
    if item["kind"] == "line":
        (x1, y1), (x2, y2) = item["from"], item["to"]
        return f"line {x1},{y1} to {x2},{y2}, colour {item.get('color')}"
    if item["kind"] == "frame":
        return f"frame around {x},{y} {w}x{h}"
    return f"box at {x},{y} {w}x{h}, colour {item.get('color')}, trans {item.get('trans')}"


def describe_matcher(matcher, count):
    parts = []
    for field in ("kind", "text", "contains", "font", "color", "rect", "inside"):
        if field in matcher:
            value = matcher[field]
            parts.append(f'{field} {json.dumps(value) if isinstance(value, str) else value}')
    if count is None:
        quantity = "at least one item"
    elif count == 0:
        quantity = "no item"
    else:
        quantity = f"exactly {count} item{'s' if count > 1 else ''}"
    return f"{quantity} with {', '.join(parts)}"


def item_matches(matcher, item):
    name = plain_text(item["name"])
    return (matcher.get("code", item["code"]) == item["code"]
            and matcher.get("id", item["id"]) == item["id"]
            and matcher.get("name", name) == name
            and matcher.get("contains", "") in name)


def check_items(matcher, count, items):
    """Whether the items on the ground match, as check_drawn judges what BH
    drew. Returns (ok, the matching items)."""
    found = [item for item in items if item_matches(matcher, item)]
    ok = len(found) == count if count is not None else bool(found)
    return ok, found


def near_item_misses(matcher, items):
    """Items of the code asked for, or named like it, for a check that found
    none: usually the item, named otherwise than expected."""
    needle = matcher.get("name", matcher.get("contains", "")).lower()
    return [item for item in items
            if item["code"] == matcher.get("code")
            or (needle and needle in plain_text(item["name"]).lower())][:NEAR_MISSES]


def describe_ground_item(item):
    x, y = item["position"]
    name = plain_text(item["name"]).replace("\n", " / ")
    return f'{item["code"]} "{name}" at {x},{y}, id {item["id"]}'


def describe_item_matcher(matcher, count):
    parts = [f'{field} {json.dumps(matcher[field]) if isinstance(matcher[field], str) else matcher[field]}'
             for field in ("code", "name", "contains", "id") if field in matcher]
    if count is None:
        quantity = "at least one item on the ground"
    elif count == 0:
        quantity = "no item on the ground"
    else:
        quantity = f"exactly {count} item{'s' if count > 1 else ''} on the ground"
    return f"{quantity} with {', '.join(parts)}"


def mark(bgra, width, height, marks):
    """A copy of the picture with each rectangle outlined in its colour, from
    a list of (rectangle, BGRA)."""
    picture = bytearray(bgra)

    def put(x, y):
        if 0 <= x < width and 0 <= y < height:
            offset = (y * width + x) * 4
            picture[offset:offset + 4] = colour

    for (x, y, w, h), colour in marks:
        left, top, right, bottom = x - 1, y - 1, x + max(w, 1), y + max(h, 1)
        if right < 0 or bottom < 0 or left >= width or top >= height:
            continue
        for px in range(max(left, 0), min(right, width - 1) + 1):
            put(px, top)
            put(px, bottom)
        for py in range(max(top, 0), min(bottom, height - 1) + 1):
            put(left, py)
            put(right, py)
    return bytes(picture)


def slug(text, limit=40):
    words = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return words[:limit].rstrip("-") or "check"


# --- running ------------------------------------------------------------------

def repo_path(text):
    """A path a scenario gives: from the repository's root unless absolute."""
    path = Path(text)
    return path if path.is_absolute() else bhctl.REPO / path


def resolve_drops(scenario):
    """Each drop step's packet, and what it is, by step number: read before the
    game starts, so a capture that is missing or lacks the drop costs nothing."""
    read = {}
    drops = {}
    for index, step in enumerate(scenario["steps"], 1):
        if "drop" not in step:
            continue
        source = step["drop"]
        if "packet" in source:
            drops[index] = (bytes.fromhex(source["packet"]), {"packet": source["packet"]})
            continue
        path = repo_path(source["capture"])
        try:
            if path not in read:
                read[path] = captures.read_drops(path)
            picked = captures.pick(read[path], source.get("code"), source.get("name"), source.get("nth", 0))
        except captures.CaptureError as error:
            fail(f"step {index}", str(error))
        drops[index] = (picked["packet"], {"capture": shown_path(path), "line": picked["line"],
                                           "code": picked["code"], "name": picked["name"]})
    return drops


class Runner:
    def __init__(self, scenario, out, drops):
        self.scenario = scenario
        self.out = out
        self.drops = drops
        self.run_id = None

    def file(self, index, name, suffix):
        return f"{index:02d}-{name}{suffix}"

    def evidence_shot(self, index, name, marks=None, crop=None):
        width, height, pixels, _ = bhctl.grab(self.run_id, crop)
        files = [self.file(index, name, ".png")]
        bhctl.write_png(self.out / files[0], width, height, pixels)
        if marks:
            files.append(self.file(index, name, ".marked.png"))
            bhctl.write_png(self.out / files[1], width, height, mark(pixels, width, height, marks))
        return files

    def save_json(self, name, data):
        (self.out / name).write_text(json.dumps(data, indent=1, ensure_ascii=False), encoding="utf-8")
        return name

    def check(self, index, step):
        timeout = step.get("timeout", CHECK_TIMEOUT_SECONDS)
        name = slug(step["check"])
        record = {"check": step["check"]}

        def attempt():
            deadline = time.time() + timeout
            while True:
                ok = True
                state = bhctl.state(self.run_id) if "state" in step or "item" in step else None
                if "state" in step:
                    state_ok, seen = check_state(step["state"], state)
                    record["expected"] = step["state"]
                    record["seen"] = seen
                    ok = ok and state_ok
                if "item" in step:
                    items = lookup(state, "game.items")
                    items = [] if items is MISSING else items
                    items_ok, found = check_items(step["item"], step.get("count"), items)
                    record["looked_for"] = describe_item_matcher(step["item"], step.get("count"))
                    record["found_items"] = found
                    record["ground_items"] = len(items)
                    ok = ok and items_ok
                if "drawn" in step:
                    frame = bhctl.drawlog(self.run_id)
                    drawn_ok, found = check_drawn(step["drawn"], step.get("count"), frame["items"])
                    record["looked_for"] = describe_matcher(step["drawn"], step.get("count"))
                    record["found"] = found
                    record["drawn_items"] = len(frame["items"])
                    ok = ok and drawn_ok
                if ok or time.time() >= deadline:
                    break
                time.sleep(CHECK_RETRY_SECONDS)
            if "item" in step and not ok and not record["found_items"]:
                record["near_items"] = near_item_misses(step["item"], items)
            if "drawn" in step:
                if not ok and not record["found"]:
                    record["near_misses"] = near_misses(step["drawn"], frame["items"])
                record["drawlog"] = self.save_json(self.file(index, name, ".drawlog.json"), frame)
            marks = ([(bounds(item), MATCH_BGRA) for item in record.get("found", [])]
                     + [(bounds(item), NEAR_BGRA) for item in record.get("near_misses", [])])
            record["evidence"] = self.evidence_shot(index, name, marks)
            return ok

        hold = step.get("hold", [])
        ok = bhctl.send_input(self.run_id, [f"down:{key}" for key in hold], then=attempt)
        record["outcome"] = "passed" if ok else "failed"
        return record

    def step(self, index, step):
        kind = next(kind for kind in STEP_KINDS if kind in step)
        if kind == "check":
            return self.check(index, step)
        record = {"outcome": "done"}
        if kind == "do":
            bhctl.send_input(self.run_id, step["do"])
        elif kind == "wait":
            screens = step["wait"] if isinstance(step["wait"], list) else [step["wait"]]
            record["screen"] = bhctl.wait_screen(self.run_id, screens, step.get("timeout", 60))["screen"]
        elif kind == "shot":
            record["evidence"] = self.evidence_shot(index, step["shot"], crop=step.get("crop"))
        elif kind == "drawlog":
            record["evidence"] = [self.save_json(self.file(index, step["drawlog"], ".json"),
                                                 bhctl.drawlog(self.run_id))]
        elif kind == "drop":
            packet, record["dropped"] = self.drops[index]
            landing = bhctl.drop(self.run_id, packet, step.get("offset", [0, 0]))
            record.update({key: landing[key] for key in ("id", "position", "landed")})
        elif kind == "leave":
            record["screen"] = bhctl.leave(self.run_id, 60)
        elif kind == "enter":
            record["screen"] = self.enter()["screen"]
        return record

    def enter(self):
        return bhctl.enter(self.run_id, self.scenario["name"], 60, self.scenario["difficulty"])

    def run(self, bh_dll, keep):
        """Starts the game, runs every step, and tears the run down unless
        keep. Returns what happened: a record per step run, and the error that
        stopped it, if one did, with a screenshot of the game at that point."""
        records = []
        report = {"records": records}
        current = 0
        try:
            character = self.scenario["character"] and repo_path(self.scenario["character"])
            self.run_id = bhctl.create(bh_dll, self.scenario["settings"], character)
            report["run"] = self.run_id
            bhctl.launch(self.run_id, ["-w", "-ns"], bhctl.LIFETIME_SECONDS)
            if self.scenario["start"] == "game":
                self.enter()
            for current, step in enumerate(self.scenario["steps"], 1):
                records.append({"index": current, **self.step(current, step)})
        # OSError is a Windows call failing, as posting to a window that has
        # gone does: the run ends as a harness error would.
        except (HarnessError, OSError) as error:
            report["error"] = str(error)
            report["error_step"] = current
            if current:
                records.append({"index": current, "outcome": "error", "error": str(error)})
            if self.run_id:
                try:
                    report["error_evidence"] = self.evidence_shot(current, "at-error")
                except (HarnessError, OSError):
                    pass
        finally:
            if self.run_id and not keep:
                bhctl.teardown(self.run_id)
        return report


# --- reporting ----------------------------------------------------------------

def git(*args):
    try:
        return subprocess.run(["git", *args], cwd=bhctl.REPO, capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def shown_path(path):
    """A path as a report shows it: from the repository's root when inside it."""
    try:
        return str(path.resolve().relative_to(bhctl.REPO))
    except ValueError:
        return str(path.resolve())


def build_facts(bh_dll):
    commit = git("rev-parse", "--short", "HEAD")
    return {
        "bh": shown_path(bh_dll),
        "sha256": hashlib.sha256(bh_dll.read_bytes()).hexdigest()[:12],
        "built": time.strftime("%Y-%m-%d %H:%M", time.localtime(bh_dll.stat().st_mtime)),
        "commit": commit,
        "dirty": bool(git("status", "--porcelain", "--untracked-files=no")) if commit else None,
    }


def result_of(report):
    if "error" in report:
        return "error"
    if any(r["outcome"] == "failed" for r in report["records"]):
        return "failed"
    return "passed"


def describe_step(step):
    kind = next(kind for kind in STEP_KINDS if kind in step)
    if kind == "do":
        return "do " + " ".join(md_code(a) for a in step["do"])
    if kind == "wait":
        screens = step["wait"] if isinstance(step["wait"], list) else [step["wait"]]
        return "wait for " + " or ".join(md_code(s) for s in screens)
    if kind == "check":
        return md_text(step["check"])
    if kind == "drop":
        source = step["drop"]
        if "packet" in source:
            what = "a packet"
        else:
            picked = " ".join(f"{field} {md_code(source[field])}" for field in ("code", "name", "nth") if field in source)
            what = f"from {md_code(source['capture'])}" + (f", {picked}" if picked else "")
        dx, dy = step.get("offset", [0, 0])
        return f"drop {what}, {dx},{dy} from the character"
    if kind in ("shot", "drawlog"):
        return f"{kind} {md_code(step[kind])}"
    return kind


def md_text(text):
    """Text from the game or the scenario, safe inside a Markdown line."""
    return re.sub(r"([\\`*_\[\]<>|])", r"\\\1", text)


def md_code(text):
    """Text shown verbatim, in a code span its own backticks cannot end. A |
    would still end a table cell, so it alone is escaped, as GitHub's tables
    read it even inside code."""
    text = str(text).replace("|", "\\|")
    fence = "``" if "`" in text else "`"
    pad = " " if text.startswith("`") or text.endswith("`") else ""
    return f"{fence}{pad}{text}{pad}{fence}"


def md_evidence(files):
    """Pictures shown, other files linked. A screenshot with its matches marked
    is shown in place of the plain one, which is linked for what the marks
    cover."""
    marked = {name.replace(".marked.png", ".png") for name in files if name.endswith(".marked.png")}
    lines = []
    for name in sorted(files, key=lambda n: not n.endswith(".marked.png")):
        if name in marked:
            lines += ["", f"Unmarked: [{name}]({name})"]
        elif name.endswith(".png"):
            lines += ["", f"![{name}]({name})"]
        else:
            lines += ["", f"[{name}]({name})"]
    return lines


def md_drop(record):
    dropped = record["dropped"]
    if "capture" in dropped:
        name = plain_text(dropped["name"]).replace("\n", " / ")
        what = (f"{md_code(dropped['code'])} \"{md_text(name)}\", recorded at line {dropped['line']}"
                f" of {md_code(dropped['capture'])},")
    else:
        what = "The packet"
    x, y = record["position"]
    if record["landed"]:
        return ["", f"{what} landed at {x},{y} as item {record['id']}."]
    return ["", f"{what} did not land at {x},{y}: BH's item filter stopped it, which is how it hides an item."]


def md_check(record):
    lines = []
    if "found_items" in record:
        found = len(record["found_items"])
        lines += ["", f"Looked for {md_text(record['looked_for'])}, among the {record['ground_items']} there."
                  f" Found {found}" + (":" if found else ".")]
        lines += [f"- {md_text(describe_ground_item(item))}" for item in record["found_items"]]
        if record.get("near_items"):
            lines += ["", "What was most like it:"]
            lines += [f"- {md_text(describe_ground_item(item))}" for item in record["near_items"]]
    if "expected" in record:
        lines.append("")
        for path, value in record["expected"].items():
            seen = record["seen"][path]
            verdict = ("as expected" if seen == value
                       else f"expected {md_code(json.dumps(value, ensure_ascii=False))}")
            lines.append(f"- {md_code(path)} is {md_code(json.dumps(seen, ensure_ascii=False))}, {verdict}")
    if "found" in record:
        found = len(record["found"])
        lines += ["", f"Looked for {md_text(record['looked_for'])}, among the {record['drawn_items']} items"
                  f" BH drew ([draw log]({record['drawlog']})). Found {found}"
                  + (", outlined in magenta:" if found else ".")]
        lines += [f"- {md_text(describe_item(item))}" for item in record["found"]]
        if record.get("near_misses"):
            lines += ["", "What read most like it, outlined in yellow:"]
            lines += [f"- {md_text(describe_item(item))}" for item in record["near_misses"]]
    return lines


def render_markdown(report):
    steps = report["steps"]
    records = {r["index"]: r for r in report["records"]}
    checks = [i for i, step in enumerate(steps, 1) if "check" in step]
    held = sum(records.get(i, {}).get("outcome") == "passed" for i in checks)

    lines = [f"# {md_text(report['title'])}", ""]
    if report["result"] == "error":
        lines.append(f"**Could not finish:** {md_text(report['error'])}")
    elif report["result"] == "failed":
        lines.append(f"**Failed:** {len(checks) - held} of {len(checks)} checks do not hold.")
    else:
        lines.append(f"**Passed:** all {len(checks)} checks hold.")

    build = report["build"]
    lines += ["", "| | |", "|---|---|", f"| Scenario | {md_code(report['scenario'])} |",
              f"| Build | {md_code(build['bh'])}, sha256 {md_code(build['sha256'])}, built {build['built']} |"]
    if build["commit"]:
        changes = ", with uncommitted changes" if build["dirty"] else ""
        lines.append(f"| Checkout | {md_code(build['commit'])}{changes} |")
    if report.get("character"):
        lines.append(f"| Character | {md_code(report['character'])}, in {report['difficulty'].title()} |")
    if report["settings"]:
        listed = "<br>".join(md_code(f"{k}: {v}") for k, v in report["settings"].items())
        lines.append(f"| Settings | {listed} |")
    lines.append(f"| Run | {md_code(report.get('run', 'none'))}, started {report['started']},"
                 f" took {report['seconds']} s |")
    for note in report["notes"]:
        lines.append(f"| Note | {md_text(note)} |")

    if checks:
        lines += ["", "## Checks", "", "| Step | Check | Result |", "|---|---|---|"]
        for index in checks:
            outcome = records[index]["outcome"] if index in records else "not run"
            lines.append(f"| {index} | {md_text(steps[index - 1]['check'])} | {outcome} |")

    lines += ["", "## Steps"]
    if report.get("error") and not report.get("error_step"):
        lines += ["", f"Starting the game failed: {md_text(report['error'])}"]
        lines += md_evidence(report.get("error_evidence", []))
    for index, step in enumerate(steps, 1):
        record = records.get(index)
        lines += ["", f"### {index}. {describe_step(step)} ({record['outcome'] if record else 'not run'})"]
        if not record:
            continue
        if "error" in record:
            lines += ["", md_text(record["error"])]
        if "screen" in record:
            lines += ["", f"At {md_code(record['screen'])}."]
        if "dropped" in record:
            lines += md_drop(record)
        lines += md_check(record)
        lines += md_evidence(record.get("evidence", []))
        if index == report.get("error_step"):
            lines += md_evidence(report.get("error_evidence", []))
    return "\n".join(lines) + "\n"


def run_scenario(path, out, bh_dll, keep):
    """Runs the scenario at path and writes report.md and report.json into out.
    Returns the report."""
    try:
        # PowerShell 5.1 writes UTF-8 with a byte order mark.
        data = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError as error:
        raise HarnessError(f"scenario: {path} is not JSON: {error}") from None
    scenario = load_scenario(data)
    if not bh_dll.exists():
        raise HarnessError(f"no BH.dll at {bh_dll}")
    if scenario["character"]:
        bhctl.character_name(repo_path(scenario["character"]))
    drops = resolve_drops(scenario)
    out = Path(out or REPORTS / f"{time.strftime('%Y%m%d-%H%M%S')}-{Path(path).stem}")
    out.mkdir(parents=True, exist_ok=True)

    shipped = bhctl.shipped_settings_keys()
    started = time.time()
    report = {
        "title": scenario["title"],
        "scenario": shown_path(Path(path)),
        "build": build_facts(bh_dll),
        "character": scenario["character"],
        "difficulty": scenario["difficulty"],
        "settings": scenario["settings"],
        "notes": [f'"{key}" is not in the shipped BH_settings.cfg: it was added, and does nothing unless BH reads it'
                  for key in scenario["settings"] if key not in shipped],
        "started": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(started)),
        "steps": scenario["steps"],
    }
    report.update(Runner(scenario, out, drops).run(bh_dll, keep))
    report["seconds"] = round(time.time() - started)
    report["result"] = result_of(report)
    (out / "report.json").write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding="utf-8")
    (out / "report.md").write_text(render_markdown(report), encoding="utf-8")
    report["out"] = str(out.resolve())
    return report
