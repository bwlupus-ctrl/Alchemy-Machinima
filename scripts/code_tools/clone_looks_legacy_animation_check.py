"""Additional real-avatar clipping, cycle-averaged appearance and phase-seam gates.

Called by the normal GL/safety entry points, with their production WGL context.
The cycle-averaged still, perceptibility and safety gates are independently enforced.
"""
import json
import numpy as np
from PIL import Image
from clone_looks_scan_build_check import ScanBuildReference, HeldScanFront

PERIOD = 628.31853071795864769  # exactly the host's double-precision 200*pi
CLIP_EXCESS = .01
STEADY_MAE, STEADY_P99 = .02, .06  # lead-approved replacement for t=0 identity
WRAP_ROUNDOFF = 1e-5  # < .003 of one 8-bit display code value


def host_phase(time, fps):
    stepped = np.floor(time*fps)/fps if fps else time
    return float(np.float32(stepped % PERIOD))


class MeanMotifReference:
    """Old still + analytic 5/16 DC, constructed independently of the candidate.

    The compensation contains no spatial phase and no fitted image map.
    Changes to authored shading or a static sine subtrahend cannot enter it.
    """
    def __init__(self,tree): self.tree=tree
    def read(self,path):
        source=self.tree.read(path)
        if path.endswith('actorghostF.glsl'):
            import re
            additions={
                64: '        rgb += min(tint * (ghostSceneValid != 0 ? 0.12 : 0.8), max(vec3(0.0), vec3(0.98)-rgb)) * 0.3125 * (1.0-F);\n',
                66: '        rgb += min(vec3(0.12,0.115,0.105), max(vec3(0.0), vec3(0.98)-rgb)) * 0.3125;\n',
                67: '        rgb += min(mix(color.rgb,vec3(1.0),0.65)*0.28, max(vec3(0.0),vec3(0.98)-rgb)) * 0.3125;\n',
                71: '        rgb *= 0.95;\n',
                76: """        float support = clamp(step(d,0.0)+band+lattice*0.55,0.0,1.0);
        float coverage = max(support,0.25);
        vec3 room = max(vec3(0.0),vec3(0.90)/coverage-rgb);
        rgb += min(planC*mix(0.55,0.16,step(d,0.0))/coverage,room)*0.3125*step(0.00001,support);
"""}
            for look,addition in additions.items():
                pattern=r'    else if \(ghostLook == '+str(look)+r'\) // Studio:[\s\S]*?(?=    else if \(ghostLook ==)'
                match=re.search(pattern,source); assert match,look
                branch=match.group();end=branch.rfind('    }')
                source=source[:match.start()]+branch[:end]+addition+branch[end:]+source[match.end():]
        return source


