"""Colour chart offline / production-GLSL tests; no viewer build or launch.

Run with PYTHONUTF8=1 python scripts/code_tools/colorchart_check.py [--gpu].
GPU refuses to start while AlchemyTest.exe runs. Mutations are memory-only.
"""
import argparse
import ctypes as C
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import numpy as np
import colorchart_gen as G
import slog_gamut_check as S
import slog_gamut_external_check as E

ROOT = G.ROOT
OUT = ROOT / '.tmp/color_chart_20261027/tests'
TOL = .25 / 1023.
BASE = '398f7592e05'
RESULT = {}


def record(name, **evidence):
    RESULT[name] = evidence
    print('PASS ' + name + ': ' + json.dumps(evidence), flush=True)


def close(actual, expected, tolerance=TOL):
    np.testing.assert_allclose(actual, expected, rtol=0., atol=tolerance)


def negative(name, actual, expected, tolerance=TOL):
    error = float(np.max(np.abs(np.asarray(actual) - expected)))
    assert error > tolerance, name + ': NEGATIVE CONTROL SURVIVED'
    record('NEGATIVE ' + name, result='FAIL as required', max_error=error, tolerance=tolerance)


def matrix(gamut):
    return np.linalg.solve(S.rgb_xyz(S.PRIMARIES[gamut]), S.rgb_xyz(S.PRIMARIES[0]))


def model(values, gamut=1, purity=.9, ev=0., video=0, exposure=1., transpose=False):
    values = np.maximum(np.array(values, dtype=float), 0.) * exposure * 2.**ev
    reduced = S.apply_purity(values, purity)
    # Production explicitly preserves exact neutrals in purity arithmetic.
    neutrals = np.all(values == values[..., :1], axis=-1)
    reduced[neutrals] = values[neutrals]
    camera = np.maximum(reduced @ (matrix(gamut) if transpose else matrix(gamut).T), 0.)
    code = E.sony(camera)
    return (code * 876. + 64.) / 1023. if video else code


