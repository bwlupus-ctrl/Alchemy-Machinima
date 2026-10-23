"""Metrics comparing a visibility source with the ray-traced ground truth (grid of pixels + probe points)."""
import numpy as np

ON, OFF, HIDDEN, ABSENT = 0, 1, 2, 3
CLS_NAMES = ['on-screen', 'off-screen', 'hidden', 'absent']
LIT_T = 0.98           # ground-truth visibility below this counts as shadowed
LEAK_DELTA = 0.25      # production brighter than 0.75 where ground truth < 0.5 = leak (false-lit)
DARK_DELTA = 0.25      # production below 0.75 where ground truth >= 0.98 = acne (false-dark)
EDGE_MISS_PX = 12.0    # a ground-truth edge with no production edge within this many 1080p px is "missed"
EDGE_CAP_PX = 64.0
EDGE_LEVELS = (0.5, 0.8)   # 0.5 = umbra edge; 0.8 = visible boundary of a soft/thin shadow (<=20% darkening)


def classify(nblk, reasons):
    """Per-pixel blocker category from the GT hit reasons: 0 on-screen (>=half of blocked rays hit something the
    G-buffer shows), else dominant failure reason 1/2/3; 255 for pixels with no blocked ray."""
    on = reasons[..., 0].astype(np.int32)
    other = reasons[..., 1:].astype(np.int32)
    dom = 1 + other.argmax(-1)
    cls = np.where(on * 2 >= nblk, ON, dom)
    return np.where(nblk > 0, cls, 255).astype(np.uint8)


def _pair_valid(gt, a, b, axis):
    """Valid neighbour pairs along axis (1: horizontal, 0: vertical): both candidates, same surface, smooth depth."""
    sl_a = (slice(None), slice(0, -1)) if axis == 1 else (slice(0, -1), slice(None))
    sl_b = (slice(None), slice(1, None)) if axis == 1 else (slice(1, None), slice(None))
    ok = gt['cand'][sl_a] & gt['cand'][sl_b] & (gt['surf'][sl_a] == gt['surf'][sl_b])
    za, zb = gt['zl'][sl_a], gt['zl'][sl_b]
    ok &= np.abs(za - zb) < np.maximum(0.1, 0.03 * np.minimum(za, zb))
    return ok, sl_a, sl_b


def crossings(v, gt, stride, level=0.5):
    """Sub-pixel iso-crossings (visibility == level) of visibility map v (grid) on valid neighbour pairs.
    Returns (n,2) positions in 1080p pixels and the (cell a, cell b) grid indices for classification."""
    pts, cells = [], []
    for axis in (1, 0):
        ok, sa, sb = _pair_valid(gt, None, None, axis)
        a, b = v[sa], v[sb]
        m = ok & ((a - level) * (b - level) < 0)
        jj, ii = np.nonzero(m)
        t = (level - a[m]) / (b[m] - a[m])
        if axis == 1:
            x, y = (ii + t) * stride + stride / 2, jj * stride + stride / 2
            ca = (jj, ii)
            cb = (jj, ii + 1)
        else:
            x, y = ii * stride + stride / 2, (jj + t) * stride + stride / 2
            ca = (jj, ii)
            cb = (jj + 1, ii)
        pts.append(np.stack([x, y], 1))
        cells.append((ca, cb))
    return pts, cells


def _nearest(src, dst, cap=EDGE_CAP_PX, chunk=1024):
    """Distance from every src point to the nearest dst point, capped."""
    if len(src) == 0:
        return np.zeros(0, np.float32)
    if len(dst) == 0:
        return np.full(len(src), cap, np.float32)
    out = np.empty(len(src), np.float32)
    dst = dst.astype(np.float32)
    for c in range(0, len(src), chunk):
        s = src[c:c + chunk].astype(np.float32)
        d2 = ((s[:, None, :] - dst[None, :, :]) ** 2).sum(2)
        out[c:c + chunk] = np.minimum(np.sqrt(d2.min(1)), cap)
    return out


def _subsample(pts, limit=8000):
    if len(pts) <= limit:
        return pts, np.arange(len(pts))
    idx = np.linspace(0, len(pts) - 1, limit).astype(np.int64)
    return pts[idx], idx


def _pct(x, q):
    return float(np.percentile(x, q)) if len(x) else float('nan')