def cycle_average(r, current, old, look, mask, opts):
    from clone_looks_safety_check import display_rgb, G
    opts = dict(opts)
    held_scan = opts.pop('held_scan', False)
    if not hasattr(r,'mean_reference'):
        r.mean_reference=r.program(MeanMotifReference(ScanBuildReference(G.SourceTree('69cfd4637fd'))),('HAS_SKIN',))
    reference = r.mean_reference
    if held_scan:
        if not hasattr(r, 'held_scan_reference'):
            r.held_scan_reference = r.program(MeanMotifReference(HeldScanFront(ScanBuildReference(G.SourceTree('69cfd4637fd')))), ('HAS_SKIN',))
        reference = r.held_scan_reference
    rate = {64:.85,66:.75,67:.85,71:.75,76:.85}[look]
    # Automatic Scan progress (.3) and sheen (.85) share a .05 fundamental,
    # including at the real shared .5 default. No Scan progress UI exists.
    if look==76 and not held_scan: rate=.05
    n = 256 if look == 76 and not held_scan else 32
    mean = np.zeros((*mask.shape,3), np.float64)
    expected = np.zeros_like(mean); before=np.zeros_like(mean)
    for t in (np.arange(n)+.5)/n * (2*np.pi/rate):
        mean += display_rgb(r.frame(current,look,time=t,**opts))/n
        expected += display_rgb(r.frame(reference,look,time=t,**opts))/n
        if look==76: before+=display_rgb(r.frame(old,look,time=t,**opts))/n
    if look!=76: before=display_rgb(r.frame(old,look,time=0.,**opts)).astype(np.float64)
    # Independent analytic compensation for the positive motif's mean through
    # authored support, headroom and alpha. It contains NO phase or fitted map.
    dc=expected-before
    residual=mean-expected
    error=np.abs(residual[mask]); blocks=[]
    yy,xx=np.where(mask); y0,y1=yy.min(),yy.max()+1; x0,x1=xx.min(),xx.max()+1
    for ys in np.array_split(np.arange(y0,y1),16):
        for xs in np.array_split(np.arange(x0,x1),8):
            m=mask[np.ix_(ys,xs)]
            if m.sum()>=16: blocks.append(np.abs(residual[np.ix_(ys,xs)][m].mean(axis=0)).max())
    return dict(mae=float(error.mean()),p99=float(np.quantile(error,.99)),
                mean_bias=dc[mask].mean(axis=0).tolist(),dc_p99=float(np.quantile(np.abs(dc[mask]),.99)),
                uncompensated_mae=float(np.abs((mean-before)[mask]).mean()),
                static_pattern=float(max(blocks)),samples=n,period=2*np.pi/rate,
                compensation='analytic 5/16 motif mean through authored support/headroom/alpha; no phase, no fitted map')


def clipping(frame, mask):
    from clone_looks_safety_check import display_rgb
    rgb = display_rgb(frame)[mask]
    return dict(fraction=float(np.any(rgb >= 1., axis=-1).mean()),
                white_fraction=float(np.all(rgb >= 1., axis=-1).mean()),
                p99=float(np.quantile(rgb, .99)), maximum=float(rgb.max()))


def wrap_metrics(frames, phases, mask, continuation=None):
    from clone_looks_safety_check import display_rgb
    deltas = []
    for a, b in zip(frames, frames[1:]):
        error = np.abs(display_rgb(a)[mask] - display_rgb(b)[mask])
        deltas.append(dict(mae=float(error.mean()), p99=float(np.quantile(error, .99)), maximum=float(error.max())))
    seam = int(np.argmin(np.diff(phases)))
    assert phases[seam] > phases[seam+1], 'fixture missed the phase wrap'
    normal = {key:max(row[key] for i,row in enumerate(deltas) if i != seam) for key in deltas[0]}
    # The seam can naturally coincide with a local speed maximum. Compare
    # also with the SAME step on the smooth, unwrapped periodic curve; this
    # separates a phase discontinuity from ordinary derivative curvature.
    if continuation is not None:
        error=np.abs(display_rgb(frames[seam])[mask]-display_rgb(continuation)[mask])
        matched=dict(mae=float(error.mean()),p99=float(np.quantile(error,.99)),maximum=float(error.max()))
        normal={key:max(normal[key],matched[key]) for key in normal}
    failed = [key for key in normal if deltas[seam][key] > normal[key]*1.001 + WRAP_ROUNDOFF]
    return dict(phases=phases, seam=deltas[seam], normal_max=normal, failed=failed)