def contracts():
    from sensor_post_dispatch_check import body
    pipe = (ROOT/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    finalize = body(pipe, 'renderFinalize')
    assert finalize.index('renderUnlit(') < finalize.index('renderSensor(sourceBuffer, targetBuffer)')
    assert finalize.index('sensorVignette(') < finalize.index('renderUnlit(')
    assert finalize.index('applySMAA(') < finalize.index('renderUnlit(')
    assert finalize.index('renderFinalizePresent(') < finalize.index('presentUnlit(')
    for marker in ('renderUnlit(', 'presentUnlit('):
        gate = finalize[finalize.rfind('if (chart_show', 0, finalize.index(marker)):finalize.index(marker)]
        for flag in ('mMainRT', 'gCubeSnapshot', 'sPrismLensRender', 'sPrismPreviewRender',
                     'sImpostorRender', 'sRenderingHUDs', 'colorCorrectWillApplyExposure(hdr)'):
            assert flag in gate, (marker, flag)
    chart = (ROOT/'indra/newview/alcolorchart.cpp').read_text(encoding='utf-8')
    assert 'patch.linear' in chart and 'patch.srgb' in chart
    assert 'LLColor4U' not in chart
    assert 'SensorStage::Exposure' in chart and 'SensorStage::Camera' in chart
    assert 'GL_RGBA32F' in chart and 'LLGLDepthTest depth(GL_FALSE, GL_FALSE)' in chart
    assert 'sendMessage' not in chart and 'sendReliable' not in chart
    import xml.etree.ElementTree as X
    settings = list(X.parse(ROOT/'indra/newview/app_settings/settings.xml').getroot().find('map'))
    for key in ('Show', 'Placement', 'DebugReadback'):
        data = settings[next(i for i,n in enumerate(settings) if n.text=='AlchemyColorChart'+key)+1]
        entries = list(data)
        assert entries[next(i for i,n in enumerate(entries) if n.text=='Persist')+1].text == '0'
    record('source contracts', sensor_before_noise=True, legacy_separate_float_encoder=True,
           final_optics_bypassed=True, local_only=True, session_state_nonpersistent=True)


def cpu():
    assert G.HEADER.read_bytes() == G.generate().encode('utf-8'), 'header differs from generator'
    # Parse the compiled floats, not just the Python generator's input list.
    import re
    parsed = re.findall(r'\{"[^"]+", \{[^}]+\}, \{([^}]+)\}, \{([^}]+)\}\}', G.HEADER.read_text())
    assert len(parsed) == 27
    values = np.array([[float(v.strip().removesuffix('f')) for v in row[0].split(',')] for row in parsed], np.float32)
    close(values, G.values(), 3e-8)
    assert np.all(values[25] == np.float32(.18))
    rects = [tuple(map(int,row[1].split(','))) for row in parsed]
    assert rects == G.rectangles() and len(set(rects)) == 27
    cases = 0
    for gamut,purity,ev,video in itertools.product((0,1),(1.,.9),(0.,2.),(0,1)):
        codes = model(values,gamut,purity,ev,video)
        recovered = S.slog3_inverse((codes*1023.-64.)/876. if video else codes)
        recovered = S.inverse_purity(recovered @ np.linalg.inv(matrix(gamut)).T, purity) / 2.**ev
        close(recovered, values, 2e-14)
        cases += len(values)
    expected = model(np.array(G.values()))
    close(expected[[14,13,12]], [[.4435,.3191,.2991],[.3922,.4444,.3551],[.3088,.3235,.4455]], .001)
    close(expected[[25,24,26],0], [.410557,.596027,.092864], 1e-6)
    record('offline', patches=27, cases=cases, header_byte_identical=True,
           red_green_blue=expected[[14,13,12]].tolist(), grey_white_black=expected[[25,24,26],0].tolist())
    grey = model([[.18]*3],gamut=0,purity=1.)
    negative('injected after encoder', [[1.055*.18**(1/2.4)-.055]*3], grey)
    negative('sRGB code fed as linear', model([[1.055*.18**(1/2.4)-.055]*3],0,1.), grey)
    quantized = model([[G.linear(118)]*3],0,1.)
    negative('TE texture quantisation', quantized, grey)
    # VERIFY correction: 118 decodes to .1811642442, an error of .693776
    # ten-bit codes, so it fails BOTH .25 and .5 (the plan's .5-pass claim
    # is arithmetically false). Keep the requested stricter .25 gate.
    assert np.max(abs(quantized-grey)) > .5/1023.
    twice = model([[.18]*3],0,1.,exposure=2.)
    negative('sensor exposure missing', grey, twice)
    negative('sensor exposure doubled', model([[.18]*3],0,1.,exposure=4.), twice)
    negative('transposed gamut matrix', model(values,transpose=True), model(values))
    contracts()
    return values


def ocio(codes, values):
    import slog_gamut_ocio_check as O
    E.OUT = OUT
    search = E.local_aces_search()
    assert search['exact_configs'], 'PENDING-OCIO: exact local ACES 1.2 Sony IDT config unavailable'
    conversion = O.run(OUT, Path(search['exact_configs'][0]), codes.tolist())
    assert conversion['status'] == 'PASS', conversion
    d65 = np.array([.3127/.3290,1.,.3583/.3290])
    white = np.array([.32168/.33767,1.,(1.-.32168-.33767)/.33767])
    ap0 = E.xyz(((.7347,.2653),(0.,1.),(.0001,-.077)),(.32168,.33767))
    errors = {}
    transforms = {}
    for name,cat in [('CAT02',[[.7328,.4296,-.1624],[-.7036,1.6975,.0061],[.003,.0136,.9834]]),
                     ('Bradford',[[.8951,.2664,-.1614],[-.7502,1.7135,.0367],[.0389,-.0685,1.0296]])]:
        cat = np.array(cat)
        adapt = np.linalg.solve(cat, np.diag((cat@white)/(cat@d65))@cat)
        transforms[name] = np.linalg.solve(ap0,adapt@E.XYZ709)
        expected = values @ transforms[name].T
        errors[name] = float(np.max(abs(np.array(conversion['AP0'])-expected)))
    close(conversion['AP0'], values @ transforms['CAT02'].T, .001)
    original = np.array(G.values())
    recovered = np.array(conversion['AP0']) @ np.linalg.inv(transforms['CAT02']).T
    restored = np.concatenate((recovered[:27], S.inverse_purity(recovered[27:], .9)))
    close(restored, np.concatenate((original,original)), 1e-4)
    record('non-circular OCIO anchor', config=conversion['config'], errors=errors,
           inverse_purity_error=float(np.max(abs(restored-np.concatenate((original,original))))))


def gpu(values):
    import sensor_post_gl_check as P
    r = P.Runner()
    chart = r.program('class1/deferred/colorChartF.glsl', vertex='class1/deferred/colorChartV.glsl')
    identity = np.eye(4,dtype=np.float32)
    r.gl.use(chart)
    r.gl.matrix(r.gl.loc(chart,b'chart_matrix'),1,0,identity.ctypes.data_as(C.POINTER(C.c_float)))
    decoy = r.upload('chart_adaptation', np.full((1,1,3),.25,np.float32),fmt=0x8814)
    black = r.upload('chart_black', np.zeros((1,27,3),np.float32),fmt=0x8814)
    pixel = r.target('chart_pixel',1,1,fmt=0x8814)
    def inject(exposed, iris):
        return np.array([r.draw(chart,pixel,1,1,[('exposureMap',decoy)],
            dict(chart_colour=tuple(map(float,v)),chart_sample=0,chart_exposed=int(exposed),
                 exposure=iris,tonemap_type=8),matrix=False)[0][0,0,:3] for v in values])
    exposure_program = P.camera(r,'EXPOSURE')
    camera = P.camera(r,'CAMERA')
    legacy = P.camera(r,None)
    targets = [r.target('chart_'+name,27,1,fmt=0x8814) for name in ('exposed','encoded')]
    def draw(program, rows, uniforms):
        source = r.upload('chart_input',np.array(rows,np.float32)[None,:,:],fmt=0x8814)
        textures = [('diffuseRect',source),('diffuseMap',black),('bloomMap',black),('exposureMap',decoy)]
        return r.draw(program,targets[1],27,1,textures,uniforms)[0][0,:,:3]
    saved = []
    worst = 0.
    neutral = {}
    for gamut,purity,ev,video,iris in itertools.product((0,1),(1.,.9),(0.,2.),(0,1),(1.,2.)):
        u = dict(tonemap_type=8,tonemap_slog_gamut=gamut,tonemap_slog_purity=purity,
                 tonemap_params=(0.,float(video),0.,ev),tonemap_grade=(1.,1.),tonemap_mix=1.,
                 exposure=iris,gamma=1.,bloom_strength=0.)
        expected = model(values,gamut,purity,ev,video,iris)
        for name,got in [('Sensor',draw(camera,inject(True,iris),u)),
                         ('Legacy chart atlas',draw(camera,draw(exposure_program,inject(False,iris),u),u)),
                         ('Production Legacy',draw(legacy,values,u))]:
            close(got,expected)
            worst = max(worst,float(np.max(abs(got-expected))))
            key = (name,ev,video,iris)
            neutral_bytes=got[[19,20,22,23,24,25,26]].tobytes()
            assert neutral_bytes == neutral.setdefault(key,neutral_bytes), 'A2 neutral identity'
            if name == 'Sensor' and gamut==1 and ev==0 and video==0 and iris==1:
                saved.append((purity,got.copy()))
    record('production GPU codes', routes=3, patch_cases=27*32*3, tolerance=TOL, max_error=worst,
           neutral_byte_identity=True, adaptation_decoy=.25)
    # The actual chart shader must observe normal adaptation outside log capture.
    got = r.draw(chart,pixel,1,1,[('exposureMap',decoy)],
        dict(chart_colour=(.18,.18,.18),chart_sample=0,chart_exposed=1,exposure=2.,tonemap_type=0),matrix=False)[0][0,0,:3]
    close(got,[.09]*3,1e-7)
    base = dict(tonemap_type=8,tonemap_slog_gamut=0,tonemap_slog_purity=1.,
                tonemap_params=(0.,0.,0.,0.),tonemap_grade=(1.,1.),exposure=1.,bloom_strength=0.)
    greys = np.full((27,3),.18,np.float32)
    expected = model(greys,0,1.)
    # The real final blit receives already encoded values; an atlas injected
    # there without its encoder would deliver .18 directly, not Sony grey.
    negative('GPU injected after encoder',inject(False,1.)[25],expected[25])
    negative('GPU missing exposure',draw(camera,greys,dict(base,exposure=2.)),model(greys,0,1.,exposure=2.))
    negative('GPU doubled exposure',draw(legacy,greys*2,dict(base,exposure=2.)),model(greys,0,1.,exposure=2.))
    negative('GPU TE quantisation',draw(legacy,np.full_like(greys,G.linear(118)),base),expected)
    negative('GPU sRGB code as linear',draw(legacy,np.full_like(greys,1.055*.18**(1/2.4)-.055),base),expected)
    # Bloom contamination uses the unchanged production bloom composite, with a
    # supplied nonzero pyramid sample representing an adjacent white patch.
    scene = r.upload('bloom-grey',greys[None,:,:],fmt=0x8814)
    bloom = np.zeros((1,27,3),np.float32); bloom[0,1] = .05
    tex = r.upload('adjacent-white-bloom',bloom,fmt=0x8814)
    contaminated = r.draw(legacy,targets[1],27,1,[('diffuseRect',scene),('diffuseMap',black),
        ('bloomMap',tex),('exposureMap',decoy)],dict(base,bloom_strength=1.))[0][0,:,:3]
    negative('GPU early injection before bloom',contaminated[1],contaminated[0])
    # Mutate the utility only in memory, retaining the real production entry point.
    program=r.gl.program()
    for module in P.common_modules()+(S.TONE,'class1/deferred/LPMUtil.glsl'):
        source=r.source(module)
        if module==S.TONE: source=source.replace('slf_gamut_matrix() * c','transpose(slf_gamut_matrix()) * c')
        r.gl.attach(program,r.compile(source,'#define TONEMAP 1\n'))
    r.gl.attach(program,r.compile(r.source('class1/alchemy/colorCorrectF.glsl'),'#define TONEMAP 1\n'))
    r.gl.attach(program,r.base.vertex); r.gl.link(program)
    ok=C.c_int();r.gl.getprog(program,0x8B82,C.byref(ok));assert ok.value, 'mutation must compile'
    negative('GPU transposed gamut',draw(program,values,dict(base,tonemap_slog_gamut=1)),model(values,1,1.))
    bloom_fixture(r,legacy,decoy)
    saved.sort(reverse=True,key=lambda p:p[0])
    ocio(np.concatenate([p[1] for p in saved]), np.concatenate((np.array(G.values()),S.apply_purity(np.array(G.values()),.9))))
    sampling_fixture(r,chart)
    # Reuse the A7 transport harness: production colour-correct + final blit,
    # exact bytes at the requested immutable baseline, all scratch formats.
    for fmt in (0x8058,0x881A,0x8814):
        outputs=[]
        for ref in (None,BASE):
            pr=P.camera(r,None,ref=ref)
            intermediate=r.target('hidden-'+str(ref),27,1,fmt=fmt)
            source=r.upload('hidden-scene',values[None,:,:],fmt=0x8814)
            r.draw(pr,intermediate,27,1,[('diffuseRect',source),('diffuseMap',black),('bloomMap',black),('exposureMap',decoy)],base)
            blit=r.program('class1/alchemy/blitWithEffectsF.glsl',modules=P.common_modules()+('class1/alchemy/postEffectUtilsF.glsl',),ref=ref)
            outputs.append(r.draw(blit,r.target('hidden-final',27,1,fmt=0x8058),27,1,[('diffuseRect',intermediate[0])])[0])
        assert outputs[0].tobytes()==outputs[1].tobytes(), 'hidden A7 frame bytes changed'
    record('A7 hidden frame transport', baseline=BASE, byte_identical=True, scratch_formats=['RGBA8','RGBA16F','RGBA32F'])
    hidden_dispatch(r)


def sampling_fixture(r,chart):
    """Actual sample=1/2 shader branches with projective placement and viewport offsets."""
    import sensor_post_gl_check as P
    # Production Exposure + Camera encode the complete atlas, including gutters.
    width,height=880,736
    linear=np.zeros((height,width,3),np.float32)
    for colour,(x,y,w,h) in zip(G.values(),G.rectangles()):
        linear[y:y+h,x:x+w]=colour
    source=r.upload('sample-atlas-linear',linear,fmt=0x8814)
    exposed=r.target('sample-atlas-exposed',width,height,fmt=0x8814)
    encoded=r.target('sample-atlas-encoded',width,height,fmt=0x8814)
    adaptation=r.upload('sample-adaptation',np.ones((1,1,3),np.float32),fmt=0x8814)
    uniforms=dict(tonemap_type=8,tonemap_slog_gamut=1,tonemap_slog_purity=.9,
        tonemap_params=(0.,0.,0.,2.),tonemap_grade=(1.,1.),tonemap_mix=1.,exposure=2.)
    r.draw(P.camera(r,'EXPOSURE'),exposed,width,height,
        [('diffuseRect',source),('exposureMap',adaptation)],uniforms,read=False)
    atlas=r.draw(P.camera(r,'CAMERA'),encoded,width,height,
        [('diffuseRect',exposed[0])],uniforms)[0]
    for colour,(x,y,w,h) in zip(G.values(),G.rectangles()):
        close(atlas[y+h//2,x+w//2,:3],model([colour],ev=2.,exposure=2.)[0])

    sw,sh=48,36
    sy,sx=np.mgrid[:sh,:sw]
    screen=np.stack((.1+.6*sx/sw,.15+.5*sy/sh,.2+.4*(sx+sy)/(sw+sh)),axis=-1).astype(np.float32)
    screen_tex=r.upload('sample-screen-codes',screen,fmt=0x8814)
    canvas_w,canvas_h=128,96
    vx,vy,vw,vh=11,9,96,72
    matrix=np.eye(4,dtype=np.float32)
    matrix[0,0]=.65;matrix[1,1]=.68
    matrix[0,3]=.12;matrix[1,3]=-.05
    matrix[1,0]=.08;matrix[3,0]=.12
    packed=matrix.T.copy()
    r.gl.use(chart)
    r.gl.matrix(r.gl.loc(chart,b'chart_matrix'),1,0,packed.ctypes.data_as(C.POINTER(C.c_float)))
    clear_colour=r.gl.fn('glClearColor',None,C.c_float,C.c_float,C.c_float,C.c_float)
    clear=r.gl.fn('glClear',None,C.c_uint)
    sentinel=np.array([.037,.071,.113],np.float32)
    canvas=r.target('sample-canvas',canvas_w,canvas_h,fmt=0x8814)
    def render(mode,texture):
        # drawChart binds its encoded source with LLTexUnit::TFO_POINT.
        r.gl.bind_tex(0x0DE1,texture)
        r.gl.param(0x0DE1,0x2801,0x2600)
        r.gl.param(0x0DE1,0x2800,0x2600)
        r.draw(chart,canvas,canvas_w,canvas_h,[('diffuseRect',texture)],
            dict(chart_sample=mode,chart_exposed=0,chart_colour=(0.,0.,0.),
                 chart_viewport=(float(vx),float(vy),float(vw),float(vh))),read=False,matrix=False)
        clear_colour(*map(float,sentinel),1.)
        clear(0x4000)
        r.gl.viewport(vx,vy,vw,vh)
        r.gl.draw(0x0004,0,6)
        assert r.base.error()==0,'sampling GL state error'
        pixels=np.empty((canvas_h,canvas_w,4),np.float32)
        r.read_buffer(0x8CE0)
        r.gl.read(0,0,canvas_w,canvas_h,0x1908,0x1406,pixels.ctypes.data)
        return pixels[:,:,:3]

    yy,xx=np.mgrid[:canvas_h,:canvas_w]
    ndc=np.stack((2*(xx+.5-vx)/vw-1,2*(yy+.5-vy)/vh-1,np.ones_like(xx)),axis=-1)
    homography=matrix[np.ix_([0,1,3],[0,1,3])].astype(float)
    local=ndc@np.linalg.inv(homography).T
    local=local[:,:,:2]/local[:,:,2:]
    viewport=(xx>=vx)&(xx<vx+vw)&(yy>=vy)&(yy<vy+vh)
    interior=viewport&np.all(abs(local)<.98,axis=-1)
    outside=(~viewport)|np.any(abs(local)>1.02,axis=-1)
    counts={}
    for mode,texture,pixels in ((1,encoded[0],atlas[:,:,:3]),(2,screen_tex,screen)):
        uv=local*.5+.5 if mode==1 else ndc[:,:,:2]*.5+.5
        th,tw=pixels.shape[:2]
        position=uv*np.array([tw,th])
        # Exclude texel boundaries where floating raster arithmetic can choose
        # either neighbour; the numeric copy tolerance remains exactly zero.
        fraction=position-np.floor(position)
        samples=interior&np.all((fraction>.001)&(fraction<.999),axis=-1)
        index=np.floor(position).astype(int)
        expected=pixels[np.clip(index[:,:,1],0,th-1),np.clip(index[:,:,0],0,tw-1)]
        got=render(mode,texture)
        close(got[samples],expected[samples],0.)
        close(got[outside],np.broadcast_to(sentinel,got[outside].shape),0.)
        assert samples.sum()>2000,'sampling fixture lost coverage'
        wrong=render(3-mode,texture)
        negative('GPU sampling mode '+str(mode)+' uses wrong coordinate branch',wrong[samples],expected[samples])
        counts[mode]=int(samples.sum())
    record('chart sampling modes',modes=[1,2],pixels=counts,copy_tolerance=0.,
           viewport=[vx,vy,vw,vh],perspective=True,untouched_pixels=True,full_production_atlas=True)


def bloom_fixture(r,legacy,adaptation):
    """Adjacent white/grey through production bloom extraction/filter shaders."""
    w,h=32,16
    scene=np.full((h,w,3),.18,np.float32);scene[:,16:]=1.
    source=r.upload('bloom-chart-source',scene,fmt=0x8814)
    vertex='class1/effects/glowExtractV.glsl'
    extract=r.program('class1/effects/bloomExtractF.glsl',vertex=vertex)
    down=r.program('class1/effects/bloomDownsampleF.glsl','#define FIRST_DOWNSAMPLE 1\n',vertex=vertex)
    up=r.program('class1/effects/bloomUpsampleF.glsl',vertex=vertex)
    top=r.target('bloom-chart-extract',w,h,fmt=0x8814)
    middle=r.target('bloom-chart-down',w//2,h//2,fmt=0x8814)
    pyramid=r.target('bloom-chart-up',w,h,fmt=0x8814)
    r.draw(extract,top,w,h,[('diffuseMap',source)],dict(bloom_threshold=.6,bloom_knee=.1,alpha_glow_boost=0.))
    r.draw(down,middle,w//2,h//2,[('diffuseMap',top[0])],dict(bloom_texel_size=(1/w,1/h)))
    r.draw(up,pyramid,w,h,[('diffuseMap',middle[0])],dict(bloom_texel_size=(2/w,2/h),bloom_scatter=2.))
    black=r.upload('bloom-chart-zero',np.zeros_like(scene),fmt=0x8814)
    u=dict(tonemap_type=8,tonemap_slog_gamut=0,tonemap_slog_purity=1.,tonemap_params=(0.,0.,0.,0.),
           tonemap_grade=(1.,1.),exposure=1.,bloom_strength=1.)
    contaminated=r.draw(legacy,r.target('bloom-chart-coded',w,h,fmt=0x8814),w,h,
        [('diffuseRect',source),('diffuseMap',black),('bloomMap',pyramid[0]),('exposureMap',adaptation)],u)[0]
    negative('production bloom adjacent-white grey',contaminated[h//2,14,:3],contaminated[h//2,2,:3])
    record('bloom fixture',programs=['bloomExtractF','bloomDownsampleF FIRST_DOWNSAMPLE','bloomUpsampleF','colorCorrectF'],
           adjacent=contaminated[h//2,14,:3].tolist(),isolated=contaminated[h//2,2,:3].tolist())


def hidden_dispatch(r):
    # Native DLLs contain identically named C++ inline/static seams. Isolate each
    # reference in a fresh process instead of relying on Windows DLL co-loading.
    evidence=[]
    for reference in ('current','baseline'):
        log=OUT/('dispatch-'+reference+'.log')
        with log.open('w',encoding='utf-8') as stream:
            result=subprocess.run([__import__('sys').executable,str(Path(__file__).resolve()),
                '--dispatch',reference],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,timeout=240)
        assert result.returncode==0, log.read_text(encoding='utf-8')
        evidence.append(json.loads((OUT/('dispatch-'+reference+'.json')).read_text(encoding='utf-8')))
    current,baseline=evidence
    assert current['traces']==baseline['traces'], 'hidden full-finalize GL event stream changed'
    assert current['negative_trace']!=baseline['traces'][0], 'hidden-work negative control survived'
    record('NEGATIVE hidden chart work',result='FAIL as required',event_stream=current['negative_trace'])
    record('hidden full finalize dispatch',baseline=BASE,cases=len(current['traces']),
        byte_identical_GL_event_stream=True,current_regression_gates=current['regression'],
        baseline_regression_gates=baseline['regression'],
        event_hash=hashlib.sha256(json.dumps(current['traces']).encode()).hexdigest())


def dispatch_worker(reference):
    import sensor_post_dispatch_check as D
    import sensor_post_gl_check as P
    assert S.gpu_idle(), 'PENDING-GPU: python scripts/code_tools/colorchart_check.py --gpu'
    r=P.Runner()
    root=ROOT
    if reference=='baseline':
        root=OUT/'baseline-source-fixture'
        path=root/'indra/newview/pipeline.cpp'
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(subprocess.check_output(['git','show',BASE+':indra/newview/pipeline.cpp'],cwd=ROOT,timeout=15))
    directory=OUT/('dispatch-'+reference)
    regression=D.run(r,root,directory)
    regression.pop('source_methods')
    lib=r.native_lib
    callback=r.dispatch_callback
    def trace(dll):
        result=[]
        for i in range(2000):
            event=dll.event(i)
            if event==-1:return result
            result.append(event)
        raise AssertionError('unbounded dispatch trace')
    traces=[]
    lib.chart_settings.argtypes=[C.c_int,C.c_int,C.c_int]
    for enabled,aa,layers,chart_debug in itertools.product((0,1),(0,1,2),(0,1,2,3),(0,1)):
        lib.configure(callback,3,enabled,aa,layers,0,64,48)
        lib.chart_settings(0,chart_debug,0)
        lib.execute()
        got=trace(lib)
        assert not any(e in got for e in (990,991,992)), 'hidden chart performed work'
        traces.append(got)
    for enabled in (0,1):
        lib.configure(callback,3,enabled,0,0,0,64,48)
        lib.chart_settings(1,1,1)
        lib.execute()
        got=trace(lib)
        assert not any(e in got for e in (990,991,992)), 'disconnected chart performed work'
        traces.append(got)
    result=dict(traces=traces,regression=regression)
    if reference=='current':
        source=(directory/'dispatch.cpp').read_text(encoding='utf-8')
        assert source.count('if (chart_show && ')==2
        source=source.replace('if (chart_show && ','if (')
        (directory/'hidden_mutant.cpp').write_text(source,encoding='utf-8')
        command=(directory/'dispatch.cmd').read_text(encoding='utf-8').replace('dispatch.cpp','hidden_mutant.cpp').replace('dispatch.dll','hidden_mutant.dll').replace('dispatch.lib','hidden_mutant.lib')
        (directory/'hidden_mutant.cmd').write_text(command,encoding='utf-8')
        compiled=subprocess.run(['cmd','/d','/c',str(directory/'hidden_mutant.cmd')],cwd=ROOT,capture_output=True,text=True,timeout=90)
        (directory/'hidden_mutant.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
        assert compiled.returncode==0,compiled.stdout+compiled.stderr
        mutant=C.CDLL(str(directory/'hidden_mutant.dll'))
        mutant.configure.argtypes=lib.configure.argtypes
        mutant.configure(callback,3,0,0,0,0,64,48);mutant.execute()
        result['negative_trace']=trace(mutant)
    (OUT/('dispatch-'+reference+'.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--gpu', action='store_true')
    parser.add_argument('--dispatch', choices=('current','baseline'), help=argparse.SUPPRESS)
    args = parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    if args.dispatch:
        dispatch_worker(args.dispatch)
        return 0
    values=cpu()
    import colorchart_policy_check
    record('native chart policy', **colorchart_policy_check.run(OUT/'policy'))
    if args.gpu:
        if not S.gpu_idle():
            print('PENDING-GPU: python scripts/code_tools/colorchart_check.py --gpu',flush=True)
            return 2
        gpu(values)
    (OUT/'results.json').write_text(json.dumps(RESULT,indent=2)+'\n',encoding='utf-8')
    print('PENDING-INWORLD: enable AlchemyColorChartDebugReadback; no viewer launched',flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
