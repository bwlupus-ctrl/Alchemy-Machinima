"""Phase 0 grain/acne gate for the screen-space Relight soft shadows (production GLSL, no viewer).

Imported by relight_area_check (also run by relight_shader_check); run directly it re-launches
the GPU harness (--area-only --grain-only) so it never reports a silent success.

What it proves (RELIGHT_WORLD_SHADOWS_PLAN.md, Phase 0):
  * planner  - an independent closed-form ladder re-derives every row of the production C++
               planner (preset x 720p..4K x K1-8 x Thin ON/OFF x feed reservation): ray floor,
               admitted lights A independent of the visible set, Thin near 48 and far floor, the regular
               lights of a hybrid march EXACTLY the Thin-OFF plan, the Thin lights of a hybrid march the
               regular RAY COUNT (round 4), and with feeds (the grid's worst case: eight 2048 x 2048) a Thin or hybrid
               main never costs more than the Thin-OFF main.
  * oracles  - analytic edge location (1 full-resolution pixel), contrast and penumbra width of a half-plane
               blocker lit by a disk light, on a converged march (64 rays x 512 steps); no reference passed
               through the shader filter. Mutations: flipped receiver bias, a light 10 cm off, a 64-step march.
  * noise    - for every effective plan: RMS against a 64-ray reference at the plan's own march (not around the
               images' own seed mean, which would hide a bias), <= 6/255 at Medium 1080p K4 and <= 8/255
               elsewhere; the spread around the mean is bounded too. The pre-P0 commit must fail where the plan says.
  * spectrum - (a) the penumbra patch minus its expected value (mean of the other seeds): NO bin above 2/255 at any
               non-DC frequency, plus the line/noise ratio. (b) the ensemble mean of the rendered patch against ONE
               global fit of the geometry-derived 2-D analytic expectation (plus a smooth quadratic drift in y, at most
               0.5/255): no bin above 2/255 (read on a 2x zero-padded grid, so half-bin stripes do not hide in the Hann
               window's scalloping), and above 0.08 cycles/px none above 0.5/255. Mutations (production-filter shader
               variants that write the structure into the rendered image): 4x4 interleave, the x+y-degenerate rank,
               x, y, diagonal, half-bin diagonal, multiplicative and row-parity stripes and +3/255 on every 8th row.
               Fix round 5: a block-structure read (cross-half covariance of high-passed 4/8/16 px block means) and a row-mean read
               fail random +-3/255 4x4 and 8x8 blocks and +3/255 / +6/255 on every 16th / 32nd row; a y-ramp added to the rendered mean
               fails the drift assertion alone.
  * boundary - the hybrid Thin/regular boundary (share of rays that march the regular way), on every PRODUCTION hybrid
               plan of the grid, is exact at both ends (byte for byte raw, <= 1/255 after the production filter) and moves by
               at most one ray per step; the previous round's unequal-ray plan fails the same equality; the upsample fallback
               of a thin wall takes the pixel's own texel.
  * receivers- speckle on flat, grazing, curved and normal-mapped receivers.
  * mutations- every fix has a mutation that fails its own gate.
The pre-P0 programs are pinned to commit PRE_P0, never to a moving HEAD.
"""
from pathlib import Path
import subprocess

PRE_P0 = '4dd9ff661c1'

# ---------------------------------------------------------------- planner oracle (CPU only)
PRESETS = [(4, 8, True), (8, 12, True), (12, 20, True), (24, 32, False)]
FEED_SLOTS, FEED_PIXELS, FULL_RES_RAYS, FAR_FLOOR, RAY_FLOOR, RAY_RUNG = 8, 1024 * 1024, 12, 8, 4, 8


def quality(preset, thin):
    s, st, h = PRESETS[preset]
    if not thin:
        return dict(samples=s, steps=st, half=h, near=0, minfar=0)
    return dict(samples=s, steps=80, half=h, near=48, minfar=st)


