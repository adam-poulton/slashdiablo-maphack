"""Turns item captures into the fixtures the tests replay.

A capture taken while playing is mostly repetition: the same potion landing
four hundred times says nothing the first one did not. This keeps what carries
information and drops the rest, so the fixtures stay small enough to live in the
repository while still standing for everything the capture saw.

Two sets come out of it, because they answer different questions.

  filter-cases.txt  Whole sessions, rules included, for replaying the filter.
                    A session's rules are what its recorded decisions mean, so
                    they are kept complete: leaving a rule out would renumber
                    the others and change every decision that refers to one.

                    Sessions played against the same rules share one copy of
                    them, named by a ruleSet on the header and on each rule. A
                    capture writes neither, having only ever one set in hand, so
                    a reader treats their absence as everything belonging
                    together.

  parse-cases.txt   Packets and what they parsed into, with no rules and no
                    session, for testing the packet reader on its own. Captures
                    too old to replay still serve here, since reading a packet
                    does not depend on the world it was read in.

  tables.txt        The rows of the game's tables that reading those packets
                    depends on: how wide each stat is written, and the
                    attributes of the item codes the cases mention. Without
                    these a recorded packet cannot be read back at all.

Usage:
    python tools/captures/curate.py <capture.txt> [more captures...]
"""

import collections
import os
import re
import sys

# A capture records the character's level and class but not which character it
# was, so drops from a second character sharing a session can only be told apart
# by those. This one was played briefly under a filter level its owner could not
# vouch for, and uncertain data does not belong in a baseline.
EXCLUDED_CHAR_LEVELS = {'84'}

# Enough of a repeated item to prove it repeats, without keeping all of it.
MAX_PER_OUTCOME = 2
MAX_PER_AREA = 4
JUNK_SAMPLE = 80          # keep one in this many otherwise uninteresting drops

# Packets kept for the parse cases beyond those that are interesting in
# themselves, so the ordinary shapes are represented without keeping thousands.
PARSE_SAMPLE = 6

RUNE = re.compile(r'^r\d\d$')
GEM_PREFIX = ('gc', 'gf', 'gs', 'gl', 'gp', 'gz', 'sk', 'am', 'to', 'em',
              'di', 'ru', 'sa')

OUTCOME_FIELDS = ('blocked', 'showOnMap', 'noTracking', 'keepIndex',
                  'ignoreIndex', 'color', 'pingLevel')
# What a packet read out as. Recorded alongside the packet so that reading it
# again can be checked field by field rather than only for not crashing.
PARSE_FIELDS = ('code', 'name', 'action', 'packetSize', 'quality', 'level',
                'sockets', 'usedSockets', 'defense', 'durability',
                'maxDurability', 'amount', 'prefix', 'suffix', 'setCode',
                'uniqueCode', 'runewordId', 'properties', 'identified',
                'ethereal', 'runeword', 'personalized', 'isGold', 'ear',
                'simpleItem', 'hasSockets', 'packet')


def parse_fields(parts):
    fields = {}
    for part in parts[1:]:
        if '=' in part:
            key, value = part.split('=', 1)
            fields[key] = value
    return fields


def read_sessions(paths):
    """Every session across every capture, in the order they were recorded."""
    sessions = []
    current = None
    for path in paths:
        for line in open(path, encoding='latin-1'):
            parts = line.rstrip('\n').split('\t')
            if not parts or not parts[0]:
                continue
            if parts[0] == 'header':
                current = {'header': line.rstrip('\n'),
                           'fields': parse_fields(parts),
                           'rules': [], 'drops': [], 'tables': []}
                sessions.append(current)
            elif current is None:
                continue
            elif parts[0] == 'rule':
                current['rules'].append(line.rstrip('\n'))
            elif parts[0] == 'drop':
                current['drops'].append((line.rstrip('\n'), parse_fields(parts)))
            elif parts[0] in ('itemattrs', 'statwidths'):
                current['tables'].append((parts[0], parse_fields(parts)))
    return sessions


def is_uncommon(code):
    """Item kinds a session sees few of, so all of them are worth keeping."""
    if RUNE.match(code):
        return True
    if len(code) == 3 and code[:2] in GEM_PREFIX and code[2].isdigit():
        return True
    return code.startswith(('cm', 'jew'))


def replayable(session):
    """A session recorded with the world state a replay needs."""
    return session['drops'] and 'areaId' in session['drops'][0][1]


def curate_drops(drops):
    """The drops worth keeping, in the order they were recorded.

    A drop earns its place by being the first of something: a packet, an item,
    an outcome, an area. What earns nothing is sampled rather than dropped
    outright, so the ordinary case is still represented.
    """
    seen_packets = set()
    seen_codes = set()
    outcomes = collections.Counter()
    areas = collections.Counter()
    kept = []
    passed_over = 0

    for line, f in drops:
        if f.get('charLevel') in EXCLUDED_CHAR_LEVELS:
            continue

        packet = f.get('packet', '')
        if packet in seen_packets:
            continue
        seen_packets.add(packet)

        code = f.get('code', '')
        outcome = tuple(f.get(k) for k in OUTCOME_FIELDS)
        area = (f.get('areaId'), f.get('areaLevel'))

        reasons = []
        if code not in seen_codes:
            reasons.append('code')
        if is_uncommon(code):
            reasons.append('uncommon')
        if int(f.get('packetSize', 0)) >= 30:
            reasons.append('large')       # carries affixes and a stat list
        # Being shown on the map is not a reason of its own: it is part of an
        # outcome, and keeping every one of them collected thousands of drops
        # that said the same thing.
        if outcomes[outcome] < MAX_PER_OUTCOME:
            reasons.append('outcome')
        if areas[area] < MAX_PER_AREA:
            reasons.append('area')

        if not reasons:
            passed_over += 1
            if passed_over % JUNK_SAMPLE:
                continue

        seen_codes.add(code)
        outcomes[outcome] += 1
        areas[area] += 1
        kept.append((line, f))

    return kept


