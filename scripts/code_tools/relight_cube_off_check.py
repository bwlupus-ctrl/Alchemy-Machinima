"""P1-lite OFF identity: with the world cube maps off, the soft-shadow output is BYTE-IDENTICAL to the base (P0) shaders.

    py -3 -I scripts/code_tools/relight_cube_off_check.py [--base <commit>] [--limit N] [--jobs N]

An image diff, because a runtime branch cannot prove it (quality brief section 7.1):
  1. every pose of the 3D harness (R, A, B, C = 181 poses) x every production quality config (Default, Default+Thin,
     Cinematic, Cinematic+Thin): the RGBA8 output of the base commit's areaShadowF/areaShadowFilterF programs equals the
     WORKTREE programs run with area_world = 0 (no world dict at all), byte for byte, in ONE GL context;
  2. mutation: the same WORKTREE programs with a non-zero area_world and a dummy atlas of blockers MUST differ;
  3. static: with the setting off, generateWorldShadowCubes has no GL call before its early return except the guarded lazy
     release, never changes sBytesAllocated otherwise; the world uniforms are uploaded only while a world channel exists or
     once on the way out; the new settings have no reload listener and no shader-manager dependency.
Exit 0 = PASS. Logs under <repo>/.tmp/cube_r1/ (work files).
"""
import argparse
import multiprocessing as mp
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import numpy as np  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SPAWN = mp.get_context('spawn')
BASE_DEFAULT = 'e150b927cf4'   # Relight Phase 0 as committed, the last commit WITHOUT P1-lite


def gbuffer_task(args):
    """Worker: exact G-buffer of one pose -> npz (depth32, norm16)."""
    import relight_3d_scene as S
    import relight_3d_tasks as T
    scene, pidx, work = args
    sc = T.get_scene(scene)
    pose = sc.poses[pidx]
    cam = S.Camera(pose.eye, pose.target, 1920, 1080, pose.vfov)
    g = S.cast_primary(sc, cam)
    _, depth32, norm16 = S.encode_gbuffer(cam, g)
    path = Path(work) / (pose.id + '.off.npz')
    np.savez(path, depth32=depth32, norm16=norm16)
    return scene, pidx, str(path)