def segmented(u, preset):
    far = min(max(u['steps'], 1), 64)
    return dict(samples=u['samples'], steps=max(far, 32) + 48, half=u['half'], near=48, minfar=far)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def ladder(q, width, height, lights, budget, requested, hold=0, mutation=None):
    """Closed-form restatement of the plan's P0.1 ladder (no decrement loops)."""
    near = q['near']
    s, steps, half = clamp(q['samples'], 1, 32), clamp(q['steps'], 1, 128 if near else 64), q['half']
    minfar = clamp(q['minfar'], 1, 64) if near else 0
    lights = min(lights, 8)
    basis = max(lights, clamp(requested, 1, 8))
    if mutation == 'visible':
        basis = max(lights, 1)  # A planned from the visible set instead of the requested cap
    budget = min(budget, 1500000000)
    cap = budget // 2
    planned = budget - min(hold, budget) if near else budget

    def px(h):
        return ((width + 1) // 2) * ((height + 1) // 2) if h else width * height

    def fits(smp, st, a, h):
        return px(h) * a * smp * st <= planned and px(h) * smp * st <= cap

    def finish(smp, st, h, a):
        per = px(h) * smp * st
        channels = min(4, cap // per) if lights and per else 0
        out_near = 24 if (mutation == 'thin-near' and near) else near
        return dict(samples=smp, steps=st - (near - out_near), half=h, near=out_near, admitted=a, channels=channels,
                    iterations=px(h) * min(lights, a) * smp * st, pixels=px(h))

    if mutation == 'rays-first':  # the superseded order: rays down to 1, then steps, then half
        smp, st, h = s, steps, half
        if near and not h:
            r = min(s, FULL_RES_RAYS)
            if px(False) * basis * r * st > planned or px(False) * r * st > cap:
                h = True
        smp = max(1, min(smp, cap // (px(h) * st), planned // (px(h) * basis * st)))
        return finish(smp, st, h, lights if lights else basis)
    if not half:
        r = min(s, FULL_RES_RAYS)
        if px(False) * basis * r * steps > planned or px(False) * r * steps > cap:
            half = True
    thin = steps - near
    farmin = max(1, min(minfar, FAR_FLOOR)) if near else min(steps, FAR_FLOOR)
    halfsteps = min(thin, max(farmin, thin // 2))

    def squeeze_rays(smp, st, floor):
        best = min(smp, cap // (px(half) * st), planned // (px(half) * basis * st))
        return max(min(floor, smp), best)

    def squeeze_steps(smp, st, floor):
        best = min(st, cap // (px(half) * smp), planned // (px(half) * basis * smp))
        return max(near + min(floor, thin), best) if best < st else st

    smp, st = s, steps
    smp = squeeze_rays(smp, st, min(s, RAY_RUNG))
    st = squeeze_steps(smp, st, halfsteps)
    smp = squeeze_rays(smp, st, min(s, RAY_FLOOR))
    st = squeeze_steps(smp, st, min(thin, farmin))
    if fits(smp, st, basis, half):
        return finish(smp, st, half, basis)
    if px(half) * smp * st <= cap:
        a = planned // (px(half) * smp * st)
        if a >= 1:
            return finish(smp, st, half, min(a, basis))
    raise AssertionError('sub-floor regime: not part of the default-budget grid')


def px_of(width, height, half):
    return ((width + 1) // 2) * ((height + 1) // 2) if half else width * height


def thin_group(thin, off, width, height, nthin, budget, planned):
    """Thin objects hybrid, restated as an enumeration of the far-step states. The regular group is the Thin-OFF plan `off`
    EXACTLY (its rays, steps and resolution never change); `nthin` Thin lights share only what the planning limit `planned` leaves
    over, in the OFF plan's resolution AND WITH ITS RAY COUNT (fix round 4: the two groups' disk strata must match so a Thin light
    that ties the first regular light marches the regular light's estimator exactly); only the far steps differ, from the Thin
    request down to the common floor. None when nothing fits."""
    near = thin['near']
    tst = clamp(thin['steps'], 1, 128)
    admitted = off['admitted']
    nrest = admitted - nthin
    half = off['half']
    p = px_of(width, height, half)
    rest_cost = nrest * p * off['samples'] * off['steps']
    if not off['channels'] or rest_cost > planned:
        return None
    room = planned - rest_cost
    cap = min(budget, 1500000000) // 2
    ts = off['samples']
    thin_part = tst - near
    t_farmin = max(1, min(clamp(thin['minfar'], 1, 64), FAR_FLOOR))
    floor_steps = near + min(thin_part, t_farmin)
    # Fix round 5 (DeepSeek P1): a Thin light also marches regular rays at the regular step count, unclamped, so every Thin light is
    # charged ts x max(its Thin steps, the regular steps) in the fit test, the draw channel count and the iterations.
    rst = clamp(off['steps'], 1, 64)
    for ast in range(tst, floor_steps - 1, -1):
        charged = max(ast, rst)
        if nthin * p * ts * charged <= room and p * ts * charged <= cap:
            return dict(samples=ts, steps=ast, half=half, near=near, admitted=admitted, lead=nthin,
                        channels=min(4, cap // (p * ts * charged)),
                        rest_samples=off['samples'], rest_steps=off['steps'], rest_channels=off['channels'],
                        iterations=p * (nthin * ts * charged + nrest * off['samples'] * off['steps']))
    return None


def feed_spend_bound(request, width, height, requested):
    """The most a live feed's plan can ever cost: its target at the requested march's resolution x K x the requested rays x steps."""
    dense = clamp(request['near'], 0, 64)
    samples, steps = clamp(request['samples'], 1, 32), clamp(request['steps'], 1, 128 if dense else 64)
    px = px_of(width, height, request['half'])
    return px * clamp(requested, 1, 8) * samples * steps


def feed_spend(request, budget, regular_main, requested, feeds):
    """What the LIVE feeds can spend in a frame when each gets its Thin-OFF allowance (budget - the Thin-OFF main) / count; the whole
    pool when every feed is bound by its allowance."""
    if not feeds:
        return 0
    budget = min(budget, 1500000000)
    pool = budget - min(regular_main, budget)
    allowance = pool // len(feeds)
    total, saturated = 0, True
    for w, h in feeds:
        bound = feed_spend_bound(request, w, h, requested)
        if bound < allowance:
            saturated = False
            total += bound
        else:
            total += allowance
    return pool if saturated else total


def hybrid_of(thin, uniform, width, height, lights, budget, requested, hold, feeds=(), mutation=None):
    """workloadOrUniform for a Thin request: pure Thin when it admits at least what OFF admits, else the hybrid with the
    most Thin lights beside the regular (Thin-OFF) lights, else OFF (nothing Thin fits beside the regular lights). While feeds
    are live the planning limit also holds back what they can spend (feed_spend)."""
    off = ladder(uniform, width, height, lights, budget, requested, 0)
    off = dict(off, lead=off['admitted'], rest_samples=0, rest_steps=0, rest_channels=0)
    capped = min(budget, 1500000000)
    regular_main = off['pixels'] * off['admitted'] * off['samples'] * off['steps'] if off['channels'] else 0
    hold_all = max(min(hold, capped), feed_spend(thin, capped, regular_main, requested, feeds))
    planned = capped - hold_all
    try:
        pure = ladder(thin, width, height, lights, budget, requested, hold_all)
    except AssertionError:
        pure = dict(channels=0, admitted=0)
    if pure['channels']:
        pure = dict(pure, lead=pure['admitted'], rest_samples=0, rest_steps=0, rest_channels=0)
    if mutation == 'no-hybrid':   # the first fallback: the pure Thin plan, OFF only when Thin does not fit at all
        return pure if pure['channels'] else off
    if pure['channels'] and pure['admitted'] >= off['admitted']:
        return pure
    if not off['channels']:
        return pure if pure['channels'] else off
    for nthin in range(off['admitted'], 0, -1):
        plan = thin_group(thin, off, width, height, nthin, budget, planned)
        if plan:
            return plan
    return off


def worst_feeds(count=FEED_SLOTS):
    return [(2048, 2048)] * count


def feed_reservation(width, height, budget, cap, feeds):
    """The floor-level guarantee (feedReservation in the header): the sum over the live feeds of min(allowance, the feed's need)."""
    if not feeds:
        return 0
    cheapest = ladder(dict(samples=1, steps=1, half=True, near=0, minfar=0), width, height, cap, budget, cap)
    reserve = cheapest['iterations'] if cheapest['channels'] else 0
    allowance = (min(budget, 1500000000) - reserve) // len(feeds)
    total = 0
    for w, h in feeds:
        need = min(px_of(w, h, True), FEED_PIXELS) * max(clamp(cap, 1, 8), 2)
        total += min(allowance, need)
    return total


def plan_of(preset, thin, height, cap, feeds, mutation=None):
    """workloadOrUniform as renderAreaShadows calls it (main view, 1500M, K lights requested). `feeds` is the live feed list
    ([(w, h)]), or a truthy flag for the worst case the grid uses (eight 2048 x 2048 feeds)."""
    width = height * 16 // 9
    u = quality(preset, False)
    q = segmented(u, preset) if thin else u
    budget = 1500000000
    feed_list = feeds if isinstance(feeds, (list, tuple)) else (worst_feeds() if feeds else [])
    hold = feed_reservation(width, height, budget, cap, feed_list) if q['near'] else 0
    if not q['near']:
        w = ladder(q, width, height, cap, budget, cap, 0, mutation)
        return dict(w, lead=w['admitted'], rest_samples=0, rest_steps=0, rest_channels=0), hold
    if mutation in (None, 'no-hybrid'):
        return hybrid_of(q, u, width, height, cap, budget, cap, hold, feed_list, mutation), hold
    w = ladder(q, width, height, cap, budget, cap, hold, mutation)
    if not w['channels']:
        w = ladder(u, width, height, cap, budget, cap, 0, mutation)
    return dict(w, lead=w['admitted'], rest_samples=0, rest_steps=0, rest_channels=0), hold


def filter_radius(setting, scale, half):
    scaled = clamp(setting * scale * (0.5 if half else 1.0), 0.0, 8.0)
    return clamp(scaled, 2.0, 8.0) if scaled > 0 else 0.0


def check_planner(policy):
    """Re-derive every production planner row; print the effective coverage tables."""
    rows = policy['grid']
    assert len(rows) == 2 * 4 * 8 * 4 * 2, len(rows)
    by_key = {}
    # The Thin-OFF rows: (samples, steps, near, half, admitted, channels, iterations) per (thin=0, height, K, preset, feeds).
    off_rows = {tuple(r[:5]): (r[5], r[6], r[7], r[8], r[9], r[10], r[11]) for r in rows if r[0] == 0}
    hybrids = 0
    for thin, height, cap, preset, feeds, smp, st, near, half, adm, ch, it, lead, rsmp, rst, rch in rows:
        w, hold = plan_of(preset, thin, height, cap, feeds)
        got = (smp, st, near, bool(half), adm, ch, it, lead, rsmp, rst, rch)
        want = (w['samples'], w['steps'], w['near'], w['half'], w['admitted'], w['channels'], w['iterations'],
                w['lead'], w['rest_samples'], w['rest_steps'], w['rest_channels'])
        assert got == want, ('planner row differs from the independent ladder', thin, height, cap, preset, feeds, got, want)
        # Invariants of the plan, from the target size and the budget only.
        width = height * 16 // 9
        assert adm >= 1 and adm <= cap and 1 <= lead <= adm, (cap, adm, lead)
        px = px_of(width, height, half)
        charged = max(st, rst) if lead < adm else st   # fix round 5: a hybrid's Thin lights are charged the larger of the two step counts
        assert it == px * (lead * smp * charged + (adm - lead) * rsmp * rst) and it <= 1500000000 and px * smp * charged <= 750000000
        if lead < adm:
            hybrids += 1
            assert thin and near == 48 and px * rsmp * rst <= 750000000 and rch >= 1
            assert rsmp >= min(quality(preset, False)['samples'], RAY_FLOOR) and rst >= min(quality(preset, False)['steps'], FAR_FLOOR), ('regular group floors', thin, height, cap, preset)
            assert it <= 1500000000 - hold, ('a hybrid with Thin lights holds back the feed reservation', thin, height, cap, preset, feeds)
            # Fix round 3 (Opus P1): the regular lights march EXACTLY the Thin-OFF plan, in every row (rays, steps, resolution).
            off_row = off_rows[(0, height, cap, preset, feeds)]
            assert (rsmp, rst, bool(half)) == (off_row[0], off_row[1], bool(off_row[3])) and rch == off_row[5] and adm == off_row[4], \
                ('the regular group is not the Thin-OFF plan', thin, height, cap, preset, feeds, (rsmp, rst, half), off_row)
            # Fix round 4 (Codex P0, GLM/DeepSeek P1): the Thin group uses EXACTLY the regular ray count, so the disk strata of a
            # boundary light match its regular neighbour.
            assert smp == rsmp, ('the Thin group of a hybrid does not march the regular ray count', thin, height, cap, preset, feeds, smp, rsmp)
        else:
            assert not (rsmp or rst or rch), ('only a hybrid has regular lights', thin, height, cap, preset)
        if near:
            assert near == 48 and st - near >= min(quality(preset, False)['steps'], FAR_FLOOR), ('Thin floors', thin, height, cap, preset)
        floor_ok = smp >= min(quality(preset, False)['samples'], RAY_FLOOR)
        if not floor_ok:  # only when ONE light at the floors cannot fit
            width_px = ((width + 1) // 2) * ((height + 1) // 2)
            steps = (48 + min(quality(preset, False)['steps'], FAR_FLOOR)) if near else min(quality(preset, False)['steps'], FAR_FLOOR)
            assert width_px * RAY_FLOOR * steps > 1500000000 - (hold if near else 0) or width_px * RAY_FLOOR * steps > 750000000, ('below the floor', thin, height, cap, preset, feeds, got)
        by_key[(thin, height, cap, preset, feeds)] = (smp, adm, lead)
    # Thin objects never removes a shadow that Thin OFF draws (user decision 10/22): at every height, K, preset and
    # feed reservation, Thin ON admits at least what Thin OFF admits.
    on_costs = {tuple(r[:5]): r[11] for r in rows if r[0] == 1}
    for (thin, height, cap, preset, feeds), (smp, adm, lead) in by_key.items():
        if thin:
            assert adm >= by_key[(0, height, cap, preset, feeds)][1], ('Thin ON admits fewer lights than OFF', height, cap, preset, feeds)
            # Fix round 3 (Codex P0 / Opus P0): while VCam feeds can run, a Thin or hybrid main plan never costs more than the
            # Thin-OFF main plan, so the feeds get exactly the OFF allowance and lose no light to Thin.
            if feeds:
                assert on_costs[(1, height, cap, preset, feeds)] <= off_rows[(0, height, cap, preset, feeds)][6], \
                    ('Thin ON main costs more than OFF with feeds', height, cap, preset)
    assert hybrids > 0, 'the default-budget grid exercises the hybrid (4K)'
    # A never depends on the visible set (the C++ grid is requested == visible; vary visible here).
    for thin in (0, 1):
        for height in (720, 1080, 1440, 2160):
            for cap in range(1, 9):
                for preset in range(4):
                    width = height * 16 // 9
                    u = quality(preset, False)
                    q = segmented(u, preset) if thin else u
                    full = ladder(q, width, height, cap, 1500000000, cap)
                    for visible in range(1, cap + 1):
                        part = ladder(q, width, height, visible, 1500000000, cap)
                        assert (part['samples'], part['steps'], part['half'], part['admitted']) == \
                            (full['samples'], full['steps'], full['half'], full['admitted']), ('A depends on the visible set', thin, height, cap, preset, visible)
    # Mutations the table comparison and the invariants must reject.
    # The previous round squeezed the regular group to the floors to fit more Thin lights: the exact-OFF-march invariant above is the
    # gate that rejects it; it must have something to reject, i.e. some OFF march above the floors beside a hybrid.
    squeezable = sum(1 for r in rows if r[0] == 1 and r[12] < r[9] and
                     (off_rows[(0, r[1], r[2], r[3], r[4])][0] > min(quality(r[3], False)['samples'], RAY_FLOOR) or
                      off_rows[(0, r[1], r[2], r[3], r[4])][1] > min(quality(r[3], False)['steps'], FAR_FLOOR)))
    assert squeezable > 0, 'the squeezed-regular-group mutation has no hybrid row to be rejected on'
    for mutation in ('rays-first', 'visible', 'thin-near', 'no-hybrid'):
        broke = 0
        for thin, height, cap, preset, feeds, smp, st, near, half, adm, ch, it, lead, rsmp, rst, rch in rows:
            w, hold = plan_of(preset, thin, height, cap, feeds, mutation)
            if mutation == 'visible':
                width = height * 16 // 9
                u = quality(preset, False)
                q = segmented(u, preset) if thin else u
                alt = ladder(q, width, height, 1, 1500000000, cap, 0, 'visible')
                base = ladder(q, width, height, cap, 1500000000, cap, 0, 'visible')
                broke += (alt['admitted'], alt['samples'], alt['steps']) != (base['admitted'], base['samples'], base['steps'])
            elif mutation == 'rays-first':
                broke += w['samples'] < min(quality(preset, False)['samples'], RAY_FLOOR) and w['channels'] > 0 and \
                    not (w['admitted'] == 0)
            elif mutation == 'no-hybrid':   # Thin ON admitting fewer lights than OFF
                broke += (thin and w['admitted'] < by_key[(0, height, cap, preset, feeds)][1])
            else:
                broke += (w['near'] != 48) if thin else 0
        assert broke > 0, ('planner mutation survived', mutation)
    return rows


def coverage_tables(rows):
    """Effective coverage (admitted/requested, rays) for 1080p/4K x K4/K8 x Thin OFF/ON; Thin ON cells add the
    Thin-quality lights ("T" = how many of the admitted lights march the Thin objects march)."""
    lines = []
    for height in (1080, 2160):
        for cap in (4, 8):
            for thin in (0, 1):
                cells = []
                for preset in range(4):
                    for row in rows:
                        if row[:5] == [thin, height, cap, preset, 0]:
                            cell = '%d/%d lights %dray %dstep%s' % (row[9], cap, row[5], row[6], '' if row[8] else ' full')
                            if thin:
                                lead = row[12] if row[7] else 0   # the plan fell back to the regular march: no Thin light ("0 thin-quality")
                                cell += ' (%d thin-quality%s)' % (lead, '' if lead == row[9] or not lead else ', rest %dray %dstep' % (row[13], row[14]))
                            cells.append(cell)
                lines.append('%dp K%d Thin %s: %s' % (height, cap, 'ON ' if thin else 'OFF', ' | '.join(cells)))
    return lines


# ------------------------------------------------------------------ pre-P0 planner (CPU)
OLD_PLANNER_CPP = r'''
#include "alareashadowmath.h"
#include <iostream>
int main() {
    bool first = true;
    std::cout << "[";
    for (int thin=0; thin<2; ++thin) for (unsigned height : {720u,1080u,1440u,2160u})
    for (unsigned cap=1; cap<=8; ++cap) for (int preset=0; preset<4; ++preset) for (int feeds=0; feeds<2; ++feeds) {
        const auto uniform=ALAreaShadow::quality(preset);
        const auto requested=thin ? ALAreaShadow::segmented(uniform,preset) : uniform;
        const auto plan=ALAreaShadow::workloadOrUniform(requested,uniform,height*16/9,height,cap,1500,cap,
            feeds ? ALAreaShadow::kFeedSlots : 0);
        if (!first) std::cout << ",";
        first=false;
        std::cout << "[" << thin << "," << height << "," << cap << "," << preset << "," << feeds << ","
            << plan.q.samples << "," << plan.q.steps << "," << plan.q.nearSteps << "," << plan.q.half << ","
            << plan.channels << "," << plan.iterations << "]";
    }
    std::cout << "]";
}
'''


def old_planner_rows():
    """The planner at PRE_P0, compiled as it was: [thin,height,K,preset,feeds,samples,steps,near,half,channels,it]."""
    import json, os
    directory = Path('.tmp/ws_p0/oldplan').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    header = subprocess.check_output(['git', 'show', PRE_P0 + ':indra/newview/alareashadowmath.h'], text=True)
    (directory / 'alareashadowmath.h').write_text(header, encoding='utf-8', newline='\n')
    (directory / 'old.cpp').write_text(OLD_PLANNER_CPP, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory / 'compile.cmd').write_text(
        '@echo off\ncall "%s" >nul\ncl /nologo /std:c++17 /EHsc /I"%s" /Fo"%s" /Fe"%s" "%s"\nexit /b %%ERRORLEVEL%%\n' %
        (vcvars, directory, directory / 'old.obj', directory / 'old.exe', directory / 'old.cpp'), encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(directory / 'compile.cmd')], check=True, capture_output=True)
    return json.loads(subprocess.check_output([str(directory / 'old.exe')], text=True))


# ------------------------------------------------------------------------- GPU fixtures
class Rig:
    """Runs the pinned pre-P0 and the production programs on synthetic G-buffers."""

    def __init__(self, g):
        self.g = g
        self.np = g['np']
        self.C, self.F, self.U = g['C'], g['F'], g['U']
        self.loc, self.ui, self.uf = g['loc'], g['ui'], g['uf']
        self.root = g['root']
        self.original = g['read_shader']
        self.paths = {
            'gather': self.root / 'class3/deferred/areaShadowF.glsl',
            'trace': self.root / 'class1/deferred/screenSpaceRayUtil.glsl',
            'filter': self.root / 'class3/deferred/areaShadowFilterF.glsl'}
        self.programs = {'new': (g['area_gather'], g['area_filter'])}
        pinned = {p: subprocess.check_output(['git', 'show', PRE_P0 + ':' + p.as_posix()], text=True)
                  for p in self.paths.values()}
        self.programs['old'] = (self.variant('pre-P0 gather', pinned, 'class3/deferred/areaShadowF.glsl'),
                                self.variant('pre-P0 filter', pinned, 'class3/deferred/areaShadowFilterF.glsl'))
        self.pinned = pinned
        self.uploaded = None

    def variant(self, label, replacements, name):
        g = self.g
        try:
            g['read_shader'] = lambda p: replacements.get(Path(p), self.original(p))
            return g['link_production'](name, '', label)
        finally:
            g['read_shader'] = self.original

    def text(self, key):
        return self.original(self.paths[key])

    # -- GL plumbing -------------------------------------------------------------------
    def draw(self, pr, w, h, rows=None):
        """Draw the fullscreen triangle into a fresh RGBA8 target; with rows=(y0,y1) only that band is
        rasterised (scissor) and read back, as a (y1-y0, w, 4) array."""
        g, np = self.g, self.np
        out = g['area_texture'](0, np.zeros((h, w, 4), np.float32), 0x8058)
        g['attachment'](0x8D40, 0x8CE0, 0x0DE1, out, 0)
        assert g['status'](0x8D40) == 0x8CD5
        g['viewport'](0, 0, w, h)
        y0, y1 = rows if rows else (0, h)
        if rows:
            g['gl']('glEnable', None, g['U'])(0x0C11)
            g['gl']('glScissor', None, g['I'], g['I'], g['I'], g['I'])(0, y0, w, y1 - y0)
        g['draw'](4, 0, 3)
        image = np.empty((y1 - y0, w, 4), np.uint8)
        g['read'](0, y0, w, y1 - y0, 0x1908, 0x1401, image.ctypes.data)
        if rows:
            g['disable'](0x0C11)
        return image

    def matrices(self, pr, scene):
        g, np = self.g, self.np
        for name, value in ((b'projection_matrix', scene['projection']), (b'inv_proj', scene['inverse'])):
            value = np.ascontiguousarray(value.T, dtype=np.float32)
            g['matrix'](self.loc(pr, name), 1, 0, value.ctypes.data_as(self.C.POINTER(self.F)))

    def chain(self, which, scene, light, plan, radius, screen_h, seed=12345, thickness=.35, light_bias=.25,
              max_dist=8.0, upto=None, keep=8, programs=None, full=False, mix=0.0, rest_steps=0):
        """Gather -> H -> V -> upsample with the production uniforms; returns uint8 (h, w) of channel 0."""
        g, np, loc, ui, uf = self.g, self.np, self.loc, self.ui, self.uf
        gather, filt = programs if programs else self.programs[which]
        depth, normals = scene['depth'], scene['normals']
        h, w = depth.shape[:2]
        half = bool(plan['half'])
        tw, th = ((w + 1) // 2, (h + 1) // 2) if half else (w, h)
        # The G-buffer stays bound on units 1/2 between chains of one scene (tens of MB at 4K).
        if self.uploaded is None or self.uploaded[0] is not depth or self.uploaded[1] is not normals:
            g['area_texture'](1, np.ascontiguousarray(depth, dtype=np.float32))
            g['area_texture'](2, np.ascontiguousarray(normals, dtype=np.float32))
            self.uploaded = (depth, normals)  # references: an id() could be reused after a free
        g['area_use'](gather)
        self.matrices(gather, scene)
        for name, value in ((b'depthMap', 1), (b'normalMap', 2), (b'area_count', 1), (b'area_first', 0),
                            (b'area_samples', plan['samples']), (b'area_steps', plan['steps']),
                            (b'area_near_steps', plan['near']), (b'area_thin_objects', int(plan['near'] > 0))):
            ui(loc(gather, name), value)
        uf[2](loc(gather, b'area_trace_res'), tw, th)
        uf[3](loc(gather, b'area_params'), max_dist, thickness, screen_h / 1080.0 * (.5 if half else 1.0))
        uf[1](loc(gather, b'area_light_bias'), light_bias)
        uf[4](loc(gather, b'area_contact_dist'), 0, 0, 0, 0)
        uf[4](loc(gather, b'area_contact_fade'), 0, 0, 0, 0)
        # Hybrid boundary (fix round 3): the share of this light's rays that march the regular way, and that march's steps.
        # The shader's defaults (0, 0) are what every other chain runs with; the pre-P0 programs have no such uniforms (-1).
        uf[4](loc(gather, b'area_regular_mix'), mix, mix, mix, mix)
        ui(loc(gather, b'area_rest_steps'), rest_steps)
        lights = np.array([light] * 4, np.float32)
        ids = np.array([seed, 9999, 7, 54321], np.uint32)
        g['uniform4fv'](loc(gather, b'area_lights'), 4, lights.ctypes.data_as(self.C.POINTER(self.F)))
        g['uniform4uiv'](loc(gather, b'area_seeds'), 1, ids.ctypes.data_as(self.C.POINTER(self.U)))
        # Evaluate the central band; the blurs need margin rows around it (<= 8 + 1 trace rows).
        scale = 2 if half else 1
        margin = 12  # keep = evaluated full-res rows on each side of the centre
        centre = th // 2
        span = (keep // scale + margin) if half else (keep + margin)
        band = (max(0, centre - span), min(th, centre + span))
        if full:
            band = (0, th)
        gathered = np.zeros((th, tw, 4), np.uint8)
        gathered[band[0]:band[1]] = self.draw(gather, tw, th, band)
        current = gathered
        if upto == 'gather':
            return current[band[0]:band[1], :, 0]

        def run(axis, width, height, rows):
            nonlocal current
            g['area_texture'](3, np.ascontiguousarray(current.astype(np.float32) / 255), 0x8058)
            g['area_use'](filt)
            self.matrices(filt, scene)
            for name, value in ((b'depthMap', 1), (b'normalMap', 2), (b'area_source', 3), (b'area_debug_channel', 0)):
                ui(loc(filt, name), value)
            uf[2](loc(filt, b'area_axis'), *axis)
            uf[1](loc(filt, b'area_radius'), radius)
            drawn = self.draw(filt, width, height, rows)
            full = np.zeros((height, width, 4), np.uint8)
            full[rows[0]:rows[1]] = drawn
            current = full

        if radius > 0:
            run((1, 0), tw, th, band)
            run((0, 1), tw, th, band)
        out_rows = (0, h) if full else (h // 2 - keep, h // 2 + keep)
        run((0, 0), w, h, out_rows)
        return current[out_rows[0]:out_rows[1], :, 0]

    def upload_scene(self, scene):
        g, np = self.g, self.np
        if self.uploaded is None or self.uploaded[0] is not scene['depth'] or self.uploaded[1] is not scene['normals']:
            g['area_texture'](1, np.ascontiguousarray(scene['depth'], dtype=np.float32))
            g['area_texture'](2, np.ascontiguousarray(scene['normals'], dtype=np.float32))
            self.uploaded = (scene['depth'], scene['normals'])

    def filter_only(self, scene, raw, radius, clip=(0.0, 0.0), offset=0.0, filt=None, fade=(1.0, 1.0, 1.0, 1.0)):
        """The production bilateral chain (blur x, blur y, full-resolution upsample) on a prepared raw gather image
        `raw` (float 0..1, trace-resolution, RGBA); the G-buffer is the scene's. The raw image is quantised to RGBA8 like
        the production target. `fade` is the per-channel contact fade the pipeline uploads (0 = no native map: the hand-over
        stop is off for that channel). Returns the full-resolution uint8 (h, w, 4)."""
        g, np, loc, ui, uf = self.g, self.np, self.loc, self.ui, self.uf
        filt = filt or g['area_filter']
        self.upload_scene(scene)
        h, w = scene['depth'].shape[:2]
        th, tw = raw.shape[:2]
        current = np.clip(np.round(raw * 255), 0, 255).astype(np.uint8)
        passes = ([((1, 0), tw, th), ((0, 1), tw, th)] if radius > 0 else []) + [((0, 0), w, h)]
        for axis, width, height in passes:
            g['area_texture'](3, np.ascontiguousarray(current.astype(np.float32) / 255), 0x8058)
            g['area_use'](filt)
            self.matrices(filt, scene)
            for name, value in ((b'depthMap', 1), (b'normalMap', 2), (b'area_source', 3), (b'area_debug_channel', 0)):
                ui(loc(filt, name), value)
            uf[2](loc(filt, b'area_native_clip'), *clip)
            uf[1](loc(filt, b'area_native_offset'), offset)
            uf[4](loc(filt, b'area_contact_fade'), *fade)
            uf[2](loc(filt, b'area_axis'), *axis)
            uf[1](loc(filt, b'area_radius'), radius)
            current = self.draw(filt, width, height)
        return current


def wall_scene(np, w, x_range=3.0, y_half=1.0, wall_z=-8.0, blocker_z=-6.0, blocker_from=None):
    """Orthographic camera at the origin looking down -Z. A wall at wall_z and a half-plane blocker
    covering every column >= blocker_from at blocker_z. View-space x spans [-x_range, x_range]; the
    height gives square pixels, so rays toward a large light never leave the screen vertically."""
    h = int(round(w * y_half / x_range))
    projection = np.eye(4)
    projection[0, 0] = 1.0 / x_range
    projection[1, 1] = 1.0 / y_half
    projection[2, 2] = -.1
    depth = np.full((h, w, 4), (-.1 * wall_z + 1) / 2, np.float32)
    blocker_from = w // 2 if blocker_from is None else blocker_from
    depth[:, blocker_from:] = (-.1 * blocker_z + 1) / 2
    normals = np.zeros_like(depth)
    normals[:, :, :2] = .5  # encoded +Z
    return dict(depth=depth, normals=normals, projection=projection, inverse=np.linalg.inv(projection),
                x_range=x_range, wall_z=wall_z, blocker_z=blocker_z, blocker_from=blocker_from, y_half=y_half)


def analytic_profile(np, scene, light, radius, bias, disk=(48, 96)):
    """Visible fraction of a disk light along the wall, by dense quadrature of the geometry alone."""
    w = scene['depth'].shape[1]
    xs = (np.arange(w) + .5) / w * 2 * scene['x_range'] - scene['x_range']
    blocker_x = -scene['x_range'] + 2 * scene['x_range'] * scene['blocker_from'] / w
    lx, ly, lz = light[:3]
    radius = radius if radius > 0 else .1
    nr, na = disk
    r = np.sqrt((np.arange(nr) + .5) / nr) * radius
    a = (np.arange(na) + .5) / na * 2 * np.pi
    rr, aa = np.meshgrid(r, a, indexing='ij')
    rr, aa = rr.ravel(), aa.ravel()
    out = np.empty(w)
    for lo in range(0, w, 256):
        x = xs[lo:lo + 256]
        origin = np.stack([x, np.zeros_like(x), np.full_like(x, scene['wall_z'] + bias)], axis=-1)
        direction = np.array([lx, ly, lz]) - origin
        direction /= np.linalg.norm(direction, axis=-1, keepdims=True)
        up = np.where(np.abs(direction[:, 2:3]) < .99, np.array([0, 0, 1.]), np.array([0, 1., 0]))
        tangent = np.cross(direction, up)
        tangent /= np.linalg.norm(tangent, axis=-1, keepdims=True)
        bitangent = np.cross(direction, tangent)
        # points: (n, k, 3)
        target = np.array([lx, ly, lz])[None, None, :] + rr[None, :, None] * (
            np.cos(aa)[None, :, None] * tangent[:, None, :] + np.sin(aa)[None, :, None] * bitangent[:, None, :])
        s = (scene['blocker_z'] - origin[:, None, 2]) / (target[:, :, 2] - origin[:, None, 2])
        cx = origin[:, None, 0] + (target[:, :, 0] - origin[:, None, 0]) * s
        out[lo:lo + 256] = np.mean(cx < blocker_x, axis=1)
    return out, xs


def analytic_rows(np, scene, light, radius, bias, row_ys, col_lo, col_hi, disk=(48, 96)):
    """The 2-D expectation, from the geometry alone (never a render): the visible fraction of a disk light for every ROW (the
    receiver's view-space y in `row_ys`) of the wall columns [col_lo, col_hi). Rows differ only by the receiver's height against the
    light's (the disk is turned toward each ray), so this is the one profile of the penumbra plus its small vertical drift."""
    w = scene['depth'].shape[1]
    xs = (np.arange(col_lo, col_hi) + .5) / w * 2 * scene['x_range'] - scene['x_range']
    blocker_x = -scene['x_range'] + 2 * scene['x_range'] * scene['blocker_from'] / w
    lx, ly, lz = light[:3]
    radius = radius if radius > 0 else .1
    nr, na = disk
    r = np.sqrt((np.arange(nr) + .5) / nr) * radius
    a = (np.arange(na) + .5) / na * 2 * np.pi
    rr, aa = np.meshgrid(r, a, indexing='ij')
    rr, aa = rr.ravel(), aa.ravel()
    out = np.empty((len(row_ys), len(xs)))
    for ri, y in enumerate(row_ys):
        origin = np.stack([xs, np.full_like(xs, y), np.full_like(xs, scene['wall_z'] + bias)], axis=-1)
        direction = np.array([lx, ly, lz]) - origin
        direction /= np.linalg.norm(direction, axis=-1, keepdims=True)
        up = np.where(np.abs(direction[:, 2:3]) < .99, np.array([0, 0, 1.]), np.array([0, 1., 0]))
        tangent = np.cross(direction, up)
        tangent /= np.linalg.norm(tangent, axis=-1, keepdims=True)
        bitangent = np.cross(direction, tangent)
        target = np.array([lx, ly, lz])[None, None, :] + rr[None, :, None] * (
            np.cos(aa)[None, :, None] * tangent[:, None, :] + np.sin(aa)[None, :, None] * bitangent[:, None, :])
        s_ = (scene['blocker_z'] - origin[:, None, 2]) / (target[:, :, 2] - origin[:, None, 2])
        cx = origin[:, None, 0] + (target[:, :, 0] - origin[:, None, 0]) * s_
        out[ri] = np.mean(cx < blocker_x, axis=1)
    return out


def crossing(np, xs, profile, level):
    """x where a decreasing profile first falls through `level` (linear interpolation)."""
    below = np.flatnonzero(profile < level)
    if not len(below) or below[0] == 0:
        return float('nan')
    i = below[0]
    return xs[i - 1] + (level - profile[i - 1]) / (profile[i] - profile[i - 1]) * (xs[i] - xs[i - 1])



# --------------------------------------------------------------------------- measurements
SEEDS = [12345, 1152, 777, 424242, 99, 31337, 8675309, 55555]  # light UUID hashes
LIGHT = (0.7, 0.0, -4.0, .3)                                   # view space, source radius 0.3 m
NOISE_LIMIT, MEDIUM_1080P_LIMIT, SPECKLE_LEVEL = 8.0, 6.0, 250
EDGE_WORLD_TOLERANCE = .01      # P0 may not move the mean shadow edge by more than 1 cm at the same plan
SPECTRUM_DRIFT = 0.5            # /255: the fitted quadratic y-drift of the shared structure may not exceed this (honest reading ~0.1); fix round 4
SPECTRUM_PEAK = 2.0             # /255: no bin of the detrended penumbra, at ANY non-DC frequency, above this (plan O6)
SPECTRUM_BAND_LIMITS = (2.0, 2.0, 0.5, 0.5)   # /255 per band f < .04, .04-.08, .08-.15, >= .15 (shared structure): the plan's 2/255 where the
# fit and the penumbra's own shape live, 0.5 above 0.08 cycles/px (honest 0.26 at most; a +3/255 line on every 8th row reads 0.75)
SPECTRUM_BLOCKS = (4, 8, 16)               # fix round 5 (Opus P2): block sizes (pixels) of the block-structure read of the shared residual
SPECTRUM_BLOCK_LIMITS = (0.8, 1.2, 1.5)      # /255: sqrt of the cross-half covariance of the HIGH-PASSED block means of the shared residual, per block size (honest Low .31/.63/.95, Medium .14/.32/.24; a random +-3 per 4x4 / 8x8 block reads 1.7 / 2.1 or more)
SPECTRUM_ROW_LIMIT = 1.0                    # /255: the largest row mean of the shared residual after the drift (honest 0.41; +3/255 on every 16th row reads 2.6, +6 on every 32nd 5.1)
SPECTRUM_LINE_RATIO = 11.0      # strongest bin / 99th percentile: flat noise ~3, R2 <= 8.5, a surviving dither grid >= 14
ORACLE_STEPS = 512              # converged march for the analytic oracles (64 steps leave a 4-pixel quantisation edge shift)
REFERENCE_RAYS = 64             # the plan's reference: 64 rays, at the plan's own march


def encode_normals(np, n):
    n = n / np.abs(n).sum(axis=-1, keepdims=True)
    return n[..., :2] * .5 + .5


def bumpy(np, scene, block=4, degrees=25, seed=7):
    """The same wall with a blocky random normal map (up to `degrees` off the geometric normal)."""
    h, w = scene['depth'].shape[:2]
    rng = np.random.default_rng(seed)
    gh, gw = (h + block - 1) // block, (w + block - 1) // block
    tilt = np.radians(degrees) * rng.random((gh, gw)) ** .5
    azim = rng.random((gh, gw)) * 2 * np.pi
    n = np.stack([np.sin(tilt) * np.cos(azim), np.sin(tilt) * np.sin(azim), np.cos(tilt)], axis=-1)
    n = np.repeat(np.repeat(n, block, axis=0), block, axis=1)[:h, :w]
    out = dict(scene)
    out['normals'] = scene['normals'].copy()
    out['normals'][:, :, :2] = encode_normals(np, n)
    return out


class Measure:
    """Ensemble statistics of one plan over several light-UUID seeds on one wall scene."""

    def __init__(self, g, rig, width, screen_h):
        self.np = g['np']
        self.rig, self.w, self.screen_h = rig, width, screen_h
        self.scene = wall_scene(self.np, width, y_half=.5)
        self.bias = max(.02, 1.5 * 6 / width)
        self.fa, self.xs = analytic_profile(self.np, self.scene, LIGHT, LIGHT[3], self.bias)
        np = self.np
        self.px = 6.0 / width
        self.receiver = self.xs < -0.05
        self.penumbra = (self.fa > .002) & (self.fa < .998) & self.receiver
        self.x50 = crossing(np, self.xs, self.fa, .5)
        self.width_a = crossing(np, self.xs, self.fa, .1) - crossing(np, self.xs, self.fa, .9)

    def images(self, which, plan, radius, scene=None, seeds=SEEDS, keep=8, programs=None):
        np = self.np
        scene = scene or self.scene
        return np.stack([self.rig.chain(which, scene, LIGHT, plan, radius, self.screen_h, seed=s, keep=keep,
                                        programs=programs).astype(float) / 255 for s in seeds])

    def noise(self, imgs, mask=None):
        np = self.np
        mask = self.penumbra if mask is None else mask
        mean = imgs.mean(axis=0)
        n = len(imgs)
        return np.sqrt(np.mean((imgs[:, :, mask] - mean[None, :, mask]) ** 2) * n / (n - 1)) * 255

    def reference_error(self, imgs, reference):
        """RMS (/255) over the penumbra of every image in `imgs` against ONE reference image (rows x columns)."""
        np = self.np
        return float(np.sqrt(np.mean((imgs[:, :, self.penumbra] - reference[None, :, self.penumbra]) ** 2)) * 255)

    def edge(self, imgs):
        np = self.np
        profile = imgs.mean(axis=(0, 1))
        e50 = crossing(np, self.xs, profile, .5)
        wd = crossing(np, self.xs, profile, .1) - crossing(np, self.xs, profile, .9)
        return e50 - self.x50, wd


def margin_mask(np, base, pixels):
    """True where `base` holds and no pixel within `pixels` columns violates it."""
    bad = ~base
    near = np.zeros_like(base)
    for s in range(-pixels, pixels + 1):
        near |= np.roll(bad, s)
    return base & ~near


def radius_for(setting_scale, half, new):
    """The Denoise radius (setting 2) the pipeline hands the filter: P0.2 floor of 2 trace pixels or HEAD's raw scale."""
    scaled = clamp(2.0 * setting_scale * (.5 if half else 1.0), 0.0, 8.0)
    return filter_radius(2.0, setting_scale, half) if new else scaled


def plan_dict(row):
    thin, height, cap, preset, feeds, smp, st, near, half = row[:9]
    return dict(samples=smp, steps=st, near=near, half=bool(half))


def unique_rows(rows, heights=(1080, 2160), caps=(4, 8)):
    """Distinct effective marches by (height, samples, steps, near, half). The pre-P0 rows have 11 columns and one march per
    row; a production row also carries the regular march of a Thin hybrid's other lights (marked 'R' in the labels)."""
    seen = {}
    for row in rows:
        thin, height, cap, preset, feeds = row[:5]
        if feeds or height not in heights or cap not in caps:
            continue
        key = (height,) + tuple(row[5:9])
        seen.setdefault(key, []).append((thin, cap, preset))
        if len(row) > 12 and row[12] < row[9]:
            rest_key = (height, row[13], row[14], 0, row[8])
            seen.setdefault(rest_key, []).append((thin, cap, preset, 'R'))
    return seen


R2_BLOCK = ('        uvec2 block = pixel >> 3u;\n'
            '        uint shift = areaHash(block.x ^ areaHash(block.y) ^ area_seeds[light]); // per light and 8x8 block\n')
R2_SEED = '        uint seed = pixel.x*(0u-AREA_R2_B)+pixel.y*AREA_R2_A+areaHash(shift^0x9e3779b9u); // ray start rank\n'
R2_ROT = '        float rotation = float((pixel.x*AREA_R2_A+pixel.y*AREA_R2_B+shift) >> 16)*(6.28318530718/65536.0);\n'


def bayer_mutation(text):
    """The 4x4 ordered-dither (interleave) alternative to the R2 rank: a regular lattice of period 4."""
    block = R2_BLOCK + R2_SEED + R2_ROT
    assert block in text
    bayer = 'const int AREA_BAYER[16] = int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);\n'
    text = text.replace('void main()', bayer + 'void main()', 1)
    return text.replace(block,
        '        uint shift = area_seeds[light];\n'
        '        float rotation = float((AREA_BAYER[int(pixel.x&3u)+4*int(pixel.y&3u)]+int(shift&15u))&15)*(6.28318530718/16.0);\n'
        '        uint seed = uint((AREA_BAYER[int((pixel.x+2u)&3u)+4*int((pixel.y+1u)&3u)]+int((shift>>4)&15u))&15)*268435456u+(shift&0x0fffffffu);\n')


def degenerate_mutation(text):
    """The first Phase 0 rank: ray start lattice (a2, a1), so that rotation + start phase depends on x + y only, and
    no per-block re-roll of the light shift."""
    block = R2_BLOCK + R2_SEED + R2_ROT
    assert block in text
    return text.replace(block,
        '        uint shift = area_seeds[light];\n'
        '        uint seed = pixel.x*AREA_R2_B+pixel.y*AREA_R2_A+areaHash(shift^0x9e3779b9u); // ray start rank\n' + R2_ROT)


def spectrum_lines(np, residual):
    """Spectrum of a detrended penumbra patch (Hann window): (strongest bin in /255 over every non-DC frequency,
    strongest bin / 99th percentile of all non-DC bins, strongest bin per band f < .04, .04-.08, .08-.15, >= .15).
    The caller removes the expected penumbra first. Noise has a flat spectrum (ratio ~3); a lattice or dither
    structure that survives the blur is a line (ratio >> 10) and, below the noise floor, an absolute peak."""
    h, w = residual.shape
    window = np.outer(np.hanning(h), np.hanning(w))
    data = (residual - residual.mean()) * window
    spec = np.fft.fft2(data)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    freq = np.sqrt(fx * fx + fy * fy)
    amp = 2 * np.abs(spec) / window.sum() * 255
    amp[0, 0] = 0.0                               # DC
    non_dc = freq > 0
    ratio = float(amp[non_dc].max() / np.percentile(amp[non_dc], 99))
    # Fix round 4 (Opus P2): the peak and band reads are taken on a grid zero-padded 2x in both axes, so a stripe whose frequency falls
    # half-way between two bins of the patch is read at (nearly) its true amplitude instead of losing up to 28 percent to the Hann
    # window's scalloping (a 2.6/255 diagonal at a half-bin offset read 1.88 and passed a 2.0 limit). The line ratio keeps the
    # unpadded bins (its limits were calibrated on them).
    pad = np.fft.fft2(data, s=(2 * h, 2 * w))
    pfy = np.fft.fftfreq(2 * h)[:, None]
    pfx = np.fft.fftfreq(2 * w)[None, :]
    pfreq = np.sqrt(pfx * pfx + pfy * pfy)
    pamp = 2 * np.abs(pad) / window.sum() * 255
    pamp[0, 0] = 0.0
    pnon_dc = pfreq > 0
    bands = [float(pamp[(pfreq >= lo) & (pfreq < hi) & pnon_dc].max()) for lo, hi in ((0, .04), (.04, .08), (.08, .15), (.15, 1.0))]
    return float(pamp[pnon_dc].max()), ratio, bands


def structure_blocks(np, residual, halves):
    """Fix round 5 (Opus P2): the spectrum alone is blind to energy that is not periodic, or too weak in any one bin: a random +-3/255
    pattern of 4x4 / 8x8 pixel blocks spreads over many bins (every bin reads below 1/255), and +3/255 on every 16th or 32nd row is a
    comb whose bins fall under the 0.5 band limit. Two direct reads of the shared residual (after the smooth y-drift) catch them.
    BLOCKS: the RMS of the block means is dominated by the honest noise of the seed mean (Low reads 1.9/255 at 4x4), so the block read is
    the CROSS-covariance of the block means of two independent halves of the seeds (even and odd): the render noise differs between the
    halves and cancels in expectation, a structure the render adds identically to every seed does not. Returned as sqrt(max(cov, 0)),
    in /255. The block means are first HIGH-PASSED (each minus the mean of its four neighbouring blocks): the honest residual carries a
    smooth, deterministic misfit of the plan's own penumbra (Low reads 1.7/255 on the plain block means, equally at every block size),
    which the high-pass removes, while a structure that changes from block to block (a random +-3 per block) passes it at full
    strength. ROWS: the largest single row mean of the whole residual."""
    h, w = residual.shape
    blocks = []
    for b in SPECTRUM_BLOCKS:
        hb, wb = h // b * b, w // b * b
        means = [half[:hb, :wb].reshape(hb // b, b, wb // b, b).mean(axis=(1, 3)) for half in halves]
        means = [m[1:-1, 1:-1] - .25 * (m[:-2, 1:-1] + m[2:, 1:-1] + m[1:-1, :-2] + m[1:-1, 2:]) for m in means]
        blocks.append(float(np.sqrt(max(float((means[0] * means[1]).mean()), 0.0)) * 255))
    return tuple(blocks), float(np.abs(residual.mean(axis=1)).max() * 255)


def grazing_scene(g, np, width=1920, height=1080):
    """A floor seen at ~87 degrees incidence, lit from behind a standing panel: the floor's depth step per
    pixel is large (the old raw-depth edge-stop collapsed here) yet it is one plane."""
    import relight_scene_fixture as S
    floor = S.plane((0, 0, 0), (0, 1, 0), 0)
    panel = S.plane((1.5, .9, -12), (0, 0, 1), 1, rect=((1, 0, 0), (0, 1, 0), 1.5, .9))
    light = np.array([1.0, 2.6, -15.0])
    eye = np.array([0.0, .35, 2.0])
    # Find the 50 % point of the shadow edge on the floor line x = 1.5 and look at it.
    zs = np.linspace(-11, -1, 400)
    origins = np.stack([np.full_like(zs, 1.5), np.full_like(zs, .02), zs], axis=-1)
    fraction = S.visible_fraction(np, origins, light, .3, [panel])
    z50 = zs[np.argmin(abs(fraction - .5))]
    target = np.array([1.5, 0, z50])
    forward = target - eye
    yaw = np.degrees(np.arctan2(forward[0], -forward[2]))
    pitch = np.degrees(np.arcsin(forward[1] / np.linalg.norm(forward)))
    cam = S.camera(np, eye, yaw, pitch, 0.0, width, height)
    scene = S.cast(np, cam, [floor, panel])
    bias = S.receiver_bias(np, scene, (width + 1) // 2, (height + 1) // 2)
    return dict(scene=scene, S=S, light=light, panel=panel, bias=bias, cam=cam, eye=eye)


def check_grain(g):
    import time
    np = g['np']
    t_start = time.time()
    failures = []

    def require(ok, message):
        if not ok:
            failures.append(message)
            print('[RelightQ] FAIL ' + message, flush=True)
        return ok

    # ---- planner: independent ladder vs the production C++ planner -------------------------------
    policy = g['policy']
    rows = check_planner(policy)
    for setting, scale, half, value in policy['radius']:
        require(abs(value - filter_radius(setting, scale, bool(half))) < 1e-5, 'filterRadius row %s' % [setting, scale, half, value])
    old_rows = old_planner_rows()
    old_floor_breaks = sum(1 for r in old_rows if not r[4] and r[5] < RAY_FLOOR and r[9] > 0)
    require(old_floor_breaks > 0, 'the pre-P0 planner must break the ray floor (4K Cinematic: 1 ray)')
    for line in coverage_tables(rows):
        print('[RelightQ] coverage ' + line, flush=True)
    print('[RelightQ] grain planner PASS: 512 production rows re-derived by an independent ladder; ray floor %d, A independent of the visible set, Thin near 48 + far floor; mutations (rays-first, A from the visible set, Thin near thinned) rejected; pre-P0 planner breaks the floor in %d rows' %
          (RAY_FLOOR, old_floor_breaks), flush=True)

    # ---- source guards ----------------------------------------------------------------------------
    gather_text = g['read_shader'](g['root'] / 'class3/deferred/areaShadowF.glsl')
    filter_text = g['read_shader'](g['root'] / 'class3/deferred/areaShadowFilterF.glsl')
    for forbidden in ('areaHash(pixel.x ^', 'gFrameCount', 'uniform float time', 'noiseSine'):
        require(forbidden not in gather_text, 'gather must not contain ' + forbidden)
    require('pow(max(dot(n,areaNormal(uv)),0.0),32.0)' not in filter_text and 'getLinearDepth' not in filter_text,
            'the filter must not use the normal-mapped pow(dot,32) / raw linear-depth edge-stops')

    # ---- horizon decision (P0.4 is conditional) ------------------------------------------------
    import relight_horizon_study as H
    wins, total, worst_head, worst_skip, _ = H.horizon_study()
    require(worst_skip > 3 * worst_head and wins < total,
            'P0.4(a) must lose on a normal map tilted toward the light (it would leak light on the dark side)')
    print('[RelightQ] horizon PASS: P0.4(a) wins %d of %d terminator-band cases but loses the tilted-normal-map case badly (worst %.1f/255 vs HEAD %.1f/255): NOT adopted, gather keeps its below-horizon behaviour' %
          (wins, total, worst_skip, worst_head), flush=True)

    # ---- GPU ----------------------------------------------------------------------------------------
    rig = Rig(g)
    mutated = {}

    def mutant(label, transform=None, filter_text_override=None, use_old_filter=False):
        text = transform(gather_text) if transform else gather_text
        gather_program = rig.variant(label, {rig.paths['gather']: text}, 'class3/deferred/areaShadowF.glsl') \
            if transform else g['area_gather']
        filter_program = rig.programs['old'][1] if use_old_filter else g['area_filter']
        return gather_program, filter_program

    white = mutant('white-noise mutation', lambda t: t.replace(
        t[t.index(R2_BLOCK):t.index('        // Preserve Phase 1 radii')],
        '        uint seed = areaHash(pixel.x ^ areaHash(pixel.y) ^ area_seeds[light]);\n'
        '        float rotation = float(seed & 0xffffu)*(6.28318530718/65536.0);\n'
        '        float jitter = float((seed >> 16) & 0xffffu)/65536.0;\n'))
    mutated['white'] = white
    mutated['old filter'] = mutant('old filter mutation', use_old_filter=True)
    mutated['flipped bias'] = mutant('flipped receiver bias mutation', lambda t: t.replace(
        'vec3 origin = pos+normal*areaReceiverBias(uv,pos);', 'vec3 origin = pos-normal*areaReceiverBias(uv,pos);'))
    mutated['degenerate'] = mutant('x+y-degenerate rank mutation', degenerate_mutation)
    mutated['bayer'] = mutant('4x4 interleave mutation', bayer_mutation)

    measures = {}

    def measure_for(width):
        if width not in measures:
            measures[width] = Measure(g, rig, width, width * 9 // 16)
        return measures[width]

    # ---- 1. independent oracles: converged march, analytic disk-light profile --------------------
    # (plan RELIGHT_WORLD_SHADOWS_PLAN Phase 0, C14.) The analytic profile is the geometry alone, never a reference passed
    # through the shader filter. The oracle plan is CONVERGED (64 rays x 512 steps: the marcher's step quantisation, a
    # property of the plan the user picks and identical at HEAD, is below a pixel) so the plan's limit applies as written:
    # the 50 % crossing within 1 full-resolution pixel, umbra <= analytic + 3/255, lit >= 252/255, 10-90 % width within 20 %.
    formula = 1.374 * LIGHT[3] * (4.0 - 2.0) / 2.0  # 1.374 r (d_r - d_b) / d_b
    for width in (1920, 3840):
        m = measure_for(width)
        require(abs(m.width_a - formula) <= .2 * formula, 'analytic penumbra width %.3f vs 1.374 r (dr-db)/db = %.3f' % (m.width_a, formula))
        for half in (False, True):
            plan = dict(samples=64, steps=ORACLE_STEPS, near=0, half=half)
            radius = filter_radius(2.0, width * 9 // 16 / 1080, half)
            imgs = m.images('new', plan, radius, seeds=SEEDS[:4])
            err, wd = m.edge(imgs)
            require(abs(err) <= m.px, 'edge location %dp half=%s: %.2f px off the analytic 50%% crossing, allowed 1' % (width * 9 // 16, half, err / m.px))
            require(abs(wd - m.width_a) <= .2 * m.width_a, 'penumbra width %dp half=%s: %.3f vs analytic %.3f' % (width * 9 // 16, half, wd, m.width_a))
            margin = int(2 * radius * (2 if half else 1)) + 3
            umbra = margin_mask(np, (m.fa <= .001) & m.receiver, margin)
            lit = margin_mask(np, (m.fa >= .999) & m.receiver, margin)
            hi, lo = float(imgs[:, :, umbra].max()) * 255, float(imgs[:, :, lit].min()) * 255
            require(hi <= 3.0, 'umbra contrast %dp half=%s: %.1f/255 > 3' % (width * 9 // 16, half, hi))
            require(lo >= 252.0, 'lit contrast %dp half=%s: %.1f/255 < 252' % (width * 9 // 16, half, lo))
            print('[RelightQ] oracle %dp half=%d 64r/%ds: edge %+.2f px (limit 1), width %.3f vs %.3f, umbra max %.1f, lit min %.1f /255' %
                  (width * 9 // 16, half, ORACLE_STEPS, err / m.px, wd, m.width_a, hi, lo), flush=True)
    # The oracles reject: a gather whose receiver bias points into the surface (self-shadowing acne), a wrong light position, and the
    # pre-converged march (64 steps: the quantisation moves the edge by several pixels, so the 1 px bound has teeth).
    m = measure_for(1920)
    plan = dict(samples=64, steps=ORACLE_STEPS, near=0, half=False)
    imgs = m.images('new', plan, 2.0, seeds=SEEDS[:2], programs=mutated['flipped bias'])
    lit = margin_mask(np, (m.fa >= .999) & m.receiver, 6)
    flipped_lit = float(imgs[:, :, lit].min()) * 255
    require(flipped_lit < 252.0, 'mutation: flipped receiver bias must fail the lit-contrast oracle (lit min %.1f)' % flipped_lit)
    shifted = np.stack([m.rig.chain('new', m.scene, (LIGHT[0] + .1, LIGHT[1], LIGHT[2], LIGHT[3]), plan, 2.0, 1080, seed=s).astype(float) / 255 for s in SEEDS[:2]])
    err_shift, _ = m.edge(shifted)
    require(abs(err_shift) > m.px, 'mutation: a light 10 cm off must fail the edge oracle (%.2f px)' % (err_shift / m.px))
    coarse, _ = m.edge(m.images('new', dict(samples=64, steps=64, near=0, half=False), 2.0, seeds=SEEDS[:2]))
    require(abs(coarse) > m.px, 'mutation: an unconverged 64-step march must fail the 1 px edge oracle (%.2f px)' % (coarse / m.px))
    print('[RelightQ] oracle mutations rejected: flipped receiver bias (lit min %.1f/255), light 10 cm off (edge %+.1f px), 64-step march (edge %+.1f px)' %
          (flipped_lit, err_shift / m.px, coarse / m.px), flush=True)

    # ---- 2. P0 does not move the mean shadow edge or width at the same plan ----------------------
    for width in (1920, 3840):
        m = measure_for(width)
        half_scale = width * 9 // 16 / 1080
        for plan in (dict(samples=4, steps=8, near=0, half=True), dict(samples=8, steps=12, near=0, half=True),
                     dict(samples=5, steps=64, near=48, half=True), dict(samples=16, steps=32, near=0, half=False)):
            radius = filter_radius(2.0, half_scale, plan['half'])
            # 80 rows x 8 seeds: the white-noise pre-P0 mean edge is itself a noisy estimate (+-4 px at 16 rows).
            e_new, w_new = m.edge(m.images('new', plan, radius, seeds=SEEDS, keep=40))
            e_old, w_old = m.edge(m.images('old', plan, radius, seeds=SEEDS, keep=40))
            require(abs(e_new - e_old) <= EDGE_WORLD_TOLERANCE, 'P0 moved the mean edge %dp %s by %.1f px' % (width * 9 // 16, plan, (e_new - e_old) / m.px))
            require(abs(w_new - w_old) <= .05 * w_old, 'P0 changed the penumbra width %dp %s: %.3f vs %.3f' % (width * 9 // 16, plan, w_new, w_old))
    print('[RelightQ] oracle no-shift PASS: mean edge within 1 cm and width within 5 percent of the pre-P0 shader at equal plans (Low, Medium, Thin 48+16, full-res)', flush=True)

    # ---- 3. noise AND reference error: every effective plan, pre-P0 pipeline vs P0 pipeline -------
    # The limits are the plan's: filtered RMS against a 64-ray reference <= 6/255 at Medium 1080p K4 and <= 8/255 for every
    # other preset / size / K / Thin state at the default budget. The error is measured against a reference image, not
    # around the images' own seed mean (that would hide a bias): each plan is compared with the same pipeline's 64-ray
    # run at the plan's own march (the marcher's step count is the plan's, identical at HEAD; the converged oracle above
    # covers the marcher itself). The spread of the plan around its own mean (noise) is reported and bounded too.
    new_plans, old_plans = unique_rows(rows), unique_rows(old_rows)
    results = {}
    true_refs, true_report = {}, []
    for version, plans in (('new', new_plans), ('old', old_plans)):
        for (height, samples, steps, near, half), labels in sorted(plans.items()):
            width = height * 16 // 9
            m = measure_for(width)
            plan = dict(samples=samples, steps=steps, near=near, half=bool(half))
            radius = radius_for(height / 1080, plan['half'], version == 'new')
            imgs = m.images(version, plan, radius)
            noise = m.noise(imgs)
            refs = m.images(version, dict(plan, samples=REFERENCE_RAYS), radius, seeds=SEEDS[:4])
            error = m.reference_error(imgs, refs.mean(axis=0))
            bias = m.reference_error(imgs.mean(axis=0, keepdims=True), refs.mean(axis=0))
            results[(version, height, samples, steps, near, half)] = (noise, labels, radius, error)
            print('[RelightQ] grain %-3s %4dp %2d rays x %3d steps%s %s r=%.1f: error vs %d-ray reference %.2f/255 (noise %.2f, mean offset %.2f)   %s' %
                  (version, height, samples, steps, ' +%d near' % near if near else '', 'half' if half else 'full', radius, REFERENCE_RAYS,
                   error, noise, bias,
                   ' '.join('%s%s/K%d/%s' % ('R' if len(l) > 3 else ('T' if l[0] else 'U'), '', l[1], ['Low', 'Medium', 'High', 'Cinematic'][l[2]]) for l in labels[:4])), flush=True)
            if version == 'new':
                # REPORT ONLY (user decision 10/22, gate redefined): the error against the TRUE 64-ray x 64-step reference, which
                # includes the coarse-step march bias of the plan. Phase 0 is judged on GRAIN (the gate above: error against a
                # 64-ray reference at the plan's own steps); the march bias is left to the world-space phase and tracked here.
                key = (width, plan['half'])
                if key not in true_refs:
                    true_refs[key] = m.images('new', dict(samples=64, steps=64, near=0, half=plan['half']), radius, seeds=SEEDS[:4]).mean(axis=0)
                true_error = m.reference_error(imgs, true_refs[key])
                true_bias = m.reference_error(imgs.mean(axis=0, keepdims=True), true_refs[key])
                true_report.append((height, samples, steps, near, half, true_error, true_bias, labels))
    new_results = {k: v for k, v in results.items() if k[0] == 'new'}
    worst_new = max(v[3] for v in new_results.values())
    worst_noise = max(v[0] for v in new_results.values())
    require(worst_new <= NOISE_LIMIT, 'P0 reference error %.2f/255 exceeds %.1f at some plan' % (worst_new, NOISE_LIMIT))
    require(worst_noise <= NOISE_LIMIT, 'P0 noise %.2f/255 exceeds %.1f at some plan' % (worst_noise, NOISE_LIMIT))
    medium = [v[3] for k, v in new_results.items() if k[1] == 1080 and (k[2], k[3], k[4], k[5]) == (8, 12, 0, 1)]
    require(medium and max(medium) <= MEDIUM_1080P_LIMIT, 'Medium 1080p K4 reference error %s exceeds %.1f/255' % (medium, MEDIUM_1080P_LIMIT))
    # pre-P0 must fail where the plan says: 4K Cinematic (1 ray)
    old_4k_cinematic = [v[3] for k, v in results.items() if k[0] == 'old' and k[1] == 2160 and k[5] == 0 and k[2] == 1]
    require(old_4k_cinematic and min(old_4k_cinematic) > NOISE_LIMIT, 'the pre-P0 pipeline must fail the limit at 4K Cinematic: %s' % old_4k_cinematic)
    # per labelled configuration: never worse than pre-P0 (0.5/255 slack), strictly better where pre-P0 failed
    lookup_new = {l: v[3] for k, v in new_results.items() for l in v[1]}
    lookup_old = {l: v[3] for k, v in results.items() if k[0] == 'old' for l in v[1]}
    improved = 0
    for label, old_error in lookup_old.items():
        new_error = lookup_new[label]
        require(new_error <= old_error + .5, 'P0 is further from the reference than pre-P0 at %s: %.2f vs %.2f' % (label, new_error, old_error))
        if old_error > NOISE_LIMIT:
            improved += 1
            require(new_error <= NOISE_LIMIT, 'P0 still above the limit where pre-P0 failed: %s %.2f' % (label, new_error))
    require(improved > 0, 'at least one configuration must have failed before P0')
    for height, samples, steps, near, half, true_error, true_bias, labels in true_report:
        names = sorted({['Low', 'Medium', 'High', 'Cinematic'][l[2]] for l in labels})
        print('[RelightQ] grain REPORT-ONLY %4dp %2d rays x %3d steps%s %s: error vs the true 64x64 reference %.2f/255 (mean offset %.2f; not gated, left to the world-space phase)  presets %s' %
              (height, samples, steps, ' +%d near' % near if near else '', 'half' if half else 'full', true_error, true_bias, '/'.join(names)), flush=True)
    print('[RelightQ] grain REPORT-ONLY worst error vs the true 64x64 reference %.2f/255 (gate redefined by user decision 10/22: Phase 0 is judged on grain; the plan\'s original wording, 64 rays / 64 steps within 6 and 8/255, is reported not gated)' %
          max(r[5] for r in true_report), flush=True)
    print('[RelightQ] grain noise PASS: worst P0 reference error %.2f/255 (noise %.2f; limit %.1f, Medium 1080p K4 %.2f <= %.1f); pre-P0 failed %d configurations and fails 4K Cinematic at %.1f/255' %
          (worst_new, worst_noise, NOISE_LIMIT, max(medium), MEDIUM_1080P_LIMIT, improved, min(old_4k_cinematic)), flush=True)

    # ---- 4. mutations of each fix must fail the noise/receiver gates ---------------------------
    m = measure_for(3840)
    low = dict(samples=4, steps=8, near=0, half=True)
    base_imgs = m.images('new', low, 2.0)
    base = m.noise(base_imgs)
    white_noise = m.noise(m.images('new', low, 2.0, programs=mutated['white']))
    require(white_noise >= 1.25 * base, 'mutation: per-cell white noise must be >= 25 %% noisier (%.2f vs %.2f)' % (white_noise, base))
    r1 = m.noise(m.images('new', low, 1.0))
    require(r1 >= 1.15 * base, 'mutation: a 1-trace-pixel blur (pre-P0 at 1080p half) must be >= 15 %% noisier (%.2f vs %.2f)' % (r1, base))
    bumped = bumpy(np, m.scene, 4, 25)
    new_bumpy = m.noise(m.images('new', low, 2.0, scene=bumped))
    old_filter_bumpy = m.noise(m.images('new', low, 2.0, scene=bumped, programs=mutated['old filter']))
    require(new_bumpy <= 1.15 * base + .3, 'normal-mapped wall: P0 noise %.2f vs flat %.2f' % (new_bumpy, base))
    require(old_filter_bumpy >= 1.3 * new_bumpy, 'mutation: the normal-mapped edge-stop must be >= 30 %% noisier on a normal-mapped wall (%.2f vs %.2f)' % (old_filter_bumpy, new_bumpy))
    old_plan = dict(samples=7, steps=12, near=0, half=True)
    old_bumpy = m.noise(m.images('old', old_plan, 2.0, scene=bumped))
    old_flat = m.noise(m.images('old', old_plan, 2.0))
    require(old_bumpy > NOISE_LIMIT and old_bumpy >= 1.3 * old_flat, 'the pre-P0 pipeline must fail the normal-mapped wall (%.2f flat %.2f)' % (old_bumpy, old_flat))
    speckle = int(np.sum(base_imgs[:, :, m.receiver & (m.fa > .999)] * 255 < SPECKLE_LEVEL))
    speckle_bump = int(np.sum(m.images('new', low, 2.0, scene=bumped)[:, :, m.receiver & (m.fa > .999)] * 255 < SPECKLE_LEVEL))
    require(speckle == 0 and speckle_bump == 0, 'lit-region speckle below %d/255: flat %d, normal-mapped %d' % (SPECKLE_LEVEL, speckle, speckle_bump))
    print('[RelightQ] grain mutations rejected: white noise %.2f vs R2 %.2f, 1-px blur %.2f, old normal edge-stop on a normal-mapped wall %.2f vs %.2f; pre-P0 normal-mapped wall %.2f (flat %.2f); lit speckle 0' %
          (white_noise, base, r1, old_filter_bumpy, new_bumpy, old_bumpy, old_flat), flush=True)

    # ---- 5. grazing receiver --------------------------------------------------------------------------
    gz = grazing_scene(g, np)
    S, scene, bias = gz['S'], gz['scene'], gz['bias']
    h, w = scene['ident'].shape
    keep = 24
    band = slice(h // 2 - keep, h // 2 + keep)
    origins = (scene['world'] + scene['normal_world'] * bias[:, :, None])[band].reshape(-1, 3)
    fraction = S.visible_fraction(np, origins, gz['light'], .3, [gz['panel']]).reshape(2 * keep, w)
    view_light = (gz['cam']['view'] @ np.append(gz['light'], 1))[:3]
    floor_pixels = scene['ident'][band] == 0
    pen_mask = floor_pixels & (fraction > .02) & (fraction < .98)
    lit_mask = floor_pixels & (fraction > .999)
    for _ in range(6):  # stay 6 pixels clear of the penumbra: the blur legitimately reaches 3-4 pixels into the lit side
        lit_mask = lit_mask & np.roll(lit_mask, 1, 0) & np.roll(lit_mask, -1, 0) & np.roll(lit_mask, 1, 1) & np.roll(lit_mask, -1, 1)
    require(pen_mask.sum() > 2000, 'grazing fixture has a penumbra (%d pixels)' % pen_mask.sum())
    grazing = {}
    for version, plan, radius in (('old', dict(samples=8, steps=12, near=0, half=True), radius_for(1.0, True, False)),
                                  ('new', dict(samples=8, steps=12, near=0, half=True), radius_for(1.0, True, True))):
        imgs = np.stack([rig.chain(version, scene, (*view_light, .3), plan, radius, 1080, seed=s, keep=keep).astype(float) / 255 for s in SEEDS])
        mean = imgs.mean(axis=0)
        grazing[version] = float(np.sqrt(np.mean((imgs[:, pen_mask] - mean[None, pen_mask]) ** 2) * len(SEEDS) / (len(SEEDS) - 1)) * 255)
        if version == 'new':
            require(int(np.sum(imgs[:, lit_mask] * 255 < SPECKLE_LEVEL)) == 0, 'grazing floor: lit-region speckle')
    require(grazing['new'] <= NOISE_LIMIT, 'grazing floor P0 noise %.2f exceeds %.1f' % (grazing['new'], NOISE_LIMIT))
    require(grazing['old'] > .75 * NOISE_LIMIT and grazing['old'] >= 3 * grazing['new'], 'the pre-P0 pipeline must fail the grazing floor (%.2f vs %.2f)' % (grazing['old'], grazing['new']))
    print('[RelightQ] grain grazing floor PASS (~87 degrees incidence, Medium 8r/12s half): P0 %.2f/255, pre-P0 %.2f/255, lit speckle 0' % (grazing['new'], grazing['old']), flush=True)

    # ---- 6. spectrum: no periodic structure in a uniform penumbra patch ----------------------------
    # Plan O6: no bin above 2/255 at ANY frequency. The expected penumbra is removed first (the ensemble mean of the other
    # light-UUID seeds: it carries the ramp, the plan's own march bias and the slow vertical drift of the geometry, so none
    # of them leaks into the spectrum as a false low-frequency line), then EVERY non-DC bin is bounded, from the longest
    # periods of the patch up to Nyquist. The line/noise ratio test stays. A structure that is only 0.09 cycles per pixel
    # (11-pixel period) was invisible to the previous gate, which looked at f >= 0.15 only.
    width = 1920
    spec_scene = wall_scene(np, width, y_half=.5, blocker_z=-5.2, blocker_from=int((0.04 + 3) / 6 * width))
    fa, xs = analytic_profile(np, spec_scene, LIGHT, LIGHT[3], .02)
    centre = int(np.argmin(abs(fa - .5)))
    patch_cols = slice(centre - 128, centre + 128)
    spectrum_plans = (('Low', dict(samples=4, steps=8, near=0, half=True)), ('Medium', dict(samples=8, steps=12, near=0, half=True)))
    patch_cache = {}

    def patch_images(programs, plan):
        key = (id(programs), plan['samples'], plan['steps'])
        if key not in patch_cache:
            patch_cache[key] = np.stack([rig.chain('new', spec_scene, LIGHT, plan, 2.0, 1080, seed=s, keep=64, programs=programs).astype(float) / 255
                                         for s in SEEDS])[:, :, patch_cols]
        return patch_cache[key]

    def spectra(programs, plan, injected=None):
        """Per seed: the spectrum of (image - mean of the other seeds). Returns (peak, ratio, per-band peaks) of the worst seed."""
        imgs = patch_images(programs, plan)
        peaks, ratios, bands = [], [], []
        for i in range(len(imgs)):
            residual = imgs[i] - np.delete(imgs, i, axis=0).mean(axis=0)
            if injected is not None:
                residual = residual + injected
            peak, ratio, band = spectrum_lines(np, residual)
            peaks.append(peak), ratios.append(ratio), bands.append(band)
        return max(peaks), max(ratios), [max(b[k] for b in bands) for k in range(4)]

    # (a) structure that depends on the light UUID: image minus the mean of the OTHER seeds (above). By construction this
    # cancels anything every seed shares, i.e. a screen-fixed pattern of the filter or the gather that does not depend on the
    # light; that is checked separately below against an independent profile.
    rank = {}
    for label, plan in spectrum_plans:
        rank[label] = spectra(None, plan)
        require(rank[label][0] <= SPECTRUM_PEAK and rank[label][1] <= SPECTRUM_LINE_RATIO,
                'R2 %s penumbra residual: strongest bin %.2f/255 (limit %.1f), line ratio %.1f (limit %.1f)' %
                (label, rank[label][0], SPECTRUM_PEAK, rank[label][1], SPECTRUM_LINE_RATIO))
    # (b) structure SHARED by every seed (fix round 2, Codex P0 / Opus P1): the ensemble mean of the rendered images minus an
    # INDEPENDENT expected profile, the analytic disk-light profile of the geometry (never a render), warped by the only freedoms
    # the plan's own march bias has (a shift and a scale of the position axis, a gain and an offset), fitted to the mean profile.
    # A screen-fixed stripe of the production filter or gather does not fit those four numbers and stays in the residual.
    # The plan's rays are the plan's, but its march is converged (512 steps): the coarse-step march bias is not noise structure,
    # is reported by the 64x64 line, and its non-affine shape would otherwise land in the lowest bins as a false line.
    oracle = measure_for(1920)
    fa_cols = np.arange(width)
    oracle_fa = oracle.fa
    oracle_centre = int(np.argmin(abs(oracle_fa - .5)))
    shared_cols = slice(oracle_centre - 128, oracle_centre + 128)
    shared_cache = {}

    def shared_images(programs, plan):
        key = (id(programs), plan['samples'], plan['steps'])
        if key not in shared_cache:
            shared_cache[key] = np.stack([rig.chain('new', oracle.scene, LIGHT, plan, 2.0, 1080, seed=sd, keep=64, programs=programs).astype(float) / 255
                                          for sd in SEEDS])[:, :, shared_cols]
        return shared_cache[key]

    sigmas, scales, shifts = (0.0, 1.0, 2.0, 3.0, 4.0, 6.0), np.arange(.90, 1.1001, .01), np.arange(-40.0, 40.01, .5)
    cols = np.arange(shared_cols.start, shared_cols.stop, dtype=float)
    pivot = 0.5 * (shared_cols.start + shared_cols.stop - 1)
    # The 2-D expectation (fix round 3, Codex P0 / Opus P1): the analytic visible fraction for EVERY row of the patch (the receiver's
    # own height against the light), over the patch columns plus the margin the fit's shift, scale and blur reach into.
    scene_h = oracle.scene['depth'].shape[0]
    row_ids = np.arange(scene_h // 2 - 64, scene_h // 2 + 64)
    row_y = ((row_ids + .5) / scene_h * 2 - 1) * oracle.scene['y_half']
    ext_lo, ext_hi = shared_cols.start - 64, shared_cols.stop + 64
    ext_cols = np.arange(ext_lo, ext_hi, dtype=float)
    expected2d = analytic_rows(np, oracle.scene, LIGHT, LIGHT[3], oracle.bias, row_y, ext_lo, ext_hi)
    profile1d = expected2d.mean(axis=0)

    def blurred(values, sigma):
        if sigma <= 0:
            return values
        taps = np.arange(-int(4 * sigma) - 1, int(4 * sigma) + 2)
        kernel = np.exp(-taps ** 2 / (2 * sigma ** 2))
        kernel /= kernel.sum()
        padded = np.pad(values, [(0, 0)] * (values.ndim - 1) + [(len(taps), len(taps))], mode='edge')
        flat = padded.reshape(-1, padded.shape[-1])
        out = np.stack([np.convolve(row, kernel, mode='same') for row in flat])
        return out.reshape(padded.shape)[..., len(taps):-len(taps)]

    # ONE global fit for the whole patch: gain and offset by least squares, (blur, scale, shift) by the smallest residual, on the
    # row-averaged image against the row-averaged expectation. Five numbers for 128 x 256 pixels, none of which can follow a
    # stripe in x, in y or on a diagonal (the per-row fit of the previous round could: it removed anything constant along a row).
    # Afterwards only a smooth quadratic drift of the rows' mean (three numbers) is removed, never a narrow-band structure.
    grid = [(sg, sc, sh) for sg in sigmas for sc in scales for sh in shifts]
    blur_cache = {sg: blurred(profile1d, sg) for sg in sigmas}
    bases = np.stack([np.interp(pivot + sc * (cols - pivot) + sh, ext_cols, blur_cache[sg]) for sg, sc, sh in grid])
    base_centred = bases - bases.mean(axis=1, keepdims=True)
    base_energy = (base_centred ** 2).sum(axis=1)

    def shared_residual(programs, plan, injected=None):
        imgs = shared_images(programs, plan)
        extra = 0.0 if injected is None else injected(imgs.shape[1:])   # an in-memory structure added to the rendered mean (a mutant)
        mean2d = imgs.mean(axis=0) + extra                            # (rows, patch columns)
        profile = mean2d.mean(axis=0)
        centred = profile - profile.mean()
        gain = (base_centred @ centred) / np.maximum(base_energy, 1e-12)
        err = (centred ** 2).sum() - gain ** 2 * base_energy
        k = int(np.argmin(err))
        sg, sc, sh = grid[k]
        warped = np.stack([np.interp(pivot + sc * (cols - pivot) + sh, ext_cols, row) for row in blurred(expected2d, sg)])
        model = gain[k] * (warped - bases[k].mean()) + profile.mean()
        residual = mean2d - model
        rows_index = np.arange(residual.shape[0], dtype=float)
        coefficients = np.polyfit(rows_index, residual.mean(axis=1), 2)
        drift = np.polyval(coefficients, rows_index)
        # The two independent halves of the seeds (even / odd), against the same model and drift, for the block read.
        halves = [imgs[k::2].mean(axis=0) + extra - model - drift[:, None] for k in (0, 1)]
        return residual - drift[:, None], (0.0, sg, sc, sh), float(np.abs(drift).max() * 255), halves

    def shared_spectrum(programs, plan, injected=None):
        residual, best, drift, halves = shared_residual(programs, dict(plan, steps=ORACLE_STEPS), injected)
        peak, ratio, band = spectrum_lines(np, residual)
        blocks, rowmax = structure_blocks(np, residual, halves)
        return peak, band, best, drift, blocks, rowmax

    def spectral_failure(r):
        """The spectral part of the shared-structure gate: the overall peak and the four bands."""
        return r[0] > SPECTRUM_PEAK or any(v > lim for v, lim in zip(r[1], SPECTRUM_BAND_LIMITS))

    def gate_failures(r):
        """Every part of the shared-structure gate that this reading fails: spectral, drift, block structure, row comb."""
        out = []
        if spectral_failure(r): out.append('spectrum')
        if r[3] > SPECTRUM_DRIFT: out.append('drift')
        if any(v > lim for v, lim in zip(r[4], SPECTRUM_BLOCK_LIMITS)): out.append('blocks')
        if r[5] > SPECTRUM_ROW_LIMIT: out.append('rows')
        return out

    shared = {}
    for label, plan in spectrum_plans:
        shared[label] = shared_spectrum(None, plan)
        require(shared[label][0] <= SPECTRUM_PEAK,
                'shared (all-seed) structure %s: strongest bin %.2f/255 of the rendered mean against the 2-D analytic expectation, limit %.1f' %
                (label, shared[label][0], SPECTRUM_PEAK))
        for band, limit, value in zip(('f<.04', '.04-.08', '.08-.15', '>=.15'), SPECTRUM_BAND_LIMITS, shared[label][1]):
            require(value <= limit, 'shared (all-seed) structure %s: band %s strongest bin %.2f/255 above its limit %.2f' % (label, band, value, limit))
        # Fix round 4 (Opus P2): the quadratic y-drift removed from the residual is bounded too, or a slow y wave / ramp of any size hid in it.
        require(shared[label][3] <= SPECTRUM_DRIFT,
                'shared (all-seed) structure %s: the fitted quadratic y-drift is %.2f/255, limit %.2f' % (label, shared[label][3], SPECTRUM_DRIFT))
        # Fix round 5 (Opus P2): block structure and row comb of the honest render.
        for block, limit, value in zip(SPECTRUM_BLOCKS, SPECTRUM_BLOCK_LIMITS, shared[label][4]):
            require(value <= limit, 'shared (all-seed) structure %s: cross-half covariance of the %dx%d block means %.2f/255 above its limit %.2f' % (label, block, block, value, limit))
        require(shared[label][5] <= SPECTRUM_ROW_LIMIT, 'shared (all-seed) structure %s: largest row mean %.2f/255 above its limit %.2f' % (label, shared[label][5], SPECTRUM_ROW_LIMIT))
        print('[RelightQ] grain honest block structure %s: cross-half block covariance 4x4 %.2f, 8x8 %.2f, 16x16 %.2f; largest row mean %.2f (/255)' %
              (label, *shared[label][4], shared[label][5]), flush=True)
    # Mutations, injected into the RENDERED output before any measurement: production-filter shader variants that add a structure to
    # the final (full-resolution) image, measured through the same chain. Fix round 3 (Codex P0 / Opus P1): the per-row fit of the
    # previous round removed anything constant along a row, so a y-oriented stripe (0.078, 0.094 or 0.03 cycles/px, additive or
    # multiplicative) and row-parity stripes passed. Every one of them MUST fail now.
    filter_src = filter_text.rstrip()
    assert filter_src.endswith('}')
    TAU = '6.28318530718'

    def block_mutant(shift, amplitude):
        """A +-amplitude/255 sign per (1 << shift)-pixel block from an integer hash of the block: the same for every light (a shared structure)."""
        return ('uvec2 mb = uvec2(gl_FragCoord.xy) >> %du; uint mh = mb.x*73856093u ^ mb.y*19349663u; mh ^= mh >> 13; mh *= 0x5bd1e995u; mh ^= mh >> 15; '
                'frag_color.rgb += vec3(((mh&1u)==1u ? %d.0 : -%d.0)/255.0);' % (shift, amplitude, amplitude))
    patch_shape = patch_images(None, spectrum_plans[0][1]).shape[1:3]
    stripe_defs = (
        ('x-stripe 0.094 c/px', 'frag_color.rgb += vec3(3.0/255.0*sin(%s*0.094*gl_FragCoord.x));' % TAU),
        ('y-stripe 0.078125 c/px', 'frag_color.rgb += vec3(3.0/255.0*sin(%s*0.078125*gl_FragCoord.y));' % TAU),
        ('y-stripe 0.094 c/px', 'frag_color.rgb += vec3(3.0/255.0*sin(%s*0.094*gl_FragCoord.y));' % TAU),
        ('y-stripe 0.03 c/px', 'frag_color.rgb += vec3(3.0/255.0*sin(%s*0.03*gl_FragCoord.y));' % TAU),
        ('multiplicative y-stripe 0.094 c/px', 'frag_color.rgb *= vec3(1.0+8.0/255.0*sin(%s*0.094*gl_FragCoord.y));' % TAU),
        ('diagonal stripe 0.094 c/px', 'frag_color.rgb += vec3(3.0/255.0*sin(%s*0.094*0.70710678*(gl_FragCoord.x+gl_FragCoord.y)));' % TAU),
        ('row parity +-3/255', 'frag_color.rgb += vec3(((int(gl_FragCoord.y)&1)==1 ? 3.0 : -3.0)/255.0);'),
        ('+3/255 on every 8th row', 'if ((int(gl_FragCoord.y)&7)==0) frag_color.rgb += vec3(3.0/255.0);'),
        # Fix round 4 (Opus P2): a 2.6/255 diagonal stripe whose frequency falls half-way between two bins of the patch in BOTH axes
        # read 1.88 under the Hann window and passed the 2.0 limit; the zero-padded read must fail it.
        ('half-bin diagonal 2.6/255', 'frag_color.rgb += vec3(2.6/255.0*sin(%s*(%.9f*gl_FragCoord.x+%.9f*gl_FragCoord.y)));' %
         (TAU, (int(0.0957 * patch_shape[1]) + .5) / patch_shape[1], (int(0.05 * patch_shape[0]) + .5) / patch_shape[0])),
        # Fix round 5 (Opus P2): energy the spectrum cannot see. A random +-3/255 per block (4x4 and 8x8 pixels) and a comb of +3/255 on
        # every 16th row / +6/255 on every 32nd row each read below 1/255 in every spectral bin; the block-mean and row-mean reads fail them.
        ('4x4 block +-3/255 random', block_mutant(2, 3)),
        ('8x8 block +-3/255 random', block_mutant(3, 3)),
        ('every 16th row +3/255', 'if ((int(gl_FragCoord.y)&15)==0) frag_color.rgb += vec3(3.0/255.0);'),
        ('every 32nd row +6/255', 'if ((int(gl_FragCoord.y)&31)==0) frag_color.rgb += vec3(6.0/255.0);'))
    stripe_results = {}
    caught = {}
    stripe_blind = None
    for label, body in stripe_defs:
        src = filter_src[:-1] + '    if (area_axis == vec2(0.0)) { %s }\n}\n' % body
        mutated['stripe ' + label] = (g['area_gather'], rig.variant('filter mutation ' + label, {rig.paths['filter']: src}, 'class3/deferred/areaShadowFilterF.glsl'))
        stripe_results[label] = [shared_spectrum(mutated['stripe ' + label], plan) for _, plan in spectrum_plans]
        caught_by = sorted({part for r in stripe_results[label] for part in gate_failures(r)})
        require(caught_by, 'mutation: a rendered %s in the production filter must fail the shared-structure gate (peaks %s, bands %s, block RMS %s, row %s)' %
                (label, [round(r[0], 2) for r in stripe_results[label]], [[round(v, 2) for v in r[1]] for r in stripe_results[label]],
                 [[round(v, 2) for v in r[4]] for r in stripe_results[label]], [round(r[5], 2) for r in stripe_results[label]]))
        caught[label] = caught_by
        if label == 'x-stripe 0.094 c/px':
            stripe_blind = spectra(mutated['stripe ' + label], spectrum_plans[0][1])
    # Fix round 5 (Opus P2): the stripe loop above never exercised the drift assertion on its own. A linear ramp in y across the patch
    # (-3/255 .. +3/255 over the rows) has no spectral line at all and is absorbed by the quadratic drift fit that precedes the spectrum:
    # only the drift assertion can fail it, and it must. It is added to the rendered mean in memory (a shader ramp would clip against
    # the 0 and 1 of the shadowed and lit sides of the patch and stop being a pure function of y).
    def y_ramp(shape):
        return (3.0 / 255.0) * np.linspace(-1.0, 1.0, shape[0])[:, None]
    ramp_results = [shared_spectrum(None, plan, y_ramp) for _, plan in spectrum_plans]
    require(all(not spectral_failure(r) for r in ramp_results),
            'the y-ramp mutant is meant to be invisible to the spectrum (peaks %s, bands %s)' % ([round(r[0], 2) for r in ramp_results], [[round(v, 2) for v in r[1]] for r in ramp_results]))
    require(all(gate_failures(r) == ['drift'] for r in ramp_results),
            'mutation: a y-ramp must fail on the drift assertion ALONE (drifts %s, failing parts %s)' % ([round(r[3], 2) for r in ramp_results], [gate_failures(r) for r in ramp_results]))
    rejected = {}
    for name, programs in (('4x4 interleave', mutated['bayer']), ('x+y-degenerate rank', mutated['degenerate'])):
        worst = [spectra(programs, plan) for _, plan in spectrum_plans]
        rejected[name] = worst
        require(any(w[0] > SPECTRUM_PEAK or w[1] > SPECTRUM_LINE_RATIO for w in worst),
                'mutation: the %s must fail the spectrum gate (peaks %s, ratios %s)' % (name, [round(w[0], 2) for w in worst], [round(w[1], 1) for w in worst]))
    print('[RelightQ] grain spectrum PASS: every non-DC bin: seed-mean detrend R2 Low strongest %.2f/255 (bands f<.04 %.2f, .04-.08 %.2f, .08-.15 %.2f, >=.15 %.2f), Medium %.2f/255 (limit %.1f); line ratio Low %.1f, Medium %.1f (limit %.0f)' %
          (rank['Low'][0], *rank['Low'][2], rank['Medium'][0], SPECTRUM_PEAK, rank['Low'][1], rank['Medium'][1], SPECTRUM_LINE_RATIO), flush=True)
    print('[RelightQ] grain spectrum shared-structure PASS (ONE global fit of the 2-D analytic expectation + quadratic drift in y): Low strongest %.2f/255 (bands f<.04 %.2f, .04-.08 %.2f, .08-.15 %.2f, >=.15 %.2f; fit blur %.1f px scale %.3f shift %+.1f px, y drift %.2f/255), Medium %.2f/255 (bands %s; fit blur %.1f px scale %.3f shift %+.1f px, y drift %.2f/255), limits %.1f overall, bands %s' %
          (shared['Low'][0], *shared['Low'][1], shared['Low'][2][1], shared['Low'][2][2], shared['Low'][2][3], shared['Low'][3], shared['Medium'][0],
           [round(v, 2) for v in shared['Medium'][1]], shared['Medium'][2][1], shared['Medium'][2][2], shared['Medium'][2][3], shared['Medium'][3],
           SPECTRUM_PEAK, SPECTRUM_BAND_LIMITS), flush=True)
    print('[RelightQ] grain shared-structure mutants (caught by): ' + '; '.join('%s: %s' % (label, '+'.join(parts)) for label, parts in caught.items()) +
          '; y-ramp 3/255 over the patch: drift only (drift %s, spectral peaks %s)' % ([round(r[3], 2) for r in ramp_results], [round(r[0], 2) for r in ramp_results]), flush=True)
    print('[RelightQ] grain block/row reads of the block and comb mutants (Low, Medium; cross-half block covariance 4x4/8x8/16x16, largest row mean, /255): ' + '; '.join(
        '%s %s' % (label, [(tuple(round(v, 2) for v in r[4]), round(r[5], 2)) for r in stripe_results[label]])
        for label in stripe_results if 'block' in label or 'row +' in label), flush=True)
    print('[RelightQ] grain spectrum mutations rejected: ' + '; '.join(
        '%s peaks %s ratios %s' % (name, [round(w[0], 2) for w in worst], [round(w[1], 1) for w in worst]) for name, worst in rejected.items()) +
          '; production-filter shader mutants injected in the rendered output (Low, Medium peaks /255, band peaks of the worst): ' + '; '.join(
              '%s %s %s' % (label, [round(r[0], 2) for r in results], [round(max(r[1][k] for r in results), 2) for k in range(4)])
              for label, results in stripe_results.items()) +
          ' (the seed-mean detrend alone reads %.2f on the x-stripe: blind to it)' % stripe_blind[0], flush=True)

    # ---- 6b. hybrid boundary, thin wall ------------------------------------------------------------
    # (i) Thin/regular boundary (fix round 3, Opus P1). A Thin light next to the first regular light marches the share
    # area_regular_mix of its rays the regular way. On the dense-rail fixture of the Thin check (a 4 cm rail the regular march
    # misses and the Thin march catches) the gather of that light must be: byte-identical to a regular light at share 1 (a Thin
    # light that ties the first regular light IS regular), byte-identical to the pure Thin light at share 0, and move by at most
    # one ray per step of 1/rays in between (continuous, no pop). A mutant that switches ALL rays as soon as the share is non-zero
    # pops and fails; a shader that ignores the share fails the identity.
    rail_w, rail_h = 1536, 256
    rail_projection = np.eye(4)
    rail_projection[0, 0] = 1 / 3
    rail_projection[1, 1] = 2
    rail_projection[2, 2] = -.1
    rail_x = (np.arange(rail_w) + .5) / rail_w * 6 - 3
    rail_depth = np.full((rail_h, rail_w, 4), .7, np.float32)
    rail_depth[:, abs(rail_x) < .02] = .655
    rail_normals = np.zeros_like(rail_depth)
    rail_normals[:, :, :2] = .5
    rail_scene = dict(depth=rail_depth, normals=rail_normals, projection=rail_projection, inverse=np.linalg.inv(rail_projection))
    rail_light = (.8, 0.0, -4 + np.sqrt(2.5 ** 2 - .8 ** 2), .1)

    def boundary(plan, mix=0.0, programs=None, rest=0, filtered=False):
        """One light on the rail fixture: the raw gather (default) or the production chain (gather + blur + upsample, Denoise 2)."""
        if filtered:
            return rig.chain('new', rail_scene, rail_light, plan, 2.0, 1080, seed=12345, thickness=.035, keep=8,
                             programs=programs, mix=mix, rest_steps=rest).astype(np.int32)
        return rig.chain('new', rail_scene, rail_light, plan, 0, 1080, seed=12345, thickness=.035, upto='gather', keep=8,
                         programs=programs, mix=mix, rest_steps=rest).astype(np.int32)

    # Fix round 4 (Codex P0; GLM / DeepSeek P1): the boundary is gated on the PRODUCTION hybrid plans (every distinct hybrid of the
    # default-budget grid, Thin ON, no live feed), whose Thin group marches EXACTLY the regular ray count, so that the regular
    # light's disk strata and the boundary light's agree. Endpoint equality is measured after the production filter too (<= 1/255), not
    # only on the raw gather. The previous round's plans (Codex's 4K Medium K4: Thin 4 x 64 beside regular 8 x 12, 22/255 apart after
    # the filter) are the mutation and must FAIL the same equality.
    hybrid_plans = sorted({(r[5], r[6], bool(r[8]), r[13], r[14]) for r in policy['grid']
                           if r[0] == 1 and r[4] == 0 and r[7] and r[12] < r[9]})
    require(len(hybrid_plans) >= 2, 'the default-budget grid must contain production hybrid plans for the boundary gate (found %d)' % len(hybrid_plans))
    # Fix round 5 (DeepSeek P1): two more rows, where the regular march has MORE steps than the Thin draw's far segment (a Cinematic
    # uniform 32 beside a Thin far floor of 8) and more steps than the whole Thin draw (a Custom uniform 64 beside 48 near + 8 far = 56): the
    # shader no longer clamps the regular step count to the Thin draw's, so a boundary light at share 1 still IS the regular light.
    hybrid_plans = hybrid_plans + [(4, 56, True, 4, 64), (4, 56, True, 4, 32)]
    plan_reports = []
    for thin_samples, thin_steps, plan_half, rest_samples, rest_steps in hybrid_plans:
        require(thin_samples == rest_samples, 'production hybrid plan with unequal ray counts %d vs %d' % (thin_samples, rest_samples))
        thin_plan = dict(samples=thin_samples, steps=thin_steps, near=48, half=plan_half)
        regular_plan = dict(samples=rest_samples, steps=rest_steps, near=0, half=plan_half)
        label = '%dx%d Thin beside %dx%d regular%s' % (thin_samples, thin_steps, rest_samples, rest_steps, ' half' if plan_half else ' full')
        reference_regular, reference_thin = boundary(regular_plan), boundary(thin_plan)
        teeth = int(np.abs(reference_regular - reference_thin).max())
        require(teeth > 100, '%s: the rail fixture must separate the regular and the Thin march (max difference %d/255)' % (label, teeth))
        require(np.array_equal(boundary(thin_plan, 1.0, rest=rest_steps), reference_regular),
                '%s: a Thin light at share 1 must BE the regular light, byte for byte (raw gather)' % label)
        require(np.array_equal(boundary(thin_plan, 0.0, rest=rest_steps), reference_thin), '%s: share 0 must be the pure Thin light' % label)
        endpoint = int(np.abs(boundary(thin_plan, 1.0, rest=rest_steps, filtered=True) - boundary(regular_plan, filtered=True)).max())
        require(endpoint <= 1, '%s: the filtered endpoint differs by %d/255 (limit 1)' % (label, endpoint))
        # Continuity: one ray per step of 1/rays over the whole share range, and the production range 0 .. 0.5 in 1/64 steps.
        steps_of_share = [k / thin_samples for k in range(thin_samples + 1)]
        levels = [boundary(thin_plan, m, rest=rest_steps) for m in steps_of_share]
        worst_step = max(int(np.abs(levels[k] - levels[k - 1]).max()) for k in range(1, len(levels)))
        one_ray = int(np.ceil(255.0 / thin_samples)) + 1
        require(worst_step <= one_ray, '%s: the boundary moves by more than one ray per 1/%d step of the share (%d/255 > %d)' %
                (label, thin_samples, worst_step, one_ray))
        fine = [boundary(thin_plan, m / 64.0, rest=rest_steps) for m in range(0, 33, 8)]
        worst_fine = max(int(np.abs(fine[k] - fine[k - 1]).max()) for k in range(1, len(fine)))
        require(worst_fine <= one_ray, '%s: a share step of 1/8 over the production range 0 .. 0.5 moves the light by more than one ray (%d/255)' % (label, worst_fine))
        filtered_levels = [boundary(thin_plan, m, rest=rest_steps, filtered=True) for m in steps_of_share]
        worst_filtered = max(int(np.abs(filtered_levels[k] - filtered_levels[k - 1]).max()) for k in range(1, len(filtered_levels)))
        require(worst_filtered <= one_ray, '%s: the FILTERED boundary moves by more than one ray per step of the share (%d/255 > %d)' %
                (label, worst_filtered, one_ray))
        plan_reports.append((label, teeth, endpoint, worst_step, worst_filtered, one_ray - 1))
    # Mutation: the previous shader clamped the regular step count to the Thin draw's steps (here 56 < 64), shortening the regular
    # rays of the boundary light: the identity at share 1 breaks.
    clamp_old_text = gather_text.replace('clamp(area_rest_steps,1,64)', 'clamp(area_rest_steps,1,area_steps)')
    assert clamp_old_text != gather_text
    clamp_old = (rig.variant('rest steps clamp mutation', {rig.paths['gather']: clamp_old_text}, 'class3/deferred/areaShadowF.glsl'), g['area_filter'])
    clamp_thin = dict(samples=4, steps=56, near=48, half=True)
    clamp_regular = dict(samples=4, steps=64, near=0, half=True)
    require(not np.array_equal(boundary(clamp_thin, 1.0, programs=clamp_old, rest=64), boundary(clamp_regular)),
            'mutation: clamping the regular step count to the Thin draw\'s steps must break the share-1 identity (4x56 Thin beside 4x64 regular)')
    # Mutation (the previous round's production plan, Codex's 4K Medium K4 case): unequal ray counts. The boundary light at share 1
    # marches 4 rays beside a regular neighbour's 8: the filtered endpoints differ by far more than 1/255.
    bad_thin = dict(samples=4, steps=64, near=48, half=True)
    bad_regular = dict(samples=8, steps=12, near=0, half=True)
    unequal = int(np.abs(boundary(bad_thin, 1.0, rest=12, filtered=True) - boundary(bad_regular, filtered=True)).max())
    require(unequal > 4, 'mutation: a 4-ray boundary light beside an 8-ray regular light must differ visibly after the filter (%d/255 <= 4)' % unequal)
    # Shader mutations, on one production plan: switching every ray at once as soon as the share is non-zero pops; a shader that
    # ignores the share breaks the identity.
    thin_plan = dict(samples=hybrid_plans[0][0], steps=hybrid_plans[0][1], near=48, half=hybrid_plans[0][2])
    regular_plan = dict(samples=hybrid_plans[0][3], steps=hybrid_plans[0][4], near=0, half=hybrid_plans[0][2])
    boundary_rays, boundary_rest = thin_plan['samples'], hybrid_plans[0][4]
    reference_regular = boundary(regular_plan)
    steps_of_share = [k / boundary_rays for k in range(boundary_rays + 1)]
    one_ray = int(np.ceil(255.0 / boundary_rays)) + 1
    ray_text = gather_text
    pop_text = ray_text.replace('bool regular_ray = area_thin_objects && ray_u < area_regular_mix[light];',
                                'bool regular_ray = area_thin_objects && area_regular_mix[light] > 0.0;')
    ignore_text = ray_text.replace('bool regular_ray = area_thin_objects && ray_u < area_regular_mix[light];',
                                   'bool regular_ray = false;')
    assert pop_text != ray_text and ignore_text != ray_text
    popped = (rig.variant('boundary pop mutation', {rig.paths['gather']: pop_text}, 'class3/deferred/areaShadowF.glsl'), g['area_filter'])
    ignored = (rig.variant('boundary ignored mutation', {rig.paths['gather']: ignore_text}, 'class3/deferred/areaShadowF.glsl'), g['area_filter'])
    pop_levels = [boundary(thin_plan, m, programs=popped, rest=boundary_rest) for m in steps_of_share[:3]]
    pop_step = max(int(np.abs(pop_levels[k] - pop_levels[k - 1]).max()) for k in range(1, len(pop_levels)))
    require(pop_step > one_ray, 'mutation: switching every ray as soon as the share is non-zero must pop (%d/255 <= %d)' % (pop_step, one_ray))
    require(not np.array_equal(boundary(thin_plan, 1.0, programs=ignored, rest=boundary_rest), reference_regular),
            'mutation: ignoring the share must break the identity at share 1')
    print('[RelightQ] grain hybrid boundary PASS on %d production hybrid plans (Thin and regular march the SAME ray count): %s; the previous round plan 4x64 beside 8x12 differs by %d/255 after the filter (mutation rejected); the all-rays-at-once mutant pops %d/255 and the share-ignoring mutant breaks the identity' %
          (len(plan_reports), '; '.join('%s: rail separation %d/255, share 1 == regular light byte for byte, filtered endpoint %d/255 (limit 1), raw step <= %d, filtered step <= %d (one ray = %d)' % r for r in plan_reports), unequal, pop_step), flush=True)

    # (ii) Upsample fallback (fix round 3, GLM P1 / DeepSeek P2): where every bilinear tap of the full-resolution pixel is across
    # an edge (a thin wall, or a thin object against the sky) the fallback must be the pixel's OWN texel, never the tap of another
    # surface. A strip one full-resolution pixel wide sits between half-resolution taps on the far surface; each trace cell carries
    # a distinct value, so the tap that is picked is identifiable. The previous fallback (the tap with the lowest hand-over weight,
    # even when its geometric weight had been rejected) picks a tap across the edge and must differ.
    fallback_old_text = filter_text.replace('if (bilinear > 0.0 && geo_q > 0.05)\n            {', 'if (bilinear > 0.0)\n            {')
    assert fallback_old_text != filter_text
    fallback_old = rig.variant('upsample fallback mutation', {rig.paths['filter']: fallback_old_text}, 'class3/deferred/areaShadowFilterF.glsl')
    # Fix round 4 (DeepSeek P2): the pixel's own texel is a texelFetch, never a (filtered) textureLod at the pixel centre.
    own_line = 'fallback = texelFetch(area_source,clamp(ivec2(base)+ivec2(greaterThanEqual(f,vec2(0.5-1.e-3))),ivec2(0),size_i-ivec2(1)),0);'
    bilinear_text = filter_text.replace(own_line, 'fallback = textureLod(area_source,uv,0.0);')
    assert bilinear_text != filter_text
    fallback_bilinear = rig.variant('upsample bilinear fallback mutation', {rig.paths['filter']: bilinear_text}, 'class3/deferred/areaShadowFilterF.glsl')
    # Fix round 5 (DeepSeek P2): the own texel is taken in the SAME base alignment as the bilinear neighbourhood (the nearest of its four
    # taps), not floor(uv*res), which ignores the base shift for a target whose trace texels sit on uneven full-resolution pixels. On an
    # odd-sized target the two name different texels for half of the pixels; on an even one they agree.
    old_alignment_text = filter_text.replace(own_line, 'fallback = texelFetch(area_source,clamp(ivec2(floor(uv*res)),ivec2(0),size_i-ivec2(1)),0);')
    assert old_alignment_text != filter_text
    fallback_old_alignment = rig.variant('upsample old own-texel alignment mutation', {rig.paths['filter']: old_alignment_text}, 'class3/deferred/areaShadowFilterF.glsl')

    def own_index(full, trace, i):
        """The trace texel the upsample names for full-resolution pixel i along one axis: the nearest tap of the bilinear neighbourhood,
        in the neighbourhood's own (areaTexelUV-shifted) alignment; a tie goes to the higher texel. Mirrors the shader in float64."""
        import math
        def texel_uv(u):
            return (min(max(math.floor(u * full + 1.e-3), 0), full - 1) + .5) / full
        def clamped(u):
            return min(max(u, .5 / trace), 1 - .5 / trace)
        uv = (i + .5) / full
        base = math.floor(uv * trace - .5)
        if uv < texel_uv(clamped((base + .5) / trace)):
            base -= 1
        lo, hi = texel_uv(clamped((base + .5) / trace)), texel_uv(clamped((base + 1.5) / trace))
        f = min(max((uv - lo) / max(hi - lo, 1.e-8), 0.0), 1.0)
        return min(max(base + (1 if f >= .5 - 1.e-3 else 0), 0), trace - 1)

    def thin_wall(thin_w, thin_h, column):
        th_, tw_ = (thin_h + 1) // 2, (thin_w + 1) // 2
        projection_t = np.eye(4)
        projection_t[0, 0] = 1.0 / 3.0
        projection_t[1, 1] = 1.0 / (3.0 * thin_h / thin_w)
        projection_t[2, 2] = -.1
        cell_values = ((np.arange(tw_)[None, :] * 37 + np.arange(th_)[:, None] * 11) % 200 + 20).astype(np.float64)
        raw = np.stack([cell_values, (cell_values * 3 + 17) % 255, (cell_values * 5 + 29) % 255, (cell_values * 7 + 3) % 255], axis=-1) / 255.0

        def thin_scene(sky):
            depth = np.full((thin_h, thin_w, 4), .9, np.float32)
            depth[:, column] = .8                   # the thin wall, 2 m in front of the far wall, one full-res column wide
            if sky:
                depth[:] = 1.0
                depth[:, column] = .8               # a thin object against the sky
            normals = np.zeros_like(depth)
            normals[:, :, :2] = .5
            return dict(depth=depth, normals=normals, projection=projection_t, inverse=np.linalg.inv(projection_t))

        differs = {}
        away = np.ones(thin_w, bool)
        away[max(column - 4, 0):column + 5] = False
        rows = slice(8, thin_h - 8)
        own_col = own_index(thin_w, tw_, column)
        own_rows = np.array([own_index(thin_h, th_, y) for y in range(thin_h)])
        if thin_w % 2 == 0 and thin_h % 2 == 0:   # even targets: the nearest tap is the texel that contains the pixel
            assert own_col == (column * tw_) // thin_w and np.array_equal(own_rows, np.floor((np.arange(thin_h) + .5) * th_ / thin_h).astype(int))
        own = np.round(raw[own_rows[rows], own_col, 0] * 255).astype(int)
        for sky in (False, True):
            scene = thin_scene(sky)
            name = 'thin %s %dx%d' % ('object against the sky' if sky else 'wall', thin_w, thin_h)
            now = rig.filter_only(scene, raw, 0.0)
            before = rig.filter_only(scene, raw, 0.0, filt=fallback_old)
            strip = now[rows, column, 0].astype(int)
            require(np.array_equal(strip, own), '%s: the strip pixels must take their OWN trace texel (got %s, want %s)' % (name, strip[:6].tolist(), own[:6].tolist()))
            differs[('bilinear', sky)] = int(np.sum(rig.filter_only(scene, raw, 0.0, filt=fallback_bilinear)[rows, column, 0] != now[rows, column, 0]))
            differs[('alignment', sky)] = int(np.sum(rig.filter_only(scene, raw, 0.0, filt=fallback_old_alignment)[rows, column, 0] != now[rows, column, 0]))
            require(int(np.sum(before[rows, column, 0] != now[rows, column, 0])) > 0,
                    'mutation: the previous fallback (a tap across the edge) must differ on the %s' % name)
            require(np.array_equal(before[rows][:, away], now[rows][:, away]), 'the fallback change must not touch pixels away from the strip (%s)' % name)
            require(np.array_equal(rig.filter_only(scene, raw, 0.0, filt=fallback_old_alignment)[rows][:, away], now[rows][:, away]),
                    'the alignment change must not touch pixels away from the strip (%s)' % name)
        return differs

    even = thin_wall(256, 44, 100)
    require(even[('alignment', False)] == 0 and even[('alignment', True)] == 0,
            'on an even-sized target the old and the new own texel are the same (%s)' % even)
    odd = thin_wall(255, 45, 101)
    require(odd[('alignment', False)] > 0 and odd[('alignment', True)] > 0,
            'mutation: floor(uv*res) must name a different own texel than the neighbourhood alignment on an odd-sized target (%s)' % odd)
    print('[RelightQ] grain thin-wall fallback PASS: a one-pixel wall and a one-pixel object against the sky take their own trace texel when every tap is across the edge, on an even (256x44) and an odd (255x45) target, in the bilinear neighbourhood\'s own base alignment; the previous fallback (lowest hand-over weight among rejected taps) pastes a tap of the other surface; pixels away from the strip are unchanged; mutation floor(uv*res) differs on the odd target in %d (wall) / %d (sky) strip pixels and not on the even one; (informational: a filtered textureLod for the own texel would change %d (wall) / %d (sky) strip pixels on the even target, the production filter inputs are bound TFO_POINT so both are the same texel)' %
          (odd[('alignment', False)], odd[('alignment', True)], even[('bilinear', False)], even[('bilinear', True)]), flush=True)

    # ---- 7. determinism ---------------------------------------------------------------------------
    m = measure_for(1920)
    plan = dict(samples=8, steps=12, near=0, half=True)
    a = rig.chain('new', m.scene, LIGHT, plan, 2.0, 1080, seed=SEEDS[0])
    b = rig.chain('new', m.scene, LIGHT, plan, 2.0, 1080, seed=SEEDS[0])
    c = rig.chain('new', m.scene, LIGHT, plan, 2.0, 1080, seed=SEEDS[1])
    require(a.tobytes() == b.tobytes(), 'grain pipeline is not deterministic')
    require(a.tobytes() != c.tobytes(), 'different light UUIDs must give different noise')
    print('[RelightQ] grain determinism PASS: repeated chains byte-identical, other UUID differs; %.0f s' % (time.time() - t_start), flush=True)
    assert not failures, '\n'.join(failures)
    print('[RelightQ] grain check PASS (RELIGHT_WORLD_SHADOWS_PLAN Phase 0)', flush=True)


if __name__ == '__main__':
    import sys
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name('relight_shader_check.py')),
                                      '--area-only', '--grain-only', *sys.argv[1:]]))
