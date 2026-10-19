"""Converts the art and Tiled files in assets/ into C source for GBDK. Run by the Makefile.

    python scripts/assets.py tileset  assets/tilesets/hotel.tsx  build/gen
    python scripts/assets.py room     assets/rooms/hall_ns.tmx  build/gen
    python scripts/assets.py roomlist build/gen assets/rooms/*.tmx
    python scripts/assets.py sprite   assets/sprites/player.json build/gen
    python scripts/assets.py font     assets/fonts/font.json    build/gen
    python scripts/assets.py image    assets/title/logo.gbc.png build/gen
    python scripts/assets.py songfix  build/gen/raw/music_x.c build/gen/music_x.c   (after uge2source)

Every graphic exists twice (owner's decision, Docs/PROJECT.md): `<name>.gbc.png` in colour for the GBC ROM
and `<name>.dmg.png` in the four DMG greys, with the same layout. The generated .c file holds both versions
behind `#ifdef CGB`, so each ROM only links its own.

Rules the art has to follow (the converter stops with a message naming the place that breaks one):
  GBC background: at most 4 colours per 8x8 tile, at most 7 palettes for a tileset (palette 7 is the font's).
  GBC sprites:    at most 3 colours (+ transparency) per 8x16 hardware sprite, at most 8 palettes.
  GBC font:       one palette (at most 4 colours) for the whole font.
  DMG:            only the 4 greys (FF, AA, 55, 00); a sprite column uses white or light grey, not both.
Rooms: see Docs/ASSETS.md (doors, drawers, key spots, map properties).
"""
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

DMG_LEVELS = [255, 170, 85, 0]  # shade 0..3
# DMG sprite palettes, which must match platform_dmg.c: index 1..3 -> shade
DMG_OBP = {0: (0, 2, 3), 1: (1, 2, 3)}  # OBP0: white, dark, black. OBP1: light, dark, black

# Background VRAM plan, which must match src/room.h: tileset tiles from 0, generated door-plate tiles at
# 184-191, the font at 192-255. Metatiles 120-127 are made at runtime (door plates).
TILESET_MAX_TILES = 184
TILESET_MAX_METATILES = 120
TILESET_MAX_PALETTES = 7
FONT_FIRST_TILE = 192
FONT_MAX_TILES = 64

# Room objects (src/assets.h OBJ_*, WALL_*)
OBJ_TYPES = {"entry": 1, "exit": 2, "drawer": 3, "keyspot": 4, "spawn": 5, "closet": 6, "bed": 7, "paper": 8,
             "figure": 9, "seek": 10, "snare": 11}
# Hiding spots and the metatiles they must cover (top, bottom)
HIDING = {"closet": ("closet_top", "closet_bottom"), "bed": ("bed_top", "bed_bottom")}
WALLS = {"top": 1, "bottom": 2, "left": 3, "right": 4}
ROOM_KINDS = {"normal": 0, "start": 1, "test": 2, "halt": 3, "library": 4,
              "seek_start": 5, "seek_4way": 6, "seek_end": 7, "courtyard": 8, "greenhouse": 9}
# Seek's chases (src/run.c, seek.c, Minecraft docs 08): a start hallway, four-way rooms, an end hallway
SEEK_KINDS = ("seek_start", "seek_4way", "seek_end")
# The Courtyard (room 89) and the Greenhouse (90-99) (src/run.c, garden.c, Minecraft docs 09)
GARDEN_KINDS = ("courtyard", "greenhouse")
# Metatiles the code names (MT_HOTEL_* in src/game.c, halt.c): every tileset keeps them at the hotel's numbers,
# so the same code works whatever tileset a room uses
SHARED_METATILES = ("void", "floor", "door_top", "door_bottom", "door_open_top", "door_open_bottom",
                    "door_bottom_locked", "side_door_top", "side_door_bottom", "side_door_bottom_locked",
                    "doorway_s", "wall_top", "dresser_open")
# Snares (garden.c): the run picks this many of a room's candidate cells
SNARES_PER_ROOM = 3
# The Library (src/library.c, figure.c): Figure's route search uses the room map's unused tail, one byte a
# cell plus a 512-byte queue (figure.c QUEUE_BYTES), so a library may use at most this much of ROOM_MAX
ROOM_MAX = 2816        # src/room.h
LIBRARY_MAX_CELLS = (ROOM_MAX - 512) // 2
LIBRARY_BOOKS = 8      # books placed per run (library.c LIB_BOOKS)
EXITS_MAX = 3   # a Dupe room has up to 3 exits, one real per run (run_exit in src/run.c)


class AssetError(Exception):
    pass


def ident(name: str) -> str:
    return re.sub(r"\W", "_", name).lower()


# ---------------------------------------------------------------------------------------------- pixels

def rgb15(c):
    """8-bit RGB to the GBC's 5 bits per channel."""
    return (c[0] >> 3, c[1] >> 3, c[2] >> 3)


def luminance(c15):
    return 0.299 * c15[0] + 0.587 * c15[1] + 0.114 * c15[2]


def dmg_shade(c, where):
    level = sum(c[:3]) / 3
    shade = min(range(4), key=lambda i: abs(DMG_LEVELS[i] - level))
    if abs(DMG_LEVELS[shade] - level) > 24 or max(c[:3]) - min(c[:3]) > 24:
        raise AssetError(f"{where}: colour {c[:3]} is not one of the 4 DMG greys (FF, AA, 55, 00)")
    return shade


def tile_2bpp(indices):
    """8x8 list of rows of colour indices -> 16 bytes."""
    out = []
    for row in indices:
        lo = hi = 0
        for x, v in enumerate(row):
            lo |= (v & 1) << (7 - x)
            hi |= ((v >> 1) & 1) << (7 - x)
        out += [lo, hi]
    return bytes(out)


def pack_palettes(colour_sets, size, limit, what):
    """Greedy packing of colour sets into palettes of `size` colours. Returns (palettes, index per set)."""
    palettes = []
    order = sorted(range(len(colour_sets)), key=lambda i: -len(colour_sets[i]))
    assign = [None] * len(colour_sets)
    for i in order:
        s = colour_sets[i]
        best = None
        for p, pal in enumerate(palettes):
            union = pal | s
            if len(union) <= size and (best is None or len(union) < len(palettes[best] | s)):
                best = p
        if best is None:
            palettes.append(set(s))
            best = len(palettes) - 1
        else:
            palettes[best] |= s
        assign[i] = best
    if len(palettes) > limit:
        raise AssetError(f"{what}: needs {len(palettes)} GBC palettes, the limit is {limit}")
    # Light to dark, so index order matches the DMG shade order where possible
    return [sorted(p, key=luminance, reverse=True) for p in palettes], assign


def c_bytes(data, per_line=16):
    data = list(data)
    lines = []
    for i in range(0, len(data), per_line):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + per_line]) + ",")
    return "\n".join(lines) if lines else "    0"


def c_palettes(palettes, size=4, transparent_first=False):
    """Sprite palettes start with an unused colour 0 (transparent), so their colours are at 1..3."""
    rows = []
    for pal in palettes:
        cols = ([(31, 0, 31)] if transparent_first else []) + list(pal)
        cols += [(0, 0, 0)] * (size - len(cols))
        rows.append("    " + ", ".join(f"RGB({r}, {g}, {b})" for r, g, b in cols) + ",")
    return "\n".join(rows)


