"""Real-WGL light-shape dot regression, using production deferred/forward GLSL.

Run directly; --shader-ref COMMIT reads sources without moving HEAD. The exact
4f1c152fa08 negative control must fail the peak/width checks; 7ef9dc7320a
must fail retained reflection energy, cookie colour and auxiliary-view checks.
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
dots_view_cases = []


def dots_check_view_bind():
    """Compile the exact production master upload; no viewer build."""
    path = Path('indra/newview/alcinerigrim.cpp')
    text = (subprocess.check_output(['git', 'show', shader_ref+':'+path.as_posix()], text=True)
            if shader_ref else path.read_text())
    start = text.index('    static LLCachedControl<bool> shape_enabled')
    binding = text[start:text.index('    static const LLStaticHashedString limits', start)]
    path = Path('indra/newview/pipeline.cpp')
    pipeline = (subprocess.check_output(['git', 'show', shader_ref+':'+path.as_posix()], text=True)
                if shader_ref else path.read_text())
    start = pipeline.index('static bool light_shape_master_enabled()')
    cpu_shape = pipeline[start:pipeline.index('void LLPipeline::setupHWLights()', start)]
    shim = r'''
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sstream>
#include "alrelightmath.h"
using F32=float;
template<class T> T llclamp(T v,T lo,T hi) { return std::clamp(v,lo,hi); }
struct LLVector3 { float mV[3]={1,1,1}; };
struct LLVector4 { float mV[4]; LLVector4(float x=0,float y=0,float z=0,float w=0):mV{x,y,z,w} {} };
struct LLVOVolume {
    int getID() const { return 0; }
    float getLightRadius() const { return 10; }
    float getLightFalloff(int) const { return .5f; }
    const LLVector3& getScale() const { static LLVector3 v; return v; }
};
struct ALCineLightRigManager {
    static ALCineLightRigManager& instance() { static ALCineLightRigManager m; return m; }
    bool lightShapeSourceFor(int,LLVector4&) const { return false; }
};
constexpr int DEFERRED_LIGHT_FALLOFF=0;
std::ostringstream debug;
#define LL_DEBUGS(x) debug
#define LL_ENDL ""
bool shapeEnabled=false,areaEnabled=false,gCubeSnapshot=false;
struct Settings {} gSavedSettings;
template<class T> struct LLCachedControl {
    const char* name;
    LLCachedControl(Settings&,const char* n,T):name(n) {}
    T operator()() const {
        if (!std::strcmp(name,"RenderLightShapeEnable")) return T(shapeEnabled);
        if (!std::strcmp(name,"RenderLocalLightDefaultRadius")) return T(.45f);
        if (!std::strcmp(name,"RenderLocalLightNearFloor")) return T(.1f);
        if (!std::strcmp(name,"RenderLocalLightPhysicalMaxBoost")) return T(4);
        return T(0);
    }
};
struct LLPipeline {
    static inline bool sPrismLensRender=false,sRenderingHUDs=false,sImpostorRender=false,sReflectionRender=false;
} ;
struct Pipeline { bool areaShadowsActive() { return areaEnabled; } } gPipeline;
struct GL { bool shape=false; void setLightShapeActive(bool v) { shape=v; } } gGL;
namespace LLShaderMgr { enum { RELIGHT_ENABLED }; }
struct Shader { int flags=-1; void uniform1i(int,int v) { flags=v; } };
'''
    shim += cpu_shape + r'''
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
        int expected=((mask&1) && !(mask&24) ? 1:0) | (areaEnabled ? 2:0);
        if (shader.flags!=expected || gGL.shape!=shapeEnabled) ++failures;
        LLVOVolume volume;
        LLVector4 shape=light_shape_for_volume(&volume);
        if (shape.mV[0] != (expected&1 ? .45f:0.f) || shape.mV[1]!=0 || shape.mV[2]!=1 || shape.mV[3]!=-1) ++failures;
        std::printf("VIEW %d %d %.9g %.9g %.9g %.9g\n",mask,shader.flags,shape.mV[0],shape.mV[1],shape.mV[2],shape.mV[3]);
    }
    std::printf("[dots-view] 128 production binding cases: %d failures\n",failures);
    return failures ? 1:0;
}
'''
    out = dots_out/('view_'+dots_tag); out.mkdir(parents=True, exist_ok=True)
    source = (out/'view.cpp').resolve()
    vcvars = Path('C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat')
    exe = (out/'view.exe').resolve()
    cmd = (out/'compile.cmd').resolve()
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++17 /EHsc "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\n' %
                   (vcvars, source, exe, (out/'view.obj').resolve(), exe))
    header = Path('indra/newview/alrelightmath.h').resolve()
    # Use the selected revision's math header as well as its CPU functions.
    if shader_ref:
        header_text = subprocess.check_output(['git', 'show', shader_ref+':'+header.relative_to(Path.cwd()).as_posix()], text=True)
        header = (out/'alrelightmath.h').resolve()
        header.write_text(header_text)
    source.write_text((shim+binding+test).replace('#include "alrelightmath.h"',
                      '#include "'+header.as_posix()+'"'))
    result = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=out, text=True, capture_output=True)
    for line in result.stdout.splitlines():
        if line.startswith('VIEW '):
            parts = line.split()
            dots_view_cases.append((int(parts[1]), int(parts[2]), [float(x) for x in parts[3:]]))
        else:
            print(line, flush=True)
    if result.stderr: print(result.stderr, flush=True)
    dots_require(len(dots_view_cases) == 128, 'production CPU view/shape cases executed')
    dots_require(result.returncode == 0,
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
    gb.albedo=vec4(dotsComponent!=0 && dotsPbr==0 ? vec3(0) :
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
        entry = 'void localLightMain(float source_radius)' if 'void localLightMain(float source_radius)' in original else 'void main()'
        if path == 'spot':
            # Observe the actual reflected term as a second readback component.
            # Extract its production expression rather than copying lighting.
            pattern = r'final_color \+= (color\.rgb \* texture2DLodSpecular\(stc\.xy,[^;]+);'
            original = re.sub(pattern, r'{ dotsReflected = \1; final_color += dotsReflected; }', original)
            original = original.replace(entry+'\n{', entry+'\n{\nvec3 dotsReflected=vec3(0);', 1)
            reflected = 'dotsReflected'+(' + projectorImage' if 'vec3 relightProjectorImage(' in original else '')
            original = original.replace('frag_color.rgb = final_color * final_scale;',
                'frag_color.rgb = (dotsComponent==2 ? '+reflected+' : final_color) * final_scale;')
        header, body = original.split(entry, 1)
        for name, value in {'light_shape': 'dotsShape', 'trans_center': 'dotsOrigin',
                            'proj_origin': 'dotsOrigin'}.items():
            body = re.sub(r'\b'+name+r'\b', value, body)
        original = header+entry+body
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
              physical=0, side=257, height=1, extent=1., shift=0., wood=0., spec_color=None):
    # Distance is the centre-to-receiver length at the centre pixel.
    angle = np.deg2rad(view)
    origin = (-4*np.tan(angle)-distance*np.sin(angle)+shift, 0, -4+distance*np.cos(angle))
    dots_uniform('dotsOrigin', *origin)
    dots_uniform('dotsView', np.tan(angle)); dots_uniform('dotsExtent', extent)
    dots_uniform('dotsSide', side); dots_uniform('dotsHeight', height); dots_uniform('dotsWood', wood)
    spec_color = (gloss,)*3 if spec_color is None else (spec_color,)*3
    dots_uniform('dotsSpec', *spec_color, gloss)
    dots_uniform('dotsRoughness', roughness); dots_uniform('dotsMetallic', metallic)
    dots_uniform('dotsEnv', env); dots_int('dotsPbr', pbr); dots_int('dotsComponent', component)
    dots_uniform('dotsShape', radius, physical, 128/9 if physical else 1., -1.)
    dots_uniform('light_shape', radius, physical, 128/9 if physical else 1., -1.)
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
                                if dots_path == 'spot' and radius == 0. and softness == 0. and physical == 0:
                                    dots_require(np.array_equal(actual, off), ('legacy endpoint bytes', material, env, distance, view))
                                if dots_path == 'spot' and env > 0.:
                                    reflected = dots_read(**dict(kw, component=2), radius=radius,
                                                          softness=softness, physical=physical)
                                    reference = dots_read(**dict(kw, component=2), enabled=0)
                                    dots_require(dots_metric(reflected)['peak'] <= dots_metric(reference)['peak']+1.e-7,
                                                 ('reflection peak conservation', material, distance, view, radius, softness))
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
        metrics = dict(highlight=dots_metric(shaped), area=dots_metric(no_env))
        # The image must survive: a zero output no longer satisfies this gate.
        reflection = dots_read(**dict(kw, component=2))
        metrics['reflection'] = dots_metric(reflection)
        dots_require(metrics['reflection']['peak'] > 1.e-7, 'nonzero reflected projector image')
        dots_require(metrics['highlight']['peak'] <= metrics['area']['peak']+1.e-7,
                     'user-like highlight peak')
        dots_require(metrics['highlight']['width'] >= metrics['area']['width'],
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
            matte = dots_read(**dict(kw, gloss=0., env=0.), physical=physical)
            dots_require(np.max(matte[:, :, :3]) == 0., ('zero shininess without environment', physical))
        np.save(dots_out/(dots_tag+'_highlight.npy'), shaped)
        dots_image = dots_read(**dict(kw, component=0, wood=1.))
        Image.fromarray(np.uint8(np.clip(dots_image[:, :, :3], 0, 1)**(1/2.2)*255)).save(
            dots_out/(dots_tag+'.png'))
        # Moving light/camera, including the aperture leaving individual pixels.
        frames = [dots_read(**dict(kw, shift=t*.02, view=t*2.)) for t in np.linspace(-1, 1, 41)]
        area_frames = [dots_read(**dict(kw, env=0., shift=t*.02, view=t*2.)) for t in np.linspace(-1, 1, 41)]
        peaks = [dots_metric(frame)['peak'] for frame in frames]
        temporal = float(max(np.max(np.abs(b[:, :, :3]-a[:, :, :3])) for a, b in zip(frames, frames[1:])))
        area_temporal = float(max(np.max(np.abs(b[:, :, :3]-a[:, :, :3])) for a, b in zip(area_frames, area_frames[1:])))
        area_peak = max(dots_metric(frame)['peak'] for frame in area_frames)
        dots_require(max(peaks) <= area_peak+1.e-7, 'moving-light firefly')
        dots_require(temporal <= area_temporal+1.e-7, 'temporal source-image step')
        metrics.update(temporal_max_delta=temporal, moving_peak=max(peaks),
                       area_temporal_max_delta=area_temporal, area_moving_peak=area_peak)
        # Independent specular colour / environment controls, with positive
        # OFF references and energy bounds on the observed production image.
        energy_rows = []
        endpoint_rows = []
        reflected_hashes = {}
        dots_target(1025)
        reflection_kw = dict(side=1025, height=1025, extent=2., gloss=.25, component=2)
        for spec_color in (0., 1.):
            for environment in (0., .5, 1.):
                case = dict(reflection_kw, spec_color=spec_color, env=environment)
                off = dots_read(**case, enabled=0)
                on = dots_read(**case)
                full = dots_read(**dict(case, component=1))
                legacy_full = dots_read(**dict(case, component=1), enabled=0)
                endpoint = dots_read(**dict(case, component=1), radius=0., softness=0.)
                dots_require(np.array_equal(endpoint, legacy_full),
                             ('zero radius/softness legacy endpoint', spec_color, environment))
                errors = []
                for epsilon in (1.e-3, 1.e-4, 1.e-5, 1.e-6):
                    tiny = dots_read(**dict(case, component=1), radius=epsilon, softness=epsilon)
                    errors.append(float(np.max(np.abs(tiny-legacy_full))))
                dots_require(errors[-1] <= float(legacy_full.max())*.001+1.e-7,
                             ('joint source/material continuity', spec_color, environment, errors))
                endpoint_rows.append(dict(spec_color=spec_color, env=environment, errors=errors))
                on_energy = float(on[:, :, :3].sum(dtype=np.float64))
                off_energy = float(off[:, :, :3].sum(dtype=np.float64))
                reflected_hashes[(spec_color, environment)] = hashlib.sha256(on.tobytes()).hexdigest()
                if environment:
                    dots_require(off_energy > 1., ('nonzero reference reflection', spec_color, environment))
                    ratio = on_energy/max(off_energy, 1.e-20)
                    dots_require(.5 <= ratio <= 1.1, ('retained reflection energy', spec_color, environment, ratio))
                    dots_require(np.all(full[:, :, :3]+1.e-7 >= on[:, :, :3]), ('image included in full light', spec_color, environment))
                    if spec_color == 0.:
                        dots_require(np.array_equal(full, on), ('black spec retains environment', environment))
                else:
                    ratio = None
                    dots_require(np.max(on[:, :, :3]) == 0. and np.max(off[:, :, :3]) == 0.,
                                 ('environment zero removes image', spec_color))
                    dots_require(float(full[:, :, :3].max()) > .1 if spec_color else np.max(full) == 0.,
                                 ('independent specular positive control', spec_color))
                energy_rows.append(dict(spec_color=spec_color, env=environment, off=off_energy,
                                        on=on_energy, ratio=ratio))
        metrics['reflection_energy'] = energy_rows
        metrics['legacy_endpoint'] = endpoint_rows
        for environment in (0., .5, 1.):
            dots_require(reflected_hashes[(0., environment)] == reflected_hashes[(1., environment)],
                         ('reflection independent of specular colour', environment))
        for spec_color in (0., 1.):
            half, full = [r['on'] for r in energy_rows if r['spec_color'] == spec_color and r['env'] > 0.]
            dots_require(abs(full-2*half) <= full*1.e-7, ('linear environment intensity', spec_color))

        footprint_rows = []
        for gloss in (.25, .75):
            for softness in (0., .5, 1.):
                previous = None
                for radius in (0., .03, .12, .45, .9):
                    case = dict(reflection_kw, gloss=gloss, env=1., spec_color=0., radius=radius, softness=softness)
                    image = dots_read(**case)
                    reference = dots_read(**case, enabled=0)
                    measured = dots_metric(image)
                    ratio = float(image[:, :, :3].sum(dtype=np.float64)/max(
                                  reference[:, :, :3].sum(dtype=np.float64), 1.e-20))
                    dots_require(.5 <= ratio <= 1.1, ('source/material reflection energy', gloss, softness, radius, ratio))
                    dots_require(measured['peak'] > 1.e-7, ('source/material reflection nonzero', gloss, softness, radius))
                    if previous:
                        dots_require(measured['peak'] <= previous['peak']+1.e-7 and measured['width'] >= previous['width']-1,
                                     ('source reflection peak/width', gloss, softness, radius))
                    previous = measured
                    footprint_rows.append(dict(gloss=gloss, softness=softness, radius=radius,
                                               energy_ratio=ratio, **measured))
        metrics['reflection_footprints'] = footprint_rows

        # The same CPU-generated shape and master must render identically in
        # main, Prism, cube/probe and reflection views, with area visibility
        # toggled independently. HUD/impostor exclusions are checked above.
        dots_target(513)
        main = dots_read(**kw)
        aux_count = 0
        aux_matches = 0
        for mask, flags, shape in dots_view_cases:
            if (mask&1) and not (mask&24):
                aux = dots_read(**kw, radius=shape[0], physical=shape[1], enabled=flags)
                dots_require(np.array_equal(main, aux), ('auxiliary sized-image parity', mask))
                aux_matches += int(np.array_equal(main, aux))
                aux_count += 1
        metrics['aux_view_parity_cases'] = aux_count
        metrics['aux_view_parity_matches'] = aux_matches
        dots_require(aux_count == 16, 'all eligible auxiliary-view combinations rendered')

        # Low-frequency RGB structure must survive source/material filtering.
        # Compare images in normalized measured bounds; no CPU shader oracle.
        multicolour = cookie_pixels.copy()
        u, v = (xx+.5)/128, (yy+.5)/128
        multicolour[:, :, :3] *= np.stack((.15+.8*u, .15+.8*v, .15+.8*(1-u)*(.3+.7*(1-v))), axis=2)
        active(0x84C2); bindTex(0x0DE1, dots_cookie)
        teximage(0x0DE1, 0, 0x8814, 128, 128, 0, 0x1908, 0x1406, multicolour.ctypes.data)
        gl('glGenerateMipmap', None, U)(0x0DE1); active(0x84C0)
        dots_target(1025)
        colour_kw = dict(reflection_kw, gloss=.75, env=1., spec_color=0.)
        colour_off = dots_read(**colour_kw, enabled=0)
        colour_on = dots_read(**colour_kw)

        def low_frequency(pixels):
            rgb = pixels[:, :, :3]
            ys, xs = np.where(rgb.max(axis=2) > rgb.max()*1.e-3)
            dots_require(len(xs) > 0, 'nonzero multicolour image')
            if not len(xs): return np.zeros((32, 32, 3))
            crop = rgb[ys.min():ys.max()+1, xs.min():xs.max()+1]
            return np.stack([np.array(Image.fromarray(crop[:, :, c], mode='F').resize(
                (32, 32), Image.Resampling.BILINEAR)) for c in range(3)], axis=2)

        before_colour, after_colour = low_frequency(colour_off), low_frequency(colour_on)
        rgb_correlation = (float(np.corrcoef(before_colour.ravel(), after_colour.ravel())[0, 1])
                           if before_colour.std() > 0 and after_colour.std() > 0 else 0.)
        before_luma, after_luma = before_colour.sum(axis=2), after_colour.sum(axis=2)
        valid = (before_luma > before_luma.max()*.15) & (after_luma > after_luma.max()*.15)
        chroma = []
        for rgb, luma in ((before_colour, before_luma), (after_colour, after_luma)):
            values = rgb[valid]/np.maximum(luma[valid, None], 1.e-20)
            chroma.append(values-values.mean(axis=0) if len(values) else np.zeros((1, 3)))
        correlation = (float(np.corrcoef(chroma[0].ravel(), chroma[1].ravel())[0, 1])
                       if min(c.std() for c in chroma) > 1.e-6 else 0.)
        colour_ratio = float(colour_on[:, :, :3].sum(dtype=np.float64)/max(
                            colour_off[:, :, :3].sum(dtype=np.float64), 1.e-20))
        dots_require(np.isfinite(correlation) and correlation >= .9, ('cookie colour correlation', correlation))
        dots_require(np.isfinite(rgb_correlation) and rgb_correlation >= .9,
                     ('cookie RGB image correlation', rgb_correlation))
        dots_require(.5 <= colour_ratio <= 1.1, ('coloured reflection energy', colour_ratio))
        metrics.update(cookie_colour_correlation=correlation, cookie_rgb_correlation=rgb_correlation,
                       cookie_energy_ratio=colour_ratio)
        for label, pixels in (('cookie_off', colour_off), ('cookie_on', colour_on)):
            scaled = pixels[:, :, :3]/max(float(pixels[:, :, :3].max()), 1.e-20)
            Image.fromarray(np.uint8(np.clip(scaled, 0, 1)**(1/2.2)*255)).save(dots_out/(dots_tag+'_'+label+'.png'))
        active(0x84C2); bindTex(0x0DE1, dots_cookie)
        teximage(0x0DE1, 0, 0x8814, 128, 128, 0, 0x1908, 0x1406, cookie_pixels.ctypes.data)
        gl('glGenerateMipmap', None, U)(0x0DE1); active(0x84C0)
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
            # The previous commit deleted this image; restored reflection is
            # bounded separately above. Bound the combined peak by the old
            # direct lobe plus the original unsized projector response.
            dots_require(after['on']['peak'] <= before['on']['peak']+after['off']['peak']+1.e-7,
                         'baseline direct plus reflected peak bound')
        if after['path'] == 'spot' and after['env'] > 0. and after['radius'] >= .45 and after['softness'] == 1. and after['view'] == 0.:
            dots_require(after['on']['peak'] <= before['on']['peak']+1.e-7,
                         'baseline sized spotlight peak/firefly')
    print('[dots-gl] baseline comparison', len(dots_rows), 'OFF and',
          sum(row['pbr'] for row in dots_rows), 'PBR readbacks', flush=True)

(dots_out/(dots_tag+'_matrix.json')).write_text(json.dumps(dict(
    shader_ref=shader_ref or 'worktree', rows=dots_rows, failures=dots_failures), indent=2)+'\n')
print('[dots-gl]', 'FAIL' if dots_failures else 'PASS', len(dots_rows), 'matrix rows;',
      len(dots_failures), 'failures', dots_failures[:12], flush=True)
assert not dots_failures, dots_failures[:12]
