"""GPU cost gate for the screen-space Relight soft shadows (production GLSL, GL timestamp queries).

The plan (RELIGHT_WORLD_SHADOWS_PLAN.md, Phase 0 "Cost") set a target of at most 1.15 x the pre-P0 commit for the
production gather + filter. User decision 10/22 ("i can accept a cost penalty, so long as it works"): the ratio is REPORTED
against that 1.15 target and the gate FAILS only above a hard ceiling of 2.5 x HEAD. Both versions run their OWN effective production plan (the planner of the
version, the blur radius of the version) on the same synthetic G-buffer, exactly as renderAreaShadows draws
a frame: per plane of four lights the gather draws (split by the channels per draw), then the horizontal
blur, the vertical blur and the full-resolution bilateral upsample. The GPU time of a batch of whole frames
is read with GL_TIME_ELAPSED; the gate takes the fastest of many alternating batches per version (the least
noisy estimate of the true cost on a shared GPU).

Configurations: Medium 1080p K4 and Cinematic 4K K8 (Thin objects OFF, default 1500 M budget, VCam feeds OFF), and (fix round 3,
Opus) the Thin objects plans of the same budget: a pure Thin plan (Medium 1080p K4) and the hybrids that put the top lights on
the Thin march and keep the regular march for the others (Low 4K K4: 3 Thin + 1 regular; Low 4K K8: 2 Thin + 6 regular), each
against the pre-P0 plan of the same Thin request, drawn exactly as the pipeline draws them (the Thin group first with its own
uniforms, then the regular group, one plane at a time).
A sensitivity control inflates the P0 plan by half (rays x 1.5, i.e. (3 s + 1) // 2 with the actual counts printed; for a hybrid the
Thin group's rays): it must read at least 1.05 x the plan's own ratio (otherwise the gate measures nothing). The viewer (AlchemyTest.exe) may be running on the same GPU: another GPU client
can only inflate a time, so the minima are what is reported. Run directly it re-launches the GPU harness
(--area-only --cost-only).
"""
from pathlib import Path
import subprocess

TARGET = 1.15      # the plan's original target: reported, no longer a failure
CEILING = 2.5      # hard ceiling (user decision 10/22): above this the gate fails
BATCH_FRAMES = 6   # whole frames per timed batch
BATCHES = 14       # alternating batches per version; the minimum is used
ATTEMPTS = 3       # rounds of BATCHES while the best ratio is still above the target (a contended GPU)
CONFIGS = (('Medium 1080p K4', 1080, 4, 1, 0), ('Cinematic 4K K8', 2160, 8, 3, 0),
           ('Thin Medium 1080p K4', 1080, 4, 1, 1), ('Thin Low 4K K4 hybrid', 2160, 4, 0, 1), ('Thin Low 4K K8 hybrid', 2160, 8, 0, 1))


class CostRig:
    """Draws whole frames of one configuration of one shader version and reads their GPU time."""

    def __init__(self, g):
        import relight_scene_fixture as S
        from relight_orbit_check import build_world
        self.g, self.S = g, S
        self.np, self.C, self.U = g['np'], g['C'], g['U']
        self.loc, self.ui, self.uf = g['loc'], g['ui'], g['uf']
        C, U = self.C, self.U
        self.gen_queries = g['gl']('glGenQueries', None, g['I'], C.POINTER(U))
        self.begin_query = g['gl']('glBeginQuery', None, U, U)
        self.end_query = g['gl']('glEndQuery', None, U)
        self.query_result = g['gl']('glGetQueryObjectui64v', None, U, U, C.POINTER(C.c_uint64))
        self.finish = g['gl']('glFinish', None)
        # One TIME_ELAPSED query per draw: the sum is the GPU execution time of the draws themselves. A single query
        # around a whole frame would also count the idle gaps between the small passes (the driver's state changes and
        # the CPU issuing the next pass), which belong to the harness and not to the shaders.
        self.queries = (U * 512)()
        self.gen_queries(512, self.queries)
        self.used = 0
        # The 1080p world (the orbit-sweep world from a representative pose) is cast once; the 4K G-buffer is its 2x
        # nearest-neighbour upscale (same geometry, four times the pixels).
        np = self.np
        surfaces, _ = build_world(S)
        target, distance, yaw, pitch = np.array([0.0, 0.5, -4.6]), 6.5, 30.0, -20.0
        yr, pr = np.radians(yaw), np.radians(pitch)
        forward = np.array([np.sin(yr) * np.cos(pr), np.sin(pr), -np.cos(yr) * np.cos(pr)])
        self.cam = S.camera(np, target - forward * distance, yaw, pitch, 0.0, 1920, 1080)
        self.scene_1080 = S.cast(np, self.cam, surfaces)
        self.light_world = np.array([0.4, 2.6, -1.2])

    def scene(self, factor):
        np = self.np
        if factor == 1:
            return self.scene_1080
        out = dict(self.scene_1080)
        out['depth'] = np.repeat(np.repeat(self.scene_1080['depth'], factor, axis=0), factor, axis=1)
        out['normals'] = np.repeat(np.repeat(self.scene_1080['normals'], factor, axis=0), factor, axis=1)
        return out

    def bind_scene(self, rig, scene):
        rig.uploaded = None
        rig.upload_scene(scene)   # one G-buffer on units 1/2 for every version of a configuration

    def frame(self, rig, programs, plan, radius, scene, lights):
        return Frame(self, rig, programs, plan, radius, scene, lights)


