"""WP-B review regressions using production CoF/gather/combine and transfers."""
import argparse
import json
import numpy as np
import sensor_post_gl_check as P


def tests(r, check):
    w,h=192,160
    output=r.target('fix-output',w,h,fmt=0x8814)
    cof=r.program('class1/deferred/cofF.glsl',modules=P.common_modules())
    gather=r.program('class1/deferred/postDeferredF.glsl',
        '#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 1\n',P.common_modules())
    reduce_program=r.program('class1/deferred/postDeferredF.glsl',
        '#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 1\n#define AL_SENSOR_DOF_REDUCE 1\n',P.common_modules())
    reduced=r.target('fix-reduced',w,h)
    def source_cells(source,scale,radius):
        if r.reference:return source
        r.draw(reduce_program,reduced,int(w*scale),int(h*scale),[('diffuseRect',source)],
            dict(screen_res=(w,h),res_scale=scale,max_cof=radius),read=False)
        return reduced[0]
    combine=r.program('class1/deferred/dofCombineF.glsl','#define AL_SENSOR_HDR 1\n#define FRONT_BLUR 1\n')
    depth=r.upload('fix-depth',np.full((h,w,4),.5,np.float32))
    rows=[]
    for scale in (1.,.7,.5):
        for radius in (1.,4.,8.,32.):
            for offset in (0,1):
                image=np.zeros((h,w,4),np.float32);image[:,:,3]=1
                image[h//2+offset,w//2+offset,:3]=(16.,4.,1.)
                src=r.upload('fix-point',image)
                coc=r.target('fix-coc',w,h)
                blurred=r.target('fix-blur',w,h)
                r.draw(cof,coc,w,h,[('diffuseRect',src),('depthMap',depth)],dict(
                    focal_distance=-8.,blur_constant=.002,tan_pixel_angle=.0002,
                    magnification=1.,max_cof=radius,res_scale=scale),read=False)
                ww,hh=int(w*scale),int(h*scale)
                cells=source_cells(coc[0],scale,radius)
                r.gl.color_mask(1,1,1,0)
                r.draw(gather,blurred,ww,hh,[('diffuseRect',cells)],
                    dict(screen_res=(w,h),max_cof=radius,res_scale=scale),read=False)
                r.gl.color_mask(1,1,1,1)
                result=r.draw(combine,output,w,h,[('diffuseRect',blurred[0]),('lightMap',coc[0])],dict(
                    max_cof=radius,res_scale=scale,dof_width=(ww-1)/w,dof_height=(hh-1)/h))[0]
                ratio=result[:,:,:3].sum(axis=(0,1))/np.array([16.,4.,1.])
                yy,xx=np.mgrid[:h,:w]
                radial2=(xx-w//2-offset)**2+(yy-h//2-offset)**2
                rms=float(np.sqrt((radial2*result[:,:,0]).sum()/max(result[:,:,0].sum(),1e-9)))
                rows.append(dict(scale=scale,coc=radius,offset=offset,flux=ratio.tolist(),rms_radius=rms))
    check('isolated HDR highlights conserve flux through CoF/gather/combine at 1/.7/.5',
        all(max(abs(np.array(row['flux'])-1))<.005 for row in rows),rows)
    check('isolated HDR bokeh retains circular aperture extent',all(
        .45*row['coc']<row['rms_radius']<1.1*row['coc'] for row in rows if row['coc']>=4.),rows)
    # A spatially constant HDR field must stay constant, including reduction.
    image[:,:,:3]=16.;src=r.upload('fix-constant',image)
    constants=[]
    for scale in (1.,.7,.5):
        ww,hh=int(w*scale),int(h*scale)
        r.draw(cof,coc,w,h,[('diffuseRect',src),('depthMap',depth)],dict(focal_distance=-8.,
            blur_constant=.002,tan_pixel_angle=.0002,magnification=1.,max_cof=4.),read=False)
        cells=source_cells(coc[0],scale,4.)
        r.draw(gather,blurred,ww,hh,[('diffuseRect',cells)],dict(screen_res=(w,h),max_cof=4.,res_scale=scale),read=False)
        result=r.draw(combine,output,w,h,[('diffuseRect',blurred[0]),('lightMap',coc[0])],dict(
            max_cof=4.,res_scale=scale,dof_width=(ww-1)/w,dof_height=(hh-1)/h))[0]
        constants.append(float(abs(result[:,:,:3]-16).max()))
    check('constant HDR field through reduced DOF chain',max(constants)<.02,constants)
    # Focused detail bypasses the reduced blur lattice, including 0.7 scale.
    image[:,:,:3]=np.random.default_rng(12).uniform(.01,16.,(h,w,3));image[:,:,3]=.5
    src=r.upload('fix-focus',image)
    r.draw(gather,blurred,int(w*.7),int(h*.7),[('diffuseRect',src)],
        dict(screen_res=(w,h),max_cof=4.,res_scale=.7),read=False)
    focused=r.draw(combine,output,w,h,[('diffuseRect',blurred[0]),('lightMap',src)],
        dict(max_cof=4.,res_scale=.7))[0]
    check('focused detail bypasses reduced DOF lattice',np.max(abs(focused[:,:,:3]-image[:,:,:3]))<.008)
    # Compare the original alpha/CoC reconstruction on heterogeneous source
    # alpha and depth, including the special transition interval.
    image[:,:,:3]=.2;image[:,:,3]=np.random.default_rng(73).uniform(0,1,(h,w))
    contract_source=r.upload('fix-alpha-source',image)
    image[:,:,3]=np.random.default_rng(74).uniform(0,1,(h,w))
    contract_coc=r.upload('fix-alpha-coc',image)
    legacy_combine=r.program('class1/deferred/dofCombineF.glsl')
    alpha_errors=[]
    for scale in (.7,.5):
        u=dict(max_cof=4.,res_scale=scale,dof_width=(int(w*scale)-1)/w,dof_height=(int(h*scale)-1)/h)
        textures=[('diffuseRect',contract_source),('lightMap',contract_coc)]
        a=r.draw(combine,output,w,h,textures,u)[0]
        b=r.draw(legacy_combine,output,w,h,textures,u)[0]
        alpha_errors.append(float(abs(a[:,:,3]-b[:,:,3]).max()))
    check('DOF preserves the original alpha/CoC combine contract',max(alpha_errors)<1e-6,alpha_errors)
    # Run the actual production camera transfer both before and after the
    # converted display ray. Manual/adapted exposure must not scale the ray.
    ray=P.camera(r,'GODRAYS');expose=P.camera(r,'EXPOSURE');camera=P.camera(r,'CAMERA')
    import sensor_post_recipe_check as recipes
    controls=recipes.lpm_controls(P.ROOT,P.OUT/('lpm_controls_'+str(__import__('os').getpid())))
    lpm_uniforms={'tonemap_amd['+str(i)+']':row.copy() for i,row in enumerate(controls.reshape(24,4))}
    def tone_parameters(tone):
        if tone==8:return (0.,0.,0.,0.)
        if tone==2:
            white=6.*1.8
            return ((white*(white+.0245786)-.000090537)/(white*(.983729*white+.432951)+.238081),0.,0.,0.)
        if tone==4:
            white=6.
            return ((white*(.88*white+.06)+.002)/(white*(.88*white+.6)+.06)-.01/.3,0.,0.,0.)
        if tone==6:return P.S.agx_params()
        return (36.,0.,0.,0.)
    godray_rows=[]
    for tone in range(9):
        for manual,adapt in ((.5,.25),(1.,1.),(4.,3.)):
            for level in (0.,.05,.3,16.):
                image[:,:,:3]=level;src=r.upload('fix-ray-scene',image)
                adapted=r.upload('fix-adapt',np.full_like(image,adapt))
                u=dict(exposure=manual,tonemap_type=tone,tonemap_mix=1.,tonemap_grade=(1.,1.),
                    tonemap_params=tone_parameters(tone),tonemap_khronos_params=(.15,.76),**lpm_uniforms)
                exposed=r.draw(expose,output,w,h,[('diffuseRect',src),('exposureMap',adapted)],u)[0]
                exposed_tex=r.upload('fix-ray-exposed',exposed)
                base=r.draw(camera,output,w,h,[('diffuseRect',exposed_tex)],u)[0]
                display_increment=np.array([.1,.07,.03])
                legacy=exposed.copy();legacy[:,:,:3]=display_increment
                legacy_tex=r.upload('fix-ray-legacy',legacy)
                if r.reference:
                    # The pre-fix production dispatch added display-unit rays
                    # directly to the exposed HDR target before stage C.
                    converted=exposed.copy();converted[:,:,:3]+=display_increment
                else:
                    converted=r.draw(ray,output,w,h,[('diffuseRect',exposed_tex),('diffuseMap',legacy_tex)],u)[0]
                converted_tex=r.upload('fix-ray-converted',converted)
                final=r.draw(camera,output,w,h,[('diffuseRect',converted_tex)],u)[0]
                desired=np.minimum(base[:,:,:3]+display_increment,1.)
                error=float(abs(final[:,:,:3]-desired).max())
                godray_rows.append(dict(tone=tone,manual=manual,adaptation=adapt,level=level,error=error,
                    clipped=bool(desired.max()>=1.-1e-6),finite=bool(np.isfinite(final).all())))
    check('godray display-unit conversion manual/adaptation/S-Log3',
        all(row['finite'] and row['error']<.001 for row in godray_rows if row['tone'] in (3,8)),godray_rows)
    check('godray conversion across all nine production operators inside display gamut',
        all(row['finite'] and (row['clipped'] or row['error']<.001) for row in godray_rows),godray_rows)
    # Run the production sun raymarch, using real all-lit shadow comparison
    # textures. Its HDR increment must be independent of the HDR scene base.
    C=P.C;gl=r.gl
    shadow=C.c_uint();gl.gen_tex(1,C.byref(shadow));gl.bind_tex(0x0DE1,shadow)
    for key in (0x2800,0x2801):gl.param(0x0DE1,key,0x2600)
    for key in (0x2802,0x2803):gl.param(0x0DE1,key,0x812F)
    gl.param(0x0DE1,0x884C,0x884E);gl.param(0x0DE1,0x884D,0x0203)
    shadow_data=np.ones((8,8),np.float32)
    gl.teximage(0x0DE1,0,0x8CAC,8,8,0,0x1902,0x1406,shadow_data.ctypes.data)
    defs='#define SUN_SHADOW 1\n#define GODRAYS_FADE 0\n'
    modules=P.common_modules()+('class1/deferred/shadowUtil.glsl',)
    programs=[r.program('class3/deferred/volumetricLightF.glsl',defs+extra,modules,ref=ref)
        for extra,ref in (('#define AL_SENSOR_HDR 1\n',None),('',None),('','33f08838acf'))]
    matrices=np.tile(np.eye(4,dtype=np.float32),(4,1,1))
    for pr in programs:
        gl.use(pr);gl.matrix(gl.loc(pr,b'shadow_matrix[0]'),4,0,matrices.ctypes.data_as(C.POINTER(C.c_float)))
    ray_depth=r.upload('fix-lit-depth',np.full_like(image,.99))
    ray_u=dict(godray_res=8,godray_multiplier=.2,falloff_multiplier=.1,blue_density=(.1,.1,.1),
        haze_density=.1,sunlight_color=(.5,.4,.3),shadow_clip=(100.,100.,100.,100.),shadow_res=(8.,8.))
    rendered=[]
    for base in (0.,16.,128.):
        image[:,:,:3]=base;src_ray=r.upload('fix-raymarch-input',image)
        textures=[('diffuseRect',src_ray),('depthMap',ray_depth)]+[('shadowMap'+str(i),shadow) for i in range(4)]
        rendered.append([r.draw(pr,output,w,h,textures,ray_u)[0] for pr in programs])
    check('production HDR godray increment avoids half-float base cancellation',
        np.max(rendered[0][0][:,:,:3])>0 and all(np.array_equal(row[0][:,:,:3],rendered[0][0][:,:,:3]) for row in rendered) and
        np.max(abs(rendered[0][0][:,:,:3]-rendered[0][1][:,:,:3]))<1e-6)
    check('legacy sun godray full-frame identity',all(np.array_equal(row[1],row[2]) for row in rendered))
    # White affine overlays at the image edge must not smear into the fisheye
    # black border. Derive that border with the actual color warp shader.
    image[:,:,:3]=1.;src=r.upload('fix-white',image)
    zero=r.upload('fix-overlay-black',np.zeros_like(image))
    fish=r.program('class1/deferred/cineFisheyeF.glsl')
    params=dict(fisheye_params=(1.2,.3,.92,.01),fisheye_params2=(0.,0.,1.,0.))
    warped=r.draw(fish,output,w,h,[('diffuseMap',src)],params)[0]
    overlay=r.program('class1/alchemy/sensorOverlayF.glsl')
    guide=r.draw(overlay,output,w,h,[('diffuseRect',zero),('diffuseMap',src),('emissiveRect',zero)],
        dict(params,depth_warp_params=(0.,1.,w/h,0.),sensor_overlay_mode=0))[0]
    # Compute the OOB mask independently from the published mapping, because
    # the fisheye color shader also has an intentional in-bounds vignette.
    yy,xx=np.mgrid[:h,:w];cx=((xx+.5)/w*2-1)*w/h;cy=(yy+.5)/h*2-1
    rr=(cx*cx+cy*cy)/(.92*.92);factor=(1+1.2*rr+.3*rr*rr)/2.5
    outside=(abs(cx*factor/(w/h))>1)|(abs(cy*factor)>1)
    check('fisheye out-of-bounds overlays suppressed',not np.any(guide[outside,:3]),
        dict(false_pixels=int(np.any(guide[outside,:3]!=0,axis=1).sum()),border_pixels=int(outside.sum())))
    # Compile and exercise an edge variant while the default predication
    # setting is ON. The sensor variant's compile define wins over it.
    mods=('class1/environment/srgbF.glsl','class1/deferred/SMAA.glsl')
    defs='#define SMAA_GLSL_4 1\n#define SMAA_PRESET_HIGH 1\n#define SMAA_PREDICATION 0\n#define SMAA_REPROJECTION 0\n#define AL_SENSOR_HDR 1\n'
    edge=r.program('class1/deferred/SMAAEdgeDetectF.glsl',defs,mods,'class1/deferred/SMAAEdgeDetectV.glsl')
    image[:,:w//2,:3]=.3;image[:,w//2:,:3]=.31;src=r.upload('fix-pred-edge',image)
    results=[]
    for pred in (src,zero):
        results.append(r.draw(edge,output,w,h,[('diffuseRect',src),('predicationTex',pred)],
            {'SMAA_RT_METRICS':(1/w,1/h,w,h)})[0])
    check('sensor SMAA edges invariant to unwarped predication with default ON',np.array_equal(*results))
    # The optional layer format has the same ten-bit display RGB precision as
    # the legacy presentation target. Bound the complete affine composite.
    rng=np.random.default_rng(17)
    addition=rng.uniform(0,1,(h,w,4)).astype(np.float32)
    transmission=rng.uniform(0,1,(h,w,4)).astype(np.float32)
    errors=[]
    for mode in (0,1):
        outputs=[]
        for fmt in (0x8814,0x8059):
            add=r.upload('fix-layer-add'+str(fmt),addition,fmt=fmt)
            tr=r.upload('fix-layer-tr'+str(fmt),transmission,fmt=fmt)
            outputs.append(r.draw(overlay,output,w,h,[('diffuseRect',src),('diffuseMap',add),('emissiveRect',tr)],
                dict(depth_warp_params=(0.,0.,w/h,0.),sensor_overlay_mode=mode))[0])
        errors.append(float(abs(outputs[0][:,:,:3]-outputs[1][:,:,:3]).max()))
    check('ten-bit optional affine layers preserve display precision',max(errors)<1.1/1023.,errors)
    # Guide black capture is copied into the existing depth-free display
    # target before white capture reuses the depth-sharing target.
    copy=r.program('class1/deferred/postDeferredNoDoFF.glsl',modules=P.common_modules())
    packed=r.upload('fix-guide-copy-input',addition,fmt=0x8059)
    display=r.target('fix-guide-copy-target',w,h,fmt=0x8059)
    copied=r.draw(copy,display,w,h,[('diffuseRect',packed),('depthMap',depth)])[0]
    gl.bind_fbo(0x8D40,r.base.fbo)
    gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,packed,0)
    r.read_buffer(0x8CE0)
    expected=np.empty_like(copied)
    gl.read(0,0,w,h,0x1908,0x1406,expected.ctypes.data)
    check('existing display guide capture copy is byte-identical',np.array_equal(expected,copied))
    # Diagnostics are affine marks over the processed image in D. They must
    # not gather/warp the already-processed background a second time.
    yy,xx=np.mgrid[:h,:w]
    image[:,:,:3]=np.stack((xx/w,yy/h,(xx+yy)/(w+h)),axis=2)
    src=r.upload('fix-diag-image',image)
    original=image[:,:,:3].astype(np.float16).astype(np.float32)
    kal=r.program('class1/deferred/ultimateKaleidoF.glsl','#define AL_SENSOR_HDR 1\n',P.common_modules())
    mrt=r.target('fix-diag-kal',w,h,3)
    marks=r.draw(kal,mrt,w,h,[('diffuseRect',src),('depthMap',depth)],dict(
        kal_pattern=(0.,6.,1.,0.),kal_center=(.5,.5,.5,.5),kal_source=(0.,1.,0.,0.),
        kal_look=(0.,1.,1.,0.),sensor_diagnostics=1))[0]
    residual=marks[:,:,:3]-.55*original
    swatches=np.array([[.15,.35,.55],[.55,.30,.15]])*.45
    err=np.min(np.max(abs(residual[:,:,None,:]-swatches[None,None,:,:]),axis=3),axis=2)
    check('stage-D kaleidoscope diagnostics preserve processed background',float(err.max())<.001)
    diopter=r.program('class1/deferred/ultimateDiopterF.glsl','#define AL_SENSOR_HDR 1\n',P.common_modules())
    mask=np.zeros_like(image);mask[:,:,3]=.5
    mask=r.upload('fix-diag-mask',mask)
    result=r.draw(diopter,output,w,h,[('diffuseRect',mask),('emissiveRect',zero),('diffuseMap',src),('depthMap',depth)],
        dict(diopter_comp2=(0.,0.,1.,1.),sensor_diagnostics=1))[0]
    expected=original*.775+np.array([1.,.25,.25])*.225
    check('stage-D diopter diagnostics preserve processed background',float(abs(result[:,:,:3]-expected).max())<.001)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--reference');args=ap.parse_args()
    r=P.Runner(args.reference);rows=[]
    def check(name,passed,details=None):
        rows.append(dict(name=name,passed=bool(passed),details=details));print(('PASS ' if passed else 'FAIL ')+name,flush=True)
    tests(r,check)
    path=P.OUT/('fix_before.json' if args.reference else 'fix_after.json')
    path.write_text(json.dumps(rows,indent=2),encoding='utf-8')
    return int(any(not row['passed'] for row in rows))

if __name__=='__main__':raise SystemExit(main())
