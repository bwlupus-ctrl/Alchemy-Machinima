"""Permanent stackable clone FX safety fixtures using production WGL shaders.

Called by clone_looks_safety_check, hence also by clone_looks_gl_check. Real SL
LLM meshes, skeleton palettes, authored invisible faces and real draw defaults;
no viewer build, new viewer render targets or render passes.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import relight_3d_gl as G
from clone_looks_gl_check import Runner, F, V, SH, ROOT

BASE = '66e124787e6'
EFFECTS = ('Slice', 'Dropout', 'Power flicker', 'Wipe', 'Interlace', 'Position jitter')
LOOKS = (1, 65, 69, 85)  # Clone, Mercury, Magma, Aureole
BOUNDS = (.12, 0., 0., 0., .018, .035)  # metres, lateral only
TIMES = tuple(.17 + .41 * i for i in range(25))
# Changes below these display-space amounts are not perceptible evidence.
MIN_RGB_DELTA = .0015
MIN_CHANGED_FRACTION = .008
FX_SHADER_SHA256 = '37376983ab6949160718894018f22b14741f7d204c6bca8705461869b04f0034'


def pre_fx_shader(source, path):
    """Reverse only pinned FX hunks, requiring the entire historical source."""
    manifest=ROOT/'scripts/code_tools/clone_fx_shader_hunks.json'
    raw=manifest.read_bytes().replace(b'\r\n',b'\n')
    assert hashlib.sha256(raw).hexdigest()==FX_SHADER_SHA256, 'clone FX shader allowance pin'
    data=json.loads(raw)
    assert data['base']==BASE
    original=G.SourceTree(BASE).read(path)
    if source==original:return source
    for hunk in data['files'][path]:
        assert original.count(hunk['before'])==1, ('FX baseline hunk',path)
        assert source.count(hunk['after'])==1, ('exact FX hunk',path)
        source=source.replace(hunk['after'],hunk['before'],1)
    assert source==original, ('FX reversal must reproduce entire baseline',path)
    return source


def knobs(index, amount):
    values = [0.] * 6
    values[index] = amount
    return dict(fx2=values[:4], fx3=values[4:])


def perceptible(frames, baseline, mask, animated=True):
    comparisons = list(zip(frames, frames[1:])) if animated else list(zip(frames, baseline))
    mean = max(float(np.mean(np.abs(a[mask, :3] - b[mask, :3]))) for a, b in comparisons)
    fraction = max(float(np.mean(np.max(np.abs(a[mask] - b[mask]), axis=-1) > .04))
                   for a, b in comparisons)
    return mean >= MIN_RGB_DELTA and fraction >= MIN_CHANGED_FRACTION, dict(rgb_delta=mean, changed_fraction=fraction)


class DisableEffect:
    """Mutate the actual production uniform reads in memory, in both stages."""
    def __init__(self, tree, index):
        self.tree, self.index = tree, index

    def read(self, path):
        source = self.tree.read(path)
        if path.endswith(('actorghostV.glsl', 'avatarActorGhostV.glsl', 'eyeballActorGhostV.glsl', 'actorghostF.glsl')):
            channel = ('ghostFx2.' + 'xyzw'[self.index]) if self.index < 4 else ('ghostFx3.' + 'xy'[self.index-4])
            assert channel in source or (path != F and self.index in (1, 2, 3)) or (path == F and self.index >= 4), (path, channel)
            source = source.replace(channel, '0.0')
        return source


class OversizedDisplacement:
    def __init__(self, tree):
        self.tree=tree

    def read(self, path):
        source=self.tree.read(path)
        if path.endswith(('actorghostV.glsl','avatarActorGhostV.glsl','eyeballActorGhostV.glsl')):
            original='pos.xyz += ghostFxLateral * offset;'
            assert source.count(original)==1
            source=source.replace(original,'pos.xyz += ghostFxLateral * (offset * 20.0);')
        return source


def geometry_silhouette(r, pr):
    """Coverage of the actual material geometry, without texture sample holes.

    A subpixel opaque strand can be missed by every original pixel centre and
    become sampled after a 2-pixel shift. Dilation of visible texels then reports
    arbitrary displacement along that strand. Bound GEOMETRY against geometry;
    retain separate strict alpha-zero tests for the untouched authored textures.
    Fully invisible/hidden faces are excluded, never made into allowed shells.
    """
    texture=r.texture(np.ones((2,2,4),np.float32),2,2,0x8814)
    before=[(b,b['texture'],b.get('alpha_mode',False),b.get('cutoff',0.)) for b in r.batches]
    only=r.only
    try:
        r.only={b['name'] for b in r.batches if not b['name'].startswith('hidden_')
                and b['name'] not in ('extra_invisible','extra_degenerate','extra_blend')}
        for b,_,_,_ in before:
            b.update(texture=texture,alpha_mode=False,cutoff=0.)
        return r.frame(pr,1)[...,3]>0
    finally:
        r.only=only
        for b,tex,alpha,cutoff in before:b.update(texture=tex,alpha_mode=alpha,cutoff=cutoff)
        value=r.gl.U(texture)
        r.gl.fn('glDeleteTextures',None,r.gl.I,C.POINTER(r.gl.U))(1,C.byref(value))


def silhouette_envelope(mask, pixels):
    if not pixels:
        return mask
    return np.asarray(Image.fromarray(mask).filter(ImageFilter.MaxFilter(2 * pixels + 1))) > 0


def permutation_checks(tree, out, gl=None):
    r = Runner(160, 192, gl)
    base = G.SourceTree(BASE)
    world = ('GHOST_WORLD_PASS', 'GHOST_SHARED_DISSOLVE')
    variants = [('studio_static', (), V), ('studio_skinned', ('HAS_SKIN',), V),
                ('world_static', world, V), ('world_skinned', world+('HAS_SKIN',), V),
                ('indexed', world+('HAS_SKIN', 'GHOST_INDEXED_WORLD'), V),
                ('system', world+('GHOST_SYSTEM_AVATAR',), SH+'avatar/avatarActorGhostV.glsl'),
                ('cloth', world+('GHOST_SYSTEM_AVATAR', 'AVATAR_CLOTH'), SH+'avatar/avatarActorGhostV.glsl'),
                ('eyeball', world+('GHOST_SYSTEM_AVATAR',), SH+'avatar/eyeballActorGhostV.glsl'),
                ('key_fill_static', ('AL_KEY_FILL',), V),
                ('key_fill_skinned', ('HAS_SKIN', 'AL_KEY_FILL'), V)]
    result = {}
    for name, defs, vertex in variants:
        pr, old = r.program(tree, defs, vertex), r.program(base, defs, vertex)
        # Every selectable Studio/shared prefix ID, including reserved legacy IDs.
        for look in range(101):
            a, b = r.render(pr, look), r.render(old, look)
            assert np.array_equal(a, b), (name, look, 'FX zero must be bit-identical', float(np.max(np.abs(a-b))))
        active = {}
        for index, effect in enumerate(EFFECTS):
            frames = [r.render(pr, 1, time=t, **knobs(index, 1.)) for t in TIMES[:8]]
            assert all(np.isfinite(a).all() for a in frames), (name, effect, 'nonfinite')
            active[effect] = float(max(np.max(a) for a in frames))
        delete=r.gl.fn('glDeleteProgram',None,r.gl.U)
        delete(pr); delete(old)
        result[name] = dict(zero_exact_look_ids=list(range(101)), active_finite=active)
    # Every independently linked vertex block must be the same implementation.
    blocks=[]
    for _, _, vertex in variants:
        text=tree.read(vertex)
        block=text.split('// BEGIN STACKABLE CLONE FX VERTEX')[1].split('// END STACKABLE CLONE FX VERTEX')[0]
        blocks.append(hashlib.sha256(block.encode()).hexdigest())
    assert len(set(blocks)) == 1, 'divergent FX vertex implementations'
    result['vertex_block_sha256'] = blocks[0]
    print('PASS clone FX: zero exact for all 101 IDs in 10 permutations; active shader compilation/finite output', flush=True)
    return result


def clock_wrap_checks(tree, gl=None):
    """Actual blackouts across a legacy look wrap AND the independent FX wrap."""
    r=Runner(64,64,gl)
    pr=r.program(tree,('HAS_SKIN',))
    def intervals(program,period,seed,amount):
        times=np.arange(period-1.2,period+1.2,1/60)
        alpha=np.array([r.render(program,1,time=float(t%period),fx_time=float(t),
                                phase=seed,fx2=(0,0,amount,0))[...,3].sum() for t in times])
        black=alpha==0
        # The first sample might already be inside a blackout, not its onset.
        starts=times[black & ~np.r_[black[0],black[:-1]]]
        return dict(period=period,seed=seed,amount=amount,starts=(starts-period).tolist(),
                    min_interval=float(np.diff(starts).min()) if len(starts)>1 else None)
    rows=[]
    for period in (200*np.pi,6144.):
        for seed in (.1,.73,2.47,5.7):
            for amount in (.5,1.):
                row=intervals(pr,period,seed,amount)
                assert row['min_interval'] is None or row['min_interval']>=.5-1/60, ('flicker wrap rate',row)
                rows.append(row)
    class OldClock:
        def read(self,path):
            text=tree.read(path)
            if path==F:
                before='float t = ghostFx3.w + ghostAux.w * 0.73;'
                assert text.count(before)==1
                text=text.replace(before,'float t = ghostTime + ghostAux.w * 0.73;')
            return text
    bad=r.program(OldClock(),('HAS_SKIN',))
    control=intervals(bad,200*np.pi,.1,1.)
    assert control['min_interval'] is not None and control['min_interval']<.5-1/60, 'look-clock rate mutation escaped'
    delete=r.gl.fn('glDeleteProgram',None,r.gl.U)
    delete(pr);delete(bad)
    print('PASS clone FX clock: .5/1 flicker rate at legacy/FX wraps; old look-clock mutation rejected',flush=True)
    return dict(matrix=rows,old_look_clock_rejected=control)


def check(out=None, runner=None, tree=None, program=None):
    from clone_looks_safety_check import RealAvatar, SEEDS, save, hazards
    out = out or ROOT/'.tmp/looks16/renders/clone_fx'
    out.mkdir(parents=True, exist_ok=True)
    tree = tree or G.SourceTree('WORKTREE')
    result = dict(base=BASE, looks=LOOKS, amounts=(.5, 1.), times=TIMES,
                  limits=dict(displacement_m=dict(zip(EFFECTS, BOUNDS)), rgb_delta=MIN_RGB_DELTA,
                              changed_fraction=MIN_CHANGED_FRACTION, max_blackouts_per_second=2),
                  permutations=permutation_checks(tree, out, runner.gl if runner else None),
                  clock_wrap=clock_wrap_checks(tree, runner.gl if runner else None),
                  effects={}, negative_controls={})
    from clone_fx_fix1_check import check as fix1_assertions
    result['fix_round_1'] = fix1_assertions(ROOT/'.tmp/looks16/renders/clone_fx_fix1', runner.gl if runner else None, tree)
    r = runner or RealAvatar(640, 640)
    pr = program or r.program(tree, ('HAS_SKIN',))
    # Always exercise this suite with the real-avatar front view and a metre
    # scale. A passed-in safety runner may have been left in another pose.
    r.only=None; r.pose='bind'; r.camera=0.; r.camera_center_z=sum(r.pose_bounds['bind'])*.5
    r.show_hidden=False; r.include_blend=False
    geometry_mask=geometry_silhouette(r,pr)
    assert np.count_nonzero(geometry_mask)>100, 'empty geometry silhouette oracle'
    strips=[]
    for index, effect in enumerate(EFFECTS):
        rows=[]
        for amount in (.5, 1.):
            options=knobs(index, amount)
            for look in LOOKS:
                base=[r.frame(pr, look, time=t) for t in TIMES]
                neutral=r.frame(pr, 1)
                mask=neutral[...,3] > 0
                assert mask.sum() > 100, 'empty real avatar oracle'
                frames=[r.frame(pr, look, time=t, **options) for t in TIMES]
                # Physical bound projected through the fixture's orthographic
                # camera; 1 px raster rounding allowed ONLY when displaced.
                pixels=int(np.ceil(BOUNDS[index]*amount*r.projection_x*r.width*.5))
                allowed=silhouette_envelope(geometry_mask if pixels else mask, pixels+1 if pixels else 0)
                for t, a in zip(TIMES, frames):
                    bad, metrics=hazards(a, mask, allowed)
                    assert not bad, (effect, amount, look, t, bad, metrics)
                # Look animation cannot masquerade as effect motion: compare
                # the time-dependent treatment residual, not just final images.
                residual=[a-b for a,b in zip(frames,base)]
                visible, perception=perceptible(residual, [np.zeros_like(a) for a in base], mask, index!=3)
                if index!=3 or amount==.5:
                    assert visible, (effect, amount, look, 'imperceptible', perception)
                # The partial plane skips rigid prims; full hide covers EVERY part.
                if index==3 and amount==1.:
                    assert all(not np.any(a) for a in frames), 'Wipe 1 clone/attachment remains visible'
                seed=max(float(np.mean(np.abs(
                    (r.frame(pr,look,time=t,phase=SEEDS[1],**options)-r.frame(pr,look,time=t,phase=SEEDS[1]))
                    - residual[ti]))) for ti,t in enumerate(TIMES))
                if index!=3:
                    assert seed > .0001, (effect, amount, look, 'identical crowd seeds', seed)
                a=r.frame(pr,look,time=.501,effect_fps=12,**options)
                b=r.frame(pr,look,time=.54,effect_fps=12,**options)
                assert np.array_equal(a,b), (effect,look,'Effect fps hold')
                # Poisoned alpha-zero body/face geometry must still publish zero.
                r.show_hidden=True
                for only in ({'extra_invisible'},{'hidden_head','hidden_upper_body','hidden_lower_body'}):
                    r.only=only
                    for t in TIMES[:8]:
                        assert not np.any(r.frame(pr,look,time=t,**options)), (effect,look,'alpha-zero face visible')
                r.only=None; r.show_hidden=False
                rows.append(dict(amount=amount,look=look,perception=perception,seed_delta=seed,
                                 silhouette_padding_px=pixels,finite=True,alpha_zero=True,fps_hold=True))
                # Strip: three actual samples, selected for strongest temporal
                # difference from frame zero (Wipe is intentionally not animated).
                choices=[0, max(range(len(frames)),key=lambda i:float(np.mean(abs(frames[i]-frames[0])))),len(frames)-1]
                strip=Image.new('RGB',(3*240,290),'#101010');draw=ImageDraw.Draw(strip)
                for column, ti in enumerate(choices):
                    path=out/f'{index}_{amount}_{look}_{ti}.png';save(frames[ti],path)
                    with Image.open(path) as im:
                        im.thumbnail((240,260));strip.paste(im,(column*240+(240-im.width)//2,30))
                    draw.text((column*240+5,5),f'{effect} {amount:g} look {look} t={TIMES[ti]:.2f}',fill='white')
                strip.save(out/f'{index}_{amount}_{look}_strip.png');strips.append(strip)
        result['effects'][effect]=rows
        # A static treatment cannot pass by borrowing the look's own animation.
        mutant=r.program(DisableEffect(tree,index),('HAS_SKIN',))
        options=knobs(index,.5)
        frames=[r.frame(mutant,1,time=t,**options) for t in TIMES]
        baseline=[r.frame(pr,1,time=t) for t in TIMES]
        mask=r.frame(pr,1)[...,3]>0
        visible, metrics=perceptible([a-b for a,b in zip(frames,baseline)],
                                    [np.zeros_like(a) for a in baseline],mask,index!=3)
        assert not visible, (effect,'disabled negative control accepted',metrics)
        result['negative_controls'][effect]=dict(disabled_production_reads_rejected=True,metrics=metrics)
        r.gl.fn('glDeleteProgram',None,r.gl.U)(mutant)
        if BOUNDS[index]:
            mutant=r.program(OversizedDisplacement(tree),('HAS_SKIN',))
            pixels=int(np.ceil(BOUNDS[index]*.5*r.projection_x*r.width*.5))
            allowed=silhouette_envelope(geometry_mask,pixels+1)
            rejected=any(np.any(r.frame(mutant,1,time=t,**knobs(index,.5))[~allowed]) for t in TIMES)
            assert rejected, (effect,'oversized-displacement negative control escaped')
            result['negative_controls'][effect]['oversized_displacement_rejected']=True
            r.gl.fn('glDeleteProgram',None,r.gl.U)(mutant)
        print('PASS clone FX real SL:',effect,'amounts .5/1; Clone + Mercury/Magma/Aureole; motion, seeds, bounds, alpha, FPS, negative control',flush=True)
        (out/'metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    # Complete-alpha temporal probe: detects actual global blackouts on a static
    # Clone, not a CPU approximation of the shader. Sample 12 s at 60 Hz.
    r.only={'upper_body'}
    alpha=np.array([r.frame(pr,1,time=float(t),**knobs(2,1.))[...,3].sum() for t in np.arange(0,12,1/60)])
    r.only=None
    black=alpha==0;starts=np.flatnonzero(black & ~np.r_[black[0],black[:-1]])/60.
    assert len(starts)>=2 and np.diff(starts).min() >= .5-1/60, ('flicker bound',starts.tolist())
    result['flicker_blackout_starts_s']=starts.tolist()
    result['stacked']=[]
    for amount in (.5,1.):
        pixels=int(np.ceil(sum(BOUNDS)*amount*r.projection_x*r.width*.5))
        allowed=silhouette_envelope(geometry_mask,pixels+1)
        for look in LOOKS:
            for t in TIMES[:8]:
                a=r.frame(pr,look,time=t,fx2=(amount,)*4,fx3=(amount,)*2)
                bad,metrics=hazards(a,geometry_mask,allowed)
                assert not bad, ('all six stacked',amount,look,t,bad,metrics)
            result['stacked'].append(dict(amount=amount,look=look,finite=True,bounds=True))
    # Crowd footprint: ensure Interlace becomes subpixel-safe instead of
    # inventing camera-dependent lattice frequencies. Real-avatar 130 px crop.
    crowd=RealAvatar(224,192,r.gl);cp=crowd.program(tree,('HAS_SKIN',))
    crowd.camera_center_z=r.camera_center_z
    scale=130/(.85*crowd.height*.5*(crowd.pose_bounds['bind'][1]-crowd.pose_bounds['bind'][0]))
    frames=[crowd.frame(cp,1,time=t,scale=scale,**knobs(4,1.)) for t in TIMES[:8]]
    assert all(np.isfinite(a).all() for a in frames)
    crowd_mask=crowd.frame(cp,1,scale=scale)[...,3]>0
    visible,perception=perceptible(frames,frames,crowd_mask)
    assert visible, ('crowd Interlace imperceptible',perception)
    result['crowd_interlace']=dict(finite=True,perception=perception,avatar_height_px=130)
    sheet=Image.new('RGB',(3*360,((len(strips)+2)//3)*145),'#101010')
    for i,strip in enumerate(strips):
        strip.thumbnail((360,145));sheet.paste(strip,((i%3)*360,(i//3)*145))
    sheet.save(out/'contact_sheet.png')
    assert r.gl.fn('glGetError',r.gl.U)()==0, 'clone FX GL error'
    (out/'metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result


if __name__=='__main__':
    check()