def static_checks():
    pipeline = (REPO / 'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    start = pipeline.index('void LLPipeline::generateWorldShadowCubes(LLCamera& camera)')
    body = pipeline[start:start + 6000]
    off = body[body.index('if (!cubes() || !soft())'):]
    off = off[:off.index('return;')]
    calls = re.findall(r'\b(gl[A-Z]\w*|gGL\.\w+|\.allocate\(|sBytesAllocated)\b', off)
    assert not calls, 'GL call in the OFF branch: %s' % calls
    assert 'if (mWorldShadowAtlas.isComplete() || mWorldShadowScratch.isComplete()) releaseWorldShadowTargets();' in off, 'lazy release guard'
    first_alloc = body.index('.allocate(')
    assert first_alloc > body.index('mWorldShadowResident.count()'), 'allocation before the residency check'
    assert 'RenderAreaShadowCubeMaps' in body and 'RenderAreaShadows' in body
    # the world uniforms: uploaded only with a live reason (a world channel, or the one zero upload on the way out)
    assert 'const bool upload_world = world_any || mWorldShadowUniformsLive;' in pipeline
    for name in ('area_world"', 'area_world_light', 'area_world_info', 'area_world_seed', 'area_world_rot'):
        for m in re.finditer(r'uniform\w+\(LLStaticHashedString\("' + re.escape(name.rstrip('"')), pipeline):
            window = pipeline[max(0, m.start() - 900):m.start()]
            assert 'upload_world' in window or 'upsample && upload_world' in window, 'unguarded upload of ' + name
    control = (REPO / 'indra/newview/llviewercontrol.cpp').read_text(encoding='utf-8')
    manager = (REPO / 'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    for setting in ('RenderAreaShadowCubeMaps', 'RenderAreaShadowCubeLights'):
        assert setting not in manager, setting + ' shader-manager dependency'
        assert not re.search(r'setting_setup_signal_listener\([^;]*"' + setting + '"', control), setting + ' reload listener'
    print('[cube-off] static PASS: no GL call before the early return except the guarded lazy release, uniforms guarded, no reload listener', flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--base', default=BASE_DEFAULT)
    ap.add_argument('--limit', type=int, default=0, help='debug: first N poses per scene')
    ap.add_argument('--jobs', type=int, default=16)
    ap.add_argument('--scenes', default='R,A,B,C')
    args = ap.parse_args()
    static_checks()
    import relight_3d_gl as G
    import relight_3d_plan as PL
    import relight_3d_scene as S
    import relight_3d_tasks as T
    work = REPO / '.tmp' / 'cube_r1' / 'offwork'
    work.mkdir(parents=True, exist_ok=True)
    tasks = []
    for name in args.scenes.split(','):
        sc = T.get_scene(name)
        n = len(sc.poses) if not args.limit else min(args.limit, len(sc.poses))
        tasks += [(name, i, str(work)) for i in range(n)]
    print('[cube-off] base %s vs WORKTREE, %d poses x 4 configs' % (args.base, len(tasks)), flush=True)
    base_tree, work_tree = G.SourceTree(args.base), G.SourceTree('WORKTREE')
    out = REPO / '.tmp' / 'cube_r1'
    plans_base, _ = PL.compute_plans(base_tree, out)
    plans_work, _ = PL.compute_plans(work_tree, out)
    assert plans_base == plans_work, 'the planner differs between base and worktree'
    base_runner = G.AreaRunner(base_tree)
    work_runner = G.AreaRunner(work_tree, gl=base_runner.gl)
    pool = SPAWN.Pool(args.jobs)
    pending = [pool.apply_async(gbuffer_task, (t,)) for t in tasks]
    compared = 0
    mutated = False
    for p in pending:
        scene, pidx, path = p.get()
        sc = T.get_scene(scene)
        pose = sc.poses[pidx]
        cam = S.Camera(pose.eye, pose.target, 1920, 1080, pose.vfov)
        lights = sc.light_sets[pose.cfg]
        with np.load(path) as z:
            d32, n16 = z['depth32'], z['norm16']
        for cfg, plans in plans_work.items():
            plan = plans[len(lights)]
            a = base_runner.run(cam, d32, n16, lights, plan)
            b = work_runner.run(cam, d32, n16, lights, plan)
            # run() returns float32 = byte / 255 exactly: equal float arrays are equal RGBA8 bytes.
            if a.tobytes() != b.tobytes():
                print('[cube-off] FAIL %s %s: base and worktree differ (%d of %d values)' % (pose.id, cfg, int((a != b).sum()), a.size), flush=True)
                return 1
            compared += 1
        if not mutated and scene == 'B':
            # Mutation: the same worktree programs with a non-zero area_world and a dummy atlas of near blockers must differ.
            atlas = np.zeros((1088, 1632), np.float32)   # window depth 0 = a blocker at the near plane everywhere
            channels = [{'row': 0, 'near': 0.25, 'far': float(lights[0].range), 'r_map': 0.1, 'contact': 0.0, 'seed': 1}] + [None] * (len(lights) - 1)
            plan = plans_work['Default'][len(lights)]
            ref = work_runner.run(cam, d32, n16, lights, plan)
            alt = work_runner.run(cam, d32, n16, lights, plan, world={'atlas': atlas, 'channels': channels})
            assert ref.tobytes() != alt.tobytes(), 'MUTATION SURVIVED: a non-zero area_world with a blocker atlas did not change the output'
            ref2 = work_runner.run(cam, d32, n16, lights, plan, world={'atlas': atlas, 'channels': [None] * len(lights)})
            assert ref.tobytes() == ref2.tobytes(), 'an atlas bound with every channel off must not change the output'
            mutated = True
            print('[cube-off] mutation PASS: non-zero area_world + blocker atlas differs; atlas bound with channels off is identical', flush=True)
        Path(path).unlink(missing_ok=True)
    pool.close()
    print('[cube-off] PASS: %d (pose, config) outputs byte-identical between %s and WORKTREE with area_world = 0' % (compared, args.base), flush=True)
    return 0


if __name__ == '__main__':
    mp.freeze_support()
    sys.exit(main())
