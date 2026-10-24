"""Real-WGL light-shape dot regression, using production deferred/forward GLSL.

Run directly; --shader-ref COMMIT reads sources without moving HEAD. The exact
4f1c152fa08 negative control must fail the peak/width and emitter checks.
--dots-out DIR saves JSON, float readbacks and images (default .tmp/lshape_dots).
--dots-compare JSON verifies exact OFF and PBR bytes against a baseline matrix.
Only geometry, material/G-buffer inputs and projector texture are fixtures.
"""
import json
import hashlib
import re
import subprocess
import sys
from pathlib import Path

if 'link_production' not in globals():
    raise SystemExit(subprocess.call([sys.executable, str(Path(__file__).with_name(
        'relight_shader_check.py')), '--dots-only', *sys.argv[1:]]))

from PIL import Image

dots_out = Path(sys.argv[sys.argv.index('--dots-out')+1] if '--dots-out' in sys.argv
                else '.tmp/lshape_dots')
dots_out.mkdir(parents=True, exist_ok=True)
dots_tag = 'before' if shader_ref else 'after'
dots_failures = []
dots_rows = []


def dots_check_view_bind():
    """Compile the exact production master upload; no viewer build."""
    path = Path('indra/newview/alcinerigrim.cpp')
    text = (subprocess.check_output(['git', 'show', shader_ref+':'+path.as_posix()], text=True)
            if shader_ref else path.read_text())
    start = text.index('    static LLCachedControl<bool> shape_enabled')
    binding = text[start:text.index('    static const LLStaticHashedString limits', start)]
    shim = r'''
#include <cstdio>
bool shapeEnabled=false,areaEnabled=false,gCubeSnapshot=false;
struct Settings {} gSavedSettings;
template<class T> struct LLCachedControl {
    LLCachedControl(Settings&,const char*,T) {}
    T operator()() const { return shapeEnabled; }
};
struct LLPipeline {
    static inline bool sPrismLensRender=false,sRenderingHUDs=false,sImpostorRender=false,sReflectionRender=false;
} ;
struct Pipeline { bool areaShadowsActive() { return areaEnabled; } } gPipeline;
struct GL { bool shape=false; void setLightShapeActive(bool v) { shape=v; } } gGL;
namespace LLShaderMgr { enum { RELIGHT_ENABLED }; }
struct Shader { int flags=-1; void uniform1i(int,int v) { flags=v; } };
void bind(Shader& shader) {
'''
    test = r'''
}
int main() {
    int failures=0;
    for (int mask=0;mask<128;++mask) {
        shapeEnabled=mask&1;gCubeSnapshot=mask&2;LLPipeline::sPrismLensRender=mask&4;
        LLPipeline::sRenderingHUDs=mask&8;LLPipeline::sImpostorRender=mask&16;
        LLPipeline::sReflectionRender=mask&32;areaEnabled=mask&64;
        Shader shader;bind(shader);
        int expected=((mask&1) && !(mask&62) ? 1:0) | (areaEnabled ? 2:0);
        if (shader.flags!=expected || gGL.shape!=shapeEnabled) ++failures;
    }
    std::printf("[dots-view] 128 production binding cases: %d failures\n",failures);
    return failures ? 1:0;
}
'''
    out = dots_out/('view_'+dots_tag); out.mkdir(parents=True, exist_ok=True)
    source = (out/'view.cpp').resolve(); source.write_text(shim+binding+test)
    vcvars = Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat')
    exe = (out/'view.exe').resolve()
    cmd = (out/'compile.cmd').resolve()
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++17 /EHsc "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\n' %
                   (vcvars, source, exe, (out/'view.obj').resolve(), exe))
    dots_require(subprocess.call(['cmd', '/d', '/c', str(cmd)], cwd=out) == 0,
                 'production auxiliary-view master binding')


def dots_require(condition, label):
    if not condition:
        dots_failures.append(label)


dots_check_view_bind()


# Same LUT settings, R16F storage and filtering as pipeline::createLUTBuffers.
import xml.etree.ElementTree as ET
items = list(ET.parse('indra/newview/app_settings/settings.xml').getroot().find('map'))


def dots_setting(key):
    node = list(items[next(i for i, n in enumerate(items) if n.tag == 'key' and n.text == key)+1])
    return float(node[next(i for i, n in enumerate(node) if n.tag == 'key' and n.text == 'Value')+1].text)