def _light_edges(L, g, p, gt, stride, level):
    """Per-light edge distances: ground-truth edge -> nearest production edge, and the reverse (invented edges)."""
    cand, cls = L['cand'], L['cls']
    gl = {'cand': cand, 'surf': gt['surf'], 'zl': gt['zl']}
    gpts, gcells = crossings(g, gl, stride, level)
    ppts, pcells = crossings(p, gl, stride, level)
    gall, gcat, pall, pcat = [], [], [], []
    for axis in range(2):
        (ja, ia), (jb, ib) = gcells[axis]
        ca, cb = cls[ja, ia], cls[jb, ib]
        pick = np.where((g[ja, ia] < level) & (ca != 255), ca, np.where(cb != 255, cb, ca))
        gall.append(gpts[axis])
        gcat.append(np.where((pick == ON) | (pick == 255), ON, OFF))
        (ja, ia), (jb, ib) = pcells[axis]
        ca, cb = cls[ja, ia], cls[jb, ib]
        pick = np.where(ca != 255, ca, cb)
        pall.append(ppts[axis])
        pcat.append(np.where((pick == ON) | (pick == 255), ON, OFF))
    gall, gcat = np.concatenate(gall), np.concatenate(gcat)
    pall, pcat = np.concatenate(pall), np.concatenate(pcat)
    out = {}
    for k in (ON, OFF):
        sel, _ = _subsample(gall[gcat == k], 4000)
        out['g%d' % k] = _nearest(sel, pall)
    sel, _ = _subsample(pall[pcat == ON], 4000)
    out['false'] = _nearest(sel, gall)
    out['ng'] = [int((gcat == ON).sum()), int((gcat == OFF).sum())]
    out['np_on'] = int((pcat == ON).sum())
    return out


def eval_pose(gt, prod, stride, pool=None):
    """gt: dict(surf, zl, pos, lights:[{vis,cand,cls,tmean,L0}]); prod: (L,Hg,Wg) visibility in [0,1]."""
    out = {}
    acc = {k: [] for k in ('g', 'p', 'cls', 'tm')}
    edge = []
    for li, L in enumerate(gt['lights']):
        g, p, cand = L['vis'], prod[li], L['cand']
        cls = L['cls']
        sel = cand
        acc['g'].append(g[sel])
        acc['p'].append(p[sel])
        acc['cls'].append(cls[sel])
        acc['tm'].append(L['tmean'][sel])
        edge.append({lv: _light_edges(L, g, p, gt, stride, lv) for lv in EDGE_LEVELS})
    g = np.concatenate(acc['g'])
    p = np.concatenate(acc['p'])
    cls = np.concatenate(acc['cls'])
    tm = np.concatenate(acc['tm'])
    shadow = g < LIT_T
    s_on = shadow & (cls == ON)
    s_exp = shadow & (cls != ON) & (cls != 255)
    lit = g >= LIT_T
    out['n_cand'] = int(len(g))
    out['n_shadow'] = int(shadow.sum())
    out['n_on'] = int(s_on.sum())
    out['n_exp'] = int(s_exp.sum())
    out['n_cls'] = [int((shadow & (cls == c)).sum()) for c in range(4)]
    out['mae_on'] = float(np.abs(p - g)[s_on].mean()) if s_on.any() else float('nan')
    out['mae_exp'] = float(np.abs(p - g)[s_exp].mean()) if s_exp.any() else float('nan')
    out['lit_mae'] = float(np.abs(p - g)[lit].mean()) if lit.any() else float('nan')
    out['false_dark'] = float(((p < 1 - DARK_DELTA)[lit]).mean()) if lit.any() else float('nan')
    for key, mask in (('on', s_on), ('exp', s_exp)):
        dark = mask & (g < 0.5)
        out['leak_' + key] = float((p[dark] > 1 - LEAK_DELTA).mean()) if dark.any() else float('nan')
    # penumbra coverage (on-screen blockers): how much of the ground-truth penumbra the source also shows as penumbra
    pen = s_on & (g > 0.1) & (g < 0.9)
    for name, band in (('near', tm < 0.6), ('far', tm > 1.0)):
        m = pen & band
        out['pen_n_' + name] = int(m.sum())
        out['pen_cover_' + name] = float(((p > 0.1) & (p < 0.9))[m].mean()) if m.any() else float('nan')
        out['pen_bias_' + name] = float((p - g)[m].mean()) if m.any() else float('nan')
    gp = (pen).sum()
    pp = ((p > 0.1) & (p < 0.9) & ~s_exp).sum()
    out['pen_ratio'] = float(pp / gp) if gp > 20 else float('nan')
    # edges (matched per light so one light's shadow edge can never excuse another's)
    for lv in EDGE_LEVELS:
        tag = '' if lv == 0.5 else '80'
        for key, k in (('on', ON), ('exp', OFF)):
            d = np.concatenate([e[lv]['g%d' % k] for e in edge])
            out['edge%s_n_%s' % (tag, key)] = int(sum(e[lv]['ng'][k] for e in edge))
            out['edge%s_p50_%s' % (tag, key)] = _pct(d, 50)
            out['edge%s_p95_%s' % (tag, key)] = _pct(d, 95)
            out['edge%s_miss_%s' % (tag, key)] = float((d > EDGE_MISS_PX).mean()) if len(d) else float('nan')
            out['edge%s_mean_%s' % (tag, key)] = float(d.mean()) if len(d) else float('nan')
        dd = np.concatenate([e[lv]['false'] for e in edge])
        out['edge%s_false' % tag] = float((dd > EDGE_MISS_PX).mean()) if len(dd) else float('nan')
        out['edge%s_false_n' % tag] = int(sum(e[lv]['np_on'] for e in edge))
    # lamp pool (scene C)
    if pool is not None:
        li, ids, radius, lamp = pool['light'], pool['ids'], pool['radius'], pool['pos']
        L = gt['lights'][li]
        near = np.linalg.norm(gt['pos'] - np.array(lamp), axis=2) < radius
        m = L['cand'] & near & np.isin(gt['surf'], ids)
        pv, gv = prod[li][m], L['vis'][m]
        out['pool_n'] = int(m.sum())
        out['pool_gt_min'] = float(gv.min()) if m.any() else float('nan')
        out['pool_prod_min'] = float(pv.min()) if m.any() else float('nan')
        out['pool_prod_mean'] = float(pv.mean()) if m.any() else float('nan')
        out['pool_dark'] = float(((pv < 1 - DARK_DELTA) & (gv >= LIT_T)).sum() / max(1, (gv >= LIT_T).sum())) if m.any() else float('nan')
    return out


