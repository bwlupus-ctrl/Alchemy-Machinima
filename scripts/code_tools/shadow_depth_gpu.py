"""Shadow depth/reach extensions to the existing production WGL fixtures.

Shader mutations are compiled in memory; tracked shader files are never mutated.
"""
from pathlib import Path
import ctypes as C
import json
import subprocess

BASE = '0c0385f401b'
OUT = Path('.tmp/shadow_depth_reach_20261030')


def timer(gl, draw, variants, iterations=120):
    """Alternate warmed whole draws; no allocation/readback in the measured window."""
    queries = (C.c_uint*(iterations*len(variants)))()
    gl.gen_queries(len(queries), queries)
    for _ in range(12):
        for setup in variants.values(): setup(); draw()
    for i in range(iterations):
        for j, setup in enumerate(variants.values()):
            setup()
            gl.begin_query(0x88BF, queries[i*len(variants)+j]); draw(); gl.end_query(0x88BF)
    rows = []
    for i in range(iterations):
        row = {}
        for j, name in enumerate(variants):
            ns = C.c_uint64(); gl.query_u64(queries[i*len(variants)+j],0x8866,C.byref(ns))
            row[name] = ns.value*1.e-6
        rows.append(row)
    import numpy as np
    return dict(p50_ms={k:float(np.median([r[k] for r in rows])) for k in variants},
                p95_ms={k:float(np.percentile([r[k] for r in rows],95)) for k in variants}, raw_ms=rows)