def hdr_checks(out,r,tree,current):
    """RGBA32F readback without display clipping, with and without blending."""
    from clone_looks_safety_check import LEGACY_ANIMATED, SEEDS, G
    rows=[]; original=r.background.copy()
    old=r.program(ScanBuildReference(G.SourceTree('69cfd4637fd')),('HAS_SKIN',))
    scale=600/(.85*r.height*.5*(r.pose_bounds['bind'][1]-r.pose_bounds['bind'][0]))
    for look in LEGACY_ANIMATED:
        for bg in (0.,.05):
            r.background[...,:3]=bg;r.background[...,3]=1.
            for scene in ((False,True) if look==64 else (False,)):
                for raw in (False,True):
                    r.raw_hdr=raw
                    minimum=float('inf');maximum=0.
                    times=sorted(set([0.,2.,4.,6.,8.,10.,628.30,.015,float(np.float32(PERIOD))]+list((np.arange(32)+.5)*PERIOD/32)))
                    for seed in SEEDS:
                        for t in times:
                            frame=r.frame(current,look,time=t,phase=seed,scale=scale,scene=scene,composite=True)
                            assert np.isfinite(frame).all(),('nonfinite HDR',look,bg,raw,t)
                            minimum=min(minimum,float(frame[...,:3].min()));maximum=max(maximum,float(frame[...,:3].max()))
                            assert minimum>=0,('negative HDR',look,bg,raw,t,minimum)
                            if raw:
                                before=r.frame(old,look,time=t,phase=seed,scale=scale,scene=scene,composite=True)
                                delta=frame[...,:3]-before[...,:3]
                                if look!=71:
                                    assert float(delta.min())>=-2e-6,('motif subtracts authored light',look,bg,t,float(delta.min()))
                                else:
                                    assert np.all(frame[...,:3]>=before[...,:3]*.84-2e-6),('ink exceeds multiplicative bound',bg,t)
                        np.savez_compressed(out/f'{look}_hdr_bg{bg}_scene{int(scene)}_raw{int(raw)}.npz',rgb=frame[...,:3])
                    rows.append(dict(look=look,background=bg,scene=scene,unblended=raw,minimum=minimum,maximum=maximum,times=times))
    r.raw_hdr=False;r.background[:]=original
    return rows


def shared_fx_wrap(r,tree):
    """All optional shared temporal inputs, both Studio and world main paths."""
    from clone_looks_safety_check import LEGACY_ANIMATED, display_rgb
    class SharedFxInputs:
        def read(self,path):
            text=tree.read(path)
            if path.endswith('actorghostF.glsl'):
                # Same production expressions with fixed non-default uniform
                # inputs, including a deliberately non-harmonic shimmer rate.
                text=text.replace('ghostFx.x','1.234').replace('ghostFx.y','0.4')
            return text
    rows=[]
    for defs in (('HAS_SKIN',),('HAS_SKIN','GHOST_WORLD_PASS','GHOST_SHARED_DISSOLVE')):
        p=r.program(SharedFxInputs(),defs)
        scale=600/(.85*r.height*.5*(r.pose_bounds['bind'][1]-r.pose_bounds['bind'][0]))
        mask=r.frame(p,1,scale=scale)[...,3]>0
        for look in LEGACY_ANIMATED:
            # The neutral fixture depth look must use the same periodic clock
            # policy as its beauty when comparing an unwrapped continuation.
            r.prime_look=look
            for distortion in (0,4,6,7,8):
                opts=dict(scale=scale,distort=distortion,params=(.4,1.,.3,8.),glitch=.3)
                a,b,c=[r.frame(p,look,time=t,**opts) for t in (628.30,.015,PERIOD+.015)]
                # Matched unwrapped continuation has identical shared tick/hash
                # laws, including their ordinary authored discrete transitions.
                # Match the actual F32 folded phase of the continuation;
                # F32(period+.015) cannot retain the same bits as F32(.015).
                reference_time=float(np.float32(PERIOD+.015)-np.float32(PERIOD))
                equivalent=r.frame(p,look,time=reference_time,**opts)
                error=np.abs(display_rgb(equivalent)[mask]-display_rgb(c)[mask])
                maximum=float(error.max())
                assert maximum<.001,('shared FX seam',defs,look,distortion,maximum)
                rounded=r.frame(p,look,time=float(np.float32(PERIOD)),**opts)
                zero=r.frame(p,look,time=0.,**opts)
                e=np.abs(display_rgb(rounded)[mask]-display_rgb(zero)[mask])
                rounded_maximum=float(e.max())
                assert rounded_maximum<.001,('shared FX F32 period',defs,look,distortion,rounded_maximum)
                rows.append(dict(definitions=defs,look=look,distortion=distortion,continuation_reference_time=reference_time,maximum=maximum,f32_period_maximum=rounded_maximum))
    del r.prime_look
    return rows