def load_pair(png_gbc: Path):
    png_dmg = Path(str(png_gbc).replace(".gbc.png", ".dmg.png"))
    if not png_gbc.name.endswith(".gbc.png"):
        raise AssetError(f"{png_gbc}: colour art must be named <name>.gbc.png")
    if not png_dmg.exists():
        raise AssetError(f"{png_dmg}: missing (every graphic needs a .dmg.png next to its .gbc.png)")
    gbc = Image.open(png_gbc).convert("RGBA")
    dmg = Image.open(png_dmg).convert("RGBA")
    if gbc.size != dmg.size:
        raise AssetError(f"{png_dmg}: size {dmg.size} differs from {png_gbc.name} {gbc.size}")
    return gbc, dmg


# ---------------------------------------------------------------------------------------------- tileset

def tsx_tiles(tsx: Path):
    root = ET.parse(tsx).getroot()
    tw, th = int(root.get("tilewidth")), int(root.get("tileheight"))
    if (tw, th) != (16, 16):
        raise AssetError(f"{tsx}: tiles must be 16x16 (metatiles), not {tw}x{th}")
    image = tsx.parent / root.find("image").get("source")
    count = int(root.get("tilecount"))
    props = [{} for _ in range(count)]
    for t in root.findall("tile"):
        tid = int(t.get("id"))
        for p in t.findall("properties/property"):
            v = p.get("value")
            if p.get("type") == "bool":
                v = v == "true"
            props[tid][p.get("name")] = v
    return root.get("name"), image, int(root.get("columns")), count, props