def curate_parse_cases(sessions):
    """One line per distinct packet, with only what reading it produces.

    Every item code and every packet long enough to carry a stat list is kept,
    since those are the shapes the reader has most to get wrong. The rest is
    sampled: a thousandth potion exercises nothing the first did not.
    """
    seen_packets = set()
    seen_codes = set()
    ordinary = 0
    out = []
    for session in sessions:
        for line, f in session['drops']:
            packet = f.get('packet', '')
            if not packet or packet in seen_packets:
                continue
            seen_packets.add(packet)

            code = f.get('code', '')
            interesting = (code not in seen_codes or is_uncommon(code) or
                           int(f.get('packetSize', 0)) >= 30)
            if not interesting:
                ordinary += 1
                if ordinary % PARSE_SAMPLE:
                    continue
            seen_codes.add(code)

            record = ['parse']
            for key in PARSE_FIELDS:
                if key in f:
                    record.append('%s=%s' % (key, f[key]))
            out.append('\t'.join(record))
    return out


def curate_tables(sessions, needed_codes):
    """The table rows the kept cases refer to, each written once.

    Stat widths are kept whole. Which of them a packet needs is only known by
    reading it against them, and there are few enough that guessing is not worth
    the risk of leaving out the one that mattered. Item attributes are kept only
    for the codes the cases mention, which is the larger table and the one where
    most rows stand for items nobody dropped.
    """
    widths = {}
    attributes = {}
    for session in sessions:
        for kind, f in session['tables']:
            if kind == 'statwidths':
                widths.setdefault(f.get('at'), f)
            elif f.get('code') in needed_codes:
                attributes.setdefault(f.get('code'), f)

    lines = []
    for at in sorted(widths, key=lambda value: int(value)):
        f = widths[at]
        lines.append('\t'.join(['statwidths'] +
            ['%s=%s' % (k, f[k]) for k in
             ('at', 'name', 'saveBits', 'saveParamBits', 'saveAdd', 'op',
              'sendParamBits') if k in f]))
    for code in sorted(attributes):
        f = attributes[code]
        lines.append('\t'.join(['itemattrs'] +
            ['%s=%s' % (k, f[k]) for k in
             ('code', 'name', 'category', 'width', 'height', 'stackable',
              'useable', 'throwable', 'itemLevel', 'flags', 'flags2',
              'qualityLevel', 'magicLevel') if k in f]))
    return lines


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    sessions = read_sessions(sys.argv[1:])
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        '..', '..', 'BHTests', 'fixtures')
    root = os.path.normpath(root)
    if not os.path.isdir(root):
        os.makedirs(root)

    # Rules first, each distinct set once, so the sessions that follow can name
    # the one they were played against.
    rule_set_ids = {}
    rule_lines = []
    session_lines = []
    print('sessions read: %d' % len(sessions))
    for i, session in enumerate(sessions):
        if not replayable(session):
            print('  %d: not replayable, parse cases only (%d drops)'
                  % (i + 1, len(session['drops'])))
            continue
        kept = curate_drops(session['drops'])
        if not kept:
            continue

        key = tuple(session['rules'])
        if key not in rule_set_ids:
            rule_set_ids[key] = len(rule_set_ids) + 1
            for rule in session['rules']:
                rule_lines.append('%s\tset=%d' % (rule, rule_set_ids[key]))
        set_id = rule_set_ids[key]

        print('  %d: filter=%s ping=%s, %d drops -> %d kept, rule set %d'
              % (i + 1, session['fields'].get('filterLevel'),
                 session['fields'].get('pingLevel'), len(session['drops']),
                 len(kept), set_id))
        session_lines.append('%s\truleSet=%d' % (session['header'], set_id))
        session_lines.extend(line for line, f in kept)

    print('distinct rule sets: %d (%d rules)'
          % (len(rule_set_ids), len(rule_lines)))
    filter_lines = rule_lines + session_lines

    parse_lines = curate_parse_cases(sessions)

    # Only the codes something kept refers to, so the larger table does not
    # arrive whole for the sake of a few hundred items.
    needed_codes = set()
    for line in parse_lines + [l for l in filter_lines if l.startswith('drop\t')]:
        for part in line.split('\t'):
            if part.startswith('code='):
                needed_codes.add(part[len('code='):])
    table_lines = curate_tables(sessions, needed_codes)
    print('table rows kept: %d for %d item codes' %
          (len(table_lines), len(needed_codes)))

    for name, lines in (('filter-cases.txt', filter_lines),
                        ('parse-cases.txt', parse_lines),
                        ('tables.txt', table_lines)):
        path = os.path.join(root, name)
        with open(path, 'w', encoding='latin-1', newline='\n') as handle:
            for line in lines:
                handle.write(line + '\n')
        print('%s: %d lines, %.0f KB' %
              (name, len(lines), os.path.getsize(path) / 1024.0))
    return 0


if __name__ == '__main__':
    sys.exit(main())
