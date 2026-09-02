"""Writes the table fixtures the item tests read.

The tests stand the game's data tables up from files rather than from the MPQ
archives, so this copies the tables out of a directory of extracted `.txt` files
and merges the three `.tbl` string tables into one `key<TAB>text` file.

    python make_fixtures.py <extracted tables dir> <repo root>

Reference tables go in whole: the wording code searches them, and a trimmed one
would let a test pass against a table the game does not have. Subject tables,
which hold the items a test is about, are trimmed to those items with their
header row kept.

The string table is trimmed to the keys the fixtures can reach, which is every
key ItemStatCost.txt names for a stat line or a grouped line, the skill names
SkillDesc.txt points at, the class and skill tab labels, the name each base item
kept is called by, the name each set and each piece of one is called by, and
the handful of keys the wording code names itself. Keys
whose text carries a tab or a newline are dropped, since the fixture is one key
to a line.
"""
import csv
import os
import sys

import tbl

# Read whole. These are what a property or an item type is looked up in.
REFERENCE_TABLES = [
    'ItemStatCost.txt',
    'Properties.txt',
    'CharStats.txt',
    'Skills.txt',
    'SkillDesc.txt',
    'ItemTypes.txt',
]

# Read trimmed to the rows named here, which are the items under test. The base
# items between them cover what a base can carry: each of the three tiers, one
# and two handed damage, throw damage, defense, a durability the game hides, and
# a name only the string table gets right.
SUBJECT_TABLES = {
    'UniqueItems.txt': ('index', [
        'Harlequin Crest',
        'Skin of the Vipermagi',
        "Mara's Kaleidoscope",
        'Guardian Angel',
    ]),
    'Weapons.txt': ('code', [
        'gsc',      # Grand Scepter, under Civerb's Cudgel
        'oba',      # Swirling Crystal, under Tal Rasha's Lidless Eye
        'crs',      # Crystal Sword, one handed
        '7cr',      # Phase Blade, elite, and wears out never
        '7gd',      # Colossus Blade, swung in either hand
        '7bk',      # Winged Knife, thrown and stacked
        '7vo',      # Colossus Voulge, two handed only
        '6lw',      # Hydra Bow, a durability the game never shows
    ]),
    'Armor.txt': ('code', [
        'cap',      # Cap, normal
        'xea',      # Serpentskin Armor, exceptional
        'uap',      # Shako, elite
        'xlt',      # Templar Coat
        'lrg',      # Large Shield, under Civerb's Ward
        'zmb',      # Mesh Belt, under Tal Rasha's Fire-Spun Cloth
        'uth',      # Lacquered Plate, under Tal Rasha's Howling Wind
        'xsk',      # Death Mask, under Tal Rasha's Horadric Crest
    ]),
    'Misc.txt': ('code', [
        'amu',      # Amulet, which carries no numbers at all
        'cm1',      # Small Charm, whose name only the string table gets right
        'r33',      # Zod Rune
    ]),
    'Sets.txt': ('index', [
        "Civerb's Vestments",
        "Tal Rasha's Wrappings",
    ]),
    'SetItems.txt': ('index', [
        # Civerb's three, which between them carry each of the three ways a
        # piece's own partial bonuses are unlocked: all at once, one at a time,
        # and the blank that grants them never.
        "Civerb's Ward",
        "Civerb's Icon",
        "Civerb's Cudgel",
        # Tal Rasha's five, one of which the file still calls by the working
        # title the string table corrects.
        "Tal Rasha's Fire-Spun Cloth",
        "Tal Rasha's Adjudication",
        "Tal Rasha's Lidless Eye",
        "Tal Rasha's Howling Wind",
        "Tal Rasha's Horadric Crest",
    ]),
}

# Columns a subject table keys a name into the string table by, beyond the base
# items' own. A set and a piece of one are both named by their index.
INDEX_KEY_TABLES = ['Sets.txt', 'SetItems.txt']

# The column a base item keys its name into the string table by.
NAME_COLUMN = 'namestr'

# Keys the wording code names in its own source rather than reading from a table.
LITERAL_KEYS = ['ModStre8c', 'ModStre10b', 'strModEnhancedDamage',
                'ItemStast1k', 'ItemStats1d', 'ItemStats1e', 'ItemStats1f',
                'ItemStats1h', 'ItemStats1l', 'ItemStats1m', 'ItemStats1n',
                'ItemStats1p', 'StrSkill106'] + \
    ['StrSklTabItem%d' % i for i in range(1, 22)]


def read_table(path):
    """The header and the rows as they stand. Read as rows rather than into
    dictionaries because Armor.txt names two of its columns twice, and a
    dictionary would lose one of each pair."""
    with open(path, encoding='latin-1', newline='') as handle:
        rows = list(csv.reader(handle, delimiter='\t'))
    header = rows[0]
    body = [(row + [''] * len(header))[:len(header)] for row in rows[1:]]
    return header, body


def cell(header, row, name):
    return row[header.index(name)] if name in header else ''


def write_table(path, header, rows):
    with open(path, 'w', encoding='latin-1', newline='\n') as handle:
        handle.write('\t'.join(header) + '\n')
        for row in rows:
            handle.write('\t'.join(row) + '\n')


def keys_in(tables, name, columns):
    header, rows = tables[name]
    return set(cell(header, row, column) for row in rows for column in columns)


def wanted_keys(tables):
    keys = set(LITERAL_KEYS)
    keys |= keys_in(tables, 'ItemStatCost.txt',
                    ('descstrpos', 'descstrneg', 'descstr2',
                     'dgrpstrpos', 'dgrpstrneg', 'dgrpstr2'))
    keys |= keys_in(tables, 'SkillDesc.txt', ('str name',))
    keys |= keys_in(tables, 'CharStats.txt',
                    ('StrAllSkills', 'StrSkillTab1', 'StrSkillTab2',
                     'StrSkillTab3', 'StrClassOnly'))
    for name in SUBJECT_TABLES:
        keys |= keys_in(tables, name, (NAME_COLUMN,))
    for name in INDEX_KEY_TABLES:
        keys |= keys_in(tables, name, ('index',))
    keys.discard(None)
    keys.discard('')
    return keys


def main(source, root):
    out = os.path.join(root, 'BHTests', 'fixtures', 'tables')
    os.makedirs(out, exist_ok=True)

    tables = {}
    for name in REFERENCE_TABLES:
        header, rows = read_table(os.path.join(source, name))
        tables[name] = (header, rows)
        write_table(os.path.join(out, name), header, rows)
        print('%s: %d rows' % (name, len(rows)))

    for name, (key, wanted) in SUBJECT_TABLES.items():
        header, rows = read_table(os.path.join(source, name))
        kept = [row for row in rows if cell(header, row, key) in wanted]
        missing = set(wanted) - set(cell(header, row, key) for row in kept)
        if missing:
            raise SystemExit('not in %s: %s' % (name, ', '.join(sorted(missing))))
        tables[name] = (header, kept)
        write_table(os.path.join(out, name), header, kept)
        print('%s: %d of %d rows' % (name, len(kept), len(rows)))

    strings = tbl.load_all(source)
    keys = wanted_keys(tables)
    path = os.path.join(root, 'BHTests', 'fixtures', 'strings.txt')
    written = 0
    with open(path, 'w', encoding='latin-1', newline='\n') as handle:
        for key in sorted(keys):
            text = strings.get(key)
            if text is None or '\t' in text or '\n' in text or '\r' in text:
                continue
            handle.write('%s\t%s\n' % (key, text))
            written += 1
    print('strings.txt: %d of %d keys' % (written, len(strings)))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
