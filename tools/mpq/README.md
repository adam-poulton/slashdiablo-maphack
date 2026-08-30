MPQ data tools
==============

BH reads the game's data tables (`Runes.txt`, `ItemTypes.txt`, `Properties.txt`,
the `.tbl` string tables and so on) out of the Diablo II MPQ archives at run
time. These tools pull the same files out on the desktop, so you can see what
the data actually says while working on a feature rather than guessing at column
names and values.

They are development tools. They are not part of the BH build and nothing in the
DLL depends on them.

## dumpmpq

Extracts one file from an archive.

```
dumpmpq <archive> <internal path> <out file>
```

### Building

It links `ThirdParty/StormLib.lib`, which is a **32 bit import library** whose
exports are decorated `__stdcall` (`_SFileOpenArchive@16`). So it has to be built
for x86, and `StormLib.dll` must sit next to the executable when you run it.
Declaring the functions `__cdecl` compiles but fails to link.

From a shell, with Visual Studio installed:

```bash
cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat\" >nul && cl /nologo /EHsc /MT dumpmpq.cpp /link ..\..\ThirdParty\StormLib.lib"
```

Then copy the DLL beside it:

```bash
cp ../../ThirdParty/StormLib.dll .
```

### Where the files live

The archives sit in the Diablo II install directory. Which archive holds a given
file is not obvious, and `SFileOpenFileEx` only searches the one you opened, so
you may have to try a few. At the time of writing:

| File                                    | Archive        |
| --------------------------------------- | -------------- |
| `data\global\excel\*.txt`               | `Patch_D2.mpq` |
| `data\local\lng\eng\patchstring.tbl`    | `Patch_D2.mpq` |
| `data\local\lng\eng\string.tbl`         | `d2data.mpq`   |
| `data\local\lng\eng\expansionstring.tbl`| `d2exp.mpq`    |

`eng` is the locale the client was installed with, so a non-English install uses
a different directory. BH itself goes through the game's own MPQ layer, which
searches every loaded archive, so it only needs the path.

Examples:

```bash
./dumpmpq.exe "/c/Program Files (x86)/Diablo II/Patch_D2.mpq" "data\global\excel\runes.txt" runes.txt
./dumpmpq.exe "/c/Program Files (x86)/Diablo II/d2data.mpq" "data\local\lng\eng\string.tbl" string.tbl
```

The `.txt` files are tab separated with a header row, so anything that reads TSV
will do:

```python
import csv
rows = list(csv.DictReader(open('runes.txt', encoding='latin-1'), delimiter='\t'))
```

Note that the header names are exactly as they appear in the file, comment
columns and all: `Rune Name`, `*Patch Release`, `T1Code1`.

## tbl.py

Reads the extracted `.tbl` files, which map the keys the data tables refer to
onto the text the game shows. It merges whichever of the three are present in the
directory you point it at, so extract all of them (they come from three different
archives, per the table above) or keys will come back as `None`:

```bash
./dumpmpq.exe "$D2/d2data.mpq"   "data\local\lng\eng\string.tbl"          string.tbl
./dumpmpq.exe "$D2/d2exp.mpq"    "data\local\lng\eng\expansionstring.tbl" expansionstring.tbl
./dumpmpq.exe "$D2/Patch_D2.mpq" "data\local\lng\eng\patchstring.tbl"     patchstring.tbl
```

```bash
python tbl.py .                                  # 8683 strings
python tbl.py . ModStr3k Runeword130 StrSklTabItem7
#   ModStr3k -> 'to All Skills'
#   Runeword130 -> 'Spirit'
#   StrSklTabItem7 -> '+%d to Poison and Bone Skills'
```

It is also importable, which is the more useful form when cross checking a
change against every row of a table:

```python
import csv, tbl
strings = tbl.load_all('.')
for row in csv.DictReader(open('ItemStatCost.txt', encoding='latin-1'), delimiter='\t'):
    print(row['Stat'], strings.get(row['descstrpos']))
```

## make_fixtures.py

Writes the table fixtures `BHTests` reads, so the tests can stand the game's
tables up with no game running. Point it at a directory holding the extracted
`.txt` tables and all three `.tbl` files, and at the repository root:

```bash
python make_fixtures.py . /c/repos/slashdiablo-maphack
```

The tables a property or an item type is looked up in go in whole; the tables
holding the items under test, which include the three the base items are split
across, are trimmed to those items with their header row kept; the three string
tables are merged into one `key<TAB>text` file trimmed to the keys the fixtures
can reach. Rerun it when the tests need an item the fixtures do not have, adding
the item to `SUBJECT_TABLES` first.

## What these are good for

Reading the data, and checking a change against all of it at once. The runeword
stat rendering was built this way: dumping `Runes.txt`, `Gems.txt`,
`Properties.txt`, `ItemStatCost.txt` and the string tables, then rendering every
recipe offline to find the entries that came out wrong. That caught, among other
things, `dmg%` producing no line at all, which had silently dropped Enhanced
Damage from 38 of the 78 runewords.

What they are **not** good for is testing the shipped code. A script that
reimplements what `StatDescriptions.cpp` does only tests your understanding of
the game's rules; it drifts the moment the C++ changes, and it can't see faults
that live in the C++ itself. To check the real code, run the real code against
the tables: `make_fixtures.py` is how they reach `BHTests`.