def scan_comparison(out,r,current,old):
    from PIL import ImageDraw,ImageFont
    from clone_looks_safety_check import G,display_rgb
    r.camera=np.pi/2
    scale=600/(.85*r.height*.5*(r.pose_bounds['bind'][1]-r.pose_bounds['bind'][0]))
    mask=r.frame(current,1,scale=scale)[...,3]>0
    yy,xx=np.where(mask[::-1]);box=(int(xx.min())-12,int(yy.min())-12,int(xx.max())+13,int(yy.max())+13)
    fix2=r.program(G.SourceTree('594fc49c8ae'),('HAS_SKIN',))
    images=[]
    for label,p in (('Old shading, corrected front',old),('Fix 2 (signed)',fix2),('Fix 4 (automatic)',current)):
        frame=r.frame(p,76,time=7.,scale=scale,dissolve_progress=1.)
        rgb=display_rgb(frame)
        im=Image.fromarray((np.clip(rgb[::-1],0,1)*255+.5).astype(np.uint8)).crop(box)
        panel=Image.new('RGB',(im.width,im.height+48),'#111111');panel.paste(im,(0,48))
        draw=ImageDraw.Draw(panel);draw.text((6,4),label,fill='white',font=ImageFont.truetype('arial.ttf',16))
        draw.text((6,25),f"clipped {clipping(frame,mask)['fraction']*100:.3f}%",fill='white')
        images.append(panel)
    combined=Image.new('RGB',(sum(i.width for i in images),images[0].height))
    x=0
    for im in images:combined.paste(im,(x,0));x+=im.width
    combined.save(out/'76_fix4_clip_comparison.png')


