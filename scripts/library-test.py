"""The Library (room 50, Phase 6) in PyBoy: the puzzle, sneaking, and Figure.

    tools/venv/Scripts/python.exe scripts/library-test.py build/gbc-debug/doors.gbc
    tools/venv/Scripts/python.exe scripts/library-test.py build/dmg-debug/doors.gb

Needs a DEBUG or AUTOTEST build (game_warp, which the script pokes to go straight to room 50). Three runs,
each from a fresh boot:
  puzzle  Figure switched off (figure_state poked to 0): reads the paper and all eight books, checks the
          notes, dials the code from the books (library.c's lib_* tables), opens the padlock and walks out
          into room 51 (Figure and the music gone).
  sneak   after the grace, walks the length of the Library holding B: Figure must never start hunting.
  figure  walks without B: Figure must hear, hunt and kill (death cause 8, his jumpscare).
Screenshots go to build/screens/lib-<console>-*.png. Exits non-zero on any failure.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bot import Game, Bot, J, GAME_PLAYING, GAME_DEAD, OBJ_EXIT, RF_EXIT_OPEN  # noqa: E402

LIBRARY = 50
OBJ_PAPER = 8
SHAPES = "%[&$()^]"
ROOT = Path(__file__).resolve().parent.parent
FIG_OFF, FIG_FILL, FIG_WALK, FIG_PAUSE = 0, 1, 2, 3

rom = Path(sys.argv[1])
tag = "gbc" if rom.suffix == ".gbc" else "dmg"
failures = []


def check(cond, what):
    print(("  ok   " if cond else "  FAIL ") + what)
    if not cond:
        failures.append(what)


def boot():
    g = Game(rom)
    b = Bot(g)
    b.wait_playing()
    for _ in range(60):
        g.tick()
    g.mem[g.syms["_game_warp"]] = LIBRARY
    for _ in range(90):
        g.tick()
    return g, b


def shot(g, name):
    g.tick(0, render=True)
    g.pyboy.screen.image.save(ROOT / "build" / "screens" / f"lib-{tag}-{name}.png")


def hud_text(g):
    """The HUD's text row (window row 1), as characters"""
    tiles = [g.mem[0x9C20 + i] for i in range(20)]
    chars = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?-:'/_~@*+{}#>=|%[&$()^]"
    return "".join(chars[t - 192] if 192 <= t < 192 + len(chars) else "?" for t in tiles).strip()


def win_row(g, y):
    chars = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?-:'/_~@*+{}#>=|%[&$()^]"
    return "".join(chars[g.mem[0x9C00 + y * 32 + i] - 192] for i in range(20)).rstrip()


def face_and_a(b, face):
    for _ in range(3):
        b.step(J[face])
    b.step(0)
    b.press_a_until(lambda: False, tries=1)
    for _ in range(4):
        b.g.tick(0)


# ------------------------------------------------------------------------------------------ the puzzle
print(f"{rom}: puzzle")
g, b = boot()
check(g.room() == LIBRARY and g.state() == GAME_PLAYING, f"in room {LIBRARY}")
check(g.u8("_figure_state") != FIG_OFF, "Figure is there")
shot(g, "enter")
g.mem[g.syms["_figure_state"]] = FIG_OFF
books = [(g.u8("_lib_book_x", i), g.u8("_lib_book_y", i)) for i in range(8)]
shapes = [g.u8("_lib_book_shape", i) for i in range(8)]
digits = [g.u8("_lib_book_digit", i) for i in range(8)]
paper = [g.u8("_lib_paper", k) for k in range(5)]
code = [digits[p] for p in paper]
check(sorted(shapes) == list(range(8)), "eight different shapes")
check(len(set(books)) == 8, "eight different shelves")
print("  books", books, "code", code)
paper_obj = next(o for o in g.objects() if o[0] == OBJ_PAPER)
b.use(paper_obj)
want = " ".join(f"{k + 1}={SHAPES[shapes[paper[k]]]}" for k in range(5))
check(hud_text(g) == want, f"the paper says {want!r} (HUD: {hud_text(g)!r})")
shot(g, "paper")
for i, (x, y) in enumerate(books):
    if not b.walk_to((x, y + 1)):
        check(False, f"reached book {i} at {(x, y)}")
        continue
    face_and_a(b, "up")
    want = f"A BOOK SAYS: {SHAPES[shapes[i]]}={digits[i]}"
    check(hud_text(g) == want, f"book {i} says {want!r} (HUD: {hud_text(g)!r})")
    if i == 0:
        shot(g, "book")
