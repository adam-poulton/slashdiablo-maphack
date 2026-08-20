"""Reads a Diablo II .tbl string table into a {key: value} mapping.

The .tbl files hold the text the game shows, keyed by the names the data tables
refer to ("ModStr3k" -> "to All Skills", "Runeword130" -> "Spirit"). Extract them
with dumpmpq first; see README.md.

The same format is parsed at run time by BH/StatDescriptions.cpp, so this is
useful for answering questions about the data rather than for testing that code.
"""
import struct
import sys

# Offsets within the file. Also documented in BH/TableReader.cpp.
HEADER_SIZE = 0x15
ELEMENT_SIZE = 0x02
NODE_SIZE = 0x11
NUM_ELEMENTS_OFFSET = 0x02
ACTIVE_OFFSET = 0x00
KEY_STRING_OFFSET = 0x07
VALUE_STRING_OFFSET = 0x0B


def parse(path):
    """Parses one .tbl file. Returns {key: value}."""
    data = open(path, 'rb').read()
    count = struct.unpack_from('<H', data, NUM_ELEMENTS_OFFSET)[0]
    first_node = HEADER_SIZE + ELEMENT_SIZE * count

    def string_at(offset):
        if offset < 0 or offset >= len(data):
            return None
        end = data.index(b'\0', offset)
        return data[offset:end].decode('latin-1')

    out = {}
    for i in range(count):
        node = struct.unpack_from('<H', data, HEADER_SIZE + ELEMENT_SIZE * i)[0]
        pos = first_node + NODE_SIZE * node
        if pos + NODE_SIZE > len(data) or not data[pos + ACTIVE_OFFSET]:
            continue
        key = string_at(struct.unpack_from('<i', data, pos + KEY_STRING_OFFSET)[0])
        value = string_at(struct.unpack_from('<i', data, pos + VALUE_STRING_OFFSET)[0])
        if key is not None and value is not None:
            out[key] = value
    return out


def load_all(directory='.'):
    """Merges the three tables the game uses, in the order it applies them."""
    strings = {}
    for name in ('string.tbl', 'expansionstring.tbl', 'patchstring.tbl'):
        try:
            strings.update(parse(f'{directory}/{name}'))
        except FileNotFoundError:
            pass
    return strings


if __name__ == '__main__':
    strings = load_all(sys.argv[1] if len(sys.argv) > 1 else '.')
    keys = sys.argv[2:]
    if not keys:
        print(f'{len(strings)} strings')
    for key in keys:
        print(f'{key} -> {strings.get(key)!r}')
