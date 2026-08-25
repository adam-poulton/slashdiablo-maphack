# Area Ids and Area Levels

Two item filter codes describe the area an item is lying in. Both only ever match items
on the ground, so a rule using them never applies to inventory, stash, cube, belt or
equipped items. Match results are cached per item, so an item you pick up keeps the
match it had on the ground until something about the item itself changes.

| Code | Meaning |
| --- | --- |
| `AREAID` | Id of the area the item is lying in, as listed below. |
| `AREALVL` | Monster level of that area for the current difficulty, as listed below. |

Both support the usual comparison operators, for example:

```
ItemDisplay[AREALVL>83]: %NAME%
ItemDisplay[AREAID=108]: %NAME%
```

Area levels are the expansion values; classic characters use the classic monster levels
of the same area. Towns and other areas without monsters have an area level of 0.

| Id | Hex | Act | Area | Normal | Nightmare | Hell |
| ---: | --- | ---: | --- | ---: | ---: | ---: |
| 1 | 0x01 | 1 | Rogue Encampment | 0 | 0 | 0 |
| 2 | 0x02 | 1 | Blood Moor | 1 | 36 | 67 |
| 3 | 0x03 | 1 | Cold Plains | 2 | 36 | 68 |
| 4 | 0x04 | 1 | Stony Field | 4 | 37 | 68 |
| 5 | 0x05 | 1 | Dark Wood | 5 | 38 | 68 |
| 6 | 0x06 | 1 | Black Marsh | 6 | 38 | 69 |
| 7 | 0x07 | 1 | Tamoe Highland | 8 | 39 | 69 |
| 8 | 0x08 | 1 | Den of Evil | 1 | 36 | 79 |
| 9 | 0x09 | 1 | Cave Level 1 | 2 | 36 | 77 |
| 10 | 0x0A | 1 | Underground Passage Level 1 | 4 | 37 | 69 |
| 11 | 0x0B | 1 | Hole Level 1 | 5 | 38 | 80 |
| 12 | 0x0C | 1 | Pit Level 1 | 7 | 39 | 85 |
| 13 | 0x0D | 1 | Cave Level 2 | 2 | 37 | 78 |
| 14 | 0x0E | 1 | Underground Passage Level 2 | 4 | 38 | 83 |
| 15 | 0x0F | 1 | Hole Level 2 | 5 | 39 | 81 |
| 16 | 0x10 | 1 | Pit Level 2 | 7 | 40 | 85 |
| 17 | 0x11 | 1 | Burial Grounds | 3 | 36 | 80 |
| 18 | 0x12 | 1 | Crypt | 3 | 37 | 83 |
| 19 | 0x13 | 1 | Mausoleum | 3 | 37 | 85 |
| 20 | 0x14 | 1 | Forgotten Tower | 0 | 0 | 0 |
| 21 | 0x15 | 1 | Tower Cellar Level 1 | 7 | 38 | 75 |
| 22 | 0x16 | 1 | Tower Cellar Level 2 | 7 | 39 | 76 |
| 23 | 0x17 | 1 | Tower Cellar Level 3 | 7 | 40 | 77 |
| 24 | 0x18 | 1 | Tower Cellar Level 4 | 7 | 41 | 78 |
| 25 | 0x19 | 1 | Tower Cellar Level 5 | 7 | 42 | 79 |
| 26 | 0x1A | 1 | Monastery Gate | 8 | 40 | 70 |
| 27 | 0x1B | 1 | Outer Cloister | 9 | 40 | 70 |
| 28 | 0x1C | 1 | Barracks | 9 | 40 | 70 |
| 29 | 0x1D | 1 | Jail Level 1 | 10 | 41 | 71 |
| 30 | 0x1E | 1 | Jail Level 2 | 10 | 41 | 71 |
| 31 | 0x1F | 1 | Jail Level 3 | 10 | 41 | 71 |
| 32 | 0x20 | 1 | Inner Cloister | 10 | 41 | 72 |
| 33 | 0x21 | 1 | Cathedral | 11 | 42 | 72 |
| 34 | 0x22 | 1 | Catacombs Level 1 | 11 | 42 | 72 |
| 35 | 0x23 | 1 | Catacombs Level 2 | 11 | 42 | 73 |
| 36 | 0x24 | 1 | Catacombs Level 3 | 12 | 43 | 73 |
| 37 | 0x25 | 1 | Catacombs Level 4 | 12 | 43 | 73 |
| 38 | 0x26 | 1 | Tristram | 6 | 39 | 76 |
| 39 | 0x27 | 1 | Moo Moo Farm | 28 | 64 | 81 |
| 40 | 0x28 | 2 | Lut Gholein | 0 | 0 | 0 |
| 41 | 0x29 | 2 | Rocky Waste | 14 | 43 | 75 |
| 42 | 0x2A | 2 | Dry Hills | 15 | 44 | 76 |
| 43 | 0x2B | 2 | Far Oasis | 16 | 45 | 76 |
| 44 | 0x2C | 2 | Lost City | 17 | 46 | 77 |
| 45 | 0x2D | 2 | Valley of Snakes | 18 | 46 | 77 |
| 46 | 0x2E | 2 | Canyon of the Magi | 16 | 48 | 79 |
| 47 | 0x2F | 2 | Sewers Level 1 | 13 | 43 | 74 |
| 48 | 0x30 | 2 | Sewers Level 2 | 13 | 43 | 74 |
| 49 | 0x31 | 2 | Sewers Level 3 | 14 | 44 | 75 |
| 50 | 0x32 | 2 | Harem Level 1 | 0 | 0 | 0 |
| 51 | 0x33 | 2 | Harem Level 2 | 13 | 47 | 78 |
| 52 | 0x34 | 2 | Palace Cellar Level 1 | 13 | 47 | 78 |
| 53 | 0x35 | 2 | Palace Cellar Level 2 | 13 | 47 | 78 |
| 54 | 0x36 | 2 | Palace Cellar Level 3 | 13 | 48 | 78 |
| 55 | 0x37 | 2 | Stony Tomb Level 1 | 12 | 44 | 78 |
| 56 | 0x38 | 2 | Halls of the Dead Level 1 | 12 | 44 | 79 |
| 57 | 0x39 | 2 | Halls of the Dead Level 2 | 13 | 45 | 81 |
| 58 | 0x3A | 2 | Claw Viper Temple Level 1 | 14 | 47 | 82 |
| 59 | 0x3B | 2 | Stony Tomb Level 2 | 12 | 44 | 79 |
| 60 | 0x3C | 2 | Halls of the Dead Level 3 | 13 | 45 | 82 |
| 61 | 0x3D | 2 | Claw Viper Temple Level 2 | 14 | 47 | 83 |
| 62 | 0x3E | 2 | Maggot Lair Level 1 | 17 | 45 | 84 |
| 63 | 0x3F | 2 | Maggot Lair Level 2 | 17 | 45 | 84 |
| 64 | 0x40 | 2 | Maggot Lair Level 3 | 17 | 46 | 85 |
| 65 | 0x41 | 2 | Ancient Tunnels | 17 | 46 | 85 |
| 66 | 0x42 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 67 | 0x43 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 68 | 0x44 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 69 | 0x45 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 70 | 0x46 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 71 | 0x47 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 72 | 0x48 | 2 | Tal Rasha's Tomb | 17 | 49 | 80 |
| 73 | 0x49 | 2 | Duriel's Lair | 17 | 49 | 80 |
| 74 | 0x4A | 2 | Arcane Sanctuary | 14 | 48 | 79 |
| 75 | 0x4B | 3 | Kurast Docktown | 0 | 0 | 0 |
| 76 | 0x4C | 3 | Spider Forest | 21 | 49 | 79 |
| 77 | 0x4D | 3 | Great Marsh | 21 | 50 | 80 |
| 78 | 0x4E | 3 | Flayer Jungle | 22 | 50 | 80 |
| 79 | 0x4F | 3 | Lower Kurast | 22 | 52 | 80 |
| 80 | 0x50 | 3 | Kurast Bazaar | 22 | 52 | 81 |
| 81 | 0x51 | 3 | Upper Kurast | 23 | 52 | 81 |
| 82 | 0x52 | 3 | Kurast Causeway | 24 | 53 | 81 |
| 83 | 0x53 | 3 | Travincal | 24 | 54 | 82 |
| 84 | 0x54 | 3 | Spider Cave | 21 | 50 | 79 |
| 85 | 0x55 | 3 | Spider Cavern | 21 | 50 | 79 |
| 86 | 0x56 | 3 | Swampy Pit Level 1 | 21 | 51 | 80 |
| 87 | 0x57 | 3 | Swampy Pit Level 2 | 21 | 51 | 81 |
| 88 | 0x58 | 3 | Flayer Dungeon Level 1 | 22 | 51 | 81 |
| 89 | 0x59 | 3 | Flayer Dungeon Level 2 | 22 | 51 | 82 |
| 90 | 0x5A | 3 | Swampy Pit Level 3 | 21 | 51 | 82 |
| 91 | 0x5B | 3 | Flayer Dungeon Level 3 | 22 | 51 | 83 |
| 92 | 0x5C | 3 | Sewers Level 1 | 23 | 52 | 84 |
| 93 | 0x5D | 3 | Sewers Level 2 | 24 | 53 | 85 |
| 94 | 0x5E | 3 | Ruined Temple | 23 | 53 | 84 |
| 95 | 0x5F | 3 | Disused Fane | 23 | 53 | 84 |
| 96 | 0x60 | 3 | Forgotten Reliquary | 23 | 53 | 84 |
| 97 | 0x61 | 3 | Forgotten Temple | 24 | 54 | 85 |
| 98 | 0x62 | 3 | Ruined Fane | 24 | 54 | 85 |
| 99 | 0x63 | 3 | Disused Reliquary | 24 | 54 | 85 |
| 100 | 0x64 | 3 | Durance of Hate Level 1 | 25 | 55 | 83 |
| 101 | 0x65 | 3 | Durance of Hate Level 2 | 25 | 55 | 83 |
| 102 | 0x66 | 3 | Durance of Hate Level 3 | 25 | 55 | 83 |
| 103 | 0x67 | 4 | The Pandemonium Fortress | 0 | 0 | 0 |
| 104 | 0x68 | 4 | Outer Steppes | 26 | 56 | 82 |
| 105 | 0x69 | 4 | Plains of Despair | 26 | 56 | 83 |
| 106 | 0x6A | 4 | City of the Damned | 27 | 57 | 84 |
| 107 | 0x6B | 4 | River of Flame | 27 | 57 | 85 |
| 108 | 0x6C | 4 | Chaos Sanctum | 28 | 58 | 85 |
| 109 | 0x6D | 5 | Harrogath | 0 | 0 | 0 |
| 110 | 0x6E | 5 | Bloody Foothills | 24 | 58 | 80 |
| 111 | 0x6F | 5 | Rigid Highlands | 25 | 59 | 81 |
| 112 | 0x70 | 5 | Arreat Plateau | 26 | 60 | 81 |
| 113 | 0x71 | 5 | Crystalized Cavern Level 1 | 29 | 61 | 82 |
| 114 | 0x72 | 5 | Cellar of Pity | 29 | 61 | 83 |
| 115 | 0x73 | 5 | Crystalized Cavern Level 2 | 29 | 61 | 83 |
| 116 | 0x74 | 5 | Echo Chamber | 29 | 61 | 84 |
| 117 | 0x75 | 5 | Tundra Wastelands | 27 | 60 | 81 |
| 118 | 0x76 | 5 | Glacial Caves Level 1 | 29 | 62 | 82 |
| 119 | 0x77 | 5 | Glacial Caves Level 2 | 29 | 62 | 83 |
| 120 | 0x78 | 5 | Rocky Summit | 37 | 68 | 87 |
| 121 | 0x79 | 5 | Nihlathaks Temple | 32 | 63 | 83 |
| 122 | 0x7A | 5 | Halls of Anguish | 33 | 63 | 83 |
| 123 | 0x7B | 5 | Halls of Death's Calling | 34 | 64 | 84 |
| 124 | 0x7C | 5 | Halls of Vaught | 36 | 64 | 84 |
| 125 | 0x7D | 5 | Hell1 | 39 | 60 | 81 |
| 126 | 0x7E | 5 | Hell2 | 39 | 61 | 82 |
| 127 | 0x7F | 5 | Hell3 | 39 | 62 | 83 |
| 128 | 0x80 | 5 | The Worldstone Keep Level 1 | 39 | 65 | 85 |
| 129 | 0x81 | 5 | The Worldstone Keep Level 2 | 40 | 65 | 85 |
| 130 | 0x82 | 5 | The Worldstone Keep Level 3 | 42 | 66 | 85 |
| 131 | 0x83 | 5 | Throne of Destruction | 43 | 66 | 85 |
| 132 | 0x84 | 5 | The Worldstone Chamber | 43 | 66 | 85 |
| 133 | 0x85 | 5 | Pandemonium Run 1 | 50 | 75 | 83 |
| 134 | 0x86 | 5 | Pandemonium Run 2 | 50 | 75 | 83 |
| 135 | 0x87 | 5 | Pandemonium Run 3 | 50 | 75 | 83 |
| 136 | 0x88 | 5 | Tristram | 50 | 75 | 83 |
