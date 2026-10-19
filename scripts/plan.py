"""A copy of src/run.c's planner in Python, for choosing AUTOTEST_SEED (Docs/DEBUGGING.md).

    tools/venv/Scripts/python.exe scripts/plan.py build/gen/room_templates.c          # search seeds
    tools/venv/Scripts/python.exe scripts/plan.py build/gen/room_templates.c 1331     # one seed's rooms

It reads the template tables the converter wrote, and must be kept in step with plan_room() in src/run.c:
the same draws in the same order. Check a seed against the ROM (scripts/bot.py prints the templates it
saw). The search keeps seeds whose first 30 rooms have Halt's door by 22, a Dupe room by 12, a wrong door
without Dupe, a dark room, Eyes, Ambush and Rush; edit the conditions at the bottom for other needs."""
import re, sys

src = open(sys.argv[1]).read()
names = re.findall(r'\},\s*// (\w+)', src.split('room_template_info')[1].split('};')[0])
info = [list(map(int, m)) for m in re.findall(r'\{ (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+) \}',
                                               src.split('room_template_info')[1].split('};')[0])]
exits = [list(map(int, m)) for m in re.findall(r'\{ (\d+), (\d+), (\d+) \}',
                                                src.split('room_template_exits')[1].split('};')[0])]
TOP, BOTTOM, LEFT, RIGHT = 1, 2, 3, 4
OPP = {TOP: BOTTOM, LEFT: RIGHT, RIGHT: LEFT, BOTTOM: TOP}
M = 0xFFFF
LIBRARY = 50   # run.h RUN_LIBRARY_ROOM
CHASES = [(30, 35), (70, 79)]   # run.h RUN_CHASE_START, RUN_CHASE_END
HAS_SEEK = all(any(x[0] == k for x in info) for k in (5, 6, 7))   # room_templates.h ROOM_HAS_SEEK


def kind_at(n):
    """run.c run_kind_at"""
    if n == 0:
        return 1
    if n == LIBRARY:
        return 4
    for s, e in CHASES if HAS_SEEK else []:
        if n == s:
            return 5
        if s < n < e:
            return 6
        if n == e:
            return 7
    return 0


class Plan:
    def __init__(self, seed):
        self.seed = seed
        self.t, self.flags, self.f2, self.exit, self.slot = [], [], [], [], []
        self.halt_planned = False

    def seed_rng(self, s, n):
        r = (s ^ ((n * 0x9E37) & M) ^ 0x5A5A) & M
        if not r:
            r = 1
        self.rng = r
        for _ in range(4):
            self.rnd()

    def rnd(self):
        r = self.rng
        r ^= (r << 7) & M
        r ^= r >> 9
        r ^= (r << 8) & M
        self.rng = r
        return r

    def below(self, n):
        return self.rnd() % n

    def room(self, n):
        self.seed_rng(self.seed, n)
        kind = kind_at(n)
        chase = kind >= 5
        before_fixed = n < 100 and kind_at(n + 1) in (4, 5)
        if kind in (1, 4):
            t = [i for i, x in enumerate(info) if x[0] == kind][-1]
        else:
            need = OPP[exits[self.t[n - 1]][self.exit[n - 1]]]
            cands = []
            for p in range(2):
                for i, x in enumerate(info):
                    if x[0] != kind or x[1] != need:
                        continue
                    if before_fixed and (exits[i][0] != TOP or exits[i][1]):
                        continue
                    recent = p == 0 and not chase and any(self.t[n - b] == i for b in range(1, 4) if b <= n)
                    if not recent:
                        cands.append(i)
                if cands:
                    break
            t = cands[self.below(len(cands))]
        self.t.append(t)
        x = info[t]
        flags, slot = 0, 0
        if n == 100:
            flags = 0x10
        elif x[0] == 1:
            flags = 0x21
        elif x[0] == 4:
            flags = 1
        elif x[3] and (x[4] or x[5]) and self.below(100) < 18:
            flags = 1
            drawer = bool(x[4]) and (not x[5] or self.below(100) < 75)
            if drawer:
                flags |= 2
                slot = self.below(x[4])
            else:
                slot = self.below(x[5])
        lib = n == LIBRARY or chase
        if not lib and 3 <= n < 100 and self.below(100) < 18:
            flags |= 0x40
            if self.below(100) < 10:
                flags |= 0x80
        f2 = 0
        if not lib and 15 <= n < 100 and self.below(100) < 10:
            f2 |= 1
        if not lib and 8 <= n < 100 and self.below(100) < 5:
            f2 |= 2
        ne = sum(1 for w in exits[t] if w)
        real = 0
        if ne > 1:
            real = self.below(ne)
            if not chase and 6 <= n < 100 and self.below(100) < 35:
                f2 |= 8
        elif (not self.halt_planned and not lib and not before_fixed and 10 <= n < 100 and not (flags & 1)
              and self.below(100) < 3):
            f2 |= 0x40
            self.halt_planned = True
        self.flags.append(flags)
        self.f2.append(f2)
        self.exit.append(real)
        self.slot.append(slot)


def plan(seed, upto=40):
    p = Plan(seed)
    for n in range(upto + 1):
        p.room(n)
    return p


def describe(p, upto=32):
    for n in range(upto + 1):
        bits = []
        if p.flags[n] & 1: bits.append('locked')
        if p.flags[n] & 0x40: bits.append('AMBUSH' if p.flags[n] & 0x80 else 'rush')
        if p.f2[n] & 1: bits.append('dark')
        if p.f2[n] & 2: bits.append('eyes')
        if p.f2[n] & 8: bits.append('DUPE')
        if p.f2[n] & 0x40: bits.append('HALT')
        ne = sum(1 for w in exits[p.t[n]] if w)
        print(f"{n:3} {names[p.t[n]]:12} exit {p.exit[n]}/{ne} {' '.join(bits)}")


if len(sys.argv) > 2:
    describe(plan(int(sys.argv[2], 16)))
else:
    best = []
    for s in range(1, 0x10000):
        p = plan(s, 30)
        R = range(1, 30)
        halt = [n for n in R if p.f2[n] & 0x40]
        dupe = [n for n in R if p.f2[n] & 8]
        multi = [n for n in R if sum(1 for w in exits[p.t[n]] if w) > 1 and not p.f2[n] & 8]
        dark = [n for n in R if p.f2[n] & 1]
        eyes = [n for n in R if p.f2[n] & 2]
        amb = [n for n in R if p.flags[n] & 0x80]
        rush = [n for n in R if p.flags[n] & 0x40 and not p.flags[n] & 0x80]
        if halt and halt[0] <= 22 and dupe and dupe[0] <= 12 and multi and dark and eyes and amb and rush:
            score = halt[0] + dupe[0] + len(dark) * -2
            best.append((score, s, halt, dupe, multi, dark, eyes, amb))
    best.sort()
    for b in best[:12]:
        print(hex(b[1]), 'halt', b[2], 'dupe', b[3], 'fake', b[4], 'dark', b[5], 'eyes', b[6], 'ambush', b[7])
    print(len(best), 'seeds')
