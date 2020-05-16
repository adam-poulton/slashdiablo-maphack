Note: As of BH 1.9.9, Monster Color lines go in the `BH_settings.cfg` file, not `BH.cfg`.

The "Monster Color" parameters in BH_settings.cfg let you control the colors of monster symbols placed on the map. Here is the default configuration:

    Monster Color[Normal]:   0x5B
    Monster Color[Minion]:   0x60
    Monster Color[Champion]: 0x91
    Monster Color[Boss]:     0x84

This makes normal monsters appear red on the map, boss monsters appear green, minions of bosses appear orange, and champions appear blue. (See the link at the bottom of this page to determine the available colors, and the codes corresponding to them.)

You can also control the colors of individual monster types. Suppose you want to make the basic Act 1 zombie appear purple on the map. First find the monster in this [monster list](Monsters). Zombies show up in that list with monster number 5. So add this line to BH.cfg:

    Monster Color[5]: 0x9B

Now all zombies on the map will be purple. This purple color will override the Minion/Champion colors, but not the Boss color -- a zombie boss will still appear green (though its minions will be purple).

Monsters will often show up in the monster list multiple times (this can happen for example when a monster is also a "guest" monster with different stats in a different act, so it gets multiple entries in the list). When this happens, find all occurrences of the monster in the list and put an entry in BH.cfg for each monster number.

Here is a basic example that makes gloams and those little exploding undead doll guys appear purple on the map:

    Monster Color[118]:      0x9B
    Monster Color[119]:      0x9B
    Monster Color[120]:      0x9B
    Monster Color[121]:      0x9B
    Monster Color[639]:      0x9B
    Monster Color[640]:      0x9B
    Monster Color[641]:      0x9B
    Monster Color[733]:      0x9B
    Monster Color[212]:      0x9B
    Monster Color[213]:      0x9B
    Monster Color[214]:      0x9B
    Monster Color[215]:      0x9B
    Monster Color[216]:      0x9B
    Monster Color[690]:      0x9B
    Monster Color[691]:      0x9B

[[To see all the colors you can use, look here.|https://github.com/planqi/slashdiablo-maphack/blob/master/readme_gfx/color_palette.png]]