lut_x, lut_y = [int(dots_setting(k)) for k in ('RenderSpecularResX', 'RenderSpecularResY')]
lut_exp = dots_setting('RenderSpecularExponent')
sa = np.linspace(0, 1, lut_x, dtype=np.float32)[None, :]
exponent = np.linspace(0, 1, lut_y, dtype=np.float32)[:, None]**2*lut_exp
lut = np.ascontiguousarray(sa**exponent*((exponent+2)*(exponent+4)/(
    8*np.pi*(2**(-exponent/2)+exponent))))
dots_gloss = U(); genTex(1, C.byref(dots_gloss)); active(0x84C1); bindTex(0x0DE1, dots_gloss)
for key, value in ((0x2801, 0x2600), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F)):
    param(0x0DE1, key, value)
teximage(0x0DE1, 0, 0x822D, lut_x, lut_y, 0, 0x1903, 0x1406, lut.ctypes.data)

# Circular projector aperture, with real mip levels used by the production
# projector reflection. Its near-plane image must not bypass area-light size.
cookie_pixels = np.ones((128, 128, 4), np.float32)
yy, xx = np.mgrid[:128, :128]
cookie_pixels[:, :, :3] = (np.hypot((xx+.5)/128-.5, (yy+.5)/128-.5) < .45)[:, :, None]
dots_cookie = U(); genTex(1, C.byref(dots_cookie)); active(0x84C2); bindTex(0x0DE1, dots_cookie)
for key, value in ((0x2801, 0x2703), (0x2800, 0x2601), (0x2802, 0x812F), (0x2803, 0x812F)):
    param(0x0DE1, key, value)
teximage(0x0DE1, 0, 0x8814, 128, 128, 0, 0x1908, 0x1406, cookie_pixels.ctypes.data)
gl('glGenerateMipmap', None, U)(0x0DE1)
active(0x84C0)

dots_inputs = '''
vec3 relightUnit(vec3 v, vec3 fallback);
vec3 pbrCalcPointLightOrSpotLightRimKeepRef(vec3 diffuseColor, vec3 specularColor,
    float perceptualRoughness, float metallic, vec3 n, vec3 p, vec3 v,
    vec3 lp, vec3 ld, vec3 lightColor, float lightSize, float falloff,
    float is_pointlight, float ambiance, vec4 shape, vec4 rim, float spec_keep);
uniform vec4 dotsShape, dotsSpec;
uniform vec3 dotsOrigin;
uniform float dotsRoughness, dotsMetallic, dotsEnv, dotsView, dotsExtent, dotsSide, dotsHeight;
uniform float dotsWood;
uniform int dotsPbr, dotsComponent;
vec3 dotsPosition() {
    return vec3((gl_FragCoord.xy-vec2(dotsSide,dotsHeight)*.5)/max((dotsSide-1.)*.5,1.)*
        dotsExtent-vec2(dotsView*4.,0),-4);
}
GBufferInfo dotsBuffer() {
    GBufferInfo gb;
    vec3 p=dotsPosition();
    float grain=.9+.1*sin(p.y*35.+2.*sin(p.x*7.));
    gb.albedo=vec4(dotsComponent==1 && dotsPbr==0 ? vec3(0) :
        mix(vec3(.6),vec3(.5,.35,.16)*grain,dotsWood),1);
    gb.specular=dotsPbr!=0 ? vec4(1,dotsRoughness,dotsMetallic,0) : dotsSpec;
    gb.normal=vec3(0,0,1); gb.emissive=vec4(0);
    gb.gbufferFlag=dotsPbr!=0 ? GBUFFER_FLAG_HAS_PBR : 0.;
    gb.envIntensity=dotsEnv;
    return gb;
}
'''