def start_until(cond):
    """Taps Start until cond() holds (on DMG a tap can fall on a frame the game skipped reading the joypad)"""
    for _ in range(8):
        g.tick(J["start"])
        for _ in range(4):
            g.tick(0)
        if cond():
            return


start_until(lambda: g.u8("_hud_panel") > 0)
check(win_row(g, 5).strip() == "BOOKS  8 OF 8", f"notes: {win_row(g, 5).strip()!r}")
shot(g, "notes")
start_until(lambda: g.u8("_hud_panel") == 0)
exit_obj = next(o for o in g.objects() if o[0] == OBJ_EXIT)
cellpos, face = b.approach(exit_obj)
b.walk_to(cellpos)
face_and_a(b, face)
check(win_row(g, 2).strip() == "PADLOCK", "the padlock's panel is open")
# A wrong code first: the wheels go back to 0
g.tick(J["up"]); g.tick(0); g.tick(0)
g.tick(J["a"]); g.tick(0); g.tick(0)
check(win_row(g, 5).strip() == "0 0 0 0 0" and "WRONG" in hud_text(g), "a wrong code resets the wheels")
def wheels():
    return [int(c) for c in win_row(g, 5).split()]


for k, d in enumerate(code):
    # Each turn checked on the wheel (DMG: a one-frame tap can fall on a frame the game skipped reading the joypad)
    for _ in range(40):
        if wheels()[k] == d:
            break
        g.tick(J["up"])
        g.tick(0)
        g.tick(0)
    nxt = 5 + ((k + 1) % len(code)) * 2   # library.c draw_wheels: the cursor '^' under the wheel
    for _ in range(8):
        if len(win_row(g, 4)) > nxt and win_row(g, 4)[nxt] == "^":
            break
        g.tick(J["right"])
        g.tick(0)
        g.tick(0)
check(wheels() == code, f"the code on the wheels ({wheels()})")
shot(g, "padlock")
g.tick(J["a"]); g.tick(0); g.tick(0)
check(g.flags() & RF_EXIT_OPEN, "the code opens the padlock")
shot(g, "open")
b.walk_to((exit_obj[1], exit_obj[2] + 1))
for _ in range(40):
    g.tick(J["up"])
for _ in range(60):
    g.tick(0)
check(g.room() == LIBRARY + 1, f"walked out into room {LIBRARY + 1} (room {g.room()})")
check(g.u8("_figure_state") == FIG_OFF and g.u8("_library_left") == 1, "Figure stays behind")
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ sneaking
print(f"{rom}: sneak")
g, b = boot()
for _ in range(200):   # the grace (3 s) runs out
    g.tick(0)
hunted = 0
x0, y0 = g.feet()
for f in range(900):
    g.tick(J["b"] | (J["up"] if (f // 150) % 2 == 0 else J["down"]))
    hunted |= g.u8("_figure_hunting")
    if g.state() != GAME_PLAYING:
        break
# Wandering, he can still blunder into a player who isn't moving away (the Minecraft rule too: STATUS.md), so
# a contact death here isn't hearing; any other death is a failure
if g.state() != GAME_PLAYING:
    print(f"  note: he wandered into the player after {f} frames, without hearing them (cause {g.u8('_game_death')})")
check(not hunted and (g.state() == GAME_PLAYING or g.u8("_game_death") == 8), "sneaking (B held): never heard")
x1, y1 = g.feet()
g.pyboy.stop(save=False)

# ------------------------------------------------------------------------------------------ Figure
print(f"{rom}: figure")
g, b = boot()
for _ in range(200):
    g.tick(0)
died_at = near = None
for f in range(1800):
    g.tick(J["up"] if (f // 100) % 2 == 0 else J["down"])
    if f == 60:
        shot(g, "hunted")
    fx, fy = g.s16("_figure_x"), g.s16("_figure_y")
    px, py = g.feet()
    if near is None and abs(fx - px) < 48 and abs(fy - py) < 40:
        near = f
        shot(g, "near")
    if g.state() == GAME_DEAD:
        died_at = f
        break
check(died_at is not None and g.u8("_game_death") == 8, f"walking, Figure kills (after {died_at} frames)")
for _ in range(20):
    g.tick(0)
shot(g, "scare")
g.pyboy.stop(save=False)

print("all passed" if not failures else f"{len(failures)} failed")
sys.exit(1 if failures else 0)