def check(out, r, tree, current):
    from clone_looks_safety_check import G, LEGACY_ANIMATED, SEEDS, VIEWS, display_rgb, note
    out.mkdir(parents=True, exist_ok=True)
    previous = r.program(ScanBuildReference(G.SourceTree('69cfd4637fd')), ('HAS_SKIN',))
    reviewed = r.program(G.SourceTree('f1aebace614'), ('HAS_SKIN',))
    result = dict(base='69cfd4637fd', reviewed='f1aebace614', period=PERIOD,
                  clip_metric='fraction of fixed neutral silhouette pixels with ANY display RGB channel >= 1 after RGB .035 compositing',
                  clip_excess_limit=CLIP_EXCESS, times=list(range(2,11)),
                  wrap_roundoff=WRAP_ROUNDOFF, looks={}, failures=[])
    for look in LEGACY_ANIMATED:
        rows=[]
        for pose in ('bind','raised','turned'):
            r.pose=pose;r.camera_center_z=sum(r.pose_bounds[pose])*.5
            scale=600/(.85*r.height*.5*(r.pose_bounds[pose][1]-r.pose_bounds[pose][0]))
            for camera,angle in VIEWS.items():
                r.camera=angle
                mask=r.frame(current,1,scale=scale)[...,3]>0
                for scene in ((False,True) if look==64 else (False,)):
                    for fps in (0,12):
                        for seed in SEEDS:
                            # Scan also exercises its unheld front and fully built endpoint.
                            for progress in ((None,0.,1.) if look==76 and pose=='bind' else (None,)):
                                opts=dict(scale=scale,scene=scene,phase=seed,dissolve_progress=progress)
                                row=dict(pose=pose,camera=camera,scene=scene,fps=fps,seed=seed,progress=progress,clip=[])
                                for time in range(2,11):
                                    t=host_phase(time,fps)
                                    a=r.frame(current,look,time=t,**opts)
                                    b=r.frame(previous,look,time=t,**opts)
                                    ca,cb=clipping(a,mask),clipping(b,mask)
                                    item=dict(time=time,current=ca,old=cb,excess=ca['fraction']-cb['fraction'])
                                    if pose=='bind' and fps==0:
                                        c=r.frame(reviewed,look,time=t,**opts)
                                        item['reviewed']=clipping(c,mask)
                                        item['reviewed_excess']=item['reviewed']['fraction']-cb['fraction']
                                    row['clip'].append(item)
                                    if item['excess']>CLIP_EXCESS:
                                        result['failures'].append(dict(look=look,check='per-pixel clipping',context={**row,'clip':[item]}))
                                    empty=(b[...,3]==0)
                                    if np.any(np.abs(a[empty])>1e-6):
                                        result['failures'].append(dict(look=look,check='lit empty space',pose=pose,camera=camera,time=time))
                                dt=1/(fps or 60)
                                center=np.floor(PERIOD*fps)/fps if fps else PERIOD
                                times=center+np.arange(-2,4)*dt
                                phases=[host_phase(t,fps) for t in times]
                                frames=[r.frame(current,look,time=t,**opts) for t in phases]
                                seam=int(np.argmin(np.diff(phases)))
                                continuation=r.frame(current,look,time=float(np.float32(times[seam+1])),**opts)
                                row['wrap']=wrap_metrics(frames,phases,mask,continuation)
                                if row['wrap']['failed']:
                                    result['failures'].append(dict(look=look,check='wrap jump',context={k:v for k,v in row.items() if k!='clip'}))
                                # Explicit lead-specified forced step and F32 period endpoint.
                                forced=[628.30,.015]
                                ff=[r.frame(current,look,time=t,**opts) for t in forced]
                                continuation=r.frame(current,look,time=PERIOD+.015,**opts)
                                def delta(a,b):
                                    e=np.abs(display_rgb(a)[mask]-display_rgb(b)[mask])
                                    return dict(mae=float(e.mean()),p99=float(np.quantile(e,.99)),maximum=float(e.max()))
                                seam_delta=delta(*ff)
                                # A normal step through zero on the centered, continuous
                                # phase curve avoids F32 loss in a never-uploaded >period time.
                                centered_left=float(np.float32(628.30)-np.float32(PERIOD))
                                left=r.frame(current,look,time=centered_left,**opts)
                                normal_delta=delta(left,ff[1])
                                failed=[k for k in seam_delta if seam_delta[k]>normal_delta[k]*1.001+WRAP_ROUNDOFF]
                                rounded=r.frame(current,look,time=float(np.float32(PERIOD)),**opts)
                                zero=r.frame(current,look,time=0.,**opts)
                                step=r.frame(current,look,time=1/60,**opts)
                                rounding=delta(rounded,zero);ordinary=delta(step,zero)
                                failed += ['rounding '+k for k in rounding if rounding[k]>ordinary[k]+WRAP_ROUNDOFF]
                                row['forced_wrap']=dict(phases=forced,centered_reference_time=centered_left,seam=seam_delta,matched_step=normal_delta,
                                                        f32_period=float(np.float32(PERIOD)),rounding=rounding,normal=ordinary,failed=failed)
                                if failed: result['failures'].append(dict(look=look,check='forced wrap',context=row['forced_wrap']))
                                rows.append(row)
        result['looks'][str(look)]=dict(matrix=rows,
            clip_excess_max=max(i['excess'] for row in rows for i in row['clip']),
            reviewed_clip_excess_max=max(i.get('reviewed_excess',0.) for row in rows for i in row['clip']),
            wrap_failures=sum(bool(row['wrap']['failed']) for row in rows))
        (out/'legacy_animation_stability_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print('LEGACY STABILITY',look,{k:v for k,v in result['looks'][str(look)].items() if k!='matrix'},flush=True)
    # Actual old shader must fail continuity; a deliberately saturated output
    # must fail the same per-pixel classifier (one bright channel is enough).
    r.pose='bind';r.camera=0.;r.camera_center_z=sum(r.pose_bounds['bind'])*.5
    scale=600/(.85*r.height*.5*(r.pose_bounds['bind'][1]-r.pose_bounds['bind'][0]))
    mask=r.frame(current,1,scale=scale)[...,3]>0
    phases=[host_phase(PERIOD+k/60,0) for k in (-2,-1,0,1,2,3)]
    bad=wrap_metrics([r.frame(reviewed,64,time=t,scale=scale) for t in phases],phases,mask)
    assert bad['failed'], 'unwrapped/re-easing shader escaped seam classifier'
    poison=r.frame(previous,66,time=6.,scale=scale);poison[mask,0]=1.1
    assert clipping(poison,mask)['fraction']>clipping(r.frame(previous,66,time=6.,scale=scale),mask)['fraction']+CLIP_EXCESS
    result['negative_controls']=dict(old_wrap_rejected=bad,single_channel_clip_rejected=True)
    # A static body stripe and the real signed predecessor must fail the same
    # independent old+DC comparison. A saturated channel still fails clipping.
    class Stripe:
        def read(self,path):
            text=tree.read(path)
            if path.endswith('actorghostF.glsl'):
                marker='        rgb += min(vec3(0.12,0.115,0.105)'
                text=text.replace(marker,'        rgb -= vec3(0.12) * pow(0.5+0.5*sin(phase),3.0);\n'+marker)
            return text
    striped=r.program(Stripe(),('HAS_SKIN',))
    stripe_metrics=cycle_average(r,striped,previous,66,mask,dict(scale=scale))
    assert stripe_metrics['static_pattern']>.02,('static stripe escaped',stripe_metrics)
    signed=r.program(G.SourceTree('594fc49c8ae'),('HAS_SKIN',))
    signed_metrics=cycle_average(r,signed,previous,66,mask,dict(scale=scale))
    assert signed_metrics['static_pattern']>.02,('signed predecessor escaped',signed_metrics)
    result['negative_controls'].update(static_stripe_rejected=stripe_metrics,signed_predecessor_rejected=signed_metrics)
    result['shared_fx_wrap']=shared_fx_wrap(r,tree)
    result['scan_mean_endpoints']=[]
    # Preserve the old held-front endpoint shading checks in an explicit
    # diagnostic permutation; the production look now always auto-builds.
    held_current = r.program(HeldScanFront(tree), ('HAS_SKIN',))
    held_previous = r.program(HeldScanFront(ScanBuildReference(G.SourceTree('69cfd4637fd'))), ('HAS_SKIN',))
    for progress in (0.,1.):
        for camera,angle in VIEWS.items():
            r.camera=angle
            endpoint_mask=r.frame(current,1,scale=scale)[...,3]>0
            metrics=cycle_average(r,held_current,held_previous,76,endpoint_mask,dict(scale=scale,dissolve_progress=progress,held_scan=True))
            assert metrics['mae']<=.02 and metrics['p99']<=.06 and metrics['static_pattern']<=.02,('Scan endpoint mean',progress,camera,metrics)
            result['scan_mean_endpoints'].append(dict(progress=progress,camera=camera,**metrics))
    scan_comparison(out,r,current,previous)
    r.camera=0.
    result['hdr'] = hdr_checks(out,r,tree,current)
    full=r.frame(current,76,time=2.,scale=scale)
    half=r.frame(current,76,time=2.,scale=scale,opacity=.5)
    ratio=float(half[mask,:3].mean()/max(float(full[mask,:3].mean()),1e-12))
    assert ratio<=.55, ('Scan sheen cancels operator opacity',ratio)
    assert not np.any(r.frame(current,76,time=2.,scale=scale,opacity=0.))
    result['scan_operator_half_opacity_ratio']=ratio
    (out/'legacy_animation_stability_metrics.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    assert not result['failures'], 'Legacy animation stability failed: '+str(result['failures'][:3])
    note('- PASS legacy per-pixel clipping <= old + 1%, full-cycle similarity, empty-space and normal-frame phase-wrap checks (both Glass paths; Scan held/unheld/built).')
    return result


if __name__=='__main__':
    from clone_looks_safety_check import G, ROOT, RealAvatar
    r=RealAvatar(1024,768);tree=G.SourceTree('WORKTREE');p=r.program(tree,('HAS_SKIN',))
    check(ROOT/'.tmp/looks16/renders/legacy_anim',r,tree,p)
