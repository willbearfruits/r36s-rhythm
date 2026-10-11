#!/usr/bin/env python3
"""Every screen of the R36S rhythm machine, drawn pixel for pixel at the device's 640x480, and a contact sheet.
Usage: sheet.py OUTDIR"""
import colorsys, math, os, random, sys
from PIL import Image, ImageDraw, ImageFont

W, H = 640, 480
FD = '/usr/share/fonts/misc/'
def font(name, size): return ImageFont.truetype(FD + name + '.otb', size)
F12 = font('ter-u12n', 12); F12B = font('ter-u12b', 12)
F14 = font('ter-u14b', 14); F14N = font('ter-u14n', 14); F16 = font('ter-u16b', 16)
F20 = font('ter-u20b', 20); F24 = font('ter-u24b', 24); F32 = font('ter-u32b', 32)

BG = (10, 10, 16); PANEL = (18, 18, 28); PANEL2 = (30, 30, 46); LINE = (50, 50, 72); KTRACK = (42, 42, 62)
TEXT = (236, 236, 244); DIM = (132, 132, 154); DIMMER = (74, 74, 96); WHITE = (255, 255, 255); INK = (14, 10, 18)
AMBER = (255, 190, 60); GREEN = (96, 236, 132); RED = (255, 76, 76); SKY = (90, 190, 255)

def mix(a, b, t): return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))
def scale(c, k): return tuple(max(0, min(255, int(v * k))) for v in c)
def grey(c): g = int(sum(c) / 3 * 0.55); return (g, g, g)
def hsv(h, s=0.66, v=1.0): return tuple(int(x * 255) for x in colorsys.hsv_to_rgb(h, s, v))
def tw(s, f): return int(f.getlength(s))
def text(d, xy, s, f, c): d.text(xy, s, font=f, fill=c)
def text_c(d, cx, y, s, f, c): text(d, (cx - tw(s, f) // 2, y), s, f, c)
def text_r(d, rx, y, s, f, c): text(d, (rx - tw(s, f), y), s, f, c)

TRACKS = [
    # name, engine, colour, steps, length, speed, muted, sound
    ('KICK',    'SYN', (255,  72,  72), 'X.......X.x.....', 16, '',   False, '808 LONG'),
    ('SNARE',   'SYN', (255, 146,  52), '....X..o....X..o', 16, '',   False, '909 CRACK'),
    ('CLAP',    'SYN', (255, 214,  60), '....x.......x..?', 16, '',   False, 'HOUSE CLAP'),
    ('RIM',     'FM',  (176, 234,  64), '..x....p..x...x.', 16, '',   False, 'FM WOOD'),
    ('C.HAT',   'SYN', ( 72, 224, 112), 'xoxoxoxoxoxoxoRo', 16, '',   False, 'TIGHT 808'),
    ('O.HAT',   'SYN', ( 48, 220, 200), '......x.......x.', 16, '',   False, 'OPEN 808'),
    ('TOM',     'SYN', ( 56, 186, 255), '.............ppp', 16, '',   False, 'LOW TOM'),
    ('CONGA',   'FM',  ( 92, 128, 255), '..x..x...x.x',     12, '',   False, 'FM CONGA'),
    ('SHAKER',  'NOI', (150, 112, 255), 'oxoxoxoxoxoxoxox', 16, 'x2', False, 'DUST SHAKER'),
    ('RIDE',    'SMP', (214, 104, 255), 'x.x.x.x.x.x.x.x.', 16, '',   True,  'RIDE_LOFI.WAV'),
    ('CHOP',    'SMP', (255,  84, 206), '1..3..5.2..6.78.', 16, '',   False, 'DUSTY_94.WAV'),
    ('CRACKLE', 'NOI', (255, 128, 150), '.......?......f.', 16, '',   False, 'TOUCH CRACKLE'),
]
TC = [t[2] for t in TRACKS]
NAV = ['PATTERN', 'PATTERNS', 'SONG', 'PERFORM', 'KIT', 'SOUND', 'SAMPLE', 'SAMPLING',
       'MIXER', 'FX', 'MASTER', 'BROWSER', 'PROJECT', 'MIDI', 'SYNC', 'SETTINGS']
SCOL = {n: hsv(i / 16) for i, n in enumerate(NAV)}

PLAY = 6
GRID_Y, ROW_H, CELL = 48, 26, 22
GRID_X, PITCH, GAP = 104, 26, 6
PANEL_Y, FOOT_Y = 364, 460
def cell_x(i): return GRID_X + i * PITCH + (i // 4) * GAP
def row_y(r): return GRID_Y + r * ROW_H

# ---------------------------------------------------------------- toolkit
def new():
    img = Image.new('RGB', (W, H), BG)
    return img, draw(img)

def draw(img):
    d = ImageDraw.Draw(img); d.fontmode = '1'
    return d

def dimmed(img, k=0.62): return Image.blend(img, Image.new('RGB', img.size, (0, 0, 0)), k)

def chip(d, x, y, s, f, fg, bg, padx=3, h=None, outline=None):
    w = tw(s, f) + 2 * padx
    hh = h or (f.size + 2)
    d.rounded_rectangle([x, y, x + w - 1, y + hh - 1], 2, fill=bg, outline=outline)
    text(d, (x + padx, y + (hh - f.size) // 2), s, f, fg)
    return w

def chips(d, x, y, items, active, col, f=F12, h=16, gap=4, padx=5):
    for i, s in enumerate(items):
        on = (i in active) if isinstance(active, (set, tuple, list)) else (i == active)
        x += chip(d, x, y, s, f, INK if on else DIM, col if on else PANEL2, padx=padx, h=h) + gap
    return x

def big_text(img, xy, s, f, col, k):
    tmp = Image.new('L', (tw(s, f) + 2, f.size + 2), 0)
    dd = ImageDraw.Draw(tmp); dd.fontmode = '1'; dd.text((0, 0), s, font=f, fill=255)
    tmp = tmp.resize((tmp.width * k, tmp.height * k), Image.NEAREST)
    img.paste(Image.new('RGB', tmp.size, col), xy, tmp)

def box(d, x0, y0, x1, y1, title=None, col=DIM, fill=PANEL, right=None):
    d.rounded_rectangle([x0, y0, x1, y1], 4, fill=fill)
    if title: text(d, (x0 + 8, y0 + 5), title, F12B, col)
    if right: text_r(d, x1 - 8, y0 + 5, right, F12, DIM)

def bar(d, x, y, w, v, col, bipolar=False, h=8):
    d.rectangle([x, y, x + w - 1, y + h - 1], fill=KTRACK)
    if bipolar:
        m = x + w // 2; e = m + int(v * (w // 2))
        d.rectangle([min(m, e), y, max(m, e), y + h - 1], fill=col)
        d.line([m, y - 2, m, y + h + 1], fill=DIM)
    elif v > 0:
        d.rectangle([x, y, x + int((w - 1) * v), y + h - 1], fill=col)

def param(d, x, y, w, label, v, s, col, bip=False, sel=False, lw=None):
    lw = lw or 72
    if sel: d.rectangle([x - 6, y - 4, x + w + 4, y + 13], fill=mix(PANEL, col, 0.18)); d.rectangle([x - 6, y - 4, x - 5, y + 13], fill=col)
    text(d, (x, y - 1), label, F12, TEXT if sel else DIM)
    bar(d, x + lw, y + 2, w - lw - 40, v, col, bip)
    text_r(d, x + w, y - 1, s, F12B, TEXT)

def setting(d, x, y, w, label, value, sel=False, col=TEXT, vcol=TEXT):
    if sel:
        d.rectangle([x - 6, y - 3, x + w + 4, y + 14], fill=mix(PANEL, col, 0.2))
        d.rectangle([x - 6, y - 3, x - 5, y + 14], fill=col)
    text(d, (x, y), label, F12, TEXT if sel else DIM)
    text_r(d, x + w, y, value, F12B, vcol)

def knob(d, cx, cy, r, v, col, label=None, val=None, bip=False, w=4):
    bb = [cx - r, cy - r, cx + r, cy + r]
    d.arc(bb, 135, 405, fill=KTRACK, width=w)
    if bip:
        a = 270 + v * 135
        if abs(v) > 0.01: d.arc(bb, min(270, a), max(270, a), fill=col, width=w)
        t = (v + 1) / 2
    else:
        if v > 0.005: d.arc(bb, 135, 135 + 270 * v, fill=col, width=w)
        t = v
    ang = math.radians(135 + 270 * t)
    ri = max(2, r - w - 5)
    d.line([cx + math.cos(ang) * ri, cy + math.sin(ang) * ri, cx + math.cos(ang) * (r - 1), cy + math.sin(ang) * (r - 1)], fill=TEXT, width=2)
    if val: text_c(d, cx, cy - 6, val, F12B, TEXT)
    if label: text_c(d, cx, cy + r + 3, label, F12, DIM)

def fader(d, x, y0, y1, v, col):
    d.rectangle([x - 1, y0, x + 1, y1], fill=KTRACK)
    yc = int(y1 - (y1 - y0) * v)
    d.rectangle([x - 1, yc, x + 1, y1], fill=scale(col, 0.85))
    d.rounded_rectangle([x - 8, yc - 4, x + 8, yc + 4], 2, fill=TEXT)
    d.line([x - 6, yc, x + 6, yc], fill=col)

def meter_v(d, x, y0, y1, v, col, w=5):
    n = (y1 - y0) // 4
    lit = int(n * v)
    for k in range(n):
        yb = y1 - k * 4
        c = col if k < n * 0.72 else (AMBER if k < n * 0.9 else RED)
        d.rectangle([x, yb - 2, x + w - 1, yb], fill=c if k < lit else (28, 28, 40))

def meter_h(d, x0, y, x1, v, col, h=6):
    n = (x1 - x0) // 4
    lit = int(n * v)
    for k in range(n):
        xb = x0 + k * 4
        c = col if k < n * 0.72 else (AMBER if k < n * 0.9 else RED)
        d.rectangle([xb, y, xb + 2, y + h - 1], fill=c if k < lit else (28, 28, 40))

def toggle(d, x, y, on, col):
    d.rounded_rectangle([x, y, x + 25, y + 12], 6, fill=col if on else KTRACK)
    cx = x + 19 if on else x + 6
    d.ellipse([cx - 4, y + 2, cx + 4, y + 10], fill=TEXT)

def star(d, cx, cy, r, col):
    pts = []
    for k in range(10):
        a = -math.pi / 2 + k * math.pi / 5
        rr = r if k % 2 == 0 else r * 0.45
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    d.polygon(pts, fill=col)

def stick(d, cx, cy, dx, dy, col, lab=None, val=None, vcol=None, r=12, dead=None):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=LINE, width=1)
    if dead: d.ellipse([cx - dead, cy - dead, cx + dead, cy + dead], outline=DIMMER)
    d.line([cx - r + 4, cy, cx + r - 4, cy], fill=DIMMER); d.line([cx, cy - r + 4, cx, cy + r - 4], fill=DIMMER)
    px, py = cx + int(dx * (r - 3)), cy + int(dy * (r - 3))
    d.ellipse([px - 3, py - 3, px + 3, py + 3], fill=col)
    if lab:
        text(d, (cx + r + 6, cy - 11), lab, F12, DIM)
        text(d, (cx + r + 6, cy + 1), val, F12B, vcol or col)

def wrap(s, f, width):
    out, line = [], ''
    for w_ in s.split():
        t = (line + ' ' + w_).strip()
        if tw(t, f) > width and line: out.append(line); line = w_
        else: line = t
    if line: out.append(line)
    return out

def footer(d, hints):
    d.rectangle([0, FOOT_Y - 2, W - 1, H - 1], fill=BG)
    x = 8
    for btn, lab in hints:
        x += chip(d, x, FOOT_Y + 4, btn, F12B, (16, 16, 24), (200, 200, 214), padx=3, h=13) + 4
        text(d, (x, FOOT_Y + 4), lab, F12, DIM)
        x += tw(lab, F12) + 12

def rainbow(d, y=30):
    seg = W / 12
    for k, c in enumerate(TC):
        d.rectangle([int(k * seg), y, int((k + 1) * seg) - 1, y + 1], fill=c)

def status(d):
    for x, lab, val, vc in [(452, 'OUT', '26k', AMBER), (484, 'MIDI', 'PC', GREEN), (518, 'CLK', 'OUT', GREEN), (606, 'CPU', '31%', TEXT)]:
        text(d, (x, 1), lab, F12, DIM)
        text(d, (x, 14), val, F14, vc)
    bx, by = 554, 9
    d.rectangle([bx, by, bx + 30, by + 13], outline=DIM)
    d.rectangle([bx + 31, by + 4, bx + 33, by + 9], fill=DIM)
    d.rectangle([bx + 2, by + 2, bx + 2 + int(26 * 0.78), by + 11], fill=GREEN)
    text(d, (bx + 6, by + 1), '78', F12, (8, 30, 12))

def header(d, title, col, fields=None):
    d.rectangle([0, 0, W - 1, 29], fill=PANEL)
    x = 6 + chip(d, 6, 5, title, F16, INK, col, padx=5, h=20) + 10
    d.polygon([(x, 8), (x, 22), (x + 11, 15)], fill=GREEN)
    text(d, (x + 17, 5), '124.0', F20, TEXT); text(d, (x + 68, 12), 'BPM', F12, DIM)
    x += 100
    for lab, val, vc in [('PAT', 'A01', TEXT)] + list(fields or []):
        text(d, (x, 1), lab, F12, DIM); text(d, (x, 14), val, F14, vc)
        x += max(tw(lab, F12), tw(val, F14)) + 14
    status(d)
    rainbow(d)

# waveforms -------------------------------------------------------------
def env_break(n, seed=94, hits=(1.0, 0.5, 0.8, 0.45, 1.0, 0.6, 0.85, 0.5)):
    rnd = random.Random(seed); m = len(hits); out = []
    for k in range(n):
        s = min(m - 1, int(k / n * m)); t = (k - s * n / m) / (n / m)
        out.append(min(1.0, hits[s] * math.exp(-t * 3.4) * (0.35 + 0.65 * rnd.random()) + 0.04 * rnd.random()))
    return out

def env_hit(n, decay=5.0, seed=3, tone=0.0, noise=1.0):
    rnd = random.Random(seed); out = []
    for k in range(n):
        u = k / n
        a = math.exp(-u * decay) * (1 - math.exp(-u * 300))
        out.append(min(1.0, a * (noise * (0.4 + 0.6 * rnd.random()) + tone)))
    return out

def draw_env(d, x0, y0, x1, y1, vals, col, hl=None, hlcol=None):
    mid = (y0 + y1) // 2; amp = (y1 - y0) // 2 - 2
    for k, a in enumerate(vals):
        x = x0 + k
        if x > x1: break
        c = hlcol if (hl and hl[0] <= x <= hl[1]) else col
        h = int(amp * a)
        d.line([x, mid - h, x, mid + h], fill=c)

def draw_tone(d, x0, y0, x1, y1, col, cycles=(4, 30, 8), decay=3.2):
    mid = (y0 + y1) // 2; amp = (y1 - y0) // 2 - 2
    pts, ph, n = [], 0.0, x1 - x0
    for k in range(n):
        u = k / n
        ph += (cycles[0] + cycles[1] * math.exp(-u * cycles[2])) / n
        a = math.exp(-u * decay) * (1 - math.exp(-u * 200))
        px, py = x0 + k, mid - int(amp * a * math.sin(2 * math.pi * ph))
        d.line([px, mid, px, py], fill=scale(col, 0.32))
        pts.append((px, py))
    d.line(pts, fill=col)

def spectrum(d, x0, y0, x1, y1, seed=5):
    rnd = random.Random(seed)
    n = (x1 - x0) // 5
    for k in range(n):
        u = k / n
        h = (0.55 + 0.45 * math.exp(-((u - 0.08) / 0.08) ** 2)) * (1 - 0.6 * u) * (0.7 + 0.3 * rnd.random())
        h = max(0.04, min(1.0, h + (0.25 * math.exp(-((u - 0.62) / 0.06) ** 2))))
        c = TC[min(11, int(u * 12))]
        x = x0 + k * 5
        yt = int(y1 - (y1 - y0) * h)
        d.rectangle([x, yt, x + 3, y1], fill=scale(c, 0.85))
        d.rectangle([x, yt, x + 3, yt + 1], fill=mix(c, WHITE, 0.5))

def euclid(h, s, r): return [1 if (((i - r) % s) * h) % s < h else 0 for i in range(s)]

# icons for the menu -------------------------------------------------------
def ic_grid(d, cx, cy, c):
    on = {(0, 0), (0, 3), (1, 2), (2, 0), (2, 1), (2, 2), (2, 3), (2, 4)}
    for r in range(3):
        for k in range(5):
            x, y = cx - 22 + k * 9, cy - 13 + r * 9
            d.rectangle([x, y, x + 6, y + 6], fill=c if (r, k) in on else scale(c, 0.4))
def ic_tiles(d, cx, cy, c):
    for r in range(2):
        for k in range(4):
            x, y = cx - 23 + k * 12, cy - 12 + r * 13
            d.rectangle([x, y, x + 9, y + 10], fill=c if (r * 4 + k) < 5 else scale(c, 0.4))
def ic_song(d, cx, cy, c):
    x = cx - 24
    for w_ in (10, 6, 14, 9):
        d.rectangle([x, cy - 6, x + w_ - 1, cy + 6], fill=c); x += w_ + 3
    d.line([cx - 6, cy - 12, cx - 6, cy + 12], fill=scale(c, 0.6), width=2)
def ic_xy(d, cx, cy, c):
    d.rectangle([cx - 15, cy - 14, cx + 15, cy + 14], outline=c, width=2)
    d.line([cx - 13, cy + 2, cx + 13, cy + 2], fill=scale(c, 0.5)); d.line([cx + 5, cy - 12, cx + 5, cy + 12], fill=scale(c, 0.5))
    d.ellipse([cx + 1, cy - 2, cx + 9, cy + 6], fill=c)
def ic_kit(d, cx, cy, c):
    for r in range(3):
        for k in range(4):
            x, y = cx - 18 + k * 12, cy - 12 + r * 12
            d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=c if (r + k) % 2 == 0 else scale(c, 0.5))
def ic_wave(d, cx, cy, c):
    pts = [(cx - 22 + k, cy - int(11 * math.sin(k / 44 * 4 * math.pi) * math.exp(-k / 40))) for k in range(45)]
    d.line(pts, fill=c, width=2)
def ic_slices(d, cx, cy, c):
    rnd = random.Random(2)
    for k in range(44):
        h = int(12 * math.exp(-(k % 11) / 5) * (0.5 + 0.5 * rnd.random()))
        d.line([cx - 22 + k, cy - h, cx - 22 + k, cy + h], fill=c)
    for k in (11, 22, 33): d.line([cx - 22 + k, cy - 14, cx - 22 + k, cy + 14], fill=scale(c, 0.5))
def ic_rec(d, cx, cy, c):
    d.ellipse([cx - 13, cy - 13, cx + 13, cy + 13], outline=c, width=2)
    d.ellipse([cx - 7, cy - 7, cx + 7, cy + 7], fill=c)
def ic_faders(d, cx, cy, c):
    for k, v in enumerate((0.7, 0.4, 0.85, 0.55)):
        x = cx - 18 + k * 12
        d.line([x, cy - 14, x, cy + 14], fill=scale(c, 0.5), width=2)
        y = int(cy + 14 - 28 * v); d.rectangle([x - 4, y - 2, x + 4, y + 2], fill=c)
def ic_knobs(d, cx, cy, c):
    for x, v in ((cx - 12, 0.7), (cx + 12, 0.35)):
        d.arc([x - 10, cy - 10, x + 10, cy + 10], 135, 405, fill=scale(c, 0.4), width=3)
        d.arc([x - 10, cy - 10, x + 10, cy + 10], 135, 135 + 270 * v, fill=c, width=3)
def ic_meter(d, cx, cy, c):
    for k in range(6):
        h = 6 + k * 4; x = cx - 20 + k * 7
        d.rectangle([x, cy + 12 - h, x + 4, cy + 12], fill=c)
def ic_folder(d, cx, cy, c):
    d.polygon([(cx - 18, cy - 12), (cx - 6, cy - 12), (cx - 2, cy - 8), (cx + 18, cy - 8), (cx + 18, cy + 12), (cx - 18, cy + 12)], fill=c)
def ic_disk(d, cx, cy, c):
    d.polygon([(cx - 14, cy - 14), (cx + 10, cy - 14), (cx + 14, cy - 10), (cx + 14, cy + 14), (cx - 14, cy + 14)], fill=c)
    d.rectangle([cx - 8, cy - 14, cx + 6, cy - 6], fill=scale(c, 0.4)); d.rectangle([cx - 9, cy + 2, cx + 9, cy + 14], fill=scale(c, 0.6))
def ic_midi(d, cx, cy, c):
    d.ellipse([cx - 14, cy - 14, cx + 14, cy + 14], outline=c, width=2)
    for a in (180, 225, 270, 315, 360):
        r_ = math.radians(a); x, y = cx + math.cos(r_) * 8, cy - math.sin(r_) * 8 * -1 + 2
        d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=c)
    d.rectangle([cx - 3, cy + 11, cx + 3, cy + 14], fill=c)
def ic_pulse(d, cx, cy, c):
    x, pts = cx - 24, []
    for k in range(4):
        pts += [(x, cy + 8), (x, cy - 8), (x + 5, cy - 8), (x + 5, cy + 8), (x + 12, cy + 8)]; x += 12
    d.line(pts, fill=c, width=2)
def ic_gear(d, cx, cy, c):
    for k in range(8):
        a = math.radians(k * 45); x, y = cx + math.cos(a) * 12, cy + math.sin(a) * 12
        d.ellipse([x - 4, y - 4, x + 4, y + 4], fill=c)
    d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], fill=c)
    d.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=PANEL)