def dots_program(path):
    deferred = path in ('point', 'spot')
    file = ('class3/deferred/'+path+'LightF.glsl' if deferred else
            'class3/deferred/materialF.glsl' if path == 'material' else
            'class2/deferred/'+path+'F.glsl')
    original = read_shader(root/file)
    if deferred:
        original = original.replace('getScreenCoord(vary_fragcoord)', 'vec2(.5)')
        original = original.replace('getPosition(tc).xyz', 'dotsPosition()')
        original = original.replace('getGBuffer(tc)', 'dotsBuffer()')
        header, body = original.split('void main()', 1)
        for name, value in {'light_shape': 'dotsShape', 'trans_center': 'dotsOrigin',
                            'proj_origin': 'dotsOrigin'}.items():
            body = re.sub(r'\b'+name+r'\b', value, body)
        original = header+'void main()'+body
    else:
        original = original.replace('void main()', 'void unusedProductionMain()')
        common = 'dotsPosition(),vec3(0,0,1),vec4(dotsOrigin,1),vec3(0,0,-1),9./15.,1.5,1,'
        if path in ('material', 'alpha'):
            args = 'vec3(1),'
            if path == 'material': args += 'relightUnit(-dotsPosition(),vec3(0,0,1)),'
            args += '(dotsComponent==1 ? vec3(0) : vec3(.6)),'
            if path == 'material': args += 'vec4(srgb_to_linear(dotsSpec.rgb),dotsSpec.a),'
            args += common
            if path == 'material': args += 'glare,'
            args += '0,dotsShape,vec4(0),rigRimNeutralPixel(vec3(0,0,1))'
            call = 'vec3 c=calcPointLightOrSpotLight('+args+');'
        else:
            call = '''vec3 c=pbrCalcPointLightOrSpotLightRimKeepRef(
                vec3(.576)*(1.-dotsMetallic),
                mix(vec3(.04),vec3(.6),dotsMetallic),dotsRoughness,dotsMetallic,
                vec3(0,0,1),dotsPosition(),relightUnit(-dotsPosition(),vec3(0,0,1)),
                dotsOrigin,vec3(0,0,-1),vec3(1),15.,.5,1.,0.,dotsShape,vec4(0),1.);'''
        original += '\nvoid main(){float glare=0;'+call+'frag_color=vec4(c,glare);}\n'
    original = original.replace('/*[EXTRA_CODE_HERE]*/', '/*[EXTRA_CODE_HERE]*/\n'+dots_inputs)
    return link_production(file, '#define DIFFUSE_ALPHA_MODE 1\n#define USE_DIFFUSE_TEX 1\n',
                           'dot regression '+path, original)


def dots_uniform(name, *values):
    uf[len(values)](loc(dots_pr, name.encode()), *values)


def dots_int(name, value):
    ui(loc(dots_pr, name.encode()), value)


def dots_target(side, height=None):
    active(0x84C0); bindTex(0x0DE1, tex)
    teximage(0x0DE1, 0, 0x8814, side, height or side, 0, 0x1908, 0x1406, None)
    viewport(0, 0, side, height or side)


def dots_read(distance=2., radius=.45, softness=1., view=0., enabled=1,
              gloss=.25, roughness=.6, metallic=0., env=0., pbr=0, component=1,
              physical=0, side=257, height=1, extent=1., shift=0., wood=0.):
    # Distance is the centre-to-receiver length at the centre pixel.
    angle = np.deg2rad(view)
    origin = (-4*np.tan(angle)-distance*np.sin(angle)+shift, 0, -4+distance*np.cos(angle))
    dots_uniform('dotsOrigin', *origin)
    dots_uniform('dotsView', np.tan(angle)); dots_uniform('dotsExtent', extent)
    dots_uniform('dotsSide', side); dots_uniform('dotsHeight', height); dots_uniform('dotsWood', wood)
    dots_uniform('dotsSpec', gloss, gloss, gloss, gloss)
    dots_uniform('dotsRoughness', roughness); dots_uniform('dotsMetallic', metallic)
    dots_uniform('dotsEnv', env); dots_int('dotsPbr', pbr); dots_int('dotsComponent', component)
    dots_uniform('dotsShape', radius, physical, 128/9 if physical else 1., -1.)
    dots_uniform('relight_highlight_scale', softness); dots_int('relight_enabled', enabled)
    # A 90-degree projector. The emitting near plane is 8 cm from its origin;
    # its tiny reflected aperture is independent of the World light size.
    dots_uniform('proj_p', origin[0], 0, origin[2]-.08)
    projection = np.array([[.5,0,-.5,-.5*origin[0]+.5*origin[2]],
                           [0,.5,-.5,.5*origin[2]], [0,0,-1,origin[2]],
                           [0,0,-1,origin[2]]], np.float32)
    matrix(loc(dots_pr, b'proj_mat'), 1, 0, np.ascontiguousarray(projection.T).ctypes.data_as(C.POINTER(F)))
    gl('glClearColor', None, F, F, F, F)(0, 0, 0, 0)
    gl('glClear', None, U)(0x4000)
    draw(4, 0, 3)
    pixels = np.empty((height, side, 4), np.float32)
    read(0, 0, side, height, 0x1908, 0x1406, pixels.ctypes.data)
    assert np.all(np.isfinite(pixels)), ('nonfinite dot fixture', dots_path)
    return pixels


