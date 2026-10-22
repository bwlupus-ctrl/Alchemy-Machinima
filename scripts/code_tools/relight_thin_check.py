"""Independent dense, far-seed and curved-receiver production-GPU oracles.

Imported by relight_area_check (also run by relight_shader_check); keep this file
in the commit. Mutations use pinned review HEAD, never a moving self-reference.
Run directly it re-launches the GPU harness (--area-only --thin-only): it never
reports a silent success without executing the oracles.
"""
from pathlib import Path
import subprocess


def check_thin(g):
    np, C, F = (g[n] for n in ('np', 'C', 'F'))
    loc, ui, uf = (g[n] for n in ('loc', 'ui', 'uf'))
    gather = g['root']/'class3/deferred/areaShadowF.glsl'
    trace = g['root']/'class1/deferred/screenSpaceRayUtil.glsl'
    original = g['read_shader']
    heads = {p: subprocess.check_output(['git', 'show', 'd4f4ec1192ff:'+p.as_posix()], text=True)
             for p in (gather, trace)}

    def compile_variant(label, replacements):
        try:
            g['read_shader'] = lambda p: replacements.get(Path(p), original(p))
            return g['link_production']('class3/deferred/areaShadowF.glsl', '', label)
        finally:
            g['read_shader'] = original

    head = compile_variant('review HEAD uniform/common phase', heads)
    current_head = compile_variant('HEAD OFF byte baseline', {p: subprocess.check_output(
        ['git','show','HEAD:'+p.as_posix()],text=True) for p in (gather,trace)})
    # Independent-phase round-1 mutation, including its original uniform march.
    start = original(gather).index('            float offset =')
    end = original(gather).index('            float end_gap =')
    independent = original(gather)[:start]+'''            uint ray_seed = areaHash(seed ^ areaHash(uint(sample_idx)+0x9e3779b9u));
            float offset = (float(ray_seed & 0xffffu)+0.5)/65536.0;
'''+original(gather)[end:]
    independent = compile_variant('independent phases mutation', {gather: independent, trace: heads[trace]})
    interval = '    float dt = limit/float(steps);'
    sample = '        vec3 p = origin+direction*((float(step)+offset)*dt);'
    assert heads[trace].count(interval) == heads[trace].count(sample) == 1
    quadratic = heads[trace].replace(interval, '''    float interval_start = 0.0;
    float previous_t = 0.0;''').replace(sample, '''        float n = float(steps);
        float i = float(step+1);
        float first = min(0.03,limit/n);
        float interval_end = steps == 1 ? limit :
            first*i+(limit-first*n)*i*(i-1.0)/(n*(n-1.0));
        float t = mix(interval_start,interval_end,offset);
        float dt = t-previous_t;
        interval_start = interval_end;
        previous_t = t;
        vec3 p = origin+direction*t;''')
    quadratic = compile_variant('quadratic spacing mutation', {trace: quadratic})
    # Independent negative control: drop the join straddle; all 48 near samples are
    # then regular and none is pinned either side of the join.
    joined_trace = original(trace)
    assert 'bool join_pair = near_steps >= 4;' in joined_trace
    join_gap = compile_variant('segment join gap mutation', {trace: joined_trace.replace(
        'bool join_pair = near_steps >= 4;', 'bool join_pair = false;')})
    # Static guards for what a pixel test cannot see: the sample never leaves the ray,
    # and the 2 cm floor never overshoots a ray shorter than 2 cm.
    for text in ('float near_start = min(0.02,near_dist);', 't = min(t,limit);',
                 'float near_dist = near_steps > 0 ? min(1.85,limit):0.0;',
                 'int near_steps = clamp(area_near_steps,0,steps-1);',
                 'float near_end = join_pair ? max(near_start,near_dist-0.01):near_dist;'):
        assert text in joined_trace, text
    # Round-5 layout (2 m span, 47 gaps of 4.26 cm, pinned pair at 1.99/2.01 replacing
    # two regular samples, a 2 cm clamp): it leaves a 6.9 cm hole before the join.
    r5_trace = joined_trace
    for old, new in (
        ('min(1.85,limit)', 'min(2.0,limit)'),
        ('near_dt = (near_end-near_start)/float(max(near_regular,1));',
         'near_dt = near_steps > 1 ? near_dist/float(near_steps-1):near_dist;'),
        ('float t = near_segment ? near_start+(float(step)+offset)*near_dt:',
         'float t = near_segment ? max(0.02,(float(step)+offset)*near_dt):'),
        ('if (join_pair && step == near_regular) t = max(near_start,near_dist-0.01);',
         'if (near_steps >= 2 && step == near_steps-2) t = max(0.02,near_dist-0.01);'),
        ('if (join_pair && step == near_regular+1) t = near_dist+0.01;',
         'if (near_steps >= 2 && step == near_steps-1) t = min(limit,near_dist+0.01);')):
        assert r5_trace.count(old) == 1, old
        r5_trace = r5_trace.replace(old, new)
    r5_spacing = compile_variant('round-5 near spacing mutation', {trace: r5_trace})
    final = g['area_gather']
    # Fixed 64-step UNIFORM trace with today's disk/phase sampler, no near segment.
    dense = compile_variant('fixed dense uniform 64-step reference', {trace: heads[trace]})
    # Effective 1080p/K4 counts are returned by the production C++ planner.
    presets = [(name,*plan) for name,plan in zip(
        ('Low','Medium','High','Cinematic'),g['policy']['presets'])]
    failures = []
    def require(ok, message):
        if not ok:
            failures.append(message)
            print('[RelightQ] FAIL '+message, flush=True)

    def textures(depth, normals):
        g['area_texture'](1, np.ascontiguousarray(depth, dtype=np.float32))
        g['area_texture'](2, np.ascontiguousarray(normals, dtype=np.float32))
        output = g['area_texture'](0, np.zeros_like(depth), 0x8058)
        g['attachment'](0x8D40, 0x8CE0, 0x0DE1, output, 0)
        assert g['status'](0x8D40) == 0x8CD5

    scissor = g['gl']('glScissor',None,g['I'],g['I'],g['I'],g['I'])
    enable = g['gl']('glEnable',None,g['U'])
    def render(pr, samples, steps, half, seed, light, thickness=.035, near=0, box=None, distance=8, thin=True, fade=0, contact=.15, rgba=False):
        g['area_use'](pr)
        for name, value in ((b'projection_matrix', projection), (b'inv_proj', inverse)):
            value = np.ascontiguousarray(value.T,dtype=np.float32)
            g['matrix'](loc(pr,name),1,0,value.ctypes.data_as(C.POINTER(F)))
        for name,value in ((b'depthMap',1),(b'normalMap',2),(b'area_count',1),
                           (b'area_first',0),(b'area_samples',samples),(b'area_steps',steps),
                           (b'area_near_steps',near),(b'area_thin_objects',int(thin))):
            ui(loc(pr,name),value)
        w,h = ((width+1)//2,(height+1)//2) if half else (width,height)
        uf[2](loc(pr,b'area_trace_res'),w,h)
        uf[3](loc(pr,b'area_params'),distance,thickness,.5 if half else 1)
        uf[1](loc(pr,b'area_light_bias'),.25)
        uf[4](loc(pr,b'area_contact_dist'),contact,0,0,0)
        uf[4](loc(pr,b'area_contact_fade'),fade,0,0,0)
        lights=np.array([light]*4,np.float32); ids=np.array([seed,9999,7,54321],np.uint32)
        g['uniform4fv'](loc(pr,b'area_lights'),4,lights.ctypes.data_as(C.POINTER(F)))
        g['uniform4uiv'](loc(pr,b'area_seeds'),1,ids.ctypes.data_as(C.POINTER(g['U'])))
        box = box or (0,0,w,h)
        enable(0x0C11);scissor(*box)
        g['viewport'](0,0,w,h);g['draw'](4,0,3)
        image=np.empty((box[3],box[2],4),np.uint8)
        g['read'](*box,0x1908,0x1401,image.ctypes.data)
        g['disable'](0x0C11)
        return image if rgba else image[:,:,0]

    # Fixed dense reference, same receiver-only ROI for all presets and seeds.
    width,height=1536,256
    projection=np.eye(4);projection[0,0]=1/3;projection[1,1]=2;projection[2,2]=-.1
    inverse=np.linalg.inv(projection)
    x=(np.arange(width)+.5)/width*6-3
    depth=np.full((height,width,4),.7,np.float32);depth[:,abs(x)<.02]=.655
    normals=np.zeros_like(depth);normals[:,:,:2]=.5
    textures(depth,normals)
    # OFF: independent HEAD gather + utility, with production planner values.
    # Full RGBA bytes, both trace resolutions, unmapped and mapped contact fades.
    for preset,label in enumerate(('Low','Medium','High','Cinematic')):
        for half in (False,True):
            samples,steps,planned_half=g['policy']['off_presets'][preset*2+int(half)]
            assert bool(planned_half)==half
            for fade in (0,.25,.5,.75,1):
                for seed in (12345,1152,12348):
                    args=(samples,steps,half,seed,[.8,0,-4+np.sqrt(2.5**2-.8**2),.1])
                    expected=render(current_head,*args,thin=False,fade=fade,rgba=True)
                    actual=render(final,*args,thin=False,fade=fade,rgba=True)
                    require(np.array_equal(actual,expected),f'OFF HEAD bytes {label} half={half} fade={fade} seed={seed}')
                    if fade==0:
                        require(np.any(expected[:,:,0]<255),f'OFF positive shadow {label}/{half}')
            print(f'[RelightQ] OFF HEAD RGBA byte identity {label} half={half}: production S{samples}/steps{steps}, unmapped + mapped fades 0/.25/.5/.75/1, 3 seeds',flush=True)
    rail_rows=[]
    for label,samples,steps,half,near in presets:
        head_steps={'Low':8,'Medium':12,'High':20,'Cinematic':32}[label]
        ratios=[];head_ratios=[]
        coords=(np.arange(width//2 if half else width)+.5)/(width//2 if half else width)*6-3
        roi=(coords>-.7)&(coords<-.25)
        # Outside the entire analytic shadow footprint AND the visible rail.
        lit=(coords<-.9)|(coords>1.1)
        for seed in (12345,1152,12348):
            ref=render(dense,32,64,half,seed,[.8,0,-4+np.sqrt(2.5**2-.8**2),.1],distance=2)
            image=render(final,samples,steps+near,half,seed,[.8,0,-4+np.sqrt(2.5**2-.8**2),.1],near=near)
            baseline=render(head,samples,head_steps,half,seed,[.8,0,-4+np.sqrt(2.5**2-.8**2),.1])
            darkness=lambda im: np.sum(255-im[:,roi],dtype=np.int64)
            denominator=darkness(ref);assert denominator>0
            ratio=darkness(image)/denominator;head_ratio=darkness(baseline)/denominator
            ratios.append(ratio);head_ratios.append(head_ratio)
            require(.8<=ratio<=1.2,f'dense rail {label} seed {seed}: {ratio:.6f} outside [0.8,1.2]')
            require(np.all(image[:,lit]==255),f'rail lit pixels {label} seed {seed}: min {image[:,lit].min()}')
            replay=render(final,samples,steps+near,half,seed,[.8,0,-4+np.sqrt(2.5**2-.8**2),.1],near=near)
            require(np.array_equal(image,replay),f'rail deterministic {label} seed {seed}')
        rail_rows.append((label,ratios,head_ratios))
        print(f'[RelightQ] fixed dense rail {label}: final {ratios}; HEAD {head_ratios}',flush=True)
    rejected=sum(any(r<.8 or r>1.2 for r in row[2]) for row in rail_rows)
    require(rejected>=3,f'HEAD must fail dense railing at most presets: rejected {rejected}/4')

    # 4 cm rail centered EXACTLY at the near/far join (1.85 m) from the biased receiver origin.
    # Tiny aperture isolates the segment join from disk penumbra coverage.
    width,height=1536,128
    projection=np.eye(4);projection[0,0]=1/3;projection[1,1]=2;projection[2,2]=-.1
    inverse=np.linalg.inv(projection)
    x=(np.arange(width)+.5)/width*6-3
    missed=0
    for half in (False,True):
        pixel=160 if half else 320
        receiver_x=x[pixel*2+1 if half else pixel]
        direction=np.array([.9,0,np.sqrt(1-.9**2)])
        bias=max(.02,1.5*max(6/(width//2 if half else width),1/(height//2 if half else height)))
        origin=np.array([receiver_x,0,-8+bias])
        join=origin+1.85*direction
        light=origin+6*direction
        depth=np.full((height,width,4),.9,np.float32) # receiver Z=-8
        depth[:,abs(x-join[0])<=.02]=.5-.05*join[2]
        normals=np.zeros_like(depth);normals[:,:,:2]=.5
        textures(depth,normals)
        for label,samples,steps,_,near in presets:
            for seed in [1152,12348]+list(range(62)):
                args=(samples,steps+near,half,seed,[*light,1.e-5])
                image=render(final,*args,thickness=.001,near=near,box=(pixel,16,1,1))
                require(np.all(image==0),f'join rail {label} half={half} seed={seed}: {image.tolist()}')
                mutant=render(join_gap,*args,thickness=.001,near=near,box=(pixel,16,1,1))
                missed+=int(np.any(mutant>0))
        print(f'[RelightQ] join rail exactly 1.85 m half={half}: all presets / 64 seeds shadowed',flush=True)
    require(missed>0,'join-gap mutation must miss the SAME 4 cm rail')
    print(f'[RelightQ] join-gap mutation rejected {missed} preset/seed/pixels',flush=True)

    # Near-spacing sweep. A 4 cm rail is centred every 5 mm along the ray from 9 cm
    # (past the first sample, and clear of the receiver's own column) to the join; EVERY ray of every
    # disk sample must hit it. The ray is steep in depth (x component 0.3), so the rail
    # is 13 cm long along it and the spacing, not the footprint, decides detection.
    # Round 5 (4.26 cm spacing, 6.9 cm hole before the join) must fail the same sweep.
    sweep = [round(.09+.005*i,3) for i in range(int(round((2.0-.09)/.005))+1)]
    steep = np.array([.3,0,np.sqrt(1-.3**2)])
    sweep_missed = 0
    sweep_pairs = [row for row in presets if row[0] in ('Low','Cinematic')]
    for half in (False,True):
        pixel=160 if half else 320
        receiver_x=x[pixel*2+1 if half else pixel]
        bias=max(.02,1.5*max(6/(width//2 if half else width),1/(height//2 if half else height)))
        origin=np.array([receiver_x,0,-8+bias])
        light=origin+6*steep
        for centre in sweep:
            join=origin+centre*steep
            depth=np.full((height,width,4),.9,np.float32)
            depth[:,abs(x-join[0])<=.02]=.5-.05*join[2]
            normals=np.zeros_like(depth);normals[:,:,:2]=.5
            textures(depth,normals)
            for label,samples,steps,_,near in sweep_pairs:
                for seed in (1152,12348):
                    args=(samples,steps+near,half,seed,[*light,1.e-5])
                    if centre <= 1.85:
                        image=render(final,*args,thickness=.001,near=near,box=(pixel,16,1,1))
                        require(np.all(image==0),f'near sweep {label} half={half} seed={seed} rail at {centre} m: {image.tolist()}')
                    old=render(r5_spacing,*args,thickness=.001,near=near,box=(pixel,16,1,1))
                    sweep_missed+=int(np.any(old>0))
        print(f'[RelightQ] near sweep half={half}: {len([c for c in sweep if c<=1.85])} rail positions 0.09-1.85 m, Low/Cinematic, 2 seeds, every ray shadowed',flush=True)
    require(sweep_missed>0,'round-5 near spacing must miss the SAME 4 cm rail in the sweep')
    print(f'[RelightQ] round-5 spacing mutation rejected {sweep_missed} rail/preset/seed/pixels',flush=True)

    # Straddle guard: fewer than four near steps skip the join pair. A wide wall
    # across the ray (1.9 m out, past every near sample) stays fully shadowed and a
    # clear ray fully lit for 0..5 near steps.
    for near in range(6):
        for half in (False,True):
            pixel=160 if half else 320
            receiver_x=x[pixel*2+1 if half else pixel]
            bias=max(.02,1.5*max(6/(width//2 if half else width),1/(height//2 if half else height)))
            origin=np.array([receiver_x,0,-8+bias])
            light=origin+6*steep
            wall=origin+1.9*steep
            depth=np.full((height,width,4),.9,np.float32)
            normals=np.zeros_like(depth);normals[:,:,:2]=.5
            textures(depth,normals)
            clear=render(final,2,near+8,half,1152,[*light,1.e-5],thickness=.001,near=near,box=(pixel,16,1,1))
            require(np.all(clear==255),f'straddle guard near={near} half={half}: clear ray {clear.tolist()}')
            depth[:,x>=receiver_x+.05]=.5-.05*wall[2]
            textures(depth,normals)
            for seed in (1152,12348):
                blocked=render(final,2,near+8,half,seed,[*light,1.e-5],thickness=.001,near=near,box=(pixel,16,1,1))
                require(np.all(blocked==0),f'straddle guard near={near} half={half} seed={seed}: wall {blocked.tolist()}')
    print('[RelightQ] straddle guard: 0..5 near steps, wide wall shadowed and clear ray lit, full/half',flush=True)

    # Review P0: visible receiver and a panel seven metres along the ray.
    width,height=1920,1080
    projection=np.zeros((4,4));focal=1/np.tan(np.pi/6)
    projection[0,0]=focal*height/width;projection[1,1]=focal
    projection[2,2]=-(100+.1)/(100-.1);projection[2,3]=-2*100*.1/(100-.1);projection[3,2]=-1
    inverse=np.linalg.inv(projection)
    x=(np.arange(width)+.5)/width*2-1;y=(np.arange(height)+.5)/height*2-1
    panel_z=-15.050
    panel=(abs(x[None,:]*(-panel_z)/projection[0,0]-4.950)<=.5)&(abs(y[:,None]*(-panel_z)/focal)<=.5)
    def depth_at(z):
        clip=projection@np.array([0,0,z,1]);return (clip[2]/clip[3]+1)/2
    depth=np.full((height,width,4),depth_at(-20),np.float32);depth[panel]=depth_at(panel_z)
    normals=np.zeros_like(depth);normals[:,:,:2]=.5
    assert not panel[567,935]
    textures(depth,normals)
    seeds=[1152,12348]+list(range(62))
    mutations={independent:0,quadratic:0}
    for label,samples,steps,_,near in presets:
        head_steps={'Low':8,'Medium':12,'High':20,'Cinematic':32}[label]
        for half in (False,True):
            # Small receiver patch around the review pixel; full viewport and
            # full-size textures preserved. Scissor only avoids unrelated work.
            box=(460,274,16,16) if half else (920,548,32,32)
            worst=0;blocked=0
            for seed in seeds:
                base=render(head,samples,head_steps,half,seed,[6.364,0,-13.636,.1],.35,box=box)
                image=render(final,samples,steps+near,half,seed,[6.364,0,-13.636,.1],.35,near=near,box=box)
                mask=base<=2;blocked+=np.count_nonzero(mask)
                delta=image.astype(int)-base.astype(int)
                worst=max(worst,int(delta[mask].max()) if np.any(mask) else 0)
                for pr in mutations:
                    mutant=render(pr,samples,head_steps,half,seed,[6.364,0,-13.636,.1],.35,box=box)
                    mutations[pr]+=np.count_nonzero(mask&(mutant.astype(int)>base.astype(int)+2))
            require(blocked>0,f'far fixture has blocked pixels {label}/{half}')
            require(worst<=2,f'far {label} half={half}: maximum final-HEAD {worst}/255 >2/255')
            print(f'[RelightQ] far 64 seeds {label} half={half}: {blocked} HEAD-blocked pixels, worst increase {worst}/255',flush=True)
    for pr,name in ((independent,'independent-phase'),(quadratic,'quadratic')):
        require(mutations[pr]>0,f'{name} mutation must fail SAME far sweep')
        print(f'[RelightQ] {name} mutation rejected at {mutations[pr]} blocked seed/pixels',flush=True)

    # Convex cylinder, orthographic camera; analytic normals near terminator.
    width,height=768,128
    projection=np.eye(4);projection[0,0]=1/2;projection[1,1]=2;projection[2,2]=-.1
    inverse=np.linalg.inv(projection)
    x=(np.arange(width)+.5)/width*4-2
    inside=abs(x)<1.5
    z=np.sqrt(np.maximum(0,1.5**2-x*x))
    depth=np.ones((height,width,4),np.float32)
    depth[:,inside,:]=((6-z[inside])*.05+.5)[None,:,None]
    normal=np.stack([x/1.5,np.zeros(width),z/1.5],axis=-1)
    encoded=normal[:,:2]/np.maximum(np.sum(abs(normal),axis=-1,keepdims=True),1.e-9)*.5+.5
    normals=np.zeros_like(depth);normals[:,:,:2]=encoded[None,:,:]
    textures(depth,normals)
    light=np.array([4.,0,-4.5])
    positions=np.stack([x,np.zeros(width),-6+z],axis=-1)
    delta=light-positions;delta/=np.linalg.norm(delta,axis=-1,keepdims=True)
    ndl=np.sum(normal*delta,axis=-1)
    for label,samples,steps,_,near in presets:
        head_steps={'Low':8,'Medium':12,'High':20,'Cinematic':32}[label]
        for half in (False,True):
            indices=np.arange(width//2)*2+1 if half else np.arange(width)
            for band,mask,radius in (
                ('N.L>0.2',inside[indices]&(ndl[indices]>.2),.1),
                ('0<N.L<=0.2',inside[indices]&(ndl[indices]>0)&(ndl[indices]<=.2),1.e-5)):
                # Point-like aperture keeps every grazing ray unoccluded;
                # a finite disk can legitimately straddle the terminator.
                assert np.count_nonzero(mask)>0
                minimum=255
                for seed in (12345,1152,12348):
                    image=render(final,samples,steps+near,half,seed,[*light,radius],.35,near=near)
                    minimum=min(minimum,int(image[:,mask].min()))
                require(minimum==255,f'curved receiver {label} half={half} {band}: lit minimum {minimum}/255')
                print(f'[RelightQ] curved receiver {label} half={half}: {band} minimum {minimum}/255',flush=True)
    # Restore the main program's optional near segment for later fixtures.
    g['area_use'](final);ui(loc(final,b'area_near_steps'),0);ui(loc(final,b'area_thin_objects'),0)
    assert not failures, '\n'.join(failures)
    print('[RelightQ] thin switch PASS OFF HEAD bytes; ON dense [0.8,1.2], 64-seed far/join, curved terminator receiver and mutations',flush=True)


if __name__ == '__main__':
    # Running this file directly would otherwise import and exit 0 having checked
    # nothing. Re-launch the GPU harness that supplies the production context.
    import sys
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name('relight_shader_check.py')),
                                      '--area-only', '--thin-only', *sys.argv[1:]]))