def check_depth(g):
    np, gl = g['np'], g['gl']
    runners, programs, Runner = g['runners'], g['programs'], g['Runner']
    setup, tex = g['setup'], g['tex']
    source = runners['off'].source('class3/deferred/softenLightF.glsl')
    baseline = Runner(BASE,True,2)
    runners['depth_base'] = baseline; programs['depth_base','soften'] = baseline.program('soften')
    gl.active(0x84C0+18); visibility = tex((0.,0.,0.,0.))
    gl.active(0x84C0+19); poison = tex((float('nan'),)*4)

    def draw(label='off', depth=0., count=1, fill=0., sun=0., mode=0, pbr=True, emissive=False, nan=False, lit=False):
        p = setup(label,'soften',mode,pbr=pbr)
        r = runners[label]
        # Fixture inputs only: the complete production soften main and modules run unchanged.
        r.set(p,'ambient_color',fill,fill,fill); r.set(p,'sunlight_color',sun,sun,sun)
        r.set(p,'moonlight_color',sun,sun,sun); r.set(p,'sky_gi_scale',1.)
        r.set(p,'ssao_irradiance_scale',1.); r.set(p,'ssao_irradiance_max',1000000.)
        r.set(p,'reflection_probe_ambiance',0.); r.set(p,'env_only_mode',mode)
        r.bind(p,'area_shadow_map0',18,visibility); r.bind(p,'area_shadow_map1',19,poison)
        gl.active(0x84C0+18); gl.bind_tex(0x0DE1,visibility)
        a=np.array((1.,)*4 if lit else (0.,)*4,np.float32)
        gl.teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,a.ctypes.data)
        for c in range(8):
            # Reconstructed position is (NDC x,y,-2); large range makes the fixture flat.
            r.set(p,f'area_depth_light[{c}]',0.,0.,8.,100.)
            r.set(p,f'area_depth_info[{c}]',float('nan') if nan else 10.,.5,-2.,0.)
            r.set(p,f'area_depth_axis[{c}]',0.,0.,-1.,0.)
        if depth is not None:r.set(p,'area_depth',depth,float(count))
        if emissive:
            gl.active(0x84C0+5); gl.bind_tex(0x0DE1,g['gbuf'][3])
            e=np.array((.5,.4,.3,1.),np.float32)
            gl.teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,e.ctypes.data)
        gl.draw(4,0,3)
        pixels=np.empty((g['height'],g['width'],4),np.float32)
        gl.read(0,0,g['width'],g['height'],0x1908,0x1406,pixels.ctypes.data)
        assert g['err']()==0, 'shadow-depth GL error'
        # Restore emissive G-buffer input for the parent gate.
        gl.active(0x84C0+5); gl.bind_tex(0x0DE1,g['gbuf'][3])
        z=np.zeros(4,np.float32);gl.teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,z.ctypes.data)
        return pixels

    def exercise(label):
        for pbr in (True,False):
            old=draw('depth_base',fill=.2,sun=.5,pbr=pbr)
            off=draw(label,depth=0.,fill=.2,sun=.5,pbr=pbr,nan=True)
            assert old.tobytes()==off.tobytes(), 'depth zero HEAD bytes / poisoned unused inputs'
            base=draw(label,depth=0.,fill=.2,pbr=pbr)
            on=draw(label,depth=.3,fill=.2,pbr=pbr)
            assert np.isfinite(on).all() and np.mean(on[...,:3]) < np.mean(base[...,:3])*.98, 'night fill must darken'
            # Evaluate the production helper with the very same linked full main below for exact weight.
            overlap=draw(label,depth=.3,count=4,fill=.2,pbr=pbr)
            assert np.array_equal(on,overlap), 'overlap must be max, never sum'
            daylight=draw(label,depth=.3,fill=100000.,pbr=pbr)
            day_off=draw(label,depth=0.,fill=100000.,pbr=pbr)
            relative=np.max(abs(daylight-day_off))/max(np.max(abs(day_off)),1.)
            assert relative < 1/255, 'bright fill must suppress depth'
            # Isolate direct and emissive by subtracting an identical environment-fill draw.
            for key in ('sun',):
                kw={key:.8 if key=='sun' else True}
                a=draw(label,depth=.3,fill=.2,pbr=pbr,**kw)-on
                b=draw(label,depth=0.,fill=.2,pbr=pbr,**kw)-base
                assert np.max(abs(b))>1.e-6, key+' positive control'
                assert np.allclose(a,b,atol=2.e-6,rtol=2.e-6), key+' changed by depth'
            assert np.array_equal(base,draw(label,depth=.3,fill=.2,pbr=pbr,lit=True)), 'fully visible identity'
            assert np.array_equal(draw(label,depth=0.,fill=.2,pbr=pbr,mode=1)[:,:g['width']//2],draw(label,depth=.3,fill=.2,pbr=pbr,mode=1)[:,:g['width']//2]), 'Environment only identity'

    # A fresh linked program whose area_depth has NEVER been uploaded must match HEAD.
    fresh=Runner('WORKTREE',True,2);runners['depth_fresh']=fresh
    programs['depth_fresh','soften']=fresh.program('soften')
    assert np.array_equal(draw('depth_base',fill=.2,sun=.5),draw('depth_fresh',depth=None,fill=.2,sun=.5)), 'never-uploaded default identity'
    exercise('off')
    # HAS_EMISSIVE is a real production permutation, with a nonzero positive
    # control. The default soften permutation cannot exercise emissiveRect.
    for label,ref in (('depth_emissive_base',BASE),('depth_emissive','WORKTREE')):
        r=Runner(ref,True,2);r.emissive=True
        runners[label]=r;programs[label,'soften']=r.program('soften')
    base=draw('depth_emissive',depth=0.,fill=.2)
    on=draw('depth_emissive',depth=.3,fill=.2)
    off_emissive=draw('depth_emissive',depth=0.,fill=.2,emissive=True)
    on_emissive=draw('depth_emissive',depth=.3,fill=.2,emissive=True)
    assert np.max(abs(off_emissive-base))>.01, 'emissive positive control'
    assert np.allclose(on_emissive-on,off_emissive-base,atol=2.e-6,rtol=2.e-6), 'emissive changed by depth'
    assert np.array_equal(draw('depth_emissive_base',fill=.2,emissive=True),off_emissive), 'emissive depth zero HEAD bytes'
    # Legacy fullbright is the G-buffer diffuse alpha=1 path.
    gl.active(0x84C0+2);gl.bind_tex(0x0DE1,g['gbuf'][0])
    fullbright=np.array((.5,.45,.4,1.),np.float32)
    gl.teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,fullbright.ctypes.data)
    assert np.array_equal(draw(depth=0.,fill=.2,pbr=False),draw(depth=.3,fill=.2,pbr=False)), 'legacy fullbright changed'
    gl.active(0x84C0+2);gl.bind_tex(0x0DE1,g['gbuf'][0]);fullbright[3]=0.
    gl.teximage(0x0DE1,0,0x8814,1,1,0,0x1908,0x1406,fullbright.ctypes.data)
    print('PASS HAS_EMISSIVE nonzero isolation/HEAD bytes and legacy fullbright identity',flush=True)
    # Full production programs; expected failures come from the SAME image assertions.
    mutants={
        'depth_scales_sunlit':source.replace('        vec3 diffuseColor = vec3(0.0);','        sunlit_linear *= areaDepthFill(tc,pos.xyz,gb.normal,irradiance);\n        vec3 diffuseColor = vec3(0.0);'),
        'sum_not_max':source.replace('occ = max(occ,(1.0-vis)*w);','occ += (1.0-vis)*w;'),
        'weight_ignores_fill':source.replace('e/(e+fill+0.0001)','e/(e+0.0001)'),
        'identity_branch_removed':source.replace('if (area_depth.x > 0.0 && !envOnlyIdentity()) irradiance *=','irradiance *=')}
    for label,text in mutants.items():
        assert text!=source,label
        r=Runner('WORKTREE',True,2);r.srccache['class3/deferred/softenLightF.glsl']=text
        runners[label]=r;programs[label,'soften']=r.program('soften')
        # Keep fixture routing/flag encoding identical to the passing control.
        original_runner,original_program=runners['off'],programs['off','soften']
        runners['off'],programs['off','soften']=r,programs[label,'soften']
        try:exercise('off')
        except AssertionError as e:print('PASS shadow-depth mutant rejected:',label,str(e),flush=True)
        else:raise AssertionError('shadow-depth mutant survived: '+label)
        finally:runners['off'],programs['off','soften']=original_runner,original_program
    # Plane 1 is a resource-access contract even if a driver optimizes unused lanes away.
    assert 'if (area_depth.y > 4.0) map1 = texture(area_shadow_map1,tc);' in source
    unconditional=source.replace('if (area_depth.y > 4.0) map1 =','map1 =')
    assert 'if (area_depth.y > 4.0) map1 = texture(area_shadow_map1,tc);' not in unconditional
    print('PASS shadow-depth mutant rejected: plane1_read_with_four (resource-access anchor)',flush=True)

    # Exact factor readback: only the entry point is a fixture; the production helper
    # and attenuation module are linked verbatim. Independent scalar expected values.
    helper_source=source[:source.index('void main()')]+"uniform float fixture_fill;\nvoid main(){float k=areaDepthFill(vec2(.5),vec3(0,0,-2),vec3(0,0,1),vec3(fixture_fill));frag_color=vec4(k);}"
    helper=Runner('WORKTREE',True,2);helper.srccache['class3/deferred/softenLightF.glsl']=helper_source
    runners['depth_factor']=helper;programs['depth_factor','soften']=helper.program('soften')
    for fill in (0.,.2,100000.):
        p=programs['depth_factor','soften'];gl.use(p);helper.set(p,'fixture_fill',fill)
        # draw supplies the same 10m delta, 100m range and 10 radiance units.
        pixel=draw('depth_factor',depth=.3,count=4,fill=fill)[0,0,0]
        energy=10.*(1.-(.1+.5)/(1.+.5))**2*2.
        expected=1.-.3*energy/(energy+fill+.0001)
        assert abs(float(pixel)-expected)<2.e-6,('depth * weight exact oracle',fill,pixel,expected)
    # Timed full production soften at 4K, eight live shadow-depth channels.
    p=setup('off','soften',0)
    r=runners['off'];r.set(p,'area_depth',.3,8.)
    for c in range(8):
        r.set(p,f'area_depth_light[{c}]',0.,0.,8.,100.);r.set(p,f'area_depth_info[{c}]',10.,.5,-2.,0.)
    visibility_pixels=np.zeros((2160,3840,4),np.float32)
    gl.active(0x84C0+18);visibility0=tex(visibility_pixels,3840,2160,0x8058)
    gl.active(0x84C0+19);visibility1=tex(visibility_pixels,3840,2160,0x8058)
    del visibility_pixels
    r.bind(p,'area_shadow_map0',18,visibility0);r.bind(p,'area_shadow_map1',19,visibility1)
    gl.active(0x84C0);gl.bind_tex(0x0DE1,g['target']);gl.teximage(0x0DE1,0,0x8814,3840,2160,0,0x1908,0x1406,None)
    gl.viewport(0,0,3840,2160);r.set(p,'screen_res',3840.,2160.)
    def state(d):
        def bind():gl.use(p);r.set(p,'area_depth',d,8.)
        return bind
    times=timer(gl,lambda:gl.draw(4,0,3),dict(off=state(0.),off_repeat=state(0.),on=state(.3)))
    times['delta_p50_ms']=times['p50_ms']['on']-times['p50_ms']['off']
    OUT.mkdir(exist_ok=True,parents=True);(OUT/'depth_gpu.json').write_text(json.dumps(times,indent=2))
    print('PASS 4K shadow depth ms:',{k:v for k,v in times.items() if k!='raw_ms'},flush=True)
    # Restore the parent gate's extent and explicit OFF uniforms.
    r.set(p,'area_depth',0.,0.);gl.active(0x84C0);gl.bind_tex(0x0DE1,g['target'])
    gl.teximage(0x0DE1,0,0x8814,g['width'],g['height'],0,0x1908,0x1406,None)
    gl.viewport(0,0,g['width'],g['height'])
    print('PASS shadow depth: production PBR/legacy OFF bytes, night/day, overlap, direct/emissive and identity',flush=True)


def check_reach(g):
    import relight_scene_fixture as S
    from relight_grain_check import Rig
    np=g['np'];rig=Rig(g)
    path=rig.paths['gather'];current=rig.text('gather')
    old=subprocess.check_output(['git','show',BASE+':'+path.as_posix()],text=True,timeout=20)
    baseline=rig.variant('reach baseline',{path:old},'class3/deferred/areaShadowF.glsl')
    plan=dict(samples=8,steps=32,near=0,half=False)
    u=(24.,12.,36.,3**.5)
    def scene(z,fov=60,w=256,h=192,block_distance=14.):
        cam=S.camera(np,(0,0,0),0,0,0,w,h,fov_deg=fov,far=100.)
        surfaces=[S.plane((0,0,-z),(0,0,1),0),S.plane((0,0,-z+block_distance),(0,0,1),1,rect=((1,0,0),(0,1,0),.5,1.5))]
        return S.cast(np,cam,surfaces)
    def render(pr,sc,on,world=False):
        g['area_use'](pr);g['uf'][4](g['loc'](pr,b'area_far_reach'),*(u if on else (0.,)*4))
        g['uf'][4](g['loc'](pr,b'area_world'),*((1.,)*4 if world else (0.,)*4))
        return rig.chain('new',sc,(sc.get('light_x',5.),0.,-sc['camera']['far_receiver']+25.,.001) if 'far_receiver' in sc['camera'] else (5.,0.,-5.,.001),plan,0,1080,
                         programs=(pr,g['area_filter']),upto='gather',full=True,thickness=.1,light_bias=.1,contact=sc.get('contact',0.))
    def fixture(z,fov=60):
        sc=scene(z,fov,block_distance=3. if z<=12 else 14.);sc['camera']['far_receiver']=z;return sc
    far=fixture(30);near=fixture(10);tele=fixture(30,20)
    world_cam=S.camera(np,(0,0,0),0,0,0,256,192,far=100.)
    world_scene=S.cast(np,world_cam,[S.plane((0,0,-30),(0,0,1),0),S.plane((0,0,-29),(0,0,1),1,rect=((1,0,0),(0,1,0),.5,1.5))])
    world_scene['camera']['far_receiver']=30;world_scene['contact']=2.;world_scene['light_x']=20.

    def exercise(pr,text=current):
        for sc in (far,near,tele):
            assert np.array_equal(render(baseline,sc,False),render(pr,sc,False)), 'reach OFF HEAD bytes'
        assert np.any(render(baseline,near,False)<255), 'near fixture must contain a real shadow'
        assert np.array_equal(render(pr,near,False),render(pr,near,True)), 'near pixels changed'
        assert np.array_equal(render(pr,tele,False),render(pr,tele,True)), 'tele projection must stay near'
        for sc in (near,far,world_scene):
            expected=render(baseline,sc,False,True)
            assert np.array_equal(expected,render(pr,sc,False,True)) and np.array_equal(expected,render(pr,sc,True,True)), 'world channel changed'
            if sc is world_scene:assert np.any(expected<255), 'world contact fixture must contain a real shadow'
        off=render(pr,far,False);on=render(pr,far,True)
        receiver=far['ident']==0
        caught=receiver & (on+np.int16(20)<off)
        assert np.count_nonzero(caught)>20, ('14m blocker on 30m receiver not reached',np.count_nonzero(caught))
        # Exact operands/step counts are a workload contract, independent of image luck.
        nonworld=text[text.index('            else traced = regular_ray ?'):text.index('            visibility += traced.x;')]
        assert 'thickness,area_steps,offset' in nonworld and 'clamp(area_rest_steps,1,64)' in nonworld, 'extra march steps'
        ramp=text[text.index('    float trace_dist'):text.index('    float native_strength')]
        assert ramp.index('if (zeq > area_far_reach.y)') < ramp.index('trace_thick = area_params.y*mix') and ramp.count('trace_thick =')==2, 'ungated thickness'
        return int(caught.sum())
    caught=exercise(g['area_gather'])
    mutants={
        'reach_on_world_channels':current.replace('min(area_params.x,area_contact_dist[light])','trace_dist'),
        'thickness_without_ramp_gate':current.replace('    float native_strength','    trace_thick = area_params.y*area_far_reach.w;\n    float native_strength'),
        'extra_far_steps':current.replace('thickness,area_steps,offset','thickness,area_steps+8,offset'),
        'ramp_ignores_projection':current.replace('/projection_matrix[1][1]','/1.7320508')}
    for label,text in mutants.items():
        assert text!=current,label
        pr=rig.variant(label,{path:text},'class3/deferred/areaShadowF.glsl')
        try:exercise(pr,text)
        except AssertionError as e:print('PASS reach mutant rejected:',label,str(e),flush=True)
        else:raise AssertionError('reach mutant survived: '+label)
    # 4K production plan, channels and split draws, on an upscaled far-receiver
    # field. Queries cover gather draws only, through the existing cost harness.
    from relight_cost_check import CostRig,planner_row,lights_of
    small=scene(30,w=320,h=180)
    large=dict(small)
    large['depth']=np.repeat(np.repeat(small['depth'],12,axis=0),12,axis=1)
    large['normals']=np.repeat(np.repeat(small['normals'],12,axis=0),12,axis=1)
    cost=CostRig(g);cost.cam=small['camera'];cost.light_world=np.array([5.,0.,-5.])
    effective=planner_row(g['policy']['grid'],2160,8,3,True)
    cost.bind_scene(rig,large)
    # Existing accuracy fixtures use RGBA32F for both inputs. The live main view
    # allocates D24 depth and RGBA16 HDR normals (pipeline addDeferredAttachments,
    # LLRenderTarget::allocate's default), so time those real bandwidth costs.
    depth_pixels=np.ascontiguousarray(large['depth'][...,0],dtype=np.float32)
    g['active'](0x84C0+1)
    g['teximage'](0x0DE1,0,0x81A6,3840,2160,0,0x1902,0x1406,depth_pixels.ctypes.data)
    normal_pixels=np.ascontiguousarray(large['normals'],dtype=np.float32)
    g['active'](0x84C0+2)
    g['teximage'](0x0DE1,0,0x805B,3840,2160,0,0x1908,0x1406,normal_pixels.ctypes.data)
    frame=cost.frame(rig,(g['area_gather'],g['area_filter']),effective,0.,large,lights_of(effective,8))
    g['area_use'](g['area_gather']);g['uf'][4](g['loc'](g['area_gather'],b'area_world'),0.,0.,0.,0.)
    rows=[]
    for i in range(18):
        row={}
        for on in ((False,True) if i%2 else (True,False)):
            frame.reach=u if on else (0.,)*4
            row['on' if on else 'off']=frame.timed(('gather',),frames=6)
        if i>=4:rows.append(row)
    times=dict(resolution=[3840,2160],formats=['D24','RGBA16'],plan=effective,lights=lights_of(effective,8),
        p50_ms={k:float(np.median([r[k] for r in rows])) for k in ('off','on')},
        p95_ms={k:float(np.percentile([r[k] for r in rows],95)) for k in ('off','on')},raw_ms=rows)
    times['delta_p50_ms']=times['p50_ms']['on']-times['p50_ms']['off']
    OUT.mkdir(exist_ok=True,parents=True);(OUT/'reach_gpu.json').write_text(json.dumps(times,indent=2))
    print('PASS 4K gather reach ms:',{k:v for k,v in times.items() if k!='raw_ms'},flush=True)
    frame.reach=(0.,)*4;frame.apply()
    print('PASS reach: OFF/near/tele/world exact bytes; 14m blocker caught on 30m receiver pixels=',caught,flush=True)