# ------------------------------------------------------------------ temporal (probe points)
def temporal(seq, flags, cfg_gt):
    """Camera-motion flicker between adjacent poses of one sequence for one config.

    seq:    list of dicts with 'probe' (L,K) production visibility (NaN = probe not visible), 'cls' (L,K) blocker
            category of the probe in that pose (0 on-screen / 1-3 off-screen, hidden, absent / 255 unshadowed), 'cfg'
    flags:  list of bool, pose-level "every key blocker group on-screen" (reporting only)
    cfg_gt: {cfg index: (L,K) ground-truth probe visibility}
    flicker = |(prod_j - prod_i) - (gt_j - gt_i)| over probes visible in both poses and shadowed in either pose.
    With a fixed light the ground truth does not change, so this is the production change caused by the camera alone.
    UNEXPECTED pairs of probes have every shadowing blocker on-screen in BOTH poses (or no blocker); the rest is the
    expected screen-space failure (a blocker leaving the screen / hiding changes the production shadow) and is reported
    separately under the 'x_' keys.
    """
    pairs = []
    for i in range(len(seq) - 1):
        a, b = seq[i], seq[i + 1]
        pa, pb = a['probe'], b['probe']
        ga, gb = cfg_gt[a['cfg']], cfg_gt[b['cfg']]
        ok = np.isfinite(pa) & np.isfinite(pb)
        active = ok & ((ga < LIT_T) | (gb < LIT_T) | (pa < LIT_T) | (pb < LIT_T))
        good = ((a['cls'] == ON) | (a['cls'] == 255)) & ((b['cls'] == ON) | (b['cls'] == 255))
        f = np.abs((pb - pa) - (gb - ga))
        rec = {'i': i, 'on': bool(flags[i] and flags[i + 1]), 'n': int((active & good).sum()), 'nx': int((active & ~good).sum())}
        if rec['n'] >= 30:
            fu = f[active & good]
            rec.update({'mean': float(fu.mean()), 'p95': float(np.percentile(fu, 95)), 'max': float(fu.max()),
                        'pop': float((fu > 0.25).mean())})
        if rec['nx'] >= 30:
            fx = f[active & ~good]
            rec.update({'x_mean': float(fx.mean()), 'x_p95': float(np.percentile(fx, 95)), 'x_pop': float((fx > 0.25).mean())})
        pairs.append(rec)
    return pairs