class Frame:
    """One configuration of one version, ready to draw whole frames with no CPU work in the way."""

    def __init__(self, cost, rig, programs, plan, radius, scene, lights):
        g, np = cost.g, cost.np
        loc, ui, uf = cost.loc, cost.ui, cost.uf
        self.cost, self.programs, self.plan, self.radius = cost, programs, plan, radius
        h, w = scene['depth'].shape[:2]
        self.w, self.h = w, h
        self.tw, self.th = ((w + 1) // 2, (h + 1) // 2) if plan['half'] else (w, h)
        self.gather_tex = g['area_texture'](5, np.zeros((self.th, self.tw, 4), np.float32), 0x8058)
        self.scratch_tex = g['area_texture'](6, np.zeros((self.th, self.tw, 4), np.float32), 0x8058)
        self.vis_tex = g['area_texture'](7, np.zeros((h, w, 4), np.float32), 0x8058)
        self.rig, self.scene = rig, scene
        self.apply()
        self.planes = []
        for plane in range((lights + 3) // 4):
            count = min(4, lights - plane * 4)
            view_lights = np.zeros((4, 4), np.float32)
            for i in range(count):
                world = cost.light_world + np.array([0.12 * (plane * 4 + i), 0.0, 0.05 * (plane * 4 + i)])
                view_lights[i, :3] = (cost.cam['view'] @ np.append(world, 1))[:3]
                view_lights[i, 3] = .3
            ids = np.array([12345 + plane * 100 + i * 7 for i in range(4)], np.uint32)
            self.planes.append((count, view_lights, ids))

    def apply(self):
        """Uniforms are program state: a program shared by several frames (the P0 gather) gets this frame's own again."""
        cost, plan, scene, rig = self.cost, self.plan, self.scene, self.rig
        g, loc, ui, uf = cost.g, cost.loc, cost.ui, cost.uf
        h = self.h
        gather, filt = self.programs
        g['area_use'](gather)
        rig.matrices(gather, scene)
        for name, value in ((b'depthMap', 1), (b'normalMap', 2), (b'area_samples', plan['samples']),
                            (b'area_steps', plan['steps']), (b'area_near_steps', plan['near']),
                            (b'area_thin_objects', int(plan['near'] > 0))):
            ui(loc(gather, name), value)
        uf[2](loc(gather, b'area_trace_res'), self.tw, self.th)
        uf[3](loc(gather, b'area_params'), 8.0, .35, h / 1080.0 * (.5 if plan['half'] else 1.0))
        uf[1](loc(gather, b'area_light_bias'), .25)
        uf[4](loc(gather, b'area_contact_dist'), 0, 0, 0, 0)
        uf[4](loc(gather, b'area_contact_fade'), 0, 0, 0, 0)
        # A mapped-light hand-over window (the native map ends at 27..64 m), so the filter's hand-over stop runs its real path.
        uf[2](loc(gather, b'area_native_clip'), 27.0, 64.0)
        uf[1](loc(gather, b'area_native_offset'), .04)
        g['area_use'](filt)
        rig.matrices(filt, scene)
        for name, value in ((b'depthMap', 1), (b'normalMap', 2), (b'area_source', 3), (b'area_debug_channel', 0)):
            ui(loc(filt, name), value)
        uf[1](loc(filt, b'area_radius'), self.radius)
        uf[2](loc(filt, b'area_native_clip'), 27.0, 64.0)
        uf[1](loc(filt, b'area_native_offset'), .04)
        uf[4](loc(filt, b'area_contact_fade'), 1, 1, 1, 1)   # every light mapped: the hand-over stop runs its real path

    def pass_(self, program, source, target, width, height, axis=None):
        g, loc, uf = self.cost.g, self.cost.loc, self.cost.uf
        cost = self.cost
        g['use'](program)
        g['active'](0x84C0 + 3)
        g['bindTex'](0x0DE1, source)
        g['attachment'](0x8D40, 0x8CE0, 0x0DE1, target, 0)
        g['viewport'](0, 0, width, height)
        if axis is not None:
            uf[2](loc(program, b'area_axis'), *axis)
        query = cost.queries[cost.used]
        cost.used += 1
        cost.begin_query(0x88BF, query)
        g['draw'](4, 0, 3)
        cost.end_query(0x88BF)

    def march(self, plan):
        """The march uniforms of one light group (the pipeline sets them per group: area_samples, steps, thin, near)."""
        g, loc, ui = self.cost.g, self.cost.loc, self.cost.ui
        gather = self.programs[0]
        g['use'](gather)
        for name, value in ((b'area_samples', plan['samples']), (b'area_steps', plan['steps']),
                            (b'area_near_steps', plan['near']), (b'area_thin_objects', int(plan['near'] > 0))):
            ui(loc(gather, name), value)

    def frame(self, parts=('gather', 'filter')):
        g, loc, ui, C, U = self.cost.g, self.cost.loc, self.cost.ui, self.cost.C, self.cost.U
        gather, filt = self.programs
        # A hybrid plan: the first `lead` lights march self.plan, the others self.plan['rest'] (the regular march).
        rest = self.plan.get('rest')
        lead = self.plan.get('lead', 8)
        for plane, (count, view_lights, ids) in enumerate(self.planes):
            g['use'](gather)
            g['uniform4fv'](loc(gather, b'area_lights'), 4, view_lights.ctypes.data_as(C.POINTER(g['F'])))
            g['uniform4uiv'](loc(gather, b'area_seeds'), 1, ids.ctypes.data_as(C.POINTER(U)))
            for group_plan, begin, end in ((self.plan, 0, lead), (rest, lead, 8)):
                if group_plan is None:
                    continue
                lo, hi = min(max(begin - plane * 4, 0), count), min(max(end - plane * 4, 0), count)
                if lo >= hi:
                    continue
                self.march(group_plan)
                for first in range(lo, hi, group_plan['channels']):
                    last = min(hi, first + group_plan['channels'])
                    g['use'](gather)
                    ui(loc(gather, b'area_first'), first)
                    ui(loc(gather, b'area_count'), last)
                    if 'gather' in parts:
                        self.pass_(gather, self.scratch_tex, self.gather_tex, self.tw, self.th)
            if 'filter' not in parts:
                continue
            if self.radius > 0:
                self.pass_(filt, self.gather_tex, self.scratch_tex, self.tw, self.th, (1.0, 0.0))
                self.pass_(filt, self.scratch_tex, self.gather_tex, self.tw, self.th, (0.0, 1.0))
            self.pass_(filt, self.gather_tex, self.vis_tex, self.w, self.h, (0.0, 0.0))

    def timed(self, parts=('gather', 'filter'), frames=BATCH_FRAMES):
        """GPU milliseconds per frame over one batch."""
        cost, C = self.cost, self.cost.C
        self.apply()
        cost.finish()
        cost.used = 0
        for _ in range(frames):
            self.frame(parts)
        cost.finish()
        total = 0
        for i in range(cost.used):
            result = C.c_uint64(0)
            cost.query_result(cost.queries[i], 0x8866, C.byref(result))
            total += result.value
        return total * 1.e-6 / frames


def planner_row(rows, height, cap, preset, new, thin=0):
    """The production plan (feeds OFF). A pre-P0 row has one march; a P0 row of a hybrid also carries the regular march of
    the lights after the first `lead` (the Thin lights)."""
    for r in rows:
        if (r[0], r[1], r[2], r[3], r[4]) == (thin, height, cap, preset, 0):
            plan = dict(samples=r[5], steps=r[6], near=r[7], half=bool(r[8]), channels=r[10] if new else r[9])
            if new and r[12] < r[9]:
                plan.update(lead=r[12], admitted=r[9], rest=dict(samples=r[13], steps=r[14], near=0, half=bool(r[8]), channels=r[15]))
            return plan
    raise AssertionError(('planner row', height, cap, preset))


def lights_of(plan, cap):
    """How many lights the plan draws (a pre-P0 plan draws all K)."""
    return plan.get('admitted', cap) if plan.get('admitted') else cap


def check_cost(g):
    import time
    from relight_grain_check import Rig, old_planner_rows, radius_for
    rig = Rig(g)
    t0 = time.time()
    cost = CostRig(g)
    old_rows, new_rows = old_planner_rows(), g['policy']['grid']
    summary = []
    for label, height, cap, preset, thin in CONFIGS:
        scene = cost.scene(height // 1080)
        new_plan, old_plan = planner_row(new_rows, height, cap, preset, True, thin), planner_row(old_rows, height, cap, preset, False, thin)
        # The +50 % control: (3 s + 1) // 2 rays (s = 8 -> 12, never the 13 of int(1.5 s) + 1), of the Thin group of a hybrid.
        inflated = dict(new_plan, samples=(3 * new_plan['samples'] + 1) // 2)
        new_radius = radius_for(height / 1080, new_plan['half'], True)
        variants = {
            'head': (rig.programs['old'], old_plan, radius_for(height / 1080, old_plan['half'], False)),
            'p0': ((g['area_gather'], g['area_filter']), new_plan, new_radius),
            'p0 +50% rays': ((g['area_gather'], g['area_filter']), inflated, new_radius),
        }
        cost.bind_scene(rig, scene)
        frames = {name: cost.frame(rig, programs, plan, radius, scene, lights_of(plan, cap)) for name, (programs, plan, radius) in variants.items()}
        best = {name: float('inf') for name in frames}
        # Warm-up, then alternate whole batches so that clock or thermal drift hits every version alike.
        for name in frames:
            frames[name].timed()
        for _ in range(ATTEMPTS):
            for _ in range(BATCHES):
                for name in frames:
                    best[name] = min(best[name], frames[name].timed())
            # Another GPU client (the viewer) can only INFLATE a time, so more batches only tighten the minima; the
            # ratio is the best of them, never an average.
            if best['p0'] / best['head'] <= TARGET:
                break
        ratio, control = best['p0'] / best['head'], best['p0 +50% rays'] / best['head']
        # Where the time goes (diagnostic, not gated): the gather and the filter chain alone, same alternation.
        split = {}
        for part in ('gather', 'filter'):
            for name in ('head', 'p0'):
                split[(name, part)] = min(frames[name].timed((part,)) for _ in range(BATCHES))
        print('[RelightQ] cost split %-22s gather HEAD %.3f / P0 %.3f ms (%.3fx) | filter HEAD %.3f / P0 %.3f ms (%.3fx)' %
              (label, split[('head', 'gather')], split[('p0', 'gather')], split[('p0', 'gather')] / split[('head', 'gather')],
               split[('head', 'filter')], split[('p0', 'filter')], split[('p0', 'filter')] / split[('head', 'filter')]), flush=True)
        summary.append((label, best['head'], best['p0'], ratio, control))
        hybrid_note = '' if 'rest' not in new_plan else ' + %d regular %d x %d' % (
            new_plan['admitted'] - new_plan['lead'], new_plan['rest']['samples'], new_plan['rest']['steps'])
        print('[RelightQ] cost %-22s HEAD %2d rays x %2d steps %s r=%.1f: %.3f ms | P0 %s%2d rays x %2d steps %s r=%.1f: %.3f ms | ratio %.3f (target %.2f, ceiling %.1f); control +50%% rays %d -> %d: %.3f' %
              (label, old_plan['samples'], old_plan['steps'], 'half' if old_plan['half'] else 'full', variants['head'][2], best['head'],
               ('%d Thin lights, ' % new_plan['lead']) if 'rest' in new_plan else '',
               new_plan['samples'], new_plan['steps'], 'half' if new_plan['half'] else 'full', new_radius, best['p0'], ratio, TARGET, CEILING,
               new_plan['samples'], inflated['samples'], control) + hybrid_note,
              flush=True)
    failures = [s for s in summary if s[3] > CEILING]
    assert not failures, ('P0 gather + filter cost exceeds the %.1f x HEAD ceiling' % CEILING, [(s[0], round(s[3], 3)) for s in failures])
    # The control proves the gate can see a cost increase: a plan 50 % heavier in rays must read slower than the P0 plan.
    for label, head, p0, ratio, control in summary:
        assert control > ratio * 1.05, ('cost gate is insensitive: +50%% rays reads %.3f vs %.3f' % (control, ratio), label)
    over = [s[0] for s in summary if s[3] > TARGET]
    print('[RelightQ] cost PASS: GPU timestamp queries, production gather + filter at each version\'s effective plan: ' +
          '; '.join('%s %.3fx HEAD (%.3f vs %.3f ms)' % (s[0], s[3], s[2], s[1]) for s in summary) +
          ' (target %.2fx %s; hard ceiling %.1fx; +50%% rays control reads >= %.2fx its own ratio ... %s); %.0f s' %
          (TARGET, ('MET' if not over else 'EXCEEDED at ' + ', '.join(over) + ' and accepted by user decision 10/22'), CEILING,
           min(s[4] / s[3] for s in summary), ', '.join('%s %.2fx' % (s[0], s[4]) for s in summary), time.time() - t0), flush=True)
    return summary


if __name__ == '__main__':
    import sys
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name('relight_shader_check.py')),
                                      '--area-only', '--cost-only', *sys.argv[1:]]))
