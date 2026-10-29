from gen_common import *
import os, math

OUT = '../Assets'; os.makedirs(OUT, exist_ok=True); SRC = '../Source'
PAD_C = (150, 206); PAD_SZ = 190
DISPLAY = (284, 98, 432, 216)
KEYS = [('freeze', 'FREEZE', 'hold the tail', (772, 122, 160, 58)),
        ('latch',  'LATCH',  'throw stays on', (772, 194, 160, 58))]
RET_METER = (772, 276, 160, 8)
CHIPS = [('vox', 'LEAD VOX'), ('backing', 'BACKING'), ('guitar', 'GUITAR'), ('drums', 'DRUMS'), ('keys', 'KEYS'), ('wild', 'WILD')]
CHIP_X0, CHIP_DX, CHIP_Y, CHIP_W, CHIP_H = 98, 136, 340, 124, 34
KNOB_Y = 486; KNOB_D = 46; KNOB_X0 = 70; KNOB_DX = 62
KNOBS = [('catch', 'CATCH', 0, 0), ('attack', 'ATTACK', 0, 0), ('release', 'RELEASE', 0, 0), ('duck', 'DUCK', 0, 0),
         ('echoLvl', 'LEVEL', 0, 1), ('echoDiv', 'DIV', 6, 1), ('echoFb', 'FEEDBACK', 0, 1),
         ('hallLvl', 'LEVEL', 0, 2), ('decay', 'DECAY', 0, 2),
         ('shimLvl', 'LEVEL', 0, 3), ('shimPitch', 'PITCH', 6, 3),
         ('tone', 'TONE', 0, 4), ('lowcut', 'LOW CUT', 0, 4), ('width', 'WIDTH', 0, 4), ('ret', 'RETURN', 0, 4)]
GROUPS = ['SEND', 'ECHO', 'HALL', 'SHIMMER', 'OUTPUT']
PLATE = (686, 578, 246, 46)
COPPER = (214, 134, 70)

def P(v): return v * S

def print_text(cv, items, ink, alpha=0.92):
    a = text_layer(cv.shape[1], cv.shape[0], items)
    cv[:] = cv * (1 - a[..., None] * alpha) + f32(ink) * a[..., None] * alpha

def circ_mask(size, cx, cy, r):
    yy, xx = np.mgrid[0:size[0], 0:size[1]].astype(np.float32)
    return np.clip(r - np.hypot(xx - cx, yy - cy) + 0.5, 0, 1)

def recess_rr(cv, x, y, w, h, r, fill, depth=8, strength=0.7):
    x, y, w, h, r = [int(round(v * S)) for v in (x, y, w, h, r)]
    m = rr_mask(w, h, r)
    pad = int(depth * S * 3)
    big = np.zeros((h + 2 * pad, w + 2 * pad), np.float32); big[pad:pad + h, pad:pad + w] = m
    sh = blur(1 - big, depth * S * 0.5); sh = np.roll(sh, int(depth * 0.45 * S), axis=0)[pad:pad + h, pad:pad + w]
    reg = cv[y:y + h, x:x + w]
    reg[:] = reg * (1 - m[..., None]) + f32(fill) * m[..., None]
    reg[:] = reg * (1 - (sh * m * strength)[..., None])
    # light lip under the recess
    mm = np.zeros((h + 6 * S, w), np.float32); mm[3 * S:3 * S + h] = m
    low = np.clip(np.roll(mm, int(1.4 * S), axis=0) - mm, 0, 1)
    sl = cv[y - 3 * S:y + h + 3 * S, x:x + w]
    sl[:] = sl * (1 - low[..., None] * 0.35) + low[..., None] * 0.35 * 0.85