def convert_tileset(tsx: Path, out: Path):
    name, image, columns, count, props = tsx_tiles(tsx)
    sym = ident(name)
    gbc, dmg = load_pair(image)

    def metatile_tiles(img, m):
        ox, oy = (m % columns) * 16, (m // columns) * 16
        return [(ox + dx, oy + dy) for dy in (0, 8) for dx in (0, 8)]  # TL, TR, BL, BR

    # GBC: palettes per 8x8 tile
    cells = [(m, x, y) for m in range(count) for (x, y) in metatile_tiles(gbc, m)]
    colour_sets = []
    for m, x, y in cells:
        s = {rgb15(gbc.getpixel((x + i, y + j))) for j in range(8) for i in range(8)}
        if len(s) > 4:
            raise AssetError(f"{image.name}: metatile {m} ({props[m].get('name', '?')}), 8x8 tile at "
                             f"({x}, {y}) has {len(s)} colours; the GBC allows 4")
        colour_sets.append(s)
    if count > TILESET_MAX_METATILES:
        raise AssetError(f"{tsx.name}: {count} metatiles; the limit is {TILESET_MAX_METATILES}")
    palettes, assign = pack_palettes(colour_sets, 4, TILESET_MAX_PALETTES, image.name)

    def dedupe(entries):
        tiles, index, refs = [], {}, []
        for data in entries:
            if data not in index:
                index[data] = len(tiles)
                tiles.append(data)
            refs.append(index[data])
        return tiles, refs

    gbc_data = []
    for (m, x, y), p in zip(cells, assign):
        pal = palettes[p]
        gbc_data.append(tile_2bpp([[pal.index(rgb15(gbc.getpixel((x + i, y + j)))) for i in range(8)]
                                   for j in range(8)]))
    gbc_tiles, gbc_refs = dedupe(gbc_data)
    dmg_data = [tile_2bpp([[dmg_shade(dmg.getpixel((x + i, y + j)), f"{image.name.replace('.gbc', '.dmg')} ({x + i}, {y + j})")
                            for i in range(8)] for j in range(8)]) for (m, x, y) in cells]
    dmg_tiles, dmg_refs = dedupe(dmg_data)
    for label, tiles in (("GBC", gbc_tiles), ("DMG", dmg_tiles)):
        if len(tiles) > TILESET_MAX_TILES:
            raise AssetError(f"{tsx.name}: {len(tiles)} unique {label} tiles; a tileset has room for "
                             f"{TILESET_MAX_TILES} (the rest of VRAM is door plates and the font)")

    # MTF_SOLID, MTF_BOOKCASE, MTF_FIRE, MTF_WALL, MTF_SNARE (assets.h): a bookcase is where the Library's books
    # can be; fire kills whoever walks into it (Seek's end hallway); a wall can show Seek's eyes before a chase;
    # a snare is the Greenhouse's trap (the run draws it on a 'floor' cell, garden.c)
    flags = [(1 if props[m].get("solid") else 0) | (2 if props[m].get("bookcase") else 0) |
             (4 if props[m].get("fire") else 0) | (8 if props[m].get("wall") else 0) |
             (16 if props[m].get("snare") else 0) for m in range(count)]
    for m in range(count):
        if props[m].get("fire") and props[m].get("solid"):
            raise AssetError(f"{tsx.name}: metatile {m} is fire and solid; fire must be walkable (it kills)")
    names = [props[m].get("name", f"mt{m}") for m in range(count)]
    if len(set(names)) != len(names):
        dup = sorted({n for n in names if names.count(n) > 1})
        raise AssetError(f"{tsx.name}: metatile names must be different; used twice: {', '.join(dup)}")
    if name != "hotel":
        h_names = [p.get("name", f"mt{i}") for i, p in enumerate(tsx_tiles(tsx.parent / "hotel.tsx")[4])]
        for n in SHARED_METATILES:
            m = h_names.index(n)
            if m >= count or names[m] != n:
                raise AssetError(f"{tsx.name}: metatile {m} must be '{n}' as in hotel.tsx (the code uses the "
                                 "hotel's numbers for doors, walls and drawers in every room; ASSETS.md)")
    if sum(1 for m in range(count) if props[m].get("snare")) > 1:
        raise AssetError(f"{tsx.name}: at most one metatile has snare = true")

    h = [f"// Generated by scripts/assets.py from {tsx.as_posix()}. Do not edit.",
         f"#ifndef TILESET_{sym.upper()}_H", f"#define TILESET_{sym.upper()}_H", '#include "assets.h"',
         f"BANKREF_EXTERN(tileset_{sym})", f"extern const tileset_t tileset_{sym};"]
    for m, n in enumerate(names):
        h.append(f"#define MT_{sym.upper()}_{ident(n).upper()} {m}")
    snare = next((m for m in range(count) if props[m].get("snare")), None)
    if snare is not None:
        h.append(f"#define TILESET_{sym.upper()}_SNARE {snare}   // the metatile a Snare is drawn with")
    # Background tiles 128 and up are sprite tiles 128 and up (sprites.h SPR_TILE_LIMIT)
    h += ["#ifdef CGB", f"#define TILESET_{sym.upper()}_TILES {len(gbc_tiles)}", "#else",
          f"#define TILESET_{sym.upper()}_TILES {len(dmg_tiles)}", "#endif", "#endif"]
    c = [f"// Generated by scripts/assets.py from {tsx.as_posix()}. Do not edit.",
         "#pragma bank 255", f'#include "tileset_{sym}.h"', f"BANKREF(tileset_{sym})",
         "#ifdef CGB",
         f"static const uint8_t tiles[] = {{\n{c_bytes(b''.join(gbc_tiles))}\n}};",
         f"static const uint8_t mt_tiles[] = {{\n{c_bytes(gbc_refs, 4)}\n}};",
         f"static const uint8_t mt_attrs[] = {{\n{c_bytes(assign, 4)}\n}};",
         f"static const palette_color_t palettes[] = {{\n{c_palettes(palettes)}\n}};",
         f"#define TILE_COUNT {len(gbc_tiles)}", f"#define PALETTE_COUNT {len(palettes)}",
         "#else",
         f"static const uint8_t tiles[] = {{\n{c_bytes(b''.join(dmg_tiles))}\n}};",
         f"static const uint8_t mt_tiles[] = {{\n{c_bytes(dmg_refs, 4)}\n}};",
         f"#define TILE_COUNT {len(dmg_tiles)}",
         "#endif",
         f"static const uint8_t mt_flags[] = {{\n{c_bytes(flags)}\n}};",
         f"const tileset_t tileset_{sym} = {{",
         f"    TILE_COUNT, tiles, {count}, mt_tiles, mt_flags,",
         "#ifdef CGB", "    mt_attrs, PALETTE_COUNT, palettes,", "#endif", "};"]
    write(out, f"tileset_{sym}", h, c)
    print(f"{tsx.name}: {count} metatiles, GBC {len(gbc_tiles)} tiles / {len(palettes)} palettes, "
          f"DMG {len(dmg_tiles)} tiles")


# ---------------------------------------------------------------------------------------------- room

def parse_room(tmx: Path):
    root = ET.parse(tmx).getroot()
    w, h = int(root.get("width")), int(root.get("height"))
    if root.get("infinite") == "1":
        raise AssetError(f"{tmx}: infinite maps aren't supported")
    if w > 127 or h > 127 or w * h > ROOM_MAX:
        raise AssetError(f"{tmx}: {w}x{h} is too big (max 127 per side and {ROOM_MAX} metatiles; ROOM_MAX in room.h)")
    ts = root.findall("tileset")
    if len(ts) != 1:
        raise AssetError(f"{tmx}: a room uses exactly one tileset")
    firstgid = int(ts[0].get("firstgid"))
    tsx = (tmx.parent / ts[0].get("source")).resolve()
    ts_name, _, _, ts_count, ts_props = tsx_tiles(tsx)
    ts_names = [p.get("name", f"mt{i}") for i, p in enumerate(ts_props)]
    layers = root.findall("layer")
    if len(layers) != 1:
        raise AssetError(f"{tmx}: a room has exactly one tile layer (objects go in an object layer)")
    data = layers[0].find("data")
    if data.get("encoding") != "csv":
        raise AssetError(f"{tmx}: save with tile layer format CSV (Map > Map Properties)")
    gids = [int(v) for v in data.text.replace("\n", "").split(",") if v.strip()]
    cells = []
    for i, g in enumerate(gids):
        if g & 0xF0000000:
            raise AssetError(f"{tmx}: flipped/rotated tile at ({i % w}, {i // w}) isn't supported")
        m = g - firstgid if g else 0  # empty cells become metatile 0 (void)
        if not 0 <= m < ts_count:
            raise AssetError(f"{tmx}: tile id {g} at ({i % w}, {i // w}) is outside the tileset")
        cells.append(m)

    def prop(el, name, default=None):
        for p in el.findall("properties/property"):
            if p.get("name") == name:
                v = p.get("value")
                return (v == "true") if p.get("type") == "bool" or v in ("true", "false") else v
        return default

    kind = prop(root, "kind", "normal")
    if kind not in ROOM_KINDS:
        raise AssetError(f"{tmx}: map property kind = '{kind}'; use one of {', '.join(ROOM_KINDS)}")
    allow_lock = bool(prop(root, "allow_lock", False))

    # Objects, in metatile units. Rectangles must sit on the metatile grid.
    objects = []
    spawn = None
    for obj in root.iter("object"):
        name = obj.get("name") or obj.get("type") or obj.get("class") or ""
        where = f"{tmx.name}: object '{name}' (id {obj.get('id')})"
        if name not in OBJ_TYPES:
            raise AssetError(f"{where}: unknown name; use one of {', '.join(OBJ_TYPES)}")
        px, py = float(obj.get("x")), float(obj.get("y"))
        if name == "spawn":
            spawn = (int(px), int(py))
            objects.append((OBJ_TYPES[name], int(px) // 16, int(py) // 16, 1, 1, 0))
            continue
        if name in ("figure", "seek"):   # points: Figure's start; where Seek rises (optional)
            fx, fy = int(px) // 16, int(py) // 16
            if not (0 <= fx < w and 0 <= fy < h) or ts_props[cells[fy * w + fx]].get("solid"):
                raise AssetError(f"{where}: must be on a floor cell inside the map")
            objects.append((OBJ_TYPES[name], fx, fy, 1, 1, 0))
            continue
        if obj.find("point") is not None:
            if name == "snare":
                raise AssetError(f"{where}: a 'snare' object is a rectangle over the cells a Snare may be on")
            pw = ph = 16
        else:
            pw, ph = float(obj.get("width", 16)), float(obj.get("height", 16))
        if px % 16 or py % 16 or pw % 16 or ph % 16:
            raise AssetError(f"{where}: must line up with the 16x16 grid (hold Ctrl while drawing in Tiled)")
        x, y, ow, oh = int(px) // 16, int(py) // 16, int(pw) // 16, int(ph) // 16
        if not (0 <= x and x + ow <= w and 0 <= y and y + oh <= h):
            raise AssetError(f"{where}: outside the map")
        param = 0
        if name in ("entry", "exit"):
            wall = prop(obj, "wall")
            if wall not in WALLS:
                raise AssetError(f"{where}: needs a property wall = top, bottom, left or right")
            if name == "exit" and wall == "bottom":
                raise AssetError(f"{where}: exits can be on the top, left or right wall (owner's rule, DESIGN.md)")
            if name == "entry" and wall == "top":
                raise AssetError(f"{where}: an entry can't be on the top wall (exits only go up or sideways)")
            expect = {"top": (1, 2), "bottom": (1, 1), "left": (1, 2), "right": (1, 2)}[wall]
            if (ow, oh) != expect:
                raise AssetError(f"{where}: a {wall}-wall door is {expect[0]}x{expect[1]} metatiles, not {ow}x{oh}")
            edge = {"top": y == 1, "bottom": y + oh == h, "left": x == 0, "right": x + ow == w}[wall]
            if not edge:
                raise AssetError(f"{where}: a {wall}-wall door must be in that wall "
                                 "(top: rows 1-2; bottom: last row; left/right: first/last column)")
            param = WALLS[wall]
        elif name == "keyspot":
            param = 1 if prop(obj, "fixed", False) else 0
        elif name == "drawer":
            if (ow, oh) != (1, 1):
                raise AssetError(f"{where}: a drawer is one metatile")
            if ts_names[cells[y * w + x]] != "dresser":
                raise AssetError(f"{where}: must sit on a 'dresser' metatile, not '{ts_names[cells[y * w + x]]}'")
        elif name == "paper":
            if (ow, oh) != (1, 1) or ts_names[cells[y * w + x]] != "desk_paper":
                raise AssetError(f"{where}: the paper is one metatile on a 'desk_paper' metatile")
        elif name == "snare":
            pass   # candidate cells: counted below
        elif name in HIDING:
            if (ow, oh) != (1, 2):
                raise AssetError(f"{where}: a {name} is 1x2 metatiles (its top and bottom halves)")
            found = (ts_names[cells[y * w + x]], ts_names[cells[(y + 1) * w + x]])
            if found != HIDING[name]:
                raise AssetError(f"{where}: must cover the metatiles {HIDING[name]}, not {found}")
        objects.append((OBJ_TYPES[name], x, y, ow, oh, param))

    def count(t):
        return sum(1 for o in objects if o[0] == OBJ_TYPES[t])

    entry_wall = next((o[5] for o in objects if o[0] == OBJ_TYPES["entry"]), 0)
    exit_walls = [o[5] for o in objects if o[0] == OBJ_TYPES["exit"]]
    exit_wall = exit_walls[0] if exit_walls else 0
    if kind == "normal" and (count("entry") != 1 or not 1 <= count("exit") <= EXITS_MAX):
        raise AssetError(f"{tmx.name}: a normal room needs exactly one 'entry' and 1-{EXITS_MAX} 'exit' objects "
                         "(more than one: a Dupe room, where the run picks the real one)")
    if kind not in ("normal", "seek_4way", "greenhouse") and count("exit") > 1:
        raise AssetError(f"{tmx.name}: only normal, seek_4way and greenhouse rooms can have more than one exit")
    if kind in GARDEN_KINDS:
        if count("entry") != 1 or not count("exit"):
            raise AssetError(f"{tmx.name}: a {kind} room needs one 'entry' and 1-{EXITS_MAX} 'exit' objects")
        if kind == "courtyard" and entry_wall != WALLS["bottom"]:
            raise AssetError(f"{tmx.name}: the Courtyard is entered from the bottom wall (room 88 has one door, on "
                             "top)")
        if kind == "courtyard" and count("snare"):
            raise AssetError(f"{tmx.name}: no Snares in the Courtyard (nothing hunts there)")
    # Snares: the candidates are the 'floor' cells inside the room's 'snare' rectangles; the run picks a few
    snare_cells, covered = set(), set()
    for o in objects:
        if o[0] == OBJ_TYPES["snare"]:
            rect = {(xx, yy) for yy in range(o[2], o[2] + o[4]) for xx in range(o[1], o[1] + o[3])}
            if rect & covered:
                raise AssetError(f"{tmx.name}: 'snare' rectangles overlap (garden.c counts a shared cell twice)")
            covered |= rect
            snare_cells |= {c for c in rect if ts_names[cells[c[1] * w + c[0]]] == "floor"}
    # Never where you land coming through a door, or stand to go through one: a Snare there is a trap you can't
    # have seen
    for o in objects:
        if o[0] in (OBJ_TYPES["entry"], OBJ_TYPES["exit"]):
            inside = {1: [(o[1], o[2] + o[4])], 2: [(o[1], o[2] - 1)], 3: [(o[1] + 1, o[2]), (o[1] + 1, o[2] + 1)],
                      4: [(o[1] - 1, o[2]), (o[1] - 1, o[2] + 1)]}[o[5]]
            if snare_cells & set(inside):
                raise AssetError(f"{tmx.name}: a 'snare' rectangle covers {sorted(snare_cells & set(inside))}, the "
                                 "cell just inside a door; keep Snares a cell away from doors")
    if count("snare"):
        if not any(p.get("snare") for p in ts_props):
            raise AssetError(f"{tmx.name}: has 'snare' objects but its tileset has no metatile with snare = true "
                             "(use garden.tsx)")
        if not SNARES_PER_ROOM <= len(snare_cells) <= 255:
            raise AssetError(f"{tmx.name}: its 'snare' rectangles cover {len(snare_cells)} 'floor' cells; the "
                             f"Snares need {SNARES_PER_ROOM} to 255")
    if kind in SEEK_KINDS:
        if count("entry") != 1 or not count("exit"):
            raise AssetError(f"{tmx.name}: a {kind} room needs one 'entry' and at least one 'exit'")
        if kind == "seek_4way" and count("exit") < 2:
            raise AssetError(f"{tmx.name}: a four-way room needs 2-{EXITS_MAX} exits (one is real per run, the "
                             "others are fake)")
        if kind == "seek_start" and entry_wall != WALLS["bottom"]:
            raise AssetError(f"{tmx.name}: the start hallway is entered from the bottom wall (the room before it has "
                             "one door, on top)")
        if count("drawer") + count("keyspot") + count("closet") + count("bed"):
            raise AssetError(f"{tmx.name}: chase rooms have no drawers, key spots, closets or beds (nothing else "
                             "runs in a chase)")
    if count("seek") and kind != "seek_start":
        raise AssetError(f"{tmx.name}: a 'seek' point (where he rises) is only for kind = seek_start")
    if count("seek") > 1:
        raise AssetError(f"{tmx.name}: at most one 'seek' point")
    if kind == "halt" and (count("entry") != 1 or exit_walls != [WALLS["right"]] or entry_wall != WALLS["left"]):
        raise AssetError(f"{tmx.name}: Halt's hallway runs left to right: one 'entry' on the left wall and one "
                         "'exit' on the right wall")
    books = 0
    if kind == "library":
        if (count("entry") != 1 or entry_wall != WALLS["bottom"] or exit_walls != [WALLS["top"]] or
                count("paper") != 1 or count("figure") != 1):
            raise AssetError(f"{tmx.name}: the Library needs one 'entry' on the bottom wall, one 'exit' on the top "
                             "wall (the padlocked door), one 'paper' and one 'figure' (his starting point)")
        if w * h > LIBRARY_MAX_CELLS:
            raise AssetError(f"{tmx.name}: {w * h} metatiles; a library may have at most {LIBRARY_MAX_CELLS} "
                             "(Figure's route search uses the rest of the room map)")
        # Figure's route search skips bounds checks: the map's edge must be wall (the entry is sealed in play)
        entry = next(o for o in objects if o[0] == OBJ_TYPES["entry"])
        for i, m in enumerate(cells):
            x, y = i % w, i // w
            if (x in (0, w - 1) or y in (0, h - 1)) and not ts_props[m].get("solid") and (x, y) != entry[1:3]:
                raise AssetError(f"{tmx.name}: ({x}, {y}) is on the map's edge and isn't solid (Figure, figure.c)")
        # A book can be on any bookcase metatile with floor below it (where the player stands to read it)
        books = sum(1 for i, m in enumerate(cells) if ts_props[m].get("bookcase") and i + w < len(cells)
                    and not ts_props[cells[i + w]].get("solid"))
        if not LIBRARY_BOOKS <= books <= 255:
            raise AssetError(f"{tmx.name}: {books} bookcase metatiles with floor below; the books need "
                             f"{LIBRARY_BOOKS} to 255")
    elif count("paper") or count("figure"):
        raise AssetError(f"{tmx.name}: 'paper' and 'figure' objects are for the Library (kind = library)")
    if kind == "start" and (count("exit") != 1 or spawn is None):
        raise AssetError(f"{tmx.name}: the start room needs an 'exit' and a 'spawn' object")
    if kind == "start" and not any(o[0] == OBJ_TYPES["keyspot"] and o[5] for o in objects):
        raise AssetError(f"{tmx.name}: the start room's door is always locked, so it needs a keyspot with fixed = true")
    if kind == "test" and spawn is None:
        raise AssetError(f"{tmx.name}: no object named 'spawn' (the player's start point)")
    if allow_lock and kind == "normal" and count("drawer") + count("keyspot") == 0:
        raise AssetError(f"{tmx.name}: allow_lock is on but there is nowhere to put the key "
                         "(add a 'drawer' or 'keyspot' object): a locked door with no key ends the run")
    if count("drawer") > 8:
        raise AssetError(f"{tmx.name}: at most 8 drawers per room (their state is one byte)")
    if count("closet") + count("bed") > 8:
        raise AssetError(f"{tmx.name}: at most 8 closets and beds together per room (Hide's bans are one byte)")

    # Rush and Ambush's route through the room: entry door to exit door (start room: spawn to exit), as
    # straight lines between door centres (Minecraft docs 04), in pixels
    def centre(o):
        return (o[1] * 16 + o[3] * 8, o[2] * 16 + o[4] * 8)
    # One route per exit, in object order (a Dupe room's real exit is picked per run)
    routes = []
    entry_obj = next((o for o in objects if o[0] == OBJ_TYPES["entry"]), None)
    for exit_obj in (o for o in objects if o[0] == OBJ_TYPES["exit"]):
        if entry_obj or spawn:
            a = centre(entry_obj) if entry_obj else (spawn[0] // 8 * 8, spawn[1] // 8 * 8)
            routes.append((a, centre(exit_obj)))
    # Spawn objects are only used by start/test rooms; keep them out of the object list's way
    objects.sort(key=lambda o: o[0])
    fires = sum(1 for m in cells if ts_props[m].get("fire"))
    if fires and kind != "seek_end":
        print(f"note: {tmx.name} has {fires} fire metatiles (deadly) outside an end hallway")
    return dict(sym=ident(tmx.stem), w=w, h=h, cells=cells, ts_name=ts_name, kind=kind, allow_lock=allow_lock,
                objects=objects, spawn=spawn or (0, 0), entry_wall=entry_wall, exit_wall=exit_wall,
                exit_walls=exit_walls,
                drawers=count("drawer"), keyspots=sum(1 for o in objects if o[0] == OBJ_TYPES["keyspot"] and not o[5]),
                hiding=count("closet") + count("bed"), routes=routes, books=books, snare_cells=len(snare_cells))


def convert_room(tmx: Path, out: Path):
    r = parse_room(tmx)
    sym, w, h = r["sym"], r["w"], r["h"]
    objs = "\n".join("    { " + ", ".join(str(v) for v in o) + " }," for o in r["objects"])
    hd = [f"// Generated by scripts/assets.py from {tmx.as_posix()}. Do not edit.",
          f"#ifndef ROOM_{sym.upper()}_H", f"#define ROOM_{sym.upper()}_H", '#include "assets.h"',
          f"BANKREF_EXTERN(room_{sym})", f"extern const room_def_t room_{sym};", "#endif"]
    c = [f"// Generated by scripts/assets.py from {tmx.as_posix()}. Do not edit.",
         "#pragma bank 255", f'#include "room_{sym}.h"', f"BANKREF(room_{sym})",
         f"static const uint8_t map[] = {{\n{c_bytes(r['cells'], w if w <= 32 else 16)}\n}};",
         "// type, x, y, w, h, param (metatiles; OBJ_* in assets.h)",
         f"static const room_obj_t objects[] = {{\n{objs or '    { 0, 0, 0, 0, 0, 0 },'}\n}};",
         f"const room_def_t room_{sym} = {{ {w}, {h}, TILESET_{ident(r['ts_name']).upper()}, "
         f"{r['spawn'][0]}, {r['spawn'][1]}, map, {len(r['objects'])}, objects }};"]
    write(out, f"room_{sym}", hd, c)
    print(f"{tmx.name}: {w}x{h} metatiles, {r['kind']}, {len(r['objects'])} objects")


def convert_roomlist(out: Path, tmxs):
    """All room templates in one table the generator (src/run.c) can read without switching banks."""
    rooms = [parse_room(t) for t in sorted(tmxs, key=lambda p: p.stem)]
    starts = [r for r in rooms if r["kind"] == "start"]
    if len(starts) != 1:
        raise AssetError(f"assets/rooms: exactly one room must have kind = start, found {len(starts)}")
    normal = [r for r in rooms if r["kind"] == "normal"]
    # Every exit must lead somewhere: an exit on the top wall needs a room entered from the bottom, etc.
    needed = {WALLS["top"]: WALLS["bottom"], WALLS["left"]: WALLS["right"], WALLS["right"]: WALLS["left"]}
    names = {v: k for k, v in WALLS.items()}
    for r in starts + normal:
        for wall in r["exit_walls"]:
            if not any(n["entry_wall"] == needed[wall] for n in normal):
                raise AssetError(f"{r['sym']}.tmx exits through the {names[wall]} wall, but no normal room "
                                 f"is entered from the {names[needed[wall]]} wall")
    if sum(1 for r in rooms if r["kind"] == "halt") > 1:
        raise AssetError("assets/rooms: at most one room can have kind = halt")
    libraries = [r for r in rooms if r["kind"] == "library"]
    if len(libraries) > 1:
        raise AssetError("assets/rooms: at most one room can have kind = library")
    for lib in libraries:
        # Room 49 (src/run.c) must lead into it: for every wall a room can be entered from, a normal room with
        # one exit, on the wall the Library is entered through's opposite
        want = {WALLS["bottom"]: WALLS["top"]}[lib["entry_wall"]]
        for wall in sorted({n["entry_wall"] for n in normal}):
            if not any(n["entry_wall"] == wall and n["exit_walls"] == [want] for n in normal):
                raise AssetError(f"assets/rooms: the room before the Library needs one door, on the top wall, but no "
                                 f"normal room entered from the {names[wall]} wall has only that")
        if not any(n["entry_wall"] == needed[lib["exit_walls"][0]] for n in normal):
            raise AssetError(f"{lib['sym']}.tmx: no normal room is entered from the bottom wall to follow it")
    # Seek's chases: all three kinds or none (Minecraft docs 08: a chase with no end hallway is worse than none)
    pools = {k: [r for r in rooms if r["kind"] == k] for k in SEEK_KINDS}
    if any(pools.values()) and not all(pools.values()):
        raise AssetError("assets/rooms: a chase needs seek_start, seek_4way and seek_end rooms; found only "
                         + ", ".join(k for k in SEEK_KINDS if pools[k]))
    if all(pools.values()):
        def follows(after, into, what):
            for r in after:
                for wall in r["exit_walls"]:
                    if not any(n["entry_wall"] == needed[wall] for n in into):
                        raise AssetError(f"{r['sym']}.tmx exits through the {names[wall]} wall, but no {what} is "
                                         f"entered from the {names[needed[wall]]} wall")
        follows(pools["seek_start"], pools["seek_4way"], "seek_4way room")
        follows(pools["seek_4way"], pools["seek_4way"], "seek_4way room")
        follows(pools["seek_4way"], pools["seek_end"], "seek_end room")
        follows(pools["seek_end"], normal, "normal room")
    # The Courtyard and the Greenhouse: both or neither (without them rooms 89-99 are ordinary rooms)
    garden = {k: [r for r in rooms if r["kind"] == k] for k in GARDEN_KINDS}
    if any(garden.values()) and not all(garden.values()):
        raise AssetError("assets/rooms: rooms 89-99 need courtyard and greenhouse rooms; found only "
                         + ", ".join(k for k in GARDEN_KINDS if garden[k]))
    if all(garden.values()):
        def leads(after, into, what):
            for r in after:
                for wall in r["exit_walls"]:
                    if not any(n["entry_wall"] == needed[wall] for n in into):
                        raise AssetError(f"{r['sym']}.tmx exits through the {names[wall]} wall, but no {what} is "
                                         f"entered from the {names[needed[wall]]} wall")
        leads(garden["courtyard"], garden["greenhouse"], "greenhouse room")
        leads(garden["greenhouse"], garden["greenhouse"], "greenhouse room")
        leads(garden["greenhouse"], normal, "normal room (room 100)")
    if all(pools.values()) or all(garden.values()):
        # The room before a chase's start hallway and before the Courtyard: one door, on top (as the Library's)
        for wall in sorted({n["entry_wall"] for n in normal}):
            if not any(n["entry_wall"] == wall and n["exit_walls"] == [WALLS["top"]] for n in normal):
                raise AssetError(f"assets/rooms: the room before a chase or the Courtyard needs one door, on the "
                                 f"top wall, but no normal room entered from the {names[wall]} wall has only that")
    h = ["// Generated by scripts/assets.py from assets/rooms/*.tmx. Do not edit.",
         "#ifndef ROOM_TEMPLATES_H", "#define ROOM_TEMPLATES_H", '#include "assets.h"',
         f"#define ROOM_TEMPLATE_COUNT {len(rooms)}",
         f"#define ROOM_HAS_SEEK {1 if all(pools.values()) else 0}   // seek_start, seek_4way and seek_end rooms exist",
         f"#define ROOM_HAS_GARDEN {1 if all(garden.values()) else 0}   // courtyard and greenhouse rooms exist"]
    for i, r in enumerate(rooms):
        h.append(f"#define ROOM_T_{r['sym'].upper()} {i}")
    h += [f"#define ROOM_EXITS_MAX {EXITS_MAX}",
          "// Per template: kind, entry wall, first exit's wall, flags (bit 0 allow_lock), drawers, free key",
          "// spots, closets + beds",
          "extern const uint8_t room_template_info[ROOM_TEMPLATE_COUNT][7];",
          "// Each exit's wall, in object order (0: no such exit). More than one: a Dupe room.",
          "extern const uint8_t room_template_exits[ROOM_TEMPLATE_COUNT][ROOM_EXITS_MAX];",
          "// Rush's route through each template, one per exit: entry x, y, exit x, y in units of 8 px (door",
          "// centres), and its length in pixels",
          "extern const uint8_t room_template_route[ROOM_TEMPLATE_COUNT][ROOM_EXITS_MAX][4];",
          "extern const uint16_t room_template_length[ROOM_TEMPLATE_COUNT][ROOM_EXITS_MAX];",
          "// The template's data and its ROM bank",
          "const room_def_t *room_template(uint8_t id, uint8_t *bank);", "#endif"]
    c = ["// Generated by scripts/assets.py from assets/rooms/*.tmx. Do not edit.",
         '#include "room_templates.h"'] + [f'#include "room_{r["sym"]}.h"' for r in rooms]
    c.append("const uint8_t room_template_info[ROOM_TEMPLATE_COUNT][7] = {")
    for r in rooms:
        c.append(f"    {{ {ROOM_KINDS[r['kind']]}, {r['entry_wall']}, {r['exit_wall']}, {1 if r['allow_lock'] else 0}, "
                 f"{r['drawers']}, {r['keyspots']}, {r['hiding']} }},  // {r['sym']}")
    c.append("};")
    c.append("const uint8_t room_template_exits[ROOM_TEMPLATE_COUNT][ROOM_EXITS_MAX] = {")
    for r in rooms:
        walls = (r["exit_walls"] + [0] * EXITS_MAX)[:EXITS_MAX]
        c.append("    { " + ", ".join(map(str, walls)) + f" }},  // {r['sym']}")
    c.append("};")
    c.append("const uint8_t room_template_route[ROOM_TEMPLATE_COUNT][ROOM_EXITS_MAX][4] = {")
    lengths = []
    for r in rooms:
        parts, lens = [], []
        for k in range(EXITS_MAX):
            (ax, ay), (bx, by) = r["routes"][k] if k < len(r["routes"]) else ((0, 0), (0, 0))
            parts.append(f"{{ {ax // 8}, {ay // 8}, {bx // 8}, {by // 8} }}")
            lens.append(max(1, round(((bx - ax) ** 2 + (by - ay) ** 2) ** 0.5)))
        c.append("    { " + ", ".join(parts) + f" }},  // {r['sym']}")
        lengths.append("{ " + ", ".join(map(str, lens)) + " }")
    c.append("};")
    c.append("const uint16_t room_template_length[ROOM_TEMPLATE_COUNT][ROOM_EXITS_MAX] = {\n    "
             + ",\n    ".join(lengths) + " };")
    c += [ "const room_def_t *room_template(uint8_t id, uint8_t *bank) {", "    switch (id) {"]
    for i, r in enumerate(rooms):
        c.append(f"    case {i}: *bank = BANK(room_{r['sym']}); return &room_{r['sym']};")
    c += [f"    default: *bank = BANK(room_{starts[0]['sym']}); return &room_{starts[0]['sym']};", "    }", "}"]
    write(out, "room_templates", h, c)
    print(f"room list: {len(rooms)} templates ({len(normal)} normal)")


# ---------------------------------------------------------------------------------------------- font

def convert_font(js: Path, out: Path):
    meta = json.loads(js.read_text(encoding="utf-8"))
    chars = meta["chars"]
    if len(chars) > FONT_MAX_TILES:
        raise AssetError(f"{js}: {len(chars)} characters; the font has room for {FONT_MAX_TILES}")
    gbc, dmg = load_pair(js.with_name(js.stem + ".gbc.png"))
    cols = gbc.size[0] // 8
    cells = [((i % cols) * 8, (i // cols) * 8) for i in range(len(chars))]
    colours = {rgb15(gbc.getpixel((x + i, y + j))) for x, y in cells for j in range(8) for i in range(8)}
    if len(colours) > 4:
        raise AssetError(f"{js.stem}.gbc.png: {len(colours)} colours; the font shares one 4-colour palette")
    pal = sorted(colours, key=luminance, reverse=True)
    gbc_tiles = b"".join(tile_2bpp([[pal.index(rgb15(gbc.getpixel((x + i, y + j)))) for i in range(8)]
                                    for j in range(8)]) for x, y in cells)
    dmg_tiles = b"".join(tile_2bpp([[dmg_shade(dmg.getpixel((x + i, y + j)), f"{js.stem}.dmg.png ({x + i}, {y + j})")
                                     for i in range(8)] for j in range(8)]) for x, y in cells)
    # ASCII 32-127 -> tile number; lower case maps to upper case, unknown characters to the first (space)
    amap = []
    for code in range(32, 128):
        ch = chr(code).upper()
        amap.append(FONT_FIRST_TILE + (chars.index(ch) if ch in chars else 0))
    h = [f"// Generated by scripts/assets.py from {js.as_posix()}. Do not edit.",
         "#ifndef FONT_H", "#define FONT_H", '#include "assets.h"', "BANKREF_EXTERN(font)",
         "extern const font_t font;", f"#define FONT_FIRST_TILE {FONT_FIRST_TILE}", "#endif"]
    c = [f"// Generated by scripts/assets.py from {js.as_posix()}. Do not edit.",
         "#pragma bank 255", '#include "font.h"', "BANKREF(font)",
         "#ifdef CGB", f"static const uint8_t tiles[] = {{\n{c_bytes(gbc_tiles)}\n}};",
         f"static const palette_color_t palette[] = {{\n{c_palettes([pal])}\n}};",
         "#else", f"static const uint8_t tiles[] = {{\n{c_bytes(dmg_tiles)}\n}};", "#endif",
         "// Tile for each ASCII code from 32 to 127",
         f"static const uint8_t ascii[] = {{\n{c_bytes(amap)}\n}};",
         f"const font_t font = {{ {len(chars)}, tiles, ascii,", "#ifdef CGB", "    palette,", "#endif", "};"]
    write(out, "font", h, c)
    print(f"{js.name}: {len(chars)} characters")


# ---------------------------------------------------------------------------------------------- sprite

def convert_sprite(js: Path, out: Path):
    meta = json.loads(js.read_text(encoding="utf-8"))
    sym = ident(js.stem)
    fw, fh = meta.get("frame_width", 16), meta.get("frame_height", 16)
    if fw % 16 or fh % 16 or not fw or not fh:
        raise AssetError(f"{js}: frames must be multiples of 16x16 (got {fw}x{fh})")
    names = meta["frames"]
    gbc, dmg = load_pair(js.with_name(js.stem + ".gbc.png"))
    cols = gbc.size[0] // fw
    if len(names) > cols * (gbc.size[1] // fh):
        raise AssetError(f"{js}: {len(names)} frames listed, the sheet has room for fewer")
    # A frame bigger than 16x16 is drawn as several 16x16 parts, left to right, top to bottom (spr_draw_big)
    pw, ph = fw // 16, fh // 16
    parts = pw * ph
    frames, origins = [], []
    for k, name in enumerate(names):
        for p in range(parts):
            frames.append(name if parts == 1 else f"{name} part {p}")
            origins.append(((k % cols) * fw + (p % pw) * 16, (k // cols) * fh + (p // pw) * 16))

    # Each 16x16 part is two 8x16 hardware sprites (left and right column)
    columns = []
    for f, (ox, oy) in enumerate(origins):
        columns += [(f, ox, oy), (f, ox + 8, oy)]

    def pixels(img, x, y):
        return [[img.getpixel((x + i, y + j)) for i in range(8)] for j in range(16)]

    # GBC
    sets = []
    for f, x, y in columns:
        s = {rgb15(p) for row in pixels(gbc, x, y) for p in row if p[3] >= 128}
        if len(s) > 3:
            raise AssetError(f"{js.stem}.gbc.png: frame {frames[f]}, column at x={x}: {len(s)} colours, "
                             "a GBC sprite has 3 plus transparency")
        sets.append(s)
    palettes, assign = pack_palettes(sets, 3, 8, f"{js.stem}.gbc.png")

    def column_data(px, mapper):
        if all(p is None for row in px for p in row):
            return None
        return tile_2bpp([[mapper(p) for p in row] for row in px[:8]]) + \
            tile_2bpp([[mapper(p) for p in row] for row in px[8:]])

    gbc_cols = []
    for (f, x, y), p in zip(columns, assign):
        px = [[(rgb15(c) if c[3] >= 128 else None) for c in row] for row in pixels(gbc, x, y)]
        gbc_cols.append((column_data(px, lambda c: 0 if c is None else palettes[p].index(c) + 1), p))

    # DMG: choose OBP0 or OBP1 per column
    dmg_cols = []
    for f, x, y in columns:
        where = f"{js.stem}.dmg.png frame {frames[f]}, column at x={x}"
        px = [[(dmg_shade(c, where) if c[3] >= 128 else None) for c in row] for row in pixels(dmg, x, y)]
        used = {s for row in px for s in row if s is not None}
        obp = next((o for o, shades in DMG_OBP.items() if used <= set(shades)), None)
        if obp is None:
            raise AssetError(f"{where}: uses white and light grey together; a DMG sprite can have only one")
        dmg_cols.append((column_data(px, lambda s: 0 if s is None else DMG_OBP[obp].index(s) + 1), obp))

    def build(cols_, attr_fn):
        tiles, index, table = [], {}, []
        for data, pal in cols_:
            if data is None:
                table += [0xFF, 0]
                continue
            if data not in index:
                index[data] = len(tiles) * 2
                tiles.append(data)
            table += [index[data], attr_fn(pal)]
        return b"".join(tiles), len(tiles) * 2, table

    g_tiles, g_count, g_table = build(gbc_cols, lambda p: p)            # CGB attr bits 0-2: palette
    d_tiles, d_count, d_table = build(dmg_cols, lambda o: o << 4)       # DMG attr bit 4: OBP1
    hd = [f"// Generated by scripts/assets.py from {js.as_posix()}. Do not edit.",
          f"#ifndef SPRITE_{sym.upper()}_H", f"#define SPRITE_{sym.upper()}_H", '#include "assets.h"',
          f"BANKREF_EXTERN(sprite_{sym})", f"extern const sprite_sheet_t sprite_{sym};"]
    for i, n in enumerate(names):
        hd.append(f"#define SPR_{sym.upper()}_{ident(n).upper()} {i * parts}")
    if parts > 1:
        hd += [f"#define SPR_{sym.upper()}_PARTS_W {pw}", f"#define SPR_{sym.upper()}_PARTS_H {ph}"]
    hd += [f"#define SPR_{sym.upper()}_FRAME_COUNT {len(frames)}   // 16x16 parts, for the frame buffer",
           "// Its VRAM, for the build-time sprite budgets (game.c, halt.c, library.c)",
           "#ifdef CGB", f"#define SPR_{sym.upper()}_TILES {g_count}", "#else", f"#define SPR_{sym.upper()}_TILES {d_count}",
           "#endif", f"#define SPR_{sym.upper()}_PALETTES {len(palettes)}   // GBC", "#endif"]
    c = [f"// Generated by scripts/assets.py from {js.as_posix()}. Do not edit.",
         "#pragma bank 255", f'#include "sprite_{sym}.h"', f"BANKREF(sprite_{sym})",
         "// Per frame: left column tile, attributes, right column tile, attributes (tile 0xFF = empty)",
         "#ifdef CGB",
         f"static const uint8_t tiles[] = {{\n{c_bytes(g_tiles)}\n}};",
         f"static const uint8_t frames[] = {{\n{c_bytes(g_table, 4)}\n}};",
         f"static const palette_color_t palettes[] = {{\n{c_palettes(palettes, transparent_first=True)}\n}};",
         f"#define TILE_COUNT {g_count}", f"#define PALETTE_COUNT {len(palettes)}",
         "#else",
         f"static const uint8_t tiles[] = {{\n{c_bytes(d_tiles)}\n}};",
         f"static const uint8_t frames[] = {{\n{c_bytes(d_table, 4)}\n}};",
         f"#define TILE_COUNT {d_count}",
         "#endif",
         f"const sprite_sheet_t sprite_{sym} = {{",
         f"    TILE_COUNT, tiles, {len(frames)}, frames,",
         "#ifdef CGB", "    PALETTE_COUNT, palettes,", "#endif", "};"]
    write(out, f"sprite_{sym}", hd, c)
    print(f"{js.name}: {len(names)} frames of {fw}x{fh}, GBC {g_count} tiles / {len(palettes)} palettes, DMG {d_count} tiles")


# ---------------------------------------------------------------------------------------------- image

def convert_image(png_gbc: Path, out: Path):
    """A still picture (the title logo): its own tiles, a tile map and, on GBC, per-tile palettes.
    Loaded over the tileset's VRAM while it's shown, so it can use up to TILESET_MAX_TILES tiles."""
    sym = ident(png_gbc.name.replace(".gbc.png", ""))
    gbc, dmg = load_pair(png_gbc)
    w, h = gbc.size
    if w % 8 or h % 8:
        raise AssetError(f"{png_gbc.name}: {w}x{h}; width and height must be multiples of 8")
    cells = [(x, y) for y in range(0, h, 8) for x in range(0, w, 8)]
    sets = []
    for x, y in cells:
        s = {rgb15(gbc.getpixel((x + i, y + j))) for j in range(8) for i in range(8)}
        if len(s) > 4:
            raise AssetError(f"{png_gbc.name}: 8x8 tile at ({x}, {y}) has {len(s)} colours; the GBC allows 4")
        sets.append(s)
    palettes, assign = pack_palettes(sets, 4, TILESET_MAX_PALETTES, png_gbc.name)

    def build(datas):
        tiles, index, refs = [], {}, []
        for d in datas:
            if d not in index:
                index[d] = len(tiles)
                tiles.append(d)
            refs.append(index[d])
        if len(tiles) > TILESET_MAX_TILES:
            raise AssetError(f"{png_gbc.name}: {len(tiles)} unique tiles; at most {TILESET_MAX_TILES}")
        return tiles, refs

    g_tiles, g_map = build([tile_2bpp([[palettes[p].index(rgb15(gbc.getpixel((x + i, y + j)))) for i in range(8)]
                                       for j in range(8)]) for (x, y), p in zip(cells, assign)])
    d_tiles, d_map = build([tile_2bpp([[dmg_shade(dmg.getpixel((x + i, y + j)), f"{png_gbc.name.replace('.gbc', '.dmg')} ({x + i}, {y + j})")
                                        for i in range(8)] for j in range(8)]) for x, y in cells])
    hd = [f"// Generated by scripts/assets.py from {png_gbc.as_posix()}. Do not edit.",
          f"#ifndef IMAGE_{sym.upper()}_H", f"#define IMAGE_{sym.upper()}_H", '#include "assets.h"',
          f"BANKREF_EXTERN(image_{sym})", f"extern const image_t image_{sym};", "#endif"]
    c = [f"// Generated by scripts/assets.py from {png_gbc.as_posix()}. Do not edit.",
         "#pragma bank 255", f'#include "image_{sym}.h"', f"BANKREF(image_{sym})",
         "#ifdef CGB",
         f"static const uint8_t tiles[] = {{\n{c_bytes(b''.join(g_tiles))}\n}};",
         f"static const uint8_t map[] = {{\n{c_bytes(g_map, w // 8)}\n}};",
         f"static const uint8_t attrs[] = {{\n{c_bytes(assign, w // 8)}\n}};",
         f"static const palette_color_t palettes[] = {{\n{c_palettes(palettes)}\n}};",
         f"#define TILE_COUNT {len(g_tiles)}", f"#define PALETTE_COUNT {len(palettes)}",
         "#else",
         f"static const uint8_t tiles[] = {{\n{c_bytes(b''.join(d_tiles))}\n}};",
         f"static const uint8_t map[] = {{\n{c_bytes(d_map, w // 8)}\n}};",
         f"#define TILE_COUNT {len(d_tiles)}",
         "#endif",
         f"const image_t image_{sym} = {{ {w // 8}, {h // 8}, TILE_COUNT, tiles, map,",
         "#ifdef CGB", "    attrs, PALETTE_COUNT, palettes,", "#endif", "};"]
    write(out, f"image_{sym}", hd, c)
    print(f"{png_gbc.name}: {w // 8}x{h // 8} tiles, GBC {len(g_tiles)} unique / {len(palettes)} palettes, "
          f"DMG {len(d_tiles)} unique")


# ---------------------------------------------------------------------------------------------- songs

def fix_song(src: Path, out: Path):
    """Post-processes uge2source's C (hUGETracker 1.0.11): a song that uses no instruments of a kind gets an
    empty array, `{ }`, which SDCC rejects ("empty initializer requires ISO C23"). Gives it one unused
    entry instead. Everything else is copied unchanged."""
    text = src.read_text(encoding="utf-8")
    fixed = re.sub(r"(static const hUGE\w+Instr_t \w+\[\] = \{)\s*\};", r"\1 {0, 0, 0, 0, 0} };", text)
    out.write_text(fixed, encoding="utf-8")
    src.unlink()


# ---------------------------------------------------------------------------------------------- output

def write(out: Path, stem: str, header, source):
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{stem}.h").write_text("\n".join(header) + "\n", encoding="utf-8")
    (out / f"{stem}.c").write_text("\n".join(source) + "\n", encoding="utf-8")


def main():
    kinds = {"tileset": convert_tileset, "room": convert_room, "sprite": convert_sprite, "font": convert_font,
             "image": convert_image, "songfix": fix_song}
    try:
        if len(sys.argv) >= 3 and sys.argv[1] == "roomlist":
            convert_roomlist(Path(sys.argv[2]), [Path(p) for p in sys.argv[3:]])
            return
        if len(sys.argv) != 4 or sys.argv[1] not in kinds:
            sys.exit(__doc__)
        kinds[sys.argv[1]](Path(sys.argv[2]), Path(sys.argv[3]))
    except AssetError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
