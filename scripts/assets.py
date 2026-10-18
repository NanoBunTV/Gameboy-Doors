"""Converts the art and Tiled files in assets/ into C source for GBDK. Run by the Makefile.

    python scripts/assets.py tileset assets/tilesets/hotel.tsx build/gen
    python scripts/assets.py room    assets/rooms/test.tmx     build/gen
    python scripts/assets.py sprite  assets/sprites/player.json build/gen

Every graphic exists twice (owner's decision, Docs/PROJECT.md): `<name>.gbc.png` in colour for the GBC ROM
and `<name>.dmg.png` in the four DMG greys, with the same layout. The generated .c file holds both versions
behind `#ifdef CGB`, so each ROM only links its own.

Rules the art has to follow (the converter stops with a message naming the place that breaks one):
  GBC background: at most 4 colours per 8x8 tile, at most 8 palettes for the whole tileset.
  GBC sprites:    at most 3 colours (+ transparency) per 8x16 hardware sprite, at most 8 palettes.
  DMG:            only the 4 greys (FF, AA, 55, 00); a sprite column uses white or light grey, not both.
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
    palettes, assign = pack_palettes(colour_sets, 4, 8, image.name)

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
        if len(tiles) > 256:
            raise AssetError(f"{tsx.name}: {len(tiles)} unique {label} tiles; the background has room for 256")

    flags = [(1 if props[m].get("solid") else 0) for m in range(count)]
    names = [props[m].get("name", f"mt{m}") for m in range(count)]

    h = [f"// Generated by scripts/assets.py from {tsx.as_posix()}. Do not edit.",
         f"#ifndef TILESET_{sym.upper()}_H", f"#define TILESET_{sym.upper()}_H", '#include "assets.h"',
         f"BANKREF_EXTERN(tileset_{sym})", f"extern const tileset_t tileset_{sym};"]
    for m, n in enumerate(names):
        h.append(f"#define MT_{sym.upper()}_{ident(n).upper()} {m}")
    h.append("#endif")
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

def convert_room(tmx: Path, out: Path):
    root = ET.parse(tmx).getroot()
    sym = ident(tmx.stem)
    w, h = int(root.get("width")), int(root.get("height"))
    if root.get("infinite") == "1":
        raise AssetError(f"{tmx}: infinite maps aren't supported")
    if w > 127 or h > 127 or w * h > 3072:
        raise AssetError(f"{tmx}: {w}x{h} is too big (max 127 per side and 3072 metatiles; ROOM_MAX in room.h)")
    ts = root.findall("tileset")
    if len(ts) != 1:
        raise AssetError(f"{tmx}: a room uses exactly one tileset")
    firstgid = int(ts[0].get("firstgid"))
    tsx = (tmx.parent / ts[0].get("source")).resolve()
    ts_name, _, _, ts_count, _ = tsx_tiles(tsx)
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
    spawn = None
    for obj in root.iter("object"):
        if obj.get("name") == "spawn" or obj.get("type") == "spawn" or obj.get("class") == "spawn":
            spawn = (int(float(obj.get("x"))), int(float(obj.get("y"))))
    if spawn is None:
        raise AssetError(f"{tmx}: no object named 'spawn' (the player's start point)")

    hd = [f"// Generated by scripts/assets.py from {tmx.as_posix()}. Do not edit.",
          f"#ifndef ROOM_{sym.upper()}_H", f"#define ROOM_{sym.upper()}_H", '#include "assets.h"',
          f"BANKREF_EXTERN(room_{sym})", f"extern const room_def_t room_{sym};", "#endif"]
    c = [f"// Generated by scripts/assets.py from {tmx.as_posix()}. Do not edit.",
         "#pragma bank 255", f'#include "room_{sym}.h"', f"BANKREF(room_{sym})",
         f"static const uint8_t map[] = {{\n{c_bytes(cells, w if w <= 32 else 16)}\n}};",
         f"const room_def_t room_{sym} = {{ {w}, {h}, TILESET_{ident(ts_name).upper()}, "
         f"{spawn[0]}, {spawn[1]}, map }};"]
    write(out, f"room_{sym}", hd, c)
    print(f"{tmx.name}: {w}x{h} metatiles, tileset {ts_name}, spawn {spawn}")


# ---------------------------------------------------------------------------------------------- sprite

def convert_sprite(js: Path, out: Path):
    meta = json.loads(js.read_text(encoding="utf-8"))
    sym = ident(js.stem)
    fw, fh = meta.get("frame_width", 16), meta.get("frame_height", 16)
    if (fw, fh) != (16, 16):
        raise AssetError(f"{js}: only 16x16 frames are supported so far")
    frames = meta["frames"]
    gbc, dmg = load_pair(js.with_name(js.stem + ".gbc.png"))
    cols = gbc.size[0] // fw
    if len(frames) > cols * (gbc.size[1] // fh):
        raise AssetError(f"{js}: {len(frames)} frames listed, the sheet has room for fewer")

    # Each frame is two 8x16 hardware sprites (left and right column)
    columns = []
    for f in range(len(frames)):
        ox, oy = (f % cols) * fw, (f // cols) * fh
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
    for i, n in enumerate(frames):
        hd.append(f"#define SPR_{sym.upper()}_{ident(n).upper()} {i}")
    hd += [f"#define SPR_{sym.upper()}_FRAME_COUNT {len(frames)}", "#endif"]
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
    print(f"{js.name}: {len(frames)} frames, GBC {g_count} tiles / {len(palettes)} palettes, DMG {d_count} tiles")


# ---------------------------------------------------------------------------------------------- output

def write(out: Path, stem: str, header, source):
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{stem}.h").write_text("\n".join(header) + "\n", encoding="utf-8")
    (out / f"{stem}.c").write_text("\n".join(source) + "\n", encoding="utf-8")


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("tileset", "room", "sprite"):
        sys.exit(__doc__)
    kind, src, out = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
    try:
        {"tileset": convert_tileset, "room": convert_room, "sprite": convert_sprite}[kind](src, out)
    except AssetError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main()
