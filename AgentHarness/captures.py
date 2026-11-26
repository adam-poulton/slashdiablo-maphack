"""Reads the item drops an item capture recorded (BH's "Capture Item Drops"
setting, item-captures.txt), so they can be dropped again in a sandboxed game.

A capture is the line format BH/Modules/Item/CaptureFormat.h describes: one
record per line, its type and then tab separated name=value fields, escaped.
"""

from pathlib import Path


class CaptureError(Exception):
    """A capture cannot be read, or holds no drop like the one asked for."""


def unescape(value):
    """The bytes a field's value stands for, as CaptureFormat::Unescape reads
    them: text that is not its escaping is kept as it stands."""
    out = bytearray()
    i = 0
    while i < len(value):
        c = value[i]
        if c != "\\":
            out += c.encode("latin-1")
            i += 1
            continue
        if i + 1 >= len(value):
            out += b"\\"
            break
        marker = value[i + 1]
        if marker in "\\tnr":
            out += {"\\": b"\\", "t": b"\t", "n": b"\n", "r": b"\r"}[marker]
            i += 2
        elif marker == "x" and i + 3 < len(value) and is_hex(value[i + 2:i + 4]):
            out.append(int(value[i + 2:i + 4], 16))
            i += 4
        else:
            out += b"\\"
            i += 1
    return bytes(out)


def is_hex(text):
    return len(text) == 2 and all(c in "0123456789abcdefABCDEF" for c in text)


def read_drops(path):
    """Every drop record in a capture, in order, as a dict of its fields with
    the item's code, its name as text and its packet as bytes."""
    try:
        # Latin-1 maps every byte to one character, and escaping leaves only
        # ASCII in a capture, so nothing is lost before unescaping.
        lines = Path(path).read_text(encoding="latin-1").splitlines()
    except OSError as error:
        raise CaptureError(f"cannot read the capture {path}: {error.strerror}") from None
    drops = []
    for number, line in enumerate(lines, 1):
        fields = line.split("\t")
        if fields[0] != "drop":
            continue
        values = dict(field.partition("=")[::2] for field in fields[1:])
        if "packet" not in values:
            raise CaptureError(f"{path}:{number}: a drop with no packet")
        drops.append({
            **values,
            "line": number,
            "code": values.get("code", ""),
            "name": unescape(values.get("name", "")).decode("latin-1"),
            "packet": unescape(values["packet"]),
        })
    return drops


def pick(drops, code=None, name=None, nth=0):
    """The nth drop (from 0) of those with the code and with name in theirs,
    ignoring case: either may be left out."""
    matching = [drop for drop in drops
                if (code is None or drop["code"] == code)
                and (name is None or name.lower() in drop["name"].lower())]
    if nth >= len(matching):
        wanted = " and ".join(part for part in (code and f"code {code!r}", name and f"a name with {name!r}") if part)
        raise CaptureError(f"the capture has {len(matching)} drop{'s' if len(matching) != 1 else ''}"
                           f" with {wanted or 'anything'}, so no number {nth}")
    return matching[nth]