ICONS = dict(zip(NAV, [ic_grid, ic_tiles, ic_song, ic_xy, ic_kit, ic_wave, ic_slices, ic_rec,
                       ic_faders, ic_knobs, ic_meter, ic_folder, ic_disk, ic_midi, ic_pulse, ic_gear]))

# ---------------------------------------------------------------- PATTERN (home)
def checker(d, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if ((x >> 1) + (y >> 1)) & 1: d.point((x, y), fill=c)

def draw_cell(d, x, y, code, col, beat, hot, muted, outside, sel=False, ch=CELL):
    c = grey(col) if muted else col
    box_ = [x, y, x + CELL - 1, y + ch - 1]
    if outside:
        d.rectangle(box_, fill=(14, 14, 20))
        for k in range(0, CELL + ch, 5):
            d.line([x + max(0, k - ch + 1), y + min(ch - 1, k), x + min(CELL - 1, k), y + max(0, k - CELL + 1)], fill=(24, 24, 32))
        return
    empty = mix(BG, c, (0.32 if beat else 0.24) if sel else (0.20 if beat else 0.12))
    rr = 3 if ch > 12 else 1
    if code == '.':
        d.rounded_rectangle(box_, rr, fill=empty); return
    vel = {'o': 0.5, 'X': 1.0}.get(code, 0.86)
    fill = scale(c, vel)
    if hot:
        fill = mix(c, WHITE, 0.55)
        d.rounded_rectangle([x - 2, y - 2, x + CELL + 1, y + ch + 1], rr + 1, outline=c, width=1)
    if code == 'o':
        d.rounded_rectangle(box_, rr, fill=mix(BG, c, 0.22), outline=scale(c, 0.85) if not hot else fill)
    elif code == '?':
        d.rounded_rectangle(box_, rr, fill=empty); checker(d, x + 2, y + 2, x + CELL - 3, y + ch - 3, fill)
    elif code == 'R':
        w = (CELL - 4) // 3
        for k in range(3): d.rectangle([x + k * (w + 2), y, x + k * (w + 2) + w - 1, y + ch - 1], fill=fill)
    elif code == 'f':
        d.rounded_rectangle(box_, rr, fill=empty)
        d.rectangle([x, y + 3, x + 3, y + ch - 4], fill=scale(c, 0.55))
        d.rounded_rectangle([x + 6, y, x + CELL - 1, y + ch - 1], rr, fill=fill)
    else:
        d.rounded_rectangle(box_, rr, fill=fill)
        if code == 'X' and ch > 12: d.rectangle([x + 2, y + 1, x + CELL - 3, y + 3], fill=mix(c, WHITE, 0.7))
        if code == 'p' and ch > 12: d.polygon([(x + CELL - 8, y), (x + CELL - 1, y), (x + CELL - 1, y + 7)], fill=WHITE)
        if code.isdigit() and ch > 12: text(d, (x + (CELL - tw(code, F14)) // 2, y + 4), code, F14, (12, 6, 14))

def header_main(d):
    d.rectangle([0, 0, W - 1, 29], fill=PANEL)
    chip(d, 6, 5, 'A01', F16, TEXT, PANEL2, padx=4, h=20)
    d.polygon([(50, 8), (50, 22), (61, 15)], fill=GREEN)
    text(d, (68, 5), '124.0', F20, TEXT); text(d, (120, 12), 'BPM', F12, DIM)
    for x, lab, val in [(150, 'SWING', '54%'), (192, 'BAR', '03.07'), (240, 'KIT', 'BASEMENT')]:
        text(d, (x, 1), lab, F12, DIM); text(d, (x, 14), val, F14, TEXT)
    text(d, (322, 1), 'BANK A', F12, DIM)
    for k in range(8):
        x, y = 322 + k * 15, 16
        if k == 0: d.rectangle([x, y, x + 10, y + 10], fill=WHITE)
        elif k == 1: d.rectangle([x, y, x + 10, y + 10], fill=DIMMER, outline=AMBER, width=2)
        elif k < 5: d.rectangle([x, y, x + 10, y + 10], fill=DIMMER)
        else: d.rectangle([x, y, x + 10, y + 10], outline=LINE)
    status(d); rainbow(d)

def grid(d, sel_row, cursor):
    random.seed(7)
    text(d, (8, 34), 'PAGE 1/2', F12, DIMMER)
    for i in range(16):
        x = cell_x(i)
        if i == PLAY: d.rectangle([x - 2, 33, x + CELL + 1, 46], fill=PANEL2)
        if i % 4 == 0 or i == PLAY:
            s = str(i + 1); text(d, (x + (CELL - tw(s, F12)) // 2, 34), s, F12, TEXT if i == PLAY else DIM)
        else:
            d.rectangle([x + CELL // 2 - 1, 39, x + CELL // 2, 40], fill=DIMMER)
    text(d, (546, 34), 'LEVEL', F12, DIMMER); text(d, (596, 34), 'LEN', F12, DIMMER)
    x = cell_x(PLAY)
    d.rectangle([x - 3, GRID_Y - 2, x + CELL + 2, row_y(12) - 3], fill=mix(BG, WHITE, 0.07))
    for r, (name, eng, col, steps, length, speed, muted, _) in enumerate(TRACKS):
        y = row_y(r); c = grey(col) if muted else col
        if r == sel_row:
            d.rectangle([0, y - 2, W - 1, y + CELL + 1], fill=mix(BG, col, 0.07))
            d.rectangle([x - 3, y - 2, x + CELL + 2, y + CELL + 1], fill=mix(mix(BG, col, 0.07), WHITE, 0.07))
        d.rectangle([4, y, 7, y + CELL - 1], fill=c)
        text(d, (12, y + 4), name, F14, c if not muted else DIM)
        chip(d, 74, y + 4, eng, F12, scale(c, 0.9) if not muted else DIMMER, mix(BG, c, 0.18), padx=2, h=14)
        for i in range(16):
            code = steps[i] if i < len(steps) else '.'
            draw_cell(d, cell_x(i), y, code, col, i % 4 == 0, i == PLAY and code != '.' and not muted, muted, i >= length, r == sel_row)
        playing = PLAY < len(steps) and steps[PLAY] != '.'
        lvl = 0 if muted else (0.92 if playing else random.uniform(0.05, 0.35))
        d.rectangle([546, y + 8, 586, y + 13], fill=(26, 26, 36))
        if lvl: d.rectangle([546, y + 8, 546 + int(40 * lvl), y + 13], fill=c)
        text(d, (596, y + 4), str(length), F14, TEXT if length != 16 else DIMMER)
        if speed: text(d, (614, y + 4), '×2', F14, AMBER)
        if muted: chip(d, 614, y + 4, 'M', F12, (20, 12, 0), AMBER, padx=3, h=14)
    if cursor:
        r, i = cursor; x, y = cell_x(i), row_y(r)
        d.rectangle([x - 4, y - 4, x + CELL + 3, y + CELL + 3], outline=WHITE, width=2)

def panel_frame(d, col):
    d.rectangle([0, PANEL_Y, W - 1, FOOT_Y - 3], fill=PANEL)
    d.rectangle([0, PANEL_Y, W - 1, PANEL_Y + 1], fill=col)

def panel_synth(d, r):
    name, col = TRACKS[r][0], TRACKS[r][2]
    panel_frame(d, col)
    text(d, (10, 372), '%02d' % (r + 1), F14, DIM); text(d, (34, 368), name, F24, col)
    x = 10 + chip(d, 10, 398, 'SYNTH', F12, (14, 8, 8), col, padx=3, h=14) + 4
    chip(d, x, 398, 'KICK MODEL', F12, col, mix(BG, col, 0.2), padx=3, h=14)
    text(d, (10, 416), TRACKS[r][7], F16, TEXT); text(d, (10, 436), 'CHOKE -  VEL 60%', F12, DIM)
    for k, (lab, v, s, bip) in enumerate([('TUNE', 0.25, '+3', True), ('PUNCH', 0.62, '62', False), ('LENGTH', 0.80, '80', False),
                                         ('TONE', 0.40, '40', False), ('GRIT', 0.25, '25', False)]):
        y = 372 + k * 16
        text(d, (192, y - 3), lab, F12, DIM); bar(d, 238, y, 150, v, col, bip); text(d, (394, y - 3), s, F12B, TEXT)
    d.rectangle([428, 370, 632, 418], fill=(12, 12, 20), outline=LINE)
    draw_tone(d, 430, 372, 630, 416, col)
    text(d, (590, 372), '0.62 s', F12, DIM)
    stick(d, 444, 440, -0.35, -0.6, col, 'L STICK', 'TUNE/LEN', col)
    stick(d, 540, 440, 0.5, 0.3, WHITE, 'R STICK', 'REPEAT', AMBER)

def panel_sample(d, r):
    name, col, steps = TRACKS[r][0], TRACKS[r][2], TRACKS[r][3]
    panel_frame(d, col)
    text(d, (10, 372), '%02d' % (r + 1), F14, DIM); text(d, (34, 368), name, F24, col)
    x = 10 + chip(d, 10, 398, 'SAMPLE', F12, (14, 8, 14), col, padx=3, h=14) + 4
    chip(d, x, 398, 'SLICE 8', F12, col, mix(BG, col, 0.2), padx=3, h=14)
    text(d, (10, 416), 'DUSTY_94.WAV', F16, TEXT); text(d, (10, 436), '1.92 s  48k  16 BIT', F12, DIM)
    slices_wave(d, 192, 368, 632, 424, col, int(steps[PLAY]) - 1)
    x = 192
    for lab, val in [('PITCH', '+0'), ('DECAY', '70'), ('BITS', '12'), ('RATE', '26k'), ('LOOP', 'OFF'), ('REV', 'OFF')]:
        lw = tw(lab, F12) + tw(val, F12B) + 14
        d.rounded_rectangle([x, 432, x + lw - 1, 450], 2, fill=PANEL2)
        text(d, (x + 4, 436), lab, F12, DIM)
        text(d, (x + 8 + tw(lab, F12), 436), val, F12B, AMBER if lab in ('BITS', 'RATE') else TEXT)
        x += lw + 6

def slices_wave(d, bx0, by0, bx1, by1, col, playing, n=8, numbers=True, start=None):
    d.rectangle([bx0, by0, bx1, by1], fill=(12, 12, 20), outline=LINE)
    w = bx1 - bx0 - 2
    sx = [bx0 + 1 + int(w * k / n) for k in range(n + 1)]
    if playing >= 0: d.rectangle([sx[playing], by0 + 1, sx[playing + 1], by1 - 1], fill=mix(BG, col, 0.22))
    vals = env_break(w)
    mid = (by0 + by1) // 2 + (4 if numbers else 0); amp = (by1 - by0) // 2 - (8 if numbers else 3)
    for k, a in enumerate(vals):
        x = bx0 + 1 + k; s = min(n - 1, int(k / w * n))
        h = int(amp * a)
        d.line([x, mid - h, x, mid + h], fill=mix(col, WHITE, 0.35) if s == playing else scale(col, 0.85))
    for k in range(n):
        d.line([sx[k], by0 + 1, sx[k], by1 - 1], fill=WHITE if k == playing else DIMMER)
        if numbers:
            chip(d, sx[k] + 1, by0 + 1, str(k + 1), F12, INK if k == playing else TEXT, WHITE if k == playing else PANEL2, padx=2, h=12)

MAIN_HINTS = [('A', 'hit'), ('A+↑↓', 'velocity'), ('X', 'step'), ('Y', 'sound'), ('L1', 'pads'), ('R2', 'edit'), ('SEL', 'menu'), ('START', 'play')]

def s_pattern(sel_row=0, cursor=(0, 8), panel='synth'):
    img, d = new()
    header_main(d); grid(d, sel_row, cursor)
    (panel_synth if panel == 'synth' else panel_sample)(d, sel_row)
    footer(d, MAIN_HINTS)
    return img

# ---------------------------------------------------------------- screens
def s_boot():
    img, d = new()
    rows = [(0, 'X...X...X...X...'), (1, '....X.......X...'), (4, 'xxxxxxxxxxxxxxxx'), (10, '1..3..5.2..6.78.')]
    s, g = 20, 4; x0 = (W - 16 * (s + g) + g) // 2
    for k, (ti, pat) in enumerate(rows):
        col = TC[ti]
        for i, ch in enumerate(pat):
            x, y = x0 + i * (s + g) + (i // 4) * 0, 64 + k * (s + g)
            d.rounded_rectangle([x, y, x + s - 1, y + s - 1], 3, fill=mix(BG, col, 0.14) if ch == '.' else col)
    name, k = 'STICKS', 3
    x = (W - tw(name, F24) * k) // 2
    for i, ch in enumerate(name): big_text(img, (x + i * 12 * k, 176), ch, F24, TC[i * 2], k)
    text_c(d, W // 2, 262, 'RHYTHM MACHINE  ·  SAMPLER', F14, DIM)
    text_c(d, W // 2, 282, 'v0.1', F12, DIMMER)
    log = [('rk3326  4 cores  1008 MB', 'ok'), ('screen 640x480 from u-boot', 'ok'), ('codec rk817  48000 Hz  1.3 ms', 'ok'),
           ('sd card  29.7 GB free', 'ok'), ('usb  computer  midi + drive', 'ok'), ('kit BASEMENT  12 sounds', '...')]
    for i, (s_, st) in enumerate(log):
        y = 322 + i * 15
        text(d, (160, y), s_, F12, DIM); text_r(d, 480, y, st, F12B, GREEN if st == 'ok' else AMBER)
    for i in range(12):
        x = 160 + i * 27
        d.rectangle([x, 424, x + 23, 429], fill=TC[i] if i < 9 else PANEL2)
    return img

def s_menu():
    img = dimmed(s_pattern(), 0.72); d = draw(img)
    d.rounded_rectangle([16, 36, 623, 454], 6, fill=(16, 16, 26), outline=LINE)
    text(d, (28, 42), 'MENU', F14, TEXT)
    text_r(d, 612, 44, 'SELECT + D-PAD:  ← PATTERN  ↑ MIXER  → SOUND  ↓ PERFORM', F12, DIM)
    for i, name in enumerate(NAV):
        r, c = divmod(i, 4)
        x, y = 27 + c * 148, 62 + r * 98
        col = SCOL[name]; sel = name == 'SOUND'
        d.rounded_rectangle([x, y, x + 141, y + 91], 5, fill=col if sel else mix(BG, col, 0.16), outline=WHITE if sel else scale(col, 0.7), width=2 if sel else 1)
        ICONS[name](d, x + 70, y + 38, INK if sel else col)
        text_c(d, x + 70, y + 68, name, F14, INK if sel else TEXT)
    footer(d, [('D-PAD', 'move'), ('A', 'open'), ('B', 'close'), ('SEL', 'close')])
    return img

def s_step():
    base = s_pattern(sel_row=6, cursor=None)
    img = dimmed(base, 0.5); d = draw(img)
    col = TRACKS[6][2]
    x, y = cell_x(13), row_y(6)
    d.rectangle([x - 4, y - 4, x + CELL + 3, y + CELL + 3], outline=WHITE, width=2)
    d.rounded_rectangle([12, 246, 627, 454], 6, fill=PANEL, outline=col)
    x = 24 + chip(d, 24, 254, 'STEP 14', F14, INK, col, padx=5, h=18) + 8
    text(d, (x, 256), 'TOM  ·  LOW TOM  ·  SYNTH', F14, col)
    L = 28
    param(d, L, 284, 280, 'VELOCITY', 0.75, '96', col, sel=True)
    param(d, L, 306, 280, 'NUDGE', -0.25, '-6 ms', col, bip=True)
    text(d, (L, 327), 'RATCHET', F12, DIM); chips(d, L + 72, 325, ['×1', '×2', '×3', '×4', '×6', '×8'], 2, col, h=15, padx=4)
    text(d, (L, 349), 'SHAPE', F12, DIM); chips(d, L + 72, 347, ['FLAT', 'FADE', 'RISE'], 1, col, h=15, padx=4)
    param(d, L, 372, 280, 'CHANCE', 1.0, '100%', col)
    text(d, (L, 393), 'CONDITION', F12, DIM); chips(d, L + 72, 391, ['ALWAYS', '1:2', '2:2', '1:4', 'FILL', 'NOT FILL'], 0, col, h=15, padx=3, gap=3)
    param(d, L, 416, 280, 'GATE', 1.0, '100%', col)
    R = 334
    text(d, (R, 282), 'LOCKED ON THIS STEP', F12B, col)
    param(d, R, 302, 278, 'TUNE', -0.4, '-5 st', col, bip=True, lw=60)
    param(d, R, 322, 278, 'LENGTH', 0.30, '30', col, lw=60)
    param(d, R, 342, 278, 'TONE', 0.70, '70', col, lw=60)
    text(d, (R, 360), 'Hold A and move the left stick to lock more.', F12, DIM)
    bx0, by0, bx1, by1 = R, 380, 612, 440
    d.rectangle([bx0, by0, bx1, by1], fill=(12, 12, 20), outline=LINE)
    sw = (bx1 - bx0) // 3
    for k in range(3):
        xs = bx0 + k * sw
        if k: d.line([xs, by0 + 1, xs, by1 - 1], fill=LINE)
        text(d, (xs + 4, by0 + 2), str(13 + k), F12, DIM if k != 1 else TEXT)
    s0 = bx0 + sw - 8
    for j, h in enumerate((1.0, 0.66, 0.4)):
        xx = s0 + j * sw // 3
        d.rectangle([xx, by1 - 4 - int(36 * h), xx + 5, by1 - 4], fill=col)
    d.line([bx0 + sw, by1 - 2, s0, by1 - 2], fill=AMBER, width=2)
    text(d, (bx0 + sw + 24, by0 + 2), '×3 FADE', F12, AMBER)
    footer(d, [('↑↓', 'choose'), ('←→', 'change'), ('Y', 'clear locks'), ('X', 'copy step'), ('B', 'close')])
    return img

def s_pads():
    img, d = new()
    header(d, 'PADS', SCOL['PATTERN'], [('REC', 'ON', RED), ('QUANT', '1/16', TEXT)])
    x = cell_x(PLAY)
    d.rectangle([x - 2, 36, x + CELL + 1, 36 + 12 * 13], fill=mix(BG, WHITE, 0.07))
    new_hits = {(1, 6), (1, 7), (4, 6)}
    for r, t in enumerate(TRACKS):
        y = 38 + r * 13; col, steps, length, muted = t[2], t[3], t[4], t[6]
        text(d, (8, y), t[0], F12, col if not muted else DIM)
        for i in range(16):
            code = steps[i] if i < len(steps) else '.'
            if (r, i) in new_hits: code = 'x'
            draw_cell(d, cell_x(i), y + 1, code, col, i % 4 == 0, False, muted, i >= length, ch=10)
            if (r, i) in new_hits: d.rectangle([cell_x(i) - 1, y, cell_x(i) + CELL, y + 11], outline=WHITE)
    y = 202
    x = 8 + chip(d, 8, y, '● REC', F12B, WHITE, RED, padx=5, h=16) + 4
    chips(d, x, y, ['OVERDUB', 'QUANTIZE 1/16', 'COUNT-IN 1 BAR', 'R2 = ACCENT'], -1, SCOL['PATTERN'], h=16)
    text_c(d, W // 2, 228, 'L1 HELD  ·  TRACKS 1-8', F12B, TEXT)
    def cluster(cx, cy, pads):
        for (dx, dy, w_, h_), (lab, ti) in zip([(-42, -90, 84, 54), (-132, -27, 84, 54), (48, -27, 84, 54), (-42, 36, 84, 54)], pads):
            col = TC[ti]; x0, y0 = cx + dx, cy + dy
            lit = ti == 1
            if lit: d.rounded_rectangle([x0 - 4, y0 - 4, x0 + w_ + 3, y0 + h_ + 3], 9, outline=col, width=2)
            d.rounded_rectangle([x0, y0, x0 + w_ - 1, y0 + h_ - 1], 7, fill=mix(col, WHITE, 0.45) if lit else mix(BG, col, 0.28), outline=col)
            text(d, (x0 + 6, y0 + 4), lab, F12B, INK if lit else scale(col, 0.9))
            text_c(d, x0 + w_ // 2, y0 + 20, TRACKS[ti][0], F16, INK if lit else TEXT)
            text_c(d, x0 + w_ // 2, y0 + 38, '%02d' % (ti + 1), F12, INK if lit else DIM)
    cluster(170, 336, [('↑', 0), ('←', 1), ('→', 3), ('↓', 2)])
    cluster(470, 336, [('X', 4), ('Y', 5), ('A', 7), ('B', 6)])
    text_c(d, W // 2, 440, 'L2 + ABXY: TRACKS 9-12', F12, DIM)
    footer(d, [('L1', 'hold'), ('D-PAD ABXY', 'play'), ('R2', 'accent'), ('START', 'play'), ('R2+B', 'undo take')])
    return img

def s_generate():
    img, d = new()
    header(d, 'GENERATE', SCOL['PATTERN'])
    col = TC[3]
    box(d, 8, 36, 292, 452, 'EUCLID  ·  04 RIM', col)
    cx, cy, R = 150, 178, 96
    pat = euclid(5, 16, 2)
    pts = []
    for i in range(16):
        a = math.radians(-90 + i * 22.5); x, y = cx + math.cos(a) * R, cy + math.sin(a) * R
        if pat[i]: pts.append((x, y))
    if len(pts) > 2: d.polygon(pts, fill=mix(PANEL, col, 0.14)); d.line(pts + [pts[0]], fill=scale(col, 0.6))
    for i in range(16):
        a = math.radians(-90 + i * 22.5); x, y = cx + math.cos(a) * R, cy + math.sin(a) * R
        if pat[i]: d.ellipse([x - 8, y - 8, x + 8, y + 8], fill=col)
        else: d.ellipse([x - 5, y - 5, x + 5, y + 5], outline=DIMMER, width=2)
        if i == PLAY: d.ellipse([x - 11, y - 11, x + 11, y + 11], outline=WHITE, width=2)
    d.polygon([(cx - 5, cy - R - 20), (cx + 5, cy - R - 20), (cx, cy - R - 13)], fill=AMBER)
    text_c(d, cx, cy - 12, '5 / 16', F24, TEXT)
    for k, (lab, v, s, bip) in enumerate([('HITS', 5 / 16, '5', False), ('STEPS', 1.0, '16', False), ('ROTATE', 2 / 8, '+2', True), ('ACCENT', 3 / 8, 'EVERY 3', False)]):
        param(d, 22, 306 + k * 26, 258, lab, v, s, col, bip=bip, sel=k == 0, lw=60)
    text(d, (22, 414), 'Writes into the pattern when you press A.', F12, DIM)
    text(d, (22, 428), 'B leaves it as it was.', F12, DIM)
    box(d, 300, 36, 632, 262, 'ALL TRACKS', DIM)
    eu = ['E 4·16', '—', '—', 'E 5·16 +2', 'E 16·16', '—', '—', 'E 4·12 +2', 'E 16·16', 'E 8·16', '—', 'E 2·16 +7']
    for r, t in enumerate(TRACKS):
        y = 56 + r * 17; c = t[2] if not t[6] else DIM
        if r == 3: d.rectangle([304, y - 2, 628, y + 13], fill=mix(PANEL, col, 0.2))
        text(d, (310, y), t[0], F12, c); text(d, (370, y), eu[r], F12, DIM if eu[r] == '—' else TEXT)
        steps = euclid(5, 16, 2) if r == 3 else [1 if (i < len(t[3]) and t[3][i] != '.') else 0 for i in range(16)]
        for i in range(16):
            x = 458 + i * 10 + (i // 4) * 2
            out = i >= t[4]
            d.rectangle([x, y + 1, x + 7, y + 10], fill=(16, 16, 22) if out else (scale(c, 0.9) if steps[i] else mix(PANEL, c, 0.2)))
    box(d, 300, 270, 632, 452, 'DICE  ·  FILL', DIM)
    param(d, 312, 298, 306, 'PATTERN', 0.35, '35%', AMBER, lw=72, sel=True)
    param(d, 312, 320, 306, 'SOUND', 0.20, '20%', AMBER, lw=72)
    text(d, (312, 341), 'KEEP', F12, DIM)
    for i in range(12):
        x = 384 + i * 19; keep = i in (0, 1, 2)
        d.rectangle([x, 340, x + 14, 353], fill=TC[i] if keep else mix(PANEL, TC[i], 0.25))
        if keep: d.line([x + 3, 347, x + 6, 350, x + 11, 343], fill=INK, width=2)
    text(d, (312, 365), 'FILL', F12, DIM); chips(d, 384, 363, ['ROLL', 'BUILD', 'BREAK', 'DROP'], 1, AMBER, h=15, padx=4)
    text(d, (312, 387), 'LENGTH', F12, DIM); chips(d, 384, 385, ['1/2 BAR', '1 BAR', '2 BARS'], 1, AMBER, h=15, padx=4)
    x = 312 + chip(d, 312, 414, 'A  ROLL', F14, INK, AMBER, padx=6, h=20) + 6
    chip(d, x, 414, 'B  UNDO', F14, TEXT, PANEL2, padx=6, h=20)
    footer(d, [('↑↓', 'track'), ('←→', 'change'), ('A', 'apply / roll'), ('B', 'undo'), ('L1', 'listen')])
    return img

def variation(seed):
    rnd = random.Random(seed); rows = []
    for t in TRACKS:
        s = list(t[3].ljust(16, '.'))
        for i in range(16):
            if rnd.random() < 0.12: s[i] = '.' if s[i] != '.' else 'x'
        rows.append(''.join(s))
    return rows

def s_patterns():
    img, d = new()
    header(d, 'PATTERNS', SCOL['PATTERNS'])
    for k, b in enumerate('ABCDEFGH'):
        x = 8 + k * 34
        on = k == 0; has = k in (0, 1, 3)
        chip(d, x, 36, ' ' + b + ' ', F14, INK if on else (TEXT if has else DIMMER), WHITE if on else (PANEL2 if has else PANEL), padx=4, h=18)
    text_r(d, 632, 39, 'BANK A  ·  7 OF 16 USED', F12, DIM)
    used = {0: 'PLAY', 1: 'NEXT', 2: '', 3: '', 4: '', 8: '', 9: ''}
    for n in range(16):
        r, c = divmod(n, 4)
        x, y = 8 + c * 157, 60 + r * 98
        st = used.get(n)
        if st is None:
            for k in range(x, x + 150, 6): d.line([k, y, min(k + 2, x + 150), y], fill=LINE); d.line([k, y + 91, min(k + 2, x + 150), y + 91], fill=LINE)
            for k in range(y, y + 92, 6): d.line([x, k, x, min(k + 2, y + 91)], fill=LINE); d.line([x + 150, k, x + 150, min(k + 2, y + 91)], fill=LINE)
            text(d, (x + 8, y + 6), 'A%02d' % (n + 1), F14, DIMMER); text_c(d, x + 75, y + 42, 'EMPTY', F12, DIMMER)
            continue
        oc = GREEN if st == 'PLAY' else (AMBER if st == 'NEXT' else LINE)
        d.rounded_rectangle([x, y, x + 150, y + 91], 4, fill=PANEL, outline=oc, width=2 if st else 1)
        text(d, (x + 8, y + 5), 'A%02d' % (n + 1), F14, TEXT)
        if st == 'PLAY': d.polygon([(x + 50, y + 8), (x + 50, y + 18), (x + 58, y + 13)], fill=GREEN)
        if st: text_r(d, x + 142, y + 6, st, F12B, oc)
        else: text_r(d, x + 142, y + 6, '16' if n != 9 else '32', F12, DIM)
        rows = TRACKS and (variation(n) if n else [t[3].ljust(16, '.') for t in TRACKS])
        for rr, srow in enumerate(rows):
            for i in range(16):
                xx, yy = x + 11 + i * 8, y + 24 + rr * 5
                on = srow[i] != '.' and i < TRACKS[rr][4]
                d.rectangle([xx, yy, xx + 6, yy + 3], fill=(TC[rr] if not TRACKS[rr][6] else DIM) if on else mix(PANEL, TC[rr], 0.12))
    footer(d, [('A', 'queue'), ('START', 'play now'), ('R2+A', 'copy'), ('R2+X', 'paste'), ('R2+Y', 'clear'), ('L1/R1', 'bank')])
    return img

def s_song():
    img, d = new()
    header(d, 'SONG', SCOL['SONG'], [('LEN', '48 BARS', TEXT), ('TIME', '1:33', TEXT)])
    chain = [('A01', 4), ('A02', 2), ('A01', 4), ('A03', 4), ('A04', 8), ('A02', 1), ('A05', 8), ('A03', 4), ('A01', 8), ('A04', 4), ('A02', 1)]
    pc = {'A01': TC[0], 'A02': TC[2], 'A03': TC[6], 'A04': TC[9], 'A05': TC[4]}
    x0, x1, y0 = 10, 630, 58
    ppb = (x1 - x0) / 48
    box(d, 4, 36, 636, 196, None)
    for b in range(0, 49, 4):
        x = int(x0 + b * ppb); d.line([x, y0 - 6, x, y0 + 110], fill=LINE if b % 16 else DIMMER)
        if b < 48: text(d, (x + 2, 40), str(b + 1), F12, DIM)
    lx0, lx1 = int(x0 + 8 * ppb), int(x0 + 16 * ppb)
    d.rectangle([lx0, 52, lx1, 55], fill=AMBER); text(d, (lx0 + 2, 172), 'LOOP 9-16', F12B, AMBER)
    b = 0
    for k, (p, n) in enumerate(chain):
        xa, xb = int(x0 + b * ppb) + 1, int(x0 + (b + n) * ppb) - 1
        col = pc[p]
        d.rounded_rectangle([xa, y0, xb, y0 + 100], 3, fill=mix(BG, col, 0.30), outline=col)
        rows = variation(int(p[1:])) if p != 'A01' else [t[3].ljust(16, '.') for t in TRACKS]
        span = (xb - xa - 6) / (n * 16)
        for r in range(12):
            yy = y0 + 24 + r * 6
            for bb in range(n * 16):
                if rows[r][bb % 16] != '.' and bb % 16 < TRACKS[r][4]:
                    xx = int(xa + 3 + bb * span)
                    d.rectangle([xx, yy, xx + max(0, int(span) - 1), yy + 3], fill=scale(TC[r], 0.9) if not TRACKS[r][6] else DIMMER)
        if xb - xa > 30: text(d, (xa + 4, y0 + 4), p, F14, TEXT)
        if xb - xa > 60: text_r(d, xb - 4, y0 + 5, '×%d' % n, F12B, TEXT)
        if k == 3: d.rounded_rectangle([xa - 2, y0 - 2, xb + 2, y0 + 102], 4, outline=WHITE, width=2)
        b += n
    px = int(x0 + 13.5 * ppb); d.line([px, y0 - 4, px, y0 + 104], fill=WHITE, width=2)
    box(d, 4, 202, 316, 452, 'CHAIN', DIM)
    b = 0
    for k, (p, n) in enumerate(chain[:11]):
        y = 222 + k * 20
        if k == 3: d.rectangle([8, y - 3, 312, y + 14], fill=mix(PANEL, pc[p], 0.25))
        text(d, (14, y), '%02d' % (k + 1), F12, DIM)
        d.rectangle([40, y + 1, 48, y + 10], fill=pc[p])
        text(d, (56, y), p, F12B, TEXT); text(d, (96, y), '×%d' % n, F12, TEXT)
        text(d, (136, y), 'BARS %d-%d' % (b + 1, b + n), F12, DIM)
        if k == 5: text(d, (230, y), 'FILL', F12B, AMBER)
        if k == 4: text(d, (230, y), '120 BPM', F12, SKY)
        b += n
    col = pc['A03']
    box(d, 322, 202, 636, 452, 'ENTRY 04', col)
    text(d, (334, 224), 'A03', F24, col)
    text(d, (390, 232), 'BARS 11-14', F12, DIM)
    param(d, 334, 262, 290, 'REPEAT', 4 / 16, '×4', col, sel=True, lw=70)
    param(d, 334, 284, 290, 'TEMPO', 0, 'SONG', col, lw=70)
    text(d, (334, 305), 'MUTES', F12, DIM)
    for i in range(12):
        x = 404 + i * 18; m = i in (9, 11)
        d.rectangle([x, 304, x + 13, 316], fill=mix(PANEL, TC[i], 0.2) if m else TC[i])
        if m: text(d, (x + 4, 304), 'M', F12, AMBER)
    text(d, (334, 327), 'LAST BAR', F12, DIM); chips(d, 404, 325, ['AS IS', 'FILL', 'DROP'], 0, col, h=15, padx=4)
    text(d, (334, 349), 'NEXT', F12, DIM); chips(d, 404, 347, ['GO ON', 'LOOP', 'STOP'], 0, col, h=15, padx=4)
    text(d, (334, 380), 'A plays from here. START plays the song', F12, DIM)
    text(d, (334, 394), 'from the top.', F12, DIM)
    footer(d, [('←→', 'entry'), ('A', 'play here'), ('R2+X', 'insert'), ('R2+Y', 'delete'), ('R2+A', 'copy'), ('L1', 'loop')])
    return img

def s_perform():
    img, d = new()
    header(d, 'PERFORM', SCOL['PERFORM'])
    x0, y0, x1, y1 = 8, 36, 300, 328
    d.rectangle([x0, y0, x1, y1], fill=(12, 12, 20), outline=LINE)
    for k in range(1, 8):
        x = x0 + k * (x1 - x0) // 8; y = y0 + k * (y1 - y0) // 8
        d.line([x, y0 + 1, x, y1 - 1], fill=(26, 26, 38)); d.line([x0 + 1, y, x1 - 1, y], fill=(26, 26, 38))
    for k in range(4):
        yy = y0 + 28 + k * (y1 - y0 - 64) // 3
        text(d, (x0 + 6, yy), ['1/32', '1/16', '1/8', '1/4'][k], F12, DIMMER)
    text(d, (x0 + 6, y1 - 16), 'LOW', F12, DIMMER); text_r(d, x1 - 6, y1 - 16, 'HIGH', F12, DIMMER)
    rnd = random.Random(4); trail = []
    px, py = 0.22, 0.78
    for k in range(40):
        px = min(0.9, max(0.08, px + 0.0115 + rnd.uniform(-0.008, 0.008))); py = min(0.9, max(0.12, py - 0.0115 + rnd.uniform(-0.008, 0.008)))
        trail.append((x0 + px * (x1 - x0), y0 + py * (y1 - y0)))
    for k in range(1, len(trail)):
        d.line([trail[k - 1], trail[k]], fill=mix((12, 12, 20), AMBER, k / len(trail)), width=2)
    cx, cy = trail[-1]
    d.line([cx, y0 + 1, cx, y1 - 1], fill=scale(AMBER, 0.35)); d.line([x0 + 1, cy, x1 - 1, cy], fill=scale(AMBER, 0.35))
    d.ellipse([cx - 9, cy - 9, cx + 9, cy + 9], outline=AMBER, width=2); d.ellipse([cx - 4, cy - 4, cx + 4, cy + 4], fill=WHITE)
    chip(d, x1 - 66, y0 + 6, 'LATCHED', F12B, INK, AMBER, padx=4, h=14)
    text(d, (x0 + 6, y0 + 4), '↑ REPEAT', F12B, AMBER); text_r(d, x1 - 6, y1 - 30, 'FILTER →', F12B, SKY)
    box(d, 308, 36, 632, 150, 'ON THE STICK', DIM)
    fx = ['REPEAT', 'REVERSE', 'TAPE STOP', 'GATE', 'FILTER', 'CRUSH', 'DUB', 'FREEZE']
    for i, n in enumerate(fx):
        r, c = divmod(i, 4); x, y = 316 + c * 78, 58 + r * 44
        on = {'REPEAT': AMBER, 'FILTER': SKY}.get(n)
        d.rounded_rectangle([x, y, x + 72, y + 36], 4, fill=on if on else PANEL2)
        text_c(d, x + 36, y + 5, n, F12B, INK if on else TEXT)
        if on: text_c(d, x + 36, y + 20, 'Y AXIS' if n == 'REPEAT' else 'X AXIS', F12, INK)
    box(d, 308, 156, 632, 236, 'MUTES', DIM)
    for i, t in enumerate(TRACKS):
        r, c = divmod(i, 6); x, y = 316 + c * 52, 176 + r * 28
        m = t[6] or i == 11
        d.rounded_rectangle([x, y, x + 47, y + 23], 3, fill=mix(PANEL, t[2], 0.18) if m else t[2])
        text_c(d, x + 24, y + 5, t[0][:5], F12B, DIM if m else INK)
    box(d, 308, 242, 632, 328, 'SCENES', DIM)
    text(d, (318, 266), 'A', F20, TEXT); text_r(d, 622, 266, 'B', F20, TEXT)
    bar(d, 340, 272, 262, 0.35, SCOL['PERFORM'], h=8)
    text_c(d, 470, 290, 'MORPH 35%  ·  L2 + LEFT STICK', F12, DIM)
    text(d, (318, 306), 'A: BASEMENT DRY    B: WIDE + DUB', F12, DIM)
    box(d, 8, 336, 632, 452, 'MOTION  ·  FILTER (X)  ·  RECORDED WITH R1', SKY, right='BAR 1 OF 2')
    rnd = random.Random(9); v = 0.4
    for k in range(64):
        v = min(1, max(0.05, v + rnd.uniform(-0.12, 0.14)))
        x = 16 + k * 9.6; y1_ = 444
        d.rectangle([int(x), int(y1_ - 80 * v), int(x) + 6, y1_], fill=SKY if k < 40 else mix(PANEL, SKY, 0.3))
    footer(d, [('R STICK', 'play'), ('R3', 'latch'), ('R1', 'record'), ('A', 'choose fx'), ('L2+↑', 'fill'), ('X', 'mute')])
    return img

def s_kit():
    img, d = new()
    header(d, 'KIT', SCOL['KIT'], [('KIT', 'BASEMENT', TEXT)])
    cols = [(14, '#'), (34, 'NAME'), (104, 'ENGINE'), (156, 'SOUND'), (290, 'LEVEL'), (380, 'PAN'), (432, 'CHOKE'), (480, 'OUT'), (530, 'NOTE'), (590, 'CH')]
    for x, s in cols: text(d, (x, 36), s, F12, DIMMER)
    for r, t in enumerate(TRACKS):
        y = 54 + r * 25; col = t[2]; muted = t[6]
        if r == 4:
            d.rectangle([0, y - 3, W - 1, y + 18], fill=mix(BG, col, 0.14)); d.rectangle([0, y - 3, 1, y + 18], fill=col)
        d.rectangle([4, y - 1, 7, y + 15], fill=col)
        text(d, (14, y + 1), '%02d' % (r + 1), F12, DIM)
        text(d, (34, y), t[0], F14, col)
        chip(d, 104, y + 1, {'SYN': 'SYNTH', 'FM': 'FM', 'NOI': 'NOISE', 'SMP': 'SAMPLE'}[t[1]], F12, scale(col, 0.9), mix(BG, col, 0.18), padx=3, h=14)
        text(d, (156, y + 1), t[7], F12B, TEXT)
        lv = [0.8, 0.74, 0.62, 0.55, 0.5, 0.48, 0.66, 0.58, 0.4, 0.45, 0.7, 0.35][r]
        bar(d, 290, y + 4, 70, lv, col)
        pan = [0, 0.1, -0.15, 0.35, -0.3, -0.3, 0.2, -0.4, 0.5, 0.25, 0, -0.6][r]
        d.line([380, y + 7, 420, y + 7], fill=KTRACK, width=2); d.line([400, y + 3, 400, y + 11], fill=DIMMER)
        px = 400 + int(pan * 20); d.ellipse([px - 3, y + 4, px + 3, y + 10], fill=col)
        ch = {4: 'A', 5: 'A', 6: 'B', 7: 'B'}.get(r, '-')
        if ch != '-': chip(d, 444, y + 1, ch, F12B, INK, AMBER if ch == 'A' else SKY, padx=3, h=14)
        else: text(d, (446, y + 1), '-', F12, DIMMER)
        text(d, (480, y + 1), 'MAIN' if r not in (10,) else 'BUS B', F12, TEXT if r == 10 else DIM)
        note = ['C1 36', 'D1 38', 'D#1 39', 'C#1 37', 'F#1 42', 'A#1 46', 'F1 41', 'D#4 63', 'A#3 70', 'D#2 51', 'C3 60', 'G#3 68'][r]
        text(d, (530, y + 1), note, F12, TEXT); text(d, (596, y + 1), '10', F12, DIM)
    d.rectangle([0, 360, W - 1, 362], fill=SCOL['KIT'])
    box(d, 4, 368, 636, 454, None)
    text(d, (14, 374), 'BASEMENT', F20, TEXT)
    text(d, (14, 398), '12 SOUNDS  ·  2 SAMPLES  ·  9.4 MB', F12, DIM)
    text(d, (14, 414), 'CHOKE A: C.HAT CUTS O.HAT   CHOKE B: TOM, CONGA', F12, DIM)
    x = 14
    for k, (b, s) in enumerate([('A', 'LOAD KIT'), ('X', 'SAVE'), ('R2+X', 'SAVE AS'), ('Y', 'OPEN SOUND'), ('R2+Y', 'RESET')]):
        x += chip(d, x, 432, b, F12B, INK, (200, 200, 214), padx=3, h=14) + 3
        text(d, (x, 433), s, F12, TEXT); x += tw(s, F12) + 12
    rnd = random.Random(1)
    for i in range(12):
        x = 430 + i * 17
        h = int(20 + 30 * rnd.random())
        d.rectangle([x, 418 - h, x + 12, 418], fill=TC[i])
    text(d, (430, 422), 'LEVELS', F12, DIMMER)
    footer(d, [('↑↓', 'track'), ('←→', 'column'), ('A', 'change'), ('Y', 'sound'), ('L1', 'listen')])
    return img

def s_sound_synth():
    img, d = new()
    col = TC[1]
    header(d, 'SOUND', SCOL['SOUND'], [('TRK', '02', col)])
    text(d, (10, 38), '02', F14, DIM); text(d, (34, 34), 'SNARE', F24, col)
    text(d, (10, 62), '909 CRACK', F16, TEXT)
    x = 130 + chip(d, 130, 40, 'SYNTH', F12, INK, col, padx=3, h=14) + 4
    chip(d, x, 40, 'SNARE MODEL', F12, col, mix(BG, col, 0.2), padx=3, h=14)
    text(d, (130, 62), 'CHOKE -   VEL 60%   PAN +10', F12, DIM)
    text(d, (372, 40), 'A/B', F12, DIM)
    chip(d, 398, 37, 'A', F14, INK, col, padx=5, h=18); chip(d, 420, 37, 'B', F14, col, PANEL2, padx=5, h=18)
    bar(d, 446, 43, 90, 0.0, col); text(d, (446, 54), 'MORPH 0%', F12, DIM)
    chip(d, 552, 37, 'DICE', F14, INK, AMBER, padx=5, h=18); text(d, (552, 58), 'WILD 30%', F12, DIM)
    tabs = ['BODY', 'CLICK', 'NOISE', 'MODES', 'DRIVE', 'EQ', 'SHAPE']
    x = 8
    for i, t in enumerate(tabs):
        on = t == 'NOISE'
        w_ = tw(t, F14) + 16
        d.rounded_rectangle([x, 84, x + w_, 104], 3, fill=col if on else PANEL2)
        text(d, (x + 8, 87), t, F14, INK if on else TEXT); x += w_ + 4
    text_r(d, 632, 88, 'SUB METAL CLAP: NOT IN THIS MODEL', F12, DIMMER)
    d.rectangle([0, 104, W - 1, 105], fill=col)
    box(d, 4, 110, 404, 334, None)
    ks = [('LEVEL', 0.72, '72'), ('COLOUR', 0.4, '40'), ('FREQ', 0.55, '4.2k'), ('Q', 0.25, '1.4'),
          ('ATTACK', 0.0, '0ms'), ('DECAY', 0.45, '180'), ('CURVE', 0.3, '30')]
    for i, (lab, v, s) in enumerate(ks):
        r, c = divmod(i, 4)
        knob(d, 56 + c * 98, 160 + r * 104, 28, v, col, lab, s)
        if i == 5: d.rounded_rectangle([56 + c * 98 - 44, 160 + r * 104 - 40, 56 + c * 98 + 44, 160 + r * 104 + 52], 5, outline=WHITE, width=2)
    text_c(d, 350, 228, 'MODE', F12, DIM)
    for k, m in enumerate(['LP', 'BP', 'HP']):
        chip(d, 316 + k * 24, 246, m, F12B, INK if m == 'BP' else DIM, col if m == 'BP' else PANEL2, padx=3, h=16)
    box(d, 410, 110, 636, 214, 'SOUND', DIM, right='0.31 s')
    vals = env_hit(220, decay=5.2, seed=11, tone=0.35)
    draw_env(d, 414, 126, 632, 210, vals, col)
    box(d, 410, 220, 636, 334, 'ENVELOPES', DIM)
    for k, (c_, dec, lab) in enumerate([(col, 9.0, 'BODY'), (WHITE, 4.6, 'NOISE'), (AMBER, 40.0, 'CLICK')]):
        pts = [(416 + j, 322 - int(80 * math.exp(-j / 216 * dec) * (1 - math.exp(-j / 2.0)))) for j in range(216)]
        d.line(pts, fill=c_)
        text(d, (560, 240 + k * 14), lab, F12B, c_)
    text(d, (10, 340), 'MACROS', F12B, DIM)
    for k, (lab, v, s, stk, bip) in enumerate([('TUNE', 0.1, '+1', 'L ←→', True), ('PUNCH', 0.7, '70', '', False), ('LENGTH', 0.45, '45', 'L ↑↓', False),
                                               ('TONE', 0.6, '60', '', False), ('GRIT', 0.15, '15', '', False)]):
        x = 4 + k * 127
        d.rounded_rectangle([x, 356, x + 122, 452], 4, fill=PANEL)
        text(d, (x + 8, 362), lab, F12B, DIM)
        if stk: text_r(d, x + 114, 362, stk, F12B, col)
        text(d, (x + 8, 380), s, F24, TEXT)
        bar(d, x + 8, 414, 106, v, col, bip, h=10)
        text(d, (x + 8, 432), 'every section', F12, DIMMER)
    footer(d, [('L1/R1', 'section'), ('D-PAD', 'move'), ('A+↑↓', 'change'), ('X', 'A/B'), ('R2+Y', 'dice'), ('B', 'back')])
    return img

ALGOS = [([(4, 3), (3, 2), (2, 1)], {1}), ([(4, 2), (3, 2), (2, 1)], {1}), ([(4, 3), (3, 1), (2, 1)], {1}), ([(4, 3), (2, 1)], {1, 3}),
         ([(4, 1), (4, 2), (4, 3)], {1, 2, 3}), ([(4, 3)], {1, 2, 3}), ([(3, 1), (4, 2)], {1, 2}), ([], {1, 2, 3, 4})]

def algo_layout(edges, carriers, x0, y0, w, h):
    depth = {c: 0 for c in carriers}
    for _ in range(4):
        for m, c in edges:
            if c in depth: depth[m] = max(depth.get(m, 0), depth[c] + 1)
    for o in (1, 2, 3, 4): depth.setdefault(o, 0)
    maxd = max(depth.values())
    pos = {}
    for dd in range(maxd + 1):
        ops = sorted(o for o in depth if depth[o] == dd)
        for k, o in enumerate(ops):
            pos[o] = (x0 + w * (k + 1) / (len(ops) + 1), y0 + h - (dd + 0.5) * h / (maxd + 1))
    return pos

def s_sound_fm():
    img, d = new()
    col = TC[3]
    header(d, 'SOUND', SCOL['SOUND'], [('TRK', '04', col)])
    text(d, (10, 38), '04', F14, DIM); text(d, (34, 34), 'RIM', F24, col)
    text(d, (10, 62), 'FM WOOD', F16, TEXT)
    x = 130 + chip(d, 130, 40, 'FM', F12, INK, col, padx=3, h=14) + 4
    chip(d, x, 40, '4 OPERATORS', F12, col, mix(BG, col, 0.2), padx=3, h=14)
    text(d, (130, 62), 'from BARE! and bootloaderg4', F12, DIM)
    box(d, 4, 84, 254, 334, 'ALGORITHM 3 / 8', col)
    edges, car = ALGOS[2]
    pos = algo_layout(edges, car, 14, 108, 230, 160)
    for m, c in edges:
        (xa, ya), (xb, yb) = pos[m], pos[c]
        d.line([xa, ya + 12, xb, yb - 12], fill=scale(col, 0.7), width=2)
    for o, (x, y) in pos.items():
        d.rounded_rectangle([x - 22, y - 13, x + 22, y + 13], 4, fill=col if o in car else mix(PANEL, col, 0.3), outline=col)
        text_c(d, x, y - 7, 'OP%d' % o, F12B, INK if o in car else TEXT)
        if o in car: d.line([x, y + 13, x, 286], fill=col, width=2)
    x4, y4 = pos[4]
    d.arc([x4 + 14, y4 - 22, x4 + 40, y4 + 4], 270, 180, fill=AMBER, width=2)
    text(d, (x4 + 30, y4 - 30), 'FB', F12B, AMBER)
    d.line([20, 286, 240, 286], fill=col, width=2); text_c(d, 129, 290, 'OUT', F12B, col)
    for k, (e, c_) in enumerate(ALGOS):
        x, y = 12 + k * 30, 308
        d.rounded_rectangle([x, y, x + 26, y + 22], 2, fill=col if k == 2 else PANEL2)
        p = algo_layout(e, c_, x, y + 1, 26, 20)
        for m, c in e: d.line([p[m], p[c]], fill=INK if k == 2 else DIM)
        for o, (xx, yy) in p.items(): d.rectangle([xx - 1, yy - 1, xx + 1, yy + 1], fill=INK if k == 2 else TEXT)
    box(d, 260, 84, 636, 334, None)
    ops = [('SINE', '1.00', 0.9, 0.0, 0.18), ('TRI', '3.50', 0.55, 0.0, 0.10), ('SINE', '1.41', 0.7, 0.02, 0.25), ('SAW', '7.00', 0.35, 0.0, 0.05)]
    for k in range(4):
        x = 330 + k * 76
        on = k + 1 in car
        d.rounded_rectangle([x, 90, x + 70, 108], 3, fill=col if on else mix(PANEL, col, 0.3))
        text_c(d, x + 35, 93, 'OP%d' % (k + 1), F12B, INK if on else TEXT)
    rows = ['WAVE', 'RATIO', 'LEVEL', 'ATTACK', 'DECAY', 'ENV']
    for r, lab in enumerate(rows):
        y = 118 + r * 34
        text(d, (270, y + 6), lab, F12, DIM)
        for k, (wv, ra, lv, at, dc) in enumerate(ops):
            x = 330 + k * 76
            sel = (k == 2 and lab == 'RATIO')
            if sel: d.rounded_rectangle([x - 2, y - 2, x + 72, y + 26], 3, outline=WHITE, width=2)
            if lab == 'WAVE': text_c(d, x + 35, y + 6, wv, F12B, TEXT)
            elif lab == 'RATIO': text_c(d, x + 35, y + 4, ra, F16, TEXT)
            elif lab == 'LEVEL': bar(d, x + 4, y + 9, 62, lv, col)
            elif lab == 'ATTACK': text_c(d, x + 35, y + 6, '%d ms' % int(at * 1000), F12B, TEXT)
            elif lab == 'DECAY': text_c(d, x + 35, y + 6, '%d ms' % int(dc * 1000), F12B, TEXT)
            else:
                pts = [(x + 4 + j, y + 24 - int(20 * math.exp(-j / 62 * (0.2 / dc + 1.5)))) for j in range(62)]
                d.line(pts, fill=col)
    text(d, (270, 324), 'OP4 FEEDBACK 40', F12B, AMBER)
    box(d, 4, 340, 316, 452, 'PITCH ENVELOPE  ·  FILTER', col)
    param(d, 16, 364, 290, 'START', 0.6, '+24 st', col, lw=60)
    param(d, 16, 384, 290, 'TIME', 0.15, '12 ms', col, lw=60)
    param(d, 16, 404, 290, 'FILTER', 0.3, 'HP 400', col, lw=60)
    param(d, 16, 424, 290, 'DRIVE', 0.2, 'WARM 20', col, lw=60)
    box(d, 322, 340, 636, 452, 'SOUND', DIM, right='0.09 s')
    draw_tone(d, 326, 356, 632, 448, col, cycles=(18, 40, 6), decay=7.0)
    footer(d, [('D-PAD', 'move'), ('A+↑↓', 'change'), ('L1/R1', 'algorithm'), ('R2+Y', 'dice'), ('B', 'back')])
    return img

def s_sample():
    img, d = new()
    col = TC[10]
    header(d, 'SAMPLE', SCOL['SAMPLE'], [('TRK', '11', col), ('FILE', 'DUSTY_94.WAV', TEXT)])
    tabs = ['TRIM', 'SLICE', 'LOOP', 'PLAY', 'PROCESS']
    x = 8
    for t in tabs:
        on = t == 'SLICE'; w_ = tw(t, F14) + 16
        d.rounded_rectangle([x, 36, x + w_, 56], 3, fill=col if on else PANEL2)
        text(d, (x + 8, 39), t, F14, INK if on else TEXT); x += w_ + 4
    text_r(d, 632, 40, 'ZOOM 1×   1.92 s   48 kHz   16 BIT', F12, DIM)
    slices_wave(d, 4, 62, 636, 222, col, 4)
    x = 4 + int(632 * 0.58); d.line([x, 63, x, 221], fill=WHITE, width=2)
    d.rectangle([4, 228, 636, 242], fill=(12, 12, 20), outline=LINE)
    vals = env_break(630)
    for k in range(0, 630, 1):
        h = int(5 * vals[k]); d.line([5 + k, 235 - h, 5 + k, 235 + h], fill=scale(col, 0.6))
    d.rectangle([4, 228, 636, 242], outline=WHITE)
    box(d, 4, 248, 316, 340, 'SLICE BY', col)
    chips(d, 82, 252, ['GRID', 'TRANSIENT', 'MANUAL'], 1, col, h=15, padx=4)
    param(d, 14, 276, 294, 'SENSITIVITY', 0.6, '60', col, lw=84, sel=True)
    param(d, 14, 296, 294, 'SLICES', 8 / 16, '8', col, lw=84)
    text(d, (14, 316), 'SLICE 5  0.960 s → 1.200 s', F12, TEXT); text(d, (230, 316), 'SNAP', F12, DIM); toggle(d, 266, 316, True, col)
    box(d, 322, 248, 636, 340, 'PLAY', col)
    for k, (lab, v, s, bip, c_) in enumerate([('PITCH', 0.0, '+0', True, col), ('ATTACK', 0.0, '0', False, col), ('DECAY', 0.7, '70', False, col),
                                              ('FILTER', 0.8, '12k', False, col), ('BITS', 0.5, '12', False, AMBER), ('RATE', 0.4, '26k', False, AMBER)]):
        knob(d, 352 + k * 50, 292, 17, v, c_, lab, None, bip=bip, w=3)
        text_c(d, 352 + k * 50, 286, s, F12B, TEXT)
    box(d, 4, 346, 636, 452, 'PROCESS', DIM)
    x = 14
    for s in ['NORMALIZE', 'REVERSE', 'CROP', 'FADE IN', 'FADE OUT', 'SILENCE', 'RESAMPLE', 'EXPORT SLICES']:
        w_ = tw(s, F12B) + 12
        if x + w_ > 630: break
        d.rounded_rectangle([x, 368, x + w_, 388], 3, fill=PANEL2); text(d, (x + 6, 372), s, F12B, TEXT); x += w_ + 6
    text(d, (14, 400), 'PLAY MODE', F12, DIM); chips(d, 100, 398, ['ONE SHOT', 'GATE', 'LOOP'], 0, col, h=15, padx=4)
    text(d, (14, 422), 'SLICES CUT EACH OTHER', F12, DIM); toggle(d, 160, 422, True, col)
    text(d, (230, 422), 'EVERY STEP CAN PICK ITS SLICE (X ON A STEP)', F12, DIMMER)
    footer(d, [('L1/R1', 'tab'), ('←→', 'slice'), ('A', 'listen'), ('X', 'move marker'), ('R2+A', 'add slice'), ('B', 'back')])
    return img

def s_sampling():
    img, d = new()
    col = SCOL['SAMPLING']
    header(d, 'SAMPLING', col)
    box(d, 4, 36, 228, 334, 'SOURCE', col)
    src = [('MASTER', 'resample the output', True), ('ONE TRACK', 'TRK 01-12', True), ('TWOFOLD IN', 'stereo  ·  connected', True),
           ('TWOFOLD IN L', '', True), ('TWOFOLD IN R', '', True), ('USB FROM COMPUTER', 'not in this version', False)]
    for k, (n, sub, ok) in enumerate(src):
        y = 58 + k * 30
        if k == 2:
            d.rectangle([8, y - 3, 224, y + 25], fill=mix(PANEL, col, 0.25)); d.rectangle([8, y - 3, 9, y + 25], fill=col)
        text(d, (16, y), n, F14, TEXT if ok else DIMMER)
        if sub: text(d, (16, y + 14), sub, F12, GREEN if 'connected' in sub else (DIM if ok else DIMMER))
    setting(d, 16, 248, 200, 'START', 'OVER -30 dB')
    setting(d, 16, 266, 200, 'LENGTH', '2 BARS', sel=True, col=col)
    setting(d, 16, 284, 200, 'MONITOR', 'ON')
    setting(d, 16, 302, 200, 'COUNT-IN', 'OFF')
    box(d, 234, 36, 304, 334, 'IN', DIM)
    for k, v in enumerate((0.72, 0.66)):
        meter_v(d, 248 + k * 22, 60, 300, v, GREEN, w=14)
    yth = int(300 - 240 * 0.55); d.line([242, yth, 298, yth], fill=AMBER, width=2)
    text(d, (244, 306), 'L   R', F12, DIM); text(d, (240, 320), '-30 dB', F12, AMBER)
    box(d, 310, 36, 636, 200, None)
    chip(d, 318, 42, '● REC  0:03.41', F14, WHITE, RED, padx=5, h=18)
    text_r(d, 628, 44, 'BAR 2 OF 2', F12B, TEXT)
    x0, x1 = 316, 630
    for b in range(9):
        x = x0 + b * (x1 - x0) // 8; d.line([x, 68, x, 194], fill=LINE if b % 4 else DIMMER)
    vals = env_break(int((x1 - x0) * 0.62), seed=31, hits=(0.9, 0.6, 1.0, 0.5, 0.9))
    draw_env(d, x0, 70, x1, 192, vals, RED)
    d.line([x0 + len(vals), 66, x0 + len(vals), 196], fill=WHITE, width=2)
    box(d, 310, 206, 636, 334, 'AFTER', col)
    setting(d, 320, 228, 306, 'TO', 'TRACK 11  CHOP', vcol=TC[10])
    setting(d, 320, 246, 306, 'THEN', 'SLICE 8  ·  NORMALIZE')
    setting(d, 320, 264, 306, 'NAME', 'REC_0042.WAV')
    setting(d, 320, 282, 306, 'ROOM', '618 MB MEMORY  ·  29.7 GB CARD')
    text(d, (320, 306), 'The take stays in memory until you keep it.', F12, DIM)
    box(d, 4, 340, 636, 452, 'INPUT', DIM)
    param(d, 16, 364, 300, 'GAIN', 0.6, '+6 dB', col, lw=80)
    param(d, 16, 386, 300, 'LOW CUT', 0.1, '30 Hz', col, lw=80)
    text(d, (16, 408), 'STEREO → MONO', F12, DIM); toggle(d, 120, 408, False, col)
    text(d, (330, 364), 'TWOFOLD', F12B, GREEN)
    text(d, (330, 380), 'USB  ·  48 kHz  ·  2 IN 2 OUT', F12, DIM)
    text(d, (330, 396), 'OUT GOES TO TWOFOLD TOO', F12, DIM); toggle(d, 500, 396, True, col)
    text(d, (330, 420), 'Without TWOFOLD: MASTER and ONE TRACK only.', F12, DIMMER)
    footer(d, [('A', 'arm'), ('START', 'record now'), ('B', 'stop'), ('Y', 'keep'), ('X', 'listen')])
    return img

def s_mixer():
    img, d = new()
    header(d, 'MIXER', SCOL['MIXER'])
    labels = [(62, 'DRIVE'), (88, 'DELAY'), (114, 'REVERB'), (138, 'PAN')]
    for y, s in labels: text(d, (4, y - 6), s, F12, DIMMER)
    for y, s in [(160, '+6'), (200, '0'), (260, '-12'), (320, '-24'), (390, '-48')]:
        text_r(d, 44, y - 6, s, F12, DIMMER)
    rnd = random.Random(3)
    for i, t in enumerate(TRACKS):
        x = 50 + i * 40; col = t[2]; muted = t[6]; cx = x + 18
        if i == 0: d.rectangle([x, 34, x + 37, 452], fill=mix(BG, col, 0.12))
        d.rectangle([x + 2, 36, x + 35, 38], fill=col)
        text_c(d, cx, 42, t[0][:5], F12B, col if not muted else DIM)
        for k, v in enumerate([rnd.random() * 0.5, rnd.random() * 0.6, rnd.random() * 0.7]):
            y = 62 + k * 26
            d.arc([cx - 9, y - 9, cx + 9, y + 9], 135, 405, fill=KTRACK, width=3)
            if v > 0.02: d.arc([cx - 9, y - 9, cx + 9, y + 9], 135, 135 + 270 * v, fill=col if k == 0 else (AMBER if k == 1 else SKY), width=3)
        pan = [0, 0.1, -0.15, 0.35, -0.3, -0.3, 0.2, -0.4, 0.5, 0.25, 0, -0.6][i]
        d.line([cx - 14, 138, cx + 14, 138], fill=KTRACK, width=2); d.line([cx, 134, cx, 142], fill=DIMMER)
        px = cx + int(pan * 14); d.ellipse([px - 3, 135, px + 3, 141], fill=col)
        lv = [0.78, 0.72, 0.62, 0.55, 0.5, 0.48, 0.66, 0.58, 0.4, 0.45, 0.7, 0.35][i]
        fader(d, cx - 5, 154, 392, lv, col)
        meter_v(d, cx + 8, 154, 392, 0 if muted else lv * (0.55 + 0.35 * rnd.random()), col, w=5)
        text_c(d, cx, 398, '%+.1f' % ((lv - 0.72) * 30), F12, TEXT)
        for k, s in enumerate('MS'):
            on = (s == 'M' and muted)
            chip(d, x + 3 + k * 17, 418, s, F12B, INK if on else DIM, AMBER if on else PANEL2, padx=4, h=15)
        text_c(d, cx, 438, '%02d' % (i + 1), F12, DIMMER)
    x = 534; col = SCOL['MASTER']
    d.rounded_rectangle([x, 34, 636, 452], 4, fill=PANEL)
    text_c(d, 585, 40, 'MASTER', F12B, col)
    text(d, (544, 58), 'DLY RTN', F12, DIM); bar(d, 544, 72, 84, 0.5, AMBER, h=6)
    text(d, (544, 84), 'REV RTN', F12, DIM); bar(d, 544, 98, 84, 0.4, SKY, h=6)
    text(d, (544, 110), 'COMP', F12, DIM); text_r(d, 628, 110, '-2.1', F12B, AMBER)
    text(d, (544, 126), 'LIMIT', F12, DIM); text_r(d, 628, 126, '-0.8', F12B, AMBER)
    fader(d, 560, 154, 392, 0.8, col)
    meter_v(d, 584, 154, 392, 0.82, GREEN, w=10); meter_v(d, 600, 154, 392, 0.78, GREEN, w=10)
    text_c(d, 560, 398, '0.0', F12, TEXT); text_c(d, 600, 398, '-3.2', F12, TEXT)
    text_c(d, 585, 420, 'PEAK -1.1', F12, DIM)
    footer(d, [('←→', 'strip'), ('↑↓', 'row'), ('A+↑↓', 'change'), ('A', 'mute'), ('X', 'solo'), ('Y', 'sound')])
    return img

def s_fx():
    img, d = new()
    header(d, 'FX', SCOL['FX'])
    box(d, 4, 36, 316, 300, None)
    chip(d, 12, 42, 'DELAY  ·  TAPE', F14, INK, AMBER, padx=5, h=18)
    text(d, (12, 64), 'Modelled on the PT2399 pedal.', F12, DIM)
    d.rectangle([12, 82, 308, 150], fill=(12, 12, 20), outline=LINE)
    for k in range(9):
        x = 20 + k * 32 + int(3 * math.sin(k * 1.7))
        h = int(60 * 0.78 ** k)
        c = mix(AMBER, (110, 50, 20), k / 8)
        d.rectangle([x, 148 - h, x + 8, 148], fill=c)
    text(d, (220, 86), '3/16  ·  363 ms', F12B, AMBER)
    for k, (lab, v, s) in enumerate([('TIME', 0.45, '3/16'), ('REPEATS', 0.55, '55'), ('TONE', 0.4, '40'), ('MIX', 0.3, '30')]):
        knob(d, 46 + k * 74, 190, 24, v, AMBER, lab, s)
    for k, (lab, v, s) in enumerate([('WOBBLE', 0.3, '0.6'), ('DEPTH', 0.25, '25'), ('DIRT', 0.4, '40')]):
        knob(d, 46 + k * 74, 262, 22, v, AMBER, lab, s)
    text(d, (246, 246), 'SYNC', F12, DIM); toggle(d, 280, 246, True, AMBER)
    text(d, (246, 268), 'PING', F12, DIM); toggle(d, 280, 268, True, AMBER)
    box(d, 324, 36, 636, 300, None)
    chip(d, 332, 42, 'REVERB', F14, INK, SKY, padx=5, h=18)
    text(d, (332, 64), 'From BARE!: four delay lines, mixed.', F12, DIM)
    d.rectangle([332, 82, 628, 150], fill=(12, 12, 20), outline=LINE)
    rnd = random.Random(2)
    for k in range(294):
        a = math.exp(-k / 90) * (1 - math.exp(-k / 6)) * (0.5 + 0.5 * rnd.random())
        h = int(60 * a); d.line([334 + k, 148 - h, 334 + k, 148], fill=mix(SKY, (20, 40, 80), k / 294))
    text(d, (540, 86), '2.4 s', F12B, SKY)
    for k, (lab, v, s) in enumerate([('SIZE', 0.7, '70'), ('DECAY', 0.55, '2.4s'), ('DAMP', 0.5, '50'), ('PRE', 0.2, '12ms')]):
        knob(d, 366 + k * 74, 190, 24, v, SKY, lab, s)
    for k, (lab, v, s) in enumerate([('WIDTH', 1.0, '100'), ('MOVE', 0.2, '20'), ('RETURN', 0.4, '40')]):
        knob(d, 366 + k * 74, 262, 22, v, SKY, lab, s)
    text(d, (570, 242), 'FREEZE', F12, DIM); toggle(d, 576, 256, False, SKY)
    box(d, 4, 306, 636, 452, 'SENDS', DIM, right='DELAY / REVERB')
    rnd = random.Random(8)
    for i, t in enumerate(TRACKS):
        x = 18 + i * 51; col = t[2]
        a, b = rnd.random() * 0.8, rnd.random() * 0.9
        for k, (v, c) in enumerate(((a, AMBER), (b, SKY))):
            d.rectangle([x + k * 14, 330, x + k * 14 + 9, 420], fill=(26, 26, 38))
            d.rectangle([x + k * 14, 420 - int(90 * v), x + k * 14 + 9, 420], fill=c)
        text_c(d, x + 12, 426, t[0][:5], F12B, col)
    footer(d, [('D-PAD', 'move'), ('A+↑↓', 'change'), ('L1/R1', 'delay / reverb'), ('B', 'back')])
    return img

def s_master():
    img, d = new()
    col = SCOL['MASTER']
    header(d, 'MASTER', col)
    box(d, 4, 36, 212, 250, 'BUS COMPRESSOR', col)
    for k, (lab, v, s) in enumerate([('THRESH', 0.6, '-12'), ('RATIO', 0.4, '4:1'), ('ATTACK', 0.2, '10ms'), ('RELEASE', 0.4, '120')]):
        r, c = divmod(k, 2); knob(d, 58 + c * 100, 90 + r * 84, 24, v, col, lab, s)
    text(d, (14, 214), 'GR', F12, DIM); meter_h(d, 34, 216, 200, 0.35, AMBER); text_r(d, 202, 228, '-3.5 dB', F12B, AMBER)
    box(d, 216, 36, 424, 250, 'TONE  ·  DRIVE', col)
    for k, (lab, v, s) in enumerate([('LOW CUT', 0.1, '30'), ('HIGH CUT', 0.9, '18k'), ('DRIVE', 0.2, '20'), ('WIDTH', 0.6, '100')]):
        r, c = divmod(k, 2); knob(d, 270 + c * 100, 90 + r * 84, 24, v, col, lab, s)
    chips(d, 226, 220, ['WARM', 'TAPE', 'FOLD'], 1, col, h=15, padx=5)
    box(d, 428, 36, 636, 250, 'OUTPUT', AMBER)
    modes = [('CLEAN', '48 kHz'), ('32k', ''), ('26k', 'SP-1200 RATE'), ('22k', ''), ('12k', ''), ('8k', 'PHONE')]
    for k, (m, s) in enumerate(modes):
        y = 58 + k * 20
        if m == '26k': d.rectangle([432, y - 3, 632, y + 14], fill=mix(PANEL, AMBER, 0.25)); d.rectangle([432, y - 3, 433, y + 14], fill=AMBER)
        text(d, (442, y), m, F12B, AMBER if m == '26k' else TEXT); text(d, (490, y), s, F12, DIM)
    text(d, (440, 182), 'The codec runs at this rate.', F12, DIM)
    text(d, (440, 200), 'BITS', F12, DIM); chips(d, 480, 198, ['16', '12', '8'], 1, AMBER, h=15, padx=5)
    text(d, (440, 222), 'LIMITER', F12, DIM); text_r(d, 626, 222, '-0.3 dB', F12B, TEXT)
    box(d, 4, 256, 424, 452, 'SPECTRUM', DIM)
    spectrum(d, 12, 276, 416, 432)
    for f_, x in [('50', 20), ('200', 110), ('1k', 210), ('5k', 310), ('15k', 390)]: text(d, (x, 436), f_, F12, DIMMER)
    box(d, 428, 256, 530, 452, 'SCOPE', DIM)
    cx, cy = 479, 360
    rnd = random.Random(6)
    for k in range(300):
        a = k * 0.21; r_ = 30 * (0.5 + 0.5 * rnd.random())
        d.point((cx + int(r_ * math.cos(a) * 0.8 + rnd.uniform(-3, 3)), cy + int(r_ * math.sin(a * 1.01) * 0.5 + rnd.uniform(-8, 8))), fill=GREEN)
    box(d, 534, 256, 636, 452, 'OUT', DIM)
    meter_v(d, 556, 278, 420, 0.8, GREEN, w=16); meter_v(d, 588, 278, 420, 0.76, GREEN, w=16)
    text(d, (550, 426), '-4.1 -3.8', F12, TEXT)
    footer(d, [('D-PAD', 'move'), ('A+↑↓', 'change'), ('L1/R1', 'section'), ('B', 'back')])
    return img

def s_browser(confirm=False):
    img, d = new()
    col = SCOL['BROWSER']
    header(d, 'BROWSER', col)
    tabs = ['SAMPLES', 'DRUMS', 'KITS', 'PATTERNS', 'PROJECTS']
    x = 8
    for t in tabs:
        on = t == 'SAMPLES'; w_ = tw(t, F14) + 16
        d.rounded_rectangle([x, 36, x + w_, 56], 3, fill=col if on else PANEL2)
        text(d, (x + 8, 39), t, F14, INK if on else TEXT); x += w_ + 4
    box(d, 4, 62, 196, 440, None)
    folders = [('FAVOURITES', 12, 0, 'star'), ('BUNDLED', None, 0, 'open'), ('808', 24, 1, ''), ('909', 22, 1, ''), ('FM PERC', 31, 1, ''),
               ('NOISE', 18, 1, ''), ('LO-FI', 40, 1, ''), ('BREAKS', 16, 1, 'sel'), ('MY SAMPLES', 57, 0, 'closed'), ('RECORDINGS', 9, 0, 'closed'), ('TWOFOLD', 3, 0, 'closed')]
    for k, (n, c, ind, kind) in enumerate(folders):
        y = 70 + k * 21
        if kind == 'sel': d.rectangle([8, y - 3, 192, y + 15], fill=mix(PANEL, col, 0.25))
        x = 14 + ind * 14
        if kind == 'star': star(d, x + 5, y + 6, 6, AMBER)
        elif kind == 'open': text(d, (x, y), '▼', F12, DIM)
        elif kind == 'closed': text(d, (x, y), '►', F12, DIM)
        else: d.rectangle([x + 2, y + 3, x + 8, y + 9], fill=scale(col, 0.7))
        text(d, (x + 14, y), n, F12B if kind == 'sel' else F12, TEXT)
        if c is not None: text_r(d, 188, y, str(c), F12, DIM)
    box(d, 202, 62, 430, 440, None)
    files = [('BASEMENT_LOOP.WAV', '3.84'), ('DUSTY_94.WAV', '1.92'), ('BROKEN_120.WAV', '2.00'), ('TAPE_FUNK_98.WAV', '2.45'), ('GHOST_SHUFFLE.WAV', '1.71'),
             ('HALFTIME_80.WAV', '3.00'), ('CRUNCH_BREAK.WAV', '1.88'), ('RAIN_BEAT.WAV', '4.10'), ('SWUNG_HATS_92.WAV', '2.61'), ('KNOCK_LOOP.WAV', '1.33'),
             ('WIRED_88.WAV', '2.73'), ('LOOSE_TOMS.WAV', '3.12'), ('VINYL_DRUMS.WAV', '2.05'), ('PAPER_BREAK.WAV', '1.95'), ('OFFBEAT_104.WAV', '2.30'), ('LAST_ONE.WAV', '1.50')]
    for k, (n, s) in enumerate(files):
        y = 70 + k * 23
        if k == 1:
            d.rectangle([206, y - 4, 420, y + 17], fill=mix(PANEL, col, 0.3)); d.rectangle([206, y - 4, 207, y + 17], fill=col)
            d.polygon([(210, y + 1), (210, y + 11), (217, y + 6)], fill=GREEN)
        text(d, (222, y), n, F12B if k == 1 else F12, TEXT)
        text_r(d, 416, y, s + ' s', F12, DIM)
    d.rectangle([424, 70, 426, 436], fill=PANEL2); d.rectangle([424, 70, 426, 140], fill=DIM)
    box(d, 436, 62, 636, 440, 'DUSTY_94.WAV', TEXT)
    d.rectangle([444, 82, 628, 150], fill=(12, 12, 20), outline=LINE)
    draw_env(d, 445, 84, 627, 148, env_break(183), col)
    info = [('LENGTH', '1.92 s'), ('RATE', '48 kHz'), ('BITS', '16  STEREO'), ('TEMPO', '94 BPM (FOUND)'), ('SIZE', '361 KB'), ('LICENCE', 'CC0')]
    for k, (a, b) in enumerate(info): setting(d, 446, 162 + k * 17, 180, a, b, vcol=GREEN if a == 'LICENCE' else TEXT)
    y = 274
    for b, s in [('A', 'LOAD TO TRACK 11'), ('R2+A', 'LOAD AS 8 SLICES'), ('X', 'FAVOURITE'), ('L1', 'LISTEN: ON')]:
        x = 446 + chip(d, 446, y, b, F12B, INK, (200, 200, 214), padx=3, h=14) + 4
        text(d, (x, y + 1), s, F12, TEXT); y += 22
    text(d, (446, 372), 'Plays as you move.', F12, DIM)
    text(d, (446, 388), 'At the tempo: L2.', F12, DIM)
    text(d, (8, 444), 'SD CARD  ·  29.7 GB FREE  ·  1,284 FILES', F12, DIMMER)
    footer(d, [('L1/R1', 'tab'), ('←→', 'column'), ('A', 'load'), ('X', 'favourite'), ('B', 'back')])
    if confirm:
        img = dimmed(img, 0.6); d = draw(img)
        d.rounded_rectangle([120, 150, 520, 330], 6, fill=PANEL, outline=col, width=2)
        text(d, (138, 162), 'REPLACE THE SAMPLE?', F16, col)
        for k, s in enumerate(wrap('Track 11 has slice edits that are not saved. Load DUSTY_94.WAV anyway?', F14N, 360)):
            text(d, (138, 194 + k * 18), s, F14N, TEXT)
        x = 138
        for b, s, c in [('A', 'LOAD', col), ('Y', 'SAVE FIRST', PANEL2), ('B', 'CANCEL', PANEL2)]:
            x += chip(d, x, 288, b + '  ' + s, F14, INK if c == col else TEXT, c, padx=8, h=24) + 8
        footer(d, [('A', 'load'), ('Y', 'save first'), ('B', 'cancel')])
    return img

def s_project(name_entry=False):
    img, d = new()
    col = SCOL['PROJECT']
    header(d, 'PROJECT', col)
    box(d, 4, 36, 300, 392, 'ON THE CARD', DIM)
    projs = [('BASEMENT SESSIONS', '09 OCT  ·  9.4 MB', True), ('TAPE HISS', '07 OCT  ·  22 MB', False), ('OCT 09 JAM', '09 OCT  ·  1.2 MB', False),
             ('LUNETTA SYNC TEST', '08 OCT  ·  0.3 MB', False), ('TWOFOLD RESAMPLES', '06 OCT  ·  41 MB', False), ('UNTITLED 7', '05 OCT  ·  0.1 MB', False),
             ('KIDS BEATS', '02 OCT  ·  3.0 MB', False), ('NOISE FLOOR', '30 SEP  ·  12 MB', False)]
    for k, (n, s, cur) in enumerate(projs):
        y = 58 + k * 41
        if k == 0: d.rectangle([8, y - 4, 296, y + 33], fill=mix(PANEL, col, 0.22)); d.rectangle([8, y - 4, 9, y + 33], fill=col)
        text(d, (18, y), n, F14, TEXT); text(d, (18, y + 17), s, F12, DIM)
        if cur: chip(d, 240, y, 'OPEN', F12B, INK, GREEN, padx=4, h=14)
    box(d, 306, 36, 636, 196, None)
    text(d, (316, 44), 'BASEMENT SESSIONS', F20, TEXT)
    for k, (a, b) in enumerate([('TEMPO', '124 BPM'), ('PATTERNS', '7 USED'), ('SONG', '48 BARS  ·  1:33'), ('SAMPLES', '2  ·  9.4 MB'), ('SAVED', '2 MIN AGO')]):
        setting(d, 316, 74 + k * 18, 310, a, b)
    x = 316
    for b, s in [('A', 'SAVE'), ('X', 'SAVE AS'), ('Y', 'NEW'), ('R2+X', 'COPY')]:
        x += chip(d, x, 172, b, F12B, INK, (200, 200, 214), padx=3, h=14) + 3
        text(d, (x, 173), s, F12, TEXT); x += tw(s, F12) + 10
    box(d, 306, 202, 636, 392, 'RENDER TO WAV', col)
    text(d, (316, 226), 'WHAT', F12, DIM); chips(d, 370, 224, ['SONG', 'PATTERN ×4', 'STEMS'], 2, col, h=15, padx=4)
    setting(d, 316, 250, 310, 'FORMAT', 'WAV  48 kHz  24 BIT')
    setting(d, 316, 268, 310, 'TAIL', '2 BARS')
    setting(d, 316, 286, 310, 'OUTPUT MODE', 'AS SET (26k)', vcol=AMBER)
    setting(d, 316, 304, 310, 'TO', 'RENDERS/BASEMENT_SESSIONS/')
    for i in range(12):
        x = 316 + i * 26
        d.rectangle([x, 330, x + 22, 344], fill=TC[i] if i < 8 else (PANEL2 if i > 8 else mix(PANEL2, TC[i], 0.5)))
    text(d, (316, 350), 'STEMS  8 OF 12  ·  CONGA', F12B, TEXT); text_r(d, 626, 350, '00:21 LEFT', F12, DIM)
    text(d, (316, 370), 'Renders faster than real time.', F12, DIM)
    box(d, 4, 398, 636, 452, None)
    chip(d, 14, 408, 'R2+START', F12B, INK, (200, 200, 214), padx=3, h=14)
    text(d, (80, 409), 'USB DRIVE', F12B, TEXT)
    text(d, (14, 428), 'Plug into a computer and copy samples onto the card without taking it out.', F12, DIM)
    chip(d, 520, 42, '✓ SAVED', F14, INK, GREEN, padx=6, h=20)
    footer(d, [('↑↓', 'project'), ('A', 'open / save'), ('X', 'save as'), ('START', 'render'), ('B', 'back')])
    if name_entry:
        img = dimmed(img, 0.62); d = draw(img)
        d.rounded_rectangle([36, 70, 604, 448], 6, fill=PANEL, outline=col, width=2)
        text(d, (52, 80), 'SAVE AS', F16, col)
        d.rectangle([52, 106, 588, 138], fill=(10, 10, 16), outline=col)
        name = 'BASEMENT SESSIONS 2'
        text(d, (62, 113), name, F20, TEXT)
        cx = 62 + tw(name, F20); d.rectangle([cx + 1, 113, cx + 10, 133], fill=col)
        text_r(d, 584, 142, '%d / 24' % len(name), F12, DIM)
        rows = ['ABCDEFGHIJ', 'KLMNOPQRST', 'UVWXYZ0123', '456789-_.&']
        for r, row in enumerate(rows):
            for k, ch in enumerate(row):
                x, y = 73 + k * 50, 166 + r * 44
                sel = ch == 'S'
                c = TC[(r * 3 + k) % 12]
                d.rounded_rectangle([x, y, x + 44, y + 36], 5, fill=c if sel else mix(PANEL, c, 0.16), outline=WHITE if sel else scale(c, 0.6), width=2 if sel else 1)
                text_c(d, x + 22, y + 8, ch, F20, INK if sel else TEXT)
        acts = [('SPACE', 140), ('DELETE', 120), ('CLEAR', 100), ('DONE', 104)]
        x = 73
        for s, w_ in acts:
            done = s == 'DONE'
            d.rounded_rectangle([x, 346, x + w_, 376], 5, fill=GREEN if done else PANEL2)
            if s == 'DELETE':
                d.polygon([(x + 18, 361), (x + 26, 353), (x + 40, 353), (x + 40, 369), (x + 26, 369)], outline=TEXT)
                text(d, (x + 48, 353), s, F14, TEXT)
            else: text_c(d, x + w_ // 2, 353, s, F14, INK if done else TEXT)
            x += w_ + 10
        text(d, (73, 392), 'Holding ↑↓ on a letter steps through the alphabet,', F12, DIM)
        text(d, (73, 406), 'like a high-score table.', F12, DIM)
        footer(d, [('D-PAD', 'move'), ('A', 'type'), ('B', 'delete'), ('L1/R1', 'cursor'), ('START', 'done')])
    return img

def s_usb():
    img, d = new()
    col = SCOL['PROJECT']
    header(d, 'USB DRIVE', col)
    lx, ly = 120, 90
    d.rounded_rectangle([lx, ly, lx + 120, ly + 78], 4, outline=TEXT, width=2)
    d.rectangle([lx + 8, ly + 8, lx + 112, ly + 70], fill=PANEL2)
    for k in range(4): d.rectangle([lx + 16, ly + 16 + k * 12, lx + 16 + 20 + k * 14, ly + 22 + k * 12], fill=TC[k * 3])
    d.polygon([(lx - 14, ly + 80), (lx + 134, ly + 80), (lx + 146, ly + 92), (lx - 26, ly + 92)], outline=TEXT)
    rx, ry = 420, 70
    d.rounded_rectangle([rx, ry, rx + 90, ry + 140], 10, outline=TEXT, width=2)
    d.rectangle([rx + 10, ry + 10, rx + 80, ry + 62], fill=PANEL2)
    for r in range(4):
        for k in range(8):
            d.rectangle([rx + 14 + k * 8, ry + 16 + r * 11, rx + 19 + k * 8, ry + 23 + r * 11], fill=TC[r * 3] if (r + k) % 3 == 0 else mix(PANEL2, TC[r * 3], 0.3))
    d.ellipse([rx + 14, ry + 76, rx + 34, ry + 96], outline=TEXT); d.ellipse([rx + 58, ry + 76, rx + 78, ry + 96], outline=TEXT)
    d.ellipse([rx + 22, ry + 108, rx + 38, ry + 124], outline=DIM); d.ellipse([rx + 54, ry + 108, rx + 70, ry + 124], outline=DIM)
    for k in range(9):
        x = 262 + k * 16
        d.rectangle([x, 136, x + 8, 140], fill=col if k % 3 == 1 else DIMMER)
    text_c(d, W // 2, 234, 'CONNECTED AS A DRIVE', F24, TEXT)
    text_c(d, W // 2, 270, 'The computer sees these folders on the card:', F14N, DIM)
    names = ['SAMPLES', 'KITS', 'PATTERNS', 'PROJECTS', 'RENDERS']
    total = sum(tw(n, F14) + 18 for n in names) + 8 * 4
    x = (W - total) // 2
    for k, n in enumerate(names):
        x += chip(d, x, 294, n, F14, INK, TC[k * 2 + 1], padx=9, h=22) + 8
    for k, s in enumerate(['Copy WAV files into SAMPLES. Folders inside it are fine.', 'Eject on the computer, then press B here.',
                           'Patterns keep playing from memory meanwhile.']):
        text_c(d, W // 2, 336 + k * 20, s, F14N, TEXT)
    text_c(d, W // 2, 412, '29.7 GB FREE  ·  3 NEW FILES  ·  LAST CHANGE 12 S AGO', F12, DIM)
    footer(d, [('B', 'done (eject first)'), ('START', 'play / stop')])
    return img

def s_midi():
    img, d = new()
    col = SCOL['MIDI']
    header(d, 'MIDI', col)
    box(d, 4, 36, 214, 452, 'USB PORT', col)
    chips(d, 14, 56, ['COMPUTER', 'DEVICES'], 0, col, h=18, padx=6)
    text(d, (14, 82), '● connected to a computer', F12, GREEN)
    text(d, (14, 96), 'class-compliant MIDI + drive', F12, DIM)
    rows = [('NOTES OUT', 'ON'), ('NOTES IN', 'PLAY + REC'), ('CLOCK OUT', 'ON'), ('CLOCK IN', 'OFF'), ('START/STOP', 'SEND'),
            ('SONG POSITION', 'SEND'), ('PROGRAM CHG', 'PATTERN'), ('CC OUT', 'MACROS'), ('CC IN', 'LEARN'), ('THRU', 'OFF'), ('CHANNEL IN', 'ALL')]
    for k, (a, b) in enumerate(rows):
        setting(d, 16, 122 + k * 21, 186, a, b, sel=k == 2, col=col)
    text(d, (14, 360), 'DEVICES mode: plug keyboards,', F12, DIMMER)
    text(d, (14, 374), 'pads or TWOFOLD into the', F12, DIMMER)
    text(d, (14, 388), 'R36S with an OTG adapter.', F12, DIMMER)
    box(d, 220, 36, 434, 452, 'TRACKS', col)
    for x, s in [(240, 'TRACK'), (300, 'SEND'), (356, 'CH'), (384, 'NOTE')]: text(d, (x, 54), s, F12, DIMMER)
    sends = ['BOTH', 'INT', 'INT', 'INT', 'BOTH', 'BOTH', 'INT', 'MIDI', 'INT', 'INT', 'INT', 'INT']
    notes = ['C1 36', 'D1 38', 'D#1 39', 'C#1 37', 'F#1 42', 'A#1 46', 'F1 41', 'D#4 63', 'A#3 70', 'D#2 51', 'C3 60', 'G#3 68']
    for i, t in enumerate(TRACKS):
        y = 72 + i * 31 // 1 if False else 72 + i * 30
        d.rectangle([228, y, 231, y + 14], fill=t[2])
        text(d, (240, y + 1), t[0][:6], F12B, t[2])
        s = sends[i]; c = {'INT': PANEL2, 'MIDI': col, 'BOTH': mix(col, WHITE, 0.3)}[s]
        chip(d, 300, y, s, F12B, INK if s != 'INT' else DIM, c, padx=4, h=15)
        text(d, (358, y + 1), '10' if i != 7 else '02', F12, TEXT)
        text(d, (384, y + 1), notes[i], F12, TEXT)
    box(d, 440, 36, 636, 452, 'MONITOR', DIM)
    log = [('→', 'CLOCK', '×24', AMBER), ('→', 'NOTE 10', 'C1 127', AMBER), ('←', 'CC 1 #74', 'KICK TONE', GREEN), ('→', 'NOTE 10', 'F#1 96', AMBER),
           ('→', 'NOTE 02', 'D#4 100', AMBER), ('←', 'NOTE 10', 'D1 88', GREEN), ('→', 'NOTE 10', 'A#1 90', AMBER), ('←', 'PROG 1', 'A02 NEXT', GREEN),
           ('→', 'CLOCK', '×24', AMBER), ('→', 'NOTE 10', 'C1 127', AMBER), ('→', 'NOTE 10', 'F#1 70', AMBER), ('←', 'CC 1 #71', 'KICK PUNCH', GREEN),
           ('→', 'NOTE 10', 'D1 110', AMBER), ('→', 'CLOCK', '×24', AMBER), ('→', 'NOTE 10', 'F#1 64', AMBER), ('←', 'NOTE 10', 'F#1 101', GREEN),
           ('→', 'NOTE 02', 'D#4 92', AMBER), ('→', 'START', '', AMBER), ('→', 'SPP', 'BAR 1', AMBER), ('←', 'CC 1 #74', 'KICK TONE', GREEN),
           ('→', 'NOTE 10', 'C1 127', AMBER), ('→', 'CLOCK', '×24', AMBER)]
    for k, (a, m, v, c) in enumerate(log):
        y = 56 + k * 17
        text(d, (448, y), a, F12B, c); text(d, (464, y), m, F12, TEXT if k < 20 else DIM); text_r(d, 628, y, v, F12, DIM)
    footer(d, [('↑↓', 'setting'), ('A+←→', 'change'), ('L1/R1', 'column'), ('X', 'learn'), ('B', 'back')])
    return img

def s_sync():
    img, d = new()
    col = SCOL['SYNC']
    header(d, 'SYNC', col)
    box(d, 4, 36, 214, 452, 'CLOCK', col)
    text(d, (14, 58), 'SOURCE', F12, DIM)
    chips(d, 14, 74, ['INTERNAL', 'MIDI', 'AUTO'], 0, col, h=18, padx=5)
    text(d, (14, 108), '124.0', F32, TEXT); text(d, (110, 124), 'BPM', F14, DIM)
    for k in range(4):
        x = 16 + k * 22; on = k == 2
        d.ellipse([x, 152, x + 14, 166], fill=col if on else KTRACK)
    text(d, (110, 152), 'BEAT 3', F12, DIM)
    x = 14 + chip(d, 14, 178, 'Y', F12B, INK, (200, 200, 214), padx=3, h=14) + 4
    text(d, (x, 179), 'TAP TEMPO', F12, TEXT)
    param(d, 16, 210, 186, 'SWING', 0.54, '54%', col, lw=56, sel=True)
    param(d, 16, 232, 186, 'NUDGE', 0.0, '0', col, bip=True, lw=56)
    setting(d, 16, 260, 186, 'MIDI IN', 'FOLLOWS START')
    setting(d, 16, 278, 186, 'LATENCY', '1.3 ms')
    text(d, (14, 312), 'AUTO: follows MIDI clock', F12, DIMMER)
    text(d, (14, 326), 'when it arrives, else', F12, DIMMER)
    text(d, (14, 340), 'runs on its own.', F12, DIMMER)
    box(d, 220, 36, 636, 252, 'HEADPHONE JACK', AMBER)
    modes = [('STEREO', 'MIX', 'MIX'), ('SYNC + MONO', 'PULSE', 'MIX'), ('SPLIT', 'TRK 1-6', 'TRK 7-12')]
    for k, (n, l, r) in enumerate(modes):
        x, y = 230 + k * 136, 58
        on = k == 1
        d.rounded_rectangle([x, y, x + 128, y + 82], 5, fill=mix(PANEL, AMBER, 0.2) if on else PANEL2, outline=AMBER if on else LINE, width=2 if on else 1)
        text_c(d, x + 64, y + 6, n, F12B, AMBER if on else TEXT)
        for j, (ch, v) in enumerate((('L', l), ('R', r))):
            yy = y + 30 + j * 24
            text(d, (x + 10, yy), ch, F14, DIM)
            if v == 'PULSE':
                pts = []; xx = x + 26
                for p in range(3): pts += [(xx, yy + 12), (xx, yy + 2), (xx + 4, yy + 2), (xx + 4, yy + 12), (xx + 14, yy + 12)]; xx += 14
                text_r(d, x + 122, yy, 'PULSE', F12, AMBER)
                d.line(pts, fill=AMBER, width=2)
            else:
                rnd = random.Random(k * 10 + j)
                for p in range(40):
                    h = int(5 * rnd.random()); d.line([x + 28 + p, yy + 7 - h, x + 28 + p, yy + 7 + h], fill=scale(TC[(k * 4 + j * 6) % 12], 0.8))
                text_r(d, x + 122, yy, v, F12, TEXT)
    text(d, (232, 154), 'PULSES', F12, DIM); chips(d, 290, 152, ['1', '2', '4', '24'], 1, AMBER, h=15, padx=6)
    text(d, (410, 154), 'PER BEAT', F12, DIM)
    param(d, 232, 180, 392, 'WIDTH', 0.25, '5 ms', AMBER, lw=72)
    param(d, 232, 200, 392, 'LEVEL', 1.0, '100%', AMBER, lw=72)
    param(d, 232, 220, 392, 'OFFSET', -0.15, '-3 ms', AMBER, bip=True, lw=72)
    box(d, 220, 258, 636, 452, 'WHAT GOES OUT', DIM)
    x0, x1 = 236, 624
    for i in range(16):
        x = x0 + i * (x1 - x0) / 16
        d.rectangle([int(x) + 2, 282, int(x) + 18, 296], fill=TC[0] if TRACKS[0][3][i] != '.' else (mix(PANEL, TC[0], 0.25)))
        if i % 2 == 0:
            d.line([int(x) + 2, 330, int(x) + 2, 310, int(x) + 6, 310, int(x) + 6, 330], fill=AMBER, width=2)
        text(d, (int(x) + 4, 334), str(i + 1) if i % 4 == 0 else '', F12, DIM)
    d.line([x0, 330, x1, 330], fill=scale(AMBER, 0.5))
    text(d, (232, 352), 'PRESETS', F12, DIM)
    chips(d, 232, 368, ['POCKET OPERATOR', 'VOLCA', 'LUNETTA (4)', 'DAW (MIDI)'], 0, AMBER, h=16, padx=5)
    text(d, (232, 396), 'Pocket Operators and Volcas: 2 per beat, left channel.', F12, DIM)
    text(d, (232, 410), 'With SYNC + MONO the beat is in mono on the right.', F12, DIM)
    footer(d, [('↑↓', 'setting'), ('A+←→', 'change'), ('Y', 'tap'), ('B', 'back')])
    return img

def s_settings():
    img, d = new()
    col = SCOL['SETTINGS']
    header(d, 'SETTINGS', col)
    box(d, 4, 36, 170, 452, None)
    cats = ['AUDIO', 'DISPLAY', 'CONTROLS', 'PADS', 'POWER', 'STORAGE', 'BUTTON MAP', 'DIAGNOSTICS', 'ABOUT']
    for k, c in enumerate(cats):
        y = 48 + k * 30
        if c == 'CONTROLS': d.rectangle([8, y - 6, 166, y + 18], fill=col); text(d, (18, y), c, F14, INK)
        else: text(d, (18, y), c, F14, TEXT)
    box(d, 176, 36, 636, 452, 'CONTROLS', col)
    param(d, 190, 62, 430, 'STICK DEAD ZONE', 0.08, '8%', col, lw=150, sel=True)
    text(d, (190, 85), 'STICK CURVE', F12, DIM); chips(d, 340, 83, ['STRAIGHT', 'SOFT', 'STEEP'], 1, col, h=15, padx=5)
    rows = [('LEFT STICK ←→', 'TUNE'), ('LEFT STICK ↑↓', 'LENGTH'), ('RIGHT STICK', 'PERFORM FX'), ('STICK LET GO', 'SPRINGS BACK'),
            ('BUTTON REPEAT', '80 ms'), ('SWAP A AND B', 'OFF'), ('PAD VELOCITY', 'R2 = ACCENT'), ('SELECT + D-PAD', 'JUMP')]
    for k, (a, b) in enumerate(rows):
        setting(d, 190, 110 + k * 21, 430, a, b)
    box(d, 182, 286, 630, 446, 'STICKS NOW', DIM)
    for k, (dx, dy, raw) in enumerate(((-0.05, 0.04, 'X 2031  Y 2086'), (0.55, -0.3, 'X 3210  Y 1540'))):
        cx = 260 + k * 200
        d.ellipse([cx - 52, 366 - 52, cx + 52, 366 + 52], outline=LINE, width=2)
        d.ellipse([cx - 6, 360, cx + 6, 372], outline=AMBER)
        d.line([cx - 48, 366, cx + 48, 366], fill=(30, 30, 44)); d.line([cx, 318, cx, 414], fill=(30, 30, 44))
        px, py = cx + dx * 50, 366 + dy * 50
        d.ellipse([px - 5, py - 5, px + 5, py + 5], fill=col)
        text_c(d, cx, 424, ('LEFT  ' if k == 0 else 'RIGHT  ') + raw, F12, DIM)
    chip(d, 556, 300, 'CALIBRATE', F12B, INK, col, padx=4, h=15)
    footer(d, [('↑↓', 'setting'), ('A+←→', 'change'), ('L1/R1', 'page'), ('B', 'back')])
    return img

def s_controls(home):
    img, d = new()
    col = SCOL['SETTINGS']
    header(d, 'BUTTON MAP', col)
    bx0, by0, bx1, by1 = 214, 52, 426, 448
    for x, s in ((bx0 + 14, 'L2'), (bx0 + 52, 'L1'), (bx1 - 82, 'R1'), (bx1 - 44, 'R2')):
        d.rounded_rectangle([x, 40, x + 30, 52], 3, fill=PANEL2, outline=DIM); text_c(d, x + 15, 40, s, F12B, TEXT)
    d.rounded_rectangle([bx0, by0, bx1, by1], 22, fill=(36, 36, 52), outline=DIM, width=2)
    sx0, sy0 = bx0 + 18, by0 + 18
    shot = home.resize((176, 132), Image.LANCZOS)
    d.rectangle([sx0 - 3, sy0 - 3, sx0 + 178, sy0 + 134], fill=(0, 0, 0))
    img.paste(shot, (sx0, sy0))
    dcx, dcy = bx0 + 50, by0 + 200
    d.rectangle([dcx - 8, dcy - 24, dcx + 8, dcy + 24], fill=(16, 16, 22)); d.rectangle([dcx - 24, dcy - 8, dcx + 24, dcy + 8], fill=(16, 16, 22))
    acx, acy = bx1 - 50, by0 + 200
    for lab, dx, dy in (('X', 0, -20), ('Y', -20, 0), ('A', 20, 0), ('B', 0, 20)):
        d.ellipse([acx + dx - 10, acy + dy - 10, acx + dx + 10, acy + dy + 10], fill=(16, 16, 22), outline=DIM)
        text_c(d, acx + dx, acy + dy - 7, lab, F12B, TEXT)
    lsx, lsy, rsx, rsy = bx0 + 72, by0 + 286, bx1 - 72, by0 + 286
    for x, y in ((lsx, lsy), (rsx, rsy)):
        d.ellipse([x - 24, y - 24, x + 24, y + 24], fill=(22, 22, 30), outline=DIM)
        d.ellipse([x - 14, y - 14, x + 14, y + 14], fill=(46, 46, 62))
    sel_x, st_x, ss_y = 300, 340, by0 + 348
    for x in (sel_x, st_x): d.rounded_rectangle([x - 14, ss_y - 5, x + 14, ss_y + 5], 5, fill=(16, 16, 22), outline=DIM)
    text_c(d, sel_x, ss_y + 8, 'SELECT', F12, DIMMER); text_c(d, st_x, ss_y + 8, 'START', F12, DIMMER)
    left = [(46, 'L1  HOLD', 'pads, tracks 1-8', (bx0 + 67, 46)), (90, 'L2  HOLD', 'pads 9-12, fills, repeats', (bx0 + 29, 46)),
            (200, 'D-PAD', 'move', (dcx - 24, dcy)), (286, 'LEFT STICK', 'track macros, L3 latches', (lsx - 24, lsy)),
            (372, 'SELECT', 'menu; hold + D-pad: jump', (sel_x - 14, ss_y))]
    right = [(46, 'R1  HOLD', 'record stick motion', (bx1 - 67, 46)), (90, 'R2  HOLD', 'edit: copy paste clear undo', (bx1 - 29, 46)),
             (160, 'X', 'step details', (acx, acy - 30)), (186, 'Y', 'track sound', (acx - 30, acy)), (212, 'A', 'hit, choose', (acx + 30, acy)),
             (238, 'B', 'back', (acx, acy + 30)), (286, 'RIGHT STICK', 'perform FX, R3 latches', (rsx + 24, rsy)), (372, 'START', 'play / stop', (st_x + 14, ss_y))]
    for y, a, b, (tx, ty) in left:
        text(d, (8, y), a, F12B, TEXT); text(d, (8, y + 13), b, F12, DIM)
        d.line([8 + max(tw(a, F12B), tw(b, F12)) + 6, y + 10, tx, ty], fill=DIMMER)
    for y, a, b, (tx, ty) in right:
        text_r(d, 632, y, a, F12B, TEXT); text_r(d, 632, y + 13, b, F12, DIM)
        d.line([632 - max(tw(a, F12B), tw(b, F12)) - 6, y + 10, tx, ty], fill=DIMMER)
    text(d, (8, 430), 'Drawing, not to scale.', F12, DIMMER)
    footer(d, [('B', 'back')])
    return img

def s_diag():
    img, d = new()
    col = SCOL['SETTINGS']
    header(d, 'DIAGNOSTICS', col)
    box(d, 4, 36, 250, 176, 'BUTTONS', DIM)
    btns = [('L2', 14, 58), ('L1', 50, 58), ('R1', 168, 58), ('R2', 204, 58), ('↑', 40, 86), ('←', 16, 108), ('→', 64, 108), ('↓', 40, 130),
            ('X', 196, 86), ('Y', 172, 108), ('A', 220, 108), ('B', 196, 130), ('SEL', 96, 148), ('START', 132, 148), ('L3', 96, 108), ('R3', 136, 108)]
    pressed = {'A', 'L1'}
    for s, x, y in btns:
        on = s in pressed; w_ = max(22, tw(s, F12B) + 8)
        d.rounded_rectangle([x, y, x + w_, y + 16], 3, fill=GREEN if on else PANEL2)
        text_c(d, x + w_ // 2, y + 2, s, F12B, INK if on else DIM)
    box(d, 4, 182, 250, 300, 'STICKS', DIM)
    for k, (dx, dy, raw) in enumerate(((-0.05, 0.04, '2031 2086'), (0.55, -0.3, '3210 1540'))):
        cx = 64 + k * 116
        stick(d, cx, 238, dx, dy, col, r=34, dead=4)
        text_c(d, cx, 278, raw, F12, DIM)
    box(d, 256, 36, 636, 176, 'AUDIO', DIM)
    text(d, (266, 58), 'RK817  ·  I2S  ·  48000 Hz  ·  64 FRAMES  ·  1.3 ms', F12B, TEXT)
    x = 266
    for s in ['TONE 1k', 'SWEEP', 'SILENCE', 'LEFT', 'RIGHT', 'SPEAKER']:
        x += chip(d, x, 80, s, F12B, INK if s == 'SWEEP' else TEXT, AMBER if s == 'SWEEP' else PANEL2, padx=5, h=18) + 6
    text(d, (266, 110), 'L', F12, DIM); meter_h(d, 300, 112, 620, 0.62, GREEN)
    text(d, (266, 128), 'R', F12, DIM); meter_h(d, 300, 130, 620, 0.6, GREEN)
    setting(d, 266, 150, 360, 'DROPOUTS SINCE BOOT', '0', vcol=GREEN)
    box(d, 256, 182, 636, 300, 'CPU', DIM)
    for k, (n, v, c) in enumerate([('CORE 0  SCREEN', 0.18, SKY), ('CORE 1  AUDIO', 0.41, AMBER), ('CORE 2', 0.0, DIM), ('CORE 3', 0.0, DIM)]):
        y = 204 + k * 18
        text(d, (266, y), n, F12, TEXT if v else DIMMER); bar(d, 380, y + 2, 200, v, c); text_r(d, 626, y, '%d%%' % int(v * 100) if v else 'IDLE', F12B, TEXT if v else DIMMER)
    text(d, (266, 280), '1416 MHz  ·  52 °C', F12, DIM)
    box(d, 4, 306, 636, 452, 'SYSTEM', DIM)
    info = [('BOOT', 'u-boot 2017.09'), ('DEVICE TREE', 'rk3326-r36s-linux.dtb'), ('SCREEN', '640x480  from u-boot'), ('SD CARD', 'dw_mmc  4 bit  50 MHz'),
            ('USB', 'dwc2  device  MIDI + drive'), ('MEMORY', '1008 MB  samples 94 MB'), ('BATTERY', '3.92 V  78%  not charging'), ('UPTIME', '00:41:12')]
    for k, (a, b) in enumerate(info):
        c, r = divmod(k, 4)
        setting(d, 16 + c * 314, 330 + r * 28, 290, a, b)
    text_r(d, 628, 438, 'EXAMPLE VALUES', F12, DIMMER)
    footer(d, [('L1+R1+START', 'leave'), ('A', 'run test')])
    return img

# ---------------------------------------------------------------- sheet
def main():
    out = sys.argv[1]
    sd = os.path.join(out, 'screens'); os.makedirs(sd, exist_ok=True)
    home = s_pattern()
    shots = [
        ('boot', 'BOOT', 'On in about 2 s. The log is also a hardware check.', s_boot()),
        ('menu', 'MENU', 'SELECT opens every screen. SELECT + D-pad jumps.', s_menu()),
        ('pattern', 'PATTERN', 'Home. 12 tracks by 16 steps, a synth track selected.', home),
        ('pattern-sample', 'PATTERN', 'A sample track selected: its slices under the grid.', s_pattern(10, (10, 6), 'sample')),
        ('step', 'STEP', 'X on a step: velocity, nudge, ratchet, chance, locks.', s_step()),
        ('pads', 'PADS', 'Hold L1: D-pad and ABXY play 8 tracks and record.', s_pads()),
        ('generate', 'GENERATE', 'Euclid per track, dice with a wildness amount, fills.', s_generate()),
        ('perform', 'PERFORM', 'Right stick over 8 live effects. Mutes, scenes, motion.', s_perform()),
        ('patterns', 'PATTERNS', 'Banks A-H, 16 patterns each. Queue the next one.', s_patterns()),
        ('song', 'SONG', 'A chain of patterns with repeats.', s_song()),
        ('kit', 'KIT', 'All 12 sounds: engine, level, pan, choke, MIDI note.', s_kit()),
        ('sound-synth', 'SOUND  SYNTH', "thump's drum engine, one section at a time.", s_sound_synth()),
        ('sound-fm', 'SOUND  FM', '4 operators, 8 algorithms, pitch envelope.', s_sound_fm()),
        ('sample', 'SAMPLE', 'Trim, slice, loop, process. Bits and rate.', s_sample()),
        ('sampling', 'SAMPLING', "Record TWOFOLD's inputs, or resample the output.", s_sampling()),
        ('mixer', 'MIXER', '12 strips and the master. Drive and sends per track.', s_mixer()),
        ('fx', 'FX', 'Tape delay after the PT2399 pedal, reverb, sends.', s_fx()),
        ('master', 'MASTER', 'Compressor, tone, limiter, and the codec rate.', s_master()),
        ('browser', 'BROWSER', 'Samples, drums, kits, patterns, projects. Plays as you move.', s_browser()),
        ('confirm', 'CONFIRM', 'Asks before throwing work away.', s_browser(confirm=True)),
        ('project', 'PROJECT', 'Save, open, render song, loops or stems to WAV.', s_project()),
        ('name', 'NAME', 'Typing with the D-pad.', s_project(name_entry=True)),
        ('usb-drive', 'USB DRIVE', 'Plugged into a computer, the card is a drive.', s_usb()),
        ('midi', 'MIDI', 'USB to a computer or to devices. Notes, clock, monitor.', s_midi()),
        ('sync', 'SYNC', 'Clock, tap, and sync pulses out of the headphone jack.', s_sync()),
        ('settings', 'SETTINGS', 'The CONTROLS page: dead zone, curves, assignments.', s_settings()),
        ('button-map', 'BUTTON MAP', 'What every button does.', s_controls(home)),
        ('diagnostics', 'DIAGNOSTICS', 'Bring-up page. The values are examples.', s_diag()),
    ]
    for k, (key, title, desc, img) in enumerate(shots):
        img.save(os.path.join(sd, '%02d-%s.png' % (k + 1, key)))
    cols, gap, cap, top = 4, 28, 44, 96
    rows = math.ceil(len(shots) / cols)
    SW = cols * W + (cols + 1) * gap
    SH = top + rows * (H + cap + gap) + gap
    sheet = Image.new('RGB', (SW, SH), (6, 6, 10))
    d = draw(sheet)
    big_text(sheet, (gap, 22), 'R36S RHYTHM MACHINE', F24, TEXT, 2)
    x = gap + tw('R36S RHYTHM MACHINE', F24) * 2 + 24
    text(d, (x, 26), 'ALL SCREENS  ·  DRAFT 1  ·  2026-10-09', F20, DIM)
    text(d, (x, 52), '640x480, drawn pixel for pixel in Terminus. Working name STICKS. Values are examples.', F16, DIMMER)
    for k, c in enumerate(TC): d.rectangle([gap + k * 60, 80, gap + k * 60 + 54, 83], fill=c)
    for idx, (key, title, desc, img) in enumerate(shots):
        r, c = divmod(idx, cols)
        x = gap + c * (W + gap); y = top + r * (H + cap + gap)
        sheet.paste(img, (x, y))
        d.rectangle([x - 1, y - 1, x + W, y + H], outline=(40, 40, 56))
        text(d, (x, y + H + 8), '%02d' % (idx + 1), F16, DIMMER)
        text(d, (x + 30, y + H + 8), title, F16, TEXT)
        text(d, (x + 30, y + H + 27), desc, F14N, DIM)
    sheet.save(os.path.join(out, 'sheet.png'))
    print(os.path.join(out, 'sheet.png'), sheet.size, len(shots), 'screens')

main()
