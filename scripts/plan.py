"""A copy of src/run.c's planner in Python, for choosing AUTOTEST_SEED (Docs/DEBUGGING.md).

    tools/venv/Scripts/python.exe scripts/plan.py build/gen/room_templates.c          # search seeds
    tools/venv/Scripts/python.exe scripts/plan.py build/gen/room_templates.c 1331     # one seed's rooms

It reads the template tables the converter wrote, and must be kept in step with plan_room() in src/run.c:
the same draws in the same order. Check a seed against the ROM (scripts/bot.py prints the templates it
saw). The search keeps seeds whose first 30 rooms have Halt's door by 22, a Dupe room by 12, a wrong door
without Dupe, a dark room, Eyes, Ambush and Rush; edit the conditions at the bottom for other needs."""
import re, sys

names, info, exits = [], [], []
HAS_SEEK = HAS_GARDEN = HAS_ELECTRICAL = HAS_SHOP = False


def load(path):
    """Reads the converter's template tables (build/gen/room_templates.c). scripts/garden-test.py imports this
    module and calls it."""
    global HAS_SEEK, HAS_GARDEN, HAS_ELECTRICAL, HAS_SHOP
    src = open(path).read()
    table = src.split('room_template_info')[1].split('};')[0]
    names[:] = re.findall(r'\},\s*// (\w+)', table)
    info[:] = [list(map(int, m)) for m in re.findall(r'\{ (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+) \}', table)]
    exits[:] = [list(map(int, m)) for m in re.findall(r'\{ (\d+), (\d+), (\d+) \}',
                                                     src.split('room_template_exits')[1].split('};')[0])]
    HAS_SEEK = all(any(x[0] == k for x in info) for k in (5, 6, 7))    # room_templates.h ROOM_HAS_SEEK
    HAS_GARDEN = all(any(x[0] == k for x in info) for k in (8, 9))     # room_templates.h ROOM_HAS_GARDEN
    HAS_ELECTRICAL = any(x[0] == 10 for x in info)                     # room_templates.h ROOM_HAS_ELECTRICAL
    HAS_SHOP = any(x[0] == 11 for x in info)                           # room_templates.h ROOM_HAS_SHOP


TOP, BOTTOM, LEFT, RIGHT = 1, 2, 3, 4
OPP = {TOP: BOTTOM, LEFT: RIGHT, RIGHT: LEFT, BOTTOM: TOP}
M = 0xFFFF
LIBRARY = 50   # run.h RUN_LIBRARY_ROOM
CHASES = [(30, 35), (70, 79)]   # run.h RUN_CHASE_START, RUN_CHASE_END
COURTYARD, GREENHOUSE_LAST = 89, 99   # run.h RUN_COURTYARD_ROOM, RUN_GREENHOUSE_LAST
SHOP = 55   # run.h RUN_SHOP_ROOM


def kind_at(n):
    """run.c run_kind_at"""
    if n == 0:
        return 1
    if n == LIBRARY:
        return 4
    if HAS_ELECTRICAL and n == 100:
        return 10
    if HAS_SHOP and n == SHOP:
        return 11
    for s, e in CHASES if HAS_SEEK else []:
        if n == s:
            return 5
        if s < n < e:
            return 6
        if n == e:
            return 7
    if HAS_GARDEN and n == COURTYARD:
        return 8
    if HAS_GARDEN and COURTYARD < n <= GREENHOUSE_LAST:
        return 9
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
        chase = 5 <= kind <= 7
        garden = kind in (8, 9)
        safe = kind == 8 or (kind == 9 and n == GREENHOUSE_LAST) or kind == 11
        before_fixed = n < 100 and kind_at(n + 1) in (4, 5, 8, 10, 11)
        if kind in (1, 4, 10, 11):
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
        lib = n == LIBRARY or chase or safe
        if not lib and 3 <= n < 100 and self.below(100) < 18:
            flags |= 0x40
            if self.below(100) < 10:
                flags |= 0x80
        f2 = 0
        if not lib and 15 <= n < 100 and self.below(100) < 10:
            f2 |= 1
        if kind == 9:
            f2 = 1
        elif kind == 8:
            f2 = 0
        if not lib and 8 <= n < 100 and self.below(100) < 5:
            f2 |= 2
        ne = sum(1 for w in exits[t] if w)
        real = 0
        if ne > 1:
            real = self.below(ne)
            if not chase and not safe and 6 <= n < 100 and (kind == 9 or self.below(100) < 35):
                f2 |= 8
        elif (not self.halt_planned and not lib and not garden and not before_fixed and 10 <= n < 100 and not (flags & 1)
              and self.below(100) < 3):
            f2 |= 0x40
            self.halt_planned = True
        self.flags.append(flags)
        self.f2.append(f2)
        self.exit.append(real)
        self.slot.append(slot)


ITEM_WEIGHT = [35, 28, 18, 12, 4, 3]   # run.c item_weight: LOOT_LOCKPICK .. LOOT_SKELETON


def loot(seed, n, slot, loose, flash):
    """run.c run_loot: (gold, item, battery, bandage) in drawer or loose spot `slot` of room n.
    scripts/items-test.py checks the ROM against it."""
    p = Plan(seed)
    p.seed_rng((seed ^ 0x6017 ^ ((slot << 11) & M) ^ (0x0480 if loose else 0)) & M, n)
    gold = 10 + p.below(41)
    if loose:
        battery, bandage, g = p.below(100) < 3, p.below(100) < 3, p.below(100) < 20
        if flash and battery:
            return 0, 0, 1, 0
        if bandage:
            return 0, 0, 0, 1
        return (gold if g else 0), 0, 0, 0
    out_gold = gold if p.below(100) < 35 else 0
    item = 0
    if p.below(100) < 6:
        w = p.below(100)
        while w >= ITEM_WEIGHT[item]:
            w -= ITEM_WEIGHT[item]
            item += 1
        item += 1
    battery = 1 if p.below(100) < 4 and flash else 0
    bandage = 1 if p.below(100) < 4 else 0
    return out_gold, item, battery, bandage


def shop_items(seed):
    """run.c run_shop_item: the four items (LOOT_*) on Jeff's counter, in stock-place order"""
    p = Plan(seed)
    p.seed_rng(seed ^ 0x5409, SHOP)
    items = [1, 2, 3, 4, 5, 6]   # lockpick, vitamins, lighter, flashlight, crucifix, skeleton key
    for i in range(5, 0, -1):
        j = p.below(i + 1)
        items[i], items[j] = items[j], items[i]
    return items[:4]


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


def main():
    load(sys.argv[1])
    if len(sys.argv) > 2:
        describe(plan(int(sys.argv[2], 16)))
        return
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


if __name__ == "__main__":
    main()
