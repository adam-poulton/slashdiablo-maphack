"""Writes the table fixtures the stat description tests read.

The tests stand the game's data tables up from files rather than from the MPQ
archives, so this copies the tables out of a directory of extracted `.txt` files
and merges the three `.tbl` string tables into one `key<TAB>text` file.

    python make_fixtures.py <extracted tables dir> <repo root>

Reference tables go in whole: the wording code searches them, and a trimmed one
would let a test pass against a table the game does not have. Subject tables,
which hold the items a test is about, are trimmed to those items with their
header row kept.

The string table is trimmed to the keys the reference tables can reach, which is
every key ItemStatCost.txt names for a stat line or a grouped line, the skill
names SkillDesc.txt points at, the class and skill tab labels, and the handful of
keys the wording code names itself. Keys whose text carries a tab or a newline
are dropped, since the fixture is one key to a line.
"""
import csv
import os
import sys

import tbl

# Read whole. These are what a property is looked up in.
REFERENCE_TABLES = [
    'ItemStatCost.txt',
    'Properties.txt',
    'CharStats.txt',
    'Skills.txt',
    'SkillDesc.txt',
]

# Read trimmed to the rows named here, which are the items under test.
SUBJECT_TABLES = {
    'UniqueItems.txt': ('index', [
        'Harlequin Crest',
        'Skin of the Vipermagi',
        "Mara's Kaleidoscope",
        'Guardian Angel',
    ]),
}

# Keys the wording code names in its own source rather than reading from a table.
LITERAL_KEYS = ['ModStre10b', 'strModEnhancedDamage'] + \
    ['StrSklTabItem%d' % i for i in range(1, 22)]


def read_rows(path):
    with open(path, encoding='latin-1', newline='') as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def write_rows(path, header, rows):
    with open(path, 'w', encoding='latin-1', newline='\n') as handle:
        handle.write('\t'.join(header) + '\n')
        for row in rows:
            handle.write('\t'.join((row[name] or '') for name in header) + '\n')


def wanted_keys(tables):
    keys = set(LITERAL_KEYS)
    for row in tables['ItemStatCost.txt']:
        for column in ('descstrpos', 'descstrneg', 'descstr2',
                       'dgrpstrpos', 'dgrpstrneg', 'dgrpstr2'):
            keys.add(row[column])
    for row in tables['SkillDesc.txt']:
        keys.add(row['str name'])
    for row in tables['CharStats.txt']:
        for column in ('StrAllSkills', 'StrSkillTab1', 'StrSkillTab2',
                       'StrSkillTab3', 'StrClassOnly'):
            keys.add(row[column])
    keys.discard(None)
    keys.discard('')
    return keys


def main(source, root):
    out = os.path.join(root, 'BHTests', 'fixtures', 'tables')
    os.makedirs(out, exist_ok=True)

    tables = {}
    for name in REFERENCE_TABLES:
        rows = read_rows(os.path.join(source, name))
        tables[name] = rows
        write_rows(os.path.join(out, name), list(rows[0].keys()), rows)
        print('%s: %d rows' % (name, len(rows)))

    for name, (column, wanted) in SUBJECT_TABLES.items():
        rows = read_rows(os.path.join(source, name))
        kept = [row for row in rows if row[column] in wanted]
        missing = set(wanted) - set(row[column] for row in kept)
        if missing:
            raise SystemExit('not in %s: %s' % (name, ', '.join(sorted(missing))))
        write_rows(os.path.join(out, name), list(rows[0].keys()), kept)
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