def make_faceplate():
    # ---- anodised, bead-blasted black aluminium
    g1 = ndi.gaussian_filter(rng.standard_normal((H, W)).astype(np.float32), 0.7); g1 /= g1.std()
    g2 = ndi.gaussian_filter(rng.standard_normal((H, W)).astype(np.float32), 3.0); g2 /= g2.std()
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    # chamfer height from the window edge
    edge = np.minimum(np.minimum(xx, W - 1 - xx), np.minimum(yy, H - 1 - yy))
    bevel = np.clip(edge / (9 * S), 0, 1) ** 0.8
    hmap = g1 * 0.0016 + g2 * 0.0007 + bevel * 0.035
    base = np.ones((H, W, 3), np.float32) * f32((30, 31, 35))
    vg = 1.18 - 0.38 * (yy / H) - 0.10 * ((xx / W - 0.5) ** 2) * 4
    base *= vg[..., None]
    cv = shade(hmap, base, light=(-0.4, -0.7, 0.6), hscale=60, spec=0.04, shin=20, amb=0.82, diff=0.42)
    # brighter anodised edge rim
    rim = np.clip(1 - edge / (2.2 * S), 0, 1)
    cv = np.clip(cv + rim[..., None] * f32((70, 70, 74)) * 0.55, 0, 1)

    # ---- pad well (circular recess)
    px, py = PAD_C[0] * S, PAD_C[1] * S
    well = circ_mask((H, W), px, py, 98 * S)
    sh = blur(1 - well, 7 * S); sh = np.roll(sh, 3 * S, axis=0)
    cv = cv * (1 - well[..., None]) + f32((14, 14, 16)) * well[..., None]
    cv = cv * (1 - (sh * well * 0.65)[..., None])
    ring = np.clip(circ_mask((H, W), px, py, 99 * S) - circ_mask((H, W), px, py, 97.2 * S), 0, 1)
    cv = cv * (1 - ring[..., None] * 0.3) + ring[..., None] * 0.3 * f32((150, 150, 155))
    # ---- display glass
    dx, dy, dw, dh = DISPLAY
    recess_rr(cv, dx, dy, dw, dh, 12, (7, 11, 13), depth=10, strength=0.85)
    im = Image.fromarray((np.clip(cv, 0, 1) * 255).astype(np.uint8), 'RGB').convert('RGBA')
    ov = Image.new('RGBA', im.size, (0, 0, 0, 0)); d = ImageDraw.Draw(ov)
    gx0, gy0, gx1, gy1 = P(dx + 16), P(dy + 34), P(dx + dw - 16), P(dy + dh - 24)
    for i in range(0, 11):
        x = gx0 + (gx1 - gx0) * i / 10
        d.line([x, gy0, x, gy1], fill=(70, 190, 190, 22 if i % 5 else 48), width=max(1, S // 2))
    for j in range(0, 5):
        y = gy0 + (gy1 - gy0) * j / 4
        d.line([gx0, y, gx1, y], fill=(70, 190, 190, 22 if j not in (0, 4) else 48), width=max(1, S // 2))
    im = Image.alpha_composite(im, ov)
    cv = np.asarray(im.convert('RGB'), dtype=np.float32) / 255.0
    # ---- key wells, preset wells
    for (_, _, _, (x, y, w, h)) in KEYS:
        recess_rr(cv, x - 5, y - 5, w + 10, h + 10, 12, (16, 16, 18), depth=6, strength=0.7)
    recess_rr(cv, RET_METER[0] - 3, RET_METER[1] - 3, RET_METER[2] + 6, RET_METER[3] + 6, 5, (10, 10, 12), depth=4, strength=0.8)
    for i in range(len(CHIPS)):
        x = CHIP_X0 + i * CHIP_DX
        recess_rr(cv, x - 4, CHIP_Y - 4, CHIP_W + 8, CHIP_H + 8, 21, (15, 15, 17), depth=5, strength=0.7)

    # ---- typography
    cream = (214, 212, 204); dim = (150, 150, 148)
    f_logo = font('InterDisplay-Bold.otf', 30); f_sub = font('Inter-SemiBold.otf', 9); f_lab = font('Inter-SemiBold.otf', 8.2)
    f_grp = font('Inter-Bold.otf', 8.8); f_small = font('Inter-Medium.otf', 8)
    print_text(cv, [dict(xy=(P(84), P(60)), text='CATAPULT', font=f_logo, tracking=P(6.5))], cream)
    print_text(cv, [dict(xy=(P(1000 - 48), P(46)), text='THROW FX PROCESSOR', font=f_sub, tracking=P(3.6), anchor='r'),
                    dict(xy=(P(1000 - 48), P(60)), text='MODEL CT-1  ·  RETRO CATCH ENGINE', font=f_small, tracking=P(2.4), anchor='r')], dim)
    print_text(cv, [dict(xy=(P(DISPLAY[0] + 18), P(DISPLAY[1] + 22)), text='TRAJECTORY', font=f_lab, tracking=P(3.2)),
                    dict(xy=(P(DISPLAY[0] + DISPLAY[2] - 18), P(DISPLAY[1] + 22)), text='WET RETURN', font=f_lab, tracking=P(3.2), anchor='r'),
                    dict(xy=(P(DISPLAY[0] + 18), P(DISPLAY[1] + DISPLAY[3] - 8)), text='PAST', font=f_small, tracking=P(2.6)),
                    dict(xy=(P(DISPLAY[0] + DISPLAY[2] - 18), P(DISPLAY[1] + DISPLAY[3] - 8)), text='NOW', font=f_small, tracking=P(2.6), anchor='r')],
                (96, 160, 160), 0.85)
    print_text(cv, [dict(xy=(P(RET_METER[0] + RET_METER[2] / 2), P(RET_METER[1] + 26)), text='RETURN LEVEL', font=f_small, tracking=P(3), anchor='m')], dim)
    print_text(cv, [dict(xy=(P(90), P(CHIP_Y + 21)), text='TARGET', font=f_lab, tracking=P(3), anchor='r')], COPPER)
    items = []
    for i, (pid, lab, steps, grp) in enumerate(KNOBS):
        cx = KNOB_X0 + i * KNOB_DX
        items.append(dict(xy=(P(cx), P(KNOB_Y + 42)), text=lab, font=f_lab, tracking=P(1.4), anchor='m'))
    print_text(cv, items, cream, 0.85)
    # group headers (copper) with rules
    gi_items = []
    for gi, gname in enumerate(GROUPS):
        idx = [i for i, k in enumerate(KNOBS) if k[3] == gi]
        xa = KNOB_X0 + idx[0] * KNOB_DX - 28; xb = KNOB_X0 + idx[-1] * KNOB_DX + 28
        gi_items.append(dict(xy=(P((xa + xb) / 2), P(KNOB_Y - 56)), text=gname, font=f_grp, tracking=P(3.6), anchor='m'))
    print_text(cv, gi_items, COPPER, 0.95)
    im = Image.fromarray((np.clip(cv, 0, 1) * 255).astype(np.uint8)); d = ImageDraw.Draw(im)
    for gi, gname in enumerate(GROUPS):
        idx = [i for i, k in enumerate(KNOBS) if k[3] == gi]
        xa = KNOB_X0 + idx[0] * KNOB_DX - 28; xb = KNOB_X0 + idx[-1] * KNOB_DX + 28
        tw = f_grp.getlength(gname) / S + 3.6 * (len(gname) - 1) + 12
        mid = (xa + xb) / 2; y = KNOB_Y - 59
        for (a, b) in ((xa, mid - tw / 2), (mid + tw / 2, xb)):
            if b - a > 3:
                d.line([P(a), P(y), P(b), P(y)], fill=(96, 62, 36), width=max(1, S // 2))
        # end ticks
        d.line([P(xa), P(y), P(xa), P(y + 5)], fill=(96, 62, 36), width=max(1, S // 2))
        d.line([P(xb), P(y), P(xb), P(y + 5)], fill=(96, 62, 36), width=max(1, S // 2))
    # knob scales
    for i, (pid, lab, steps, grp) in enumerate(KNOBS):
        cx = KNOB_X0 + i * KNOB_DX
        n = steps if steps else 11
        for t in range(n):
            a = math.radians(-135 + 270 * t / (n - 1)); major = steps or t % 5 == 0
            r0 = KNOB_D / 2 + 5; r1 = r0 + (4 if major else 2.4)
            d.line([(P(cx + r0 * math.sin(a)), P(KNOB_Y - r0 * math.cos(a))), (P(cx + r1 * math.sin(a)), P(KNOB_Y - r1 * math.cos(a)))],
                   fill=(120, 120, 120) if major else (84, 84, 86), width=max(1, int(S * 1.0)))
    # header rule with trajectory arc ornament
    d.line([P(48), P(78), P(1000 - 48), P(78)], fill=(58, 58, 62), width=max(1, S // 2))
    # screws
    for sx, sy in ((22, 22), (978, 22), (22, 618), (978, 618)):
        c = (sx * S, sy * S); rr = 6.5 * S
        d.ellipse([c[0] - rr, c[1] - rr, c[0] + rr, c[1] + rr], fill=(46, 46, 50), outline=(88, 88, 92))
        d.ellipse([c[0] - rr * .62, c[1] - rr * .62, c[0] + rr * .62, c[1] + rr * .62], fill=(20, 20, 22))
        hx = [(c[0] + rr * .38 * math.cos(math.radians(60 * k)), c[1] + rr * .38 * math.sin(math.radians(60 * k))) for k in range(6)]
        d.polygon(hx, fill=(8, 8, 9))
    # logo mark: trajectory arc with a dot
    m0 = (P(48), P(58))
    pts = [(P(48 + 26 * t), P(58 - 34 * (1 - (2 * t - 1) ** 2) * 0.9 - 0 * t)) for t in [i / 40 for i in range(41)]]
    d.line(pts, fill=COPPER, width=max(2, int(S * 2.0)))
    ex, ey = pts[-1]; r_ = 4.2 * S
    d.ellipse([ex - r_, ey - r_, ex + r_, ey + r_], fill=COPPER)
    d.line([P(44), P(58), P(52), P(58)], fill=(150, 150, 150), width=max(1, S))
    cv = np.asarray(im, dtype=np.float32) / 255.0

    # ---- copper signature plate
    px_, py_, pw, ph = [int(v * S) for v in PLATE]
    br = ndi.gaussian_filter(rng.standard_normal((ph, pw)).astype(np.float32), (0.5, 30 * S / 2)); br /= br.std()
    m = rr_mask(pw, ph, int(5 * S))
    yy3 = np.linspace(0, 1, ph)[:, None]
    basec = np.ones((ph, pw, 3), np.float32) * f32((176, 104, 62)) * (1.10 - 0.34 * yy3)[..., None] + br[..., None] * 0.02
    lit = shade(br * 0.004, basec, hscale=40, spec=0.14, amb=0.8, diff=0.3)
    dist = ndi.distance_transform_edt(m > 0.5); bev = np.clip(1 - dist / (2.2 * S), 0, 1)
    lit = lit + bev[..., None] * (0.30 * np.linspace(1, -1, ph)[:, None, None])
    reg = cv[py_:py_ + ph, px_:px_ + pw]
    shd = np.roll(blur(np.pad(m, 8 * S), 2.5 * S), 2 * S, axis=0)[8 * S:8 * S + ph, 8 * S:8 * S + pw]
    reg[:] = reg * (1 - m[..., None]) + np.clip(lit, 0, 1) * m[..., None]
    im = Image.fromarray((np.clip(cv, 0, 1) * 255).astype(np.uint8)); d = ImageDraw.Draw(im)
    for sx in (px_ + 9 * S, px_ + pw - 9 * S):
        sy = py_ + ph // 2; rr = 3.3 * S
        d.ellipse([sx - rr, sy - rr, sx + rr, sy + rr], fill=(110, 66, 38), outline=(56, 32, 18))
        d.line([sx - rr * .7, sy - rr * .4, sx + rr * .7, sy + rr * .4], fill=(46, 26, 14), width=max(1, S))
    d.rounded_rectangle([px_ + 18 * S, py_ + 5 * S, px_ + pw - 18 * S, py_ + ph - 5 * S], radius=3 * S, outline=(120, 70, 40), width=max(1, S // 2))
    cv = np.asarray(im, dtype=np.float32) / 255.0
    Image.fromarray((np.clip(cv, 0, 1) * 255).astype(np.uint8)).save(f'{OUT}/faceplate.png')

def make_pad(down):
    sz = PAD_SZ * S; c = sz / 2
    yy, xx = np.mgrid[0:sz, 0:sz].astype(np.float32); r = np.hypot(xx - c, yy - c); ang = np.arctan2(yy - c, xx - c)
    R_out, R_in, R_dome = 86 * S, 79 * S, 71 * S
    h = np.zeros_like(r)
    ringm = (r <= R_out) & (r > R_in)
    h += np.where(ringm, 0.07 + 0.04 * np.sin((r - R_in) / (R_out - R_in) * math.pi), 0)
    groove = (r <= R_in) & (r > R_dome)
    h += np.where(groove, -0.05, 0)
    dome_n = np.clip(r / R_dome, 0, 1)
    h += np.where(r <= R_dome, (0.13 if not down else 0.07) * (1 - dome_n ** 2) ** 0.6 + 0.02, 0)
    base = np.ones(r.shape + (3,), np.float32) * f32((196, 196, 198))
    metal = shade(h, base, hscale=50, spec=0.4, shin=22, amb=0.5, diff=0.7)
    metal += ((np.cos((ang - 0.7) * 2) ** 8) * 0.25)[..., None] * 0.5 * ringm[..., None]
    domec = np.ones(r.shape + (3,), np.float32) * (f32((34, 34, 37)) if not down else f32((24, 24, 26)))
    dome = shade(h, domec, hscale=60, spec=0.5 if not down else 0.2, shin=18, amb=0.5, diff=0.7)
    gcol = np.ones(r.shape + (3,), np.float32) * f32((10, 10, 11))
    col = np.where((r <= R_dome)[..., None], dome, np.where(groove[..., None], gcol, metal))
    # legend
    im = Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)); 
    a = Image.new('L', (sz, sz), 0); d = ImageDraw.Draw(a)
    tracked_text(d, (c, c + 4 * S), 'THROW', font('InterDisplay-Bold.otf', 22), 255, tracking=P(4), anchor='m')
    tracked_text(d, (c, c + 22 * S), 'HOLD TO SEND', font('Inter-SemiBold.otf', 7.6), 255, tracking=P(3), anchor='m')
    al = np.asarray(a, dtype=np.float32) / 255.0
    ink = f32((224, 220, 208)) if not down else f32((255, 190, 110))
    col = np.asarray(im, dtype=np.float32) / 255.0
    col = col * (1 - al[..., None] * 0.95) + ink * al[..., None] * 0.95
    if down: col = np.clip(col + blur(al, 5 * S)[..., None] * f32((255, 140, 50)) * 0.5, 0, 1)
    mk = np.clip(R_out - r + 0.5, 0, 1)
    sh = ndi.shift(blur(mk, 3 * S), (2.4 * S, 0)) * 0.0
    rgba = np.dstack([col * mk[..., None], mk])
    Image.fromarray((np.clip(rgba, 0, 1) * 255).astype(np.uint8), 'RGBA').save(f'{OUT}/pad_{"down" if down else "up"}.png')

def make_key(key, title, sub, size, down):
    x, y, w, h = [int(v * S) for v in size]; w = size[2] * S; h = size[3] * S
    m = rr_mask(w, h, int(9 * S))
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dist = ndi.distance_transform_edt(m > 0.5)
    hm = np.clip(dist / (4 * S), 0, 1) ** 0.6 * 0.5 * (0.7 if down else 1)
    base = np.ones((h, w, 3), np.float32) * (f32((40, 40, 43)) if not down else f32((28, 28, 30)))
    cap = shade(hm, base, hscale=55, spec=0.18 if not down else 0.08, shin=24, amb=0.55, diff=0.65)
    a = Image.new('L', (w, h), 0); d = ImageDraw.Draw(a)
    tracked_text(d, (20 * S, h * 0.46), title, font('InterDisplay-Bold.otf', 14), 255, tracking=P(2.4))
    tracked_text(d, (20 * S, h * 0.74), sub.upper(), font('Inter-Medium.otf', 7.4), 255, tracking=P(2))
    al = np.asarray(a, dtype=np.float32) / 255.0
    ink = f32((222, 218, 206)) if not down else f32((255, 196, 120))
    cap = cap * (1 - al[..., None] * 0.95) + ink * al[..., None] * 0.95
    im = Image.fromarray((np.clip(cap, 0, 1) * 255).astype(np.uint8)); dd = ImageDraw.Draw(im)
    cx, cy, rr = w - 22 * S, h / 2, 5 * S
    dd.ellipse([cx - rr - S, cy - rr - S, cx + rr + S, cy + rr + S], fill=(10, 10, 11), outline=(80, 80, 84))
    cap = np.asarray(im, dtype=np.float32) / 255.0
    Image.fromarray((np.clip(np.dstack([cap, m]), 0, 1) * 255).astype(np.uint8), 'RGBA').save(f'{OUT}/key_{key}_{"down" if down else "up"}.png')

def make_chip(key, label, down):
    w, h = CHIP_W * S, CHIP_H * S
    m = rr_mask(w, h, h // 2)
    dist = ndi.distance_transform_edt(m > 0.5)
    hm = np.clip(dist / (3.4 * S), 0, 1) ** 0.6 * 0.4 * (0.6 if down else 1)
    base = np.ones((h, w, 3), np.float32) * (f32((44, 44, 47)) if not down else f32((70, 44, 28)))
    cap = shade(hm, base, hscale=55, spec=0.16, shin=24, amb=0.55, diff=0.65)
    a = Image.new('L', (w, h), 0); d = ImageDraw.Draw(a)
    tracked_text(d, (w / 2, h * 0.62), label, font('Inter-Bold.otf', 9.4), 255, tracking=P(2.2), anchor='m')
    al = np.asarray(a, dtype=np.float32) / 255.0
    ink = f32((214, 212, 204)) if not down else f32((255, 206, 140))
    cap = cap * (1 - al[..., None] * 0.95) + ink * al[..., None] * 0.95
    Image.fromarray((np.clip(np.dstack([cap, m]), 0, 1) * 255).astype(np.uint8), 'RGBA').save(f'{OUT}/chip_{key}_{"down" if down else "up"}.png')

def make_knob():
    D = KNOB_D; PADD = 8; size = (D + 2 * PADD) * S
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = size / 2; r = np.hypot(xx - c, yy - c); ang = np.arctan2(yy - c, xx - c); R = D / 2 * S; rn = r / R
    mk = np.clip(R - r + 0.5, 0, 1)
    h = np.zeros_like(rn)
    grip = rn > 0.66
    h += np.where(grip, 0.11 + 0.03 * np.sin(ang * 48), 0)
    h += np.where((rn > 0.62) & (rn <= 0.66), -0.03, 0)
    cap = rn <= 0.62
    h += np.where(cap, 0.14 + 0.003 * np.sin(rn * 180) - 0.02 * rn ** 2, 0)
    colg = np.ones(rn.shape + (3,), np.float32) * f32((26, 26, 29)); colc = np.ones(rn.shape + (3,), np.float32) * f32((184, 184, 188))
    base = np.where(cap[..., None], colc, colg)
    lit = shade(h, base, hscale=40, spec=0.55, shin=22, amb=0.5, diff=0.78)
    cone = (np.cos((ang - 0.8) * 2) ** 6) * 0.22 + (np.cos((ang + 2.2) * 2) ** 6) * 0.14
    lit = np.clip(lit + (cone * cap)[..., None] * 0.55, 0, 1)
    ringline = np.clip(1 - np.abs(rn - 0.64) / 0.012, 0, 1)
    lit = lit * (1 - 0.5 * ringline[..., None])
    lit *= (1 - 0.22 * np.clip((rn - 0.96) / 0.04, 0, 1))[..., None]
    sh = ndi.shift(blur(mk, 3.4 * S), (2.8 * S, 1.0 * S)); a = mk + sh * 0.6 * (1 - mk)
    Image.fromarray((np.clip(np.dstack([lit * mk[..., None], a]), 0, 1) * 255).astype(np.uint8), 'RGBA').save(f'{OUT}/knob.png')

make_faceplate(); make_pad(False); make_pad(True); make_knob()
for (k, t, s_, rect) in KEYS:
    make_key(k, t, s_, rect, False); make_key(k, t, s_, rect, True)
for (k, t) in CHIPS:
    make_chip(k, t, False); make_chip(k, t, True)

L = ['// GENERATED by tools/gen_assets.py - do not edit', '#pragma once', 'namespace layout {',
     f'constexpr int W = {LW}, H = {LH};',
     f'constexpr int padCx = {PAD_C[0]}, padCy = {PAD_C[1]}, padSize = {PAD_SZ};',
     f'constexpr int display[4] = {{{",".join(map(str, DISPLAY))}}};',
     f'constexpr int retMeter[4] = {{{",".join(map(str, RET_METER))}}};',
     f'constexpr int chipX0 = {CHIP_X0}, chipDX = {CHIP_DX}, chipY = {CHIP_Y}, chipW = {CHIP_W}, chipH = {CHIP_H};',
     f'constexpr int knobY = {KNOB_Y}, knobD = {KNOB_D}; constexpr float knobX0 = {KNOB_X0}.0f, knobDX = {KNOB_DX}.0f;',
     f'constexpr int plate[4] = {{{",".join(map(str, PLATE))}}};',
     f'constexpr int numKnobs = {len(KNOBS)}, numChips = {len(CHIPS)};',
     'struct KeyDef { const char* id; const char* asset; int rect[4]; };',
     'inline const KeyDef keys[] = {']
for (k, t, s_, r_) in KEYS: L.append(f'    {{"{k}", "key_{k}", {{{",".join(map(str, r_))}}}}},')
L += ['};', 'inline const char* const chipIds[] = {' + ','.join(f'"{k}"' for k, _ in CHIPS) + '};',
      'inline const char* const knobIds[] = {' + ','.join(f'"{k[0]}"' for k in KNOBS) + '};', '}']
open(f'{SRC}/Layout.h', 'w').write('\n'.join(L) + '\n'); print('ok')