def dots_metric(pixels):
    row = pixels[pixels.shape[0]//2, :, :3].mean(axis=1)
    peak = float(row.max())
    return dict(peak=peak, width=int(np.count_nonzero(row > peak*.5)) if peak > 1.e-8 else 0)


dots_programs = {path: dots_program(path) for path in ('point', 'spot', 'material', 'pbralpha', 'alpha')}
dots_target(257, 1)
for dots_path, dots_pr in dots_programs.items():
    use(dots_pr)
    dots_uniform('relight_limits', .1, 4); dots_int('rig_rim_mode', 0)
    dots_uniform('size', 15); dots_uniform('falloff', .5); dots_uniform('color', 1, 1, 1)
    dots_uniform('far_z', -100); dots_int('lightFunc', 1); dots_int('projectionMap', 2)
    dots_int('proj_shadow_idx', -1); dots_uniform('proj_n', 0, 0, -1)
    dots_uniform('proj_range', 15); dots_uniform('proj_focus', 0); dots_uniform('proj_lod', 7)
    dots_uniform('gobo_tint', 1, 1, 1); dots_int('gobo_pattern', -1)
    dots_uniform('gobo_anim_params', 0, 0, 1, 0)
    kinds = [0, 1] if dots_path in ('point', 'spot') else [1] if dots_path == 'pbralpha' else [0]
    for pbr in kinds:
        materials = [(0., 0., 0.), (.25, 0., 0.), (.5, 0., 0.), (.75, 0., 0.),
                     (.25, .25, 0.), (.5, .5, 0.), (.75, .75, 0.)] if not pbr else [
            (r, 0., m) for r in (.3, .6, 1.) for m in (0., 1.)]
        for material, env, metallic in materials:
            for distance in (.5, 1., 2., 5., 10.):
                for view in (0., 60.):
                    kw = dict(pbr=pbr, gloss=material, roughness=material, metallic=metallic,
                              env=env, distance=distance, view=view, extent=max(distance, 1.))
                    off = dots_read(**kw, enabled=0)
                    off_hash = hashlib.sha256(off.tobytes()).hexdigest()
                    for physical in (0, 1):
                        point = dots_metric(dots_read(**kw, radius=0., physical=physical))
                        for radius in (0., .03, .12, .45, .9):
                            widths = []
                            for softness in (0., .5, 1.):
                                actual = dots_read(**kw, radius=radius, softness=softness, physical=physical)
                                off_again = dots_read(**kw, radius=radius, softness=softness,
                                                      physical=physical, enabled=0)
                                dots_require(np.array_equal(off, off_again), ('OFF bytes', dots_path, pbr))
                                measured = dots_metric(actual)
                                widths.append(measured['width'])
                                if not pbr and material == 0. and dots_path != 'alpha':
                                    dots_require(np.max(actual[:, :, :3]) == 0., ('matte specular', dots_path))
                                # Test peak conservation for the pure legacy
                                # specular lobe outside the emitting sphere.
                                # PBR total irradiance includes diffuse wrap;
                                # its unchanged bytes are checked against the ref.
                                if view == 0. and env == 0. and not pbr and radius < distance:
                                    dots_require(measured['peak'] <= point['peak']*1.001+1.e-7,
                                                 ('new peak/firefly', dots_path, pbr, material, physical))
                                dots_rows.append(dict(path=dots_path, pbr=pbr, material=material, env=env,
                                    metallic=metallic, distance=distance, view=view, radius=radius,
                                    softness=softness, physical=physical, on=measured, off=dots_metric(off),
                                    on_hash=hashlib.sha256(actual.tobytes()).hexdigest(), off_hash=off_hash))
                            if view == 0. and not pbr and env == 0.:
                                dots_require(min(widths) >= point['width']-1,
                                             ('source width', dots_path, material))
                                if radius < distance:
                                    # One pixel accounts for nearest-LUT/FWHM quantization.
                                    dots_require(all(b >= a-1 for a, b in zip(widths, widths[1:])),
                                                 ('softness width', dots_path, material))
    print('[dots-gl] matrix', dots_path, 'complete', flush=True)

    if dots_path == 'spot':
        # Low shiny, unmapped wood prim: diffuseF packs vertex alpha into both
        # specular/gloss and environment intensity. Isolate the highlight to
        # measure the small source image separately from the diffuse pool.
        dots_target(513)
        kw = dict(side=513, height=513, extent=1., gloss=.25, env=.25)
        shaped = dots_read(**kw)
        no_env = dots_read(**dict(kw, env=0.))
        metrics = dict(highlight=dots_metric(shaped), area=dots_metric(no_env),
                       emitter=dots_metric(shaped-no_env))
        dots_require(metrics['emitter']['peak'] < 1.e-7, 'projector source bypasses area highlight')
        dots_require(metrics['highlight']['peak'] <= metrics['area']['peak']*1.01,
                     'user-like highlight peak')
        dots_require(metrics['highlight']['width'] >= metrics['area']['width']*.99,
                     'user-like highlight width')
        for physical in (0, 1):
            zero = dots_read(**kw, radius=0., physical=physical)
            tiny = dots_read(**kw, radius=1.e-6, physical=physical)
            zero_metric, tiny_metric = dots_metric(zero), dots_metric(tiny)
            # The nearest-filtered production LUT can cross a texel boundary
            # with last-bit direction changes. Bound that local quantization,
            # while checking the highlight's peak and width independently.
            dots_require(abs(zero_metric['peak']-tiny_metric['peak']) <= zero_metric['peak']*3.e-5+1.e-7
                         and abs(zero_metric['width']-tiny_metric['width']) <= 1
                         and np.max(np.abs(zero-tiny)) <= zero_metric['peak']*.001+1.e-7,
                         ('zero-size continuity', physical))
            matte = dots_read(**dict(kw, gloss=0.), physical=physical)
            dots_require(np.max(matte[:, :, :3]) == 0., ('zero shininess with environment', physical))
        np.save(dots_out/(dots_tag+'_highlight.npy'), shaped)
        dots_image = dots_read(**dict(kw, component=0, wood=1.))
        Image.fromarray(np.uint8(np.clip(dots_image[:, :, :3], 0, 1)**(1/2.2)*255)).save(
            dots_out/(dots_tag+'.png'))
        # Moving light/camera, including the aperture leaving individual pixels.
        frames = [dots_read(**dict(kw, shift=t*.02, view=t*2.)) for t in np.linspace(-1, 1, 41)]
        peaks = [dots_metric(frame)['peak'] for frame in frames]
        temporal = float(max(np.max(np.abs(b[:, :, :3]-a[:, :, :3])) for a, b in zip(frames, frames[1:])))
        dots_require(max(peaks) <= metrics['area']['peak']*1.1, 'moving-light firefly')
        dots_require(temporal <= max(metrics['area']['peak']*.1, 1.e-5), 'temporal source-image step')
        metrics.update(temporal_max_delta=temporal, moving_peak=max(peaks))
        (dots_out/(dots_tag+'_fixture.json')).write_text(json.dumps(metrics, indent=2)+'\n')
        print('[dots-gl] user fixture', metrics, flush=True)
        dots_target(257, 1)

if '--dots-compare' in sys.argv:
    baseline = json.loads(Path(sys.argv[sys.argv.index('--dots-compare')+1]).read_text())['rows']
    dots_require(len(baseline) == len(dots_rows), 'baseline matrix size')
    for before, after in zip(baseline, dots_rows):
        keys = ('path', 'pbr', 'material', 'env', 'metallic', 'distance', 'view', 'radius', 'softness', 'physical')
        dots_require(all(before[k] == after[k] for k in keys), 'baseline fixture identity')
        dots_require(before['off_hash'] == after['off_hash'], ('baseline OFF bytes', after['path']))
        if after['pbr']:
            dots_require(before['on_hash'] == after['on_hash'], ('baseline PBR bytes', after['path']))
        if after['path'] != 'spot' or after['env'] == 0.:
            dots_require(before['on_hash'] == after['on_hash'], ('unaffected lighting bytes', after['path']))
        else:
            dots_require(after['on']['peak'] <= before['on']['peak']+1.e-7,
                         'baseline spotlight peak/firefly')
    print('[dots-gl] baseline comparison', len(dots_rows), 'OFF and',
          sum(row['pbr'] for row in dots_rows), 'PBR readbacks', flush=True)

(dots_out/(dots_tag+'_matrix.json')).write_text(json.dumps(dict(
    shader_ref=shader_ref or 'worktree', rows=dots_rows, failures=dots_failures), indent=2)+'\n')
print('[dots-gl]', 'FAIL' if dots_failures else 'PASS', len(dots_rows), 'matrix rows;',
      len(dots_failures), 'failures', dots_failures[:12], flush=True)
assert not dots_failures, dots_failures[:12]
